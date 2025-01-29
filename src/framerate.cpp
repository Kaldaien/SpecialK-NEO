// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++, C#, and Java: https://pvs-studio.com
/**
 * This file is part of Special K.
 *
 * Special K is free software : you can redistribute it
 * and/or modify it under the terms of the GNU General Public License
 * as published by The Free Software Foundation, either version 3 of
 * the License, or (at your option) any later version.
 *
 * Special K is distributed in the hope that it will be useful,
 *
 * But WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Special K.
 *
 *   If not, see <http://www.gnu.org/licenses/>.
 *
**/

#include <SpecialK/stdafx.h>

#include <SpecialK/framerate.h>
#include <SpecialK/commands/limit_reset.inl>

#include <SpecialK/render/dxgi/dxgi_swapchain.h>

#include <SpecialK/nvapi.h>
#include <SpecialK/diagnostics/cpu.h>

#include <SpecialK/log.h>

#include <imgui/font_awesome.h>

#pragma comment(lib, "dwmapi.lib")

#include <concurrent_unordered_map.h>

#ifdef  __SK_SUBSYSTEM__
#undef  __SK_SUBSYSTEM__
#endif
#define __SK_SUBSYSTEM__ L"  Timing  "

#pragma float_control (precise,  on)
#pragma float_control ( except, off)
#pragma fp_contract   (         off)
#pragma fenv_access   (         off)

float SK_LatentSyncAlpha             = 0.991f;
float SK_LatentSyncDeltaMultiplier   = 0.133f;
float SK_LatentSyncBackOffMultiplier = 1.020f;

bool SK_HasHighResWaitableTimer = false;

// Set these clocks to non-zero before init. to prevent division by zero races
int64_t                     SK_QpcFreq        = 1;
int64_t                     SK_QpcTicksPerMs  = 1;
uint32_t                    SK_QpcFreqInTsc   = 1UL;
int64_t                     SK_TscFreq        = 1;
int64_t                     SK_PerfFreq       = 1;
int64_t                     SK_PerfTicksPerMs = 1;
uint32_t                    SK_PerfFreqInTsc  = 1UL;
bool                        SK_TscInvariant   = false;
bool                        SK_CPU_HasMWAITX  = false;
SK::Framerate::EventCounter SK::Framerate::events;

float __target_fps      = 0.0;
float __target_fps_bg   = 0.0;
float __target_fps_temp = 0.0;

enum class SK_LimitApplicationSite {
  BeforeBufferSwap,
  DuringBufferSwap,
  AfterBufferSwap,
  DontCare,
  EndOfFrame // = 4 (Default)
};

float fSwapWaitRatio = 3.33f;
float fSwapWaitFract = 0.66f;

float
SK::Framerate::Limiter::undershoot_percent = 7.5f;


bool     __SK_BFI                      = false;
int      __SK_BFI_Interval             =     2;
int      __SK_LatentSyncFrame          =     0;
int      __SK_LatentSyncSkip           =     0;
LONGLONG __SK_LatentSyncPostDelay      =   0LL;
LONGLONG __SK_LatentSyncSwapTime       =   0LL;
LONGLONG __SK_LatentSync_LastSwap      =   0LL;
LONGLONG __SK_LatentSync_FrameInterval =   0LL;
float    __SK_LatentSync_SwapSecs      =  3.3f;
int      __SK_LatentSync_Adaptive      =    15;

__forceinline
static void
SK_AMD_MWAITX (INT64 qpcTarget = 0)
{
  if (! config.render.framerate.use_amd_mwaitx)
    return YieldProcessor ();

  static alignas(64) uint64_t monitor = 0ULL;

  _mm_monitorx (&monitor, 0, 0);
  _mm_mwaitx   (0x2, 0, (qpcTarget > 0) ? ((DWORD)qpcTarget * SK_PerfFreqInTsc + 1) : SK_PerfFreqInTsc);
}

__forceinline
static void
SK_YieldProcessor (INT64 qpcTarget = 0)
{
  static const bool bIsCompatibleZen =
    SK_CPU_IsZen () && SK_CPU_HasMWAITX;

  if (bIsCompatibleZen)
    SK_AMD_MWAITX (qpcTarget);
  else YieldProcessor ();
}

struct {
#define _MAX_WAIT_SAMPLES 120
  LONGLONG busy          = 0LL;
  LONGLONG sleep         = 0LL;
                         
  LONGLONG start_busy    = 0LL;
  LONGLONG start_sleep   = 0LL;

  ULONG64  last_rollover = 0ULL;

  void reset (void) noexcept
  {
    busy  = 0;
    sleep = 0;
  }

  void beginBusy (void) noexcept
  {
    start_busy = SK_QueryPerf ().QuadPart;
  }

  void endBusy (void) noexcept
  {
    if (last_rollover < SK_GetFramesDrawn () - _MAX_WAIT_SAMPLES) {
        last_rollover = SK_GetFramesDrawn ();
        busy  = 0;
        sleep = 0;
    }

    busy += (SK_QueryPerf ().QuadPart - start_busy);
  }

  void beginSleep (void) noexcept
  {
    start_sleep = SK_QueryPerf ().QuadPart;
  }

  void endSleep (void) noexcept
  {
    if (last_rollover < SK_GetFramesDrawn () - _MAX_WAIT_SAMPLES) {
        last_rollover = SK_GetFramesDrawn ();
        busy  = 0;
        sleep = 0;
    }

    sleep += (SK_QueryPerf ().QuadPart - start_sleep);
  }

  float getBusyPercent (void) noexcept
  {
    return static_cast < float> (100.0 *
         ( static_cast <double> (busy) /
           static_cast <double> (busy  +
                                 sleep) ) );
  }

  float getBusyMs (void) noexcept
  {
    return static_cast < float> (1000.0 *
         ( static_cast <double> (busy) /
           static_cast <double> (SK_PerfFreq) ) ) /
           static_cast < float> (SK_GetFramesDrawn () - last_rollover);
  }

  float getSleepMs (void) noexcept
  {
    return static_cast < float> (1000.0 *
         ( static_cast <double> (sleep) /
           static_cast <double> (SK_PerfFreq) ) ) /
           static_cast < float> (SK_GetFramesDrawn () - last_rollover);
  }
} wait_time;

float SK_Framerate_GetBusyWaitPercent (void) noexcept
{
  return
    wait_time.getBusyPercent ();
}

float SK_Framerate_GetBusyWaitMs (void) noexcept
{
  return
    wait_time.getBusyMs ();
}

float SK_Framerate_GetSleepWaitMs (void) noexcept
{
  return
    wait_time.getSleepMs ();
}

void SK_LatentSync_BeginSwap (void) noexcept
{
  __SK_LatentSync_LastSwap =
    SK_QueryPerf ().QuadPart;
}

void SK_LatentSync_EndSwap (void) noexcept
{
  const auto qpcNow =
    SK_QueryPerf ();

  static constexpr int _MAX_SWAPS = 768;

  struct swap_history_s {
    struct swap_record_s {
      LONGLONG qpc_delta;
      LONGLONG qpc_sampled;
    } records [_MAX_SWAPS] = { { 0, 0UL },
                               { 0, 0UL } };

    using span =
     gsl::span <swap_record_s, _MAX_SWAPS>;

    int total  = 0;
  } static history;

  const LONGLONG maxAge =
    static_cast <LONGLONG> (
      static_cast <double> (__SK_LatentSync_SwapSecs) *
      static_cast <double> (SK_PerfFreq)
    );

  history.records [history.total++ % _MAX_SWAPS] = {
    qpcNow.QuadPart - __SK_LatentSync_LastSwap,
    qpcNow.QuadPart
  };

  LONGLONG avg_swaps     = 0LL;
  LONGLONG sampled_swaps = 0LL;

  for ( const auto& swap : swap_history_s::span (history.records) )
  {
    if ( (qpcNow.QuadPart -
              swap.qpc_sampled) < maxAge )
    {
      sampled_swaps ++;
          avg_swaps +=
              swap.qpc_delta;
    }
  }

  __SK_LatentSyncSwapTime =
                (sampled_swaps > 0) ?
    (avg_swaps / sampled_swaps)     :
                 sampled_swaps;
}


struct scanline_target_s {
  LARGE_INTEGER qpc_t0 = { 0L, 0UL };
  LARGE_INTEGER qpc_tL = { 0L, 0UL };

  struct {
    bool     acquired  = false;
    int      target    =     0;
    LONGLONG margin    =     0;
    LONGLONG scan_time =     0; // How long did it take to lock-on?
                                   //   Useful for tuning tolerances

    struct {
      SK_AutoHandle acquire;
      SK_AutoHandle resync;
    } signals;

    void requestResync (void)
    {
      const SK_RenderBackend& rb =
        SK_GetCurrentRenderBackend ();

      const auto pDisplay =
        &rb.displays [rb.active_display];

      if (pDisplay->signal.timing.vsync_freq.Numerator > 0)
      {
        __scanline.qpc_tL.QuadPart =
          (((SK_PerfFreq * pDisplay->signal.timing.vsync_freq.Denominator) /
                          (pDisplay->signal.timing.vsync_freq.Numerator) ) /
                           pDisplay->signal.timing.total_size.cy) *
              config.render.framerate.latent_sync.scanline_offset;
      }

      if (        signals.resync.isValid ())
        SetEvent (signals.resync);
      
    }

    void notifyAcquired (void)
    {
      if (signals.resync.isValid  ()) ResetEvent (signals.resync);
      if (signals.acquire.isValid ()) SetEvent   (signals.acquire);
    }

    bool isResyncing  (void) { return WaitForSingleObject (signals.resync,  0) == WAIT_OBJECT_0; }
    bool isPending    (void) { return WaitForSingleObject (signals.acquire, 0) == WAIT_OBJECT_0; }

    void resetSignals (void) { if (signals.resync.isValid  ()) ResetEvent (signals.resync);
                               if (signals.acquire.isValid ()) ResetEvent (signals.acquire);     }
  } lock;
} __scanline;


bool
SK_LatentSync_AllowFrameSkip (void)
{
  const SK_RenderBackend& rb =
    SK_GetCurrentRenderBackend ();

  if ( SK_API_IsLayeredOnD3D12 (rb.api) ||
       rb.isTrueFullscreen ()           ||
       SK_GL_OnD3D11                    )
  {
    return false;
  }

  return
    SK_API_IsLayeredOnD3D11 (rb.api) ||
    SK_API_IsLayeredOnD3D10 (rb.api) ||
    SK_API_IsDirect3D9      (rb.api) ||
    SK_API_IsGDIBased       (rb.api);
}


