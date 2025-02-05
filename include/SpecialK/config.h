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
#ifndef __SK__CONFIG_H__
#define __SK__CONFIG_H__

#include <Unknwnbase.h>

#include <Windows.h>
#include <powerbase.h>
#include <string>
#include <set>
#include <unordered_set>
#include <concurrent_unordered_set.h>
#include <concurrent_unordered_map.h>
#include <filesystem>
#include <intsafe.h>

#include <SpecialK/diagnostics/cpu.h>
#include <SpecialK/render/backend.h>
#include <SpecialK/window.h>
#include <SpecialK/core.h>

static constexpr int SK_NoPreference = -1;
static constexpr int SK_Disabled     =  0;
static constexpr int SK_Enabled      =  1;

enum {
  SK_HDR_CLIPBOARD_FORMAT_PNG  = 1,
  SK_HDR_CLIPBOARD_FORMAT_AVIF = 2,
};

struct SK_Keybind
{
  const char*  bind_name      = nullptr;
  std::wstring human_readable =     L"";

  struct {
    BOOL ctrl  = FALSE,
         shift = FALSE,
         alt   = FALSE;
  };

  SHORT vKey        =   0;
  UINT  masked_code = 0x0; // For fast comparison

  void parse  (void);
  void update (void);
};

namespace sk
{
  class ParameterStringW;
};

void    SK_ImGui_BeginKeybindEditorFrame   (void);
ULONG64 SK_ImGui_GetLastKeybindEditorFrame (void);

// Adds a parameter to store and retrieve the keybind in an INI / XML file
struct SK_ConfigSerializedKeybind : public SK_Keybind
{
  SK_ConfigSerializedKeybind ( SK_Keybind&& bind,
                             const wchar_t* cfg_name) :
                               SK_Keybind  (bind)
  {
    if (cfg_name != nullptr)
    {
      wcsncpy_s ( short_name, 32,
                    cfg_name, _TRUNCATE );
    }
  }

  bool                  assigning       = false;
  wchar_t               short_name [32] = L"Uninitialized";
  sk::ParameterStringW* param           = nullptr;
};


namespace skif
{
  enum class AutoStopBehavior {
    Never   = 0,
    AtStart = 1,
    AtExit  = 2
  };
}

enum SK_FrametimeMethod
{
  SK_FrametimeMeasures_LimiterPacing = 0,
  SK_FrametimeMeasures_PresentSubmit = 1,
  SK_FrametimeMeasures_NewFrameBegin = 2
};

enum SK_TearingMode
{
  AlwaysOn             = 0,
  AlwaysOff            = 1,
  AlwaysOff_LowLatency = 2,
  AdaptiveOn           = 3,
  AdaptiveOff          = 4, // Adaptive VSync
  AppControlled        = SK_NoPreference
};

struct sk_config_t
{
  sk_config_t (void)
  {
    // This struct is one of the earliest non-static initalized
    //   parts of the DLL, so important early init is best performed
    //     in the constructor of sk_config_t
    //
    LARGE_INTEGER               liQpcFreq = { };
    QueryPerformanceFrequency (&liQpcFreq);

    SK_QpcFreq       = liQpcFreq.QuadPart;
    SK_QpcTicksPerMs = SK_QpcFreq / 1000LL;
    SK_PerfFreq      = SK_QpcFreq;

    PROCESSOR_POWER_INFORMATION pwi   [64] = { };
    int                         cpuid [ 4] = { }; // Store eax,ebx,ecx,edx

    // Setup TSC-based timing instead of QPC when applicable
    //   (i.e. CPU has invariant timestamps)
    if ( 0x0 ==
           CallNtPowerInformation (ProcessorInformation, nullptr, 0, pwi, sizeof (pwi)) )
    {
      ZeroMemory (cpuid, sizeof (int) * 4);

#ifndef SK_BUILT_BY_CLANG
      __cpuid (cpuid, 0x80000007);
#else
      __llvm_cpuid (0x80000007, cpuid [0], cpuid [1],
                                cpuid [1], cpuid [2]);
#endif

      SK_TscFreq =
        (1000LL * 1000LL * static_cast <int64_t> (pwi [0].MaxMhz));

      SK_QpcFreqInTsc = (DWORD)(SK_TscFreq / SK_QpcFreq);
      SK_TscInvariant =
        false;
      //(cpuid [3] & (1 << 8)) != 0;

      SK_PerfFreqInTsc = 1;

// Using Tsc instead of QPC was experimented with a few years ago (~2023),
//   but it broke Latent Sync at the time... it may be worth revisiting.
#if 0
      if (SK_TscInvariant)
        SK_PerfFreq = SK_TscFreq;

      else
#endif
        SK_PerfFreqInTsc = SK_QpcFreqInTsc;
    }

    // Determine number of CPU cores total, and then the subset of those
    //   cores that the process is allowed to run threads on.
    SYSTEM_INFO        si = { };
    SK_GetSystemInfo (&si);

    DWORD     cpu_pop          = std::max (1UL, si.dwNumberOfProcessors);
    DWORD_PTR process_affinity = 0,
              system_affinity  = 0;

    if (GetProcessAffinityMask (GetCurrentProcess (), &process_affinity,
                                                       &system_affinity))
    {
      cpu_pop = 0;

      for ( DWORD_PTR i = 0 ; i < (DWORD_PTR)SK_GetBitness () ; ++i )
      {
        if ((process_affinity >> i) & 0x1)
          ++cpu_pop;
      }
    }

    priority.available_cpu_cores =
      std::max (1UL, std::min (cpu_pop, si.dwNumberOfProcessors));

    screenshots.avif.max_threads =
      std::max (2, static_cast <int> (priority.available_cpu_cores) / 3);


    ZeroMemory (cpuid, sizeof (int) * 4);

#ifndef SK_BUILT_BY_CLANG
      __cpuid (cpuid, 0x80000001);
#else
      __llvm_cpuid (0x80000001, cpuid [0], cpuid [1],
                                cpuid [1], cpuid [2]);
#endif

    // MWAITX = ECX Bit 29 (8000_0001h)
    SK_CPU_HasMWAITX = (cpuid [2] & (1 << 28)) != 0;

    if (! SK_CPU_HasMWAITX)
          SK_CPU_HasMWAITX =
      SK_CPU_TestForMWAITX ();

    SK_PerfTicksPerMs = SK_PerfFreq / 1000LL;

    if (! SK_GetHostAppUtil ()->isBlacklisted ())
    {
      utility.hSignalAsyncSave =
        SK_CreateEvent (nullptr, FALSE, FALSE, nullptr);
    }
  }
  struct whats_new_s {
    float  duration       = 20.0F;
  } version_banner;

  struct time_osd_s {
    LONG   format         = LOCALE_USER_DEFAULT;

    struct keybinds_s {
      BYTE toggle [4]     = { VK_CONTROL, VK_SHIFT, 'T', 0 };
    } keys;

    bool   show           = true;
  } time;

  struct title_s {
    bool   show           = true;
  } title;

  struct mem_osd_s {
    float  reserve        = 0.0F;// Unused / Unlimited
    float  interval       = 0.25F;

    struct keybinds_s {
      BYTE toggle [4]     = { VK_CONTROL, VK_SHIFT, 'M', 0 };
    } keys;

    bool   show           = false;
  } mem;


  struct io_osd_s {
    float  interval       = 0.25F; // 250 msecs (4 Hz)

    struct keybinds_s {
      BYTE toggle [4]     = { VK_CONTROL, VK_SHIFT, 'I', 0 };
    } keys;

    bool   show           = false;
  } io;


  struct sli_s {
    bool   show           = false;

    struct keybinds_s {
      BYTE toggle [4]     = { VK_CONTROL, VK_SHIFT, 'S', 0 };
    } keys;
  } sli;


  struct basic_osd_s {
    int    red            = MAXDWORD32;
    int    green          = MAXDWORD32;
    int    blue           = MAXDWORD32;
    float  scale          =  1.0F;
    int    pos_x          =  0;
    int    pos_y          =  0;

