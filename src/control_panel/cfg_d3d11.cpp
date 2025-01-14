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

#include <SpecialK/nvapi.h>


#include <SpecialK/control_panel/d3d11.h>
#include <SpecialK/control_panel/osd.h>

#include <SpecialK/render/dxgi/dxgi_swapchain.h>
#include <SpecialK/render/d3d11/d3d11_core.h>
#include <SpecialK/render/dstorage/dstorage.h>
#include <SpecialK/render/ngx/ngx.h>
#include <SpecialK/render/ngx/ngx_dlss.h>

#include <imgui/font_awesome.h>

extern float __target_fps;

extern float fSwapWaitFract;
extern float fSwapWaitRatio;

const char*
DXGIColorSpaceToStr (DXGI_COLOR_SPACE_TYPE space) noexcept;


extern bool SK_D3D11_EnableTracking;
extern bool SK_D3D11_EnableMMIOTracking;
extern volatile
       LONG SK_D3D11_DrawTrackingReqs;
extern volatile
       LONG SK_D3D11_CBufferTrackingReqs;


using namespace SK::ControlPanel;

bool SK::ControlPanel::D3D11::show_shader_mod_dlg = false;

extern concurrency::concurrent_unordered_map <ID3D12PipelineState*, bool>
                                                   _pixelShaders;
extern concurrency::concurrent_unordered_map <ID3D12PipelineState*, std::string>
                                                   _latePSOBlobs;

void
SK_ImGui_NV_DepthBoundsD3D11 (void)
{
  static bool  enable = false;
  static float fMin   = 0.0f;
  static float fMax   = 1.0f;

  bool changed = false;

  changed |= ImGui::Checkbox ("Enable Depth Bounds Test", &enable);

  if (enable)
  {
    changed |= ImGui::SliderFloat ("fMinDepth", &fMin, 0.0f, fMax);
    changed |= ImGui::SliderFloat ("fMaxDepth", &fMax, fMin, 1.0f);
  }

  if (changed)
  {
    NvAPI_D3D11_SetDepthBoundsTest ( SK_GetCurrentRenderBackend ().device,
                                       enable ? 0x1 : 0x0,
                                         fMin, fMax );
  }
}

SK_LazyGlobal <SK_D3D11_TexCacheResidency_s> SK_D3D11_TexCacheResidency;

void
SK_ImGui_DrawTexCache_Chart (void)
{
  if (config.textures.d3d11.cache)
  {
    ImGui::PushStyleColor (ImGuiCol_Border, ImVec4 (0.961f,0.961f,0.961f,1.f));
    ImGui::Separator (   );
    ImGui::PushStyleColor (ImGuiCol_Text,   ImVec4 (  1.0f,  1.0f,  1.0f,1.f));
    ImGui::Columns   ( 3 );
      ImGui::Text    ( "          Size" );                                                                 ImGui::NextColumn ();
      ImGui::Text    ( "      Activity" );                                                                 ImGui::NextColumn ();
      ImGui::Text    ( "       Savings" );
    ImGui::Columns   ( 1 );
    ImGui::PopStyleColor
                     (   );

    ImGui::PushStyleColor
                     ( ImGuiCol_Text, ImVec4 (0.75f, 0.75f, 0.75f, 1.0f) );

    ImGui::Separator      ( );
    ImGui::PopStyleColor  (1);

    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.5f, 0.5f, 0.5f, 0.666f));
    ImGui::Columns   ( 3 );
      ImGui::Text    ( "%7zu      MiB",
                                                     SK_D3D11_Textures->AggregateSize_2D >> 20ui64 );    ImGui::NextColumn ();
       ImGui::TextColored
                     (ImVec4 (0.3f, 1.0f, 0.3f, 1.0f),
                       "%5u      Hits",              SK_D3D11_Textures->RedundantLoads_2D.load () );     ImGui::NextColumn ();
       if (SK_D3D11_Textures->Budget != 0)
         ImGui::Text ( "Budget:  %7zu MiB  ",        SK_D3D11_Textures->Budget / 1048576ui64 );
    ImGui::Columns   ( 1 );

    ImGui::Separator (   );

    ImGui::Columns   ( 3 );
      ImGui::Text    ( "%7u Textures",               SK_D3D11_Textures->Entries_2D.load () );            ImGui::NextColumn ();
      ImGui::TextColored
                     ( ImVec4 (1.0f, 0.3f, 0.3f, 1.60f),
                       "%5u   Misses",             SK_D3D11_Textures->CacheMisses_2D.load () );          ImGui::NextColumn ();
     ImGui::Text   ( "Time:        %#7.01lf ms  ", SK_D3D11_Textures->RedundantTime_2D       );
    ImGui::Columns   ( 1 );

    ImGui::Separator (   );

    ImGui::Columns   ( 3 );
      ImGui::Text    ( "%6u   Evictions",              SK_D3D11_Textures->Evicted_2D.load ()   );        ImGui::NextColumn ();
      ImGui::TextColored (ImColor::HSV (std::min ( 0.4f * (float)SK_D3D11_Textures->RedundantLoads_2D /
                                                          (float)SK_D3D11_Textures->CacheMisses_2D, 0.4f ), 0.95f, 0.8f),
                       " %.2f  Hit/Miss",                (double)SK_D3D11_Textures->RedundantLoads_2D /
                                                         (double)SK_D3D11_Textures->CacheMisses_2D  );   ImGui::NextColumn ();
      ImGui::Text    ( "Driver I/O: %7llu MiB  ",      SK_D3D11_Textures->RedundantData_2D >> 20ui64 );

    ImGui::Columns   ( 1 );
    ImGui::Separator (   );
    ImGui::PopStyleColor();

    float size =
      sk::narrow_cast <float> (config.textures.cache.max_size);

    ImGui::TreePush  ( "" );

    if (ImGui::SliderFloat ( "Maximum Cache Size", &size,
                               512.f, 16384.f, "%.0f MiB" ))
    {
      config.textures.cache.max_size =
        sk::narrow_cast <int> (size);

      auto cp =
        SK_GetCommandProcessor ();

      cp->ProcessCommandFormatted ( "TexCache.MaxSize %d ",
                        config.textures.cache.max_size );
    }

    const float ui_scale =
      config.imgui.scale;

    ImGui::SameLine      ();
    ImGui::ItemSize      (ImVec2 (50.0f * ui_scale, 0.0f));
    ImGui::SameLine      ();

    ImGui::SeparatorEx   (ImGuiSeparatorFlags_Vertical);

    ImGui::SameLine      ();
    ImGui::ItemSize      (ImVec2 (50.0f * ui_scale, 0.0f));
    ImGui::SameLine      ();
    ImGui::Checkbox      ("Vibrate on cache miss", &config.textures.cache.vibrate_on_miss);

    ImGui::TreePop       ();
    ImGui::PopStyleColor ();

  //if (SK_DXGI_IsTrackingBudget ())
    {
      //
      // Residency queries have been broken for a long time, and would only
      //   cause crashes... so hide this option.  (4/29/24)
      //
      //ImGui::Checkbox ("Measure residency", &config.textures.cache.residency_managemnt);
      //ImGui::SameLine ();

      if (config.textures.cache.residency_managemnt)
      {
        ImGui::Separator ();

        static SK_D3D11_TexCacheResidency_s* tex_res =
          SK_D3D11_TexCacheResidency.getPtr ();

        auto& residency_count =
          tex_res->count;
        auto& residency_size  =
          tex_res->size;

        const int fully_resident = ReadAcquire (&residency_count.InVRAM);
        const int shared_memory  = ReadAcquire (&residency_count.Shared);
        const int on_disk        = ReadAcquire (&residency_count.PagedOut);

        const LONG64 size_vram   = ReadAcquire64 (&residency_size.InVRAM);
        const LONG64 size_shared = ReadAcquire64 (&residency_size.Shared);
        const LONG64 size_disk   = ReadAcquire64 (&residency_size.PagedOut);

        ImGui::BeginGroup ();
        if (fully_resident != 0)
          ImGui::TextColored (ImColor (0.3f, 0.78f, 0.3f),   "%d Textures in VRAM\t",          fully_resident);

        if (shared_memory != 0)
          ImGui::TextColored (ImColor (0.78f, 0.78f, 0.55f), "%d Textures in Shared Memory\t", shared_memory);

        if (on_disk != 0)
          ImGui::TextColored (ImColor (0.78f, 0.3f, 0.3f),   "%d Textures Paged to Disk\t",    on_disk);
        ImGui::EndGroup ();

        ImGui::SameLine ();

        SK_TLS* pTLS =
              SK_TLS_Bottom ();

        ImGui::BeginGroup ();
        if (fully_resident != 0)
          ImGui::TextColored (ImColor (0.1f, 0.98f, 0.1f),   "\t\t%hs",
                   SK_WideCharToUTF8 (SK_File_SizeToStringF (size_vram,   2, 3, Auto, pTLS).data ()).c_str ());
        if (shared_memory != 0)
          ImGui::TextColored (ImColor (0.98f, 0.98f, 0.25f), "\t\t%hs",
                   SK_WideCharToUTF8 (SK_File_SizeToStringF (size_shared, 2, 3, Auto, pTLS).data ()).c_str ());
        if (on_disk != 0)
          ImGui::TextColored (ImColor (0.98f, 0.1f, 0.1f),   "\t\t%hs",
                   SK_WideCharToUTF8 (SK_File_SizeToStringF (size_disk,   2, 3, Auto, pTLS).data ()).c_str ());
        ImGui::EndGroup ();
      }
    }
  }
}

void
SK_ImGui_DrawVRAMGauge (void)
{
  uint64_t vram_used   = SK_GPU_GetVRAMUsed   (0),
           vram_budget = SK_GPU_GetVRAMBudget (0);
  
  auto     vram_quota  =
    static_cast <uint64_t> (
      static_cast <double> (vram_budget) *
        (config.render.dxgi.warn_if_vram_exceeds / 100.0f));
  
  float vram_used_percent =
    static_cast <float> (
      static_cast <double> (vram_used) /
      static_cast <double> (vram_budget)
    );
  
  static constexpr size_t max_len = 31;
  
  if ( config.render.dxgi.warn_if_vram_exceeds > 0.0f &&
       vram_used > vram_quota )
  {
    ImGui::TextColored ( ImVec4 (1.f, 1.f, 0.0f, 1.f),
                           ICON_FA_EXCLAMATION_TRIANGLE );
  
    if (ImGui::IsItemHovered ())
    {
      static char szQuota [max_len + 1] = { };
  
      std::string_view vQuota =
        SK_File_SizeToStringAF (vram_quota, 0, 2);
  
      strncpy (szQuota, vQuota.data (), std::min (max_len, vQuota.size ()));
  
      ImGui::BeginTooltip ( );
      ImGui::Text         ( "VRAM Quota (%hs) Exhausted",
                             szQuota );
      ImGui::Separator    ( );
      ImGui::BulletText   ( "Consider lowering in-game graphics settings or"
                            " closing background applications." );
      ImGui::BulletText   ( "Right-click the VRAM gauge to set VRAM quotas"
                            " and/or reset quota warnings." );
      ImGui::EndTooltip   ( );
    }
  
    ImGui::SameLine ();
  }
  
  ImGui::BeginGroup ();
  
  static char szUsed   [max_len + 1] = { },
              szBudget [max_len + 1] = { };
  
  auto      vUsed   = SK_File_SizeToStringAF ( vram_used,   0, 2 );
  strncpy (szUsed,     vUsed.data (), std::min (max_len,   vUsed.size ()));
  
  auto      vBudget = SK_File_SizeToStringAF ( vram_budget, 0, 2 );
  strncpy (szBudget, vBudget.data (), std::min (max_len, vBudget.size ()));
  
  static char                  label_txt [max_len * 2 + 1] = { };
  std::string_view label_view (label_txt, max_len * 2);
       size_t      label_len         =
    SK_FormatStringView ( label_view,
                            "%0.2f%% of Available VRAM Used\t(%hs / %hs)",
                              vram_used_percent * 100.0f, szUsed, szBudget );
  
  label_txt [ std::max ((size_t)0,
              std::min ((size_t)max_len * 2, label_len)) ] = '\0';
  
  ImVec4 label_color =
    ImColor::HSV ((1.0f - vram_used_percent) * 0.278f, 0.88f, 0.666f);
  
  if (config.render.dxgi.warn_if_vram_exceeds > 0)
  {
    float x_pos =
      ImGui::GetCursorPosX ();
    ImGui::PushStyleColor  (ImGuiCol_PlotHistogram, (const ImVec4&)ImColor (0.3f, 0.3f, 0.3f, 1.0f));
    ImGui::ProgressBar     (
      static_cast <float>
        ( static_cast <double> (vram_quota) /
          static_cast <double> (vram_budget) ), ImVec2 (-1.0f, 0.0f), ""
    );
    ImGui::PopStyleColor   ();
    ImGui::SameLine        ();
    ImGui::SetCursorPosX   (x_pos);
  }
  
  ImGui::PushStyleColor (ImGuiCol_FrameBg,       (const ImVec4&)ImColor (0.0f, 0.0f, 0.0f, 0.0f));
  ImGui::PushStyleColor (ImGuiCol_PlotHistogram, label_color);
  ImGui::ProgressBar    (
    static_cast <float>
      ( static_cast <double> (vram_used) /
        static_cast <double> (vram_budget) ), ImVec2 (-1.0f, 0.0f), label_txt
  );
  ImGui::PopStyleColor (2);
  ImGui::EndGroup      ( );
}