void
SK_ImGui_LatentSyncConfig (void)
{
  if (config.render.framerate.present_interval == 0)
  {
    // Show Advanced Options and Stats
    static bool
      bAdvanced = false;

    static std::set <SK_ConfigSerializedKeybind *>
      timing_keybinds = {
        &config.render.framerate.latent_sync.tearline_move_up_keybind,
        &config.render.framerate.latent_sync.tearline_move_down_keybind,
        &config.render.framerate.latent_sync.timing_resync_keybind,
        &config.render.framerate.latent_sync.toggle_fcat_bars_keybind
      };

    const SK_RenderBackend& rb =
      SK_GetCurrentRenderBackend ();

    const auto pDisplay =
      &rb.displays [rb.active_display];

    if (! pDisplay->signal.timing.hsync_freq.Numerator)
      return;

    if (config.render.framerate.target_fps > 0.0f)
    {
      ImGui::TreePush ("");

      static bool show_hidden = false;
      if (ImGui::IsItemClicked (1))
        show_hidden = true;

      if (show_hidden)
      {
        ImGui::InputFloat ("Alpha",    &SK_LatentSyncAlpha);
        ImGui::InputFloat ("DeltaMul", &SK_LatentSyncDeltaMultiplier);
        ImGui::InputFloat ("BackOff",  &SK_LatentSyncBackOffMultiplier);
      }

      const bool
        readjust_offset =
          ImGui::SliderInt ( "Sync Offset",
                               &config.render.framerate.latent_sync.scanline_offset,
                                 -pDisplay->signal.timing.total_size.cy / 3,
                                  pDisplay->signal.timing.total_size.cy / 3,
                                    "%d Scanlines" );

      auto *pLimiter         =
        SK::Framerate::GetLimiter (rb.swapchain);

      auto _DelayBiasPercent = [&]() -> int
      {
        return static_cast <int> (
          std::round (
            config.render.framerate.latent_sync.delay_bias * 100.0f
          )
        );
      };

      static int iDelayBiasPercent =
                 _DelayBiasPercent ();

      int sel = 0;

      if (config.render.framerate.latent_sync.auto_bias)
      {
        static double dMilliseconds = 0.0;

        dMilliseconds += std::max (
          1000.0 / pLimiter->get_limit  (),
          pLimiter->effective_frametime ()
        );

        if (dMilliseconds >= 200.0)
        {
          iDelayBiasPercent =
          _DelayBiasPercent ();

          dMilliseconds = 0.0;
        }

        ImGui::PushItemFlag (
          ImGuiItemFlags_Disabled,
          true
        );
      }

      else
      {
        if      (config.render.framerate.latent_sync.delay_bias >= 0.99f ) sel = 0;
        else if (config.render.framerate.latent_sync.delay_bias >= 0.90f ) sel = 1;
        else if (config.render.framerate.latent_sync.delay_bias >= 0.75f ) sel = 2;
        else if (config.render.framerate.latent_sync.delay_bias >= 0.50f ) sel = 3;
        else if (config.render.framerate.latent_sync.delay_bias >= 0.25f ) sel = 4;
        else if (config.render.framerate.latent_sync.delay_bias >= 0.10f ) sel = 5;
        else                                                               sel = 6;
      }

      if  (
            ImGui::Combo (
              config.render.framerate.latent_sync.auto_bias
                ? "Delay Bias (Auto)"
                : "Delay Bias",
              &sel,
              config.render.framerate.latent_sync.auto_bias ? (
                std::format (
                  "{} Input,\t{} Display\0\0",
                  iDelayBiasPercent == 100
                    ? "All"
                    : iDelayBiasPercent == 0
                        ? "*No"
                        : std::to_string       ( iDelayBiasPercent ) + "%",
                  iDelayBiasPercent == 100
                    ? "No"
                    : iDelayBiasPercent == 0
                        ? "All"
                        : std::to_string ( 100 - iDelayBiasPercent ) + "%"
                ).c_str ()
              ) : (
                "All Input,\t No Display\0"
                "90% Input,\t10% Display\0"
                "75% Input,\t25% Display\0"
                "50% Input,\t50% Display\0"
                "25% Input,\t75% Display\0"
                "10% Input,\t90% Display\0"
                "*No Input,\t All Display\0\0"
              )
            )
          )
      {
        switch (sel)
        {
          case 0:  config.render.framerate.latent_sync.delay_bias = 1.0f;   break;
          case 1:  config.render.framerate.latent_sync.delay_bias = 0.90f;  break;
          case 2:  config.render.framerate.latent_sync.delay_bias = 0.75f;  break;
          case 3:  config.render.framerate.latent_sync.delay_bias = 0.50f;  break;
          case 4:  config.render.framerate.latent_sync.delay_bias = 0.25f;  break;
          case 5:  config.render.framerate.latent_sync.delay_bias = 0.10f;  break;
          default:
          case 6:  config.render.framerate.latent_sync.delay_bias = 0.00f;
                   __SK_LatentSyncPostDelay                       = 0;      break;
        }

        __scanline.lock.requestResync ();
      }

      if (ImGui::IsItemHovered (ImGuiHoveredFlags_AllowWhenDisabled))
      {
        ImGui::BeginTooltip    (  );
        ImGui::TextUnformatted ("Controls the Distribution of Idle Time Per-Frame");
        ImGui::Separator       (  );
        //ImGui::BeginGroup      (  );
        //ImGui::BulletText      ("Display");
        //ImGui::BulletText      ("Input");
        //ImGui::EndGroup        (  );
        //ImGui::SameLine        (  );
        //ImGui::BeginGroup      (  );
        //ImGui::TextUnformatted ("Waiting for screen refresh after finishing a frame");
        //ImGui::TextUnformatted ("Waiting after screen refresh to begin a new frame");
        //ImGui::EndGroup        (  );
        ImGui::TextUnformatted ("");
        ImGui::TextUnformatted ("Increasing input bias will reduce input latency, but gives the CPU / GPU less time to complete each frame");
        ImGui::TextUnformatted ("");
        ImGui::TextUnformatted ("NOTE:\tEven at '*No Input,\tAll Display', Latent Sync eliminates normal VSYNC latency");
        ImGui::EndTooltip      (  );
      }

      if (config.render.framerate.latent_sync.auto_bias)
      {
        ImGui::PopItemFlag     (  );
      }

      static constexpr int _MAX_FRAMES = 30;

      struct {
        double input   [_MAX_FRAMES] = { };
        double display [_MAX_FRAMES] = { };

        int frames = 0;

        double getInput (void) noexcept
        {
          double avg     = 0.0,
                 samples = 0.0;

          for (int i = 0; i < std::min (frames, _MAX_FRAMES); ++i)
          {
            ++samples; avg += input [i];
          }

          return
            ( avg / samples );
        }

        double getDisplay (void) noexcept
        {
          double avg     = 0.0,
                 samples = 0.0;

          for (int i = 0; i < std::min (frames, _MAX_FRAMES); ++i)
          {
            ++samples; avg += display [i];
          }

          return
            ( avg / samples );
        }
      } static latency_avg;

      latency_avg.input [latency_avg.frames     % _MAX_FRAMES] =
        (1000.0 / pLimiter->get_limit           ()) -
                  pLimiter->effective_frametime ();
      latency_avg.display [latency_avg.frames++ % _MAX_FRAMES] =
                  pLimiter->effective_frametime ();

      ImGui::Text ( ICON_FA_MOUSE " %5.2f ms\t" ICON_FA_DESKTOP " %5.2f ms",
                    latency_avg.getInput (),    latency_avg.getDisplay () );

      ImGui::SameLine ();
      ImGui::Spacing  ();
      ImGui::SameLine ();
      ImGui::SeparatorEx (ImGuiSeparatorFlags_Vertical);
      ImGui::SameLine ();
      ImGui::Spacing  ();
      ImGui::SameLine ();

      ImGui::Checkbox ("Auto Bias", &config.render.framerate.latent_sync.auto_bias);

      if (ImGui::IsItemHovered ())
      {
        ImGui::SetTooltip ("Experimental, may lead to visible tearing during extreme CPU/GPU load changes.");
      }

      if (config.render.framerate.latent_sync.auto_bias)
      {
        bool bTargetInputInMs                =
          ( config.render.framerate.latent_sync.auto_bias_target.percent <= 0.0f );

        static float fLastTargetInputMs      =  bTargetInputInMs
          ? config.render.framerate.latent_sync.auto_bias_target.ms
          : 0.85f;

        static float fLastTargetInputPercent = !bTargetInputInMs
          ? config.render.framerate.latent_sync.auto_bias_target.percent
          : 0.5f;

        if (ImGui::Checkbox ("Target Input Latency in milliseconds", &bTargetInputInMs))
        {
          if (bTargetInputInMs)
          {
            config.render.framerate.latent_sync.auto_bias_target.percent = 0.0f;
            config.render.framerate.latent_sync.auto_bias_target.ms      =
              fLastTargetInputMs;
          }

          else
          {
            config.render.framerate.latent_sync.auto_bias_target.ms      = 0.0f;
            config.render.framerate.latent_sync.auto_bias_target.percent =
              fLastTargetInputPercent;
          }
        }

        if (bTargetInputInMs)
        {
          if (ImGui::InputFloat ("Target Input Latency", &config.render.framerate.latent_sync.auto_bias_target.ms))
          {
            fLastTargetInputMs = (
              config.render.framerate.latent_sync.auto_bias_target.ms = std::clamp (
                config.render.framerate.latent_sync.auto_bias_target.ms,
                0.0f,
                25.0f
              )
            );
          }
        }

        else
        {
          float fTargetInputPercent = config.render.framerate.latent_sync.auto_bias_target.percent * 100.0f;

          if (ImGui::SliderFloat ("Target Input Latency", &fTargetInputPercent, 0.000001f, 100.0f, "%4.1f%%"))
          {
            fLastTargetInputPercent = (
              config.render.framerate.latent_sync.auto_bias_target.percent = std::clamp (
                fTargetInputPercent / 100.0f,
                0.000001f,
                1.0f
              )
            );
          }
        }

        if (ImGui::IsItemHovered ())
        {
          ImGui::SetTooltip (
            std::format     (
              "Setting this too {} is likely to cause visible tearing and possible framerate instability.",
              bTargetInputInMs ? "low" : "high"
            ).c_str         ()
          );
        }

        if (bTargetInputInMs)
        {
          float fMaxBiasPercent = config.render.framerate.latent_sync.max_auto_bias * 100.0f;

          if (ImGui::SliderFloat ("Maximum Bias", &fMaxBiasPercent, 0.0f, 90.0f, "%4.1f%%"))
          {
            config.render.framerate.latent_sync.max_auto_bias = std::clamp (
              fMaxBiasPercent / 100.0f,
              0.0f,
              1.0f
            );
          }
        }
      }

      bool bIsD3D9 =
        SK_API_IsDirect3D9 (rb.api);

      ImGui::Separator ();

      ImGui::Combo (
        "Tearing Mode",
        &config.render.framerate.tearing_mode,
        bIsD3D9 ? (
          "Always On\0"
          "Always Off\0"
          "Always Off (Low Latency)\0\0"
        ) : (
          "Always On\0"
          "Always Off\0"
          "Always Off (Low Latency)\0"
          "Adaptive (Prefer On)\0"
          "Adaptive (Prefer Off)\0\0"
        )
      );

      if (ImGui::IsItemHovered ())
      {
        ImGui::BeginTooltip ();
        ImGui::Text         ("Enabling tearing produces the Lowest Possible Input Latency");
        ImGui::Separator    ();
        ImGui::BulletText   ("If GPU load is very high, you may need to disable tearing");
        ImGui::BulletText   ("Disabling tearing will add between 0 and 1 refresh cycles of latency, depending on GPU load");
        ImGui::BulletText   ("Low Latency mode temporarily decreases FPS limit if Render Latency exceeds 1 frame");
        if (! bIsD3D9)
        {
          ImGui::Separator  ();
          ImGui::Text       ("Adaptive modes control tearing based on Frame Stability or Render Latency");
          ImGui::Separator  ();
          ImGui::BulletText ("Adaptive (Prefer On) : only disables tearing if FPS is unstable");
          ImGui::BulletText ("Adaptive (Prefer Off) : only enables tearing if FPS is unstable or Render Latency exceeds 1 frame");
        }
        ImGui::EndTooltip   ();
      }

      int iMultiplier = static_cast <int> (
        std::round (
          pLimiter->get_limit     () /
          rb.getActiveRefreshRate ()
        )
      );

      bool bIsInvalidTearingMode = false;

      if ( config.render.framerate.tearing_mode != SK_TearingMode::AlwaysOn &&
                                    iMultiplier >= 2                        )
      {
        bool bIsTearingModeAdaptiveOff =
          config.render.framerate.tearing_mode ==
            SK_TearingMode::AdaptiveOff;

        bool bAllowFrameSkip  =
          SK_LatentSync_AllowFrameSkip ();

        int iBufferCount      =
          config.render.framerate.buffer_count;

        int iMaxDeviceLatency =
          config.render.framerate.pre_render_limit;

        int iRequiredBufferCount      =
          5 + (2 * (iMultiplier - 2));

        int iRequiredMaxDeviceLatency =
          iRequiredBufferCount + 1;

        bool bIsInvalidBufferCount =
                      iBufferCount <
              iRequiredBufferCount;

        bool bIsInvalidMaxDeviceLatency = ! (
          ( bIsTearingModeAdaptiveOff &&
            bAllowFrameSkip           &&
            iMaxDeviceLatency == 1     ) ||
          ( iMaxDeviceLatency >=
            iRequiredMaxDeviceLatency  )
        );

        if ( bIsInvalidMaxDeviceLatency ||
             bIsInvalidBufferCount      ||
             bIsD3D9                    )
        {
          bIsInvalidTearingMode = true;

          ImGui::SameLine    ();
          ImGui::TextColored (
            ImColor (1.0f, 1.0f, 0.0f),
            ICON_FA_EXCLAMATION_TRIANGLE
          );

          if (ImGui::IsItemHovered ())
          {
            bool bIsD3D12 =
              SK_API_IsLayeredOnD3D12 (rb.api);

            ImGui::BeginTooltip   ();
            ImGui::Text           (
              std::format         (
                "Please set Tearing Mode to \"Always On\" for {}x Scan Mode!",
                iMultiplier
              ).c_str             ()
            );

            ImGui::Separator      ();
            ImGui::BulletText     (
              std::format         (
                "You may enable{}AMD's Enhanced Sync to eliminate tearing",
                !bIsD3D12 ? " NVIDIA's Fast Sync or " : " "
              ).c_str             ()
            );

            if (bIsD3D12)
            {
              ImGui::BulletText   ("NVIDIA's Fast Sync is not compatible with Direct3D 12");
            }

            if ( ! ( bIsD3D12 && !config.render.dxgi.allow_d3d12_footguns ) &&
                 ! ( bIsD3D9  ||  iRequiredBufferCount > 15               ) )
            {
              ImGui::Separator    ();
              ImGui::Text         (
                std::format       (
                  "Other tearing options require certain settings for {}x Mode (Best to Worst)",
                  iMultiplier
                ).c_str           ()
              );

              ImGui::Separator    ();
              ImGui::BeginGroup   ();
              ImGui::BulletText   ("Adaptive (Prefer On)");

              if (bAllowFrameSkip)
              {
                ImGui::BulletText ("Adaptive (Prefer Off)");
              }

              ImGui::BulletText   ("Adaptive (Prefer Off)");
              ImGui::BulletText   ("Always Off (LL)");
              ImGui::BulletText   ("Always Off");
              ImGui::EndGroup     ();
              ImGui::SameLine     (0.0f, 0.0f);
              ImGui::BeginGroup   ();

              auto _PrintRequiredSettings = [](
                int requiredBufferCount,
                int requiredMaxDeviceLatency
              ) -> void {
                ImGui::Text       (
                  std::format     (
                    "  :  Buffer Count = {}\tMax Device Latency = {}",
                    requiredBufferCount,
                    requiredMaxDeviceLatency
                  ).c_str         ()
                );
              };

              _PrintRequiredSettings (
                iRequiredBufferCount,
                iRequiredMaxDeviceLatency
              );

              if (bAllowFrameSkip)
              {
                _PrintRequiredSettings (
                  iRequiredBufferCount,
                  1
                );
              }

              for ( int i = 0; i < 3; i++ )
              {
                _PrintRequiredSettings (
                  iRequiredBufferCount,
                  iRequiredMaxDeviceLatency
                );
              }

              ImGui::EndGroup     ();
            }

            ImGui::EndTooltip     ();
          }
        }
      }

      if (! bIsInvalidTearingMode)
      {
        if ( config.render.framerate.tearing_mode == SK_TearingMode::AdaptiveOff ||
             config.render.framerate.tearing_mode == SK_TearingMode::AdaptiveOn  )
        {
          if (rb.isTrueFullscreen ())
          {
            bIsInvalidTearingMode = true;

            ImGui::SameLine    ();
            ImGui::TextColored (
              ImColor (0.0f, 1.0f, 1.0f),
              ICON_FA_EXCLAMATION_TRIANGLE
            );

            if (ImGui::IsItemHovered ())
            {
              ImGui::SetTooltip ("Adaptive Tearing works better in Fake Fullscreen or Windowed Mode");
            }
          }
        }
      }

      if (bIsD3D9)
      {
        bool bIsTearingD3D9 = [&]() -> bool
        {
          auto pDev9 =
            rb.getDevice <IDirect3DDevice9> ();

          if (pDev9 != nullptr)
          {
            SK_ComQIPtr <IDirect3DSwapChain9> pSwap9 (rb.swapchain);

            if (pSwap9 != nullptr)
            {
              D3DPRESENT_PARAMETERS pparams = { };

              if (SUCCEEDED (pSwap9->GetPresentParameters (&pparams)))
              {
                return
                  pparams.PresentationInterval == D3DPRESENT_INTERVAL_IMMEDIATE ||
                  pparams.PresentationInterval == D3DPRESENT_FORCEIMMEDIATE;
              }
            }
          }

          return false;
        }();

        if ( bIsTearingD3D9 != (config.render.framerate.tearing_mode == SK_TearingMode::AlwaysOn) )
        {
          if (SK_IsInjected () && config.compatibility.init_on_separate_thread)
          {
            config.compatibility.init_on_separate_thread = false;
          }

          if (! bIsInvalidTearingMode)
          {
            bIsInvalidTearingMode = true;

            ImGui::SameLine    ();
            ImGui::TextColored (
              ImColor (1.0f, 1.0f, 0.0f),
              ICON_FA_EXCLAMATION_TRIANGLE
            );

            if (ImGui::IsItemHovered ())
            {
              ImGui::SetTooltip ("Game Restart Required");
            }
          }
        }
      }

      ImGui::Checkbox ("Visualize Tearlines", &config.render.framerate.latent_sync.show_fcat_bars);

      if (ImGui::IsItemHovered ())
      {
        ImGui::SetTooltip ("Draws color-cycling bars to help locate tearing while VSYNC is off");
      }

      ImGui::SameLine ();
      ImGui::SeparatorEx (ImGuiSeparatorFlags_Vertical);
      ImGui::SameLine ();

      if (ImGui::BeginMenu ("Tear Control Keybinds###TearingMenu"))
      {
        const auto Keybinding =
        [] (SK_ConfigSerializedKeybind *binding) ->
        auto
        {
          if (binding == nullptr)
            return false;

          std::string label =
            SK_WideCharToUTF8      (binding->human_readable);

          ImGui::PushID            (binding->bind_name);

          binding->assigning =
            SK_ImGui_KeybindSelect (binding, label.c_str ());

          ImGui::PopID             ();

          return true;
        };

        ImGui::BeginGroup ();
        for ( auto& keybind : timing_keybinds )
        {
          ImGui::Text ( "%s:  ",
                          keybind->bind_name );
        }
        ImGui::EndGroup   ();
        ImGui::SameLine   ();
        ImGui::BeginGroup ();
        for ( auto& keybind : timing_keybinds )
        {
          Keybinding  (   keybind );
        }
        ImGui::EndGroup   ();

        ImGui::EndMenu    ();
      }

      ImGui::Separator ();

      bAdvanced =
        ImGui::TreeNode ("Advanced");

      if (bAdvanced)
      {
        if ( ImGui::SliderFloat ( "Resync Rate",
                                    &config.render.framerate.latent_sync.scanline_resync,
                                      0.0f, 900.0f, "%.0f Second(s)" ) )
        {   config.render.framerate.latent_sync.scanline_resync = std::max (0.0f,
            config.render.framerate.latent_sync.scanline_resync);
        }

        if ( ImGui::SliderInt ( "Anti-Roll",
                                  &config.render.framerate.latent_sync.scanline_error,
                                    0, 2, (config.render.framerate.latent_sync.scanline_error != 1) ?
                                                                                 "%d Clock Ticks" :
                                                                                 "%d Clock Tick" )
           )
        {
          __scanline.lock.requestResync ();
        }

        ImGui::InputFloat ("Retire Stats",  &__SK_LatentSync_SwapSecs, 0.1f, 1.0f, "After %.3f Seconds");

        if (SK_GetCurrentRenderBackend ().api == SK_RenderAPI::OpenGL)
        {
          ImGui::Checkbox ("Flush Before Present (GL)",  &config.render.framerate.latent_sync.flush_before_present);
          ImGui::Checkbox ("Finish Before Present (GL)", &config.render.framerate.latent_sync.finish_before_present);
          ImGui::Checkbox ("Flush After Present (GL)",   &config.render.framerate.latent_sync.flush_after_present);
          ImGui::Checkbox ("Finish After Present (GL)",  &config.render.framerate.latent_sync.finish_after_present);
        }

        __SK_LatentSync_Adaptive =
          std::max (0, std::min (25, __SK_LatentSync_Adaptive));

        ImGui::TreePop ();
      }

      if (readjust_offset)
      {
        __scanline.lock.requestResync ();

        const LONGLONG llVSync0 = static_cast <LONGLONG> (
          ( 1.0 / ( static_cast <double> (pDisplay->signal.timing.vsync_freq.Numerator) /
                    static_cast <double> (pDisplay->signal.timing.vsync_freq.Denominator) ) ) * static_cast <double> (SK_PerfFreq)
        );

        const LONGLONG llVSync1 =
          (pDisplay->signal.timing.vsync_freq.Denominator * SK_PerfFreq) /
           pDisplay->signal.timing.vsync_freq.Numerator;

        SK_ReleaseAssert (llVSync0 == llVSync1);
      }

      ImGui::TreePop  (  );
    }

    ImGui::Separator  (  );
    ImGui::TreePush   ("");

    ImGui::Text       ("");
    ImGui::Text       ("Status: ");

    ImGui::SameLine   (  );
    ImGui::BeginGroup (  );

    const bool locked =
      config.render.framerate.target_fps > 0.0f &&
      __scanline.lock.acquired;

    if (locked)
      ImGui::TextColored (ImColor (0.0f, 1.0f, 0.0f), "Locked-On");
    else
    {
      if (config.render.framerate.target_fps <= 0.0f)
        ImGui::TextColored (ImColor (0.25f, 0.5f, 1.0f), "Unlocked"  );
      else
        ImGui::TextColored (ImColor ( 1.0f, 1.0f, 0.0f), "Locking...");
    }

    ImGui::Text       ("");

    ImGui::BeginGroup ( );
    ImGui::Text       ("VSync Freq:      ");
    ImGui::Text       ("HSync Freq:      ");
    ImGui::Text       ("Blanking:        ");
    if (bAdvanced)
    {
      ImGui::Text     ("Flip Time:       ");
      ImGui::Text     ("Busy Wait %%:    ");
      ImGui::Text     ("");
      ImGui::Text     ("Anti-Roll:       ");
      if (config.render.framerate.latent_sync.delay_bias != 0.0f)
        ImGui::Text   ("Latency Boost:   ");
    }
    ImGui::Text       ("");
    ImGui::Text       ("Acquired Lock:   ");
    ImGui::Text       ("Margin of Error: ");
    ImGui::Text       ("");
    if (bAdvanced)
    {
      ImGui::Text     ("Search Duration: ");
    }
    ImGui::EndGroup   ( );

    ImGui::SameLine   ( );

    ImGui::BeginGroup ( );
    ImGui::Text       ("%5.2f Hz",
        static_cast <double> (pDisplay->signal.timing.vsync_freq.Numerator)   /
        static_cast <double> (pDisplay->signal.timing.vsync_freq.Denominator) );
    ImGui::Text       ("%5.2f kHz",
      ( static_cast <double> (pDisplay->signal.timing.hsync_freq.Numerator)   /
        static_cast <double> (pDisplay->signal.timing.hsync_freq.Denominator) ) / 1000.0);
    ImGui::Text       ("%d Scanlines",
                              pDisplay->signal.timing.total_size.cy -
                              pDisplay->signal.timing.active_size.cy);

    if (bAdvanced)
    {
      ImGui::Text       ((const char *)u8"%5.2f μs",
                                (static_cast <double> (__SK_LatentSyncSwapTime) /
                                 static_cast <double> (SK_PerfTicksPerMs)) * 10000.0);
      ImGui::Text       ("%3.1f%%", wait_time.getBusyPercent ());
      ImGui::Text       ("");
      ImGui::Text       ((const char *)u8"%5.2f μs",
                                (static_cast <double> (config.render.framerate.latent_sync.scanline_error) /
                                 static_cast <double> (SK_PerfTicksPerMs)) * 10000.0);
      if (config.render.framerate.latent_sync.delay_bias != 0.0f)
        ImGui::Text     ("-%5.2f ms",
                                (static_cast <double> (__SK_LatentSyncPostDelay) /
                                 static_cast <double> (SK_PerfTicksPerMs)));
    }

    ImGui::Text       ("");
    if (locked)
    {
      ImGui::Text     ("Scanline %d",  __scanline.lock.target);
      ImGui::Text     ((const char *)u8"± %5.2f μs",
                              (static_cast <double> (__scanline.lock.margin) /
                               static_cast <double> (SK_PerfTicksPerMs)) * 10000.0);
      ImGui::Text     (" (%5.2f scanlines)",
                               static_cast <double> (__scanline.lock.margin) /
                       ( static_cast <double> (pDisplay->signal.timing.hsync_freq.Denominator * SK_PerfFreq) /
                         static_cast <double> (pDisplay->signal.timing.hsync_freq.Numerator) ));
      if (bAdvanced)
        ImGui::Text   ("%5.2f ms", static_cast <double> (__scanline.lock.scan_time) /
                                   static_cast <double> (SK_PerfTicksPerMs));
    }
    else
    {
      ImGui::Text     ("N/A");
      ImGui::Text     ("N/A");
      ImGui::Text     (" (N/A)");
      if (bAdvanced)
        ImGui::Text   ("%5.2f ms", static_cast <double> (__scanline.lock.scan_time) /
                                   static_cast <double> (SK_PerfTicksPerMs));
    }
    ImGui::EndGroup   ( );
    ImGui::EndGroup   ( );
    ImGui::Text       ("");
    ImGui::TreePop    ( );
  }

  if (SK_GetCurrentRenderBackend ().api == SK_RenderAPI::D3D11 ||
      SK_GetCurrentRenderBackend ().api == SK_RenderAPI::OpenGL)
  {
    ImGui::Separator ();
    ImGui::Checkbox  ("Black Frame Insertion", &__SK_BFI);
    if (__SK_BFI)
    {
      if (SK_GetCurrentRenderBackend ().api == SK_RenderAPI::D3D11)
      {
        ImGui::InputInt ("BFI Interval", &__SK_BFI_Interval);

        if (__SK_BFI_Interval < 1)
            __SK_BFI_Interval = 1;

        if (__SK_BFI_Interval > 4)
            __SK_BFI_Interval = 4;

        if (__SK_BFI)
        {
          config.render.framerate.present_interval =
            __SK_BFI_Interval;
        }
      }
    }

    if (ImGui::IsItemHovered ())
        ImGui::SetTooltip ("This feature is incomplete, it should not be used.");
  }
}


