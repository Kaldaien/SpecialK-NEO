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
#include <SpecialK/resource.h>

#define _L2(w)  L ## w
#define  _L(w) _L2(w)

#ifndef __SK_SUBSYSTEM__
#define __SK_SUBSYSTEM__ L"   D3D9   "
#endif

#include <SpecialK/render/d3d9/d3d9_device.h>
#include <SpecialK/render/d3d9/d3d9_screenshot.h>

MIDL_INTERFACE("D0223B96-BF7A-43fd-92BD-A43B0D82B9EB") IDirect3DDevice9;
MIDL_INTERFACE("B18B10CE-2649-405a-870F-95F777D4313A") IDirect3DDevice9Ex;

#include <imgui/backends/imgui_d3d9.h>


#include <d3d9.h>
#include <SpecialK/nvapi.h>

#include <filesystem>

using namespace SK::D3D9;


volatile LONG               __d3d9_ready = FALSE;

volatile LONG ImGui_Init = FALSE;

SK_LazyGlobal <PipelineStatsD3D9> SK::D3D9::pipeline_stats_d3d9;

using Direct3DCreate9_pfn   =
  IDirect3D9* (STDMETHODCALLTYPE *)(UINT           SDKVersion);
using Direct3DCreate9Ex_pfn =
  HRESULT     (STDMETHODCALLTYPE *)(UINT           SDKVersion,
                                    IDirect3D9Ex** d3d9ex);


#define SK_D3D9_DeclTrampoline(apiset,func) \
apiset##_##func##_pfn apiset##_##func##_Original = nullptr;

#define SK_D3D9_Trampoline(apiset,func) apiset##_##func##_Original
#define SK_D3D9_TrampolineForVFTblHook(apiset,func) SK_D3D9_Trampoline(apiset,func), apiset##_##func##_pfn

Direct3DCreate9_pfn
Direct3DCreate9_Import   = nullptr;

Direct3DCreate9Ex_pfn
Direct3DCreate9Ex_Import = nullptr;

Direct3DCreate9On12Ex_pfn
Direct3DCreate9On12Ex    = nullptr;

SK_D3D9_DeclTrampoline (D3D9,         CreateDevice)
SK_D3D9_DeclTrampoline (D3D9Ex,       CreateDevice)
SK_D3D9_DeclTrampoline (D3D9Ex,       CreateDeviceEx)

SK_D3D9_DeclTrampoline (D3D9Device,   Present)
SK_D3D9_DeclTrampoline (D3D9Device,   Reset)
SK_D3D9_DeclTrampoline (D3D9Device,   SetGammaRamp)
SK_D3D9_DeclTrampoline (D3D9Device,   CreateAdditionalSwapChain)
SK_D3D9_DeclTrampoline (D3D9Device,   TestCooperativeLevel)
SK_D3D9_DeclTrampoline (D3D9Device,   BeginScene)
SK_D3D9_DeclTrampoline (D3D9Device,   EndScene)
SK_D3D9_DeclTrampoline (D3D9Device,   DrawPrimitive)
SK_D3D9_DeclTrampoline (D3D9Device,   DrawIndexedPrimitive)
SK_D3D9_DeclTrampoline (D3D9Device,   DrawPrimitiveUP)
SK_D3D9_DeclTrampoline (D3D9Device,   DrawIndexedPrimitiveUP)
SK_D3D9_DeclTrampoline (D3D9Device,   GetTexture)
SK_D3D9_DeclTrampoline (D3D9Device,   SetTexture)
SK_D3D9_DeclTrampoline (D3D9Device,   SetSamplerState)
SK_D3D9_DeclTrampoline (D3D9Device,   SetViewport)
SK_D3D9_DeclTrampoline (D3D9Device,   SetRenderState)
SK_D3D9_DeclTrampoline (D3D9Device,   SetVertexShaderConstantF)
SK_D3D9_DeclTrampoline (D3D9Device,   SetPixelShaderConstantF)
SK_D3D9_DeclTrampoline (D3D9Device,   SetPixelShader)
SK_D3D9_DeclTrampoline (D3D9Device,   SetVertexShader)
SK_D3D9_DeclTrampoline (D3D9Device,   SetScissorRect)
SK_D3D9_DeclTrampoline (D3D9Device,   CreateTexture)
SK_D3D9_DeclTrampoline (D3D9Device,   CreateVolumeTexture)
SK_D3D9_DeclTrampoline (D3D9Device,   CreateCubeTexture)
SK_D3D9_DeclTrampoline (D3D9Device,   CreateVertexBuffer)
SK_D3D9_DeclTrampoline (D3D9Device,   CreateIndexBuffer)
SK_D3D9_DeclTrampoline (D3D9Device,   SetStreamSource)
SK_D3D9_DeclTrampoline (D3D9Device,   SetStreamSourceFreq)
SK_D3D9_DeclTrampoline (D3D9Device,   SetFVF)
SK_D3D9_DeclTrampoline (D3D9Device,   SetVertexDeclaration)
SK_D3D9_DeclTrampoline (D3D9Device,   CreateVertexDeclaration)
SK_D3D9_DeclTrampoline (D3D9Device,   CreateRenderTarget)
SK_D3D9_DeclTrampoline (D3D9Device,   CreateDepthStencilSurface)
SK_D3D9_DeclTrampoline (D3D9Device,   SetRenderTarget)
SK_D3D9_DeclTrampoline (D3D9Device,   SetDepthStencilSurface)
SK_D3D9_DeclTrampoline (D3D9Device,   UpdateTexture)
SK_D3D9_DeclTrampoline (D3D9Device,   StretchRect)
SK_D3D9_DeclTrampoline (D3D9Device,   SetCursorPosition)
SK_D3D9_DeclTrampoline (D3D9Device,   CreateOffscreenPlainSurface)

SK_D3D9_DeclTrampoline (D3D9ExDevice, PresentEx)
SK_D3D9_DeclTrampoline (D3D9ExDevice, ResetEx)

SK_D3D9_DeclTrampoline (D3D9Swap,     Present)


#pragma data_seg (".SK_D3D9_Hooks")
extern "C"
{
  // Global DLL's cache
  __declspec (dllexport) SK_HookCacheEntryGlobal (Direct3DCreate9)
  __declspec (dllexport) SK_HookCacheEntryGlobal (D3D9CreateDevice)
  __declspec (dllexport) SK_HookCacheEntryGlobal (D3D9PresentSwap)
  __declspec (dllexport) SK_HookCacheEntryGlobal (D3D9Present)
  __declspec (dllexport) SK_HookCacheEntryGlobal (D3D9Reset)
  __declspec (dllexport) SK_HookCacheEntryGlobal (D3D9CreateAdditionalSwapChain)
  __declspec (dllexport) SK_HookCacheEntryGlobal (D3D9TestCooperativeLevel)
  __declspec (dllexport) SK_HookCacheEntryGlobal (D3D9BeginScene)
  __declspec (dllexport) SK_HookCacheEntryGlobal (D3D9EndScene)

  __declspec (dllexport) SK_HookCacheEntryGlobal (Direct3DCreate9Ex)
  __declspec (dllexport) SK_HookCacheEntryGlobal (D3D9CreateDeviceEx)
  __declspec (dllexport) SK_HookCacheEntryGlobal (D3D9PresentEx)
  __declspec (dllexport) SK_HookCacheEntryGlobal (D3D9ResetEx)
};
#pragma data_seg ()
#pragma comment  (linker, "/SECTION:.SK_D3D9_Hooks,RWS")

// Local DLL's cached addresses
SK_HookCacheEntryLocal (Direct3DCreate9,               L"d3d9.dll", Direct3DCreate9,                        &Direct3DCreate9_Import)
SK_HookCacheEntryLocal (D3D9CreateDevice,              L"d3d9.dll", D3D9CreateDevice_Override,              &SK_D3D9_Trampoline (D3D9,       CreateDevice))
SK_HookCacheEntryLocal (D3D9PresentSwap,               L"d3d9.dll", D3D9Swap_Present,                       &SK_D3D9_Trampoline (D3D9Swap,   Present))
SK_HookCacheEntryLocal (D3D9Present,                   L"d3d9.dll", D3D9Device_Present,                     &SK_D3D9_Trampoline (D3D9Device, Present))
SK_HookCacheEntryLocal (D3D9Reset,                     L"d3d9.dll", D3D9Reset_Override,                     &SK_D3D9_Trampoline (D3D9Device, Reset))
SK_HookCacheEntryLocal (D3D9CreateAdditionalSwapChain, L"d3d9.dll", D3D9CreateAdditionalSwapChain_Override, &SK_D3D9_Trampoline (D3D9Device, CreateAdditionalSwapChain))
SK_HookCacheEntryLocal (D3D9TestCooperativeLevel,      L"d3d9.dll", D3D9TestCooperativeLevel_Override,      &SK_D3D9_Trampoline (D3D9Device, TestCooperativeLevel))
SK_HookCacheEntryLocal (D3D9BeginScene,                L"d3d9.dll", D3D9BeginScene_Override,                &SK_D3D9_Trampoline (D3D9Device, BeginScene))
SK_HookCacheEntryLocal (D3D9EndScene,                  L"d3d9.dll", D3D9EndScene_Override,                  &SK_D3D9_Trampoline (D3D9Device, EndScene))

SK_HookCacheEntryLocal (Direct3DCreate9Ex,             L"d3d9.dll", Direct3DCreate9Ex,                      &Direct3DCreate9Ex_Import)
SK_HookCacheEntryLocal (D3D9CreateDeviceEx,            L"d3d9.dll", D3D9CreateDeviceEx_Override,            &SK_D3D9_Trampoline (D3D9Ex,       CreateDeviceEx))
SK_HookCacheEntryLocal (D3D9PresentEx,                 L"d3d9.dll", D3D9ExDevice_PresentEx,                 &SK_D3D9_Trampoline (D3D9ExDevice, PresentEx))
SK_HookCacheEntryLocal (D3D9ResetEx,                   L"d3d9.dll", D3D9ResetEx,                            &SK_D3D9_Trampoline (D3D9ExDevice, ResetEx))

static
sk_hook_cache_array global_d3d9_records =
  { &GlobalHook_D3D9BeginScene,           &GlobalHook_D3D9EndScene,
    //&GlobalHook_D3D9Reset,                &GlobalHook_D3D9ResetEx,

    &GlobalHook_D3D9Present,
    &GlobalHook_D3D9PresentEx,
    &GlobalHook_D3D9PresentSwap,          &GlobalHook_D3D9CreateAdditionalSwapChain,

    //&GlobalHook_D3D9TestCooperativeLevel,

    &GlobalHook_Direct3DCreate9,          &GlobalHook_Direct3DCreate9Ex,
    &GlobalHook_D3D9CreateDevice,         &GlobalHook_D3D9CreateDeviceEx  };

static
sk_hook_cache_array local_d3d9_records =
  {  &LocalHook_D3D9BeginScene,           &LocalHook_D3D9EndScene,
     //&LocalHook_D3D9Reset,                &LocalHook_D3D9ResetEx,

     &LocalHook_D3D9Present,
     &LocalHook_D3D9PresentEx,
     &LocalHook_D3D9PresentSwap,          &LocalHook_D3D9CreateAdditionalSwapChain,

     //&LocalHook_D3D9TestCooperativeLevel,

     &LocalHook_Direct3DCreate9,          &LocalHook_Direct3DCreate9Ex,
     &LocalHook_D3D9CreateDevice,         &LocalHook_D3D9CreateDeviceEx   };


void
WINAPI
WaitForInit_D3D9 (void)
{
  if (Direct3DCreate9_Import == nullptr)
  {
    SK_RunOnce (SK_BootD3D9 ());
  }

  if (SK_TLS_Bottom ()->render->d3d9->ctx_init_thread == TRUE)
  {   SK_TLS_Bottom ()->render->d3d9->ctx_init_thread = FALSE;
    return;
  }

  if (ReadAcquire (&__d3d9_ready) == FALSE)
    SK_Thread_SpinUntilFlagged (&__d3d9_ready);
}

void
WINAPI
WaitForInit_D3D9Ex (void)
{
  if (Direct3DCreate9_Import == nullptr)
  {
    SK_RunOnce (SK_BootD3D9 ());
  }

  if (SK_TLS_Bottom ()->render->d3d9->ctx_init_thread_ex == TRUE)
  {   SK_TLS_Bottom ()->render->d3d9->ctx_init_thread_ex = FALSE;
    return;
  }

  if (ReadAcquire (&__d3d9_ready) == FALSE)
    SK_Thread_SpinUntilFlagged (&__d3d9_ready);
}

__declspec (noinline)
HRESULT
STDMETHODCALLTYPE
D3D9CreateOffscreenPlainSurface_Override (
  _In_  IDirect3DDevice9   *This,
  _In_  UINT                Width,
  _In_  UINT                Height,
  _In_  D3DFORMAT           Format,
  _In_  D3DPOOL             Pool,
  _Out_ IDirect3DSurface9 **ppSurface,
  _In_  HANDLE             *pSharedHandle )
{
  if (SK_GetCurrentGameID () == SK_GAME_ID::YS_Seven)
  {
    SK_ComPtr <IDirect3DSwapChain9> pSwapChain = nullptr;

    if (SUCCEEDED (This->GetSwapChain (0, &pSwapChain.p)))
    {
      D3DPRESENT_PARAMETERS pparams = { };
      pSwapChain->GetPresentParameters (&pparams);

      if (Format == D3DFMT_R5G6B5 && ( Width == pparams.BackBufferWidth ) )
      {
        Format = D3DFMT_X8R8G8B8;
      }

      if (Format == D3DFMT_D16 && ( Width == pparams.BackBufferWidth ) )
      {
        Format = D3DFMT_D24X8;
      }
    }
  }

  return
    SK_D3D9_Trampoline (D3D9Device, CreateOffscreenPlainSurface)
      ( This, Width, Height, Format, Pool, ppSurface, pSharedHandle );
}


#define D3D9_VIRTUAL_HOOK(_Base,_Index,_Name,_Override,_Original,_Type) {\
  void** _vftable = *(void***)*(_Base);                                  \
                                                                         \
  if ((_Original) == nullptr) {                                          \
    SK_CreateVFTableHook2 ( L##_Name,                                    \
                              _vftable,                                  \
                                (_Index),                                \
                                  (_Override),                           \
                                    (LPVOID *)&(_Original));             \
  }                                                                      \
}



std::unique_ptr <SK_Thread_HybridSpinlock> lock_vs = nullptr;
std::unique_ptr <SK_Thread_HybridSpinlock> lock_ps = nullptr;
std::unique_ptr <SK_Thread_HybridSpinlock> lock_vb = nullptr;

SK_LazyGlobal <KnownObjects>        SK::D3D9::known_objs;
SK_LazyGlobal <KnownShaders>        SK::D3D9::Shaders;
SK_LazyGlobal <ShaderTracker>       SK::D3D9::tracked_vs;
SK_LazyGlobal <ShaderTracker>       SK::D3D9::tracked_ps;

SK_LazyGlobal <VertexBufferTracker> SK::D3D9::tracked_vb;
SK_LazyGlobal <RenderTargetTracker> SK::D3D9::tracked_rt;

SK_LazyGlobal <DrawState>           SK::D3D9::draw_state;
SK_LazyGlobal <FrameState>          SK::D3D9::last_frame;

void
SK::D3D9::VertexBufferTracker::clear (void)
{
  active = false;

  InterlockedExchange (&num_draws, 0);

  instances = 0;

  vertex_shaders.clear ();
  pixel_shaders.clear  ();
  textures.clear       ();

  for ( auto& it : vertex_decls ) it->Release ();

  vertex_decls.clear ();
}

void
SK::D3D9::VertexBufferTracker::use (void)
{
  const SK_RenderBackend& rb =
    SK_GetCurrentRenderBackend ();

  if (! rb.device)
    return;

  std::scoped_lock <SK_Thread_HybridSpinlock>
        scope_lock (*lock_vb);

  IDirect3DVertexDeclaration9* decl = nullptr;

  auto pDev =
    rb.getDevice <IDirect3DDevice9> ();

  if (! pDev)
  {
    return;
  }

  static auto& _Shaders    = Shaders.get    ();
  static auto& _draw_state = draw_state.get ();

  const uint32_t vs_checksum = _Shaders.vertex.current.crc32c;
  const uint32_t ps_checksum = _Shaders.pixel.current.crc32c;

  vertex_shaders.emplace (vs_checksum);
  pixel_shaders.emplace  (ps_checksum);

  if (SUCCEEDED (pDev->GetVertexDeclaration (&decl)))
  {
    static D3DVERTEXELEMENT9 elem_decl [MAXD3DDECLLENGTH];
    static UINT              num_elems;

    // Run through the vertex decl and figure out which samplers have texcoords,
    //   that is a pretty good indicator as to which textures are actually used...
    if (SUCCEEDED (decl->GetDeclaration (elem_decl, &num_elems)))
    {
      for ( UINT i = 0; i < num_elems; i++ )
      {
        if (elem_decl [i].Usage == D3DDECLUSAGE_TEXCOORD)
          textures.emplace (_draw_state.current_tex [elem_decl [i].UsageIndex]);
      }
    }

    if (! vertex_decls.count   (decl))
          vertex_decls.emplace (decl);
    else
      decl->Release ();
  }
}

// For now, let's just focus on stream0 and pretend nothing else exists...
IDirect3DVertexBuffer9* vb_stream0 = nullptr;

SK_LazyGlobal <std::unordered_map <uint32_t, ShaderDisassembly>> ps_disassembly;
SK_LazyGlobal <std::unordered_map <uint32_t, ShaderDisassembly>> vs_disassembly;

bool SK_D3D9_ShouldSkipRenderPass (D3DPRIMITIVETYPE PrimitiveType, UINT PrimitiveCount, UINT StartVertex, bool& wireframe);
void SK_D3D9_EndFrame             (void);
void SK_D3D9_EndScene             (void);
void SK_D3D9_SetPixelShader       ( IDirect3DDevice9       *pDev,
                                    IDirect3DPixelShader9  *pShader );
void SK_D3D9_SetVertexShader      ( IDirect3DDevice9       *pDev,
                                    IDirect3DVertexShader9 *pShader );

static volatile ULONG __gui_reset_d3d9     = TRUE;
static volatile ULONG __imgui_frames_drawn =   0L;

void SK_ImGUI_ResetD3D9 (IDirect3DDevice9* pDev);

void
SK_ImGui_DrawD3D9 (IDirect3DDevice9* pDev, IDirect3DSwapChain9* pSwapChain)
{
  static volatile LONG             __first_frame =       TRUE;
  if (InterlockedCompareExchange (&__first_frame, FALSE, TRUE) == TRUE)
  {
    for ( auto it : local_d3d9_records )
    {
      SK_Hook_ResolveTarget (*it);

      // Don't cache addresses that were screwed with by other injectors
      const wchar_t* wszSection =
        StrStrIW (it->target.module_path, LR"(d3d9.dll)") ?
                                            L"D3D9.Hooks" : nullptr;

      if (! wszSection)
      {
        SK_LOGs0 ( L"Hook Cache",
                   L"Hook for '%hs' resides in '%s', will not cache!",
                      it->target.symbol_name,
       SK_ConcealUserDir (
            std::wstring (
                      it->target.module_path
                         ).data ()
                 )       );
      }

      else
        SK_Hook_CacheTarget ( *it, wszSection );
    }

    if (SK_IsInjected ())
    {
      auto it_local  = std::cbegin (local_d3d9_records);
      auto it_global = std::cbegin (global_d3d9_records);

      while ( it_local != std::cend (local_d3d9_records) )
      {
        if (( *it_local )->hits &&
  StrStrIW (( *it_local )->target.module_path, LR"(d3d9.dll)") &&
            ( *it_local )->active)
          SK_Hook_PushLocalCacheOntoGlobal ( **it_local,
                                               **it_global );
        else
        {
          ( *it_global )->target.addr = nullptr;
          ( *it_global )->hits        = 0;
          ( *it_global )->active      = false;
        }

        it_global++, it_local++;
      }
    }
  }



  static bool init = false;

  InterlockedIncrementAcquire           (&__imgui_frames_drawn);
  if (InterlockedCompareExchangeRelease (&__gui_reset_d3d9, FALSE, TRUE))
  {

    //SK_TextOverlayManager::getInstance ()->destroyAllOverlays ();
    //SK_PopupManager::getInstance       ()->destroyAllPopups   ();

    init = false;
  }

  else if (std::exchange (init, true) == false)
  {
    D3DPRESENT_PARAMETERS              pparams = { };
    pSwapChain->GetPresentParameters (&pparams);

    SK_InstallWindowHook (pparams.hDeviceWindow);

    SK_ComQIPtr <IWrapDirect3DDevice9> pWrappedDevice (pDev);

    if (pWrappedDevice != nullptr)
    {
#ifdef _DEBUG
      dll_log.Log (L"Using wrapper for SK_ImGUI_ResetD3D9!");
#endif
      SK_ImGUI_ResetD3D9 (pDev/*pWrappedDevice*/);
    }

    else
      SK_ImGUI_ResetD3D9 (pDev);
  }

  else if (pDev != nullptr)
  {
    SK_ComPtr <IDirect3DStateBlock9>     pStateBlock = nullptr;
    pDev->CreateStateBlock (D3DSBT_ALL, &pStateBlock.p);

    if (! pStateBlock)
      return;

    pStateBlock->Capture ();

    pDev->SetRenderState (D3DRS_CULLMODE,                 D3DCULL_NONE);

    pDev->SetRenderState (D3DRS_LIGHTING,                 FALSE);
    pDev->SetRenderState (D3DRS_SPECULARENABLE,           FALSE);
    pDev->SetRenderState (D3DRS_FOGENABLE,                FALSE);
    pDev->SetRenderState (D3DRS_AMBIENT,                  0);

    pDev->SetRenderState (D3DRS_ALPHABLENDENABLE,         TRUE);
    pDev->SetRenderState (D3DRS_ALPHATESTENABLE,          FALSE);

    pDev->SetRenderState (D3DRS_SRCBLEND,                 D3DBLEND_INVSRCALPHA);
    pDev->SetRenderState (D3DRS_DESTBLEND,                D3DBLEND_SRCALPHA);
    pDev->SetRenderState (D3DRS_BLENDOP,                  D3DBLENDOP_ADD);
    pDev->SetRenderState (D3DRS_BLENDOPALPHA,             D3DBLENDOP_ADD);

    pDev->SetRenderState (D3DRS_STENCILENABLE,            FALSE);
    pDev->SetRenderState (D3DRS_SCISSORTESTENABLE,        FALSE);

    pDev->SetRenderState (D3DRS_ZENABLE,                  FALSE);
    pDev->SetRenderState (D3DRS_ZWRITEENABLE,             FALSE);
    pDev->SetRenderState (D3DRS_SRGBWRITEENABLE,          FALSE);
    pDev->SetRenderState (D3DRS_COLORWRITEENABLE,         D3DCOLORWRITEENABLE_RED   |
                                                          D3DCOLORWRITEENABLE_GREEN |
                                                          D3DCOLORWRITEENABLE_BLUE  |
                                                          D3DCOLORWRITEENABLE_ALPHA );

    pDev->SetSamplerState        (0, D3DSAMP_SRGBTEXTURE, FALSE);

    pDev->SetTextureStageState   (0, D3DTSS_COLOROP,      D3DTOP_MODULATE);
    pDev->SetTextureStageState   (0, D3DTSS_COLORARG0,    D3DTA_CURRENT);
    pDev->SetTextureStageState   (0, D3DTSS_COLORARG1,    D3DTA_TEXTURE);
    pDev->SetTextureStageState   (0, D3DTSS_COLORARG2,    D3DTA_DIFFUSE);

    pDev->SetTextureStageState   (0, D3DTSS_ALPHAOP,      D3DTOP_MODULATE);
    pDev->SetTextureStageState   (0, D3DTSS_ALPHAARG0,    D3DTA_CURRENT);
    pDev->SetTextureStageState   (0, D3DTSS_ALPHAARG1,    D3DTA_TEXTURE);
    pDev->SetTextureStageState   (0, D3DTSS_ALPHAARG2,    D3DTA_DIFFUSE);

    pDev->SetTextureStageState   (0, D3DTSS_RESULTARG,    D3DTA_CURRENT);

    pDev->SetSamplerState        (0, D3DSAMP_MINFILTER,   D3DTEXF_POINT);
    pDev->SetSamplerState        (0, D3DSAMP_MAGFILTER,   D3DTEXF_POINT);

    D3DVIEWPORT9        vp_orig = { };
    pDev->GetViewport (&vp_orig);

    D3DCAPS9              caps  = { };
    pDev->GetDeviceCaps (&caps);

    SK_ComPtr <IDirect3DSurface9> pBackBuffer = nullptr;
    SK_ComPtr <IDirect3DSurface9> rts [8]     = { };
    SK_ComPtr <IDirect3DSurface9> ds          = nullptr;

    for (UINT target = 0; target < std::min (caps.NumSimultaneousRTs, 8UL); target++) {
      pDev->GetRenderTarget (target, &rts [target].p);
    }

    pDev->GetDepthStencilSurface (&ds);

    D3DSURFACE_DESC surf_desc = { };

    if (SUCCEEDED (pDev->GetBackBuffer (0, 0, D3DBACKBUFFER_TYPE_MONO, &pBackBuffer.p)))
    {
      pBackBuffer->GetDesc (&surf_desc);

      pDev->SetRenderTarget (0, pBackBuffer);

      for (UINT target = 1; target < std::min (caps.NumSimultaneousRTs, 8UL); target++)
        pDev->SetRenderTarget (target, nullptr);
    }

    D3DPRESENT_PARAMETERS pp;

    if (SUCCEEDED (pSwapChain->GetPresentParameters (&pp)) && pp.hDeviceWindow != nullptr)
    {
      if (IsWindow (SK_GetCurrentRenderBackend ().windows.device) == FALSE)//pp.hDeviceWindow != hWndRender)
        SK_GetCurrentRenderBackend ().windows.setDevice (pp.hDeviceWindow);
    }


#if 0
    surf_desc.Width  = ImGui::GetIO ().DisplaySize.x;
    surf_desc.Height = ImGui::GetIO ().DisplaySize.y;

    D3DVIEWPORT9 vp_new;

    vp_new.X      = 0;
    vp_new.Y      = 0;
    vp_new.Width  = surf_desc.Width;
    vp_new.Height = surf_desc.Height;

    vp_new.MinZ =  0.0f;
    vp_new.MaxZ =  1.0f;

    pDev->SetViewport (&vp_new);
#else
    // Keep within the confines of the game's viewport, in case it is managing
    //   aspect ratio itself
    surf_desc.Width  = vp_orig.Width;
    surf_desc.Height = vp_orig.Height;
#endif

    pDev->SetRenderState (D3DRS_SRGBWRITEENABLE,          FALSE);
    pDev->SetRenderState (D3DRS_ALPHABLENDENABLE,         TRUE);
    pDev->SetRenderState (D3DRS_SEPARATEALPHABLENDENABLE, FALSE);
    pDev->SetRenderState (D3DRS_SRCBLEND,                 D3DBLEND_INVSRCALPHA);
    pDev->SetRenderState (D3DRS_DESTBLEND,                D3DBLEND_SRCALPHA);

    pDev->SetRenderState (D3DRS_ALPHATESTENABLE,          FALSE);

    const auto api = SK_GetCurrentRenderBackend ().api;

    if ( api == SK_RenderAPI::D3D9   ||
         api == SK_RenderAPI::D3D9Ex ||
         api == SK_RenderAPI::Reserved )
    {
      extern DWORD SK_ImGui_DrawFrame (DWORD dwFlags, void* user);
                   SK_ImGui_DrawFrame (       0x00,     nullptr );
    }


    //if ((uintptr_t)cegD3D9 > 1 && SK_Steam_DrawOSD () != 0)
    //{
    //  pDev->SetRenderState (D3DRS_SRGBWRITEENABLE,          FALSE);
    //  pDev->SetRenderState (D3DRS_ALPHABLENDENABLE,         TRUE);
    //  pDev->SetRenderState (D3DRS_SEPARATEALPHABLENDENABLE, FALSE);
    //  pDev->SetRenderState (D3DRS_SRCBLEND,                 D3DBLEND_INVSRCALPHA);
    //  pDev->SetRenderState (D3DRS_DESTBLEND,                D3DBLEND_SRCALPHA);
    //
    //  pDev->SetRenderState (D3DRS_ALPHATESTENABLE,          FALSE);
    //}

    pDev->SetViewport (&vp_orig);

    for (UINT target = 0; target < std::min (8UL, caps.NumSimultaneousRTs); target++)
      pDev->SetRenderTarget (target, rts [target]);

    pDev->SetDepthStencilSurface (ds);
    pStateBlock->Apply ();
  }
}

void
SK_ImGui_QueueResetD3D9 (void)
{
  InterlockedExchangeAcquire (&__gui_reset_d3d9, TRUE);
}

void
SK_ImGUI_ResetD3D9 (IDirect3DDevice9* pDev)
{
  SK_Window_RepositionIfNeeded ();

  if (pDev != nullptr)
  {
    SK_Steam_ClearPopups ();

    //
    // Initialize ImGui for D3D9 games
    //
    D3DDEVICE_CREATION_PARAMETERS params;
    pDev->GetCreationParameters (&params);

    if ( ImGui_ImplDX9_Init ( (void *)params.hFocusWindow,
                                    pDev,
                                     nullptr )
       )
    {
      InterlockedExchange ( &ImGui_Init, TRUE );
    }
  }
}



using finish_pfn =
  void (WINAPI *)(void);

void
WINAPI
SK_HookD3D9 (void)
{
  if (config.apis.translated == SK_RenderAPI::D3D9)
    return;

  static volatile LONG hooked = FALSE;

  if (! InterlockedCompareExchangeAcquire (&hooked, TRUE, FALSE))
  {
    // XXX: Kind of a hack, we may need to implicitly load-up D3D9.DLL so
    //        we can wait for VBlank in OpenGL..
    if (! SK_GetModuleHandle (L"d3d9.dll"))
      SK_Modules->LoadLibraryLL (L"d3d9.dll");


    HMODULE hBackend =
      ((SK_GetDLLRole () & DLL_ROLE::D3D9)
                        == DLL_ROLE::D3D9) ? backend_dll :
                                  SK_GetModuleHandle (L"d3d9.dll");

    SK_LOGi0 (L"Importing Direct3DCreate9{Ex}...");
    SK_LOGi0 (L"================================");

    if (! _wcsicmp (SK_GetModuleName (SK_GetDLL ()).c_str (), L"d3d9.dll"))
    {
      if (! LocalHook_Direct3DCreate9.active)
      {
        Direct3DCreate9_Import =  \
          (Direct3DCreate9_pfn)SK_GetProcAddress (hBackend, "Direct3DCreate9");

        LocalHook_Direct3DCreate9.target.addr = Direct3DCreate9_Import;
        LocalHook_Direct3DCreate9.active      = true;
      }

      SK_LOGi0 ( L"  Direct3DCreate9:   %s",
                  SK_MakePrettyAddress (Direct3DCreate9_Import).c_str () );
      SK_LogSymbolName                 (Direct3DCreate9_Import);

      if (config.apis.d3d9ex.hook)
      {
        if (!LocalHook_Direct3DCreate9Ex.active)
        {
          Direct3DCreate9Ex_Import =  \
            (Direct3DCreate9Ex_pfn)SK_GetProcAddress (hBackend, "Direct3DCreate9Ex");

          SK_LOGs0 ( L"  D3D9Ex  ",
                     L"  Direct3DCreate9Ex: %s",
                        SK_MakePrettyAddress (Direct3DCreate9Ex_Import).c_str () );
          SK_LogSymbolName                   (Direct3DCreate9Ex_Import);

          LocalHook_Direct3DCreate9Ex.target.addr = Direct3DCreate9Ex_Import;
          LocalHook_Direct3DCreate9Ex.active      = true;
        }
      }
    }

    else
    {
      LPVOID pfnDirect3DCreate9   = nullptr;
      LPVOID pfnDirect3DCreate9Ex = nullptr;

      if ( (! LocalHook_Direct3DCreate9.active) && SK_GetProcAddress (hBackend, "Direct3DCreate9") &&
             MH_OK ==
             SK_CreateDLLHook2 ( L"d3d9.dll",
                                  "Direct3DCreate9",
                                   Direct3DCreate9,
          static_cast_p2p <void> (&Direct3DCreate9_Import),
                               &pfnDirect3DCreate9 )
         )
      {
        LocalHook_Direct3DCreate9.target.addr = pfnDirect3DCreate9;
        LocalHook_Direct3DCreate9.active      = true;
      }
      else if (LocalHook_Direct3DCreate9.active) {
        pfnDirect3DCreate9 = LocalHook_Direct3DCreate9.target.addr;
      }

      if ( (! LocalHook_Direct3DCreate9Ex.active) && SK_GetProcAddress (hBackend, "Direct3DCreate9Ex") &&
             config.apis.d3d9ex.hook &&
             MH_OK ==
               SK_CreateDLLHook2 ( L"d3d9.dll",
                                    "Direct3DCreate9Ex",
                                     Direct3DCreate9Ex,
            static_cast_p2p <void> (&Direct3DCreate9Ex_Import),
                                 &pfnDirect3DCreate9Ex )
         )
      {
        LocalHook_Direct3DCreate9Ex.target.addr = pfnDirect3DCreate9Ex;
        LocalHook_Direct3DCreate9Ex.active      = true;
      }
      else if (LocalHook_Direct3DCreate9Ex.active) {
        pfnDirect3DCreate9Ex = LocalHook_Direct3DCreate9Ex.target.addr;
      }

      bool success = LocalHook_Direct3DCreate9.active ||
        ( MH_QueueEnableHook (pfnDirect3DCreate9) == MH_OK );

      if (config.apis.d3d9ex.hook)
        success &= ( LocalHook_Direct3DCreate9Ex.active ||
                    (MH_QueueEnableHook (pfnDirect3DCreate9Ex) == MH_OK) );

      SK_LOGi0 ( L"  Direct3DCreate9:   %s  { Hooked  }",
                    SK_MakePrettyAddress (pfnDirect3DCreate9).c_str () );
      SK_LogSymbolName                   (pfnDirect3DCreate9);

      SK_LOGs0 ( L"  D3D9Ex  ",
                 L"  Direct3DCreate9Ex: %s  { Hooked  }",
                    SK_MakePrettyAddress (pfnDirect3DCreate9Ex).c_str () );
      SK_LogSymbolName                   (pfnDirect3DCreate9Ex);
    }

    HookD3D9 (nullptr);

    // Load user-defined DLLs (Plug-In)
    SK_RunLHIfBitness ( 64, SK_LoadPlugIns64 (),
                            SK_LoadPlugIns32 () );

    InterlockedIncrementRelease (&hooked);
  }

  SK_Thread_SpinUntilAtomicMin (&hooked, 2);
  SK_Thread_SpinUntilFlagged   (&__d3d9_ready);
}

//
// D3DX9 Ghetto Delay Load :)
//
HMODULE SK_D3DX9_Unpack (void);

HMODULE
SK_D3DX9_GetModule (bool acquire = false)
{
  static auto     hModD3DX9_43 =
    SK_LoadLibraryA ("d3dx9_43.dll");

  if ( nullptr == hModD3DX9_43 && acquire )
  {  SK_RunOnce ( hModD3DX9_43 =
                   SK_D3DX9_Unpack () );  }

  if ( nullptr != hModD3DX9_43 )
  { 
    SK_RunOnce (  std::atexit ( []
    {FreeLibrary (hModD3DX9_43);}) );
  }

  return
    hModD3DX9_43;
}

LPCVOID
SK_D3DX9_GetProcAddress (const char* szProc)
{
  return
    SK_GetProcAddress (
      SK_D3DX9_GetModule (true),
        szProc );
}

using D3DXDisassembleShader_pfn =
  HRESULT (WINAPI *)( const DWORD*, BOOL,
                            LPCSTR, LPD3DXBUFFER* );

using D3DXGetShaderConstantTable_pfn =
  HRESULT (WINAPI *)( const DWORD*,
                        LPD3DXCONSTANTTABLE* );

using D3DXSaveTextureToFileW_pfn = 
  HRESULT (WINAPI *)( LPCWSTR, D3DXIMAGE_FILEFORMAT,
                      LPDIRECT3DBASETEXTURE9,
                const PALETTEENTRY* );

using D3DXGetImageInfoFromFileInMemory_pfn =
  HRESULT (WINAPI *)( LPCVOID, UINT, 
                      D3DXIMAGE_INFO* );

HRESULT
WINAPI
SK_D3DXDisassembleShader (
const DWORD         *pShader, 
      BOOL           EnableColorCode, 
      LPCSTR         pComments, 
      LPD3DXBUFFER *ppDisassembly )
{
  static auto
    _D3DXDisassembleShader =
    (D3DXDisassembleShader_pfn)SK_D3DX9_GetProcAddress
   ("D3DXDisassembleShader");

  return
    ( _D3DXDisassembleShader != nullptr )
    ? _D3DXDisassembleShader (
                     pShader, EnableColorCode,
                     pComments, ppDisassembly )
    : E_NOTIMPL;
}

HRESULT
WINAPI
SK_D3DXGetShaderConstantTable (
const DWORD               *pFunction,
      LPD3DXCONSTANTTABLE *ppConstantTable )
{
  static auto
    _D3DXGetShaderConstantTable =
    (D3DXGetShaderConstantTable_pfn)SK_D3DX9_GetProcAddress
   ("D3DXGetShaderConstantTable");

  return
    ( _D3DXGetShaderConstantTable != nullptr )
    ? _D3DXGetShaderConstantTable (pFunction, ppConstantTable)
    : E_NOTIMPL;
}

HRESULT
WINAPI
SK_D3DXGetImageInfoFromFileInMemory (
  LPCVOID         pSrcData,
  UINT            SrcDataSize,
  D3DXIMAGE_INFO* pSrcInfo)
{
  HRESULT hr = E_FAIL;

  SK::D3D9::TextureManager& tex_mgr =
    SK_D3D9_GetTextureManager ();

  bool bLoading =
    tex_mgr.injector.beginLoad ();

  static auto
    _D3DXGetImageInfoFromFileInMemory =
    (D3DXGetImageInfoFromFileInMemory_pfn)SK_D3DX9_GetProcAddress
   ("D3DXGetImageInfoFromFileInMemory");

  hr =
    ( _D3DXGetImageInfoFromFileInMemory != nullptr )
    ? _D3DXGetImageInfoFromFileInMemory (pSrcData, SrcDataSize, pSrcInfo)
    : E_NOTIMPL;

  tex_mgr.injector.endLoad (bLoading);

  return hr;
}

HRESULT
WINAPI
SK_D3DXSaveTextureToFileW (
      LPCWSTR                pDestFile,
      D3DXIMAGE_FILEFORMAT   DestFormat,
      LPDIRECT3DBASETEXTURE9 pSrcTexture,
const PALETTEENTRY           *pSrcPalette )
{
  HRESULT hr = E_FAIL;

  SK::D3D9::TextureManager& tex_mgr =
    SK_D3D9_GetTextureManager ();

  bool bLoading =
    tex_mgr.injector.beginLoad ();

  static auto
    _D3DXSaveTextureToFileW =
    (D3DXSaveTextureToFileW_pfn)SK_D3DX9_GetProcAddress
   ("D3DXSaveTextureToFileW");

  hr =
    ( _D3DXSaveTextureToFileW != nullptr )
    ? _D3DXSaveTextureToFileW ( pDestFile,   DestFormat,
                                pSrcTexture, pSrcPalette )
    : E_NOTIMPL;

  tex_mgr.injector.endLoad (bLoading);

  return hr;
}

D3DXCreateTextureFromFileInMemoryEx_pfn
D3DXCreateTextureFromFileInMemoryEx_Original = nullptr;

D3DXCreateVolumeTextureFromFileInMemoryEx_pfn
D3DXCreateVolumeTextureFromFileInMemoryEx_Original = nullptr;

D3DXCreateCubeTextureFromFileInMemoryEx_pfn
D3DXCreateCubeTextureFromFileInMemoryEx_Original = nullptr;

HRESULT
WINAPI
SK_D3DXCreateTextureFromFileInMemoryEx (
  _In_        LPDIRECT3DDEVICE9  pDevice,
  _In_        LPCVOID            pSrcData,
  _In_        UINT               SrcDataSize,
  _In_        UINT               Width,
  _In_        UINT               Height,
  _In_        UINT               MipLevels,
  _In_        DWORD              Usage,
  _In_        D3DFORMAT          Format,
  _In_        D3DPOOL            Pool,
  _In_        DWORD              Filter,
  _In_        DWORD              MipFilter,
  _In_        D3DCOLOR           ColorKey,
  _Inout_opt_ D3DXIMAGE_INFO     *pSrcInfo,
  _Out_opt_   PALETTEENTRY       *pPalette,
  _Out_       LPDIRECT3DTEXTURE9 *ppTexture )
{
  if (config.render.d3d9.force_d3d9ex)
  {
    if (Pool == D3DPOOL_MANAGED)
    {   Pool  = D3DPOOL_DEFAULT;
       Usage |= D3DUSAGE_DYNAMIC;
    }
  }

  HRESULT hr = E_FAIL;

  SK::D3D9::TextureManager& tex_mgr =
    SK_D3D9_GetTextureManager ();

  bool bLoading =
    tex_mgr.injector.beginLoad ();

  if (D3DXCreateTextureFromFileInMemoryEx_Original != nullptr) hr =
      D3DXCreateTextureFromFileInMemoryEx_Original (
        pDevice,
          pSrcData,         SrcDataSize,
            Width,          Height,    MipLevels,
              Usage,        Format,    Pool,
                Filter,     MipFilter, ColorKey,
                  pSrcInfo, pPalette,
                    ppTexture );

  else
  {
    // Game does not use D3DX, but we do... load an unhooked proc
    static auto
         D3DXCreateTextureFromFileInMemoryEx_NotHooked =
        (D3DXCreateTextureFromFileInMemoryEx_pfn)SK_D3DX9_GetProcAddress (
        "D3DXCreateTextureFromFileInMemoryEx" );

    if ( D3DXCreateTextureFromFileInMemoryEx_NotHooked != nullptr ) hr =
         D3DXCreateTextureFromFileInMemoryEx_NotHooked (
           pDevice,
             pSrcData,         SrcDataSize,
               Width,          Height,    MipLevels,
                 Usage,        Format,    Pool,
                   Filter,     MipFilter, ColorKey,
                     pSrcInfo, pPalette,
                       ppTexture );

    // Uh oh
    else
      hr = E_NOTIMPL;
  }

  tex_mgr.injector.endLoad (bLoading);

  return hr;
}

D3DXCreateTextureFromFileExW_pfn
D3DXCreateTextureFromFileExW_Original = nullptr;

D3DXCreateTextureFromFileExA_pfn
D3DXCreateTextureFromFileExA_Original = nullptr;

HMODULE
SK_D3DX9_Unpack (void)
{
  static
    SK_AutoHandle hTransferComplete (
      SK_CreateEvent (nullptr, TRUE, FALSE, nullptr)
    );

  wchar_t      wszArchive     [MAX_PATH + 2] = { };
  wchar_t      wszDestination [MAX_PATH + 2] = { };
  
  wcsncpy_s (  wszDestination, MAX_PATH,
           SK_GetHostPath (), _TRUNCATE  );
  
  if (GetFileAttributesW (  wszDestination  ) == INVALID_FILE_ATTRIBUTES)
    SK_CreateDirectoriesEx (wszDestination, false);

  wcsncpy_s (  wszArchive,      MAX_PATH,
               wszDestination, _TRUNCATE );
  PathAppendW (wszArchive, L"D3DX9_43.7z");

  SK_RunOnce (
  SK_Network_EnqueueDownload (
    sk_download_request_s (wszArchive,
      SK_RunLHIfBitness ( 64, R"(https://sk-data.special-k.info/redist/D3DX9_43_64.7z)",
                              R"(https://sk-data.special-k.info/redist/D3DX9_43_32.7z)" ),
      []( const std::vector <uint8_t>&& data,
          const std::wstring_view       file ) -> bool
      {
        SK_LOGs0 ( L"D3DCompile",
                   L"Unpacking D3DX9_43.dll because user does not have "
                   L"June 2010 DirectX Redistributables installed." );

        std::filesystem::path
                    full_path (file.data ());

        if ( FILE *fPackedD3DX9 = _wfopen (full_path.c_str (), L"wb") ;
                   fPackedD3DX9 != nullptr )
        {
          fwrite (data.data (), 1, data.size (), fPackedD3DX9);
          fclose (                               fPackedD3DX9);

          SK_Decompress7zEx ( full_path.c_str (),
                              full_path.parent_path (
                                      ).c_str (), nullptr );
          DeleteFileW       ( full_path.c_str ()          );

          SetEvent (hTransferComplete);
        }

        return true;
      }
    ), true // High Priority
  ));

  if ( WAIT_TIMEOUT ==
         SK_WaitForSingleObject (hTransferComplete, 4500UL) )
  {
    // Download thread timed-out
    return nullptr;
  }

  return
    SK_LoadLibraryW (
      (std::filesystem::path (wszDestination) / L"d3dx9_43.dll").c_str ()
    );

#if 0
  HMODULE hModSelf =
    SK_GetDLL ();

  HRSRC res =
    FindResource ( hModSelf, MAKEINTRESOURCE (IDR_D3DX9_PACKAGE), L"7ZIP" );

  if (res)
  {
    SK_LOG0 ( ( L"Unpacking D3DX9_43.dll because user does not have "
                L"June 2010 DirectX Redistributables installed." ),
                L"D3DCompile" );

    DWORD   res_size     =
      SizeofResource ( hModSelf, res );

    HGLOBAL packed_d3dx9 =
      LoadResource   ( hModSelf, res );

    if (! packed_d3dx9) return nullptr;

    const void* const locked =
      (void *)LockResource (packed_d3dx9);


    if (locked != nullptr)
    {
      wchar_t      wszArchive     [MAX_PATH + 2] = { };
      wchar_t      wszDestination [MAX_PATH + 2] = { };

      wcscpy (wszDestination, SK_GetHostPath ());

      if (GetFileAttributesW (wszDestination) == INVALID_FILE_ATTRIBUTES)
        SK_CreateDirectories (wszDestination);

      wcscpy      (wszArchive, wszDestination);
      PathAppendW (wszArchive, L"D3DX9_43.7z");

      FILE* fPackedD3DX9 =
        _wfopen   (wszArchive, L"wb");

      if (fPackedD3DX9 != nullptr)
      {
        fwrite    (locked, 1, res_size, fPackedD3DX9);
        fclose    (fPackedD3DX9);
      }

      SK_Decompress7zEx (wszArchive, wszDestination, nullptr);
      DeleteFileW       (wszArchive);
    }

    UnlockResource (packed_d3dx9);
  }

  return
    SK_LoadLibraryW (L"d3dx9_43.dll");
#endif
};

void
WINAPI
d3d9_init_callback (finish_pfn finish)
{
  if (! SK_IsHostAppSKIM ())
  {
    SK_BootD3D9 ();

    SK_Thread_SpinUntilFlagged (&__d3d9_ready);
  }

  // Another Render API was seen first, we should ignore this stuff
  if (SK_GetFramesDrawn () == 0)
  {
    bool local_d3d9 = false;
    if (! SK_COMPAT_IsSystemDllInstalled (L"d3dx9_43.dll", &local_d3d9))
    {
      if (! local_d3d9)
        SK_D3DX9_GetModule (true); // Fetch / Unpack if not installed
    }

    else
    {
      // Load the system-wide DLL
      SK_D3DX9_GetModule (false);
    }
  }

  else
  {
    SK_LOGi0 (
      L"Ignoring d3d9_init_callback (...) because frames have"
      L" been drawn using a different graphics API (%ws).",
                                  SK_Render_GetAPIName (
                 SK_GetCurrentRenderBackend ().api     )
             );
  }

  SK_ICommandProcessor *pCommandProc = nullptr;

  SK_RunOnce (
    pCommandProc = 
      SK_Render_InitializeSharedCVars ()
  );

  if (pCommandProc != nullptr)
  {
    // TODO: Any D3D9 Specific Console Variables..?
  }

  finish ();
}

bool
SK::D3D9::Startup (void)
{
  return SK_StartupCore (L"d3d9", d3d9_init_callback);
}

bool
SK::D3D9::Shutdown (void)
{
  SK::D3D9::TextureManager& tex_mgr =
    SK_D3D9_GetTextureManager ();

  // The texture manager built-in to SK is derived from these ...
  //   until those projects are modified to use THIS texture manager,
  //     they need special treatment.
  if ( SK_GetModuleHandle (L"tzfix.dll") == nullptr &&
       SK_GetModuleHandle (L"tsfix.dll") == nullptr )
  {
    if (tex_mgr.init)
      tex_mgr.Shutdown ();
  }


  if (SK_GetFramesDrawn () < 2)
  {
    SK_LOGs0 ( L"Hook Cache",
      L" !!! No frames drawn using D3D9 backend; "
             L"purging injection address cache..." );

    for ( auto it : local_d3d9_records )
    {
      SK_Hook_RemoveTarget (
        *it,
          L"D3D9.Hooks" );
    }

  /////SK_GetDLLConfig ()->write (SK_GetDLLConfig ()->get_filename ());
  }


  return SK_ShutdownCore (L"d3d9");
}



#define D3D9_INTERCEPT(_Base,_Index,_Name,_Override,_Original,_Type) { \
    D3D9_VIRTUAL_HOOK (   _Base,   _Index, _Name, _Override,           \
                        _Original, _Type );                            \
}

#define D3D9_CALL(_Ret, _Call) {                               \
  (_Ret) = (_Call);                                            \
  dll_log->Log ( L"[   D3D9   ] [@]  Return: %s  -  "          \
                                    L"< " __FUNCTIONW__ L" >", \
                   SK_DescribeHRESULT (_Ret) );                \
}

void
WINAPI
SK_D3D9_FixUpBehaviorFlags (DWORD& BehaviorFlags)
{
  ////BehaviorFlags &= ~D3DCREATE_NOWINDOWCHANGES;

  if (config.render.d3d9.force_impure)
    BehaviorFlags &= ~D3DCREATE_PUREDEVICE;

  if ( SK_GetModuleHandle (L"tbfix.dll") ||
       SK_GetModuleHandle (L"tzfix.dll") ||
       SK_GetModuleHandle (L"tsfix.dll") )
  {
    BehaviorFlags |= D3DCREATE_MULTITHREADED;
  }
}


void
WINAPI
SK_D3D9_SetFPSTarget ( D3DPRESENT_PARAMETERS* pPresentationParameters,
                       D3DDISPLAYMODEEX*      pFullscreenMode = nullptr )
{
  auto TargetFPS =
    static_cast <int> (config.render.framerate.target_fps);

  // First consider D3D9Ex FullScreen Mode
  int Refresh   = pFullscreenMode != nullptr ?
                    pFullscreenMode->RefreshRate :
                    0;

  // Then, use the presentation parameters
  if (Refresh == 0)
  {
    if ( pPresentationParameters           != nullptr &&
         pPresentationParameters->Windowed == FALSE )
      Refresh = pPresentationParameters->FullScreen_RefreshRateInHz;
    else
      Refresh = 0;
  }

  if (config.render.framerate.refresh_rate > 0.0f)
  {
    if ( pPresentationParameters           != nullptr &&
         pPresentationParameters->Windowed == FALSE)
    {
      SK_LOGi0 ( L" >> Refresh Rate Override: %f",
                       config.render.framerate.refresh_rate );

      Refresh = (int)config.render.framerate.refresh_rate;

      if ( pFullscreenMode != nullptr )
        pFullscreenMode->RefreshRate = Refresh;

      pPresentationParameters->FullScreen_RefreshRateInHz = Refresh;
    }
  }

  if ( TargetFPS != 0 && Refresh != 0       &&
       pPresentationParameters   != nullptr &&
       pPresentationParameters->Windowed == FALSE)
  {
    //if (Refresh >= TargetFPS) {
    //  if (! (Refresh % TargetFPS)) {
    //    dll_log.Log ( L"[   D3D9   ]  >> Targeting %li FPS - using 1:%li VSYNC;"
    //                  L" (refresh = %li Hz)",
    //                    TargetFPS,
    //                      Refresh / TargetFPS,
    //                        Refresh );
    //
    //    pPresentationParameters->SwapEffect           = D3DSWAPEFFECT_DISCARD;
    //    pPresentationParameters->PresentationInterval = Refresh / TargetFPS;
    //
    //  } else {
    //    dll_log.Log ( L"[   D3D9   ]  >> Cannot target %li FPS using VSYNC - no such factor exists;"
    //                  L" (refresh = %li Hz)",
    //                    TargetFPS,
    //                      Refresh );
    //  }
    //} else {
    //  dll_log.Log ( L"[   D3D9   ]  >> Cannot target %li FPS using VSYNC - higher than refresh rate;"
    //                L" (refresh = %li Hz)",
    //                  TargetFPS,
    //                    Refresh );
    //}
  }

  if (pPresentationParameters != nullptr)
  {
    if (       config.render.framerate.buffer_count != SK_NoPreference &&
         (UINT)config.render.framerate.buffer_count !=
           pPresentationParameters->BackBufferCount )
    {
      SK_LOGi0 ( L" >> Backbuffer Override: (Requested=%lu, Override=%li)",
                       pPresentationParameters->BackBufferCount,
                           config.render.framerate.buffer_count );
      pPresentationParameters->BackBufferCount =
        config.render.framerate.buffer_count;
    }

    auto
    _SK_D3D9_IsPresentIntervalEquivalent =
      [](int sk_interval, int d3d9_interval)
   -> bool
      {
        switch (sk_interval)
        {
          case 0:
            return (d3d9_interval == D3DPRESENT_INTERVAL_IMMEDIATE ||
                    d3d9_interval == D3DPRESENT_FORCEIMMEDIATE);

          case 1:
            return (d3d9_interval == D3DPRESENT_INTERVAL_ONE ||
                    d3d9_interval == D3DPRESENT_INTERVAL_DEFAULT);
          
          case 2:
            return d3d9_interval == D3DPRESENT_INTERVAL_TWO;

          case 3:
            return d3d9_interval == D3DPRESENT_INTERVAL_THREE;
          
          case 4:
            return d3d9_interval == D3DPRESENT_INTERVAL_FOUR;

          default:
            SK_ReleaseAssert (! L"Invalid D3D9 Present Interval");
            return false;
        }
      };

    int presentationInterval =
      config.render.framerate.present_interval;

    // Latent VSync...
    if  ( ( config.render.framerate.present_interval == 0 &&
            config.render.framerate.target_fps > 0.0f     &&
            pPresentationParameters->Windowed == FALSE     ) &&
          ( config.render.framerate.tearing_mode ==
              SK_TearingMode::AlwaysOff                   ||
            config.render.framerate.tearing_mode ==
              SK_TearingMode::AlwaysOff_LowLatency         ) )
    {
      presentationInterval = 1;
    }

    if (                           presentationInterval != SK_NoPreference &&
         (! _SK_D3D9_IsPresentIntervalEquivalent (
                                   presentationInterval,
          pPresentationParameters->PresentationInterval) ) )
    {
      SK_LOGi0 (
        L" >> VSYNC Override: (Requested=%lu, Override=%li)",

        SK_D3D9_GetNominalPresentInterval (
          pPresentationParameters->PresentationInterval
        ),                         presentationInterval
      );

      if (                       presentationInterval == 0)
        pPresentationParameters->PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;
      else if (                  presentationInterval == 1)
        pPresentationParameters->PresentationInterval = D3DPRESENT_INTERVAL_ONE;
      else if (                  presentationInterval == 2)
        pPresentationParameters->PresentationInterval = D3DPRESENT_INTERVAL_TWO;
      else if (                  presentationInterval == 3)
        pPresentationParameters->PresentationInterval = D3DPRESENT_INTERVAL_THREE;
      else if (                  presentationInterval == 4)
        pPresentationParameters->PresentationInterval = D3DPRESENT_INTERVAL_FOUR;
      else
      {
        SK_LOGi0 (
          L"Invalid Present Interval: %d Requested; defaulting to 1:1 Refresh",
                                 presentationInterval );

        pPresentationParameters->PresentationInterval = D3DPRESENT_INTERVAL_ONE;
      }
    }
  }
}


__inline
HRESULT
_HandleSwapChainException (const wchar_t* wszOrigin)
{
  SK_LOGi0 (
    L"*** First-chance exception during SwapChain Presentation (%ws)",
                                          wszOrigin );

  SK_ImGui_QueueResetD3D9 ();

  return D3DERR_DEVICELOST;
};

__declspec (noinline)
HRESULT
WINAPI
SK_D3D9_PresentEx (IDirect3DDevice9Ex *This,
                   _In_ const RECT    *pSourceRect,
                   _In_ const RECT    *pDestRect,
                   _In_       HWND     hDestWindowOverride,
                   _In_ const RGNDATA *pDirtyRegion,
                              DWORD    dwFlags)
{
  HRESULT
    hr = S_OK;

  auto orig_se =
  SK_SEH_ApplyTranslator (SK_FilteringStructuredExceptionTranslator (EXCEPTION_ACCESS_VIOLATION));
  try           { hr = SK_D3D9_Trampoline (D3D9ExDevice, PresentEx)
                                            ( This,
                                                pSourceRect,
                                                  pDestRect,
                                                    hDestWindowOverride,
                                                      pDirtyRegion,
                                                        dwFlags );
  }
  catch (const SK_SEH_IgnoredException&)
  {
    hr =
      _HandleSwapChainException (L"IDirect3DDevice9Ex::PresentEx");
  }
  SK_SEH_RemoveTranslator (orig_se);

  return hr;
}

__declspec (noinline)
HRESULT
WINAPI
SK_D3D9_Present (IDirect3DDevice9 *This,
                 _In_ const RECT             *pSourceRect,
                 _In_ const RECT             *pDestRect,
                 _In_       HWND              hDestWindowOverride,
                 _In_ const RGNDATA          *pDirtyRegion)
{
  HRESULT
    hr = S_OK;

  auto orig_se =
  SK_SEH_ApplyTranslator (SK_FilteringStructuredExceptionTranslator (EXCEPTION_ACCESS_VIOLATION));
  try {
    hr = SK_D3D9_Trampoline (D3D9Device, Present)
                              ( This,
                                  pSourceRect,
                                    pDestRect,
                                      hDestWindowOverride,
                                        pDirtyRegion );
  }

  catch (const SK_SEH_IgnoredException&)
  {
    hr =
      _HandleSwapChainException (L"IDirect3DDevice9::Present");
  }
  SK_SEH_RemoveTranslator (orig_se);

  return hr;
}

__declspec (noinline)
HRESULT
WINAPI
SK_D3D9_PresentSwap (IDirect3DSwapChain9 *This,
                     _In_ const RECT     *pSourceRect,
                     _In_ const RECT     *pDestRect,
                     _In_       HWND      hDestWindowOverride,
                     _In_ const RGNDATA  *pDirtyRegion,
                                DWORD     dwFlags)
{
  HRESULT
    hr = S_OK;

  auto orig_se =
  SK_SEH_ApplyTranslator (SK_FilteringStructuredExceptionTranslator (EXCEPTION_ACCESS_VIOLATION));
  try           { hr = SK_D3D9_Trampoline (D3D9Swap, Present)
                                              ( This,
                                                  pSourceRect,
                                                    pDestRect,
                                                      hDestWindowOverride,
                                                        pDirtyRegion,
                                                          dwFlags );
  }
  catch (const SK_SEH_IgnoredException&)
  {
    hr =
      _HandleSwapChainException (L"IDirect3DSwapChain9::Present");
  }
  SK_SEH_RemoveTranslator (orig_se);

  return hr;
}



__inline
bool
SK_D3D9_ShouldProcessPresentCall (SK_D3D9_PresentSource Source)
{
  const SK_RenderBackend_V2& rb =
    SK_GetCurrentRenderBackend ();

  if (rb.api == SK_RenderAPI::Reserved)
    return true;

  // No transient graphics APIs please.
  if (! ((rb.api == SK_RenderAPI::D3D9) ||
         (rb.api == SK_RenderAPI::D3D9Ex)) )
    return false;


  bool has_wrapped_swapchains =
    ( ReadAcquire (&SK_D3D9_LiveWrappedSwapChains) +
      ReadAcquire (&SK_D3D9_LiveWrappedSwapChainsEx) ) > 0;

  // We need at least one wrapped SwapChain for this to work, otherwise
  //   fallback to the regular hook technique.
  if (config.render.osd.draw_in_vidcap && has_wrapped_swapchains)
  {
    // Not sure how we managed to dispatch something from a wrapper
    //   without a wrapper; sanity's overrated.
    return (Source == SK_D3D9_PresentSource::Wrapper);
  }

  else
  {
    return (Source == SK_D3D9_PresentSource::Hook) ||
                  (! has_wrapped_swapchains);
  }
}


__inline
HRESULT
STDMETHODCALLTYPE
SK_D3D9_Present_GrandCentral ( sk_d3d9_swap_dispatch_s* dispatch )
{
#define PACKAGED_ARGS dispatch->pSourceRect,         \
                      dispatch->pDestRect,           \
                      dispatch->hDestWindowOverride, \
                      dispatch->pDirtyRegion
#define PACKAGED_ARGS_EX PACKAGED_ARGS,              \
                      dispatch->dwFlags

  IDirect3DDevice9*    pDev       =
    static_cast <IDirect3DDevice9    *> (dispatch->pDevice);
  IDirect3DDevice9Ex*  pDevEx     =
    static_cast <IDirect3DDevice9Ex  *> (dispatch->pDevice);
  IDirect3DSwapChain9* pSwapChain =
    static_cast <IDirect3DSwapChain9 *> (dispatch->pSwapChain);

  if (dispatch->Type != SK_D3D9_PresentType::Device9Ex_PresentEx)
    pDevEx = nullptr;


  auto CallFunc = [&](void) ->
  HRESULT
  {
    if (config.render.framerate.enforcement_policy == 2) // Low Latency
    {
      if (config.render.framerate.present_interval == 0) // VSYNC Off
      {
        if (config.render.framerate.target_fps > 0.0f)   // Limit Configured
        {
          // Now we WaitForVBLANK
          extern void SK_D3DKMT_WaitForVBlank (void);
                      SK_D3DKMT_WaitForVBlank ();
        }
      }
    }

    // Route Present through PresentEx to gain access to Force Immediate
    //
    //   ( This allows turning VSYNC on/off without resetting the device )
    //
    if ( config.render.d3d9.force_d3d9ex )
    {
      pDevEx             = static_cast <IDirect3DDevice9Ex *> (pDev);
      dispatch->Type     = SK_D3D9_PresentType::Device9Ex_PresentEx;

      D3DPRESENT_PARAMETERS              pparams = { };
      pSwapChain->GetPresentParameters (&pparams);

      // This silently fails in fullscreen
      if (! pparams.Windowed)
        dispatch->dwFlags |= D3DPRESENT_FORCEIMMEDIATE;
    }


    switch (dispatch->Type)
    {
      // IDirect3DDevice9::Present (...)
      //
      case SK_D3D9_PresentType::Device9_Present:
      {
        switch (dispatch->Source)
        {
          case SK_D3D9_PresentSource::Hook:    // VFTable Hook
          {
            return SK_D3D9_Present
                   ( pDev, PACKAGED_ARGS );
          }

          case SK_D3D9_PresentSource::Wrapper: // COM Wrapper
          {
            return pDev->Present
                   ( PACKAGED_ARGS );
          }
        }
      } break;


      // IDirect3DDevice9Ex::PresentEx (...)
      //
      case SK_D3D9_PresentType::Device9Ex_PresentEx:
      {
        switch (dispatch->Source)
        {
          case SK_D3D9_PresentSource::Hook:    // VFTable Hook
          {
            return SK_D3D9_PresentEx
                   ( pDevEx,
                     PACKAGED_ARGS_EX );
          }

          case SK_D3D9_PresentSource::Wrapper: // COM Wrapper
          {
            return pDevEx->PresentEx
                   ( PACKAGED_ARGS_EX );
          }
        }
      } break;


      // IDirect3DSwapChain9::Present (...)
      //
      case SK_D3D9_PresentType::SwapChain9_Present:
      {
        switch (dispatch->Source)
        {
          case SK_D3D9_PresentSource::Hook:    // VFTable Hook
          {
            return SK_D3D9_PresentSwap
                   ( pSwapChain,
                     PACKAGED_ARGS_EX );
          }

          case SK_D3D9_PresentSource::Wrapper: // COM Wrapper
          {
            return pSwapChain->Present
                   ( PACKAGED_ARGS_EX );
          }
        }
      } break;
    };

    // This ain't good!   (Log it REPEATEDLY)
    //
    SK_LOGi0 ( L" Dispatch failure for D3D9 SwapChain Presentation; %s",
                  L"Unknown Combination of Parameters" );

    return E_NOTIMPL;
  };

  bool process =
    SK_D3D9_ShouldProcessPresentCall (dispatch->Source);



  SK_RenderBackend& rb =
    SK_GetCurrentRenderBackend ();


  if (process || trigger_reset != reset_stage_e::Clear)
  {
    if ( rb.api == SK_RenderAPI::D3D9   ||
         rb.api == SK_RenderAPI::D3D9Ex ||
         rb.api == SK_RenderAPI::Reserved )
    {
      if (dispatch->pDevice != rb.device || rb.swapchain != dispatch->pSwapChain)
      {
        ImGui_ImplDX9_InvalidateDeviceObjects (nullptr);
        rb.releaseOwnedResources              (       );
      }

      rb.setDevice  (dispatch->pDevice);
      rb.swapchain = dispatch->pSwapChain;
    }

#if 0
    SK_SetThreadIdealProcessor (GetCurrentThread (),       6);
    SetThreadAffinityMask      (GetCurrentThread (), (1 << 7) | (1 << 6));//config.render.framerate.pin_render_thread);
#endif


#if 0
    if (g_D3D9PresentParams.SwapEffect == D3DSWAPEFFECT_FLIPEX)
    {
      HRESULT hr =
        D3D9ExDevice_PresentEx ( static_cast <IDirect3DDevice9Ex *> (pDeviceEx),
                                  nullptr,
                                    nullptr,
                                      nullptr,
                                        nullptr,
                                            D3DPRESENT_FORCEIMMEDIATE |
                                            D3DPRESENT_DONOTWAIT );

      SK_D3D9_EndFrame ();

      return hr;
    }
#endif


    if (rb.surface.d3d9 != nullptr)
    {
        rb.surface.d3d9  = nullptr;
        rb.surface.nvapi = nullptr;
    }

    if (SUCCEEDED (pSwapChain->GetBackBuffer (0, D3DBACKBUFFER_TYPE_MONO, &rb.surface.d3d9)))
    {
      D3DPRESENT_PARAMETERS              pparams = { };
      pSwapChain->GetPresentParameters (&pparams);

      rb.setDevice  (pDev);
      rb.swapchain = pSwapChain;

      // Queue-up Pre-SK OSD Screenshots
      SK_D3D9_ProcessScreenshotQueue (SK_ScreenshotStage::BeforeOSD);

      SK_ImGui_DrawD3D9 ( pDev,
                            pSwapChain );

      // Queue-up Post-SK OSD Screenshots (Does not include ReShade)
      SK_D3D9_ProcessScreenshotQueue (SK_ScreenshotStage::PrePresent);

      //
      // Update G-Sync; doing this here prevents trying to do this on frames where
      //   the swapchain was resized, which would deadlock the software.
      //
      if ( sk::NVAPI::nv_hardware && config.apis.NvAPI.gsync_status &&
           ( (config.fps.show && config.osd.show) || SK_ImGui_Visible ) )
      {
        if ( NVAPI_OK ==
               NvAPI_D3D9_GetSurfaceHandle ( rb.surface.d3d9,
                                            &rb.surface.nvapi )
           )
        {
          rb.gsync_state.update ();
        }

        // Don't leave dangling references to the surface around...
        else
        {
          rb.surface.d3d9  = nullptr;
          rb.surface.nvapi =       0;
        }
      }
    }


    SK_ComQIPtr <IDirect3DDevice9Ex> pUpgradedDev (pDev);
  //pDev->QueryInterface (IID_IDirect3DDevice9Ex, &pUpgradedDev);

    if (pDevEx != nullptr || pUpgradedDev != nullptr)
    {
      reinterpret_cast <int &> (rb.api)  = ( static_cast <int> (SK_RenderAPI::D3D9  ) |
                                             static_cast <int> (SK_RenderAPI::D3D9Ex)   );
    }

    else
    {
      rb.api = SK_RenderAPI::D3D9;
    }


    if (config.fps.timing_method == SK_FrametimeMeasures_PresentSubmit)
    {
      SK::Framerate::TickEx (false, 0.0, { 0,0 }, rb.swapchain.p);
    }

    SK_BeginBufferSwap ();


    HRESULT hr =
      D3DERR_DEVICELOST;

    if (trigger_reset == reset_stage_e::Clear)
    {
      hr = CallFunc ();

      if (hr == D3DERR_WASSTILLDRAWING)
          hr = D3D_OK;

      hr =
        SK_EndBufferSwap ( hr, pDev );
    }

    else
    {
      hr = CallFunc ();

      if (hr == D3DERR_WASSTILLDRAWING)
          hr = D3D_OK;

      if (hr == D3D_OK)
        hr = D3DERR_DEVICELOST;
    }

    // Queue-up End of Frame Screenshots
    SK_D3D9_ProcessScreenshotQueue (SK_ScreenshotStage::EndOfFrame);

    SK_D3D9_EndFrame ();

    extern float                                                          __target_fps;
    if (config.fps.timing_method == SK_FrametimeMeasures_NewFrameBegin ||
       (config.fps.timing_method == SK_FrametimeMeasures_LimiterPacing && __target_fps <= 0.0f))
    {
      SK::Framerate::TickEx (false, 0.0, { 0,0 }, rb.swapchain.p);
    }


    if (hr != D3D_OK && trigger_reset == reset_stage_e::Clear)
      trigger_reset = reset_stage_e::Initiate;


    return hr;
  }


  else
  {
    HRESULT hr =
      CallFunc ();

    // While we're triggering a reset, D3D_OK --> D3DERR_DEVICELOST,
    //
    //   * Any other errors or status must pass through unaltered.
    //
    if (trigger_reset != reset_stage_e::Clear)
    {
      if (hr == D3D_OK)
        return D3DERR_DEVICELOST;
    }

    return hr;
  }
}





__declspec (noinline)
HRESULT
WINAPI
D3D9Device_Present ( IDirect3DDevice9 *This,
          _In_ const RECT             *pSourceRect,
          _In_ const RECT             *pDestRect,
          _In_       HWND              hDestWindowOverride,
          _In_ const RGNDATA          *pDirtyRegion )
{
  // Uh, what?
  if (This == nullptr)
    return E_NOINTERFACE;

  SK_ComPtr <IDirect3DSwapChain9> pSwapChain = nullptr;


  if ( SUCCEEDED (This->GetSwapChain (0, &pSwapChain.p)) &&
                                          pSwapChain != nullptr )
  {
    sk_d3d9_swap_dispatch_s dispatch =
    {
      This,                pSwapChain,
      pSourceRect,         pDestRect,
      hDestWindowOverride, pDirtyRegion,
      0x00,
      SK_D3D9_Trampoline (D3D9Device, Present),
      SK_D3D9_PresentSource::Hook,
      SK_D3D9_PresentType::Device9_Present
    };

    return
      SK_D3D9_Present_GrandCentral (&dispatch);
  }

  return D3DERR_DEVICELOST;
}

__declspec (noinline)
HRESULT
WINAPI
D3D9ExDevice_PresentEx ( IDirect3DDevice9Ex *This,
              _In_ const RECT               *pSourceRect,
              _In_ const RECT               *pDestRect,
              _In_       HWND                hDestWindowOverride,
              _In_ const RGNDATA            *pDirtyRegion,
              _In_       DWORD               dwFlags )
{
  if (This == nullptr)
    return E_NOINTERFACE;

  SK_ComPtr <IDirect3DSwapChain9> pSwapChain = nullptr;

  if ( SUCCEEDED (This->GetSwapChain (0, &pSwapChain.p)) &&
                                          pSwapChain != nullptr )
  {
    sk_d3d9_swap_dispatch_s dispatch =
    {
      This,                pSwapChain,
      pSourceRect,         pDestRect,
      hDestWindowOverride, pDirtyRegion,
      dwFlags,
      SK_D3D9_Trampoline (D3D9ExDevice, PresentEx),
      SK_D3D9_PresentSource::Hook,
      SK_D3D9_PresentType::Device9Ex_PresentEx
    };

    return
      SK_D3D9_Present_GrandCentral (&dispatch);
  }

  return D3DERR_DEVICELOST;
}


#define D3D9_STUB_HRESULT(_Return, _Name, _Proto, _Args)                  \
  __declspec (noinline) _Return STDMETHODCALLTYPE                         \
  _Name _Proto {                                                          \
    WaitForInit ();                                                       \
                                                                          \
    typedef _Return (STDMETHODCALLTYPE *passthrough_pfn) _Proto;          \
    static passthrough_pfn _default_impl = nullptr;                       \
                                                                          \
    if (_default_impl == nullptr) {                                       \
      static const char* szName = #_Name;                                 \
      _default_impl = (passthrough_pfn)                                   \
        SK_GetProcAddress (backend_dll, szName);                          \
                                                                          \
      if (_default_impl == nullptr) {                                     \
        dll_log->Log (                                                    \
          L"[   D3D9   ] Unable to locate symbol '%s' in d3d9.dll",       \
          L#_Name);                                                       \
        return E_NOTIMPL;                                                 \
      }                                                                   \
    }                                                                     \
                                                                          \
    /*dll_log.Log (L"[!] %s %s - "                                      */\
             /*L"[Calling Thread: 0x%04x]",                             */\
      /*L#_Name, L#_Proto, SK_Thread_GetCurrentId ());                  */\
                                                                          \
    return _default_impl _Args;                                           \
}

#define D3D9_STUB_VOIDP(_Return, _Name, _Proto, _Args)                    \
  __declspec (noinline) _Return STDMETHODCALLTYPE                         \
  _Name _Proto {                                                          \
    WaitForInit ();                                                       \
                                                                          \
    typedef _Return (STDMETHODCALLTYPE *passthrough_pfn) _Proto;          \
    static passthrough_pfn _default_impl = nullptr;                       \
                                                                          \
    if (_default_impl == nullptr) {                                       \
      static const char* szName = #_Name;                                 \
      _default_impl = (passthrough_pfn)                                   \
        SK_GetProcAddress (backend_dll, szName);                          \
                                                                          \
      if (_default_impl == nullptr) {                                     \
        dll_log->Log (                                                    \
          L"[   D3D9   ] Unable to locate symbol '%s' in d3d9.dll",       \
          L#_Name);                                                       \
        return nullptr;                                                   \
      }                                                                   \
    }                                                                     \
                                                                          \
    /*dll_log.Log (L"[!] %s %s - "                                      */\
           /*L"[Calling Thread: 0x%04x]",                               */\
      /*L#_Name, L#_Proto, SK_Thread_GetCurrentId ());                  */\
                                                                          \
    return _default_impl _Args;                                           \
}

#define D3D9_STUB_VOID(_Return, _Name, _Proto, _Args)                     \
  __declspec (noinline) _Return STDMETHODCALLTYPE                         \
  _Name _Proto {                                                          \
    WaitForInit ();                                                       \
                                                                          \
    typedef _Return (STDMETHODCALLTYPE *passthrough_pfn) _Proto;          \
    static passthrough_pfn _default_impl = nullptr;                       \
                                                                          \
    if (_default_impl == nullptr) {                                       \
      static const char* szName = #_Name;                                 \
      _default_impl = (passthrough_pfn)                                   \
        SK_GetProcAddress (backend_dll, szName);                          \
                                                                          \
      if (_default_impl == nullptr) {                                     \
        dll_log->Log (                                                    \
          L"[   D3D9   ] Unable to locate symbol '%s' in d3d9.dll",       \
          L#_Name);                                                       \
        return;                                                           \
      }                                                                   \
    }                                                                     \
                                                                          \
    /*dll_log.Log (L"[!] %s %s - "                                      */\
             /*L"[Calling Thread: 0x%04x]",                             */\
      /*L#_Name, L#_Proto, SK_Thread_GetCurrentId ());                  */\
                                                                          \
    _default_impl _Args;                                                  \
}

#define D3D9_STUB_INT(_Return, _Name, _Proto, _Args)                      \
  __declspec (noinline) _Return STDMETHODCALLTYPE                         \
  _Name _Proto {                                                          \
    WaitForInit ();                                                       \
                                                                          \
    typedef _Return (STDMETHODCALLTYPE *passthrough_pfn) _Proto;          \
    static passthrough_pfn _default_impl = nullptr;                       \
                                                                          \
    if (_default_impl == nullptr) {                                       \
      static const char* szName = #_Name;                                 \
      _default_impl = (passthrough_pfn)                                   \
        SK_GetProcAddress (backend_dll, szName);                          \
                                                                          \
      if (_default_impl == nullptr) {                                     \
        dll_log->Log (                                                    \
          L"[   D3D9   ] Unable to locate symbol '%s' in d3d9.dll",       \
          L#_Name);                                                       \
        return 0;                                                         \
      }                                                                   \
    }                                                                     \
                                                                          \
    /*dll_log.Log (L"[!] %s %s - "                                      */\
             /*L"[Calling Thread: 0x%04x]",                             */\
      /*L#_Name, L#_Proto, SK_Thread_GetCurrentId ());                  */\
                                                                          \
    return _default_impl _Args;                                           \
}

D3D9_STUB_VOIDP   (void*, Direct3DShaderValidatorCreate, (void),
                                                         (    ))
D3D9_STUB_VOIDP   (void*, Direct3DShaderValidatorCreate9,(void),
                                                         (    ))
D3D9_STUB_VOIDP   (void*, DebugSetMute,                  (void),
                                                         (    ))

D3D9_STUB_INT     (int,   D3DPERF_BeginEvent, (D3DCOLOR color, LPCWSTR name),
                                                       (color,         name))
D3D9_STUB_INT     (int,   D3DPERF_EndEvent,   (void),          ( ))

D3D9_STUB_INT     (DWORD, D3DPERF_GetStatus,  (void),          ( ))
D3D9_STUB_VOID    (void,  D3DPERF_SetOptions, (DWORD options), (options))

D3D9_STUB_INT     (BOOL,  D3DPERF_QueryRepeatFrame, (void),    ( ))
D3D9_STUB_VOID    (void,  D3DPERF_SetMarker, (D3DCOLOR color, LPCWSTR name),
                                                      (color,         name))
D3D9_STUB_VOID    (void,  D3DPERF_SetRegion, (D3DCOLOR color, LPCWSTR name),
                                                      (color,         name))

// If this isn't exported, games will crash if 'Fullscreen Optimizations' are
//   forcefully overriden by the end-user.
D3D9_STUB_INT     (int,   Direct3D9EnableMaximizedWindowedModeShim, (BOOL mEnable),
                                                                         (mEnable))



int
SK_D3D9_HookDeviceAndSwapchain (
   IDirect3DDevice9    *pDevice,
   IDirect3DSwapChain9 *pSwapChain = nullptr );

__declspec (noinline)
HRESULT
WINAPI
D3D9Swap_Present ( IDirect3DSwapChain9 *This,
        _In_ const RECT                *pSourceRect,
        _In_ const RECT                *pDestRect,
        _In_       HWND                 hDestWindowOverride,
        _In_ const RGNDATA             *pDirtyRegion,
        _In_       DWORD                dwFlags )
{
 if (This == nullptr)
    return E_NOINTERFACE;

  SK_ComPtr <IDirect3DDevice9> pDevice = nullptr;

  if (SUCCEEDED (This->GetDevice (&pDevice.p)) && pDevice != nullptr)
  {
    sk_d3d9_swap_dispatch_s dispatch =
    {
      pDevice,             This,
      pSourceRect,         pDestRect,
      hDestWindowOverride, pDirtyRegion,
      dwFlags,
      SK_D3D9_Trampoline (D3D9Swap, Present),
      SK_D3D9_PresentSource::Hook,
      SK_D3D9_PresentType::SwapChain9_Present
    };

    HRESULT hr =
      SK_D3D9_Present_GrandCentral (&dispatch);

    return hr;
  }

  return D3DERR_DEVICELOST;
}



__declspec (noinline)
HRESULT
STDMETHODCALLTYPE
D3D9CreateAdditionalSwapChain_Override ( IDirect3DDevice9       *This,
                                         D3DPRESENT_PARAMETERS  *pPresentationParameters,
                                         IDirect3DSwapChain9   **ppSwapChain )
{
  SK_LOGi0 ( L"[!] %s (%08" _L(PRIxPTR) L"h, %08" _L(PRIxPTR) L"h,"
                    L" %08" _L(PRIxPTR) L"h) - "
    L"%s",
    L"IDirect3DDevice9::CreateAdditionalSwapChain", This,
      (uintptr_t)pPresentationParameters, (uintptr_t)ppSwapChain,
        SK_SummarizeCaller ().c_str ()
  );

  HRESULT    hr = E_FAIL;
  D3D9_CALL (hr,D3D9Device_CreateAdditionalSwapChain_Original ( This,
                                                                  pPresentationParameters,
                                                                    ppSwapChain ) );


  SK_SetPresentParamsD3D9 (This, pPresentationParameters);


  if (! LocalHook_D3D9PresentSwap.active)
  {
    SK_D3D9_HookDeviceAndSwapchain (This, *ppSwapChain);
  }

  return hr;
}

__declspec (noinline)
HRESULT
STDMETHODCALLTYPE
D3D9TestCooperativeLevel_Override (IDirect3DDevice9 *This)
{
  if (trigger_reset == reset_stage_e::Initiate)
  {
    trigger_reset = reset_stage_e::Respond;

    return D3DERR_DEVICELOST;
  }


  else if (trigger_reset == reset_stage_e::Respond)
  {
    return D3DERR_DEVICENOTRESET;
  }

  return D3D9Device_TestCooperativeLevel_Original (This);
}

__declspec (noinline)
HRESULT
STDMETHODCALLTYPE
D3D9BeginScene_Override (IDirect3DDevice9 *This)
{
  SK::D3D9::TextureManager& tex_mgr =
    SK_D3D9_GetTextureManager ();

  if ( tex_mgr.init &&
       tex_mgr.thread_id == SK_Thread_GetCurrentId () &&
       tex_mgr.last_frame < SK_GetFramesDrawn      () )
  {
    tex_mgr.last_frame = SK_GetFramesDrawn ();

    if (tex_mgr.injector.hasPendingLoads ())
        tex_mgr.loadQueuedTextures ();
  }

  HRESULT hr =
    D3D9Device_BeginScene_Original (This);

  //D3D9_CALL (hr, D3D9Begincene_Original (This));

  return hr;
}

__declspec (noinline)
HRESULT
STDMETHODCALLTYPE
D3D9EndScene_Override (IDirect3DDevice9 *This)
{
  SK::D3D9::TextureManager& tex_mgr =
    SK_D3D9_GetTextureManager ();

#if 0
  dll_log->Log (L"[   D3D9   ] [!] %s (%ph) - "
    L"[Calling Thread: 0x%04x]",
    L"IDirect3DDevice9::EndScene", This,
    SK_Thread_GetCurrentId ()
  );
#endif

  SK_D3D9_EndScene ();

  HRESULT hr = D3D9Device_EndScene_Original (This);

  if ( tex_mgr.init && // Ys 7 is doing stuff on multiple threads...
       tex_mgr.thread_id == SK_Thread_GetCurrentId () &&
       tex_mgr.last_frame < SK_GetFramesDrawn      () )
  {
    tex_mgr.last_frame = SK_GetFramesDrawn ();

    if (tex_mgr.injector.hasPendingLoads ())
        tex_mgr.loadQueuedTextures ();
  }

  return hr;
}

int
SK_D3D9_HookReset (IDirect3DDevice9 *pDev)
{
  int hooked = 0;

  if (! LocalHook_D3D9Reset.active)
  {
    D3D9_INTERCEPT ( &pDev, 16,
                     "IDirect3DDevice9::Reset",
                      D3D9Reset_Override,
                      D3D9Device_Reset_Original,
                      D3D9Device_Reset_pfn );

    SK_Hook_TargetFromVFTable (
      LocalHook_D3D9Reset,
        (void **)&pDev, 16 );

    if (LocalHook_D3D9Reset.active) ++hooked;
  }


  if (! config.apis.d3d9ex.hook)
    return hooked;


  //
  // D3D9Ex Specific Stuff
  //

  SK_ComQIPtr <IDirect3DDevice9Ex> pDevEx (pDev);

  if (pDevEx != nullptr)
  {
    if (! LocalHook_D3D9ResetEx.active)
    {
      D3D9_INTERCEPT ( &pDevEx.p, 132,
                       "IDirect3DDevice9Ex::ResetEx",
                        D3D9ResetEx,
                        D3D9ExDevice_ResetEx_Original,
                        D3D9ExDevice_ResetEx_pfn );

      SK_Hook_TargetFromVFTable (
        LocalHook_D3D9ResetEx,
          (void **)&pDevEx.p, 132 );

      if (LocalHook_D3D9ResetEx.active) ++hooked;
    }
  }

  return hooked;
}

int
WINAPI
SK_D3D9_HookPresent (IDirect3DDevice9 *pDev)
{
  int hooked = 0;

  if (! LocalHook_D3D9Present.active)
  {
    D3D9_INTERCEPT ( &pDev, 17,
                     "IDirect3DDevice9::Present",
                      D3D9Device_Present,
                      D3D9Device_Present_Original,
                      D3D9Device_Present_pfn );

    SK_Hook_TargetFromVFTable (
      LocalHook_D3D9Present,
        (void **)&pDev, 17 );

    if (LocalHook_D3D9Present.active) ++hooked;
  }


  if (! LocalHook_D3D9PresentSwap.active)
  {
    SK_ComPtr        <IDirect3DSwapChain9> pSwapChain = nullptr;
    if (SUCCEEDED (pDev->GetSwapChain (0, &pSwapChain.p)))
    {
      void** vftable_ = nullptr;

      SK_ComQIPtr <IWrapDirect3DSwapChain9>
          pWrappedSwapChain (pSwapChain);
      if (pWrappedSwapChain != nullptr)
        vftable_ = (void **)&(pWrappedSwapChain->pReal);
      else
        vftable_ = (void **)&pSwapChain.p;

      if (*vftable_ != nullptr)
      {
        D3D9_INTERCEPT ( vftable_, 3,
                         "IDirect3DSwapChain9::Present",
                          D3D9Swap_Present,
                          D3D9Swap_Present_Original,
                          D3D9Swap_Present_pfn );

        SK_Hook_TargetFromVFTable (
          LocalHook_D3D9PresentSwap,
            vftable_, 3 );

        if (LocalHook_D3D9PresentSwap.active) ++hooked;
      }
    }
  }


  if (config.apis.d3d9ex.hook)
  {
    SK_ComQIPtr <IDirect3DDevice9Ex> pDevEx (pDev);

    if (pDevEx != nullptr)
    {
      if (! LocalHook_D3D9PresentEx.active)
      {
        //
        // D3D9Ex Specific Stuff
        //
        D3D9_INTERCEPT ( &pDevEx.p, 121,
                           "IDirect3DDevice9Ex::PresentEx",
                            D3D9ExDevice_PresentEx,
                            D3D9ExDevice_PresentEx_Original,
                            D3D9ExDevice_PresentEx_pfn );

        SK_Hook_TargetFromVFTable (
          LocalHook_D3D9PresentEx,
            (void **)&pDevEx.p, 121 );

        if (LocalHook_D3D9PresentEx.active) ++hooked;
      }
    }
  }

  return hooked;
}


__declspec (noinline)
void
STDMETHODCALLTYPE
D3D9Reset_Pre ( IDirect3DDevice9      *This,
                D3DPRESENT_PARAMETERS *pPresentationParameters,
                D3DDISPLAYMODEEX      *pFullscreenDisplayMode )
{
  UNREFERENCED_PARAMETER (This);
  UNREFERENCED_PARAMETER (pFullscreenDisplayMode);

  if ( SK_GetCurrentRenderBackend ().device        == nullptr ||
       SK_GetCurrentRenderBackend ().device.IsEqualObject (This) )
  {
    SK_GetCurrentRenderBackend ().releaseOwnedResources ();

    if (ReadAcquire (&ImGui_Init))
    {
      SK_ImGUI_ResetD3D9                    (nullptr);
      ImGui_ImplDX9_InvalidateDeviceObjects (pPresentationParameters);
    }

    SK::D3D9::TextureManager& tex_mgr =
      SK_D3D9_GetTextureManager ();

    if (tex_mgr.init)
    {
      //if (tex_mgr.injector.hasPendingLoads ())
          tex_mgr.loadQueuedTextures ();

      static auto llLastFrame =
        (ULONG64)-1;

      //if (llLastFrame != SK_GetFramesDrawn ())
      {
        tex_mgr.reset ();

    //need_reset.textures = false;

        tex_mgr.resetUsedTextures ( );

    //need_reset.graphics = false;

        llLastFrame = SK_GetFramesDrawn ();
      }
    }

    known_objs->clear ();

    last_frame->clear ();
    tracked_rt->clear ();
    tracked_vs->clear ();
    tracked_ps->clear ();
    tracked_vb->clear ();

    // Clearing the tracked VB only clears state, it doesn't
    //   get rid of any data pointers.
    //
    //  (WE DID NOT QUERY THIS FROM THE D3D RUNTIME, DO NOT RELEASE)
    tracked_vb->vertex_buffer = nullptr;
    tracked_vb->wireframe     = false;
    tracked_vb->wireframes.clear ();
    // ^^^^ This is stupid, add a reset method.

    Shaders->vertex.clear ();
    Shaders->pixel.clear  ();
  }
}

__declspec (noinline)
void
STDMETHODCALLTYPE
D3D9Reset_Post ( IDirect3DDevice9      *This,
                 D3DPRESENT_PARAMETERS *pPresentationParameters,
                 D3DDISPLAYMODEEX      *pFullscreenDisplayMode )
{
  UNREFERENCED_PARAMETER (pFullscreenDisplayMode);

  if ( SK_GetCurrentRenderBackend ().device        == nullptr ||
       SK_GetCurrentRenderBackend ().device.IsEqualObject (This) )
  {
    if (ImGui_ImplDX9_Init ( (void *)pPresentationParameters->hDeviceWindow,
                               This,
                                 pPresentationParameters) )
    {
      InterlockedExchange ( &ImGui_Init, TRUE );
    }

    trigger_reset       = reset_stage_e::Clear;
    request_mode_change = mode_change_request_e::None;

    if ( SK_GetCurrentRenderBackend ().api == SK_RenderAPI::D3D9   ||
         SK_GetCurrentRenderBackend ().api == SK_RenderAPI::D3D9Ex ||
         SK_GetCurrentRenderBackend ().api == SK_RenderAPI::Reserved )
    {
      SK_GetCurrentRenderBackend ().device = This;
    }
  }
}

__declspec (noinline)
HRESULT
STDMETHODCALLTYPE
D3D9Reset_Override ( IDirect3DDevice9      *This,
                     D3DPRESENT_PARAMETERS *pPresentationParameters )
{
  HRESULT hr = E_FAIL;

  if (This == nullptr)
    return E_NOINTERFACE;

 if (pPresentationParameters == nullptr)
   return D3DERR_INVALIDCALL;

  if (SK_TLS_Bottom ()->render->d3d9->ctx_init_thread)
  {
    return D3D9Device_Reset_Original ( This,
                                         pPresentationParameters );
  }


  SK_ComQIPtr <IDirect3DDevice9Ex> pDev9Ex (This);

  bool bD3D9ExDevice = ( config.render.d3d9.force_d3d9ex &&
                           pDev9Ex.p != nullptr );

  if (bD3D9ExDevice)
  {
    return
      D3D9ResetEx (pDev9Ex, pPresentationParameters, nullptr);
  }


  SK_LOGi0 ( L"[!] %s (%08" _L(PRIxPTR) L"h, %08" _L(PRIxPTR) L"h) - "
                 L"%s",
                 L"IDirect3DDevice9::Reset",
                   (uintptr_t)This, (uintptr_t)pPresentationParameters,
                     SK_SummarizeCaller ().c_str ()
  );

  SK_InitWindow ( pPresentationParameters->hDeviceWindow,
               (! pPresentationParameters->Windowed) );



  SK_D3D9_SetFPSTarget    (      pPresentationParameters);
  SK_SetPresentParamsD3D9 (This, pPresentationParameters);


  D3D9Reset_Pre           (This, pPresentationParameters, nullptr);

  auto orig_se =
  SK_SEH_ApplyTranslator (SK_FilteringStructuredExceptionTranslator (EXCEPTION_ACCESS_VIOLATION));
  try {
    D3D9_CALL (hr, D3D9Device_Reset_Original (This,
                                                pPresentationParameters));
  } catch (const SK_SEH_IgnoredException&) {  hr = D3DERR_DEVICENOTRESET; }
  SK_SEH_RemoveTranslator (orig_se);

  if (SUCCEEDED (hr))
  {
    SK_SetPresentParamsD3D9 (This, pPresentationParameters);
    D3D9Reset_Post          (This, pPresentationParameters, nullptr);
  }

  else
  {
    // Parameters passed were invalid... are SK overrides to blame?
    if (hr == D3DERR_INVALIDCALL)
    {
      if ((! config.window.res.override.isZero ()) || config.render.framerate.refresh_rate > 0.0f)
      {
        // Maybe resolution is not supported in fullscreen?
        if (pPresentationParameters->Windowed == FALSE)
        {
          pPresentationParameters->Windowed                   =    TRUE;
          pPresentationParameters->FullScreen_RefreshRateInHz =       0;
        }
      }

      if (pPresentationParameters->Windowed)
          pPresentationParameters->FullScreen_RefreshRateInHz = 0;

      if (config.render.framerate.present_interval != SK_NoPreference)
      {
        pPresentationParameters->PresentationInterval = D3DPRESENT_INTERVAL_DEFAULT;
      }

      D3D9_CALL (hr, D3D9Device_Reset_Original (This,
                                                  pPresentationParameters));

      if (SUCCEEDED (hr))
      {
        SK_SetPresentParamsD3D9 (This, pPresentationParameters);
        D3D9Reset_Post          (This, pPresentationParameters, nullptr);

        return hr;
      }
    }

    else
      D3D9Reset_Pre ( This, pPresentationParameters, nullptr );
  }

  return hr;
}

__declspec (noinline)
HRESULT
STDMETHODCALLTYPE
D3D9ResetEx ( IDirect3DDevice9Ex    *This,
              D3DPRESENT_PARAMETERS *pPresentationParameters,
              D3DDISPLAYMODEEX      *pFullscreenDisplayMode )
{
  HRESULT hr = E_FAIL;

  if (This == nullptr)
    return E_NOINTERFACE;

  if (pPresentationParameters == nullptr)
    return D3DERR_INVALIDCALL;


  if (SK_TLS_Bottom ()->render->d3d9->ctx_init_thread_ex)
  {
    return D3D9ExDevice_ResetEx_Original ( This,
                                             pPresentationParameters,
                                               pFullscreenDisplayMode );
  }


  D3DDISPLAYMODEEX _d3dex_implicit_mode = { };

  if ( config.render.d3d9.force_d3d9ex &&
       config.render.d3d9.enable_flipex )
  {
    pPresentationParameters->BackBufferCount     =       3;
    pPresentationParameters->SwapEffect          = D3DSWAPEFFECT_FLIPEX;
    pPresentationParameters->Flags              &= ~D3DPRESENTFLAG_LOCKABLE_BACKBUFFER;
    pPresentationParameters->Flags              &= ~D3DPRESENTFLAG_DEVICECLIP;
    pPresentationParameters->MultiSampleType     = D3DMULTISAMPLE_NONE;
    pPresentationParameters->MultiSampleQuality  = 0;

    if (! pPresentationParameters->Windowed)
    {
      _d3dex_implicit_mode = {
        sizeof (D3DDISPLAYMODEEX),

        pPresentationParameters->BackBufferWidth,
        pPresentationParameters->BackBufferHeight,
        pPresentationParameters->FullScreen_RefreshRateInHz,
        pPresentationParameters->BackBufferFormat,
        D3DSCANLINEORDERING_UNKNOWN
      };

      SK_LOGi0 (L"Using Implicit D3D9Ex Fullscreen Mode");

      pFullscreenDisplayMode = &_d3dex_implicit_mode;
    }
  }



  SK_LOGi0 ( L"[!] %s (%08" _L(PRIxPTR) L"h, %08" _L(PRIxPTR) L"h,"
                    L" %08"  _L(PRIxPTR) L"h) - "
                 L"%s",
                   L"IDirect3DDevice9Ex::ResetEx",
                     (uintptr_t)This, (uintptr_t)pPresentationParameters,
                     (uintptr_t)pFullscreenDisplayMode,
                       SK_SummarizeCaller ().c_str () );

  SK_InitWindow ( pPresentationParameters->hDeviceWindow,
               (! pPresentationParameters->Windowed) );



  SK_D3D9_SetFPSTarget    (      pPresentationParameters, pFullscreenDisplayMode);
  SK_SetPresentParamsD3D9 (This, pPresentationParameters);



  D3D9Reset_Pre           (This, pPresentationParameters, pFullscreenDisplayMode);

  auto orig_se =
  SK_SEH_ApplyTranslator (SK_FilteringStructuredExceptionTranslator (EXCEPTION_ACCESS_VIOLATION));
  try {
    D3D9_CALL (hr, D3D9ExDevice_ResetEx_Original ( This,
                                                     pPresentationParameters,
                                                       pFullscreenDisplayMode ));
  } catch (const SK_SEH_IgnoredException&) {  hr = D3DERR_DEVICENOTRESET; }
  SK_SEH_RemoveTranslator (orig_se);

  if (SUCCEEDED (hr))
  {
    SK_SetPresentParamsD3D9 (This, pPresentationParameters);
    D3D9Reset_Post          (This, pPresentationParameters, pFullscreenDisplayMode);
  }

  else
  {
    // Parameters passed were invalid... are SK overrides to blame?
    if (hr == D3DERR_INVALIDCALL)
    {
      if ((! config.window.res.override.isZero ()) || config.render.framerate.refresh_rate > 0.0f)
      {
        // Maybe resolution is not supported in fullscreen?
        if (pPresentationParameters->Windowed == FALSE)
        {
          pPresentationParameters->Windowed                   =    TRUE;
          pPresentationParameters->FullScreen_RefreshRateInHz =       0;
        }
      }

      if (pPresentationParameters->Windowed)
          pPresentationParameters->FullScreen_RefreshRateInHz = 0;

      if (config.render.framerate.present_interval != SK_NoPreference)
      {
        pPresentationParameters->PresentationInterval = D3DPRESENT_INTERVAL_DEFAULT;
      }

      D3D9_CALL (hr, D3D9ExDevice_ResetEx_Original ( This,
                                                     pPresentationParameters,
                                                       pFullscreenDisplayMode ));

      if (SUCCEEDED (hr))
      {
        SK_SetPresentParamsD3D9 (This, pPresentationParameters);
        D3D9Reset_Post          (This, pPresentationParameters, pFullscreenDisplayMode);

        return hr;
      }
    }

    D3D9Reset_Pre ( This, pPresentationParameters, pFullscreenDisplayMode );
  }


  return hr;
}

__declspec (noinline)
void
STDMETHODCALLTYPE
D3D9SetGammaRamp_Override ( IDirect3DDevice9 *This,
                 _In_       UINT              iSwapChain,
                 _In_       DWORD             Flags,
                 _In_ const D3DGAMMARAMP     *pRamp )
{
  SK_LOGi0 (L"SetGammaRamp (...) ");

  return
    D3D9Device_SetGammaRamp_Original ( This,
                                         iSwapChain,
                                           Flags,
                                             pRamp );
}

__declspec (noinline)
HRESULT
STDMETHODCALLTYPE
D3D9DrawPrimitive_Override ( IDirect3DDevice9 *This,
                             D3DPRIMITIVETYPE  PrimitiveType,
                             UINT              StartVertex,
                             UINT              PrimitiveCount )
{
  static auto& _draw_state = draw_state.get ();

  ++_draw_state.draws;
  ++_draw_state.draw_count;

  bool wireframe = false;

  if (SK_D3D9_ShouldSkipRenderPass (PrimitiveType, PrimitiveCount, StartVertex, wireframe))
    return S_OK;

  HRESULT hr =
    D3D9Device_DrawPrimitive_Original ( This,
                                          PrimitiveType,
                                            StartVertex,
                                              PrimitiveCount );

  if (wireframe)
    This->SetRenderState (D3DRS_FILLMODE, D3DFILL_SOLID);

  return hr;
}

__declspec (noinline)
HRESULT
STDMETHODCALLTYPE
D3D9DrawIndexedPrimitive_Override ( IDirect3DDevice9 *This,
                                    D3DPRIMITIVETYPE  Type,
                                    INT               BaseVertexIndex,
                                    UINT              MinVertexIndex,
                                    UINT              NumVertices,
                                    UINT              startIndex,
                                    UINT              primCount )
{
  static auto& _draw_state = draw_state.get ();

  ++_draw_state.draws;
  ++_draw_state.draw_count;

  bool wireframe = false;

  if (SK_D3D9_ShouldSkipRenderPass (Type, primCount, startIndex, wireframe))
    return S_OK;

  HRESULT hr =
    D3D9Device_DrawIndexedPrimitive_Original ( This,
                                                 Type,
                                                   BaseVertexIndex,
                                                     MinVertexIndex,
                                                       NumVertices,
                                                         startIndex,
                                                           primCount );

  if (wireframe)
    This->SetRenderState (D3DRS_FILLMODE, D3DFILL_SOLID);

  return hr;
}

__declspec (noinline)
HRESULT
STDMETHODCALLTYPE
D3D9DrawPrimitiveUP_Override (       IDirect3DDevice9 *This,
                                     D3DPRIMITIVETYPE  PrimitiveType,
                                     UINT              PrimitiveCount,
                               const void             *pVertexStreamZeroData,
                                     UINT              VertexStreamZeroStride )
{
  static auto& _draw_state = draw_state.get ();

  ++_draw_state.draws;
  ++_draw_state.draw_count;

  bool wireframe = false;

  if (SK_D3D9_ShouldSkipRenderPass (PrimitiveType, PrimitiveCount, 0, wireframe))
    return S_OK;

  HRESULT hr =
    D3D9Device_DrawPrimitiveUP_Original ( This,
                                            PrimitiveType,
                                              PrimitiveCount,
                                                pVertexStreamZeroData,
                                                  VertexStreamZeroStride );

  if (wireframe)
    This->SetRenderState (D3DRS_FILLMODE, D3DFILL_SOLID);

  return hr;
}

__declspec (noinline)
HRESULT
STDMETHODCALLTYPE
D3D9DrawIndexedPrimitiveUP_Override (       IDirect3DDevice9 *This,
                                            D3DPRIMITIVETYPE  PrimitiveType,
                                            UINT              MinVertexIndex,
                                            UINT              NumVertices,
                                            UINT              PrimitiveCount,
                                      const void             *pIndexData,
                                            D3DFORMAT         IndexDataFormat,
                                      const void             *pVertexStreamZeroData,
                                            UINT              VertexStreamZeroStride )
{
  static auto& _draw_state = draw_state.get ();

  ++_draw_state.draws;
  ++_draw_state.draw_count;

  bool wireframe = false;

  if (SK_D3D9_ShouldSkipRenderPass (PrimitiveType, PrimitiveCount, MinVertexIndex, wireframe))
    return S_OK;

  HRESULT hr =
    D3D9Device_DrawIndexedPrimitiveUP_Original (
      This,
        PrimitiveType,
          MinVertexIndex,
            NumVertices,
              PrimitiveCount,
                pIndexData,
                  IndexDataFormat,
                    pVertexStreamZeroData,
                      VertexStreamZeroStride );

  if (wireframe)
    This->SetRenderState (D3DRS_FILLMODE, D3DFILL_SOLID);

  return hr;
}

__declspec (noinline)
HRESULT
STDMETHODCALLTYPE
D3D9GetTexture_Override ( IDirect3DDevice9      *This,
                     _In_ DWORD                  Stage,
                    _Out_ IDirect3DBaseTexture9 *pTexture )
{
//  dll_log.Log (L"Impure Get Texture");

  return
    D3D9Device_GetTexture_Original ( This,
                                       Stage,
                                         pTexture );
}

__declspec (noinline)
HRESULT
STDMETHODCALLTYPE
D3D9SetTexture_Override ( IDirect3DDevice9      *This,
                    _In_  DWORD                  Stage,
                    _In_  IDirect3DBaseTexture9 *pTexture )
{
  return
    D3D9Device_SetTexture_Original ( This,
                                       Stage,
                                         pTexture );
}

__declspec (noinline)
HRESULT
STDMETHODCALLTYPE
D3D9SetSamplerState_Override ( IDirect3DDevice9    *This,
                               DWORD                Sampler,
                               D3DSAMPLERSTATETYPE  Type,
                               DWORD                Value )
{
  if (config.textures.clamp_lod_bias)
  {
    if (Type == D3DSAMP_MIPMAPLODBIAS)
    {
      auto pfValue = (FLOAT *)&Value;
          *pfValue =
            std::max (0.0f, *pfValue);
    }
  }

  return
    D3D9Device_SetSamplerState_Original ( This,
                                            Sampler,
                                              Type,
                                                Value );
}

__declspec (noinline)
HRESULT
STDMETHODCALLTYPE
D3D9SetViewport_Override (       IDirect3DDevice9 *This,
                           CONST D3DVIEWPORT9     *pViewport )
{
#if 0
  if (SK_GetCurrentGameID () == SK_GAME_ID::YS_Seven)
  {
    D3DVIEWPORT9 scaled_vp = *pViewport;

    scaled_vp.Width  <<= 1;
    scaled_vp.Height <<= 1;

    float left_ndc = 2.0f * ((float)pViewport->X / pViewport->Width)  - 1.0f;
    float top_ndc  = 2.0f * ((float)pViewport->Y / pViewport->Height) - 1.0f;

    scaled_vp.Y = (LONG)((top_ndc  * scaled_vp.Height + scaled_vp.Height) / 2.0f);
    scaled_vp.X = (LONG)((left_ndc * scaled_vp.Width  + scaled_vp.Width)  / 2.0f);

    return
      D3D9SetViewport_Original ( This,
                                   &scaled_vp );
  }
#endif

  return
    D3D9Device_SetViewport_Original ( This,
                                        pViewport );
}

__declspec (noinline)
HRESULT
STDMETHODCALLTYPE
D3D9SetRenderState_Override ( IDirect3DDevice9   *This,
                              D3DRENDERSTATETYPE  State,
                              DWORD               Value )
{
  return
    D3D9Device_SetRenderState_Original ( This,
                                           State,
                                             Value );
}

__declspec (noinline)
HRESULT
STDMETHODCALLTYPE
D3D9SetVertexShaderConstantF_Override (       IDirect3DDevice9 *This,
                                              UINT              StartRegister,
                                        CONST float            *pConstantData,
                                              UINT              Vector4fCount )
{
  return
    D3D9Device_SetVertexShaderConstantF_Original ( This,
                                                     StartRegister,
                                                       pConstantData,
                                                         Vector4fCount );
}

__declspec (noinline)
HRESULT
STDMETHODCALLTYPE
D3D9SetPixelShaderConstantF_Override (       IDirect3DDevice9 *This,
                                             UINT              StartRegister,
                                       CONST float            *pConstantData,
                                             UINT              Vector4fCount )
{
  return
    SK_D3D9_Trampoline (D3D9Device, SetPixelShaderConstantF)
                         ( This,
                             StartRegister,
                               pConstantData,
                                 Vector4fCount );
}

__declspec (noinline)
HRESULT
STDMETHODCALLTYPE
D3D9SetPixelShader_Override ( IDirect3DDevice9      *This,
                              IDirect3DPixelShader9 *pShader )
{
  HRESULT hr =
    SK_D3D9_Trampoline (D3D9Device, SetPixelShader)
                       (This,       pShader);

  if (SUCCEEDED (hr))
  {
    SK_D3D9_SetPixelShader (This, pShader);
  }

  return hr;
}

__declspec (noinline)
HRESULT
STDMETHODCALLTYPE
D3D9SetVertexShader_Override ( IDirect3DDevice9       *This,
                               IDirect3DVertexShader9 *pShader )
{
  HRESULT hr =
    SK_D3D9_Trampoline (D3D9Device, SetVertexShader)
                       (This,       pShader);

  if (SUCCEEDED (hr))
  {
    SK_D3D9_SetVertexShader (This, pShader);
  }

  return hr;
}

__declspec (noinline)
HRESULT
STDMETHODCALLTYPE
D3D9SetScissorRect_Override ( IDirect3DDevice9 *This,
                        const RECT             *pRect)
{
  return
    D3D9Device_SetScissorRect_Original ( This,
                                           pRect );
}

__declspec (noinline)
HRESULT
STDMETHODCALLTYPE
D3D9CreateVolumeTexture_Override ( IDirect3DDevice9         *This,
                                   UINT                      Width,
                                   UINT                      Height,
                                   UINT                      Depth,
                                   UINT                      Levels,
                                   DWORD                     Usage,
                                   D3DFORMAT                 Format,
                                   D3DPOOL                   Pool,
                                   IDirect3DVolumeTexture9 **ppVolumeTexture,
                                   HANDLE                   *pSharedHandle )
{
  SK_ComQIPtr <IDirect3DDevice9Ex>
       pDev9Ex (This);
  if ( pDev9Ex.p != nullptr &&
            Pool == D3DPOOL_MANAGED )
  {
    SK_LOGi1 (L" >> Reassigning Managed Volume Texture to 'Default' Pool (D3D9Ex Override)");

    Pool   =  D3DPOOL_DEFAULT;
    Usage |= D3DUSAGE_DYNAMIC;
  }

  return
    D3D9Device_CreateVolumeTexture_Original ( This,
                                                Width, Height, Depth, Levels, Usage,
                                                  Format, Pool, ppVolumeTexture,
                                                    pSharedHandle );
}

__declspec (noinline)
HRESULT
STDMETHODCALLTYPE
D3D9CreateCubeTexture_Override ( IDirect3DDevice9       *This,
                                 UINT                    EdgeLength,
                                 UINT                    Levels,
                                 DWORD                   Usage,
                                 D3DFORMAT               Format,
                                 D3DPOOL                 Pool,
                                 IDirect3DCubeTexture9 **ppCubeTexture,
                                 HANDLE                 *pSharedHandle )
{
  SK_ComQIPtr <IDirect3DDevice9Ex>
       pDev9Ex (This);
  if ( pDev9Ex.p != nullptr &&
            Pool == D3DPOOL_MANAGED )
  {
    SK_LOGi1 (L" >> Reassigning Managed Cube Texture to 'Default' Pool (D3D9Ex Override)");

    Pool   =  D3DPOOL_DEFAULT;
    Usage |= D3DUSAGE_DYNAMIC;
  }

  return
    D3D9Device_CreateCubeTexture_Original ( This,
                                              EdgeLength, Levels, Usage,
                                                Format, Pool, ppCubeTexture,
                                                  pSharedHandle );
}

__declspec (noinline)
HRESULT
STDMETHODCALLTYPE
D3D9CreateTexture_Override ( IDirect3DDevice9   *This,
                             UINT                Width,
                             UINT                Height,
                             UINT                Levels,
                             DWORD               Usage,
                             D3DFORMAT           Format,
                             D3DPOOL             Pool,
                             IDirect3DTexture9 **ppTexture,
                             HANDLE             *pSharedHandle )
{
  return
    D3D9Device_CreateTexture_Original ( This,
                                          Width, Height, Levels,
                                            Usage, Format, Pool,
                                              ppTexture, pSharedHandle );
}


SK_LazyGlobal <std::unordered_set <IDirect3DVertexBuffer9 *>>          ffxiii_dynamic;
SK_LazyGlobal <std::unordered_map <IDirect3DVertexBuffer9 *, ULONG64>> ffxiii_dynamic_updates;

using D3D9VertexBuffer_Lock_pfn = HRESULT (STDMETHODCALLTYPE *)( IDirect3DVertexBuffer9 *This,
                                                                 UINT                    OffsetToLock,
                                                                 UINT                    SizeToLock,
                                                                 void**                  ppbData,
                                                                 DWORD                   Flags );

D3D9VertexBuffer_Lock_pfn D3D9VertexBuffer_Lock_Original = nullptr;

__declspec (noinline)
HRESULT
STDMETHODCALLTYPE
D3D9VertexBuffer_Lock_Override ( IDirect3DVertexBuffer9 *This,
                                 UINT                    OffsetToLock,
                                 UINT                    SizeToLock,
                                 void**                  ppbData,
                                 DWORD                   Flags )
{
  static bool ffxiii =
    SK_GetModuleHandle (L"ffxiiiimg.exe") != nullptr;

  if (ffxiii)
  {
    std::scoped_lock <SK_Thread_HybridSpinlock>
          scope_lock (*lock_vb);

    if (ffxiii_dynamic->count (This))
    {
      static auto& _ffxiii_dynamic_updates =
                    ffxiii_dynamic_updates.get ();

      ULONG64 current_frame = SK_GetFramesDrawn ();
      DWORD   dwFlags       = D3DLOCK_NOOVERWRITE;

      if (_ffxiii_dynamic_updates [This] != current_frame)
      {
        // Discard each frame, and no-overwrite updates mid-frame
        dwFlags                       = D3DLOCK_DISCARD;
        _ffxiii_dynamic_updates [This] = current_frame;
      }

      return D3D9VertexBuffer_Lock_Original (This, OffsetToLock, SizeToLock, ppbData, dwFlags);
    }
  }

  return D3D9VertexBuffer_Lock_Original (This, OffsetToLock, SizeToLock, ppbData, Flags);
}


__declspec (noinline)
HRESULT
STDMETHODCALLTYPE
D3D9CreateVertexBuffer_Override
(
  _In_  IDirect3DDevice9        *This,
  _In_  UINT                     Length,
  _In_  DWORD                    Usage,
  _In_  DWORD                    FVF,
  _In_  D3DPOOL                  Pool,
  _Out_ IDirect3DVertexBuffer9 **ppVertexBuffer,
  _In_  HANDLE                  *pSharedHandle )
{
  SK_ComQIPtr <IDirect3DDevice9Ex>
       pDev9Ex (This);
  if ( pDev9Ex.p != nullptr &&
            Pool == D3DPOOL_MANAGED )
  {
    SK_LOGi1 (L" >> Reassigning Managed Vertex Buffer to 'Default' Pool (D3D9Ex Override)");

    Pool   =  D3DPOOL_DEFAULT;
    Usage |= D3DUSAGE_DYNAMIC;
  //Usage |= D3DUSAGE_WRITEONLY;
  }

  static bool ffxiii =
    SK_GetModuleHandle (L"ffxiiiimg.exe") != nullptr;

  if (ffxiii)
  {
    // This is the UI vertex buffer
    //if (Length == 358400)
    if (Length >= 10240)
    {
      Usage = D3DUSAGE_DYNAMIC | Usage | D3DUSAGE_WRITEONLY;
      Pool  = D3DPOOL_DEFAULT;
    }
  }


  HRESULT hr =
    D3D9Device_CreateVertexBuffer_Original ( This,
                                               Length, Usage,
                                               FVF,    Pool,
                                                 ppVertexBuffer,
                                                   pSharedHandle );

  if (SUCCEEDED (hr) && ffxiii && ppVertexBuffer != nullptr)
  {
    std::scoped_lock <SK_Thread_HybridSpinlock>
          scope_lock (*lock_vb);

    if (Length >= 10240)
    {
      *ppVertexBuffer = nullptr;

      static bool          hooked = false;
      if (! std::exchange (hooked,  true))
      {
        D3D9_INTERCEPT ( ppVertexBuffer, 11,
                         "IDirect3DVertexBuffer9::Lock",
                          D3D9VertexBuffer_Lock_Override,
                          D3D9VertexBuffer_Lock_Original,
                          D3D9VertexBuffer_Lock_pfn );
        SK_ApplyQueuedHooks ( );
      }

      ffxiii_dynamic->emplace      (*ppVertexBuffer);
      ffxiii_dynamic_updates.get ()[*ppVertexBuffer] = 0;
    }

    else
    {
      ffxiii_dynamic->erase        (*ppVertexBuffer);
      ffxiii_dynamic_updates.get ()[*ppVertexBuffer] = 0;
    }
  }

  if (SUCCEEDED (hr) && ppVertexBuffer != nullptr)
  {
    static auto& _known_objs = known_objs.get ();

    std::scoped_lock <SK_Thread_HybridSpinlock>
          scope_lock (*lock_vb);

    if (Usage & D3DUSAGE_DYNAMIC)
      _known_objs.dynamic_vbs.emplace (*ppVertexBuffer);
    else
      _known_objs.static_vbs.emplace  (*ppVertexBuffer);
  }

  return hr;
}

__declspec (noinline)
HRESULT
STDMETHODCALLTYPE
D3D9CreateIndexBuffer_Override ( IDirect3DDevice9         *This,
                                 UINT                      Length,
                                 DWORD                     Usage,
                                 D3DFORMAT                 Format,
                                 D3DPOOL                   Pool,
                                 IDirect3DIndexBuffer9   **ppIndexBuffer,
                                 HANDLE                   *pSharedHandle )
{
  SK_ComQIPtr <IDirect3DDevice9Ex>
       pDev9Ex (This);
  if ( pDev9Ex.p != nullptr &&
            Pool == D3DPOOL_MANAGED )
  {
    SK_LOGi1 (L" >> Reassigning Managed Index Buffer to 'Default' Pool (D3D9Ex Override)");

    Pool   =  D3DPOOL_DEFAULT;
    Usage |= D3DUSAGE_DYNAMIC;
  //Usage |= D3DUSAGE_WRITEONLY;
  }

  return
    D3D9Device_CreateIndexBuffer_Original ( This,
                                              Length, Usage, Format, Pool,
                                                ppIndexBuffer, pSharedHandle );
}

__declspec (noinline)
HRESULT
STDMETHODCALLTYPE
D3D9SetStreamSource_Override
(
  IDirect3DDevice9       *This,
  UINT                    StreamNumber,
  IDirect3DVertexBuffer9 *pStreamData,
  UINT                    OffsetInBytes,
  UINT                    Stride )
{
  HRESULT hr =
    D3D9Device_SetStreamSource_Original ( This,
                                            StreamNumber,
                                              pStreamData,
                                                OffsetInBytes,
                                                  Stride );

  if (pStreamData != nullptr)
  {
    static auto& _known_objs = known_objs.get ();
    static auto& _last_frame = last_frame.get ();

    if (SUCCEEDED (hr))
    {
      std::scoped_lock <SK_Thread_HybridSpinlock>
            scope_lock (*lock_vb);

      if (_known_objs.dynamic_vbs.count (pStreamData))
        _last_frame.vertex_buffers.dynamic.emplace (pStreamData);
      else if (_known_objs.static_vbs.count (pStreamData))
        _last_frame.vertex_buffers.immutable.emplace (pStreamData);

      else
      {
        D3DVERTEXBUFFER_DESC   desc = { };
        pStreamData->GetDesc (&desc);

        if (desc.Usage & D3DUSAGE_DYNAMIC)
        {
                         _known_objs.dynamic_vbs.emplace (pStreamData);
          _last_frame.vertex_buffers.dynamic.emplace     (pStreamData);
        }

        else
        {
                        _known_objs.static_vbs.emplace (pStreamData);
          _last_frame.vertex_buffers.immutable.emplace (pStreamData);
        }
      }

      if (StreamNumber == 0)
        vb_stream0 = pStreamData;
    }
  }

  return hr;
}

__declspec (noinline)
HRESULT
STDMETHODCALLTYPE
D3D9SetStreamSourceFreq_Override
(
  IDirect3DDevice9 *This,
  UINT              StreamNumber,
  UINT              FrequencyParameter )
{
  std::scoped_lock <SK_Thread_HybridSpinlock>
        scope_lock (*lock_vb);

  if (StreamNumber == 0 && FrequencyParameter & D3DSTREAMSOURCE_INDEXEDDATA)
  {
    draw_state->instances = ( FrequencyParameter & ( ~D3DSTREAMSOURCE_INDEXEDDATA ) );
  }

  if (StreamNumber == 1 && FrequencyParameter & D3DSTREAMSOURCE_INSTANCEDATA)
  {
  }

  return
    D3D9Device_SetStreamSourceFreq_Original ( This,
                                                StreamNumber,
                                                  FrequencyParameter );
}

__declspec (noinline)
HRESULT
STDMETHODCALLTYPE
D3D9SetFVF_Override ( IDirect3DDevice9 *This,
                      DWORD             FVF )
{
  return
    D3D9Device_SetFVF_Original ( This,
                                   FVF );
}

__declspec (noinline)
HRESULT
STDMETHODCALLTYPE
D3D9SetVertexDeclaration_Override ( IDirect3DDevice9            *This,
                                    IDirect3DVertexDeclaration9 *pDecl )
{
  return
    D3D9Device_SetVertexDeclaration_Original ( This,
                                                 pDecl );
}

__declspec (noinline)
HRESULT
STDMETHODCALLTYPE
D3D9CreateVertexDeclaration_Override
(
        IDirect3DDevice9             *This,
  CONST D3DVERTEXELEMENT9            *pVertexElements,
        IDirect3DVertexDeclaration9 **ppDecl )
{
  return
    D3D9Device_CreateVertexDeclaration_Original ( This,
                                                    pVertexElements,
                                                      ppDecl );
}

__declspec (noinline)
HRESULT
STDMETHODCALLTYPE
D3D9CreateRenderTarget_Override (IDirect3DDevice9     *This,
                                 UINT                  Width,
                                 UINT                  Height,
                                 D3DFORMAT             Format,
                                 D3DMULTISAMPLE_TYPE   MultiSample,
                                 DWORD                 MultisampleQuality,
                                 BOOL                  Lockable,
                                 IDirect3DSurface9   **ppSurface,
                                 HANDLE               *pSharedHandle)
{
  if (SK_GetCurrentGameID () == SK_GAME_ID::YS_Seven)
  {
    SK_ComPtr <IDirect3DSwapChain9> pSwapChain = nullptr;

    if (SUCCEEDED (This->GetSwapChain (0, &pSwapChain.p)))
    {
      D3DPRESENT_PARAMETERS pparams = { };
      pSwapChain->GetPresentParameters (&pparams);

      if (Format == D3DFMT_R5G6B5 && ( Width == pparams.BackBufferWidth ) )
      {
        Format = D3DFMT_X8R8G8B8;
#if 0
        Width  <<= 1;
        Height <<= 1;
#endif
      }
    }
  }

  return
    D3D9Device_CreateRenderTarget_Original ( This,
                                               Width, Height,
                                                 Format,
                                                   MultiSample, MultisampleQuality,
                                                     Lockable,
                                                       ppSurface, pSharedHandle );
}

__declspec (noinline)
HRESULT
STDMETHODCALLTYPE
D3D9CreateDepthStencilSurface_Override ( IDirect3DDevice9     *This,
                                         UINT                  Width,
                                         UINT                  Height,
                                         D3DFORMAT             Format,
                                         D3DMULTISAMPLE_TYPE   MultiSample,
                                         DWORD                 MultisampleQuality,
                                         BOOL                  Discard,
                                         IDirect3DSurface9   **ppSurface,
                                         HANDLE               *pSharedHandle )
{
  if (SK_GetCurrentGameID () == SK_GAME_ID::YS_Seven)
  {
    SK_ComPtr <IDirect3DSwapChain9> pSwapChain = nullptr;

    if (SUCCEEDED (This->GetSwapChain (0, &pSwapChain.p)))
    {
      D3DPRESENT_PARAMETERS pparams = { };
      pSwapChain->GetPresentParameters (&pparams);

      if (Format == D3DFMT_D16 && ( Width == pparams.BackBufferWidth ) )
      {
        Format = D3DFMT_D24X8;
#if 0
        Width  <<= 1;
        Height <<= 1;
#endif
      }
    }
  }

  return
    D3D9Device_CreateDepthStencilSurface_Original ( This,
                                                      Width, Height,
                                                        Format,
                                                          MultiSample, MultisampleQuality,
                                                            Discard,
                                                              ppSurface, pSharedHandle );
}

__declspec (noinline)
HRESULT
STDMETHODCALLTYPE
D3D9SetRenderTarget_Override ( IDirect3DDevice9  *This,
                               DWORD              RenderTargetIndex,
                               IDirect3DSurface9 *pRenderTarget )
{
  return
    D3D9Device_SetRenderTarget_Original ( This,
                                            RenderTargetIndex,
                                              pRenderTarget );
}

__declspec (noinline)
HRESULT
STDMETHODCALLTYPE
D3D9SetDepthStencilSurface_Override ( IDirect3DDevice9  *This,
                                      IDirect3DSurface9 *pNewZStencil )
{
  return
    D3D9Device_SetDepthStencilSurface_Original ( This,
                                                   pNewZStencil );
}

extern SK_LazyGlobal <std::wstring> SK_D3D11_res_root;

__declspec (noinline)
HRESULT
STDMETHODCALLTYPE
D3D9UpdateTexture_Override ( IDirect3DDevice9      *This,
                             IDirect3DBaseTexture9 *pSourceTexture,
                             IDirect3DBaseTexture9 *pDestinationTexture )
{
  SK::D3D9::TextureManager& tex_mgr =
    SK_D3D9_GetTextureManager ();

  if ((! config.textures.d3d9_mod) && (! tex_mgr.init))
  {
    return
      D3D9Device_UpdateTexture_Original ( This,
                                            pSourceTexture,
                                              pDestinationTexture );
  }


  IDirect3DBaseTexture9* pRealSource = pSourceTexture;
  IDirect3DBaseTexture9* pRealDest   = pDestinationTexture;

  bool src_is_wrapped = false;
  bool dst_is_wrapped = false;

  void* dontcare   = nullptr;
  if (pSourceTexture != nullptr &&
      pSourceTexture->QueryInterface (IID_SKTextureD3D9, &dontcare) == S_OK)
  {
    src_is_wrapped = true;
    pRealSource    = static_cast <ISKTextureD3D9 *> (pSourceTexture)->pTex;
  }

  if (pDestinationTexture != nullptr &&
      pDestinationTexture->QueryInterface (IID_SKTextureD3D9, &dontcare) == S_OK)
  {
    dst_is_wrapped = true;
    pRealDest      = static_cast <ISKTextureD3D9 *> (pDestinationTexture)->pTex;
  }


  if (! (pRealSource && pRealDest))
    return D3DERR_INVALIDCALL;


  HRESULT hr =
    D3D9Device_UpdateTexture_Original ( This,
                                          pRealSource,
                                            pRealDest );

  bool bCallerIsSteam = false;
    //StrStrIW (SK_GetModuleName (SK_GetCallingDLL ()).c_str (), L"GameOverlayRenderer") != nullptr;

  if (SUCCEEDED (hr) && src_is_wrapped && dst_is_wrapped && (! bCallerIsSteam) && (! tex_mgr.injector.isInjectionThread ()))
  {
    auto* pSrc =
      static_cast <ISKTextureD3D9 *> (pSourceTexture);
    auto* pDst =
      static_cast <ISKTextureD3D9 *> (pDestinationTexture);

    extern
    LARGE_INTEGER                liLastReset;
    LARGE_INTEGER                liNow = SK_QueryPerf ();

                               // Rudimentary protection against video textures
    if (pDestinationTexture && pDst->tex_crc32c == 0x0)
    {
      pSrc->last_used.QuadPart = liNow.QuadPart;
      pDst->last_used.QuadPart = liNow.QuadPart;

      pSrc->uses++;
      pDst->uses++;

      Texture *pTex = nullptr;

      if (pSrc->tex_crc32c != 0x0)
      {
        pTex = new Texture ();

        pTex->refs++;
        pTex->d3d9_tex = pDst;
        pTex->d3d9_tex->AddRef ();

        pTex->d3d9_tex->tex_crc32c = pSrc->tex_crc32c;
        pTex->d3d9_tex->tex_size   = pSrc->tex_size;
        pTex->crc32c               = pSrc->tex_crc32c;
        pTex->size                 = pSrc->tex_size;

        tex_mgr.addTexture (pSrc->tex_crc32c, pTex, /*SrcDataSize*/pSrc->tex_size);

        //tex_mgr.addTexture (pDst->tex_crc32c, pTex, /*SrcDataSize*/pDst->tex_size);

        bool bInjectable =
          tex_mgr.isTextureInjectable (pDst->tex_crc32c);

        if ((! bInjectable) || (injected_textures.count (pDst->tex_crc32c) && injected_textures [pDst->tex_crc32c] != nullptr))
        {
          if (bInjectable)
          {
            pDst->pTexOverride  = injected_textures   [pDst->tex_crc32c];
            pDst->override_size = injected_sizes      [pDst->tex_crc32c];
            pTex->load_time     = injected_load_times [pDst->tex_crc32c];
          }

          tex_mgr.refTextureEx (pTex);
        }

        else
        {
          tex_mgr.missTexture ();

          TexLoadRequest* load_op =
            nullptr;

          bool remap_stream =
            tex_mgr.injector.isStreaming (pDst->tex_crc32c);

          //
          // Generic injectable textures
          //
          if (tex_mgr.isTextureInjectable (pDst->tex_crc32c))
          {
            tex_log->LogEx (true, L"[Inject Tex] Injectable texture for checksum (%08x)... ",
                            pDst->tex_crc32c);

            TexRecord& record =
              tex_mgr.getInjectableTexture (pDst->tex_crc32c);

            if (record.method == TexLoadMethod::DontCare)
                record.method =  TexLoadMethod::Streaming;

            wchar_t wszInjectFileName [MAX_PATH + 2] = { };

            // If -1, load from disk...
            if (record.archive == -1)
            {
              if (record.method == TexLoadMethod::Streaming)
              {
                swprintf ( wszInjectFileName, LR"(%s\inject\textures\streaming\%08x%s)",
                             SK_D3D11_res_root->c_str (),
                               pDst->tex_crc32c,
                                 L".dds" );
              }

              else if (record.method == TexLoadMethod::Blocking)
              {
                swprintf ( wszInjectFileName, LR"(%s\inject\textures\blocking\%08x%s)",
                             SK_D3D11_res_root->c_str (),
                               pDst->tex_crc32c,
                                 L".dds");
              }
            }

            load_op =
              new TexLoadRequest ();

            load_op->pDevice  = This;
            load_op->checksum = pDst->tex_crc32c;

            if (record.method == TexLoadMethod::Streaming)
              load_op->type    = TexLoadRequest::Stream;
            else
              load_op->type = TexLoadRequest::Immediate;

            wcscpy (load_op->wszFilename, wszInjectFileName);

            if (load_op->type == TexLoadRequest::Stream)
            {
              if (( !remap_stream ))
                tex_log->LogEx (false, L"streaming\n");
              else
                tex_log->LogEx (false, L"in-flight already\n");
            }

            else
            {
              tex_log->LogEx (false, L"blocking (deferred)\n");
            }

            if ( load_op->type == TexLoadRequest::Stream ||
                 load_op->type == TexLoadRequest::Immediate )
            {
              load_op->SrcDataSize =
                sk::narrow_cast <UINT> (
                  record.size
                );

              load_op->pSrc =
                 ((ISKTextureD3D9 *)pDestinationTexture);

              load_op->pDest =
                 ((ISKTextureD3D9 *)pDestinationTexture);

              tex_mgr.injector.lockStreaming ();

              if (load_op->type == TexLoadRequest::Immediate)
                static_cast <ISKTextureD3D9 *> (pDestinationTexture)->must_block = true;

              if (tex_mgr.injector.isStreaming (load_op->checksum))
              {
              //tex_mgr.injector.lockStreaming ();

                auto* pTexOrig =
                  static_cast <ISKTextureD3D9 *> (
                    tex_mgr.injector.getTextureInFlight (load_op->checksum)->pDest
                   );

                // Remap the output of the in-flight texture
                tex_mgr.injector.getTextureInFlight (load_op->checksum)->pDest =
                  pDst;

              //tex_mgr.injector.unlockStreaming ();

                const Texture* pExistingTex =
                  tex_mgr.getTexture (load_op->checksum);

                if (pExistingTex != nullptr)
                {
                  for (int i = 0;
                           i < pExistingTex->refs;
                         ++i)
                  {
                    static_cast <ISKTextureD3D9 *> (pDestinationTexture)->AddRef ();
                  }
                }

                tex_mgr.removeTexture (pTexOrig);
              }

              else
              {
                tex_mgr.injector.addTextureInFlight (load_op);
                pTex->d3d9_tex->AddRef              (       );
                stream_pool.postJob                 (load_op);
              }

              tex_mgr.injector.unlockStreaming ();
            }
          }
        }
      }
    }
  }

  return hr;
}

__declspec (noinline)
HRESULT
STDMETHODCALLTYPE
D3D9StretchRect_Override (      IDirect3DDevice9    *This,
                                IDirect3DSurface9   *pSourceSurface,
                          const RECT                *pSourceRect,
                                IDirect3DSurface9   *pDestSurface,
                          const RECT                *pDestRect,
                                D3DTEXTUREFILTERTYPE Filter )
{
  if (SK_GetCurrentGameID () == SK_GAME_ID::YS_Seven)
  {
    if (pSourceRect != nullptr && pDestRect != nullptr)
    {
      SK_LOGs0 ( L"CompatHack",
                 L"StretchRect: { %i, %i, %ix%i } -> { %i, %i, %ix%i }",
                       pSourceRect->left, pSourceRect->top, pSourceRect->right  - pSourceRect->left,
                                                            pSourceRect->bottom - pSourceRect->top,
                       pDestRect->left,   pDestRect->top,   pDestRect->right    - pDestRect->left,
                                                            pDestRect->bottom   - pDestRect->top );
    }
  }

  return
    D3D9Device_StretchRect_Original ( This,
                                        pSourceSurface,
                                        pSourceRect,
                                          pDestSurface,
                                          pDestRect,
                                            Filter );
}

__declspec (noinline)
HRESULT
STDMETHODCALLTYPE
D3D9SetCursorPosition_Override (      IDirect3DDevice9 *This,
                                 _In_ INT               X,
                                 _In_ INT               Y,
                                 _In_ DWORD             Flags )
{
  return
    D3D9Device_SetCursorPosition_Original ( This,
                                              X,
                                                Y,
                                                  Flags );
}

//
// Returns true if x1 != x2, but x1 is within (tolerance)-many units of x2
//
bool
SK_DiscontEpsilon (int x1, int x2, int tolerance)
{
  if (x1 == x2)
    return false;

  if ( x1 <= (x2 + tolerance) &&
       x1 >= (x2 - tolerance) )
    return true;

  return false;
}

__declspec (noinline)
D3DPRESENT_PARAMETERS*
WINAPI
SK_SetPresentParamsD3D9 ( IDirect3DDevice9      *pDevice,
                          D3DPRESENT_PARAMETERS *pparams )
{
  SK_LOG_FIRST_CALL

  D3DDISPLAYMODEEX  implict_mode_ex = { };
  D3DDISPLAYMODEEX *pImplicitModeEx = &implict_mode_ex;


  return
    SK_SetPresentParamsD3D9Ex (pDevice, pparams, &pImplicitModeEx);
}

__declspec (noinline)
D3DPRESENT_PARAMETERS*
WINAPI
SK_SetPresentParamsD3D9Ex ( IDirect3DDevice9       *pDevice,
                            D3DPRESENT_PARAMETERS  *pparams,
                            D3DDISPLAYMODEEX      **ppFullscreenDisplayMode )
{
  SK_LOG_FIRST_CALL

  SK_RenderBackend& rb =
    SK_GetCurrentRenderBackend ();

  bool bContextInit =
    SK_TLS_Bottom ()->render->d3d9->ctx_init_thread_ex;

  if (! bContextInit)
  {
    if ( pDevice != nullptr &&
         pparams != nullptr && config.render.output.force_10bpc &&
         pparams->BackBufferFormat != D3DFMT_A2R10G10B10 )
    {
      SK_ComPtr <IDirect3D9> pD3D9;
      pDevice->GetDirect3D (&pD3D9);

      // Current Mode (relevant in -Windowed- mode only)
      D3DDISPLAYMODE                dispMode = { };
      pD3D9->GetAdapterDisplayMode (
               D3DADAPTER_DEFAULT, &dispMode);

      static constexpr
        auto OverrideFormat (D3DFMT_A2R10G10B10);

      // Fullscreen can change display modes
      if ( ( config.render.d3d9.force_d3d9ex &&
             config.render.d3d9.enable_flipex ) || (! pparams->Windowed))
        dispMode.Format = OverrideFormat;

      if (SUCCEEDED (
      pD3D9->CheckDeviceType ( D3DADAPTER_DEFAULT,
                               D3DDEVTYPE_HAL, dispMode.Format,
                                               OverrideFormat, pparams->Windowed )))
      {
        SK_LOGi0 ( L" >> Overriding D3D9 SwapChain Format (original = %hs) "
                                                          "override = %hs",
                  SK_D3D9_FormatToStr (pparams->BackBufferFormat).c_str (),
                  SK_D3D9_FormatToStr (           OverrideFormat).c_str () );

        pparams->
          BackBufferFormat =
            OverrideFormat;
      }
    }

    if (pparams != nullptr)
    {
      if (! config.window.res.override.isZero ())
      {
        pparams->BackBufferWidth  = config.window.res.override.x;
        pparams->BackBufferHeight = config.window.res.override.y;
      }

      if ( rb.api == SK_RenderAPI::D3D9   ||
           rb.api == SK_RenderAPI::D3D9Ex ||
           rb.api == SK_RenderAPI::Reserved )
      {
        SK_D3D9_SetFPSTarget (pparams);

        D3DDEVICE_CREATION_PARAMETERS dcparams = {};

        if (pDevice != nullptr)
          pDevice->GetCreationParameters (&dcparams);

        auto& windows =
          rb.windows;

        if (game_window.hWnd == nullptr || (! IsWindow (game_window.hWnd)))
        {
          if (dcparams.hFocusWindow)
            windows.setFocus (dcparams.hFocusWindow);
          else if (pparams->hDeviceWindow)
            windows.setFocus (pparams->hDeviceWindow);
        }

        if (windows.device == nullptr || (! IsWindow (windows.device)))
        {
          windows.setDevice (
            pparams->hDeviceWindow     != nullptr ?
              pparams->hDeviceWindow   :
                (HWND)windows.device   != nullptr?
                  (HWND)windows.device :
                    (HWND)windows.focus );
        }


        bool switch_to_fullscreen = ( config.display.force_fullscreen && pparams->Windowed )  ||
                                       ( (! rb.fullscreen_exclusive)  &&
                                            request_mode_change       == mode_change_request_e::Fullscreen );

        bool switch_to_windowed   = ( config.display.force_windowed   && (! pparams->Windowed) ) ||
                                       (    rb.fullscreen_exclusive   &&
                                            request_mode_change       == mode_change_request_e::Windowed   );


        if (switch_to_fullscreen && windows.device )
        {
          pparams->hDeviceWindow = windows.device;

          HMONITOR hMonitor =
            MonitorFromWindow ( game_window.hWnd,
                                  MONITOR_DEFAULTTONEAREST );

          MONITORINFO mi  = { };
          mi.cbSize       = sizeof (mi);
          GetMonitorInfo (hMonitor, &mi);

          // This must be non-zero to go fullscreen
          if (pparams->FullScreen_RefreshRateInHz == 0)
          {
            if (config.render.framerate.refresh_rate == SK_NoPreference)
            {
              if (ppFullscreenDisplayMode != nullptr &&
                 *ppFullscreenDisplayMode != nullptr)
                pparams->FullScreen_RefreshRateInHz = (*ppFullscreenDisplayMode)->RefreshRate;

              if (pparams->FullScreen_RefreshRateInHz == 0)
              {
                extern double
                SK_Display_GetDefaultRefreshRate (HMONITOR hMonitor);

                pparams->FullScreen_RefreshRateInHz =
                  static_cast <UINT> (SK_Display_GetDefaultRefreshRate (hMonitor));
              }
            }
          }

          if (config.render.framerate.refresh_rate != -1.0f)
          {
            pparams->FullScreen_RefreshRateInHz =
              (UINT)config.render.framerate.refresh_rate;
          }

          if (pparams->FullScreen_RefreshRateInHz != 0)
          {
            pparams->BackBufferCount        = std::min (D3DPRESENT_BACK_BUFFERS_MAX, std::max ((LONG)pparams->BackBufferCount, 1L));
            pparams->EnableAutoDepthStencil = true;

            pparams->Windowed               = FALSE;


            UINT monitor_width  = mi.rcMonitor.right  - mi.rcMonitor.left;
            UINT monitor_height = mi.rcMonitor.bottom - mi.rcMonitor.top;

            if ( pparams->BackBufferWidth < 512 ||
                 pparams->BackBufferWidth > monitor_width )
                 pparams->BackBufferWidth = monitor_width;

            if ( pparams->BackBufferHeight < 256 ||
                 pparams->BackBufferHeight > monitor_height )
                 pparams->BackBufferHeight = monitor_height;


            if (ppFullscreenDisplayMode != nullptr)
            {
              if (*ppFullscreenDisplayMode == nullptr)
                  *ppFullscreenDisplayMode = (D3DDISPLAYMODEEX *)SK_TLS_Bottom ()->render->d3d9->allocTempFullscreenStorage ();

              if (*ppFullscreenDisplayMode != nullptr)
              {
                (*ppFullscreenDisplayMode)->Height           = pparams->BackBufferHeight;
                (*ppFullscreenDisplayMode)->Width            = pparams->BackBufferWidth;
                (*ppFullscreenDisplayMode)->RefreshRate      = pparams->FullScreen_RefreshRateInHz;
                (*ppFullscreenDisplayMode)->Format           = pparams->BackBufferFormat;
                (*ppFullscreenDisplayMode)->ScanLineOrdering = D3DSCANLINEORDERING_PROGRESSIVE;
              }
            }
          }

          else
          {
            pparams->Windowed = TRUE;

            // Must be NULL if the fullscreen override failed
            if (ppFullscreenDisplayMode != nullptr)
              (*ppFullscreenDisplayMode) = nullptr;

            SK_LOGi0 (L" *** Could not force fullscreen mode due to "
                                       L"indeterminate refresh rate!");
          }
        }


        else if (switch_to_windowed)
        {
          if (pparams->hDeviceWindow || dcparams.hFocusWindow != nullptr)
          {
            pparams->Windowed                   = TRUE;
            pparams->FullScreen_RefreshRateInHz = 0;

            // Must be NULL if forcing fullscreen -> windowed
            if (*ppFullscreenDisplayMode != nullptr)
              *ppFullscreenDisplayMode = nullptr;
          }

          else
          {
            SK_LOGi0 (L" *** Could not force windowed mode, because "
                                      L"game has no device window ?!");

            pparams->Windowed = FALSE;
          }
        }


        //
        //  <-(+] Forced Borderless Window [+)->
        //     ------------------------------
        //        -> ( NOT FULLSCREEN ) <-
        //
        if (pparams->Windowed && config.window.borderless && (! config.window.fullscreen))
        {
          //  <- @  [No Resolution Override]  @ ->
          //
          if (config.window.res.override.isZero ())
          {
            RECT wnd_rect =
              *SK_GetGameRect ();

            static int x_dlg = SK_GetSystemMetrics (SM_CXDLGFRAME);
            static int y_dlg = SK_GetSystemMetrics (SM_CYDLGFRAME);
            static int title = SK_GetSystemMetrics (SM_CYCAPTION );

            if ( SK_DiscontEpsilon ( pparams->BackBufferWidth,
                                       ( wnd_rect.right  - wnd_rect.left ),
                                       (       2 * x_dlg + 1             ) )

                                   ||

                 SK_DiscontEpsilon ( pparams->BackBufferHeight,
                                       ( wnd_rect.bottom - wnd_rect.top  ),
                                       (       2 * y_dlg + title + 1     ) )
               )
            {
              pparams->BackBufferWidth  = ( wnd_rect.right  - wnd_rect.left );
              pparams->BackBufferHeight = ( wnd_rect.bottom - wnd_rect.top  );

              SK_LOGs0 ( L"Window Mgr",
                         L"Border Compensated Resolution ==> (%lu x %lu)",
                               pparams->BackBufferWidth,
                                 pparams->BackBufferHeight );
            }

            else
            {
              RECT                                 client_rect = { };
              SK_GetClientRect (game_window.hWnd, &client_rect);

              pparams->BackBufferWidth  = ( client_rect.right  - client_rect.left );
              pparams->BackBufferHeight = ( client_rect.bottom - client_rect.top  );
            }
          }

          //  <- @  [OVERRIDE Resolution]  @ ->
          //
          else
          {
            pparams->BackBufferWidth  = config.window.res.override.x;
            pparams->BackBufferHeight = config.window.res.override.y;
          }
        }

               RECT        client = {        };
        SK_GetClientRect ( pparams->hDeviceWindow,
                          &client );


        //  Non-Zero Values for Backbuffer Width / Height
        //  =============================================
        //
        //   An override may be necessary; at the very least we will make
        //     note of the explicit values provided by the game.
        //
        //
        //  NOTE:    If (Zero, Zero) is supplied for (Width, Height) -- it is a
        // ~~~~~~~     special-case indicating to Windows that the client rect.
        //               determines SwapChain resolution
        //
        //     -=> * The client rectangle can be spoofed by Special K even +
        //         +   to the D3D9 Runtime itself !                        * <=-
        //
        //
        //    -------------------------------------------------------------------
        //    (  The 0x0 case is one we have to consider carefully in order     )
        //    (    to figure out how the game is designed to behave vis-a-vis.  )
        //    (      resolution scaling if the window is resized.               )
        //
        /* User wants an override, so let's get down to brass tacks... */
        if (! config.window.res.override.isZero ())
        {
          pparams->BackBufferWidth  = config.window.res.override.x;
          pparams->BackBufferHeight = config.window.res.override.y;
        }

        if (pparams->BackBufferWidth != 0 && pparams->BackBufferHeight != 0)
        {
          /* If this is Zero, we need to actually create the render device / swapchain and
               then get the value Windows assigned us... */
          SK_SetWindowResX (pparams->BackBufferWidth);
          SK_SetWindowResY (pparams->BackBufferHeight);
        }
        
        // Implicit Resolution
        //
        //
        else
        {
          // If this is zero, we need to actually create the render device / swapchain and
          //   then get the value Windows assigned us...
          SK_SetWindowResX (client.right  - client.left);
          SK_SetWindowResY (client.bottom - client.top);
        }

        // Range Restrict to prevent D3DERR_INVALID_CALL
        if (pparams->PresentationInterval > D3DPRESENT_INTERVAL_IMMEDIATE)
            pparams->PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;

        if (pparams->Windowed)
        {
          //SetWindowPos_Original ( hWndRender,
                                    //HWND_TOP,
                                      //0, 0,
                                        //pparams->BackBufferWidth, pparams->BackBufferHeight,
                                          //SWP_NOZORDER | SWP_NOSENDCHANGING );
        }

        else if (switch_to_fullscreen)
        {
          //if ( SetWindowLongPtrW_Original == nullptr ||
          //     GetWindowLongPtrW_Original == nullptr )
          //{
          //  SetWindowLongPtrW (pparams->hDeviceWindow, GWL_EXSTYLE, (GetWindowLongPtrW (pparams->hDeviceWindow, GWL_EXSTYLE) & ~(WS_EX_TOPMOST)) | (WS_EX_APPWINDOW));
          //  SetWindowPos      (pparams->hDeviceWindow, HWND_TOP, 0, 0, 0, 0, SWP_NOSENDCHANGING | SWP_NOMOVE             | SWP_NOSIZE     | SWP_DEFERERASE |
          //                                                                   SWP_NOCOPYBITS     | SWP_ASYNCWINDOWPOS     | SWP_SHOWWINDOW | SWP_NOREPOSITION );
          //
          //  SK_InstallWindowHook (pparams->hDeviceWindow);
          //}
          //
          //else
          //{
          //  SetWindowLongPtrW_Original (game_window.hWnd, GWL_EXSTYLE, (GetWindowLongPtrW_Original (game_window.hWnd, GWL_EXSTYLE) & ~(WS_EX_TOPMOST)) | (WS_EX_APPWINDOW));
          //  SetWindowPos_Original      (game_window.hWnd, HWND_TOP, 0, 0, 0, 0, SWP_NOSENDCHANGING | SWP_NOMOVE         | SWP_NOSIZE     | SWP_DEFERERASE |
          //                                                                      SWP_NOCOPYBITS     | SWP_ASYNCWINDOWPOS | SWP_SHOWWINDOW | SWP_NOREPOSITION );
          //}
        }

        rb.fullscreen_exclusive = (! pparams->Windowed);
      }
    }
  }

  if (bContextInit || (! pDevice))
  {
    if ( config.render.d3d9.force_d3d9ex &&
         config.render.d3d9.enable_flipex )
    { 
      if (0x0 != (pparams->Flags & D3DPRESENTFLAG_LOCKABLE_BACKBUFFER))
          SK_ImGui_Warning (L"D3D9 Game is not Compatible With FlipEx (Lockable Backbuffer)");

      pparams->Flags &= ~D3DPRESENTFLAG_LOCKABLE_BACKBUFFER;

      if ( 0x0 == (pparams->Flags & D3DPRESENTFLAG_LOCKABLE_BACKBUFFER) &&
           0x0 == (pparams->Flags & D3DPRESENTFLAG_DEVICECLIP) )
      {
        pparams->BackBufferCount                = std::max (3U, pparams->BackBufferCount);
        pparams->PresentationInterval           = D3DPRESENT_INTERVAL_DEFAULT;
        pparams->SwapEffect                     = D3DSWAPEFFECT_FLIPEX;
        pparams->MultiSampleType                = D3DMULTISAMPLE_NONE;
        pparams->MultiSampleQuality             = 0;
    
        if (pparams->Windowed)
        {   pparams->FullScreen_RefreshRateInHz = 0;
            pparams->PresentationInterval       = D3DPRESENT_INTERVAL_IMMEDIATE;
        }
        else
        {
          // We can't switch VSYNC on/off on the fly in fullscreen
          if (config.render.framerate.present_interval == 0)
            pparams->PresentationInterval       = D3DPRESENT_INTERVAL_IMMEDIATE;
        }

        rb.fullscreen_exclusive                 = (! pparams->Windowed);
      }
    
      else
      {
        if (0x0 != (pparams->Flags & D3DPRESENTFLAG_DEVICECLIP))
          SK_ImGui_Warning (L"D3D9 Game is not Compatible With FlipEx (Device Clip)");
      }
    }

    return pparams;
  }

  SK_ComQIPtr <IWrapDirect3DDevice9>
                    pWrappedDevice (pDevice);

  if (pWrappedDevice != nullptr)
  {
#ifdef _DEBUG
    dll_log.Log (L"Using wrapper for SetPresentParams!");
#endif
    // Filter out devices used only for video playback
    if ( rb.api == SK_RenderAPI::D3D9   ||
         rb.api == SK_RenderAPI::D3D9Ex ||
         rb.api == SK_RenderAPI::Reserved )
    {
      rb.setDevice (pDevice);
    }
  }

  SK_ComPtr
   < IDirect3DDevice9Ex >
                 pDevEx = nullptr;

  if ( pparams != nullptr &&
       pDevice != nullptr &&
       ( SUCCEEDED ( ( (IUnknown *)pDevice )->
           QueryInterface <IDirect3DDevice9Ex> (
                                  &pDevEx.p    )
       )           )
     )
  {
    if ( config.render.d3d9.force_d3d9ex &&
         config.render.d3d9.enable_flipex )
    {
      if (0x0 != (pparams->Flags & D3DPRESENTFLAG_LOCKABLE_BACKBUFFER))
        SK_ImGui_Warning (L"D3D9 Game is not Compatible With FlipEx (Lockable Backbuffer)");

      pparams->Flags &= ~D3DPRESENTFLAG_LOCKABLE_BACKBUFFER;

      if ( (pparams->Flags & D3DPRESENTFLAG_LOCKABLE_BACKBUFFER) == 0x0 &&
           (pparams->Flags & D3DPRESENTFLAG_DEVICECLIP)          == 0x0 )
      {
        pparams->BackBufferCount                = std::max (3U, pparams->BackBufferCount);
        pparams->PresentationInterval           = D3DPRESENT_INTERVAL_DEFAULT;
        pparams->SwapEffect                     = D3DSWAPEFFECT_FLIPEX;
        pparams->MultiSampleType                = D3DMULTISAMPLE_NONE;
        pparams->MultiSampleQuality             = 0;
    
        if (pparams->Windowed)
        {   pparams->FullScreen_RefreshRateInHz = 0;
            pparams->PresentationInterval       = D3DPRESENT_INTERVAL_IMMEDIATE;
        }
        else
        {
          // We can't switch VSYNC on/off on the fly in fullscreen
          if (config.render.framerate.present_interval == 0)
            pparams->PresentationInterval       = D3DPRESENT_INTERVAL_IMMEDIATE;
        }

        // This is probably a bad idea: the mode hasn't been set yet.
        rb.fullscreen_exclusive                 = (! pparams->Windowed);
      }
    
      else
      {
        if (0x0 != (pparams->Flags & D3DPRESENTFLAG_DEVICECLIP))
          SK_ImGui_Warning (L"D3D9 Game is not Compatible With FlipEx (Device Clip)");
      }
    }
  }

  if (SK_GetCurrentGameID () == SK_GAME_ID::YS_Seven && pparams != nullptr)
  {
    SK_LOGs0 ( L"CompatHack",
               L"D3D9 Backbuffer using format %hs changed to %hs.",
      SK_D3D9_FormatToStr (pparams->BackBufferFormat).c_str (),
      SK_D3D9_FormatToStr (D3DFMT_X8R8G8B8          ).c_str () );

    pparams->BackBufferFormat       = D3DFMT_X8R8G8B8;
    if (! config.render.d3d9.force_d3d9ex)
    {
      pparams->BackBufferCount      = 1;
      pparams->SwapEffect           = D3DSWAPEFFECT_COPY;
    }
    pparams->MultiSampleType        = D3DMULTISAMPLE_NONE;
    pparams->MultiSampleQuality     = 0;
    pparams->EnableAutoDepthStencil = true;
    pparams->AutoDepthStencilFormat = D3DFMT_D24X8;
  }

  return pparams;
}


int
SK_D3D9_HookDeviceAndSwapchain (
   IDirect3DDevice9    *pDevice_,
   IDirect3DSwapChain9 *pSwapChain )
{
  if (pDevice_ == nullptr) return 0;

  UNREFERENCED_PARAMETER (pSwapChain);

  int num_hooked = 0;

  SK_ComPtr <IDirect3DDevice9> pDevice = nullptr;

  if ( FAILED ( pDevice_->QueryInterface ( IID_IWrapDirect3DDevice9,
                                             (void **)&pDevice.p ) ) )
  {
    // Not wrapped, we're good to go
    pDevice = pDevice_;
  }

  else if (pDevice != nullptr)
  {
    IWrapDirect3DDevice9 *pWrapper = (IWrapDirect3DDevice9 *)pDevice.p;
                           pDevice = pWrapper->pReal;
  }

  else
  {
    return 0;
  }


  //vtbl (This)
  //-----------
  // 3   TestCooperativeLevel
  // 4   GetAvailableTextureMem
  // 5   EvictManagedResources
  // 6   GetDirect3D
  // 7   GetDeviceCaps
  // 8   GetDisplayMode
  // 9   GetCreationParameters
  // 10  SetCursorProperties
  // 11  SetCursorPosition
  // 12  ShowCursor
  // 13  CreateAdditionalSwapChain
  // 14  GetSwapChain
  // 15  GetNumberOfSwapChains
  // 16  Reset
  // 17  Present
  // 18  GetBackBuffer
  // 19  GetRasterStatus
  // 20  SetDialogBoxMode
  // 21  SetGammaRamp
  // 22  GetGammaRamp
  // 23  CreateTexture
  // 24  CreateVolumeTexture
  // 25  CreateCubeTexture
  // 26  CreateVertexBuffer
  // 27  CreateIndexBuffer
  // 28  CreateRenderTarget
  // 29  CreateDepthStencilSurface
  // 30  UpdateSurface
  // 31  UpdateTexture
  // 32  GetRenderTargetData
  // 33  GetFrontBufferData
  // 34  StretchRect
  // 35  ColorFill
  // 36  CreateOffscreenPlainSurface
  // 37  SetRenderTarget
  // 38  GetRenderTarget
  // 39  SetDepthStencilSurface
  // 40  GetDepthStencilSurface
  // 41  BeginScene
  // 42  EndScene
  // 43  Clear
  // 44  SetTransform
  // 45  GetTransform
  // 46  MultiplyTransform
  // 47  SetViewport
  // 48  GetViewport
  // 49  SetMaterial
  // 50  GetMaterial
  // 51  SetLight
  // 52  GetLight
  // 53  LightEnable
  // 54  GetLightEnable
  // 55  SetClipPlane
  // 56  GetClipPlane
  // 57  SetRenderState
  // 58  GetRenderState
  // 59  CreateStateBlock
  // 60  BeginStateBlock
  // 61  EndStateBlock
  // 62  SetClipStatus
  // 63  GetClipStatus
  // 64  GetTexture
  // 65  SetTexture
  // 66  GetTextureStageState
  // 67  SetTextureStageState
  // 68  GetSamplerState
  // 69  SetSamplerState
  // 70  ValidateDevice
  // 71  SetPaletteEntries
  // 72  GetPaletteEntries
  // 73  SetCurrentTexturePalette
  // 74  GetCurrentTexturePalette
  // 75  SetScissorRect
  // 76  GetScissorRect
  // 77  SetSoftwareVertexProcessing
  // 78  GetSoftwareVertexProcessing
  // 79  SetNPatchMode
  // 80  GetNPatchMode
  // 81  DrawPrimitive
  // 82  DrawIndexedPrimitive
  // 83  DrawPrimitiveUP
  // 84  DrawIndexedPrimitiveUP
  // 85  ProcessVertices
  // 86  CreateVertexDeclaration
  // 87  SetVertexDeclaration
  // 88  GetVertexDeclaration
  // 89  SetFVF
  // 90  GetFVF
  // 91  CreateVertexShader
  // 92  SetVertexShader
  // 93  GetVertexShader
  // 94  SetVertexShaderConstantF
  // 95  GetVertexShaderConstantF
  // 96  SetVertexShaderConstantI
  // 97  GetVertexShaderConstantI
  // 98  SetVertexShaderConstantB
  // 99  GetVertexShaderConstantB
  // 100 SetStreamSource
  // 101 GetStreamSource
  // 102 SetStreamSourceFreq
  // 103 GetStreamSourceFreq
  // 104 SetIndices
  // 105 GetIndices
  // 106 CreatePixelShader
  // 107 SetPixelShader
  // 108 GetPixelShader
  // 109 SetPixelShaderConstantF
  // 110 GetPixelShaderConstantF
  // 111 SetPixelShaderConstantI
  // 112 GetPixelShaderConstantI
  // 113 SetPixelShaderConstantB
  // 114 GetPixelShaderConstantB
  // 115 DrawRectPatch
  // 116 DrawTriPatch
  // 117 DeletePatch
  // 118 CreateQuery


  static volatile LONG                      __hooked      = FALSE;
  if (! InterlockedCompareExchangeAcquire (&__hooked, TRUE, FALSE))
  {
    if (! LocalHook_D3D9TestCooperativeLevel.active)
    {
      D3D9_INTERCEPT ( &pDevice, 3,
                          "IDirect3DDevice9::TestCooperativeLevel",
                          D3D9TestCooperativeLevel_Override,
                          D3D9Device_TestCooperativeLevel_Original,
                          D3D9Device_TestCooperativeLevel_pfn );

      SK_Hook_TargetFromVFTable (
        LocalHook_D3D9TestCooperativeLevel,
          (void **)&pDevice, 3 );

      if (LocalHook_D3D9TestCooperativeLevel.active) ++num_hooked;
    }

//  D3D9_INTERCEPT ( &pDevice, 11,
//                      "IDirect3DDevice9::SetCursorPosition",
//                      D3D9SetCursorPosition_Override,
//                      D3D9SetCursorPosition_Original,
//                      SetCursorPosition_pfn );

    if (! LocalHook_D3D9CreateAdditionalSwapChain.active)
    {
      D3D9_INTERCEPT ( &pDevice, 13,
                          "IDirect3DDevice9::CreateAdditionalSwapChain",
                          D3D9CreateAdditionalSwapChain_Override,
                          D3D9Device_CreateAdditionalSwapChain_Original,
                          D3D9Device_CreateAdditionalSwapChain_pfn );

      SK_Hook_TargetFromVFTable (
        LocalHook_D3D9CreateAdditionalSwapChain,
          (void **)&pDevice, 13 );

      if (LocalHook_D3D9CreateAdditionalSwapChain.active) ++num_hooked;
    }

    if (! SK_D3D9_Trampoline (D3D9Device, SetGammaRamp))
    {
      D3D9_INTERCEPT ( &pDevice, 21,
                          "IDirect3DDevice9::SetGammaRamp",
                          D3D9SetGammaRamp_Override,
                          D3D9Device_SetGammaRamp_Original,
                          D3D9Device_SetGammaRamp_pfn );

      if (SK_D3D9_Trampoline (D3D9Device, SetGammaRamp)) ++num_hooked;
    }

    if (! SK_D3D9_Trampoline (D3D9Device, CreateTexture))
    {
      D3D9_INTERCEPT ( &pDevice, 23,
                          "IDirect3DDevice9::CreateTexture",
                          D3D9CreateTexture_Override,
                          D3D9Device_CreateTexture_Original,
                          D3D9Device_CreateTexture_pfn );

      if (SK_D3D9_Trampoline (D3D9Device, CreateTexture)) ++num_hooked;
    }

    if (! SK_D3D9_Trampoline (D3D9Device, CreateVolumeTexture))
    {
      D3D9_INTERCEPT ( &pDevice, 24,
                          "IDirect3DDevice9::CreateVolumeTexture",
                          D3D9CreateVolumeTexture_Override,
                          D3D9Device_CreateVolumeTexture_Original,
                          D3D9Device_CreateVolumeTexture_pfn );

      if (SK_D3D9_Trampoline (D3D9Device, CreateVolumeTexture)) ++num_hooked;
    }

    if (! SK_D3D9_Trampoline (D3D9Device, CreateCubeTexture))
    {
      D3D9_INTERCEPT ( &pDevice, 25,
                          "IDirect3DDevice9::CreateCubeTexture",
                          D3D9CreateCubeTexture_Override,
                          D3D9Device_CreateCubeTexture_Original,
                          D3D9Device_CreateCubeTexture_pfn );

      if (SK_D3D9_Trampoline (D3D9Device, CreateCubeTexture)) ++num_hooked;
    }

    if (! SK_D3D9_Trampoline (D3D9Device, CreateVertexBuffer))
    {
      D3D9_INTERCEPT ( &pDevice, 26,
                          "IDirect3DDevice9::CreateVertexBuffer",
                          D3D9CreateVertexBuffer_Override,
                          D3D9Device_CreateVertexBuffer_Original,
                          D3D9Device_CreateVertexBuffer_pfn );

      if (SK_D3D9_Trampoline (D3D9Device, CreateVertexBuffer)) ++num_hooked;
    }

    if (! SK_D3D9_Trampoline (D3D9Device, CreateIndexBuffer))
    {
      D3D9_INTERCEPT ( &pDevice, 27,
                          "IDirect3DDevice9::CreateIndexBuffer",
                          D3D9CreateIndexBuffer_Override,
                          D3D9Device_CreateIndexBuffer_Original,
                          D3D9Device_CreateIndexBuffer_pfn );

      if (SK_D3D9_Trampoline (D3D9Device, CreateIndexBuffer)) ++num_hooked;
    }

    if (! SK_D3D9_Trampoline (D3D9Device, CreateRenderTarget))
    {
      D3D9_INTERCEPT ( &pDevice, 28,
                          "IDirect3DDevice9::CreateRenderTarget",
                          D3D9CreateRenderTarget_Override,
                          D3D9Device_CreateRenderTarget_Original,
                          D3D9Device_CreateRenderTarget_pfn );
      if (SK_D3D9_Trampoline (D3D9Device, CreateRenderTarget)) ++num_hooked;
    }

    if (! SK_D3D9_Trampoline (D3D9Device, CreateDepthStencilSurface))
    {
      D3D9_INTERCEPT ( &pDevice, 29,
                          "IDirect3DDevice9::CreateDepthStencilSurface",
                          D3D9CreateDepthStencilSurface_Override,
                          D3D9Device_CreateDepthStencilSurface_Original,
                          D3D9Device_CreateDepthStencilSurface_pfn );

      if (SK_D3D9_Trampoline (D3D9Device, CreateDepthStencilSurface)) ++num_hooked;
    }

    if (! SK_D3D9_Trampoline (D3D9Device, UpdateTexture))
    {
      D3D9_INTERCEPT ( &pDevice, 31,
                          "IDirect3DDevice9::UpdateTexture",
                          D3D9UpdateTexture_Override,
                          D3D9Device_UpdateTexture_Original,
                          D3D9Device_UpdateTexture_pfn );

      if (SK_D3D9_Trampoline (D3D9Device, UpdateTexture)) ++num_hooked;
    }

    if (! SK_D3D9_Trampoline (D3D9Device, StretchRect))
    {
      D3D9_INTERCEPT ( &pDevice, 34,
                          "IDirect3DDevice9::StretchRect",
                          D3D9StretchRect_Override,
                          D3D9Device_StretchRect_Original,
                          D3D9Device_StretchRect_pfn );

      if (SK_D3D9_Trampoline (D3D9Device, StretchRect)) ++num_hooked;
    }

    if (! SK_D3D9_Trampoline (D3D9Device, CreateOffscreenPlainSurface))
    {
      D3D9_INTERCEPT ( &pDevice, 36,
                          "IDirect3DDevice9::CreateOffscreenPlainSurface",
                          D3D9CreateOffscreenPlainSurface_Override,
                          D3D9Device_CreateOffscreenPlainSurface_Original,
                          D3D9Device_CreateOffscreenPlainSurface_pfn );

      if (SK_D3D9_Trampoline (D3D9Device, CreateOffscreenPlainSurface)) ++num_hooked;
    }

    if (! SK_D3D9_Trampoline (D3D9Device, SetRenderTarget))
    {
      D3D9_INTERCEPT ( &pDevice, 37,
                          "IDirect3DDevice9::SetRenderTarget",
                          D3D9SetRenderTarget_Override,
                          D3D9Device_SetRenderTarget_Original,
                          D3D9Device_SetRenderTarget_pfn );

      if (SK_D3D9_Trampoline (D3D9Device, SetRenderTarget)) ++num_hooked;
    }

    if (! SK_D3D9_Trampoline (D3D9Device, SetDepthStencilSurface))
    {
      D3D9_INTERCEPT ( &pDevice, 39,
                       "IDirect3DDevice9::SetDepthStencilSurface",
                        D3D9SetDepthStencilSurface_Override,
                        D3D9Device_SetDepthStencilSurface_Original,
                        D3D9Device_SetDepthStencilSurface_pfn );

      if (SK_D3D9_Trampoline (D3D9Device, SetDepthStencilSurface)) ++num_hooked;
    }

    if (! LocalHook_D3D9BeginScene.active)
    {
      D3D9_INTERCEPT ( &pDevice,
                       41,
                       "IDirect3DDevice9::BeginScene",
                        D3D9BeginScene_Override,
                        D3D9Device_BeginScene_Original,
                        D3D9Device_BeginScene_pfn );

      SK_Hook_TargetFromVFTable (
        LocalHook_D3D9BeginScene,
          (void **)&pDevice, 41 );

      if (LocalHook_D3D9BeginScene.active) ++num_hooked;
    }

    if (! LocalHook_D3D9EndScene.active)
    {
      D3D9_INTERCEPT ( &pDevice,
                       42,
                       "IDirect3DDevice9::EndScene",
                        D3D9EndScene_Override,
                        D3D9Device_EndScene_Original,
                        D3D9Device_EndScene_pfn );

      SK_Hook_TargetFromVFTable (
        LocalHook_D3D9EndScene,
          (void **)&pDevice, 42 );

      if (LocalHook_D3D9EndScene.active) ++num_hooked;
    }

    if (! SK_D3D9_Trampoline (D3D9Device, SetViewport))
    {
      D3D9_INTERCEPT ( &pDevice, 47,
                       "IDirect3DDevice9::SetViewport",
                        D3D9SetViewport_Override,
                        D3D9Device_SetViewport_Original,
                        D3D9Device_SetViewport_pfn );

      if (SK_D3D9_Trampoline (D3D9Device, SetViewport)) ++num_hooked;
    }

    if (! SK_D3D9_Trampoline (D3D9Device, SetRenderState))
    {
      D3D9_INTERCEPT ( &pDevice, 57,
                       "IDirect3DDevice9::SetRenderState",
                        D3D9SetRenderState_Override,
                        D3D9Device_SetRenderState_Original,
                        D3D9Device_SetRenderState_pfn);

      if (SK_D3D9_Trampoline (D3D9Device, SetRenderState)) ++num_hooked;
    }

    if (! SK_D3D9_Trampoline (D3D9Device, SetTexture))
    {
      D3D9_INTERCEPT ( &pDevice, 65,
                       "IDirect3DDevice9::SetTexture",
                        D3D9SetTexture_Override,
                        D3D9Device_SetTexture_Original,
                        D3D9Device_SetTexture_pfn );

      if (SK_D3D9_Trampoline (D3D9Device, SetTexture)) ++num_hooked;
    }

    if (! SK_D3D9_Trampoline (D3D9Device, SetSamplerState))
    {
      D3D9_INTERCEPT ( &pDevice, 69,
                       "IDirect3DDevice9::SetSamplerState",
                        D3D9SetSamplerState_Override,
                        D3D9Device_SetSamplerState_Original,
                        D3D9Device_SetSamplerState_pfn );

      if (SK_D3D9_Trampoline (D3D9Device, SetSamplerState)) ++num_hooked;
    }

    if (! SK_D3D9_Trampoline (D3D9Device, SetScissorRect))
    {
      D3D9_INTERCEPT ( &pDevice, 75,
                       "IDirect3DDevice9::SetScissorRect",
                        D3D9SetScissorRect_Override,
                        D3D9Device_SetScissorRect_Original,
                        D3D9Device_SetScissorRect_pfn );

      if (SK_D3D9_Trampoline (D3D9Device, SetScissorRect)) ++num_hooked;
    }

    if (! SK_D3D9_Trampoline (D3D9Device, DrawPrimitive))
    {
      D3D9_INTERCEPT ( &pDevice, 81,
                       "IDirect3DDevice9::DrawPrimitive",
                        D3D9DrawPrimitive_Override,
                        D3D9Device_DrawPrimitive_Original,
                        D3D9Device_DrawPrimitive_pfn );

      if (SK_D3D9_Trampoline (D3D9Device, DrawPrimitive)) ++num_hooked;
    }

    if (! SK_D3D9_Trampoline (D3D9Device, DrawIndexedPrimitive))
    {
      D3D9_INTERCEPT ( &pDevice, 82,
                       "IDirect3DDevice9::DrawIndexedPrimitive",
                        D3D9DrawIndexedPrimitive_Override,
                        D3D9Device_DrawIndexedPrimitive_Original,
                        D3D9Device_DrawIndexedPrimitive_pfn );

      if (SK_D3D9_Trampoline (D3D9Device, DrawIndexedPrimitive)) ++num_hooked;
    }

    if (! SK_D3D9_Trampoline (D3D9Device, DrawPrimitiveUP))
    {
      D3D9_INTERCEPT ( &pDevice, 83,
                       "IDirect3DDevice9::DrawPrimitiveUP",
                        D3D9DrawPrimitiveUP_Override,
                        D3D9Device_DrawPrimitiveUP_Original,
                        D3D9Device_DrawPrimitiveUP_pfn );

      if (SK_D3D9_Trampoline (D3D9Device, DrawPrimitiveUP)) ++num_hooked;
    }

    if (! SK_D3D9_Trampoline (D3D9Device, DrawIndexedPrimitiveUP))
    {
      D3D9_INTERCEPT ( &pDevice, 84,
                       "IDirect3DDevice9::DrawIndexedPrimitiveUP",
                        D3D9DrawIndexedPrimitiveUP_Override,
                        D3D9Device_DrawIndexedPrimitiveUP_Original,
                        D3D9Device_DrawIndexedPrimitiveUP_pfn );

      if (SK_D3D9_Trampoline (D3D9Device, DrawIndexedPrimitiveUP)) ++num_hooked;
    }

    if (! SK_D3D9_Trampoline (D3D9Device, CreateVertexDeclaration))
    {
      D3D9_INTERCEPT ( &pDevice, 86,
                       "IDirect3DDevice9::CreateVertexDeclaration",
                        D3D9CreateVertexDeclaration_Override,
                        D3D9Device_CreateVertexDeclaration_Original,
                        D3D9Device_CreateVertexDeclaration_pfn );

      if (SK_D3D9_Trampoline (D3D9Device, CreateVertexDeclaration)) ++num_hooked;
    }

    if (! SK_D3D9_Trampoline (D3D9Device, SetVertexDeclaration))
    {
      D3D9_INTERCEPT ( &pDevice, 87,
                       "IDirect3DDevice9::SetVertexDeclaration",
                        D3D9SetVertexDeclaration_Override,
                        D3D9Device_SetVertexDeclaration_Original,
                        D3D9Device_SetVertexDeclaration_pfn );

      if (SK_D3D9_Trampoline (D3D9Device, SetVertexDeclaration)) ++num_hooked;
    }

    if (! SK_D3D9_Trampoline (D3D9Device, SetFVF))
    {
      D3D9_INTERCEPT ( &pDevice, 89,
                       "IDirect3DDevice9::SetFVF",
                        D3D9SetFVF_Override,
                        D3D9Device_SetFVF_Original,
                        D3D9Device_SetFVF_pfn );

      if (SK_D3D9_Trampoline (D3D9Device, SetFVF)) ++num_hooked;
    }

    if (! SK_D3D9_Trampoline (D3D9Device, SetVertexShader))
    {
      D3D9_INTERCEPT ( &pDevice, 92,
                       "IDirect3DDevice9::SetVertexShader",
                        D3D9SetVertexShader_Override,
                        D3D9Device_SetVertexShader_Original,
                        D3D9Device_SetVertexShader_pfn );

      if (SK_D3D9_Trampoline (D3D9Device, SetVertexShader)) ++num_hooked;
    }

    if (! SK_D3D9_Trampoline (D3D9Device, SetVertexShaderConstantF))
    {
      D3D9_INTERCEPT ( &pDevice, 94,
                       "IDirect3DDevice9::SetSetVertexShaderConstantF",
                        D3D9SetVertexShaderConstantF_Override,
                        D3D9Device_SetVertexShaderConstantF_Original,
                        D3D9Device_SetVertexShaderConstantF_pfn );

      if (SK_D3D9_Trampoline (D3D9Device, SetVertexShaderConstantF)) ++num_hooked;
    }

    if (! SK_D3D9_Trampoline (D3D9Device, SetStreamSource))
    {
      D3D9_INTERCEPT ( &pDevice, 100,
                       "IDirect3DDevice9::SetStreamSource",
                        D3D9SetStreamSource_Override,
                        D3D9Device_SetStreamSource_Original,
                        D3D9Device_SetStreamSource_pfn );

      if (SK_D3D9_Trampoline (D3D9Device, SetStreamSource)) ++num_hooked;
    }

    if (! SK_D3D9_Trampoline (D3D9Device, SetStreamSourceFreq))
    {
      D3D9_INTERCEPT ( &pDevice, 102,
                       "IDirect3DDevice9::SetStreamSourceFreq",
                        D3D9SetStreamSourceFreq_Override,
                        D3D9Device_SetStreamSourceFreq_Original,
                        D3D9Device_SetStreamSourceFreq_pfn );

      if (SK_D3D9_Trampoline (D3D9Device, SetStreamSourceFreq)) ++num_hooked;
    }

    if (! SK_D3D9_Trampoline (D3D9Device, SetPixelShader))
    {
      D3D9_INTERCEPT ( &pDevice, 107,
                       "IDirect3DDevice9::SetPixelShader",
                        D3D9SetPixelShader_Override,
                        D3D9Device_SetPixelShader_Original,
                        D3D9Device_SetPixelShader_pfn );

      if (SK_D3D9_Trampoline (D3D9Device, SetPixelShader)) ++num_hooked;
    }

    if (! SK_D3D9_Trampoline (D3D9Device, SetPixelShaderConstantF))
    {
      D3D9_INTERCEPT ( &pDevice, 109,
                       "IDirect3DDevice9::SetPixelShaderConstantF",
                        D3D9SetPixelShaderConstantF_Override,
                        D3D9Device_SetPixelShaderConstantF_Original,
                        D3D9Device_SetPixelShaderConstantF_pfn );

      if (SK_D3D9_Trampoline (D3D9Device, SetPixelShaderConstantF)) ++num_hooked;
    }

    num_hooked += SK_D3D9_HookReset   (pDevice);
    num_hooked += SK_D3D9_HookPresent (pDevice);

//#ifdef SK_AGGRESSIVE_HOOKS
    if (num_hooked > 0)
      SK_RunOnce (SK_ApplyQueuedHooks ());
//#endif

    InterlockedIncrementRelease (&__hooked);
  }

  SK_Thread_SpinUntilAtomicMin (&__hooked, 2);

  return num_hooked;
}

bool
SK_D3D9_IsDummyD3D9Device (D3DPRESENT_PARAMETERS *pPresentationParameters)
{
  if (pPresentationParameters == nullptr)
    return false; // Indeterminate

  if ( ( pPresentationParameters->BackBufferWidth  != 0 &&
         pPresentationParameters->BackBufferWidth  < 32 ) ||
       ( pPresentationParameters->BackBufferHeight != 0 &&
         pPresentationParameters->BackBufferHeight < 32 ) )
  {
    return true;
  }

  if (pPresentationParameters->hDeviceWindow != HWND_DESKTOP)
  {
    if (SK_Win32_IsDummyWindowClass (pPresentationParameters->hDeviceWindow))
      return true;
  }

  return false;
}


//#define WRAP_D3D9_DEVICE
//#define WRAP_D3D9EX_DEVICE

IWrapDirect3DDevice9*
SK_D3D9_WrapDevice ( IUnknown               *pDevice,
                     D3DPRESENT_PARAMETERS  *pPresentationParameters,
                     IDirect3DDevice9      **ppDest )
{
#ifdef WRAP_D3D9_DEVICE
  if (pPresentationParameters != nullptr)
  {
    if (! SK_D3D9_IsDummyD3D9Device (pPresentationParameters))
    {
      SK_LOG0 ( (L" + Direct3DDevice9 (%08" PRIxPTR L"h) wrapped", (uintptr_t)pDevice ),
                 L"   D3D9   " );
      *ppDest =
         (IDirect3DDevice9 *)new IWrapDirect3DDevice9 ((IDirect3DDevice9 *)pDevice);
    }

    else
    {
      *ppDest = (IDirect3DDevice9 *)pDevice;
    }
  }

  else
  {
    SK_LOG0 ( (L" + Direct3DDevice9 {Headless} (%08" PRIxPTR L"h) wrapped", (uintptr_t)pDevice ),
               L"   D3D9   " );

    *ppDest=
       (IDirect3DDevice9 *)new IWrapDirect3DDevice9 ((IDirect3DDevice9 *)pDevice);
  }
#else
  UNREFERENCED_PARAMETER (pPresentationParameters);
  *ppDest = (IDirect3DDevice9*)pDevice;
#endif

  return (IWrapDirect3DDevice9 *)*ppDest;
}

IWrapDirect3DDevice9Ex*
SK_D3D9Ex_WrapDevice ( IUnknown               *pDevice,
                       D3DPRESENT_PARAMETERS  *pPresentationParameters,
                       IDirect3DDevice9Ex    **ppDest )
{
#ifdef WRAP_D3D9EX_DEVICE
  if (pPresentationParameters != nullptr)
  {
    if (! SK_D3D9_IsDummyD3D9Device (pPresentationParameters))
    {
      SK_LOG0 ( (L" + Direct3DDevice9Ex (%08" PRIxPTR L"h) wrapped", (uintptr_t)pDevice ),
                 L"  D3D9Ex  " );
      *ppDest =
         (IDirect3DDevice9Ex *)new IWrapDirect3DDevice9 ((IDirect3DDevice9Ex *)pDevice);
    }

    else
    {
      *ppDest = (IDirect3DDevice9Ex *)pDevice;
    }
  }

  else
  {
    SK_LOG0 ( (L" + Direct3DDevice9Ex {Headless} (%08" PRIxPTR L"h) wrapped", (uintptr_t)pDevice ),
               L"  D3D9Ex  " );

    *ppDest=
       (IDirect3DDevice9Ex *)new IWrapDirect3DDevice9 ((IDirect3DDevice9Ex *)pDevice);
  }
#else
  UNREFERENCED_PARAMETER (pPresentationParameters);
  *ppDest = (IDirect3DDevice9Ex *)pDevice;
#endif

  return (IWrapDirect3DDevice9Ex *)*ppDest;
}


__declspec (noinline)
HRESULT
STDMETHODCALLTYPE
D3D9CreateDeviceEx_Override ( IDirect3D9Ex           *This,
                              UINT                    Adapter,
                              D3DDEVTYPE              DeviceType,
                              HWND                    hFocusWindow,
                              DWORD                   BehaviorFlags,
                              D3DPRESENT_PARAMETERS  *pPresentationParameters,
                              D3DDISPLAYMODEEX       *pFullscreenDisplayMode,
                              IDirect3DDevice9Ex    **ppReturnedDeviceInterface )
{
  // Many D3D11/12/GL games use D3D9 for HW video playback, we need to ignore these
  //
  bool skip_device =
    StrStrIW (SK_GetCallerName ().c_str (), L"action_") || // Action! Video Capture  - TODO: Use Window Class Name
     ( IsWindow (game_window.hWnd) && SK_GetCurrentRenderBackend ().api != SK_RenderAPI::D3D9   &&
                                      SK_GetCurrentRenderBackend ().api != SK_RenderAPI::D3D9Ex &&
                                      SK_GetCurrentRenderBackend ().api != SK_RenderAPI::Reserved ) ||
    SK_TLS_Bottom ()->render->d3d9->ctx_init_thread_ex;

  SK_LOGi0 ( L"[!] %s (%08" _L(PRIxPTR) L"h, %lu, %lu,"
                    L" %08" _L(PRIxPTR) L"h, 0x%04X,"
                    L" %08" _L(PRIxPTR) L"h, %08" _L(PRIxPTR) L"h,"
                    L" %08" _L(PRIxPTR) L"h) - "
                L"%s",
                L"IDirect3D9Ex::CreateDeviceEx",
                  (uintptr_t)This, Adapter, (DWORD)DeviceType,
                    hFocusWindow, BehaviorFlags, (uintptr_t)pPresentationParameters,
                      (uintptr_t)pFullscreenDisplayMode, (uintptr_t)ppReturnedDeviceInterface,
                        SK_SummarizeCaller ().c_str () );

  if (skip_device)
  {
    return
      D3D9Ex_CreateDeviceEx_Original ( This,
                                         Adapter,
                                           DeviceType,
                                             hFocusWindow,
                                               BehaviorFlags,
                                                 pPresentationParameters,
                                                   pFullscreenDisplayMode,
                                                     ppReturnedDeviceInterface );
  }

  HRESULT ret = E_FAIL;


  // In case we need to override this setting...
  D3DDISPLAYMODEEX* pModeEx = pFullscreenDisplayMode;

  if (! SK_D3D9_IsDummyD3D9Device (pPresentationParameters))
  {
    if (config.textures.d3d9_mod)
      BehaviorFlags |= D3DCREATE_MULTITHREADED;

    if (config.display.force_fullscreen && hFocusWindow)
      pPresentationParameters->hDeviceWindow = hFocusWindow;


    SK_D3D9_SetFPSTarget       (pPresentationParameters, pFullscreenDisplayMode);
    SK_D3D9_FixUpBehaviorFlags (BehaviorFlags);
    SK_SetPresentParamsD3D9Ex  ( nullptr,
                                   pPresentationParameters,
                                     &pModeEx );


    if ((! pPresentationParameters->Windowed) && (! hFocusWindow))
    {
      hFocusWindow = pPresentationParameters->hDeviceWindow;

      if (hFocusWindow == nullptr)
      {
        pModeEx = nullptr; pPresentationParameters->Windowed = TRUE;
      }
    }
  }


  IDirect3DDevice9Ex *pTemp = nullptr;

  D3D9_CALL ( ret, D3D9Ex_CreateDeviceEx_Original ( This,
                                                      Adapter,
                                                        DeviceType,
                                                          hFocusWindow,
                                                            BehaviorFlags,
                                                              pPresentationParameters,
                                                                pModeEx,
                                                                  &pTemp ) );


  // Ignore video swapchains
  if (pPresentationParameters->Flags & D3DPRESENTFLAG_VIDEO)
  {
    if (!StrStrIW (SK_GetHostApp (), L"vlc.exe"))
    {
      SK_LOGs0 (            L"  D3D9Ex  ",
                 L" %% Ignoring D3D9Ex device created using a video-only "
                 L"SwapChain (%08" _L(PRIxPTR) L"h)", (uintptr_t)This
               );
      *ppReturnedDeviceInterface = pTemp;
      return ret;
    }
  }


  if (SUCCEEDED (ret))
  {
    SK_D3D9Ex_WrapDevice (pTemp, pPresentationParameters,(IDirect3DDevice9Ex **)ppReturnedDeviceInterface);
  }

  else
    return ret;


  if (! SK_D3D9_IsDummyD3D9Device (pPresentationParameters))
  {
    SK_SetPresentParamsD3D9Ex      ( *ppReturnedDeviceInterface,
                                       pPresentationParameters,
                                         &pModeEx );
    SK_D3D9_HookDeviceAndSwapchain ( *ppReturnedDeviceInterface );

    SK_ComQIPtr <IWrapDirect3DDevice9> pWrappedDevice (*ppReturnedDeviceInterface);
  //(*ppReturnedDeviceInterface)->QueryInterface (IID_IWrapDirect3DDevice9, &pWrappedDevice);

    if (pWrappedDevice != nullptr)
    {
      SK_LOGi0 (L"Using (D3D9Ex) wrapper for SK_ImGUI_ResetD3D9!");
      SK_ImGUI_ResetD3D9 (*ppReturnedDeviceInterface/*pWrappedDevice*/);
    }

    else
      SK_ImGUI_ResetD3D9 (*ppReturnedDeviceInterface);

    D3D9ResetEx (*ppReturnedDeviceInterface, pPresentationParameters, nullptr);
  }


  return ret;
}

#include <SpecialK/render/d3d11/d3d11_core.h>
#include <shaders/vs_gl_dx_interop.h>
#include <shaders/ps_gl_dx_interop.h>

static auto constexpr _DXBackBuffers = 3;

struct SK_IndirectX_InteropCtx_D3D9;
struct SK_IndirectX_PresentManager {
  HANDLE          hThread         = INVALID_HANDLE_VALUE;
  HANDLE          hNotifyPresent  = INVALID_HANDLE_VALUE;
  HANDLE          hAckPresent     = INVALID_HANDLE_VALUE;
  HANDLE          hNotifyReset    = INVALID_HANDLE_VALUE;
  HANDLE          hAckReset       = INVALID_HANDLE_VALUE;
  volatile LONG64 frames          = 0;
  volatile LONG64 frames_complete = 0;

  int             interval        = 0;

  void Start   (SK_IndirectX_InteropCtx_D3D9 *pCtx);
  void Present (SK_IndirectX_InteropCtx_D3D9 *pCtx, int interval);
  void Reset   (SK_IndirectX_InteropCtx_D3D9 *pCtx);
};

struct SK_IndirectX_InteropCtx_D3D9
{
  ////struct gl_s {
  ////  GLuint                                 texture    = 0;
  ////  GLuint                                 color_rbo  = 0;
  ////  GLuint                                 fbo        = 0;
  ////  HDC                                    hdc        = nullptr;
  ////  HGLRC                                  hglrc      = nullptr;
  ////
  ////  bool                                   fullscreen = false;
  ////  UINT                                   width      = 0;
  ////  UINT                                   height     = 0;
  ////} gl;

  struct d3d11_s {
    SK_ComPtr <IDXGIFactory2>              pFactory;
    SK_ComPtr <ID3D11Device>               pDevice;
    SK_ComPtr <ID3D11DeviceContext>        pDevCtx;
    HANDLE                                 hInteropDevice = nullptr;

    struct staging_s {
      SK_ComPtr <ID3D11SamplerState>       colorSampler;
      SK_ComPtr <ID3D11Texture2D>          colorBuffer;
      SK_ComPtr <ID3D11ShaderResourceView> colorView;
      HANDLE                               hColorBuffer = nullptr;
    } staging; // Copy the finished image from another API here

    struct flipper_s {
      SK_ComPtr <ID3D11InputLayout>        pVertexLayout;
      SK_ComPtr <ID3D11VertexShader>       pVertexShader;
      SK_ComPtr <ID3D11PixelShader>        pPixelShader;
    } flipper; // GL's pixel origin is upside down, we must flip it

    SK_ComPtr <ID3D11BlendState>           pBlendState;
  } d3d11;

  struct output_s {
    SK_ComPtr <IDXGISwapChain1>            pSwapChain;
    HANDLE                                 hSemaphore = nullptr;

    struct backbuffer_s {
      SK_ComPtr <ID3D11Texture2D>          image;
      SK_ComPtr <ID3D11RenderTargetView>   rtv;
    } backbuffer;

    struct caps_s {
      BOOL                                 tearing      = FALSE;
      BOOL                                 flip_discard = FALSE;
    } caps;

    UINT                                   swapchain_flags = 0x0;
    DXGI_SWAP_EFFECT                       swap_effect     = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;

    HWND                                   hWnd     = nullptr;
    HMONITOR                               hMonitor = nullptr;
    RECT                                   lastKnownRect = { };
    D3D11_VIEWPORT                         viewport      = { };
  } output;

  SK_IndirectX_PresentManager              present_man;

  bool     stale = false;
};


void
SK_IndirectX_PresentManager::Start (SK_IndirectX_InteropCtx_D3D9 *pCtx)
{
  static volatile LONG lLock = 0;

  if (! InterlockedCompareExchange (&lLock, 1, 0))
  {
    if (hNotifyPresent == INVALID_HANDLE_VALUE) hNotifyPresent = SK_CreateEvent (nullptr, FALSE, FALSE, nullptr);
    if (hAckPresent    == INVALID_HANDLE_VALUE) hAckPresent    = SK_CreateEvent (nullptr, FALSE, FALSE, nullptr);
    if (hNotifyReset   == INVALID_HANDLE_VALUE) hNotifyReset   = SK_CreateEvent (nullptr, FALSE, FALSE, nullptr);
    if (hAckReset      == INVALID_HANDLE_VALUE) hAckReset      = SK_CreateEvent (nullptr, FALSE, FALSE, nullptr);
    if (hThread        == INVALID_HANDLE_VALUE)
        hThread =
      SK_Thread_CreateEx ([](LPVOID pUser) ->
      DWORD
      {
        SK_Thread_SetCurrentPriority (THREAD_PRIORITY_TIME_CRITICAL);

        SK_IndirectX_InteropCtx_D3D9* pCtx =
          (SK_IndirectX_InteropCtx_D3D9 *)pUser;

        HANDLE events [3] =
        {
                    __SK_DLL_TeardownEvent,
          pCtx->present_man.hNotifyPresent,
          pCtx->present_man.hNotifyReset
        };

        enum PresentEvents {
          Shutdown = WAIT_OBJECT_0,
          Present  = WAIT_OBJECT_0 + 1,
          Reset    = WAIT_OBJECT_0 + 2
        };

        do
        {
          DWORD dwWaitState =
            WaitForMultipleObjects ( _countof (events),
                                               events, FALSE, INFINITE );

          if (dwWaitState == PresentEvents::Reset)
          {
            if (pCtx->output.hSemaphore != nullptr)
            {
              WaitForSingleObject (pCtx->output.hSemaphore, 500UL);
            }

            auto& pDevCtx = pCtx->d3d11.pDevCtx.p;

            pDevCtx->RSSetViewports       (   1, &pCtx->output.viewport          );
            pDevCtx->PSSetShaderResources (0, 1, &pCtx->d3d11.staging.colorView.p);

            pDevCtx->IASetPrimitiveTopology (D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
            pDevCtx->IASetVertexBuffers     (0, 1, std::array <ID3D11Buffer *, 1> { nullptr }.data (),
                                                   std::array <UINT,           1> { 0       }.data (),
                                                   std::array <UINT,           1> { 0       }.data ());
            pDevCtx->IASetInputLayout       (pCtx->d3d11.flipper.pVertexLayout);
            pDevCtx->IASetIndexBuffer       (nullptr, DXGI_FORMAT_UNKNOWN, 0);

            static constexpr FLOAT fBlendFactor [4] = { 0.0f, 0.0f, 0.0f, 0.0f };

            pDevCtx->OMSetBlendState        (pCtx->d3d11.pBlendState,
                                                                 fBlendFactor, 0xFFFFFFFF  );
            pDevCtx->VSSetShader            (pCtx->d3d11.flipper.pVertexShader, nullptr, 0);
            pDevCtx->PSSetShader            (pCtx->d3d11.flipper.pPixelShader,  nullptr, 0);
            pDevCtx->PSSetSamplers   (0, 1, &pCtx->d3d11.staging.colorSampler.p);

            pDevCtx->RSSetState               (nullptr   );
            pDevCtx->RSSetScissorRects        (0, nullptr);
            pDevCtx->OMSetDepthStencilState   (nullptr, 0);

            if (pCtx->d3d11.pDevice->GetFeatureLevel () >= D3D_FEATURE_LEVEL_11_0)
            {
              pDevCtx->HSSetShader   (nullptr, nullptr, 0);
              pDevCtx->DSSetShader   (nullptr, nullptr, 0);
            }
            pDevCtx->GSSetShader     (nullptr, nullptr, 0);
            pDevCtx->SOSetTargets    (0, nullptr, nullptr);

            SK_GetCurrentRenderBackend ().setDevice (pCtx->d3d11.pDevice);

            SetEvent (pCtx->present_man.hAckReset);

            InterlockedCompareExchange (&lLock, 0, 1);
          }


          if (dwWaitState == PresentEvents::Present)
          {
            if (pCtx->output.hSemaphore != nullptr)
            {
              WaitForSingleObject (pCtx->output.hSemaphore, 100UL);
            }


            auto& pDevCtx = pCtx->d3d11.pDevCtx.p;



            InterlockedIncrement64 (&pCtx->present_man.frames);


            pDevCtx->OMSetRenderTargets ( 1,
                      &pCtx->output.backbuffer.rtv.p, nullptr );
            pDevCtx->Draw  (              4,                0 );
            pDevCtx->OMSetRenderTargets ( 0, nullptr, nullptr );
            pDevCtx->Flush (                                  );


            auto pSwapChain =
              pCtx->output.pSwapChain.p;

            BOOL bSuccess =
              SUCCEEDED ( pSwapChain->Present ( pCtx->present_man.interval,
                  (pCtx->output.caps.tearing && pCtx->present_man.interval == 0) ? DXGI_PRESENT_ALLOW_TEARING
                                                                                 : 0x0 ) );


            InterlockedIncrement64 (&pCtx->present_man.frames_complete);


            if (bSuccess)
              SetEvent (pCtx->present_man.hAckPresent);
            else
              SetEvent (pCtx->present_man.hAckPresent);

            InterlockedCompareExchange (&lLock, 0, 1);
          }

          if (dwWaitState == PresentEvents::Shutdown)
            break;

        } while (! ReadAcquire (&__SK_DLL_Ending));

        SK_CloseHandle (
          std::exchange (pCtx->present_man.hNotifyPresent, INVALID_HANDLE_VALUE)
        );

        SK_Thread_CloseSelf ();

        return 0;
      }, L"[SK] IndirectX PresentMgr", (LPVOID)pCtx);
  }

  else
  {
    while (ReadAcquire (&lLock) != 0)
      YieldProcessor ();
  }
}

void
SK_IndirectX_PresentManager::Reset (SK_IndirectX_InteropCtx_D3D9* pCtx)
{
  if (hThread == INVALID_HANDLE_VALUE)
    Start (pCtx);

  SignalObjectAndWait (hNotifyReset, hAckReset, INFINITE, FALSE);
}

void
SK_IndirectX_PresentManager::Present (SK_IndirectX_InteropCtx_D3D9* pCtx, int Interval)
{
  if (hThread == INVALID_HANDLE_VALUE)
    Start (pCtx);

  this->interval = Interval;

  //SetEvent (hNotifyPresent);
  SignalObjectAndWait (hNotifyPresent, hAckPresent, INFINITE, FALSE);
}

static HWND hwndLast;
static concurrency::concurrent_unordered_map <IDirect3DDevice9*, SK_IndirectX_InteropCtx_D3D9>
                                                                             _interop_contexts;


HRESULT
SK_D3D9_CreateInteropSwapChain ( IDXGIFactory2         *pFactory,
                                 ID3D11Device          *pDevice, HWND               hWnd,
                                 DXGI_SWAP_CHAIN_DESC1 *desc1,   IDXGISwapChain1 **ppSwapChain )
{
  HRESULT hr =
    pFactory->CreateSwapChainForHwnd ( pDevice, hWnd,
                                       desc1,   nullptr,
                                                nullptr, ppSwapChain );

  return hr;
}

HRESULT
__declspec (noinline)
WINAPI
D3D9CreateDevice_Override ( IDirect3D9*            This,
                            UINT                   Adapter,
                            D3DDEVTYPE             DeviceType,
                            HWND                   hFocusWindow,
                            DWORD                  BehaviorFlags,
                            D3DPRESENT_PARAMETERS* pPresentationParameters,
                            IDirect3DDevice9**     ppReturnedDeviceInterface )
{
  if (config.render.d3d9.force_d3d9ex)
  {
    static IDirect3D9Ex*
        pD3D9Ex  = nullptr;
    if (pD3D9Ex == nullptr)
      Direct3DCreate9Ex (D3D_SDK_VERSION, &pD3D9Ex);

    if (pD3D9Ex != nullptr)
    {
      HRESULT
      WINAPI
      D3D9ExCreateDevice_Override ( IDirect3D9*            This,
                                    UINT                   Adapter,
                                    D3DDEVTYPE             DeviceType,
                                    HWND                   hFocusWindow,
                                    DWORD                  BehaviorFlags,
                                    D3DPRESENT_PARAMETERS* pPresentationParameters,
                                    IDirect3DDevice9**     ppReturnedDeviceInterface );

      return
        D3D9ExCreateDevice_Override ( (IDirect3D9*)pD3D9Ex, Adapter, DeviceType,
                                        hFocusWindow, BehaviorFlags, pPresentationParameters,
                                          ppReturnedDeviceInterface );
    }
  }

  // Many D3D11/12/GL games use D3D9 for HW video playback, we need to ignore these
  //
  bool skip_device =
    StrStrIW (SK_GetCallerName ().c_str (), L"action_") || // Action! Video Capture  - TODO: Use Window Class Name
     ( IsWindow (game_window.hWnd) && SK_GetCurrentRenderBackend ().api != SK_RenderAPI::D3D9   &&
                                      SK_GetCurrentRenderBackend ().api != SK_RenderAPI::D3D9Ex &&
                                      SK_GetCurrentRenderBackend ().api != SK_RenderAPI::Reserved );

  if (SK_TLS_Bottom ()->render->d3d9->ctx_init_thread)
  {   SK_TLS_Bottom ()->render->d3d9->ctx_init_thread = FALSE;
      skip_device = true;
  }

  SK_LOGi0 ( L"[!] %s (%08" _L(PRIxPTR) L"h, %lu, %lu, %08"
                            _L(PRIxPTR) L"h, 0x%04X, %08"
                            _L(PRIxPTR) L"h, %08"
                            _L(PRIxPTR) L"h) - "
               L"%s",
                 L"IDirect3D9::CreateDevice", (uintptr_t)This, Adapter, (DWORD)DeviceType,
                   hFocusWindow, BehaviorFlags, (uintptr_t)pPresentationParameters,
                     (uintptr_t)ppReturnedDeviceInterface,
                       SK_SummarizeCaller ().c_str () );

  if (skip_device)
  {
    return
      D3D9_CreateDevice_Original ( This, Adapter,
                                     DeviceType,
                                       hFocusWindow,
                                         BehaviorFlags,
                                           pPresentationParameters,
                                             ppReturnedDeviceInterface );
  }


  if ( pPresentationParameters != nullptr &&
      (! SK_D3D9_IsDummyD3D9Device (pPresentationParameters) ))
  {
    if (config.textures.d3d9_mod)
      BehaviorFlags |= D3DCREATE_MULTITHREADED;

    if (config.display.force_fullscreen   &&   hFocusWindow)
      pPresentationParameters->hDeviceWindow = hFocusWindow;


    SK_D3D9_SetFPSTarget       (pPresentationParameters);
    SK_D3D9_FixUpBehaviorFlags (BehaviorFlags);

    SK_SetPresentParamsD3D9    ( nullptr,
                                   pPresentationParameters );


    if (config.display.force_fullscreen && (! hFocusWindow))
      hFocusWindow = pPresentationParameters->hDeviceWindow;
    //if (config.display.force_windowed)
    //  hFocusWindow = pPresentationParameters->hDeviceWindow;
  }


  IDirect3DDevice9* pTemp = nullptr;

  HRESULT     ret = E_FAIL;
  D3D9_CALL ( ret, D3D9_CreateDevice_Original ( This, Adapter,
                                                  DeviceType,
                                                    hFocusWindow,
                                                      BehaviorFlags,
                                                        pPresentationParameters,
                                                          &pTemp ) );

  *ppReturnedDeviceInterface =
     SUCCEEDED (ret) ?
       SK_D3D9_WrapDevice (pTemp, pPresentationParameters,(IDirect3DDevice9 **)ppReturnedDeviceInterface) :
                           pTemp;


  // Ignore video swapchains
  if ( pPresentationParameters != nullptr &&
       pPresentationParameters->Flags     & D3DPRESENTFLAG_VIDEO )
  {
    if (! StrStrIW (SK_GetHostApp (), L"vlc.exe"))
    {
      SK_LOGi0 ( L" %% Ignoring D3D9 device created using a video-only "
                 L"SwapChain (%08" _L(PRIxPTR) L")", (uintptr_t)This );

      return ret;
    }
  }

  // Do not attempt to do vftable override stuff if this failed,
  //   that will cause an immediate crash! Instead log some information that
  //     might help diagnose the problem.
  if (! SUCCEEDED (ret))
  {
    if (pPresentationParameters != nullptr)
    {
      SK_LOGi0 ( L" SwapChain Settings:   Res=(%ux%u), Format=%hs, "
                           L"Count=%lu - "
                           L"SwapEffect: %hs, Flags: 0x%04X, "
                           L"AutoDepthStencil: %hs "
                           L"PresentationInterval: %u\n",
                         pPresentationParameters->BackBufferWidth,
                         pPresentationParameters->BackBufferHeight,
    SK_D3D9_FormatToStr (pPresentationParameters->BackBufferFormat).c_str (),
                         pPresentationParameters->BackBufferCount,
SK_D3D9_SwapEffectToStr (pPresentationParameters->SwapEffect).c_str (),
                         pPresentationParameters->Flags,
                         pPresentationParameters->EnableAutoDepthStencil ?
    SK_D3D9_FormatToStr (pPresentationParameters->AutoDepthStencilFormat).c_str () :
                         "N/A",
                         pPresentationParameters->PresentationInterval );

      if (! pPresentationParameters->Windowed)
      {
        SK_LOGi0 ( L" Fullscreen Settings:  Refresh Rate: %u\n",
                   pPresentationParameters->FullScreen_RefreshRateInHz );
        SK_LOGi0 ( L" Multisample Settings: Type: %X, Quality: %u\n",
                   pPresentationParameters->MultiSampleType,
                   pPresentationParameters->MultiSampleQuality );

        //
        // Fullscreen Mode Failed:
        //
        //   Try creating a Fullscreen Window instead of letting the game crash
        //
        HMONITOR hMonitorPrimary =
          MonitorFromPoint (POINT { 0, 0 }, MONITOR_DEFAULTTOPRIMARY);

        MONITORINFO                        mon_info = { };
                                           mon_info.cbSize = sizeof (MONITORINFO);
        GetMonitorInfoW (hMonitorPrimary, &mon_info);

        pPresentationParameters->BackBufferWidth  = mon_info.rcMonitor.right  - mon_info.rcMonitor.left;
        pPresentationParameters->BackBufferHeight = mon_info.rcMonitor.bottom - mon_info.rcMonitor.top;
        pPresentationParameters->Windowed         = TRUE;

        if (pPresentationParameters->hDeviceWindow != nullptr && hFocusWindow == nullptr)
            hFocusWindow = pPresentationParameters->hDeviceWindow;

        if (pPresentationParameters->hDeviceWindow == nullptr && hFocusWindow != nullptr)
            pPresentationParameters->hDeviceWindow = hFocusWindow;

        config.window.res.override.x     = pPresentationParameters->BackBufferWidth;
        config.window.res.override.y     = pPresentationParameters->BackBufferHeight;
        config.window.center             = true;
        config.window.borderless         = true;
        config.window.fullscreen         = true;
        config.display.force_windowed    = true;

        if ((uintptr_t)hFocusWindow > 0)
        {
          SetWindowPos (
            hFocusWindow, 0,
              0, 0,
                pPresentationParameters->BackBufferWidth,
                pPresentationParameters->BackBufferHeight, SWP_SHOWWINDOW
          );
        }

        SK_SetPresentParamsD3D9 ( nullptr,
                                    pPresentationParameters );

        SK_GetCurrentRenderBackend ().swapchain_consistent = false;

                    ret = E_FAIL;
        D3D9_CALL ( ret, D3D9_CreateDevice_Original ( This, Adapter,
                                                        DeviceType,
                                                          hFocusWindow,
                                                            BehaviorFlags,
                                                              pPresentationParameters,
                                                                &pTemp ) );

        *ppReturnedDeviceInterface =
           SUCCEEDED (ret) ?
             SK_D3D9_WrapDevice (pTemp, pPresentationParameters,(IDirect3DDevice9 **)ppReturnedDeviceInterface) :
                                 pTemp;

        if (SUCCEEDED (ret))
        {
          SK_ImGui_Warning (
            L"D3D9 Game Failed to Engage Fullscreen Exclusive\r\n\r\n\t\t"
            L"* Falling back to windowed instead of crashing."
          );
        }
      }
    }

    return ret;
  }


  if (! SK_D3D9_IsDummyD3D9Device (pPresentationParameters))
  {
    SK_SetPresentParamsD3D9        (*ppReturnedDeviceInterface, pPresentationParameters);
    SK_D3D9_HookDeviceAndSwapchain (*ppReturnedDeviceInterface);

    SK_ComQIPtr <IWrapDirect3DDevice9>
        pWrappedDevice (*ppReturnedDeviceInterface);
    if (pWrappedDevice != nullptr)
    {
#ifdef _DEBUG
      dll_log.Log (L"Using wrapper for SK_ImGUI_ResetD3D9!");
#endif
      SK_ImGUI_ResetD3D9 (*ppReturnedDeviceInterface/*pWrappedDevice*/);
    }

    else
      SK_ImGUI_ResetD3D9 (*ppReturnedDeviceInterface);

/////D3D9Reset_Override (*ppReturnedDeviceInterface, pPresentationParameters);
  }

#if 0
  SK_IndirectX_InteropCtx_D3D9& d3d9_d3d11_interop = 
    _interop_contexts [*ppReturnedDeviceInterface];

  auto& pDevice    = d3d9_d3d11_interop.d3d11.pDevice;
  auto& pDevCtx    = d3d9_d3d11_interop.d3d11.pDevCtx;
  auto& pFactory   = d3d9_d3d11_interop.d3d11.pFactory;
  auto& pSwapChain = d3d9_d3d11_interop.output.pSwapChain;

  if (true)
  {
    if (config.apis.dxgi.d3d11.hook)
    {
      extern void
      WINAPI SK_HookDXGI (void);
             SK_HookDXGI (    ); // Setup the function to create a D3D11 Device

      SK_ComPtr <IDXGIAdapter>  pAdapter [16] = { nullptr };
      SK_ComPtr <IDXGIFactory7> pFactory7     =   nullptr;

      if (D3D11CreateDeviceAndSwapChain_Import != nullptr)
      {
        if ( SUCCEEDED (
               CreateDXGIFactory2 ( 0x2,
                                    __uuidof (IDXGIFactory7),
                                        (void **)&pFactory7.p ) )
           )
        {
          pFactory7->QueryInterface <IDXGIFactory2> (&pFactory.p);

          UINT uiAdapterIdx = 0;

          SK_LOG0 ((" DXGI Adapters from Highest to Lowest Perf. "), L"RedirectX9");
          SK_LOG0 ((" ------------------------------------------ "), L"RedirectX9");

          while ( SUCCEEDED (
                    pFactory7->EnumAdapterByGpuPreference ( uiAdapterIdx,
                     DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,
                                  __uuidof (IDXGIAdapter),
                                      (void **)&pAdapter [uiAdapterIdx].p
                )           )                             )
          {
            DXGI_ADAPTER_DESC         adapter_desc = { };
            pAdapter [uiAdapterIdx]->
                            GetDesc (&adapter_desc);

            SK_LOG0 ( ( L" %d) '%ws'",
                        uiAdapterIdx, adapter_desc.Description ),
                        L"RedirectX9" );

            ++uiAdapterIdx;
          }
        }

        D3D_FEATURE_LEVEL        levels [] = { D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0,
                                               D3D_FEATURE_LEVEL_10_1, D3D_FEATURE_LEVEL_10_0 };
        D3D_FEATURE_LEVEL featureLevel;

        HRESULT hr =
          DXGI_ERROR_UNSUPPORTED;

        hr =
          D3D11CreateDevice_Import (
            nullptr,//pAdapter [0].p,
              D3D_DRIVER_TYPE_HARDWARE,
                nullptr,
                  0x0,
                    levels,
        _ARRAYSIZE (levels),
                        D3D11_SDK_VERSION,
                          &pDevice.p,
                            &featureLevel,
                              &pDevCtx.p );

        if (FAILED (hr))
        {
          pDevCtx = nullptr;
          pDevice = nullptr;
        }

        if ( SK_ComPtr <IDXGIDevice>
                     pDevDXGI = nullptr ;

            SUCCEEDED (hr)                                                     &&
                      pDevice != nullptr                                       &&
                ( (pFactory.p != nullptr && pAdapter [0].p != nullptr)         ||
           SUCCEEDED (pDevice->QueryInterface <IDXGIDevice> (&pDevDXGI))       &&
           SUCCEEDED (pDevDXGI->GetAdapter                  (&pAdapter [0].p)) &&
           SUCCEEDED (pAdapter [0]->GetParent (IID_PPV_ARGS (&pFactory)))
                )
           )
        {
          //SK_GL_OnD3D11 = true;

          SK_ComQIPtr <IDXGIFactory5>
                           pFactory5 (pFactory);

          if (pFactory5 != nullptr)
          {
            SK_ReleaseAssert (
              SUCCEEDED (
                pFactory5->CheckFeatureSupport (
                  DXGI_FEATURE_PRESENT_ALLOW_TEARING,
                             &d3d9_d3d11_interop.output.caps.tearing,
                      sizeof (d3d9_d3d11_interop.output.caps.tearing)
                                               )
                        )
            );

            d3d9_d3d11_interop.output.caps.flip_discard = true;
          }

          if (d3d9_d3d11_interop.output.caps.flip_discard)
              d3d9_d3d11_interop.output.swap_effect = DXGI_SWAP_EFFECT_FLIP_DISCARD;

          if (d3d9_d3d11_interop.output.caps.tearing)
          {
            d3d9_d3d11_interop.output.swapchain_flags =
              ( DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING |
                DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT );
          }

          //dx_gl_interop.d3d11.hInteropDevice =
          //  wglDXOpenDeviceNV (dx_gl_interop.d3d11.pDevice);
        }

        D3D11_BLEND_DESC
          blend_desc                                        = { };
          blend_desc.AlphaToCoverageEnable                  = FALSE;
          blend_desc.RenderTarget [0].BlendEnable           = FALSE;
          blend_desc.RenderTarget [0].SrcBlend              = D3D11_BLEND_ONE;
          blend_desc.RenderTarget [0].DestBlend             = D3D11_BLEND_ZERO;
          blend_desc.RenderTarget [0].BlendOp               = D3D11_BLEND_OP_ADD;
          blend_desc.RenderTarget [0].SrcBlendAlpha         = D3D11_BLEND_ONE;
          blend_desc.RenderTarget [0].DestBlendAlpha        = D3D11_BLEND_ZERO;
          blend_desc.RenderTarget [0].BlendOpAlpha          = D3D11_BLEND_OP_ADD;
          blend_desc.RenderTarget [0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

        pDevice->CreateBlendState ( &blend_desc,
                                      &d3d9_d3d11_interop.d3d11.pBlendState.p );

        SK_ReleaseAssert (
          SUCCEEDED (
            pDevice->CreatePixelShader ( gl_dx_interop_ps_bytecode,
                                 sizeof (gl_dx_interop_ps_bytecode),
                   nullptr, &d3d9_d3d11_interop.d3d11.flipper.pPixelShader.p )
                    )
          );

        SK_ReleaseAssert (
          SUCCEEDED (
            pDevice->CreateVertexShader ( gl_dx_interop_vs_bytecode,
                                  sizeof (gl_dx_interop_vs_bytecode),
                  nullptr, &d3d9_d3d11_interop.d3d11.flipper.pVertexShader.p )
                    )
          );

        D3D11_SAMPLER_DESC
          sampler_desc                    = { };

          sampler_desc.Filter             = D3D11_FILTER_MIN_MAG_MIP_POINT;
          sampler_desc.AddressU           = D3D11_TEXTURE_ADDRESS_WRAP;
          sampler_desc.AddressV           = D3D11_TEXTURE_ADDRESS_WRAP;
          sampler_desc.AddressW           = D3D11_TEXTURE_ADDRESS_WRAP;
          sampler_desc.MipLODBias         = 0.f;
          sampler_desc.MaxAnisotropy      =   1;
          sampler_desc.ComparisonFunc     =  D3D11_COMPARISON_NEVER;
          sampler_desc.MinLOD             = -D3D11_FLOAT32_MAX;
          sampler_desc.MaxLOD             =  D3D11_FLOAT32_MAX;

        pDevice->CreateSamplerState ( &sampler_desc,
                                        &d3d9_d3d11_interop.d3d11.staging.colorSampler.p );

        D3D11_INPUT_ELEMENT_DESC local_layout [] = {
          { "", 0, DXGI_FORMAT_R32_UINT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 }
        };

        pDevice->CreateInputLayout ( local_layout, 1,
                                     (void *)(gl_dx_interop_vs_bytecode),
                                      sizeof (gl_dx_interop_vs_bytecode) /
                                      sizeof (gl_dx_interop_vs_bytecode [0]),
                                        &d3d9_d3d11_interop.d3d11.flipper.pVertexLayout.p );

        ////dx_gl_interop.gl.hdc          = hDC;
        ////dx_gl_interop.gl.hglrc        = thread_hglrc;
        ////dx_gl_interop.output.hWnd     = hWnd;
        ////dx_gl_interop.output.hMonitor = MonitorFromWindow (hWnd, MONITOR_DEFAULTTONEAREST);

        d3d9_d3d11_interop.stale = true;
      }

      RECT                          rcWnd = { };
      GetClientRect (hFocusWindow, &rcWnd);

      HMONITOR hMonitor =
        MonitorFromWindow (hFocusWindow, MONITOR_DEFAULTTONEAREST);

      d3d9_d3d11_interop.output.lastKnownRect = rcWnd;

      auto w = d3d9_d3d11_interop.output.lastKnownRect.right  - d3d9_d3d11_interop.output.lastKnownRect.left;
      auto h = d3d9_d3d11_interop.output.lastKnownRect.bottom - d3d9_d3d11_interop.output.lastKnownRect.top;

      d3d9_d3d11_interop.output.hWnd     = hFocusWindow;
      d3d9_d3d11_interop.output.hMonitor = hMonitor;
    //d3d9_d3d11_interop.gl.hdc          = hDC;
    //d3d9_d3d11_interop.gl.hglrc        = thread_hglrc;

      DXGI_FORMAT target_fmt =
        config.render.output.force_10bpc ?
        DXGI_FORMAT_R10G10B10A2_UNORM :
           DXGI_FORMAT_R8G8B8A8_UNORM;

      DXGI_SWAP_CHAIN_DESC1
        desc1                    = { };
        desc1.Format             = target_fmt;
        desc1.SampleDesc.Count   = 1;
        desc1.SampleDesc.Quality = 0;
        desc1.Width              = w;
        desc1.Height             = h;
        desc1.BufferUsage        = DXGI_USAGE_RENDER_TARGET_OUTPUT |
                                   DXGI_USAGE_BACK_BUFFER;
        desc1.BufferCount        = _DXBackBuffers;
        desc1.AlphaMode          = DXGI_ALPHA_MODE_IGNORE;
        desc1.Scaling            = DXGI_SCALING_NONE;
        desc1.SwapEffect         = d3d9_d3d11_interop.output.swap_effect;
        desc1.Flags              = d3d9_d3d11_interop.output.swapchain_flags;

      if (                             d3d9_d3d11_interop.output.hSemaphore != nullptr)
        SK_CloseHandle (std::exchange (d3d9_d3d11_interop.output.hSemaphore,   nullptr));

      if (std::exchange (d3d9_d3d11_interop.output.hWnd, hFocusWindow) != hFocusWindow || pSwapChain == nullptr)
      {
        pSwapChain = nullptr;

        SK_D3D9_CreateInteropSwapChain ( pFactory, d3d9_d3d11_interop.d3d11.pDevice,
                                           d3d9_d3d11_interop.output.hWnd,
                                             &desc1, &pSwapChain.p );

        pFactory->MakeWindowAssociation ( d3d9_d3d11_interop.output.hWnd,
                                            DXGI_MWA_NO_ALT_ENTER |
                                            DXGI_MWA_NO_WINDOW_CHANGES );
      }

      // Dimensions were wrong, SwapChain just needs a resize...
      else
      {
        pSwapChain->ResizeBuffers (
                   _DXBackBuffers, w, h,
                    DXGI_FORMAT_UNKNOWN, d3d9_d3d11_interop.output.swapchain_flags
        );
      }
    }
  }
#endif

  return ret;
}



// This is only called when forcing D3D9Ex on, which is generally only desirable for
//   G-Sync in windowed mode.
HRESULT
__declspec (noinline)
WINAPI
D3D9ExCreateDevice_Override ( IDirect3D9*            This,
                              UINT                   Adapter,
                              D3DDEVTYPE             DeviceType,
                              HWND                   hFocusWindow,
                              DWORD                  BehaviorFlags,
                              D3DPRESENT_PARAMETERS* pPresentationParameters,
                              IDirect3DDevice9**     ppReturnedDeviceInterface )
{
  static concurrency::concurrent_unordered_map <HWND, IDirect3DDevice9Ex*> ex_devices_;



  // Many D3D11/12/GL games use D3D9 for HW video playback, we need to ignore these
  //
  bool skip_device =
    StrStrIW (SK_GetCallerName ().c_str (), L"action_") || // Action! Video Capture  - TODO: Use Window Class Name
     ( IsWindow (game_window.hWnd) && SK_GetCurrentRenderBackend ().api != SK_RenderAPI::D3D9   &&
                                      SK_GetCurrentRenderBackend ().api != SK_RenderAPI::D3D9Ex &&
                                      SK_GetCurrentRenderBackend ().api != SK_RenderAPI::Reserved ) ||
    SK_TLS_Bottom ()->render->d3d9->ctx_init_thread_ex;

  SK_LOGi0 ( L"[!] %s (%08" _L(PRIxPTR) L"h, %lu, %lu,"
                    L" %08" _L(PRIxPTR) L"h, 0x%04X,"
                    L" %08" _L(PRIxPTR) L"h,"
                    L" %08" _L(PRIxPTR) L"h) - "
                L"%s",
                L"IDirect3D9Ex::CreateDevice",
                  (uintptr_t)This, Adapter, (DWORD)DeviceType,
                    hFocusWindow, BehaviorFlags, (uintptr_t)pPresentationParameters,
                      (uintptr_t)ppReturnedDeviceInterface,
                        SK_SummarizeCaller ().c_str () );

  if (skip_device)
  {
    return
      D3D9Ex_CreateDeviceEx_Original ( (IDirect3D9Ex *)This,
                                         Adapter,
                                           DeviceType,
                                             nullptr,
                                               BehaviorFlags,
                                                 pPresentationParameters,
                                                   nullptr,
                                                     (IDirect3DDevice9Ex **)
                                                     ppReturnedDeviceInterface );
  }

  HRESULT ret = E_FAIL;


  if (! SK_D3D9_IsDummyD3D9Device (pPresentationParameters))
  {
    if (config.textures.d3d9_mod)
      BehaviorFlags |= D3DCREATE_MULTITHREADED;

    if (config.display.force_fullscreen && hFocusWindow)
      pPresentationParameters->hDeviceWindow = hFocusWindow;

    auto desc_dcparams =
      SK_FormatStringW (
        L"Resolution:   (%dx%d @%d Hz)\n"
        L"Backbuffers:  %d\n"
        L"Format:       %hs\n"
        L"Multisample:  (%d, %d)\n"
        L"SwapEffect:   %hs\n"
        L"SwapInterval: %x\n"
        L"HWND:         %x\n"
        L"Windowed:     %s\n"
        L"DepthStencil: %hs (%s)\n"
        L"Flags:        %hs",               pPresentationParameters->BackBufferWidth,
                                            pPresentationParameters->BackBufferHeight,
                                            pPresentationParameters->FullScreen_RefreshRateInHz,
                                            pPresentationParameters->BackBufferCount,
        SK_D3D9_FormatToStr                (pPresentationParameters->BackBufferFormat).c_str (),
                                            pPresentationParameters->MultiSampleType,
                                            pPresentationParameters->MultiSampleQuality,
        SK_D3D9_SwapEffectToStr            (pPresentationParameters->SwapEffect).c_str (),
                                            pPresentationParameters->PresentationInterval,
                                            pPresentationParameters->hDeviceWindow,
                                            pPresentationParameters->Windowed ?
                                                                       L"Yes" : L"No",
        SK_D3D9_FormatToStr                (pPresentationParameters->AutoDepthStencilFormat).c_str (),
                                            pPresentationParameters->EnableAutoDepthStencil ?
                                                                                     L"Yes" : L"No",
        SK_D3D9_PresentParameterFlagsToStr (pPresentationParameters->Flags).c_str ()
      );

    SK_LOGi0 (
      L" >> Device Creation Parameters (Before Overrides):\n%ws",
        desc_dcparams.c_str ()
    );

    SK_D3D9_SetFPSTarget       (pPresentationParameters, nullptr);
    SK_D3D9_FixUpBehaviorFlags (BehaviorFlags);
    SK_SetPresentParamsD3D9    ( nullptr,
                                   pPresentationParameters );

    if (pPresentationParameters->Windowed)
        pPresentationParameters->BackBufferFormat = D3DFMT_UNKNOWN;

    if ((! pPresentationParameters->Windowed) && (! hFocusWindow))
    {
      hFocusWindow = pPresentationParameters->hDeviceWindow;
    
      if (hFocusWindow == nullptr)
      {
        pPresentationParameters->Windowed = TRUE;
      }
    }

    desc_dcparams =
      SK_FormatStringW (
        L"Resolution:   (%dx%d @%d Hz)\n"
        L"Backbuffers:  %d\n"
        L"Format:       %hs\n"
        L"Multisample:  (%d, %d)\n"
        L"SwapEffect:   %hs\n"
        L"SwapInterval: %x\n"
        L"HWND:         %x\n"
        L"Windowed:     %s\n"
        L"DepthStencil: %hs (%s)\n"
        L"Flags:        %hs",               pPresentationParameters->BackBufferWidth,
                                            pPresentationParameters->BackBufferHeight,
                                            pPresentationParameters->FullScreen_RefreshRateInHz,
                                            pPresentationParameters->BackBufferCount,
        SK_D3D9_FormatToStr                (pPresentationParameters->BackBufferFormat).c_str (),
                                            pPresentationParameters->MultiSampleType,
                                            pPresentationParameters->MultiSampleQuality,
        SK_D3D9_SwapEffectToStr            (pPresentationParameters->SwapEffect).c_str (),
                                            pPresentationParameters->PresentationInterval,
                                            pPresentationParameters->hDeviceWindow,
                                            pPresentationParameters->Windowed ?
                                                                       L"Yes" : L"No",
        SK_D3D9_FormatToStr                (pPresentationParameters->AutoDepthStencilFormat).c_str (),
                                            pPresentationParameters->EnableAutoDepthStencil ?
                                                                                     L"Yes" : L"No",
        SK_D3D9_PresentParameterFlagsToStr (pPresentationParameters->Flags).c_str ()
      );

    SK_LOGi0 (
      L" >> Device Creation Parameters (After Overrides):\n%ws",
        desc_dcparams.c_str ()
    );
  }


  IDirect3DDevice9Ex *pTemp = nullptr;

  if (ex_devices_ [hFocusWindow] != nullptr)
  {
    SK_LOGi0 (
      L" >> (FORCE D3D9Ex): Returning cached D3D9Ex device for HWND '%x'", hFocusWindow
    );

    *ppReturnedDeviceInterface =
      ex_devices_ [hFocusWindow];

    (*ppReturnedDeviceInterface)->AddRef ();

    ex_devices_ [hFocusWindow]->ResetEx (pPresentationParameters, nullptr);

    return S_OK;
  }


  D3DDISPLAYMODEEX dispModeEx = { };
  auto           *pDispModeEx = &dispModeEx;
  
  if (pPresentationParameters->Windowed)
                  pDispModeEx = nullptr;
  else
  {
    pDispModeEx->Format           = pPresentationParameters->BackBufferFormat;
    pDispModeEx->Width            = pPresentationParameters->BackBufferWidth;
    pDispModeEx->Height           = pPresentationParameters->BackBufferHeight;
    pDispModeEx->RefreshRate      = pPresentationParameters->FullScreen_RefreshRateInHz;
    pDispModeEx->ScanLineOrdering = D3DSCANLINEORDERING_PROGRESSIVE;
  }

  SK_LOGi0 (L"BehaviorFlags=%x", BehaviorFlags);

  static SK_ComPtr <IDirect3D9Ex>        pD3D9Ex;
  if (                                   pD3D9Ex == nullptr)
    Direct3DCreate9Ex (D3D_SDK_VERSION, &pD3D9Ex.p);

  if (pD3D9Ex == nullptr)
  {
    MessageBoxW (
      nullptr,
        L"Special K's D3D9Ex Translation Layer Is Unsupported By This Game",
        L"Fatal D3D9-IK Error", MB_ICONERROR | MB_APPLMODAL
    );

    return E_FAIL;
  }

  D3D9_CALL ( ret, D3D9Ex_CreateDeviceEx_Original ( pD3D9Ex,
                                                      Adapter,
                                                        DeviceType,
                                                          hFocusWindow,
                                                            BehaviorFlags,
                                                              pPresentationParameters,
                                                                pDispModeEx,
                                                                  &pTemp ) );


  // Ignore video swapchains
  if (pPresentationParameters->Flags & D3DPRESENTFLAG_VIDEO)
  {
    SK_LOGs0 (            L"  D3D9Ex  ",
               L" %% Ignoring D3D9Ex device created using a video-only "
               L"SwapChain (%08" _L(PRIxPTR) L"h)", (uintptr_t)This
             );
    *ppReturnedDeviceInterface = pTemp;
    return ret;
  }


  if (SUCCEEDED (ret))
  {
    SK_D3D9Ex_WrapDevice (pTemp, pPresentationParameters,(IDirect3DDevice9Ex **)ppReturnedDeviceInterface);
  }

  else
  {
    return ret;
  }


  if (! SK_D3D9_IsDummyD3D9Device (pPresentationParameters))
  {
    SK_SetPresentParamsD3D9       ( *ppReturnedDeviceInterface,
                                       pPresentationParameters );
    SK_D3D9_HookDeviceAndSwapchain ( *ppReturnedDeviceInterface );

    SK_ComQIPtr <IWrapDirect3DDevice9> pWrappedDevice (*ppReturnedDeviceInterface);
  //(*ppReturnedDeviceInterface)->QueryInterface (IID_IWrapDirect3DDevice9, &pWrappedDevice);

    if (pWrappedDevice != nullptr)
    {
      SK_LOGi0 (L"Using (D3D9Ex) wrapper for SK_ImGUI_ResetD3D9!");
      SK_ImGUI_ResetD3D9 (*ppReturnedDeviceInterface/*pWrappedDevice*/);
    }

    else
      SK_ImGUI_ResetD3D9 (*ppReturnedDeviceInterface);

    D3D9ResetEx ((IDirect3DDevice9Ex *)*ppReturnedDeviceInterface, pPresentationParameters, nullptr);

    // XXX: Is this unique to FlipEx, or D3D9Ex in general?  (1 SwapChain per-HWND)
    if ( config.render.d3d9.force_d3d9ex &&
         config.render.d3d9.enable_flipex )
    {
      SK_ComQIPtr <IDirect3DDevice9Ex> pDev9Ex (
        *ppReturnedDeviceInterface
      );

      if (pDev9Ex)
      {   pDev9Ex.p->AddRef ();

        ex_devices_ [hFocusWindow] =
          pDev9Ex;
      }
    }
  }


  return ret;
}


bool tex_init = false;

__declspec (noinline)
IDirect3D9*
STDMETHODCALLTYPE
Direct3DCreate9 (UINT SDKVersion)
{
  if (SK_COMPAT_IgnoreDxDiagnCall ())
    return nullptr;

  WaitForInit_D3D9    ();

  if (! SK_TLS_Bottom ()->render->d3d9->ctx_init_thread)
    WaitForInit       ();


  SK_LOGi0 ( L"[!] %s (%lu) - "
                 L"%s",
                   L"Direct3DCreate9",
                     SDKVersion,
                       SK_SummarizeCaller ().c_str () );

  bool force_d3d9ex = config.render.d3d9.force_d3d9ex;

  // Disable D3D9EX whenever GeDoSaTo is present
  if (force_d3d9ex)
  {
    if (SK_GetModuleHandle (L"GeDoSaTo.dll"))
    {
      SK_LOGs0 ( L"CompatHack",
                 L" <!> Disabling D3D9Ex optimizations "
                      "because GeDoSaTo.dll is present!" );
      force_d3d9ex = false;
    }
  }


  IDirect3D9*   d3d9    = nullptr;
  IDirect3D9Ex* pD3D9Ex = nullptr;

  if ((! force_d3d9ex) || FAILED (Direct3DCreate9Ex (D3D_SDK_VERSION/*SDKVersion*/, &pD3D9Ex)))
  {
    if (Direct3DCreate9_Import != nullptr)
    {
      if (force_d3d9ex)
      {
        SK_LOGi0 ( L"[!] %s (%lu) - "
                       L"%s",
                       L"Direct3DCreate9", SDKVersion,
                         SK_SummarizeCaller ().c_str () );
      }

      d3d9 = Direct3DCreate9_Import (D3D_SDK_VERSION/*SDKVersion*/);
    }
  }

  else if (force_d3d9ex)
  {
    return pD3D9Ex;
  }

  SK_ReleaseAssert (Direct3DCreate9_Import != nullptr);

  if (d3d9 == nullptr && Direct3DCreate9_Import != nullptr)
  {
    d3d9 = Direct3DCreate9_Import (D3D_SDK_VERSION/*SDKVersion*/);
  }

  return d3d9;
}

HRESULT
__declspec (noinline)
STDMETHODCALLTYPE
Direct3DCreate9Ex (_In_ UINT SDKVersion, _Out_ IDirect3D9Ex **ppD3D)
{
  if (SK_COMPAT_IgnoreDxDiagnCall ())
    return E_NOTIMPL;

  if (Direct3DCreate9On12Ex != nullptr)
  {
    SK_LOGi0 (L" >> Using D3D9On12");

    D3D9ON12_ARGS args             = {  };
                  args.Enable9On12 = TRUE;
    
    return
      Direct3DCreate9On12Ex (D3D_SDK_VERSION, &args, 1, ppD3D);
  }


  WaitForInit_D3D9Ex  ();

  if (! SK_TLS_Bottom ()->render->d3d9->ctx_init_thread_ex)
    WaitForInit       ();


  SK_LOGi0 ( L"[!] %s (%lu, %08" _L(PRIxPTR) L"h) - "
                 L"%s",
                   L"Direct3DCreate9Ex",
                     SDKVersion,
                       (uintptr_t)ppD3D,
                         SK_SummarizeCaller ().c_str () );

  HRESULT       hr     = E_FAIL;
  IDirect3D9Ex* d3d9ex = nullptr;

  if (Direct3DCreate9Ex_Import)
  {
    D3D9_CALL (hr, Direct3DCreate9Ex_Import (D3D_SDK_VERSION/*SDKVersion*/, &d3d9ex));

    if (SUCCEEDED (hr) && d3d9ex != nullptr)
    {
      *ppD3D = d3d9ex;
    }
  }

  return hr;
}

void
__stdcall
SK_D3D9_UpdateRenderStats (IDirect3DSwapChain9* pSwapChain, IDirect3DDevice9* pDevice)
{
  if (! ((pDevice || pSwapChain) && config.render.show))
    return;

  PipelineStatsD3D9& pipeline_stats =
    pipeline_stats_d3d9;

  SK_ComPtr <IDirect3DDevice9> dev = pDevice;

  if (  pDevice    != nullptr ||
       (pSwapChain != nullptr && SUCCEEDED (pSwapChain->GetDevice (&dev.p))) )
  {
    if (pipeline_stats.query.object != nullptr)
    {
      if (pipeline_stats.query.active)
      {
        pipeline_stats.query.object->Issue (D3DISSUE_END);
        pipeline_stats.query.active = false;
      }

      else
      {
        HRESULT hr =
          pipeline_stats.query.object->GetData (
                              &pipeline_stats.last_results,
                                sizeof D3DDEVINFO_D3D9PIPELINETIMINGS,
                                  D3DGETDATA_FLUSH );
        if (hr == S_OK)
        {
          pipeline_stats.query.object->Release ();
          pipeline_stats.query.object = nullptr;
        }
      }
    }

    else
    {
      if (SUCCEEDED (dev->CreateQuery (D3DQUERYTYPE_PIPELINETIMINGS, &pipeline_stats.query.object)))
      {
        pipeline_stats.query.object->Issue (D3DISSUE_BEGIN);
        pipeline_stats.query.active = true;
      }
    }
  }
}

std::string
WINAPI
SK::D3D9::getPipelineStatsDesc (void)
{
  char szDesc [1024] = { };

  D3DDEVINFO_D3D9PIPELINETIMINGS& stats =
    pipeline_stats_d3d9->last_results;

  //
  // VERTEX SHADING
  //
  if (stats.VertexProcessingTimePercent > 0.0f)
  {
    sprintf ( szDesc,
                 "  VERTEX : %5.2f%%\n",
                   stats.VertexProcessingTimePercent );
  }

  else
  {
    sprintf ( szDesc,
                 "  VERTEX : <Unused>\n" );
  }

  //
  // PIXEL SHADING
  //
  if (stats.PixelProcessingTimePercent > 0.0f)
  {
    sprintf ( szDesc,
                 "%s  PIXEL  : %5.2f%%\n",
                   std::string (szDesc).c_str (),
                     stats.PixelProcessingTimePercent );
  }

  else
  {
    sprintf ( szDesc,
                 "%s  PIXEL  : <Unused>\n",
                   std::string (szDesc).c_str () );
  }

  //
  // OTHER
  //
  if (stats.OtherGPUProcessingTimePercent > 0.0f)
  {
    sprintf ( szDesc,
                 "%s  OTHER  : %5.2f%%\n",
                   std::string (szDesc).c_str (), stats.OtherGPUProcessingTimePercent);
  }

  //
  // IDLE
  //
  if (stats.GPUIdleTimePercent > 0.0f)
  {
    sprintf ( szDesc,
                 "%s  IDLE   : %5.2f%%\n",
                   std::string (szDesc).c_str (),
                     stats.GPUIdleTimePercent );
  }

  return szDesc;
}

unsigned int
__stdcall
HookD3D9 (LPVOID user)
{
  UNREFERENCED_PARAMETER (user);

  if (config.apis.translated == SK_RenderAPI::D3D9 || (! config.apis.d3d9.hook))
  {
    return 0;
  }

  if (__SK_bypass || ReadAcquire (&__d3d9_ready))
  {
    return 0;
  }

  if (! Direct3DCreate9_Import)
    return 0;

  static volatile LONG __hooked = FALSE;

  if (! InterlockedCompareExchangeAcquire (&__hooked, TRUE, FALSE))
  {
    SK_TLS* pTLS =
      SK_TLS_Bottom ();

    SK_AutoCOMInit auto_com;
    {
      pTLS->render->
            d3d9->ctx_init_thread   = TRUE;

      HWND                   hwnd   = nullptr;
      SK_ComPtr <IDirect3D9> pD3D9  =
         Direct3DCreate9_Import (D3D_SDK_VERSION);
      SK_ComPtr <IDirect3D9Ex>
                            pD3D9Ex = nullptr;

      if (pD3D9 != nullptr)
      {
        hwnd =
          SK_Win32_CreateDummyWindow ();

        D3DPRESENT_PARAMETERS pparams = { };

        pparams.SwapEffect            = D3DSWAPEFFECT_FLIP;
        pparams.BackBufferFormat      = D3DFMT_UNKNOWN;
        pparams.Windowed              = TRUE;
        pparams.BackBufferCount       = 2;
        pparams.hDeviceWindow         = hwnd;
        pparams.BackBufferHeight      = 2;
        pparams.BackBufferWidth       = 2;

        SK_ComPtr <IDirect3DDevice9> pD3D9Dev = nullptr;

        SK_LOGi0 (L" Hooking D3D9...");

        pTLS->render->d3d9->ctx_init_thread = TRUE;

        HRESULT hr =
          pD3D9->CreateDevice (
                  D3DADAPTER_DEFAULT,
                    D3DDEVTYPE_HAL,
                      nullptr,
                        D3DCREATE_HARDWARE_VERTEXPROCESSING,
                          &pparams,
                            &pD3D9Dev.p );

        if (SUCCEEDED (hr))
        {
          if (! LocalHook_D3D9CreateDevice.active)
          {
            if (! config.render.d3d9.force_d3d9ex)
            {
              D3D9_INTERCEPT ( &pD3D9.p, 16,
                              "IDirect3D9::CreateDevice",
                                D3D9CreateDevice_Override,
                                D3D9_CreateDevice_Original,
                                D3D9_CreateDevice_pfn );

              SK_Hook_TargetFromVFTable (
                LocalHook_D3D9CreateDevice,
                  (void **)&pD3D9.p, 16 );
            }
          }

          SK_LOGi0 (L"  * Success");
        }

        else
        {
          _com_error err (hr);
          SK_LOGi0 ( L"  * Failure  (%s)",
                         err.ErrorMessage () );

          if (! LocalHook_D3D9CreateDevice.active)
          {
            D3D9_INTERCEPT ( &pD3D9.p, 16,
                            "IDirect3D9::CreateDevice",
                              D3D9CreateDevice_Override,
                              D3D9_CreateDevice_Original,
                              D3D9_CreateDevice_pfn );

            SK_Hook_TargetFromVFTable (
              LocalHook_D3D9CreateDevice,
                (void **)&pD3D9.p, 16 );
          }
        }

        SK_Win32_CleanupDummyWindow (hwnd);

        pTLS->render->d3d9->ctx_init_thread    = FALSE;
        pTLS->render->d3d9->ctx_init_thread_ex = TRUE;


        if (config.render.d3d9.use_d3d9on12)
        {
          std::filesystem::path system_d3d9 =
            std::filesystem::path (SK_GetSystemDirectory ()) / L"d3d9.dll";

          Direct3DCreate9On12Ex = (Direct3DCreate9On12Ex_pfn)
            SK_GetProcAddress (system_d3d9.c_str (), "Direct3DCreate9On12Ex");
        }


        if (Direct3DCreate9On12Ex != nullptr)
        {
          D3D9ON12_ARGS args             = {  };
                        args.Enable9On12 = TRUE;

          hr =
            Direct3DCreate9On12Ex (D3D_SDK_VERSION, &args, 1, &pD3D9Ex.p);
        }

        else
        {
          hr = (config.apis.d3d9ex.hook &&
            Direct3DCreate9Ex_Import != nullptr) ?
            Direct3DCreate9Ex_Import (D3D_SDK_VERSION, &pD3D9Ex.p)
                             :
                        E_NOINTERFACE;
        }

        hwnd = nullptr;

        if (SUCCEEDED (hr))
        {
          SK_LOGi0 (L" Hooking D3D9Ex...");

          hwnd    =
            SK_Win32_CreateDummyWindow ();
          pparams = { };

          pparams.SwapEffect             = D3DSWAPEFFECT_FLIPEX;
          pparams.BackBufferFormat       = D3DFMT_UNKNOWN;
          pparams.Windowed               = TRUE;
          pparams.BackBufferCount        = 2;
          pparams.hDeviceWindow          = hwnd;
          pparams.BackBufferHeight       = 2;
          pparams.BackBufferWidth        = 2;

          SK_ComPtr <IDirect3DDevice9Ex>
               pD3D9DevEx                = nullptr;
          pTLS->render->
                d3d9->ctx_init_thread_ex = TRUE;

          hr = pD3D9Ex->CreateDeviceEx (
                D3DADAPTER_DEFAULT,
                  D3DDEVTYPE_HAL,
                     hwnd,
                      D3DCREATE_HARDWARE_VERTEXPROCESSING,
                        &pparams,
                          nullptr,
                            &pD3D9DevEx.p );

          if ( SUCCEEDED (hr) )
          {
            if (! LocalHook_D3D9CreateDeviceEx.active)
            {
              if (config.render.d3d9.force_d3d9ex)
              {
                D3D9_INTERCEPT ( &pD3D9Ex.p, 16,
                                 "IDirect3D9Ex::CreateDevice",
                                  D3D9ExCreateDevice_Override,
                                  D3D9Ex_CreateDevice_Original,
                                  D3D9Ex_CreateDevice_pfn );

                SK_Hook_TargetFromVFTable (
                  LocalHook_D3D9CreateDevice,
                    (void **)&pD3D9Ex.p, 16 );
              }

              D3D9_INTERCEPT ( &pD3D9Ex.p, 20,
                               "IDirect3D9Ex::CreateDeviceEx",
                                D3D9CreateDeviceEx_Override,
                                D3D9Ex_CreateDeviceEx_Original,
                                D3D9Ex_CreateDeviceEx_pfn );

              SK_Hook_TargetFromVFTable (
                LocalHook_D3D9CreateDeviceEx,
                  (void **)&pD3D9Ex.p, 20 );
            }

            SK_D3D9_HookDeviceAndSwapchain (pD3D9DevEx);

            SK_LOGi0 (L"  * Success");
          }

          else
          {
            SK_LOGi0 (L"  * Failure");

            if (! LocalHook_D3D9CreateDeviceEx.active)
            {
              D3D9_INTERCEPT ( &pD3D9Ex.p, 20,
                               "IDirect3D9Ex::CreateDeviceEx",
                                D3D9CreateDeviceEx_Override,
                                D3D9Ex_CreateDeviceEx_Original,
                                D3D9Ex_CreateDeviceEx_pfn );

              SK_Hook_TargetFromVFTable (
                LocalHook_D3D9CreateDeviceEx,
                  (void **)&pD3D9Ex.p, 20 );
            }
          }

          SK_Win32_CleanupDummyWindow (hwnd);
        }

        else
        {
          if (pD3D9Dev != nullptr)
          {
            SK_D3D9_HookDeviceAndSwapchain (pD3D9Dev);
          }
        }

        WriteRelease (&__d3d9_ready, TRUE);
      }
    }

    pTLS->render->d3d9->ctx_init_thread    = FALSE;
    pTLS->render->d3d9->ctx_init_thread_ex = FALSE;

    InterlockedIncrementRelease (&__hooked);
  }

  else
    SK_Thread_SpinUntilAtomicMin (&__hooked, 2);

  return 0;
}

unsigned int
__stdcall
HookD3D9Ex (LPVOID user)
{
  UNREFERENCED_PARAMETER (user);

  return 0;
}

void
SK_D3D9_TriggerReset (bool temporary)
{
  trigger_reset =
    reset_stage_e::Initiate;

  if (temporary)
  {
    request_mode_change =
      SK_GetCurrentRenderBackend ().fullscreen_exclusive ?
         mode_change_request_e::Windowed :
         mode_change_request_e::Fullscreen;
  }
}


//extern bool
//SK_ImGui_IsItemClicked (void);

void
SK_D3D9_DrawFileList (bool& can_scroll)
{
  const float font_size =
    ImGui::GetFont ()->FontSize * ImGui::GetIO ().FontGlobalScale;

  ImGui::PushItemWidth (500.0f);

  struct enumerated_source_s
  {
    std::string            name       = "invalid";
    std::vector <uint32_t> checksums  = { };

    struct {
      std::vector < std::pair < uint32_t, SK::D3D9::TexRecord > >
                 records              = {  };
      uint64_t   size                 = 0ULL;
    } streaming, blocking;

    uint64_t     totalSize (void) const { return streaming.size + blocking.size; };
  };

  static std::vector <enumerated_source_s> sources;
  static SK::D3D9::TexList                 injectable;
  static std::vector < std::wstring >      archives;
  static bool                              list_dirty = true;
  static int                               sel        = 0;

  auto EnumerateSource =
    [](unsigned int archive_no) ->
      enumerated_source_s
      {
        enumerated_source_s source = { };

        char szFileName [MAX_PATH + 2] = { };

        if (archive_no != std::numeric_limits <unsigned int>::max ()) {
          sprintf (szFileName, "%ws", archives [archive_no].c_str ());
        }

        else strncpy_s (szFileName, MAX_PATH, "Regular Filesystem", _TRUNCATE);

        source.name = szFileName;

        for ( auto& it : injectable )
        {
          if (it.second.archive == archive_no)
          {
            switch (it.second.method)
            {
              case DontCare:
              case Streaming:
                source.streaming.records.emplace_back (std::make_pair (it.first, it.second));
                source.streaming.size += it.second.size;
                break;

              case Blocking:
                source.blocking.records.emplace_back (std::make_pair (it.first, it.second));
                source.blocking.size += it.second.size;
                break;
            }

            source.checksums.emplace_back (it.first);
          }
        }

        return source;
      };

  if (list_dirty)
  {
    SK::D3D9::TextureManager& tex_mgr =
      SK_D3D9_GetTextureManager ();

    tex_mgr.getInjectableTextures (injectable);
    tex_mgr.getTextureArchives    (archives);

    sources.clear  ();

        sel = 0;
    int idx = 0;

    // First the .7z Data Sources
    for ( auto& it : archives )
    {
      std::ignore = it;

      sources.emplace_back (EnumerateSource (idx++));
    }

    // Then the Straight Filesystem
    sources.emplace_back (EnumerateSource (std::numeric_limits <unsigned int>::max ()));

    list_dirty = false;
  }

  ImGui::PushStyleVar   (ImGuiStyleVar_ChildRounding, 0.0f);
  ImGui::PushStyleColor (ImGuiCol_Border,             ImVec4 (0.4f, 0.6f, 0.9f, 1.0f));

#define FILE_LIST_WIDTH  (font_size * 20)
#define FILE_LIST_HEIGHT (font_size * (sources.size () + 3))

  ImGui::BeginChild ( ImGui::GetID ("Source List"),
                        ImVec2 ( FILE_LIST_WIDTH, FILE_LIST_HEIGHT ),
                          true,
                            ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NavFlattened );

  if (ImGui::IsWindowHovered ())
    can_scroll = false;

  auto DataSourceTooltip =
    [](int sel) ->
      void
      {
        auto& source =
          sources [sel];

        ImGui::BeginTooltip ();
        ImGui::TextColored  (ImVec4 (0.9f, 0.7f, 0.3f, 1.f), "Data Source  (%s)", source.name.c_str ());
        ImGui::Separator    ();

        ImGui::BeginGroup      (    );
        ImGui::TextUnformatted ( "Total Size:     ");
        ImGui::TextUnformatted ( "Blocking Data:  ");
        ImGui::TextUnformatted ( "Streaming Data: ");
        ImGui::EndGroup        (    );

        ImGui::SameLine        (    );

        ImGui::BeginGroup      (    );
        ImGui::TextUnformatted ( "" );
        ImGui::Text            ( "%4lu File%c", source.blocking.records.size  (),
                                                source.blocking.records.size  () != 1 ? 's' : ' ' );
        ImGui::Text            ( "%4lu File%c", source.streaming.records.size (),
                                                source.streaming.records.size () != 1 ? 's' : ' ' );
        ImGui::EndGroup        (    );

        ImGui::SameLine        (    );

        ImGui::BeginGroup      (    );
        ImGui::Text            ( "  %#5.2f MiB",
                                   (double)source.totalSize ()   / (1024.0 * 1024.0) );
        ImGui::Text            ( " (%#5.2f MiB)",
                                   (double)source.blocking.size  / (1024.0 * 1024.0) );
        ImGui::Text            ( " (%#5.2f MiB)",
                                   (double)source.streaming.size / (1024.0 * 1024.0) );
        ImGui::EndGroup        (    );

        ImGui::EndTooltip    ();
      };

  if (! sources.empty ())
  {
    static      int last_sel = 0;
    static bool sel_changed  = false;

    if (sel != last_sel)
      sel_changed = true;

    last_sel = sel;

    for ( int line = 0; line < sk::narrow_cast <int> (sources.size ()); line++)
    {
      if (line == sel)
      {
        bool selected = true;
        ImGui::Selectable (sources [line].name.c_str (), &selected);

        if (sel_changed)
        {
          ImGui::SetScrollHereY       (0.5f); // 0.0f:top, 0.5f:center, 1.0f:bottom
          ImGui::SetKeyboardFocusHere (    );

          sel_changed = false;
        }
      }

      else
      {
        bool selected = false;

        if (ImGui::Selectable (sources [line].name.c_str (), &selected))
        {
          sel_changed = true;
          //tex_dbg_idx                 =  line;
          sel                         =  line;
          //debug_tex_id                =  textures_used_last_dump [line];
        }
      }

      if (ImGui::IsItemHovered ())
        DataSourceTooltip (line);
    }

    ImGui::EndChild      ();

    ImVec2 list_size = ImGui::GetItemRectSize ();

    ImGui::PopStyleColor ();
    ImGui::PopStyleVar   ();

    ImGui::SameLine      ();

    ImGui::BeginGroup    ();

    ImGui::PushStyleColor  (ImGuiCol_Border, ImVec4 (0.5f, 0.5f, 0.5f, 1.0f));
    ImGui::BeginChild ( ImGui::GetID ("Texture Selection"),
                           ImVec2 (font_size * 30, list_size.y - font_size * 2),
                             true );

    if (ImGui::IsWindowHovered ())
      can_scroll = false;

    SK::D3D9::TextureManager& tex_mgr =
      SK_D3D9_GetTextureManager ();

    ImGui::BeginGroup ();
    for ( auto it : sources [sel].checksums )
    {
      const TexRecord& inject_tex =
        tex_mgr.getInjectableTexture (it);

      if (inject_tex.size != 0)
      {
        ImGui::TextColored ( ImVec4 (0.9f, 0.6f, 0.3f, 1.0f), " %08x    ", it );
      }
    }
    ImGui::EndGroup   ();
    ImGui::SameLine   ();
    ImGui::BeginGroup ();
    for ( auto it : sources [sel].checksums )
    {
      const TexRecord& inject_tex =
        tex_mgr.getInjectableTexture (it);

      if (inject_tex.size != 0)
      {
        bool streaming =
          inject_tex.method == Streaming;

        ImGui::TextColored ( streaming ?
                               ImVec4 ( 0.2f,  0.90f, 0.3f, 1.0f ) :
                               ImVec4 ( 0.90f, 0.3f,  0.2f, 1.0f ),
                                 "  %s    ",
                                   streaming ? "Streaming" :
                                               " Blocking" );
      }
    }
    ImGui::EndGroup   ();
    ImGui::SameLine   ();
    ImGui::BeginGroup ();
    for ( auto it : sources [sel].checksums )
    {
      const TexRecord& inject_tex =
        tex_mgr.getInjectableTexture (it);

      if (inject_tex.size != 0)
      {
        ImGui::TextColored ( ImVec4 (1.f, 1.f, 1.f, 1.f), "%#5.2f MiB  ",
                            (double)inject_tex.size / (1024.0 * 1024.0) );
      }
    }
    ImGui::EndGroup   ();


    ImGui::EndChild      (   );
    ImGui::PopStyleColor ( 1 );

    if (trigger_reset == reset_stage_e::Clear)
    {
      if (ImGui::Button ("  Refresh Data Sources  "))
      {
        tex_mgr.refreshDataSources ();
        list_dirty = true;
      }

      ImGui::SameLine ();

      if (ImGui::Button ("  Reload All Textures  "))
      {
        SK_D3D9_TriggerReset (false);
      }

      if (ImGui::IsItemHovered ())
          ImGui::SetTooltip ("Not all games support this, and some may lockup... reloading individual textures is always safe");
    }

    ImGui::EndGroup ();
  }

  else
    ImGui::EndChild   ();

  ImGui::PopItemWidth ();
}



void
SK_D3D9_LiveShaderClassView (SK::D3D9::ShaderClass shader_type, bool& can_scroll)
{
  ImGui::BeginGroup ();

  static float last_width = 256.0f;
  const  float font_size  = ImGui::GetFont ()->FontSize * ImGui::GetIO ().FontGlobalScale;

  struct shader_class_imp_s
  {
    std::vector <std::string> contents   = {  };
    bool                      dirty      = true;
    uint32_t                  last_sel   =    0;
    int                            sel   =   -1;
    float                     last_ht    = 256.0f;
    ImVec2                    last_min   = ImVec2 (0.0f, 0.0f);
    ImVec2                    last_max   = ImVec2 (0.0f, 0.0f);
  };

  static auto& _last_frame = last_frame.get ();

  struct {
    shader_class_imp_s vs;
    shader_class_imp_s ps;
  } static list_base;

  shader_class_imp_s*
    list    = ( shader_type == SK::D3D9::ShaderClass::Pixel ? &list_base.ps :
                                                              &list_base.vs );

  SK::D3D9::ShaderTracker*
    tracker = ( shader_type == SK::D3D9::ShaderClass::Pixel ? tracked_ps.getPtr () :
                                                              tracked_vs.getPtr () );

  std::vector <uint32_t>
    shaders   ( shader_type == SK::D3D9::ShaderClass::Pixel ? _last_frame.pixel_shaders.cbegin  () :
                                                              _last_frame.vertex_shaders.cbegin (),
                shader_type == SK::D3D9::ShaderClass::Pixel ? _last_frame.pixel_shaders.cend    () :
                                                              _last_frame.vertex_shaders.cend   () );

  std::unordered_map <uint32_t, SK::D3D9::ShaderDisassembly>&
    disassembly = ( shader_type == SK::D3D9::ShaderClass::Pixel ? *ps_disassembly :
                                                                  *vs_disassembly );

  const char*
    szShaderWord =  shader_type == SK::D3D9::ShaderClass::Pixel ? "Pixel" :
                                                                  "Vertex";

  if (list->dirty)
  {
        list->sel = -1;
    int idx       =  0;
        list->contents.clear ();

    // The underlying list is unsorted for speed, but that's not at all
    //   intuitive to humans, so sort the thing when we have the RT view open.
    std::sort ( shaders.begin (),
                shaders.end   () );



    for ( auto it : shaders )
    {
      char szDesc [16] = { };

      sprintf (szDesc, "%08" PRIxPTR, (uintptr_t)it);

      list->contents.emplace_back (szDesc);

      if ((uint32_t)it == list->last_sel)
      {
        list->sel = idx;
        //tracked_rt.tracking_tex = render_textures [sel];
      }

      ++idx;
    }
  }

  if (ImGui::IsMouseHoveringRect (list->last_min, list->last_max))
  {
         if (ImGui::IsKeyPressed (ImGuiKey_LeftBracket,  false)) { list->sel--;  ImGui::GetIO ().WantCaptureKeyboard = true; }
    else if (ImGui::IsKeyPressed (ImGuiKey_RightBracket, false)) { list->sel++;  ImGui::GetIO ().WantCaptureKeyboard = true; }
  }

  ImGui::PushStyleVar   (ImGuiStyleVar_WindowRounding, 0.0f);
  ImGui::PushStyleColor (ImGuiCol_Border, ImVec4 (0.9f, 0.7f, 0.5f, 1.0f));

  ImGui::BeginChild ( ImGui::GetID (szShaderWord),
                      ImVec2 ( font_size * 7.0f, std::max (font_size * 15.0f, list->last_ht)), true,
                        ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NavFlattened );

  if (ImGui::IsWindowHovered ())
  {
    can_scroll = false;

    ImGui::BeginTooltip ();
    ImGui::TextColored  (ImVec4 (0.9f, 0.6f, 0.2f, 1.0f), "You can cancel all render passes using the selected %s shader to disable an effect", szShaderWord);
    ImGui::Separator    ();
    ImGui::BulletText   ("Press [ while the mouse is hovering this list to select the previous shader");
    ImGui::BulletText   ("Press ] while the mouse is hovering this list to select the next shader");
    ImGui::EndTooltip   ();

         if (ImGui::IsKeyPressed (ImGuiKey_LeftBracket,  false)) { list->sel--;  ImGui::GetIO ().WantCaptureKeyboard = true; }
    else if (ImGui::IsKeyPressed (ImGuiKey_RightBracket, false)) { list->sel++;  ImGui::GetIO ().WantCaptureKeyboard = true; }
  }

  if (! shaders.empty ())
  {
    struct {
      int  last_sel    = 0;
      bool sel_changed = false;
    } static shader_state [3];

    int&  last_sel    = shader_state [static_cast <int> (shader_type)].last_sel;
    bool& sel_changed = shader_state [static_cast <int> (shader_type)].sel_changed;

    if (list->sel != last_sel)
      sel_changed = true;

    last_sel = list->sel;

    for ( size_t line = 0; line < shaders.size (); line++ )
    {
      if (line == static_cast <size_t> (list->sel))
      {
        bool selected    = true;

        ImGui::Selectable (list->contents [line].c_str (), &selected);

        if (sel_changed)
        {
          ImGui::SetScrollHereY       (0.5f);
          ImGui::SetKeyboardFocusHere (    );

          sel_changed     = false;
          list->last_sel  = (uint32_t)shaders [list->sel];
          tracker->crc32c = (uint32_t)shaders [list->sel];
        }
      }

      else
      {
        bool selected    = false;

        if (ImGui::Selectable (list->contents [line].c_str (), &selected))
        {
          sel_changed     = true;
          list->sel       = (int)line;
          list->last_sel  = (uint32_t)shaders [list->sel];
          tracker->crc32c = (uint32_t)shaders [list->sel];
        }
      }
    }
  }

  ImGui::EndChild      ();
  ImGui::PopStyleColor ();

  ImGui::SameLine      ();
  ImGui::BeginGroup    ();

  if (ImGui::IsItemHovered (ImGuiHoveredFlags_RectOnly)) {
         if (ImGui::IsKeyPressed (ImGuiKey_LeftBracket,  false)) list->sel--;
    else if (ImGui::IsKeyPressed (ImGuiKey_RightBracket, false)) list->sel++;
  }

  if (tracker->crc32c != 0x00)
  {
    ImGui::BeginGroup ();
    ImGui::Checkbox ( shader_type == SK::D3D9::ShaderClass::Pixel ? "Cancel Draws Using Selected Pixel Shader" :
                                                                    "Cancel Draws Using Selected Vertex Shader",
                        &tracker->cancel_draws );  ImGui::SameLine ();

    LONG num_draws =
     ReadAcquire (&tracker->num_draws);

    if (tracker->cancel_draws)
      ImGui::TextDisabled ("%lu Skipped Draw%c Last Frame (%lu textures)", num_draws, num_draws != 1 ? 's' : ' ', tracker->used_textures.size () );
    else
      ImGui::TextDisabled ("%lu Draw%c Last Frame         (%lu textures)", num_draws, num_draws != 1 ? 's' : ' ', tracker->used_textures.size () );

    ImGui::Checkbox ( shader_type == SK::D3D9::ShaderClass::Pixel ? "Clamp Texture Coordinates For Selected Pixel Shader" :
                                                                    "Clamp Texture Coordinates For Selected Vertex Shader",
                        &tracker->clamp_coords );

    ImGui::Separator      ();
    ImGui::EndGroup       ();

    if (ImGui::IsItemHovered (ImGuiHoveredFlags_RectOnly) && ! tracker->used_textures.empty ())
    {
      ImGui::BeginTooltip ();

      D3DFORMAT fmt = D3DFMT_UNKNOWN;

      for ( auto it : tracker->used_textures )
      {
        IDirect3DBaseTexture9* pTex = it;//ISKTextureD3D9* pTex = tex_mgr.getTexture (it)->d3d9_tex;

        if (pTex /*&& pTex->pTex*/)
        {
          D3DSURFACE_DESC desc;
          if (SUCCEEDED (((IDirect3DTexture9 *)pTex/*->pTex*/)->GetLevelDesc (0, &desc)))
          {
            fmt = desc.Format;
            ImGui::Image ( pTex/*->pTex*/, ImVec2  ( std::max (64.0f, (float)desc.Width / 16.0f),
          ((float)desc.Height / (float)desc.Width) * std::max (64.0f, (float)desc.Width / 16.0f) ),
                                       ImVec2  (0,0),             ImVec2  (1,1),
                                       ImColor (255,255,255,255), ImColor (242,242,13,255) );
          }

          ImGui::SameLine ();

          ImGui::BeginGroup ();
          ImGui::TextUnformatted ("Texture:");
          ImGui::TextUnformatted ("Format: ");
          ImGui::EndGroup   ();

          ImGui::SameLine    ();

          ImGui::BeginGroup  ();
          ImGui::Text        ("%08lx", it);
          ImGui::Text        ("%hs",   SK_D3D9_FormatToStr (fmt).c_str ());
          ImGui::EndGroup    ();
        }
      }

      ImGui::EndTooltip ();
    }

    SK_ImGui_AutoFont fixed_font (ImGui::GetIO ().Fonts->Fonts[1]);

    ImGui::PushStyleColor (ImGuiCol_Text, ImVec4 (0.80f, 0.80f, 1.0f, 1.0f));
    ImGui::TextWrapped    (disassembly [tracker->crc32c].header.c_str ());

    ImGui::SameLine       ();
    ImGui::BeginGroup     ();
    ImGui::TreePush       ("");
    ImGui::Spacing        (); ImGui::Spacing ();
    ImGui::PushStyleColor (ImGuiCol_Text, ImVec4 (0.666f, 0.666f, 0.666f, 1.0f));

    ImGui::PushItemWidth (font_size * 25);

    char szName [128] = { };

    ImGui::PushID (tracker->crc32c);

    for ( auto& it : tracker->constants )
    {
      ImGui::PushID (it.RegisterIndex);

      if (! it.struct_members.empty ())
      {
        ImGui::PushStyleColor  (ImGuiCol_Text, ImVec4 (0.9f, 0.1f, 0.7f, 1.0f));
        ImGui::TextUnformatted (it.Name);
        ImGui::PopStyleColor   ();

        for ( auto& it2 : it.struct_members )
        {
          ImGui::PushID (it2.RegisterIndex);

          bool tweakable = false;

          if (it2.Type == D3DXPT_FLOAT)
          {
            snprintf ( szName, 127, "[%s] %-32s",
                         shader_type == SK::D3D9::ShaderClass::Pixel ? "ps" :
                                                                       "vs",
                           it2.Name );

            if (it2.Class == D3DXPC_VECTOR ||
                it2.Class == D3DXPC_SCALAR)
            {
              switch (it2.Columns)
              {
                case 1:
                  tweakable = true;
                  ImGui::Checkbox    (szName, &it2.Override);
                  ImGui::SameLine    ( );
                  ImGui::InputFloat  ("", it2.Data);
                  break;
                case 2:
                  tweakable = true;
                  ImGui::Checkbox    (szName, &it2.Override);
                  ImGui::SameLine    ( );
                  ImGui::InputFloat2 ("", it2.Data);
                  break;
                case 3:
                  tweakable = true;
                  ImGui::Checkbox    (szName, &it2.Override);
                  ImGui::SameLine    ( );
                  ImGui::InputFloat3 ("", it2.Data);
                  break;
                case 4:
                  tweakable = true;
                  ImGui::Checkbox    (szName, &it2.Override);
                  ImGui::SameLine    ( );
                  ImGui::InputFloat4 ("", it2.Data);
                  break;
              }
            }
          }

          if (! tweakable)
          {
            snprintf ( szName, 127, "[%s] %-32s :(%c%-3u)",
                         shader_type == SK::D3D9::ShaderClass::Pixel ? "ps" :
                                                                       "vs",
                           it2.Name,
                             it2.RegisterSet != D3DXRS_SAMPLER ? 'c' : 's',
                               it2.RegisterIndex );

            ImGui::TreePush (""); ImGui::TextColored (ImVec4 (0.45f, 0.75f, 0.45f, 1.0f), szName); ImGui::TreePop ();
          }

          ImGui::PopID ();
        }

        ImGui::Separator ();
      }

      else
      {
        bool tweakable = false;

        snprintf ( szName, 127, "[%s] %-32s",
                     shader_type == SK::D3D9::ShaderClass::Pixel ? "ps" :
                                                                   "vs",
                       it.Name );

        if (it.Type == D3DXPT_FLOAT)
        {
          if (it.Class == D3DXPC_VECTOR ||
              it.Class == D3DXPC_SCALAR)
          {
            switch (it.Columns)
            {
              case 1:
                tweakable = true;
                ImGui::Checkbox    (szName, &it.Override);
                ImGui::SameLine    ( );
                ImGui::InputFloat  ("", it.Data);
                break;
              case 2:
                tweakable = true;
                ImGui::Checkbox    (szName, &it.Override);
                ImGui::SameLine    ( );
                ImGui::InputFloat2 ("", it.Data);
                break;
              case 3:
                tweakable = true;
                ImGui::Checkbox    (szName, &it.Override);
                ImGui::SameLine    ( );
                ImGui::InputFloat3 ("", it.Data);
                break;
              case 4:
                tweakable = true;
                ImGui::Checkbox    (szName, &it.Override);
                ImGui::SameLine    ( );
                ImGui::InputFloat4 ("", it.Data);
                break;
            }
          }
        }

        if (it.Type == D3DXPT_BOOL)
        {
          if (it.Class == D3DXPC_SCALAR)
          {
            switch (it.Columns)
            {
              case 1:
                tweakable = true;
                ImGui::Checkbox (szName, &it.Override);
                ImGui::SameLine ( );
                ImGui::Checkbox ("Enable", reinterpret_cast <bool *> (it.Data));
                break;
            }
          }
        }

        if (! tweakable)
        {
          snprintf ( szName, 127, "[%s] %-32s :(%c%-3u)",
                       shader_type == SK::D3D9::ShaderClass::Pixel ? "ps" :
                                                                     "vs",
                         it.Name,
                           it.RegisterSet != D3DXRS_SAMPLER ? 'c' : 's',
                             it.RegisterIndex );

          ImGui::TreePush (""); ImGui::TextColored (ImVec4 (0.45f, 0.75f, 0.45f, 1.0f), szName); ImGui::TreePop ();
        }
      }
      ImGui::PopID ();
    }
    ImGui::PopItemWidth   ();
    ImGui::TreePop        ();
    ImGui::EndGroup       ();

    ImGui::Separator      ();

    ImGui::PushStyleColor (ImGuiCol_Text, ImVec4 (0.99f, 0.99f, 0.01f, 1.0f));
    ImGui::TextWrapped    (disassembly [tracker->crc32c].code.c_str ());

    ImGui::Separator      ();

    ImGui::PushStyleColor (ImGuiCol_Text, ImVec4 (0.5f, 0.95f, 0.5f, 1.0f));
    ImGui::TextWrapped    (disassembly [tracker->crc32c].footer.c_str ());

    ImGui::PopStyleColor  (4);
    ImGui::PopID          ( );
  }
  else
    tracker->cancel_draws = false;

  ImGui::EndGroup      ();

  list->last_ht    = ImGui::GetItemRectSize ().y;

  list->last_min   = ImGui::GetItemRectMin ();
  list->last_max   = ImGui::GetItemRectMax ();

  ImGui::PopStyleVar   ();
  ImGui::EndGroup      ();
}

void
SK_LiveVertexStreamView (bool& can_scroll)
{
  static int filter_type = 0; // ALL

  ImGui::BeginGroup ();

  static float last_width = 256.0f;
  const  float font_size  = ImGui::GetFont ()->FontSize * ImGui::GetIO ().FontGlobalScale;

  struct vertex_stream_s
  {
    std::vector <std::string> contents;
    bool                      dirty      = true;
    uintptr_t                 last_sel   =    0;
    int                            sel   =   -1;
    float                     last_ht    = 256.0f;
    ImVec2                    last_min   = ImVec2 (0.0f, 0.0f);
    ImVec2                    last_max   = ImVec2 (0.0f, 0.0f);
  };

  struct {
    vertex_stream_s stream0;
  } static list_base;

  vertex_stream_s*
    list    = &list_base.stream0;

  SK::D3D9::VertexBufferTracker*
    tracker = tracked_vb.getPtr ();

  static auto& _last_frame = last_frame.get ();

  std::vector <IDirect3DVertexBuffer9 *> buffers;

  switch (filter_type)
  {
    case 2:
      for (auto& it : _last_frame.vertex_buffers.immutable) if (it != nullptr) buffers.emplace_back (it);
      break;
    case 1:
      for (auto& it : _last_frame.vertex_buffers.dynamic)   if (it != nullptr) buffers.emplace_back (it);
      break;
    case 0:
      for (auto& it : _last_frame.vertex_buffers.immutable) if (it != nullptr) buffers.emplace_back (it);
      for (auto& it : _last_frame.vertex_buffers.dynamic)   if (it != nullptr) buffers.emplace_back (it);
      break;
  };

  if (list->dirty)
  {
        list->sel = -1;
    int idx       =  0;
        list->contents.clear ();

    // The underlying list is unsorted for speed, but that's not at all
    //   intuitive to humans, so sort the thing when we have the RT view open.
    std::sort ( buffers.begin (),
                buffers.end   () );



    for ( auto it : buffers )
    {
      char szDesc [16] = { };

      sprintf (szDesc, "%08" PRIxPTR, (uintptr_t)it);

      list->contents.emplace_back (szDesc);

      if ((uintptr_t)it == list->last_sel)
      {
        list->sel = idx;
        //tracked_rt.tracking_tex = render_textures [sel];
      }

      ++idx;
    }
  }

  if (ImGui::IsMouseHoveringRect (list->last_min, list->last_max))
  {
         if (ImGui::IsKeyPressed (ImGuiKey_LeftBracket,  false)) { list->sel--;  ImGui::GetIO ().WantCaptureKeyboard = true; }
    else if (ImGui::IsKeyPressed (ImGuiKey_RightBracket, false)) { list->sel++;  ImGui::GetIO ().WantCaptureKeyboard = true; }
  }

  ImGui::PushStyleVar   (ImGuiStyleVar_ChildRounding, 0.0f);
  ImGui::PushStyleColor (ImGuiCol_Border, ImVec4 (0.9f, 0.7f, 0.5f, 1.0f));

  ImGui::BeginChild ( ImGui::GetID ("Stream 0"),
                      ImVec2 ( font_size * 7.0f, std::max (font_size * 15.0f, list->last_ht)),
                        true,
                          ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NavFlattened );

  if (ImGui::IsWindowHovered ())
  {
    can_scroll = false;

    ImGui::BeginTooltip ();
    ImGui::TextColored  (ImVec4 (0.9f, 0.6f, 0.2f, 1.0f), "You can cancel all render passes using the selected vertex buffer to debug a model");
    ImGui::Separator    ();
    ImGui::BulletText   ("Press [ while the mouse is hovering this list to select the previous shader");
    ImGui::BulletText   ("Press ] while the mouse is hovering this list to select the next shader");
    ImGui::EndTooltip   ();

         if (ImGui::IsKeyPressed (ImGuiKey_LeftBracket,  false)) { list->sel--;  ImGui::GetIO ().WantCaptureKeyboard = true; }
    else if (ImGui::IsKeyPressed (ImGuiKey_RightBracket, false)) { list->sel++;  ImGui::GetIO ().WantCaptureKeyboard = true; }
  }

  if (! buffers.empty ())
  {
    struct {
      int  last_sel    = 0;
      bool sel_changed = false;
    } static stream [3];

    int&  last_sel    = stream [0].last_sel;
    bool& sel_changed = stream [0].sel_changed;

    if (list->sel != last_sel)
      sel_changed = true;

    last_sel = list->sel;

    for ( size_t line = 0; line < buffers.size (); line++ )
    {
      if (line == static_cast <size_t> (list->sel))
      {
        bool selected    = true;

        ImGui::Selectable (list->contents [line].c_str (), &selected);

        if (sel_changed)
        {
          ImGui::SetScrollHereY (0.5f);

          sel_changed            = false;
          list->last_sel         = (uintptr_t)buffers [list->sel];
          tracker->vertex_buffer =            buffers [list->sel];
        }
      }

      else
      {
        bool selected    = false;

        if (ImGui::Selectable (list->contents [line].c_str (), &selected))
        {
          sel_changed            = true;
          list->sel              = (int)line;
          list->last_sel         = (uintptr_t)buffers [list->sel];
          tracker->vertex_buffer =            buffers [list->sel];
        }
      }
    }
  }

  else {
    tracker->vertex_buffer = nullptr;
  }

  ImGui::EndChild      ();
  ImGui::PopStyleColor ();

  ImGui::SameLine      ();
  ImGui::BeginGroup    ();

  if (ImGui::IsItemHovered (ImGuiHoveredFlags_RectOnly))
  {
         if (ImGui::IsKeyPressed (ImGuiKey_LeftBracket,  false)) { list->sel--;  ImGui::GetIO ().WantCaptureKeyboard = true; }
    else if (ImGui::IsKeyPressed (ImGuiKey_RightBracket, false)) { list->sel++;  ImGui::GetIO ().WantCaptureKeyboard = true; }

    if ( ImGui::IsKeyPressed (ImGuiKey_W, false) &&
         ImGui::GetIO ().KeysDown [VK_CONTROL]   &&
         ImGui::GetIO ().KeysDown [VK_SHIFT  ] )
    {
      ImGui::GetIO ().WantCaptureKeyboard = true;

      if (tracker->vertex_buffer != nullptr)
      {
        bool wireframe =
          tracker->wireframes.count   (tracker->vertex_buffer);

        if (wireframe)
          tracker->wireframes.erase   (tracker->vertex_buffer);
        else
          tracker->wireframes.emplace (tracker->vertex_buffer);
      }
    }
  }

  ImGui::Combo ("Vertex Buffer Filter", &filter_type, "  All   Geometry (Huge List)\0"
                                                      "Dynamic Geometry (Models)\0"
                                                      " Static Geometry (World)\0\0", 3 );

  ImGui::Checkbox ("Cancel Draws Using Selected Vertex Buffer",  &tracker->cancel_draws);  ImGui::SameLine ();

  LONG num_draws = ReadAcquire (&tracker->num_draws);
  LONG instanced = ReadAcquire (&tracker->instanced);

  if (tracker->cancel_draws)
    ImGui::TextDisabled ("%lu Skipped Draw%c Last Frame [%lu Instanced]", num_draws, num_draws != 1 ? 's' : ' ', instanced);
  else
    ImGui::TextDisabled ("%lu Draw%c Last Frame [%lu Instanced]        ", num_draws, num_draws != 1 ? 's' : ' ', instanced);

  ImGui::Checkbox ("Highlight Selected Vertex Buffer (Wireframe)", &tracker->wireframe);

  ImGui::Separator ();


  if ( tracker->vertex_buffer != nullptr &&
         ( _last_frame.vertex_buffers.dynamic.count   (tracker->vertex_buffer) ||
           _last_frame.vertex_buffers.immutable.count (tracker->vertex_buffer) ) )
  {
    bool wireframe =
      tracker->wireframes.count (tracker->vertex_buffer);

    D3DVERTEXBUFFER_DESC                             desc = { };
    if (SUCCEEDED (tracker->vertex_buffer->GetDesc (&desc)))
    {
      ImGui::BeginGroup   ();
      ImGui::PushStyleVar (ImGuiStyleVar_ChildRounding, 20.0f);

      ImVec4 border_color = wireframe ? ImVec4 (1.0f, 0.5f, 0.5f, 1.0f) :
                              tracker->wireframe ?
                                ImVec4 (0.5f, 0.5f, 1.0f, 1.0f) :
                                ImVec4 (0.6f, 0.6f, 0.6f, 1.0f);

      ImGui::PushStyleColor (ImGuiCol_Border, border_color);

      static int last_count = 1;

      ImGui::BeginChild     ( ImGui::GetID ("Buffer Overview"),
                              ImVec2 ( font_size * 44.0f,
                                       font_size * 8.0f +
                                       font_size * (float)last_count ),
                                true,
                                  ImGuiWindowFlags_AlwaysAutoResize );

      ImGui::BeginGroup  ();
      ImGui::TextColored (ImVec4 (1.0f, 1.0f, 1.0f, 1.0f), "Format:  ");
      ImGui::TextColored (ImVec4 (1.0f, 1.0f, 1.0f, 1.0f), "Type:    ");
      ImGui::TextColored (ImVec4 (1.0f, 1.0f, 1.0f, 1.0f), "Usage:   ");
      ImGui::TextColored (ImVec4 (1.0f, 1.0f, 1.0f, 1.0f), "Size:    ");
      ImGui::EndGroup    ();

      ImGui::SameLine    ();

      ImGui::BeginGroup  ();
      ImGui::TextColored (ImVec4 (1.0f, 1.0f, 0.4f, 1.0f), "%hs",  SK_D3D9_FormatToStr (desc.Format).c_str ());
      ImGui::TextColored (ImVec4 (1.0f, 1.0f, 0.4f, 1.0f), "%s",  desc.Type == D3DRTYPE_VERTEXBUFFER ? "Vertex Buffer" :
                                                                  desc.Type == D3DRTYPE_INDEXBUFFER  ? "Index Buffer"  :
                                                                                                       "Unknown?!" );
      ImGui::TextColored (ImVec4 (1.0f, 1.0f, 0.4f, 1.0f), "%hs",  SK_D3D9_UsageToStr (desc.Usage).c_str ());
      ImGui::TextColored (ImVec4 (1.0f, 1.0f, 0.4f, 1.0f), "%lu", desc.Size);
      ImGui::EndGroup    ();

      last_count = 0;

      for (auto vtx_decl : tracker->vertex_decls)
      {
        ++last_count;

        static D3DVERTEXELEMENT9 elem_decl [MAXD3DDECLLENGTH];
        static UINT              num_elems;

        auto SK_D3D9_DeclTypeToStr = [](D3DDECLTYPE type) ->
          const char*
          {
            switch (type)
            {
              case D3DDECLTYPE_FLOAT1:     return "float";
              case D3DDECLTYPE_FLOAT2:     return "float2";
              case D3DDECLTYPE_FLOAT3:     return "float3";
              case D3DDECLTYPE_FLOAT4:     return "float4";
              case D3DDECLTYPE_D3DCOLOR:   return "D3DCOLOR";
              case D3DDECLTYPE_UBYTE4:     return "ubyte4";
              case D3DDECLTYPE_SHORT2:     return "short2";
              case D3DDECLTYPE_SHORT4:     return "short4";
              case D3DDECLTYPE_UBYTE4N:    return "ubyte4 (UNORM)";
              case D3DDECLTYPE_SHORT2N:    return "short2 (SNORM)";
              case D3DDECLTYPE_SHORT4N:    return "short4 (SNORM)";
              case D3DDECLTYPE_USHORT2N:   return "short2 (UNORM)";
              case D3DDECLTYPE_USHORT4N:   return "short4 (UNORM)";
              case D3DDECLTYPE_UDEC3:      return "udec3";
              case D3DDECLTYPE_DEC3N:      return "dec3 (NORM)";
              case D3DDECLTYPE_FLOAT16_2:  return "half2";
              case D3DDECLTYPE_FLOAT16_4:  return "half4";
              case D3DDECLTYPE_UNUSED:     return "UNUSED";
            }

            return "UNKNOWN";
          };

        auto SK_D3D9_DeclUsageToStr = [](D3DDECLUSAGE usage, int idx, int part) ->
          const char*
          {
            static char szOut [64] = { '\0' };

            switch (part)
            {
              case 0:
                switch (usage)
                {
                  case D3DDECLUSAGE_POSITION:     snprintf (szOut, 64, "POSITION      "); break;
                  case D3DDECLUSAGE_BLENDWEIGHT:  snprintf (szOut, 64, "BLENDWEIGHT   "); break;
                  case D3DDECLUSAGE_BLENDINDICES: snprintf (szOut, 64, "BLENDINDICES  "); break;
                  case D3DDECLUSAGE_NORMAL:       snprintf (szOut, 64, "NORMAL        "); break;
                  case D3DDECLUSAGE_PSIZE:        snprintf (szOut, 64, "PSIZE         "); break;
                  case D3DDECLUSAGE_TEXCOORD:     snprintf (szOut, 64, "TEXCOORD      "); break;
                  case D3DDECLUSAGE_TANGENT:      snprintf (szOut, 64, "TANGENT       "); break;
                  case D3DDECLUSAGE_BINORMAL:     snprintf (szOut, 64, "BINORMAL      "); break;
                  case D3DDECLUSAGE_TESSFACTOR:   snprintf (szOut, 64, "TESSFACTOR    "); break;
                  case D3DDECLUSAGE_POSITIONT:    snprintf (szOut, 64, "POSITIONT     "); break;
                  case D3DDECLUSAGE_COLOR:        snprintf (szOut, 64, "COLOR         "); break;
                  case D3DDECLUSAGE_FOG:          snprintf (szOut, 64, "FOG           "); break;
                  case D3DDECLUSAGE_DEPTH:        snprintf (szOut, 64, "DEPTH         "); break;
                  case D3DDECLUSAGE_SAMPLE:       snprintf (szOut, 64, "SV_SampleIndex"); break;
                };
                break;

              case 1:
                switch (usage)
                {
                  case D3DDECLUSAGE_POSITION:     snprintf (szOut, 64, " [%i]", idx); break;
                  case D3DDECLUSAGE_BLENDWEIGHT:  snprintf (szOut, 64, " [%i]", idx); break;
                  case D3DDECLUSAGE_BLENDINDICES: snprintf (szOut, 64, " [%i]", idx); break;
                  case D3DDECLUSAGE_NORMAL:       snprintf (szOut, 64, " [%i]", idx); break;
                  case D3DDECLUSAGE_PSIZE:        snprintf (szOut, 64, " "         ); break;
                  case D3DDECLUSAGE_TEXCOORD:     snprintf (szOut, 64, " [%i]", idx); break;
                  case D3DDECLUSAGE_TANGENT:      snprintf (szOut, 64, " [%i]", idx); break;
                  case D3DDECLUSAGE_BINORMAL:     snprintf (szOut, 64, " [%i]", idx); break;
                  case D3DDECLUSAGE_TESSFACTOR:   snprintf (szOut, 64, " [%i]", idx); break;
                  case D3DDECLUSAGE_POSITIONT:    snprintf (szOut, 64, " "         ); break;
                  case D3DDECLUSAGE_COLOR:        snprintf (szOut, 64, " [%i]", idx); break;
                  case D3DDECLUSAGE_FOG:          snprintf (szOut, 64, " "         ); break;
                  case D3DDECLUSAGE_DEPTH:        snprintf (szOut, 64, " [%i]", idx); break;
                  case D3DDECLUSAGE_SAMPLE:       snprintf (szOut, 64, " "         ); break;
                };
                break;
            }

            return szOut;
          };

        if (SUCCEEDED (vtx_decl->GetDeclaration (elem_decl, &num_elems)))
        {
          ImGui::Separator  ();
          ImGui::BeginGroup ();
          for (UINT i = 0; i < num_elems; i++)
          {
            if (elem_decl [i].Type != D3DDECLTYPE_UNUSED)
            {
              ++last_count;

              ImGui::TextColored (ImVec4 (0.9f, 0.9f, 0.9f, 1.0f),    "Stream %3lu ", elem_decl [i].Stream);
            }
          }
          ImGui::EndGroup   ();
          ImGui::SameLine   ();
          ImGui::BeginGroup ();
          for (UINT i = 0; i < num_elems; i++)
          {
            if (elem_decl [i].Type != D3DDECLTYPE_UNUSED)
            {
              ImGui::TextColored (ImVec4 (0.66f, 0.66f, 0.66f, 1.0f), "(+%02lu): ",
                                  elem_decl [i].Offset);
            }
          }
          ImGui::EndGroup   ();
          ImGui::SameLine   ();
          ImGui::BeginGroup ();
          for (UINT i = 0; i < num_elems; i++)
          {
            if (elem_decl [i].Type != D3DDECLTYPE_UNUSED)
            {
              ImGui::TextColored (ImVec4 (0.35f, 0.85f, 0.35f, 1.0f), "%16s ",
                                  SK_D3D9_DeclTypeToStr ((D3DDECLTYPE)elem_decl [i].Type));
            }
          }
          ImGui::EndGroup   ();
          ImGui::SameLine   ();
          ImGui::BeginGroup ();
          for (UINT i = 0; i < num_elems; i++)
          {
            if (elem_decl [i].Type != D3DDECLTYPE_UNUSED)
            {
              ImGui::TextColored (ImVec4 (0.6f, 0.6f, 1.0f, 1.0f), R"("Attrib%02lu")", i);
            }
          }
          ImGui::EndGroup   ();
          ImGui::SameLine   ();
          ImGui::BeginGroup ();
          for (UINT i = 0; i < num_elems; i++)
          {
            if (elem_decl [i].Type != D3DDECLTYPE_UNUSED)
            {
              ImGui::TextColored (ImVec4 (1.0f, 1.0f, 1.0f, 1.0f), " : %s",
                    SK_D3D9_DeclUsageToStr ((D3DDECLUSAGE)elem_decl [i].Usage, elem_decl [i].UsageIndex, 0));
            }
          }
          ImGui::EndGroup   ();
          ImGui::SameLine   ();
          ImGui::BeginGroup ();
          for (UINT i = 0; i < num_elems; i++)
          {
            if (elem_decl [i].Type != D3DDECLTYPE_UNUSED)
            {
              ImGui::TextColored (ImVec4 (1.0f, 1.0f, 1.0f, 1.0f), " : %s",
                    SK_D3D9_DeclUsageToStr ((D3DDECLUSAGE)elem_decl [i].Usage, elem_decl [i].UsageIndex, 1));
            }
          }
          ImGui::EndGroup   ();

          --last_count;
        }
      }
      ImGui::EndChild ();

      ImGui::PopStyleColor ();
      ImGui::PopStyleVar   ();
      ImGui::EndGroup      ();

#if 0
      if (ImGui::IsItemHoveredRect () && tracker->textures.size ())
      {
        ImGui::BeginTooltip ();

        D3DFORMAT fmt = D3DFMT_UNKNOWN;

        for ( auto it : tracker->textures )
        {
          ISKTextureD3D9* pTex = tex_mgr.getTexture (it)->d3d9_tex;

          if (pTex && pTex->pTex)
          {
            D3DSURFACE_DESC desc;
            if (SUCCEEDED (pTex->pTex->GetLevelDesc (0, &desc)))
            {
              fmt = desc.Format;
              ImGui::Image ( pTex->pTex, ImVec2  ( std::max (64.0f, (float)desc.Width / 16.0f),
        ((float)desc.Height / (float)desc.Width) * std::max (64.0f, (float)desc.Width / 16.0f) ),
                                         ImVec2  (0,0),             ImVec2  (1,1),
                                         ImColor (255,255,255,255), ImColor (242,242,13,255) );
            }

            ImGui::SameLine ();

            ImGui::BeginGroup ();
            ImGui::Text       ("Texture: %08lx", it);
            ImGui::Text       ("Format:  %hs",   SK_D3D9_FormatToStr (fmt).c_str ());
            ImGui::EndGroup   ();
          }
        }

        ImGui::EndTooltip ();
      }
#endif

      ImGui::SameLine ();
      ImGui::Checkbox ("Always Draw This Buffer In Wireframe", &wireframe);

      if (wireframe)
        tracker->wireframes.emplace (tracker->vertex_buffer);
      else if (tracker->wireframes.count (tracker->vertex_buffer))
        tracker->wireframes.erase   (tracker->vertex_buffer);
    }

    if ((! tracker->vertex_shaders.empty ()) || (! tracker->pixel_shaders.empty ()))
    {
      ImGui::Separator ( );
      ImGui::Columns   (2);

      for ( auto it : tracker->vertex_shaders )
        ImGui::Text ("Vertex Shader: %08x", it);

      ImGui::NextColumn ();

      for ( auto it : tracker->pixel_shaders )
        ImGui::Text ("Pixel Shader: %08x", it);

      ImGui::Columns   (1);
    }
  }
  else
    tracker->cancel_draws = false;

  ImGui::EndGroup      ( );

  list->last_ht  = ImGui::GetItemRectSize ().y;

  list->last_min = ImGui::GetItemRectMin ( );
  list->last_max = ImGui::GetItemRectMax ( );

  ImGui::PopStyleVar   ( );
  ImGui::EndGroup      ( );
}

extern int32_t  tex_dbg_idx;
extern uint32_t debug_tex_id;
extern std::set <uint32_t> textures_used_last_dump;

bool
SK_D3D9_TextureModDlg (void)
{
  const float font_size = ImGui::GetFont ()->FontSize * ImGui::GetIO ().FontGlobalScale;
  bool        show_dlg  = true;

  ImGuiIO& io =
    ImGui::GetIO ();


  auto HandleKeyboard = [&](void)
  {
    extern std::set <uint32_t> textures_used_last_dump;

    if ( ImGui::IsKeyPressed (ImGuiKey_RightBracket, false) ||
         ImGui::IsKeyPressed (ImGuiKey_LeftBracket,  false) )
    {
      tex_dbg_idx += (ImGui::IsKeyPressed (ImGuiKey_RightBracket, false)) ? 1 : 0;
      tex_dbg_idx -= (ImGui::IsKeyPressed (ImGuiKey_LeftBracket,  false)) ? 1 : 0;

      if (tex_dbg_idx < 0 || (textures_used_last_dump.empty ()))
      {
        tex_dbg_idx  = -1;
        debug_tex_id =  0;
      }

      else
      {
        if (tex_dbg_idx >= sk::narrow_cast <int32_t> (textures_used_last_dump.size ()))
        {
          tex_dbg_idx =
            std::max (0UL, sk::narrow_cast <uint32_t> (textures_used_last_dump.size ()) - 1UL);
        }
      }

      if (tex_dbg_idx >= 0)
      {
        debug_tex_id = 0;
        int      idx = 0;

        for (auto it : textures_used_last_dump)
        {
          if (tex_dbg_idx == idx++)
          {
            debug_tex_id = it;
            break;
          }
        }
      }

      SK::D3D9::TextureManager& tex_mgr =
        SK_D3D9_GetTextureManager ();

      tex_mgr.updateOSD ();
    }
  };


  static auto& _last_frame = last_frame.get ();
  static auto& _known_objs = known_objs.get ();
  static auto& _tracked_vs = tracked_vs.get ();
  static auto& _tracked_ps = tracked_ps.get ();
  static auto& _tracked_vb = tracked_vb.get ();
  static auto& _tracked_rt = tracked_rt.get ();



  ImGui::SetNextWindowSize            (ImVec2 (io.DisplaySize.x * 0.66f, io.DisplaySize.y * 0.42f), ImGuiCond_Appearing);
  ImGui::SetNextWindowSizeConstraints ( /*ImVec2 (768.0f, 384.0f),*/
                                       ImVec2 (io.DisplaySize.x * 0.16f, io.DisplaySize.y * 0.16f),
                                       ImVec2 (io.DisplaySize.x * 0.96f, io.DisplaySize.y * 0.96f));

  static std::string ver_str =
    SK_FormatString ("D3D9 Render Mod Toolkit (v %s)", SK_GetVersionStrA ());

  ImGui::Begin ( ver_str.c_str (), &show_dlg );

  bool can_scroll = ImGui::IsWindowFocused () && ImGui::IsMouseHoveringRect ( ImVec2 (ImGui::GetWindowPos ().x,                             ImGui::GetWindowPos ().y),
                                                                              ImVec2 (ImGui::GetWindowPos ().x + ImGui::GetWindowSize ().x, ImGui::GetWindowPos ().y + ImGui::GetWindowSize ().y) );

  ImGui::PushItemWidth (ImGui::GetWindowWidth () * 0.666f);

#if 0
  if (ImGui::CollapsingHeader ("Preliminary Documentation"))
  {
    ImGui::BeginChild (ImGui::GetID ("ModDescription"), ImVec2 (font_size * 66.0f, font_size * 25.0f), true);
      ImGui::TextColored    (ImVec4 (0.9f, 0.7f, 0.5f, 1.0f), "Texture Modding Overview"); ImGui::SameLine ();
      ImGui::Text           ("    (Full Writeup Pending)");

      ImGui::Separator      ();

      ImGui::TextWrapped    ("\nReplacement textures go in (TBFix_Res\\inject\\textures\\{blocking|streaming}\\<checksum>.dds)\n\n");

      ImGui::TreePush ("");
        ImGui::BulletText ("Blocking textures have a high performance penalty, but zero chance of visible pop-in.");
        ImGui::BulletText ("Streaming textures will replace the game's original texture whenever Disk/CPU loads finish.");
        ImGui::TreePush   ("");
          ImGui::PushStyleColor (ImGuiCol_Text, ImVec4 (0.6f, 0.9f, 0.2f, 1.0f));
          ImGui::BulletText     ("Use streaming whenever possible or performance will bite you in the ass.");
          ImGui::PopStyleColor  ();
        ImGui::TreePop    (  );
      ImGui::TreePop  ();

      ImGui::TextWrapped    ("\n\nLoading modified textures from separate files is inefficient; entire groups of textures may also be packaged into \".7z\" files (See TBFix_Res\\inject\\00_License.7z as an example, and use low/no compression ratio or you will kill the game's performance).\n");

      ImGui::Separator      ();

      ImGui::PushStyleColor (ImGuiCol_Text, ImVec4 (0.9f, 0.6f, 0.3f, 1.0f));
      ImGui::TextWrapped    ( "\n\nA more detailed synopsis will follow in future versions, for now please refer to the GitHub release notes for Tales of Symphonia "
                              "\"Fix\" v 0.9.0 for a thorough description on authoring texture mods.\n\n" );
      ImGui::PopStyleColor  ();

      ImGui::Separator      ();

      ImGui::Bullet         (); ImGui::SameLine ();
      ImGui::TextWrapped    ( "If texture mods are enabled, you can click on the Injected and Base buttons on the texture cache "
                                "summary panel to compare modified and unmodified." );
    ImGui::EndChild         ();
  }
#endif

  if (ImGui::CollapsingHeader("Injectable Data Sources", ImGuiTreeNodeFlags_CollapsingHeader | ImGuiTreeNodeFlags_DefaultOpen))
  {
    SK_D3D9_DrawFileList  (can_scroll);
  }

  if (ImGui::CollapsingHeader ("Live Texture View", ImGuiTreeNodeFlags_CollapsingHeader | ImGuiTreeNodeFlags_DefaultOpen))
  {
    SK::D3D9::TextureManager& tex_mgr =
      SK_D3D9_GetTextureManager ();

    HandleKeyboard ();

    static float last_ht    = 256.0f;
    static float last_width = 256.0f;

    static std::vector <std::string> list_contents;
    static bool                      list_dirty     = false;
    static int                       sel            =     0;

    ImGui::BeginChild ( ImGui::GetID ("D3D9_ToolHeadings"),
                          ImVec2 (font_size * 66.0f, font_size * 2.5f),
                            false,
                              ImGuiWindowFlags_AlwaysUseWindowPadding |
                              ImGuiWindowFlags_NavFlattened           | ImGuiWindowFlags_AlwaysAutoResize );

    if (ImGui::Button ("  Refresh Textures  "))
    {
      SK_ICommandProcessor& command =
        *SK_Render_InitializeSharedCVars ();

      command.ProcessCommandLine ("Textures.Trace true");

      tex_mgr.updateOSD ();

      list_dirty = true;
    }

    if (ImGui::IsItemHovered ()) ImGui::SetTooltip ("Refreshes the set of texture checksums used in the last frame drawn.");

    ImGui::SameLine ();

    if (ImGui::Button (" Clear Debug "))
    {
      sel                         = -1;
      debug_tex_id                =  0;
      textures_used_last_dump.clear ();
      last_ht                     =  0;
      last_width                  =  0;
    }

    if (ImGui::IsItemHovered ()) ImGui::SetTooltip ("Exits texture debug mode.");

    ImGui::SameLine ();

    bool need_reset_graphics = false;

    if (ImGui::Checkbox ("Enable On-Demand Texture Dumping",    &config.textures.on_demand_dump)) need_reset_graphics = true;

    if (ImGui::IsItemHovered ())
    {
      ImGui::BeginTooltip ();
      ImGui::TextColored  (ImVec4 (0.9f, 0.7f, 0.3f, 1.f), "Enable dumping DXT compressed textures from VRAM.");
      ImGui::Separator    ();
      ImGui::Text         ("Drivers may not be able to manage texture memory as efficiently, and you should turn this option off when not modifying textures.\n\n");
      ImGui::BulletText   ("If this is your first time enabling this feature, the dump button will not work until you reload all textures in-game.");
      ImGui::EndTooltip   ();
    }

    ImGui::SameLine ();

    ImGui::Checkbox ("Highlight Selected Texture in Game",    &config.textures.highlight_debug_tex);

    ImGui::Separator ();
    ImGui::EndChild  ();

    if (list_dirty)
    {
      list_contents.clear ();
           sel = tex_dbg_idx;

      if (debug_tex_id == 0)
        last_ht = 0;

      for ( auto it : textures_used_last_dump )
      {
        char szDesc [16] = { };

        sprintf (szDesc, "%08x", it);

        list_contents.emplace_back (szDesc);
      }
    }

    ImGui::BeginGroup ();

    ImGui::PushStyleVar   (ImGuiStyleVar_ChildRounding, 0.0f);
    ImGui::PushStyleColor (ImGuiCol_Border, ImVec4 (0.9f, 0.7f, 0.5f, 1.0f));

    ImGui::BeginChild ( ImGui::GetID ("Item List"),
                        ImVec2 ( font_size * 6.0f, std::max (font_size * 15.0f, last_ht)),
                          true, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NavFlattened );

    if (ImGui::IsWindowHovered ())
      can_scroll = false;

    if (! textures_used_last_dump.empty ())
    {
      static      int last_sel = 0;
      static bool sel_changed  = false;
    
      if (sel != last_sel)
        sel_changed = true;
    
      last_sel = sel;
    
      for ( int line = 0; line < sk::narrow_cast <int> (textures_used_last_dump.size ()); line++)
      {
        if (line == sel)
        {
          bool selected = true;
          ImGui::Selectable (list_contents [line].c_str (), &selected);
    
          if (sel_changed)
          {
            ImGui::SetScrollHereY (0.5f); // 0.0f:top, 0.5f:center, 1.0f:bottom
            sel_changed = false;
          }
        }
    
        else
        {
          bool selected = false;
    
          if (ImGui::Selectable (list_contents [line].c_str (), &selected))
          {
            sel_changed  = true;
            tex_dbg_idx  =  line;
            sel          =  line;
    
            int idx = 0;
            for (auto it : textures_used_last_dump)
            {
              if (line == idx++)
                debug_tex_id = it;
            }
            //debug_tex_id =  textures_used_last_dump [line];
          }
        }
      }
    }
    
    ImGui::EndChild      ();
    ImGui::PopStyleColor ();
    
    if (ImGui::IsItemHovered ())
    {
      ImGui::BeginTooltip ();
      ImGui::TextColored  (ImVec4 (0.9f, 0.6f, 0.2f, 1.0f), R"(If highlighting is enabled, the "debug" texture will blink to make identifying textures easier.)");
      ImGui::Separator    ();
      ImGui::BulletText   ("Press %hs to select the previous texture from this list", SK_WideCharToUTF8 (virtKeyCodeToHumanKeyName [VK_OEM_4]).c_str ());
      ImGui::BulletText   ("Press %hs to select the next texture from this list",     SK_WideCharToUTF8 (virtKeyCodeToHumanKeyName [VK_OEM_6]).c_str ());
      ImGui::EndTooltip   ();
    }

    ImGui::SameLine     ();
    ImGui::PushStyleVar (ImGuiStyleVar_ChildRounding, 20.0f);

    last_ht    = std::max (last_ht,    16.0f);
    last_width = std::max (last_width, 16.0f);
    
    if (debug_tex_id != 0x00)
    {
      SK::D3D9::Texture* pTex =
        tex_mgr.getTexture (debug_tex_id);
    
      extern bool __remap_textures;
             bool has_alternate = ( pTex                         != nullptr &&
                                    pTex->d3d9_tex               != nullptr &&
                                    pTex->d3d9_tex->pTexOverride != nullptr &&
               SK_ValidatePointer ( pTex->d3d9_tex->pTex, true ) );
     
      if ( pTex                 != nullptr &&
           pTex->d3d9_tex       != nullptr &&
           pTex->d3d9_tex->pTex != nullptr && SK_ValidatePointer (
           pTex->d3d9_tex->pTex, true                            )
         )
      {
        D3DSURFACE_DESC desc;
    
        if (SUCCEEDED (pTex->d3d9_tex->pTex->GetLevelDesc (0, &desc)))
        {
          ImVec4 border_color = config.textures.highlight_debug_tex ?
                                    ImVec4 (0.3f, 0.3f, 0.3f, 1.0f) :
                                    (pTex->d3d9_tex->getDrawTexture () != pTex->d3d9_tex->pTex)
                                                                    ? ImVec4 (0.5f,  0.5f,  0.5f, 1.0f) :
                                                                      ImVec4 (0.3f,  1.0f,  0.3f, 1.0f);

          ImGui::PushStyleColor (ImGuiCol_Border, border_color);
          
          ImGui::BeginGroup     ();
          ImGui::BeginChild     ( ImGui::GetID ("Item Selection"),
                                  ImVec2 ( std::max (font_size * 19.0f, (float)desc.Width + 24.0f),
                                (float)desc.Height + font_size * 11.0f),
                                    true,
                                      ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NavFlattened );

          if ((! config.textures.highlight_debug_tex) && has_alternate)
          {
            if (ImGui::IsItemHovered ())
            {
              if (pTex->d3d9_tex->img_to_use != ISKTextureD3D9::ContentPreference::Original)
                 ImGui::SetTooltip ("Click me to make this the always visible version");
              else
              {
                ImGui::BeginTooltip ( );
                ImGui::Text         ("Click me to use the system-wide texture preference");
                ImGui::Separator    ( );
                ImGui::BulletText   ( "The system-wide preference is currently:  (%s)",
                                        __remap_textures ? "Draw injected textures" :
                                                           "Draw original textures" );
                ImGui::EndTooltip   ( );
              }
            }

            // Allow the user to toggle texture override by clicking the frame
            if (SK_ImGui_IsItemClicked ())
            {
              pTex->d3d9_tex->toggleOriginal ();
            }
          }

          last_width  = (float)desc.Width;
          last_ht     = (float)desc.Height + font_size * 11.0f;


          int num_lods =
            pTex->d3d9_tex->pTex->GetLevelCount ();

          ImGui::BeginGroup      (                  );
          ImGui::TextUnformatted ( "Dimensions:   " );
          ImGui::TextUnformatted ( "Format:       " );
          ImGui::TextUnformatted ( "Data Size:    " );
          ImGui::TextUnformatted ( "Load Time:    " );
          ImGui::TextUnformatted ( "References:   " );
          ImGui::EndGroup        (                  );
          
          ImGui::SameLine   ();
          
          ImGui::BeginGroup ();
          ImGui::Text ("%lux%lu (%lu %s)",
                       desc.Width, desc.Height,
                       num_lods, num_lods > 1 ? "LODs" : "LOD");
          ImGui::Text ("%hs",
                       SK_D3D9_FormatToStr (desc.Format).c_str ( ));
          ImGui::Text ("%.3f MiB",
            (double)pTex->d3d9_tex->tex_size / ( 1024.0f * 1024.0f ));
          ImGui::Text ("%.3f ms",
                       pTex->load_time);
          ImGui::Text ("%lu",
                       pTex->refs.load ());


          //extern std::unordered_map <uint32_t, int32_t>            injected_refs;
          //ImGui::Text ("%lu", injected_refs [pTex->crc32c]);
          ImGui::EndGroup   ();
          
          ImGui::Separator  ();
          
          static bool flip_vertical0   = false;
          static bool flip_horizontal0 = false;
          
          ImGui::Checkbox ("Flip Vertically##D3D9_FlipVertical0",     &flip_vertical0);   ImGui::SameLine ( );
          ImGui::Checkbox ("Flip Horizontally##D3D9_FlipHorizontal0", &flip_horizontal0);
          
          if (! tex_mgr.isTextureDumped (debug_tex_id))
          {
            if ( ImGui::Button ("  Dump Texture to Disk  ###DumpTexture") )
            {
              tex_mgr.dumpTexture (desc.Format, debug_tex_id, pTex->d3d9_tex->pTex);
            }
          
            //if (config.textures.quick_load && ImGui::IsItemHovered ())
            //  ImGui::SetTooltip ("Turn off Texture QuickLoad to use this feature.");
          }
          
          else
          {
            if ( ImGui::Button ("  Delete Dumped Texture from Disk  ###DumpTexture") )
            {
              tex_mgr.deleteDumpedTexture (desc.Format, debug_tex_id);
            }
          }

          ImVec2 uv0 (flip_horizontal0 ? 1.0f : 0.0f, flip_vertical0 ? 1.0f : 0.0f);
          ImVec2 uv1 (flip_horizontal0 ? 0.0f : 1.0f, flip_vertical0 ? 0.0f : 1.0f);
          
          ImGui::PushStyleColor  (ImGuiCol_Border, ImVec4 (0.95f, 0.95f, 0.05f, 1.0f));
          ImGui::BeginChildFrame (ImGui::GetID ("ChildFrame_XXX"), ImVec2 ((float)desc.Width + 8, (float)desc.Height + 8),
                                  ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoScrollbar );
          ImGui::Image           ( pTex->d3d9_tex->pTex,
                                     ImVec2 ((float)desc.Width, (float)desc.Height),
                                       uv0,                       uv1,
                                       ImColor (255,255,255,255), ImColor (255,255,255,128)
                                 );
          ImGui::EndChildFrame   ();
          ImGui::PopStyleColor   ();
          ImGui::EndChild        ();
          ImGui::EndGroup        ();
          ImGui::PopStyleColor   ();
        }
      }

      if (has_alternate)
      {
        ImGui::SameLine ();

        D3DSURFACE_DESC desc;
        
        if (            pTex->d3d9_tex != nullptr               &&
                        pTex->d3d9_tex->pTexOverride != nullptr &&
             SUCCEEDED (pTex->d3d9_tex->pTexOverride->GetLevelDesc (0, &desc)) )
        {
          bool override_tex = (pTex->d3d9_tex->getDrawTexture () == pTex->d3d9_tex->pTexOverride);

          ImVec4 border_color = config.textures.highlight_debug_tex ?
                                  ImVec4 (0.3f, 0.3f, 0.3f, 1.0f) :
                   override_tex ? ImVec4 (0.3f, 1.0f, 0.3f, 1.0f) :
                                  ImVec4 (0.5f, 0.5f, 0.5f, 1.0f);

          ImGui::PushStyleColor  (ImGuiCol_Border, border_color);
        
          ImGui::BeginGroup ();
          ImGui::BeginChild ( ImGui::GetID ("Item Selection2"),
                              ImVec2 ( std::max (font_size * 19.0f, (float)desc.Width  + 24.0f),
                                                                    (float)desc.Height + font_size * 10.0f),
                                true,
                                  ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NavFlattened );
          ImGui::PopStyleColor ();

          if (! config.textures.highlight_debug_tex)
          {
            if (ImGui::IsItemHovered ())
            {
              if (pTex->d3d9_tex->img_to_use != ISKTextureD3D9::ContentPreference::Override)
                ImGui::SetTooltip ("Click me to make this the always visible version");
              else
              {
                ImGui::BeginTooltip ( );
                ImGui::Text         ("Click me to use the system-wide texture preference");
                ImGui::Separator    ( );
                ImGui::BulletText   ( "The system-wide preference is currently:  (%s)",
                                        __remap_textures ? "Draw injected textures" :
                                                           "Draw original textures" );
                ImGui::EndTooltip   ( );
              }
            }

            // Allow the user to toggle texture override by clicking the frame
            if (SK_ImGui_IsItemClicked ())
            {
              pTex->d3d9_tex->toggleOverride ();
            }
          }

          last_width  = std::max (last_width, (float)desc.Width);
          last_ht     = std::max (last_ht,    (float)desc.Height + font_size * 10.0f);

          bool injected  =
            (tex_mgr.getInjectableTexture (debug_tex_id).size != 0),
               reloading = false;

          int num_lods =
            pTex->d3d9_tex->pTexOverride->GetLevelCount ();

          ImGui::BeginGroup      ();
          ImGui::TextUnformatted ( "Dimensions:   " );
          ImGui::TextUnformatted ( "Format:       " );
          ImGui::TextUnformatted ( "Data Size:    " );
          ImGui::EndGroup        ();

          ImGui::SameLine   ();

          ImGui::BeginGroup ();
          ImGui::Text       ( "%lux%lu  (%lu %s)",
                                desc.Width, desc.Height,
                                   num_lods, num_lods > 1 ? "LODs" : "LOD" );
          ImGui::Text       ( "%hs",
                                SK_D3D9_FormatToStr (desc.Format).c_str () );
          ImGui::Text       ( "%.3f MiB",
                                (double)pTex->d3d9_tex->override_size / (1024.0f * 1024.0f) );
          ImGui::EndGroup   ();

          ImGui::TextColored (ImVec4 (1.0f, 1.0f, 1.0f, 1.0f), injected ? "Injected Texture" :
                                                                          "Resampled Texture" );

          ImGui::Separator  ();

          static bool flip_vertical1   = false;
          static bool flip_horizontal1 = false;

          ImGui::Checkbox ("Flip Vertically##D3D9_FlipVertical1",     &flip_vertical1);   ImGui::SameLine ( );
          ImGui::Checkbox ("Flip Horizontally##D3D9_FlipHorizontal1", &flip_horizontal1);

          if (injected)
          {
            if ( ImGui::Button ("  Reload This Texture  ") && tex_mgr.reloadTexture (debug_tex_id) )
            {
              reloading = true;

              tex_mgr.updateOSD ();
            }
          }

          else {
            ImGui::Button ("  Resample This Texture  "); // NO-OP, but preserves alignment :P
          }

          if (! reloading)
          {
            ImVec2 uv0 (flip_horizontal1 ? 1.0f : 0.0f, flip_vertical1 ? 1.0f : 0.0f);
            ImVec2 uv1 (flip_horizontal1 ? 0.0f : 1.0f, flip_vertical1 ? 0.0f : 1.0f);

            ImGui::PushStyleColor  (ImGuiCol_Border, ImVec4 (0.95f, 0.95f, 0.05f, 1.0f));
            ImGui::BeginChildFrame (ImGui::GetID ("ChildFrame_YYY"), ImVec2 ((float)desc.Width + 8, (float)desc.Height + 8),
                                    ImGuiWindowFlags_NoInputs    |
                                    ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoNavInputs | ImGuiWindowFlags_AlwaysAutoResize);
            ImGui::Image           ( pTex->d3d9_tex->pTexOverride,
                                       ImVec2 ((float)desc.Width, (float)desc.Height),
                                       uv0,                       uv1,
                                       ImColor (255,255,255,255), ImColor (255,255,255,128)
                                   );
            ImGui::EndChildFrame   ( );
            ImGui::PopStyleColor   ( );
          }

          ImGui::EndChild          ( );
          ImGui::EndGroup          ( );
        }
      }
    }
    ImGui::EndGroup                ( );
    ImGui::PopStyleVar             (2);
  }

  if (ImGui::CollapsingHeader ("Live Render Target View"))
  {
    SK::D3D9::TextureManager& tex_mgr =
      SK_D3D9_GetTextureManager ();

    static float last_ht    = 256.0f;
    static float last_width = 256.0f;

    static std::vector <std::string> list_contents;
    static bool                      list_dirty     = true;
    static uintptr_t                 last_sel_ptr   =    0;
    static int                            sel       =   -1;

    std::vector <IDirect3DBaseTexture9*> render_textures;
           tex_mgr.getUsedRenderTargets (render_textures);

    _tracked_rt.tracking_tex = nullptr;

    if (list_dirty)
    {
          sel = -1;
      int idx =  0;
          list_contents.clear ();

      // The underlying list is unsorted for speed, but that's not at all
      //   intuitive to humans, so sort the thing when we have the RT view open.
      std::sort ( render_textures.begin (),
                  render_textures.end   (),
        []( IDirect3DBaseTexture9 *a,
            IDirect3DBaseTexture9 *b )
        {
          return (uintptr_t)a < (uintptr_t)b;
        }
      );


      for ( auto& it : render_textures )
      {
        char szDesc [16] = { };

        sprintf (szDesc, "%" PRIxPTR, (uintptr_t)it);

        list_contents.emplace_back (szDesc);

        if ((uintptr_t)it == last_sel_ptr)
        {
          sel = idx;
          _tracked_rt.tracking_tex = render_textures [sel];
        }

        ++idx;
      }
    }

    ImGui::PushStyleVar   (ImGuiStyleVar_ChildRounding, 0.0f);
    ImGui::PushStyleColor (ImGuiCol_Border, ImVec4 (0.9f, 0.7f, 0.5f, 1.0f));

    ImGui::BeginChild ( ImGui::GetID ("Item List2"),
                        ImVec2 ( font_size * 6.0f, std::max (font_size * 15.0f, last_ht)),
                          true, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NavFlattened );

    if (ImGui::IsWindowHovered ())
      can_scroll = false;

    if (! render_textures.empty ())
    {
      static      int last_sel = 0;
      static bool sel_changed  = false;
      
      if (sel != last_sel)
        sel_changed = true;

      last_sel = sel;

      for ( int line = 0; line < sk::narrow_cast <int> (render_textures.size ()); line++ )
      {
        SK_ComPtr <IDirect3DTexture9>                                         pTex;
        if (SUCCEEDED (render_textures [line]->QueryInterface (IID_PPV_ARGS (&pTex.p))))
        {
          D3DSURFACE_DESC                        desc = { };
          if (SUCCEEDED (pTex->GetLevelDesc (0, &desc)))
          {
            if (line == sel)
            {
              bool selected = true;
              ImGui::Selectable (list_contents [line].c_str (), &selected);
      
              if (sel_changed)
              {
                ImGui::SetScrollHereY (0.5f); // 0.0f:top, 0.5f:center, 1.0f:bottom
                ImGui::SetKeyboardFocusHere ( );
      
                sel_changed = false;
              }
            }
      
            else
            {
              bool selected = false;
      
              if (ImGui::Selectable (list_contents [line].c_str (), &selected))
              {
                 sel_changed  = true;
                 sel          =  line;
                 last_sel_ptr = (uintptr_t)render_textures [sel];
                _tracked_rt.tracking_tex = render_textures [sel];
              }
            }
          }
        }
      }
    }
    ImGui::EndChild      ();

    ImGui::PopStyleColor ();
    ImGui::PopStyleVar   ();

    ImGui::BeginGroup    ();

    SK_ComPtr <IDirect3DTexture9> pTex = nullptr;
    
    if ((! render_textures.empty ()) && sel >= 0)
           render_textures [sel]->QueryInterface (IID_PPV_ARGS (&pTex.p));
    
    if (pTex != nullptr)
    {
      D3DSURFACE_DESC                        desc = { };
      if (SUCCEEDED (pTex->GetLevelDesc (0, &desc)))
      {
        size_t shaders =
          std::max ( _tracked_rt.pixel_shaders.size  (),
                     _tracked_rt.vertex_shaders.size () );

        // Some Render Targets are MASSIVE, let's try to keep the damn things on the screen ;)
        const float effective_width  = std::min (0.75f * ImGui::GetIO ().DisplaySize.x, static_cast <float> (desc.Width)  / 2.0f);
        const float effective_height = std::min (0.75f * ImGui::GetIO ().DisplaySize.y, static_cast <float> (desc.Height) / 2.0f);

        ImGui::SameLine   ();

        ImGui::PushStyleColor
                          (ImGuiCol_Border, ImVec4 (0.5f, 0.5f, 0.5f, 1.0f) );
        ImGui::BeginChild (ImGui::GetID ("Item Selection3"),
                           ImVec2 ( std::max (font_size * 30.0f, effective_width  + 24.0f),
                                    std::max (256.0f,            effective_height + font_size * 4.0f + (float)shaders * font_size) ),
                             true,
                               ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NavFlattened);

        last_width  = effective_width;
        last_ht     = effective_height + font_size * 4.0f + (float)shaders * font_size;

        //extern std::wstring
        //SK_D3D9_FormatToStr (D3DFORMAT Format, bool include_ordinal = true);


        ImGui::Text ( "Dimensions:   %lux%lu",
                        desc.Width, desc.Height/*,
                          pTex->d3d9_tex->GetLevelCount ()*/ );
        ImGui::Text ( "Format:       %hs",
                        SK_D3D9_FormatToStr (desc.Format).c_str () );

        ImGui::Separator     ( );

        ImGui::PushStyleColor  (ImGuiCol_Border, ImVec4 (0.95f, 0.95f, 0.05f, 1.0f));
        ImGui::BeginChildFrame (ImGui::GetID ("ChildFrame_ZZZ"), ImVec2 (effective_width + 8.0f, effective_height + 8.0f),
                                  ImGuiWindowFlags_NoInputs    | ImGuiWindowFlags_NoNavInputs |
                                  ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_AlwaysAutoResize );
        ImGui::Image           ( pTex,
                                   ImVec2 (effective_width, effective_height),
                                     ImVec2  (0,0),             ImVec2  (1,1),
                                     ImColor (255,255,255,255), ImColor (255,255,255,128)
                               );
        ImGui::EndChildFrame ( );
        ImGui::PopStyleColor ( );

        if (shaders > 0)
        {
          ImGui::Columns     (2);

          for ( auto it : _tracked_rt.vertex_shaders )
            ImGui::Text ("Vertex Shader: %08x", it);

          ImGui::NextColumn  ( );

          for ( auto it : _tracked_rt.pixel_shaders )
            ImGui::Text ("Pixel Shader: %08x", it);

          ImGui::Columns     (1);
        }

        ImGui::EndChild      ( );
        ImGui::PopStyleColor ( );
      }
    }
    ImGui::EndGroup          ( );
  }

  if (ImGui::CollapsingHeader ("Live Shader View"))
  {
    ImGui::TreePush ("");

    if (ImGui::CollapsingHeader ("Pixel Shaders"))
    {
      std::scoped_lock <SK_Thread_HybridSpinlock>
            scope_lock (*lock_ps);

      SK_D3D9_LiveShaderClassView (SK::D3D9::ShaderClass::Pixel, can_scroll);
    }

    if (ImGui::CollapsingHeader ("Vertex Shaders"))
    {
      std::scoped_lock <SK_Thread_HybridSpinlock>
            scope_lock (*lock_vs);

      SK_D3D9_LiveShaderClassView (SK::D3D9::ShaderClass::Vertex, can_scroll);
    }

    ImGui::TreePop    ( );
  }

  if (ImGui::CollapsingHeader ("Live Vertex Buffer View"))
  {
    ImGui::TreePush  ("");

    if (ImGui::CollapsingHeader ("Stream 0"))
    {
      std::scoped_lock <SK_Thread_HybridSpinlock>
            scope_lock (*lock_vb);

      SK_LiveVertexStreamView (can_scroll);
    }

    ImGui::TreePop    ( );
  }

  if (ImGui::CollapsingHeader ("Misc. Settings"))
  {
    ImGui::TreePush  ("");
    //if (ImGui::Checkbox ("Dump ALL Shaders   (TBFix_Res\\dump\\shaders\\<ps|vs>_<checksum>.html)", &config.render.dump_shaders)) need_reset.graphics = true;
    ImGui::Checkbox (R"(Dump ALL Textures at Load  (<ResourceRoot>\dump\textures\<format>\*.dds))", &config.textures.dump_on_load);//) need_reset.graphics = true;

    if (ImGui::IsItemHovered ())
    {
      ImGui::BeginTooltip ();
      ImGui::Text         ("Enabling this will cause the game to run slower and waste disk space, only enable if you know what you are doing.");
      ImGui::EndTooltip   ();
    }

    ImGui::TreePop    ( );
  }

  ImGui::PopItemWidth ( );

  if (can_scroll)
    ImGui::SetScrollY (ImGui::GetScrollY () + 5.0f * ImGui::GetFont ()->FontSize * -ImGui::GetIO ().MouseWheel);

  ImGui::End          ( );

  std::scoped_lock <SK_Thread_HybridSpinlock, SK_Thread_HybridSpinlock,
                                              SK_Thread_HybridSpinlock>
        scope_lock (*lock_vs, *lock_ps, *lock_vb);

  _tracked_ps.clear (); _tracked_vs.clear (); _last_frame.clear ();
  _tracked_rt.clear (); _tracked_vb.clear (); _known_objs.clear ();

  return show_dlg;
}





std::string
SK_D3D9_UsageToStr (DWORD dwUsage)
{
  std::string usage;

  if (dwUsage & D3DUSAGE_RENDERTARGET)
    usage += "RenderTarget ";

  if (dwUsage & D3DUSAGE_DEPTHSTENCIL)
    usage += "Depth/Stencil ";

  if (dwUsage & D3DUSAGE_DYNAMIC)
    usage += "Dynamic ";

  if (dwUsage & D3DUSAGE_AUTOGENMIPMAP)
    usage += "Auto-Gen Mipmaps ";

  if (dwUsage & D3DUSAGE_DMAP)
    usage += "Displacement Map ";

  if (dwUsage & D3DUSAGE_WRITEONLY)
    usage += "Write-Only ";

  if (dwUsage & D3DUSAGE_SOFTWAREPROCESSING)
    usage += "Software Processing ";

  if (dwUsage & D3DUSAGE_DONOTCLIP)
    usage += "Do Not Clip ";

  if (dwUsage & D3DUSAGE_POINTS)
    usage += "Points ";

  if (dwUsage & D3DUSAGE_RTPATCHES)
    usage += "RT Patches ";

  if (dwUsage & D3DUSAGE_NPATCHES)
    usage += "N Patches ";

  if (usage.empty ())
    usage = "None ";

  return usage;
}

INT
__stdcall
SK_D3D9_BytesPerPixel (D3DFORMAT Format)
{
  switch (Format)
  {
    case D3DFMT_UNKNOWN:
      return 0;

    case D3DFMT_R8G8B8:       return 3;
    case D3DFMT_A8R8G8B8:     return 4;
    case D3DFMT_X8R8G8B8:     return 4;
    case D3DFMT_R5G6B5:       return 2;
    case D3DFMT_X1R5G5B5:     return 2;
    case D3DFMT_A1R5G5B5:     return 2;
    case D3DFMT_A4R4G4B4:     return 2;
    case D3DFMT_R3G3B2:       return 8;
    case D3DFMT_A8:           return 1;
    case D3DFMT_A8R3G3B2:     return 2;
    case D3DFMT_X4R4G4B4:     return 2;
    case D3DFMT_A2B10G10R10:  return 4;
    case D3DFMT_A8B8G8R8:     return 4;
    case D3DFMT_X8B8G8R8:     return 4;
    case D3DFMT_G16R16:       return 4;
    case D3DFMT_A2R10G10B10:  return 4;
    case D3DFMT_A16B16G16R16: return 8;
    case D3DFMT_A8P8:         return 2;
    case D3DFMT_P8:           return 1;
    case D3DFMT_L8:           return 1;
    case D3DFMT_A8L8:         return 2;
    case D3DFMT_A4L4:         return 1;
    case D3DFMT_V8U8:         return 2;
    case D3DFMT_L6V5U5:       return 2;
    case D3DFMT_X8L8V8U8:     return 4;
    case D3DFMT_Q8W8V8U8:     return 4;
    case D3DFMT_V16U16:       return 4;
    case D3DFMT_A2W10V10U10:  return 4;

#if 0
    case D3DFMT_UYVY                 :
      return std::string ("FourCC 'UYVY'");
    case D3DFMT_R8G8_B8G8            :
      return std::string ("FourCC 'RGBG'");
    case D3DFMT_YUY2                 :
      return std::string ("FourCC 'YUY2'");
    case D3DFMT_G8R8_G8B8            :
      return std::string ("FourCC 'GRGB'");
#endif
    case D3DFMT_DXT1:          return -1;
    case D3DFMT_DXT2:          return -2;
    case D3DFMT_DXT3:          return -2;
    case D3DFMT_DXT4:          return -1;
    case D3DFMT_DXT5:          return -2;

    case D3DFMT_D16_LOCKABLE:  return  2;
    case D3DFMT_D32:           return  4;
    case D3DFMT_D15S1:         return  2;
    case D3DFMT_D24S8:         return  4;
    case D3DFMT_D24X8:         return  4;
    case D3DFMT_D24X4S4:       return  4;
    case D3DFMT_D16:           return  2;
    case D3DFMT_D32F_LOCKABLE: return  4;
    case D3DFMT_D24FS8:        return  4;

/* D3D9Ex only -- */
#if !defined(D3D_DISABLE_9EX)

    /* Z-Stencil formats valid for CPU access */
    case D3DFMT_D32_LOCKABLE:  return 4;
    case D3DFMT_S8_LOCKABLE:   return 1;
#endif // !D3D_DISABLE_9EX



    case D3DFMT_L16:           return 2;

#if 0
    case D3DFMT_VERTEXDATA           :
      return std::string ("VERTEXDATA") +
                (include_ordinal ? " (100)" : "");
#endif
    case D3DFMT_INDEX16:       return 2;
    case D3DFMT_INDEX32:       return 4;

    case D3DFMT_Q16W16V16U16:  return 8;

#if 0
    case D3DFMT_MULTI2_ARGB8         :
      return std::string ("FourCC 'MET1'");
#endif

    // Floating point surface formats

    // s10e5 formats (16-bits per channel)
    case D3DFMT_R16F:          return 2;
    case D3DFMT_G16R16F:       return 4;
    case D3DFMT_A16B16G16R16F: return 8;

    // IEEE s23e8 formats (32-bits per channel)
    case D3DFMT_R32F:          return 4;
    case D3DFMT_G32R32F:       return 8;
    case D3DFMT_A32B32G32R32F: return 16;

#if 0
    case D3DFMT_CxV8U8               :
      return std::string ("CxV8U8") +
                (include_ordinal ? " (117)" : "");
#endif

/* D3D9Ex only -- */
#if !defined(D3D_DISABLE_9EX)

    // Monochrome 1 bit per pixel format
    case D3DFMT_A1:            return -8;

#if 0
    // 2.8 biased fixed point
    case D3DFMT_A2B10G10R10_XR_BIAS  :
      return std::string ("A2B10G10R10_XR_BIAS") +
                (include_ordinal ? " (119)" : "");
#endif


#if 0
    // Binary format indicating that the data has no inherent type
    case D3DFMT_BINARYBUFFER         :
      return std::string ("BINARYBUFFER") +
                (include_ordinal ? " (199)" : "");
#endif

#endif // !D3D_DISABLE_9EX
/* -- D3D9Ex only */
  }

  return 0;
}

std::string
SK_D3D9_SwapEffectToStr (D3DSWAPEFFECT Effect)
{
  switch (Effect)
  {
    case D3DSWAPEFFECT_COPY:
      return std::string ("Copy");
    case D3DSWAPEFFECT_FLIP:
      return std::string ("Flip");
    case D3DSWAPEFFECT_DISCARD:
      return std::string ("Discard");
    case D3DSWAPEFFECT_OVERLAY:
      return std::string ("Overlay");
    case D3DSWAPEFFECT_FLIPEX:
      return std::string ("FlipEx");
    default:
      return "UNKNOWN";
  }
};

std::string
SK_D3D9_PresentParameterFlagsToStr (DWORD dwFlags)
{
  std::string out = "";

  if (dwFlags & D3DPRESENTFLAG_LOCKABLE_BACKBUFFER)
    out += "D3DPRESENTFLAG_LOCKABLE_BACKBUFFER  ";

  if (dwFlags & D3DPRESENTFLAG_DISCARD_DEPTHSTENCIL)
    out += "D3DPRESENTFLAG_DISCARD_DEPTHSTENCIL  ";

  if (dwFlags & D3DPRESENTFLAG_DEVICECLIP)
    out += "D3DPRESENTFLAG_DEVICECLIP  ";

  if (dwFlags & D3DPRESENTFLAG_VIDEO)
    out += "D3DPRESENTFLAG_VIDEO  ";

  if (dwFlags & D3DPRESENTFLAG_NOAUTOROTATE)
    out += "D3DPRESENTFLAG_NOAUTOROTATE  ";

  if (dwFlags & D3DPRESENTFLAG_UNPRUNEDMODE)
    out += "D3DPRESENTFLAG_UNPRUNEDMODE  ";

  if (dwFlags & D3DPRESENTFLAG_OVERLAY_LIMITEDRGB)
    out += "D3DPRESENTFLAG_OVERLAY_LIMITEDRGB  ";

  if (dwFlags & D3DPRESENTFLAG_OVERLAY_YCbCr_BT709)
    out += "D3DPRESENTFLAG_OVERLAY_YCbCr_BT709  ";

  if (dwFlags & D3DPRESENTFLAG_OVERLAY_YCbCr_xvYCC)
    out += "D3DPRESENTFLAG_OVERLAY_YCbCr_xvYCC  ";

  if (dwFlags & D3DPRESENTFLAG_RESTRICTED_CONTENT)
    out += "D3DPRESENTFLAG_RESTRICTED_CONTENT  ";

  if (dwFlags & D3DPRESENTFLAG_RESTRICT_SHARED_RESOURCE_DRIVER)
    out += "D3DPRESENTFLAG_RESTRICT_SHARED_RESOURCE_DRIVER  ";

  return out;
};

std::string
SK_D3D9_FormatToStr (D3DFORMAT Format, bool include_ordinal)
{
  switch (Format)
  {
    case D3DFMT_UNKNOWN:
      return std::string ("Unknown") + (include_ordinal ? " (0)" :
                                                          "");

    case D3DFMT_R8G8B8:
      return std::string ("R8G8B8")   +
                (include_ordinal ? " (20)" : "");
    case D3DFMT_A8R8G8B8:
      return std::string ("A8R8G8B8") +
                (include_ordinal ? " (21)" : "");
    case D3DFMT_X8R8G8B8:
      return std::string ("X8R8G8B8") +
                (include_ordinal ? " (22)" : "");
    case D3DFMT_R5G6B5               :
      return std::string ("R5G6B5")   +
                (include_ordinal ? " (23)" : "");
    case D3DFMT_X1R5G5B5             :
      return std::string ("X1R5G5B5") +
                (include_ordinal ? " (24)" : "");
    case D3DFMT_A1R5G5B5             :
      return std::string ("A1R5G5B5") +
                (include_ordinal ? " (25)" : "");
    case D3DFMT_A4R4G4B4             :
      return std::string ("A4R4G4B4") +
                (include_ordinal ? " (26)" : "");
    case D3DFMT_R3G3B2               :
      return std::string ("R3G3B2")   +
                (include_ordinal ? " (27)" : "");
    case D3DFMT_A8                   :
      return std::string ("A8")       +
                (include_ordinal ? " (28)" : "");
    case D3DFMT_A8R3G3B2             :
      return std::string ("A8R3G3B2") +
                (include_ordinal ? " (29)" : "");
    case D3DFMT_X4R4G4B4             :
      return std::string ("X4R4G4B4") +
                (include_ordinal ? " (30)" : "");
    case D3DFMT_A2B10G10R10          :
      return std::string ("A2B10G10R10") +
                (include_ordinal ? " (31)" : "");
    case D3DFMT_A8B8G8R8             :
      return std::string ("A8B8G8R8") +
                (include_ordinal ? " (32)" : "");
    case D3DFMT_X8B8G8R8             :
      return std::string ("X8B8G8R8") +
                (include_ordinal ? " (33)" : "");
    case D3DFMT_G16R16               :
      return std::string ("G16R16") +
                (include_ordinal ? " (34)" : "");
    case D3DFMT_A2R10G10B10          :
      return std::string ("A2R10G10B10") +
                (include_ordinal ? " (35)" : "");
    case D3DFMT_A16B16G16R16         :
      return std::string ("A16B16G16R16") +
                (include_ordinal ? " (36)" : "");

    case D3DFMT_A8P8                 :
      return std::string ("A8P8") +
                (include_ordinal ? " (40)" : "");
    case D3DFMT_P8                   :
      return std::string ("P8") +
                (include_ordinal ? " (41)" : "");

    case D3DFMT_L8                   :
      return std::string ("L8") +
                (include_ordinal ? " (50)" : "");
    case D3DFMT_A8L8                 :
      return std::string ("A8L8") +
                (include_ordinal ? " (51)" : "");
    case D3DFMT_A4L4                 :
      return std::string ("A4L4") +
                (include_ordinal ? " (52)" : "");

    case D3DFMT_V8U8                 :
      return std::string ("V8U8") +
                (include_ordinal ? " (60)" : "");
    case D3DFMT_L6V5U5               :
      return std::string ("L6V5U5") +
                (include_ordinal ? " (61)" : "");
    case D3DFMT_X8L8V8U8             :
      return std::string ("X8L8V8U8") +
                (include_ordinal ? " (62)" : "");
    case D3DFMT_Q8W8V8U8             :
      return std::string ("Q8W8V8U8") +
                (include_ordinal ? " (63)" : "");
    case D3DFMT_V16U16               :
      return std::string ("V16U16") +
                (include_ordinal ? " (64)" : "");
    case D3DFMT_A2W10V10U10          :
      return std::string ("A2W10V10U10") +
                (include_ordinal ? " (67)" : "");

    case D3DFMT_UYVY                 :
      return std::string ("FourCC 'UYVY'");
    case D3DFMT_R8G8_B8G8            :
      return std::string ("FourCC 'RGBG'");
    case D3DFMT_YUY2                 :
      return std::string ("FourCC 'YUY2'");
    case D3DFMT_G8R8_G8B8            :
      return std::string ("FourCC 'GRGB'");
    case D3DFMT_DXT1                 :
      return std::string ("DXT1");
    case D3DFMT_DXT2                 :
      return std::string ("DXT2");
    case D3DFMT_DXT3                 :
      return std::string ("DXT3");
    case D3DFMT_DXT4                 :
      return std::string ("DXT4");
    case D3DFMT_DXT5                 :
      return std::string ("DXT5");

    case D3DFMT_D16_LOCKABLE         :
      return std::string ("D16_LOCKABLE") +
                (include_ordinal ? " (70)" : "");
    case D3DFMT_D32                  :
      return std::string ("D32") +
                (include_ordinal ? " (71)" : "");
    case D3DFMT_D15S1                :
      return std::string ("D15S1") +
                (include_ordinal ? " (73)" : "");
    case D3DFMT_D24S8                :
      return std::string ("D24S8") +
                (include_ordinal ? " (75)" : "");
    case D3DFMT_D24X8                :
      return std::string ("D24X8") +
                (include_ordinal ? " (77)" : "");
    case D3DFMT_D24X4S4              :
      return std::string ("D24X4S4") +
                (include_ordinal ? " (79)" : "");
    case D3DFMT_D16                  :
      return std::string ("D16") +
                (include_ordinal ? " (80)" : "");

    case D3DFMT_D32F_LOCKABLE        :
      return std::string ("D32F_LOCKABLE") +
                (include_ordinal ? " (82)" : "");
    case D3DFMT_D24FS8               :
      return std::string ("D24FS8") +
                (include_ordinal ? " (83)" : "");

/* D3D9Ex only -- */
#if !defined(D3D_DISABLE_9EX)

    /* Z-Stencil formats valid for CPU access */
    case D3DFMT_D32_LOCKABLE         :
      return std::string ("D32_LOCKABLE") +
                (include_ordinal ? " (84)" : "");
    case D3DFMT_S8_LOCKABLE          :
      return std::string ("S8_LOCKABLE") +
                (include_ordinal ? " (85)" : "");

#endif // !D3D_DISABLE_9EX



    case D3DFMT_L16                  :
      return std::string ("L16") +
                (include_ordinal ? " (81)" : "");

    case D3DFMT_VERTEXDATA           :
      return std::string ("VERTEXDATA") +
                (include_ordinal ? " (100)" : "");
    case D3DFMT_INDEX16              :
      return std::string ("INDEX16") +
                (include_ordinal ? " (101)" : "");
    case D3DFMT_INDEX32              :
      return std::string ("INDEX32") +
                (include_ordinal ? " (102)" : "");

    case D3DFMT_Q16W16V16U16         :
      return std::string ("Q16W16V16U16") +
                (include_ordinal ? " (110)" : "");

    case D3DFMT_MULTI2_ARGB8         :
      return std::string ("FourCC 'MET1'");

    // Floating point surface formats

    // s10e5 formats (16-bits per channel)
    case D3DFMT_R16F                 :
      return std::string ("R16F") +
                (include_ordinal ? " (111)" : "");
    case D3DFMT_G16R16F              :
      return std::string ("G16R16F") +
                (include_ordinal ? " (112)" : "");
    case D3DFMT_A16B16G16R16F        :
      return std::string ("A16B16G16R16F") +
               (include_ordinal ? " (113)" : "");

    // IEEE s23e8 formats (32-bits per channel)
    case D3DFMT_R32F                 :
      return std::string ("R32F") +
                (include_ordinal ? " (114)" : "");
    case D3DFMT_G32R32F              :
      return std::string ("G32R32F") +
                (include_ordinal ? " (115)" : "");
    case D3DFMT_A32B32G32R32F        :
      return std::string ("A32B32G32R32F") +
                (include_ordinal ? " (116)" : "");

    case D3DFMT_CxV8U8               :
      return std::string ("CxV8U8") +
                (include_ordinal ? " (117)" : "");

/* D3D9Ex only -- */
#if !defined(D3D_DISABLE_9EX)

    // Monochrome 1 bit per pixel format
    case D3DFMT_A1                   :
      return std::string ("A1") +
                (include_ordinal ? " (118)" : "");

    // 2.8 biased fixed point
    case D3DFMT_A2B10G10R10_XR_BIAS  :
      return std::string ("A2B10G10R10_XR_BIAS") +
                (include_ordinal ? " (119)" : "");


    // Binary format indicating that the data has no inherent type
    case D3DFMT_BINARYBUFFER         :
      return std::string ("BINARYBUFFER") +
                (include_ordinal ? " (199)" : "");

#endif // !D3D_DISABLE_9EX
/* -- D3D9Ex only */
  }

  return SK_FormatString ("UNKNOWN?! (%d)", Format);
}

const char*
SK_D3D9_PoolToStr (D3DPOOL pool)
{
  switch (pool)
  {
    case D3DPOOL_DEFAULT:
      return "    Default   (0)";
    case D3DPOOL_MANAGED:
      return "    Managed   (1)";
    case D3DPOOL_SYSTEMMEM:
      return "System Memory (2)";
    case D3DPOOL_SCRATCH:
      return "   Scratch    (3)";
    default:
      return "   UNKNOWN?!     ";
  }
}

void
SK_D3D9_DumpShader ( const wchar_t* wszPrefix,
                           uint32_t crc32c,
                           LPVOID   pbFunc )
{
  static bool dump = false;//config.render.dump_shaders;

  /////if (dump)
  /////{
  /////  if (GetFileAttributes (L"TBFix_Res\\dump\\shaders") !=
  /////       FILE_ATTRIBUTE_DIRECTORY)
  /////  {
  /////    CreateDirectoryW (L"TBFix_Res",                nullptr);
  /////    CreateDirectoryW (L"TBFix_Res\\dump",          nullptr);
  /////    CreateDirectoryW (L"TBFix_Res\\dump\\shaders", nullptr);
  /////  }
  /////
  /////  wchar_t wszDumpName [MAX_PATH + 2] = { };
  /////
  /////  swprintf_s ( wszDumpName,
  /////                 MAX_PATH,
  /////                   L"TBFix_Res\\dump\\shaders\\%s_%08x.html",
  /////                     wszPrefix, crc32c );
  /////
  /////  if ( GetFileAttributes (wszDumpName) == INVALID_FILE_ATTRIBUTES )
  /////  {
  /////    SK_ComPtr <ID3DXBuffer> pDisasm = nullptr;
  /////
  /////    HRESULT hr =
  /////      D3DXDisassembleShader ((DWORD *)pbFunc, TRUE, "", &pDisasm);
  /////
  /////    if (SUCCEEDED (hr))
  /////    {
  /////      FILE* fDump = _wfsopen (wszDumpName,  L"wb", _SH_DENYWR);
  /////
  /////      if (fDump != NULL)
  /////      {
  /////        fwrite ( pDisasm->GetBufferPointer (),
  /////                   pDisasm->GetBufferSize  (),
  /////                     1,
  /////                       fDump );
  /////        fclose (fDump);
  /////      }
  /////    }
  /////  }
  /////}

  SK_ComPtr <ID3DXBuffer> pDisasm = nullptr;

  HRESULT hr =
    SK_D3DXDisassembleShader ((DWORD *)pbFunc, FALSE, "", &pDisasm.p);

  // Is this nul-terminated?
  if (SUCCEEDED (hr) &&      *(const char *)pDisasm->GetBufferPointer () != '\0')
  { char* szDisasm = _strdup ((const char *)pDisasm->GetBufferPointer ());

    char* comments_end  =                szDisasm != nullptr ?
                                         strstr (szDisasm,          "\n ") : nullptr;
    char* footer_begins = comments_end ? strstr (comments_end + 1, "\n\n") : nullptr;

    if (comments_end  != nullptr) *comments_end  = '\0'; else (comments_end  = (char *)"  ");
    if (footer_begins != nullptr) *footer_begins = '\0'; else (footer_begins = (char *)"  ");

    if ((! _wcsicmp (wszPrefix, L"ps")) && szDisasm != nullptr)
    {
      ps_disassembly->emplace ( crc32c, SK::D3D9::ShaderDisassembly {
                                         szDisasm,
                                           comments_end + 1,
                                             footer_begins + 1 }
                             );
    }

    else
    {
      vs_disassembly->emplace ( crc32c, SK::D3D9::ShaderDisassembly {
                                         szDisasm,
                                           comments_end + 1,
                                             footer_begins + 1 }
                             );
    }

    free (szDisasm);
  }
}

void
SK_D3D9_SetVertexShader ( IDirect3DDevice9*       /*pDev*/,
                          IDirect3DVertexShader9 *pShader )
{
  static auto& _Shaders    = Shaders.get    ();
  static auto& _last_frame = last_frame.get ();
  static auto& _tracked_rt = tracked_rt.get ();
  static auto& _tracked_vs = tracked_vs.get ();

  if (_Shaders.vertex.current.ptr != pShader)
  {
    if (pShader != nullptr)
    {
      lock_vs->lock ();

      if (_Shaders.vertex.rev.find (pShader) == _Shaders.vertex.rev.cend ())
      {
        lock_vs->unlock ();

        UINT len = 0;

        pShader->GetFunction (nullptr, &len);

        void* pbFunc =
          SK_TLS_Bottom ()->render->
                      d3d9->allocStackScratchStorage (len);

        if (pbFunc != nullptr)
        {
          pShader->GetFunction (pbFunc, &len);

          uint32_t checksum =
            safe_crc32c (0, pbFunc, len);

          SK_D3D9_DumpShader (L"vs", checksum, pbFunc);

          lock_vs->lock ();

          _Shaders.vertex.rev [pShader] = checksum;

          lock_vs->unlock ();
        }
      }

      else
        lock_vs->unlock ();
    }

    else
      _Shaders.vertex.current.crc32c = 0;
  }

  std::scoped_lock <SK_Thread_HybridSpinlock>
        scope_lock (*lock_vs);

  const uint32_t vs_checksum =
    _Shaders.vertex.rev [pShader];

  _Shaders.vertex.current.crc32c = vs_checksum;
  _Shaders.vertex.current.ptr    = pShader;


  if (vs_checksum != 0x00)
  {
    _last_frame.vertex_shaders.emplace (vs_checksum);

    if (_tracked_rt.active)
      _tracked_rt.vertex_shaders.emplace (vs_checksum);

    if (vs_checksum == _tracked_vs.crc32c)
    {
      _tracked_vs.use (pShader);

      for ( auto& current_texture : _tracked_vs.current_textures )
        current_texture = nullptr;
    }
  }
}

void
SK_D3D9_SetPixelShader ( IDirect3DDevice9*     /*pDev*/,
                         IDirect3DPixelShader9 *pShader )
{
  static auto& _Shaders    = Shaders.get    ();
  static auto& _last_frame = last_frame.get ();
  static auto& _tracked_rt = tracked_rt.get ();
  static auto& _tracked_ps = tracked_ps.get ();

  if (_Shaders.pixel.current.ptr != pShader)
  {
    if (pShader != nullptr)
    {
      lock_ps->lock ();

      if (_Shaders.pixel.rev.find (pShader) == _Shaders.pixel.rev.cend ())
      {
        lock_ps->unlock ();

        UINT len = 0;

        pShader->GetFunction (nullptr, &len);

        void* pbFunc =
          SK_TLS_Bottom ()->render->
                      d3d9->allocStackScratchStorage (len);

        if (pbFunc != nullptr)
        {
          pShader->GetFunction (pbFunc, &len);

          uint32_t checksum =
            safe_crc32c (0, pbFunc, len);

          SK_D3D9_DumpShader (L"ps", checksum, pbFunc);

          lock_ps->lock ();

          _Shaders.pixel.rev  [pShader] = checksum;

          lock_ps->unlock ();
        }
      }

      else
        lock_ps->unlock ();
    }

    else
      _Shaders.pixel.current.crc32c = 0x0;
  }


  std::scoped_lock <SK_Thread_HybridSpinlock>
        scope_lock (*lock_ps);

  const uint32_t ps_checksum =
    _Shaders.pixel.rev [pShader];

  _Shaders.pixel.current.crc32c = ps_checksum;
  _Shaders.pixel.current.ptr    = pShader;



  if (ps_checksum != 0x00)
  {
    _last_frame.pixel_shaders.emplace (ps_checksum);

    if (_tracked_rt.active)
      _tracked_rt.pixel_shaders.emplace (ps_checksum);

    if (ps_checksum == _tracked_ps.crc32c)
    {
      _tracked_ps.use (pShader);

      for ( auto& current_texture : _tracked_ps.current_textures )
        current_texture = nullptr;
    }
  }
}

void
SK_D3D9_EndScene (void)
{
  //
  //
  //
  //
  //
  //
  //
}

void
SK_D3D9_EndFrame (void)
{
  const SK_RenderBackend& rb =
    SK_GetCurrentRenderBackend ();

  static auto& _Shaders    = Shaders.get    ();
  static auto& _draw_state = draw_state.get ();

  //draw_state.last_vs     = 0;
  _draw_state.scene_count = 0;

  _draw_state.draw_count = 0;
  _draw_state.next_draw  = 0;

  _Shaders.vertex.clear_state ();
  _Shaders.pixel.clear_state  ();

  {
    std::scoped_lock <SK_Thread_HybridSpinlock>
          scope_lock (*lock_vs);
    last_frame->clear ();

//    for (auto& it : tracked_vs.used_textures) it->Release ();

    tracked_vs->clear ();
    tracked_vb->clear ();
    tracked_rt->clear ();
  }

  {
    std::scoped_lock <SK_Thread_HybridSpinlock>
          scope_lock (*lock_ps);

//    for (auto& it : tracked_ps.used_textures) it->Release ();

    tracked_ps->clear ();
  }

  SK::D3D9::TextureManager& tex_mgr =
    SK_D3D9_GetTextureManager ();

  if (trigger_reset == Clear)
  {
    if (tex_mgr.init)
    {
      if (tex_mgr.injector.hasPendingLoads ())
          tex_mgr.loadQueuedTextures ();

      tex_mgr.logUsedTextures ();
    }

    //auto pDev = rb.getDevice <IDirect3DDevice9> ();
    //if ( pDev == nullptr ||
    //    (pDev->GetAvailableTextureMem () / 1048576UL) < 64UL )
    SK_ComQIPtr <IDirect3DDevice9> pDev (rb.device);

    if (pDev == nullptr || (pDev->GetAvailableTextureMem () / 1048576UL) < 64UL)

    {
      SK_D3D9_TriggerReset (false);
    }
  }

  SK_Screenshot_ProcessQueue  (SK_ScreenshotStage::BeforeGameHUD, rb);
  SK_Screenshot_ProcessQueue  (SK_ScreenshotStage::BeforeOSD,     rb);
  SK_Screenshot_ProcessQueue  (SK_ScreenshotStage::EndOfFrame,    rb);
  SK_Screenshot_ProcessQueue  (SK_ScreenshotStage::ClipboardOnly, rb);
  SK_Screenshot_ProcessQueue  (SK_ScreenshotStage::_FlushQueue,   rb);

  if (tex_mgr.init)
      tex_mgr.resetUsedTextures ();
}

__declspec (noinline)
bool
SK_D3D9_ShouldSkipRenderPass (D3DPRIMITIVETYPE /*PrimitiveType*/, UINT/* PrimitiveCount*/, UINT /*StartVertex*/, bool& wireframe)
{
  const SK_RenderBackend& rb =
    SK_GetCurrentRenderBackend ();

  if (rb.device == nullptr)
    return false;

  auto pDevice    = rb.getDevice <IDirect3DDevice9> ();
  if ( pDevice.p == nullptr )
    return false;

  static auto& _Shaders    = Shaders.get    ();
  static auto& _tracked_vs = tracked_vs.get ();
  static auto& _tracked_ps = tracked_ps.get ();
  static auto& _tracked_vb = tracked_vb.get ();
  static auto& _draw_state = draw_state.get ();

  const uint32_t vs_checksum = _Shaders.vertex.current.crc32c;
  const uint32_t ps_checksum = _Shaders.pixel.current.crc32c;

  const bool tracking_vs = ( vs_checksum == _tracked_vs.crc32c );
  const bool tracking_ps = ( ps_checksum == _tracked_ps.crc32c );
  const bool tracking_vb = { vb_stream0  == _tracked_vb.vertex_buffer };


  bool skip = false;

  {
    std::scoped_lock <SK_Thread_HybridSpinlock>
          scope_lock (*lock_vs);

    if (_Shaders.vertex.blacklist.count (vs_checksum))
      skip      = true;

    if (_Shaders.vertex.wireframe.count (vs_checksum))
      wireframe = true;
  }

  {
    std::scoped_lock <SK_Thread_HybridSpinlock>
          scope_lock (*lock_ps);

    if (_Shaders.pixel.blacklist.count (ps_checksum))
      skip      = true;

    if (_Shaders.pixel.wireframe.count (ps_checksum))
      wireframe = true;
  }


  if (tracking_vs)
  {
    InterlockedIncrementAcquire (&_tracked_vs.num_draws);

    for (auto& current_texture : _tracked_vs.current_textures)
    {
      if (current_texture != nullptr)
      {
        if (! _tracked_vs.used_textures.count (current_texture))
        {
          _tracked_vs.used_textures.emplace (current_texture);
          //current_texture->AddRef ();
        }
      }
    }

    //
    // TODO: Make generic and move into class -- must pass shader type to function
    //
    for (auto& it : _tracked_vs.constants)
    {
      for (auto& it2 : it.struct_members)
      {
        if (it2.Override)
          pDevice->SetVertexShaderConstantF (it2.RegisterIndex, it2.Data, 1);
      }

      if (it.Override)
        pDevice->SetVertexShaderConstantF (it.RegisterIndex, it.Data, 1);
    }
  }

  if (tracking_ps)
  {
    std::scoped_lock <SK_Thread_HybridSpinlock>
          scope_lock (*lock_ps);

    InterlockedIncrementAcquire (&_tracked_ps.num_draws);

    for ( auto& current_texture : _tracked_ps.current_textures )
    {
      if (current_texture != nullptr)
      {
        if (! _tracked_ps.used_textures.count (current_texture))
        {
          _tracked_ps.used_textures.emplace (current_texture);
          //current_texture->AddRef ();
        }
      }
    }

    //
    // TODO: Make generic and move into class -- must pass shader type to function
    //
    for (auto& it : _tracked_ps.constants)
    {
      for (auto& it2 : it.struct_members)
      {
        if (it2.Override)
          pDevice->SetPixelShaderConstantF (it2.RegisterIndex, it2.Data, 1);
      }

      if (it.Override)
        pDevice->SetPixelShaderConstantF (it.RegisterIndex, it.Data, 1);
    }
  }


  if (_tracked_vb.wireframes.count (vb_stream0))
    wireframe = true;

  if (tracking_vb)
  {
    _tracked_vb.use ();

    _tracked_vb.instances  = _draw_state.instances;

    InterlockedExchangeAdd (&_tracked_vb.instanced, sk::narrow_cast <ULONG> (_draw_state.instances));
    InterlockedIncrement   (&_tracked_vb.num_draws);

    if (_tracked_vb.wireframe)
      wireframe = true;
  }


  if (skip)
    return true;


  if (tracking_vb && _tracked_vb.cancel_draws)
    return true;


  if (tracking_vs && _tracked_vs.cancel_draws)
    return true;

  if (tracking_ps && _tracked_ps.cancel_draws)
    return true;


  //last_vs = vs_checksum;


  if (wireframe)
    pDevice->SetRenderState (D3DRS_FILLMODE, D3DFILL_WIREFRAME);


  return false;
}

void
SK_D3D9_InitShaderModTools (void)
{
  static auto& _last_frame = last_frame.get ();
  static auto& _known_objs = known_objs.get ();
  static auto& _Shaders    = Shaders.get    ();

  _last_frame.vertex_shaders.reserve           (256);
  _last_frame.pixel_shaders.reserve            (256);
  _last_frame.vertex_buffers.dynamic.reserve   (128);
  _last_frame.vertex_buffers.immutable.reserve (256);
  _known_objs.dynamic_vbs.reserve              (2048);
  _known_objs.static_vbs.reserve               (8192);
   ps_disassembly->reserve                     (512);
   vs_disassembly->reserve                     (512);
  _Shaders.vertex.rev.reserve                  (8192);
  _Shaders.pixel.rev.reserve                   (8192);

  lock_vs = std::make_unique <SK_Thread_HybridSpinlock> (/*1024*/);
  lock_ps = std::make_unique <SK_Thread_HybridSpinlock> (/*512*/);
  lock_vb = std::make_unique <SK_Thread_HybridSpinlock> (/*64*/);
}



void
EnumConstant ( SK::D3D9::ShaderTracker           *pShader,
               ID3DXConstantTable                *pConstantTable,
               D3DXHANDLE                         hConstant,
               SK::D3D9::ShaderTracker::
               shader_constant_s&                 constant,
               std::vector <
                 SK::D3D9::ShaderTracker::
                             shader_constant_s >& list )
{
  if (! (hConstant && pConstantTable))
    return;

  UINT              one           =  1;
  D3DXCONSTANT_DESC constant_desc = { };

  if (SUCCEEDED (pConstantTable->GetConstantDesc (hConstant, &constant_desc, &one)))
  {
    strncpy (constant.Name, constant_desc.Name, 128);
    constant.Class         = constant_desc.Class;
    constant.Type          = constant_desc.Type;
    constant.RegisterSet   = constant_desc.RegisterSet;
    constant.RegisterIndex = constant_desc.RegisterIndex;
    constant.RegisterCount = constant_desc.RegisterCount;
    constant.Rows          = constant_desc.Rows;
    constant.Columns       = constant_desc.Columns;
    constant.Elements      = constant_desc.Elements;

    if (constant_desc.DefaultValue != nullptr)
    {
      memcpy ( constant.Data, constant_desc.DefaultValue,
                 std::min ( static_cast <size_t> (constant_desc.Bytes),
                              sizeof (float) * 4 )
             );
    }

    SK::D3D9::ShaderTracker::shader_constant_s
      struct_constant = { };

    for ( UINT j = 0; j < constant_desc.StructMembers; j++ )
    {
      D3DXHANDLE hConstantStruct =
        pConstantTable->GetConstant (hConstant, j);

      if (hConstantStruct != nullptr)
      {
        EnumConstant ( pShader, pConstantTable,
                                hConstantStruct,
                          struct_constant,
                                 constant.struct_members );
      }
    }

    list.emplace_back (constant);
  }
};



void
SK::D3D9::ShaderTracker::use (IUnknown *pShader)
{
  if (shader_obj != pShader)
  {
    std::scoped_lock <SK_Thread_HybridSpinlock, SK_Thread_HybridSpinlock>
          scope_lock (*lock_vs, *lock_ps);

    constants.clear ();

    shader_obj = pShader;

    UINT len = 0;

    if (SUCCEEDED (((IDirect3DVertexShader9 *)pShader)->GetFunction (nullptr, &len)))
    {
      void* pbFunc =
        SK_TLS_Bottom ()->render->
                    d3d9->allocStackScratchStorage (len);

      if (pbFunc != nullptr)
      {
        if ( SUCCEEDED ( ((IDirect3DVertexShader9 *)pShader)->GetFunction ( pbFunc,
                                                                              &len )
                       )
           )
        {
          SK_ComPtr <ID3DXConstantTable> pConstantTable = nullptr;

          if (SUCCEEDED (SK_D3DXGetShaderConstantTable ((DWORD *)pbFunc, &pConstantTable)))
          {
            D3DXCONSTANTTABLE_DESC ct_desc = { };

            if (SUCCEEDED (pConstantTable->GetDesc (&ct_desc)))
            {
              const UINT constant_count = ct_desc.Constants;

              for (UINT i = 0; i < constant_count; i++)
              {
                D3DXHANDLE hConstant =
                  pConstantTable->GetConstant (nullptr, i);

                shader_constant_s constant = { };

                EnumConstant (this, pConstantTable, hConstant, constant, constants);
              }
            }
          }
        }
      }
    }
  }
}




void
SK_D3D9_QuickHook (void)
{
  // We don't want to hook this, and we certainly don't want to hook it using
  //   cached addresses!
  if ( (   config.apis.translated == SK_RenderAPI::D3D9       ) ||
       (! (config.apis.d3d9.hook  || config.apis.d3d9ex.hook) ) )
    return;

  if (config.steam.preload_overlay)
    return;

  extern BOOL
      __SK_DisableQuickHook;
  if (__SK_DisableQuickHook)
    return;


  static volatile LONG quick_hooked = FALSE;

  if (! InterlockedCompareExchangeAcquire (&quick_hooked, TRUE, FALSE))
  {
    sk_hook_cache_enablement_s state =
      SK_Hook_PreCacheModule ( L"D3D9",
                                 local_d3d9_records,
                                   global_d3d9_records );

    if ( ( state.hooks_loaded.from_shared_dll +
           state.hooks_loaded.from_game_ini     ) > 0 )
    {
//#ifdef SK_AGGRESSIVE_HOOKS
      bool bEnable =
        SK_EnableApplyQueuedHooks ();

      SK_ApplyQueuedHooks ();

      if (! bEnable)
        SK_DisableApplyQueuedHooks ();
//#endif
    }

    else
    {
      for ( auto& it : local_d3d9_records )
      {
        it->active = false;
      }
    }

    InterlockedIncrementRelease (&quick_hooked);
  }

  SK_Thread_SpinUntilAtomicMin (&quick_hooked, 2);
}





//D3DFORMAT
//SK_D3D9_FormatFromDXGI (DXGI_FORMAT format)
//{
//
//}

DXGI_FORMAT
SK_D3D9_FormatToDXGI (D3DFORMAT format)
{
  switch (format)
  {
    case D3DFMT_UNKNOWN:
      return DXGI_FORMAT_UNKNOWN;

    // These may not be accurate, but work insofar as screenshots
    case D3DFMT_X8R8G8B8:     return DXGI_FORMAT_B8G8R8X8_UNORM;
    case D3DFMT_A8R8G8B8:     return DXGI_FORMAT_B8G8R8A8_UNORM;

    case D3DFMT_X8B8G8R8:     return DXGI_FORMAT_B8G8R8X8_UNORM; // ?


    case D3DFMT_R8G8B8:       return DXGI_FORMAT_R8G8B8A8_UNORM;
  //case D3DFMT_A8R8G8B8:     return DXGI_FORMAT_A8G8R8B8_UNORM;
  //case D3DFMT_X8R8G8B8:     return DXGI_FORMAT_X8R8G8B8_UNORM;
    case D3DFMT_R5G6B5:       return DXGI_FORMAT_B5G6R5_UNORM;
  //case D3DFMT_X1R5G5B5:     return DXGI_FORMAT_B5G6R5_UNORM;
    case D3DFMT_A1R5G5B5:     return DXGI_FORMAT_B5G5R5A1_UNORM;
    case D3DFMT_A4R4G4B4:     return DXGI_FORMAT_B4G4R4A4_UNORM; // Reversed
    case D3DFMT_A8:           return DXGI_FORMAT_A8_UNORM;
  //case D3DFMT_A8R3G3B2:     return ;
  //case D3DFMT_X4R4G4B4:     return 2;
    case D3DFMT_A2B10G10R10:  return DXGI_FORMAT_R10G10B10A2_UNORM;
    case D3DFMT_A8B8G8R8:     return DXGI_FORMAT_R8G8B8A8_UNORM;
  //case D3DFMT_X8B8G8R8:     return 4;
    case D3DFMT_G16R16:       return DXGI_FORMAT_R16G16_UNORM;

  //case D3DFMT_UYVY:           return DXGI_FORMAT_UYVY;
    case D3DFMT_R8G8_B8G8:      return DXGI_FORMAT_R8G8_B8G8_UNORM;
    case D3DFMT_YUY2:           return DXGI_FORMAT_YUY2;
    case D3DFMT_G8R8_G8B8:      return DXGI_FORMAT_G8R8_G8B8_UNORM;



    case D3DFMT_DXT1:          return DXGI_FORMAT_BC1_UNORM;
    case D3DFMT_DXT2:          return DXGI_FORMAT_BC2_UNORM;
    case D3DFMT_DXT3:          return DXGI_FORMAT_BC3_UNORM;
    case D3DFMT_DXT4:          return DXGI_FORMAT_BC4_UNORM;
    case D3DFMT_DXT5:          return DXGI_FORMAT_BC5_UNORM;

    case D3DFMT_D16_LOCKABLE:  return  DXGI_FORMAT_D16_UNORM;
    case D3DFMT_D24S8:         return  DXGI_FORMAT_D24_UNORM_S8_UINT;
    case D3DFMT_D24X8:         return  DXGI_FORMAT_D24_UNORM_S8_UINT;
    case D3DFMT_D16:           return  DXGI_FORMAT_D16_UNORM;
    case D3DFMT_D32F_LOCKABLE: return  DXGI_FORMAT_D32_FLOAT;
  //case D3DFMT_D24FS8:        return  DXGI_FORMAT_D24_UNORM_S8_UINT;


    case D3DFMT_L16:           return DXGI_FORMAT_R16_TYPELESS;

    // Floating point surface formats

    // s10e5 formats (16-bits per channel)
    case D3DFMT_R16F:          return DXGI_FORMAT_R16_FLOAT;
    case D3DFMT_G16R16F:       return DXGI_FORMAT_R16G16_FLOAT;
    case D3DFMT_A16B16G16R16F: return DXGI_FORMAT_R16G16B16A16_FLOAT;

    // IEEE s23e8 formats (32-bits per channel)
    case D3DFMT_R32F:          return DXGI_FORMAT_R32_FLOAT;
    case D3DFMT_G32R32F:       return DXGI_FORMAT_R32G32_FLOAT;
    case D3DFMT_A32B32G32R32F: return DXGI_FORMAT_R32G32B32A32_FLOAT;
  }

  return DXGI_FORMAT_UNKNOWN;
}