    struct keybinds_s {
      BYTE toggle [4]     = { VK_CONTROL, VK_SHIFT, 'O',          0 };
      BYTE shrink [4]     = { VK_CONTROL, VK_SHIFT, VK_OEM_MINUS, 0 };
      BYTE expand [4]     = { VK_CONTROL, VK_SHIFT, VK_OEM_PLUS,  0 };
      SK_ConfigSerializedKeybind
        console_toggle = {
          SK_Keybind {
            "Toggle SK's Command Console", L"Ctrl+Shift+Tab",
             true, true, false, VK_TAB
          }, L"ConsoleToggle"
        };
    } keys;

    bool   remember_state = false;
    bool   show           = false;
  } osd;


  struct cpu_osd_s {
    GUID      power_scheme_guid      = { };
    GUID      power_scheme_guid_orig = { };

    float  interval       = 0.33333333f;

    bool   simple         = true;
    bool   show           = false;

    struct keybinds_s {
      BYTE toggle  [4]    = { VK_CONTROL, VK_SHIFT, 'C', 0 };
    } keys;
  } cpu;


  struct fps_osd_s {
    bool   show           = true;
    bool   compact        = false;
    bool   advanced       = false;
    bool   frametime      = true;
    bool   framenumber    = false;
    bool   compact_vrr    = false;
    int    timing_method  = SK_FrametimeMeasures_LimiterPacing;

    struct keybinds_s {
      BYTE toggle [4]     = { VK_CONTROL, VK_SHIFT, 'F', 0 };
    } keys;
    //float fps_interval  = 1.0F;
  } fps;


  struct gpu_osd_s {
    bool   show           = true;
    bool   print_slowdown = false;
    float  interval       = 0.333333F;

    struct keybinds_s {
      BYTE toggle [4]     = { VK_CONTROL, VK_SHIFT, 'G', 0 };
    } keys;
  } gpu;


  struct disk_osd_s {
    bool   show            = false;

    float  interval        = 0.333333F;
    int    type            = 0; // Physical = 0,
                                // Logical  = 1

    struct keybinds_s {
      BYTE toggle [4]      = { VK_CONTROL, VK_MENU, VK_SHIFT, 'D' };
    } keys;
  } disk;


  struct pagefile_osd_s {
    bool   show            = false;
    float  interval        = 2.5F;

    struct keybinds_s {
      BYTE toggle [4]      = { VK_CONTROL, VK_MENU, VK_SHIFT, 'P' };
    } keys;
  } pagefile;


  struct dlss_osd_s {
    bool   show            = false;
    bool   show_output_res = false;
    bool   show_quality    = true;
    bool   show_fg         = true;
    bool   show_preset     = false;

    struct keybinds_s {
      BYTE toggle [4]     = { VK_MENU, VK_SHIFT, 'D', 0 };
    } keys;
  } dlss;


  struct cegui_s {
    ULONG   frames_drawn       = 0;     //   Count the number of frames drawn using it
    bool    enable             = true;
    bool    orig_enable        = false; // Since CEGUI is a frequent source of crashes.
    bool    safe_init          = true;
  } cegui;

  struct imgui_s {
    float   scale              = 1.0F;
    bool    show_eula          = false; // Will be flipped on if no AppCache is present
    bool    show_input_apis    = true;
    bool    use_mac_style_menu = false;

    struct font_s {
      struct font_params_s {
        std::string file   = "";
        float       size   = 7.0F;
      } chinese      { "msyh.ttc",     18.0f },
        cyrillic     { "arial.ttf",    18.0f },
        default_font { "arial.ttf",    18.0f },
        japanese     { "msgothic.ttc", 18.0f },
        korean       { "malgun.ttf",   18.0f };
    } font;

    // Per-game (mostly compatibility) settings
    struct render_s
    {
      bool  strip_alpha        = false; // Alpha is forcefully stripped
      bool  disable_alpha      = false; // Window backgrounds are opaque
      bool  antialias_lines    = true;
      bool  antialias_contours = true;
    } render;
  } imgui;

  struct notifications_s
  {
    int  location = 2; // 0=Top-Left,
                       // 1=Top-Right,
                       // 2=Bottom-Left,
                       // 3=Bottom-Right,
                       // 4=Don't Care
    bool silent   = false;
  } notifications;


  struct steam_s {
    struct callback_cache_s {
      HMODULE
        module               = nullptr;
      uintptr_t
        offset               = 0;
      void* resolved         = nullptr;
    } cached_overlay_callback;

    struct cloud_s {
      std::set <std::string> blacklist;
    } cloud;

    std::wstring
                dll_path              = L"";

    int64_t     appid                 = 0LL;
    int         online_status         = SK_NoPreference;
                                             // Force a certain online status at all times
    int         init_delay            = 0UL; // Disable to prevent crashing in many games
    int         callback_throttle     = SK_NoPreference;
    bool        preload_client        = false;
    bool        preload_overlay       = false;  // For input processing, this is important
    bool        force_load_steamapi   = false;  // Load steam_api{64}.dll even in games
    bool        auto_pump_callbacks   =  true;
    bool        block_stat_callback   = false;
    bool        filter_stat_callback  = false;
    bool        spoof_BLoggedOn       = false;
    bool        auto_inject           =  true;  // Control implicit steam_api.dll bootstrapping
    bool        disable_overlay       = false;  // Sets an Env. Var to prevent drawing
    bool        crapcom_mode          = false;  // Workaround for CAPCOM DRM

    struct screenshot_handler_s {
      bool      enable_hook           =  true;
    } screenshots;
  } steam;

  struct platform_s {
    struct achievements_s {
      std::wstring
                sound_file            = L"";

      struct popup_s {
        float   inset                 = 0.005F;
        int     origin                = 0;
        int     duration              = 5000UL;
        bool    show                  =  true;
        bool    show_title            =  true;
        bool    animate               =  true;
      } popup;

      bool      take_screenshot       = false;
      bool      play_sound            =  true;
      bool      pull_friend_stats     =  true;
      bool      pull_global_stats     =  true; // N/A for EOS
    } achievements;

    float       overlay_hdr_luminance = 4.37F; // 350 nits
                                               //   that do not use it

    int         notify_corner         = 4;     // 0=Top-Left,
                                               // 1=Top-Right,
                                               // 2=Bottom-Left,
                                               // 3=Bottom-Right,
                                               // 4=Don't Care
    bool        show_playtime         =  true; // In the control panel title
    bool        overlay_hides_sk_osd  =  true;
    bool        reuse_overlay_pause   =  false;// Use Steam's overlay pause mode for our own
                                               //   control panel
    bool        silent                = false;
    bool        steam_is_b0rked       = false; // Need to swallow some exceptions or Streamline may crash games
  } platform;

  struct epic_s {
    std::string app_name              = "";
    float       overlay_luminance     = 4.375F; // 350 nits
    bool        present               = false;  // Is the overlay detected?
    bool        warned_online         = false;
  } epic;

  struct uplay_s {
    float       overlay_luminance     = 4.375F; // 350 nits
    bool        present               = false;  // Is the overlay detected?
  } uplay;

  struct discord_s {
    float       overlay_luminance     = 4.375F; // 350 nits
    bool        present               = false;  // Is the overlay detected?
  } discord;

  struct rtss_s {
    float       overlay_luminance     = 4.375F; // 350 nits
    bool        present               = false;  // Is the overlay detected?
  } rtss;

  struct reshade_s {
    bool        draw_first            = true;
    bool        is_addon              = false;  // True if ReShade AddOn registration succeeded
    bool        is_addon_hookless     = false;  // True if ReShade has no hooked effect runtimes
    bool        has_local_ini         = false;  // Using local ReShade.ini instead of SK's
    SK_ConfigSerializedKeybind
                toggle_overlay_keybind= {
                    SK_Keybind {
                      "Toggle ReShade Overlay", L"Shift+Home",
                        true, false, false, VK_HOME
                    }, L"ToggleReShadeOverlay"
    };
    SK_ConfigSerializedKeybind
                inject_reshade_keybind  = {
                    SK_Keybind {
                      "Inject ReShade (6.0+)", L"Ctrl+Alt+Shift+R",
                      true, true, true, 'R'
                    }, L"InjectReShade"
    };
  } reshade;

  struct sound_s {
    SK_ConfigSerializedKeybind
         game_mute_keybind = {
      SK_Keybind {
        "Mute the Game", L"Ctrl+Shift+Home",
         true, true, false, VK_HOME
      }, L"MuteGame"
    };