class SK_FramerateLimiter_CfgProxy : public SK_IVariableListener {
  bool OnVarChange (SK_IVariable* var, void* val = nullptr)
  {
    if ( static_cast <float *> (var->getValuePointer ()) == &__target_fps )
    {
      config.render.framerate.target_fps =
        *static_cast <float *> (val);

      __target_fps = config.render.framerate.target_fps;
    }

    if ( static_cast <float *> (var->getValuePointer ()) == &__target_fps_bg )
    {
      config.render.framerate.target_fps_bg =
        *static_cast <float *> (val);

      __target_fps_bg = config.render.framerate.target_fps_bg;
    }

    if ( static_cast <int *> (var->getValuePointer ()) == &config.render.framerate.latent_sync.scanline_offset )
    {
      config.render.framerate.latent_sync.scanline_offset =
        *static_cast <int *> (val);


    }

    if ( static_cast <float *> (var->getValuePointer ()) == &config.render.framerate.latent_sync.scanline_resync )
    {
      config.render.framerate.latent_sync.scanline_resync =
        *static_cast <float *> (val);

      __scanline.lock.requestResync ();
    }

    return true;
  }
} __ProdigalFramerateSon;

extern NtQueryTimerResolution_pfn NtQueryTimerResolution;
extern NtSetTimerResolution_pfn   NtSetTimerResolution;
extern NtSetTimerResolution_pfn   NtSetTimerResolution_Original;

void
SK::Framerate::Init (void)
{
  SK_ICommandProcessor
    *pCommandProc = nullptr;

  SK_RunOnce (
    pCommandProc =
      SK_Render_InitializeSharedCVars ()
  );

  if (pCommandProc == nullptr)
    return; // Uh oh?! Something tells me we've been here before...

  PROCESSOR_POWER_INFORMATION pwi [64] = { };

  if ( STATUS_SUCCESS == 
         CallNtPowerInformation (ProcessorInformation, nullptr, 0, pwi, sizeof (pwi)) )
  {
    SK_LOGi0 (
      L"Nominal Clock Frequency (P0): %d MHz; RDTSC ticks per-QPC tick: %d",
        pwi [0].MaxMhz, SK_QpcFreqInTsc
    );
  }

  if (! SK_CPU_IsZen ())
  {
    config.render.framerate.use_amd_mwaitx = false;
  }

  SK_FPU_LogPrecision ();

  pCommandProc->AddVariable ( "LimitSite",
          new SK_IVarStub <int> (&config.render.framerate.enforcement_policy));

  pCommandProc->AddVariable ( "WaitForVBLANK",
          new SK_IVarStub <bool> (&config.render.framerate.wait_for_vblank));


  pCommandProc->AddVariable ( "TargetFPS",
          new SK_IVarStub <float> (&__target_fps,    &__ProdigalFramerateSon));
  pCommandProc->AddVariable ( "BackgroundFPS",
          new SK_IVarStub <float> (&__target_fps_bg, &__ProdigalFramerateSon));

  pCommandProc->AddVariable ( "SwapWaitRatio",
          new SK_IVarStub <float> (&fSwapWaitRatio));

  pCommandProc->AddVariable ( "SwapWaitFract",
          new SK_IVarStub <float> (&fSwapWaitFract));

  pCommandProc->AddVariable ( "Undershoot",
          new SK_IVarStub <float> (&SK::Framerate::Limiter::undershoot_percent));

  pCommandProc->AddVariable ( "LatentSync.TearLocation",
          new SK_IVarStub <int>   (&config.render.framerate.latent_sync.scanline_offset, &__ProdigalFramerateSon));

  pCommandProc->AddVariable ( "LatentSync.ResyncRate",
          new SK_IVarStub <float> (&config.render.framerate.latent_sync.scanline_resync, &__ProdigalFramerateSon));

  pCommandProc->AddVariable ( "LatentSync.ShowFCATBars",
          new SK_IVarStub <bool>  (&config.render.framerate.latent_sync.show_fcat_bars));

  // Implicitly applies queued hooks
  SK_Scheduler_Init ();

  pCommandProc->AddVariable ( "PresentInterval",
      new SK_IVarStub <int> (&config.render.framerate.present_interval));
  pCommandProc->AddVariable ( "MaxDeltaTime",
      new SK_IVarStub <int> (&config.render.framerate.max_delta_time));

  if ( NtQueryTimerResolution != nullptr &&
       NtSetTimerResolution   != nullptr )
  {
    auto _SetTimerResolution =
      ( NtSetTimerResolution_Original != nullptr ) ?
        NtSetTimerResolution_Original              :
        NtSetTimerResolution;

    double& dTimerRes =
      SK::Framerate::Limiter::timer_res_ms;

    ULONG                         min,  max,  cur;
    if ( NtQueryTimerResolution (&min, &max, &cur) ==
           STATUS_SUCCESS )
    {
      dTimerRes =
        static_cast <double> (cur) / 10000.0;

      SK_LOG0 ( ( L"Kernel resolution.: %f ms", dTimerRes ),
                  L"  Timing  " );

      if ( _SetTimerResolution (max, TRUE, &cur) ==
             STATUS_SUCCESS )
      {
        dTimerRes =
          static_cast <double> (cur) / 10000.0;

        SK_LOG0 ( ( L"New resolution....: %f ms", dTimerRes ),
                    L"  Timing  " );
      }
    }
  }

  __scanline.lock.signals.resync.Attach (
    SK_CreateEvent (nullptr, TRUE, TRUE, nullptr));
  __scanline.lock.signals.acquire.Attach (
    SK_CreateEvent (nullptr, TRUE, FALSE, nullptr));
}

void
SK::Framerate::Shutdown (void)
{
  SK_Scheduler_Shutdown ();
}


SK::Framerate::Limiter::Limiter (double target, bool tracks_game_window)
{
  effective_ms  = 0.0;
  tracks_window = tracks_game_window;

  init (target);
}


IDirect3DDevice9Ex*
SK_D3D9_GetTimingDevice (void)
{
  static auto* pTimingDevice =
    reinterpret_cast <IDirect3DDevice9Ex *> (-1);

  if (pTimingDevice == reinterpret_cast <IDirect3DDevice9Ex *> (-1))
  {
    SK_ComPtr <IDirect3D9Ex> pD3D9Ex = nullptr;

    using Direct3DCreate9ExPROC =
      HRESULT (STDMETHODCALLTYPE *)(UINT           SDKVersion,
                                    IDirect3D9Ex** d3d9ex);

    extern Direct3DCreate9ExPROC Direct3DCreate9Ex_Import;

    // For OpenGL, bootstrap D3D9
    SK_BootD3D9 ();
    
    const HRESULT
      hr = (config.apis.d3d9ex.hook) ?
       Direct3DCreate9Ex_Import (D3D_SDK_VERSION, &pD3D9Ex.p)
                                     :
                                E_NOINTERFACE;

    HWND                hwnd    = nullptr;
    IDirect3DDevice9Ex* pDev9Ex = nullptr;

    if (SUCCEEDED (hr))
    {
      hwnd =
        SK_Win32_CreateDummyWindow ();

      D3DPRESENT_PARAMETERS pparams = { };

      pparams.SwapEffect       = D3DSWAPEFFECT_FLIPEX;
      pparams.BackBufferFormat = D3DFMT_X8R8G8B8;
      pparams.hDeviceWindow    = hwnd;
      pparams.Windowed         = TRUE;
      pparams.BackBufferCount  = 2;
      pparams.BackBufferHeight = 2;
      pparams.BackBufferWidth  = 2;

      if ( FAILED ( pD3D9Ex->CreateDeviceEx (
                      D3DADAPTER_DEFAULT,
                        D3DDEVTYPE_HAL,
                          hwnd,
                            D3DCREATE_HARDWARE_VERTEXPROCESSING,
                              &pparams,
                                nullptr,
                                  &pDev9Ex )
                  )           || ( pDev9Ex == nullptr )
          )
      {
        pTimingDevice = nullptr;
      }

      else
      {
        pD3D9Ex.p->AddRef ();
        pDev9Ex->AddRef   ();
        pTimingDevice = pDev9Ex;
      }
    }

    else
    {
      pTimingDevice = nullptr;
    }
  }

  return pTimingDevice;
}


bool
SK_Framerate_WaitForVBlank (void)
{
  const SK_RenderBackend& rb =
    SK_GetCurrentRenderBackend ();

  if (rb.adapter.d3dkmt != 0)
  {
    if (D3DKMTWaitForVerticalBlankEvent == nullptr)
        D3DKMTWaitForVerticalBlankEvent = SK_GetProcAddress (L"gdi32.dll",
       "D3DKMTWaitForVerticalBlankEvent");

    if (D3DKMTWaitForVerticalBlankEvent != nullptr)
    {
      D3DKMT_WAITFORVERTICALBLANKEVENT
             waitForVerticalBlankEvent               = { };
             waitForVerticalBlankEvent.hAdapter      = rb.adapter.d3dkmt;
             waitForVerticalBlankEvent.VidPnSourceId = rb.adapter.VidPnSourceId;

      if ( STATUS_SUCCESS ==
           ((D3DKMTWaitForVerticalBlankEvent_pfn)
             D3DKMTWaitForVerticalBlankEvent)(&waitForVerticalBlankEvent) )
      {
        return true;
      }
    }

    if (D3DKMTGetScanLine == nullptr)
        D3DKMTGetScanLine = SK_GetProcAddress (L"gdi32.dll",
       "D3DKMTGetScanLine");

    D3DKMT_GETSCANLINE
      getScanLine               = { };
      getScanLine.hAdapter      = rb.adapter.d3dkmt;
      getScanLine.VidPnSourceId = rb.adapter.VidPnSourceId;

    if (D3DKMTGetScanLine != nullptr)
    {
      ////if ( STATUS_SUCCESS ==
      ////          D3DKMTGetScanLine (&getScanLine) && getScanLine.InVerticalBlank )
      ////{
      ////  return true;
      ////}

      UINT max_visible_scanline = 0u;
      UINT max_scanline         = 0u;

      bool stage_two = false;

      // Has been modified to wait for the END of VBLANK
      //
      while ( STATUS_SUCCESS ==
                ((D3DKMTGetScanLine_pfn)
                  D3DKMTGetScanLine)(&getScanLine) )
      {
        if (! getScanLine.InVerticalBlank)
        {
          // We found the maximum scanline, now we are back at the top
          //   of the screen...
          if (max_scanline > 0 && getScanLine.ScanLine == 0)
          {
            stage_two = true;
          }

          SK_YieldProcessor ();

          max_visible_scanline =
            std::max (max_visible_scanline, getScanLine.ScanLine);
        }

        else
        {
          if (stage_two && getScanLine.ScanLine == max_scanline - 2)
            return true;

          max_scanline =
            std::max (max_scanline, getScanLine.ScanLine);

          // Indiscriminately returning true would get us any time during VBLANK
          //
          //return true;
        }
      }
    }
  }

#if 1
  return true;
#else
  // D3D10/11/12
  SK_ComQIPtr <IDXGISwapChain>     dxgi_swap (rb.swapchain);
  SK_ComPtr   <IDXGIOutput>        dxgi_output = nullptr;

  const SK_RenderAPI      api  = rb.api;
  if (                    api ==                    SK_RenderAPI::D3D10  ||
       static_cast <int> (api) & static_cast <int> (SK_RenderAPI::D3D11) ||
       static_cast <int> (api) & static_cast <int> (SK_RenderAPI::D3D12) )
  {
    if (            dxgi_swap != nullptr &&
         SUCCEEDED (dxgi_swap->GetContainingOutput (&dxgi_output)) )
    {
      DwmFlush ();

      // Dispatch through the trampoline, rather than hook
      //
      extern WaitForVBlank_pfn
             WaitForVBlank_Original;
      if (   WaitForVBlank_Original != nullptr)
             WaitForVBlank_Original (dxgi_output);
      else                           dxgi_output->WaitForVBlank ();

      return true;
    }
  }

  else
  {
    // If available (Windows 8+), wait on the swapchain
    auto d3d9ex =
      rb.getDevice <IDirect3DDevice9Ex> ();

    // This can be used in graphics APIs other than D3D,
    //   but it would be preferable to simply use D3DKMT
    if (d3d9ex == nullptr)
    {
      d3d9ex =
        SK_D3D9_GetTimingDevice ();
    }

    if (d3d9ex != nullptr)
    {
      DwmFlush ();

      UINT                             orig_latency = 3;
      d3d9ex->GetMaximumFrameLatency (&orig_latency);
      d3d9ex->SetMaximumFrameLatency (1);

      //for (size_t i = 0; i < d3d9ex->GetNumberOfSwapChains (); ++i)
      //{
        d3d9ex->WaitForVBlank (0);//static_cast <UINT> (i));
      //}
      d3d9ex->SetMaximumFrameLatency (
        config.render.framerate.pre_render_limit == SK_NoPreference ?
                                                       orig_latency :
        config.render.framerate.pre_render_limit
      );

      return true;
    }
  }

  return false;
#endif
}

