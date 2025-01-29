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

#ifdef  __SK_SUBSYSTEM__
#undef  __SK_SUBSYSTEM__
#endif
#define __SK_SUBSYSTEM__ L"   DXGI   "

#define _L2(w)  L ## w
#define  _L(w) _L2(w)

#include <SpecialK/render/dxgi/dxgi_interfaces.h>
#include <SpecialK/render/dxgi/dxgi_swapchain.h>
#include <SpecialK/render/dxgi/dxgi_util.h>
#include <SpecialK/render/dxgi/dxgi_hdr.h>

#include <SpecialK/render/d3d11/d3d11_core.h>
#include <SpecialK/render/d3d11/d3d11_tex_mgr.h>
#include <SpecialK/render/d3d11/d3d11_state_tracker.h>
#include <SpecialK/render/ngx/ngx.h>

#include <imgui/backends/imgui_d3d11.h>
#include <imgui/backends/imgui_d3d12.h>

#include <imgui/font_awesome.h>

#include <d3d12.h>

#include <SpecialK/nvapi.h>
#include <math.h>

#include <CoreWindow.h>
#include <VersionHelpers.h>

#undef IMGUI_VERSION_NUM
#include <ReShade/reshade.hpp>
#include <ReShade/reshade_api.hpp>

BOOL _NO_ALLOW_MODE_SWITCH = FALSE;
DXGI_SWAP_CHAIN_DESC  _ORIGINAL_SWAP_CHAIN_DESC  = { };
DXGI_SWAP_CHAIN_DESC1 _ORIGINAL_SWAP_CHAIN_DESC1 = { };

int SK_DXGI_HighestFactorySupported = -1;

#include <../depends/include/DirectXTex/d3dx12.h>

using D3D12GetDebugInterface_pfn = HRESULT (WINAPI* )( _In_ REFIID riid, _COM_Outptr_opt_ void** ppvDebug );

   auto _IsD3D12SwapChain = [](IDXGISwapChain *pSwapChain)
-> bool
   {
     SK_ComPtr            <ID3D12Device>                pDev12;
     return       pSwapChain != nullptr &&
       SUCCEEDED (pSwapChain->GetDevice (IID_PPV_ARGS (&pDev12.p))) &&
                                                        pDev12.p != nullptr;
   };

// For querying the name of the monitor from the system registry when
//   EDID and other more purpose-built driver functions fail to help.
#pragma comment (lib, "SetupAPI.lib")

CreateSwapChain_pfn               CreateSwapChain_Original               = nullptr;

PresentSwapChain_pfn              Present_Original                       = nullptr;
Present1SwapChain1_pfn            Present1_Original                      = nullptr;
SetFullscreenState_pfn            SetFullscreenState_Original            = nullptr;
GetFullscreenState_pfn            GetFullscreenState_Original            = nullptr;
ResizeBuffers_pfn                 ResizeBuffers_Original                 = nullptr;
ResizeBuffers1_pfn                ResizeBuffers1_Original                = nullptr;
ResizeTarget_pfn                  ResizeTarget_Original                  = nullptr;

GetDisplayModeList_pfn            GetDisplayModeList_Original            = nullptr;
FindClosestMatchingMode_pfn       FindClosestMatchingMode_Original       = nullptr;
WaitForVBlank_pfn                 WaitForVBlank_Original                 = nullptr;
CreateSwapChainForHwnd_pfn        CreateSwapChainForHwnd_Original        = nullptr;
CreateSwapChainForCoreWindow_pfn  CreateSwapChainForCoreWindow_Original  = nullptr;
CreateSwapChainForComposition_pfn CreateSwapChainForComposition_Original = nullptr;

GetDesc_pfn                       GetDesc_Original                       = nullptr;
GetDesc1_pfn                      GetDesc1_Original                      = nullptr;
GetDesc2_pfn                      GetDesc2_Original                      = nullptr;

EnumAdapters_pfn                  EnumAdapters_Original                  = nullptr;
EnumAdapters1_pfn                 EnumAdapters1_Original                 = nullptr;

CreateDXGIFactory_pfn             CreateDXGIFactory_Import               = nullptr;
CreateDXGIFactory1_pfn            CreateDXGIFactory1_Import              = nullptr;
CreateDXGIFactory2_pfn            CreateDXGIFactory2_Import              = nullptr;

extern bool bSwapChainNeedsResize;
extern bool bRecycledSwapChains;

using DXGIDevice1_SetMaximumFrameLatency_pfn = HRESULT (WINAPI *)(IDXGIDevice1 *This, UINT   MaxLatency);
using DXGIDevice1_GetMaximumFrameLatency_pfn = HRESULT (WINAPI *)(IDXGIDevice1 *This, UINT *pMaxLatency);

DXGIDevice1_SetMaximumFrameLatency_pfn DXGIDevice1_SetMaximumFrameLatency_Original = nullptr;
DXGIDevice1_GetMaximumFrameLatency_pfn DXGIDevice1_GetMaximumFrameLatency_Original = nullptr;

static UINT lastDeviceLatency = 0;

HRESULT
WINAPI
DXGIDevice1_SetMaximumFrameLatency_Override (IDXGIDevice1 *This, UINT MaxLatency)
{
  SK_LOG_FIRST_CALL

  lastDeviceLatency = MaxLatency;

  if (config.render.framerate.pre_render_limit > 0)
  {
    MaxLatency =
      std::clamp (config.render.framerate.pre_render_limit, 1, 14);
  }

  return
    DXGIDevice1_SetMaximumFrameLatency_Original (This, MaxLatency);
}

HRESULT
WINAPI
DXGIDevice1_GetMaximumFrameLatency_Override (IDXGIDevice1 *This, UINT *pMaxLatency)
{
  SK_LOG_FIRST_CALL

  if (config.render.framerate.pre_render_limit > 0)
  {
    if (                                             0 == lastDeviceLatency)
      DXGIDevice1_GetMaximumFrameLatency_Original (This, &lastDeviceLatency);

    if (pMaxLatency != nullptr)
       *pMaxLatency  = lastDeviceLatency;

    return
      DXGIDevice1_SetMaximumFrameLatency_Original (This,
        std::clamp (config.render.framerate.pre_render_limit, 1, 14)
      );
  }

  return
    DXGIDevice1_GetMaximumFrameLatency_Original (This, pMaxLatency);
}

HRESULT
WINAPI
SK_DXGI_SetDeviceMaximumFrameLatency (IDXGIDevice1 *This, UINT MaxLatency)
{
  if (This == nullptr)
    return E_POINTER;

  if (DXGIDevice1_SetMaximumFrameLatency_Original != nullptr)
  {
    return
      DXGIDevice1_SetMaximumFrameLatency_Original (This, MaxLatency);
  }

  return
    This->SetMaximumFrameLatency (MaxLatency);
}

using DXGIDisableVBlankVirtualization_pfn =
HRESULT (WINAPI *)(void);

using DXGIDeclareAdapterRemovalSupport_pfn =
HRESULT (WINAPI *)(void);

using DXGIGetDebugInterface1_pfn =
HRESULT (WINAPI *)(
  UINT   Flags,
  REFIID riid,
  void   **pDebug
);

#pragma data_seg (".SK_DXGI_Hooks")
extern "C"
{
#define SK_HookCacheEntryGlobalDll(x) SK_API\
                                      SK_HookCacheEntryGlobal (x)
  // Global DLL's cache
  SK_HookCacheEntryGlobalDll (IDXGIFactory_CreateSwapChain)
  SK_HookCacheEntryGlobalDll (IDXGIFactory2_CreateSwapChainForHwnd)
  SK_HookCacheEntryGlobalDll (IDXGIFactory2_CreateSwapChainForCoreWindow)
  SK_HookCacheEntryGlobalDll (IDXGIFactory2_CreateSwapChainForComposition)
  SK_HookCacheEntryGlobalDll (IDXGISwapChain_Present)
  SK_HookCacheEntryGlobalDll (IDXGISwapChain1_Present1)
  SK_HookCacheEntryGlobalDll (IDXGISwapChain_ResizeTarget)
  SK_HookCacheEntryGlobalDll (IDXGISwapChain_ResizeBuffers)
  SK_HookCacheEntryGlobalDll (IDXGISwapChain3_ResizeBuffers1)
  SK_HookCacheEntryGlobalDll (IDXGISwapChain_GetFullscreenState)
  SK_HookCacheEntryGlobalDll (IDXGISwapChain_SetFullscreenState)
  SK_HookCacheEntryGlobalDll (CreateDXGIFactory)
  SK_HookCacheEntryGlobalDll (CreateDXGIFactory1)
  SK_HookCacheEntryGlobalDll (CreateDXGIFactory2)
};
#pragma data_seg ()
#pragma comment  (linker, "/SECTION:.SK_DXGI_Hooks,RWS")


// Local DLL's cached addresses
SK_HookCacheEntryLocal (IDXGIFactory_CreateSwapChain,
  L"dxgi.dll",           DXGIFactory_CreateSwapChain_Override,
                                    &CreateSwapChain_Original)
SK_HookCacheEntryLocal (IDXGIFactory2_CreateSwapChainForHwnd,
  L"dxgi.dll",           DXGIFactory2_CreateSwapChainForHwnd_Override,
                                     &CreateSwapChainForHwnd_Original)
SK_HookCacheEntryLocal (IDXGIFactory2_CreateSwapChainForCoreWindow,
  L"dxgi.dll",           DXGIFactory2_CreateSwapChainForCoreWindow_Override,
                                     &CreateSwapChainForCoreWindow_Original)
SK_HookCacheEntryLocal (IDXGIFactory2_CreateSwapChainForComposition,
  L"dxgi.dll",           DXGIFactory2_CreateSwapChainForComposition_Override,
                                     &CreateSwapChainForComposition_Original)
SK_HookCacheEntryLocal (IDXGISwapChain_Present,
  L"dxgi.dll",                         PresentCallback,
                                      &Present_Original)
SK_HookCacheEntryLocal (IDXGISwapChain1_Present1,
  L"dxgi.dll",                          Present1Callback,
                                       &Present1_Original)
SK_HookCacheEntryLocal (IDXGISwapChain_ResizeTarget,
  L"dxgi.dll",                DXGISwap_ResizeTarget_Override,
                                      &ResizeTarget_Original)
SK_HookCacheEntryLocal (IDXGISwapChain_ResizeBuffers,
  L"dxgi.dll",                DXGISwap_ResizeBuffers_Override,
                                      &ResizeBuffers_Original)
SK_HookCacheEntryLocal (IDXGISwapChain3_ResizeBuffers1,
  L"dxgi.dll",                DXGISwap3_ResizeBuffers1_Override,
                                       &ResizeBuffers1_Original)
SK_HookCacheEntryLocal (IDXGISwapChain_GetFullscreenState,
  L"dxgi.dll",                DXGISwap_GetFullscreenState_Override,
                                      &GetFullscreenState_Original)
SK_HookCacheEntryLocal (IDXGISwapChain_SetFullscreenState,
  L"dxgi.dll",                DXGISwap_SetFullscreenState_Override,
                                      &SetFullscreenState_Original)
SK_HookCacheEntryLocal (CreateDXGIFactory,
  L"dxgi.dll",          CreateDXGIFactory,
                       &CreateDXGIFactory_Import)
SK_HookCacheEntryLocal (CreateDXGIFactory1,
  L"dxgi.dll",          CreateDXGIFactory1,
                       &CreateDXGIFactory1_Import)
SK_HookCacheEntryLocal (CreateDXGIFactory2,
  L"dxgi.dll",          CreateDXGIFactory2,
                       &CreateDXGIFactory2_Import)

// Counter-intuitively, the fewer of these we cache the more compatible we get.
static
sk_hook_cache_array global_dxgi_records =
  { &GlobalHook_IDXGIFactory_CreateSwapChain,
    &GlobalHook_IDXGIFactory2_CreateSwapChainForHwnd,

    &GlobalHook_IDXGISwapChain_Present,
    &GlobalHook_IDXGISwapChain1_Present1,

    &GlobalHook_IDXGIFactory2_CreateSwapChainForCoreWindow,
  //&GlobalHook_IDXGIFactory2_CreateSwapChainForComposition,

  //&GlobalHook_IDXGISwapChain_ResizeTarget,
  //&GlobalHook_IDXGISwapChain_ResizeBuffers,
  //&GlobalHook_IDXGISwapChain3_ResizeBuffers1,
  //&GlobalHook_IDXGISwapChain_GetFullscreenState,
  //&GlobalHook_IDXGISwapChain_SetFullscreenState,
  //&GlobalHook_CreateDXGIFactory,
  //&GlobalHook_CreateDXGIFactory1,
  //&GlobalHook_CreateDXGIFactory2
  };

static
sk_hook_cache_array local_dxgi_records =
  { &LocalHook_IDXGIFactory_CreateSwapChain,
    &LocalHook_IDXGIFactory2_CreateSwapChainForHwnd,

    &LocalHook_IDXGISwapChain_Present,
    &LocalHook_IDXGISwapChain1_Present1,

    &LocalHook_IDXGIFactory2_CreateSwapChainForCoreWindow,
  //&LocalHook_IDXGIFactory2_CreateSwapChainForComposition,
  //&LocalHook_IDXGISwapChain_ResizeTarget,
  //&LocalHook_IDXGISwapChain_ResizeBuffers,
  //&LocalHook_IDXGISwapChain3_ResizeBuffers1,
  //&LocalHook_IDXGISwapChain_GetFullscreenState,
  //&LocalHook_IDXGISwapChain_SetFullscreenState,
  //&LocalHook_CreateDXGIFactory,
  //&LocalHook_CreateDXGIFactory1,
  //&LocalHook_CreateDXGIFactory2
  };



#define SK_LOG_ONCE(x) { static bool logged = false; if (! logged) \
                       { dll_log->Log ((x)); logged = true; } }

void SK_DXGI_HookSwapChain (IDXGISwapChain* pSwapChain);

void
ImGui_DX11Shutdown ( void )
{
  ImGui_ImplDX11_Shutdown ();
}

void
ImGui_DX12Shutdown ( void )
{
  ImGui_ImplDX12_Shutdown ();
}

static bool __d3d12 = false;

bool
ImGui_DX12Startup ( IDXGISwapChain* pSwapChain )
{
  if (pSwapChain == nullptr)
    return false;

  SK_RenderBackend& rb =
    SK_GetCurrentRenderBackend ();

  SK_ComPtr <ID3D12Device>       pD3D12Dev = nullptr;
  SK_ComPtr <ID3D12CommandQueue> p12Queue  = nullptr;

  if ( SUCCEEDED (pSwapChain->GetDevice (
                 __uuidof (ID3D12Device),
                 (void **)&pD3D12Dev.p)
     ) )
  {
    DXGI_SWAP_CHAIN_DESC swap_desc = { };

    if (SUCCEEDED (pSwapChain->GetDesc (&swap_desc)))
    {
      if (swap_desc.OutputWindow != nullptr)
      {
        if (rb.windows.focus.hwnd != swap_desc.OutputWindow)
            rb.windows.setFocus (swap_desc.OutputWindow);

        if (rb.windows.device.hwnd == nullptr)
            rb.windows.setDevice (swap_desc.OutputWindow);
      }
    }

    SK_DXGI_UpdateSwapChain (pSwapChain);

    return true;
  }

  return false;
}

bool
ImGui_DX11Startup ( IDXGISwapChain* pSwapChain )
{
  SK_LOGi2 (L"ImGui_DX11Startup (%p)", pSwapChain);

  SK_RenderBackend& rb =
    SK_GetCurrentRenderBackend ();

  SK_ComPtr <ID3D11Device>        pD3D11Dev         = nullptr;
  SK_ComPtr <ID3D11DeviceContext> pImmediateContext = nullptr;

  //assert (pSwapChain == rb.swapchain ||
  //                      rb.swapchain.IsEqualObject (pSwapChain));

  if (pSwapChain == nullptr)
    return false;

  if ( SUCCEEDED (pSwapChain->GetDevice (IID_PPV_ARGS (&pD3D11Dev.p))) )
  {
    SK_LOGi2 (L" -> GetDevice (D3D11) = %p", pD3D11Dev.p);

    assert ( pD3D11Dev.IsEqualObject ()   ||
         rb.getDevice <ID3D11Device> ().p == nullptr );

    if (pD3D11Dev == nullptr)
        pD3D11Dev = rb.getDevice <ID3D11Device> ();

    if (  rb.swapchain != pSwapChain ||
          pD3D11Dev    != rb.device  ||
          nullptr      == rb.d3d11.immediate_ctx )
    {
      if (pD3D11Dev != nullptr)
          pD3D11Dev->GetImmediateContext (&pImmediateContext);
    }

    else
    {
      pImmediateContext = rb.d3d11.immediate_ctx;
    }

    assert (pImmediateContext == rb.d3d11.immediate_ctx ||
                                 rb.d3d11.immediate_ctx == nullptr);

    if ( pImmediateContext != nullptr )
    {
      SK_LOGi2 (L" # D3D11 Immediate Context = %p", pImmediateContext.p);

      if (! (( rb.device    == nullptr || rb.device   == pD3D11Dev  ) ||
             ( rb.swapchain == nullptr || rb.swapchain== (IUnknown *)pSwapChain )) )
      {
        DXGI_SWAP_CHAIN_DESC swap_desc = { };

        if (SUCCEEDED (pSwapChain->GetDesc (&swap_desc)))
        {
          if (swap_desc.OutputWindow != nullptr)
          {
            if (rb.windows.focus.hwnd != swap_desc.OutputWindow)
                rb.windows.setFocus (swap_desc.OutputWindow);

            if (rb.windows.device.hwnd == nullptr)
                rb.windows.setDevice (swap_desc.OutputWindow);
          }
        }
      }

      if (_d3d11_rbk->init ( pSwapChain, pD3D11Dev, pImmediateContext))
      {
        SK_LOGi2 (L" _d3d11_rbk->init (SwapChain, %p, %p) Succeeded",
                                         pD3D11Dev.p,  pImmediateContext.p);

        SK_DXGI_UpdateSwapChain (pSwapChain);

        return true;
      }
    }
  }

  return false;
}

static volatile ULONG __gui_reset_dxgi   = TRUE;
static volatile ULONG __osd_frames_drawn = 0;

DXGI_FORMAT
SK_DXGI_PickHDRFormat ( DXGI_FORMAT fmt_orig, BOOL bWindowed,
                                              BOOL bFlipModel )
{
  SK_RenderBackend& rb =
    SK_GetCurrentRenderBackend ();

  bool TenBitSwap     = false;
  bool SixteenBitSwap = false;

  TenBitSwap     = __SK_HDR_10BitSwap && __SK_HDR_UserForced;
  SixteenBitSwap = __SK_HDR_16BitSwap && __SK_HDR_UserForced;

  // Hack to prevent NV's Vulkan/DXGI Interop SwapChain from destroying itself
  //   if HDR is not enabled.
  if (sk::NVAPI::nv_hardware && config.apis.NvAPI.vulkan_bridge == 1 && GetModuleHandle (L"vulkan-1.dll"))
  {
    // In case we are on a system with both AMD and NV GPUs, check
    //   for AMD's Vulkan layer as an indication to bail-out
    const bool bIsAMD =
      SK_IsModuleLoaded (
        SK_RunLHIfBitness (64, L"amdvlk64.dll",
                               L"amdvlk32.dll"));

    if (! bIsAMD)
    {
      TenBitSwap                       = true;
      config.render.output.force_10bpc = true;
    }
  }

  DXGI_FORMAT fmt_new = fmt_orig;

  // We cannot set colorspaces in windowed BitBlt, however ...
  //   it is actually possible to set an HDR colorspace in fullscreen bitblt.
  bool _bFlipOrFullscreen =
        bFlipModel || (! bWindowed);

  if  (_bFlipOrFullscreen && SixteenBitSwap) fmt_new = DXGI_FORMAT_R16G16B16A16_FLOAT;
  else if (_bFlipOrFullscreen && TenBitSwap) fmt_new = DXGI_FORMAT_R10G10B10A2_UNORM;

  if (fmt_new == fmt_orig)
    return fmt_orig;

  //if (rb.scanout.colorspace_override != DXGI_COLOR_SPACE_CUSTOM)
  {
    SK_ComQIPtr <IDXGISwapChain3>
                     pSwap3 (rb.swapchain);
    if (             pSwap3.p != nullptr)
    {
      DXGI_SWAP_CHAIN_FULLSCREEN_DESC swap_full_desc = { };
          pSwap3->GetFullscreenDesc (&swap_full_desc);

      if (swap_full_desc.Windowed)
      {
        if ( ( rb.scanout.dwm_colorspace == DXGI_COLOR_SPACE_CUSTOM ||
               rb.scanout.dwm_colorspace == DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020 )
                                       && SixteenBitSwap )
        {
          fmt_new = DXGI_FORMAT_R16G16B16A16_FLOAT;

          //DXGI_COLOR_SPACE_TYPE orig_space =
          //  rb.scanout.colorspace_override;
            rb.scanout.colorspace_override =
            DXGI_COLOR_SPACE_RGB_FULL_G10_NONE_P709;
        }

        else if (TenBitSwap)
        {
          fmt_new = DXGI_FORMAT_R10G10B10A2_UNORM;

          if (__SK_HDR_10BitSwap) // Only set colorspace on explicit HDR format
          {
            //DXGI_COLOR_SPACE_TYPE orig_space =
            //  rb.scanout.colorspace_override;
              rb.scanout.colorspace_override =
              DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020;
          }
        }
      }

      else if (SixteenBitSwap)
      {
        fmt_new = DXGI_FORMAT_R16G16B16A16_FLOAT;

        //DXGI_COLOR_SPACE_TYPE orig_space =
        //  rb.scanout.colorspace_override;
          rb.scanout.colorspace_override =
          DXGI_COLOR_SPACE_RGB_FULL_G10_NONE_P709;
      }
    }
  }

  if (fmt_new != fmt_orig)
  {
    SK_LOGi0 ( L" >> HDR: Overriding Original Format: '%hs' with '%hs'",
                         SK_DXGI_FormatToStr (fmt_orig).data (),
                         SK_DXGI_FormatToStr (fmt_new ).data () );
  }

  return
    fmt_new;
}

void ResetImGui_D3D12 (IDXGISwapChain* This)
{
  _d3d12_rbk->release (This);
}

void ResetImGui_D3D11 (IDXGISwapChain* This)
{
  _d3d11_rbk->release (This);
}

DWORD dwRenderThread = 0x0000;

static volatile LONG __dxgi_ready = FALSE;

bool WaitForInitDXGI (DWORD dwTimeout)
{
  extern thread_local bool
      initializing_dxgi;
  if (initializing_dxgi)
    return true;

  // Waiting while Streamline has plugins loaded would deadlock us in local injection
  if (SK_IsModuleLoaded (L"sl.common.dll"))
  {
    SK_Thread_SpinUntilFlaggedEx (&__dxgi_ready, 250UL);
  }

  else
  {
    if (dwTimeout != INFINITE)
         SK_Thread_SpinUntilFlaggedEx (&__dxgi_ready,      dwTimeout);
    else SK_Thread_SpinUntilFlaggedEx (&__dxgi_ready, 333UL, 15000UL);
  }

  return
    ReadAcquire (&__dxgi_ready);
}

DWORD __stdcall HookDXGI (LPVOID user);

#define D3D_FEATURE_LEVEL_12_0 (D3D_FEATURE_LEVEL)0xc000
#define D3D_FEATURE_LEVEL_12_1 (D3D_FEATURE_LEVEL)0xc100

const char*
SK_DXGI_DescribeScalingMode (DXGI_MODE_SCALING mode) noexcept
{
  switch (mode)
  {
    case DXGI_MODE_SCALING_CENTERED:
      return "DXGI_MODE_SCALING_CENTERED";
    case DXGI_MODE_SCALING_UNSPECIFIED:
      return "DXGI_MODE_SCALING_UNSPECIFIED";
    case DXGI_MODE_SCALING_STRETCHED:
      return "DXGI_MODE_SCALING_STRETCHED";
    default:
      return "UNKNOWN";
  }
}

const char*
SK_DXGI_DescribeScanlineOrder (DXGI_MODE_SCANLINE_ORDER order) noexcept
{
  switch (order)
  {
  case DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED:
    return "DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED";
  case DXGI_MODE_SCANLINE_ORDER_PROGRESSIVE:
    return "DXGI_MODE_SCANLINE_ORDER_PROGRESSIVE";
  case DXGI_MODE_SCANLINE_ORDER_UPPER_FIELD_FIRST:
    return "DXGI_MODE_SCANLINE_ORDER_UPPER_FIELD_FIRST";
  case DXGI_MODE_SCANLINE_ORDER_LOWER_FIELD_FIRST:
    return "DXGI_MODE_SCANLINE_ORDER_LOWER_FIELD_FIRST";
  default:
    return "UNKNOWN";
  }
}

#define DXGI_SWAP_EFFECT_FLIP_DISCARD (DXGI_SWAP_EFFECT)4

const char*
SK_DXGI_DescribeSwapEffect (DXGI_SWAP_EFFECT swap_effect) noexcept
{
  switch ((int)swap_effect)
  {
    case DXGI_SWAP_EFFECT_DISCARD:
      return    "Discard  (BitBlt)";
    case DXGI_SWAP_EFFECT_SEQUENTIAL:
      return "Sequential  (BitBlt)";
    case DXGI_SWAP_EFFECT_FLIP_DISCARD:
      return    "Discard  (Flip)";
    case DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL:
      return "Sequential  (Flip)";
    default:
      return "UNKNOWN";
  }
}

std::string
SK_DXGI_DescribeSwapChainFlags (DXGI_SWAP_CHAIN_FLAG swap_flags, INT* pLines)
{
  std::string out;

  if (swap_flags & DXGI_SWAP_CHAIN_FLAG_NONPREROTATED)
  out += " 0x001:  Non-Pre Rotated\n",                                      pLines ? (*pLines)++ : 0;

  if (swap_flags & DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH)
  out += " 0x002:  Allow Fullscreen Mode Switch\n",                         pLines ? (*pLines)++ : 0;

  if (swap_flags & DXGI_SWAP_CHAIN_FLAG_GDI_COMPATIBLE)
  out += " 0x004:  GDI Compatible\n",                                       pLines ? (*pLines)++ : 0;

  if (swap_flags & DXGI_SWAP_CHAIN_FLAG_RESTRICTED_CONTENT)
  out += " 0x008:  Copy Protected Content\n",                               pLines ? (*pLines)++ : 0;

  if (swap_flags & DXGI_SWAP_CHAIN_FLAG_RESTRICT_SHARED_RESOURCE_DRIVER)
  out += " 0x010:  DXGI_SWAP_CHAIN_FLAG_RESTRICT_SHARED_RESOURCE_DRIVER\n", pLines ? (*pLines)++ : 0;

  if (swap_flags & DXGI_SWAP_CHAIN_FLAG_DISPLAY_ONLY)
  out += " 0x020:  DXGI_SWAP_CHAIN_FLAG_DISPLAY_ONLY\n",                    pLines ? (*pLines)++ : 0;

  if (swap_flags & DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT)
  out += " 0x040:  Latency Waitable\n",                                     pLines ? (*pLines)++ : 0;

  if (swap_flags & DXGI_SWAP_CHAIN_FLAG_FOREGROUND_LAYER)
  out += " 0x080:  Foreground Layer\n",                                     pLines ? (*pLines)++ : 0;

  if (swap_flags & DXGI_SWAP_CHAIN_FLAG_FULLSCREEN_VIDEO)
  out += " 0x100:  Fullscreen Video\n",                                     pLines ? (*pLines)++ : 0;

  if (swap_flags & DXGI_SWAP_CHAIN_FLAG_YUV_VIDEO)
  out += " 0x200:  YUV Video\n",                                            pLines ? (*pLines)++ : 0;

  if (swap_flags & DXGI_SWAP_CHAIN_FLAG_HW_PROTECTED)
  out += " 0x400:  Hardware Copy Protected\n",                              pLines ? (*pLines)++ : 0;

  if (swap_flags & DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING)
  out += " 0x800:  Supports Tearing in Windowed Mode\n",                    pLines ? (*pLines)++ : 0;

  if (swap_flags & DXGI_SWAP_CHAIN_FLAG_RESTRICTED_TO_ALL_HOLOGRAPHIC_DISPLAYS)
  out += "0x1000:  Restricted To Holographic Displays\n",                   pLines ? (*pLines)++ : 0;

  return out;
}


std::string
SK_DXGI_FeatureLevelsToStr (       int    FeatureLevels,
                             const DWORD* pFeatureLevels )
{
  if (FeatureLevels == 0 || pFeatureLevels == nullptr)
    return "N/A";

  std::string out = "";

  for (int i = 0; i < FeatureLevels; i++)
  {
    switch (pFeatureLevels [i])
    {
    case D3D_FEATURE_LEVEL_9_1:
      out += " 9_1";
      break;
    case D3D_FEATURE_LEVEL_9_2:
      out += " 9_2";
      break;
    case D3D_FEATURE_LEVEL_9_3:
      out += " 9_3";
      break;
    case D3D_FEATURE_LEVEL_10_0:
      out += " 10_0";
      break;
    case D3D_FEATURE_LEVEL_10_1:
      out += " 10_1";
      break;
    case D3D_FEATURE_LEVEL_11_0:
      out += " 11_0";
      break;
    case D3D_FEATURE_LEVEL_11_1:
      out += " 11_1";
      break;
    case D3D_FEATURE_LEVEL_12_0:
      out += " 12_0";
      break;
    case D3D_FEATURE_LEVEL_12_1:
      out += " 12_1";
      break;
    default:
      out +=
        SK_FormatString (" Unknown (%u)", pFeatureLevels [i]);
      break;
    }
  }

  return out;
}


//
// In order to make DXGI Flip Model promotion work, sometimes it is necessary
//   to reuse SwapChains because the game has not destroyed the original
//     SwapChain before creating a new one...
//
//  It was never required in the original DXGI Blt Model that an HWND have
//    only one SwapChain at a time, so games that do this are not necessarily
//      broken.
//
//  * This compat hack is only needed in D3D11, and only rarely.
//
#define SK_DXGI_RECYCLE_SWAPCHAINS

UINT SK_DXGI_FixUpLatencyWaitFlag (gsl::not_null <IDXGISwapChain*> pSwapChainRaw, UINT Flags, BOOL bCreation = FALSE);

bool bAlwaysAllowFullscreen = false;

// Used for integrated GPU override
int SK_DXGI_preferred_adapter = SK_NoPreference;


void
WINAPI
SKX_D3D11_EnableFullscreen (bool bFullscreen)
{
  bAlwaysAllowFullscreen = bFullscreen;
}


void            SK_DXGI_HookPresent         (IDXGISwapChain* pSwapChain);
void  WINAPI    SK_DXGI_SetPreferredAdapter (int override_id) noexcept;


enum SK_DXGI_ResType {
  WIDTH  = 0,
  HEIGHT = 1
};

auto constexpr
SK_DXGI_RestrictResMax = []( SK_DXGI_ResType dim,
                             auto&           last,
                             auto            idx,
                             auto            covered,
             gsl::not_null <DXGI_MODE_DESC*> pDescRaw )->
bool
 {
   auto pDesc =
        pDescRaw.get ();

   UNREFERENCED_PARAMETER (last);

   auto& val = dim == WIDTH ? pDesc [idx].Width :
                              pDesc [idx].Height;

   auto  max = dim == WIDTH ? config.render.dxgi.res.max.x :
                              config.render.dxgi.res.max.y;

   bool covered_already = covered.count (idx) > 0;

   if ( (max > 0 &&
         val > max) || covered_already )
   {
     for ( int i = idx ; i > 0 ; --i )
     {
       if ( config.render.dxgi.res.max.x >= pDesc [i].Width  &&
            config.render.dxgi.res.max.y >= pDesc [i].Height &&
            covered.count (i) == 0 )
       {
         pDesc [idx] = pDesc [i];
         covered.insert (idx);
         covered.insert (i);
         return false;
       }
     }

     covered.insert (idx);

     pDesc [idx].Width  = config.render.dxgi.res.max.x;
     pDesc [idx].Height = config.render.dxgi.res.max.y;

     return true;
   }

   covered.insert (idx);

   return false;
 };

auto constexpr
SK_DXGI_RestrictResMin = []( SK_DXGI_ResType  dim,
                             auto&            first,
                             auto             idx,
                             auto             covered,
              gsl::not_null <DXGI_MODE_DESC*> pDescRaw )->
bool
 {
   auto pDesc =
        pDescRaw.get ();

   UNREFERENCED_PARAMETER (first);

   auto& val = dim == WIDTH ? pDesc [idx].Width :
                              pDesc [idx].Height;

   auto  min = dim == WIDTH ? config.render.dxgi.res.min.x :
                              config.render.dxgi.res.min.y;

   bool covered_already = covered.count (idx) > 0;

   if ( (min > 0 &&
         val < min) || covered_already )
   {
     for ( int i = 0 ; i < idx ; ++i )
     {
       if ( config.render.dxgi.res.min.x <= pDesc [i].Width  &&
            config.render.dxgi.res.min.y <= pDesc [i].Height &&
            covered.count (i) == 0 )
       {
         pDesc          [idx] = pDesc [i];
         covered.insert (idx);
         covered.insert (i);
         return false;
       }
     }

     covered.insert (idx);

     pDesc [idx].Width  = config.render.dxgi.res.min.x;
     pDesc [idx].Height = config.render.dxgi.res.min.y;

     return true;
   }

   covered.insert (idx);

   return false;
 };

bool
SK_DXGI_RemoveDynamicRangeFromModes ( int&             first,
                                      int              idx,
                                      std::set <int>&  removed,
                       gsl::not_null <DXGI_MODE_DESC*> pDescRaw,
                                      bool             no_hdr = true )
{
  UNREFERENCED_PARAMETER (first);

  auto pDesc =
       pDescRaw.get ();

  auto
    _IsWrongPresentFmt   =
  [&] (DXGI_FORMAT fmt) -> bool
  {
    switch (fmt)
    {
      case DXGI_FORMAT_R16G16B16A16_TYPELESS:
      case DXGI_FORMAT_R16G16B16A16_FLOAT:
        // FP16 is always HDR for output targets,
        //   it can be scRGB and no other encoding.
        //
        //  * For swapchain buffers it is possible to
        //    compose in SDR using a FP16 chain.
        ///
        //     -> Will not be automatically be clamped
        //          to sRGB range ; range-restrict the
        //            image unless you like pixels.

      case DXGI_FORMAT_R10G10B10A2_TYPELESS:
      case DXGI_FORMAT_R10G10B10A2_UINT:
      case DXGI_FORMAT_R10G10B10A2_UNORM:
        // 10-bit buffers may be sRGB, or HDR10 if
        //   using Flip Model / Fullscreen BitBlt.

      case DXGI_FORMAT_R10G10B10_XR_BIAS_A2_UNORM: // Is this even color-renderable?
        return (  no_hdr);

      default:
        return (! no_hdr);
    }
  };

  bool removed_already =
       removed.count (idx) > 0;

  if ( (_IsWrongPresentFmt (pDesc [idx].Format)) ||
          removed_already )
  {
    int removed_cnt = 0;

    for ( int i = 0 ; i < idx ; ++i )
    {
      if ( _IsWrongPresentFmt (pDesc [i].Format) &&
                      (removed.count (i) == 0  )
         )
      {
        pDesc          [i].Format = DXGI_FORMAT_UNKNOWN;
        removed.insert (i);
        removed_cnt++;
      }
    }

    if (removed_cnt != 0)
      return true;
  }

  return false;
} // We inserted DXGI_FORMAT_UNKNOWN for incompatible entries, but the
  //   list of descs needs to shrink after we finish inserting UNKNOWN.

dxgi_caps_t dxgi_caps;

BOOL
SK_DXGI_SupportsTearing (void)
{
  return dxgi_caps.swapchain.allow_tearing;
}

const wchar_t*
SK_DescribeVirtualProtectFlags (DWORD dwProtect) noexcept
{
  switch (dwProtect)
  {
  case 0x10:
    return L"Execute";
  case 0x20:
    return L"Execute + Read-Only";
  case 0x40:
    return L"Execute + Read/Write";
  case 0x80:
    return L"Execute + Read-Only or Copy-on-Write)";
  case 0x01:
    return L"No Access";
  case 0x02:
    return L"Read-Only";
  case 0x04:
    return L"Read/Write";
  case 0x08:
    return L" Read-Only or Copy-on-Write";
  default:
    return L"UNKNOWN";
  }
}


void
SK_DXGI_BeginHooking (void)
{
  volatile static ULONG hooked = FALSE;

  if (InterlockedCompareExchange (&hooked, TRUE, FALSE) == FALSE)
  {
#if 1
    //HANDLE hHookInitDXGI =
      SK_Thread_Create ( HookDXGI );
#else
    HookDXGI (nullptr);
#endif
  }

  else
    WaitForInitDXGI ();
}

#define WaitForInit() {      \
    WaitForInitDXGI      (); \
}

#define DXGI_STUB(_Return, _Name, _Proto, _Args)                          \
  _Return STDMETHODCALLTYPE                                               \
  _Name _Proto {                                                          \
    WaitForInit ();                                                       \
                                                                          \
    typedef _Return (STDMETHODCALLTYPE *passthrough_pfn) _Proto;          \
    static passthrough_pfn _default_impl = nullptr;                       \
                                                                          \
    if (_default_impl == nullptr) {                                       \
      static constexpr char* szName = #_Name;                             \
     _default_impl=(passthrough_pfn)SK_GetProcAddress(backend_dll,szName);\
                                                                          \
      if (_default_impl == nullptr) {                                     \
        dll_log->Log (                                                    \
          L"Unable to locate symbol  %s in dxgi.dll",                     \
          L#_Name);                                                       \
        return (_Return)E_NOTIMPL;                                        \
      }                                                                   \
    }                                                                     \
                                                                          \
    dll_log->Log (L"[   DXGI   ] [!] %s %s - "                            \
             L"[Calling Thread: 0x%04x]",                                 \
      L#_Name, L#_Proto, SK_Thread_GetCurrentId ());                      \
                                                                          \
    return _default_impl _Args;                                           \
}

#define DXGI_STUB_(_Name, _Proto, _Args)                                  \
  void STDMETHODCALLTYPE                                                  \
  _Name _Proto {                                                          \
    WaitForInit ();                                                       \
                                                                          \
    typedef void (STDMETHODCALLTYPE *passthrough_pfn) _Proto;             \
    static passthrough_pfn _default_impl = nullptr;                       \
                                                                          \
    if (_default_impl == nullptr) {                                       \
      static constexpr char* szName = #_Name;                             \
     _default_impl=(passthrough_pfn)SK_GetProcAddress(backend_dll,szName);\
                                                                          \
      if (_default_impl == nullptr) {                                     \
        dll_log->Log (                                                    \
          L"Unable to locate symbol  %s in dxgi.dll",                     \
          L#_Name );                                                      \
        return;                                                           \
      }                                                                   \
    }                                                                     \
                                                                          \
    dll_log->Log (L"[   DXGI   ] [!] %s %s - "                            \
              L"[Calling Thread: 0x%04x]",                                \
      L#_Name, L#_Proto, SK_Thread_GetCurrentId ());                      \
                                                                          \
    _default_impl _Args;                                                  \
}

int
SK_GetDXGIFactoryInterfaceVer (const IID& riid)
{
  if (riid == __uuidof (IDXGIFactory))
    return 0;
  if (riid == __uuidof (IDXGIFactory1))
    return 1;
  if (riid == __uuidof (IDXGIFactory2))
    return 2;
  if (riid == __uuidof (IDXGIFactory3))
    return 3;
  if (riid == __uuidof (IDXGIFactory4))
    return 4;
  if (riid == __uuidof (IDXGIFactory5))
    return 5;
  if (riid == __uuidof (IDXGIFactory6))
    return 6;
  if (riid == __uuidof (IDXGIFactory7))
    return 7;

  assert (false);

  return -1;
}

std::string
SK_GetDXGIFactoryInterfaceEx (const IID& riid)
{
  std::string interface_name;

  if (riid == __uuidof (IDXGIFactory))
    interface_name =  "      IDXGIFactory";
  else if (riid == __uuidof (IDXGIFactory1))
    interface_name =  "     IDXGIFactory1";
  else if (riid == __uuidof (IDXGIFactory2))
    interface_name =  "     IDXGIFactory2";
  else if (riid == __uuidof (IDXGIFactory3))
    interface_name =  "     IDXGIFactory3";
  else if (riid == __uuidof (IDXGIFactory4))
    interface_name =  "     IDXGIFactory4";
  else if (riid == __uuidof (IDXGIFactory5))
    interface_name =  "     IDXGIFactory5";
  else if (riid == __uuidof (IDXGIFactory6))
    interface_name =  "     IDXGIFactory6";
  else if (riid == __uuidof (IDXGIFactory7))
    interface_name =  "     IDXGIFactory7";
  else
  {
    wchar_t *pwszIID = nullptr;

    if (SUCCEEDED (StringFromIID (riid, (LPOLESTR *)&pwszIID)))
    {
      interface_name = SK_WideCharToUTF8 (pwszIID);
      CoTaskMemFree   (pwszIID);
    }
  }

  return interface_name;
}

int
SK_GetDXGIFactoryInterfaceVer (gsl::not_null <IUnknown *> pFactory)
{
  SK_ComPtr <IDXGIFactory5> pTemp;

  if (SUCCEEDED (
    pFactory->QueryInterface (__uuidof (IDXGIFactory7), (void **)&pTemp)))
  {
    dxgi_caps.device.enqueue_event    = true;
    dxgi_caps.device.latency_control  = true;
    dxgi_caps.present.flip_sequential = true;
    dxgi_caps.present.waitable        = true;
    dxgi_caps.present.flip_discard    = true;

    const HRESULT hr =
      pTemp->CheckFeatureSupport (
        DXGI_FEATURE_PRESENT_ALLOW_TEARING,
          &dxgi_caps.swapchain.allow_tearing,
            sizeof (dxgi_caps.swapchain.allow_tearing)
      );

    dxgi_caps.swapchain.allow_tearing =
      SUCCEEDED (hr) && dxgi_caps.swapchain.allow_tearing;

    dxgi_caps.init.store (true);

    return 7;
  }

  if (SUCCEEDED (
    pFactory->QueryInterface (__uuidof (IDXGIFactory6), (void **)&pTemp)))
  {
    dxgi_caps.device.enqueue_event    = true;
    dxgi_caps.device.latency_control  = true;
    dxgi_caps.present.flip_sequential = true;
    dxgi_caps.present.waitable        = true;
    dxgi_caps.present.flip_discard    = true;

    const HRESULT hr =
      pTemp->CheckFeatureSupport (
        DXGI_FEATURE_PRESENT_ALLOW_TEARING,
          &dxgi_caps.swapchain.allow_tearing,
            sizeof (dxgi_caps.swapchain.allow_tearing)
      );

    dxgi_caps.swapchain.allow_tearing =
      SUCCEEDED (hr) && dxgi_caps.swapchain.allow_tearing;

    dxgi_caps.init.store (true);

    return 6;
  }

  if (SUCCEEDED (
    pFactory->QueryInterface <IDXGIFactory5> ((IDXGIFactory5 **)(void **)&pTemp)))
  {
    dxgi_caps.device.enqueue_event    = true;
    dxgi_caps.device.latency_control  = true;
    dxgi_caps.present.flip_sequential = true;
    dxgi_caps.present.waitable        = true;
    dxgi_caps.present.flip_discard    = true;

    const HRESULT hr =
      pTemp->CheckFeatureSupport (
        DXGI_FEATURE_PRESENT_ALLOW_TEARING,
          &dxgi_caps.swapchain.allow_tearing,
            sizeof (dxgi_caps.swapchain.allow_tearing)
      );

    dxgi_caps.swapchain.allow_tearing =
      SUCCEEDED (hr) && dxgi_caps.swapchain.allow_tearing;

    dxgi_caps.init.store (true);

    return 5;
  }

  if (SUCCEEDED (
    pFactory->QueryInterface <IDXGIFactory4> ((IDXGIFactory4 **)(void **)&pTemp)))
  {
    dxgi_caps.device.enqueue_event    = true;
    dxgi_caps.device.latency_control  = true;
    dxgi_caps.present.flip_sequential = true;
    dxgi_caps.present.waitable        = true;
    dxgi_caps.present.flip_discard    = true;
    
    dxgi_caps.init.store (true);

    return 4;
  }

  if (SUCCEEDED (
    pFactory->QueryInterface <IDXGIFactory3> ((IDXGIFactory3 **)(void **)&pTemp)))
  {
    dxgi_caps.device.enqueue_event    = true;
    dxgi_caps.device.latency_control  = true;
    dxgi_caps.present.flip_sequential = true;
    dxgi_caps.present.waitable        = true;

    dxgi_caps.init.store (true);

    return 3;
  }

  if (SUCCEEDED (
    pFactory->QueryInterface <IDXGIFactory2> ((IDXGIFactory2 **)(void **)&pTemp)))
  {
    dxgi_caps.device.enqueue_event    = true;
    dxgi_caps.device.latency_control  = true;
    dxgi_caps.present.flip_sequential = true;

    dxgi_caps.init.store (true);

    return 2;
  }

  if (SUCCEEDED (
    pFactory->QueryInterface <IDXGIFactory1> ((IDXGIFactory1 **)(void **)&pTemp)))
  {
    dxgi_caps.device.latency_control  = true;

    dxgi_caps.init.store (true);

    return 1;
  }

  if (SUCCEEDED (
    pFactory->QueryInterface <IDXGIFactory> ((IDXGIFactory **)(void **)&pTemp)))
  {
    dxgi_caps.init.store (true);

    return 0;
  }

  assert (false);

  return -1;
}

std::string
SK_GetDXGIFactoryInterface (gsl::not_null <IUnknown *> pFactory)
{
  const int iver =
    SK_GetDXGIFactoryInterfaceVer (pFactory);

  if (iver == 7)
    return SK_GetDXGIFactoryInterfaceEx (__uuidof (IDXGIFactory7));

  if (iver == 6)
    return SK_GetDXGIFactoryInterfaceEx (__uuidof (IDXGIFactory6));

  if (iver == 5)
    return SK_GetDXGIFactoryInterfaceEx (__uuidof (IDXGIFactory5));

  if (iver == 4)
    return SK_GetDXGIFactoryInterfaceEx (__uuidof (IDXGIFactory4));

  if (iver == 3)
    return SK_GetDXGIFactoryInterfaceEx (__uuidof (IDXGIFactory3));

  if (iver == 2)
    return SK_GetDXGIFactoryInterfaceEx (__uuidof (IDXGIFactory2));

  if (iver == 1)
    return SK_GetDXGIFactoryInterfaceEx (__uuidof (IDXGIFactory1));

  if (iver == 0)
    return SK_GetDXGIFactoryInterfaceEx (__uuidof (IDXGIFactory));

  return "{Invalid-Factory-UUID}";
}

int
SK_GetDXGIAdapterInterfaceVer (const IID& riid)
{
  if (riid == __uuidof (IDXGIAdapter))
    return 0;
  if (riid == __uuidof (IDXGIAdapter1))
    return 1;
  if (riid == __uuidof (IDXGIAdapter2))
    return 2;
  if (riid == __uuidof (IDXGIAdapter3))
    return 3;
  if (riid == __uuidof (IDXGIAdapter4))
    return 4;

  assert (false);

  return -1;
}

std::string
SK_GetDXGIAdapterInterfaceEx (const IID& riid)
{
  std::string interface_name;

  if (riid == __uuidof (IDXGIAdapter))
    interface_name = "IDXGIAdapter";
  else if (riid == __uuidof (IDXGIAdapter1))
    interface_name = "IDXGIAdapter1";
  else if (riid == __uuidof (IDXGIAdapter2))
    interface_name = "IDXGIAdapter2";
  else if (riid == __uuidof (IDXGIAdapter3))
    interface_name = "IDXGIAdapter3";
  else if (riid == __uuidof (IDXGIAdapter4))
    interface_name = "IDXGIAdapter4";
  else
  {
    wchar_t *pwszIID = nullptr;

    if (SUCCEEDED (StringFromIID (riid, (LPOLESTR *)&pwszIID)))
    {
      interface_name = SK_WideCharToUTF8 (pwszIID);
      CoTaskMemFree   (pwszIID);
    }
  }

  return interface_name;
}

int
SK_GetDXGIAdapterInterfaceVer (gsl::not_null <IUnknown *> pAdapter)
{
  SK_ComPtr <IUnknown> pTemp;

  if (SUCCEEDED(
    pAdapter->QueryInterface (__uuidof (IDXGIAdapter3), (void **)&pTemp)))
  {
    return 3;
  }

  if (SUCCEEDED(
    pAdapter->QueryInterface (__uuidof (IDXGIAdapter2), (void **)&pTemp)))
  {
    return 2;
  }

  if (SUCCEEDED(
    pAdapter->QueryInterface (__uuidof (IDXGIAdapter1), (void **)&pTemp)))
  {
    return 1;
  }

  if (SUCCEEDED(
    pAdapter->QueryInterface (__uuidof (IDXGIAdapter), (void **)&pTemp)))
  {
    return 0;
  }

  assert (false);

  return -1;
}

std::string
SK_GetDXGIAdapterInterface (gsl::not_null <IUnknown *> pAdapter)
{
  const int iver =
    SK_GetDXGIAdapterInterfaceVer (pAdapter);

  if (iver == 4)
    return SK_GetDXGIAdapterInterfaceEx (__uuidof (IDXGIAdapter4));

  if (iver == 3)
    return SK_GetDXGIAdapterInterfaceEx (__uuidof (IDXGIAdapter3));

  if (iver == 2)
    return SK_GetDXGIAdapterInterfaceEx (__uuidof (IDXGIAdapter2));

  if (iver == 1)
    return SK_GetDXGIAdapterInterfaceEx (__uuidof (IDXGIAdapter1));

  if (iver == 0)
    return SK_GetDXGIAdapterInterfaceEx (__uuidof (IDXGIAdapter));

  return "{Invalid-Adapter-UUID}";
}

void
SK_ImGui_QueueResetD3D12 (void)
{
  InterlockedExchange (&__gui_reset_dxgi, TRUE);
}

void
SK_ImGui_QueueResetD3D11 (void)
{
  InterlockedExchange (&__gui_reset_dxgi, TRUE);
}

typedef HRESULT (WINAPI *IDXGISwapChain3_CheckColorSpaceSupport_pfn)
                        (IDXGISwapChain3*, DXGI_COLOR_SPACE_TYPE, UINT*);
                         IDXGISwapChain3_CheckColorSpaceSupport_pfn
                         IDXGISwapChain3_CheckColorSpaceSupport_Original =nullptr;

typedef HRESULT (WINAPI *IDXGISwapChain3_SetColorSpace1_pfn)
                        (IDXGISwapChain3*, DXGI_COLOR_SPACE_TYPE);
                         IDXGISwapChain3_SetColorSpace1_pfn
                         IDXGISwapChain3_SetColorSpace1_Original = nullptr;

typedef HRESULT (WINAPI *IDXGISwapChain4_SetHDRMetaData_pfn)
                        (IDXGISwapChain4*, DXGI_HDR_METADATA_TYPE, UINT, void*);
                         IDXGISwapChain4_SetHDRMetaData_pfn
                         IDXGISwapChain4_SetHDRMetaData_Original = nullptr;

const char*
DXGIColorSpaceToStr (DXGI_COLOR_SPACE_TYPE space) noexcept;

void
SK_DXGI_UpdateColorSpace (IDXGISwapChain3* This, DXGI_OUTPUT_DESC1 *outDesc)
{
  if (This != nullptr)
  {
    SK_RenderBackend& rb =
      SK_GetCurrentRenderBackend ();

    if (outDesc != nullptr)
    {
      ///// SDR (sRGB)
      if (outDesc->ColorSpace == DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709)
      {
        rb.setHDRCapable (false);
      }

      else if (
        outDesc->ColorSpace == DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020 ||
        outDesc->ColorSpace == DXGI_COLOR_SPACE_RGB_FULL_G10_NONE_P709    ||
        outDesc->ColorSpace == DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P2020)
      {
        rb.setHDRCapable (true);
      }

      ///else
      ///{
      ///  rb.setHDRCapable (true);
      ///
      ///  SK_LOGi0 ( L"Unexpected IDXGIOutput6 ColorSpace: %s",
      ///               DXGIColorSpaceToStr (outDesc->ColorSpace) );
      ///}

      rb.scanout.bpc             = outDesc->BitsPerColor;

      // Initialize the colorspace using the containing output.
      //
      //  * It is not necessarily the active colorspace
      //
      //  Often it just reports DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020 to
      //    indicate HDR is enabled
      //
      if (rb.scanout.dxgi_colorspace == DXGI_COLOR_SPACE_CUSTOM)
          rb.scanout.dxgi_colorspace = DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709;
    }

    if ( rb.scanout.colorspace_override != DXGI_COLOR_SPACE_CUSTOM &&
         rb.scanout.dxgi_colorspace     != rb.scanout.colorspace_override )
    {
      DXGI_COLOR_SPACE_TYPE activeColorSpace =
        rb.hdr_capable ? rb.scanout.colorspace_override :
                           DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709;

      SK_ComQIPtr <IDXGISwapChain3>
           pSwap3 (This);
      if ( pSwap3.p != nullptr && __SK_HDR_UserForced &&
           SUCCEEDED (
             pSwap3->SetColorSpace1 (
               activeColorSpace
             )
           )
         )
      {
        SK_ComQIPtr <IDXGISwapChain4> pSwap4 (pSwap3);

        DXGI_HDR_METADATA_HDR10 hdr10   = { };

        hdr10.MinMasteringLuminance     = sk::narrow_cast <UINT>   (0);//rb.display_gamut.minY / 0.0001);
        hdr10.MaxMasteringLuminance     = sk::narrow_cast <UINT>   (rb.display_gamut.maxY);
        hdr10.MaxContentLightLevel      = sk::narrow_cast <UINT16> (rb.display_gamut.maxLocalY);
        hdr10.MaxFrameAverageLightLevel = sk::narrow_cast <UINT16> (rb.display_gamut.maxAverageY);

        hdr10.BluePrimary  [0]          = sk::narrow_cast <UINT16> (50000.0 * 0.1500/*rb.display_gamut.xb*/);
        hdr10.BluePrimary  [1]          = sk::narrow_cast <UINT16> (50000.0 * 0.0600/*rb.display_gamut.yb*/);
        hdr10.RedPrimary   [0]          = sk::narrow_cast <UINT16> (50000.0 * 0.6400/*rb.display_gamut.xr*/);
        hdr10.RedPrimary   [1]          = sk::narrow_cast <UINT16> (50000.0 * 0.3300/*rb.display_gamut.yr*/);
        hdr10.GreenPrimary [0]          = sk::narrow_cast <UINT16> (50000.0 * 0.3000/*rb.display_gamut.xg*/);
        hdr10.GreenPrimary [1]          = sk::narrow_cast <UINT16> (50000.0 * 0.6000/*rb.display_gamut.yg*/);
        hdr10.WhitePoint   [0]          = sk::narrow_cast <UINT16> (50000.0 * 0.3127/*rb.display_gamut.Xw*/);
        hdr10.WhitePoint   [1]          = sk::narrow_cast <UINT16> (50000.0 * 0.3290/*rb.display_gamut.Yw*/);

        DXGI_SWAP_CHAIN_DESC                                    swapDesc = { };
        if (pSwap4.p != nullptr && SUCCEEDED (pSwap4->GetDesc (&swapDesc)))
        {
          if (DirectX::BitsPerColor (swapDesc.BufferDesc.Format) >= 10)
            pSwap4->SetHDRMetaData (DXGI_HDR_METADATA_TYPE_HDR10, sizeof (hdr10), &hdr10);
        }

        rb.scanout.dxgi_colorspace = activeColorSpace;
      }
    }
  }
}


volatile LONG __SK_NVAPI_UpdateGSync = FALSE;

void
SK_DXGI_UpdateSwapChain (IDXGISwapChain* This)
{
  if (This == nullptr)
    return;

  SK_RenderBackend& rb =
    SK_GetCurrentRenderBackend ();

  SK_ComQIPtr <IDXGISwapChain> pRealSwap (This);
  SK_ComPtr   <ID3D11Device>   pDev;

  if ( SUCCEEDED (This->GetDevice (IID_PPV_ARGS (&pDev.p))) )
  {
    if (! pDev.IsEqualObject (rb.device))
    {
                              rb.d3d11.immediate_ctx = nullptr;
                              rb.setDevice (pDev.p);
    }
    else  pDev              = rb.device;

    if (! pRealSwap.IsEqualObject (rb.swapchain))
        rb.swapchain = pRealSwap.p;

    if (rb.d3d11.immediate_ctx == nullptr)
    {
      SK_ComPtr <ID3D11DeviceContext> pDevCtx;

      pDev->GetImmediateContext (
             (ID3D11DeviceContext **)&pDevCtx.p);
      rb.d3d11.immediate_ctx    =     pDevCtx;
    }

    if (sk::NVAPI::nv_hardware && config.apis.NvAPI.gsync_status)
    {
      InterlockedExchange (&__SK_NVAPI_UpdateGSync, TRUE);
    }
  }

  else
  {
    assert (rb.device != nullptr);

    if (! pRealSwap.IsEqualObject (rb.swapchain))
          rb.swapchain =         pRealSwap.p;

    //dll_log->Log (
    //  L"UpdateSwapChain FAIL :: No D3D11 Device [ Actually: %ph ]",
    //    rb.device
    //);
  }

  if (rb.stale_display_info)
  {
    SK_ComPtr <IDXGIOutput>                    pOutput;
    if (SUCCEEDED (This->GetContainingOutput (&pOutput.p)))
    {
      DXGI_OUTPUT_DESC                  outDesc = { };
      if (SUCCEEDED (pOutput->GetDesc (&outDesc)))
      {
        SK_ComQIPtr <IDXGISwapChain>
          pChain (rb.swapchain.p);
        if (pChain.p != nullptr)
        {
          DXGI_SWAP_CHAIN_DESC     swapDesc = { };
          pChain->GetDesc        (&swapDesc);
          rb.assignOutputFromHWND (swapDesc.OutputWindow);
        }

        else
          rb.assignOutputFromHWND (game_window.hWnd);

        rb.monitor = outDesc.Monitor;
        rb.updateOutputTopology ();
        rb.stale_display_info = false;
      }
    }
  }

  SK_ComQIPtr <IDXGISwapChain3> pSwap3 (This);

  if (pSwap3 != nullptr) SK_DXGI_UpdateColorSpace (
      pSwap3.p
  );
}

HRESULT
SK_D3D11_ClearSwapchainBackbuffer (IDXGISwapChain *pSwapChain, const float *pColor = nullptr)
{
  if (! config.render.dxgi.clear_flipped_chain)
    return S_OK;

  if (pColor == nullptr)
      pColor = config.render.dxgi.chain_clear_color;

  SK_ComPtr <ID3D11Texture2D>         pBackbuffer;
  SK_ComPtr <ID3D11RenderTargetView>  pBackbufferRTV;

  SK_ComQIPtr <IDXGISwapChain3>     pSwap3  (pSwapChain);
  SK_ComPtr   <ID3D11Device>        pDev;
  SK_ComPtr   <ID3D11DeviceContext> pDevCtx;

  if (pSwapChain != nullptr)
      pSwapChain->GetDevice (IID_ID3D11Device, (void **)&pDev.p);
  else
    return E_NOTIMPL;

  if (pDev.p != nullptr)
      pDev->GetImmediateContext (&pDevCtx.p);

  if (  pSwap3.p == nullptr ||
          pDev.p == nullptr ||
       pDevCtx.p == nullptr )
  {
    return E_NOINTERFACE;
  }

  // XXX: For D3D11, simply use currentBuffer = 0 always
  UINT currentBuffer =
    0;// pSwap3->GetCurrentBackBufferIndex ();
      

  if ( SUCCEEDED ( pSwap3->GetBuffer (
                     currentBuffer,
                       IID_PPV_ARGS (&pBackbuffer.p)
     )           )                   )
  {
    ID3D11RenderTargetView* pRawRTV = nullptr;

#ifdef __SK_NO_FLIP_MODEL
    SK_ComPtr <ID3D11DepthStencilView> pOrigDSV;
    SK_ComPtr <ID3D11RenderTargetView> pOrigRTVs [D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT];
    rb.d3d11.immediate_ctx->OMGetRenderTargets (  D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT,
                                      &pOrigRTVs [0].p,
                                      &pOrigDSV     .p );

    for ( auto& rtv : pOrigRTVs )
    {
      if (rtv.p != nullptr)
      {
        SK_ComPtr   <ID3D11Resource> pResource;
        SK_ComQIPtr <ID3D11Resource> pBackbufferRes
                                    (pBackbuffer.p);

        rtv->GetResource (
          &pResource.p );
        if (pResource.IsEqualObject (pBackbufferRes.p))
        {
          pRawRTV = rtv;
          break;
        }
      }
    }
#endif

    // NOTE: This will exist even if the system does not support HDR
    if (
#ifdef __SK_NO_FLIP_MODEL
                                    pRawRTV   != nullptr ||
#endif
         ( _d3d11_rbk->frames_.size () > 0               &&
           _d3d11_rbk->frames_ [0].hdr.pRTV.p != nullptr &&
           SK_D3D11_EnsureMatchingDevices (_d3d11_rbk->frames_ [0].hdr.pRTV, pDev.p) )
       )
    {
      if (pRawRTV == nullptr)
          pRawRTV = _d3d11_rbk->frames_ [0].hdr.pRTV.p;

      SK_ComQIPtr <ID3D11DeviceContext4>
          pDevCtx4 (pDevCtx);
      if (pDevCtx4.p != nullptr)
      {
        pDevCtx4->ClearView (
          pRawRTV, pColor,
            nullptr, 0
        );

        return S_OK;
      }
    }

    else if ( SK_D3D11_EnsureMatchingDevices (pSwapChain, pDev) &&
              SUCCEEDED ( pDev->CreateRenderTargetView (
                                        pBackbuffer, nullptr,
                                       &pBackbufferRTV.p       )
            )           )
    {
      SK_ComPtr <ID3D11DepthStencilView> pOrigDSV;
      SK_ComPtr <ID3D11RenderTargetView> pOrigRTVs [D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT];
                     pDevCtx->OMGetRenderTargets (  D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT,
                                        &pOrigRTVs [0].p,
                                        &pOrigDSV     .p );

      pDevCtx->OMSetRenderTargets    (1, &pBackbufferRTV.p, nullptr);
      pDevCtx->ClearRenderTargetView (    pBackbufferRTV.p,  pColor);
      pDevCtx->OMSetRenderTargets    ( D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT,
                                       &pOrigRTVs [0].p,
                                        pOrigDSV     .p );

      return S_OK;
    }
  }

  return E_UNEXPECTED;
}

HRESULT
SK_D3D11_InsertBlackFrame (void)
{
  const SK_RenderBackend& rb =
    SK_GetCurrentRenderBackend ();

  SK_ComPtr <ID3D11Texture2D>         pBackbuffer0;
  SK_ComPtr <ID3D11Texture2D>         pBackbuffer1;

  SK_ComPtr <ID3D11RenderTargetView>  pBackbuffer0RTV;
  SK_ComPtr <ID3D11RenderTargetView>  pBackbuffer1RTV;

  SK_ComQIPtr <IDXGISwapChain3> pSwap3  (rb.swapchain);
  SK_ComQIPtr <ID3D11Device>    pDevice (rb.device);

  if (                          pSwap3.p == nullptr ||
                               pDevice.p == nullptr ||
                  rb.d3d11.immediate_ctx == nullptr )
  {
    return E_NOINTERFACE;
  }

  UINT currentBuffer = 0;

  SK_ComPtr <ID3D11Texture2D> pBackbufferCopy;

  if ( SUCCEEDED ( pSwap3->GetBuffer (
                     currentBuffer,
                       IID_PPV_ARGS (&pBackbuffer0.p)
                 )                   )
     )
  {
    D3D11_TEXTURE2D_DESC    texDesc = { };
    pBackbuffer0->GetDesc (&texDesc);

    if ( SUCCEEDED ( pDevice->CreateRenderTargetView (
                                      pBackbuffer0, nullptr,
                                     &pBackbuffer0RTV.p      )
                   ) &&
         SUCCEEDED ( pDevice->CreateTexture2D (&texDesc, nullptr, &pBackbufferCopy.p) )
       )
    {
      rb.d3d11.immediate_ctx->CopyResource (
        pBackbufferCopy.p,
        pBackbuffer0.p
      );

      SK_ComQIPtr <ID3D11DeviceContext1> pDevCtx1 (rb.d3d11.immediate_ctx);

      static constexpr FLOAT
        fClearColor [] = { 0.00f, 0.0f, 0.0f, 1.0f };

      SK_ComPtr <ID3D11DepthStencilView> pOrigDSV;
      SK_ComPtr <ID3D11RenderTargetView> pOrigRTVs [D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT];
      rb.d3d11.immediate_ctx->OMGetRenderTargets (  D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT,
                                        &pOrigRTVs [0].p,
                                        &pOrigDSV     .p );

      if (pDevCtx1 != nullptr)
          pDevCtx1->ClearView (pBackbuffer0RTV.p, fClearColor, nullptr, 0);
      else
      {
        rb.d3d11.immediate_ctx->OMSetRenderTargets (1, &pBackbuffer0RTV.p, nullptr);
        rb.d3d11.immediate_ctx->ClearRenderTargetView (
          pBackbuffer0RTV.p,  fClearColor             );
        rb.d3d11.immediate_ctx->OMSetRenderTargets (
          calc_count (                    &pOrigRTVs [0].p, D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT),
                                          &pOrigRTVs [0].p,
                                           pOrigDSV     .p );
      }

      HRESULT
      STDMETHODCALLTYPE
      SK_DXGI_Present ( IDXGISwapChain          *This,
                        UINT                     SyncInterval,
                        UINT                     Flags );

      SK_DXGI_Present ( pSwap3.p,
                          __SK_BFI_Interval, DXGI_PRESENT_RESTART | DXGI_PRESENT_DO_NOT_WAIT );

      //SK_DXGI_DispatchPresent1 ( pSwap3, 0, 0, nullptr,
      //                             SK_DXGI_Present1, SK_DXGI_PresentSource::Hook );

      if ( SUCCEEDED ( pSwap3->GetBuffer ( 0,
              IID_PPV_ARGS (&pBackbuffer1.p)
                     )                   )
         )
      {
        rb.d3d11.immediate_ctx->CopyResource (
          pBackbuffer1.p,
          pBackbufferCopy.p
        );

        pDevice->CreateRenderTargetView ( pBackbuffer1, nullptr,
                                         &pBackbuffer1RTV.p );

        pOrigRTVs [0] = pBackbuffer1RTV;

        rb.d3d11.immediate_ctx->OMSetRenderTargets (
          calc_count (                    &pOrigRTVs [0].p, D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT),
                                          &pOrigRTVs [0].p,
                                           pOrigDSV     .p );
      }

      return S_OK;
    }
  }

  return E_UNEXPECTED;
}

HRESULT
SK_D3D11_InsertDuplicateFrame (int MakeBreak = 0)
{
  if (_d3d11_rbk->frames_.empty ())
    return E_NOT_VALID_STATE;

  const SK_RenderBackend& rb =
    SK_GetCurrentRenderBackend ();

  SK_ComPtr <ID3D11Texture2D> pBackbuffer0;

  SK_ComQIPtr <IDXGISwapChain3> pSwap3  (rb.swapchain);
  SK_ComQIPtr <ID3D11Device>    pDevice (rb.device);

  if (                          pSwap3.p == nullptr ||
                               pDevice.p == nullptr ||
                  rb.d3d11.immediate_ctx == nullptr )
  {
    return E_NOINTERFACE;
  }

  UINT currentBuffer = 0;

  if ( SUCCEEDED ( pSwap3->GetBuffer (
                     currentBuffer,
                       IID_PPV_ARGS (&pBackbuffer0.p)
                 )                   )
     )
  {
    D3D11_TEXTURE2D_DESC    texDesc = { };
    pBackbuffer0->GetDesc (&texDesc);

    if ( MakeBreak == 0 &&
         ( _d3d11_rbk->frames_.at (0).latent_sync.pSwapChainCopy.p != nullptr ||
           SUCCEEDED ( pDevice->CreateTexture2D ( &texDesc, nullptr,
          &_d3d11_rbk->frames_.at (0).latent_sync.pSwapChainCopy.p )
         )           )
       )
    {
      rb.d3d11.immediate_ctx->CopyResource (
        _d3d11_rbk->frames_.at (0).latent_sync.pSwapChainCopy.p,
                                                 pBackbuffer0.p
      );
    }

    else if ( MakeBreak == 1 &&
                nullptr != _d3d11_rbk->frames_.at (0).latent_sync.pSwapChainCopy.p )
    {
      rb.d3d11.immediate_ctx->CopyResource (
                                                 pBackbuffer0.p,
        _d3d11_rbk->frames_.at (0).latent_sync.pSwapChainCopy.p
      );
    }

    return S_OK;
  }

  return E_UNEXPECTED;
}

// NVIDIA-only feature for now
//
void
SK_Framerate_AutoVRRCheckpoint (void)
{
  if (sk::NVAPI::nv_hardware)
  {
    // PresentMon is needed to handle these two cases
    if ((config.render.framerate.auto_low_latency.waiting) ||
        (config.render.framerate.auto_low_latency.triggered && config.render.framerate.auto_low_latency.policy.auto_reapply))
    {
      extern void SK_SpawnPresentMonWorker (void);
      SK_RunOnce (SK_SpawnPresentMonWorker (); config.apis.NvAPI.implicit_gsync = true;);
    }
  }
}

void
SK_D3D11_PostPresent (ID3D11Device* pDev, IDXGISwapChain* pSwap, HRESULT hr)
{
  SK_RenderBackend& rb =
    SK_GetCurrentRenderBackend ();

  if (SUCCEEDED (hr))
  {
    UINT currentBuffer = 0;

    bool __WantGSyncUpdate =
      ( (config.fps.show && config.osd.show ) || SK_ImGui_Visible || config.apis.NvAPI.implicit_gsync || config.render.framerate.auto_low_latency.waiting ) &&
                                                                 ReadAcquire (&__SK_NVAPI_UpdateGSync) != 0;

    if (__WantGSyncUpdate)
    {
      rb.surface.dxgi = nullptr;
      if ( SUCCEEDED ( pSwap->GetBuffer (
                         currentBuffer,
                           IID_PPV_ARGS (&rb.surface.dxgi.p)
                                        )
                     )
         )
      {
        if ( NVAPI_OK ==
               NvAPI_D3D_GetObjectHandleForResource (
                 pDev, rb.surface.dxgi, &rb.surface.nvapi
               )
           )
        {
          rb.gsync_state.update ();
          InterlockedExchange (&__SK_NVAPI_UpdateGSync, FALSE);
                      config.apis.NvAPI.implicit_gsync = false;
        }

        else rb.surface.dxgi = nullptr;
      }
    }

    // Queue-up Post-SK OSD Screenshots
    SK_Screenshot_ProcessQueue  (SK_ScreenshotStage::EndOfFrame,    rb);
    SK_Screenshot_ProcessQueue  (SK_ScreenshotStage::ClipboardOnly, rb);
    SK_Screenshot_ProcessQueue  (SK_ScreenshotStage::_FlushQueue,   rb);
    SK_D3D11_TexCacheCheckpoint (                                     );

    SK_Framerate_AutoVRRCheckpoint ();

    if (__SK_BFI)
    {
      SK_D3D11_InsertBlackFrame ();
    }

    else
      SK_D3D11_ClearSwapchainBackbuffer (pSwap);
  }
}

void
SK_D3D12_PostPresent (ID3D12Device* pDev, IDXGISwapChain* pSwap, HRESULT hr)
{
  UNREFERENCED_PARAMETER (pDev);
  UNREFERENCED_PARAMETER (pSwap);

  const SK_RenderBackend& rb =
    SK_GetCurrentRenderBackend ();

  if (SUCCEEDED (hr))
  {
    // Queue-up Post-SK OSD Screenshots
    SK_Screenshot_ProcessQueue  (SK_ScreenshotStage::EndOfFrame,    rb);
    SK_Screenshot_ProcessQueue  (SK_ScreenshotStage::ClipboardOnly, rb);
    SK_Screenshot_ProcessQueue  (SK_ScreenshotStage::_FlushQueue,   rb);

    SK_Framerate_AutoVRRCheckpoint ();
  }
}

void
SK_ImGui_DrawD3D12 (IDXGISwapChain* This)
{
  if (ReadAcquire (&__SK_DLL_Ending) != 0)
    return;

  if (This == nullptr)
    return;

  SK_RenderBackend_V2 &rb =
    SK_GetCurrentRenderBackend ();

  InterlockedIncrement (&__osd_frames_drawn);

  SK_ComQIPtr <IDXGISwapChain3>
                   pSwapChain3 (This);

  DXGI_SWAP_CHAIN_DESC1
              swapDesc1 = { };

  if (pSwapChain3 != nullptr)
      pSwapChain3->GetDesc1 (&swapDesc1);

  static bool init_once  = false;
  if (        init_once == false)
  {
    DXGI_SWAP_CHAIN_DESC
                    swapDesc = { };
    This->GetDesc (&swapDesc);

    if (IsWindow (swapDesc.OutputWindow) &&
                  swapDesc.OutputWindow  != 0)
    {
              init_once = true;

      SK_InstallWindowHook (swapDesc.OutputWindow);
    }
  }

  rb.framebuffer_flags &= (~SK_FRAMEBUFFER_FLAG_FLOAT);
  rb.framebuffer_flags &= (~SK_FRAMEBUFFER_FLAG_RGB10A2);
  rb.framebuffer_flags &= (~SK_FRAMEBUFFER_FLAG_SRGB);
//rb.framebuffer_flags &= (~SK_FRAMEBUFFER_FLAG_HDR); // We'll set and clear this in response to SetColorSpace1

  // sRGB Correction for UIs
  switch (swapDesc1.Format)
  {
    // This shouldn't happen in D3D12...
    case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
    case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
    {
      SK_ReleaseAssert (!"D3D12 Backbuffer sRGB?!");
      rb.framebuffer_flags |= SK_FRAMEBUFFER_FLAG_SRGB;
    } break;

    case DXGI_FORMAT_R16G16B16A16_FLOAT:
    {
      rb.framebuffer_flags |= SK_FRAMEBUFFER_FLAG_FLOAT;
    } break;

    case DXGI_FORMAT_R10G10B10A2_TYPELESS:
    case DXGI_FORMAT_R10G10B10A2_UNORM:
      rb.framebuffer_flags |=  SK_FRAMEBUFFER_FLAG_RGB10A2;
      // Deliberately fall-through to default
      [[fallthrough]];

    default:
    {
    } break;
  }

  // Test for colorspaces that are NOT HDR and then unflag HDR if necessary
  if (! rb.scanout.nvapi_hdr.active)
  {
    if ( (rb.fullscreen_exclusive    &&
          rb.scanout.dxgi_colorspace == DXGI_COLOR_SPACE_CUSTOM) ||
          rb.scanout.dwm_colorspace  == DXGI_COLOR_SPACE_CUSTOM )
    {
      rb.framebuffer_flags &= ~SK_FRAMEBUFFER_FLAG_HDR;
    }
  }

  if (ImGui_DX12Startup (This))
  {
    auto& io =
      ImGui::GetIO ();

    io.DisplaySize.x             = static_cast <float> (swapDesc1.Width);
    io.DisplaySize.y             = static_cast <float> (swapDesc1.Height);

    io.DisplayFramebufferScale.x = static_cast <float> (swapDesc1.Width);
    io.DisplayFramebufferScale.y = static_cast <float> (swapDesc1.Height);

    _d3d12_rbk->present ((IDXGISwapChain3 *)This);
  }
}

void
SK_ImGui_DrawD3D11 (IDXGISwapChain* This)
{
  if (ReadAcquire (&__SK_DLL_Ending) != 0)
    return;

  if (! This)
    return;

  SK_RenderBackend& rb =
    SK_GetCurrentRenderBackend ();

  auto pDev =
    rb.getDevice <ID3D11Device> ();

  InterlockedIncrement (&__osd_frames_drawn);

  SK_TLS* pTLS =
    SK_TLS_Bottom ();

  if (! pTLS)
    return;

#define _SetupThreadContext()                                                   \
                             pTLS->imgui->drawing                      = FALSE; \
                             pTLS->texture_management.injection_thread = FALSE; \
  SK_ScopedBool auto_bool0 (&pTLS->imgui->drawing);                             \
  SK_ScopedBool auto_bool1 (&pTLS->texture_management.injection_thread);        \
                             pTLS->imgui->drawing                      = TRUE;  \
                             pTLS->texture_management.injection_thread = TRUE;  \

  _SetupThreadContext ();

  auto flag_result =
    SK_ImGui_FlagDrawing_OnD3D11Ctx (
      SK_D3D11_GetDeviceContextHandle (rb.d3d11.immediate_ctx)
    );

  SK_ScopedBool auto_bool2 (flag_result.first);
                           *flag_result.first = flag_result.second;

  if (InterlockedCompareExchange (&__gui_reset_dxgi, FALSE, TRUE))
  {
    ResetImGui_D3D11 (This);
  }

  else
  {
    static bool          once = false;
    if (! std::exchange (once, true))
    {     DXGI_SWAP_CHAIN_DESC
                      swapDesc = { };
      This->GetDesc (&swapDesc);
      if (            swapDesc.OutputWindow != game_window.hWnd &&
          IsWindow (  swapDesc.OutputWindow))
      {
        if (! SK_InstallWindowHook (swapDesc.OutputWindow))
          once = false;
      }
      // Uh oh, try again?
      else if (! IsWindow (swapDesc.OutputWindow))
          once = false;
    }
  }


  if ( pDev != nullptr )
  {
    assert (rb.device == pDev);

    if (ImGui_DX11Startup (This))
    {
      SK_ComPtr   <ID3D11Texture2D>     pBackBuffer       (_d3d11_rbk->frames_ [0].pRenderOutput);
      SK_ComQIPtr <ID3D11DeviceContext> pImmediateContext (rb.d3d11.immediate_ctx);

      if (! pBackBuffer)
      {
        _d3d11_rbk->release (This);
      }

      else
      {
        // Either rb.d3d11.immediate_ctx is not initialized yet,
        //   or something has gone very wrong.
        if (   pImmediateContext.p != nullptr &&
             ! pImmediateContext.IsEqualObject (_d3d11_rbk->_pDeviceCtx) )
        {
          SK_RunOnce (
            SK_LOGi0 (L"Immediate Context Unsafely Wrapped By Other Software")
          );
        }

        D3D11_TEXTURE2D_DESC          tex2d_desc = { };
        D3D11_RENDER_TARGET_VIEW_DESC rtdesc     = { };

        pBackBuffer->GetDesc (&tex2d_desc);

        rb.framebuffer_flags &= (~SK_FRAMEBUFFER_FLAG_FLOAT);
        rb.framebuffer_flags &= (~SK_FRAMEBUFFER_FLAG_RGB10A2);
        rb.framebuffer_flags &= (~SK_FRAMEBUFFER_FLAG_SRGB);
      //rb.framebuffer_flags &= (~SK_FRAMEBUFFER_FLAG_HDR); // We'll set and clear this in response to SetColorSpace1


        rtdesc.ViewDimension = tex2d_desc.SampleDesc.Count > 1 ?
                                 D3D11_RTV_DIMENSION_TEXTURE2DMS :
                                 D3D11_RTV_DIMENSION_TEXTURE2D;

        // sRGB Correction for UIs
        switch (tex2d_desc.Format)
        {
          case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
          case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
          {
            rtdesc.Format =
              ( tex2d_desc.Format == DXGI_FORMAT_R8G8B8A8_UNORM_SRGB ?
                                          DXGI_FORMAT_R8G8B8A8_UNORM :
                                          DXGI_FORMAT_B8G8R8A8_UNORM );

            rb.framebuffer_flags |= SK_FRAMEBUFFER_FLAG_SRGB;
          } break;

          case DXGI_FORMAT_R16G16B16A16_FLOAT:
            rb.framebuffer_flags |= SK_FRAMEBUFFER_FLAG_FLOAT;

            rtdesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;

            break;

          case DXGI_FORMAT_R10G10B10A2_TYPELESS:
          case DXGI_FORMAT_R10G10B10A2_UNORM:
            rb.framebuffer_flags |=  SK_FRAMEBUFFER_FLAG_RGB10A2;
            // Deliberately fall-through to default
            [[fallthrough]];

          default:
          {
            rtdesc.Format = tex2d_desc.Format;
          } break;
        }

        DXGI_SWAP_CHAIN_DESC swapDesc = { };
             This->GetDesc (&swapDesc);

        if ( tex2d_desc.SampleDesc.Count > 1 ||
               swapDesc.SampleDesc.Count > 1 )
        {
          rb.framebuffer_flags |= SK_FRAMEBUFFER_FLAG_MSAA;
        }

        else
        {
          rb.framebuffer_flags &= ~SK_FRAMEBUFFER_FLAG_MSAA;
        }


        // Test for colorspaces that are NOT HDR and then unflag HDR if necessary
        if (! rb.scanout.nvapi_hdr.active)
        {
          if ( (rb.fullscreen_exclusive    &&
                rb.scanout.dxgi_colorspace == DXGI_COLOR_SPACE_CUSTOM) ||
                rb.scanout.dwm_colorspace  == DXGI_COLOR_SPACE_CUSTOM )
          {
            rb.framebuffer_flags &= ~SK_FRAMEBUFFER_FLAG_HDR;
          }
        }

        _d3d11_rbk->present (This);
      }
    }
  }
}

HRESULT
STDMETHODCALLTYPE
SK_DXGI_Present ( IDXGISwapChain *This,
                  UINT            SyncInterval,
                  UINT            Flags )
{
  return
    Present_Original (This, SyncInterval, Flags);
}

HRESULT
STDMETHODCALLTYPE
SK_DXGI_Present1 ( IDXGISwapChain1         *This,
                   UINT                     SyncInterval,
                   UINT                     Flags,
             const DXGI_PRESENT_PARAMETERS *pPresentParameters )
{
  return
    Present1_Original (This, SyncInterval, Flags, pPresentParameters);
}

static bool first_frame = true;

enum class SK_DXGI_PresentSource
{
  Wrapper = 0,
  Hook    = 1
};

bool
SK_DXGI_TestSwapChainCreationFlags (DWORD dwFlags)
{
  if ( (dwFlags & DXGI_SWAP_CHAIN_FLAG_FULLSCREEN_VIDEO)   ==
                  DXGI_SWAP_CHAIN_FLAG_FULLSCREEN_VIDEO    ||
       (dwFlags & DXGI_SWAP_CHAIN_FLAG_YUV_VIDEO)          ==
                  DXGI_SWAP_CHAIN_FLAG_YUV_VIDEO           ||
       (dwFlags & DXGI_SWAP_CHAIN_FLAG_RESTRICTED_CONTENT) ==
                  DXGI_SWAP_CHAIN_FLAG_RESTRICTED_CONTENT  ||
       (dwFlags & DXGI_SWAP_CHAIN_FLAG_FOREGROUND_LAYER)   ==
                  DXGI_SWAP_CHAIN_FLAG_FOREGROUND_LAYER )
  {
    static bool logged = false;

    if (! logged)
    {
      logged = true;

      SK_LOGi0 ( L"Skipping SwapChain Present due to "
                 L"SwapChain Creation Flags: %x", dwFlags );
    }

    return false;
  }

  return true;
}

bool
SK_DXGI_TestPresentFlags (DWORD Flags) noexcept
{
  if (Flags & DXGI_PRESENT_TEST)
  {
    SK_LOG_ONCE ( L"[   DXGI   ] Skipping SwapChain Present due to "
                  L"SwapChain Present Flag (DXGI_PRESENT_TEST)" );

    return false;
  }

  // Commands will not execute, this call may block until VBLANK
  //   but that's all. There's not much SK is interested in, so
  //     we're gonna ignore unsequenced swaps.
  if (Flags & DXGI_PRESENT_DO_NOT_SEQUENCE)
    return false;

  return true;
}

void
SK_DXGI_SetupPluginOnFirstFrame ( IDXGISwapChain *This,
                                  UINT            SyncInterval,
                                  UINT            Flags )
{
  UNREFERENCED_PARAMETER (Flags);
  UNREFERENCED_PARAMETER (This);
  UNREFERENCED_PARAMETER (SyncInterval);

#ifdef _WIN64
  static auto game_id =
    SK_GetCurrentGameID ();

  switch (game_id)
  {
    case SK_GAME_ID::DarkSouls3:
      SK_DS3_PresentFirstFrame (This, SyncInterval, Flags);
      break;

    case SK_GAME_ID::WorldOfFinalFantasy:
    {
      SK_DeferCommand ("Window.Borderless toggle");
      SK_Sleep        (33);
      SK_DeferCommand ("Window.Borderless toggle");
    } break;
  }
#endif

  for ( auto first_frame_fn : plugin_mgr->first_frame_fns )
  {
    first_frame_fn (
      This, SyncInterval, Flags
    );
  }
}

using SK_DXGI_PresentStatusMap = std::map <HRESULT, std::string_view>;
const SK_DXGI_PresentStatusMap&
SK_DXGI_GetPresentStatusMap (void)
{
  static const
    SK_DXGI_PresentStatusMap
      static_map =
      {
        { DXGI_STATUS_OCCLUDED,                     "DXGI_STATUS_OCCLUDED"                     },
        { DXGI_STATUS_CLIPPED,                      "DXGI_STATUS_CLIPPED"                      },
        { DXGI_STATUS_MODE_CHANGED,                 "DXGI_STATUS_MODE_CHANGED"                 },
        { DXGI_STATUS_MODE_CHANGE_IN_PROGRESS,      "DXGI_STATUS_MODE_CHANGE_IN_PROGRESS"      },
        { DXGI_STATUS_GRAPHICS_VIDPN_SOURCE_IN_USE, "DXGI_STATUS_GRAPHICS_VIDPN_SOURCE_IN_USE" },
        { DXGI_STATUS_NO_REDIRECTION,               "DXGI_STATUS_NO_REDIRECTION"               },
        { DXGI_STATUS_NO_DESKTOP_ACCESS,            "DXGI_STATUS_NO_DESKTOP_ACCESS"            },

        // Not status codes, but likely to appear in the same error contexts
        { DXGI_ERROR_DEVICE_REMOVED,                "DXGI_ERROR_DEVICE_REMOVED"                },
        { DXGI_ERROR_DEVICE_RESET,                  "DXGI_ERROR_DEVICE_RESET"                  },
        { DXGI_ERROR_INVALID_CALL,                  "DXGI_ERROR_INVALID_CALL"                  },
      };

  return
    static_map;
}

std::string
SK_DXGI_DescribePresentStatus (HRESULT hrPresentStatus)
{
  static const auto& map =
    SK_DXGI_GetPresentStatusMap ();

  return
    map.contains (hrPresentStatus)         ?
    map.at       (hrPresentStatus).data () :
        SK_FormatString (
          "Unknown Present Status (%x)",
           (DWORD)hrPresentStatus );
}

auto _IsBackendD3D11 = [](const SK_RenderAPI& api) { return ( static_cast <int> (api) &
                                                              static_cast <int> (SK_RenderAPI::D3D11) ) ==
                                                              static_cast <int> (SK_RenderAPI::D3D11); };
auto _IsBackendD3D12 = [](const SK_RenderAPI& api) { return ( static_cast <int> (api) &
                                                              static_cast <int> (SK_RenderAPI::D3D12) ) ==
                                                              static_cast <int> (SK_RenderAPI::D3D12); };

volatile LONG lResetD3D11 = 0;
volatile LONG lResetD3D12 = 0;

volatile LONG lSkipNextPresent = 0;

volatile LONG lSemaphoreCount0 = 0;
volatile LONG lSemaphoreCount1 = 0;

HRESULT
SK_DXGI_PresentBase ( IDXGISwapChain         *This,
                      UINT                    SyncInterval,
                      UINT                    Flags,
                      SK_DXGI_PresentSource   Source,
                      PresentSwapChain_pfn    DXGISwapChain_Present,
                      Present1SwapChain1_pfn  DXGISwapChain1_Present1 = nullptr,
       const DXGI_PRESENT_PARAMETERS         *pPresentParameters      = nullptr
)
{
  if (This == nullptr) // This can't happen, just humor static analysis
    return DXGI_ERROR_INVALID_CALL;

  SK_RenderBackend& rb =
    SK_GetCurrentRenderBackend ();

  const auto& display =
    rb.displays [rb.active_display];

  const bool bDLSS3OnVRRDisplay =
    (__SK_IsDLSSGActive && display.nvapi.vrr_enabled);

  auto _Present = [&](UINT _SyncInterval,
                      UINT _Flags) ->
  HRESULT
  {
    // Disable fractional VSYNC for the first 120 frames in order to
    //   detect VRR support correctly.
    if (_SyncInterval > 1 && SK_GetFramesDrawn () < 120)
        _SyncInterval = 1;

    if ( config.render.framerate.target_fps_bg > 0.0f && 
         config.render.framerate.target_fps_bg < rb.getActiveRefreshRate () / 2.0f &&
         (! SK_IsGameWindowActive ()) )
    {
      if ( SK_GetFramesDrawn () > 30 && display.nvapi.vrr_enabled &&
           ( config.window.background_render ||
             config.window.always_on_top == SmartAlwaysOnTop ) )
      {
        SK_RunOnce (
          SK_ImGui_CreateNotification (
            "VRR.BackgroundFixup", SK_ImGui_Toast::Warning,

            "A very low Background FPS limit is currently active; VRR must be allowed to\r\n"
            "disengage or it will severely impact overall system responsiveness!\r\n\r\n  "

            ICON_FA_INFO_CIRCLE
            "  VRR support will be restored once the game regains foreground status."
            
            /*
            "\r\n\r\n"

            "If VRR remains active and your system continues to behave sluggishly, it is\r\n"
            "most likely because you have forced VSYNC on in driver settings...\r\n\r\n  "

            ICON_FA_COGS
            "  VRR display users should never force VSYNC on using driver overrides!"
            */,

            "\tTemporarily Suspending VRR Support for the Current Game",
              5000, SK_ImGui_Toast::ShowCaption |
                    SK_ImGui_Toast::ShowTitle   |
                    SK_ImGui_Toast::ShowOnce    |
                    SK_ImGui_Toast::UseDuration );
        );
      }

      _SyncInterval
             = std::clamp (
               (UINT)(rb.getActiveRefreshRate () / config.render.framerate.target_fps_bg),
                                              2U, 4U );
      _Flags = (_Flags & ~DXGI_PRESENT_ALLOW_TEARING)
                       |  DXGI_PRESENT_RESTART;
    }

    BOOL bFullscreen =
      rb.isTrueFullscreen ();

    // Only works in Windowed +
    //  ... needs a special SwapChain creation flag and Flip Model
    if (_Flags & DXGI_PRESENT_ALLOW_TEARING)
    {
      BOOL  bIgnore = bFullscreen;
      if (! bIgnore)
      {
        DXGI_SWAP_CHAIN_DESC swapDesc = { };
        This->GetDesc      (&swapDesc);

        if (! (swapDesc.Flags & DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING))
          bIgnore = TRUE;

        if (! SK_DXGI_IsFlipModelSwapEffect (swapDesc.SwapEffect))
          bIgnore = TRUE;
      }

      if (bIgnore || (SUCCEEDED (This->GetFullscreenState (&bIgnore, nullptr)) && bIgnore))
      {
        // Remove this flag
        _Flags &= ~DXGI_PRESENT_ALLOW_TEARING;
      }
    
      // Turn tearing off when using frame generation
      if (bDLSS3OnVRRDisplay)
      {
        _Flags &= ~DXGI_PRESENT_ALLOW_TEARING;
        _SyncInterval = 0;
      }
    }

    // Only works in Fullscreen
    if (_Flags & DXGI_PRESENT_RESTART)
    {
      if (! (bFullscreen || (SUCCEEDED (This->GetFullscreenState (&bFullscreen, nullptr)) && bFullscreen)))
      {
        // Remove this flag
        _Flags &= ~DXGI_PRESENT_RESTART;
      }
    }

    // If Unreal Engine sees DXGI_ERROR_INVALID_CALL due to flip model fullscreen
    //   transition, it's going to bitch about 'crashing'.
    auto _Ret = [&](HRESULT ret) -> HRESULT
    {
      if ( ret == DXGI_ERROR_DEVICE_REMOVED ||
           ret == DXGI_ERROR_DEVICE_RESET )
      {
        _d3d11_rbk->release (This);
        _d3d12_rbk->release (This);
      }

      if ( ret != S_OK && (! rb.active_traits.bOriginallyFlip) && rb.api != SK_RenderAPI::D3D12 )
      {
        // This would recurse infinitely without the ghetto lock
        //
        static volatile LONG             lLock  =  0;
        if (InterlockedCompareExchange (&lLock, 1, 0) == 0)
        {
          HRESULT hrTest =
            SK_DXGI_PresentBase ( This, SyncInterval, DXGI_PRESENT_TEST,
                                    Source, DXGISwapChain_Present,
                                            DXGISwapChain1_Present1,
                                              pPresentParameters );

          if ( hrTest == DXGI_STATUS_MODE_CHANGE_IN_PROGRESS )
          {
            BOOL                       bFullscreen = FALSE;
            This->GetFullscreenState (&bFullscreen, nullptr);

            DXGI_SWAP_CHAIN_DESC swapDesc = { };
            This->GetDesc      (&swapDesc);
            DXGI_MODE_DESC       modeDesc =
                     { .Width  = swapDesc.BufferDesc.Width,
                       .Height = swapDesc.BufferDesc.Height,
                       .Format = swapDesc.BufferDesc.Format };
            if ( FAILED (
              This->ResizeBuffers ( swapDesc.BufferCount,
                                    swapDesc.BufferDesc.Width,
                                    swapDesc.BufferDesc.Height,
                                    swapDesc.BufferDesc.Format,
                                    swapDesc.Flags )
                        )
               )
            {
            //_d3d11_rbk->release (This);
            //_d3d12_rbk->release (This);

              This->ResizeTarget (&modeDesc);

              if ( FAILED (
                This->ResizeBuffers ( swapDesc.BufferCount,
                                      swapDesc.BufferDesc.Width,
                                      swapDesc.BufferDesc.Height,
                                      swapDesc.BufferDesc.Format,
                                      swapDesc.Flags )
                          )
                 )
              {
                // Fullscreen Mode Switch is -BORKED- (generally due to Flip Model)
                //
                //  --> Fallback to Windowed Mode
                if (SUCCEEDED (This->SetFullscreenState (! bFullscreen, nullptr)))
                               This->ResizeBuffers ( swapDesc.BufferCount, swapDesc.BufferDesc.Width,
                                                                           swapDesc.BufferDesc.Height,
                                                     swapDesc.BufferDesc.Format,
                                                     swapDesc.Flags );

                static LONG64      lLastFrameWarned = -10000LL;
                if (std::exchange (lLastFrameWarned, (LONG64)SK_GetFramesDrawn ()) <
                                                     (LONG64)SK_GetFramesDrawn () - 50LL)
                {
                  SK_ImGui_ReportModeSwitchFailure ();
                }
              }
            }
          }
          InterlockedExchange (&lLock, 0);
        }
      }

      return
        ret == DXGI_ERROR_INVALID_CALL ? DXGI_STATUS_MODE_CHANGE_IN_PROGRESS
                                       : ret;

      // Change the FAIL'ing HRESULT to a SUCCESS status code instead; SK will
      //   finish the flip model swapchain resize on the next call to Present (...).
    };

    if (Source == SK_DXGI_PresentSource::Hook)
    {
      if (DXGISwapChain1_Present1 != nullptr)
      {
        return
          _Ret (
            DXGISwapChain1_Present1 ( (IDXGISwapChain1 *)This,
                                        _SyncInterval,
                                          _Flags,
                                            pPresentParameters )
          );
      }

      return
        _Ret (
          DXGISwapChain_Present ( This,
                                    _SyncInterval,
                                      _Flags )
        );
    }

    if (DXGISwapChain1_Present1 != nullptr)
    {
      return
        _Ret (
          ((IDXGISwapChain1 *)This)->Present1 (_SyncInterval, _Flags, pPresentParameters)
        );
    }

    return
      _Ret (
        This->Present (_SyncInterval, _Flags)
      );
    };

  if ((! config.render.dxgi.allow_tearing) || bDLSS3OnVRRDisplay)
  {                                           // Turn tearing off when using frame generation
    if (Flags & DXGI_PRESENT_ALLOW_TEARING)
    {
      SK_RunOnce (
        SK_LOGi0 (L"Removed Tearing From DXGI Present Call")
      );

      Flags &= ~DXGI_PRESENT_ALLOW_TEARING;
    }

    if (bDLSS3OnVRRDisplay)
    {
      SyncInterval = 0;
    }
  }

  //
  // Early-out for games that use testing to minimize blocking
  //
  if (! SK_DXGI_TestPresentFlags (Flags))
  {
    HRESULT hrPresent =
             _Present ( SyncInterval, Flags );

    if ((Flags & DXGI_PRESENT_TEST) ==
                 DXGI_PRESENT_TEST)
    {
      if (hrPresent != S_OK)
      {
        if (hrPresent == DXGI_STATUS_CLIPPED)
        {
          SK_LOGi1 (L" * DXGI_PRESENT_TEST returned DXGI_STATUS_CLIPPED; "
                    L"Ignored, rendering continues!");
        }

        else if (hrPresent == DXGI_STATUS_MODE_CHANGED)
        {
          SK_LOGi1 (
            L" * DXGI_PRESENT_TEST returned %hs; "
            L"Resizing the SwapChain for Compliance!",
              SK_DXGI_DescribePresentStatus (hrPresent).c_str ()
          );

          DXGI_SWAP_CHAIN_DESC swapDesc = { };
          This->GetDesc      (&swapDesc);
          This->ResizeBuffers ( swapDesc.BufferCount,       swapDesc.BufferDesc.Width,
                                                            swapDesc.BufferDesc.Height,
                                swapDesc.BufferDesc.Format, swapDesc.Flags );
        }

        else if (hrPresent == DXGI_STATUS_OCCLUDED                     ||
                 hrPresent == DXGI_STATUS_GRAPHICS_VIDPN_SOURCE_IN_USE ||
                 hrPresent == DXGI_STATUS_NO_DESKTOP_ACCESS)
        {
          SK_LOGi1 (
            L" * DXGI_PRESENT_TEST returned %hs; "
            L"yielding thread execution...",
              SK_DXGI_DescribePresentStatus (hrPresent).c_str ()
          );

          SK_DelayExecution (40.0, TRUE);
        }

        else
        {
          SK_LOGi1 (
            L" * DXGI_PRESENT_TEST returned %hs; "
            L"Ohnoes Everyone Panic!",
                SK_DXGI_DescribePresentStatus (hrPresent).c_str ()
          );

          SK_DelayExecution (5.0, TRUE);
        }
      }
    }


    // Third-party software doesn't always behave compliantly in games that
    //   use presentation testing... so we may need to resort to this or
    //     the game's performance derails itself.
    if ( config.render.dxgi.present_test_skip && (Flags & DXGI_PRESENT_TEST)
                                                       == DXGI_PRESENT_TEST )
      return S_OK;

    return
      hrPresent;
  }

  // Sync Interval Clamp  (NOTE: SyncInterval > 1 Disables VRR)
  //
  if ( config.render.framerate.sync_interval_clamp != SK_NoPreference &&
       config.render.framerate.sync_interval_clamp < sk::narrow_cast <int> (SyncInterval) )
  {
    SyncInterval = config.render.framerate.sync_interval_clamp;
  }

  const bool bWaitOnFailure =
    (Flags & DXGI_PRESENT_DO_NOT_WAIT)
          != DXGI_PRESENT_DO_NOT_WAIT;

  DXGI_SWAP_CHAIN_DESC desc = { };
       This->GetDesc (&desc);

  if (! SK_DXGI_TestSwapChainCreationFlags (desc.Flags))
  {
    SK_D3D11_EndFrame ();
    SK_D3D12_EndFrame ();

    return
      _Present ( SyncInterval, Flags );
  }

  auto *pLimiter =
    SK::Framerate::GetLimiter (This);

  bool process                = false;
  bool has_wrapped_swapchains =
    ReadAcquire (&SK_DXGI_LiveWrappedSwapChain1s) > 0 ||
    ReadAcquire (&SK_DXGI_LiveWrappedSwapChains)  > 0;
  // ^^^ It's not required that IDXGISwapChain1 or higher invokes Present1!

  if (config.render.osd.draw_in_vidcap && has_wrapped_swapchains)
  {
    process =
      (Source == SK_DXGI_PresentSource::Wrapper);
  }

  else
  {
    process =
      ( Source == SK_DXGI_PresentSource::Hook ||
        (! has_wrapped_swapchains)               );
  }


  SK_ComPtr <ID3D11Device> pDev;
  SK_ComPtr <ID3D12Device> pDev12;

  if (FAILED (This->GetDevice (IID_PPV_ARGS (&pDev12.p))))
              This->GetDevice (IID_PPV_ARGS (&pDev.p));


  struct osd_test_s {
          ULONG64 last_frame          = SK_GetFramesDrawn ();
          DWORD   dwLastCheck         = SK_timeGetTime    ();
    const DWORD         CheckInterval = 250;
  } static _osd;

  if ( SK_timeGetTime () > _osd.dwLastCheck + _osd.CheckInterval )
  {
    if (config.render.osd._last_vidcap_frame == _osd.last_frame                &&
                                      Source == SK_DXGI_PresentSource::Wrapper &&
     (! config.render.osd.draw_in_vidcap)
       )
    {
      SK_ImGui_Warning (L"Toggling 'Show OSD In Video Capture' because the OSD wasn't being drawn...");
      config.render.osd.draw_in_vidcap = true;
    }

    _osd.dwLastCheck = SK_timeGetTime    ();
    _osd.last_frame  = SK_GetFramesDrawn ();
  }


  if (process)
  {
    if (_IsBackendD3D11 (rb.api))
    {
      // Start / End / Readback Pipeline Stats
      SK_D3D11_UpdateRenderStats (This);
    }


    if (ReadULong64Acquire (&SK_Reflex_LastFrameMarked) <
        ReadULong64Acquire (&SK_RenderBackend::frames_drawn) - 1)
    {
      // D3D11 Draw Call / D3D12 Command List Exec hooks did not catch any rendering commands
      rb.setLatencyMarkerNV (SIMULATION_END);
      rb.setLatencyMarkerNV (RENDERSUBMIT_START);
    }


    if (rb.api == SK_RenderAPI::Reserved || rb.api == SK_RenderAPI::D3D12)
    {
      // Late initialization
      //
      static bool                           late_init = false;
      if (config.apis.dxgi.d3d12.hook && (! late_init) && SUCCEEDED (This->GetDevice (IID_PPV_ARGS (&pDev12.p))))
      {
        late_init =
          SK_D3D12_HotSwapChainHook ((IDXGISwapChain3 *)This, pDev12.p);
      }

      if (! pDev12.p)
      {
        // Establish the API used this frame (and handle translation layers)
        //
        switch (SK_GetDLLRole ())
        {
          case DLL_ROLE::D3D8:
            rb.api = SK_RenderAPI::D3D8On11;
            break;
          case DLL_ROLE::DDraw:
            rb.api = SK_RenderAPI::DDrawOn11;
            break;
          default:
            rb.api = SK_RenderAPI::D3D11;
            break;
        }
      }
      else
      {
        // Establish the API used this frame (and handle translation layers)
        //
        switch (SK_GetDLLRole ())
        {
          case DLL_ROLE::D3D8:
            rb.api = SK_RenderAPI::D3D8On12;
            break;
          case DLL_ROLE::DDraw:
            rb.api = SK_RenderAPI::DDrawOn12;
            break;
          default:
            rb.api = SK_RenderAPI::D3D12;
            break;
        }
      }
    }


    if (std::exchange (first_frame, false))
    {
      // Setup D3D11 Device Context Late (on first draw)
      if (rb.device.p == nullptr)
      {
        SK_ComPtr <ID3D11Device> pSwapDev;

        if (SUCCEEDED (This->GetDevice (IID_PPV_ARGS (&pSwapDev.p))))
        {
          if (rb.swapchain == nullptr)
              rb.swapchain = This;

          SK_ComPtr <ID3D11DeviceContext> pDevCtx;
          pSwapDev->GetImmediateContext (&pDevCtx.p);
                 rb.d3d11.immediate_ctx = pDevCtx;
                 rb.setDevice            (pSwapDev);

          SK_LOGs0 ( L"  D3D 11  ",
                     L"Active D3D11 Device Context Established on first Presented Frame" );
        }
      }

      rb.updateOutputTopology ();

      if ( pDev   != nullptr ||
           pDev12 != nullptr )
      {
        int hooked = 0;

        if (_IsBackendD3D11 (rb.api))
        {
          SK_D3D11_PresentFirstFrame (This);
        }

        for ( auto& it : local_dxgi_records )
        {
          if (it->active)
          {
            SK_Hook_ResolveTarget (*it);

            // Don't cache addresses that were screwed with by other injectors
            const wchar_t* wszSection =
              StrStrIW (it->target.module_path, LR"(\sys)") ?
                                                L"DXGI.Hooks" : nullptr;

            if (! wszSection)
            {
              SK_LOGs0 ( L"Hook Cache",
                         L"Hook for '%hs' resides in '%s', will not cache!",
                            it->target.symbol_name,
             SK_ConcealUserDir (
                  std::wstring (
                            it->target.module_path
                               ).data ()
                               )
                       );
            }
            SK_Hook_CacheTarget ( *it, wszSection );

            ++hooked;
          }
        }

        if (SK_IsInjected ())
        {
          auto    it_global  = std::begin (global_dxgi_records);
          auto     it_local  = std::begin ( local_dxgi_records);
          while (  it_local != std::end   ( local_dxgi_records))
          {
            if (( *it_local )->hits                            &&
      StrStrIW (( *it_local )->target.module_path, LR"(\sys)") &&
                ( *it_local )->active)
            {
              SK_Hook_PushLocalCacheOntoGlobal ( **it_local,
                                                   **it_global );
            }

            else
            {
              ( *it_global )->target.addr = nullptr;
              ( *it_global )->hits        = 0;
              ( *it_global )->active      = false;
            }

            ++it_global, ++it_local;
          }
        }

        SK_D3D11_UpdateHookAddressCache ();

        if (hooked > 0)
        {
          SK_GetDLLConfig ()->write ();
        }
      }

      if (rb.scanout.colorspace_override != DXGI_COLOR_SPACE_CUSTOM)
      {
        SK_ComQIPtr <IDXGISwapChain3>
                              pSwap3 (This);
        if (       nullptr != pSwap3) SK_DXGI_UpdateColorSpace
                             (pSwap3);
      }

      SK_DXGI_SetupPluginOnFirstFrame (This, SyncInterval, Flags);

      SK_ComPtr                     <IDXGIDevice> pDevDXGI = nullptr;
      This->GetDevice (IID_IDXGIDevice, (void **)&pDevDXGI.p);

      if ( pDev != nullptr || pDev12 != nullptr)
      {
        // TODO: Clean this code up
        SK_ComPtr <IDXGIAdapter> pAdapter = nullptr;
        SK_ComPtr <IDXGIFactory> pFactory = nullptr;

        rb.swapchain = This;

        if ( pDevDXGI != nullptr                                          &&
             SUCCEEDED (pDevDXGI->GetAdapter               (&pAdapter.p)) &&
             SUCCEEDED (pAdapter->GetParent  (IID_PPV_ARGS (&pFactory.p))) )
        {
          if (config.render.dxgi.safe_fullscreen)
          {
            pFactory->MakeWindowAssociation ( nullptr, 0 );
          }

          if (bAlwaysAllowFullscreen)
          {
            pFactory->MakeWindowAssociation (
              desc.OutputWindow, DXGI_MWA_NO_WINDOW_CHANGES
            );
          }
        }
      }
    }

    int interval =
      config.render.framerate.present_interval;

    if (config.render.framerate.target_fps > 0.0f)
    {
      // Adaptive VSync
      if  ( config.render.framerate.present_interval > 0 &&
            config.render.framerate.turn_vsync_off       &&
            config.render.framerate.tearing_mode ==
              SK_TearingMode::AdaptiveOff                )
      {
        interval = 0;
      }

      // Latent VSync...
      else if ( config.render.framerate.present_interval == 0 &&
               !config.render.dxgi.allow_tearing              &&
                rb.isTrueFullscreen ()                        )
      {
        interval = 1;
      }
    }

    // Fix flags for compliance in broken games
    //
    if (SK_DXGI_IsFlipModelSwapChain (desc))
    {
      if ( (desc.Flags & DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING) ==
                         DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING )
      {
        if (! desc.Windowed)                        Flags &= ~DXGI_PRESENT_ALLOW_TEARING;
        else
        {
          // Special K Overrides
          if (interval == 0)
          {
            if (config.render.dxgi.allow_tearing)   Flags |=  DXGI_PRESENT_ALLOW_TEARING;
            else                                    Flags &= ~DXGI_PRESENT_ALLOW_TEARING;
          }

          // No user preference, game using 0 --> opportunity to enable DWM tearing
          else if (interval == SK_NoPreference && SyncInterval == 0)
          {
            if (config.render.dxgi.allow_tearing)   Flags |=  DXGI_PRESENT_ALLOW_TEARING;
          }
        }
      } // SwapChain was not created with tearing support
      else                                          Flags &= ~DXGI_PRESENT_ALLOW_TEARING;
    } // BitBlt needs no Tearing Flags!
    else                                            Flags &= ~DXGI_PRESENT_ALLOW_TEARING;

    int flags = Flags;

    if ( SK_DXGI_IsFlipModelSwapChain (desc) && pLimiter->get_limit () > 0.0L )
    {
      if (config.render.framerate.drop_late_flips)
      {
        flags |= DXGI_PRESENT_RESTART;
      }
    }

    // Application preference
    if (interval == SK_NoPreference)
        interval = SyncInterval;

    rb.present_interval      = interval;
    rb.present_interval_orig = SyncInterval;

    if (interval != 0 || rb.isTrueFullscreen ()) // FSE can't use this flag
      flags &= ~DXGI_PRESENT_ALLOW_TEARING;
    if (     _IsBackendD3D12 (rb.api)) SK_ImGui_DrawD3D12 (This);
    else if (_IsBackendD3D11 (rb.api)) SK_ImGui_DrawD3D11 (This);

    if ( pDev != nullptr || pDev12 != nullptr )
    {
      SK_BeginBufferSwapEx (bWaitOnFailure);
    }

    rb.setLatencyMarkerNV (PRESENT_START);

    if (config.render.framerate.swapchain_wait > 0)
    {
      SK_AutoHandle hWaitHandle (rb.getSwapWaitHandle ());
      if ((intptr_t)hWaitHandle.m_h > 0)
      {
        if (SK_WaitForSingleObject (hWaitHandle.m_h, 0) == WAIT_TIMEOUT)
        {
          interval  = 0;
          flags    |= DXGI_PRESENT_RESTART;
        }
      }
    }

    if (ReadAcquire        (&SK_RenderBackend::flip_skip) > 0) {
      InterlockedDecrement (&SK_RenderBackend::flip_skip);
      interval      = 0;
      flags |= DXGI_PRESENT_RESTART;
    }


    bool _SkipThisFrame = false;


    if (__SK_BFI)
      flags &= ~DXGI_PRESENT_RESTART;


    // Latent Sync
    if (config.render.framerate.present_interval == 0 &&
        config.render.framerate.target_fps        > 0.0f)
    {
      if (__SK_LatentSyncSkip != 0 && (__SK_LatentSyncFrame % __SK_LatentSyncSkip) != 0)
        _SkipThisFrame = true;
      else
        flags |= DXGI_PRESENT_RESTART;

      if (interval == 0) flags |=  DXGI_PRESENT_ALLOW_TEARING;
      else               flags &= ~DXGI_PRESENT_ALLOW_TEARING;
    }


#if 0
    if ( config.nvidia.sleep.low_latency_boost &&
         config.nvidia.sleep.enable            &&
         sk::NVAPI::nv_hardware                &&
         config.render.framerate.target_fps != 0.0 )
    {
      flags |= DXGI_PRESENT_DO_NOT_WAIT;
    }
#endif


    SK_LatentSync_BeginSwap ();

    if (config.render.framerate.present_interval == 0 &&
        config.render.framerate.target_fps        > 0.0f)
    {
      if (rb.d3d11.immediate_ctx != nullptr)
      {
        if (! _SkipThisFrame)
          SK_D3D11_InsertDuplicateFrame (0);
        else
          SK_D3D11_InsertDuplicateFrame (1);
      }
    }

    // Measure frametime before Present is issued
    if (config.fps.timing_method == SK_FrametimeMeasures_PresentSubmit)
    {
      SK::Framerate::TickEx (false, 0.0, { 0,0 }, rb.swapchain.p);
    }

    HRESULT hr =
      _SkipThisFrame ? _Present ( rb.d3d11.immediate_ctx != nullptr ?
                                                                  0 : 1,
                                                        DXGI_PRESENT_RESTART | DXGI_PRESENT_DO_NOT_WAIT |
                                ( ( rb.d3d11.immediate_ctx != nullptr) ? DXGI_PRESENT_ALLOW_TEARING
                                                                       : 0 ) ) :
                       _Present ( interval, flags );

    if (_SkipThisFrame)
      hr = S_OK;

    SK_LatentSync_EndSwap ();

    rb.setLatencyMarkerNV (PRESENT_END);

    if (hr == DXGI_ERROR_WAS_STILL_DRAWING)
        hr = S_OK;


    if ( pDev != nullptr || pDev12 != nullptr )
    {
      HRESULT ret = E_UNEXPECTED;

      auto _EndSwap = [&](void)
      {
        IUnknown *dev =
                 pDev.p != nullptr ? (IUnknown *)pDev.p
                                   : (IUnknown *)pDev12.p;

        ret =
          SK_EndBufferSwap (hr, dev);

        if (SUCCEEDED (hr))
        {
          if (     _IsBackendD3D12 (rb.api)) SK_D3D12_PostPresent (pDev12.p, This, hr);
          else if (_IsBackendD3D11 (rb.api)) SK_D3D11_PostPresent (pDev.p,   This, hr);

          if (config.render.framerate.swapchain_wait > 0 || rb.active_traits.bImplicitlyWaitable)
          {
            if ((intptr_t)rb.getSwapWaitHandle () > 0)
            {
              SK_AutoHandle               hWaitHandle (rb.getSwapWaitHandle ());
              if (SK_WaitForSingleObject (hWaitHandle.m_h, 0) == WAIT_TIMEOUT)
              {
                if (rb.active_traits.bImplicitlyWaitable || pLimiter->get_limit () > 0.0)
                {
                  // Wait on the SwapChain for up to a frame to try and
                  //   shrink the queue without a full-on stutter.
                  SK_WaitForSingleObject (
                    hWaitHandle.m_h, rb.active_traits.bImplicitlyWaitable ? INFINITE : (DWORD)pLimiter->get_ms_to_next_tick ()
                  );
                }
              }
            }
          }
        }
      };

      _EndSwap ();

      
      // All hooked chains need to be servicing >= this reset request, or restart them
  ////if (_IsBackendD3D11 (rb.api) && InterlockedCompareExchange (&lResetD3D11, 0, 1) == 1) _d3d11_rbk->release (This);
  ////if (_IsBackendD3D12 (rb.api) && InterlockedCompareExchange (&lResetD3D12, 0, 1) == 1) _d3d12_rbk->release (This);

      rb.setLatencyMarkerNV (SIMULATION_START);

      // Measure frametime after Present returns, and after any additional code SK runs after Present finishes
      if (config.fps.timing_method == SK_FrametimeMeasures_NewFrameBegin ||
         (config.fps.timing_method == SK_FrametimeMeasures_LimiterPacing && __target_fps <= 0.0f))
      {
        SK::Framerate::TickEx (false, 0.0, { 0,0 }, rb.swapchain.p);
      }

    // We have hooks in the D3D11/12 state tracker that should take care of this
    //rb.setLatencyMarkerNV (RENDERSUBMIT_START);

      return ret;
    }

    // Not a D3D11 device -- weird...
    return SK_EndBufferSwap (hr);
  }

  HRESULT hr =
    _Present (SyncInterval, Flags);

  config.render.osd._last_vidcap_frame =
    SK_GetFramesDrawn ();

  return hr;
}

HRESULT
STDMETHODCALLTYPE
SK_DXGI_DispatchPresent1 (IDXGISwapChain1         *This,
                          UINT                    SyncInterval,
                          UINT                    Flags,
           const DXGI_PRESENT_PARAMETERS         *pPresentParameters,
                          Present1SwapChain1_pfn  DXGISwapChain1_Present1,
                          SK_DXGI_PresentSource   Source)
{
  return
    SK_DXGI_PresentBase ( This, SyncInterval, Flags, Source,
                            nullptr, DXGISwapChain1_Present1,
                                                   pPresentParameters );
}

HRESULT
  STDMETHODCALLTYPE Present1Callback (IDXGISwapChain1         *This,
                                      UINT                     SyncInterval,
                                      UINT                     Flags,
                                const DXGI_PRESENT_PARAMETERS *pPresentParameters)
{
  // Almost never used by anything, so log it if it happens.
  SK_LOG_ONCE (L"Present1 ({Hooked SwapChain})");

  return
    SK_DXGI_DispatchPresent1 ( This, SyncInterval, Flags, pPresentParameters,
                               SK_DXGI_Present1, SK_DXGI_PresentSource::Hook );
}

HRESULT
STDMETHODCALLTYPE
SK_DXGI_DispatchPresent (IDXGISwapChain        *This,
                         UINT                   SyncInterval,
                         UINT                   Flags,
                         PresentSwapChain_pfn   DXGISwapChain_Present,
                         SK_DXGI_PresentSource  Source)
{
  if ( (Flags & DXGI_PRESENT_TEST           ) ||
       (Flags & DXGI_PRESENT_DO_NOT_SEQUENCE) )
  {
    return SK_DXGI_PresentBase ( This, SyncInterval,
                                   Flags, Source,
                                     DXGISwapChain_Present );
  }

  auto& rb =
    SK_GetCurrentRenderBackend ();

  // Backup and restore the RTV bindings Before / After Present for games that
  //   are using Flip Model but weren't originally designed to use it
  //
  //     ( Flip Model unbinds these things during Present (...) )
  //
  SK_ComPtr      <ID3D11DepthStencilView> pOrigDSV;
  SK_ComPtrArray <ID3D11RenderTargetView, D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT>
                                          pOrigRTVs;
  SK_ComPtr      <ID3D11Device>           pDevice;
  SK_ComPtr      <ID3D11DeviceContext>    pDevCtx;

  if (! rb.active_traits.bOriginallyFlip &&
          SUCCEEDED (This->GetDevice (IID_ID3D11Device, (void **)&pDevice.p)))
  {
    pDevice->GetImmediateContext (
       &pDevCtx.p );
    if (pDevCtx != nullptr)
    {
      pDevCtx->OMGetRenderTargets (D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT, &pOrigRTVs [0].p, &pOrigDSV.p);
    }
  }

  HRESULT ret =
    SK_DXGI_PresentBase ( This, SyncInterval,
                            Flags, Source,
                              DXGISwapChain_Present );

  if (pDevCtx != nullptr)
  {
    pDevCtx->OMSetRenderTargets (D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT, &pOrigRTVs [0].p, pOrigDSV.p);
  }

  return ret;
}


__declspec (noinline)
HRESULT
STDMETHODCALLTYPE PresentCallback ( IDXGISwapChain *This,
                                    UINT            SyncInterval,
                                    UINT            Flags )
{
  SK_GetCurrentRenderBackend ().in_present_call = true;

  return
    SK_DXGI_DispatchPresent (
      This, SyncInterval, Flags,
        SK_DXGI_Present, SK_DXGI_PresentSource::Hook
    );
}

struct {
  std::unique_ptr <std::recursive_mutex> mutex = nullptr;

  struct {
    IDXGIFactory*  pFactory    = nullptr;
    IDXGIFactory1* pFactory1   = nullptr;
    IDXGIFactory2* pFactory2   = nullptr;
    IDXGIFactory3* pFactory3   = nullptr;
    IDXGIFactory4* pFactory4   = nullptr;
    IDXGIFactory5* pFactory5   = nullptr;
    // This is wrong, but don't want to update headers for stuff we do not use
    IDXGIFactory5* pFactory6   = nullptr;
    IDXGIFactory5* pFactory7   = nullptr;

    std::pair <IDXGIAdapter1*,
               IDXGIFactory1*>
                   pAdapter1_0 =
                      { nullptr, nullptr };

    bool isCaching (void) noexcept {
      return pFactory  != nullptr || pFactory1 != nullptr ||
             pFactory2 != nullptr || pFactory3 != nullptr ||
             pFactory4 != nullptr || pFactory5 != nullptr ||
             pFactory6 != nullptr || pFactory7 != nullptr;
    }
  } cache;

  struct {
    // TODO
  } debug_cache;

  concurrency::concurrent_unordered_map < uint32_t,
                             std::vector < DXGI_MODE_DESC > >
                                              cached_descs;
  concurrency::concurrent_unordered_map < uint32_t, size_t >
                                               cache_hits;

  std::recursive_mutex& getMutex (void)
  {
    if (mutex == nullptr)
        mutex = std::make_unique <std::recursive_mutex> ();

    return
      *mutex.get ();
  }

  bool hasInterface (REFIID riid)
  {
    std::scoped_lock <std::recursive_mutex>
                lock             (getMutex ());

    if (riid ==       IID_IDXGIFactory )  return cache.pFactory  != nullptr;
    if (riid ==       IID_IDXGIFactory1)  return cache.pFactory1 != nullptr;
    if (riid ==       IID_IDXGIFactory2)  return cache.pFactory2 != nullptr;
    if (riid ==       IID_IDXGIFactory3)  return cache.pFactory3 != nullptr;
    if (riid ==       IID_IDXGIFactory4)  return cache.pFactory4 != nullptr;
    if (riid ==       IID_IDXGIFactory5)  return cache.pFactory5 != nullptr;
    if (riid == __uuidof (IDXGIFactory6)) return cache.pFactory6 != nullptr;
    if (riid == __uuidof (IDXGIFactory7)) return cache.pFactory7 != nullptr;

    return false;
  }

  bool isCurrent (void)
  {
    std::scoped_lock <std::recursive_mutex>
                lock             (getMutex ());

    bool current =
      cache.isCaching ();

    if (current)
    {
      if (     cache.pFactory1 != nullptr && (! cache.pFactory1->IsCurrent ()))
        current = false;
      else if (cache.pFactory2 != nullptr && (! cache.pFactory2->IsCurrent ()))
        current = false;
      else if (cache.pFactory3 != nullptr && (! cache.pFactory3->IsCurrent ()))
        current = false;
      else if (cache.pFactory4 != nullptr && (! cache.pFactory4->IsCurrent ()))
        current = false;
      else if (cache.pFactory5 != nullptr && (! cache.pFactory5->IsCurrent ()))
        current = false;
      else if (cache.pFactory6 != nullptr && (! cache.pFactory6->IsCurrent ()))
        current = false;
      else if (cache.pFactory7 != nullptr && (! cache.pFactory7->IsCurrent ()))
        current = false;
      else if (cache.pFactory  != nullptr)
      {
        SK_ComQIPtr <IDXGIFactory1>    pFactory1 (cache.pFactory);
        if (pFactory1 != nullptr && (! pFactory1->IsCurrent ()))
          current = false;
      }
    }

    return current;
  }

  HRESULT addRef (void** ppFactory, REFIID riid)
  {
    if (ppFactory == nullptr)
      return E_INVALIDARG;

    std::scoped_lock <std::recursive_mutex>
                lock             (getMutex ());

    IDXGIFactory* pFactory = nullptr;

    if (     riid == IID_IDXGIFactory && cache.pFactory != nullptr)
                              pFactory = cache.pFactory;
    else if (riid == IID_IDXGIFactory1 && cache.pFactory1 != nullptr)
                              pFactory =  cache.pFactory1;
    else if (riid == IID_IDXGIFactory2 && cache.pFactory2 != nullptr)
                              pFactory =  cache.pFactory2;
    else if (riid == IID_IDXGIFactory3 && cache.pFactory3 != nullptr)
                              pFactory =  cache.pFactory3;
    else if (riid == IID_IDXGIFactory4 && cache.pFactory4 != nullptr)
                              pFactory =  cache.pFactory4;
    else if (riid == IID_IDXGIFactory5 && cache.pFactory5 != nullptr)
                              pFactory =  cache.pFactory5;
    else if (riid ==
              __uuidof (IDXGIFactory6) && cache.pFactory6 != nullptr)
                              pFactory =  cache.pFactory6;
    else if (riid ==
              __uuidof (IDXGIFactory7) && cache.pFactory7 != nullptr)
                              pFactory =  cache.pFactory7;

    if (pFactory != nullptr)
        pFactory->AddRef (), *ppFactory = pFactory;

    return S_OK;
  }

  void addFactory (void** ppFactory, REFIID riid)
  {
    if (ppFactory == nullptr)
      return;

    std::scoped_lock <std::recursive_mutex>
                lock             (getMutex ());

    if (! isCurrent ())
              reset ();

    if (     riid == IID_IDXGIFactory)
    {
      if (cache.pFactory != nullptr)
        std::exchange (cache.pFactory, nullptr)->Release ();

      cache.pFactory = (IDXGIFactory *)*ppFactory;
      cache.pFactory->AddRef ();
    }

    else if (riid == IID_IDXGIFactory1)
    {
      if (cache.pFactory1 != nullptr)
        std::exchange (cache.pFactory1, nullptr)->Release ();

      cache.pFactory1 = (IDXGIFactory1 *)*ppFactory;
      cache.pFactory1->AddRef ();
    }

    else if (riid == IID_IDXGIFactory2)
    {
      if (cache.pFactory2 != nullptr)
        std::exchange (cache.pFactory2, nullptr)->Release ();

      cache.pFactory2 = (IDXGIFactory2 *)*ppFactory;
      cache.pFactory2->AddRef ();
    }

    else if (riid == IID_IDXGIFactory3)
    {
      if (cache.pFactory3 != nullptr)
        std::exchange (cache.pFactory3, nullptr)->Release ();

      cache.pFactory3 = (IDXGIFactory3 *)*ppFactory;
      cache.pFactory3->AddRef ();
    }

    else if (riid == IID_IDXGIFactory4)
    {
      if (cache.pFactory4 != nullptr)
        std::exchange (cache.pFactory4, nullptr)->Release ();

      cache.pFactory4 = (IDXGIFactory4 *)*ppFactory;
      cache.pFactory4->AddRef ();
    }

    else if (riid == IID_IDXGIFactory5)
    {
      if (cache.pFactory5 != nullptr)
        std::exchange (cache.pFactory5, nullptr)->Release ();

      cache.pFactory5 = (IDXGIFactory5 *)*ppFactory;
      cache.pFactory5->AddRef ();
    }

    else if (riid == __uuidof (IDXGIFactory6))
    {
      if (cache.pFactory6 != nullptr)
        std::exchange (cache.pFactory6, nullptr)->Release ();

      cache.pFactory6 = (IDXGIFactory5 *)*ppFactory;
      cache.pFactory6->AddRef ();
    }

    else if (riid == __uuidof (IDXGIFactory7))
    {
      if (cache.pFactory7 != nullptr)
        std::exchange (cache.pFactory7, nullptr)->Release ();

      cache.pFactory7 = (IDXGIFactory5 *)*ppFactory;
      cache.pFactory7->AddRef ();
    }
  }

  IDXGIAdapter1* getAdapter0 (IDXGIFactory1* pFactory1)
  {
    std::scoped_lock <std::recursive_mutex>
                lock              (getMutex ());

    if (  cache.pAdapter1_0.second == pFactory1 &&
          cache.pAdapter1_0.first  != nullptr )
    {     cache.pAdapter1_0.first->AddRef (); return
          cache.pAdapter1_0.first; }
    else {
      if (cache.pAdapter1_0.first != nullptr)std::exchange
         (cache.pAdapter1_0.first,   nullptr)->Release ();

          cache.pAdapter1_0.second = pFactory1;

      auto hr =
        EnumAdapters1_Original (pFactory1, 0,
         &cache.pAdapter1_0.first);

      if (SUCCEEDED (hr) &&
          cache.pAdapter1_0.first != nullptr)
      {   cache.pAdapter1_0.first->AddRef (); return
          cache.pAdapter1_0.first; }
     else cache.pAdapter1_0.second = nullptr;
    }
    return nullptr;
  }

  ULONG reset (void) {
    std::scoped_lock <std::recursive_mutex>
                lock             (getMutex ());

    cached_descs.clear ();
    cache_hits.clear   ();

    if (cache.pFactory          != nullptr) { std::exchange (cache.pFactory,          nullptr)->Release (); }
    if (cache.pFactory1         != nullptr) { std::exchange (cache.pFactory1,         nullptr)->Release (); }
    if (cache.pFactory2         != nullptr) { std::exchange (cache.pFactory2,         nullptr)->Release (); }
    if (cache.pFactory3         != nullptr) { std::exchange (cache.pFactory3,         nullptr)->Release (); }
    if (cache.pFactory4         != nullptr) { std::exchange (cache.pFactory4,         nullptr)->Release (); }
    if (cache.pFactory5         != nullptr) { std::exchange (cache.pFactory5,         nullptr)->Release (); }
    if (cache.pFactory6         != nullptr) { std::exchange (cache.pFactory6,         nullptr)->Release (); }
    if (cache.pFactory7         != nullptr) { std::exchange (cache.pFactory7,         nullptr)->Release (); }

    if (cache.pAdapter1_0.first != nullptr) { std::exchange (cache.pAdapter1_0.first, nullptr)->Release (); }

    return 0;
  }
} __SK_DXGI_FactoryCache;


auto
 _EnumFlagsToStr =
  [](UINT Flags)->
    std::string
  { std::string str;

    if (Flags == 0x0)
    {   str  = "0x0";
    } else
    { if ( 0x0 != ( Flags & DXGI_ENUM_MODES_INTERLACED ) )
        str +=                    "Interlaced";
      if ( 0x0 != ( Flags & DXGI_ENUM_MODES_SCALING    ) )
        str += ( str.empty () ?    "Stretched"
                              : " & Stretched" );
    } return     str;
  };


__declspec (noinline)
HRESULT
STDMETHODCALLTYPE
DXGIOutput_GetDisplayModeList_Override ( IDXGIOutput    *This,
                              /* [in] */ DXGI_FORMAT     EnumFormat,
                              /* [in] */ UINT            Flags,
                              /* [annotation][out][in] */
                                _Inout_  UINT           *pNumModes,
                              /* [annotation][out] */
_Out_writes_to_opt_(*pNumModes,*pNumModes)
                                         DXGI_MODE_DESC *pDesc)
{
  auto& rb =
    SK_GetCurrentRenderBackend ();

  // For sanity sake, clear the number of modes rather than leaving it undefined in log calls :)
  if (pDesc == nullptr && pNumModes != nullptr)
    *pNumModes = 0;
  else if (pDesc != nullptr)
          *pDesc = { };

  auto _LogCall = [&](void)
  { DXGI_LOG_CALL_I5 ( L"     IDXGIOutput", L"GetDisplayModeList       ",
                     L"%08" _L(PRIxPTR) L"h, %hs, %hs, %u, %08"
                              _L(PRIxPTR) L"h",
            (uintptr_t)This,
                       SK_DXGI_FormatToStr (EnumFormat).data  (),
                           _EnumFlagsToStr (     Flags).c_str (),
                             pNumModes != nullptr ?
                            *pNumModes            : -1,
                                (uintptr_t)pDesc ); };

  uint32_t tag = 0;

  if (config.render.dxgi.use_factory_cache && pNumModes != nullptr)
  {
    struct callframe_s {
      DXGI_OUTPUT_DESC   desc;
      DXGI_FORMAT      format;
      UINT              flags;
    };

    callframe_s
        frame =
           { { }, EnumFormat,
                  Flags };

    This->GetDesc (&frame.desc);

    tag =
      crc32c (0x0, &frame, sizeof (callframe_s));

    if (__SK_DXGI_FactoryCache.cached_descs.count (tag) != 0)
    {
      auto& cache =
        __SK_DXGI_FactoryCache.cached_descs [tag];

      size_t cache_size =
             cache.size ();

      if (*pNumModes != 0)
          *pNumModes = sk::narrow_cast <UINT> (std::min (cache_size, static_cast <size_t> (*pNumModes)));
      else
          *pNumModes = sk::narrow_cast <UINT> (          cache_size);

      if (pDesc != nullptr)
      {
        for (size_t i = 0; i < *pNumModes; i++)
        {
          pDesc [i] = cache [i];
        }
      }

      size_t& hits =
        __SK_DXGI_FactoryCache.cache_hits [tag];

      // First cache hit needs to be logged
      if (! hits++)
        _LogCall ();

      SK_LOG1 ( ( L" IDXGIOutput::GetDisplayModeList (...): Returning Cached Data" ),
                  L"   DXGI   " );

      return S_OK;
    }
  }

  _LogCall ();

  if (pNumModes == nullptr)
    return DXGI_ERROR_INVALID_CALL;

  if (     config.render.dxgi.scaling_mode ==  0)
    Flags &= ~DXGI_ENUM_MODES_SCALING;
  else if (config.render.dxgi.scaling_mode != SK_NoPreference)
    Flags |=  DXGI_ENUM_MODES_SCALING;

  if (     config.render.dxgi.scanline_order ==
                DXGI_MODE_SCANLINE_ORDER_PROGRESSIVE )
    Flags &= ~DXGI_ENUM_MODES_INTERLACED;
  else if (config.render.dxgi.scanline_order != SK_NoPreference)
    Flags |=  DXGI_ENUM_MODES_INTERLACED;

  // Game would be missing resolutions
  if (rb.active_traits.bMisusingDXGIScaling)
    Flags &= ~DXGI_ENUM_MODES_SCALING;

  HRESULT hr =
    GetDisplayModeList_Original (
      This,
        EnumFormat,
          Flags,
            pNumModes,
              pDesc );

  if (SUCCEEDED (hr))
  {
    if (pDesc == nullptr && *pNumModes != 0)
    {
      DXGI_MODE_DESC*
        pDescLocal = nullptr;

      SK_TLS *pTLS =
        SK_TLS_Bottom ();

      if (pTLS != nullptr)
      {
        pDescLocal =
          pTLS->scratch_memory->dxgi.mode_list.alloc (*pNumModes);

        if (pDescLocal != nullptr)
        {
          hr =
            GetDisplayModeList_Original ( This,
              EnumFormat,
                Flags,
                  pNumModes,
                    pDescLocal );

          if (SUCCEEDED (hr))
            pDesc = pDescLocal;
        }
      }
    }

    int removed_count = 0;

    if (pDesc != nullptr)
    {
      if ( ( ! ( config.render.dxgi.res.min.isZero ()     &&
                 config.render.dxgi.res.max.isZero () ) ) &&
                    *pNumModes != 0 )
      {
        int last  = *pNumModes;
        int first = 0;

        std::set <int> min_cover;
        std::set <int> max_cover;

        if (! config.render.dxgi.res.min.isZero ())
        {
          // Restrict MIN (Sequential Scan: Last->First)
          for ( int i = *pNumModes - 1 ; i >= 0 ; --i )
          {
            if (SK_DXGI_RestrictResMin (WIDTH,  first, i, min_cover, pDesc) ||
                SK_DXGI_RestrictResMin (HEIGHT, first, i, min_cover, pDesc))
            {
              ++removed_count;
            }
          }
        }

        if (! config.render.dxgi.res.max.isZero ())
        {
          // Restrict MAX (Sequential Scan: $->Last)
          for ( UINT i = 0 ; i < *pNumModes ; ++i )
          {
            if (min_cover.count (i) != 0)
                break;

            if (SK_DXGI_RestrictResMax (WIDTH,  last, i, max_cover, pDesc) ||
                SK_DXGI_RestrictResMax (HEIGHT, last, i, max_cover, pDesc))
            {
              ++removed_count;
            }
          }
        }
      }

      if (config.render.dxgi.scaling_mode != SK_NoPreference)
      {
        if ( config.render.dxgi.scaling_mode != DXGI_MODE_SCALING_UNSPECIFIED &&
             config.render.dxgi.scaling_mode != DXGI_MODE_SCALING_CENTERED )
        {
          for ( INT i  = sk::narrow_cast <INT> (*pNumModes) - 1 ;
                    i >= 0                                       ;
                  --i )
          {
            if ( pDesc [i].Scaling != DXGI_MODE_SCALING_UNSPECIFIED &&
                 pDesc [i].Scaling != DXGI_MODE_SCALING_CENTERED    &&
                 pDesc [i].Scaling !=
     static_cast <DXGI_MODE_SCALING> (config.render.dxgi.scaling_mode) )
            {
              pDesc    [i] =
                pDesc  [i + 1];

              ++removed_count;
            }
          }
        }
      }

#ifdef _DEBUG
      if (removed_count > 0)
      {        UINT idx = 0;
        for ( auto pIt = pDesc ; pIt < pDesc + *pNumModes; ++pIt, ++idx )
        {
          dll_log->Log ( L"[   DXGI   ]  Mode%03u: %lux%lu [%hs] @ %4.1f Hz",
                                              idx,  pIt->Width,
                                                    pIt->Height,
                               SK_DXGI_FormatToStr (pIt->Format).c_str (),
                               static_cast <float> (pIt->RefreshRate.Numerator)
                             / static_cast <float> (pIt->RefreshRate.Denominator) );
        }
      }
#endif

      dll_log->Log ( L"[   DXGI   ]      >> %lu modes (%li removed)",
                       *pNumModes,
                         removed_count );

      if (tag != 0x0)
      {
        auto& cache =
          __SK_DXGI_FactoryCache.cached_descs [tag];

        std::unordered_set <uint32_t>       reduced_set;
        std::vector        <DXGI_MODE_DESC> reduced_list;

        for (UINT i = 0; i < *pNumModes; i++)
        {
          if ( reduced_set.emplace
                 ( crc32c (0x0, &pDesc [i], sizeof (DXGI_MODE_DESC)) ).second
             )
          {
            reduced_list.emplace_back (
              pDesc [i]
            );
          }
        }

        cache.resize (0);

        //
        // Apply refresh rate filtering on the now smaller list
        //

        float fMinRefresh = config.render.dxgi.refresh.min;
        float fMaxRefresh = config.render.dxgi.refresh.max;

        if (fMaxRefresh < 1.0f)
            fMaxRefresh = 1000.0f;

        for ( const auto& it : reduced_list )
        {
          double fRefresh =
            static_cast <float> (
              static_cast <double> (it.RefreshRate.Numerator) /
              static_cast <double> (it.RefreshRate.Denominator) );

          if ( fRefresh >= fMinRefresh &&
               fRefresh <= fMaxRefresh )
          {
            cache.push_back (it);
          }
        }

        // If refresh rate filtering would have eliminated all resolutions, then
        //   ignore the limits and give the entire list or the game will panic :)
        if (cache.empty ())
          for ( const auto&  it : reduced_list )
            cache.push_back (it);

        *pNumModes =
          sk::narrow_cast <UINT> (cache.size ());

        dll_log->Log ( L"[   DXGI   ]      (!) Final Mode Cache: %lu entries",
                         *pNumModes );
      }
    }
  }

  return hr;
}

HRESULT
STDMETHODCALLTYPE
SK_DXGI_FindClosestMode ( IDXGISwapChain *pSwapChain,
                   _In_   DXGI_MODE_DESC *pModeToMatch,
                   _Out_  DXGI_MODE_DESC *pClosestMatch,
                _In_opt_  IUnknown       *pConcernedDevice,
                          BOOL            bApplyOverrides )
{
  if (pSwapChain == nullptr || pModeToMatch == nullptr || pClosestMatch == nullptr)
    return DXGI_ERROR_INVALID_CALL;

  if (! FindClosestMatchingMode_Original)
  {
    SK_LOGi0 ( L"SK_DXGI_FindClosestMode (...) called without a hooked IDXGIOutput" );

    return E_NOINTERFACE;
  }

  // Do not allow DXGI_FORMAT_UNKNOWN
  if (pModeToMatch->Format == DXGI_FORMAT_UNKNOWN)
  {
    DXGI_SWAP_CHAIN_DESC  swapDesc = { };
    pSwapChain->GetDesc (&swapDesc);

    SK_LOGi0 (
      L"Replacing DXGI_FORMAT_UNKNOWN with %hs",
        SK_DXGI_FormatToStr (swapDesc.BufferDesc.Format).data ()
    );

    pModeToMatch->Format = swapDesc.BufferDesc.Format;
  }

  SK_ComPtr <IDXGIDevice> pSwapDevice;
  SK_ComPtr <IDXGIOutput> pSwapOutput;

  if ( SUCCEEDED (
         pSwapChain->GetContainingOutput (&pSwapOutput.p)
                 )
           &&
       SUCCEEDED (
         pSwapChain->GetDevice (
           __uuidof (IDXGIDevice), (void**)& pSwapDevice.p)
                 )
     )
  {
    DXGI_MODE_DESC mode_to_match = *pModeToMatch;

    if (bApplyOverrides)
    {
      if (config.render.framerate.rescan_.Denom != 1)
      {
              mode_to_match.RefreshRate.Numerator   =
        config.render.framerate.rescan_.Numerator;
              mode_to_match.RefreshRate.Denominator =
        config.render.framerate.rescan_.Denom;
      }

      else if (config.render.framerate.refresh_rate != -1.0f &&
                mode_to_match.RefreshRate.Numerator !=
         sk::narrow_cast <UINT> (config.render.framerate.refresh_rate))
      {
        mode_to_match.RefreshRate.Numerator   =
          sk::narrow_cast <UINT> (config.render.framerate.refresh_rate);
        mode_to_match.RefreshRate.Denominator = 1;
      }

      if ( config.render.dxgi.scaling_mode != SK_NoPreference            &&
                mode_to_match.Scaling != config.render.dxgi.scaling_mode/*&&
                      (DXGI_MODE_SCALING)config.render.dxgi.scaling_mode !=
                       DXGI_MODE_SCALING_CENTERED*/ )
      {
        mode_to_match.Scaling =
          (DXGI_MODE_SCALING)config.render.dxgi.scaling_mode;
      }
    }

    pModeToMatch = &mode_to_match;

    HRESULT hr =
      FindClosestMatchingMode_Original (
        pSwapOutput, pModeToMatch,
                    pClosestMatch, pConcernedDevice
      );

    if (FAILED (hr))
    {
      ZeroMemory (pClosestMatch, sizeof (DXGI_MODE_DESC));

      // Prevent code that does not check error messages from
      //   potentially dividing by zero.
      pClosestMatch->RefreshRate.Denominator = 1;
      pClosestMatch->RefreshRate.Numerator   = 1;
    }

    return
      hr;
  }

  return DXGI_ERROR_NOT_FOUND;
}


//[[deprecated]]
HRESULT
STDMETHODCALLTYPE
SK_DXGI_ResizeTarget ( IDXGISwapChain *This,
                  _In_ DXGI_MODE_DESC *pNewTargetParameters,
                       BOOL            bApplyOverrides )
{
  assert (pNewTargetParameters != nullptr);

  if (pNewTargetParameters == nullptr)
    return DXGI_ERROR_INVALID_CALL;

  SK_RenderBackend& rb =
    SK_GetCurrentRenderBackend ();

  if (rb.scanout.colorspace_override != DXGI_COLOR_SPACE_CUSTOM)
  {
    SK_ComQIPtr <IDXGISwapChain3>
                          pSwap3 (This);
    if (       nullptr != pSwap3) SK_DXGI_UpdateColorSpace
                         (pSwap3);
  }

  HRESULT ret =
    E_UNEXPECTED;

  DXGI_MODE_DESC new_new_params =
    *pNewTargetParameters;

  DXGI_MODE_DESC* pNewNewTargetParameters =
    &new_new_params;

  if (bApplyOverrides)
  {
    const bool fake_fullscreen =
      SK_GetCurrentRenderBackend ().isFakeFullscreen ();

    bool borderless = config.window.borderless || fake_fullscreen;

    if ( borderless || new_new_params.Format == DXGI_FORMAT_UNKNOWN ||
         ( config.render.dxgi.scaling_mode != SK_NoPreference &&
            pNewTargetParameters->Scaling  !=
              (DXGI_MODE_SCALING)config.render.dxgi.scaling_mode )
                                  ||
         ( config.render.framerate.refresh_rate          != -1.0f &&
             pNewTargetParameters->RefreshRate.Numerator !=
               sk::narrow_cast <UINT> (
           config.render.framerate.refresh_rate )
         )
      )
    {
      // Do not allow DXGI_FORMAT_UNKNOWN
      if (new_new_params.Format == DXGI_FORMAT_UNKNOWN)
      {
        DXGI_SWAP_CHAIN_DESC swapDesc = { };
        This->GetDesc      (&swapDesc);

        SK_LOGi0 (
          L"Replacing DXGI_FORMAT_UNKNOWN with %hs",
            SK_DXGI_FormatToStr (swapDesc.BufferDesc.Format).data ()
        );

        new_new_params.Format = swapDesc.BufferDesc.Format;
      }


      if ( config.render.framerate.rescan_.Denom          !=  1    ||
           (config.render.framerate.refresh_rate          != -1.0f &&
                     new_new_params.RefreshRate.Numerator != sk::narrow_cast <UINT>
           (config.render.framerate.refresh_rate) )
         )
      {
        DXGI_MODE_DESC modeDesc  = { };
        DXGI_MODE_DESC modeMatch = { };

        modeDesc.Format           = new_new_params.Format;
        modeDesc.Width            = new_new_params.Width;
        modeDesc.Height           = new_new_params.Height;
        modeDesc.Scaling          = DXGI_MODE_SCALING_UNSPECIFIED;
        modeDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;

        if (config.render.framerate.rescan_.Denom != 1)
        {
                     modeDesc.RefreshRate.Numerator   =
          config.render.framerate.rescan_.Numerator;
                     modeDesc.RefreshRate.Denominator =
          config.render.framerate.rescan_.Denom;
        }

        else
        {
          modeDesc.RefreshRate.Numerator   =
            sk::narrow_cast <UINT> (config.render.framerate.refresh_rate);
          modeDesc.RefreshRate.Denominator = 1;
        }

        if ( SUCCEEDED (
               SK_DXGI_FindClosestMode ( This, &modeDesc,
                                               &modeMatch, FALSE )
                       )
           )
        {
          new_new_params.RefreshRate.Numerator   =
               modeMatch.RefreshRate.Numerator;
          new_new_params.RefreshRate.Denominator =
               modeMatch.RefreshRate.Denominator;
        }
      }

      if ( config.render.dxgi.scanline_order        != SK_NoPreference &&
            pNewTargetParameters->ScanlineOrdering  !=
              (DXGI_MODE_SCANLINE_ORDER)config.render.dxgi.scanline_order )
      {
        new_new_params.ScanlineOrdering =
          (DXGI_MODE_SCANLINE_ORDER)config.render.dxgi.scanline_order;
      }

      if ( config.render.dxgi.scaling_mode != SK_NoPreference &&
            pNewTargetParameters->Scaling  !=
              (DXGI_MODE_SCALING)config.render.dxgi.scaling_mode )
      {
        new_new_params.Scaling =
          (DXGI_MODE_SCALING)config.render.dxgi.scaling_mode;
      }

      if (! config.window.res.override.isZero ())
      {
        new_new_params.Width  = config.window.res.override.x;
        new_new_params.Height = config.window.res.override.y;
      }

      // Clamp the buffer dimensions if the user has a min/max preference
      const UINT
        max_x = config.render.dxgi.res.max.x < new_new_params.Width  ?
                config.render.dxgi.res.max.x : new_new_params.Width,
        min_x = config.render.dxgi.res.min.x > new_new_params.Width  ?
                config.render.dxgi.res.min.x : new_new_params.Width,
        max_y = config.render.dxgi.res.max.y < new_new_params.Height ?
                config.render.dxgi.res.max.y : new_new_params.Height,
        min_y = config.render.dxgi.res.min.y > new_new_params.Height ?
                config.render.dxgi.res.min.y : new_new_params.Height;

      new_new_params.Width   =  std::max ( max_x , min_x );
      new_new_params.Height  =  std::max ( max_y , min_y );

      pNewNewTargetParameters->Format =
        SK_DXGI_PickHDRFormat (pNewNewTargetParameters->Format, FALSE, TRUE);
    }

    ret =
      This->ResizeTarget (pNewNewTargetParameters);

    if (FAILED (ret))
    {
      if (_IsBackendD3D12 (rb.api))
        _d3d12_rbk->release (This);
      else
        _d3d11_rbk->release (This);
    }

    else
    {
      rb.swapchain = This;

      if ( pNewNewTargetParameters->Width  != 0 &&
           pNewNewTargetParameters->Height != 0 )
      {
        SK_SetWindowResX (pNewNewTargetParameters->Width);
        SK_SetWindowResY (pNewNewTargetParameters->Height);
      }

      else
      {
        DXGI_SWAP_CHAIN_DESC swapDesc = {};
        This->GetDesc      (&swapDesc);

        SK_ReleaseAssert (swapDesc.OutputWindow == game_window.hWnd ||
                                              0 == game_window.hWnd);

        RECT client = { };

        GetClientRect (swapDesc.OutputWindow, &client);

        SK_SetWindowResX (client.right  - client.left);
        SK_SetWindowResY (client.bottom - client.top);
      }
    }
  }

  return ret;
}



__declspec (noinline)
HRESULT
STDMETHODCALLTYPE
DXGIOutput_FindClosestMatchingMode_Override (
                IDXGIOutput    *This,
 _In_     const DXGI_MODE_DESC *pModeToMatch,
 _Out_          DXGI_MODE_DESC *pClosestMatch,
 _In_opt_       IUnknown       *pConcernedDevice )
{
  auto& rb =
    SK_GetCurrentRenderBackend ();

  DXGI_LOG_CALL_I4 (
    L"       IDXGIOutput", L"FindClosestMatchingMode         ",
    L"%p, %08" _L(PRIxPTR) L"h, %08" _L(PRIxPTR) L"h, %08"
                                     _L(PRIxPTR) L"h",
      This, (uintptr_t)pModeToMatch, (uintptr_t)pClosestMatch,
            (uintptr_t)pConcernedDevice
  );

  SK_LOGi0 (
    L"[?]  Desired Mode:  %lux%lu@%.2f Hz, Format=%hs, Scaling=%hs, "
                                         L"Scanlines=%hs",
      pModeToMatch->Width, pModeToMatch->Height,
        pModeToMatch->RefreshRate.Denominator != 0 ?
          static_cast <float> (pModeToMatch->RefreshRate.Numerator) /
          static_cast <float> (pModeToMatch->RefreshRate.Denominator) :
            std::numeric_limits <float>::quiet_NaN (),
      SK_DXGI_FormatToStr           (pModeToMatch->Format).data (),
      SK_DXGI_DescribeScalingMode   (pModeToMatch->Scaling),
      SK_DXGI_DescribeScanlineOrder (pModeToMatch->ScanlineOrdering)
  );


  DXGI_MODE_DESC mode_to_match = *pModeToMatch;

  if (  config.render.framerate.rescan_.Denom !=  1 ||
       (config.render.framerate.refresh_rate   > 0.0f &&
         mode_to_match.RefreshRate.Numerator  != sk::narrow_cast <UINT>
       (config.render.framerate.refresh_rate)) )
  {
    if ( ( config.render.framerate.rescan_.Denom     > 0   &&
           config.render.framerate.rescan_.Numerator > 0 ) &&
         !( mode_to_match.RefreshRate.Numerator   == config.render.framerate.rescan_.Numerator &&
            mode_to_match.RefreshRate.Denominator == config.render.framerate.rescan_.Denom ) )
    {
      SK_LOGi0 ( L" >> Refresh Override (Requested: %f, Using: %f)",
          mode_to_match.RefreshRate.Denominator != 0 ?
            static_cast <float> (mode_to_match.RefreshRate.Numerator) /
            static_cast <float> (mode_to_match.RefreshRate.Denominator) :
              std::numeric_limits <float>::quiet_NaN (),
            static_cast <float> (config.render.framerate.rescan_.Numerator) /
            static_cast <float> (config.render.framerate.rescan_.Denom)
      );

      if (config.render.framerate.rescan_.Denom != 1)
      {
              mode_to_match.RefreshRate.Numerator     =
        config.render.framerate.rescan_.Numerator;
                mode_to_match.RefreshRate.Denominator =
          config.render.framerate.rescan_.Denom;
      }

      else
      {
        mode_to_match.RefreshRate.Numerator = sk::narrow_cast <UINT> (
          config.render.framerate.refresh_rate);
        mode_to_match.RefreshRate.Denominator = 1;
      }
    }
  }

  if ( config.render.dxgi.scaling_mode != SK_NoPreference &&
       mode_to_match.Scaling           != config.render.dxgi.scaling_mode /*&&
                       (DXGI_MODE_SCALING)config.render.dxgi.scaling_mode !=
                        DXGI_MODE_SCALING_CENTERED*/ )
             // nb: What purpose did removing centered scaling override serve?
  {
    dll_log->Log ( L"[   DXGI   ]  >> Scaling Override "
                   L"(Requested: %hs, Using: %hs)",
                     SK_DXGI_DescribeScalingMode (
                       mode_to_match.Scaling
                     ),
                       SK_DXGI_DescribeScalingMode (
                         (DXGI_MODE_SCALING)config.render.dxgi.scaling_mode
                       )
                 );

    mode_to_match.Scaling =
      (DXGI_MODE_SCALING)config.render.dxgi.scaling_mode;
  }

  if ( rb.active_traits.bMisusingDXGIScaling && mode_to_match.Scaling != DXGI_MODE_SCALING_UNSPECIFIED )
  {
    dll_log->Log ( L"[   DXGI   ]  >> Scaling Override "
                   L"(Requested: %hs, Using: %hs) "
                   L" [ Because Developer Misunderstands This API ]", // Game's Developer
                     SK_DXGI_DescribeScalingMode (
                       mode_to_match.Scaling
                     ),
                       SK_DXGI_DescribeScalingMode (
                         DXGI_MODE_SCALING_UNSPECIFIED
                       )
                 );

    mode_to_match.Scaling =
      DXGI_MODE_SCALING_UNSPECIFIED;
  }

  pModeToMatch = &mode_to_match;


  HRESULT     ret = E_UNEXPECTED;
  DXGI_CALL ( ret, FindClosestMatchingMode_Original (
                     This, pModeToMatch, pClosestMatch,
                                         pConcernedDevice ) );

  if (SUCCEEDED (ret))
  {
    SK_LOGi0 (
      L"[#]  Closest Match: %lux%lu@%.2f Hz, Format=%hs, Scaling=%hs, "
                                           L"Scanlines=%hs",
      pClosestMatch->Width, pClosestMatch->Height,
        pClosestMatch->RefreshRate.Denominator != 0 ?
          static_cast <float> (pClosestMatch->RefreshRate.Numerator) /
          static_cast <float> (pClosestMatch->RefreshRate.Denominator) :
            std::numeric_limits <float>::quiet_NaN (),
      SK_DXGI_FormatToStr           (pClosestMatch->Format).data (),
      SK_DXGI_DescribeScalingMode   (pClosestMatch->Scaling),
      SK_DXGI_DescribeScanlineOrder (pClosestMatch->ScanlineOrdering)
    );
  }

  return ret;
}

__declspec (noinline)
HRESULT
STDMETHODCALLTYPE
DXGIOutput_WaitForVBlank_Override ( IDXGIOutput *This ) noexcept
{
  SK_LOG_FIRST_CALL

  return
    WaitForVBlank_Original (This);
}

__declspec (noinline)
HRESULT
STDMETHODCALLTYPE
DXGISwap_GetFullscreenState_Override ( IDXGISwapChain  *This,
                            _Out_opt_  BOOL            *pFullscreen,
                            _Out_opt_  IDXGIOutput    **ppTarget )
{
  return
    GetFullscreenState_Original (This, pFullscreen, ppTarget);
}

__declspec (noinline)
HRESULT
STDMETHODCALLTYPE
DXGISwap_SetFullscreenState_Override ( IDXGISwapChain *This,
                                       BOOL            Fullscreen,
                              _In_opt_ IDXGIOutput    *pTarget )
{
  DXGI_LOG_CALL_I2 ( L"    IDXGISwapChain", L"SetFullscreenState         ",
                   L"%s, %08" _L(PRIxPTR) L"h",
                    Fullscreen ? L"{ Fullscreen }" :
                                 L"{ Windowed }",   (uintptr_t)pTarget );

  return
    SK_DXGI_SwapChain_SetFullscreenState_Impl (
      This, Fullscreen, pTarget, FALSE
    );
}

__declspec (noinline)
HRESULT
STDMETHODCALLTYPE
DXGISwap3_ResizeBuffers1_Override (IDXGISwapChain3* This,
  _In_           UINT             BufferCount,
  _In_           UINT             Width,
  _In_           UINT             Height,
  _In_           DXGI_FORMAT      NewFormat,
  _In_           UINT             SwapChainFlags,
  _In_opt_ const UINT            *pCreationNodeMask,
  _In_           IUnknown* const *ppPresentQueue)
{
  DXGI_SWAP_CHAIN_DESC swap_desc = { };
  This->GetDesc      (&swap_desc);

  DXGI_LOG_CALL_I5 ( L"   IDXGISwapChain3", L"ResizeBuffers1        ",
                     L"%u,%u,%u,%hs,0x%08X",
                     BufferCount,
                       Width, Height,
    SK_DXGI_FormatToStr (NewFormat).data (),
                           SwapChainFlags );

  if (BufferCount != 0 && ppPresentQueue == nullptr)
    return DXGI_ERROR_INVALID_CALL;

  SwapChainFlags =
    SK_DXGI_FixUpLatencyWaitFlag (This, SwapChainFlags);
  NewFormat      =
    SK_DXGI_PickHDRFormat ( NewFormat, swap_desc.Windowed,
        SK_DXGI_IsFlipModelSwapEffect (swap_desc.SwapEffect) );


  //
  // Do not apply backbuffer count overrides in D3D12 unless user presents
  //   a valid footgun license and can afford to lose a few toes.
  //
  if (                            SK_ComPtr <ID3D12Device> pSwapDev12;
      FAILED (This->GetDevice (IID_ID3D12Device, (void **)&pSwapDev12.p)) ||
               config.render.dxgi.allow_d3d12_footguns
     )
  {
    if (       config.render.framerate.buffer_count != SK_NoPreference &&
         (UINT)config.render.framerate.buffer_count !=  BufferCount    &&
         BufferCount                                !=  0              &&

             config.render.framerate.buffer_count   >   0              &&
             config.render.framerate.buffer_count   <   16 )
    {
      BufferCount =
        config.render.framerate.buffer_count;

      dll_log->Log ( L"[   DXGI   ]  >> Buffer Count Override: %lu buffers",
                                        BufferCount );
    }
  }

  // Fix-up BufferCount and Flags in Flip Model
  if (SK_DXGI_IsFlipModelSwapEffect (swap_desc.SwapEffect))
  {
    if (! dxgi_caps.swapchain.allow_tearing)
    {
      SwapChainFlags &= ~DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;
    }

    else
    {
      SwapChainFlags |= DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;
      dll_log->Log ( L"[ DXGI 1.5 ]  >> Tearing Option:  Enable" );
    }

    if (BufferCount != 0 || swap_desc.BufferCount < 2)
    {
      BufferCount =
        std::min (16ui32,
         std::max (2ui32, BufferCount)
                 );
    }
  }

  if (! config.window.res.override.isZero ())
  {
    Width  = config.window.res.override.x;
    Height = config.window.res.override.y;
  }

  // Clamp the buffer dimensions if the user has a min/max preference
  const UINT
    max_x = config.render.dxgi.res.max.x < Width  ?
            config.render.dxgi.res.max.x : Width,
    min_x = config.render.dxgi.res.min.x > Width  ?
            config.render.dxgi.res.min.x : Width,
    max_y = config.render.dxgi.res.max.y < Height ?
            config.render.dxgi.res.max.y : Height,
    min_y = config.render.dxgi.res.min.y > Height ?
            config.render.dxgi.res.min.y : Height;

  Width   =  std::max ( max_x , min_x );
  Height  =  std::max ( max_y , min_y );

  SK_RenderBackend& rb =
    SK_GetCurrentRenderBackend ();

  // If forcing flip-model, we can't allow sRGB formats...
  if ( config.render.framerate.flip_discard &&
       SK_DXGI_IsFlipModelSwapChain (swap_desc) )
  {
    rb.active_traits.bFlipMode =
      dxgi_caps.present.flip_sequential;

    // Format overrides must be performed in some cases (sRGB)
    switch (NewFormat)
    {
      case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
        NewFormat = DXGI_FORMAT_B8G8R8A8_UNORM;
        dll_log->Log ( L"[ DXGI 1.2 ]  >> sRGB (B8G8R8A8) Override "
                       L"Required to Enable Flip Model" );
        rb.srgb_stripped = true;
        break;
      case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
        NewFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
        dll_log->Log ( L"[ DXGI 1.2 ]  >> sRGB (R8G8B8A8) Override "
                       L"Required to Enable Flip Model" );
        rb.srgb_stripped = true;
        break;
    }
  }

  if (_IsBackendD3D12 (rb.api))
    _d3d12_rbk->release (This);
  else
    _d3d11_rbk->release (This);

  HRESULT     ret =
    ResizeBuffers1_Original ( This, BufferCount, Width, Height,
                                NewFormat, SwapChainFlags,
                                  pCreationNodeMask, ppPresentQueue );

  DXGI_CALL (ret, ret);

  if (SUCCEEDED (ret))
  {
    if (Width != 0 && Height != 0)
    {
      SK_SetWindowResX (Width);
      SK_SetWindowResY (Height);
    }
  }

  return ret;
}

#include <dxgidebug.h>

HRESULT
SK_DXGI_GetDebugInterface (REFIID riid, void **ppDebug)
{
  if (ppDebug == nullptr)
    return ERROR_INVALID_PARAMETER;

  static HMODULE hModDXGIDebug =
    LoadLibraryExW ( L"dxgidebug.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32 );

  using   DXGIGetDebugInterface_pfn = HRESULT (WINAPI*)(REFIID riid, void **ppDebug);
  static  DXGIGetDebugInterface_pfn
         _DXGIGetDebugInterface =
         (DXGIGetDebugInterface_pfn) SK_GetProcAddress ( hModDXGIDebug,
         "DXGIGetDebugInterface"                       );

  return _DXGIGetDebugInterface != nullptr      ?
         _DXGIGetDebugInterface (riid, ppDebug) : E_NOINTERFACE;
}

HRESULT
SK_DXGI_OutputDebugString (const std::string& str, DXGI_INFO_QUEUE_MESSAGE_SEVERITY severity)
{
  if ((! SK_IsDebuggerPresent ()) && (! config.render.dxgi.debug_layer))
    return S_OK;

  try
  {
    SK_ComPtr <IDXGIInfoQueue>                               pDXGIInfoQueue;
    ThrowIfFailed (SK_DXGI_GetDebugInterface (IID_PPV_ARGS (&pDXGIInfoQueue.p)));

    return
      pDXGIInfoQueue->AddApplicationMessage (severity, str.c_str ());
  }

  catch (const SK_ComException&)
  {
    return E_NOTIMPL;
  }
};

HRESULT
SK_DXGI_ReportLiveObjects (IUnknown *pDev)
{
  if ((! SK_IsDebuggerPresent ()) && (! config.render.dxgi.debug_layer))
    return S_OK;

  try
  {
    if (pDev != nullptr)
    {
      SK_ComQIPtr <ID3D12DebugDevice> pDebugDevice12 (pDev);
      if (                            pDebugDevice12 != nullptr)
      return                          pDebugDevice12->ReportLiveDeviceObjects
                 (D3D12_RLDO_DETAIL | D3D12_RLDO_SUMMARY);
    }

    SK_ComPtr <IDXGIDebug>                                   pDXGIDebug;
    ThrowIfFailed (SK_DXGI_GetDebugInterface (IID_PPV_ARGS (&pDXGIDebug.p)));

    return
      pDXGIDebug->ReportLiveObjects  (DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_ALL);
  }

  catch (const SK_ComException&)
  {
    return E_NOTIMPL;
  };
}

__declspec (noinline)
HRESULT
STDMETHODCALLTYPE
DXGISwap_ResizeBuffers_Override (IDXGISwapChain* This,
  _In_ UINT            BufferCount,
  _In_ UINT            Width,
  _In_ UINT            Height,
  _In_ DXGI_FORMAT     NewFormat,
  _In_ UINT            SwapChainFlags)
{
  DXGI_LOG_CALL_I5 ( L"    IDXGISwapChain", L"ResizeBuffers         ",
                   L"%u,%u,%u,%hs,0x%08X",
                   BufferCount,
                     Width, Height,
  SK_DXGI_FormatToStr (NewFormat).data (),
                         SwapChainFlags );

  return
    SK_DXGI_SwapChain_ResizeBuffers_Impl (
      This, BufferCount, Width, Height, NewFormat, SwapChainFlags, FALSE
    );
}

__declspec (noinline)
HRESULT
STDMETHODCALLTYPE
DXGISwap_ResizeTarget_Override ( IDXGISwapChain *This,
                      _In_ const DXGI_MODE_DESC *pNewTargetParameters )
{
  DXGI_LOG_CALL_I6 (
    L"    IDXGISwapChain", L"ResizeTarget         ",
      L"{ (%ux%u@%3.1f Hz),"
      L"fmt=%hs,scaling=0x%02x,scanlines=0x%02x }",
                     pNewTargetParameters->Width,
                     pNewTargetParameters->Height,
                     pNewTargetParameters->RefreshRate.Denominator != 0 ?
      static_cast <float> (pNewTargetParameters->RefreshRate.Numerator) /
      static_cast <float> (pNewTargetParameters->RefreshRate.Denominator) :
                         std::numeric_limits <float>::quiet_NaN (),
      SK_DXGI_FormatToStr (pNewTargetParameters->Format).data   (),
                           pNewTargetParameters->Scaling,
                           pNewTargetParameters->ScanlineOrdering
  );

  // Do not allow DXGI_FORMAT_UNKNOWN
  if (pNewTargetParameters->Format == DXGI_FORMAT_UNKNOWN)
  {
    DXGI_MODE_DESC mode_desc_fixed =
      *pNewTargetParameters;

    DXGI_SWAP_CHAIN_DESC swapDesc = { };
    This->GetDesc      (&swapDesc);

    mode_desc_fixed.Format = swapDesc.BufferDesc.Format;

    SK_LOGi0 (
      L"Replacing DXGI_FORMAT_UNKNOWN with %hs",
        SK_DXGI_FormatToStr (swapDesc.BufferDesc.Format).data ()
    );

    return
      SK_DXGI_SwapChain_ResizeTarget_Impl (
        This, &mode_desc_fixed, FALSE
      );
  }

  else
  {
    return
      SK_DXGI_SwapChain_ResizeTarget_Impl (
        This, pNewTargetParameters, FALSE
      );
  }
}

void
SK_DXGI_CreateSwapChain_PreInit (
  _Inout_opt_ DXGI_SWAP_CHAIN_DESC            *pDesc,
  _Inout_opt_ DXGI_SWAP_CHAIN_DESC1           *pDesc1,
  _Inout_opt_ HWND&                            hWnd,
  _Inout_opt_ DXGI_SWAP_CHAIN_FULLSCREEN_DESC *pFullscreenDesc,
  _In_opt_    IUnknown                        *pDevice,
              bool                             bIsD3D12 )
{
  auto& rb =
    SK_GetCurrentRenderBackend ();

  // Move the window to the origin of the user-forced monitor...
  //
  //   Old logic, disabled for reasons lost to time.
#if 0
  if (config.display.monitor_handle != 0)
  {
    MONITORINFO
      mi        = {         };
      mi.cbSize = sizeof (mi);

    if (GetMonitorInfo (config.display.monitor_handle, &mi))
    {
      SK_SetWindowPos ( hWnd, HWND_TOP,
                          mi.rcMonitor.left,
                          mi.rcMonitor.top,
                                         0, 0, SWP_NOZORDER       | SWP_NOSIZE |
                                               SWP_NOSENDCHANGING | SWP_ASYNCWINDOWPOS );
    }
  }
#endif

  if (config.apis.dxgi.d3d11.hook)
    WaitForInitDXGI ();

  // Stores common attributes between DESC and DESC1
  DXGI_SWAP_CHAIN_DESC stub_desc  = {   };
  bool                 translated = false;

  // If forcing flip-model, kill multisampling (of primary framebuffer)
  if (config.render.framerate.flip_discard)
  {
    rb.active_traits.bFlipMode =
      dxgi_caps.present.flip_sequential;
  }


  // Use Flip Sequential if ReShade is present, so that screenshots
  //   work as expected...
  static bool 
        bHasReShade = reshade::internal::get_reshade_module_handle (nullptr);
  if (! bHasReShade)
  {
    SK_RunOnce (
    {
      for (auto& import : imports->imports)
      {
        if ( StrStrIW (                      import.name.c_str (), L"ReShade")
                   || (import.filename != nullptr              &&
             StrStrIW (import.filename->get_value_str ().c_str (), L"ReShade")) )
        {
          bHasReShade = true;
          break;
        }
      }
    });
  }

  const DXGI_SWAP_EFFECT
    original_swap_effect =
      pDesc  != nullptr  ? pDesc ->SwapEffect :
      pDesc1 != nullptr  ? pDesc1->SwapEffect :
        DXGI_SWAP_EFFECT_DISCARD;



  auto _DescribeSwapChain = [&](const wchar_t* wszLabel) noexcept -> void
  {
    wchar_t    wszMSAA [128] = { };
    swprintf ( wszMSAA, pDesc->SampleDesc.Count > 1 ?
                                      L"%u Samples" :
                         L"Not Used (or Offscreen)",
                          pDesc->SampleDesc.Count );

    wchar_t                                  wszWindowClass [128] = { };
    RealGetWindowClass (pDesc->OutputWindow, wszWindowClass, 127);

    dll_log->LogEx ( true,
    L"[Swap Chain]  { %ws }\n"
    L"  +-------------+-------------------------------------------------------------------------+\n"
    L"  | Resolution. |  %4lux%4lu @ %6.2f Hz%-50ws|\n"
    L"  | Format..... |  %-71hs|\n"
    L"  | Buffers.... |  %-2lu%-69ws|\n"
    L"  | MSAA....... |  %-71ws|\n"
    L"  | Mode....... |  %-71ws|\n"
    L"  | Window..... |  0x%08x%-61ws|\n"
    L"  | WindowClass |  %-71ws|\n"
    L"  | Scaling.... |  %-71ws|\n"
    L"  | Scanlines.. |  %-71ws|\n",
         wszLabel,
    pDesc->BufferDesc.Width,
    pDesc->BufferDesc.Height,
    pDesc->BufferDesc.RefreshRate.Denominator != 0 ?
      static_cast <float> (pDesc->BufferDesc.RefreshRate.Numerator) /
      static_cast <float> (pDesc->BufferDesc.RefreshRate.Denominator) :
        std::numeric_limits <float>::quiet_NaN (), L" ",
    SK_DXGI_FormatToStr (pDesc->BufferDesc.Format).data (),
    pDesc->BufferCount, L" ",
    wszMSAA,
    pDesc->Windowed ?
        L"Windowed" :
        L"Fullscreen",
    sk::narrow_cast <UINT> ((uintptr_t)pDesc->OutputWindow), L" ",
    wszWindowClass,
    pDesc->BufferDesc.Scaling == DXGI_MODE_SCALING_UNSPECIFIED ?
                                                L"Unspecified" :
    pDesc->BufferDesc.Scaling == DXGI_MODE_SCALING_CENTERED    ?
                                                   L"Centered" :
                                                   L"Stretched",
    pDesc->BufferDesc.ScanlineOrdering == DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED ?
                                                                L"Unspecified" :
    pDesc->BufferDesc.ScanlineOrdering == DXGI_MODE_SCANLINE_ORDER_PROGRESSIVE ?
                                                                L"Progressive" :
    pDesc->BufferDesc.ScanlineOrdering == DXGI_MODE_SCANLINE_ORDER_UPPER_FIELD_FIRST ?
                                                                  L"Interlaced Even" :
                                                                  L"Interlaced Odd" );

    INT                                                                  lines = 0;
    std::string flags_str =
    SK_DXGI_DescribeSwapChainFlags ((DXGI_SWAP_CHAIN_FLAG)pDesc->Flags, &lines);

    const char*
      begin_str = flags_str.c_str ();

    for (int i = 0 ; i < lines ; ++i)
    {
      char* end_str = StrStrIA (begin_str, "\n");
      if (  end_str != nullptr)
           *end_str = '\0';

      if (i == 0)
      {
        if (lines == 1)
        {
          dll_log->LogEx ( false,
            L"  | Flags...... |  %-71hs|\n",     begin_str);
        }

        else
        {
          dll_log->LogEx ( false,
            L"  | Flags [%d].. |  %-71hs|\n", i, begin_str);
        }
      }

      else
      {
        dll_log->LogEx ( false,
            L"  | Flags [%d].. |  %-71hs|\n", i, begin_str);
      }

      begin_str =
        ( end_str + 1 );
    }

    // Empty Flags
    if (lines == 0)
    {
      dll_log->LogEx ( false,
        L"  | Flags...... |  0x%04x%-65ws|\n", pDesc->Flags, L" "
      );
    }

    dll_log->LogEx ( false,
      L"  | SwapEffect. |  %-71ws|\n"
      L"  +-------------+-------------------------------------------------------------------------+\n",
      pDesc->SwapEffect         == 0 ?
        L"Discard" :
        pDesc->SwapEffect       == 1 ?
          L"Sequential" :
          pDesc->SwapEffect     == 2 ?
            L"<Unknown>" :
            pDesc->SwapEffect   == 3 ?
              L"Flip Sequential" :
              pDesc->SwapEffect == 4 ?
                L"Flip Discard" :
                L"<Unknown>"
    );
  };

  if (pDesc1 != nullptr)
  {
    if (pDesc == nullptr)
    {
      pDesc = &stub_desc;

      stub_desc.BufferCount                        = pDesc1->BufferCount;
      stub_desc.BufferUsage                        = pDesc1->BufferUsage;
      stub_desc.Flags                              = pDesc1->Flags;
      stub_desc.SwapEffect                         = pDesc1->SwapEffect;
      stub_desc.SampleDesc.Count                   = pDesc1->SampleDesc.Count;
      stub_desc.SampleDesc.Quality                 = pDesc1->SampleDesc.Quality;
      stub_desc.BufferDesc.Format                  = pDesc1->Format;
      stub_desc.BufferDesc.Height                  = pDesc1->Height;
      stub_desc.BufferDesc.Width                   = pDesc1->Width;
      stub_desc.OutputWindow                       = hWnd;

      if (pFullscreenDesc != nullptr)
      {
        stub_desc.Windowed                           = pFullscreenDesc->Windowed;
        stub_desc.BufferDesc.RefreshRate.Denominator = pFullscreenDesc->RefreshRate.Denominator;
        stub_desc.BufferDesc.RefreshRate.Numerator   = pFullscreenDesc->RefreshRate.Numerator;
        stub_desc.BufferDesc.Scaling                 = pFullscreenDesc->Scaling;
        stub_desc.BufferDesc.ScanlineOrdering        = pFullscreenDesc->ScanlineOrdering;

        if (stub_desc.Windowed)
        {
          stub_desc.BufferDesc.RefreshRate.Denominator = 0;
          stub_desc.BufferDesc.RefreshRate.Numerator   = 0;
        }
      }

      else
      {
        stub_desc.BufferDesc.Scaling                 = DXGI_MODE_SCALING_UNSPECIFIED;
        stub_desc.Windowed                           = TRUE;
        stub_desc.BufferDesc.RefreshRate.Denominator = 0;
        stub_desc.BufferDesc.RefreshRate.Numerator   = 0;
      }

      // Need to take this stuff and put it back in the appropriate structures before returning :)
      translated = true;
    }
  }

  DXGI_SWAP_CHAIN_DESC orig_desc = { };

  if (pDesc != nullptr)
  {
    orig_desc = *pDesc;

    if (SK_GetCurrentGameID () == SK_GAME_ID::Elex2)
    {
      // Elex 2 needs this or its fullscreen options don't work
      pDesc->Flags |= DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    }

    if (config.render.framerate.swapchain_wait != 0)
    {
      // Turn off SK's Waitable SwapChain override, the game's already using it!
      if (pDesc->Flags & DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT)
      {
        config.render.framerate.swapchain_wait = 0;

        SK_LOGi0 (
          L"* Disabling Latency Waitable SwapChain Override; game is already using one!"
        );
      }
    }


    // Some behavior overrides are unneeded in Flip Model games
    bool already_flip_model =
      SK_DXGI_IsFlipModelSwapEffect (pDesc->SwapEffect);

    rb.active_traits.bOriginallyFlip = already_flip_model;

    // These games may want the contents of the SwapChain
    //    backbuffer to remain defined.
    //
    //   * If SK's SwapChain wrapper is active, avoid
    //       clearing backbuffers after Present (...)
    //
#if 0  // This is a user-defined preference now; useful for wrong aspect ratio rendering
    if (/*(!bOriginallyFlip) ||*/pDesc->SwapEffect == DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL
                             ||  pDesc->SwapEffect == DXGI_SWAP_EFFECT_SEQUENTIAL )
    {
      config.render.dxgi.clear_flipped_chain = false;
    }
#endif

    _DescribeSwapChain (L"ORIGINAL");

    #if 0
    bool bScalingIsProblematic = false;

    SK_ComQIPtr <IDXGIDevice>  pDevDXGI (pDevice);
    SK_ComPtr   <IDXGIAdapter> pAdapter;

    if (           pDevDXGI != nullptr &&
        SUCCEEDED (pDevDXGI->GetAdapter (&pAdapter.p)))
    {
      SK_ComPtr <IDXGIOutput> pOutput;

      for ( UINT                   idx = 0                                ;
            pAdapter->EnumOutputs (idx, &pOutput) != DXGI_ERROR_NOT_FOUND ;
                                 ++idx )
      {
        DXGI_OUTPUT_DESC   outputDesc = { };
        pOutput->GetDesc (&outputDesc);

        RECT                                 rectWindow = { };
        GetWindowRect (pDesc->OutputWindow, &rectWindow);

        if ( PtInRect (
               &outputDesc.DesktopCoordinates,
                POINT {
                  rectWindow.left + (rectWindow.right  - rectWindow.left) / 2,
                  rectWindow.top  + (rectWindow.bottom - rectWindow.top ) / 2
                      }
             )
           )
        {
          auto modeDesc =
            pDesc->BufferDesc;

          modeDesc.Width  =
            outputDesc.DesktopCoordinates.right  -
            outputDesc.DesktopCoordinates.left;
          modeDesc.Height =
            outputDesc.DesktopCoordinates.bottom -
            outputDesc.DesktopCoordinates.top;

          bScalingIsProblematic =
            SK_DXGI_IsScalingPreventingRequestedResolution (
              &pDesc->BufferDesc, pOutput, pDevice
            );

          break;
        }
      }
    }
#else
    std::ignore = pDevice;

    bool bScalingIsProblematic =
      pDesc->BufferDesc.Scaling != DXGI_MODE_SCALING_UNSPECIFIED;
#endif

    if (bScalingIsProblematic)
    {
      rb.active_traits.bMisusingDXGIScaling = true;

      SK_LOGi0 (
        L" >> Removing Scaling Mode (%hs) Because It Prevents Native Resolution",
          SK_DXGI_DescribeScalingMode (pDesc->BufferDesc.Scaling)
      );

      pDesc->BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
    }


    // Set things up to make the swap chain Alt+Enter friendly
    if (_NO_ALLOW_MODE_SWITCH)
      pDesc->Flags &= ~DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;


    if (config.render.framerate.disable_flip)
    {
      if (     pDesc->SwapEffect == (DXGI_SWAP_EFFECT)DXGI_SWAP_EFFECT_FLIP_DISCARD)
               pDesc->SwapEffect =                         DXGI_SWAP_EFFECT_DISCARD;
      else if (pDesc->SwapEffect == (DXGI_SWAP_EFFECT)DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL)
               pDesc->SwapEffect =                         DXGI_SWAP_EFFECT_SEQUENTIAL;

      pDesc->Flags &= ~DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;
      pDesc->Flags &= ~DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT;
    }

    if (! config.window.res.override.isZero ())
    {
      pDesc->BufferDesc.Width  = config.window.res.override.x;
      pDesc->BufferDesc.Height = config.window.res.override.y;
    }


    if (config.render.dxgi.msaa_samples > 0)
    {
      SK_LOGi0 ( L">> MSAA Sample Count Override: %d", config.render.dxgi.msaa_samples );

      pDesc->SampleDesc.Count = config.render.dxgi.msaa_samples;
    }


    bool fake_fullscreen =
      SK_GetCurrentRenderBackend ().isFakeFullscreen ();


    // Dummy init swapchains (i.e. 1x1 pixel) should not have
    //   override parameters applied or it's usually fatal.
    bool no_override = (! SK_DXGI_IsSwapChainReal (*pDesc));



    if (! no_override)
    {
      if (config.render.dxgi.safe_fullscreen)
        pDesc->Flags |= DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

      if (SK_DXGI_IsFlipModelSwapEffect (pDesc->SwapEffect))
        rb.active_traits.bFlipMode = true;

      if (request_mode_change == mode_change_request_e::Fullscreen)
      {
        SK_LOGi0 (L" >> User-Requested Mode Change: Fullscreen");

        pDesc->Windowed = FALSE;
      }

      if (! config.render.dxgi.fake_fullscreen_mode)
      {
        if ((! config.window.background_render) && config.display.force_fullscreen && pDesc->Windowed)
        {
          SK_LOGi0 ( L" >> Display Override "
                     L"(Requested: Windowed, Using: Fullscreen)" );

          pDesc->Windowed = FALSE;
        }

        else if ((config.window.background_render || config.display.force_windowed) && pDesc->Windowed == FALSE)
        {
          // If the chain desc is setup for Windowed, we need an
          //   OutputWindow or we cannot override this...
          //
          if (          (pDesc->OutputWindow != 0) &&
               IsWindow (pDesc->OutputWindow)
             )
          {
            SK_LOGi0 ( L" >> Display Override "
                       L"(Requested: Fullscreen, Using: Windowed)" );

            pDesc->Windowed = TRUE;
          }
        }
      }

      else
      {
        if (pDesc->Windowed == FALSE)
        {
          fake_fullscreen = true;

          SK_LOGi0 ( L" >> Display Override "
                     L"(Requested: Fullscreen, Using: Fake Fullscreen)" );

          pDesc->Windowed = TRUE;
        }
      }

      // If forcing flip-model, then force multisampling (of the primary framebuffer) off
      if (config.render.framerate.flip_discard)
      {
        rb.active_traits.bFlipMode =
          dxgi_caps.present.flip_sequential;

        // Allow manually enabling MSAA in a game while using Flip Model
        if (config.render.dxgi.msaa_samples > 0)
          rb.active_traits.uiOriginalBltSampleCount = config.render.dxgi.msaa_samples;
        else
          rb.active_traits.uiOriginalBltSampleCount = pDesc->SampleDesc.Count;

        pDesc->SampleDesc.Count   = 1;
        pDesc->SampleDesc.Quality = 0;

        // Format overrides must be performed in certain cases (sRGB)
        switch (pDesc->BufferDesc.Format)
        {
          case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
            SK_LOGs0 ( L" DXGI 1.2 ",
                       L" >> sRGB (B8G8R8A8) Override Required to Enable Flip Model" );
            rb.srgb_stripped                 = true;
            rb.active_traits.bOriginallysRGB = true;
            pDesc->BufferDesc.Format         = DXGI_FORMAT_R10G10B10A2_UNORM;
            break;//[[fallthrough]];
          case DXGI_FORMAT_B8G8R8A8_UNORM:
          case DXGI_FORMAT_B8G8R8A8_TYPELESS: // WTF? Should be typed...
            pDesc->BufferDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
            break;
          case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
            SK_LOGs0 ( L" DXGI 1.2 ",
                       L" >> sRGB (R8G8B8A8) Override Required to Enable Flip Model" );

            rb.srgb_stripped                 = true;
            rb.active_traits.bOriginallysRGB = true;
            pDesc->BufferDesc.Format         = DXGI_FORMAT_R10G10B10A2_UNORM;
            break;//[[fallthrough]];
          case DXGI_FORMAT_R8G8B8A8_UNORM:
          case DXGI_FORMAT_R8G8B8A8_TYPELESS: // WTF? Should be typed...
            pDesc->BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
            break;        }

        if (! already_flip_model)
        {
          if (          pDesc->OutputWindow != 0 &&
              IsWindow (pDesc->OutputWindow)     && (! pDesc->Windowed))
          {
            SK_LOGs0 ( L" DXGI 1.2 ",
                       L" Flip Model SwapChains Must be Created Windowed Initially" );

            pDesc->Windowed = TRUE;
          }
        }
      }

      if (config.render.output.force_10bpc && (! __SK_HDR_16BitSwap))
      {
        if ( DirectX::MakeTypeless (pDesc->BufferDesc.Format) ==
             DirectX::MakeTypeless (DXGI_FORMAT_R8G8B8A8_UNORM) || 
             DirectX::MakeTypeless (pDesc->BufferDesc.Format) ==
             DirectX::MakeTypeless (DXGI_FORMAT_B8G8R8A8_UNORM) )
        {
          SK_LOGi0 ( L" >> 8-bpc format (%hs) replaced with "
                     L"DXGI_FORMAT_R10G10B10A2_UNORM for 10-bpc override",
                       SK_DXGI_FormatToStr (pDesc->BufferDesc.Format).data () );

          pDesc->BufferDesc.Format =
            DXGI_FORMAT_R10G10B10A2_UNORM;
        }
      }

      if (       config.render.framerate.buffer_count != SK_NoPreference     &&
           (UINT)config.render.framerate.buffer_count !=  pDesc->BufferCount &&
           pDesc->BufferCount                         !=  0                  &&

           config.render.framerate.buffer_count       >   0                  &&
           config.render.framerate.buffer_count       <   16 )
      {
        pDesc->BufferCount = config.render.framerate.buffer_count;
        SK_LOGi0 (L" >> Buffer Count Override : % lu buffers", pDesc->BufferCount);
      }

      if ( ( config.render.framerate.flip_discard ||
                     rb.active_traits.bFlipMode ) &&
              dxgi_caps.swapchain.allow_tearing )
      {
        if ((pDesc->Flags & DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING) == 0)
        {
          SK_LOGs0 (L" DXGI 1.5 ",
                    L" >> Tearing Option : Enable  (Override)");

          pDesc->Flags |= DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;
        }
      }

      if ( config.render.dxgi.scaling_mode != SK_NoPreference &&
            pDesc->BufferDesc.Scaling      !=
              (DXGI_MODE_SCALING)config.render.dxgi.scaling_mode )
      {
        SK_LOGi0 ( L" >> Scaling Override "
                   L"(Requested: %hs, Using: %hs)",
                     SK_DXGI_DescribeScalingMode (
                       pDesc->BufferDesc.Scaling
                     ),
                       SK_DXGI_DescribeScalingMode (
                         (DXGI_MODE_SCALING)config.render.dxgi.scaling_mode
                       )
                 );

        pDesc->BufferDesc.Scaling =
          (DXGI_MODE_SCALING)config.render.dxgi.scaling_mode;
      }

      if ( config.render.dxgi.scanline_order   != SK_NoPreference &&
            pDesc->BufferDesc.ScanlineOrdering !=
              (DXGI_MODE_SCANLINE_ORDER)config.render.dxgi.scanline_order )
      {
        SK_LOGi0 ( L" >> Scanline Override "
                   L"(Requested: %hs, Using: %hs)",
                     SK_DXGI_DescribeScanlineOrder (
                       pDesc->BufferDesc.ScanlineOrdering
                     ),
                       SK_DXGI_DescribeScanlineOrder (
                         (DXGI_MODE_SCANLINE_ORDER)config.render.dxgi.scanline_order
                       )
                 );

        pDesc->BufferDesc.ScanlineOrdering =
          (DXGI_MODE_SCANLINE_ORDER)config.render.dxgi.scanline_order;
      }

      if (! config.display.allow_refresh_change)
      {
        if (pDesc->BufferDesc.RefreshRate.Denominator != 0)
        {
          SK_LOGi0 (
            L" >> Ignoring Requested Refresh Rate (%4.2f Hz)...",
              static_cast <float> (pDesc->BufferDesc.RefreshRate.Numerator) /
              static_cast <float> (pDesc->BufferDesc.RefreshRate.Denominator)
          );

          pDesc->BufferDesc.RefreshRate.Numerator   = 0;
          pDesc->BufferDesc.RefreshRate.Denominator = 0;
        }
      }

      if ( config.render.framerate.rescan_.Denom   !=  1 ||
          (config.render.framerate.refresh_rate    > 0.0f &&
           pDesc->BufferDesc.RefreshRate.Numerator != sk::narrow_cast <UINT> (
           config.render.framerate.refresh_rate                              )
          )
        )
      {
        if ( ( config.render.framerate.rescan_.Denom     > 0   &&
               config.render.framerate.rescan_.Numerator > 0 ) &&
         !( pDesc->BufferDesc.RefreshRate.Numerator   == config.render.framerate.rescan_.Numerator &&
            pDesc->BufferDesc.RefreshRate.Denominator == config.render.framerate.rescan_.Denom ) )
        {
          SK_LOGi0 ( L" >> Refresh Override (Requested: %f, Using: %f)",
                      pDesc->BufferDesc.RefreshRate.Denominator != 0 ?
              static_cast <float> (pDesc->BufferDesc.RefreshRate.Numerator) /
              static_cast <float> (pDesc->BufferDesc.RefreshRate.Denominator) :
                          std::numeric_limits <float>::quiet_NaN (),
              static_cast <float> (config.render.framerate.rescan_.Numerator) /
              static_cast <float> (config.render.framerate.rescan_.Denom)
                   );

          if (config.render.framerate.rescan_.Denom != 1)
          {
            pDesc->BufferDesc.RefreshRate.Numerator   = config.render.framerate.rescan_.Numerator;
            pDesc->BufferDesc.RefreshRate.Denominator = config.render.framerate.rescan_.Denom;
          }

          else
          {
            pDesc->BufferDesc.RefreshRate.Numerator   =
              sk::narrow_cast <UINT> (
                std::ceil (config.render.framerate.refresh_rate)
              );
            pDesc->BufferDesc.RefreshRate.Denominator = 1;
          }
        }
      }

      rb.active_traits.bWait =
        rb.active_traits.bFlipMode && dxgi_caps.present.waitable;

      rb.active_traits.bWait =
        rb.active_traits.bWait && ( config.render.framerate.swapchain_wait > 0 );

      // User is forcing flip mode
      //
      if (rb.active_traits.bFlipMode || SK_DXGI_IsFlipModelSwapEffect (pDesc->SwapEffect))
      {
        if (rb.active_traits.bWait)
          pDesc->Flags |= DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT;

        // Flip Presentation Model requires 3 Buffers (1 is implicit)

        if (pDesc->BufferCount < 2)
        {
          if ( config.render.framerate.flip_discard )
          {
            // Supply at minimum 2, at maximum 16 (game was not in compliance)
            config.render.framerate.buffer_count =
              std::max (2, std::min (16, config.render.framerate.buffer_count));
          }
        }

        if ( config.render.framerate.flip_discard &&
                   dxgi_caps.present.flip_discard )
        {

            pDesc->SwapEffect = 
              (original_swap_effect == DXGI_SWAP_EFFECT_DISCARD ||
               original_swap_effect == DXGI_SWAP_EFFECT_FLIP_DISCARD) ?
                                       DXGI_SWAP_EFFECT_FLIP_DISCARD  :
                                       DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
        }
        else // On Windows 8.1 and older, sequential must substitute for discard
          pDesc->SwapEffect  = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
      }

      // User is not forcing flip model, and the game is not using it either
      //
      else
      {
        // Resort to triple-buffering if flip mode is not available
        //   because AMD drivers tend not to work correctly if a game ever
        //     tries to use more than 3 buffers in BitBlit mode.
        if (config.render.framerate.buffer_count > 3)
            config.render.framerate.buffer_count = 3;
      }

      if (config.render.framerate.buffer_count > 0)
        pDesc->BufferCount = config.render.framerate.buffer_count;
    }

    // Option to force Flip Sequential for buggy systems
    if (pDesc->SwapEffect == DXGI_SWAP_EFFECT_FLIP_DISCARD && config.render.framerate.flip_sequential)
        pDesc->SwapEffect  = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;

    SK_LOGs1 ( L" DXGI 1.2 ",
               L"  >> Using %s Presentation Model  [Waitable: %s - %li ms]",
                 (pDesc->SwapEffect >= DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL) ?
                   L"Flip" : L"Traditional",
                     rb.active_traits.bWait ? L"Yes" : L"No",
                     rb.active_traits.bWait ? config.render.framerate.swapchain_wait : 0
             );


    // HDR override requires Flip Model
    //
    //  --> Flip Model requires no MSAA
    //
    if (SK_DXGI_IsFlipModelSwapEffect (pDesc->SwapEffect))
    {
      // UAV binding of D3D12 SwapChains is not allowed
      if (( __SK_HDR_16BitSwap ||
            __SK_HDR_10BitSwap ) && (! bIsD3D12))
      {
        SK_LOGs0 ( L"  SK HDR  ",
                   L"  >> Adding Unordered Access View to SwapChain for"
                   L" more advanced HDR processing." );

        pDesc->BufferUsage |=
          DXGI_USAGE_UNORDERED_ACCESS;
      }

      pDesc->BufferDesc.Format =
        SK_DXGI_PickHDRFormat (pDesc->BufferDesc.Format, FALSE, TRUE);

      if (pDesc->SampleDesc.Count != 1)
      {
        SK_LOGs0 ( L" DXGI 1.2 ",
                   L"  >> Disabling SwapChain-based MSAA for Flip Model compliance." );

        pDesc->SampleDesc.Count = 1;
      }

      if (! bIsD3D12)
      {
        if (config.render.dxgi.clear_flipped_chain)
          SK_LOGi0 (L"  >> Will Clear SwapChain Backbuffer After Present");
        else
          SK_LOGi0 (L"  >> Will Not Clear SwapChain Backbuffer After Present");
      }
    }

    bool borderless = config.window.borderless || fake_fullscreen;
    bool fullscreen = config.window.fullscreen || fake_fullscreen;

    if (borderless && fullscreen && pDesc->Windowed != FALSE)
    {
      HMONITOR hMonTarget =
        MonitorFromWindow (pDesc->OutputWindow, MONITOR_DEFAULTTONEAREST);

      MONITORINFO minfo        = { };
                  minfo.cbSize = sizeof (MONITORINFO);

      if (GetMonitorInfoW (hMonTarget, &minfo))
      {
        pDesc->BufferDesc.Width  = minfo.rcMonitor.right  - minfo.rcMonitor.left;
        pDesc->BufferDesc.Height = minfo.rcMonitor.bottom - minfo.rcMonitor.top;
      }
    }

    // Clamp the buffer dimensions if the user has a min/max preference
    const UINT
      max_x = config.render.dxgi.res.max.x < pDesc->BufferDesc.Width  ?
              config.render.dxgi.res.max.x : pDesc->BufferDesc.Width,
      min_x = config.render.dxgi.res.min.x > pDesc->BufferDesc.Width  ?
              config.render.dxgi.res.min.x : pDesc->BufferDesc.Width,
      max_y = config.render.dxgi.res.max.y < pDesc->BufferDesc.Height ?
              config.render.dxgi.res.max.y : pDesc->BufferDesc.Height,
      min_y = config.render.dxgi.res.min.y > pDesc->BufferDesc.Height ?
              config.render.dxgi.res.min.y : pDesc->BufferDesc.Height;

    pDesc->BufferDesc.Width   =  std::max ( max_x , min_x );
    pDesc->BufferDesc.Height  =  std::max ( max_y , min_y );
  }


  if ( pDesc1 != nullptr &&
       pDesc  != nullptr && translated )
  {
    pDesc1->BufferCount = pDesc->BufferCount;
    pDesc1->BufferUsage = pDesc->BufferUsage;
    pDesc1->Flags       = pDesc->Flags;
    pDesc1->SwapEffect  = pDesc->SwapEffect;
    pDesc1->Height      = pDesc->BufferDesc.Height;
    pDesc1->Width       = pDesc->BufferDesc.Width;
    pDesc1->Format      = pDesc->BufferDesc.Format;
    pDesc1->SampleDesc.Count
                        = pDesc->SampleDesc.Count;
    pDesc1->SampleDesc.Quality
                        = pDesc->SampleDesc.Quality;

    if (pFullscreenDesc != nullptr)
    {
      pFullscreenDesc->Scaling          = pDesc->BufferDesc.Scaling;
      pFullscreenDesc->ScanlineOrdering = pDesc->BufferDesc.ScanlineOrdering;
      pFullscreenDesc->RefreshRate      = pDesc->BufferDesc.RefreshRate;
      pFullscreenDesc->Windowed         = pDesc->Windowed;
    }
  }

  _ORIGINAL_SWAP_CHAIN_DESC = orig_desc;

  if (pDesc != nullptr && memcmp (&orig_desc, pDesc, sizeof (DXGI_SWAP_CHAIN_DESC)) != 0)
  {
    _DescribeSwapChain (L"SPECIAL K OVERRIDES APPLIED");
  }
}

void
SK_DXGI_UpdateLatencies (IDXGISwapChain *pSwapChain)
{
  if (! pSwapChain)
    return;

  const uint32_t max_latency =
    config.render.framerate.pre_render_limit;

  DXGI_SWAP_CHAIN_DESC  swapDesc = { };
  pSwapChain->GetDesc (&swapDesc);

  if ((swapDesc.Flags & DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT)
                     == DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT)
  {
    SK_ComQIPtr <IDXGISwapChain2>
        pSwapChain2 (pSwapChain);
    if (pSwapChain2 != nullptr)
    {
      if (max_latency < 16 && max_latency > 0)
      {
        dll_log->Log ( L"[   DXGI   ] Setting Swapchain Frame Latency: %lu",
                                             max_latency);
        pSwapChain2->SetMaximumFrameLatency (max_latency);
      }
    }
  }

  else if ( max_latency < 16 &&
            max_latency >  0 )
  {
    SK_ComPtr <IDXGIDevice1> pDevice1;

    if (SUCCEEDED ( pSwapChain->GetDevice (
                       IID_PPV_ARGS (&pDevice1.p)
                                          )
                  )
       )
    {
      dll_log->Log ( L"[   DXGI   ] Setting Device    Frame Latency: %lu",
                                                      max_latency);
      SK_DXGI_SetDeviceMaximumFrameLatency (pDevice1, max_latency);
    }
  }

  InterlockedAdd (&SK_RenderBackend::flip_skip, 1);
}

void
SK_DXGI_CreateSwapChain_PostInit (
  _In_  IUnknown              *pDevice,
  _In_  DXGI_SWAP_CHAIN_DESC  *pDesc,
  _In_  IDXGISwapChain       **ppSwapChain )
{
  SK_ReleaseAssert (pDesc != nullptr);

  if (pDesc == nullptr)
    return;

  SK_RenderBackend& rb =
    SK_GetCurrentRenderBackend ();

  wchar_t                                   wszClass [MAX_PATH + 2] = { };
  RealGetWindowClassW (pDesc->OutputWindow, wszClass, MAX_PATH);

  bool dummy_window =
    SK_Win32_IsDummyWindowClass (pDesc->OutputWindow) ||
                               // AMD's buggy OpenGL interop...
                               //   why these dimensions, nobody knows
                               ( pDesc->BufferDesc.Width  == 176 &&
                                 pDesc->BufferDesc.Height == 1 );

  if (! dummy_window)
  {
    
    HWND hWndDevice = pDesc->OutputWindow;
    HWND hWndRoot   = GetAncestor (pDesc->OutputWindow, GA_ROOTOWNER);

    auto& windows =
      rb.windows;

    if (      windows.device != nullptr &&
         pDesc->OutputWindow != nullptr &&
         pDesc->OutputWindow != windows.device )
    {
      SK_LOGi0 (L"Game created a new window?!");

      rb.releaseOwnedResources ();

      // This is a hard reset, we're going to get a new cmd queue
      _d3d11_rbk->release (_d3d11_rbk->_pSwapChain);
      _d3d12_rbk->release (_d3d12_rbk->_pSwapChain);
      _d3d12_rbk->_pCommandQueue.Release ();

      windows.setDevice (hWndDevice);
      windows.setFocus  (hWndRoot);
    }

    else
    {
      SK_InstallWindowHook (hWndRoot);

      windows.setDevice (hWndDevice);
      windows.setFocus  (hWndRoot);

      if (ppSwapChain != nullptr)
        SK_DXGI_HookSwapChain (*ppSwapChain);
    }

    SK_Inject_SetFocusWindow (hWndRoot);

    if (pDesc->Windowed != FALSE)
    {
      //if (config.window.always_on_top >= AlwaysOnTop)
      //  SK_DeferCommand ("Window.TopMost true");
    }

    SK_Window_RepositionIfNeeded ();

    // Assume the first thing to create a D3D11 render device is
    //   the game and that devices never migrate threads; for most games
    //     this assumption holds.
    if ( dwRenderThread == 0x00 )
    {
      dwRenderThread = SK_Thread_GetCurrentId ();
    }
  }

  RECT client = { };

  // TODO (XXX): This may be DPI scaled -- write a function to get the client rect in pixel coords (!!)
  //
  GetClientRect (pDesc->OutputWindow, &client);

  auto width  = client.right  - client.left;
  auto height = client.bottom - client.top;

  if (                   pDesc->BufferDesc.Width != 0)
       SK_SetWindowResX (pDesc->BufferDesc.Width);
  else SK_SetWindowResX (                  width);

  if (                   pDesc->BufferDesc.Height != 0)
       SK_SetWindowResY (pDesc->BufferDesc.Height);
  else SK_SetWindowResY (                  height);

  if (  ppSwapChain != nullptr &&
       *ppSwapChain != nullptr )
  {
    SK_ComQIPtr < IDXGISwapChain2 >
                      pSwapChain2 (*ppSwapChain);
    SK_DXGI_UpdateLatencies (
                      pSwapChain2);
    if ( nullptr  !=  pSwapChain2.p )
    {
      if (rb.swapchain.p == nullptr)
          rb.swapchain    = pSwapChain2;

      if (config.render.framerate.swapchain_wait > 0)
      {
        // Immediately following creation, wait on the SwapChain to get the semaphore initialized.
        SK_AutoHandle hWaitHandle (rb.getSwapWaitHandle ());
        if ((intptr_t)hWaitHandle.m_h > 0 )
        {
          SK_WaitForSingleObject (
                hWaitHandle.m_h, 15
          );
        }
      }
    }
  }

  SK_ComQIPtr <ID3D11Device>
      pDev (pDevice);
  if (pDev != nullptr)
  {
    g_pD3D11Dev = pDev;

    rb.fullscreen_exclusive = (! pDesc->Windowed);
  }

  if (rb.fullscreen_exclusive)
  {
    SK_Window_RemoveBorders ();
    // Fullscreen optimizations sometimes do not work (:shrug:)
  }
}

void
SK_DXGI_CreateSwapChain1_PostInit (
  _In_     IUnknown                         *pDevice,
  _In_     HWND                              hwnd,
  _In_     DXGI_SWAP_CHAIN_DESC1            *pDesc1,
  _In_opt_ DXGI_SWAP_CHAIN_FULLSCREEN_DESC  *pFullscreenDesc,
  _In_     IDXGISwapChain1                 **ppSwapChain1 )
{
  // According to some Unreal engine games, ppSwapChain is optional...
  //   the docs say otherwise, but we can't argue with engines now can we?
  //
  if (! ppSwapChain1)
    return;

  if (! pDesc1)
    return;

  // ONLY AS COMPLETE AS NEEDED, if new code is added to PostInit, this
  //   will probably need changing.
  DXGI_SWAP_CHAIN_DESC desc = { };

  desc.BufferDesc.Width   = pDesc1->Width;
  desc.BufferDesc.Height  = pDesc1->Height;

  desc.BufferDesc.Format  = pDesc1->Format;
  //desc.BufferDesc.Scaling = pDesc1->Scaling;

  desc.BufferCount        = pDesc1->BufferCount;
  desc.BufferUsage        = pDesc1->BufferUsage;
  desc.Flags              = pDesc1->Flags;
  desc.SampleDesc         = pDesc1->SampleDesc;
  desc.SwapEffect         = pDesc1->SwapEffect;
  desc.OutputWindow       = hwnd;

  if (pFullscreenDesc)
  {
    desc.Windowed                    = pFullscreenDesc->Windowed;
    desc.BufferDesc.RefreshRate.Numerator
                                     = pFullscreenDesc->RefreshRate.Numerator;
    desc.BufferDesc.RefreshRate.Denominator
                                     = pFullscreenDesc->RefreshRate.Denominator;
    desc.BufferDesc.ScanlineOrdering = pFullscreenDesc->ScanlineOrdering;
  }

  else
  {
    desc.Windowed                    = TRUE;
  }

  if (*ppSwapChain1 != nullptr)
  {
    SK_ComQIPtr <IDXGISwapChain>
       pSwapChain (*ppSwapChain1);

    return
      SK_DXGI_CreateSwapChain_PostInit ( pDevice, &desc, &pSwapChain.p );
  }
}

#include <d3d12.h>

class SK_IWrapDXGIFactory : public IDXGIFactory5
{
public:
  virtual HRESULT STDMETHODCALLTYPE QueryInterface (REFIID riid, void __RPC_FAR *__RPC_FAR *ppvObject)
                                                          { return pReal->QueryInterface (riid, ppvObject); }
  virtual ULONG   STDMETHODCALLTYPE AddRef         (void) { ++refs_;                                  return pReal->AddRef  (); };
  virtual ULONG   STDMETHODCALLTYPE Release        (void) { --refs_; if (refs_ == 0) { delete this; } return pReal->Release (); };


  virtual HRESULT STDMETHODCALLTYPE SetPrivateData          (REFGUID Name, UINT DataSize, const void *pData) { return pReal->SetPrivateData          (Name, DataSize, pData);  }
  virtual HRESULT STDMETHODCALLTYPE SetPrivateDataInterface (REFGUID Name, const IUnknown *pUnknown)         { return pReal->SetPrivateDataInterface (Name, pUnknown);         }
  virtual HRESULT STDMETHODCALLTYPE GetPrivateData          (REFGUID Name, UINT *pDataSize, void *pData)     { return pReal->GetPrivateData          (Name, pDataSize, pData); }
  virtual HRESULT STDMETHODCALLTYPE GetParent               (REFIID riid, void **ppParent)                   { return pReal->GetParent               (riid, ppParent);         }




  virtual HRESULT STDMETHODCALLTYPE EnumAdapters          (UINT             Adapter,
                                                           IDXGIAdapter **ppAdapter)        { return pReal->EnumAdapters          (Adapter, ppAdapter);  }
  virtual HRESULT STDMETHODCALLTYPE MakeWindowAssociation (HWND   WindowHandle, UINT Flags) { return pReal->MakeWindowAssociation (WindowHandle, Flags); }
  virtual HRESULT STDMETHODCALLTYPE GetWindowAssociation  (HWND *pWindowHandle)             { return pReal->GetWindowAssociation  (pWindowHandle);       }
  virtual HRESULT STDMETHODCALLTYPE CreateSwapChain       ( IUnknown              *pDevice,
                                                            DXGI_SWAP_CHAIN_DESC  *pDesc,
                                                            IDXGISwapChain       **ppSwapChain
  )
  {
    return pReal->CreateSwapChain (pDevice, pDesc, ppSwapChain);
  }
  virtual HRESULT STDMETHODCALLTYPE CreateSoftwareAdapter (HMODULE Module, IDXGIAdapter **ppAdapter) { return pReal->CreateSoftwareAdapter (Module, ppAdapter); }



  virtual HRESULT STDMETHODCALLTYPE EnumAdapters1 (UINT Adapter, IDXGIAdapter1 **ppAdapter)
  { return pReal->EnumAdapters1 (Adapter, ppAdapter); }
  virtual BOOL    STDMETHODCALLTYPE IsCurrent     (void)
  { return pReal->IsCurrent (); }



  virtual BOOL    STDMETHODCALLTYPE IsWindowedStereoEnabled (void)
  { return pReal->IsWindowedStereoEnabled (); }

  virtual HRESULT STDMETHODCALLTYPE CreateSwapChainForHwnd (
                  IUnknown                         *pDevice,
                  HWND                              hWnd,
            const DXGI_SWAP_CHAIN_DESC1            *pDesc,
            const DXGI_SWAP_CHAIN_FULLSCREEN_DESC  *pFullscreenDesc,
                  IDXGIOutput                      *pRestrictToOutput,
                  IDXGISwapChain1                 **ppSwapChain )
  {
    return pReal->CreateSwapChainForHwnd (pDevice, hWnd, pDesc, pFullscreenDesc, pRestrictToOutput, ppSwapChain);
  }

  virtual HRESULT STDMETHODCALLTYPE CreateSwapChainForCoreWindow (IUnknown  *pDevice, IUnknown *pWindow,
                                               const DXGI_SWAP_CHAIN_DESC1  *pDesc,
                                                               IDXGIOutput  *pRestrictToOutput,
                                                           IDXGISwapChain1 **ppSwapChain )
  { return pReal->CreateSwapChainForCoreWindow (pDevice, pWindow, pDesc, pRestrictToOutput, ppSwapChain); }

  virtual HRESULT STDMETHODCALLTYPE GetSharedResourceAdapterLuid (HANDLE hResource, LUID *pLuid)
  { return pReal->GetSharedResourceAdapterLuid (hResource, pLuid); }
  virtual HRESULT STDMETHODCALLTYPE RegisterStereoStatusWindow (HWND WindowHandle, UINT wMsg, DWORD *pdwCookie)
  { return pReal->RegisterStereoStatusWindow (WindowHandle, wMsg, pdwCookie); }
  virtual HRESULT STDMETHODCALLTYPE RegisterStereoStatusEvent (HANDLE hEvent, DWORD *pdwCookie)
  { return pReal->RegisterStereoStatusEvent (hEvent, pdwCookie); }
  virtual void STDMETHODCALLTYPE UnregisterStereoStatus (DWORD dwCookie)
  { return pReal->UnregisterStereoStatus (dwCookie); }
  virtual HRESULT STDMETHODCALLTYPE RegisterOcclusionStatusWindow (HWND WindowHandle, UINT wMsg, DWORD *pdwCookie)
  { return pReal->RegisterOcclusionStatusWindow (WindowHandle, wMsg, pdwCookie); }
  virtual HRESULT STDMETHODCALLTYPE RegisterOcclusionStatusEvent (HANDLE hEvent, DWORD *pdwCookie)
  { return pReal->RegisterOcclusionStatusEvent  (hEvent, pdwCookie); }
  virtual void    STDMETHODCALLTYPE UnregisterOcclusionStatus    (                 DWORD dwCookie)
  { return pReal->UnregisterOcclusionStatus     (         dwCookie); }

  virtual HRESULT STDMETHODCALLTYPE CreateSwapChainForComposition(
            IUnknown               *pDevice,
      const DXGI_SWAP_CHAIN_DESC1  *pDesc,
            IDXGIOutput            *pRestrictToOutput,
            IDXGISwapChain1       **ppSwapChain)
  {
    return pReal->CreateSwapChainForComposition (pDevice, pDesc, pRestrictToOutput, ppSwapChain);
  }



  virtual UINT STDMETHODCALLTYPE GetCreationFlags (void)
  { return pReal->GetCreationFlags (); }




  virtual HRESULT STDMETHODCALLTYPE EnumAdapterByLuid (LUID AdapterLuid, REFIID riid, void **ppvAdapter)
  { return pReal->EnumAdapterByLuid (AdapterLuid, riid, ppvAdapter); }
  virtual HRESULT STDMETHODCALLTYPE EnumWarpAdapter (REFIID riid, void **ppvAdapter)
  { return pReal->EnumWarpAdapter (riid, ppvAdapter); }





  virtual HRESULT STDMETHODCALLTYPE CheckFeatureSupport (DXGI_FEATURE Feature, void *pFeatureSupportData, UINT FeatureSupportDataSize)
  { return pReal->CheckFeatureSupport (Feature, pFeatureSupportData, FeatureSupportDataSize); }


  IDXGIFactory5 *pReal;
  ULONG          refs_;
};

void SK_DXGI_HookFactory     (IDXGIFactory    *pFactory);
void SK_DXGI_HookPresentBase (IDXGISwapChain  *pSwapChain);
void SK_DXGI_HookPresent1    (IDXGISwapChain1 *pSwapChain1);
void SK_DXGI_HookDevice1     (IDXGIDevice1    *pDevice1);

void
SK_DXGI_LazyHookFactory (IDXGIFactory *pFactory)
{
  if (pFactory != nullptr)
  {
    SK_RunOnce ({
      SK_DXGI_HookFactory (pFactory);
      SK_ApplyQueuedHooks (        );
    });
  }
}

void
SK_DXGI_LazyHookPresent (IDXGISwapChain *pSwapChain)
{
  SK_RunOnce (SK_DXGI_HookSwapChain (pSwapChain));

  // This won't catch Present1 (...), but no games use that
  //   and we can deal with it later if it happens.
  SK_DXGI_HookPresentBase ((IDXGISwapChain *)pSwapChain);

  SK_ComQIPtr <IDXGISwapChain1> pSwapChain1 (pSwapChain);

  if (pSwapChain1 != nullptr)
    SK_DXGI_HookPresent1 (pSwapChain1);
}


auto _PushInitialDWMColorSpace = [](IDXGISwapChain* pSwapChain, SK_RenderBackend& rb)
{
  SK_ComPtr <IDXGIOutput>                          pOutput;
  if (SUCCEEDED (pSwapChain->GetContainingOutput (&pOutput.p)))
  {
    SK_ComQIPtr <IDXGIOutput6>
        pOutput6 (   pOutput);
    if (pOutput6 != nullptr)
    {
      DXGI_OUTPUT_DESC1                   out_desc1 = { };
      if (SUCCEEDED (pOutput6->GetDesc1 (&out_desc1)))
      {
        rb.scanout.dwm_colorspace =
          out_desc1.ColorSpace;
      }
    }
  }
};

IWrapDXGISwapChain*
SK_DXGI_WrapSwapChain ( IUnknown        *pDevice,
                        IDXGISwapChain  *pSwapChain,
                        IDXGISwapChain **ppDest,
                        DXGI_FORMAT      original_format )
{
  if (pDevice == nullptr || pSwapChain == nullptr || ppDest == nullptr)
    return nullptr;

  SK_ComPtr <IDXGISwapChain1>                    pNativeSwapChain;
  SK_slGetNativeInterface (pSwapChain, (void **)&pNativeSwapChain.p);

  SK_DXGI_HookSwapChain   (pNativeSwapChain != nullptr ?
                           pNativeSwapChain.p          :
                                 pSwapChain);

  SK_RenderBackend& rb =
    SK_GetCurrentRenderBackend ();

  SK_ComPtr   <ID3D11Device>       pNativeDev11;
  SK_ComQIPtr <ID3D11Device>       pDev11    (pDevice);
  SK_ComQIPtr <ID3D12CommandQueue> pCmdQueue (pDevice);

  IWrapDXGISwapChain* ret = nullptr;

  if (config.apis.dxgi.d3d12.hook && pCmdQueue.p != nullptr)
  {
    rb.api  = SK_RenderAPI::D3D12;

    SK_LOGi0 (
      L" + SwapChain <IDXGISwapChain> (%08" _L(PRIxPTR) L"h) wrapped using D3D12 Command Queue",
               (uintptr_t)pSwapChain
    );

    SK_ComPtr <ID3D12CommandQueue>       pNativeCmdQueue;
    SK_ComPtr <ID3D12Device>             pNativeDev12;
    SK_ComPtr <ID3D12Device>             pDev12;
    pCmdQueue->GetDevice (IID_PPV_ARGS (&pDev12.p));

    if (SK_slGetNativeInterface (pDev12, (void **)&pNativeDev12.p) == sl::Result::eOk)
        _ExchangeProxyForNative (pDev12,           pNativeDev12);

    UINT uiSize = sizeof (void *);

    if (pNativeSwapChain != nullptr)
    {
      if (SK_slGetNativeInterface (pCmdQueue, (void **)&pNativeCmdQueue.p) == sl::Result::eOk)
          _ExchangeProxyForNative (pCmdQueue,           pNativeCmdQueue);

      pSwapChain->SetPrivateData       (SKID_D3D12_SwapChainCommandQueue, uiSize, pCmdQueue);
      pNativeSwapChain->SetPrivateData (SKID_D3D12_SwapChainCommandQueue, uiSize, pCmdQueue);

      pSwapChain = pNativeSwapChain;
    }

    ret = // TODO: Put these in a list somewhere for proper destruction
      new IWrapDXGISwapChain ((ID3D11Device *)pDev12.p, pSwapChain);

    ret->SetPrivateData (SKID_D3D12_SwapChainCommandQueue, uiSize, pCmdQueue);

    rb.setDevice            (pDev12.p);
    rb.d3d12.command_queue = pCmdQueue.p;

    _d3d12_rbk->init (
      (IDXGISwapChain3 *)ret,
        rb.d3d12.command_queue.p
    );
  }

  else if ( pDev11 != nullptr )
  {
    if (SK_slGetNativeInterface (pDev11, (void **)&pNativeDev11.p) == sl::Result::eOk)
        _ExchangeProxyForNative (pDev11,           pNativeDev11);

    ret =
      new IWrapDXGISwapChain (pDev11.p, pSwapChain);

    SK_LOGi0 (
      L" + SwapChain <IDXGISwapChain> (%08" _L(PRIxPTR) L"h) wrapped using D3D11 Device",
               (uintptr_t)pSwapChain
    );

    // Setup D3D11 Device Context Late (on successive draw)
    if (rb.device.p == nullptr && SK_GetFramesDrawn () > 0)
    {
      if (rb.swapchain == nullptr)
          rb.swapchain = pSwapChain;

      SK_ComPtr <ID3D11DeviceContext> pDevCtx;
      pDev11.p->GetImmediateContext (&pDevCtx.p);
             rb.d3d11.immediate_ctx = pDevCtx;
             rb.setDevice            (pDev11.p);

      SK_LOGs0 ( L"  D3D 11  ",
                 L"Active D3D11 Device Context Established on creation of new SwapChain" );
    }

    // Stash the pointer to this device so that we can test equality on wrapped devices
    pDev11->SetPrivateData (SKID_D3D11DeviceBasePtr, sizeof (uintptr_t), pNativeDev11.p != nullptr ? pNativeDev11.p : pDev11.p);
  }

  if (ret != nullptr)
  {
    rb.swapchain = ret;
    _PushInitialDWMColorSpace (pSwapChain, rb);

    using state_cache_s = IWrapDXGISwapChain::state_cache_s;
          state_cache_s state_cache;

    SK_DXGI_GetPrivateData <state_cache_s> (ret, &state_cache);

    if (original_format != DXGI_FORMAT_R16G16B16A16_FLOAT)
      state_cache.lastNonHDRFormat = original_format;

    SK_DXGI_SetPrivateData <state_cache_s> (ret, &state_cache);

    *ppDest = (IDXGISwapChain *)ret;
  }

  return ret;
}

#include <SpecialK\render\d3d12\d3d12_interfaces.h>

IWrapDXGISwapChain*
SK_DXGI_WrapSwapChain1 ( IUnknown         *pDevice,
                         IDXGISwapChain1  *pSwapChain,
                         IDXGISwapChain1 **ppDest,
                         DXGI_FORMAT       original_format )
{
  if (pDevice == nullptr || pSwapChain == nullptr || ppDest == nullptr)
    return nullptr;

  SK_ComPtr <IDXGISwapChain1>                    pNativeSwapChain;
  SK_slGetNativeInterface (pSwapChain, (void **)&pNativeSwapChain.p);

  SK_DXGI_HookSwapChain   (pNativeSwapChain != nullptr ?
                           pNativeSwapChain.p          :
                                 pSwapChain);

  SK_RenderBackend& rb =
    SK_GetCurrentRenderBackend ();

  SK_ComPtr   <ID3D11Device>       pNativeDev11;
  SK_ComQIPtr <ID3D11Device>       pDev11    (pDevice);
  SK_ComQIPtr <ID3D12CommandQueue> pCmdQueue (pDevice);

  IWrapDXGISwapChain* ret = nullptr;

  if (config.apis.dxgi.d3d12.hook && pCmdQueue.p != nullptr)
  {
    rb.api  = SK_RenderAPI::D3D12;

    SK_LOGi0 (
      L" + SwapChain <IDXGISwapChain1> (%08" _L(PRIxPTR) L"h) wrapped using D3D12 Command Queue",
               (uintptr_t)pSwapChain
    );

    SK_ComPtr <ID3D12CommandQueue>       pNativeCmdQueue;
    SK_ComPtr <ID3D12Device>             pNativeDev12;
    SK_ComPtr <ID3D12Device>             pDev12;
    pCmdQueue->GetDevice (IID_PPV_ARGS (&pDev12.p));

    if (SK_slGetNativeInterface (pDev12, (void **)&pNativeDev12.p) == sl::Result::eOk)
        _ExchangeProxyForNative (pDev12,           pNativeDev12);

    UINT uiSize = sizeof (void *);

    if (pNativeSwapChain != nullptr)
    {
      if (SK_slGetNativeInterface (pCmdQueue, (void **)&pNativeCmdQueue.p) == sl::Result::eOk)
          _ExchangeProxyForNative (pCmdQueue,           pNativeCmdQueue);

      pSwapChain->SetPrivateData       (SKID_D3D12_SwapChainCommandQueue, uiSize, pCmdQueue);
      pNativeSwapChain->SetPrivateData (SKID_D3D12_SwapChainCommandQueue, uiSize, pCmdQueue);

      pSwapChain = pNativeSwapChain;
    }

    ret = // TODO: Put these in a list somewhere for proper destruction
      new IWrapDXGISwapChain ((ID3D11Device *)pDev12.p, pSwapChain);

    ret->SetPrivateData (SKID_D3D12_SwapChainCommandQueue, uiSize, pCmdQueue);

    rb.setDevice            (pDev12.p);
    rb.d3d12.command_queue = pCmdQueue.p;

    _d3d12_rbk->init (
      (IDXGISwapChain3 *)ret,
        rb.d3d12.command_queue.p
    );
  }

  else if ( pDev11 != nullptr )
  {
    if (SK_slGetNativeInterface (pDev11, (void **)&pNativeDev11.p) == sl::Result::eOk)
        _ExchangeProxyForNative (pDev11,           pNativeDev11);

    ret =
      new IWrapDXGISwapChain (pDev11.p, pSwapChain);

    SK_LOGi0 (
      L" + SwapChain <IDXGISwapChain1> (%08" _L(PRIxPTR) L"h) wrapped using D3D11 Device",
               (uintptr_t)pSwapChain
    );

    // Stash the pointer to this device so that we can test equality on wrapped devices
    pDev11->SetPrivateData (SKID_D3D11DeviceBasePtr, sizeof (uintptr_t), pDev11.p);

    // Setup D3D11 Device Context Late (on successive draw)
    if (rb.device.p == nullptr && SK_GetFramesDrawn () > 0)
    {
      if (rb.swapchain == nullptr)
          rb.swapchain = pSwapChain;

      SK_ComPtr <ID3D11DeviceContext> pDevCtx;
      pDev11.p->GetImmediateContext (&pDevCtx.p);
             rb.d3d11.immediate_ctx = pDevCtx;
             rb.setDevice            (pDev11.p);

      SK_LOGs0 ( L"  D3D 11  ",
                 L"Active D3D11 Device Context Established on creation of new SwapChain" );
    }
  }

  if (ret != nullptr)
  {
    rb.swapchain = ret;
    _PushInitialDWMColorSpace (pSwapChain, rb);

    using state_cache_s = IWrapDXGISwapChain::state_cache_s;
          state_cache_s state_cache;

    SK_DXGI_GetPrivateData <state_cache_s> (ret, &state_cache);

    if (original_format != DXGI_FORMAT_R16G16B16A16_FLOAT)
      state_cache.lastNonHDRFormat = original_format;

    SK_DXGI_SetPrivateData <state_cache_s> (ret, &state_cache);

    *ppDest = (IDXGISwapChain1 *)ret;
  }

  return ret;
}

#include <d3d12.h>

__declspec (noinline)
HRESULT
STDMETHODCALLTYPE
DXGIFactory_CreateSwapChain_Override (
              IDXGIFactory          *This,
  _In_        IUnknown              *pDevice,
  _In_  const DXGI_SWAP_CHAIN_DESC  *pDesc,
  _Out_       IDXGISwapChain       **ppSwapChain )
{
  auto& rb =
    SK_GetCurrentRenderBackend ();

  SK_ReleaseAssert (pDesc       != nullptr);
//SK_ReleaseAssert (ppSwapChain != nullptr); // This happens from time to time

  if (! config.render.dxgi.hooks.create_swapchain)
  {
    return
      CreateSwapChain_Original ( This, pDevice,
                                   pDesc, ppSwapChain );
  }

  if (SK_GetCallingDLL () == SK_GetModuleHandleW (L"sl.dlss_g.dll") ||
                             SK_GetModuleHandleW (L"nvngx_dlssg.dll") != nullptr)
  {
    if (__SK_HDR_16BitSwap && (! config.nvidia.dlss.allow_scrgb))
    {
      __SK_HDR_10BitSwap =  true;
      __SK_HDR_16BitSwap = false;

      SK_HDR_SetOverridesForGame (__SK_HDR_16BitSwap, __SK_HDR_10BitSwap);

      SK_ImGui_Warning (
        L"scRGB HDR has been changed to HDR10 because DLSS Frame Generation was detected."
        L"\r\n\r\n\t"
        L"If not using Frame Generation, you can turn scRGB back on by clicking scRGB in the HDR widget and this warning will not be shown again."
      );
    }
  }

  auto *pOrigDesc =
    (DXGI_SWAP_CHAIN_DESC *)pDesc;

  std::wstring iname =
    SK_UTF8ToWideChar (
      SK_GetDXGIFactoryInterface (This)
    );

  if (iname == L"{Invalid-Factory-UUID}")
  {
    SK_ReleaseAssert (! L"Factory Interface UUID Unknown");

    static bool run_once = false;
    if (! run_once)
    {
      run_once = true;
      DXGI_LOG_CALL_I3 ( iname.c_str (), L"CreateSwapChain         ",
                           L"%08" _L(PRIxPTR) L"h, %08" _L(PRIxPTR) L"h, %08"
                                  _L(PRIxPTR) L"h",
                             (uintptr_t)pDevice, (uintptr_t)pDesc,
                             (uintptr_t)ppSwapChain );
    }

    IDXGISwapChain* pTemp = nullptr;
    HRESULT         hr    =
      CreateSwapChain_Original (This, pDevice, pDesc, &pTemp);

    if (SUCCEEDED (hr) && ppSwapChain != nullptr)
    {
      *ppSwapChain = pTemp;
    }

    else if (pTemp != nullptr)
    {
      pTemp->Release ();
    }

    return hr;
  }

  HRESULT ret = E_FAIL;

  DXGI_LOG_CALL_I3 ( iname.c_str (), L"CreateSwapChain         ",
                       L"%08" _L(PRIxPTR) L"h, %08" _L(PRIxPTR) L"h, %08"
                              _L(PRIxPTR) L"h",
                         (uintptr_t)pDevice, (uintptr_t)pDesc,
                         (uintptr_t)ppSwapChain );

  // This makes no sense, so ignore it...
  if (     pDevice == nullptr ||
             pDesc == nullptr ||
       ppSwapChain == nullptr || (! SK_DXGI_IsSwapChainReal (*pDesc))
     )
  {
    DXGI_CALL ( ret,
                  CreateSwapChain_Original ( This, pDevice,
                                            pDesc, ppSwapChain ) );

    return ret;
  }

  WaitForInitDXGI ();

  auto                 orig_desc =  pDesc;
  DXGI_SWAP_CHAIN_DESC new_desc  = *pDesc;

  SK_ComQIPtr <ID3D11Device>       pD3D11Dev (pDevice);
  SK_ComQIPtr <ID3D12CommandQueue> pCmdQueue (pDevice);
  SK_ComPtr   <ID3D12Device>       pDev12;

  if (pCmdQueue != nullptr)
  {   pCmdQueue->GetDevice (IID_PPV_ARGS (&pDev12.p));

    //if ((new_desc.Flags  & DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT) == 0)
    //{    new_desc.Flags |= DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT;
    //  rb.active_traits.bImplicitlyWaitable = true;
    //}
    //
    //else
    {
      rb.active_traits.bImplicitlyWaitable = false;
    }

    SK_LOGs0 ( L"Direct3D12", L" <*> Native D3D12 SwapChain Captured" );

    if (! config.render.dxgi.allow_d3d12_footguns)
    {
      if ( config.render.framerate.buffer_count >= 0 &&
           config.render.framerate.buffer_count != sk::narrow_cast <int> (pDesc->BufferCount) )
      {
        SK_LOGs0 ( L"Direct3D12",
                   L" [-] SwapChain Buffer Count Override Disabled (due to D3D12)" );

        config.render.framerate.buffer_count = SK_NoPreference;
      }
    }
  }

  if ( config.render.framerate.buffer_count >= 0 &&
       config.render.framerate.buffer_count < sk::narrow_cast <int> (pDesc->BufferCount) )
  {
    SK_LOGi0 ( L" [-] SwapChain Buffer Count Override uses fewer "
               L"buffers than the engine requested -(OVERRIDE DISABLED)-" );

    config.render.framerate.buffer_count =
      sk::narrow_cast <int> (pDesc->BufferCount);
  }

  SK_DXGI_CreateSwapChain_PreInit (
    &new_desc,              nullptr,
     new_desc.OutputWindow, nullptr, pDevice, pDev12.p != nullptr
  );

#ifdef SK_DXGI_RECYCLE_SWAPCHAINS
  static SK_ComPtr <IDXGISwapChain> pSwapToRecycle = nullptr;

  auto descCopy = *pDesc;

  if ( SK_DXGI_IsFlipModelSwapChain (new_desc) &&
    (! SK_DXGI_IsFlipModelSwapChain (descCopy)) )
  {
    if (pSwapToRecycle != nullptr)
    {
      DXGI_SWAP_CHAIN_DESC      recycle_desc = { };
      pSwapToRecycle->GetDesc (&recycle_desc);

      if (recycle_desc.OutputWindow == new_desc.OutputWindow || IsChild (recycle_desc.OutputWindow, new_desc.OutputWindow))
      {
        SK_ComPtr <ID3D11Device>       pOriginalD3D11Dev;
        SK_ComPtr <ID3D12CommandQueue> pOriginalD3D12Queue;

        bool workable = true;

        if (pD3D11Dev.p != nullptr && (FAILED (pSwapToRecycle->GetDevice (__uuidof (ID3D11Device),       (void **)&pOriginalD3D11Dev.p))   || (!pD3D11Dev.IsEqualObject (pOriginalD3D11Dev  ))))
          workable = false;
        if (pCmdQueue.p != nullptr && (FAILED (pSwapToRecycle->GetDevice (__uuidof (ID3D12CommandQueue), (void **)&pOriginalD3D12Queue.p)) || (!pCmdQueue.IsEqualObject (pOriginalD3D12Queue))))
          workable = false;

        if (! workable)
        {
          _d3d11_rbk->release ((IDXGISwapChain *)rb.swapchain.p);
          _d3d12_rbk->release ((IDXGISwapChain *)rb.swapchain.p);

          rb.releaseOwnedResources ();
        }

        else
        {
          // Our hopefully clean SwapChain needs a facelift before recycling :)
          if ( recycle_desc.BufferDesc.Width  != pDesc->BufferDesc.Width  ||
               recycle_desc.BufferDesc.Height != pDesc->BufferDesc.Height ||
               recycle_desc.BufferDesc.Format != pDesc->BufferDesc.Format ||
               recycle_desc.BufferCount       != pDesc->BufferCount )
        {
          _d3d11_rbk->release (pSwapToRecycle.p);
          _d3d12_rbk->release (pSwapToRecycle.p);

          // This could well fail if anything references the backbufers still...
          if ( FAILED ( pSwapToRecycle->ResizeBuffers (
                          pDesc->BufferCount,
                            pDesc->BufferDesc.Width, pDesc->BufferDesc.Height,
                            pDesc->BufferDesc.Format, SK_DXGI_FixUpLatencyWaitFlag (
                              pSwapToRecycle, recycle_desc.Flags ) )
                      )
             )
          {
            if ( config.window.res.override.x != pDesc->BufferDesc.Width ||
                 config.window.res.override.y != pDesc->BufferDesc.Height )
            {
              if ( recycle_desc.BufferDesc.Width  != pDesc->BufferDesc.Width ||
                   recycle_desc.BufferDesc.Height != pDesc->BufferDesc.Height )
              {
                SK_ImGui_WarningWithTitle ( L"Engine is Leaking References and Cannot Change Resolutions\r\n\r\n\t"
                                            L">> An override has been configured, please restart the game.",
                                              L"SK Flip Model Override" );
              }

              config.window.res.override.x = pDesc->BufferDesc.Width;
              config.window.res.override.y = pDesc->BufferDesc.Height;
            }
          }

          if (recycle_desc.Flags != pDesc->Flags)
          {
            SK_LOGs0 ( L"DXGI Reuse",
                       L" Flags (%x) on SwapChain to be recycled for HWND (%p) do not match requested (%x)...",
             recycle_desc.Flags,                        pDesc->OutputWindow,
                   pDesc->Flags
            );
          }
        }

          // Add Ref because game expects to create a new object and we are
          //   returning an existing ref-counted object in its place.
                         pSwapToRecycle.p->AddRef ();
          *ppSwapChain = pSwapToRecycle.p;

          bRecycledSwapChains = true;

          if (            nullptr != pOrigDesc)
            pSwapToRecycle->GetDesc (pOrigDesc);

          return S_OK;
        }
      }

      SK_ComPtr <IDXGISwapChain> pSwapToKill =
                                 pSwapToRecycle;
      pSwapToRecycle = nullptr;
      pSwapToKill.p->Release ();
    }
  }
#endif

  pDesc = &new_desc;

  IDXGISwapChain* pTemp = nullptr;

  ret =
    CreateSwapChain_Original ( This, pDevice,
                                 pDesc, &pTemp );

  // Retry creation after releasing the Render Backend resources
  if ( ret == E_ACCESSDENIED )
  {
    SK_ReleaseAssert (pDesc->OutputWindow == rb.windows.getDevice ());

    rb.d3d11.clearState ();

    _d3d11_rbk->release ((IDXGISwapChain *)rb.swapchain.p);
    _d3d12_rbk->release ((IDXGISwapChain *)rb.swapchain.p);

    // Release our reference to the SwapChain, and hope that other overlays
    //   notice this so that clearState () flushes deferred object destruction
    if (rb.swapchain.p != nullptr)
        rb.swapchain.Release ();

    rb.d3d11.clearState ();

    rb.releaseOwnedResources ();

    ret =
      CreateSwapChain_Original ( This, pDevice,
                                   pDesc, &pTemp );

    if (FAILED (ret))
    {
      SK_DXGI_OutputDebugString ( "IDXGIFactory::CreateSwapChain (...) failed, look alive...",
                                    DXGI_INFO_QUEUE_MESSAGE_SEVERITY_ERROR );

      SK_DXGI_ReportLiveObjects (pDevice);
    }
  }

  DXGI_CALL ( ret, ret );

  if ( SUCCEEDED (ret) )
  {
    if (pTemp != nullptr)
    {
      //
      // HACK for HDR RenderTargetView issues in D3D12
      //
      if (pCmdQueue != nullptr && pDev12 != nullptr)
      {
        SK_ComPtr <ID3D12CommandQueue>                    pNativeCmdQueue;
        if (SK_slGetNativeInterface (pCmdQueue, (void **)&pNativeCmdQueue.p) == sl::Result::eOk)
            _ExchangeProxyForNative (pCmdQueue,           pNativeCmdQueue);

        pTemp->SetPrivateData (SKID_D3D12_SwapChainCommandQueue, sizeof (void *), pCmdQueue);

        SK_ComQIPtr <IDXGISwapChain3> pSwap3 (pTemp);
        SK_D3D12_HotSwapChainHook (   pSwap3, pDev12.p);

        if (rb.active_traits.bImplicitlyWaitable)
          pSwap3->SetMaximumFrameLatency (config.render.framerate.pre_render_limit > 0 ?
                                          config.render.framerate.pre_render_limit     : 2);
      }

      SK_DXGI_CreateSwapChain_PostInit (pDevice, &new_desc, &pTemp);
      SK_DXGI_WrapSwapChain            (pDevice,             pTemp,
                                               ppSwapChain, orig_desc->BufferDesc.Format);

    if (   nullptr != pOrigDesc)
        pTemp->GetDesc (pOrigDesc);
    }

#ifdef  SK_DXGI_RECYCLE_SWAPCHAINS
    if ( SK_DXGI_IsFlipModelSwapChain (new_desc) &&
      (! SK_DXGI_IsFlipModelSwapChain (descCopy)) )
    {
      if (  pCmdQueue.p    == nullptr &&
            pSwapToRecycle == nullptr &&
           ppSwapChain     != nullptr /*&& config.apis.translated != SK_RenderAPI::D3D9*/ )
      {
        pSwapToRecycle = *ppSwapChain;

        // We are going to reuse this, it needs a long and prosperous life.
        pSwapToRecycle.p->AddRef ();
      }
    }
#endif

    return ret;
  }

  SK_LOGi0 (L"SwapChain Creation Failed, trying again without overrides...");

  DXGI_CALL ( ret,
                CreateSwapChain_Original ( This, pDevice,
                                             orig_desc, &pTemp ) );

  if (pTemp != nullptr)
  {
    auto origDescCopy =
      *orig_desc;

    SK_DXGI_CreateSwapChain_PostInit (pDevice, &origDescCopy, &pTemp);
    SK_DXGI_WrapSwapChain            (pDevice,                 pTemp,
                                               ppSwapChain, orig_desc->BufferDesc.Format);
  }

  return ret;
}


HRESULT
STDMETHODCALLTYPE
DXGIFactory2_CreateSwapChainForCoreWindow_Override (
                IDXGIFactory2             *This,
     _In_       IUnknown                  *pDevice,
     _In_       IUnknown                  *pWindow,
     _In_ const DXGI_SWAP_CHAIN_DESC1     *pDesc,
 _In_opt_       IDXGIOutput               *pRestrictToOutput,
    _Out_       IDXGISwapChain1          **ppSwapChain )
{
  std::wstring iname = SK_UTF8ToWideChar (
    SK_GetDXGIFactoryInterface (This)
  );

  // Wrong prototype, but who cares right now? :P
  DXGI_LOG_CALL_I3 ( iname.c_str (),
                     L"CreateSwapChainForCoreWindow         ",
                     L"%08" _L(PRIxPTR) L"h, %08" _L(PRIxPTR) L"h, "
                     L"%08" _L(PRIxPTR) L"h", (uintptr_t)pDevice,
                                              (uintptr_t)pDesc,
                                              (uintptr_t)ppSwapChain );

  HRESULT ret = E_FAIL;

  if ( pDevice == nullptr || pWindow     == nullptr ||
       pDesc   == nullptr || ppSwapChain == nullptr ||
         iname == L"{Invalid-Factory-UUID}" )
  {
    DXGI_CALL ( ret,
                  CreateSwapChainForCoreWindow_Original (
                    This, pDevice, pWindow,
                          pDesc,   pRestrictToOutput, ppSwapChain ) );

    return ret;
  }

  auto                   orig_desc = pDesc;
  DXGI_SWAP_CHAIN_DESC1  new_desc1 =
    pDesc != nullptr ?
      *pDesc :
        DXGI_SWAP_CHAIN_DESC1 { };

  SK_ComQIPtr <ICoreWindowInterop>
                   window_interop (pWindow);

  HWND              hWndInterop = 0;
  if (SUCCEEDED (window_interop->get_WindowHandle (&hWndInterop)))
  {
    if (              game_window.hWnd == 0 ||
         (! IsWindow (game_window.hWnd) )
       )
    {
      SK_RunOnce (
        SK_InstallWindowHook (hWndInterop)
      );

      RAWINPUTDEVICE rid [2] = { };
      rid [0].hwndTarget  = hWndInterop;
      rid [0].usUsage     = HID_USAGE_GENERIC_MOUSE;
      rid [0].usUsagePage = HID_USAGE_PAGE_GENERIC;
      rid [0].dwFlags     = RIDEV_DEVNOTIFY;

      rid [1].hwndTarget  = hWndInterop;
      rid [1].usUsage     = HID_USAGE_GENERIC_KEYBOARD;
      rid [1].usUsagePage = HID_USAGE_PAGE_GENERIC;
      rid [1].dwFlags     = RIDEV_DEVNOTIFY;

      SK_RegisterRawInputDevices (rid, 2, sizeof (RAWINPUTDEVICE));
    }
  }

  SK_DXGI_CreateSwapChain_PreInit (nullptr, &new_desc1, hWndInterop, nullptr, pDevice, false);

  if (pDesc != nullptr) pDesc = &new_desc1;

  auto CreateSwapChain_Lambchop =
    [&] (void) ->
      BOOL
      {
        IDXGISwapChain1* pTemp = nullptr;

        ret = CreateSwapChainForCoreWindow_Original (
                This,
                  pDevice,
                    pWindow,
                      pDesc,
                        pRestrictToOutput,
                          &pTemp );

        // DXGI Flip Model only allows 1 chain per-HWND, release our owned resources
        //   and try this again...
        if ( ret == E_ACCESSDENIED )
        {
          SK_RenderBackend& rb =
            SK_GetCurrentRenderBackend ();

          rb.d3d11.clearState ();

          _d3d11_rbk->release ((IDXGISwapChain *)rb.swapchain.p);
          _d3d12_rbk->release ((IDXGISwapChain *)rb.swapchain.p);

          rb.d3d11.clearState ();

          rb.releaseOwnedResources ();

          ret = CreateSwapChainForCoreWindow_Original (
                  This,
                    pDevice,
                      pWindow,
                        pDesc,
                          pRestrictToOutput,
                            &pTemp );
        }

        DXGI_CALL (ret, ret);

        if ( SUCCEEDED (ret) )
        {
          if (pTemp != nullptr)
          {
            SK_DXGI_CreateSwapChain1_PostInit (pDevice, hWndInterop, &new_desc1,
                                                        nullptr,     &pTemp);
            SK_DXGI_WrapSwapChain1            (pDevice,               pTemp,
                                                        ppSwapChain, orig_desc->Format);
          }


          return TRUE;
        }

        return FALSE;
      };


  if (! CreateSwapChain_Lambchop ())
  {
    // Fallback-on-Fail
    pDesc = orig_desc;

    CreateSwapChain_Lambchop ();
  }

  return ret;
}



#define __PAIR_DEVICE_TO_CHAIN
#ifdef  __PAIR_DEVICE_TO_CHAIN
static concurrency::concurrent_unordered_map <HWND, concurrency::concurrent_unordered_map <IUnknown*, IDXGISwapChain1*>> _recyclable_d3d11;
#else
static concurrency::concurrent_unordered_map <HWND, IDXGISwapChain1*> _discarded;
static concurrency::concurrent_unordered_map <HWND, IDXGISwapChain1*> _recyclables;
#endif
IDXGISwapChain1*
SK_DXGI_GetCachedSwapChainForHwnd (HWND hWnd, IUnknown* pDevice)
{
  SK_ComQIPtr <ID3D11Device> pDev11 (pDevice);
  SK_ComQIPtr <ID3D12Device> pDev12 (pDevice);

  IDXGISwapChain1* pChain = nullptr;

#ifdef __PAIR_DEVICE_TO_CHAIN
  if ( pDev11.p != nullptr && _recyclable_d3d11.count (hWnd) )
  {
    // This is expected during NVIDIA DXGI/Vulkan interop, don't
    //   show this assertion unless debugging.
    if (config.system.log_level > 0)
    {
      SK_ReleaseAssert (_recyclable_d3d11 [hWnd].count (pDevice) > 0);
    }

    pChain = (IDXGISwapChain1 *)_recyclable_d3d11 [hWnd][pDevice];

    if (pChain != nullptr)
        pChain->AddRef ();
  }
#else
  if ( _recyclables.count (hWnd) != 0 )
  {
    pChain =
      _recyclables.at (   hWnd);
  }
#endif

  return pChain;
}

IDXGISwapChain1*
SK_DXGI_MakeCachedSwapChainForHwnd (IDXGISwapChain1* pSwapChain, HWND hWnd, IUnknown* pDevice)
{
  SK_ComQIPtr <ID3D11Device> pDev11 (pDevice);
  SK_ComQIPtr <ID3D12Device> pDev12 (pDevice);

#ifdef __PAIR_DEVICE_TO_CHAIN
  if (pDev11.p != nullptr) _recyclable_d3d11 [hWnd][pDevice] = pSwapChain;
#else
  _recyclables [hWnd] = pSwapChain;
#endif

  return pSwapChain;
}

UINT
SK_DXGI_ReleaseSwapChainOnHWnd (IDXGISwapChain1* pChain, HWND hWnd, IUnknown* pDevice)
{
  SK_ComQIPtr <ID3D11Device> pDev11 (pDevice);
  SK_ComQIPtr <ID3D12Device> pDev12 (pDevice);

#ifdef _DEBUG
  auto* pValidate =
    _recyclables [hWnd];

  assert (pValidate == pChain);
#endif

  DBG_UNREFERENCED_PARAMETER (pChain);

  UINT ret =
    std::numeric_limits <UINT>::max ();

#if 0
  _recyclables [hWnd] = nullptr;
#else
#ifdef __PAIR_DEVICE_TO_CHAIN
  if (pDev11.p != nullptr && _recyclable_d3d11 [hWnd][pDevice] != nullptr)
  {
    auto swapchain =
      _recyclable_d3d11 [hWnd][pDevice];

    ret = 0;
    _recyclable_d3d11 [hWnd][pDevice] = nullptr;

    _d3d11_rbk->release (swapchain);
  }
#else
  if (_recyclables.count (hWnd))
    ret = 0;

  _discarded [hWnd] = pChain;
#endif
#endif

  return ret;
}

std::pair < ID3D11Device   *,
            IDXGISwapChain * >
SK_D3D11_MakeCachedDeviceAndSwapChainForHwnd ( IDXGISwapChain *pSwapChain,
                                               HWND            hWnd,
                                               ID3D11Device   *pDevice );

BOOL
SK_AMD_CheckForOpenGLInterop (LPCVOID lpReturnAddr, HWND& hWnd)
{
  bool bAMDInteropOpenGL =
    (StrStrIW (SK_GetCallerName (lpReturnAddr).c_str (), L"amdxc"));
  if ( bAMDInteropOpenGL &&  config.apis.OpenGL.hook == true &&
                    SK_IsModuleLoaded (L"OpenGL32.dll") && !SK_IsModuleLoaded (L"EOSOVH-Win64-Shipping.dll") )
  {
    // Search for common Vulkan layers, if they are loaded, then assume
    //   the game is actually using Vulkan rather than OpenGL.
    if (! (config.apis.Vulkan.hook && (SK_IsModuleLoaded (
            SK_RunLHIfBitness (64, L"SteamOverlayVulkanLayer64.dll",
                                   L"SteamOverlayVulkanLayer32.dll")) ||
                                       SK_IsModuleLoaded (
            SK_RunLHIfBitness (64, L"amdvlk64.dll",
                                   L"amdvlk32.dll")))))
    {
      HWND hWndFake =
        SK_Win32_CreateDummyWindow (0);

      SK_ShowWindowAsync (
             hWndFake, SW_HIDE);
      hWnd = hWndFake;

      return TRUE;
    }
  }

  return FALSE;
}

BOOL
SK_AMD_CheckForVkInterop (LPCVOID lpReturnAddr)
{
  bool bAMDInteropVk =
    (StrStrIW (SK_GetCallerName (lpReturnAddr).c_str (), L"amdxc"));
  if ( bAMDInteropVk && config.apis.Vulkan.hook == true &&
                    SK_IsModuleLoaded (L"vulkan-1.dll") )
  {
    return TRUE;
  }

  return FALSE;
}

void
SK_NV_DisableVulkanOn12Interop (void)
{
  SK_RunOnce (
    SK_LOGi0 (L"Disabling D3D12/Vulkan Interop in favor of D3D11/Vulkan Interop.")
  );

  config.compatibility.disable_dx12_vk_interop = true;
}

HRESULT
STDMETHODCALLTYPE
DXGIFactory2_CreateSwapChainForHwnd_Override (
  IDXGIFactory2                                *This,
    _In_       IUnknown                        *pDevice,
    _In_       HWND                             hWnd,
    _In_ const DXGI_SWAP_CHAIN_DESC1           *pDesc,
_In_opt_       DXGI_SWAP_CHAIN_FULLSCREEN_DESC *pFullscreenDesc,
_In_opt_       IDXGIOutput                     *pRestrictToOutput,
   _Out_       IDXGISwapChain1                 **ppSwapChain )
{
  SK_ReleaseAssert (pDesc   != nullptr);
  SK_ReleaseAssert (pDevice != nullptr);

  if (! IsWindow (hWnd))
  {
    SK_LOGi0 (
      L"IDXGIFactory2::CreateSwapChainForHwnd (pDevice=%p, {hWnd=%x}, ...)"
      L" was passed an invalid window!",       pDevice,     hWnd);

    return E_INVALIDARG;
  }

  const bool bAMDOpenGLInterop =
    (SK_AMD_CheckForOpenGLInterop (_ReturnAddress (), hWnd));

  const bool bAMDVulkanInterop =
    (! bAMDOpenGLInterop) &&
    (    SK_AMD_CheckForVkInterop (_ReturnAddress ()));

  if (! bAMDOpenGLInterop)
  {
    void SK_Window_WaitForAsyncSetWindowLong (void);
         SK_Window_WaitForAsyncSetWindowLong ();

    auto ex_style =
      SK_GetWindowLongPtrW (hWnd, GWL_EXSTYLE),
            style =
      SK_GetWindowLongPtrW (hWnd, GWL_STYLE);

    if (ex_style & WS_EX_TOPMOST)
    {
      bool style_compatible = 
        (style & (WS_POPUP   | WS_BORDER      | WS_CAPTION     |
                  WS_SYSMENU | WS_MAXIMIZEBOX | WS_MINIMIZEBOX |
                  WS_THICKFRAME)) != 0UL;

      bool exstyle_compatible =
        (ex_style & (WS_EX_CLIENTEDGE    | WS_EX_CONTEXTHELP |
                     WS_EX_DLGMODALFRAME | WS_EX_TOOLWINDOW  |
                     WS_EX_WINDOWEDGE)) != 0UL;

      if (! (style_compatible && exstyle_compatible))
      {
        SK_LOGi0 (
          L"IDXGIFactory2::CreateSwapChainForHwnd (...) called on a window with"
          L" the extended WS_EX_TOPMOST style, which is invalid... removing style!"
        );

        SK_SetWindowLongPtrW (hWnd, GWL_EXSTYLE, ex_style & ~WS_EX_TOPMOST);

        if (SK_GetForegroundWindow () == hWnd)
             SK_RealizeForegroundWindow (hWnd);
      }
    }
  }

  if (! config.render.dxgi.hooks.create_swapchain4hwnd)
  {
    return
      CreateSwapChainForHwnd_Original ( This, pDevice, hWnd,
                                        pDesc, pFullscreenDesc,
                                          pRestrictToOutput, ppSwapChain );
  }

  IID                                                        IID_IStreamlineDXGIFactory;
  IIDFromString (L"{ADEC44E2-61F0-45C3-AD9F-1B37379284FF}", &IID_IStreamlineDXGIFactory);
  SK_ComPtr <IUnknown>                                           pStreamlineFactory;
  This->QueryInterface (IID_IStreamlineDXGIFactory,    (void **)&pStreamlineFactory.p);

  std::wstring iname = SK_UTF8ToWideChar (
    SK_GetDXGIFactoryInterface (This)
   );

  HRESULT ret = DXGI_ERROR_NOT_CURRENTLY_AVAILABLE;

  DXGI_LOG_CALL_I3 ( iname.c_str (), L"CreateSwapChainForHwnd         ",
                       L"%08" _L(PRIxPTR) L"h, %08" _L(PRIxPTR) L"h, %08"
                              _L(PRIxPTR) L"h",
                         (uintptr_t)pDevice, (uintptr_t)hWnd, (uintptr_t)pDesc );

  // This makes no sense, so ignore it...
  if ( pStreamlineFactory != nullptr ||
                   bAMDOpenGLInterop ||
                  pDevice == nullptr ||
                    pDesc == nullptr ||
              ppSwapChain == nullptr || ((! SK_DXGI_IsSwapChainReal1 (*pDesc, hWnd)) && (! bAMDVulkanInterop))
     )
  {
    DXGI_CALL ( ret,
                  CreateSwapChainForHwnd_Original ( This, pDevice, hWnd,
                                                      pDesc, pFullscreenDesc,
                                                        pRestrictToOutput, ppSwapChain ) );

    if (pStreamlineFactory)
      SK_LOGi0 (L"Ignoring call because it came from a Streamline proxy factory...");

    return ret;
  }

  WaitForInitDXGI ();

  auto& rb =
    SK_GetCurrentRenderBackend ();

  auto *pOrigDesc =
    (DXGI_SWAP_CHAIN_DESC1 *)pDesc;

  auto *pOrigFullscreenDesc =
    (DXGI_SWAP_CHAIN_FULLSCREEN_DESC *)pFullscreenDesc;

//  if (iname == L"{Invalid-Factory-UUID}")
//    return CreateSwapChainForHwnd_Original (This, pDevice, hWnd, pDesc, pFullscreenDesc, pRestrictToOutput, ppSwapChain);

  auto orig_desc1           = *pDesc;
  auto orig_fullscreen_desc =  pFullscreenDesc != nullptr ?
                              *pFullscreenDesc            : DXGI_SWAP_CHAIN_FULLSCREEN_DESC { };

  DXGI_SWAP_CHAIN_DESC1           new_desc1            = pDesc           ? *pDesc           :
                       DXGI_SWAP_CHAIN_DESC1           { };
  DXGI_SWAP_CHAIN_FULLSCREEN_DESC new_fullscreen_desc  = pFullscreenDesc ? *pFullscreenDesc :
                       DXGI_SWAP_CHAIN_FULLSCREEN_DESC { };

  pDesc           =            &new_desc1;
  pFullscreenDesc =
  pFullscreenDesc != nullptr ? &new_fullscreen_desc
                             : nullptr;

  SK_ComPtr   <ID3D12Device>       pDev12;
  SK_ComQIPtr <ID3D11Device>       pDev11    (pDevice);
  SK_ComQIPtr <ID3D12CommandQueue> pCmdQueue (pDevice);

  if (pCmdQueue != nullptr)
  {   pCmdQueue->GetDevice (IID_PPV_ARGS (&pDev12.p));

    //if ((new_desc1.Flags  & DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT) == 0)
    //{    new_desc1.Flags |= DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT;
    //  rb.active_traits.bImplicitlyWaitable = true;
    //}
    //
    //else
    {
      rb.active_traits.bImplicitlyWaitable = false;
    }

    if (config.compatibility.disable_dx12_vk_interop && pCmdQueue.p != nullptr)
    {
      SK_LOGs0 ( L"Vk Interop",
                 L" <*> Vulkan/D3D12 SwapChain Disabled" );
    }
    else
      SK_LOGs0 ( L"Direct3D12",
                 L" <*> Native D3D12 SwapChain Captured" );

    if (! config.render.dxgi.allow_d3d12_footguns)
    {
      if ( config.render.framerate.buffer_count >= 0 &&
           config.render.framerate.buffer_count != sk::narrow_cast <int> (pDesc->BufferCount) )
      {
        SK_LOGs0 ( L"Direct3D12",
                   L" [-] SwapChain Buffer Count Override Disabled (due to D3D12)" );

        config.render.framerate.buffer_count = SK_NoPreference;
      }
    }
  }

  if ( config.render.framerate.buffer_count >= 0 &&
       config.render.framerate.buffer_count < sk::narrow_cast <int> (pDesc->BufferCount) )
  {
    SK_LOGi0 ( L" [-] SwapChain Buffer Count Override uses fewer "
               L"buffers than the engine requested -(OVERRIDE DISABLED)-" );

    config.render.framerate.buffer_count =
      sk::narrow_cast <int> (pDesc->BufferCount);
  }

  // We got something that was neither D3D11 nor D3D12, uh oh!
  SK_ReleaseAssert (pDev11.p != nullptr || pDev12.p != nullptr);

  SK_TLS *pTLS =
    SK_TLS_Bottom ();

  if (pTLS != nullptr)
      pTLS->render->d3d11->ctx_init_thread = true;

  SK_DXGI_CreateSwapChain_PreInit (
    nullptr, &new_desc1, hWnd, pFullscreenDesc, pDevice, pDev12.p != nullptr
  );

  IDXGISwapChain1 *pTemp (nullptr);
  IDXGISwapChain1 *pCache = nullptr;

  // Cache (D3D11) Flip Model Chains
  if (pDev11.p != nullptr && SK_DXGI_IsFlipModelSwapEffect (new_desc1.SwapEffect)) pCache =
    SK_DXGI_GetCachedSwapChainForHwnd (
                                 hWnd, static_cast <IUnknown *> (pDev11.p)
                                      );
  if (pCache != nullptr)
  {
    bRecycledSwapChains = true;

    DXGI_CALL ( ret, S_OK );
    SK_LOGs0  ( L" DXGI 1.0 ",
                L" ### Returning Cached SwapChain for HWND previously seen" );

    //// Expect to be holding one reference at this point, or something's gone wrong!
    //SK_ReleaseAssert (pCache->AddRef  () == 2 && pCache->Release () == 1);

    SK::Framerate::GetLimiter (pCache)->reset  (true);
    *ppSwapChain       =       pCache;

    return
      ret;
  }

  pDesc = &new_desc1;

  _d3d12_rbk->drain_queue ();

  ret = (config.compatibility.disable_dx12_vk_interop && pCmdQueue.p != nullptr) ? E_ACCESSDENIED :
    CreateSwapChainForHwnd_Original ( This, pDevice, hWnd, pDesc, pFullscreenDesc,
                                        pRestrictToOutput, &pTemp );

  if ( ret == E_ACCESSDENIED )
  {
    if (config.compatibility.disable_dx12_vk_interop && pCmdQueue.p != nullptr)
      return E_NOTIMPL;

    if (config.system.log_level > 0)
      SK_ReleaseAssert (hWnd == rb.windows.getDevice ());

    rb.d3d11.clearState ();

    _d3d11_rbk->release ((IDXGISwapChain *)rb.swapchain.p);
    _d3d12_rbk->release ((IDXGISwapChain *)rb.swapchain.p);

    // Release our reference to the SwapChain, and hope that other overlays
    //   notice this so that clearState () flushes deferred object destruction
    if (rb.swapchain.p != nullptr)
        rb.swapchain.Release ();

    rb.d3d11.clearState ();

    rb.releaseOwnedResources ();

    ret =
      CreateSwapChainForHwnd_Original ( This, pDevice, hWnd, pDesc, pFullscreenDesc,
                                          pRestrictToOutput, &pTemp );

    if (FAILED (ret))
    {
      SK_DXGI_OutputDebugString ( "IDXGIFactory::CreateSwapChainForHwnd (...) failed, look alive...",
                                    DXGI_INFO_QUEUE_MESSAGE_SEVERITY_ERROR );

      SK_DXGI_ReportLiveObjects (pDevice);
    }
  }

  DXGI_CALL ( ret, ret );

  if ( SUCCEEDED (ret) )
  {
    if (pTemp != nullptr)
    {
      //
      // HACK for HDR RenderTargetView issues in D3D12
      //
      if (pCmdQueue != nullptr && pDev12 != nullptr)
      {
        SK_ComPtr <IDXGISwapChain3> pNativeSwap3;
        SK_ComPtr <ID3D12Device>    pNativeDev12;

        if (                         pDev12.p                            != nullptr &&
            SK_slGetNativeInterface (pDev12.p, (void **)&pNativeDev12.p) == sl::Result::eOk)
            _ExchangeProxyForNative (pDev12,             pNativeDev12);
        SK_ComQIPtr<IDXGISwapChain3> pSwap3 (pTemp);
        if (                         pSwap3.p                            != nullptr &&
            SK_slGetNativeInterface (pSwap3.p, (void **)&pNativeSwap3.p) == sl::Result::eOk)
            _ExchangeProxyForNative (pSwap3,             pNativeSwap3);

        SK_D3D12_HotSwapChainHook   (pSwap3, pDev12);

        if (rb.active_traits.bImplicitlyWaitable)
          pSwap3->SetMaximumFrameLatency (config.render.framerate.pre_render_limit > 0 ?
                                          config.render.framerate.pre_render_limit     : 2);
      }

      SK_DXGI_CreateSwapChain1_PostInit (pDevice, hWnd, pOrigDesc, pOrigFullscreenDesc, &pTemp);
        SK_DXGI_WrapSwapChain1          (pDevice,                                        pTemp,
                                        ppSwapChain,    orig_desc1.Format);

      if (    nullptr != pOrigDesc)
        pTemp->GetDesc1 (pOrigDesc);
      if (             nullptr != pOrigFullscreenDesc)
        pTemp->GetFullscreenDesc (pOrigFullscreenDesc);
    }

    bool bNvInterop =
      SK_Render_GetVulkanInteropSwapChainType (This) == SK_DXGI_VK_INTEROP_TYPE_NV;
    bool bAmdInterop =
      SK_Render_GetVulkanInteropSwapChainType (This) == SK_DXGI_VK_INTEROP_TYPE_AMD;

    // Cache Flip Model Chains, and Detect Vulkan/DXGI Interop
    if (SK_DXGI_IsFlipModelSwapEffect (new_desc1.SwapEffect))
    {
      if (pDev11.p != nullptr)
      {
        extern bool SK_NV_D3D11_HasInteropDevice;

        //
        // Detect NVIDIA's Interop SwapChain
        //
        if (bNvInterop || StrStrIW (SK_GetCallerName ().c_str (), L"nvoglv")   ||
                          StrStrIW (SK_GetCallerName ().c_str (), L"vulkan-1") || SK_NV_D3D11_HasInteropDevice)
        {
          UINT                                 uiFlagAsInterop = SK_DXGI_VK_INTEROP_TYPE_NV;
          (*ppSwapChain)->SetPrivateData (
            SKID_DXGI_VK_InteropSwapChain, 4, &uiFlagAsInterop
          );

          SK_LOGi0 (L"Detected a Vulkan/DXGI Interop SwapChain");

          bNvInterop = true;
        }

        // Don't cache SwapChains, NVIDIA always creates a new device
        if (! bNvInterop)
        {
          //SK_DXGI_MakeCachedSwapChainForHwnd
          //     ( *ppSwapChain, hWnd, pDev11.p );
        }
      }

      // D3D12
      else if (pCmdQueue.p != nullptr)
      {
        SK_ComPtr <ID3D12CommandQueue>                    pNativeCmdQueue;
        if (SK_slGetNativeInterface (pCmdQueue, (void **)&pNativeCmdQueue.p) == sl::Result::eOk)
            _ExchangeProxyForNative (pCmdQueue,           pNativeCmdQueue);

        (*ppSwapChain)->SetPrivateData (SKID_D3D12_SwapChainCommandQueue, sizeof (void *), pCmdQueue);

        //
        // Detect AMD's Interop SwapChain
        //
        if (bAmdInterop || StrStrIW (SK_GetCallerName ().c_str (), L"amdxc"))
        {
          UINT                                 uiFlagAsInterop = SK_DXGI_VK_INTEROP_TYPE_AMD;
          (*ppSwapChain)->SetPrivateData (
            SKID_DXGI_VK_InteropSwapChain, 4, &uiFlagAsInterop
          );

          SK_LOGi0 (L"Detected a Vulkan/DXGI Interop SwapChain");
        }

        //
        // Detect NVIDIA's Interop SwapChain (this is not its final form)
        //
        if (bNvInterop || StrStrIW (SK_GetCallerName ().c_str (), L"nvoglv") ||
                          StrStrIW (SK_GetCallerName ().c_str (), L"vulkan-1"))
        {
          UINT                                 uiFlagAsInterop = SK_DXGI_VK_INTEROP_TYPE_NV;
          (*ppSwapChain)->SetPrivateData (
            SKID_DXGI_VK_InteropSwapChain, 4, &uiFlagAsInterop
          );

          // Shortly after this, NV's driver will check colorspace support,
          //   we will fake non-support, and it will respond by creating
          //     a D3D11 SwapChain...
          SK_NV_DisableVulkanOn12Interop ();
        }
      }
    }



    return ret;
  }

  SK_LOGi0 (L"SwapChain Creation Failed, trying again without overrides...");

  DXGI_CALL ( ret, CreateSwapChainForHwnd_Original ( This, pDevice, hWnd, &orig_desc1, &orig_fullscreen_desc,
                                                       pRestrictToOutput, &pTemp ) );

  if (pTemp != nullptr)
  {
    SK_DXGI_CreateSwapChain1_PostInit (pDevice, hWnd, &orig_desc1, &orig_fullscreen_desc, &pTemp);
      SK_DXGI_WrapSwapChain1          (pDevice,                                            pTemp,
                                      ppSwapChain, orig_desc1.Format);
  }

  return ret;
}

HRESULT
STDMETHODCALLTYPE
DXGIFactory2_CreateSwapChainForComposition_Override (
               IDXGIFactory2          *This,
_In_           IUnknown               *pDevice,
_In_     const DXGI_SWAP_CHAIN_DESC1  *pDesc,
_In_opt_       IDXGIOutput            *pRestrictToOutput,
_Outptr_       IDXGISwapChain1       **ppSwapChain )
{
  SK_ReleaseAssert (pDesc != nullptr);

  //
  // This code is out of date, but SK is not expected to handle Composition SwapChains anyway
  //

  std::wstring iname =
    SK_UTF8ToWideChar (
      SK_GetDXGIFactoryInterface (This)
    );

  // Wrong prototype, but who cares right now? :P
  DXGI_LOG_CALL_I3 ( iname.c_str (),
                     L"CreateSwapChainForComposition         ",
                     L"%08" _L(PRIxPTR) L"h, %08" _L(PRIxPTR) L"h, %08" _L(PRIxPTR) L"h",
                       (uintptr_t)pDevice,
                       (uintptr_t)pDesc,
                       (uintptr_t)ppSwapChain
                   );

  HRESULT ret = DXGI_ERROR_NOT_CURRENTLY_AVAILABLE;

  if ( ppSwapChain == nullptr ||
             pDesc == nullptr ||
           pDevice == nullptr )
  {
    DXGI_CALL ( ret,
                  CreateSwapChainForComposition_Original (
                    This, pDevice, pDesc,
                      pRestrictToOutput, ppSwapChain ) );

    return ret;
  }

  DXGI_SWAP_CHAIN_DESC1           new_desc1           = {    };
  DXGI_SWAP_CHAIN_FULLSCREEN_DESC new_fullscreen_desc = {    };

  IDXGISwapChain1* pTemp = nullptr;

  if (nullptr != pDesc)
    new_desc1 = *pDesc;

  HWND hWnd = nullptr;

  SK_DXGI_CreateSwapChain_PreInit (
    nullptr, &new_desc1, hWnd,
    nullptr, pDevice, false
  );

  DXGI_CALL ( ret,
    CreateSwapChainForComposition_Original (
      This, pDevice, &new_desc1,
        pRestrictToOutput, &pTemp          )
            );

  if ( SUCCEEDED (ret) )
  {
    if (pTemp != nullptr)
    {
      SK_DXGI_CreateSwapChain1_PostInit (pDevice, hWnd, &new_desc1, &new_fullscreen_desc, &pTemp);
        SK_DXGI_WrapSwapChain1          (pDevice,                                          pTemp,
                                        ppSwapChain, new_desc1.Format);
    }

    return ret;
  }

  DXGI_CALL ( ret,
    CreateSwapChainForComposition_Original (
      This, pDevice, pDesc,
        pRestrictToOutput, ppSwapChain     )
            );

  return ret;
}

typedef enum skUndesirableVendors {
  __Microsoft = 0x1414,
  __Intel     = 0x8086
} Vendors;

void
WINAPI
SK_DXGI_AdapterOverride ( IDXGIAdapter**   ppAdapter,
                          D3D_DRIVER_TYPE* DriverType )
{
  if (ppAdapter == nullptr || DriverType == nullptr)
    return;

  if (SK_DXGI_preferred_adapter == SK_NoPreference)
    return;

  if (EnumAdapters_Original == nullptr)
  {
    WaitForInitDXGI ();

    if (EnumAdapters_Original == nullptr)
      return;
  }

  IDXGIAdapter* pGameAdapter     = (*ppAdapter);
  IDXGIAdapter* pOverrideAdapter = nullptr;
  IDXGIFactory* pFactory         = nullptr;

  HRESULT res = E_FAIL;

  if (    (*ppAdapter) != nullptr)
    res = (*ppAdapter)->GetParent (IID_PPV_ARGS (&pFactory));

  if (FAILED (res))
  {
    if (CreateDXGIFactory2_Import != nullptr)
      res = CreateDXGIFactory2 (0x0, __uuidof (IDXGIFactory1), static_cast_p2p <void> (&pFactory));

    else if (CreateDXGIFactory1_Import != nullptr)
      res = CreateDXGIFactory1 (     __uuidof (IDXGIFactory),  static_cast_p2p <void> (&pFactory));

    else
      res = CreateDXGIFactory  (     __uuidof (IDXGIFactory),  static_cast_p2p <void> (&pFactory));
  }


  if (SUCCEEDED (res))
  {
    if (pFactory != nullptr)
    {
      if ((*ppAdapter) == nullptr)
        EnumAdapters_Original (pFactory, 0, &pGameAdapter);

      DXGI_ADAPTER_DESC game_desc { };

      if (pGameAdapter != nullptr)
      {
        *ppAdapter  = pGameAdapter;
        *DriverType = D3D_DRIVER_TYPE_UNKNOWN;

        GetDesc_Original (pGameAdapter, &game_desc);
      }

      if ( SK_DXGI_preferred_adapter != SK_NoPreference &&
           SUCCEEDED (EnumAdapters_Original (pFactory, SK_DXGI_preferred_adapter, &pOverrideAdapter)) &&
                                                                        nullptr != pOverrideAdapter )
      {
        DXGI_ADAPTER_DESC override_desc;
        GetDesc_Original (pOverrideAdapter, &override_desc);

        if ( game_desc.VendorId     == Vendors::__Intel     &&
             override_desc.VendorId != Vendors::__Microsoft &&
             override_desc.VendorId != Vendors::__Intel )
        {
          dll_log->Log ( L"[   DXGI   ] !!! DXGI Adapter Override: (Using '%s' instead of '%s') !!!",
                         override_desc.Description, game_desc.Description );

          *ppAdapter = pOverrideAdapter;

          if (pGameAdapter != nullptr)
              pGameAdapter->Release ();
        }

        else
        {
          dll_log->Log ( L"[   DXGI   ] !!! DXGI Adapter Override: (Tried '%s' instead of '%s') !!!",
                        override_desc.Description, game_desc.Description );
          //SK_DXGI_preferred_adapter = SK_NoPreference;
          pOverrideAdapter->Release ();
        }
      }

      else
      {
        dll_log->Log ( L"[   DXGI   ] !!! DXGI Adapter Override Failed, returning '%s' !!!",
                         game_desc.Description );
      }

      pFactory->Release ();
    }
  }
}

HRESULT
STDMETHODCALLTYPE GetDesc2_Override (IDXGIAdapter2      *This,
                              _Out_  DXGI_ADAPTER_DESC2 *pDesc)
{
#if 0
  std::wstring iname = SK_GetDXGIAdapterInterface (This);

  DXGI_LOG_CALL_I2 (iname.c_str (), L"GetDesc2", L"%ph, %ph", This, pDesc);

  HRESULT    ret;
  DXGI_CALL (ret, GetDesc2_Original (This, pDesc));
#else
  HRESULT ret = GetDesc2_Original (This, pDesc);
#endif

  //// OVERRIDE VRAM NUMBER
  if (nvapi_init && sk::NVAPI::CountSLIGPUs () > 0)
  {
    dll_log->LogEx ( true,
      L" <> GetDesc2_Override: Looking for matching NVAPI GPU for %s...: ",
      pDesc->Description );

    DXGI_ADAPTER_DESC* match =
      sk::NVAPI::FindGPUByDXGIName (pDesc->Description);

    if (match != nullptr)
    {
      dll_log->LogEx (false, L"Success! (%s)\n", match->Description);
      pDesc->DedicatedVideoMemory = match->DedicatedVideoMemory;
    }
    else
      dll_log->LogEx (false, L"Failure! (No Match Found)\n");
  }

  pDesc->DedicatedSystemMemory =
    static_cast <SIZE_T> (config.render.dxgi.vram_budget_scale *
                          static_cast <double> (pDesc->DedicatedSystemMemory));

  pDesc->DedicatedVideoMemory =
    static_cast <SIZE_T> (config.render.dxgi.vram_budget_scale *
                          static_cast <double> (pDesc->DedicatedVideoMemory));

  pDesc->SharedSystemMemory =
    static_cast <SIZE_T> (config.render.dxgi.vram_budget_scale *
                          static_cast <double> (pDesc->SharedSystemMemory));

  return ret;
}

HRESULT
STDMETHODCALLTYPE GetDesc1_Override (IDXGIAdapter1      *This,
                              _Out_  DXGI_ADAPTER_DESC1 *pDesc)
{
#if 0
  std::wstring iname = SK_GetDXGIAdapterInterface (This);

  DXGI_LOG_CALL_I2 (iname.c_str (), L"GetDesc1", L"%ph, %ph", This, pDesc);

  HRESULT    ret;
  DXGI_CALL (ret, GetDesc1_Original (This, pDesc));
#else
  HRESULT ret = GetDesc1_Original (This, pDesc);
#endif

  //// OVERRIDE VRAM NUMBER
  if (nvapi_init && sk::NVAPI::CountSLIGPUs () > 0)
  {
    dll_log->LogEx ( true,
      L" <> GetDesc1_Override: Looking for matching NVAPI GPU for %s...: ",
      pDesc->Description );

    DXGI_ADAPTER_DESC* match =
      sk::NVAPI::FindGPUByDXGIName (pDesc->Description);

    if (match != nullptr)
    {
      dll_log->LogEx (false, L"Success! (%s)\n", match->Description);
      pDesc->DedicatedVideoMemory = match->DedicatedVideoMemory;
    }

    else
      dll_log->LogEx (false, L"Failure! (No Match Found)\n");
  }

  pDesc->DedicatedSystemMemory =
    static_cast <SIZE_T> (config.render.dxgi.vram_budget_scale *
                          static_cast <double> (pDesc->DedicatedSystemMemory));

  pDesc->DedicatedVideoMemory =
    static_cast <SIZE_T> (config.render.dxgi.vram_budget_scale *
                          static_cast <double> (pDesc->DedicatedVideoMemory));

  pDesc->SharedSystemMemory =
    static_cast <SIZE_T> (config.render.dxgi.vram_budget_scale *
                          static_cast <double> (pDesc->SharedSystemMemory));

  return ret;
}

HRESULT
STDMETHODCALLTYPE GetDesc_Override (IDXGIAdapter      *This,
                             _Out_  DXGI_ADAPTER_DESC *pDesc)
{
#if 0
  std::wstring iname = SK_GetDXGIAdapterInterface (This);

  DXGI_LOG_CALL_I2 (iname.c_str (), L"GetDesc",L"%ph, %ph", This, pDesc);

  HRESULT    ret;
  DXGI_CALL (ret, GetDesc_Original (This, pDesc));
#else
  HRESULT ret = GetDesc_Original (This, pDesc);
#endif

  //// OVERRIDE VRAM NUMBER
  if (nvapi_init && sk::NVAPI::CountSLIGPUs () > 0)
  {
    dll_log->LogEx ( true,
      L" <> GetDesc_Override: Looking for matching NVAPI GPU for %s...: ",
      pDesc->Description );

    DXGI_ADAPTER_DESC* match =
      sk::NVAPI::FindGPUByDXGIName (pDesc->Description);

    if (match != nullptr)
    {
      dll_log->LogEx (false, L"Success! (%s)\n", match->Description);
      pDesc->DedicatedVideoMemory = match->DedicatedVideoMemory;
    }

    else
    {
      dll_log->LogEx (false, L"Failure! (No Match Found)\n");
    }
  }

  if (config.system.log_level >= 1)
  {
    dll_log->Log ( L"[   DXGI   ] Dedicated Video: %zu MiB, Dedicated System: %zu MiB, "
                   L"Shared System: %zu MiB",
                     pDesc->DedicatedVideoMemory    >> 20UL,
                       pDesc->DedicatedSystemMemory >> 20UL,
                         pDesc->SharedSystemMemory  >> 20UL );
  }

  pDesc->DedicatedSystemMemory =
    static_cast <SIZE_T> (config.render.dxgi.vram_budget_scale *
                          static_cast <double> (pDesc->DedicatedSystemMemory));

  pDesc->DedicatedVideoMemory =
    static_cast <SIZE_T> (config.render.dxgi.vram_budget_scale *
                          static_cast <double> (pDesc->DedicatedVideoMemory));

  pDesc->SharedSystemMemory =
    static_cast <SIZE_T> (config.render.dxgi.vram_budget_scale *
                          static_cast <double> (pDesc->SharedSystemMemory));

  if ( SK_GetCurrentGameID () == SK_GAME_ID::Fallout4 &&
          SK_GetCallerName () == L"Fallout4.exe"  )
  {
    pDesc->DedicatedVideoMemory = pDesc->SharedSystemMemory;
  }

  return ret;
}

HRESULT
STDMETHODCALLTYPE EnumAdapters_Common (IDXGIFactory       *This,
                                       UINT                Adapter,
                              _Inout_  IDXGIAdapter      **ppAdapter,
                                       EnumAdapters_pfn    pFunc,
                                       BOOL                silent = FALSE)


{
  if (ppAdapter == nullptr || *ppAdapter == nullptr || pFunc == nullptr)
    return E_POINTER;

  int iver =
    SK_GetDXGIAdapterInterfaceVer (*ppAdapter);

  DXGI_ADAPTER_DESC desc = { };

  switch (iver)
  {
    default:
    case 2:
    {
      if (! GetDesc2_Original)
      {
        SK_ComQIPtr <IDXGIAdapter2>
            pAdapter2 (*ppAdapter);
        if (pAdapter2 != nullptr)
        {
          DXGI_VIRTUAL_HOOK (ppAdapter, 11, "(*pAdapter2)->GetDesc2",
            GetDesc2_Override, GetDesc2_Original, GetDesc2_pfn);
        }
      }
    } [[fallthrough]];

    case 1:
    {
      if (! GetDesc1_Original)
      {
        SK_ComQIPtr <IDXGIAdapter1>
            pAdapter1 (*ppAdapter);
        if (pAdapter1 != nullptr)
        {
          DXGI_VIRTUAL_HOOK (&pAdapter1.p, 10, "(*pAdapter1)->GetDesc1",
            GetDesc1_Override, GetDesc1_Original, GetDesc1_pfn);
        }
      }
    } [[fallthrough]];

    case 0:
    {
      if (! GetDesc_Original)
      {
        DXGI_VIRTUAL_HOOK (ppAdapter, 8, "(*ppAdapter)->GetDesc",
          GetDesc_Override, GetDesc_Original, GetDesc_pfn);
      }

      if (GetDesc_Original)
          GetDesc_Original ( *ppAdapter, &desc );
    } break;
  }

  // Logic to skip Intel and Microsoft adapters and return only AMD / NV
  if (*desc.Description == L'\0')//! wcsnlen (desc.Description, 128))
    dll_log->LogEx (false, L" >> Assertion failed: Zero-length adapter name!\n");

#ifdef SKIP_INTEL
  if ((desc.VendorId == Microsoft || desc.VendorId == Intel) && Adapter == 0) {
#else
  if (false)
  {
#endif
    // We need to release the reference we were just handed before
    //   skipping it.
    (*ppAdapter)->Release ();

    dll_log->LogEx (false,
      L"[   DXGI   ] >> (Host Application Tried To Enum Intel or Microsoft Adapter "
      L"as Adapter 0) -- Skipping Adapter '%s' <<\n", desc.Description);

    return (pFunc (This, Adapter + 1, ppAdapter));
  }

  if (! silent)
  {
    dll_log->LogEx (true, L"[   DXGI   ]  @ Returned Adapter %lu: '%32s' (LUID: %08X:%08X)",
      Adapter,
        desc.Description,
          desc.AdapterLuid.HighPart,
            desc.AdapterLuid.LowPart );

    //
    // Windows 8 has a software implementation, which we can detect.
    //
    SK_ComQIPtr <IDXGIAdapter1> pAdapter1 (*ppAdapter);
    // XXX

    if (pAdapter1 != nullptr)
    {
      DXGI_ADAPTER_DESC1 desc1 = { };

      if (            GetDesc1_Original != nullptr &&
           SUCCEEDED (GetDesc1_Original (pAdapter1, &desc1)) )
      {
#define DXGI_ADAPTER_FLAG_REMOTE   0x1
#define DXGI_ADAPTER_FLAG_SOFTWARE 0x2
        if (desc1.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
          dll_log->LogEx (false, L" <Software>");
        else
          dll_log->LogEx (false, L" <Hardware>");
        if (desc1.Flags & DXGI_ADAPTER_FLAG_REMOTE)
          dll_log->LogEx (false, L" [Remote]");
      }
    }

    dll_log->LogEx (false, L"\n");
  }

  if (ppAdapter != nullptr && *ppAdapter != nullptr)
  {
    void
    SK_DXGI_HookAdapter (IDXGIAdapter* pAdapter);
    SK_DXGI_HookAdapter (*ppAdapter);
  }

  return S_OK;
}

HRESULT
STDMETHODCALLTYPE EnumAdapters1_Override (IDXGIFactory1  *This,
                                          UINT            Adapter,
                                   _Out_  IDXGIAdapter1 **ppAdapter)
{
  static auto current_game =
    SK_GetCurrentGameID ();

  HRESULT ret = E_UNEXPECTED;

  static int calls = 0;

  // More generically, anything using the RE engine needs this.
  static bool silent =
    (current_game == SK_GAME_ID::ResidentEvil8 ||
     current_game == SK_GAME_ID::MonsterHunterStories2);

  if (++calls > 15 && std::exchange (silent, true) == false)
  {
    SK_LOGi0 (L"Excessive calls to EnumAdapters1, will not log future calls.");
  }

  if (! silent)
  {
    std::wstring iname = SK_UTF8ToWideChar (
      SK_GetDXGIFactoryInterface (This)
    );

    DXGI_LOG_CALL_I3 ( iname.c_str (), L"EnumAdapters1         ",
                         L"%08" _L(PRIxPTR) L"h, %u, %08" _L(PRIxPTR) L"h",
                           (uintptr_t)This, Adapter, (uintptr_t)ppAdapter );

    if (ppAdapter == nullptr)
    {
      DXGI_CALL (ret, DXGI_ERROR_INVALID_CALL);
    }

    else
    {
      DXGI_CALL (ret, EnumAdapters1_Original (This,Adapter,ppAdapter));
    }
  }

  // RE8 has a performance death wish
  else if (ppAdapter != nullptr)
  {
    if (Adapter == 0)
    {
      IDXGIAdapter1 *pAdapter1 =
        __SK_DXGI_FactoryCache.getAdapter0 (This);

      if (pAdapter1 != nullptr)
      { *ppAdapter   = pAdapter1;

        return S_OK;
      }
    }

    return
      EnumAdapters1_Original (
        This, Adapter,
            ppAdapter );
  }
#if 0
  // For games that try to enumerate all adapters until the API returns failure,
  //   only override valid adapters...
  if ( SUCCEEDED (ret) &&
       SK_DXGI_preferred_adapter != SK_NoPreference &&
       SK_DXGI_preferred_adapter != Adapter )
  {
    IDXGIAdapter1* pAdapter1 = nullptr;

    if (SUCCEEDED (EnumAdapters1_Original (This, SK_DXGI_preferred_adapter, &pAdapter1)))
    {
      dll_log->Log ( L"[   DXGI   ] (Reported values reflect user override: DXGI Adapter %lu -> %lu)",
                       Adapter, SK_DXGI_preferred_adapter );
      Adapter = SK_DXGI_preferred_adapter;

      if (pAdapter1 != nullptr)
        pAdapter1->Release ();
    }

    ret = EnumAdapters1_Original (This, Adapter, ppAdapter);
  }
#endif

  if (SUCCEEDED (ret) && ppAdapter != nullptr && (*ppAdapter) != nullptr) {
    return EnumAdapters_Common (This, Adapter, reinterpret_cast <IDXGIAdapter **>  (ppAdapter),
                                               reinterpret_cast <EnumAdapters_pfn> (EnumAdapters1_Original), silent);
  }

  return ret;
}

HRESULT
STDMETHODCALLTYPE EnumAdapters_Override (IDXGIFactory  *This,
                                         UINT           Adapter,
                                  _Out_  IDXGIAdapter **ppAdapter)
{
  std::wstring iname = SK_UTF8ToWideChar (SK_GetDXGIFactoryInterface (This));

  DXGI_LOG_CALL_I3 ( iname.c_str (), L"EnumAdapters         ",
                       L"%08" _L(PRIxPTR) L"h, %u, %08" _L(PRIxPTR) L"h",
                         (uintptr_t)This, Adapter, (uintptr_t)ppAdapter );

  HRESULT    ret;
  DXGI_CALL (ret, EnumAdapters_Original (This, Adapter, ppAdapter));

#if 0
  // For games that try to enumerate all adapters until the API returns failure,
  //   only override valid adapters...
  if ( SUCCEEDED (ret) &&
       SK_DXGI_preferred_adapter != SK_NoPreference &&
       SK_DXGI_preferred_adapter != Adapter )
  {
    IDXGIAdapter* pAdapter = nullptr;

    if (SUCCEEDED (EnumAdapters_Original (This, SK_DXGI_preferred_adapter, &pAdapter)))
    {
      dll_log->Log ( L"[   DXGI   ] (Reported values reflect user override: DXGI Adapter %lu -> %lu)",
                       Adapter, SK_DXGI_preferred_adapter );
      Adapter = SK_DXGI_preferred_adapter;

      if (pAdapter != nullptr)
        pAdapter->Release ();
    }

    ret = EnumAdapters_Original (This, Adapter, ppAdapter);
  }
#endif

  if (SUCCEEDED (ret) && ppAdapter != nullptr && (*ppAdapter) != nullptr)
  {
    return EnumAdapters_Common ( This, Adapter, ppAdapter,
                                   EnumAdapters_Original );
  }

  return ret;
}

HMODULE
SK_D3D11_GetSystemDLL (void)
{
  static HMODULE
      hModSystemD3D11  = nullptr;
  if (hModSystemD3D11 == nullptr)
  {
    wchar_t    wszPath [MAX_PATH + 2] = { };
    wcsncpy_s (wszPath, MAX_PATH, SK_GetSystemDirectory (), _TRUNCATE);
    lstrcatW  (wszPath, LR"(\d3d11.dll)");

    hModSystemD3D11 =
      SK_Modules->LoadLibraryLL (wszPath);
  }

  return hModSystemD3D11;
}

HMODULE
SK_D3D11_GetLocalDLL (void)
{
  static HMODULE
      hModLocalD3D11  = nullptr;
  if (hModLocalD3D11 == nullptr)
  {
    hModLocalD3D11 =
      SK_Modules->LoadLibraryLL (L"d3d11.dll");
  }

  return hModLocalD3D11;
}

DXGIDisableVBlankVirtualization_pfn
                           DXGIDisableVBlankVirtualization_Import  = nullptr;
DXGIDeclareAdapterRemovalSupport_pfn
                           DXGIDeclareAdapterRemovalSupport_Import = nullptr;
DXGIGetDebugInterface1_pfn DXGIGetDebugInterface1_Import           = nullptr;

HRESULT
WINAPI
DXGIDisableVBlankVirtualization (void)
{
  SK_LOG_FIRST_CALL

  if (DXGIDisableVBlankVirtualization_Import == nullptr)
  {
    SK_RunOnce (SK_BootDXGI ());
            WaitForInitDXGI ();
  }

  return DXGIDisableVBlankVirtualization_Import != nullptr ?
         DXGIDisableVBlankVirtualization_Import ()         :
         E_NOTIMPL;
}

// Special K Override (during app init)
HRESULT WINAPI SK_DXGI_DisableVBlankVirtualization (void)
{
  if (DXGIDisableVBlankVirtualization_Import != nullptr)
  {
    SK_LOGi0 (L"  Disabling Windows 11 Dynamic Refresh Rate");
  }

  HRESULT hr =
    DXGIDisableVBlankVirtualization_Import != nullptr ?
    DXGIDisableVBlankVirtualization_Import ()         :
    E_NOTIMPL;

  if (DXGIDisableVBlankVirtualization_Import != nullptr)
  {
    if (FAILED (hr))
      SK_LOGi0 (L" * Failed to Disable VBLANK Virtualization (hr=%x)", hr);
  }

  return hr;
}

HRESULT
WINAPI
DXGIDeclareAdapterRemovalSupport (void)
{
  SK_LOG_FIRST_CALL

  if (DXGIDeclareAdapterRemovalSupport_Import == nullptr)
  {
    SK_RunOnce (SK_BootDXGI ());
            WaitForInitDXGI ();
  }

  return DXGIDeclareAdapterRemovalSupport_Import != nullptr ?
         DXGIDeclareAdapterRemovalSupport_Import ()         :
         E_NOTIMPL;
}

HRESULT
WINAPI
DXGIGetDebugInterface1 ( UINT     Flags,
                         REFIID   riid,
                   _Out_ void   **pDebug )
{
  SK_LOG_FIRST_CALL

  if (pDebug == nullptr)
    return DXGI_ERROR_INVALID_CALL;

  if (DXGIGetDebugInterface1_Import == nullptr)
  {
    SK_RunOnce (SK_BootDXGI ());
            WaitForInitDXGI ();
  }

  *pDebug = nullptr;

  return DXGIGetDebugInterface1_Import != nullptr            ?
         DXGIGetDebugInterface1_Import (Flags, riid, pDebug) :
         E_NOTIMPL;
}

HRESULT
WINAPI CreateDXGIFactory (REFIID   riid,
                    _Out_ void   **ppFactory)
{
  std::string iname = SK_GetDXGIFactoryInterfaceEx  (riid);
  int         iver  = SK_GetDXGIFactoryInterfaceVer (riid);

  UNREFERENCED_PARAMETER (iver);

  DXGI_LOG_CALL_2 ( L"                    CreateDXGIFactory        ",
                    L"%hs, %08" _L(PRIxPTR) L"h",
                      iname.c_str (), (uintptr_t)ppFactory );

  if (ppFactory == nullptr)
    return E_INVALIDARG;

  *ppFactory = nullptr;

  if (SK_COMPAT_IgnoreDxDiagnCall ())
    return E_NOTIMPL;

  if (SK_COMPAT_IgnoreNvCameraCall ())
    return E_NOTIMPL;

  // Forward this through CreateDXGIFactory2 to initialize DXGI Debug
  if (config.render.dxgi.debug_layer)
    return CreateDXGIFactory2 (0x1, riid, ppFactory);

  return CreateDXGIFactory2 (0x0, riid, ppFactory);
}

#ifdef _M_IX86
#pragma comment (linker, "/export:CreateDXGIFactory1@8=_CreateDXGIFactory1@8")
#endif

HRESULT
WINAPI CreateDXGIFactory1 (REFIID   riid,
                     _Out_ void   **ppFactory)

{
  std::wstring iname = SK_UTF8ToWideChar (SK_GetDXGIFactoryInterfaceEx  (riid));
  int          iver  = SK_GetDXGIFactoryInterfaceVer (riid);

  //if (riid != IID_IDXGIFactory1 || (! __SK_DXGI_FactoryCache.isCurrent ()))
  //{
    DXGI_LOG_CALL_2 (L"                    CreateDXGIFactory1       ",
                     L"%s, %08" _L(PRIxPTR) L"h",
                     iname.c_str (), (uintptr_t)ppFactory);
  //}

  //else
  //{
  //  SK_LOG_ONCE (L"[Res Enum 8] STFU Capcom, one factory is enough!");
  //}

  if (ppFactory == nullptr)
    return E_INVALIDARG;

  *ppFactory = nullptr;

  if (SK_COMPAT_IgnoreDxDiagnCall ())
    return E_NOTIMPL;

  if (SK_COMPAT_IgnoreNvCameraCall ())
    return E_NOTIMPL;

  HRESULT hr = E_UNEXPECTED;

  // Forward this through CreateDXGIFactory2 to initialize DXGI Debug
  if (config.render.dxgi.debug_layer)
    hr = CreateDXGIFactory2 (0x1, riid, ppFactory);
  else 
    hr = CreateDXGIFactory2 (0x0, riid, ppFactory);

  if (SUCCEEDED (hr))
  {
    // Detect NVIDIA Vulkan/DXGI Interop Factories
    if (iver >= 7)
    {
      if (StrStrIW (SK_GetCallerName ().c_str (), L"nvoglv") ||
          StrStrIW (SK_GetCallerName ().c_str (), L"vulkan-1"))
      {
        SK_NV_DisableVulkanOn12Interop ();
        
        UINT                                   uiFlagAsInterop = SK_DXGI_VK_INTEROP_TYPE_NV;
          ((IDXGIFactory *)(*ppFactory))->SetPrivateData (
            SKID_DXGI_VK_InteropSwapChain, 4, &uiFlagAsInterop
          );
      }
    }

    // XXX: Does AMD call CreateDXGIFactory, CreateDXGIFactory1, CreateDXGIFactoryor 2?
    if (StrStrIW (SK_GetCallerName ().c_str (), L"amdxc"))
    {
      UINT                                   uiFlagAsInterop = SK_DXGI_VK_INTEROP_TYPE_AMD;
        ((IDXGIFactory *)(*ppFactory))->SetPrivateData (
          SKID_DXGI_VK_InteropSwapChain, 4, &uiFlagAsInterop
        );
    }
  }

  return hr;
}

thread_local bool initializing_dxgi = false;

HRESULT
WINAPI CreateDXGIFactory2 (UINT     Flags,
                           REFIID   riid_,
                     _Out_ void   **ppFactory)
{
  if (ppFactory == nullptr)
    return E_INVALIDARG;

  const bool silence =
    ( SK_GetCallingDLL () == SK_GetDLL () ||
      StrStrIW (SK_GetCallerName ().c_str (), L"d3d11.dll") );

  IID riid = riid_;

  // Upgrade everything to at least IDXGIFactory7 implicitly
  if (IsEqualGUID (riid_, IID_IDXGIFactory)  ||
      IsEqualGUID (riid_, IID_IDXGIFactory1) ||
      IsEqualGUID (riid_, IID_IDXGIFactory2) ||
      IsEqualGUID (riid_, IID_IDXGIFactory3) ||
      IsEqualGUID (riid_, IID_IDXGIFactory4) ||
      IsEqualGUID (riid_, IID_IDXGIFactory5))
  {
    switch (SK_DXGI_HighestFactorySupported)
    {
      case 7: riid = IID_IDXGIFactory5; break;
      case 6: riid = IID_IDXGIFactory5; break;
      case 5: riid = IID_IDXGIFactory5; break;
      case 4: riid = IID_IDXGIFactory4; break;
      case 3: riid = IID_IDXGIFactory3; break;
      case 2: riid = IID_IDXGIFactory2; break;
      case 1: riid = IID_IDXGIFactory1; break;
      case 0: riid = IID_IDXGIFactory;  break;
    }
  }

  if (SK_COMPAT_IgnoreDxDiagnCall ())
    return E_NOTIMPL;

  if (SK_COMPAT_IgnoreNvCameraCall ())
    return E_NOTIMPL;

  if (config.render.dxgi.debug_layer)
    Flags |= 0x1;

                                                    // 0x1 == Debug Factory
  if (config.render.dxgi.use_factory_cache && Flags != 0x1 && ppFactory != nullptr)
  {
    bool current =
      __SK_DXGI_FactoryCache.isCurrent ();

    if (current)
    {
      if (__SK_DXGI_FactoryCache.hasInterface (riid))
        return
          __SK_DXGI_FactoryCache.addRef (ppFactory, riid);
    }

    else
      __SK_DXGI_FactoryCache.reset ();
  }

  std::string iname = SK_GetDXGIFactoryInterfaceEx  (riid_);
  int         iver  = SK_GetDXGIFactoryInterfaceVer (riid_);

  UNREFERENCED_PARAMETER (iver);

  if (! silence)
  {
    DXGI_LOG_CALL_3 ( L"                    CreateDXGIFactory2       ",
                      L"0x%04X, %hs, %08" _L(PRIxPTR) L"h",
                        Flags, iname.c_str (), (uintptr_t)ppFactory );
  }

  *ppFactory = nullptr;

  if (CreateDXGIFactory2_Import == nullptr)
  {
    SK_RunOnce (SK_BootDXGI ());
            WaitForInitDXGI ();
  }

  if (! silence)
  {
    WaitForInitDXGI ();
  }

  void* pFactory_ = nullptr;

  HRESULT ret;

  // Windows 7 does not have this function -- substitute it with CreateDXGIFactory1
  if (CreateDXGIFactory2_Import == nullptr)
  {
    dll_log->Log ( L"[   DXGI   ] >> Falling back to CreateDXGIFactory1 on"
                   L" Vista/7..." );

    DXGI_CALL (ret, CreateDXGIFactory1_Import (riid, &pFactory_));
  }

  else
  {
    if (! silence) {
      DXGI_CALL (ret,  CreateDXGIFactory2_Import (Flags, riid, &pFactory_));
    } else       ret = CreateDXGIFactory2_Import (Flags, riid, &pFactory_);
  }

  if ( ppFactory == nullptr ||
       pFactory_ == nullptr)
  {
    ret = E_INVALIDARG;
  }

  if (SUCCEEDED (ret) && pFactory_ != nullptr)
  {
    SK_GetDXGIFactoryInterfaceVer ((IUnknown *)pFactory_);

#if 0
    auto newFactory =
      new SK_IWrapDXGIFactory ();

    newFactory->pReal = (IDXGIFactory5 *)pFactory_;

    *ppFactory = newFactory;
#else
   *ppFactory = pFactory_;
#endif

    SK_DXGI_LazyHookFactory ((IDXGIFactory *)*ppFactory);

    if (config.render.dxgi.use_factory_cache && Flags != 0x1) // 0x1 == Debug Factory
    {
      __SK_DXGI_FactoryCache.addFactory (ppFactory, riid);
    }
  }

  return     ret;
}

DXGI_STUB (HRESULT, DXGID3D10CreateDevice,
  (HMODULE hModule, IDXGIFactory *pFactory, IDXGIAdapter *pAdapter,
    UINT    Flags,   void         *unknown,  void         *ppDevice),
  (hModule, pFactory, pAdapter, Flags, unknown, ppDevice));

struct UNKNOWN5 {
  DWORD unknown [5];
};

DXGI_STUB (HRESULT, DXGID3D10CreateLayeredDevice,
  (UNKNOWN5 Unknown),
  (Unknown))

DXGI_STUB (SIZE_T, DXGID3D10GetLayeredDeviceSize,
  (const void *pLayers, UINT NumLayers),
  (pLayers, NumLayers))

DXGI_STUB (HRESULT, DXGID3D10RegisterLayers,
  (const void *pLayers, UINT NumLayers),
  (pLayers, NumLayers))

DXGI_STUB_ (             DXGIDumpJournal,
             (const char *szPassThrough),
                         (szPassThrough) );
DXGI_STUB (HRESULT, DXGIReportAdapterConfiguration,
             (DWORD dwUnknown),
                   (dwUnknown) );

using finish_pfn = void (WINAPI *)(void);

void
SK_DXGI_DetermineHighestSupportedFactoryVersion (void)
{
  if (SK_DXGI_HighestFactorySupported != -1)
    return;

  SK_ComPtr <IDXGIFactory7> pFactory7;
  SK_ComPtr <IDXGIFactory6> pFactory6;
  SK_ComPtr <IDXGIFactory5> pFactory5;
  SK_ComPtr <IDXGIFactory4> pFactory4;
  SK_ComPtr <IDXGIFactory3> pFactory3;
  SK_ComPtr <IDXGIFactory2> pFactory2;
  SK_ComPtr <IDXGIFactory1> pFactory1;
  SK_ComPtr <IDXGIFactory>  pFactory;

  if (     SUCCEEDED (CreateDXGIFactory2_Import (0x0, IID_IDXGIFactory7, (void **)&pFactory7.p)))
                                                SK_DXGI_HighestFactorySupported          = 7;
  else if (SUCCEEDED (CreateDXGIFactory2_Import (0x0, IID_IDXGIFactory6, (void **)&pFactory6.p)))
                                                SK_DXGI_HighestFactorySupported          = 6;
  else if (SUCCEEDED (CreateDXGIFactory2_Import (0x0, IID_IDXGIFactory5, (void **)&pFactory5.p)))
                                                SK_DXGI_HighestFactorySupported          = 5;
  else if (SUCCEEDED (CreateDXGIFactory2_Import (0x0, IID_IDXGIFactory4, (void **)&pFactory4.p)))
                                                SK_DXGI_HighestFactorySupported          = 4;
  else if (SUCCEEDED (CreateDXGIFactory2_Import (0x0, IID_IDXGIFactory3, (void **)&pFactory3.p)))
                                                SK_DXGI_HighestFactorySupported          = 3;
  else if (SUCCEEDED (CreateDXGIFactory2_Import (0x0, IID_IDXGIFactory2, (void **)&pFactory2.p)))
                                                SK_DXGI_HighestFactorySupported          = 2;
  else if (SUCCEEDED (CreateDXGIFactory2_Import (0x0, IID_IDXGIFactory1, (void **)&pFactory1.p)))
                                                SK_DXGI_HighestFactorySupported          = 1;
  else if (SUCCEEDED (CreateDXGIFactory2_Import (0x0, IID_IDXGIFactory,  (void **)&pFactory.p)))
                                                SK_DXGI_HighestFactorySupported          = 0;
  else
  {
    SK_LOGi0 (L"DXGI Factories Are Not Supported On This System...");
  }
}

void
WINAPI
SK_HookDXGI (void)
{
  // Shouldn't be calling this if hooking is turned off!
  assert (config.apis.dxgi.d3d11.hook);

  if (! (config.apis.dxgi.d3d11.hook ||
         config.apis.dxgi.d3d12.hook))
  {
    return;
  }

  static volatile LONG hooked = FALSE;

  if (! InterlockedCompareExchangeAcquire (&hooked, TRUE, FALSE))
  {
    // Serves as both D3D11 and DXGI
    bool d3d11 =
      ( SK_GetDLLRole () & DLL_ROLE::D3D11 );

    HMODULE hBackend =
      ( (SK_GetDLLRole () & DLL_ROLE::DXGI) && (! d3d11) ) ?
             backend_dll : SK_Modules->LoadLibraryLL (L"dxgi.dll");


    dll_log->Log (L"[   DXGI   ] Importing CreateDXGIFactory{1|2}");
    dll_log->Log (L"[   DXGI   ] ================================");

    if (! _wcsicmp (SK_GetModuleName (SK_GetDLL ()).c_str (), L"dxgi.dll"))
    {
      CreateDXGIFactory_Import  =  (CreateDXGIFactory_pfn)
        SK_GetProcAddress (hBackend,  "CreateDXGIFactory");
      CreateDXGIFactory1_Import =  (CreateDXGIFactory1_pfn)
        SK_GetProcAddress (hBackend,  "CreateDXGIFactory1");
      CreateDXGIFactory2_Import =  (CreateDXGIFactory2_pfn)
        SK_GetProcAddress (hBackend,  "CreateDXGIFactory2");

      DXGIDeclareAdapterRemovalSupport_Import = (DXGIDeclareAdapterRemovalSupport_pfn)
        SK_GetProcAddress (hBackend, "DXGIDeclareAdapterRemovalSupport");

      DXGIGetDebugInterface1_Import = (DXGIGetDebugInterface1_pfn)
        SK_GetProcAddress (hBackend, "DXGIGetDebugInterface1");

      SK_LOGs0 ( L" DXGI 1.0 ",
                 L"  CreateDXGIFactory:      %s",
                    SK_MakePrettyAddress (CreateDXGIFactory_Import).c_str () );
      SK_LogSymbolName                   (CreateDXGIFactory_Import);

      SK_LOGs0 ( L" DXGI 1.1 ",
                 L"  CreateDXGIFactory1:     %s",
                    SK_MakePrettyAddress (CreateDXGIFactory1_Import).c_str () );
      SK_LogSymbolName                   (CreateDXGIFactory1_Import);

      SK_LOGs0 ( L" DXGI 1.3 ",
                 L"  CreateDXGIFactory2:     %s",
                    SK_MakePrettyAddress (CreateDXGIFactory2_Import).c_str () );
      SK_LogSymbolName                   (CreateDXGIFactory2_Import);

      SK_LOGs0 ( L" DXGI 1.3 ",
                 L"  DXGIGetDebugInterface1: %s",
                    SK_MakePrettyAddress (DXGIGetDebugInterface1_Import).c_str () );
      SK_LogSymbolName                   (DXGIGetDebugInterface1_Import);

      LocalHook_CreateDXGIFactory.target.addr  = CreateDXGIFactory_Import;
      LocalHook_CreateDXGIFactory1.target.addr = CreateDXGIFactory1_Import;
      LocalHook_CreateDXGIFactory2.target.addr = CreateDXGIFactory2_Import;

      DXGIDisableVBlankVirtualization_Import = (DXGIDisableVBlankVirtualization_pfn)
      SK_GetProcAddress             (hBackend, "DXGIDisableVBlankVirtualization");
    }

    else
    {
      SK_COMPAT_CheckStreamlineSupport ();

      LPVOID pfnCreateDXGIFactory    = nullptr;
      LPVOID pfnCreateDXGIFactory1   = nullptr;
      LPVOID pfnCreateDXGIFactory2   = nullptr;

      if ( (! LocalHook_CreateDXGIFactory.active) && SK_GetProcAddress (
             hBackend, "CreateDXGIFactory" ) )
      {
        if ( MH_OK ==
               SK_CreateDLLHook2 (      L"dxgi.dll",
                                         "CreateDXGIFactory",
                                          CreateDXGIFactory,
                 static_cast_p2p <void> (&CreateDXGIFactory_Import),
                                      &pfnCreateDXGIFactory ) )
        {
          SK_EnableHook (pfnCreateDXGIFactory);
        }
      }
      else if (LocalHook_CreateDXGIFactory.active) {
        pfnCreateDXGIFactory = LocalHook_CreateDXGIFactory.target.addr;
      }

      if ( (! LocalHook_CreateDXGIFactory1.active) && SK_GetProcAddress (
             hBackend, "CreateDXGIFactory1" ) )
      {
        if ( MH_OK ==
               SK_CreateDLLHook2 (      L"dxgi.dll",
                                         "CreateDXGIFactory1",
                                          CreateDXGIFactory1,
                 static_cast_p2p <void> (&CreateDXGIFactory1_Import),
                                      &pfnCreateDXGIFactory1 ) )
        {
          SK_EnableHook (pfnCreateDXGIFactory1);
        }
      }
      else if (LocalHook_CreateDXGIFactory1.active) {
        pfnCreateDXGIFactory1 = LocalHook_CreateDXGIFactory1.target.addr;
      }

      if ( (! LocalHook_CreateDXGIFactory2.active) && SK_GetProcAddress (
             hBackend, "CreateDXGIFactory2" ) )
      {
        if ( MH_OK ==
               SK_CreateDLLHook2 (      L"dxgi.dll",
                                         "CreateDXGIFactory2",
                                          CreateDXGIFactory2,
                 static_cast_p2p <void> (&CreateDXGIFactory2_Import),
                                      &pfnCreateDXGIFactory2 ) )
        {
          SK_EnableHook (pfnCreateDXGIFactory2);
        }
      }
      else if (LocalHook_CreateDXGIFactory2.active) {
        pfnCreateDXGIFactory2 = LocalHook_CreateDXGIFactory2.target.addr;
      }

      SK_LOGs0 ( L" DXGI 1.0 ",
                 L"  CreateDXGIFactory:  %s  %s",
                    SK_MakePrettyAddress (pfnCreateDXGIFactory).c_str  (),
                                             CreateDXGIFactory_Import ?
                                                        L"{ Hooked }" :
                                                        L""   );
      SK_LogSymbolName                   (pfnCreateDXGIFactory);

      SK_LOGs0 ( L" DXGI 1.1 ",
                 L"  CreateDXGIFactory1: %s  %s",
                    SK_MakePrettyAddress (pfnCreateDXGIFactory1).c_str  (),
                                             CreateDXGIFactory1_Import ?
                                                         L"{ Hooked }" :
                                                         L""   );
      SK_LogSymbolName                   (pfnCreateDXGIFactory1);

      SK_LOGs0 ( L" DXGI 1.3 ",
                 L"  CreateDXGIFactory2: %s  %s",
                    SK_MakePrettyAddress (pfnCreateDXGIFactory2).c_str  (),
                                             CreateDXGIFactory2_Import ?
                                                         L"{ Hooked }" :
                                                         L""   );
      SK_LogSymbolName                   (pfnCreateDXGIFactory2);

      LocalHook_CreateDXGIFactory.target.addr  = pfnCreateDXGIFactory;
      LocalHook_CreateDXGIFactory1.target.addr = pfnCreateDXGIFactory1;
      LocalHook_CreateDXGIFactory2.target.addr = pfnCreateDXGIFactory2;

      DXGIDisableVBlankVirtualization_Import = (DXGIDisableVBlankVirtualization_pfn)
      SK_GetProcAddress          (L"dxgi.dll", "DXGIDisableVBlankVirtualization");
    }

    static auto _InitDXGIFactoryInterfaces = [&](void)
    {
      SK_DXGI_DetermineHighestSupportedFactoryVersion ();

      static const IID iids [] = { IID_IDXGIFactory,  IID_IDXGIFactory1, IID_IDXGIFactory2,
                                   IID_IDXGIFactory3, IID_IDXGIFactory4, IID_IDXGIFactory5,
                                   IID_IDXGIFactory6, IID_IDXGIFactory7 };

      initializing_dxgi = true;
      {
        // Invoke our hook, but do not -wait- for it's initialization check
        SK_ComPtr <IDXGIFactory2>  pFactory;
        CreateDXGIFactory2 ( 0x0,
               iids [std::clamp (SK_DXGI_HighestFactorySupported, 0, 7)],
                         (void **)&pFactory.p );
      }
      initializing_dxgi = false;
    };

    // This is going to fail if performed from DllMain
    if (! SK_TLS_Bottom ()->debug.in_DllMain)
    {
      _InitDXGIFactoryInterfaces ();
    }

    else
    {
      // Thus we need to use a secondary thread
      HANDLE hSecondaryThread =
        SK_Thread_CreateEx ([](LPVOID)->DWORD
        {
          SK_AutoCOMInit _autocom;
          _InitDXGIFactoryInterfaces ();

          return 0;
        });

      // And ... wait briefly on that thread, the timeout is critical
      //   because this could deadlock otherwise.
      if (hSecondaryThread != 0)
      {
        WaitForSingleObject (hSecondaryThread, 500UL);
        SK_CloseHandle      (hSecondaryThread);
      }
    }

    initializing_dxgi = true;

    if (config.apis.dxgi.d3d11.hook)
    {
      SK_D3D11_InitTextures ();
      SK_D3D11_Init         ();
    }

    SK_ICommandProcessor *pCommandProc = nullptr;

    SK_RunOnce (
      pCommandProc =
        SK_Render_InitializeSharedCVars ()
    );

    if (pCommandProc != nullptr)
    {
      pCommandProc->AddVariable
       ( "SwapChainWait", SK_CreateVar ( SK_IVariable::Int,
           &config.render.framerate.swapchain_wait ) );

      pCommandProc->AddVariable
       ( "UseFlipDiscard", SK_CreateVar ( SK_IVariable::Boolean,
           &config.render.framerate.flip_discard ) );
    }

    SK_DXGI_BeginHooking ();

    initializing_dxgi = false;

    InterlockedIncrementRelease (&hooked);
  }

  if (! initializing_dxgi)
    SK_Thread_SpinUntilAtomicMin (&hooked, 2);
}

void
WINAPI
dxgi_init_callback (finish_pfn finish)
{
  if (finish == nullptr)
    return;

  if (! SK_IsHostAppSKIM ())
  {
    SK_BootDXGI ();
    do
    {
      if (! SwitchToThread ())
      {
        static UINT
              spin_count = 0;
        if (++spin_count > 100)
          break;
      }
    } while (! ReadAcquire (&__dxgi_ready));
  }

  finish ();
}

bool
SK::DXGI::Startup (void)
{
  if (SK_GetDLLRole () & DLL_ROLE::D3D11)
    return SK_StartupCore (L"d3d11", dxgi_init_callback);

  return SK_StartupCore (L"dxgi", dxgi_init_callback);
}

void
SK_DXGI_HookPresentBase (IDXGISwapChain* pSwapChain)
{
  if (pSwapChain == nullptr)
    return;

  if (! LocalHook_IDXGISwapChain_Present.active)
  {
    DXGI_VIRTUAL_HOOK ( &pSwapChain, 8,
                        "IDXGISwapChain::Present",
                         PresentCallback,
                         Present_Original,
                         PresentSwapChain_pfn );

    SK_Hook_TargetFromVFTable   (
      LocalHook_IDXGISwapChain_Present,
        (void **)&pSwapChain, 8 );
  }
}

HRESULT
WINAPI
IDXGISwapChain4_SetHDRMetaData ( IDXGISwapChain4*        This,
                        _In_     DXGI_HDR_METADATA_TYPE  Type,
                        _In_     UINT                    Size,
                        _In_opt_ void                   *pMetaData )
{
  SK_LOG_FIRST_CALL

  auto                    orig_type = Type;
  DXGI_HDR_METADATA_HDR10 metadata  = {};

  if (config.render.dxgi.hdr_metadata_override >= 0)
  {
    SK_LOGi0 (L"Invalid HDR metadata override, ignoring...");
  }

  else
  {
    if (config.render.dxgi.hdr_metadata_override == -2)
      return S_OK;
  }

  if (Type == DXGI_HDR_METADATA_TYPE_NONE || (Size == sizeof (DXGI_HDR_METADATA_HDR10) && Type == DXGI_HDR_METADATA_TYPE_HDR10))
  {
    if (Size == sizeof (DXGI_HDR_METADATA_HDR10) && Type == DXGI_HDR_METADATA_TYPE_HDR10)
    {
      metadata =
        *(DXGI_HDR_METADATA_HDR10 *)pMetaData;

      SK_LOGi0 (
        L"HDR Metadata: Max Mastering=%d nits, Min Mastering=%f nits, MaxCLL=%d nits, MaxFALL=%d nits",
        metadata.MaxMasteringLuminance, (double)metadata.MinMasteringLuminance * 0.0001,
        metadata.MaxContentLightLevel,          metadata.MaxFrameAverageLightLevel
      );
    }

    if (config.render.dxgi.hdr_metadata_override == -1)
    {
      auto& rb =
        SK_GetCurrentRenderBackend ();

      auto& display =
        rb.displays [rb.active_display];

      if (display.gamut.maxLocalY == 0.0f)
      {
        This->SetFullscreenState (FALSE, nullptr);

        // Make sure we're not screwed over by NVIDIA Streamline
        SK_ComPtr <IDXGIOutput>                      pOutput;
        SK_ComPtr <IDXGISwapChain4>                  pNativeSwap4;
        if (SK_slGetNativeInterface (This, (void **)&pNativeSwap4.p) == sl::Result::eOk)
                                                     pNativeSwap4->GetContainingOutput (&pOutput.p);
        else                                                 This->GetContainingOutput (&pOutput.p);

        SK_ComQIPtr <IDXGIOutput6>
            pOutput6 (   pOutput);
        if (pOutput6.p != nullptr)
        {
          DXGI_OUTPUT_DESC1    outDesc1 = { };
          pOutput6->GetDesc1 (&outDesc1);

          display.gamut.maxLocalY   = outDesc1.MaxLuminance;
          display.gamut.maxAverageY = outDesc1.MaxFullFrameLuminance;
          display.gamut.maxY        = outDesc1.MaxLuminance;
          display.gamut.minY        = outDesc1.MinLuminance;
          display.gamut.xb          = outDesc1.BluePrimary  [0];
          display.gamut.yb          = outDesc1.BluePrimary  [1];
          display.gamut.xg          = outDesc1.GreenPrimary [0];
          display.gamut.yg          = outDesc1.GreenPrimary [1];
          display.gamut.xr          = outDesc1.RedPrimary   [0];
          display.gamut.yr          = outDesc1.RedPrimary   [1];
          display.gamut.Xw          = outDesc1.WhitePoint   [0];
          display.gamut.Yw          = outDesc1.WhitePoint   [1];
          display.gamut.Zw          = 1.0f - display.gamut.Xw - display.gamut.Yw;

          SK_ReleaseAssert (outDesc1.Monitor == display.monitor || display.monitor == 0);
        }
      }

      metadata.MinMasteringLuminance     = sk::narrow_cast <UINT>   (display.gamut.minY / 0.0001);
      metadata.MaxMasteringLuminance     = sk::narrow_cast <UINT>   (display.gamut.maxY);
      metadata.MaxContentLightLevel      = sk::narrow_cast <UINT16> (display.gamut.maxLocalY);
      metadata.MaxFrameAverageLightLevel = sk::narrow_cast <UINT16> (display.gamut.maxAverageY);

      metadata.BluePrimary  [0]          = sk::narrow_cast <UINT16> (display.gamut.xb * 50000.0F);
      metadata.BluePrimary  [1]          = sk::narrow_cast <UINT16> (display.gamut.yb * 50000.0F);
      metadata.RedPrimary   [0]          = sk::narrow_cast <UINT16> (display.gamut.xr * 50000.0F);
      metadata.RedPrimary   [1]          = sk::narrow_cast <UINT16> (display.gamut.yr * 50000.0F);
      metadata.GreenPrimary [0]          = sk::narrow_cast <UINT16> (display.gamut.xg * 50000.0F);
      metadata.GreenPrimary [1]          = sk::narrow_cast <UINT16> (display.gamut.yg * 50000.0F);
      metadata.WhitePoint   [0]          = sk::narrow_cast <UINT16> (display.gamut.Xw * 50000.0F);
      metadata.WhitePoint   [1]          = sk::narrow_cast <UINT16> (display.gamut.Yw * 50000.0F);

      SK_RunOnce (
        SK_LOGi0 (
          L"Metadata Override: Max Mastering=%d nits, Min Mastering=%f nits, MaxCLL=%d nits, MaxFALL=%d nits",
          metadata.MaxMasteringLuminance, (double)metadata.MinMasteringLuminance * 0.0001,
          metadata.MaxContentLightLevel,          metadata.MaxFrameAverageLightLevel
        )
      );

      if (Size == sizeof (DXGI_HDR_METADATA_HDR10) && Type == DXGI_HDR_METADATA_TYPE_HDR10)
        *(DXGI_HDR_METADATA_HDR10 *)pMetaData = metadata;
      else
      {
        pMetaData = &metadata;
        Size      = sizeof (DXGI_HDR_METADATA_HDR10);
        Type      = DXGI_HDR_METADATA_TYPE_HDR10;
      }
    }
  }

  DXGI_SWAP_CHAIN_DESC swapDesc = { };
  This->GetDesc      (&swapDesc);

  HRESULT hr =
    IDXGISwapChain4_SetHDRMetaData_Original (This, Type, Size, pMetaData);

  //SK_HDR_GetControl ()->meta._AdjustmentCount++;

  if (SUCCEEDED (hr) && orig_type == DXGI_HDR_METADATA_TYPE_HDR10)
  {
    // HDR requires Fullscreen Exclusive or DXGI Flip Model, this should never succeed
    if (config.system.log_level > 0)
    {
      SK_ReleaseAssert (
        SK_DXGI_IsFlipModelSwapChain (swapDesc) ||
                                      swapDesc.Windowed == FALSE );
    }

    if (Size == sizeof (DXGI_HDR_METADATA_HDR10))
    {
      if ( SK_DXGI_IsFlipModelSwapChain (swapDesc) ||
                                         swapDesc.Windowed == FALSE )
      {
        // Obsolete, go by calls to SetColorSpace1 instead...
        ////rb.framebuffer_flags |= SK_FRAMEBUFFER_FLAG_HDR;
      }
    }
  }

  return hr;
}


const char*
DXGIColorSpaceToStr (DXGI_COLOR_SPACE_TYPE space) noexcept
{
  switch (space)
  {
    case DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709:
      return "DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709";
    case DXGI_COLOR_SPACE_RGB_FULL_G10_NONE_P709:
      return "DXGI_COLOR_SPACE_RGB_FULL_G10_NONE_P709";
    case DXGI_COLOR_SPACE_RGB_STUDIO_G22_NONE_P709:
      return "DXGI_COLOR_SPACE_RGB_STUDIO_G22_NONE_P709";
    case DXGI_COLOR_SPACE_RGB_STUDIO_G22_NONE_P2020:
      return "DXGI_COLOR_SPACE_RGB_STUDIO_G22_NONE_P2020";
    case DXGI_COLOR_SPACE_RESERVED:
      return "DXGI_COLOR_SPACE_RESERVED";
    case DXGI_COLOR_SPACE_YCBCR_FULL_G22_NONE_P709_X601:
      return "DXGI_COLOR_SPACE_YCBCR_FULL_G22_NONE_P709_X601";
    case DXGI_COLOR_SPACE_YCBCR_STUDIO_G22_LEFT_P601:
      return "DXGI_COLOR_SPACE_YCBCR_STUDIO_G22_LEFT_P601";
    case DXGI_COLOR_SPACE_YCBCR_FULL_G22_LEFT_P601:
      return "DXGI_COLOR_SPACE_YCBCR_FULL_G22_LEFT_P601";
    case DXGI_COLOR_SPACE_YCBCR_STUDIO_G22_LEFT_P709:
      return "DXGI_COLOR_SPACE_YCBCR_STUDIO_G22_LEFT_P709";
    case DXGI_COLOR_SPACE_YCBCR_FULL_G22_LEFT_P709:
      return "DXGI_COLOR_SPACE_YCBCR_FULL_G22_LEFT_P709";
    case DXGI_COLOR_SPACE_YCBCR_STUDIO_G22_LEFT_P2020:
      return "DXGI_COLOR_SPACE_YCBCR_STUDIO_G22_LEFT_P2020";
    case DXGI_COLOR_SPACE_YCBCR_FULL_G22_LEFT_P2020:
      return "DXGI_COLOR_SPACE_YCBCR_FULL_G22_LEFT_P2020";
    case DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020:
      return "DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020";
    case DXGI_COLOR_SPACE_YCBCR_STUDIO_G2084_LEFT_P2020:
      return "DXGI_COLOR_SPACE_YCBCR_STUDIO_G2084_LEFT_P2020";
    case DXGI_COLOR_SPACE_RGB_STUDIO_G2084_NONE_P2020:
      return "DXGI_COLOR_SPACE_RGB_STUDIO_G2084_NONE_P2020";
    case DXGI_COLOR_SPACE_YCBCR_STUDIO_G22_TOPLEFT_P2020:
      return "DXGI_COLOR_SPACE_YCBCR_STUDIO_G22_TOPLEFT_P2020";
    case DXGI_COLOR_SPACE_YCBCR_STUDIO_G2084_TOPLEFT_P2020:
      return "DXGI_COLOR_SPACE_YCBCR_STUDIO_G2084_TOPLEFT_P2020";
    case DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P2020:
      return "DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P2020";
    case DXGI_COLOR_SPACE_YCBCR_STUDIO_GHLG_TOPLEFT_P2020:
      return "DXGI_COLOR_SPACE_YCBCR_STUDIO_GHLG_TOPLEFT_P2020";
    case DXGI_COLOR_SPACE_YCBCR_FULL_GHLG_TOPLEFT_P2020:
      return "DXGI_COLOR_SPACE_YCBCR_FULL_GHLG_TOPLEFT_P2020";
    case DXGI_COLOR_SPACE_RGB_STUDIO_G24_NONE_P709:
      return "DXGI_COLOR_SPACE_RGB_STUDIO_G24_NONE_P709";
    case DXGI_COLOR_SPACE_RGB_STUDIO_G24_NONE_P2020:
      return "DXGI_COLOR_SPACE_RGB_STUDIO_G24_NONE_P2020";
    case DXGI_COLOR_SPACE_YCBCR_STUDIO_G24_LEFT_P709:
      return "DXGI_COLOR_SPACE_YCBCR_STUDIO_G24_LEFT_P709";
    case DXGI_COLOR_SPACE_YCBCR_STUDIO_G24_LEFT_P2020:
      return "DXGI_COLOR_SPACE_YCBCR_STUDIO_G24_LEFT_P2020";
    case DXGI_COLOR_SPACE_YCBCR_STUDIO_G24_TOPLEFT_P2020:
      return "DXGI_COLOR_SPACE_YCBCR_STUDIO_G24_TOPLEFT_P2020";
    case DXGI_COLOR_SPACE_CUSTOM:
      return "DXGI_COLOR_SPACE_CUSTOM";
    default:
      return "Unknown?!";
  };
};

HRESULT
WINAPI
IDXGISwapChain3_CheckColorSpaceSupport_Override (
        IDXGISwapChain3       *This,
  _In_  DXGI_COLOR_SPACE_TYPE  ColorSpace,
  _Out_ UINT                  *pColorSpaceSupported )
{
  // Avoid log spam in some games that abuse this API
  static bool bSkipLogSpam =
    SK_GetCurrentRenderBackend ().windows.capcom;

  static int
        calls = 0;
  if (++calls > 15 && std::exchange (bSkipLogSpam, true) == false)
  {
    SK_LOGi0 (L"Excessive calls to CheckColorSpaceSupport, will not log future calls.");
  }

  if (! bSkipLogSpam)
  {
    SK_LOGi0 ( "[!] IDXGISwapChain3::CheckColorSpaceSupport (%hs) ",
                     DXGIColorSpaceToStr (ColorSpace) );
  }

  if (pColorSpaceSupported == nullptr)
    return DXGI_ERROR_INVALID_CALL;

  // NVIDIA will fallback to a D3D11 SwapChain for interop if we tell it that a colorspace is unsupported.
  if (config.compatibility.disable_dx12_vk_interop || StrStrIW (SK_GetCallerName ().c_str (), L"nvoglv") || StrStrIW (SK_GetCallerName ().c_str (), L"vulkan-1"))
  {   config.compatibility.disable_dx12_vk_interop = true; // Set this so it will trigger even if called by something wrapping the SwapChain
    SK_ComPtr <ID3D11Device> pDevice11;
    if (FAILED (This->GetDevice (IID_ID3D11Device, (void **)&pDevice11.p)))
    {
      *pColorSpaceSupported = 0x0;
      return S_OK;
    }
  }

  // SK has a trick where it can override PQ to scRGB, but often when that
  //   mode is active, the game's going to fail this call. We need to lie (!)
  if ( __SK_HDR_16BitSwap &&
               ColorSpace == DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020 )
               ColorSpace  = DXGI_COLOR_SPACE_RGB_FULL_G10_NONE_P709;

  HRESULT hr =
    IDXGISwapChain3_CheckColorSpaceSupport_Original (
      This,
        ColorSpace,
          pColorSpaceSupported
    );

  if (SUCCEEDED (hr))
  {
    if (config.render.dxgi.hide_hdr_support)
    {
      if (SK_GetCallingDLL () != SK_GetDLL ())
      {
        if (ColorSpace == DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020 ||
            ColorSpace == DXGI_COLOR_SPACE_RGB_FULL_G10_NONE_P709)
        {
          *pColorSpaceSupported = 0x0;
          return hr;
        }
      }
    }

    if (! bSkipLogSpam)
    {
      if ( *pColorSpaceSupported & DXGI_SWAP_CHAIN_COLOR_SPACE_SUPPORT_FLAG_PRESENT )
        SK_LOGs0 ( L" DXGI HDR ", L"[#] Color Space Supported." );
      if ( *pColorSpaceSupported & DXGI_SWAP_CHAIN_COLOR_SPACE_SUPPORT_FLAG_OVERLAY_PRESENT )
        SK_LOGs0 ( L" DXGI HDR ", L"[#] Overlay Supported."     );
    }
  }

  return hr;
}

HRESULT
STDMETHODCALLTYPE
SK_DXGISwap3_SetColorSpace1_Impl (
  IDXGISwapChain3       *pSwapChain3,
  DXGI_COLOR_SPACE_TYPE  ColorSpace,
  BOOL                   bWrapped = FALSE,
  void                  *pCaller  = nullptr
)
{
  const auto RequestedColorSpace = ColorSpace;

  //
  // Log Spam Prevention
  //
  static int                   calls    = 0;
  static bool                  silent   = false;
  static DXGI_COLOR_SPACE_TYPE last_val = ColorSpace;

  if (std::exchange (last_val, ColorSpace) != ColorSpace)
  {
    silent = false;
    calls  = 0;
  }

  else if (++calls > 4 && std::exchange (silent, true) == false)
  {
    SK_LOGi0 (
      L"Excessive calls to SetColorSpace1 (%hs), will not log future calls using the same value.",
         DXGIColorSpaceToStr (ColorSpace)
    );
  }

  if (! silent)
  {
    SK_LOGi0 ( L"[!] IDXGISwapChain3::SetColorSpace1 (%hs)\t[%ws]",
                    DXGIColorSpaceToStr (ColorSpace), SK_GetCallerName ().c_str () );
  }

  DXGI_SWAP_CHAIN_DESC   swapDesc = { };
  pSwapChain3->GetDesc (&swapDesc);

  //
  // Special actions to turn SK HDR features on when SK HDR is turned off,
  //   and the game engages native HDR.
  //
  if (SK_GetCallingDLL (pCaller) != SK_GetDLL ())
  {
    const bool sk_is_overriding_hdr =
      (__SK_HDR_UserForced);

    void SK_HDR_RunWidgetOnce (void);
         SK_HDR_RunWidgetOnce ();

    if (ColorSpace == DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020)
    {
      if (! sk_is_overriding_hdr)
      {
        if (swapDesc.BufferDesc.Format != DXGI_FORMAT_R16G16B16A16_FLOAT)
        {
          __SK_HDR_16BitSwap = false;
          __SK_HDR_10BitSwap =  true;

          __SK_HDR_Preset       = 3;

          SK_HDR_RunWidgetOnce ();

          __SK_HDR_tonemap      = SK_HDR_TONEMAP_RAW_IMAGE;
          __SK_HDR_Content_EOTF = 2.2f;
        }
      }
    }

    else if (ColorSpace == DXGI_COLOR_SPACE_RGB_FULL_G10_NONE_P709)
    {
      if (swapDesc.BufferDesc.Format == DXGI_FORMAT_R16G16B16A16_FLOAT)
      {
        if (! sk_is_overriding_hdr)
        {
          __SK_HDR_10BitSwap = false;
          __SK_HDR_16BitSwap = true;

          __SK_HDR_Preset       = 2;

          SK_HDR_RunWidgetOnce ();

          __SK_HDR_tonemap      = SK_HDR_TONEMAP_RAW_IMAGE;
          __SK_HDR_Content_EOTF = 1.0f;
        }
      }
    }

    else
    {
      if (! sk_is_overriding_hdr)
      {
        __SK_HDR_16BitSwap = false;
        __SK_HDR_10BitSwap = false;
      }
    }
  }

  SK_RenderBackend& rb =
    SK_GetCurrentRenderBackend ();

  if ( rb.scanout.colorspace_override != DXGI_COLOR_SPACE_CUSTOM &&
                           ColorSpace != rb.scanout.colorspace_override )
  {
    // Only do scRGB colorspace overrides if we're actually in FP16
    if (swapDesc.BufferDesc.Format == DXGI_FORMAT_R16G16B16A16_FLOAT)
    {
      if (__SK_HDR_16BitSwap)
      {
        ColorSpace = rb.scanout.colorspace_override;
      }
    }

    // HDR10 Colorspace actually works on 8-bit SwapChains.
    // 
    //  * It's undocumented, but if we ever encounter this scenario, allow it.
    //
    else
    {
      // Do not apply scRGB in HDR10
      if (rb.scanout.colorspace_override !=
            DXGI_COLOR_SPACE_RGB_FULL_G10_NONE_P709)
      {
        if (__SK_HDR_10BitSwap)
        {
          ColorSpace = rb.scanout.colorspace_override;
        }
      }
    }

    if (ColorSpace == rb.scanout.colorspace_override)
    {
      SK_LOGi0 (
        L" >> HDR: Overriding Original Color Space: '%hs' with '%hs'",
          DXGIColorSpaceToStr (RequestedColorSpace),
          DXGIColorSpaceToStr ((DXGI_COLOR_SPACE_TYPE)rb.scanout.colorspace_override)
      );
    }
  }

  if ( __SK_HDR_16BitSwap &&
         swapDesc.BufferDesc.Format == DXGI_FORMAT_R16G16B16A16_FLOAT
                    && ( ColorSpace == DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020 ||
                         ColorSpace == DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709 ) )
  {                      ColorSpace  = DXGI_COLOR_SPACE_RGB_FULL_G10_NONE_P709;
    SK_LOGs0 ( L" DXGI HDR ",
               L"Game tried to use the wrong color space (HDR10), using scRGB instead." );
  }

  if ( __SK_HDR_10BitSwap &&
         swapDesc.BufferDesc.Format != DXGI_FORMAT_R16G16B16A16_FLOAT
                    && ( ColorSpace == DXGI_COLOR_SPACE_RGB_FULL_G10_NONE_P709) )
  {                      ColorSpace  = DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020;
    SK_LOGs0 ( L" DXGI HDR ",
               L"Game tried to use the wrong color space (scRGB), using HDR10 instead." );
  }

  HRESULT hr =                      bWrapped ?
    pSwapChain3->SetColorSpace1 (ColorSpace) :
              IDXGISwapChain3_SetColorSpace1_Original
              (   pSwapChain3,   ColorSpace );

  if (SUCCEEDED (hr))
  {
    config.render.hdr.last_used_colorspace = ColorSpace;
    config.utility.save_async ();

    // {018B57E4-1493-4953-ADF2-DE6D99CC05E5}
    static constexpr GUID SKID_SwapChainColorSpace =
    { 0x18b57e4, 0x1493, 0x4953, { 0xad, 0xf2, 0xde, 0x6d, 0x99, 0xcc, 0x5, 0xe5 } };

    UINT uiColorSpaceSize = sizeof (DXGI_COLOR_SPACE_TYPE);
    
    pSwapChain3->SetPrivateData (SKID_SwapChainColorSpace, uiColorSpaceSize, &ColorSpace);

    rb.scanout.dxgi_colorspace = ColorSpace;

    // Assume SDR (sRGB)
    rb.framebuffer_flags &= ~SK_FRAMEBUFFER_FLAG_HDR;

    // HDR
    if ( ColorSpace == DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020 ||
         ColorSpace == DXGI_COLOR_SPACE_RGB_FULL_G10_NONE_P709    ||
         ColorSpace == DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P2020 )
    {
      // scRGB (Gamma 1.0 & Primaries 709) requires 16-bpc FP color
      if ( ColorSpace                 != DXGI_COLOR_SPACE_RGB_FULL_G10_NONE_P709 ||
           swapDesc.BufferDesc.Format == DXGI_FORMAT_R16G16B16A16_FLOAT )
      {
        rb.setHDRCapable (true);
        rb.framebuffer_flags |= SK_FRAMEBUFFER_FLAG_HDR;
      }
    }
  }

  // In the failure case, just hide it from the game...
  if (__SK_HDR_16BitSwap || __SK_HDR_10BitSwap)
  {
    hr = S_OK;
  }

  return hr;
}

HRESULT
STDMETHODCALLTYPE
IDXGISwapChain3_SetColorSpace1_Override (
  IDXGISwapChain3       *This,
  DXGI_COLOR_SPACE_TYPE  ColorSpace )
{
  return
    SK_DXGISwap3_SetColorSpace1_Impl (This, ColorSpace, FALSE, _ReturnAddress ());
}

using IDXGIOutput6_GetDesc1_pfn = HRESULT (WINAPI *)
     (IDXGIOutput6*,              DXGI_OUTPUT_DESC1*);
      IDXGIOutput6_GetDesc1_pfn
      IDXGIOutput6_GetDesc1_Original = nullptr;


HRESULT
WINAPI
IDXGIOutput6_GetDesc1_Override ( IDXGIOutput6      *This,
                           _Out_ DXGI_OUTPUT_DESC1 *pDesc )
{
  SK_LOG_FIRST_CALL

  if (pDesc == nullptr)
    return DXGI_ERROR_INVALID_CALL;

  HRESULT hr =
    IDXGIOutput6_GetDesc1_Original (This, pDesc);

  if (config.render.dxgi.hide_hdr_support)
  {
    if (SK_GetCallingDLL () != SK_GetDLL ())
    {
      if (SUCCEEDED (hr) && pDesc->ColorSpace == DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020)
      {
        pDesc->ColorSpace = DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709;
      }
    }
  }

  return hr;
}

void
SK_DXGI_HookPresent1 (IDXGISwapChain1* pSwapChain1)
{
  if (pSwapChain1 == nullptr)
    return;

  if (! LocalHook_IDXGISwapChain1_Present1.active)
  {
    DXGI_VIRTUAL_HOOK ( &pSwapChain1, 22,
                        "IDXGISwapChain1::Present1",
                         Present1Callback,
                         Present1_Original,
                         Present1SwapChain1_pfn );

    SK_Hook_TargetFromVFTable     (
      LocalHook_IDXGISwapChain1_Present1,
        (void **)&pSwapChain1, 22 );
  }
}

void
SK_DXGI_HookPresent (IDXGISwapChain* pSwapChain)
{
  if (pSwapChain == nullptr)
    return;

  SK_DXGI_HookPresentBase (pSwapChain);

  SK_ComQIPtr <IDXGISwapChain1>
      pSwapChain1 (pSwapChain);
  if (pSwapChain1 != nullptr)
  {
    SK_DXGI_HookPresent1 (pSwapChain1);
  }
}

std::string
SK_DXGI_FormatToString (DXGI_FORMAT fmt)
{
  UNREFERENCED_PARAMETER (fmt);
  return "<NOT IMPLEMENTED>";
}


void
SK_DXGI_HookSwapChain (IDXGISwapChain* pProxySwapChain)
{
  assert (pProxySwapChain != nullptr);
  if (    pProxySwapChain == nullptr)
    return;

  static volatile
               LONG hooked   = FALSE;
  if (ReadAcquire (&hooked) != FALSE)
    return;

  const bool bHasStreamline =
    SK_IsModuleLoaded (L"sl.interposer.dll");

  SK_ComPtr <IDXGISwapChain> pSwapChain;

  if (bHasStreamline)
  {
    if (SK_slGetNativeInterface (pProxySwapChain, (void **)&pSwapChain.p) == sl::Result::eOk)
      SK_LOGi0 (L"Hooking Streamline Native Interface for IDXGISwapChain...");

    else pSwapChain = pProxySwapChain;
  } else pSwapChain = pProxySwapChain;

  if (pSwapChain == nullptr)
    return;

  if (! first_frame)
    return;

  static bool        once = false;
  if (std::exchange (once, true))
    return;

  if (! InterlockedCompareExchangeAcquire (&hooked, TRUE, FALSE))
  {
    if (! LocalHook_IDXGISwapChain_SetFullscreenState.active)
    {
      DXGI_VIRTUAL_HOOK ( &pSwapChain, 10, "IDXGISwapChain::SetFullscreenState",
                                DXGISwap_SetFullscreenState_Override,
                                         SetFullscreenState_Original,
                                           SetFullscreenState_pfn );

      SK_Hook_TargetFromVFTable (
        LocalHook_IDXGISwapChain_SetFullscreenState,
          (void **)&pSwapChain, 10 );
    }

    if (! LocalHook_IDXGISwapChain_GetFullscreenState.active)
    {
      DXGI_VIRTUAL_HOOK ( &pSwapChain, 11, "IDXGISwapChain::GetFullscreenState",
                                DXGISwap_GetFullscreenState_Override,
                                         GetFullscreenState_Original,
                                           GetFullscreenState_pfn );
      SK_Hook_TargetFromVFTable (
        LocalHook_IDXGISwapChain_GetFullscreenState,
          (void **)&pSwapChain, 11 );
    }

    if (! LocalHook_IDXGISwapChain_ResizeBuffers.active)
    {
      DXGI_VIRTUAL_HOOK ( &pSwapChain, 13, "IDXGISwapChain::ResizeBuffers",
                               DXGISwap_ResizeBuffers_Override,
                                        ResizeBuffers_Original,
                                          ResizeBuffers_pfn );
      SK_Hook_TargetFromVFTable (
        LocalHook_IDXGISwapChain_ResizeBuffers,
          (void **)&pSwapChain, 13 );
    }

    if (! LocalHook_IDXGISwapChain_ResizeTarget.active)
    {
      DXGI_VIRTUAL_HOOK ( &pSwapChain, 14, "IDXGISwapChain::ResizeTarget",
                               DXGISwap_ResizeTarget_Override,
                                        ResizeTarget_Original,
                                          ResizeTarget_pfn );
      SK_Hook_TargetFromVFTable (
        LocalHook_IDXGISwapChain_ResizeTarget,
          (void **)&pSwapChain, 14 );
    }

    // 23 IsTemporaryMonoSupported
    // 24 GetRestrictToOutput
    // 25 SetBackgroundColor
    // 26 GetBackgroundColor
    // 27 SetRotation
    // 28 GetRotation
    // 29 SetSourceSize
    // 30 GetSourceSize
    // 31 SetMaximumFrameLatency
    // 32 GetMaximumFrameLatency
    // 33 GetFrameLatencyWaitableObject
    // 34 SetMatrixTransform
    // 35 GetMatrixTransform

    // 36 GetCurrentBackBufferIndex
    // 37 CheckColorSpaceSupport
    // 38 SetColorSpace1
    // 39 ResizeBuffers1

    // 40 SetHDRMetaData

    SK_ComQIPtr <IDXGISwapChain3> pSwapChain3 (pSwapChain);

    if ( pSwapChain3                                 != nullptr &&
     IDXGISwapChain3_CheckColorSpaceSupport_Original == nullptr )
    {
      DXGI_VIRTUAL_HOOK ( &pSwapChain3.p, 37,
                          "IDXGISwapChain3::CheckColorSpaceSupport",
                           IDXGISwapChain3_CheckColorSpaceSupport_Override,
                           IDXGISwapChain3_CheckColorSpaceSupport_Original,
                           IDXGISwapChain3_CheckColorSpaceSupport_pfn );

      DXGI_VIRTUAL_HOOK ( &pSwapChain3.p, 38,
                          "IDXGISwapChain3::SetColorSpace1",
                           IDXGISwapChain3_SetColorSpace1_Override,
                           IDXGISwapChain3_SetColorSpace1_Original,
                           IDXGISwapChain3_SetColorSpace1_pfn );

      DXGI_VIRTUAL_HOOK ( &pSwapChain3.p, 39,
                          "IDXGISwapChain3::ResizeBuffers1",
                                 DXGISwap3_ResizeBuffers1_Override,
                                           ResizeBuffers1_Original,
                           IDXGISwapChain3_ResizeBuffers1_pfn );
    }

    SK_ComQIPtr <IDXGISwapChain4> pSwapChain4 (pSwapChain);

    if ( pSwapChain4                         != nullptr &&
     IDXGISwapChain4_SetHDRMetaData_Original == nullptr )
    {
      DXGI_VIRTUAL_HOOK ( &pSwapChain4.p, 40,
                       //&pSwapChain, 40,
                          "IDXGISwapChain4::SetHDRMetaData",
                           IDXGISwapChain4_SetHDRMetaData,
                           IDXGISwapChain4_SetHDRMetaData_Original,
                           IDXGISwapChain4_SetHDRMetaData_pfn );
    }

    SK_ComPtr                          <IDXGIOutput> pOutput;
    if (SUCCEEDED (pSwapChain->GetContainingOutput (&pOutput)))
    {
      if (pOutput != nullptr)
      {
        DXGI_VIRTUAL_HOOK ( &pOutput.p, 8,
                               "IDXGIOutput::GetDisplayModeList",
                                  DXGIOutput_GetDisplayModeList_Override,
                                             GetDisplayModeList_Original,
                                             GetDisplayModeList_pfn );

        DXGI_VIRTUAL_HOOK ( &pOutput.p, 9,
                               "IDXGIOutput::FindClosestMatchingMode",
                                  DXGIOutput_FindClosestMatchingMode_Override,
                                             FindClosestMatchingMode_Original,
                                             FindClosestMatchingMode_pfn );

        // Don't hook this unless you want nvspcap to crash the game.

        DXGI_VIRTUAL_HOOK ( &pOutput.p, 10,
                              "IDXGIOutput::WaitForVBlank",
                                 DXGIOutput_WaitForVBlank_Override,
                                            WaitForVBlank_Original,
                                            WaitForVBlank_pfn );

        // 11 TakeOwnership
        // 12 ReleaseOwnership
        // 13 GetGammaControlCapabilities
        // 14 SetGammaControl
        // 15 GetGammaControl
        // 16 SetDisplaySurface
        // 17 GetDisplaySurfaceData
        // 18 GetFrameStatistics
        // 19 GetDisplayModeList1
        // 20 FindClosestMatchingMode1
        // 21 GetDisplaySurfaceData1
        // 22 DuplicateOutput
        // 23 SupportsOverlays
        // 24 CheckOverlaySupport
        // 25 CheckOverlayColorSpaceSupport
        // 26 DuplicateOutput1
        // 27 GetDesc1

        SK_ComQIPtr <IDXGIOutput6>
            pOutput6 (pOutput);
        if (pOutput6 != nullptr)
        {
          DXGI_VIRTUAL_HOOK ( &pOutput6.p, 27,
                               "IDXGIOutput6::GetDesc1",
                                 IDXGIOutput6_GetDesc1_Override,
                                 IDXGIOutput6_GetDesc1_Original,
                                 IDXGIOutput6_GetDesc1_pfn );
        }
      }
    }

    // This won't catch Present1 (...), but no games use that
    //   and we can deal with it later if it happens.
    SK_DXGI_HookPresentBase ((IDXGISwapChain *)pSwapChain);

    SK_ComQIPtr <IDXGISwapChain1> pSwapChain1 (pSwapChain);

    if (pSwapChain1 != nullptr)
      SK_DXGI_HookPresent1 (pSwapChain1);

    InterlockedIncrementRelease (&hooked);
  }

  SK_Thread_SpinUntilAtomicMin (&hooked, 2);
}


void
SK_DXGI_HookDevice1 (IDXGIDevice1* pProxyDevice)
{
  assert (pProxyDevice != nullptr);
  if (    pProxyDevice == nullptr)
    return;

  static volatile
               LONG hooked   = FALSE;
  if (ReadAcquire (&hooked) != FALSE)
    return;

  const bool bHasStreamline =
    SK_IsModuleLoaded (L"sl.interposer.dll");

  SK_ComPtr <IDXGIDevice1> pDevice;

  if (bHasStreamline)
  {
    if (SK_slGetNativeInterface (pProxyDevice, (void **)&pDevice.p) == sl::Result::eOk)
      SK_LOGi0 (L"Hooking Streamline Native Interface for IDXGIDevice1...");

    else pDevice = pProxyDevice;
  } else pDevice = pProxyDevice;

  if (! InterlockedCompareExchangeAcquire (&hooked, TRUE, FALSE))
  {
    //int iver = SK_GetDXGIFactoryInterfaceVer (pFactory);

    //  0 QueryInterface
    //  1 AddRef
    //  2 Release
    //
    //  3 SetPrivateData
    //  4 SetPrivateDataInterface
    //  5 GetPrivateData
    //  6 GetParent
    // 
    //  7 GetAdapter
    //  8 CreateSurface
    //  9 QueryResourceResidency
    // 10 SetGPUThreadPriority
    // 11 GetGPUThreadPriority
    //
    // 12 SetMaximumFrameLatency
    // 13 GetMaximumFrameLatency

    if (DXGIDevice1_SetMaximumFrameLatency_Original == nullptr)
    {
      DXGI_VIRTUAL_HOOK ( &pDevice, 12,
                          "IDXGIDevice1::SetMaximumFrameLatency",
                             DXGIDevice1_SetMaximumFrameLatency_Override,
                             DXGIDevice1_SetMaximumFrameLatency_Original,
                             DXGIDevice1_SetMaximumFrameLatency_pfn );
    }

    if (DXGIDevice1_GetMaximumFrameLatency_Original == nullptr)
    {
      DXGI_VIRTUAL_HOOK ( &pDevice, 13,
                          "IDXGIDevice1::GetMaximumFrameLatency",
                             DXGIDevice1_GetMaximumFrameLatency_Override,
                             DXGIDevice1_GetMaximumFrameLatency_Original,
                             DXGIDevice1_GetMaximumFrameLatency_pfn );
    }

    InterlockedIncrementRelease (&hooked);
  }

  SK_Thread_SpinUntilAtomicMin (&hooked, 2);
}

using IDXGIAdapter3_QueryVideoMemoryInfo_pnf = HRESULT (STDMETHODCALLTYPE *)(IDXGIAdapter3*,
                                                                             UINT,DXGI_MEMORY_SEGMENT_GROUP,
                                                                             DXGI_QUERY_VIDEO_MEMORY_INFO*);

IDXGIAdapter3_QueryVideoMemoryInfo_pnf
IDXGIAdapter3_QueryVideoMemoryInfo_Original = nullptr;

HRESULT
STDMETHODCALLTYPE
IDXGIAdapter3_QueryVideoMemoryInfo_Detour ( IDXGIAdapter3                *This,
                                      _In_  UINT                          NodeIndex,
                                      _In_  DXGI_MEMORY_SEGMENT_GROUP     MemorySegmentGroup,
                                      _Out_ DXGI_QUERY_VIDEO_MEMORY_INFO *pVideoMemoryInfo )
{
  SK_LOG_FIRST_CALL;

  HRESULT hr =
    IDXGIAdapter3_QueryVideoMemoryInfo_Original (This, NodeIndex, MemorySegmentGroup, pVideoMemoryInfo);

  if (SUCCEEDED (hr))
  {
    pVideoMemoryInfo->Budget =
      static_cast <UINT64> (
      static_cast <double> (pVideoMemoryInfo->Budget) * config.render.dxgi.vram_budget_scale);
  }

  return hr;
}

void
SK_DXGI_HookAdapter (IDXGIAdapter* pAdapter)
{
  static volatile
               LONG hooked   = FALSE;
  if (ReadAcquire (&hooked) != FALSE)
    return;

  //  0 QueryInterface
  //  1 AddRef
  //  2 Release

  //  3 SetPrivateData
  //  4 SetPrivateDataInterface
  //  5 GetPrivateData
  //  6 GetParent

  // IDXGIAdapter
  // 
  //  7 EnumOutputs
  //  8 GetDesc
  //  9 CheckInterfaceSupport

  // IDXGIAdapter1
  // 
  // 10 GetDesc1

  // IDXGIAdapter2
  // 
  // 11 GetDesc2

  // IDXGIAdapter3
  // 
  // 12 RegisterHardwareContentProtectionTeardownStatusEvent
  // 13 UnregisterHardwareContentProtectionTeardownStatus
  // 14 QueryVideoMemoryInfo        
  // 15 SetVideoMemoryReservation
  // 16 RegisterVideoMemoryBudgetChangeNotificationEvent
  // 17 UnregisterVideoMemoryBudgetChangeNotification

  SK_ComQIPtr <IDXGIAdapter3>
                   pAdapter3 (pAdapter);
  if (nullptr  !=  pAdapter3)
  {
    DXGI_VIRTUAL_HOOK ( &pAdapter3.p, 14,
                        "IDXGIAdapter3::QueryVideoMemoryInfo",
                         IDXGIAdapter3_QueryVideoMemoryInfo_Detour,
                         IDXGIAdapter3_QueryVideoMemoryInfo_Original,
                         IDXGIAdapter3_QueryVideoMemoryInfo_pfn );
    SK_ApplyQueuedHooks ();
  }
}

void
SK_DXGI_HookFactory (IDXGIFactory* pProxyFactory)
{
  assert (pProxyFactory != nullptr);
  if (    pProxyFactory == nullptr)
    return;

  static volatile
               LONG hooked   = FALSE;
  if (ReadAcquire (&hooked) != FALSE)
    return;

  SK_GetDXGIFactoryInterfaceVer (pProxyFactory);

  const bool bHasStreamline =
    SK_IsModuleLoaded (L"sl.interposer.dll");

  SK_ComPtr <IDXGIFactory> pFactory;

  if (bHasStreamline)
  {
    if (SK_slGetNativeInterface (pProxyFactory, (void **)&pFactory.p) == sl::Result::eOk)
      SK_LOGi0 (L"Hooking Streamline Native Interface for IDXGIFactory...");

    else pFactory = pProxyFactory;
  } else pFactory = pProxyFactory;

  if (! InterlockedCompareExchangeAcquire (&hooked, TRUE, FALSE))
  {
    //int iver = SK_GetDXGIFactoryInterfaceVer (pFactory);

    //  0 QueryInterface
    //  1 AddRef
    //  2 Release
    //  3 SetPrivateData
    //  4 SetPrivateDataInterface
    //  5 GetPrivateData
    //  6 GetParent
    //  7 EnumAdapters
    //  8 MakeWindowAssociation
    //  9 GetWindowAssociation
    // 10 CreateSwapChain
    // 11 CreateSoftwareAdapter
    if (! LocalHook_IDXGIFactory_CreateSwapChain.active)
    {
      DXGI_VIRTUAL_HOOK ( &pFactory,     10,
                          "IDXGIFactory::CreateSwapChain",
                           DXGIFactory_CreateSwapChain_Override,
                                       CreateSwapChain_Original,
                                       CreateSwapChain_pfn );

      SK_Hook_TargetFromVFTable (
        LocalHook_IDXGIFactory_CreateSwapChain,
          (void **)&pFactory, 10 );
    }

    if (! config.compatibility.using_wine)
    {
      SK_ComQIPtr <IDXGIFactory1> pFactory1 (pFactory);

      // 12 EnumAdapters1
      // 13 IsCurrent
      if (pFactory1 != nullptr)
      {
        DXGI_VIRTUAL_HOOK ( /*&pFactory1.p*/&pFactory,     12,
                            "IDXGIFactory1::EnumAdapters1",
                             EnumAdapters1_Override,
                             EnumAdapters1_Original,
                             EnumAdapters1_pfn );
      }

      else
      {
        //
        // EnumAdapters actually calls EnumAdapters1 if the interface
        //   implements IDXGIFactory1...
        //
        //  >> Avoid some nasty recursion and only hook EnumAdapters if the
        //       interface version is DXGI 1.0.
        //
        DXGI_VIRTUAL_HOOK ( &pFactory,     7,
                            "IDXGIFactory::EnumAdapters",
                             EnumAdapters_Override,
                             EnumAdapters_Original,
                             EnumAdapters_pfn );
      }
    }


    // DXGI 1.2+

    // 14 IsWindowedStereoEnabled
    // 15 CreateSwapChainForHwnd
    // 16 CreateSwapChainForCoreWindow
    // 17 GetSharedResourceAdapterLuid
    // 18 RegisterStereoStatusWindow
    // 19 RegisterStereoStatusEvent
    // 20 UnregisterStereoStatus
    // 21 RegisterOcclusionStatusWindow
    // 22 RegisterOcclusionStatusEvent
    // 23 UnregisterOcclusionStatus
    // 24 CreateSwapChainForComposition
    if ( CreateDXGIFactory1_Import != nullptr )
    {
#if 1
      SK_ComQIPtr <IDXGIFactory2> pFactory2 (pFactory);
      if (pFactory2 != nullptr)
#else
      SK_ComPtr <IDXGIFactory2> pFactory2;
      if ( SUCCEEDED (CreateDXGIFactory1_Import (IID_IDXGIFactory2, (void **)&pFactory2.p)) )
#endif
      {
        if (! LocalHook_IDXGIFactory2_CreateSwapChainForHwnd.active)
        {
          DXGI_VIRTUAL_HOOK ( /*&pFactory2.p*/&pFactory, 15,
                              "IDXGIFactory2::CreateSwapChainForHwnd",
                               DXGIFactory2_CreateSwapChainForHwnd_Override,
                                            CreateSwapChainForHwnd_Original,
                                            CreateSwapChainForHwnd_pfn );

          SK_Hook_TargetFromVFTable (
            LocalHook_IDXGIFactory2_CreateSwapChainForHwnd,
              (void **)/*&pFactory2.p*/&pFactory, 15 );
        }

        if (! LocalHook_IDXGIFactory2_CreateSwapChainForCoreWindow.active)
        {
          DXGI_VIRTUAL_HOOK ( /*&pFactory2.p*/&pFactory, 16,
                              "IDXGIFactory2::CreateSwapChainForCoreWindow",
                               DXGIFactory2_CreateSwapChainForCoreWindow_Override,
                                            CreateSwapChainForCoreWindow_Original,
                                            CreateSwapChainForCoreWindow_pfn );
          SK_Hook_TargetFromVFTable (
            LocalHook_IDXGIFactory2_CreateSwapChainForCoreWindow,
              (void **)/*&pFactory2.p*/&pFactory, 16 );
        }

    ////if (! LocalHook_IDXGIFactory2_CreateSwapChainForComposition.active)
    ////{
    ////  DXGI_VIRTUAL_HOOK ( /*&pFactory2.p*/&pFactory, 24,
    ////                      "IDXGIFactory2::CreateSwapChainForComposition",
    ////                       DXGIFactory2_CreateSwapChainForComposition_Override,
    ////                                    CreateSwapChainForComposition_Original,
    ////                                    CreateSwapChainForComposition_pfn );
    ////
    ////  SK_Hook_TargetFromVFTable (
    ////    LocalHook_IDXGIFactory2_CreateSwapChainForComposition,
    ////      (void **)/*&pFactory2.p*/&pFactory, 24 );
    ////}
      }
    }


    // DXGI 1.3+
    SK_ComPtr <IDXGIFactory3> pFactory3;

    // 25 GetCreationFlags


    // DXGI 1.4+
    SK_ComPtr <IDXGIFactory4> pFactory4;

    // 26 EnumAdapterByLuid
    // 27 EnumWarpAdapter


    // DXGI 1.5+
    SK_ComPtr <IDXGIFactory5> pFactory5;

    // 28 CheckFeatureSupport

    InterlockedIncrementRelease (&hooked);
  }

  SK_Thread_SpinUntilAtomicMin (&hooked, 2);
}


void
SK_DXGI_InitHooksBeforePlugIn (void)
{
  if (SK_Import_GetNumberOfPlugIns () > 0)
  {
    //bool  bEnable = SK_EnableApplyQueuedHooks ();
    {
      SK_ApplyQueuedHooks ();
    }
    //if (! bEnable)  SK_DisableApplyQueuedHooks ();
  }
}

//! Dummy interface allowing us to extract the underlying base interface
struct DECLSPEC_UUID("ADEC44E2-61F0-45C3-AD9F-1B37379284FF") StreamlineRetrieveBaseInterface : IUnknown
{
};

// A hack to prevent Streamline from crashing in its hook if it does not implement the device interface
HRESULT
SK_DXGI_SafeCreateSwapChain ( IDXGIFactory          *pFactory,
                              IUnknown              *pDevice,
                              DXGI_SWAP_CHAIN_DESC  *pDesc,
                              IDXGISwapChain       **ppSwapChain )
{
  __try {
    return
      pFactory->CreateSwapChain (pDevice, pDesc, ppSwapChain);
  }

  __except (EXCEPTION_EXECUTE_HANDLER)
  {
    if (ppSwapChain != nullptr)
       *ppSwapChain  = nullptr;
  }

  return E_NOTIMPL;
}

#include <render/d3d12/d3d12_device.h>
#include <render/d3d12/d3d12_command_queue.h>

DWORD
__stdcall
HookDXGI (LPVOID user)
{
  initializing_dxgi = true;

  static SK_AutoCOMInit _autocom;

  SetCurrentThreadDescription (L"[SK] DXGI Hook Crawler");

  // "Normal" games don't change render APIs mid-game; Talos does, but it's
  //   not normal :)
  if (SK_GetFramesDrawn ())
  {
    SK_Thread_CloseSelf ();
    return 0;
  }


  UNREFERENCED_PARAMETER (user);

  if (! (config.apis.dxgi.d3d11.hook ||
         config.apis.dxgi.d3d12.hook) )
  {
    SK_Thread_CloseSelf ();
    return 0;
  }

  // Wait for DXGI to boot
  if (CreateDXGIFactory_Import == nullptr)
  {
    static volatile ULONG implicit_init = FALSE;

    // If something called a D3D11 function before DXGI was initialized,
    //   begin the process, but ... only do this once.
    if (! InterlockedCompareExchange (&implicit_init, TRUE, FALSE))
    {
      dll_log->Log (L"[  D3D 11  ]  >> Implicit Initialization Triggered <<");
      SK_BootDXGI ();
    }

    while (CreateDXGIFactory_Import == nullptr)
      MsgWaitForMultipleObjectsEx (0, nullptr, 33, QS_ALLEVENTS, MWMO_INPUTAVAILABLE);

    // TODO: Handle situation where CreateDXGIFactory is unloadable
  }

  SK_TLS *pTLS =
    SK_TLS_Bottom ();

  if ( __SK_bypass     || ReadAcquire (&__dxgi_ready) ||
       pTLS == nullptr || pTLS->render->d3d11->ctx_init_thread )
  {
    SK_Thread_CloseSelf ();
    return 0;
  }


  static volatile LONG __hooked = FALSE;

  if (! InterlockedCompareExchangeAcquire (&__hooked, TRUE, FALSE))
  {
    initializing_dxgi = true;
    pTLS->render->d3d11->ctx_init_thread = true;

    SK_AutoCOMInit auto_com;

    SK_D3D11_Init ();

    if (D3D11CreateDeviceAndSwapChain_Import == nullptr)
    {
      pTLS->render->d3d11->ctx_init_thread = false;

      if ((SK_GetDLLRole () & DLL_ROLE::DXGI) || (SK_GetDLLRole () & DLL_ROLE::DInput8))
      {
        // PlugIns need to be loaded AFTER we've hooked the device creation functions
        SK_DXGI_InitHooksBeforePlugIn ();

        // Load user-defined DLLs (Plug-In)
        SK_RunLHIfBitness ( 64, SK_LoadPlugIns64 (),
                                SK_LoadPlugIns32 () );
      }

      SK_ApplyQueuedHooks ();
      return 0;
    }

    dll_log->Log (L"[   DXGI   ]   Installing Deferred DXGI / D3D11 / D3D12 Hooks");

    D3D_FEATURE_LEVEL            levels [] = { D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0,
                                               D3D_FEATURE_LEVEL_10_1, D3D_FEATURE_LEVEL_10_0 };
    D3D_FEATURE_LEVEL          featureLevel  = D3D_FEATURE_LEVEL_11_1;
    SK_ComPtr <ID3D11Device>        pDevice           = nullptr;
    SK_ComPtr <ID3D11DeviceContext> pImmediateContext = nullptr;

    // DXGI stuff is ready at this point, we'll hook the swapchain stuff
    //   after this call.

    SK_ComPtr <IDXGISwapChain> pSwapChain = nullptr;
    DXGI_SWAP_CHAIN_DESC       desc       = { };

    desc.BufferDesc.Format           = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
    desc.BufferDesc.Scaling          = DXGI_MODE_SCALING_UNSPECIFIED;
    desc.SampleDesc.Count            = 1;
    desc.SampleDesc.Quality          = 0;
    desc.BufferDesc.Width            = 2;
    desc.BufferDesc.Height           = 2;
    desc.BufferUsage                 = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.BufferCount                 = SK_IsWindows8Point1OrGreater () ? 2 : 1;
    desc.OutputWindow                = SK_Win32_CreateDummyWindow   ();
    desc.Windowed                    = TRUE;
    desc.SwapEffect                  = SK_IsWindows10OrGreater () ? DXGI_SWAP_EFFECT_FLIP_DISCARD
                                                                  : SK_IsWindows8Point1OrGreater ()
                                                                  ? DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL
                                                                  : DXGI_SWAP_EFFECT_DISCARD;

    SK_COMPAT_UnloadFraps ();

    if ((SK_GetDLLRole () & DLL_ROLE::DXGI) || (SK_GetDLLRole () & DLL_ROLE::DInput8))
    {
      // PlugIns need to be loaded AFTER we've hooked the device creation functions
      SK_DXGI_InitHooksBeforePlugIn ();

      // Load user-defined DLLs (Plug-In)
      SK_RunLHIfBitness ( 64, SK_LoadPlugIns64 (),
                              SK_LoadPlugIns32 () );
    }

    if (! D3D11CreateDeviceAndSwapChain_Import)
      return 0;


    bool    bHookSuccess   = false;
    bool    bHasStreamline = SK_IsModuleLoaded (L"sl.interposer.dll");
    HRESULT hr             = E_NOTIMPL;

    SK_ComPtr <IDXGIAdapter>
               pAdapter0;

    const auto factory_flags =
      config.render.dxgi.debug_layer ?
           DXGI_CREATE_FACTORY_DEBUG : 0x0;

    if (config.render.dxgi.debug_layer)
    {
      static const char* D3D12SDKPath =
        (const char *)GetProcAddress (nullptr, "D3D12SDKPath");

      wchar_t    wszD3D12CorePath [MAX_PATH] = {};
      wcsncpy_s (wszD3D12CorePath, MAX_PATH, SK_GetHostPath (), _TRUNCATE);

      if (D3D12SDKPath != nullptr)
      { PathAppendW (wszD3D12CorePath, SK_UTF8ToWideChar (D3D12SDKPath).c_str ());
        PathAppendW (wszD3D12CorePath, L"D3D12Core.dll");
      } else {
        PathAppendW (wszD3D12CorePath, LR"(\D3D12\D3D12Core.dll)");
      }

      if (PathFileExistsW (wszD3D12CorePath) && SK_LoadLibraryW (wszD3D12CorePath))
      {
        using D3D12GetInterface_pfn = HRESULT (WINAPI *)(REFCLSID rclsid, REFIID riid, void **ppvDebug);

        D3D12GetInterface_pfn
       _D3D12GetInterface =
       (D3D12GetInterface_pfn)SK_GetProcAddress (wszD3D12CorePath,
       "D3D12GetInterface");

        if (_D3D12GetInterface != nullptr)
        {
          SK_ComPtr <ID3D12Debug>                                             pDebugD3D12;
          if (SUCCEEDED (_D3D12GetInterface (CLSID_D3D12Debug, IID_PPV_ARGS (&pDebugD3D12.p))))
                                                                              pDebugD3D12->EnableDebugLayer ();
        }
      }

      else if (SK_IsModuleLoaded (L"d3d12.dll"))
      {
        D3D12GetDebugInterface_pfn
       _D3D12GetDebugInterface =
       (D3D12GetDebugInterface_pfn)SK_GetProcAddress (L"d3d12.dll",
       "D3D12GetDebugInterface");

        if (_D3D12GetDebugInterface != nullptr)
        {
          SK_ComPtr <ID3D12Debug>                                pDebugD3D12;
          if (SUCCEEDED (_D3D12GetDebugInterface (IID_PPV_ARGS (&pDebugD3D12.p))))
                                                                 pDebugD3D12->EnableDebugLayer ();
        }
      }
    }

    SK_ComPtr <IDXGIFactory7>                pFactory7;
    CreateDXGIFactory2_Import ( factory_flags,
         __uuidof (IDXGIFactory7), (void **)&pFactory7.p);

    SK_ComPtr <IDXGIFactory>
        pFactory    = (IDXGIFactory *)
        pFactory7.p;
    if (pFactory7.p == nullptr)
    {
      static const IID iids [] = { IID_IDXGIFactory,  IID_IDXGIFactory1, IID_IDXGIFactory2,
                                   IID_IDXGIFactory3, IID_IDXGIFactory4, IID_IDXGIFactory5,
                                   IID_IDXGIFactory6, IID_IDXGIFactory7 };

      CreateDXGIFactory2_Import ( factory_flags,
             iids [std::clamp (SK_DXGI_HighestFactorySupported, 0, 7)],
                       (void **)&pFactory.p );
    }

    if (pFactory7 != nullptr)
        pFactory7->EnumAdapterByGpuPreference (0, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS (&pAdapter0.p));
    else pFactory->EnumAdapters               (0,                                                     &pAdapter0.p);

    SK_ComPtr <IDXGISwapChain>      pNativeSwapChain;
    SK_ComPtr <IDXGIFactory>        pNativeFactory;
    SK_ComPtr <ID3D11Device>        pNativeDevice;
    SK_ComPtr <ID3D11DeviceContext> pNativeImmediateContext;

    // Probably better named Nixxes mode, what a pain :(
    const bool bStreamlineMode =
      config.compatibility.init_sync_for_streamline;
      //SK_GetCurrentGameID () == SK_GAME_ID::HorizonForbiddenWest ||
      //(SK_GetModuleHandleW (L"sl.dlss_g.dll") && config.system.global_inject_delay == 0.0f);

    const bool bReShadeMode =
      (config.compatibility.reshade_mode && (! config.compatibility.using_wine));

    // This has benefits, but may prove unreliable with software
    //   that requires NVIDIA's DXGI/Vulkan interop layer
    if (bStreamlineMode || bReShadeMode)
    {
      using D3D12CreateDevice_pfn =
        HRESULT (WINAPI *)( IUnknown         *pAdapter,
                            D3D_FEATURE_LEVEL MinimumFeatureLevel,
                            REFIID            riid,
                            void            **ppDevice );

      SK_ComPtr <ID3D12Device>       pDevice12, pNativeDevice12;
      SK_ComPtr <ID3D12CommandQueue> pCmdQueue, pNativeCmdQueue;

      if (config.compatibility.allow_fake_streamline)
      {
        D3D11CoreCreateDevice_pfn
        D3D11CoreCreateDevice = (D3D11CoreCreateDevice_pfn)SK_GetProcAddress (
               LoadLibraryExW (L"d3d11.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32),
                                "D3D11CoreCreateDevice" );

        //// Favor this codepath because it bypasses many things like ReShade, but
        ////   it's necessary to skip this path if NVIDIA's Vk/DXGI interop layer is active
        if (D3D11CoreCreateDevice != nullptr && (! ( SK_IsModuleLoaded (L"vulkan-1.dll") ||
                                                    (SK_IsModuleLoaded (L"OpenGL32.dll") && !SK_IsModuleLoaded ((L"EOSOVH-Win64-Shipping.dll"))))))
        {
          hr =
            D3D11CoreCreateDevice (
              nullptr, pAdapter0,
                D3D_DRIVER_TYPE_UNKNOWN, nullptr,
                  config.render.dxgi.debug_layer ?
                       D3D11_CREATE_DEVICE_DEBUG : 0x0,
                                  levels,
                      _ARRAYSIZE (levels),
                        D3D11_SDK_VERSION,
                          &pDevice.p,
                            &featureLevel );
        }
        
        else
        {
          hr =
            D3D11CreateDevice_Import (
              pAdapter0, D3D_DRIVER_TYPE_UNKNOWN,
                nullptr,
                    config.render.dxgi.debug_layer ?
                         D3D11_CREATE_DEVICE_DEBUG : 0x0,
                                    levels,
                        _ARRAYSIZE (levels),
                          D3D11_SDK_VERSION,
                            &pDevice.p,
                              &featureLevel,
                                nullptr );
        }
      }
      
      if (! config.compatibility.allow_fake_streamline)
      {
        SK_slUpgradeInterface ((void **)&pFactory.p);

        SK_LoadLibraryW (L"d3d12.dll");

        // Stupid NVIDIA Streamline hack; lowers software compatibility with everything else.
        //   Therefore, just it may be better to leave Streamline unsupported.
        if (SK_IsModuleLoaded (L"d3d12.dll"))
        {
          static D3D12CreateDevice_pfn
            D3D12CreateDevice = (D3D12CreateDevice_pfn)
              SK_GetProcAddress (L"d3d12.dll",
                                "D3D12CreateDevice");

          if (SUCCEEDED (D3D12CreateDevice (pAdapter0, D3D_FEATURE_LEVEL_11_1, IID_PPV_ARGS (&pDevice12.p))))
          {
            if (SK_slGetNativeInterface (pDevice12.p, (void **)&pNativeDevice12.p) == sl::Result::eOk)
            {   _ExchangeProxyForNative (pDevice12,             pNativeDevice12);
              SK_LOGi0 (L"Got Native Interface for Streamline Proxy'd D3D12 Device...");
            }

            SK_D3D12_InstallDeviceHooks       (pDevice12.p);
            SK_D3D12_InstallCommandQueueHooks (pDevice12.p);

            if (sl::Result::eOk == SK_slUpgradeInterface ((void **)&pDevice12.p))
              SK_LOGi0 (L"Upgraded D3D12 Device to Streamline Proxy...");

            D3D12_COMMAND_QUEUE_DESC
              queue_desc       = { };
              queue_desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
              queue_desc.Type  = D3D12_COMMAND_LIST_TYPE_DIRECT;

            pDevice12->CreateCommandQueue (&queue_desc, IID_PPV_ARGS (&pCmdQueue.p));
          }
        }
      }

      if (SUCCEEDED (hr))
      {
        if (pDevice != nullptr)
            pDevice->GetImmediateContext (&pImmediateContext.p);

        if (! pDevice12)
          SK_DXGI_SafeCreateSwapChain (pFactory, pDevice.p, &desc, &pSwapChain.p);
      }

      if (pDevice12 != nullptr)
      {
        if (FAILED (SK_DXGI_SafeCreateSwapChain (pFactory, pCmdQueue.p, &desc, &pSwapChain.p)))
                    SK_DXGI_SafeCreateSwapChain (pFactory, pDevice.p,   &desc, &pSwapChain.p);
      }

      if (bHasStreamline)
      {
        if (SK_slGetNativeInterface (pFactory.p, (void **)&pNativeFactory.p) == sl::Result::eOk)
        {   _ExchangeProxyForNative (pFactory,             pNativeFactory);
          SK_LOGi0 (L"Got Native Interface for Streamline Proxy'd DXGI Factory...");
        }

        if (SK_slGetNativeInterface (pDevice.p, (void **)&pNativeDevice.p) == sl::Result::eOk)
        {   _ExchangeProxyForNative (pDevice,             pNativeDevice);
          SK_LOGi0 (L"Got Native Interface for Streamline Proxy'd D3D11 Device...");
        }

        if (SK_slGetNativeInterface (pImmediateContext.p, (void **)&pNativeImmediateContext.p) == sl::Result::eOk)
        {   _ExchangeProxyForNative (pImmediateContext,             pNativeImmediateContext);
          SK_LOGi0 (L"Got Native Interface for Streamline Proxy'd D3D11 Immediate Context...");
        }
      }

      sk_hook_d3d11_t d3d11_hook_ctx = { };

      d3d11_hook_ctx.ppDevice           = &pDevice.p;
      d3d11_hook_ctx.ppImmediateContext = &pImmediateContext.p;

      if ( SUCCEEDED (hr) || pDevice12 != nullptr )
      {
        if (SUCCEEDED (hr))
          HookD3D11           (&d3d11_hook_ctx);
        SK_DXGI_HookFactory   (pFactory);

        bHookSuccess = true;
      }
    }

    //
    // Old initialization procedure
    //
    else
    {
      hr =
        D3D11CreateDevice_Import (
          nullptr,
            D3D_DRIVER_TYPE_HARDWARE,
              nullptr,
                config.render.dxgi.debug_layer ?
                     D3D11_CREATE_DEVICE_DEBUG : 0x0,
                  levels,
                    _ARRAYSIZE (levels),
                      D3D11_SDK_VERSION,
                          &pDevice.p,
                            &featureLevel,
                              &pImmediateContext.p );

      if (SUCCEEDED (hr))
      {
        if (SK_slGetNativeInterface (pFactory, (void **)&pNativeFactory.p) == sl::Result::eOk)
            _ExchangeProxyForNative (pFactory,           pNativeFactory);

        if (SK_slGetNativeInterface (pDevice.p, (void **)&pNativeDevice.p) == sl::Result::eOk)
            _ExchangeProxyForNative (pDevice,             pNativeDevice);

        if (SK_slGetNativeInterface (pImmediateContext.p, (void **)&pNativeImmediateContext.p) == sl::Result::eOk)
            _ExchangeProxyForNative (pImmediateContext,             pNativeImmediateContext);

        SK_DXGI_SafeCreateSwapChain (pFactory, pDevice.p, &desc, &pSwapChain.p);

        sk_hook_d3d11_t d3d11_hook_ctx =
          { &pDevice.p, &pImmediateContext.p };

        HookD3D11           (&d3d11_hook_ctx);
        SK_DXGI_HookFactory (pFactory);

        bHookSuccess = true;
      }
    }

    if (bHookSuccess)
    {
      if (pSwapChain != nullptr)
      {
        if (SK_slGetNativeInterface (pSwapChain.p, (void **)&pNativeSwapChain.p) == sl::Result::eOk) {
                                     pSwapChain.p->AddRef (); // Leak the SwapChain to avoid crashes in Nixxes games
            _ExchangeProxyForNative (pSwapChain,             pNativeSwapChain);
        }

        SK_DXGI_HookSwapChain (pSwapChain);
      }

      //bool  bEnable = SK_EnableApplyQueuedHooks  ();
      {
        SK_ApplyQueuedHooks ();
      }
      //if (! bEnable)  SK_DisableApplyQueuedHooks ();

      InterlockedIncrementRelease (&SK_D3D11_initialized);

      if (config.apis.dxgi.d3d11.hook) SK_D3D11_EnableHooks ();

      WriteRelease (&__dxgi_ready, TRUE);
    }

    else
    {
      _com_error err (hr);

      dll_log->Log (L"[   DXGI   ] Unable to hook D3D11?! HRESULT=%x ('%s')",
                                err.Error (), err.ErrorMessage () != nullptr ?
                                              err.ErrorMessage ()            : L"Unknown" );

      // NOTE: Calling err.ErrorMessage () above generates the storage for these string functions
      //         --> They may be NULL if allocation failed.
      std::wstring err_desc (err.ErrorInfo () != nullptr ? err.Description () : _bstr_t (L"Unknown"));
      std::wstring err_src  (err.ErrorInfo () != nullptr ? err.Source      () : _bstr_t (L"Unknown"));

      dll_log->Log (L"[   DXGI   ]  >> %s, in %s",
                               err_desc.c_str (), err_src.c_str () );
    }

    // Let the SwapChain leak so that ReShade add-ons do not randomly crash at startup.
#define SAFETY_LEAK
#ifdef  SAFETY_LEAK
    pFactory.Detach          ();
    pDevice.Detach           ();
    pImmediateContext.Detach ();
    pSwapChain.Detach        ();
#else
    SK_Win32_CleanupDummyWindow (desc.OutputWindow);
#endif

    InterlockedIncrementRelease (&__hooked);
  }

  SK_Thread_SpinUntilAtomicMin (&__hooked, 2);

  SK_Thread_CloseSelf ();

  return 0;
}

bool
SK::DXGI::Shutdown (void)
{
  static const auto&
    game_id = SK_GetCurrentGameID ();

#ifdef _WIN64
  if (game_id == SK_GAME_ID::DarkSouls3)
  {
    SK_DS3_ShutdownPlugin (L"dxgi");
  }
#endif

  // Video capture software usually lets one frame through before @#$%'ing
  //   up the Present (...) hook.
  if (SK_GetFramesDrawn () < 2)
  {
    SK_LOGs0 ( L"Hook Cache",
               L" !!! No frames drawn using DXGI backend; purging "
               L"injection address cache..." );

    for ( auto& it : local_dxgi_records )
    {
      SK_Hook_RemoveTarget ( *it, L"DXGI.Hooks" );
    }

    SK_D3D11_PurgeHookAddressCache ();
  }


  if (config.apis.dxgi.d3d11.hook) SK_D3D11_Shutdown ();

////  if (config.apis.dxgi.d3d12.hook) SK_D3D12_Shutdown ();

  if (StrStrIW (SK_GetBackend (), L"d3d11"))
    return SK_ShutdownCore (L"d3d11");

  return SK_ShutdownCore (L"dxgi");
}



void
WINAPI
SK_DXGI_SetPreferredAdapter (int override_id) noexcept
{
  SK_DXGI_preferred_adapter = override_id;
}

memory_stats_t   dxgi_mem_stats [MAX_GPU_NODES] = { };
mem_info_t       dxgi_mem_info  [NumBuffers]    = { };

struct budget_thread_params_t
{
           IDXGIAdapter3 *pAdapter = nullptr;
           HANDLE         handle   = INVALID_HANDLE_VALUE;
           HANDLE         event    = INVALID_HANDLE_VALUE;
           HANDLE         manual   = INVALID_HANDLE_VALUE;
           HANDLE         shutdown = INVALID_HANDLE_VALUE;
           DWORD          tid      = 0UL;
           DWORD          cookie   = 0UL;
  volatile LONG           ready    = FALSE;
};

SK_LazyGlobal <budget_thread_params_t> budget_thread;

void
SK_DXGI_SignalBudgetThread (void)
{
  if (budget_thread->manual != INVALID_HANDLE_VALUE)
    SetEvent (budget_thread->manual);
}

bool
WINAPI
SK_DXGI_IsTrackingBudget (void) noexcept
{
  return
    ( budget_thread->tid != 0UL );
}


HRESULT
SK::DXGI::StartBudgetThread ( IDXGIAdapter** ppAdapter )
{
  if (ppAdapter == nullptr)
    return DXGI_ERROR_INVALID_CALL;

  //
  // If the adapter implements DXGI 1.4, then create a budget monitoring
  //  thread...
  //
  IDXGIAdapter3* pAdapter3 = nullptr;
  HRESULT        hr        = E_NOTIMPL;

  if (SUCCEEDED ((*ppAdapter)->QueryInterface <IDXGIAdapter3> (&pAdapter3)) && pAdapter3 != nullptr)
  {
    // We darn sure better not spawn multiple threads!
    std::scoped_lock <SK_Thread_HybridSpinlock> auto_lock (*budget_mutex);

    if ( budget_thread->handle == INVALID_HANDLE_VALUE )
    {

      RtlZeroMemory ( budget_thread.getPtr (),
                     sizeof budget_thread_params_t );

      dll_log->LogEx ( true,
                         L"[ DXGI 1.4 ]   "
                         L"$ Spawning Memory Budget Change Thread..: " );

      WriteRelease ( &budget_thread->ready,
                       FALSE );

      budget_thread->pAdapter = pAdapter3;
      budget_thread->tid      = 0;
      budget_log->silent      = true;
      budget_thread->event    =
        SK_CreateEvent ( nullptr,
                           FALSE,
                             TRUE, nullptr );
                               //L"DXGIMemoryBudget" );
      budget_thread->manual   =
        SK_CreateEvent ( nullptr,
                           FALSE,
                             TRUE, nullptr );
                               //L"DXGIMemoryWakeUp!" );
      budget_thread->shutdown =
        SK_CreateEvent ( nullptr,
                           TRUE,
                             FALSE, nullptr );
                               //L"DXGIMemoryBudget_Shutdown" );

      budget_thread->handle =
        SK_Thread_CreateEx
          ( BudgetThread,
              nullptr,
                (LPVOID)budget_thread.getPtr () );


      SK_Thread_SpinUntilFlagged (&budget_thread->ready);


      if ( budget_thread->tid != 0 )
      {
        dll_log->LogEx ( false,
                           L"tid=0x%04x\n",
                             budget_thread->tid );

        dll_log->LogEx ( true,
                           L"[ DXGI 1.4 ]   "
                             L"%% Setting up Budget Change Notification.: " );

        HRESULT result =
          pAdapter3->RegisterVideoMemoryBudgetChangeNotificationEvent (
                            budget_thread->event,
                           &budget_thread->cookie
          );

        if ( SUCCEEDED ( result ) )
        {
          dll_log->LogEx ( false,
                             L"eid=0x%08" _L(PRIxPTR) L", cookie=%u\n",
                               (uintptr_t)budget_thread->event,
                                          budget_thread->cookie );

          hr = S_OK;
        }

        else
        {
          dll_log->LogEx ( false,
                             L"Failed! (%s)\n",
                               SK_DescribeHRESULT ( result ) );

          hr = result;
        }
      }

      else
      {
        dll_log->LogEx (false, L"failed!\n");

        hr = E_FAIL;
      }

      dll_log->LogEx ( true,
                         L"[ DXGI 1.2 ] GPU Scheduling...:"
                                      L" Pre-Emptive" );

      DXGI_QUERY_VIDEO_MEMORY_INFO
              _mem_info = { };
      DXGI_ADAPTER_DESC2
                  desc2 = { };

      int      i      = 0;
      bool     silent = dll_log->silent;
      dll_log->silent = true;
      {
        // Don't log this call, because that would be silly...
        pAdapter3->GetDesc2 ( &desc2 );
      }
      dll_log->silent = silent;


      switch ( desc2.GraphicsPreemptionGranularity )
      {
        case DXGI_GRAPHICS_PREEMPTION_DMA_BUFFER_BOUNDARY:
          dll_log->LogEx ( false, L" (DMA Buffer)\n"         );
          break;

        case DXGI_GRAPHICS_PREEMPTION_PRIMITIVE_BOUNDARY:
          dll_log->LogEx ( false, L" (Graphics Primitive)\n" );
          break;

        case DXGI_GRAPHICS_PREEMPTION_TRIANGLE_BOUNDARY:
          dll_log->LogEx ( false, L" (Triangle)\n"           );
          break;

        case DXGI_GRAPHICS_PREEMPTION_PIXEL_BOUNDARY:
          dll_log->LogEx ( false, L" (Fragment)\n"           );
          break;

        case DXGI_GRAPHICS_PREEMPTION_INSTRUCTION_BOUNDARY:
          dll_log->LogEx ( false, L" (Instruction)\n"        );
          break;

        default:
          dll_log->LogEx (false, L"UNDEFINED\n");
          break;
      }

      dll_log->LogEx ( true,
                        L"[ DXGI 1.4 ] Local Memory.....:" );

      while ( SUCCEEDED (
                pAdapter3->QueryVideoMemoryInfo (
                  i,
                    DXGI_MEMORY_SEGMENT_GROUP_LOCAL,
                      &_mem_info
                )
              )
            )
      {
        if (i >= MAX_GPU_NODES)
        {
          dll_log->LogEx ( true, L"[ DXGI 1.4 ] Too many devices, stopping at %lu...",
                                   MAX_GPU_NODES );
          break;
        }

        if ( i > 0 )
        {
          dll_log->LogEx ( false, L"\n"                              );
          dll_log->LogEx ( true,  L"                               " );
        }

        dll_log->LogEx ( false,
                           L" Node%i       (Reserve: %#5llu / %#5llu MiB - "
                                          L"Budget: %#5llu / %#5llu MiB)",
                             i++,
                               _mem_info.CurrentReservation      >> 20ULL,
                               _mem_info.AvailableForReservation >> 20ULL,
                               _mem_info.CurrentUsage            >> 20ULL,
                               _mem_info.Budget                  >> 20ULL
                       );

        if (config.mem.reserve > 0.0f)
        {
          pAdapter3->SetVideoMemoryReservation (
                ( i - 1 ),
                  DXGI_MEMORY_SEGMENT_GROUP_LOCAL,
                    ( i == 1 ) ?
                      uint64_t ( _mem_info.AvailableForReservation *
                      static_cast <double> (config.mem.reserve) * 0.01 )
                             :
                             0
          );
        }
      }

      i = 0;

      dll_log->LogEx ( false, L"\n"                              );
      dll_log->LogEx ( true,  L"[ DXGI 1.4 ] Non-Local Memory.:" );

      while ( SUCCEEDED (
                pAdapter3->QueryVideoMemoryInfo (
                  i,
                    DXGI_MEMORY_SEGMENT_GROUP_NON_LOCAL,
                      &_mem_info
                )
              )
            )
      {
        if (i >= MAX_GPU_NODES)
        {
          dll_log->LogEx ( true, L"[ DXGI 1.4 ] Too many devices, stopping at %lu...",
                                   MAX_GPU_NODES );
          break;
        }

        if ( i > 0 )
        {
          dll_log->LogEx ( false, L"\n"                              );
          dll_log->LogEx ( true,  L"                               " );
        }

        dll_log->LogEx ( false,
                           L" Node%i       (Reserve: %#5llu / %#5llu MiB - "
                                           L"Budget: %#5llu / %#5llu MiB)",
                             i++,
                               _mem_info.CurrentReservation      >> 20ULL,
                               _mem_info.AvailableForReservation >> 20ULL,
                               _mem_info.CurrentUsage            >> 20ULL,
                               _mem_info.Budget                  >> 20ULL
                       );

        if (config.mem.reserve > .0f)
        {
          pAdapter3->SetVideoMemoryReservation (
                ( i - 1 ),
                  DXGI_MEMORY_SEGMENT_GROUP_NON_LOCAL,
                    ( i == 1 ) ?
                      uint64_t ( _mem_info.AvailableForReservation *
                      static_cast <double> (config.mem.reserve) * 0.01 )
                             :
                             0
          );
        }
      }

      dxgi_mem_info [0].nodes = ( i - 1 );
      dxgi_mem_info [1].nodes = ( i - 1 );

      dll_log->LogEx ( false, L"\n" );
    }
  }

  return hr;
}

#define min_max(ref,min,max) if ((ref) > (max)) (max) = (ref); \
                             if ((ref) < (min)) (min) = (ref);

static constexpr uint32_t
  BUDGET_POLL_INTERVAL = 133UL; // How often to sample the budget
                                //  in msecs

HANDLE __SK_DXGI_BudgetChangeEvent = INVALID_HANDLE_VALUE;

DWORD
WINAPI
SK::DXGI::BudgetThread ( LPVOID user_data )
{
  SetCurrentThreadDescription (L"[SK] DXGI Budget Tracker");

  auto* params =
    static_cast <budget_thread_params_t *> (user_data);

  budget_log->silent = true;
  params->tid        = SK_Thread_GetCurrentId ();

  InterlockedExchangeAcquire ( &params->ready, TRUE );


  SK_AutoCOMInit auto_com;


  SetThreadPriority      ( SK_GetCurrentThread (), THREAD_PRIORITY_LOWEST );
  SetThreadPriorityBoost ( SK_GetCurrentThread (), TRUE                   );

  DWORD dwWaitStatus = DWORD_MAX;

  while ( ReadAcquire ( &params->ready ) )
  {
    if (ReadAcquire (&__SK_DLL_Ending))
      break;

    if ( params->event == nullptr )
      break;

    __SK_DXGI_BudgetChangeEvent = params->event;

    HANDLE phEvents [] = {
      params->event, params->shutdown,
      params->manual // Update memory totals, but not due to a budget change
    };

    dwWaitStatus =
      WaitForMultipleObjects ( 3,
                                 phEvents,
                                   FALSE,
                                     INFINITE );

    if (! ReadAcquire ( &params->ready ) )
    {
      break;
    }

    if (dwWaitStatus == WAIT_OBJECT_0 + 1)
    {
      WriteRelease ( &params->ready, FALSE );
      ResetEvent   (  params->shutdown     );
      break;
    }


    //static DWORD dwLastEvict = 0;
    //static DWORD dwLastTest  = 0;
    //static DWORD dwLastSize  = SK_D3D11_Textures.Entries_2D.load ();
    //
    //DWORD dwNow = SK_timeGetTime ();
    //
    //if ( ( SK_D3D11_Textures.Evicted_2D.load () != dwLastEvict ||
    //       SK_D3D11_Textures.Entries_2D.load () != dwLastSize     ) &&
    //                                        dwLastTest < dwNow - 750UL )
    ////if (ImGui::Button ("Compute Residency Statistics"))
    //{
    //  dwLastTest  = dwNow;
    //  dwLastEvict = SK_D3D11_Textures.Evicted_2D.load ();
    //  dwLastSize  = SK_D3D11_Textures.Entries_2D.load ();

    int         node = 0;

    buffer_t buffer  =
      dxgi_mem_info [node].buffer;


    SK_D3D11_Textures->Budget = dxgi_mem_info [buffer].local [0].Budget -
                                dxgi_mem_info [buffer].local [0].CurrentUsage;


    // Double-Buffer Updates
    if ( buffer == Front )
      buffer = Back;
    else
      buffer = Front;


    GetLocalTime ( &dxgi_mem_info [buffer].time );

    //
    // Sample Fast nUMA (On-GPU / Dedicated) Memory
    //
    for ( node = 0; node < MAX_GPU_NODES; )
    {
      int next_node =
               node + 1;

      if ( FAILED (
             params->pAdapter->QueryVideoMemoryInfo (
               node,
                 DXGI_MEMORY_SEGMENT_GROUP_LOCAL,
                   &dxgi_mem_info [buffer].local [node]
             )
           )
         )
      {
        node = next_node;
        break;
      }

      node = next_node;
    }


    // Fix for AMD drivers, that don't allow querying non-local memory
    int nodes =
      std::max (0, node - 1);

    //
    // Sample Slow nUMA (Off-GPU / Shared) Memory
    //
    for ( node = 0; node < MAX_GPU_NODES; )
    {
      int next_node =
               node + 1;

      if ( FAILED (
             params->pAdapter->QueryVideoMemoryInfo (
               node,
                 DXGI_MEMORY_SEGMENT_GROUP_NON_LOCAL,
                   &dxgi_mem_info [buffer].nonlocal [node]
             )
           )
         )
      {
        node = next_node;
        break;
      }

      node = next_node;
    }


    // Set the number of SLI/CFX Nodes
    dxgi_mem_info [buffer].nodes = nodes;

    static uint64_t
      last_budget = dxgi_mem_info [buffer].local [0].Budget;


    if ( nodes > 0 )
    {
      int i;

      budget_log->LogEx ( true,
                            L"[ DXGI 1.4 ] Local Memory.....:" );

      for ( i = 0; i < nodes; i++ )
      {
        if ( dwWaitStatus == WAIT_OBJECT_0 )
        {
          static UINT64
            LastBudget = 0ULL;

          dxgi_mem_stats [i].budget_changes++;

          const int64_t over_budget =
            ( dxgi_mem_info [buffer].local [i].CurrentUsage -
              dxgi_mem_info [buffer].local [i].Budget );

            //LastBudget -
            //dxgi_mem_info [buffer].local [i].Budget;

          SK_D3D11_need_tex_reset = ( over_budget > 0 );

          LastBudget =
            dxgi_mem_info [buffer].local [i].Budget;
        }

        if ( i > 0 )
        {
          budget_log->LogEx ( false, L"\n"                                );
          budget_log->LogEx ( true,  L"                                 " );
        }

        budget_log->LogEx (
          false,
            L" Node%i (Reserve: %#5llu / %#5llu MiB - "
                      L"Budget: %#5llu / %#5llu MiB)",
          i,
            dxgi_mem_info       [buffer].local [i].CurrentReservation      >> 20ULL,
              dxgi_mem_info     [buffer].local [i].AvailableForReservation >> 20ULL,
                dxgi_mem_info   [buffer].local [i].CurrentUsage            >> 20ULL,
                  dxgi_mem_info [buffer].local [i].Budget                  >> 20ULL
        );

        min_max ( dxgi_mem_info [buffer].local [i].AvailableForReservation,
                                dxgi_mem_stats [i].min_avail_reserve,
                                dxgi_mem_stats [i].max_avail_reserve );

        min_max ( dxgi_mem_info [buffer].local [i].CurrentReservation,
                                dxgi_mem_stats [i].min_reserve,
                                dxgi_mem_stats [i].max_reserve );

        min_max ( dxgi_mem_info [buffer].local [i].CurrentUsage,
                                dxgi_mem_stats [i].min_usage,
                                dxgi_mem_stats [i].max_usage );

        min_max ( dxgi_mem_info [buffer].local [i].Budget,
                                dxgi_mem_stats [i].min_budget,
                                dxgi_mem_stats [i].max_budget );

        int64_t available =
          dxgi_mem_info [buffer].local [i].Budget -
          dxgi_mem_info [buffer].local [i].CurrentUsage;

        dxgi_mem_stats [i].min_availability =
          std::min (dxgi_mem_stats [i].min_availability, available);

        dxgi_mem_stats [i].max_load_percent =
          std::max (dxgi_mem_stats [i].max_load_percent,
            static_cast <double> (dxgi_mem_info [buffer].local [i].CurrentUsage) /
            static_cast <double> (dxgi_mem_info [buffer].local [i].Budget) * 100.0);

        if ( dxgi_mem_info [buffer].local [i].CurrentUsage >
             dxgi_mem_info [buffer].local [i].Budget)
        {
          uint64_t over_budget =
             ( dxgi_mem_info [buffer].local [i].CurrentUsage -
               dxgi_mem_info [buffer].local [i].Budget );

          min_max ( over_budget,
                           dxgi_mem_stats [i].min_over_budget,
                           dxgi_mem_stats [i].max_over_budget );
        }
      }

      budget_log->LogEx ( false, L"\n"                              );
      budget_log->LogEx ( true,  L"[ DXGI 1.4 ] Non-Local Memory.:" );

      for ( i = 0; i < nodes; i++ )
      {
        if ( i > 0 )
        {
          budget_log->LogEx ( false, L"\n"                                );
          budget_log->LogEx ( true,  L"                                 " );
        }

        budget_log->LogEx (
          false,
            L" Node%i (Reserve: %#5llu / %#5llu MiB - "
                      L"Budget: %#5llu / %#5llu MiB)",    i,
         dxgi_mem_info    [buffer].nonlocal [i].CurrentReservation      >> 20ULL,
          dxgi_mem_info   [buffer].nonlocal [i].AvailableForReservation >> 20ULL,
           dxgi_mem_info  [buffer].nonlocal [i].CurrentUsage            >> 20ULL,
            dxgi_mem_info [buffer].nonlocal [i].Budget                  >> 20ULL );
      }

      budget_log->LogEx ( false, L"\n" );
    }

    dxgi_mem_info [0].buffer =
                      buffer;


    const SK_RenderBackend_V2 &rb =
      SK_GetCurrentRenderBackend ();


    DXGI_ADAPTER_DESC           activeDesc = { };
    SK_ComQIPtr <IDXGIDevice>  pDXGIDevice (rb.device);
    SK_ComPtr   <IDXGIAdapter> pActiveAdapter;

    if (pDXGIDevice != nullptr)
    {   pDXGIDevice->GetAdapter (&pActiveAdapter);

      if (pActiveAdapter != nullptr)
          pActiveAdapter->GetDesc (&activeDesc);
    }

    //
    // Consistency check, re-initialize the budget thread if
    //   we are not monitoring the correct DXGI adapter.
    //
    if (activeDesc.AdapterLuid.LowPart != 0x0)
    {
      auto pAdapter =
        params->pAdapter;

      DXGI_ADAPTER_DESC   adapterDesc = {};
      pAdapter->GetDesc (&adapterDesc);

      if ( adapterDesc.AdapterLuid.HighPart != activeDesc.AdapterLuid.HighPart ||
           adapterDesc.AdapterLuid.LowPart  != activeDesc.AdapterLuid.LowPart )
      {
        auto silent =
          std::exchange (budget_log->silent, false);

        budget_log->Log (
          L"DXGI Budget Thread Monitoring Wrong Adapter (%08X:%08X)!\r\n",
                            adapterDesc.AdapterLuid.HighPart,
                            adapterDesc.AdapterLuid.LowPart );

        SK_ComPtr <IDXGIFactory4>           pFactory4 = nullptr;
        pAdapter->GetParent (IID_PPV_ARGS (&pFactory4.p));

        if (pFactory4 != nullptr)
        {
          SK_ComPtr <IDXGIAdapter3>                pNewAdapter = nullptr;
          pFactory4->EnumAdapterByLuid (
            activeDesc.AdapterLuid, IID_PPV_ARGS (&pNewAdapter.p) );

          if (pNewAdapter != nullptr)
          {
            DXGI_ADAPTER_DESC      newAdapterDesc = {};
            pNewAdapter->GetDesc (&newAdapterDesc);

            budget_log->Log (
              L"Transitioning To Another Adapter:" );
            
            budget_log->Log (
              L"  Original: (%08X:%08X, %ws)",
                  adapterDesc.AdapterLuid.HighPart,
                  adapterDesc.AdapterLuid.LowPart,
                  adapterDesc.Description );

            budget_log->Log (
              L"  Current:  (%08X:%08X, %ws)\r\n",
                  newAdapterDesc.AdapterLuid.HighPart,
                  newAdapterDesc.AdapterLuid.LowPart,
                  newAdapterDesc.Description );

            SK_ComPtr <IDXGIAdapter3>
              pOldAdapter (pAdapter);

            pOldAdapter->UnregisterVideoMemoryBudgetChangeNotification (
              params->cookie );

            pNewAdapter->RegisterVideoMemoryBudgetChangeNotificationEvent (
               params->event,
              &params->cookie );

            params->pAdapter =
              pNewAdapter.Detach ();

            pOldAdapter.p->Release ();
          }
        }

        budget_log->silent = silent;
      }
    }
  }


  return 0;
}

HRESULT
SK::DXGI::StartBudgetThread_NoAdapter (void)
{
  HRESULT hr =
    E_NOTIMPL;

  SK_AutoCOMInit auto_com;

  static HMODULE
    hDXGI = SK_Modules->LoadLibraryLL ( L"dxgi.dll" );

  if (hDXGI)
  {
    static auto
      CreateDXGIFactory2 = CreateDXGIFactory2_Import != nullptr ?
                           CreateDXGIFactory2_Import            :
        (CreateDXGIFactory2_pfn) SK_GetProcAddress ( hDXGI,
                                                       "CreateDXGIFactory2" );

    // If we somehow got here on Windows 7, get the hell out immediately!
    if (! CreateDXGIFactory2)
      return hr;

    SK_ComPtr <IDXGIFactory> factory = nullptr;
    SK_ComPtr <IDXGIAdapter> adapter = nullptr;

    // Only spawn the DXGI 1.4 budget thread if ... DXGI 1.4 is implemented.
    if ( SUCCEEDED (
           CreateDXGIFactory2 ( 0x0, IID_PPV_ARGS (&factory.p) )
         )
       )
    {
      if ( SUCCEEDED (
             factory->EnumAdapters ( 0, &adapter.p )
           )
         )
      {
        hr =
          StartBudgetThread ( &adapter.p );
      }
    }
  }

  return hr;
}

void
SK::DXGI::ShutdownBudgetThread ( void )
{
  if (                                     budget_thread->handle != INVALID_HANDLE_VALUE &&
       InterlockedCompareExchangeRelease (&budget_thread->ready, 0, 1) )
  {
    dll_log->LogEx (
      true,
        L"[ DXGI 1.4 ] Shutting down Memory Budget Change Thread... "
    );

    DWORD dwTime =
         SK_timeGetTime ();

    DWORD dwWaitState =
      SignalObjectAndWait ( budget_thread->shutdown,
                              budget_thread->handle, // Give .25 seconds, and
                                250UL,               // then we're killing
                                  TRUE );            // the thing!

    if (budget_thread->shutdown != INVALID_HANDLE_VALUE)
    {
      SK_CloseHandle (budget_thread->shutdown);
                      budget_thread->shutdown = INVALID_HANDLE_VALUE;
    }

    if ( dwWaitState == WAIT_OBJECT_0 )
    {
      dll_log->LogEx  ( false, L"done! (%4u ms)\n",
                                 SK_timeGetTime () - dwTime );
    }

    else
    {
      dll_log->LogEx  ( false, L"timed out (killing manually)!\n" );
      TerminateThread ( budget_thread->handle,                  0 );
    }

    if (budget_thread->handle != INVALID_HANDLE_VALUE)
    {
      SK_CloseHandle ( budget_thread->handle );
                       budget_thread->handle = INVALID_HANDLE_VALUE;
    }

    SK_AutoClose_LogEx (budget_log, budget);

    // Record the final statistics always
    budget_log->silent    = false;

    budget_log->Log   ( L"--------------------"   );
    budget_log->Log   ( L"Shutdown Statistics:"   );
    budget_log->Log   ( L"--------------------\n" );

    // in %10u seconds\n",
    budget_log->Log ( L" Memory Budget Changed %llu times\n",
                        dxgi_mem_stats [0].budget_changes );

    for ( int i = 0; i < MAX_GPU_NODES; i++ )
    {
      if ( dxgi_mem_stats [i].max_usage > 0 )
      {
        if ( dxgi_mem_stats [i].min_reserve     == UINT64_MAX )
             dxgi_mem_stats [i].min_reserve     =  0ULL;

        if ( dxgi_mem_stats [i].min_over_budget == UINT64_MAX )
             dxgi_mem_stats [i].min_over_budget =  0ULL;

        if ( dxgi_mem_stats [i].min_availability == INT64_MAX )
             dxgi_mem_stats [i].min_availability = dxgi_mem_info->local [i].Budget;

        budget_log->LogEx ( true,
                             L" GPU%i: Min Budget:        %05llu MiB\n",
                                          i,
                               dxgi_mem_stats [i].min_budget >> 20ULL );
        budget_log->LogEx ( true,
                             L"       Max Budget:        %05llu MiB\n",
                               dxgi_mem_stats [i].max_budget >> 20ULL );

        budget_log->LogEx ( true,
                             L"       Min Usage:         %05llu MiB\n",
                               dxgi_mem_stats [i].min_usage  >> 20ULL );
        budget_log->LogEx ( true,
                             L"       Max Usage:         %05llu MiB\n",
                               dxgi_mem_stats [i].max_usage  >> 20ULL );
        budget_log->LogEx ( true,
                             L"====================================\n");
        budget_log->LogEx ( true,
                             L"  Minimum Available:      %05lli MiB\n",
                               dxgi_mem_stats [i].min_availability
                                                  / (1024LL * 1024LL) );
        budget_log->LogEx ( true,
                             L"  Maximum Load:           %#8.2f%%\n",
                               dxgi_mem_stats [i].max_load_percent    );

        /*
        SK_BLogEx (params, true, L"       Min Reserve:       %05u MiB\n",
        mem_stats [i].min_reserve >> 20ULL);
        SK_BLogEx (params, true, L"       Max Reserve:       %05u MiB\n",
        mem_stats [i].max_reserve >> 20ULL);
        SK_BLogEx (params, true, L"       Min Avail Reserve: %05u MiB\n",
        mem_stats [i].min_avail_reserve >> 20ULL);
        SK_BLogEx (params, true, L"       Max Avail Reserve: %05u MiB\n",
        mem_stats [i].max_avail_reserve >> 20ULL);
        */

        budget_log->LogEx ( true,  L"====================================\n" );
        if ( dxgi_mem_stats [i].min_over_budget == 0 &&
             dxgi_mem_stats [i].max_over_budget == 0 )
        {
          budget_log->LogEx ( true,L"         No Budget Deficits        \n"  );
        }
        else
        {
          budget_log->LogEx ( true,L" Minimum Over Budget:     %05llu MiB\n",
                                dxgi_mem_stats [i].min_over_budget >> 20ULL  );
          budget_log->LogEx ( true,L" Maximum Over Budget:     %05llu MiB\n",
                                dxgi_mem_stats [i].max_over_budget >> 20ULL  );
        }
        budget_log->LogEx ( true,  L"------------------------------------\n" );
        budget_log->LogEx ( false, L"\n"                                     );
      }
    }
  }
}









bool
SK_D3D11_QuickHooked (void);

void
SK_DXGI_QuickHook (void)
{
  SK_COMPAT_CheckStreamlineSupport ();

  // We don't want to hook this, and we certainly don't want to hook it using
  //   cached addresses!
  if (! (config.apis.dxgi.d3d11.hook ||
         config.apis.dxgi.d3d12.hook))
    return;

  if (config.steam.preload_overlay)
    return;

  if (config.render.dxgi.debug_layer)
    return;


  if (__SK_DisableQuickHook)
    return;


  if ( PathFileExistsW (L"dxgi.dll") ||
       PathFileExistsW (L"d3d11.dll") )
  {
    SK_LOGi0 (L" # DXGI QuickHook disabled because a local dxgi.dll or d3d11.dll is present...");

    __SK_DisableQuickHook = TRUE;
  }

  if (config.compatibility.init_sync_for_reshade || SK_ReShade_IsLocalDLLPresent ())
  {
    SK_LOGi0 (L" # DXGI QuickHook disabled because a ReShade Plug-In is present...");

    //// Implicitly load ReShade (ReShade{32|64}.dll) if it exists
    //SK_ReShade_LoadIfPresent ();

    __SK_DisableQuickHook = TRUE;
  }

  if ( SK_IsModuleLoaded (L"sl.interposer.dll") )
  {
    SK_LOGi0 (L" # DXGI QuickHook disabled because an NVIDIA Streamline Interposer is present...");

    __SK_DisableQuickHook = TRUE;
  }


  if (__SK_DisableQuickHook)
    return;


  static volatile LONG                      quick_hooked    =   FALSE;
  if (! InterlockedCompareExchangeAcquire (&quick_hooked, TRUE, FALSE))
  {
    SK_D3D11_QuickHook ();

    sk_hook_cache_enablement_s state =
      SK_Hook_PreCacheModule ( L"DXGI",
                                 local_dxgi_records,
                                   global_dxgi_records );

    if ( state.hooks_loaded.from_shared_dll > 0 ||
         state.hooks_loaded.from_game_ini   > 0 ||
         SK_D3D11_QuickHooked () )
    {
      //bool bEnable =
      //  SK_EnableApplyQueuedHooks ();

      SK_ApplyQueuedHooks ();

      //if (! bEnable)
      //  SK_DisableApplyQueuedHooks ();
    }

    else
    {
      for ( auto& it : local_dxgi_records )
      {
        it->active = false;
      }
    }

    InterlockedIncrementRelease (&quick_hooked);
  }
}


// Returns true if the selected mode scaling would cause an exact match to be ignored
bool
SK_DXGI_IsScalingPreventingRequestedResolution ( DXGI_MODE_DESC *pDesc,
                                                 IDXGIOutput    *pOutput,
                                                 IUnknown       *pDevice )
{
  if (pOutput == nullptr || pDesc == nullptr)
    return false; // Hell if I know, pass valid parameters next time!

  if (pDesc->Scaling == DXGI_MODE_SCALING_UNSPECIFIED)
    return false;

  auto desc_no_scaling         = *pDesc;
       desc_no_scaling.Scaling =  DXGI_MODE_SCALING_UNSPECIFIED;

  DXGI_MODE_DESC matchedMode_NoScaling = { },
                 matchedMode           = { };

  if ( SUCCEEDED (
         pOutput->FindClosestMatchingMode (
           &desc_no_scaling, &matchedMode_NoScaling, pDevice )
       ) &&
       SUCCEEDED (
         pOutput->FindClosestMatchingMode (
           pDesc,            &matchedMode,           pDevice )
       )
     )
  {
    bool prevented =
      ( matchedMode_NoScaling.Width  != matchedMode.Width ||
        matchedMode_NoScaling.Height != matchedMode.Height );

    SK_GetCurrentRenderBackend ().active_traits.bMisusingDXGIScaling |= prevented;

    return prevented;
  }

  return false;
}