    SK_ConfigSerializedKeybind
         game_volume_up_keybind = {
      SK_Keybind {
        "Increase Volume 10%", L"Ctrl+Shift+Insert",
         true, true, false, VK_INSERT
      }, L"VolumePlus10%"
    };

    SK_ConfigSerializedKeybind
         game_volume_down_keybind = {
      SK_Keybind {
        "Decrease Volume 10%", L"Ctrl+Shift+Delete",
         true, true, false, VK_DELETE
      }, L"VolumeMinus10%"
    };

    bool         minimize_latency      = false;
  } sound;

  struct screenshots_s {
    bool         use_avif              = false;
    bool         use_hdr_png           = false;
    int          max_st2084_bits       =    11;
    bool         use_jxl               = false;
    bool         png_compress          =  true;
    bool         show_osd_by_default   =  true;
    bool         play_sound            =  true;
    bool         copy_to_clipboard     =  true;
    bool         allow_hdr_clipboard   =  true;
    int          clipboard_hdr_format  =     1; // 1 = PNG, 2 = AVIF
    bool         embed_nickname        = false;
    std::wstring override_path         =   L"";
    std::wstring filename_format       = L"%G %F %T";

    struct avif_s {
      int        scrgb_bit_depth       =    10;
      int        yuv_subsampling       =   444;
      bool       full_range            =  true;
      int        compression_speed     =     8;
      int        max_threads           =     5;
    } avif;

    int          compression_quality   =    90;
    bool         compatibility_mode    =  true;

    SK_ConfigSerializedKeybind
         game_hud_free_keybind = {
      SK_Keybind {
        "Take Screenshot without Game's HUD", L"Num -",
         false, false, false, VK_OEM_MINUS
      }, L"HUDFree"
    };

    SK_ConfigSerializedKeybind
         sk_osd_free_keybind = {
      SK_Keybind {
        "Take Screenshot without Special K's OSD", L"F9",
         false, false, false, VK_F9
      }, L"WithoutOSD"
    };

    SK_ConfigSerializedKeybind
         no_3rd_party_keybind = {
      SK_Keybind {
        "Take Screenshot without Third-Party Overlays", L"",
         false, false, false, 0
      }, L"Without3rdParty"
    };

    SK_ConfigSerializedKeybind
         sk_osd_insertion_keybind = {
      SK_Keybind {
        "Take Screenshot and insert Special K's OSD", L"F8",
         false, false, false, VK_F8
      }, L"InsertOSD"
    };

    SK_ConfigSerializedKeybind
         clipboard_only_keybind = {
      SK_Keybind {
        "Copy a Screenshot to Clipboard Only", L"",
         false, false, false, VK_PRINT
      }, L"ClipboardOnly"
    };

    SK_ConfigSerializedKeybind
         snipping_keybind = {
      SK_Keybind {
        "Snip a Screenshot to the Clipboard", L"",
         true, false, false, VK_PRINT
      }, L"Snipping"
    };
  } screenshots;

  struct monitor_s {
    SK_ConfigSerializedKeybind
         monitor_primary_keybind = {
      SK_Keybind {
        "Move Game to Primary Monitor", L"<Not Bound>",
         false, false, false, 0,
      }, L"MoveToPrimaryMonitor"
    };

    SK_ConfigSerializedKeybind
         monitor_next_keybind = {
      SK_Keybind {
        "Move Game to Next Monitor", L"<Not Bound>",
         false, false, false, 0,
      }, L"MoveToNextMonitor"
    };

    SK_ConfigSerializedKeybind
         monitor_prev_keybind = {
      SK_Keybind {
        "Move Game to Previous Monitor", L"<Not Bound>",
         false, false, false, 0,
      }, L"MoveToPrevMonitor"
    };

    SK_ConfigSerializedKeybind
         monitor_toggle_hdr = {
      SK_Keybind {
        "Toggle HDR on Active Monitor", L"<Not Bound>",
         false, false, false, 0
      }, L"ToggleHDR"
    };
    SK_ConfigSerializedKeybind
         multimonitor_focus_keybind = {
      SK_Keybind {
        "Toggle ADHD Multi-Monitor Mode", L"<Not Bound>",
         false, false, false, 0
      }, L"ToggleADHDMultiMonitor"
    };
  } monitors;

  struct widget_s {
    SK_ConfigSerializedKeybind
         hide_all_widgets_keybind = {
      SK_Keybind {
        "Hide All Widgets", L"Ctrl+Alt+Shift+H",
        true, true, true, 'H'
      }, L"HideAllWidgets"
    };
  } widgets;

  struct render_s {
    struct framerate_s {
      float   target_fps          =  0.0F;
      float   target_fps_bg       =  0.0F;
      float   last_refresh_rate   =  0.0F;
      std::wstring
              last_monitor_path   =   L"";
      int     override_num_cpus   = SK_NoPreference;
      int     pre_render_limit    = SK_NoPreference;
      int     present_interval    = SK_NoPreference;
      int     sync_interval_clamp = SK_NoPreference;
      int     tearing_mode        = SK_NoPreference;
      int     buffer_count        = SK_NoPreference;
      int     max_delta_time      =  0; // Bad old setting; needs to be phased
      int     swapchain_wait      =  0;
      float   refresh_rate        = -1.0F;
 std::wstring rescan_ratio        =L"-1/1";
      struct rescan_s {
        UINT Denom                =  1;
        UINT Numerator            =
            sk::narrow_cast <UINT> (SK_NoPreference);
      } rescan_;
      int     refresh_denom       =     1;
      int     pin_render_thread   = SK_NoPreference;
      bool    turn_vsync_off      = false; // Turns VSync Off in Adaptive VSync mode
      bool    flip_discard        =  true; // Enabled by default (7/6/21)
      bool    flip_sequential     = false;
      bool    disable_flip        = false;
      bool    drop_late_flips     =  true;
      bool    wait_for_vblank     = false;
      bool    sleepless_render    = false;
      bool    sleepless_window    = false;
      bool    enable_mmcss        =  true;
      int     enforcement_policy  =     4; // Refer to framerate.cpp
      struct {
        bool  waiting             =  true; // VRR users have the limiter default to low-latency
        bool  triggered           = false; // The limiter was VRR-optimized once
        struct {
         bool ultra_low_latency   = false; // VRR auto-optimization goes further (potential stutter)
         bool global_opt          =  true; // Opt-In for Auto Low Latency as default policy
         bool auto_reapply        =  true; // Opt-In to have Auto Low Latency re-apply if display / refresh rate changes
        } policy;
      } auto_low_latency;
      bool    enable_etw_tracing  =  true;
      bool    supports_etw_trace  = false;// Not stored in config file
      struct latent_sync_s {
        SK_ConfigSerializedKeybind
          tearline_move_up_keybind = {
            SK_Keybind {
              "Move Tear Location Up 5 Scanlines", L"<Not Bound>",
               false, false, false, 0,
            }, L"MoveTearlineUp"
          };
        SK_ConfigSerializedKeybind
          tearline_move_down_keybind = {
            SK_Keybind {
              "Move Tear Location Down 5 Scanlines", L"<Not Bound>",
               false, false, false, 0,
            }, L"MoveTearlineDown"
          };
        SK_ConfigSerializedKeybind
          timing_resync_keybind = {
            SK_Keybind {
              "Force a Timing Resync", L"<Not Bound>",
               false, false, false, 0,
            }, L"ManualResync"
          };
        SK_ConfigSerializedKeybind
          toggle_fcat_bars_keybind = {
            SK_Keybind {
              "Toggle Tearline Visualizer", L"<Not Bound>",
               false, false, false, 0,
            }, L"ToggleFCATBars"
          };
        int   scanline_offset      =    -1;
        float scanline_resync      = 30.0f;
        int   scanline_error       =     1;
        float delay_bias           =  0.0f;
        bool  auto_bias            = false;
        float max_auto_bias        = 0.75f;
        struct auto_bias_target_s {
          float ms                 = 0.85f;
          float percent            = 0.0F;
        } auto_bias_target;
        bool  show_fcat_bars       = false; // Not INI-persistent

        bool flush_before_present  =  true;
        bool finish_before_present = false;