void
SK_Framerate_WaitForVBlank2 (void)
{
  if (D3DKMTWaitForVerticalBlankEvent == nullptr)
      D3DKMTWaitForVerticalBlankEvent = SK_GetProcAddress (L"gdi32.dll",
     "D3DKMTWaitForVerticalBlankEvent");

  if (D3DKMTWaitForVerticalBlankEvent != nullptr)
  {
    const SK_RenderBackend& rb =
      SK_GetCurrentRenderBackend ();

    D3DKMT_WAITFORVERTICALBLANKEVENT
           waitForVerticalBlankEvent               = { };
           waitForVerticalBlankEvent.hAdapter      = rb.adapter.d3dkmt;
           waitForVerticalBlankEvent.VidPnSourceId = rb.adapter.VidPnSourceId;

    if ( STATUS_SUCCESS ==
           ((D3DKMTWaitForVerticalBlankEvent_pfn)
             D3DKMTWaitForVerticalBlankEvent)(&waitForVerticalBlankEvent) )
    {
      return;
    }
  }

  SK_Framerate_WaitForVBlank ();
}

void
SK_D3DKMT_WaitForVBlank (void)
{
  const SK_RenderBackend& rb =
    SK_GetCurrentRenderBackend ();

  // Flush batched commands before zonking this thread off
  if (rb.d3d11.immediate_ctx != nullptr)
      rb.d3d11.immediate_ctx->Flush ();

  SK_Framerate_WaitForVBlank ();

#if 1
  return;
#else
  SK_Framerate_WaitForVBlank2 ();
#endif
};

LONG64 __SK_VBlankLatency_QPCycles;

struct qpc_interval_s {
  int64_t t0;
  int64_t tBegin;
  int64_t tEnd;

  int64_t getNextBegin (int64_t tNow) noexcept
  {
    return (tEnd != 0) ?
      tNow + ((tNow - t0) / tEnd) * tBegin
                     : 0LL;
  }

  int64_t getNextEnd (int64_t tNow) noexcept
  {
    return (tEnd != 0) ?
      tNow + ((tNow - t0) / tEnd) * tEnd
                     : 0LL;
  }

  bool isInside (int64_t tNow) noexcept
  {
    const auto qpcBegin = getNextBegin (tNow);
    const auto qpcEnd   = getNextEnd   (tNow);

    return
      ( tNow >= qpcBegin && tNow <= qpcEnd );
  }

  void waitForBegin (void) noexcept
  {
    const int64_t qpcNow =
      SK_QueryPerf ().QuadPart;

    const auto qpcNext =
      getNextBegin (qpcNow);

    while (SK_QueryPerf ().QuadPart < qpcNext)
      SK_YieldProcessor (qpcNext - SK_QueryPerf ().QuadPart);
  }

  void waitForEnd (void) noexcept
  {
    const int64_t qpcNow =
      SK_QueryPerf ().QuadPart;

    const auto qpcNext =
      getNextEnd (qpcNow);

    while (SK_QueryPerf ().QuadPart < qpcNext)
      SK_YieldProcessor (qpcNext - SK_QueryPerf ().QuadPart);
  }
} __VBlank;

void
SK::Framerate::Limiter::init (double target, bool _tracks_window)
{
  this->tracks_window =
       _tracks_window;

  double dTicksPerFrame = 0.0;

  accum_per_frame =
          modf ( static_cast <double> (SK_PerfFreq) /
                 static_cast <double> (target), &dTicksPerFrame );
  ticks_per_frame = sk::narrow_cast <ULONGLONG> (dTicksPerFrame);

  const SK_RenderBackend& rb =
    SK_GetCurrentRenderBackend ();

  const auto now  =
    SK_QueryPerf ().QuadPart;

  auto next_vsync = 0LL;

  if (tracks_window)
  {
    if (config.render.framerate.swapchain_wait > 0)
    {
      SK_AutoHandle hWaitHandle (SK_GetCurrentRenderBackend ().getSwapWaitHandle ());
      if (          hWaitHandle.isValid ())
      {
        SK_WaitForSingleObject (hWaitHandle, 50UL);
      }
    }

    DWM_TIMING_INFO dwmTiming        = {                      };
                    dwmTiming.cbSize = sizeof (DWM_TIMING_INFO);

    if ( SUCCEEDED ( SK_DWM_GetCompositionTimingInfo (&dwmTiming) ) )
    {
      next_vsync = dwmTiming.qpcVBlank * (SK_TscInvariant ? SK_QpcFreqInTsc : 1);
    }

    auto const pDisplay =
      &rb.displays [rb.active_display];

    if (pDisplay->signal.timing.vsync_freq.Numerator > 0)
    {
      ticks_per_frame +=
        ( ticks_per_frame / ( ( pDisplay->signal.timing.vsync_freq.Denominator * SK_PerfFreq ) /
                                pDisplay->signal.timing.vsync_freq.Numerator ) ) * config.render.framerate.latent_sync.scanline_error;
    }

    if (pDisplay->signal.timing.vsync_freq.Numerator > 0 &&
        next_vsync > now - (pDisplay->signal.timing.vsync_freq.Denominator * SK_PerfFreq * 120) /
                           (pDisplay->signal.timing.vsync_freq.Numerator))
    {
#if 0
      SK_ImGui_Warning (SK_FormatStringW (L"VSync Freq: %5.2f Hz, HSync Freq: %5.2f kHz",
                                                                             ( static_cast <double> (pDisplay->signal.timing.vsync_freq.Numerator)   /
                                                                               static_cast <double> (pDisplay->signal.timing.vsync_freq.Denominator) ),
                                                                             ( static_cast <double> (pDisplay->signal.timing.hsync_freq.Numerator)   /
                                                                               static_cast <double> (pDisplay->signal.timing.hsync_freq.Denominator) ) / 1000.0).c_str ());
#endif

      while (next_vsync < now)
      {
        next_vsync +=
          ( pDisplay->signal.timing.vsync_freq.Denominator * SK_PerfFreq ) /
          ( pDisplay->signal.timing.vsync_freq.Numerator                 );
      }
    }

    else
      next_vsync = now + ticks_per_frame;
  }


  ms  = 1000.0 / static_cast <double> (target);
  fps =          static_cast <double> (target);

  const auto _frames      = ReadAcquire64     (&frames);
  const auto _framesDrawn = SK_GetFramesDrawn (       );

  frames_of_fame.frames_measured.first.initClock  (next_vsync);
  frames_of_fame.frames_measured.last.clock_val  = next_vsync;
  frames_of_fame.frames_measured.first.initFrame  (_framesDrawn);
  frames_of_fame.frames_measured.last.frame_idx += _frames;

  WriteRelease64 ( &start, next_vsync - ticks_per_frame     );
  WriteRelease64 ( &time,                               0LL );
  WriteRelease64 ( &last,  next_vsync - ticks_per_frame * 2 );
  WriteRelease64 ( &next,  next_vsync                       );
  WriteRelease64 ( &frames, 0 );
}


bool
SK::Framerate::Limiter::try_wait (void)
{
  if (limit_behavior != LIMIT_APPLY) {
    return false;
  }

  if (tracks_window)
  {
    if (SK_IsGameWindowActive () || __target_fps_bg == 0.0f)
    {
      if (__target_fps <= 0.0f) {
        return false;
      }
    }
  }

  LARGE_INTEGER
    next_ {
     .QuadPart =
       ReadAcquire64 (&frames) * sk::narrow_cast <LONGLONG> (ticks_per_frame) +
       ReadAcquire64 (&start ) };

  return
    ( SK_QueryPerf ().QuadPart < next_.QuadPart );
}


//#define _RESTORE_TIMER_RES
extern NtSetTimerResolution_pfn
       NtSetTimerResolution_Original;

class SK_ImGui_FrameHistory : public SK_Stat_DataHistory <float, 120>
{
public:
  void timeFrame       (double seconds)
  {
    addValue ((float)(1000.0 * seconds));
  }
};

extern SK_LazyGlobal <SK_ImGui_FrameHistory> SK_ImGui_Frames;
extern bool                                  reset_frame_history;