extern bool SK_D3D11_ShaderModDlg   (SK_TLS *pTLS = SK_TLS_Bottom ());
extern void SK_DXGI_UpdateLatencies (IDXGISwapChain *pSwapChain);


using SK_ReShade_OnDrawD3D11_pfn =
void (__stdcall *)(void*, ID3D11DeviceContext*, unsigned int);

//extern SK_RESHADE_CALLBACK_DRAW SK_ReShade_DrawCallback;

bool
SK::ControlPanel::D3D11::Draw (void)
{
  SK_TLS* pTLS =
    SK_TLS_Bottom ();

  if (show_shader_mod_dlg)
      show_shader_mod_dlg = SK_D3D11_ShaderModDlg ();

  const SK_RenderBackend &rb =
    SK_GetCurrentRenderBackend ();

  const bool d3d11 =
    static_cast <int> (render_api) & static_cast <int> (SK_RenderAPI::D3D11);
  const bool d3d12 =
    static_cast <int> (render_api) & static_cast <int> (SK_RenderAPI::D3D12);
  const bool vulkan =
    SK_DXGI_VK_INTEROP_TYPE_NONE !=
       SK_Render_GetVulkanInteropSwapChainType (rb.swapchain);

  // Is the underlying graphics API actually something else?
  const bool indirect =
    ( SK_GL_OnD3D11 || vulkan );

  bool uncollapsed = false;

  if (SK_GL_OnD3D11)
    uncollapsed = ImGui::CollapsingHeader ("OpenGL-IK Settings",   ImGuiTreeNodeFlags_DefaultOpen);
  else if (vulkan)
    uncollapsed = ImGui::CollapsingHeader ("Vulkan Settings",      ImGuiTreeNodeFlags_DefaultOpen);
  else if (d3d11)
    uncollapsed = ImGui::CollapsingHeader ("Direct3D 11 Settings", ImGuiTreeNodeFlags_DefaultOpen);
  else if (d3d12)
    uncollapsed = ImGui::CollapsingHeader ("Direct3D 12 Settings", ImGuiTreeNodeFlags_DefaultOpen);

  if (uncollapsed)
  {
    if (d3d11 && (! indirect))
    {
      ImGui::SameLine ();
      ImGui::TextUnformatted ("     State Tracking:  ");

      ImGui::PushStyleColor (ImGuiCol_Text, (ImVec4&&)ImColor::HSV (0.173f, 0.428f, 0.96f));
      ImGui::SameLine ();

      if (SK_D3D11_EnableTracking)
        ImGui::TextUnformatted ("( ALL State/Ops --> [Mod Tools Window Active] )");

      else
      {
        char* szThreadLocalStr =
                        pTLS->scratch_memory->cmd.alloc (
                          256,   true                  );

        bool tracking = false;

        if ( ( ReadAcquire (&SK_D3D11_DrawTrackingReqs) > 0 ||
               SK_D3D11_Shaders->hasReShadeTriggers () ) )
        {
          tracking = true;

          if (ReadAcquire (&SK_D3D11_DrawTrackingReqs) == 0)
            lstrcatA (szThreadLocalStr,   "  Draw Calls  ( ReShade Trigger ) ");

          else
          {
            if (SK_D3D11_Shaders->hasReShadeTriggers ())
              lstrcatA (szThreadLocalStr, "  Draw Calls  ( Generic & ReShade Trigger ) ");
            else
              lstrcatA (szThreadLocalStr, "  Draw Calls  ( Generic ) ");
          }
        }

        if (ReadAcquire (&SK_D3D11_CBufferTrackingReqs) > 0)
        {
          tracking = true;
          lstrcatA (szThreadLocalStr, "  Constant Buffers ");
        }

        if (SK_D3D11_EnableMMIOTracking)
        {
          tracking = true;
          lstrcatA (szThreadLocalStr, "  Memory-Mapped I/O ");
        }

        ImGui::TextUnformatted (tracking ? szThreadLocalStr : " ");
      }
      ImGui::PopStyleColor ();

      ImGui::PushStyleColor (ImGuiCol_Header,        ImVec4 (0.90f, 0.68f, 0.02f, 0.45f));
      ImGui::PushStyleColor (ImGuiCol_HeaderHovered, ImVec4 (0.90f, 0.72f, 0.07f, 0.80f));
      ImGui::PushStyleColor (ImGuiCol_HeaderActive,  ImVec4 (0.87f, 0.78f, 0.14f, 0.80f));
      ImGui::TreePush ("");

      const bool filtering =
        ImGui::CollapsingHeader ("Texture Filtering");

      if (filtering)
      {
        ImGui::TreePush ("");

        static bool restart_warning = false;

        if (ImGui::Checkbox ("Force Anisotropic Filtering", &config.render.d3d12.force_anisotropic))
        {
          restart_warning = true;

          config.utility.save_async ();
        }

        if (ImGui::IsItemHovered ())
            ImGui::SetTooltip ("Upgrade standard bilinear or trilinear filtering to anisotropic");

        ImGui::SameLine ();

        if (ImGui::SliderInt ("Anistropic Level", &config.render.d3d12.max_anisotropy, -1, 16,
                                                   config.render.d3d12.max_anisotropy > 0 ? "%dx" : "Game Default"))
        {
          restart_warning = true;

          config.utility.save_async ();
        }

        if (ImGui::IsItemHovered ())
            ImGui::SetTooltip ("Force maximum anisotropic filtering level, for native anisotropic "
                               "filtered render passes as well as any forced.");

        if (ImGui::SliderFloat ("Mipmap LOD Bias", &config.render.d3d12.force_lod_bias, -5.0f, 5.0f,
                                                    config.render.d3d12.force_lod_bias == 0.0f ? "Game Default" : "%3.2f"))
        {
          restart_warning = true;

          config.utility.save_async ();
        }

        if (ImGui::IsItemHovered ())
            ImGui::SetTooltip    ("Use a small (i.e. -0.6'ish) negative LOD bias to sharpen DLSS and FSR games");

        if (restart_warning)
        {
          ImGui::PushStyleColor (ImGuiCol_Text, ImColor::HSV (.3f, .8f, .9f).Value);
          ImGui::BulletText     ("Game Restart Required");
          ImGui::PopStyleColor  ();
        }

        ImGui::TreePop     ( );
      } ImGui::TreePop     ( );
      ImGui::PopStyleColor (3);
    }

    // D3D12
    else if (! indirect)
    {
      if (SK_DStorage_IsLoaded ())
      {
        ImGui::PushStyleColor (ImGuiCol_Header,        ImVec4 (0.90f, 0.68f, 0.02f, 0.45f));
        ImGui::PushStyleColor (ImGuiCol_HeaderHovered, ImVec4 (0.90f, 0.72f, 0.07f, 0.80f));
        ImGui::PushStyleColor (ImGuiCol_HeaderActive,  ImVec4 (0.87f, 0.78f, 0.14f, 0.80f));
        ImGui::TreePush       ("");

        bool bUncollapsedDirectStorage =
          ImGui::CollapsingHeader ("Direct Storage", ImGuiTreeNodeFlags_DefaultOpen);

        if (bUncollapsedDirectStorage)
        {
          ImGui::BeginGroup ();
          ImGui::
            TextUnformatted ("GDeflate Support: ");
          ImGui::
            TextUnformatted ("GDeflate Usage: ");
          ImGui::EndGroup   ();
          ImGui::SameLine   ();
          ImGui::BeginGroup ();
          DSTORAGE_COMPRESSION_SUPPORT gdeflate_support =
            SK_DStorage_GetGDeflateSupport ();
          if (gdeflate_support == DSTORAGE_COMPRESSION_SUPPORT::DSTORAGE_COMPRESSION_SUPPORT_NONE)
          {
            ImGui::TextUnformatted ("N/A");
          }
          else
          {
            const bool gdeflate_cpu_fallback =
              (gdeflate_support & DSTORAGE_COMPRESSION_SUPPORT_CPU_FALLBACK) != 0;
            const bool gdeflate_gpu_fallback =
              (gdeflate_support & DSTORAGE_COMPRESSION_SUPPORT_GPU_FALLBACK) != 0;
            ImGui::Text     (
              "%hs, using %hs",
                ( gdeflate_cpu_fallback ? "CPU Fallback"
                                        :
                  gdeflate_gpu_fallback ? "GPU Fallback"
                                        : "GPU Optimized" ),
                    (gdeflate_support & DSTORAGE_COMPRESSION_SUPPORT_USES_COMPUTE_QUEUE) != 0 ?
                                          "Compute Queue"
                                        : "Copy Queue"
            );
          }
          ImGui::
            TextUnformatted ( SK_DStorage_IsUsingGDeflate () ? "Yes"
                                                             : "No" );
          ImGui::EndGroup   ();

#if 0
          ImGui::BeginGroup ();
          ImGui::
            TextUnformatted ("IO Request Submit Threads: ");
          ImGui::
            TextUnformatted ("CPU Decompression Threads: ");
          ImGui::EndGroup   ();
          ImGui::SameLine   ();
          ImGui::BeginGroup ();
          ImGui::Text       ("%lu", SK_DStorage_GetNumSubmitThreads                  ());
          ImGui::Text       ("%li", SK_DStorage_GetNumBuiltInCpuDecompressionThreads ());
          ImGui::EndGroup   ();
#endif

          if (SK_GetCurrentGameID () == SK_GAME_ID::RatchetAndClank_RiftApart)
          {
            if (ImGui::TreeNode ("Priorities (Ratchet and Clank)"))
            {
              auto &dstorage =
                SK_GetDLLConfig ()->get_section (L"RatchetAndClank.DStorage");

              bool changed = false;

              auto PriorityComboBox = [&](const char *szName, const wchar_t *wszKey)->bool
              {
                int prio =
                  SK_DStorage_PriorityFromStr (dstorage.get_cvalue (wszKey).c_str ()) + 1;

                bool changed =
                  ImGui::Combo (szName, &prio, "Low\0Normal\0High\0Realtime\0\0");

                if (ImGui::IsItemHovered ())
                    ImGui::SetTooltip ("A game restart is required to change this...");

                if (changed)
                {
                  dstorage.add_key_value (wszKey,
                    SK_DStorage_PriorityToStr ((DSTORAGE_PRIORITY)(prio - 1))
                  );
                }

                return changed;
              };

              changed |=
                PriorityComboBox (      "Bulk Load",           L"BulkPriority");
              changed |=
                PriorityComboBox (    "Loose reads",      L"LooseReadPriority");
              changed |=
                PriorityComboBox (        "Texture",        L"TexturePriority");
              changed |=
                PriorityComboBox ("NxStorage Index", L"NxStorageIndexPriority");

              if (changed)
                config.utility.save_async ();

              ImGui::TreePop  ();
            }
          }

          if (ImGui::TreeNode ("Overrides"))
          {
            bool changed = false;

            changed |=
              ImGui::Checkbox ("Disable BypassIO", &config.render.dstorage.disable_bypass_io);

            changed |=
              ImGui::Checkbox ("Disable GPU Decompression", &config.render.dstorage.disable_gpu_decomp);

            changed |=
              ImGui::Checkbox ("Force File Buffering", &config.render.dstorage.force_file_buffering);

            changed |=
              ImGui::Checkbox ("Disable Telemetry", &config.render.dstorage.disable_telemetry);

            if (changed)
              config.utility.save_async ();

            ImGui::TreePop ();
          }
        }

        ImGui::TreePop       ( );
        ImGui::PopStyleColor (3);
      }

#ifdef D3D12_STATE_TRACK
#pragma region "Advanced"
      auto currentFrame =
        SK_GetFramesDrawn ();

      if ( (config.render.dxgi.allow_d3d12_footguns || config.reshade.is_addon) &&
           ImGui::TreeNode ("Recently Used Shaders")
         )
      {
        static auto constexpr _RECENT_USE_THRESHOLD = 30;
        static auto constexpr _ACTIVE_THRESHOLD     = 300;

        static constexpr GUID SKID_D3D12DisablePipelineState =
          { 0x3d5298cb, 0xd9f0,  0x6133, { 0xa1, 0x9d, 0xb1, 0xd5, 0x97, 0x92, 0x00, 0x70 } };

        static constexpr GUID SKID_D3D12KnownVtxShaderDigest =
          { 0x4d5298ca, 0xd9f0,  0x6133, { 0xa1, 0x9d, 0xb1, 0xd5, 0x97, 0x92, 0x00, 0x00 } };

        static constexpr GUID SKID_D3D12KnownPixShaderDigest =
          { 0x4d5298ca, 0xd9f0,  0x6133, { 0xa1, 0x9d, 0xb1, 0xd5, 0x97, 0x92, 0x00, 0x01 } };

        static auto constexpr   DxilContainerHashSize  = 16;
        std::multimap <uint32_t, ID3D12PipelineState *> shaders;

        for ( auto &[ps, live] : _pixelShaders )
        {
          if (! live) continue;

          UINT   size      = 8UL;
          UINT64 lastFrame = 0ULL;

          ps->GetPrivateData (SKID_D3D12LastFrameUsed, &size, &lastFrame);

        //if (currentFrame > lastFrame + _RECENT_USE_THRESHOLD) continue;

                  size =  DxilContainerHashSize;
          uint8_t digest [DxilContainerHashSize];

          ps->GetPrivateData ( SKID_D3D12KnownPixShaderDigest, &size, digest );

          shaders.emplace (
            crc32c (0x0, digest, 16), ps
          );
        }

        std::string name (" ", MAX_PATH + 1);

        ImGui::BeginGroup ();
        for ( const auto &[bucket, dump] : shaders )
        {
          bool   disable = false;
          UINT   size    = sizeof (bool);

          if ( FAILED ( dump->GetPrivateData (
                          SKID_D3D12DisablePipelineState, &size, &disable
             )        )                    ) {             size=1;disable=false; }

          bool enable =
            (! disable);

          ImGui::PushID ((int)(intptr_t)dump);

          auto range =
            shaders.equal_range (bucket);

          if (ImGui::Checkbox (" Pixel::", &enable))
          {
            disable =
              (! enable);

            size =
              sizeof (bool);

            for ( auto it = range.first ; it != range.second ; ++it )
            {
              it->second->SetPrivateData (
                SKID_D3D12DisablePipelineState, size, &disable
              );
            }
          }
          
                  size =  DxilContainerHashSize;
          uint8_t digest [DxilContainerHashSize];

          dump->GetPrivateData ( SKID_D3D12KnownPixShaderDigest, &size, digest );

          std::string out =
            std::format ("{:x}{:x}{:x}{:x}{:x}{:x}{:x}{:x}"
                         "{:x}{:x}{:x}{:x}{:x}{:x}{:x}{:x}",
                           digest[ 0],digest[ 1],digest[ 2],digest[ 3],
                           digest[ 4],digest[ 5],digest[ 6],digest[ 7],
                           digest[ 8],digest[ 9],digest[10],digest[11],
                           digest[12],digest[13],digest[14],digest[15]);

          if (ImGui::IsItemClicked (1))
          {
            SK_LOG0 ( ( L"%hs", out.c_str () ), L"   DXGI   " );
          }

          ImGui::SameLine ();
          ImGui::Text     ("  %hs", out.c_str ());
          ImGui::PopID    ();
        }
        ImGui::EndGroup   ();
        //ImGui::SameLine   ();
        //ImGui::BeginGroup ();
        //
        //for ( auto &[ps, live] : _vertexShaders )
        //{
        //  if (! live) continue;
        //
        //  UINT   size      = 8UL;
        //  UINT64 lastFrame = 0ULL;
        //
        //  ps->GetPrivateData (SKID_D3D12LastFrameUsed, &size, &lastFrame);
        //
        //  if (currentFrame > lastFrame + _RECENT_USE_THRESHOLD) continue;
        //
        //  *name.data () = '\0';
        //
        //  if ( UINT                                               uiStrLen = MAX_PATH ;
        //FAILED ( ps->GetPrivateData ( WKPDID_D3DDebugObjectName, &uiStrLen, name.data () )
        //     ) ) { *name.data () = '\0'; }
        //  
        //  ImGui::PushID ((int)(intptr_t)ps);
        //
        //  if (ImGui::InputText (SK_FormatString ("  %08x", ps).c_str (),
        //                                                  name.data  (), MAX_PATH))
        //    SK_D3D12_SetDebugName (ps, SK_UTF8ToWideChar (name));
        //
        //  ImGui::PopID ();
        //}
        //ImGui::EndGroup ();

        ImGui::Separator ();

        ImGui::BeginGroup ();
        for ( auto &[ps, str] : _latePSOBlobs )
        {
          bool   disable   = false;
          UINT   size      = sizeof (UINT64);
          UINT64 lastFrame = 0;

          ps->GetPrivateData (SKID_D3D12LastFrameUsed, &size, &lastFrame);

          if (currentFrame > lastFrame + _ACTIVE_THRESHOLD) continue;

          size =
            sizeof (bool);

          if ( FAILED ( ps->GetPrivateData (
                          SKID_D3D12DisablePipelineState, &size, &disable
             )        )                    ) {             size=1;disable=false; }

          bool enable =
            (! disable);

          ImGui::PushID ((int)(intptr_t)ps);

          if (ImGui::Checkbox (" Other::", &enable))
          {
            disable =
              (! enable);

            ps->SetPrivateData (SKID_D3D12DisablePipelineState, size, &disable);
          }

          ImGui::PopID  ();
        }
        ImGui::EndGroup   ();
        ImGui::SameLine   ();
        ImGui::BeginGroup ();

        for ( auto &[ps, str] : _latePSOBlobs )
        {
          UINT   size      = sizeof (UINT64);
          UINT64 lastFrame = 0;

          ps->GetPrivateData (SKID_D3D12LastFrameUsed, &size, &lastFrame);

          if (currentFrame > lastFrame + _ACTIVE_THRESHOLD) continue;

          *name.data () = '\0';

          ImGui::PushStyleColor ( ImGuiCol_Text,
            currentFrame > lastFrame - _RECENT_USE_THRESHOLD   ?
                           ImColor (0.5f,0.5f,0.5f,1.0f).Value :
                           ImColor (1.0f,1.0f,1.0f,1.0f).Value );

          if ( UINT                                               uiStrLen = MAX_PATH ;
        FAILED ( ps->GetPrivateData ( WKPDID_D3DDebugObjectName, &uiStrLen, name.data () )
             ) ) { *name.data () = '\0'; }

          ImGui::PopStyleColor ();
          
          ImGui::PushID ((int)(intptr_t)ps);

          if (ImGui::InputText (str.c_str (),             name.data (), MAX_PATH))
            SK_D3D12_SetDebugName (ps, SK_UTF8ToWideChar (name));

          ImGui::PopID  ();
        }
        ImGui::EndGroup ();
        ImGui::TreePop  ();
      }
#endif
#pragma endregion
      ImGui::PushStyleColor (ImGuiCol_Header,        ImVec4 (0.90f, 0.68f, 0.02f, 0.45f));
      ImGui::PushStyleColor (ImGuiCol_HeaderHovered, ImVec4 (0.90f, 0.72f, 0.07f, 0.80f));
      ImGui::PushStyleColor (ImGuiCol_HeaderActive,  ImVec4 (0.87f, 0.78f, 0.14f, 0.80f));
      ImGui::TreePush ("");

      const bool filtering =
        ImGui::CollapsingHeader ("Texture Filtering");

      if (filtering)
      {
        ImGui::TreePush ("");

        static bool restart_warning = false;

        if (ImGui::Checkbox ("Force Anisotropic Filtering", &config.render.d3d12.force_anisotropic))
        {
          restart_warning = true;

          config.utility.save_async ();
        }

        if (ImGui::IsItemHovered ())
            ImGui::SetTooltip ("Upgrade standard bilinear or trilinear filtering to anisotropic");

        ImGui::SameLine ();

        if (ImGui::SliderInt ("Anistropic Level", &config.render.d3d12.max_anisotropy, -1, 16,
                                                   config.render.d3d12.max_anisotropy > 0 ? "%dx" : "Game Default"))
        {
          restart_warning = true;

          config.utility.save_async ();
        }

        if (ImGui::IsItemHovered ())
            ImGui::SetTooltip ("Force maximum anisotropic filtering level, for native anisotropic "
                               "filtered render passes as well as any forced.");

        if (ImGui::SliderFloat ("Mipmap LOD Bias", &config.render.d3d12.force_lod_bias, -5.0f, 5.0f,
                                                    config.render.d3d12.force_lod_bias == 0.0f ? "Game Default" : "%3.2f"))
        {
          restart_warning = true;

          config.utility.save_async ();
        }

        if (ImGui::IsItemHovered ())
            ImGui::SetTooltip    ("Use a small (i.e. -0.6'ish) negative LOD bias to sharpen DLSS and FSR games");

        if (restart_warning)
        {
          ImGui::PushStyleColor (ImGuiCol_Text, ImColor::HSV (.3f, .8f, .9f).Value);
          ImGui::BulletText     ("Game Restart Required");
          ImGui::PopStyleColor  ();
        }

        ImGui::TreePop     ( );
      } ImGui::TreePop     ( );
      ImGui::PopStyleColor (3);
    }

    SK_NGX_DLSS_ControlPanel ();

    ImGui::PushStyleColor (ImGuiCol_Header,        ImVec4 (0.90f, 0.68f, 0.02f, 0.45f));
    ImGui::PushStyleColor (ImGuiCol_HeaderHovered, ImVec4 (0.90f, 0.72f, 0.07f, 0.80f));
    ImGui::PushStyleColor (ImGuiCol_HeaderActive,  ImVec4 (0.87f, 0.78f, 0.14f, 0.80f));
    ImGui::TreePush       ("");

    const bool swapchain =
      ImGui::CollapsingHeader ("SwapChain Management");

    if (ImGui::IsItemHovered ())
    {
      ImGui::BeginTooltip   ();
      ImGui::TextColored    ( ImColor (235, 235, 235),
                              "Latency and Framepacing Tweaks" );
      ImGui::EndTooltip     ();
    }

    if (swapchain)
    {
      auto _ResetLimiter = [&](void) -> void
      {
        auto *pLimiter =
          SK::Framerate::GetLimiter (rb.swapchain, false);

        if (pLimiter != nullptr)
            pLimiter->reset (true);
      };

      static bool flip         = config.render.framerate.flip_discard;
      static bool waitable     = config.render.framerate.swapchain_wait > 0;
      static int  buffer_count = config.render.framerate.buffer_count;
      static int  prerender    = config.render.framerate.pre_render_limit;

      ImGui::TreePush ("");

      ImGui::BeginGroup ();

      int iLeftGroupCount = 0;

      if (! indirect)
      {
        iLeftGroupCount++;

        if (d3d12)
          ImGui::Checkbox   ("Force Flip Discard in D3D12", &config.render.framerate.flip_discard);
        else
        {
          ImGui::Checkbox   ("Use Flip Model Presentation", &config.render.framerate.flip_discard);

          if (ImGui::IsItemHovered ())
          {
            ImGui::BeginTooltip ();
            ImGui::Text         ("High-Performance Windowed Rendering");
            ImGui::Separator    ();
            ImGui::BulletText   ("Makes Windowed Mode Perform Same as Fullscreen Exclusive");
            ImGui::EndTooltip   ();
          }
        }
      }

      iLeftGroupCount++;

      bool clamp_sync_interval =
        (config.render.framerate.sync_interval_clamp > 0);

      if (ImGui::Checkbox ("Clamp Presentation Interval", &clamp_sync_interval))
      {
        if (clamp_sync_interval)
        {
          config.render.framerate.sync_interval_clamp = 1;
          config.render.framerate.present_interval =
            std::min (config.render.framerate.present_interval,
                      config.render.framerate.sync_interval_clamp);
        }
        else
          config.render.framerate.sync_interval_clamp = SK_NoPreference;
      }

      if (ImGui::IsItemHovered ())
      {
        ImGui::BeginTooltip    ();
        ImGui::TextUnformatted ("Prevent games from setting Presentation Intervals incompatible with VRR");
        ImGui::Separator       ();
        ImGui::BulletText      ("Intervals > 1 disable VRR and switch to Fixed-Refresh");
        ImGui::BulletText      ("Interval 0 may also disable VRR, if framerate exceeds refresh");
        ImGui::EndTooltip      ();
      }

      if (config.render.framerate.flip_discard)
      {
#if 0
        bool waitable_ = config.render.framerate.swapchain_wait > 0;

        if (! ((d3d12 && !config.render.dxgi.allow_d3d12_footguns) || indirect))
        {
          if (ImGui::Checkbox ("Waitable SwapChain", &waitable_))
          {
            if (! waitable_) config.render.framerate.swapchain_wait = 0;
            else             config.render.framerate.swapchain_wait = 15;

            if (waitable_)
            {
              // Setup default values when first turned on
              if (config.render.framerate.pre_render_limit == SK_NoPreference)
              {
                config.render.framerate.pre_render_limit =
                  std::min (
                    std::max ( config.render.framerate.buffer_count + 1, 3 ),
                                                                         3 );
              }

              else
                config.render.framerate.pre_render_limit = SK_NoPreference;
            }

            _ResetLimiter ();
          }

          static bool magic_stuff  = false;
                      magic_stuff |= ImGui::IsItemClicked (1);

          if (ImGui::IsItemHovered ())
          {
            ImGui::BeginTooltip ();
            ImGui::Text         ("Reduces Input Latency in SK's Framerate Limiter");
            if (rb.api != SK_RenderAPI::D3D12)
            {
              ImGui::Separator  ();
              ImGui::BulletText ("Fullscreen Exclusive will not work while enabled");
              ImGui::BulletText ("Fullscreen Exclusive is obsolete");
            }
            ImGui::EndTooltip   ();
          }

          if (waitable_ && magic_stuff)
          {
            ImGui::SameLine   ();
            ImGui::BeginGroup ();
            ImGui::InputFloat ("SwapWaitFract", &fSwapWaitFract);
            ImGui::InputFloat ("SwapWaitRatio", &fSwapWaitRatio);
            ImGui::EndGroup   ();
          }
        }
#endif

        if (SK_DXGI_SupportsTearing ())
        {
          iLeftGroupCount++;

          bool bIsAdaptiveLatentSync =
            ( config.render.framerate.present_interval == 0 ) &&
            ( config.render.framerate.target_fps > 0.0f     ) &&
            ( config.render.framerate.tearing_mode ==
                SK_TearingMode::AdaptiveOn                 ||
              config.render.framerate.tearing_mode ==
                SK_TearingMode::AdaptiveOff                 );

          bool bIsAdaptiveVSync      =
            ( config.render.framerate.present_interval >= 1 ) &&
            ( config.render.framerate.target_fps > 0.0f     ) &&
            ( config.render.framerate.tearing_mode ==
                SK_TearingMode::AdaptiveOff                 );

          if (bIsAdaptiveVSync || bIsAdaptiveLatentSync)
          {
            ImGui::PushItemFlag (
              ImGuiItemFlags_Disabled,
              true
            );
          }

          if  (
                ImGui::Checkbox (
                  bIsAdaptiveVSync || bIsAdaptiveLatentSync
                    ? "Enable Tearing (Adaptive)"
                    : "Enable Tearing",
                  bIsAdaptiveVSync
                    ? &config.render.framerate.turn_vsync_off
                    : &config.render.dxgi.allow_tearing
                )
              )
          {
            // Latent Sync
            if ( config.render.framerate.present_interval == 0 &&
                 config.render.framerate.target_fps > 0.0f     )
            {
              static int iLastAlwaysOffTearingMode =
                SK_TearingMode::AlwaysOff;

              if ( config.render.framerate.tearing_mode == SK_TearingMode::AlwaysOff_LowLatency ||
                   config.render.framerate.tearing_mode == SK_TearingMode::AlwaysOff            )
              {
                iLastAlwaysOffTearingMode =
                  config.render.framerate.tearing_mode;
              }

              config.render.framerate.tearing_mode =
                config.render.dxgi.allow_tearing
                  ? SK_TearingMode::AlwaysOn
                  : iLastAlwaysOffTearingMode;
            }

            _ResetLimiter ();
          }

          if (ImGui::IsItemHovered (ImGuiHoveredFlags_AllowWhenDisabled))
          {
            ImGui::BeginTooltip ();
            if (! (bIsAdaptiveVSync || bIsAdaptiveLatentSync))
            {
              ImGui::Text       ("Enables True VSYNC -OFF- in Windowed Mode");
              ImGui::Separator  ();
              ImGui::BulletText ("Presentation Interval 0 will turn VSYNC off");
            }
            else
            {
              if (bIsAdaptiveVSync)
              {
                ImGui::Text     ("Tearing is currently managed by \"Adaptive V-Sync\" mode");
              }
              else
              {
                if (config.render.framerate.tearing_mode == SK_TearingMode::AdaptiveOn)
                {
                  ImGui::Text   ("Tearing is currently managed by \"Adaptive (Prefer On)\" mode");
                }
                else
                {
                  ImGui::Text   ("Tearing is currently managed by \"Adaptive (Prefer Off)\" mode");
                }
              }
              ImGui::Separator  ();
              ImGui::BulletText ("For manual control, change Tearing Mode to \"Always On/Off\"");
            }
            ImGui::EndTooltip   ();
          }

          if (bIsAdaptiveVSync  ||  bIsAdaptiveLatentSync)
          {
            ImGui::PopItemFlag  ();
          }
        }
      }

      if (! (d3d12 && !config.render.dxgi.allow_d3d12_footguns))
      {
        for ( int i = 0; i < 3 - iLeftGroupCount; i++ )
        {
          ImGui::Dummy (ImVec2 (0.0f, ImGui::GetFrameHeight ()));
        }
      }

      const float ui_scale =
        config.imgui.scale;

      ImGui::EndGroup   ();

      ImGui::SameLine   ();
      ImGui::ItemSize   (ImVec2 (50.0f * ui_scale, 0.0f));
      ImGui::SameLine   ();

      ImGui::SeparatorEx (ImGuiSeparatorFlags_Vertical);

      ImGui::SameLine   ();
      ImGui::ItemSize   (ImVec2 (50.0f * ui_scale, 0.0f));
      ImGui::SameLine   ();

      ImGui::BeginGroup ();
      ImGui::PushItemWidth (100.0f * ui_scale);

      bool present_interval_changed =
      ImGui::InputInt ("Presentation Interval",       &config.render.framerate.present_interval);

      if (ImGui::IsItemHovered ())
      {
        ImGui::BeginTooltip ();

        ImGui::Text       ("This Controls V-Sync");
        ImGui::Separator  (                                               );
        ImGui::BulletText ("-1=Game Controlled,  0=Force Off,  1=Force On");

        //if (config.render.framerate.drop_late_flips && config.render.framerate.present_interval != 0)
        //  ImGui::BulletText ("Values > 1 do not Apply unless \"Drop Late Frames\" or SK's Framerate Limiter are Disabled");
        //else
          ImGui::BulletText (">1=Fractional Refresh Rates");

        ImGui::EndTooltip ();
      }

      config.render.framerate.present_interval =
        std::max (-1, std::min (4, config.render.framerate.present_interval));

      if (present_interval_changed && config.render.framerate.present_interval > 1 &&
                                      config.render.framerate.sync_interval_clamp != SK_NoPreference)
      {
        SK_ImGui_Warning (
          L"Fractional VSYNC Rates Will Prevent VRR From Working\r\n\r\n\t>>"
          L" SyncIntervalClamp Has Been Disabled (required to use 1/n Refresh VSYNC)"
        );

        config.render.framerate.sync_interval_clamp = SK_NoPreference;
      }

      if (! (d3d12 && !config.render.dxgi.allow_d3d12_footguns))
      {
        if (ImGui::InputInt ("BackBuffer Count", &config.render.framerate.buffer_count))
        {
          auto& io =
            ImGui::GetIO ();

          if (config.render.framerate.buffer_count > 0)
          {
            if (config.render.framerate.flip_discard)
                config.render.framerate.buffer_count = std::min (15, std::max (2, config.render.framerate.buffer_count));
            else
                config.render.framerate.buffer_count = std::min (15, std::max (1, config.render.framerate.buffer_count));

            // Trigger a compliant game to invoke IDXGISwapChain::ResizeBuffers (...)
            PostMessage (SK_GetGameWindow (), WM_SIZE, SIZE_RESTORED, MAKELPARAM ( (LONG)io.DisplaySize.x,
                                                                                   (LONG)io.DisplaySize.y ) );

            _ResetLimiter ();
          }
        }
      }

      // Clamp to [-1,oo)
      if (config.render.framerate.buffer_count <  0)
          config.render.framerate.buffer_count = SK_NoPreference;

      if (! (d3d12 && !config.render.dxgi.allow_d3d12_footguns))
      {
        if (ImGui::InputInt ("Maximum Device Latency", &config.render.framerate.pre_render_limit))
        {
          if (config.render.framerate.pre_render_limit <  0)
              config.render.framerate.pre_render_limit = SK_NoPreference;

          else
          {
            if (config.render.framerate.buffer_count > 0)
                config.render.framerate.pre_render_limit = std::min ( config.render.framerate.pre_render_limit,
                                                                        config.render.framerate.buffer_count + 1 );

            SK_ComQIPtr <IDXGISwapChain>
                pSwapChain (rb.swapchain);
            if (pSwapChain != nullptr)
            {
              SK_DXGI_UpdateLatencies (pSwapChain);

              _ResetLimiter ();
            }
          }
        }
      }
      ImGui::PopItemWidth ();
      ImGui::EndGroup ();

      const bool changed =
        (flip         != config.render.framerate.flip_discard      ) ||
        (waitable     != config.render.framerate.swapchain_wait > 0) ||
        (buffer_count != config.render.framerate.buffer_count      ) ||
        (prerender    != config.render.framerate.pre_render_limit);

      if (changed)
      {
        ImGui::PushStyleColor (ImGuiCol_Text, (ImVec4&&)ImColor::HSV (.3f, .8f, .9f));
        ImGui::BulletText     ("Game Restart Required");
        ImGui::PopStyleColor  ();
      }
      ImGui::TreePop  ();
    }

    if (d3d11 && (! indirect) && ImGui::CollapsingHeader ("Texture Management"))
    {
      ImGui::TreePush ("");
      ImGui::Checkbox ("Enable Texture Caching", &config.textures.d3d11.cache);

      if (ImGui::IsItemHovered ())
      {
        ImGui::BeginTooltip    ();
        ImGui::TextUnformatted ("Reduces Driver Memory Management Overhead in Games that Stream Textures");

        static bool orig_cache = config.textures.d3d11.cache;

        if (orig_cache)
        {
#if 0
          LONG contains_ = 0L;
          LONG erase_    = 0L;
          LONG index_    = 0L;

          std::pair <int, std::pair <LONG, std::pair <LONG, char*>>> busiest = { 0, { 0, { 0, "Invalid" } } };

          int idx = 0;

          for (auto it : SK_D3D11_Textures.HashMap_2D)
          {
            LONG i = ReadAcquire (&it.contention_score.index);
            LONG c = ReadAcquire (&it.contention_score.contains);
            LONG a = 0L;
            LONG e = ReadAcquire (&it.contention_score.erase);

            a = ( i + c + a + e );

            if ( idx > 0 && busiest.second.first < a )
            {
              busiest.first        = idx;
              busiest.second.first = a;

              LONG max_val =
                std::max (i, std::max (c, e));

              if (max_val == i)
                busiest.second.second.second = "operator []()";
              else if (max_val == c)
                busiest.second.second.second = "contains ()";
              else if (max_val == e)
                busiest.second.second.second = "erase ()";
              else
                busiest.second.second.second = "unknown";

              busiest.second.second.first    = max_val;
            }

            ++idx;

            contains_ += c; erase_ += e; index_ += i;
          }

          if (busiest.first != 0)
          {
            ImGui::Separator  (                                 );
            ImGui::BeginGroup (                                 );
            ImGui::BeginGroup (                                 );
            ImGui::BulletText ( "HashMap Contains: "            );
            ImGui::BulletText ( "HashMap Erase:    "            );
            ImGui::BulletText ( "HashMap Index:    "            );
            ImGui::Text       ( ""                              );
            ImGui::BulletText ( "Most Contended:   "            );
            ImGui::EndGroup   (                                 );
            ImGui::SameLine   (                                 );
            ImGui::BeginGroup (                                 );
            ImGui::Text       ( "%li Ops", contains_            );
            ImGui::Text       ( "%li Ops", erase_               );
            ImGui::Text       ( "%li Ops", index_               );
            ImGui::Text       ( ""                              );
            ImGui::Text       ( R"(Mipmap LOD%02li (%li :: <"%s">))",
                                 busiest.first - 1,
                                   busiest.second.second.first,
                                   busiest.second.second.second );
            ImGui::EndGroup   (                                 );
            ImGui::EndGroup   (                                 );
            ImGui::SameLine   (                                 );
            ImGui::BeginGroup (                                 );
            int lod = 0;
            for ( auto it : SK_D3D11_Textures.HashMap_2D )
            {
              ImGui::BulletText ("LOD %02lu Load Factor: ", lod++);
            }
            ImGui::EndGroup   (                                 );
            ImGui::SameLine   (                                 );
            ImGui::BeginGroup (                                 );
            for ( auto it : SK_D3D11_Textures.HashMap_2D )
            {
              ImGui::Text     (" %.2f", it.entries.load_factor());
            }
            ImGui::EndGroup   (                                 );
          }
#endif
        }
        else
        {
          ImGui::Separator  (                                   );
          ImGui::BulletText ( "Requires Application Restart"    );
        }
        ImGui::EndTooltip   (                                   );
      }

      //ImGui::PushStyleColor (ImGuiCol_Text, ImVec4 (1.0f, 0.85f, 0.1f, 0.9f));
      //ImGui::SameLine (); ImGui::BulletText ("Requires restart");
      //ImGui::PopStyleColor  ();

      if (config.textures.d3d11.cache)
      {
        ImGui::SameLine ();
        ImGui::Spacing  ();
        ImGui::SameLine ();

        ImGui::Checkbox ("Ignore Textures Without Mipmaps", &config.textures.cache.ignore_nonmipped);

        if (ImGui::IsItemHovered ())
          ImGui::SetTooltip ("Important Compatibility Setting for Some Games (e.g. The Witcher 3)");

        ImGui::SameLine ();

        ImGui::Checkbox ("Cache Staged Texture Uploads", &config.textures.cache.allow_staging);

        if (ImGui::IsItemHovered ())
        {
          ImGui::BeginTooltip ();
          ImGui::Text         ("Enable Texture Dumping and Injection in Unity-based Games");
          ImGui::Separator    ();
          ImGui::BulletText   ("May cause degraded performance.");
          ImGui::BulletText   ("Try to leave this off unless textures are missing from the mod tools.");
          ImGui::EndTooltip   ();
        }
      }

      ImGui::TreePop  ();

      SK_ImGui_DrawTexCache_Chart ();
    }

    static bool enable_resolution_limits =
      ! ( config.render.dxgi.res.min.isZero () &&
          config.render.dxgi.res.max.isZero () );

    const bool res_limits =
      ImGui::CollapsingHeader ( "Resolution Limiting",
                          enable_resolution_limits ? ImGuiTreeNodeFlags_DefaultOpen : 0x00 );

    if (
      ImGui::BeginItemTooltip ())
    { ImGui::Text             ("Restrict the lowest/highest resolutions reported to a game");
      ImGui::Separator        ();
      ImGui::BulletText       ("Useful for games that compute aspect ratio based on the highest reported resolution.");
      ImGui::EndTooltip       ();
    }

    if (res_limits)
    {
      ImGui::TreePush  ("");
      ImGui::InputInt2 ("Minimum Resolution", reinterpret_cast <int *> (&config.render.dxgi.res.min.x));
      ImGui::InputInt2 ("Maximum Resolution", reinterpret_cast <int *> (&config.render.dxgi.res.max.x));

      // Fix for stupid users ... and stupid programmers who don't range validate
      //
      if (config.render.dxgi.res.max.x < config.render.dxgi.res.min.x && config.render.dxgi.res.min.x < 8192) config.render.dxgi.res.max.x = config.render.dxgi.res.min.x;
      if (config.render.dxgi.res.max.y < config.render.dxgi.res.min.y && config.render.dxgi.res.min.y < 8192) config.render.dxgi.res.max.y = config.render.dxgi.res.min.y;

      if (config.render.dxgi.res.min.x > config.render.dxgi.res.max.x && config.render.dxgi.res.max.x > 0)    config.render.dxgi.res.min.x = config.render.dxgi.res.max.x;
      if (config.render.dxgi.res.min.y > config.render.dxgi.res.max.y && config.render.dxgi.res.max.y > 0)    config.render.dxgi.res.min.y = config.render.dxgi.res.max.y;

      ImGui::TreePop   (  );
     }

    if (d3d11 && !indirect)
    {
      bool changed = false;

      if (ImGui::Button (" Render Mod Tools ")) show_shader_mod_dlg =
                                             (! show_shader_mod_dlg );
      ImGui::SameLine       ();
      changed |=
        ImGui::Checkbox     ("D3D11 Deferred Mode", &config.render.dxgi.deferred_isolation);
      ImGui::SetItemTooltip ("Try changing this option if textures / shaders are missing from the mod tools.");

      if (changed)
        config.utility.save_async ();
    }

    if (! config.reshade.is_addon)
    {
      // This only works when we have wrapped SwapChains
      if ( ReadAcquire (&SK_DXGI_LiveWrappedSwapChains)  != 0 ||
           ReadAcquire (&SK_DXGI_LiveWrappedSwapChain1s) != 0 )
      {
        if (d3d11 && !indirect) ImGui::SameLine ();

        OSD::DrawVideoCaptureOptions ();
      }
    }

    else
    {
      if (d3d11 && !indirect)
        ImGui::SameLine ();

      if (ImGui::Checkbox ("Draw ReShade First", &config.reshade.draw_first))
      {
        config.utility.save_async ();
      }

      if (ImGui::BeginItemTooltip ())
      {
        ImGui::TextUnformatted ("Apply ReShade Before SK Image Processing");
        ImGui::Separator       ();
        ImGui::PushStyleColor  (ImGuiCol_Text, ImVec4 (1.f, 1.f, 1.f, 1.f));
        ImGui::TextUnformatted ("Draw First");
        ImGui::PopStyleColor   ();
        ImGui::PushStyleColor  (ImGuiCol_Text, ImVec4 (.7f, .7f, .7f, 1.f));
        ImGui::BulletText      ("Intended for use with SDR, Native HDR and SK Inverse Tonemapped HDR.");
        ImGui::BulletText      ("This mode has highest performance and should be used by default.");
        ImGui::PopStyleColor   ();
        ImGui::Spacing         ();
        ImGui::Spacing         ();
        ImGui::PushStyleColor  (ImGuiCol_Text, ImVec4 (1.f, 1.f, 1.f, 1.f));
        ImGui::TextUnformatted ("Draw After");
        ImGui::PopStyleColor   ();
        ImGui::PushStyleColor  (ImGuiCol_Text, ImVec4 (.7f, .7f, .7f, 1.f));
        ImGui::BulletText      ("Required if using ReShade to analyze SK's HDR Tonemapping.");
        ImGui::BulletText      ("This mode has a slight performance penalty in D3D12.");
        ImGui::PopStyleColor   ();
        ImGui::EndTooltip      ();
      }
    }

    if (d3d11 && !indirect)
    {
      ImGui::SameLine ();

      if (ImGui::TreeNode ("Advanced (Debug)###Advanced_D3D11"))
      {
        ImGui::TreePop               ();
        ImGui::Separator             ();

        // Indirect K has no D3D11 depth buffer...
        if (! indirect)
        {
#ifdef _SUPPORT_ENHANCED_DEPTH
          ImGui::Checkbox ("Enhanced (64-bit) Depth+Stencil Buffer", &config.render.dxgi.enhanced_depth);

          if (ImGui::IsItemHovered ())
            ImGui::SetTooltip ("Requires application restart");
#endif

          if (sk::NVAPI::nv_hardware)
          {
#ifdef _SUPPORT_ENHANCED_DEPTH
            ImGui::SameLine              ();
#endif
            SK_ImGui_NV_DepthBoundsD3D11 ();
          }
        }

        ImGui::Checkbox ( "Enable D3D11 Debug Layer",
                    &config.render.dxgi.debug_layer );
      
        if (config.render.dxgi.debug_layer)
        {
          SK_ComQIPtr <ID3D11Debug>
              pDebugD3D11 (rb.device);
          if (pDebugD3D11 != nullptr)
          {
            static bool _pauseDebugOutput = false;

            ImGui::SameLine ();

            if (
              ImGui::Button ( _pauseDebugOutput ? "Resume Output"
                                                : "Pause Output" )
               ) _pauseDebugOutput = !_pauseDebugOutput;

            ImGui::SameLine ();
            
            bool bClearLog =
              ImGui::Button ("Clear Log");

            ImGui::SameLine ();

            static int                         minimum_severity = 2;
            ImGui::Combo ("Minimum Severity", &minimum_severity, "Corruption\0Error\0Warning\0Info\0Message\0\0");

            ImGui::BeginChild (
              ImGui::GetID ("D3D11_Debug_Panel"),
                    ImVec2 (0.0f, -1.0f),  true,
                      ImGuiWindowFlags_NavFlattened
            );

            static D3D11_MESSAGE_ID _debug_id;

            static std::vector <D3D11_MESSAGE_ID> allow_ids;
            static std::vector <D3D11_MESSAGE_ID> deny_ids;
            static std::vector <D3D11_MESSAGE_ID> break_ids;

            static iSK_INI *debug_ini = nullptr;

            SK_ComQIPtr <ID3D11InfoQueue>
                pInfoQueueD3D11 (rb.device);
            if (pInfoQueueD3D11.p != nullptr)
            {
              static bool        once = false;
              if (std::exchange (once, true) == false)
              {
                debug_ini =
                  SK_CreateINI (
                    SK_FormatStringW ( LR"(%ws\%ws)",
                      SK_GetConfigPath (), L"d3d11_debug.ini" ).c_str ()
                  );

                if (debug_ini->contains_section (L"Messages.Filter"))
                {
                  auto& sec =
                    debug_ini->get_section (L"Messages.Filter");

                  for ( auto& kvp : sec.keys )
                  {
                    D3D11_MESSAGE_ID id = (D3D11_MESSAGE_ID)
                      _wtoi (kvp.first.c_str ());

                    if (     kvp.second == L"Allow")
                    {
                      allow_ids.push_back (id);
                    }

                    else if (kvp.second == L"Deny")
                    {
                      deny_ids.push_back (id);
                    }
                  }

                  D3D11_INFO_QUEUE_FILTER filter = { };

                  filter.AllowList.NumIDs  = (UINT)allow_ids.size ();
                  filter.AllowList.pIDList =       allow_ids.data ();

                  filter.DenyList.NumIDs   = (UINT)deny_ids.size ();
                  filter.DenyList.pIDList  =       deny_ids.data ();

                  pInfoQueueD3D11->AddRetrievalFilterEntries (&filter);
                  pInfoQueueD3D11->AddStorageFilterEntries   (&filter);
                }

                if (debug_ini->contains_section (L"Messages.Break"))
                {
                  auto& sec =
                    debug_ini->get_section (L"Messages.Break");

                  for ( auto& kvp : sec.keys )
                  {
                    if (kvp.second == L"Enable")
                    {
                      break_ids.push_back ((D3D11_MESSAGE_ID)_wtoi (kvp.first.c_str ()));
                    }
                  }

                  for ( auto& msg_id : break_ids )
                  {
                    pInfoQueueD3D11->SetBreakOnID (msg_id, TRUE);
                  }
                }
              }

              if (ImGui::BeginPopup ("D3D11_Debug_MessageMenu"))
            {
              ImGui::Text ("Debug Message Configuration");

              ImGui::TreePush ("");

              bool deny =
                std::find (deny_ids.begin (), deny_ids.end (), _debug_id) != deny_ids.end ();

              bool break_ =
                std::find (break_ids.begin (), break_ids.end (), _debug_id) != break_ids.end ();

              if (ImGui::Checkbox ("Mute this message", &deny))
              {
                auto& sec =
                  debug_ini->get_section (L"Messages.Filter");

                if (deny)
                  sec.add_key_value (SK_FormatStringW (L"%d", _debug_id).c_str (), L"Deny");
                else
                  sec.add_key_value (SK_FormatStringW (L"%d", _debug_id).c_str (), L"Allow");

                if (deny)
                {
                  auto deny_id =
                    std::find (deny_ids.begin (), deny_ids.end (), _debug_id);

                  if (deny_id == deny_ids.end ())
                  {
                    deny_ids.push_back (_debug_id);
                  }

                  auto allow_id =
                    std::find (allow_ids.begin (), allow_ids.end (), _debug_id);

                  if (allow_id != allow_ids.end ())
                  {
                    allow_ids.erase (allow_id);
                  }
                }

                else
                {
                  auto allow_id =
                    std::find (allow_ids.begin (), allow_ids.end (), _debug_id);

                  if (allow_id == allow_ids.end ())
                  {
                    allow_ids.push_back (_debug_id);
                  }

                  auto deny_id =
                    std::find (deny_ids.begin (), deny_ids.end (), _debug_id);

                  if (deny_id != deny_ids.end ())
                  {
                    deny_ids.erase (deny_id);
                  }
                }

                debug_ini->write ();

                D3D11_INFO_QUEUE_FILTER filter = { };

                filter.AllowList.NumIDs  = (UINT)allow_ids.size ();
                filter.AllowList.pIDList =       allow_ids.data ();

                filter.DenyList.NumIDs   = (UINT)deny_ids.size ();
                filter.DenyList.pIDList  =       deny_ids.data ();

                pInfoQueueD3D11->ClearRetrievalFilter ();
                pInfoQueueD3D11->ClearStorageFilter   ();

                pInfoQueueD3D11->AddRetrievalFilterEntries (&filter);
                pInfoQueueD3D11->AddStorageFilterEntries   (&filter);
              }

              if (ImGui::Checkbox ("Break on this message", &break_))
              {
                auto& sec =
                  debug_ini->get_section (L"Messages.Break");

                if (break_)
                  sec.add_key_value (SK_FormatStringW (L"%d", _debug_id).c_str (), L"Enable");
                else
                  sec.add_key_value (SK_FormatStringW (L"%d", _debug_id).c_str (), L"Disable");

                auto break_id_ =
                  std::find (break_ids.begin (), break_ids.end (), _debug_id);

                if (break_id_ == break_ids.end ())
                {
                  if (break_)
                    break_ids.push_back (_debug_id);
                }

                else if (! break_)
                {
                  break_ids.erase (break_id_);
                }

                debug_ini->write ();

                pInfoQueueD3D11->SetBreakOnID (_debug_id, break_);
              }

              ImGui::TreePop    ();
              ImGui::EndPopup   ();
            }

              SK_ComPtr                     <IDXGIInfoQueue>           pInfoQueueDXGI;
              SK_DXGI_GetDebugInterface (IID_IDXGIInfoQueue, (void **)&pInfoQueueDXGI.p);
              SK_ComPtr                     <IDXGIDebug>               pDXGIDebug;
              SK_DXGI_GetDebugInterface (IID_IDXGIDebug,     (void **)&pDXGIDebug.p);

              auto *tls_stack_buf =
                &pTLS->scratch_memory->log.formatted_output;

              struct SK_DEBUG_MESSAGE {
                enum _Type {
                  SK_DEBUG_MESSAGE_TYPE_UNKNOWN = 0x0,
                  SK_DEBUG_MESSAGE_TYPE_DXGI    = 0x1,
                  SK_DEBUG_MESSAGE_TYPE_D3D11   = 0x2,
                  SK_DEBUG_MESSAGE_TYPE_D3D12   = 0x4,
                } Type;

                union {
                  struct {
                    DXGI_DEBUG_ID                    Producer;
                    DXGI_INFO_QUEUE_MESSAGE_CATEGORY Category;
                    DXGI_INFO_QUEUE_MESSAGE_SEVERITY Severity;
                    DXGI_INFO_QUEUE_MESSAGE_ID       ID;
                  } dxgi;

                  struct SK_D3D11_MESSAGE {
                    D3D11_MESSAGE_CATEGORY           Category;
                    D3D11_MESSAGE_SEVERITY           Severity;
                    D3D11_MESSAGE_ID                 ID;
                  } d3d11;
                  
                  // Not Finished
                  struct SK_D3D12_MESSAGE {
                    D3D12_MESSAGE_CATEGORY           Category;
                    D3D12_MESSAGE_SEVERITY           Severity;
                    D3D12_MESSAGE_ID                 ID;
                  } d3d12;
                };

                time_t                               Time;
                std::string                          Timestamp;
                std::string                          Text;
              };

              static std::vector <SK_DEBUG_MESSAGE> debug_messages;

              if (bClearLog)
                debug_messages.clear ();

              static bool        once_again = false;
              if (std::exchange (once_again, true) == false)
              {
                pInfoQueueD3D11->AddApplicationMessage (
                  D3D11_MESSAGE_SEVERITY_MESSAGE,
                    "Hello From The D3D11 Debug Layer..." );

                D3D11_MESSAGE_ID ia_nop =
                  D3D11_MESSAGE_ID_CREATEINPUTLAYOUT_EMPTY_LAYOUT;

                D3D11_INFO_QUEUE_FILTER
                  filter                  = {     };
                  filter.DenyList.NumIDs  =       1;
                  filter.DenyList.pIDList = &ia_nop;

                pInfoQueueD3D11->AddRetrievalFilterEntries (&filter);
                pInfoQueueD3D11->AddStorageFilterEntries   (&filter);
              }

              pInfoQueueD3D11->SetMuteDebugOutput (
                                _pauseDebugOutput );

              UINT64 uiMessagesWaiting =
                pInfoQueueD3D11->GetNumStoredMessages ();

              for ( UINT64 msgIdx = 0                 ;
                           msgIdx < uiMessagesWaiting ;
                         ++msgIdx )
              {
                if (_pauseDebugOutput)
                  break;

                SIZE_T msgLen = 0;

                if ( SUCCEEDED (
                       pInfoQueueD3D11->GetMessage
                        ( msgIdx, nullptr, &msgLen ) )
                   )
                {
                  auto pMsg =
                    (D3D11_MESSAGE *)
                      tls_stack_buf->alloc (msgLen);

                  if ( SUCCEEDED (
                         pInfoQueueD3D11->GetMessage
                          ( msgIdx, pMsg, &msgLen ) )
                     )
                  {
                    if (pMsg->Severity > (D3D11_MESSAGE_SEVERITY)minimum_severity)
                    {
                      continue;
                    }

                    auto& timestamp =
                      debug_messages.
                        emplace_back (
                          SK_DEBUG_MESSAGE {
                            .Type       = SK_DEBUG_MESSAGE::SK_DEBUG_MESSAGE_TYPE_D3D11,
                            .d3d11      = {
                              .Category = pMsg->Category,
                              .Severity = pMsg->Severity,
                              .ID       = pMsg->ID },
                            .Time       = time (nullptr),
                            .Text       =
                              std::string (
                                     pMsg->pDescription,
                                     pMsg->DescriptionByteLength
                              )            }
                        ).Timestamp;

                              timestamp.reserve (27);
                    ctime_s ( timestamp.data (), 26,
                        &debug_messages.back ().Time );

                    OutputDebugStringA (debug_messages.back ().Text.c_str ());
                  }
                }
              }

              pInfoQueueD3D11->ClearStoredMessages ();

              if (pInfoQueueDXGI != nullptr)
              {
                pInfoQueueDXGI->SetMuteDebugOutput (
                  DXGI_DEBUG_ALL, _pauseDebugOutput );

                uiMessagesWaiting =
                  pInfoQueueDXGI->GetNumStoredMessages (DXGI_DEBUG_ALL);

                for ( UINT64 msgIdx = 0                 ;
                             msgIdx < uiMessagesWaiting ;
                           ++msgIdx )
                {
                  if (_pauseDebugOutput)
                  break;

                  SIZE_T msgLen = 0;

                  if ( SUCCEEDED (
                         pInfoQueueDXGI->GetMessage ( DXGI_DEBUG_ALL,
                           msgIdx, nullptr, &msgLen ) )
                     )
                  {
                    auto pMsg =
                      (DXGI_INFO_QUEUE_MESSAGE *)
                        tls_stack_buf->alloc (msgLen);

                    if ( SUCCEEDED (
                           pInfoQueueDXGI->GetMessage ( DXGI_DEBUG_ALL,
                                msgIdx, pMsg, &msgLen ) )
                       )
                    {
                      if (pMsg->Severity > (DXGI_INFO_QUEUE_MESSAGE_SEVERITY)minimum_severity)
                      {
                        continue;
                      }

                      auto& timestamp =
                        debug_messages.
                          emplace_back (
                            SK_DEBUG_MESSAGE {
                              .Type       = SK_DEBUG_MESSAGE::SK_DEBUG_MESSAGE_TYPE_DXGI,
                              .dxgi       = {
                                .Producer = pMsg->Producer,
                                .Category = pMsg->Category,
                                .Severity = pMsg->Severity,
                                .ID       = pMsg->ID },
                              .Time       = time (nullptr),
                              .Text       =
                                std::string (
                                       pMsg->pDescription,
                                       pMsg->DescriptionByteLength
                                )            }
                          ).Timestamp;

                                timestamp.reserve (27);
                      ctime_s ( timestamp.data (), 26,
                          &debug_messages.back ().Time );

                      OutputDebugStringA (debug_messages.back ().Text.c_str ());
                    }
                  }
                }

                pInfoQueueDXGI->ClearStoredMessages (DXGI_DEBUG_ALL);
              }

              static
                std::map < D3D11_MESSAGE_SEVERITY, ImVec4 >
                  _d3d11_colors =
                  {
                    { D3D11_MESSAGE_SEVERITY_CORRUPTION, ImColor::HSV (0.836f, 1.0f, 1.0f) },
                    { D3D11_MESSAGE_SEVERITY_ERROR,      ImColor::HSV (0.0f,   1.0f, 1.0f) },
                    { D3D11_MESSAGE_SEVERITY_WARNING,    ImColor::HSV (0.169f, 1.0f, 1.0f) },
                    { D3D11_MESSAGE_SEVERITY_INFO,       ImColor::HSV (0.336f, 1.0f, 1.0f) },
                    { D3D11_MESSAGE_SEVERITY_MESSAGE,    ImColor::HSV (0.503f, 1.0f, 1.0f) }
                  };

              static
                std::map < DXGI_INFO_QUEUE_MESSAGE_SEVERITY, ImVec4 >
                  _dxgi_colors =
                  {
                    { DXGI_INFO_QUEUE_MESSAGE_SEVERITY_CORRUPTION, ImColor::HSV (0.836f, 1.0f, 1.0f) },
                    { DXGI_INFO_QUEUE_MESSAGE_SEVERITY_ERROR,	     ImColor::HSV (0.0f,   1.0f, 1.0f) },
                    { DXGI_INFO_QUEUE_MESSAGE_SEVERITY_WARNING,	   ImColor::HSV (0.169f, 1.0f, 1.0f) },
                    { DXGI_INFO_QUEUE_MESSAGE_SEVERITY_INFO,	     ImColor::HSV (0.336f, 1.0f, 1.0f) },
                    { DXGI_INFO_QUEUE_MESSAGE_SEVERITY_MESSAGE,	   ImColor::HSV (0.503f, 1.0f, 1.0f) }
                  };

              for ( const auto& debug_message : debug_messages )
              {
                if ((debug_message.Type           == SK_DEBUG_MESSAGE::_Type::SK_DEBUG_MESSAGE_TYPE_D3D11 &&
                     debug_message.d3d11.Severity  >           (D3D11_MESSAGE_SEVERITY)minimum_severity)  ||
                    (debug_message.Type           == SK_DEBUG_MESSAGE::_Type::SK_DEBUG_MESSAGE_TYPE_DXGI  &&
                     debug_message. dxgi.Severity  > (DXGI_INFO_QUEUE_MESSAGE_SEVERITY)minimum_severity))
                {
                  continue;
                }

                int message_id = 0;

                ImGui::Text        ("%*hs",
                       25, debug_message.Timestamp.c_str ());
                ImGui::SameLine    (                       );

                auto _DrawMessageText = [&](void)
                {
                  switch (debug_message.Type)
                  {
                    case SK_DEBUG_MESSAGE::SK_DEBUG_MESSAGE_TYPE_D3D11:
                      ImGui::TextColored (
                        _d3d11_colors [debug_message.d3d11.Severity], "%hs",
                                       debug_message.Text.c_str ()
                                         );
                      message_id = debug_message.d3d11.ID;
                      break;

                    case SK_DEBUG_MESSAGE::SK_DEBUG_MESSAGE_TYPE_DXGI:
                      ImGui::TextColored (
                         _dxgi_colors [debug_message.dxgi.Severity], "%hs",
                                       debug_message.Text.c_str ()
                                         );
                      message_id = debug_message.dxgi.ID;
                      break;

                    default:
                      ImGui::TextColored (
                         _d3d11_colors [D3D11_MESSAGE_SEVERITY_ERROR],
                           "UNKNOWN MESSAGE TYPE (?!)"
                                         );
                      message_id = -1;
                      break;
                  }
                };

                _DrawMessageText ();

                if (! _pauseDebugOutput)
                  ImGui::SetScrollHereY (1.0f);

                if (ImGui::IsItemClicked (1))
                {
                  if (debug_message.Type == SK_DEBUG_MESSAGE::SK_DEBUG_MESSAGE_TYPE_D3D11)
                  {
                    _debug_id = (D3D11_MESSAGE_ID)message_id;

                    ImGui::OpenPopup ("D3D11_Debug_MessageMenu");
                  }
                }

                else if (ImGui::IsItemHovered ())
                {
                  ImGui::BeginTooltip ();
                  ImGui::Text         ("Message ID: %d", message_id);
                  ImGui::Separator    ();
                     _DrawMessageText ();
                  ImGui::EndTooltip   ();
                }
              }
            }

            ImGui::EndChild ();
          }

          else
          {
            ImGui::BulletText ("Game Restart Required");
          }
        }
      }
    }

    ImGui::Separator            ();
    SK_ImGui_DrawVRAMGauge      ();
    ImGui::OpenPopupOnItemClick ("DXGI_VRAM_BUDGET", 1);

    if (
      ImGui::BeginItemTooltip())
    {
      ImGui::TextUnformatted ("Right-click to Configure VRAM Quotas or Reset Warnings");
      ImGui::Separator       ();
      ImGui::TreePush        ("");
      ImGui::TextUnformatted ("The statistics shown are graphics memory actively used by the GAME (Resident VRAM) vs. VRAM the driver can dedicate to the GAME (VRAM Budget).\r\n\r\n");
      ImGui::BulletText      ("This differs significantly from all current monitoring tools...\r\n\r\n");
      ImGui::TreePush        ("");
      ImGui::TextUnformatted ("MSI Afterburner, for example, can measure system-wide graphics memory allocation or per-process allocation depending on how it is configured,");
      ImGui::TextUnformatted ("but is incapable of measuring Resident VRAM, nor does it know anything about per-process limits on VRAM (Budgets).\r\n\r\n");
      ImGui::TreePop         ();
      ImGui::TextUnformatted ("Correctly measuring a game's VRAM requirements involves knowledge of VRAM Residency and per-process VRAM Budgets!\r\n");
      ImGui::TextUnformatted ("Budgets change dynamically in WDDM 2.0+ when there is demand by other applications for VRAM that reduces the amount available to the game.\r\n\r\n");
      ImGui::BulletText      ("Special K's VRAM gauge adjusts to changing VRAM Budgets in real-time to correctly assess VRAM availability.");
      ImGui::TreePop         ();
      ImGui::Separator       ();
      ImGui::TextUnformatted ("VRAM statistics for game sessions are summarized at exit in logs/dxgi_budget.log");
      ImGui::EndTooltip      ();
    }

    if (ImGui::BeginPopup ("DXGI_VRAM_BUDGET"))
    {
      bool warn =
        config.render.dxgi.warn_if_vram_exceeds > 0.0f;

      if (ImGui::Checkbox ( warn ? "Warn if Used VRAM Exceeds"
                                 : "Warn if Useable VRAM is Low", &warn ))
      {
        if (warn) config.render.dxgi.warn_if_vram_exceeds = 95.0f;
        else      config.render.dxgi.warn_if_vram_exceeds =  0.0f;
      }

      if (config.render.dxgi.warn_if_vram_exceeds > 0.0f)
      {
        static float limit =
          config.render.dxgi.warn_if_vram_exceeds;

        ImGui::SameLine ();

        /// XXX: FIXME
        SK_ImGui::SliderFloatDeferred (
          "###VRAM_QUOTA", &config.render.dxgi.warn_if_vram_exceeds,
                           &limit, 15.0f, 105.0f, "%2.2f%% of Available" );

        if (config.render.dxgi.warned_low_vram)
        {
          if (ImGui::SameLine ();
              ImGui::Button   ("Reset Warning"))
            config.render.dxgi.warned_low_vram = false;
        }
      }
      ImGui::EndPopup    ( );
    }
    ImGui::TreePop       ( );
    ImGui::PopStyleColor (3);

    return true;
  }

  return false;
}