        bool flush_after_present   = false;
        bool finish_after_present  =  true;
      } latent_sync;
      bool    use_amd_mwaitx       =  true;
    } framerate;
    struct d3d9_s {
      bool    force_d3d9ex         = false;
      bool    force_impure         = false;
      bool    enable_flipex        = false;
      bool    use_d3d9on12         = false;
    } d3d9;
    struct dxgi_s {
      int     adapter_override     = SK_NoPreference;
      struct resolution_s {
        struct minimum_s {
          unsigned int x           =  0;
          unsigned int y           =  0;
          bool isZero (void) noexcept { return x == 0 && y == 0; };
        } min;
        struct maximum_s {
          unsigned int x           =  0;
          unsigned int y           =  0;
          bool isZero (void) noexcept { return x == 0 && y == 0; };
        } max;
      } res;
      struct refresh_s {
        float min = 0.0f;
        float max = 0.0f;
      } refresh;
      int     exception_mode       = SK_NoPreference; // -1 = Don't Care
      int     scaling_mode         = SK_NoPreference; // -1 = Don't Care
      int     scanline_order       = SK_NoPreference; // -1 = Don't Care
      int     msaa_samples         = SK_NoPreference;
      // DXGI 1.1 (Ignored for now)
      int     rotation             = SK_NoPreference; // -1 = Don't Care
      int     srgb_behavior        =    -2; // -2 = sRGB Not Encountered Yet
                                            // -1 = Passthrough,
                                            //  0 = Strip,
                                            //  1 = Apply
      bool    test_present         = false;
      bool    full_state_cache     = false;
      bool    debug_layer          = false;
      bool    low_spec_mode        =  true; // Disable D3D11 Render Mods
      bool    allow_tearing        =  true;
      bool    safe_fullscreen      = false;
      bool    enhanced_depth       = false;
      bool    deferred_isolation   = false;
      bool    present_test_skip    = false;
      bool    hide_hdr_support     = false; // Games won't know HDR is supported
      int     hdr_metadata_override=
                           SK_NoPreference; // -1 = Don't Care, -2 Disable outright
      bool    use_factory_cache    =  true; // Fix performance issues in Resident Evil 8
      bool    skip_mode_changes    =  true; // Try to skip rendundant resolution changes
      bool    temporary_dwm_hdr    = false; // Always turns HDR on and off for this game
      bool    disable_virtual_vbi  =  true; // Disable Windows 11 Dynamic Refresh Rate
      bool    ignore_thread_flags  = false; // Remove threading flags from D3D11 devices
      bool    clear_flipped_chain  = false; // Clear buffers on present? (non-compliant)
      float   chain_clear_color[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
      bool    suppress_resize_fail =  true; // Workaround DLSS FrameGen compat. issues in D3D12
      bool    suppress_rtv_mismatch= false; // Hide SwapChain RTV format warnings for buggy games
      float   warn_if_vram_exceeds =  95.f; // Warn if VRAM usage exceeds % of available
      bool    warned_low_vram      = false; // NOT SAVED: State of warn_if_vram_exceeds
      bool    allow_d3d12_footguns = false; // Allow overrides that are unsafe in D3D12
      bool    fake_fullscreen_mode = false; // Trick APIs like NvAPI into thinking FSE is on
      float   vram_budget_scale    =  1.0f; // Over- or under-report VRAM capabilities.
      DXGI_FORMAT                           // Report this value when the game calls GetDesc (...)
              fake_swapchain_desc  = DXGI_FORMAT_UNKNOWN;
      struct hooks_s {
        bool  create_swapchain     =  true;
        bool  create_swapchain4hwnd=  true;
      } hooks;
    } dxgi;

    struct {
      bool    wrap_d3d11_dev_ctx   = false;
      bool    track_map_and_unmap  = true;
      bool    track_set_shader_res = true; // Required for used texture determination in mod tools
      bool    trace_sampler_init   = false;
    } d3d11;

    struct {
      bool    disable_bypass_io    = false;
      bool    disable_telemetry    = false;
      bool    disable_gpu_decomp   = false;
      bool    force_file_buffering = false;
      int     submit_threads       = -1;
      int     cpu_decomp_threads   = -1;
      bool    enable_hooks         = true;
    } dstorage;

    struct {
      bool    disable_fullscreen   =  true;
      bool    enable_16bit_hdr     = false;
      bool    enable_10bit_hdr     = false;
      bool    upgrade_zbuffer      =  true;
      bool    prefer_10bpc         =  true;

      struct {
#ifdef _DEBUG
        bool  enable               = true;
#else
        bool  enable               = false;
#endif
        bool  break_on_error       = true;
      } debug;
    } gl;

    struct osd_s {
      ULONG64 _last_vidcap_frame   = 0;
      ULONG64 _last_normal_frame   = 0;
      float   hdr_luminance        = 4.375F; // 350 nits
      // Required by default for compatibility with Mirillis Action!
      bool    draw_in_vidcap       = true;
    } osd;

    // OSD Render Stats
    bool      show                 = false;
    struct keybinds_s {
      SK_ConfigSerializedKeybind
        hud_toggle = {
          SK_Keybind {
            "Toggle Game's HUD", L"Alt+Shift+H",
             false, true, true, 'H'
          }, L"HUDToggle"
        };
    } keys;

    struct output_format_s {
      bool    force_8bpc           = false; ///< UNUSED
      bool    force_10bpc          = false; ///< D3D9/11/12
      bool    force_16bpc          = false; ///< UNUSED
    } output;

    struct {
      bool enable_32bpc                = false;
      bool remaster_8bpc_as_unorm      = false;
      bool remaster_subnative_as_unorm = false;
      int  last_used_colorspace        = 0;
    } hdr;

    struct {
      bool  force_anisotropic          = false;
      int   max_anisotropy             = -1;
      float force_lod_bias             = 0.0f;
    } d3d12;
  } render;

  struct display_s {
    std::wstring
              monitor_path_ccd     =   L"";
    int       monitor_idx          =     0;
    HMONITOR  monitor_handle       =     0;
    int       monitor_default      = MONITOR_DEFAULTTONEAREST;
    float     refresh_rate         =  0.0F;
    bool      force_fullscreen     = false;
    bool      force_windowed       = false;
    bool      allow_refresh_change =  true;
    bool      aspect_ratio_stretch = false;
    bool      focus_mode           = false;
    bool      focus_mode_if_focused=  true;
    bool      confirm_mode_changes =  true;
    bool      save_monitor_prefs   =  true;
    bool      warn_no_mpo_planes   = false;
    struct resolution_s {
      bool           save          =  true;
      bool           applied       = false;
      struct desktop_override_s {
        unsigned int x             = 0;
        unsigned int y             = 0;
        bool isZero (void) noexcept { return x == 0 && y == 0; };
      } override, original;
      // TODO
      std::wstring  monitor_path_ccd = L"";
    } resolution;
  } display;

  struct textures_s {
    struct d3d11_s {
      std::wstring
        res_root                   = L"SK_Res";
      bool    precise_hash         = false;
      bool    use_l3_hash          =  true;
      bool    dump                 = false;
      bool    inject               =  true;
      bool    cache                =  true;
      bool    orig_cache           =  true;// The initial setting when the game started
      bool    highlight_debug      =  true;
      bool    injection_keeps_fmt  = false;
      bool    generate_mips        = false;
      bool    cache_gen_mips       =  true;
      bool    uncompressed_mips    = false;
    } d3d11;
    struct cache_s {
      int     min_evict            = 64;
      int     max_evict            = 1024;
      int     min_entries          = 512;
      int     max_entries          = 65536; // Bump max up from 8192 since the hashmap
                                            //   is now segmented by number of mipmap LODs;
                                            //
                                            //  Overhead of managing a large hashmap is
                                            //    much lower.
      int     min_size             = 384L;
      int     max_size             = 4096L;
      bool    ignore_nonmipped     = false;
      bool    allow_staging        = false;
      bool    allow_unsafe_refs    = false; // Allow texture caching even in engines that
                                            //   are not correctly keeping track of resources
      bool    residency_managemnt  = false;// true;
      bool    vibrate_on_miss      = false;
    } cache;

    bool highlight_debug_tex       =  true;
    bool on_demand_dump            = false;
    bool d3d9_mod                  = false; // Causing compat issues in some D3D9Ex games ATM
    bool dump_on_load              = false;
    bool clamp_lod_bias            = false;
  } textures;