void
SK::Framerate::Limiter::wait (void)
{
  // This is actually counter-productive in testing... when the thread priority
  //   is lowered after this goes out of scope it may reschedule the thread.
  //SK_Thread_ScopedPriority prio_scope (THREAD_PRIORITY_TIME_CRITICAL);

  // Don't limit under certain circumstances or exiting / alt+tabbing takes
  //   longer than it should.
  if (ReadAcquire (&__SK_DLL_Ending) != 0)
    return;

  if (limit_behavior != LIMIT_APPLY)
    return;


  SK_FPU_ControlWord fpu_cw_orig =
    SK_FPU_SetPrecision (_PC_64);


  if (tracks_window && background == SK_IsGameWindowActive ())
  {
    background = (! background);

    //__scanline.lock.requestResync ();
  }


  if (! background)
  {
    set_limit ( __target_fps_temp > 0.0f ?
                __target_fps_temp        :
                __target_fps            );
  }

  else if (tracks_window)
  {
    set_limit ( __target_fps_bg   > 0.0f ?
                __target_fps_bg          :
                __target_fps_temp > 0.0f ?
                __target_fps_temp        :
                __target_fps            );
  }



  if (NtSetTimerResolution_Original != nullptr)
  {
    ULONG                         min,  max,  cur;
    if ( NtQueryTimerResolution (&min, &max, &cur) ==
           STATUS_SUCCESS )
    {
      double& dTimerRes =
        SK::Framerate::Limiter::timer_res_ms;

      dTimerRes =
        static_cast <double> (cur) / 10000.0;

      if (max != cur)
      {
        SK_LOG1 ( ( L"Kernel resolution.: %f ms", dTimerRes ),
                    L"  Timing  " );

        NTSTATUS status =
          NtSetTimerResolution_Original (max, TRUE, &cur);
        if (                             max   ==    cur &&
                 status == STATUS_SUCCESS )
        {
          SK_LOGi1 ( L"Fixing Unexpected Deviation in "
                     L"Process Timer Resolution...    " );

          dTimerRes =
            static_cast <double> (cur) / 10000.0;

          SK_LOG1 ( ( L"New resolution....: %f ms", dTimerRes ),
                      L"  Timing  " );
        }

        else
        {
          // This will fail repeatedly, only log it once.
          static bool        log_once         = false;
          if (std::exchange (log_once, true) == false)
          {
            SK_ReleaseAssert (status == STATUS_SUCCESS || max == cur);
            SK_LOGi0         (
              L"NtSetTimerResolution (...) unexpected behavior -- NtStatus=%x, "
              L"max=%d, cur=%d", status, max, cur
            );
          }
        }
      }
    }
  }


  if (tracks_window && __target_fps <= 0.0f)
  {
    SK_FPU_SetControlWord (_MCW_PC, &fpu_cw_orig);

    return;
  }

  
  const SK_RenderBackend& rb =
    SK_GetCurrentRenderBackend ();


#if 1
  // SK's framerate limiter is more energy efficient, prefer it over NVIDIA Reflex
  //   while the game is in the background
  if ((! background) && rb.isReflexSupported () && __target_fps > 0.0f)
  {
    if ((__SK_ForceDLSSGPacing) || (config.nvidia.reflex.use_limiter && config.nvidia.reflex.enable &&
         ((! config.nvidia.reflex.native) || config.nvidia.reflex.override)))
    {
      extern bool
             __SK_DoubleUpOnReflex;
      if ((! __SK_DoubleUpOnReflex) || (SK_GetCurrentGameID () != SK_GAME_ID::Starfield && (! config.nvidia.reflex.combined_limiter)))
        return;
    }
  }
#endif


  auto pDisplay =
      &rb.displays [rb.active_display];


  LONGLONG ticks_per_scanline = (pDisplay->signal.timing.hsync_freq.Numerator > 0) ?
    (pDisplay->signal.timing.hsync_freq.Denominator * SK_PerfFreq) /
    (pDisplay->signal.timing.hsync_freq.Numerator)                                 : 1;

  LONGLONG ticks_per_refresh  = (pDisplay->signal.timing.vsync_freq.Numerator > 0) ?
    (pDisplay->signal.timing.vsync_freq.Denominator * SK_PerfFreq) /
    (pDisplay->signal.timing.vsync_freq.Numerator)                                 : 1;


  auto threadId    = GetCurrentThreadId ();
  auto framesDrawn = SK_GetFramesDrawn  ();

  // Two limits applied on the same frame would cause problems, don't allow it.
  if (_frame_shame.count (threadId) &&
      _frame_shame       [threadId] == framesDrawn) return;
  else
      _frame_shame       [threadId]  = framesDrawn;

  auto _time =
    SK_QueryPerf ().QuadPart;


  bool normal = true;

  if (restart || full_restart)
  {
    ////WriteRelease64 (&start, SK_QueryPerf ().QuadPart);

    ////if (full_restart || config.render.framerate.present_interval == 0)
    {
      init (__target_fps, tracks_window);
      full_restart = false;
    }

    restart        = false;
    normal         = false;

    WriteRelease64 (&frames, 0);
  }

  WriteRelease64         (&time,  _time);
  InterlockedIncrement64 (&frames);

  LONG64 time_  = ReadAcquire64 ( &time   ),
         start_ = ReadAcquire64 ( &start  ),
         last_  = ReadAcquire64 ( &last   ),
         next_  = ReadAcquire64 ( &frames ) * ticks_per_frame
                                            + start_;

  // Actual frametime before we forced a delay
  effective_ms =
    1000.0 * ( static_cast <double> (time_ - last_) /
               static_cast <double> ( SK_PerfFreq ) );

  WriteRelease64 (&next, next_);

  if (normal)
  {
    double missed_frames = 0.0,
           missing_time  =
      static_cast <double> ( time_ - next_ ) /
      static_cast <double> ( ticks_per_frame );

    double edge_distance =
      modf ( missing_time, &missed_frames );

    double dMissingTimeBoundary = 1.0;

    if (config.render.framerate.present_interval == 0)
    {
      dMissingTimeBoundary =
        std::max (1.0,
          std::round (           (ticks_per_refresh > 1) ?
            static_cast <double> (ticks_per_refresh) /
            static_cast <double> (ticks_per_frame)       : 1.0)
        );
    }

    static constexpr double dEdgeToleranceLow  = 0.0;
    static constexpr double dEdgeToleranceHigh = 1.0;

    if ( missed_frames >= dMissingTimeBoundary &&
         edge_distance >= dEdgeToleranceLow    &&
         edge_distance <= dEdgeToleranceHigh )
    {
      SK_LOG1 ( ( L"Frame Skipping (%f frames) :: Edge Distance=%f",
                    missed_frames, edge_distance ), __SK_SUBSYSTEM__ );

      InterlockedAdd64 ( &frames,
           (LONG64)missed_frames - ((LONG64)missed_frames % (LONG64)(dMissingTimeBoundary))
                       );

      next_  =
         ReadAcquire64 ( &frames ) * ticks_per_frame
                                   + start_;

      if (config.render.framerate.present_interval != 0)
      {
        static uint64_t            ullLastReset = 0;
        if (missed_frames > 1.0 && ullLastReset < SK_GetFramesDrawn () - 16)
        {
          ullLastReset = SK_GetFramesDrawn ();
          full_restart = true;
        }
        else if (tracks_window)
          InterlockedAdd (&SK_RenderBackend::flip_skip, 1);
      }
    }
  }

  // To become the world's most accurate framerate limiter, we even account for
  //   sub-QPC frequency time accumulation!
  double                                                 dAccum = 0.0;
    modf ( accum_per_frame * ReadAcquire64 ( &frames ), &dAccum );

  next_ +=
    static_cast <LONGLONG> (dAccum);

  auto
  SK_RecalcTimeToNextFrame =
  [&](void)->
    double
    {
      return
        std::max (
          ( static_cast <double> ( next_ - SK_QueryPerf ().QuadPart ) /
            static_cast <double> ( SK_PerfFreq )                    ),
            0.0  );
    };

  auto _FrametimeSeconds = [&]() -> double
  {
    return std::max (effective_ms, ms) / 1000.0;
  };

  if (next_ > 0LL)
  {
    // Flush batched commands before zonking this thread off
    if (tracks_window && rb.d3d11.immediate_ctx != nullptr)
    {
      rb.d3d11.immediate_ctx->Flush ();
    }

    // Create an unnamed waitable timer.
    if (! timer_wait.isValid ())
    {
      // Prefer high-resolution timer when available, but this won't be available in WINE or Windows 8.1
      timer_wait.Attach (
        CreateWaitableTimerEx ( nullptr, nullptr,
           CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS )
                        );
    }

    SK_HasHighResWaitableTimer =
      (timer_wait.isValid ());

    if (! SK_HasHighResWaitableTimer)
    {
       timer_wait.Attach (
         CreateWaitableTimer (nullptr, FALSE, nullptr)
                         );
    }

    constexpr
      double duS = (1000.0 * 10000.0);

    double
      to_next_in_secs =
        SK_RecalcTimeToNextFrame ();

    // First use a kernel-waitable timer to scrub off most of the
    //   wait time without completely gobbling up a CPU core.
    if ( timer_wait.isValid () && (to_next_in_secs * 1000.0 >= timer_res_ms * 2.875) )
    {
      // Schedule the wait period just shy of the timer resolution determined
      //   by NtQueryTimerResolution (...). Excess wait time will be handled by
      //     spinning, because the OS scheduler is not accurate enough.
      LARGE_INTEGER
        liDelay
          { .QuadPart =
              std::min (
                static_cast <LONGLONG> (
                  to_next_in_secs * 1000.0 - timer_res_ms * fSwapWaitRatio
                                       ),
                static_cast <LONGLONG> (
                  to_next_in_secs * 1000.0 * fSwapWaitFract
                                       )
                       )
          };

        liDelay.QuadPart =
      -(liDelay.QuadPart * 10000LL);

      // Check if the next frame is sooner than waitable timer resolution before
      //   rescheduling this thread.
      if ( SK_HasHighResWaitableTimer ?
                         SetWaitableTimerEx ( timer_wait.m_h, &liDelay,
                                                 0, nullptr, nullptr,
                                                    nullptr, 0 )
                       : SetWaitableTimer   ( timer_wait.m_h, &liDelay,
                                                 0, nullptr, nullptr,
                                                    FALSE )
         )
      {
        SK_AutoHandle hSwapWait;

        if (config.render.framerate.present_interval != 0)
        {
          // The ideal thing to wait on is the SwapChain, since it is what we are
          //   ultimately trying to throttle :)
          if (tracks_window && config.render.framerate.swapchain_wait > 0)
          {
            hSwapWait.Attach (rb.getSwapWaitHandle ());
          }
        }

        HANDLE hWaitObjs [2] = { 0 };
        int    iWaitObjs     =   0;

        DWORD  dwWait  = WAIT_FAILED;
        while (dwWait != WAIT_OBJECT_0)

        {
          if (config.render.framerate.present_interval != 0)
          {
            if (                        hSwapWait.isValid ())
              hWaitObjs [iWaitObjs++] = hSwapWait.m_h;
          }

          to_next_in_secs =
            std::max (0.0, SK_RecalcTimeToNextFrame ());

          if (static_cast <double> (-liDelay.QuadPart) / 10000.0 > timer_res_ms * 2.0)
            hWaitObjs [iWaitObjs++] = timer_wait.m_h;

          if (iWaitObjs == 0)
            break;

          wait_time.beginSleep ();

          liDelay.QuadPart =
            -(static_cast <LONGLONG> (to_next_in_secs * duS));

          dwWait = iWaitObjs < 2  &&
                  (hWaitObjs  [0] != hSwapWait.m_h) ?
            SK_WaitForSingleObject_Micro ( timer_wait.m_h, &liDelay )
          :  WaitForMultipleObjects      ( iWaitObjs,
                                           hWaitObjs,
                                             TRUE,
                                               sk::narrow_cast <DWORD> (
                                                     static_cast <double> (-liDelay.QuadPart) / 10000.0
                                                                       ) );

          if ( dwWait != WAIT_OBJECT_0     &&
               dwWait != WAIT_OBJECT_0 + 1 &&
               dwWait != WAIT_TIMEOUT )
          {
            DWORD dwLastError =
                 GetLastError ();

            dll_log->Log (
              L"[(%s)-%s:%d] Result of WaitForSingleObject = %x (GetLastError: %x)",
                     __FUNCTIONW__, __FILEW__, __LINE__, dwWait,  dwLastError
            );
          }

          if (hSwapWait.isValid ())
              hSwapWait.Close   ();

          wait_time.endSleep ();

          break;
        }
      }
    }


    // Any remaining wait-time will degenerate into a hybrid busy-wait,
    //   this is also when VBlank synchronization is applied if user wants.
    if ( tracks_window && config.render.framerate.wait_for_vblank )
    {
      DWM_TIMING_INFO dwmTiming        = {                      };
                      dwmTiming.cbSize = sizeof (DWM_TIMING_INFO);

      BOOL bNextVBlankIsShortTerm = TRUE;

      if ( SUCCEEDED ( SK_DWM_GetCompositionTimingInfo (&dwmTiming) ) )
      {
        if ( next_ < sk::narrow_cast <LONG64> (dwmTiming.qpcVBlank) )
             bNextVBlankIsShortTerm = FALSE;
        if ( bNextVBlankIsShortTerm )
        {
          SK_Framerate_WaitForVBlank ();
        }
      }
    }


    static double      lastFps = fps;
    if (std::exchange (lastFps,  fps) != lastFps) {
      __scanline.lock.requestResync ();
    }


    bool bStopFrameSkip = false;

    auto _ManageTearing = [&]() -> void
    {
      static constexpr int _MIN_FRAMES =  6,
                           _MAX_FRAMES = 30;

      struct {
        double input_min [_MIN_FRAMES] = { },
               input_max [_MAX_FRAMES] = { };

        int frames = 0;

        double getMinInput (void) noexcept
        {
          double avg     = 0.0,
                 samples = 0.0;

          for ( int i = 0; i < std::min (frames, _MIN_FRAMES); ++i )
          {
            ++samples; avg += input_min [i];
          }

          return
            ( avg / samples );
        }

        double getMaxInput (void) noexcept
        {
          double avg     = 0.0,
                 samples = 0.0;

          for ( int i = 0; i < std::min (frames, _MAX_FRAMES); ++i )
          {
            ++samples; avg += input_max [i];
          }

          return
            ( avg / samples );
        }
      } static latency_avg;

      latency_avg.input_min [latency_avg.frames   % _MIN_FRAMES] =
      latency_avg.input_max [latency_avg.frames++ % _MAX_FRAMES] =
        (1000.0 / get_limit           ()) -
                  effective_frametime ();

      bool bIsUnstableFPS =
        latency_avg.getMaxInput () < 0.0;

      static double dWaitSeconds = 0.0;

      // TODO: Adaptive Tearing causes frame drops in HW Legacy Flip
      // -
      // Remove ACTION_FrameBecameStable when it's possible to change
      // Presentation Interval without hitches in FSE...
      static constexpr int ACTION_None              = 0,
                           ACTION_HighVariation     = 1,
                           ACTION_HighRenderLatency = 2,
                           ACTION_StuckInputLatency = 3,
                           ACTION_FrameBecameStable = 4;
      static int iACTION = ACTION_None;

      auto _RevertACTION = [&](bool bFull = false) -> void
      {
        if (bFull)
        {
          iACTION =
           ACTION_None;
        }

        __target_fps_temp = 0.0f;
             dWaitSeconds = 0.00;
      };

      auto _EnableTearing = [&](bool bEnableTearing = true) -> void
      {
        if ( bEnableTearing ||
             bIsUnstableFPS )
        {
          bStopFrameSkip = true;
        }

        if (config.render.framerate.tearing_mode != SK_TearingMode::AppControlled)
        {
          if (config.render.framerate.present_interval > 0)
          {
            // Adaptive VSync
            if (config.render.framerate.tearing_mode == SK_TearingMode::AdaptiveOff)
            {
              config.render.framerate.turn_vsync_off =
                bEnableTearing;

              config.render.dxgi.allow_tearing = true;
            }
          }

          else
          {
            config.render.dxgi.allow_tearing =
              bEnableTearing;
          }
        }
      };

      bool bIsD3D9 =
        SK_API_IsDirect3D9 (rb.api);

      switch (config.render.framerate.present_interval)
      {
        case SK_NoPreference:
        {
          if ( rb.present_interval >= 1 ||
               rb.isTrueFullscreen ()   ||
               bIsD3D9                  )
          {
            config.render.framerate.tearing_mode =
              SK_TearingMode::AppControlled;
          }

          else
          {
            config.render.framerate.tearing_mode =
              config.render.dxgi.allow_tearing
                ? SK_TearingMode::AlwaysOn
                : SK_TearingMode::AlwaysOff;
          }
        } break;

        case 0:
        {
          switch (config.render.framerate.tearing_mode)
          {
            case  SK_TearingMode::AdaptiveOn:
            case  SK_TearingMode::AdaptiveOff:
              if (bIsD3D9)
              {
                config.render.framerate.tearing_mode =
                  SK_TearingMode::AlwaysOn;
              }
            case  SK_TearingMode::AlwaysOn:
            case  SK_TearingMode::AlwaysOff:
            case  SK_TearingMode::AlwaysOff_LowLatency:
              break;
            default:
              config.render.framerate.tearing_mode =
                  SK_TearingMode::AlwaysOn;
              break;
          }
        } break;

        default:
        {
          switch (config.render.framerate.tearing_mode)
          {
            case  SK_TearingMode::AdaptiveOff:
              if (bIsD3D9)
              {
                config.render.framerate.tearing_mode =
                  SK_TearingMode::AlwaysOff;
              }
            case  SK_TearingMode::AlwaysOff:
            case  SK_TearingMode::AlwaysOff_LowLatency:
              break;
            default:
              config.render.framerate.tearing_mode =
                  SK_TearingMode::AlwaysOff;
              break;
          }
        } break;
      }

      int iTearingMode = config.render.framerate.tearing_mode;

      if (iTearingMode == SK_TearingMode::AppControlled)
      {
        if (bIsD3D9)
        {
          static double dMaxSeconds = 15.0,
                           dSeconds = 0.00;

          auto _IsTearingD3D9 = [&]() -> bool
          {
            auto pDev9 =
              rb.getDevice <IDirect3DDevice9> ();

            if (pDev9 != nullptr)
            {
              SK_ComQIPtr <IDirect3DSwapChain9> pSwap9 (rb.swapchain);

              if (pSwap9 != nullptr)
              {
                D3DPRESENT_PARAMETERS pparams = { };

                if (SUCCEEDED (pSwap9->GetPresentParameters (&pparams)))
                {
                  dSeconds = 0.0;

                  return
                    pparams.PresentationInterval == D3DPRESENT_INTERVAL_IMMEDIATE ||
                    pparams.PresentationInterval == D3DPRESENT_FORCEIMMEDIATE;
                }
              }
            }

            dSeconds = dMaxSeconds;

            return false;
          };

          static bool bIsTearingD3D9 =
                      _IsTearingD3D9 ();

          dSeconds += _FrametimeSeconds ();

          if (dSeconds >= dMaxSeconds)
          {
            bIsTearingD3D9 =
            _IsTearingD3D9 ();
          }

          iTearingMode =
            bIsTearingD3D9
              ? SK_TearingMode::AlwaysOn
              : SK_TearingMode::AlwaysOff;
        }

        else
        {
          iTearingMode =
            rb.present_interval == 0
              ? SK_TearingMode::AlwaysOn
              : SK_TearingMode::AlwaysOff;
        }
      }

      switch (iTearingMode)
      {
        // Prefer tearing, only disable tearing if FPS is unstable
        case SK_TearingMode::AdaptiveOn:

        // Prefer no tearing, only enable tearing if FPS is unstable or Render Latency exceeds 1 frame
        case SK_TearingMode::AdaptiveOff:

        // No tearing, temporarily decrease FPS limit if Render Latency exceeds 1 frame
        case SK_TearingMode::AlwaysOff_LowLatency:

        // No tearing, reset limiter if variation is too high or input latency is stuck at (-0.1, 0.1) ms
        case SK_TearingMode::AlwaysOff:
        {
          bool bIsTearingModeAdaptiveOn  =
            iTearingMode == SK_TearingMode::AdaptiveOn;

          bool bIsTearingModeAdaptiveOff =
            iTearingMode == SK_TearingMode::AdaptiveOff;

          bool bIsTearingModeAlwaysOffLL =
            iTearingMode == SK_TearingMode::AlwaysOff_LowLatency;

          bool bIsTearingModeAlwaysOff   =
            iTearingMode == SK_TearingMode::AlwaysOff;

          bool bIsComposedPresentMode    =
            rb.presentation.mode == SK_PresentMode::Composed_Composition_Atlas ||
            rb.presentation.mode == SK_PresentMode::Composed_Copy_GPU_GDI      ||
            rb.presentation.mode == SK_PresentMode::Composed_Copy_CPU_GDI      ||
            rb.presentation.mode == SK_PresentMode::Composed_Flip;

          bool bRecoveredIndependentFlip = false;

          static bool        bWasComposedPresentMode = bIsComposedPresentMode;
          if (std::exchange (bWasComposedPresentMode,  bIsComposedPresentMode) &&
                                                      !bIsComposedPresentMode)
          {
            bRecoveredIndependentFlip = true;

            if (! bIsTearingModeAdaptiveOn)
            {
              reset (true);
            }
          }

          if ( ( bIsTearingModeAdaptiveOff ||
                 bIsTearingModeAdaptiveOn   ) &&
               ( iACTION !=
                  ACTION_FrameBecameStable  ) )
          {
            static bool bWasUnstableFPS = false;

            bIsUnstableFPS |=
              latency_avg.getMinInput () < 0.0;

            if (bIsUnstableFPS)
            {
              bWasUnstableFPS = true;
            }

            if (bWasUnstableFPS)
            {
              static double dSeconds = 0.0;

              if (dSeconds < 1.5)
              {
                dSeconds += _FrametimeSeconds ();

                if ( bIsTearingModeAdaptiveOff &&
                     bIsUnstableFPS            )
                {
                  dSeconds = 0.0;
                }

                bIsUnstableFPS = true;
              }

              else
              {
                if (bIsTearingModeAdaptiveOff)
                {
                  dSeconds = 0.0;
                }

                if (! bIsUnstableFPS)
                {
                  if (bIsTearingModeAdaptiveOn)
                  {
                    dSeconds = 0.0;
                  }

                  bWasUnstableFPS = false;
                }
              }
            }
          }

          if (! bIsComposedPresentMode)
          {
            static int         iLastTearingMode = iTearingMode;
            if (std::exchange (iLastTearingMode,  iTearingMode) !=
                                                  iTearingMode)
            {
              _RevertACTION (true);
            }

            // TODO: Update this check when VRR status is detectable on all vendors
            // -
            // To preserve old behaviour, we are keeping regular "Always Off" mode unaffected for now
            bool bIsVRR =
              // rb.gsync_state.active
              bIsTearingModeAlwaysOff;

            static bool        bWasVRR = bIsVRR;
            if (std::exchange (bWasVRR,  bIsVRR) !=
                                         bIsVRR)
            {
              _RevertACTION (true);
            }

            bool bIsAboveRefresh = std::round (
              fps / rb.getActiveRefreshRate ()
            ) >= 2.0;

            static bool        bWasAboveRefresh = bIsAboveRefresh;
            if (std::exchange (bWasAboveRefresh,  bIsAboveRefresh) !=
                                                  bIsAboveRefresh)
            {
              _RevertACTION (true);
            }

            bool bIsTrueFullscreen =
               rb.isTrueFullscreen ();

            static bool        bWasTrueFullscreen = bIsTrueFullscreen;
            if (std::exchange (bWasTrueFullscreen,  bIsTrueFullscreen) !=
                                                    bIsTrueFullscreen)
            {
              _RevertACTION (true);
            }

            bool bHighVariation = [&]() -> bool
            {
              bool bIgnoreHighVariation =
                bIsTearingModeAdaptiveOn ||
                bIsUnstableFPS           ||
                bIsVRR;

              if (! bIsAboveRefresh)
              {
                bIgnoreHighVariation |=
                  config.render.framerate.enforcement_policy == 2 ||
                  config.nvidia.reflex.use_limiter                ||
                  config.fps.timing_method ==
                    SK_FrametimeMeasures_NewFrameBegin;
              }

              if (bIgnoreHighVariation)
              {
                if (iACTION == ACTION_HighVariation)
                {
                  _RevertACTION (true);
                }

                return false;
              }

              float min = FLT_MAX,
                    max = 0.0000f;

              for ( const auto& val : SK_ImGui_Frames->getValues () )
              {
                if (val > max)
                {
                  max = val;
                }

                if (val < min)
                {
                  min = val;
                }
              }

              return (double)max - (double)min > 1.0;
            }();

            bool bIsPreRenderLimit1 =
              config.render.framerate.pre_render_limit == 1;

            bool bHighRenderLatency = [&]() -> bool
            {
              bool bIgnoreHighRenderLatency =
                bIsTearingModeAdaptiveOn ||
                bIsTearingModeAlwaysOff  ||
                bIsUnstableFPS;

              if (bIgnoreHighRenderLatency)
              {
                if (iACTION == ACTION_HighRenderLatency)
                {
                  _RevertACTION (true);
                }

                return false;
              }

              if (! SK_RenderBackend_V2::latency.stale)
              {
                UINT iRenderLatency =
                  SK_RenderBackend_V2::latency.delays.PresentQueue;

                if (! (bIsAboveRefresh && !bIsPreRenderLimit1))
                {
                  return iRenderLatency > 1;
                }

                else
                {
                  static UINT iTargetRenderLatency = 2;

                  static double dMaxSeconds = 1.5,
                                   dSeconds =
                                dMaxSeconds;

                  if (bRecoveredIndependentFlip)
                  {
                    dSeconds = dMaxSeconds;
                  }

                  bool bIsHighRenderLatency =
                    iACTION == ACTION_HighRenderLatency;

                  static bool        bWasHighRenderLatency = bIsHighRenderLatency;
                  if (std::exchange (bWasHighRenderLatency,  bIsHighRenderLatency) &&
                                                            !bIsHighRenderLatency)
                  {
                    if (! bRecoveredIndependentFlip)
                    {
                      dSeconds = 0.0;
                    }
                  }

                  if (! bIsHighRenderLatency)
                  {
                    // Render Latency must be different every frame in 2x.. non-tearing mode
                    // Otherwise, framepacing gets worse for some reason :D
                    static UINT        iLastRenderLatency = iRenderLatency;
                    if (std::exchange (iLastRenderLatency,  iRenderLatency) ==
                                                            iRenderLatency)
                    {
                      if (! (bRecoveredIndependentFlip || dSeconds < dMaxSeconds))
                      {
                        return true;
                      }
                    }

                    if (dSeconds >= dMaxSeconds)
                    {
                      if (iRenderLatency < iTargetRenderLatency)
                      {
                        dSeconds += _FrametimeSeconds ();

                        if (dSeconds > dMaxSeconds + 0.5)
                        {
                          dSeconds = 0.0;
                        }
                      }

                      else
                      {
                        dSeconds = dMaxSeconds;
                      }
                    }

                    if (dSeconds < dMaxSeconds)
                    {
                      if (dSeconds < 0.5)
                      {
                        if (dSeconds == 0.0)
                        {
                          iTargetRenderLatency = 2;
                        }

                        iTargetRenderLatency = std::max (
                          iTargetRenderLatency,
                          iRenderLatency
                        );
                      }

                      dSeconds = std::min (
                        _FrametimeSeconds () +
                           dSeconds,
                        dMaxSeconds
                      );

                      return false;
                    }
                  }

                  return iRenderLatency > iTargetRenderLatency;
                }
              }

              if (rb.presentation.avg_stats.display != 0.0)
              {
                return
                  rb.presentation.avg_stats.latency /
                  rb.presentation.avg_stats.display > 1.64;
              }

              return false;
            }();

            bool bStuckInputLatency = [&]() -> bool
            {
              bool bIgnoreStuckInputLatency =
                ( bIsTearingModeAdaptiveOff &&
                  bIsUnstableFPS             ) ||
                ( bIsTearingModeAdaptiveOn   ) ||
                ( bIsVRR                     );

              if (bIgnoreStuckInputLatency)
              {
                if (iACTION == ACTION_StuckInputLatency)
                {
                  _RevertACTION (true);
                }

                return false;
              }

              return
                latency_avg.getMaxInput () > -0.1 &&
                latency_avg.getMaxInput () <  0.1;
            }();

            bool bFrameBecameStable = [&]() -> bool
            {
              bool bIgnoreFrameBecameStable =
                bIsTearingModeAlwaysOffLL ||
                bIsTearingModeAlwaysOff   ||
               !bIsTrueFullscreen;

              if (bIgnoreFrameBecameStable)
              {
                if (iACTION == ACTION_FrameBecameStable)
                {
                  _RevertACTION (true);
                }

                return false;
              }

              static bool        bWasUnstableFPS = bIsUnstableFPS;
              if (std::exchange (bWasUnstableFPS,  bIsUnstableFPS) &&
                                                  !bIsUnstableFPS)
              {
                return true;
              }

              if (iACTION == ACTION_FrameBecameStable)
              {
                bWasUnstableFPS = false;
              }

              return iACTION == ACTION_FrameBecameStable;
            }();

            // Sorted by priority
            int iACTIONS [] = {
              ACTION_FrameBecameStable,
              ACTION_HighRenderLatency,
              ACTION_StuckInputLatency,
              ACTION_HighVariation
            };

            if (! bIsAboveRefresh)
            {
              switch (iACTION)
              {
                case ACTION_StuckInputLatency:
                  iACTIONS [1] = ACTION_StuckInputLatency;
                  iACTIONS [2] = ACTION_HighRenderLatency;
                  iACTIONS [3] = ACTION_HighVariation;
                  break;
                case ACTION_HighVariation:
                  iACTIONS [1] = ACTION_HighVariation;
                  iACTIONS [2] = ACTION_HighRenderLatency;
                  iACTIONS [3] = ACTION_StuckInputLatency;
                  break;
                default:
                  break;
              }
            }

            else if ( ( bIsTearingModeAdaptiveOff &&
                        bIsPreRenderLimit1         ) ||
                      ( iACTION ==
                         ACTION_HighVariation      ) )
            {
              iACTIONS [2] = ACTION_HighVariation;
              iACTIONS [3] = ACTION_StuckInputLatency;
            }

            for ( const auto& vACTION : iACTIONS )
            {
              double dMaxWaitSeconds = 1.5;

              bool bIsNewACTION = false,
                   bAbortACTION = false;

              switch (vACTION)
              {
                case ACTION_HighVariation:
                {
                  if (iACTION != ACTION_HighVariation)
                  {
                    if (bHighVariation)
                    {
                      bIsNewACTION = true;
                           iACTION =
                            ACTION_HighVariation;
                    }
                  }

                  if (iACTION == ACTION_HighVariation)
                  {
                    double dMultiplier = std::round (
                      SK_ImGui_Frames->getCapacity () / fps
                    );

                    if (dMultiplier >= 1.0)
                    {
                      dMaxWaitSeconds = dMultiplier * 2.0;
                    }

                    bool bFirstTry =
                      bIsNewACTION;

                    bool bStopWait =
                      bIsTearingModeAdaptiveOff &&
                      bIsPreRenderLimit1        &&
                      bIsAboveRefresh;

                    if (! bIsNewACTION)
                    {
                      if (! bHighVariation)
                      {
                        bAbortACTION = true;
                        break;
                      }

                      if (! bStopWait)
                      {
                        if (dWaitSeconds < 0.0)
                        {
                          if (std::abs (dWaitSeconds) >= dMaxWaitSeconds / 1.5)
                          {
                            bIsNewACTION = true;
                          }

                          else
                          {
                            dWaitSeconds -= _FrametimeSeconds ();
                          }
                        }

                        else
                        {
                          bStopWait |=
                            bIsTearingModeAdaptiveOff &&
                           !bIsTrueFullscreen         &&
                            dWaitSeconds >=
                            dMaxWaitSeconds;

                          if (! bStopWait)
                          {
                            if ( dWaitSeconds >=
                                 dMaxWaitSeconds )
                            {
                              bIsNewACTION = true;
                            }

                            else
                            {
                              dWaitSeconds += _FrametimeSeconds ();
                            }
                          }
                        }
                      }
                    }

                    if (bIsNewACTION)
                    {
                      _RevertACTION ();

                      if (! bStopWait)
                      {
                        if (bFirstTry)
                        {
                          dWaitSeconds = -_FrametimeSeconds ();
                        }

                        else
                        {
                          reset (true);
                        }
                      }
                    }

                    if ( bStopWait &&
                         dWaitSeconds <
                         dMaxWaitSeconds )
                    {
                      dWaitSeconds = dMaxWaitSeconds;
                    }

                    _EnableTearing (
                      bStopWait
                    );

                    return;
                  }
                } break;

                case ACTION_HighRenderLatency:
                {
                  if (iACTION != ACTION_HighRenderLatency)
                  {
                    if (bHighRenderLatency)
                    {
                      bIsNewACTION = true;
                           iACTION =
                            ACTION_HighRenderLatency;
                    }
                  }

                  if (iACTION == ACTION_HighRenderLatency)
                  {
                    bool bFirstTry =
                      bIsNewACTION;

                    bool bDontTear =
                      bIsTearingModeAlwaysOffLL ||
                      bIsTrueFullscreen;

                    bool bStopWait =
                      bIsAboveRefresh &&
                     !bDontTear;

                    if (! bIsNewACTION)
                    {
                      if (__target_fps_temp > 0.0f)
                      {
                        bHighRenderLatency = true;
                      }

                      else if (bIsAboveRefresh)
                      {
                        if (bDontTear)
                        {
                          if ( dWaitSeconds >=
                               dMaxWaitSeconds )
                          {
                            bHighRenderLatency = false;
                          }
                        }

                        else if (dWaitSeconds < dMaxWaitSeconds + 0.05)
                        {
                          dWaitSeconds += _FrametimeSeconds ();

                          bHighRenderLatency = true;
                        }
                      }

                      if (! bHighRenderLatency)
                      {
                        bAbortACTION = true;
                        break;
                      }

                      if (! bStopWait)
                      {
                        if (dWaitSeconds < 0.0)
                        {
                          if (std::abs (dWaitSeconds) >= 1.0)
                          {
                            bIsNewACTION = true;
                          }

                          else
                          {
                            dWaitSeconds -= _FrametimeSeconds ();
                          }
                        }

                        else
                        {
                          if (bDontTear)
                          {
                            if (__target_fps_temp >= __target_fps)
                            {
                              bAbortACTION = true;
                              break;
                            }

                            if ( __target_fps_temp > 0.0f &&
                                      dWaitSeconds >=
                                      dMaxWaitSeconds     )
                            {
                              _RevertACTION ();
                            }

                            else
                            {
                              if ( dWaitSeconds >=
                                   dMaxWaitSeconds )
                              {
                                bIsNewACTION = true;
                              }

                              else
                              {
                                dWaitSeconds += _FrametimeSeconds ();
                              }
                            }
                          }

                          else
                          {
                            bStopWait |=
                              dWaitSeconds >=
                              dMaxWaitSeconds;

                            if (! bStopWait)
                            {
                              dWaitSeconds += _FrametimeSeconds ();
                            }
                          }
                        }
                      }
                    }

                    if (bIsNewACTION)
                    {
                      _RevertACTION ();

                      if (! bStopWait)
                      {
                        // Avoid rapid Render Latency changes
                        if (bFirstTry && ! (SK_RenderBackend_V2::latency.stale || bIsAboveRefresh))
                        {
                          dWaitSeconds = -_FrametimeSeconds ();
                        }

                        else if (bDontTear)
                        {
                          __target_fps_temp =
                          __target_fps - 2.0f;
                        }
                      }
                    }

                    if ( bStopWait &&
                         dWaitSeconds <
                         dMaxWaitSeconds )
                    {
                      dWaitSeconds = dMaxWaitSeconds;
                    }

                    if (dWaitSeconds < 0.0)
                    {
                      bDontTear = true;
                    }

                    _EnableTearing (
                      ! bDontTear && (
                        bIsNewACTION ||
                        bStopWait
                      )
                    );

                    return;
                  }
                } break;

                case ACTION_StuckInputLatency:
                {
                  if (iACTION != ACTION_StuckInputLatency)
                  {
                    if (bStuckInputLatency)
                    {
                      bIsNewACTION = true;
                           iACTION =
                            ACTION_StuckInputLatency;
                    }
                  }

                  if (iACTION == ACTION_StuckInputLatency)
                  {
                    bool bFirstTry =
                      bIsNewACTION;

                    bool bStopWait = false;

                    if (! bIsNewACTION)
                    {
                      if (! bStuckInputLatency)
                      {
                        bAbortACTION = true;
                        break;
                      }

                      if (! bStopWait)
                      {
                        if (dWaitSeconds < 0.0)
                        {
                          if (std::abs (dWaitSeconds) >= 1.0)
                          {
                            bIsNewACTION = true;
                          }

                          else
                          {
                            dWaitSeconds -= _FrametimeSeconds ();
                          }
                        }

                        else
                        {
                          bStopWait |=
                            bIsTearingModeAdaptiveOff &&
                           !bIsTrueFullscreen         &&
                            dWaitSeconds >=
                            dMaxWaitSeconds;

                          if (! bStopWait)
                          {
                            if ( dWaitSeconds >=
                                 dMaxWaitSeconds )
                            {
                              bIsNewACTION = true;
                            }

                            else
                            {
                              dWaitSeconds += _FrametimeSeconds ();
                            }
                          }
                        }
                      }
                    }

                    if (bIsNewACTION)
                    {
                      _RevertACTION ();

                      if (! bStopWait)
                      {
                        if (bFirstTry)
                        {
                          dWaitSeconds = -_FrametimeSeconds ();
                        }

                        else
                        {
                          reset (true);
                        }
                      }
                    }

                    if ( bStopWait &&
                         dWaitSeconds <
                         dMaxWaitSeconds )
                    {
                      dWaitSeconds = dMaxWaitSeconds;
                    }

                    _EnableTearing (
                      bStopWait
                    );

                    return;
                  }
                } break;

                case ACTION_FrameBecameStable:
                {
                  if (iACTION != ACTION_FrameBecameStable)
                  {
                    if (bFrameBecameStable)
                    {
                      bIsNewACTION = true;
                           iACTION =
                            ACTION_FrameBecameStable;
                    }
                  }

                  if (iACTION == ACTION_FrameBecameStable)
                  {
                    if (! bIsNewACTION)
                    {
                      if ( dWaitSeconds >=
                           dMaxWaitSeconds )
                      {
                        bAbortACTION = true;
                        break;
                      }

                      else
                      {
                        dWaitSeconds += _FrametimeSeconds ();
                      }
                    }

                    else
                    {
                      _RevertACTION ();
                    }

                    _EnableTearing (
                      bIsTearingModeAdaptiveOn
                    );

                    return;
                  }
                } break;

                default:
                {
                } break;
              }

              if (bAbortACTION)
              {
                break;
              }
            }
          }

          if (iACTION == ACTION_FrameBecameStable)
          {
            _EnableTearing (
              bIsTearingModeAdaptiveOn
            );
          }

          else
          {
            _EnableTearing (
              ( bIsTearingModeAdaptiveOff &&  bIsUnstableFPS ) ||
              ( bIsTearingModeAdaptiveOn  && !bIsUnstableFPS )
            );
          }
        }

        default:
        {
          _RevertACTION (true);

          switch (iTearingMode)
          {
            case SK_TearingMode::AlwaysOn:
            {
              _EnableTearing ();
            } break;

            default:
            {
            } break;
          }
        } break;
      }
    };

    _ManageTearing ();

    if (config.render.framerate.present_interval == 0 && ticks_per_scanline > 1)
    {
      __SK_LatentSyncSkip = static_cast <int> (
        std::round (
          fps / rb.getActiveRefreshRate ()
        )
      );

      bStopFrameSkip |=
        (! SK_LatentSync_AllowFrameSkip ());

      if (__SK_LatentSyncSkip < 2 || bStopFrameSkip)
      {
        __SK_LatentSyncSkip = 0;
      }

      if (__scanline.lock.isPending ())
      {
        if (                        __SK_LatentSyncSkip  == 0 ||
            (__SK_LatentSyncFrame % __SK_LatentSyncSkip) != 0)
        {    __SK_LatentSyncFrame = __SK_LatentSyncSkip - 1;

          double dTicksPerFrame = 0.0;

          accum_per_frame =
                  modf ( static_cast <double> (SK_PerfFreq) /
                         static_cast <double> (fps),     &dTicksPerFrame );
          ticks_per_frame  = sk::narrow_cast <ULONGLONG> (dTicksPerFrame);
          ticks_per_frame +=
            ( ticks_per_frame / ticks_per_refresh ) * config.render.framerate.latent_sync.scanline_error;

          __SK_LatentSync_FrameInterval = ticks_per_frame;

          // Update historical frametimes before restarting limiter
          frames_of_fame.frames_measured.first.initClock  (__scanline.qpc_t0.QuadPart + __scanline.qpc_tL.QuadPart);
          frames_of_fame.frames_measured.last.clock_val  = __scanline.qpc_t0.QuadPart + __scanline.qpc_tL.QuadPart;
          frames_of_fame.frames_measured.first.initFrame  (frames);
          frames_of_fame.frames_measured.last.frame_idx += frames;

          // Re-sync would slide backwards and allow the limiter to run fast if we didn't handle this
          while (__scanline.qpc_t0.QuadPart < next_ - ((ticks_per_refresh / 20) * 3)) // Allow moving the target back 15%, no more
                 __scanline.qpc_t0.QuadPart +=          ticks_per_refresh;


          LONGLONG llDelta =
               (static_cast <LONGLONG> (pDisplay->signal.timing.total_size.cy) - __scanline.lock.target);

          if (__scanline.lock.target < (pDisplay->signal.timing.total_size.cy / 2))
          {
            llDelta =
              -__scanline.lock.target;
          }

          __scanline.qpc_t0.QuadPart +=
              llDelta * ticks_per_scanline;


          // Restart the limiter because we would drift otherwise
          WriteRelease64 ( &frames, 0                               );
          WriteRelease64 ( &start,  __scanline.qpc_t0.QuadPart + __scanline.qpc_tL.QuadPart );
          WriteRelease64 ( &next,   __scanline.qpc_t0.QuadPart + __scanline.qpc_tL.QuadPart );

          next_                   = __scanline.qpc_t0.QuadPart + __scanline.qpc_tL.QuadPart;


          __SK_LatentSyncPostDelay =
            (config.render.framerate.latent_sync.delay_bias == 0.0f) ? 0
                                                                     :
            static_cast <LONGLONG> (
              static_cast <double> (ticks_per_frame) *
                           config.render.framerate.latent_sync.delay_bias );

          __scanline.lock.acquired = true;
          __scanline.lock.resetSignals ();
        }
      }

      else if (config.render.framerate.latent_sync.auto_bias)
      {
        static constexpr int _MAX_FRAMES = 6;

        struct {
          double input   [_MAX_FRAMES] = { };
          double display [_MAX_FRAMES] = { };

          int frames = 0;

          double getInput (void) noexcept
          {
            double avg     = 0.0,
                   samples = 0.0;

            for (int i = 0; i < std::min (frames, _MAX_FRAMES); ++i)
            {
              ++samples; avg += input [i];
            }

            return
              ( avg / samples );
          }

          double getDisplay (void) noexcept
          {
            double avg     = 0.0,
                   samples = 0.0;

            for (int i = 0; i < std::min (frames, _MAX_FRAMES); ++i)
            {
              ++samples; avg += display [i];
            }

            return
              ( avg / samples );
          }
        } static latency_avg;

        latency_avg.input   [latency_avg.frames   % _MAX_FRAMES] =
          (1000.0 / get_limit           ()) -
                    effective_frametime ();
        latency_avg.display [latency_avg.frames++ % _MAX_FRAMES] =
                    effective_frametime ();

        float delta = 0.0f;

        float auto_bias_target_ms      =
          config.render.framerate.latent_sync.auto_bias_target.ms;

        float auto_bias_target_percent =
          config.render.framerate.latent_sync.auto_bias_target.percent;

        if (auto_bias_target_percent > 0.0f)
        {
          auto_bias_target_ms = static_cast <float> (
            (latency_avg.getInput () + latency_avg.getDisplay ()) * (1.0f - auto_bias_target_percent)
          );
        }

        if (latency_avg.getInput () > (auto_bias_target_ms * 1.05f))
        {
          delta = (float)effective_frametime () * SK_LatentSyncDeltaMultiplier;

          config.render.framerate.latent_sync.delay_bias =
            SK_LatentSyncAlpha * config.render.framerate.latent_sync.delay_bias + (1.0f - SK_LatentSyncAlpha) * delta;
        }

        else if (latency_avg.getInput () < (auto_bias_target_ms * .95f))
        {
          delta = (float)effective_frametime () * SK_LatentSyncDeltaMultiplier * SK_LatentSyncBackOffMultiplier;

          config.render.framerate.latent_sync.delay_bias =
            SK_LatentSyncAlpha * config.render.framerate.latent_sync.delay_bias - (1.0f - SK_LatentSyncAlpha) * delta;
          //config.render.framerate.latent_sync.delay_bias -= static_cast <float> (effective_frametime () * 0.000666);
        }

        config.render.framerate.latent_sync.delay_bias = std::clamp (
          config.render.framerate.latent_sync.delay_bias,
          0.0f,
          auto_bias_target_percent <= 0.0f
            ? config.render.framerate.latent_sync.max_auto_bias
            : 1.0f
        );

        __SK_LatentSyncPostDelay =
          (config.render.framerate.latent_sync.delay_bias == 0.0f) ? 0
                                                                   :
          static_cast <LONGLONG> (
            static_cast <double> (get_ticks_per_frame ()) *
                         config.render.framerate.latent_sync.delay_bias );
      }

      __SK_LatentSyncFrame++;

      next_ -=
        (__SK_LatentSyncSwapTime / 2);
    }


    wait_time.beginBusy ();

    while (time_ < next_)
    {
      // SK's Multimedia Class Scheduling Task for this thread prevents
      //   CPU starvation, but if the service is turned off, implement
      //     a fail-safe for very low framerate limits.
      if (! config.render.framerate.enable_mmcss)
      {
        const DWORD dwWaitMS =
          sk::narrow_cast <DWORD> (
            std::max (0.0, SK_RecalcTimeToNextFrame () * 1000.0)
          );

        // This is only practical @ 30 FPS or lower.
        if (dwWaitMS > 4)
          SK_SleepEx (1, FALSE);
      }

      SK_YieldProcessor (next_ - SK_QueryPerf ().QuadPart);

      time_ =
        SK_QueryPerf ().QuadPart;
    }

    wait_time.endBusy ();
  }

  else [[unlikely]]
  {
    SK_LOG0 ( ( L"Framerate limiter lost time?! (non-monotonic clock)" ),
                L"FrameLimit" );
    InterlockedAdd64 (&start, -next_);
  }

  WriteRelease64 (&time, time_);
  WriteRelease64 (&last, time_);

  if (config.render.framerate.present_interval == 0 && ticks_per_scanline > 1)
  {
    // Disable Low-Latency Mode when using Latent Sync
    if (config.render.framerate.enforcement_policy > 0)
    {   config.render.framerate.enforcement_policy = -config.render.framerate.enforcement_policy; }

    if (D3DKMTGetScanLine == nullptr)
        D3DKMTGetScanLine = SK_GetProcAddress (L"gdi32.dll",
       "D3DKMTGetScanLine");

    static bool   bSync = true; // First time signals resync
    static double dSync = 0.00;

    if (config.render.framerate.latent_sync.scanline_resync != 0.0f)
    {
      float fResync =
        config.render.framerate.latent_sync.scanline_resync;

      if ( std::round (fps / rb.getActiveRefreshRate ()) >= 2.0 ||
                             rb.gsync_state.active              )
      {
        fResync *= 10.0f;
      }

      dSync += _FrametimeSeconds ();

      if (static_cast <float> (dSync) >= fResync)
      {
        bSync = true;
      }
    }

    if (D3DKMTGetScanLine != nullptr && bSync)
    {
      __scanline.lock.requestResync ();

      static HANDLE hReSyncThread =
        SK_Thread_CreateEx ([](LPVOID) -> DWORD
        {
          SK_RenderBackend& rb =
            SK_GetCurrentRenderBackend ();

          HANDLE hWaitHandles [] = {
            __scanline.lock.signals.resync, __SK_DLL_TeardownEvent
          };

          while ( WaitForMultipleObjects ( 2, hWaitHandles, FALSE, INFINITE ) == WAIT_OBJECT_0 )
          {
            SK_Thread_SetCurrentPriority (THREAD_PRIORITY_BELOW_NORMAL);

            __scanline.lock.acquired = false;

            auto qpc_start =
              SK_QueryPerf ();

            while (true)
            {
              auto qpc_t0 =
                SK_QueryPerf ();

              std::scoped_lock <std::recursive_mutex>
                  adapter_lock (rb.adapter.lock);

              if (rb.adapter.d3dkmt == 0)
              {   rb.assignOutputFromHWND (SK_GetGameWindow ()); }

              D3DKMT_GETSCANLINE
                getScanLine               = { };
                getScanLine.hAdapter      = rb.adapter.d3dkmt;
                getScanLine.VidPnSourceId = rb.adapter.VidPnSourceId;

              auto *pDisplay =
                &rb.displays [rb.active_display];

              unsigned int           scanlines (pDisplay->signal.timing.total_size.cy);
              LONGLONG     ticks_per_scanline = 0LL;

              if (pDisplay->signal.timing.hsync_freq.Numerator > 0)
              {
                ticks_per_scanline =
                  ( pDisplay->signal.timing.hsync_freq.Denominator * SK_PerfFreq ) /
                    pDisplay->signal.timing.hsync_freq.Numerator;
              }

              if (D3DKMTGetScanLine != nullptr && ticks_per_scanline > 0)
              {                                // Avoid Integer Divide by Zero
                auto qpc_t1 =
                  SK_QueryPerf ();

                SK_Thread_ScopedPriority prio_scope (THREAD_PRIORITY_HIGHEST);

                if ( STATUS_SUCCESS ==
                       ((D3DKMTGetScanLine_pfn)
                         D3DKMTGetScanLine)(&getScanLine) && getScanLine.InVerticalBlank)
                {
                  qpc_t1 =
                    SK_QueryPerf ();

                  UINT scanline_t0 =
                    getScanLine.ScanLine;

                  if ( STATUS_SUCCESS ==
                         ((D3DKMTGetScanLine_pfn)
                           D3DKMTGetScanLine)(&getScanLine) && getScanLine.InVerticalBlank)
                  {
                    auto tReturn =
                      SK_QueryPerf ().QuadPart;

                    static constexpr auto _MARGIN = 16u;

                    if (((tReturn         - qpc_t1.QuadPart) / ticks_per_scanline) <= _MARGIN &&
                        ((qpc_t1.QuadPart - qpc_t0.QuadPart) / ticks_per_scanline) <= _MARGIN)
                    {
                      if (( getScanLine.ScanLine <=                         (_MARGIN/3) ||
                            getScanLine.ScanLine >= scanlines - ((_MARGIN / (_MARGIN/2)) * _MARGIN) ))
                      {
                        if (getScanLine.ScanLine < scanline_t0)
                            getScanLine.ScanLine += scanlines;

                        __scanline.lock.target     = ((scanline_t0 + getScanLine.ScanLine) / 2) % scanlines;
                        __scanline.lock.margin     =  (    tReturn - qpc_t1.QuadPart     ) / 2;
                        __scanline.lock.scan_time  =       tReturn - qpc_start.QuadPart;
                        __scanline.qpc_t0.QuadPart =       tReturn - __scanline.lock.margin;

                        bSync = false;
                        dSync = 0.000;

                        __scanline.lock.notifyAcquired ();

                        SK_Thread_SetCurrentPriority (THREAD_PRIORITY_BELOW_NORMAL);

                        break;
                      }
                    }
                  }

                  LONGLONG
                    llFractionOfRemainingTime = 0;

                  if (! getScanLine.InVerticalBlank)
                  {
                    llFractionOfRemainingTime =
                        ( ( (static_cast <LONGLONG> (pDisplay->signal.timing.active_size.cy) -
                                                              getScanLine.ScanLine) / 7ULL ) * 4ULL ) *
                                                                ticks_per_scanline;
                  }

                  else
                  {
                    llFractionOfRemainingTime =
                        ( ( (static_cast <LONGLONG> (pDisplay->signal.timing.total_size.cy) -
                                                             getScanLine.ScanLine) / 7ULL ) * 3ULL ) *
                                                               ticks_per_scanline;
                  }

                  if (llFractionOfRemainingTime > 0)
                  {
                    DWORD         dwRemainingTimeInMs = sk::narrow_cast <DWORD>
                      ( llFractionOfRemainingTime / SK_PerfTicksPerMs );

                    if (          dwRemainingTimeInMs > 1 && dwRemainingTimeInMs < 50)
                      SK_SleepEx (dwRemainingTimeInMs - 1, FALSE);

                    while (SK_QueryPerf ().QuadPart < qpc_t1.QuadPart + llFractionOfRemainingTime)
                      SK_YieldProcessor ();
                  }
                }
              }

              __scanline.lock.scan_time =
                SK_QueryPerf ().QuadPart - qpc_start.QuadPart;
            };
          }

          SK_Thread_CloseSelf ();

          return 0;
        }, L"[SK] VSYNC Emulation Thread" );
    }
  }

  else
  {
    // Latent Sync -was- on, but now it's off and we need to restore original preference
    if (config.render.framerate.enforcement_policy < 0)
    {   config.render.framerate.enforcement_policy =
       -config.render.framerate.enforcement_policy;
        config.render.framerate.latent_sync
              .show_fcat_bars = false;
    }
  }

  if (! std::exchange (lazy_init, true))
  {
    init (fps, tracks_window);
  }


  SK_FPU_SetControlWord (_MCW_PC, &fpu_cw_orig);
}