bool
SK_D3D11_ShowShaderModDlg (void) noexcept
{
  return
    SK::ControlPanel::D3D11::show_shader_mod_dlg;
}


void
SK_ImGui_SummarizeDXGISwapchain (IDXGISwapChain* pSwapDXGI)
{
  if (pSwapDXGI != nullptr)
  {
    SK_ComQIPtr <IDXGISwapChain1> pSwap1 (pSwapDXGI);

    DXGI_SWAP_CHAIN_DESC1                 swap_desc = { };
    DXGI_SWAP_CHAIN_FULLSCREEN_DESC fullscreen_desc = { };

    if (           pSwap1.p != nullptr                     &&
        SUCCEEDED (pSwap1->GetDesc1          (&swap_desc)) &&
        SUCCEEDED (pSwap1->GetFullscreenDesc (&fullscreen_desc)))
    {
      const SK_RenderBackend& rb =
        SK_GetCurrentRenderBackend ();

      INT         swap_flag_count = 0;
      std::string swap_flags      =
        SK_DXGI_DescribeSwapChainFlags (
          static_cast <DXGI_SWAP_CHAIN_FLAG> (swap_desc.Flags),
                  &swap_flag_count     );

      extern DXGI_SWAP_CHAIN_DESC  _ORIGINAL_SWAP_CHAIN_DESC;

      ImGui::BeginTooltip      ();
      ImGui::PushStyleColor    (ImGuiCol_Text, ImVec4 (0.95f, 0.95f, 0.45f, 1.0f));
      ImGui::TextUnformatted   ("Framebuffer and Presentation Setup");
      ImGui::PopStyleColor     ();
      ImGui::Separator         ();

      ImGui::BeginGroup        ();
      ImGui::PushStyleColor    (ImGuiCol_Text, ImVec4 (0.685f, 0.685f, 0.685f, 1.0f));
      ImGui::TextUnformatted   ("Color:");
    //ImGui::TextUnformatted   ("Depth/Stencil:");
      ImGui::TextUnformatted   ("Resolution:");
      ImGui::TextUnformatted   ("Back Buffers:");
      if ((! fullscreen_desc.Windowed) && fullscreen_desc.Scaling          != DXGI_MODE_SCALING_UNSPECIFIED)
        ImGui::TextUnformatted ("Scaling Mode:");
      if ((! fullscreen_desc.Windowed) && fullscreen_desc.ScanlineOrdering != DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED)
        ImGui::TextUnformatted ("Scanlines:");
      if ((! fullscreen_desc.Windowed) && fullscreen_desc.RefreshRate.Denominator != 0)
        ImGui::TextUnformatted ("Refresh Rate:");
      ImGui::TextUnformatted   ("Swap Interval:");
      ImGui::TextUnformatted   ("Swap Effect:");
      if  (swap_desc.SampleDesc.Count > 1 || rb.active_traits.uiOriginalBltSampleCount > 1)
        ImGui::TextUnformatted ("MSAA Samples:");
      if (swap_desc.Flags != 0)
      {
        ImGui::TextUnformatted ("Flags:");
        if (swap_flag_count > 1) { for ( int i = 1; i < swap_flag_count; i++ ) ImGui::TextUnformatted ("\n"); }
      }
      ImGui::PopStyleColor   ();
      ImGui::EndGroup        ();

      ImGui::SameLine        ();

      ImGui::BeginGroup      ();
      ImGui::PushStyleColor  (ImGuiCol_Text, ImVec4 (1.0f, 1.0f, 1.0f, 1.0f));
      if (_ORIGINAL_SWAP_CHAIN_DESC.OutputWindow == SK_GetGameWindow ( )   &&
          _ORIGINAL_SWAP_CHAIN_DESC.BufferDesc.Format != swap_desc.Format  &&
          _ORIGINAL_SWAP_CHAIN_DESC.BufferDesc.Format != DXGI_FORMAT_UNKNOWN)
        ImGui::Text            ("%hs %hs  %hs",       SK_DXGI_FormatToStr (_ORIGINAL_SWAP_CHAIN_DESC.BufferDesc.Format).data (), (const char*)u8"\u2192",
                                                      SK_DXGI_FormatToStr (swap_desc.Format).data());
      else 
        ImGui::Text            ("%hs",                SK_DXGI_FormatToStr (swap_desc.Format).data ());

      if (_ORIGINAL_SWAP_CHAIN_DESC.OutputWindow == SK_GetGameWindow ( )   &&
         (_ORIGINAL_SWAP_CHAIN_DESC.BufferDesc.Width  != swap_desc.Width   || 
          _ORIGINAL_SWAP_CHAIN_DESC.BufferDesc.Height != swap_desc.Height) &&
         (_ORIGINAL_SWAP_CHAIN_DESC.BufferDesc.Width  != 0                 ||
          _ORIGINAL_SWAP_CHAIN_DESC.BufferDesc.Height != 0))
        ImGui::Text            ("%ux%u %hs %ux%u",                         _ORIGINAL_SWAP_CHAIN_DESC.BufferDesc.Width,
                                                                           _ORIGINAL_SWAP_CHAIN_DESC.BufferDesc.Height, (const char*)u8"\u2192",
                                                                           swap_desc.Width, swap_desc.Height);
      else
        ImGui::Text            ("%ux%u",                                   swap_desc.Width, swap_desc.Height);
      
      if (_ORIGINAL_SWAP_CHAIN_DESC.OutputWindow == SK_GetGameWindow ( )  &&
          _ORIGINAL_SWAP_CHAIN_DESC.BufferCount  != swap_desc.BufferCount &&
          _ORIGINAL_SWAP_CHAIN_DESC.BufferCount  != 0)
        ImGui::Text            ("%u %hs %u",                               std::max (1U, _ORIGINAL_SWAP_CHAIN_DESC.BufferCount),  (const char*)u8"\u2192",
                                                                           std::max (1U, swap_desc.BufferCount));
      else
        ImGui::Text            ("%u",                                      std::max (1U, swap_desc.BufferCount));

      if ((! fullscreen_desc.Windowed) && fullscreen_desc.Scaling          != DXGI_MODE_SCALING_UNSPECIFIED)
        ImGui::Text          ("%hs",        SK_DXGI_DescribeScalingMode (fullscreen_desc.Scaling));
      if ((! fullscreen_desc.Windowed) && fullscreen_desc.ScanlineOrdering != DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED)
        ImGui::Text          ("%hs",      SK_DXGI_DescribeScanlineOrder (fullscreen_desc.ScanlineOrdering));
      if ((! fullscreen_desc.Windowed) && fullscreen_desc.RefreshRate.Denominator != 0)
        ImGui::Text          ("%.2f Hz",                                 static_cast <float> (fullscreen_desc.RefreshRate.Numerator) /
                                                                         static_cast <float> (fullscreen_desc.RefreshRate.Denominator));

      std::string present_interval_text;

      if (rb.present_interval_orig != rb.present_interval)
      {
        present_interval_text  += (rb.present_interval_orig == 0)
                                    ? "0: VSYNC OFF"          :
                                  (rb.present_interval_orig == 1)
                                    ? "1: Normal V-SYNC"      :
                                  (rb.present_interval_orig == 2)
                                    ? "2: 1/2 Refresh V-SYNC" :
                                  (rb.present_interval_orig == 3)
                                    ? "3: 1/3 Refresh V-SYNC" :
                                  (rb.present_interval_orig == 4)
                                    ? "4: 1/4 Refresh V-SYNC" :
                                      "0: UNKNOWN or Invalid";

        present_interval_text += (const char*)u8" \u2192 ";
      }

      present_interval_text  += (rb.present_interval == 0)
                                  ? "0: VSYNC OFF"          :
                                (rb.present_interval == 1)
                                  ? "1: Normal V-SYNC"      :
                                (rb.present_interval == 2)
                                  ? "2: 1/2 Refresh V-SYNC" :
                                (rb.present_interval == 3)
                                  ? "3: 1/3 Refresh V-SYNC" :
                                (rb.present_interval == 4)
                                  ? "4: 1/4 Refresh V-SYNC" :
                                    "0: UNKNOWN or Invalid";

      ImGui::TextUnformatted    (present_interval_text.c_str ());


      if (_ORIGINAL_SWAP_CHAIN_DESC.OutputWindow == SK_GetGameWindow ( ) &&
          _ORIGINAL_SWAP_CHAIN_DESC.SwapEffect   != swap_desc.SwapEffect)
        ImGui::Text            ("%hs %hs %hs",    SK_DXGI_DescribeSwapEffect (_ORIGINAL_SWAP_CHAIN_DESC.SwapEffect), (const char*)u8"\u2192",
                                                  SK_DXGI_DescribeSwapEffect (swap_desc.SwapEffect));
      else
        ImGui::Text            ("%hs",            SK_DXGI_DescribeSwapEffect (swap_desc.SwapEffect));

      if (_ORIGINAL_SWAP_CHAIN_DESC.OutputWindow     == SK_GetGameWindow ( )       &&
          _ORIGINAL_SWAP_CHAIN_DESC.SampleDesc.Count != swap_desc.SampleDesc.Count &&
                          swap_desc.SampleDesc.Count > 1)
        ImGui::Text          ("%u %hs %u",                                  _ORIGINAL_SWAP_CHAIN_DESC.SampleDesc.Count, (const char*)u8"\u2192",
                                                                            swap_desc.SampleDesc.Count);
      if  (swap_desc.SampleDesc.Count   > 1)
        ImGui::Text          ("%u",                                         swap_desc.SampleDesc.Count);
      else if (rb.active_traits.uiOriginalBltSampleCount > 1)
        ImGui::Text          ("%u",                                         rb.active_traits.uiOriginalBltSampleCount);
      if (swap_desc.Flags != 0)
      {
        ImGui::Text          ("%hs",                                        swap_flags.c_str ());
      }
      ImGui::PopStyleColor   ();
      ImGui::EndGroup        ();

      ImGui::PushStyleColor  (ImGuiCol_Text, ImVec4 (0.95f, 0.95f, 0.45f, 1.0f));
      ImGui::TextUnformatted ("Display Output Configuration");
      ImGui::PopStyleColor   ();
      ImGui::Separator       ();

      ImGui::BeginGroup      ();
      ImGui::PushStyleColor  (ImGuiCol_Text, ImVec4 (0.685f, 0.685f, 0.685f, 1.0f));
      //if (rb.isHDRCapable ())
      {
        ImGui::Text ("Display Device: ");
        ImGui::Text ("HDR Color Space: ");
        ImGui::Text ("Output Bit Depth: ");
      }
      ImGui::PopStyleColor   ();
      ImGui::EndGroup        ();

      ImGui::SameLine        ();

      ImGui::BeginGroup      ();
      ImGui::PushStyleColor  (ImGuiCol_Text, ImVec4 (1.0f, 1.0f, 1.0f, 1.0f));
      //if (rb.isHDRCapable ())
      {
        bool _fullscreen = true;

        SK_ComQIPtr <IDXGISwapChain4> pSwap4 (pSwapDXGI);

        if (pSwap4 != nullptr)
        {
          DXGI_SWAP_CHAIN_FULLSCREEN_DESC full_desc = { };
              pSwap4->GetFullscreenDesc (&full_desc);

          _fullscreen =
            (! full_desc.Windowed);
        }

        ImGui::Text ("%ws", rb.display_name);

        if (! rb.scanout.nvapi_hdr.active)
        {
          if (_fullscreen)
            ImGui::Text ("%hs",                DXGIColorSpaceToStr ((DXGI_COLOR_SPACE_TYPE)rb.scanout.dxgi_colorspace));
          else
            ImGui::Text ("%hs (DWM Assigned)", DXGIColorSpaceToStr ((DXGI_COLOR_SPACE_TYPE)rb.scanout.dwm_colorspace));
          ImGui::Text   ("%d", rb.scanout.bpc);
        }

        else
        {
          ImGui::Text ("%hs (%s)", HDRModeToStr (rb.scanout.nvapi_hdr.mode),
                                                 rb.scanout.nvapi_hdr.getFormatStr ());
          ImGui::Text ("%hs",                    rb.scanout.nvapi_hdr.getBpcStr    ());
        }
      }
      ImGui::PopStyleColor   ();
      ImGui::EndGroup        ();
      ImGui::Separator       ();

      ImGui::TextColored     (ImVec4 (.4f, .8f, 1.f, 1.f), " " ICON_FA_MOUSE);
      ImGui::SameLine        ();
      ImGui::TextUnformatted ("Right-click to configure Fullscreen / Windowed Mode");
      ImGui::EndTooltip      ();
    }
  }
}