  struct file_trace_s {
    bool trace_reads               = false;
    bool trace_writes              = false;

    struct ignore_files_s {
      Concurrency::concurrent_unordered_set <std::wstring> single_file;
      Concurrency::concurrent_unordered_set <std::wstring> entire_thread;
    };

    static SK_LazyGlobal <ignore_files_s> ignore_reads;
    static SK_LazyGlobal <ignore_files_s> ignore_writes;

  } file_io;

  struct nvidia_s {
    struct sli_s {
      std::wstring
              compatibility       = L"0x00000000";
      std::wstring
              mode                = L"Auto";
      std::wstring
              num_gpus            = L"Auto";
      bool    override            = false;
    } sli;
    struct bugs_s {
      bool    kill_hdr            = false;
      bool    snuffed_ansel       = false;
      bool    bypass_ansel        =  true;
    } bugs;
    struct reflex_s {
      int     enforcement_site    =      1;
      UINT    frame_interval_us   =      0;
      bool    use_limiter         =  false;
      bool    low_latency         =  false;
      bool    low_latency_boost   =  false;
      bool    marker_optimization =  false;
      bool    enable              =  false;
      bool    native              =  false;
      bool    override            =  false;
      bool    combined_limiter    =  false;
      bool    disable_native      =  false;
      bool    show_detailed_widget=  false;
    } reflex;
    struct dlss_s {
      bool    force_dlaa          =  false;
      bool    auto_redirect_dlss  =  false;
      std::wstring
              dlss_dll            =    L"";
      std::wstring
              dlssg_dll           =    L"";
      bool    show_active_features=   true;
      bool    disable_ota_updates =  false;
      int     forced_preset       = SK_NoPreference;
      int     forced_auto_exposure= SK_NoPreference;
      int     forced_alpha_upscale= SK_NoPreference;
      int     use_sharpening      = SK_NoPreference;
      float   forced_sharpness    =   0.0f;
      struct {
        float performance         =   0.0f;
        float balanced            =   0.0f;
        float quality             =   0.0f;
        float ultra_performance   =   0.0f;
        float ultra_quality       =   0.0f;
        float dynamic_max         =   1.0f;
        float dynamic_min         =   1.0f;
      } scale;
      struct {
        int   extra_pixels        =      0;
        int   override_appid      =     -1;
      } compat;
      bool    allow_scrgb         =   true; // Use Compute Copy HDR10 <--> scRGB
      bool    dump_buffers        =  false;
      bool    spoof_support       =  false;
      bool    calculate_delta_ms  =  false;
    } dlss;
    struct misc_s {
      int     force_rebar         = SK_NoPreference;
    } misc;
  } nvidia;

  struct input_s {
    struct cursor_s {
      bool    gamepad_deactivates =   true;
      bool    keys_activate       =  false;
      bool    manage              =  false;
      int     timeout             = 1500UL;
    } cursor;

    struct ui_s {
      union {
        bool  capture             = false;
        bool  capture_mouse;      // Unconditionally capture the mouse (i.e. block it)
      };
      bool    capture_hidden      = false; // Capture mouse if HW cursor is not visible
      bool    capture_keyboard    = false; // ^^^ Disabled by default because it interferes with cursor auto-hide
      bool    capture_gamepad     = false;
      bool    use_hw_cursor       =  true;
      bool    center_cursor       = false;
      bool    nav_moves_mouse     = false;
      int     game_set_hw_cursor  =     0; // Not stored in INI, the number of times
    } ui;

    struct gamepad_s {
      int     predefined_layout   = 1;    //0 = PS4, 1 = Steam, 2 = Xbox
      int     disabled_to_game    = 0;    //0 = Never, 1 = Always, 2 = In Background
      bool    disable_hid         = false;
      bool    disable_winmm       = false;
      bool    rehook_xinput       = false;
      bool    haptic_ui           = true;
      bool    disable_rumble      = false;
      bool    hook_dinput8        = true;
      bool    hook_dinput7        = true;
      bool    hook_hid            = true;
      bool    hook_xinput         = true; // Kind of important ;)
      bool    hook_scepad         = true;
      bool    hook_raw_input      = true;
      bool    hook_windows_gaming = true;
      bool    hook_winmm          = true;
      bool    hook_game_input     = true;
      bool    allow_steam_winmm   = true;
      bool    native_ps4          = false;
      bool    bt_input_only       = false;
      float   low_battery_percent = 25.0f;
      float   impulse_strength_l  = 0.75f;
      float   impulse_strength_r  = 0.75f;

      struct xinput_s {
        unsigned
        int   ui_slot             =    0;
        bool  placehold  [4]      = { false };
        unsigned
        int   assignment [4]      = { 0, 1, 2, 3 };
        bool  disable    [4]      = { false };
        bool  hook_setstate       =  true; // Some software causes feedback loops
        bool  auto_slot_assign    = false;
        bool  blackout_api        = false;
        bool  emulate             = false;
        bool  debug               = false;
        float deadzone            =  5.0f; // 5% Left/Right Analog Stick Deadzone
      } xinput;

      struct {
        bool  blackout_api        = false;
      } windows_gaming_input;

      struct dinput_s {
        bool  blackout_gamepads   = false;
        bool  blackout_keyboards  = false;
        bool  blackout_mice       = false;
        bool  block_enum_devices  = false;
      } dinput;

      struct scepad_s {
        bool  disable_touch        =  false;
        bool  share_clicks_touch   =   true;
        bool  mute_applies_to_game =   true;
        bool  enhanced_ps_button   =   true;
        float rumble_power_level   = 100.0f;
        bool  power_save_mode      =   true;
        int   led_color_r          = SK_NoPreference;
        int   led_color_g          = SK_NoPreference;
        int   led_color_b          = SK_NoPreference;
        int   led_brightness       = SK_NoPreference;
        int   led_fade             = SK_NoPreference;
        int   show_ds4_v1_as_v2    = SK_NoPreference;
        int   hide_ds4_v2_pid      = SK_NoPreference;
        int   hide_ds_edge_pid     = SK_NoPreference;
        bool  enable_full_bluetooth=  false;
        bool  alias_trackpad_share =  false;
        std::wstring
              touch_click          = L"<Not Bound>";
        std::wstring
              left_fn              = L"<Not Bound>";
        std::wstring
              right_fn             = L"<Not Bound>";
        std::wstring
              left_paddle          = L"<Not Bound>";
        std::wstring
              right_paddle         = L"<Not Bound>";
      } scepad;

      struct steam_s
      {
        unsigned
        int   ui_slot             =     0;
        bool  disabled_to_game    = false;
      } steam;

      struct hid_s {
        int   max_allowed_buffers =     3;
        bool  calc_latency        = false;
      } hid;

      struct dualsense_s {
        enum effect : uint8_t
        {
          Off       = 0,
          Feedback  = 1,
          Weapon    = 2,
          Vibration = 3,

          Invalid
        };
        float  trigger_strength_r = 1.0f;      // Vibration intensity (can be > 1.0 to strengthen game trigger vibration)
        float  trigger_strength_l = 1.0f;      // Vibration intensity (can be > 1.0 to strengthen game trigger vibration)
        float  resist_strength_r  = -1.0f;     // Trigger resistance (0.0-1.0) [0.4275f]
        float  resist_strength_l  = -1.0f;     // Trigger resistance (0.0-1.0)
        float  resist_start_l     = -1.0f;     // Ratio (0.0-1.0) of trigger pull before resistance starts [0.18f]
        float  resist_start_r     = -1.0f;     // Ratio (0.0-1.0) of trigger pull before resistance starts
        effect trigger_effect_r   = Vibration; // Effect to apply for Xbox Impulse Triggers
        effect trigger_effect_l   = Vibration; // Effect to apply for Xbox Impulse Triggers
      } dualsense;

      bool    blocks_screensaver  =  true;
    } gamepad;

    struct keyboard_s {
      bool    block_windows_key   = false;
      bool    catch_alt_f4        =  true;
      bool    override_alt_f4     = false; // For games that have prompts (i.e. DQ XI / Yakuza)
      int     disabled_to_game    =     2; //0 = Never, 1 = Always, 2 = In Background
      int     org_disabled_to_game=     2;
      volatile
      UINT64  temporarily_allow   =     0; // Up until temporarily_allow + 1 frames,
    } keyboard;                            //   ignore "disabled_to_game"