double SK_Framerate_GetLimitEnvVar (double target)
{
  static double dEnvVarFPS = 0.0;

  SK_RunOnce (
  {
    wchar_t                                              wszEnvVarFPS [32] = { };
    if (GetEnvironmentVariableW (L"SUNSHINE_CLIENT_FPS", wszEnvVarFPS, 31))
    {
      dEnvVarFPS = _wtof (wszEnvVarFPS);
    }

    else if (GetEnvironmentVariableW (L"SK_FPS_LIMIT", wszEnvVarFPS, 31))
    {
      dEnvVarFPS = _wtof (wszEnvVarFPS);
    }
  });

  if (dEnvVarFPS == 0.0)
    return target;

  return dEnvVarFPS;
}

void
SK::Framerate::Limiter::set_limit (float& target)
{
  // Allow environment variable override
  //
  //   XXX: __target_fps should probably be changed to double
  //
  target = sk::narrow_cast <float> (SK_Framerate_GetLimitEnvVar (target));

  // Skip redundant set_limit calls
  if (fabs (fps - target) < DBL_EPSILON && tracks_window == true)
    return;

  __scanline.lock.requestResync ();
  wait_time.reset               ();

  init (target);
}

double
SK::Framerate::Limiter::effective_frametime (void)
{
  return effective_ms;
}