void
SK::ControlPanel::D3D11::TextureMenu (SK_TLS *pTLS)
{
  if (ImGui::BeginMenu ("Browse Texture Assets"))
  {
    wchar_t wszPath [MAX_PATH + 2] = { };

    if (pTLS == nullptr)
        pTLS =
      SK_TLS_Bottom ();

    static char injectable [MAX_PATH] = {};
    static char dumped     [MAX_PATH] = {};

    std::string_view inj_view (injectable, MAX_PATH),
                    dump_view (    dumped, MAX_PATH);

    SK_FormatStringView ( inj_view, "%ws",
                            SK_File_SizeToString (
                              SK_D3D11_Textures->injectable_texture_bytes, Auto, pTLS
                                                 ).data () );

    if ( ImGui::MenuItem ( "Injectable Textures", inj_view.data (), nullptr ) )
    {
      wcscpy      (wszPath, SK_D3D11_res_root->c_str ());
      PathAppendW (wszPath, LR"(inject\textures)");

      SK_ShellExecuteW (nullptr, L"explore", wszPath, nullptr, nullptr, SW_NORMAL);
    }

    if (! SK_D3D11_Textures->dumped_textures.empty ())
    {
       SK_FormatStringView ( dump_view, "%ws",
                               SK_File_SizeToString (
                                 SK_D3D11_Textures->dumped_texture_bytes, Auto, pTLS
                                                    ).data () );
       if ( ImGui::MenuItem ( "Dumped Textures", dump_view.data (), nullptr ) )
       {
         wcscpy      (wszPath, SK_D3D11_res_root->c_str ());
         PathAppendW (wszPath, LR"(dump\textures)");
         PathAppendW (wszPath, SK_GetHostApp ());

         SK_ShellExecuteW (nullptr, L"explore", wszPath, nullptr, nullptr, SW_NORMAL);
       }
     }

    ImGui::EndMenu ();
  }
}