    struct mouse_s {
      // Translate WM_MOUSEWHEEL messages into actual events that will trigger
      //   other mouse APIs such as DirectInput and RawInput.
      //
      //  * Without this, mousewheel scrolling doesn't work in MANY games on a
      //      Synaptics touchpad.
      //
      bool    fix_synaptics       = false;
      bool    ignore_small_clips  = false;// Ignore mouse clipping rects < 75% the
                                          //   dimensions of the client window, so
                                          //     that UI input works.
      int     disabled_to_game    =    0; //0 = Never, 1 = Always, 2 = In Background
      int     org_disabled_to_game=    0;
      UINT64  temporarily_allow   =    0; // Up until temporarily_allow + 1 frames,
                                          //   ignore "disabled_to_game"
    } mouse;

    // Avoids calling SK_Input_PreInit (...)
    bool dont_hook_core = false;
  } input;

  struct threads_s {
    bool    enable_mem_alloc_trace   = false;
    bool    enable_file_io_trace     = false;
    bool    enable_dynamic_spinlocks = false;
  } threads;

  struct injection_s {
    struct global_cache_s {
      bool  use_static_addresses = false;
    } global;
  } injection;

  struct window_s {
    bool    borderless          = false;
    bool    border_override     = false;
    bool    center              = false;
    int     zband               = ZBID_DESKTOP;
    struct offset_s {
      struct coordinate_s {
      int   absolute            = 0;
      float percent             = 0.0F;
      } x, y;
      bool isZero (void) noexcept
            { return x.absolute == 0        && y.absolute == 0        &&
                     x.percent  > -0.00001F && x.percent   < 0.00001F &&
                     y.percent  > -0.00001F && y.percent   < 0.00001F; }
    } offset;
    int     always_on_top       = NoPreferenceOnTop;//SmartAlwaysOnTop;
    bool    background_render   = false;
    bool    background_mute     = false;
    bool    confine_cursor      = false;
    bool    unconfine_cursor    = false;
    bool    persistent_drag     = false;
    bool    drag_lock           = false; // NOT SAVED IN INI
    bool    fullscreen          = false;
    bool    multi_monitor_mode  = false;
    bool    disable_screensaver = false;
    bool    fullscreen_no_saver = false; // In Fullscreen, disable screensaver?
    bool    manage_screensaver  = false;
    bool    treat_fg_as_active  = false; // Compat. hack for NiNoKuni 2
    bool    dont_hook_wndproc   = false;
    bool    activate_at_start   = false;
    struct resolution_s {
      struct dim_override_s {
        unsigned int x          = 0;
        unsigned int y          = 0;
        bool         fix_mouse  = false;
        bool isZero (void) noexcept { return x == 0 && y == 0; };
      } override;
    } res;
  } window;

  struct dpi_s {
    struct awareness_s{
      bool   aware                = true;
      bool   aware_on_all_threads = true;
    } per_monitor;
    bool     disable_scaling      = true;
  } dpi;

  struct compatibility_s {
    bool     rehook_loadlibrary       = false;
    bool     disable_nv_bloat         = false;
    bool     init_while_suspended     =  true;
    bool     impersonate_debugger     = false; // Can disable games' crash handlers
    bool     disable_debug_features   = false;
    bool     using_wine               = false;
    bool     allow_dxdiagn            =  true; // Slows game launches way down
    bool     auto_large_address_patch =  true;
    bool     init_on_separate_thread  =  true;
    bool     init_sync_for_reshade    = false;
    bool     init_sync_for_streamline = false;
    bool     shutdown_on_window_close = false;
    bool     disable_dx12_vk_interop  = false;
    bool     reshade_mode             = false;
    bool     fsr3_mode                = false;
    bool     allow_fake_streamline    =  true;
    int      sdl_sanity_level         =     1;
    struct sdl_s {
      int    allow_wgi                =    -1;
      int    allow_raw_input          =    -1;
      int    allow_direct_input       =    -1;
      int    allow_xinput             =    -1;
      int    allow_hid                =    -1;
      int    allow_all_ps_bt_features =    -1;
      float  switch_led_brightness    = -1.0f;
      int    use_joystick_thread      =    -1;
      int    poll_sentinel            =    -1;
    } sdl;
    // Messages that SK may post to a game's
    //   window in order to trigger various
    //     window management responsibilities.
    bool     allow_fake_displaychange =  true;
    bool     allow_fake_size          =  true;
  } compatibility;

  struct apis_s {
    struct glide_s {
      bool   hook = true;
    } glide;

#ifdef _M_IX86
    struct legacy_dx_s {
      bool   hook      = true;
      int    hook_next = SK_NoPreference;
    } d3d8,
      ddraw;
#endif

    struct d3d9_s {
      bool   hook        =            true;
      int    native_dxvk = SK_NoPreference;
      int    hook_next   = SK_NoPreference;
    } d3d9,
      d3d9ex;

    struct dxgi_s {
      struct d3d11or12_s {
        bool hook      = true;
        int  hook_next = SK_NoPreference;
      } d3d12,
        d3d11;
    } dxgi;

    struct khronos_s {
      bool   hook      = true;
      bool   translate = true;
      int    hook_next = SK_NoPreference;
    } Vulkan,
      OpenGL;

    struct NvAPI_s {
      bool         enable         = true;
      bool         gsync_status   = true;
      bool         implicit_gsync = false; // Check GSync status even if user hasn't requested
      bool         disable_hdr    = false;
      int          vulkan_bridge  = SK_NoPreference;
      std::wstring bpc_enum       = L"NV_BPC_DEFAULT";
      std::wstring col_fmt_enum   = L"NV_COLOR_FORMAT_AUTO";
    } NvAPI;

    struct ADL_s {
      bool   enable            = true;
    } ADL;

    struct D3DKMT_s {
      bool   enable_perfdata  = true;
    } D3DKMT;

    SK_RenderAPI last_last_known = SK_RenderAPI::Reserved;
    SK_RenderAPI last_known      = SK_RenderAPI::Reserved;
    SK_RenderAPI translated      = SK_RenderAPI::None;
  } apis;

  struct system_s {
    bool    first_run           =  true; // Indicates no pre-existing INI preferences
    std::wstring
            version             = SK_GetVersionStrW ();
    int     log_level           =    0;
    float   global_inject_delay = 0.0F;
#ifdef _DEBUG
    bool    trace_create_thread =  true;
#else
    bool    trace_create_thread = false;
#endif
    bool    trace_load_library  =  true;
    bool    strict_compliance   = false;
    bool    silent              = false;
    bool    handle_crashes      =  true;
    bool    silent_crash        = false;
    bool    suppress_crashes    = false;
    bool    prefer_fahrenheit   = false;
    bool    display_debug_out   = false;
    bool    game_output         =  true;
    bool    central_repository  = false;
    bool    wait_for_debugger   = false;
    bool    return_to_skif      = false;
    bool    auto_load_asi_files = false;
  } system;

  struct priority_scheduling_s {
    bool    raise_always        = false;
    bool    raise_bg            = false;
    bool    raise_fg            =  true;
    bool    highest_priority    = false;
    bool    deny_foreign_change =  true;
    bool    perf_cores_only     = false;
    int     minimum_render_prio = THREAD_PRIORITY_ABOVE_NORMAL;
    DWORD   available_cpu_cores =   1UL;
    int64_t cpu_affinity_mask   = 0xFFFFFFFFFFFFFFFFULL;
  } priority;

  struct skif_s {
    int     auto_stop_behavior  = 1; // 0=Never, 1=AtStart, 2=AtExit
  } skif;

  struct network_s {
    bool    disable_winsock     =  false;
    bool    strict_blocking     =  false; // Block third-party modules from using Winsock?
  } network;

  struct utility_functions_s {
    HANDLE hSignalAsyncSave     = nullptr;

    void save_async (void);
  } utility;
};

template <class T>
class SK_DanglingRef
{
protected:
  typedef T* (*init_fn)(void);
               init_fn init = nullptr;

  T* pPtr = nullptr;

public:
  SK_DanglingRef (void) noexcept
  {
    pPtr = nullptr;
    init = nullptr;
  }

  SK_DanglingRef (T* pRef, init_fn _init) noexcept
  {
    pPtr =  pRef;
    init = _init;
  }