SK_LazyGlobal <
  concurrency::concurrent_unordered_map < IUnknown *,
           std::unique_ptr <SK::Framerate::Limiter> >
              > SK::Framerate::limiters_;

SK::Framerate::Limiter*
SK_FramerateLimit_Factory ( IUnknown *pSwapChain_,
                            bool      bCreate = true )
{
  // Prefer to reference SwapChains we wrap by their wrapped pointer
  SK_ComQIPtr <IDXGISwapChain> pSwapChain (pSwapChain_);
  SK_ComPtr   <IDXGISwapChain> pUnwrap;

  UINT size = sizeof (LPVOID);

  if (pSwapChain.p != nullptr)
      pSwapChain.p->GetPrivateData (IID_IUnwrappedDXGISwapChain, &size, (void *)&pUnwrap.p);

  if ( pUnwrap != nullptr &&
       pUnwrap != pSwapChain_ )
     pSwapChain_ = pUnwrap;

  SK_RunOnce (
    SK_GetCommandProcessor ()->AddCommand (
      "SK::Framerate::ResetLimit", new skLimitResetCmd ()
    )
  );

  // This is a concurrent map, items cannot be removed, only set to nullptr
  if ((! SK::Framerate::limiters_->count (pSwapChain_)) ||
         SK::Framerate::limiters_->at    (pSwapChain_).get () == nullptr)
  {
    if (bCreate)
    {
      SK::Framerate::limiters_.get()[pSwapChain_] =
        std::make_unique <SK::Framerate::Limiter> (
          config.render.framerate.target_fps
        );

      SK_LOG0 ( ( L" Framerate Limiter Created to Track SwapChain (%ph)",
                                                       pSwapChain_
                ), L"FrameLimit"
              );
    }

    else
      return nullptr;
  }

  return
    SK::Framerate::limiters_->at (pSwapChain_).get ();
}

bool
SK::Framerate::HasLimiter (IUnknown *pSwapChain)
{
  return
    nullptr != SK_FramerateLimit_Factory
               ( pSwapChain,     false );
}

SK::Framerate::Limiter*
SK::Framerate::GetLimiter ( IUnknown *pSwapChain,
                            bool      bCreateIfNoneExists )
{
  return
    SK_FramerateLimit_Factory ( pSwapChain,
                                bCreateIfNoneExists );
}