  SK_DanglingRef (init_fn _init) noexcept
  {
    pPtr = nullptr;
    init = _init;
  }

  operator SK_DanglingRef& (void) = delete;
  operator SK_DanglingRef  (void) = delete;


  operator T& ()
  {
    if (pPtr != nullptr)
      return *pPtr;

    else if (init != nullptr)
    {
      pPtr = init ();
    }

    static T _dummy;

    if (pPtr == nullptr)
      return _dummy;

    return *pPtr;
  }

  T* operator -> ()
  {
    if (pPtr != nullptr)
      return pPtr;

    else if (init != nullptr)
    {
      pPtr = init ();
    }

    return pPtr;
  }
};

extern sk_config_t config;

struct SK_KeyCommand
{
  SK_Keybind   binding;
  std::wstring command;
};


bool SK_LoadConfigEx (      std::wstring  name, bool create = true);
bool SK_LoadConfig   (const std::wstring& name              = L"");
bool SK_DeleteConfig (      std::wstring  name              = L"");
void SK_SaveConfig   (      std::wstring  name              = L"",
                      bool                close_config      = false);

//
// Sort of a nasty hack until I come up with a way for plug-ins to
//   formally read the parent DLL's configuration.
//
uint32_t
__cdecl
SK_Config_GetTargetFPS (void);


struct iSK_INI;

typedef uint32_t DepotId_t;
typedef uint64_t ManifestId_t;

struct SK_Steam_DepotManifest {
  struct {
    std::string  name = "";
    DepotId_t    id   =  0;
  } depot;

  struct
  {
    std::string  date = "";
    ManifestId_t id   =  0;
  } manifest;
};

typedef std::vector <SK_Steam_DepotManifest> SK_DepotList;

struct SK_AppCache_Manager
{ enum Ownership
  { Unknown    = -1,
    DoesNotOwn =  0,
    OwnsGame   =  1,
    _Alignment_=  LONG_MAX };

  enum FriendPreference
  { Default    = -1, // Use Global Setting
    Disable    =  0,
    Enable     =  1 };

  bool          saveAppCache           (bool           close = false);
  bool          loadAppCacheForExe     (const wchar_t* wszExe);
  bool          loadAppCacheForEpicApp (const char*    szEpicApp);

  uint64_t      getAppIDFromPath       (const wchar_t* wszPath)   const;
  std::wstring  getAppNameFromID       (uint64_t       uiAppID)   const;
  std::wstring  getAppNameFromPath     (const wchar_t* wszPath)   const;
  std::wstring  getAppNameFromEpicApp  (const char*    szEpicApp) const;

  bool          addAppToCache       ( const wchar_t* wszRelativePath,
                                      const wchar_t* wszExecutable,
                                      const wchar_t* wszAppName,
                                            uint64_t uiAppID );
  bool          addAppToCache       ( const wchar_t* wszRelativePath,
                                      const wchar_t* wszExecutable,
                                      const wchar_t* wszAppName,
                                         const char* szEpicApp );

  std::wstring  getConfigPathFromAppPath   (const wchar_t* wszPath)           const; // Steam
  std::wstring  getConfigPathFromCmdLine   (const wchar_t* wszCmdLine)        const; // Epic
  std::wstring  getConfigPathForAppID      (uint64_t       uiAppID)           const;
  std::wstring  getConfigPathForEpicApp    (const char*    szEpicApp)         const;
  std::wstring  getConfigPathForGenericApp (const wchar_t *wszGenericAppName) const; // Other

  int           migrateProfileData       (LPVOID reserved = nullptr);

  SK_DepotList& getAvailableManifests    (DepotId_t steam_depot);
  ManifestId_t  getInstalledManifest     (DepotId_t steam_depot);
  int           loadDepotCache           (DepotId_t steam_depot = 0);
  int           storeDepotCache          (DepotId_t steam_depot = 0);


  time_t        setFriendOwnership  (uint64_t friend_, Ownership owns             );
  Ownership     getFriendOwnership  (uint64_t friend_, time_t*   updated = nullptr);

  time_t        setFriendAchievPct  (uint64_t friend_, float   percent          );
  float         getFriendAchievPct  (uint64_t friend_, time_t* updated = nullptr);

  void          setFriendPreference (FriendPreference);
  bool          wantFriendStats     (void); // Per-application override

#define SK_LICENSE_REVISION 20190408

  // For open source software license screen, only display when
  //   licensed software is updated.
  int           getLicenseRevision (void);
  void          setLicenseRevision (int);


protected:
  iSK_INI* app_cache_db = nullptr;
};

extern SK_LazyGlobal <SK_AppCache_Manager> app_cache_mgr;

enum class SK_GAME_ID
{
  Launcher,                     // A generic launcher...

  Tyranny,                      // Tyranny.exe
  TidesOfNumenera,              // TidesOfNumenera.exe
  MassEffect_Andromeda,         // MassEffectAndromeda.exe
  MadMax,                       // MadMax.exe
  Dreamfall_Chapters,           // Dreamfall Chapters.exe
  TheWitness,                   // witness_d3d11.exe, witness64_d3d11.exe
  Obduction,                    // Obduction-Win64-Shipping.exe
  TheWitcher3,                  // witcher3.exe
  ResidentEvil7,                // re7.exe
  DragonsDogma,                 // DDDA.exe
  DragonsDogma2,                // DD2.exe
  EverQuest,                    // eqgame.exe
  GodEater2RageBurst,           // GE2RB.exe
  GodEater3,                    // ge3.exe
  WatchDogs2,                   // WatchDogs2.exe
  NieRAutomata,                 // NieRAutomata.exe
  Warframe_x64,                 // Warframe.x64.exe
  LEGOCityUndercover,           // LEGOLCUR_DX11.exe
  Sacred,                       // sacred.exe
  Sacred2,                      // sacred2.exe
  FinalFantasy9,                // FF9.exe
  EdithFinch,                   // FinchGame.exe
  FinalFantasyX_X2,             // FFX.exe / FFX-2.exe / FFX&X-2_Will.exe
  FinalFantasy13,               // ffxiiiimg.exe / FFXiiiLauncher.exe
  DeadlyPremonition,            // DP.exe DPLauncher.exe
  GalGun_Double_Peace,          // GG2Game.exe
  YS_Seven,                     // Ys7.exe
  LifeIsStrange_BeforeTheStorm, // Life is Strange - Before the Storm.exe
  Tales_of_Symphonia,           // TOS.exe
  Tales_of_Zestiria,            // Tales of Zestiria.exe
  Tales_of_Vesperia,            // TOV_DE.exe
  Tales_of_Arise,               // Tales of Arise.exe
  Tales_of_Graces,              // Tales of Graces f Remastered.exe
  Hello_Kitty_Island_Adventure, // Hello Kitty.exe
  DivinityOriginalSin,          // EoCApp.exe
  Hob,                          // Hob.exe and HobLauncher.exe
  DukeNukemForever,             // DukeForever.exe
  BlueReflection,               // BLUE_REFLECTION.exe
  ZeroEscape,                   // Zero Escape.exe
  DotHackGU,                    // hackGU.exe
  WorldOfFinalFantasy,          // WOFF.exe
  StarOcean4,                   // StarOceanTheLastHope.exe
  StarOcean2R,                  // SO2R.exe
  LEGOMarvelSuperheroes2,       // LEGOMARVEL2_DX11.exe
  Okami,                        // okami.exe
  DuckTalesRemastered,          // DuckTales.exe
  Mafia3,                       // mafia3.exe
  Owlboy,                       // Owlboy.exe
  DarkSouls3,                   // DarkSoulsIII.exe
  Fallout4,                     // Fallout4.exe
  DisgaeaPC,                    // dis1_st.exe
  SecretOfMana,                 // Secret_of_Mana.exe
  FinalFantasyXV,               // ffxv_*.exe
  FinalFantasyXIV,              // ffxiv_dx11.exe
  FinalFantasyXVI,              // ffxvi_*.exe
  DragonBallFighterZ,           // DBFighterZ.exe
  NiNoKuni2,                    // Nino2.exe
  FarCry4,                      // FarCry4.exe
  FarCry5,                      // FarCry5.exe
  ChronoTrigger,                // Chrono Trigger.exe
  Ys_Eight,                     // ys8.exe
  PillarsOfEternity2,           // PillarsOfEternityII.exe
  Yakuza0,                      // Yakuza0.exe
  YakuzaKiwami,                 // YakuzaKiwami.exe
  YakuzaKiwami2,                // YakuzaKiwami2.exe
  YakuzaUnderflow,              // Yakuza*.exe
  YakuzaLikeADragonGaiden,      // LikeADragonGaiden.exe
  YakuzaInfiniteWealth,         // likeadragon8.exe
  MonsterHunterWorld,           // MonsterHunterWorld.exe
  MonsterHunterStories2,        // game.exe (fantastic!)
  MonsterHunterRise,            // MonsterHunterRise.exe
  Shenmue,                      // Shenmue.exe
  DragonQuestXI,                // DRAGON QUEST XI.exe
  AssassinsCreed_Odyssey,       // ACOdyssey.exe
  JustCause3,                   // JustCause3.exe
  CallOfCthulhu,                // CallOfCthulhu.exe
  TrailsOfColdSteel,            // ed8.exe
  Sekiro,                       // sekiro.exe
  OctopathTraveler,             // Octopath_Traveler-Win64-Shipping.exe
  SonicMania,                   // SonicMania.exe
  Persona4,                     // P4G.exe
  Persona5,                     // P5R.exe
  HorizonZeroDawn,              // HorizonZeroDawn.exe
  HorizonForbiddenWest,         // HorizonForbiddenWest.exe
  BaldursGate3,                 // bg3_dx11.exe
  BaldursGate3_Vulkan,          // bg3.exe
  AssassinsCreed_Valhalla,      // ACValhalla.exe / ACValhalla_Plus.exe
  AtelierRyza2,                 // Atelier_Ryza_2.exe
  Nioh2,                        // nioh2.exe
  HuniePop2,                    // HuniePop 2 - Double Date.exe
  GalGunReturns,                // GalGun Returns/game.exe
  Persona5Strikers,             // P5S/game.exe
  NieR_Sqrt_1_5,                // NieR Replicant ver.1.22474487139.exe
  ResidentEvil8,                // re8.exe
  LegendOfMana,                 // Legend Of Mana.exe
  FarCry6,                      // FarCry6.exe
  Ryujinx,                      // Ryujinx.exe
  yuzu,                         // yuzu.exe
  cemu,                         // cemu.exe
  RPCS3,                        // rpcs3.exe
  ForzaHorizon5,                // ForzaHorizon5.exe
  ForzaMotorsport,              // forza_gaming.desktop.x64_release_final.exe
  HaloInfinite,                 // HaloInfinite.exe
  FinalFantasy7,                // ff7_en.exe, ff7_fr.exe, ... ff7_*.exe
  FinalFantasy7Remake,          // ff7remake*.exe
  DyingLight2,                  // DyingLightGame_x64_rwdi.exe
  EasyAntiCheat,                // start_protected_game.exe
  EldenRing,                    // eldenring.exe
  TinyTinasWonderlands,         // Wonderlands.exe
  Elex2,                        // ELEX2.exe
  ChronoCross,                  // CHRONOCROSS.exe
  HatsuneMikuDIVAMegaMix,       // DivaMegaMix.exe
  ShinMegamiTensei3,            // smt3hd.exe
  TheQuarry,                    // TheQuarry-Win64-Shipping.exe
  GenshinImpact,                // GenshinImpact.exe
  WutheringWaves,               // WutheringWaves*****\Client-Win64-Shipping.exe
  PathOfExile,                  // PathOfExileSteam.exe
  Disgaea5,                     // Disgaea5.exe
  SoulHackers2,                 // SOUL HACKERS2.exe
  MegaManBattleNetwork,         // MMBN_LC2.exe, MMBN_LC1.exe
  HonkaiStarRail,               // StarRail.exe
  NoMansSky,                    // NMS.exe
  DiabloIV,                     // Diablo IV.exe
  CallOfDuty,                   // CoDSP.exe, CoDMP.exe (???)
  RatchetAndClank_RiftApart,    // RiftApart.exe
  SeriousSamFusion2017,         // Sam2017.exe / Sam2017_Unrestricted.exe
  Starfield,                    // Starfield.exe
  FalloutNewVegas,              // FalloutNV.exe
  Fallout3,                     // Fallout3.exe
  Oblivion,                     // Oblivion.exe
  GECK,                         // GECK.exe
  ConstructionSet,              // TESConstructionSet.exe
  CreationKit,                  // ConstructionKit.exe
  LordsOfTheFallen2,            // LOTF2-Win64-Shipping.exe
  AlanWake2,                    // AlanWake2.exe
  Cyberpunk2077,                // Cyberpunk2077.exe
  CrashReport,                  // CrashReport.exe (ironically, causes crashes)
  StreetFighter6,               // StreetFighter6.exe
  StardewValley,                // Stardew Valley.exe
  DOOMEternal,                  // DOOMEternalx64vk.exe
  Blood,                        // anuket_x64.exe
  BatmanArkhamKnight,           // BatmanAK.exe
  Noita,                        // Noita.exe
  Persona3,                     // P3R.exe
  GranblueFantasyRelink,        // granblue_fantasy_relink.exe
  WrathAeonOfRuin,              // wrath-sdl.exe
  HaroldHalibut,                // Harold Halibut.exe
  KingdomComeDeliverance,       // KingdomCome.exe
  GodOfWar,                     // GoW.exe
  TalosPrinciple2,              // Talos2-Win64-Shipping.exe
  CrashBandicootNSaneTrilogy,   // CrashBandicootNSaneTrilogy.exe
  StarWarsOutlaws,              // outlaws.exe
  ShadPS4,                      // shadPS4.exe
  GodOfWarRagnarok,             // GoWR.exe
  Metaphor,                     // METAPHOR.exe
  SonicXShadowGenerations,      // SONIC_X_SHADOW_GENERATIONS.exe
  SonicGenerations,             // SONIC_GENERATIONS.exe
  BrokenSword,                  // BS1R.exe
  YsX,                          // YsX.exe
  Transistor,                   // Transistor.exe
  MonsterHunterWilds,           // MonsterHunterWilds{Beta}.exe
  DragonAgeTheVeilguard,        // Dragon Age The Veilguard.exe
  TombRaider123Remastered,      // tomb123.exe
  Stalker2,                     // Stalker2-WinGDK-Shipping.exe (Microsoft Store) Stalker2-Win64-Shipping.exe (Steam)
  vlc,                          // vlc.exe
  ZenlessZoneZero,              // ZenlessZoneZero.exe

  UNKNOWN_GAME               = 0xffff
};

SK_GAME_ID
__stdcall
SK_GetCurrentGameID (void);

bool
__stdcall
SK_IsCurrentGame (SK_GAME_ID game_id);

const wchar_t*
__stdcall
SK_GetConfigPath (void);

const wchar_t*
__stdcall
SK_GetConfigPathEx (bool reset = false);

const wchar_t*
__stdcall
SK_GetNaiveConfigPath (void);

extern const wchar_t*
SK_GetFullyQualifiedApp (void);

const wchar_t*
__stdcall
SK_GetVersionStr (void) noexcept;

void
WINAPI
SK_Resource_SetRoot (const wchar_t* root);

std::filesystem::path
SK_Resource_GetRoot (void);

extern bool
SK_ImGui_KeybindSelect (SK_Keybind* keybind, const char* szLabel);

extern SK_API void
__stdcall
SK_ImGui_KeybindDialog (SK_Keybind* keybind);

using wstring_hash = size_t;

extern SK_LazyGlobal <std::unordered_map <wstring_hash, BYTE>>           humanKeyNameToVirtKeyCode;
extern SK_LazyGlobal <std::unordered_map <BYTE, wchar_t [32]>>           virtKeyCodeToHumanKeyName;
extern SK_LazyGlobal <std::unordered_map <BYTE, wchar_t [32]>>  virtKeyCodeToFullyLocalizedKeyName;
extern SK_LazyGlobal <std::unordered_multimap <uint32_t, SK_KeyCommand>> SK_KeyboardMacros;

iSK_INI* SK_GetNotifyINI (void);

using playstation_trigger_effect = sk_config_t::input_s::gamepad_s::dualsense_s::effect;

#endif /* __SK__CONFIG_H__ */