bool
SK::Framerate::FreeLimiter (IUnknown *pSwapChain)
{
  if (SK::Framerate::limiters_->count (pSwapChain))
  {   SK::Framerate::limiters_->at    (pSwapChain).reset ();

    return true;
  }

  return false;
}

void
SK::Framerate::TickEx ( bool       /*wait*/,
                        double        dt,
                        LARGE_INTEGER now,
                        IUnknown*     swapchain )
{
  auto *pLimiter =
    SK::Framerate::GetLimiter (swapchain);

  SK_ReleaseAssert (pLimiter != nullptr);

  // Should never happen, but better safe.
  if (pLimiter == nullptr)
    return;

  auto& snapshots =
    pLimiter->frame_history_snapshots;

  if (! snapshots.isAllocated () )
  {
    // Late initialization
    Init ();
  }

  if (now.QuadPart == 0)
      now = SK_CurrentPerf ();

  if (dt + 0.0000001 <= 0.0000001)
      dt =
    static_cast <double> (now.QuadPart -
                  pLimiter->amortization._last_frame.QuadPart) /
    static_cast <double> (SK_PerfFreq);


  // Prevent inserting infinity into the dataset
  if ( std::isnormal (dt) )
  {
    if (snapshots->frame_history.addSample (1000.0 * dt, now))
    {
      pLimiter->amortization.phase = 0;
    }

    snapshots->frame_history2.addSample (
      pLimiter->effective_frametime (),
        now
    );

    static ULONG64 last_frame         = 0;
    bool           skip_frame_history = false;

    if (last_frame < SK_GetFramesDrawn () - 1)
    {
      skip_frame_history = true;
    }

    if (std::exchange (last_frame, SK_GetFramesDrawn ())
                                != SK_GetFramesDrawn ())
    {
      if (!   (reset_frame_history ||
                skip_frame_history) ) SK_ImGui_Frames->timeFrame (dt);
      else if (reset_frame_history)   SK_ImGui_Frames->reset     (  );
    }
  }

  static constexpr int _NUM_STATS = 5;

  enum class StatType {
    Mean             = 0,
    Min              = 1,
    Max              = 2,
    PercentileClass0 = 3,
    PercentileClass1 = 4,
  };

  if (pLimiter->amortization.phase < _NUM_STATS)
  {
    static constexpr LARGE_INTEGER
      all_samples = { 0UL, 0UL };

    SK::Framerate::Stats*
      pContainers [] =
      {
        &snapshots->mean,
        &snapshots->min,
        &snapshots->max,
        &snapshots->percentile0,
        &snapshots->percentile1
      };

    auto stat_idx =
      pLimiter->amortization.phase++;

    auto* container =
      pContainers [stat_idx];

    double sample = 0.0;

    switch (static_cast <StatType> (stat_idx))
    {
      case StatType::PercentileClass0:
      case StatType::PercentileClass1:
      {
        const int idx =
          ( static_cast <StatType> (stat_idx) ==
                         StatType::PercentileClass1 ) ?
                                                    1 : 0;

        sample =
          snapshots->frame_history.calcPercentile (
            SK_Framerate_GetPercentileByIdx (idx),
              all_samples
          );
      } break;

      case StatType::Mean:
      case StatType::Min:
      case StatType::Max:
      {
        using CalcSample_pfn =
          double ( SK::Framerate::Stats::* )
                    ( LARGE_INTEGER );

        static constexpr
          CalcSample_pfn
            FrameHistoryCalcSample_FnTbl [] =
            {
              &SK::Framerate::Stats::calcMean,
              &SK::Framerate::Stats::calcMin,
              &SK::Framerate::Stats::calcMax
            };

        auto calcSample =
          std::bind (
            FrameHistoryCalcSample_FnTbl [stat_idx],
              &snapshots->frame_history,
                std::placeholders::_1
          );

        sample =
          calcSample (all_samples);
      } break;
    }

    if (std::isnormal (sample) && now.QuadPart > 0)
    {
      container->addSample (
        sample, now
      );
    }
  }

  pLimiter->amortization._last_frame = now;
}

void
SK::Framerate::Tick ( bool          wait,
                      double        dt,
                      LARGE_INTEGER now,
                      IUnknown*     swapchain )
{
  auto *pLimiter =
    SK::Framerate::GetLimiter (swapchain);

  SK_ReleaseAssert (pLimiter != nullptr);

  if (__target_fps_temp > 0.0f && __target_fps <= 0.0f)
  {
    __target_fps_temp = 0.0f;
  }

  // Should never happen, but better safe.
  if (pLimiter == nullptr)
    return;

  auto& snapshots =
    pLimiter->frame_history_snapshots;

  if (! snapshots.isAllocated () )
  {
    // Late initialization
    Init ();
  }

  if (wait)
    pLimiter->wait ();

  if (config.fps.timing_method == SK_FrametimeMeasures_LimiterPacing && __target_fps > 0.0f)
  {
    SK::Framerate::TickEx (false, dt, now, swapchain);
  }

  else
  {
    std::ignore = dt;
    std::ignore = now;
    std::ignore = swapchain;
  }
};


double
SK::Framerate::Stats::calcMean (double seconds) noexcept
{
  return
    calcMean (SK_DeltaPerf (seconds, SK_PerfFreq));
}

double
SK::Framerate::Stats::calcSqStdDev (double mean, double seconds) noexcept
{
  return
    calcSqStdDev (mean, SK_DeltaPerf (seconds, SK_PerfFreq));
}

double
SK::Framerate::Stats::calcMin (double seconds) noexcept
{
  return
    calcMin (SK_DeltaPerf (seconds, SK_PerfFreq));
}

double
SK::Framerate::Stats::calcMax (double seconds) noexcept
{
  return
    calcMax (SK_DeltaPerf (seconds, SK_PerfFreq));
}

double
SK::Framerate::Stats::calcOnePercentLow (double seconds)
{
  return
    calcOnePercentLow (SK_DeltaPerf (seconds, SK_PerfFreq));
}

double
SK::Framerate::Stats::calcPointOnePercentLow (double seconds)
{
  return
    calcPointOnePercentLow (SK_DeltaPerf (seconds, SK_PerfFreq));
}

int
SK::Framerate::Stats::calcHitches ( double tolerance,
                                    double mean,
                                    double seconds ) noexcept
{
  return
    calcHitches (tolerance, mean, SK_DeltaPerf (seconds, SK_PerfFreq));
}

int
SK::Framerate::Stats::calcNumSamples (double seconds)
{
  return
    calcNumSamples (SK_DeltaPerf (seconds, SK_PerfFreq));
}

void
SK::Framerate::DeepFrameState::reset (void)
{
  auto _clear =
    [&](SK::Framerate::Stats* pStats, auto idx) ->
    void
    {
      pStats->data [idx].when = LARGE_INTEGER { 0LL, 0L };
      pStats->data [idx].val  = 0.0;
    };

  for ( auto i = 0 ; i < MAX_SAMPLES ; ++i )
  {
    _clear (&mean,        i);
    _clear (&min,         i);
    _clear (&max,         i);
    _clear (&percentile0, i);
    _clear (&percentile1, i);
  }

  mean.samples        = 0;
  min.samples         = 0;
  max.samples         = 0;
  percentile0.samples = 0;
  percentile1.samples = 0;
}


std::vector <double>&
SK::Framerate::Stats::sortAndCacheFrametimeHistory (void) //noexcept
{
#pragma warning (push)
#pragma warning (disable: 4244)
  if (! InterlockedCompareExchange (&worker._init, 1, 0))
  {
    worker.hSignalProduce.m_h =
      SK_CreateEvent (nullptr, FALSE, FALSE, nullptr);

    worker.hSignalConsume.m_h =
      SK_CreateEvent (nullptr, FALSE, TRUE, nullptr);

    worker.hSignalShutdown.m_h =
      SK_CreateEvent (nullptr, TRUE, FALSE, nullptr);

    SK_Thread_CreateEx ([](LPVOID lpUser)->DWORD
    {
      SK_Thread_SetCurrentPriority (THREAD_PRIORITY_BELOW_NORMAL);

      auto pWorker =
        static_cast <worker_context_s *> (lpUser);

      HANDLE worker_events [] = {
        pWorker->hSignalProduce.m_h,
        pWorker->hSignalShutdown.m_h
      };

      while ( WAIT_OBJECT_0 ==
                WaitForMultipleObjects ( 2, worker_events, FALSE, INFINITE ) )
      {
        LONG work_idx =
          ReadAcquire (&pWorker->work_idx);

        auto& kSortBuffer =
          pWorker->sorted_frame_history [work_idx];

        boost::sort::pdqsort (
          kSortBuffer.second.begin (),
          kSortBuffer.second.end   (), std::greater <> ()
        );

        kSortBuffer.first =
          SK_GetFramesDrawn ();

        InterlockedExchange (&pWorker->work_idx, work_idx ? 0 : 1);

        SetEvent (pWorker->hSignalConsume.m_h);
      }

      SK_Thread_CloseSelf ();

      return 0;
    }, L"[SK] Framepacing Statistics", (LPVOID)&worker);
  }

  HANDLE worker_events [] = {
    worker.hSignalConsume.m_h,
    worker.hSignalShutdown.m_h
  };

  auto& kReadBuffer =
    worker.sorted_frame_history [
      ReadAcquire (&worker.work_idx) ? 0 : 1
    ];

  if ( WAIT_OBJECT_0 ==
         WaitForMultipleObjects ( 2, worker_events, FALSE, 0 ) )
  {
    LONG idx =
      ReadAcquire (&worker.work_idx);

    auto& kWriteBuffer =
      worker.sorted_frame_history [idx];
          kReadBuffer  =
      worker.sorted_frame_history [idx ? 0 : 1];

    kWriteBuffer.second.clear ();
    kWriteBuffer.first = SK_GetFramesDrawn ();

    for (const auto& datum : data)
    {
      if (datum.when.QuadPart >= 0)
      {
        if (isnormal (datum.val))
        {
          kWriteBuffer.second.emplace_back (datum.val);
        }
      }
    }

    worker.ulLastFrame = SK_GetFramesDrawn ();

    SetEvent (worker.hSignalProduce);
  }
#pragma warning (pop)

  return
    kReadBuffer.second;
}

double SK::Framerate::Limiter::timer_res_ms = 15.0;







void
SK_Framerate_WaitUntilQPC (LONGLONG llQPC, HANDLE& hTimer)
{
  if (llQPC < SK_QueryPerf ().QuadPart)
    return;

  if ((LONG_PTR)hTimer < 0)
  {             hTimer = SK_HasHighResWaitableTimer ?
    CreateWaitableTimerEx ( nullptr, nullptr,
       CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS ) :
    CreateWaitableTimer   ( nullptr, FALSE, nullptr );
  }

  double
    to_next_in_secs =
      static_cast <double> (llQPC - SK_QueryPerf ().QuadPart) /
      static_cast <double> (SK_PerfFreq);

  constexpr
    double duS = (1000.0 * 10000.0);

  // First use a kernel-waitable timer to scrub off most of the
  //   wait time without completely gobbling up a CPU core.
  if ( (LONG_PTR)hTimer > 0 && (to_next_in_secs * 1000.0 >= SK::Framerate::Limiter::timer_res_ms * 2.875) )
  {
    // Schedule the wait period just shy of the timer resolution determined
    //   by NtQueryTimerResolution (...). Excess wait time will be handled by
    //     spinning, because the OS scheduler is not accurate enough.
    LARGE_INTEGER
      liDelay
        { .QuadPart =
            std::min (
              static_cast <LONGLONG> (
                to_next_in_secs * 1000.0 - SK::Framerate::Limiter::timer_res_ms * fSwapWaitRatio
                                     ),
              static_cast <LONGLONG> (
                to_next_in_secs * 1000.0 * fSwapWaitFract
                                     )
                     )
        };

      liDelay.QuadPart =
    -(liDelay.QuadPart * 10000LL);

    // Check if the next frame is sooner than waitable timer resolution before
    //   rescheduling this thread.
    if ( SK_HasHighResWaitableTimer ?
           SetWaitableTimerEx ( hTimer, &liDelay,
                                          0, nullptr, nullptr,
                                             nullptr, 0 )
         : SetWaitableTimer   ( hTimer, &liDelay,
                                          0, nullptr, nullptr,
                                             FALSE ) )
    { 
      if (static_cast <double> (-liDelay.QuadPart) / 10000.0 > SK::Framerate::Limiter::timer_res_ms * 2.0)
      {
        to_next_in_secs =
          static_cast <double> (llQPC - SK_QueryPerf ().QuadPart) /
          static_cast <double> (SK_PerfFreq);

        liDelay.QuadPart =
          -(static_cast <LONGLONG> (to_next_in_secs * duS));

        wait_time.beginSleep ();

        DWORD dwWait =
          SK_WaitForSingleObject_Micro (hTimer, &liDelay);

        wait_time.endSleep ();

        if ( dwWait != WAIT_OBJECT_0     &&
             dwWait != WAIT_OBJECT_0 + 1 &&
             dwWait != WAIT_TIMEOUT ) [[unlikely]]
        {
          const DWORD dwLastError =
            GetLastError ();

          dll_log->Log (L"[SK_Framerate_WaitUntilQPC] Result of WaitForSingleObject:"
                        L" %x (GetLastError: %x)", dwWait, dwLastError);
        }
      }
    }
  }

  wait_time.beginBusy ();

  while (SK_QueryPerf ().QuadPart < llQPC)
    SK_YieldProcessor (llQPC - SK_QueryPerf ().QuadPart);

  wait_time.endBusy ();
}

double
SK::Framerate::Limiter::get_ms_to_next_tick (float ticks) noexcept
{
  return
    std::max ( 0.0,
                 static_cast <double> (next - SK_QueryPerf ().QuadPart) /
                 static_cast <double> (SK_PerfTicksPerMs) +
                (static_cast <double> (ticks_per_frame) /
                 static_cast <double> (SK_PerfTicksPerMs)) * (std::max (1.0, static_cast <double> (ticks)) - 1.0) );
}



void
SK_Framerate_EnergyControlPanel (void)
{
  if (! SK_CPU_IsZen ())
    return;

  if (! SK_CPU_HasMWAITX)
    return;

  ImGui::Separator  ();

  const bool bNodeOpen =
    ImGui::TreeNodeEx ("Energy Efficiency", ImGuiTreeNodeFlags_None/*ImGuiTreeNodeFlags_DefaultOpen*/);

  if (bNodeOpen)
  {
    bool bUseAMDWAITX =
      config.render.framerate.use_amd_mwaitx;

    if (ImGui::Checkbox ("Use AMD MWAITX Instructions", &bUseAMDWAITX))
    {
      config.render.framerate.use_amd_mwaitx = bUseAMDWAITX;
    }

    if (ImGui::IsItemHovered ())
    {
      ImGui::BeginTooltip ();
      ImGui::Text         ("Power Saving Feature For AMD CPUs");
      ImGui::Separator    ();
      ImGui::BulletText   ("Uses a more efficient busy-wait that increases frametime variance but reduces power.");
      ImGui::BulletText   ("Only affects busy-wait power consumption and has neglibile impact on CPU load%.");
      ImGui::Separator    ();
      ImGui::Text         (ICON_FA_INFO_CIRCLE " For best results, install Special K's Sensor Driver using SKIF.");
      ImGui::EndTooltip   ();
    }

    // Run the CPU statistics
    SK_ImGui_Widgets->cpu_monitor->setActive (true);
    SK_RunOnce (SK_ImGui_Widgets->cpu_monitor->draw ());

    extern void SK_ImGui_DrawCPUTemperature (void);
    extern void SK_ImGui_DrawCPUPower       (void);

    SK_ImGui_DrawCPUTemperature ();
    SK_ImGui_DrawCPUPower       ();

#if 0
    bool bAdvanced =
      ImGui::CollapsingHeader ("Advanced");

    if (ImGui::IsItemHovered ())
      ImGui::SetTooltip ("None of these settings are saved, they are all for temporary testing.");

    if (bAdvanced)
    {
      if (ImGui::Button ("Traditional Busy-Wait"))
      {
        fSwapWaitRatio = 3.33f;
        fSwapWaitFract = 0.66f;
      }

      ImGui::SameLine ();

      if (ImGui::Button ("Relaxed Busy-Wait"))
      {
        fSwapWaitRatio = 0.32f;
        fSwapWaitFract = 0.91f;
      }

      ImGui::SliderFloat ("Timer Resolution Bias", &fSwapWaitRatio, 0.0f, 5.0f);
      ImGui::SliderFloat ("Sleep Threshold",       &fSwapWaitFract, 0.0f, 1.0f);
    }
#endif

    ImGui::TreePop    ();
  }
}