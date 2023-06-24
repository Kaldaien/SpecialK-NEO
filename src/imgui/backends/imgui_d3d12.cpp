// dear imgui: Renderer for DirectX12
// This needs to be used along with a Platform Binding (e.g. Win32)

// Implemented features:
//  [X] Renderer: User texture binding. Use 'D3D12_GPU_DESCRIPTOR_HANDLE' as ImTextureID. Read the FAQ about ImTextureID in imgui.cpp.
// Issues:
//  [ ] 64-bit only for now! (Because sizeof(ImTextureId) == sizeof(void*)). See github.com/ocornut/imgui/pull/301

// You can copy and use unmodified imgui_impl_* files in your project. See main.cpp for an example of using this.
// If you are new to dear imgui, read examples/README.txt and read the documentation at the top of imgui.cpp.
// https://github.com/ocornut/imgui

// CHANGELOG
// (minor and older changes stripped away, please see git history for details)
//  2018-06-12: DirectX12: Moved the ID3D12GraphicsCommandList* parameter from NewFrame() to RenderDrawData().
//  2018-06-08: Misc: Extracted imgui_impl_dx12.cpp/.h away from the old combined DX12+Win32 example.
//  2018-06-08: DirectX12: Use draw_data->DisplayPos and draw_data->DisplaySize to setup projection matrix and clipping rectangle (to ease support for future multi-viewport).
//  2018-02-22: Merged into master with all Win32 code synchronized to other examples.

#include <SpecialK/stdafx.h>

#include <imgui/imgui.h>
#include <imgui/backends/imgui_d3d12.h>
#include <SpecialK/render/backend.h>
#include <SpecialK/render/dxgi/dxgi_backend.h>
#include <SpecialK/render/dxgi/dxgi_swapchain.h>
#include <SpecialK/render/d3d12/d3d12_device.h>
#include <SpecialK/render/d3d12/d3d12_dxil_shader.h>

#include <shaders/imgui_d3d11_vs.h>
#include <shaders/imgui_d3d11_ps.h>

#include <shaders/ps_gl_dx_interop.h>

#include <SpecialK/render/dxgi/dxgi_hdr.h>

#include <shaders/vs_colorutil.h>
#include <shaders/uber_hdr_shader_ps.h>

#include <DirectXTex/d3dx12.h>

// DirectX
//#include <dxgi1_4.h>

struct SK_ImGui_D3D12Ctx
{
  SK_ComPtr <ID3D12Device>        pDevice;
  SK_ComPtr <ID3D12RootSignature> pRootSignature;
  SK_ComPtr <ID3D12PipelineState> pPipelineState;

  DXGI_FORMAT                     RTVFormat             = DXGI_FORMAT_UNKNOWN;

  SK_ComPtr <ID3D12Resource>      pFontTexture;
  D3D12_CPU_DESCRIPTOR_HANDLE     hFontSrvCpuDescHandle = { };
  D3D12_GPU_DESCRIPTOR_HANDLE     hFontSrvGpuDescHandle = { };

  HWND                            hWndSwapChain         =   0;

  struct FrameHeap
  {
    struct buffer_s : SK_ComPtr <ID3D12Resource>
    { INT size;
    } Vb, Ib;
  } frame_heaps [DXGI_MAX_SWAP_CHAIN_BUFFERS];
} static _imgui_d3d12;

// A few ImGui pipeline states must never be disabled during render mod
extern concurrency::concurrent_unordered_set <ID3D12PipelineState *> _criticalVertexShaders;

HRESULT
WINAPI
D3D12SerializeRootSignature ( const D3D12_ROOT_SIGNATURE_DESC* pRootSignature,
                                    D3D_ROOT_SIGNATURE_VERSION Version,
                                    ID3DBlob**                 ppBlob,
                                    ID3DBlob**                 ppErrorBlob )
{
  static D3D12SerializeRootSignature_pfn
        _D3D12SerializeRootSignature =
        (D3D12SerializeRootSignature_pfn)
           SK_GetProcAddress ( L"d3d12.dll",
                                "D3D12SerializeRootSignature" );

  if (! _D3D12SerializeRootSignature)
    return E_NOTIMPL;

  return
    _D3D12SerializeRootSignature (pRootSignature, Version, ppBlob, ppErrorBlob);
}

struct VERTEX_CONSTANT_BUFFER
{
  float   mvp[4][4];

  // scRGB allows values > 1.0, sRGB (SDR) simply clamps them
  float luminance_scale [4]; // For HDR displays,    1.0 = 80 Nits
                             // For SDR displays, >= 1.0 = 80 Nits
  float steam_luminance [4];
};

void
ImGui_ImplDX12_RenderDrawData ( ImDrawData* draw_data,
              SK_D3D12_RenderCtx::FrameCtx* pFrame )
{
  if (! ( _d3d12_rbk->_pSwapChain.p || _d3d12_rbk->_pDevice.p ) )
    return;

  ImGuiIO& io =
    ImGui::GetIO ();

  // The cmd list is either closed and needs resetting, or just wants pipeline state.
  if (! pFrame->begin_cmd_list (/*g_PipelineState*/))
  {
    SK_ReleaseAssert (!"ImGui Frame Command List Could not be Reset");
    return;
  }

  auto _frame =
    _d3d12_rbk->_pSwapChain->GetCurrentBackBufferIndex ();

  SK_ReleaseAssert (_frame == pFrame->iBufferIdx);

  SK_ImGui_D3D12Ctx::FrameHeap* pHeap =
    &_imgui_d3d12.frame_heaps [pFrame->iBufferIdx];

  auto descriptorHeaps =
    pFrame->pRoot->descriptorHeaps;

  SK_ComPtr <ID3D12GraphicsCommandList> ctx =
    pFrame->pCmdList;

  bool sync_cmd_list =
    ( (! pHeap->Vb.p || pHeap->Vb.size < draw_data->TotalVtxCount) ||
      (! pHeap->Ib.p || pHeap->Ib.size < draw_data->TotalIdxCount) );

  // Creation, or reallocation of vtx / idx buffers required...
  if (sync_cmd_list)
  {
    if (! pFrame->wait_for_gpu ())
      return;

    try
    {
      if (! pHeap->Vb.p || pHeap->Vb.size < draw_data->TotalVtxCount)
      {
        auto overAlloc =
          draw_data->TotalVtxCount + 5000;

        pHeap->Vb.Release ();
        pHeap->Vb.size = 0;

        D3D12_HEAP_PROPERTIES
          props                      = { };
          props.Type                 = D3D12_HEAP_TYPE_UPLOAD;
          props.CPUPageProperty      = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
          props.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;

        D3D12_RESOURCE_DESC
          desc                  = { };
          desc.Dimension        = D3D12_RESOURCE_DIMENSION_BUFFER;
          desc.Width            = overAlloc * sizeof (ImDrawVert);
          desc.Height           = 1;
          desc.DepthOrArraySize = 1;
          desc.MipLevels        = 1;
          desc.Format           = DXGI_FORMAT_UNKNOWN;
          desc.SampleDesc.Count = 1;
          desc.Layout           = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
          desc.Flags            = D3D12_RESOURCE_FLAG_NONE;

        ThrowIfFailed (
          _imgui_d3d12.pDevice->CreateCommittedResource (
            &props, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_GENERIC_READ,
              nullptr, IID_PPV_ARGS (&pHeap->Vb.p)));
        SK_D3D12_SetDebugName (       pHeap->Vb.p,
          L"ImGui D3D12 VertexBuffer" + std::to_wstring (_frame));

        pHeap->Vb.size = overAlloc;
      }

      if (! pHeap->Ib.p || pHeap->Ib.size < draw_data->TotalIdxCount)
      {
        auto overAlloc =
          draw_data->TotalIdxCount + 10000;

        pHeap->Ib.Release ();
        pHeap->Ib.size = 0;

        D3D12_HEAP_PROPERTIES
          props                      = { };
          props.Type                 = D3D12_HEAP_TYPE_UPLOAD;
          props.CPUPageProperty      = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
          props.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;

        D3D12_RESOURCE_DESC
          desc                  = { };
          desc.Dimension        = D3D12_RESOURCE_DIMENSION_BUFFER;
          desc.Width            = overAlloc * sizeof (ImDrawIdx);
          desc.Height           = 1;
          desc.DepthOrArraySize = 1;
          desc.MipLevels        = 1;
          desc.Format           = DXGI_FORMAT_UNKNOWN;
          desc.SampleDesc.Count = 1;
          desc.Layout           = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
          desc.Flags            = D3D12_RESOURCE_FLAG_NONE;

        ThrowIfFailed (
          _imgui_d3d12.pDevice->CreateCommittedResource (
            &props, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_GENERIC_READ,
              nullptr, IID_PPV_ARGS (&pHeap->Ib.p)));
        SK_D3D12_SetDebugName (       pHeap->Ib.p,
          L"ImGui D3D12 IndexBuffer" + std::to_wstring (_frame));

        pHeap->Ib.size = overAlloc;
      }
    }

    catch (const SK_ComException& e)
    {
      SK_LOG0 ( ( L" Exception: %hs [%ws]", e.what (), __FUNCTIONW__ ),
                  L"ImGuiD3D12" );

      _d3d12_rbk->release (_d3d12_rbk->_pSwapChain);

      return;
    }
  }

  // Copy and convert all vertices into a single contiguous buffer
  ImDrawVert* vtx_heap = nullptr;
  ImDrawIdx*  idx_heap = nullptr;

  D3D12_RANGE range = { };

  try
  {
    ThrowIfFailed (pHeap->Vb->Map (0, &range, (void **)&vtx_heap));
    ThrowIfFailed (pHeap->Ib->Map (0, &range, (void **)&idx_heap));

    ImDrawVert* vtx_ptr = vtx_heap;
    ImDrawIdx*  idx_ptr = idx_heap;

    for (int n = 0; n < draw_data->CmdListsCount; n++)
    {
      const ImDrawList*
        cmd_list = draw_data->CmdLists [n];

      if (config.imgui.render.strip_alpha)
      {
        for (INT i = 0; i < cmd_list->VtxBuffer.Size; i++)
        {
          ImU32 color =
            ImColor (cmd_list->VtxBuffer.Data [i].col);

          uint8_t alpha = (((color & 0xFF000000U) >> 24U) & 0xFFU);

          // Boost alpha for visibility
          if (alpha < 93 && alpha != 0)
            alpha += (93  - alpha) / 2;

          float a = ((float)                       alpha / 255.0f);
          float r = ((float)((color & 0xFF0000U) >> 16U) / 255.0f);
          float g = ((float)((color & 0x00FF00U) >>  8U) / 255.0f);
          float b = ((float)((color & 0x0000FFU)       ) / 255.0f);

          color =                    0xFF000000U  |
                  ((UINT)((r * a) * 255U) << 16U) |
                  ((UINT)((g * a) * 255U) <<  8U) |
                  ((UINT)((b * a) * 255U)       );

          cmd_list->VtxBuffer.Data[i].col =
            (ImVec4)ImColor (color);
        }
      }

      vtx_ptr = std::copy_n (cmd_list->VtxBuffer.Data, cmd_list->VtxBuffer.Size, vtx_ptr);
      idx_ptr = std::copy_n (cmd_list->IdxBuffer.Data, cmd_list->IdxBuffer.Size, idx_ptr);
    }

    pHeap->Vb->Unmap (0, &range);
    pHeap->Ib->Unmap (0, &range);
  }

  catch (const SK_ComException& e)
  {
    if (vtx_heap != nullptr) pHeap->Vb->Unmap (0, &range);
    if (idx_heap != nullptr) pHeap->Ib->Unmap (0, &range);

    SK_LOG0 ( ( L" Exception: %hs [%ws]", e.what (), __FUNCTIONW__ ),
                L"ImGuiD3D12" );

    _d3d12_rbk->release (_d3d12_rbk->_pSwapChain);

    return;
  }

  if (! pFrame->begin_cmd_list ())
    return;

  ctx->SetGraphicsRootSignature (_imgui_d3d12.pRootSignature);
  ctx->SetPipelineState         (_imgui_d3d12.pPipelineState);

  ctx->OMSetRenderTargets       (1, &pFrame->hRenderOutput,  FALSE,  nullptr);
  ctx->SetDescriptorHeaps       (1, &descriptorHeaps.pImGui.p);



  // Don't let the user disable ImGui's pipeline state (!!)
  bool                                                                                         _enable = false;
  _imgui_d3d12.pPipelineState->SetPrivateData (SKID_D3D12DisablePipelineState, sizeof (bool), &_enable);
  SK_RunOnce (_criticalVertexShaders.insert (_imgui_d3d12.pPipelineState));


  //
  // HDR STUFF
  //
  static auto& rb =
    SK_GetCurrentRenderBackend ();

  bool hdr_display =
    (rb.isHDRCapable () &&
     rb.isHDRActive  ());


  // Setup orthographic projection matrix into our constant buffer
  // Our visible imgui space lies from draw_data->DisplayPos (top left) to draw_data->DisplayPos+data_data->DisplaySize (bottom right).
  VERTEX_CONSTANT_BUFFER vertex_constant_buffer = { };

  VERTEX_CONSTANT_BUFFER* constant_buffer =
                  &vertex_constant_buffer;

  float L = 0.0f;
  float R = 0.0f + io.DisplaySize.x;
  float T = 0.0f;
  float B = 0.0f + io.DisplaySize.y;
  float mvp[4][4] =
  {
    { 2.0f/(R-L),     0.0f,         0.0f,       0.0f },
    {  0.0f,          2.0f/(T-B),   0.0f,       0.0f },
    {  0.0f,          0.0f,         0.5f,       0.0f },
    { (R+L)/(L-R),  (T+B)/(B-T),    0.5f,       1.0f },
  };
  memcpy (&constant_buffer->mvp, mvp, sizeof (mvp));

  if (! hdr_display)
  {
    constant_buffer->luminance_scale [0] = 1.0f; constant_buffer->luminance_scale [1] = 1.0f;
    constant_buffer->luminance_scale [2] = 0.0f; constant_buffer->luminance_scale [3] = 0.0f;
    constant_buffer->steam_luminance [0] = 1.0f; constant_buffer->steam_luminance [1] = 1.0f;
    constant_buffer->steam_luminance [2] = 1.0f; constant_buffer->steam_luminance [3] = 1.0f;
  }

  else
  {
    SK_RenderBackend::scan_out_s::SK_HDR_TRANSFER_FUNC eotf =
      rb.scanout.getEOTF ();

    bool bEOTF_is_PQ =
     ( eotf == SK_RenderBackend::scan_out_s::SMPTE_2084 );

    constant_buffer->luminance_scale [0] = ( bEOTF_is_PQ ? -80.0f * rb.ui_luminance :
                                                                    rb.ui_luminance );
    constant_buffer->luminance_scale [1] = 2.2f;
    constant_buffer->luminance_scale [2] = rb.display_gamut.minY * 1.0_Nits;
    constant_buffer->steam_luminance [0] = ( bEOTF_is_PQ ? -80.0f * config.platform.overlay_hdr_luminance :
                                                                    config.platform.overlay_hdr_luminance );
    constant_buffer->steam_luminance [1] = 2.2f;
    constant_buffer->steam_luminance [2] = ( bEOTF_is_PQ ? -80.0f * config.uplay.overlay_luminance :
                                                                    config.uplay.overlay_luminance );
    constant_buffer->steam_luminance [3] = 2.2f;
  }

  // Setup viewport
  ctx->RSSetViewports (         1,
    std::array <D3D12_VIEWPORT, 1> (
    {             0.0f, 0.0f,
      io.DisplaySize.x, io.DisplaySize.y,
                  0.0f, 1.0f
    }                              ).data ()
  );

  // Bind shader and vertex buffers
  UINT                      stride = sizeof (ImDrawVert);
  D3D12_GPU_VIRTUAL_ADDRESS offset = 0;

  ctx->IASetVertexBuffers ( 0,            1,
    std::array <D3D12_VERTEX_BUFFER_VIEW, 1> (
    {       pHeap->Vb->GetGPUVirtualAddress () + offset,
      (UINT)pHeap->Vb.size * stride,
                             stride
    }                                        ).data ()
  );

  ctx->IASetIndexBuffer (
    std::array <D3D12_INDEX_BUFFER_VIEW, 1> (
    { pHeap->Ib->GetGPUVirtualAddress (),
      pHeap->Ib.size * sizeof (ImDrawIdx), // C:\Users\amcol\source\repos\SpecialK\src\imgui\backends\imgui_d3d12.cpp(361): error: non-constant-expression cannot be narrowed from type 'unsigned long long' to 'UINT' (aka 'unsigned int') in initializer list [clang-diagnostic-c++11-narrowing]
                       sizeof (ImDrawIdx) == 2 ?
                          DXGI_FORMAT_R16_UINT :
                          DXGI_FORMAT_R32_UINT
    }                                       ).data ()
  );

  ctx->IASetPrimitiveTopology        (D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
  ctx->SetGraphicsRoot32BitConstants (0, 24, &vertex_constant_buffer, 0);
  ctx->SetGraphicsRoot32BitConstants ( 2, 4,
                       std::array <float, 4> (
      { 0.0f, 0.0f,
        hdr_display ? (float)io.DisplaySize.x : 0.0f,
        hdr_display ? (float)io.DisplaySize.y : 0.0f }
                                             ).data (),
                                          0
  );

  // Setup render state
  static const float     blend_factor [4] = { 0.f, 0.f, 0.f, 0.f };
  ctx->OMSetBlendFactor (blend_factor);

  // Render command lists
  int    vtx_offset = 0;
  int    idx_offset = 0;
  ImVec2 pos        = ImVec2 (0.0f, 0.0f);

  for ( int n = 0;
            n < draw_data->CmdListsCount;
          ++n )
  {
    const ImDrawList* cmd_list =
      draw_data->CmdLists [n];

    for ( int cmd_i = 0;
              cmd_i < cmd_list->CmdBuffer.Size;
            ++cmd_i )
    {
      const ImDrawCmd* pcmd =
        &cmd_list->CmdBuffer [cmd_i];

      if (pcmd->UserCallback)
          pcmd->UserCallback (cmd_list, pcmd);

      else
      {
        const D3D12_RECT r =
        {
          (LONG)(pcmd->ClipRect.x - pos.x), (LONG)(pcmd->ClipRect.y - pos.y),
          (LONG)(pcmd->ClipRect.z - pos.x), (LONG)(pcmd->ClipRect.w - pos.y)
        };

        SK_ReleaseAssert (r.left <= r.right && r.top <= r.bottom);

        ctx->SetGraphicsRootDescriptorTable ( 1,
              *(D3D12_GPU_DESCRIPTOR_HANDLE*)&pcmd->TextureId    );
        ctx->RSSetScissorRects              ( 1,              &r );
        ctx->DrawIndexedInstanced           ( pcmd->ElemCount, 1,
                                              idx_offset,
                                              vtx_offset,      0 );
      }

      idx_offset += pcmd->ElemCount;
    } vtx_offset +=  cmd_list->VtxBuffer.Size;
  }

  extern bool SK_D3D12_ShouldSkipHUD (void);
              SK_D3D12_ShouldSkipHUD ();
}

static void
ImGui_ImplDX12_CreateFontsTexture (void)
{
  // Build texture atlas
  ImGuiIO& io =
    ImGui::GetIO ();

  if (! _imgui_d3d12.pDevice)
    return;

  unsigned char* pixels = nullptr;
  int            width  = 0,
                 height = 0;

  io.Fonts->GetTexDataAsRGBA32 ( &pixels,
                                 &width, &height );

  try {
    SK_ComPtr <ID3D12Resource>            pTexture;
    SK_ComPtr <ID3D12Resource>            uploadBuffer;

    SK_ComPtr <ID3D12Fence>               pFence;
    SK_AutoHandle                         hEvent (
                                  SK_CreateEvent ( nullptr, FALSE,
                                                            FALSE, nullptr )
                                                 );

    SK_ComPtr <ID3D12CommandQueue>        cmdQueue;
    SK_ComPtr <ID3D12CommandAllocator>    cmdAlloc;
    SK_ComPtr <ID3D12GraphicsCommandList> cmdList;


    ThrowIfFailed ( hEvent.m_h != 0 ?
                               S_OK : E_UNEXPECTED );

    // Upload texture to graphics system
    D3D12_HEAP_PROPERTIES
      props                      = { };
      props.Type                 = D3D12_HEAP_TYPE_DEFAULT;
      props.CPUPageProperty      = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
      props.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;

    D3D12_RESOURCE_DESC
      desc                       = { };
      desc.Dimension             = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
      desc.Alignment             = 0;
      desc.Width                 = width;
      desc.Height                = height;
      desc.DepthOrArraySize      = 1;
      desc.MipLevels             = 1;
      desc.Format                = DXGI_FORMAT_R8G8B8A8_UNORM;
      desc.SampleDesc.Count      = 1;
      desc.SampleDesc.Quality    = 0;
      desc.Layout                = D3D12_TEXTURE_LAYOUT_UNKNOWN;
      desc.Flags                 = D3D12_RESOURCE_FLAG_NONE;

    ThrowIfFailed (
      _imgui_d3d12.pDevice->CreateCommittedResource (
        &props, D3D12_HEAP_FLAG_NONE,
        &desc,  D3D12_RESOURCE_STATE_COPY_DEST,
        nullptr, IID_PPV_ARGS (&pTexture.p))
    );

    UINT uploadPitch = (width * 4 + D3D12_TEXTURE_DATA_PITCH_ALIGNMENT - 1u)
                                & ~(D3D12_TEXTURE_DATA_PITCH_ALIGNMENT - 1u);
    UINT uploadSize  = height * uploadPitch;

    desc.Dimension             = D3D12_RESOURCE_DIMENSION_BUFFER;
    desc.Alignment             = 0;
    desc.Width                 = uploadSize;
    desc.Height                = 1;
    desc.DepthOrArraySize      = 1;
    desc.MipLevels             = 1;
    desc.Format                = DXGI_FORMAT_UNKNOWN;
    desc.SampleDesc.Count      = 1;
    desc.SampleDesc.Quality    = 0;
    desc.Layout                = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    desc.Flags                 = D3D12_RESOURCE_FLAG_NONE;

    props.Type                 = D3D12_HEAP_TYPE_UPLOAD;
    props.CPUPageProperty      = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    props.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;

    ThrowIfFailed (
      _imgui_d3d12.pDevice->CreateCommittedResource (
        &props, D3D12_HEAP_FLAG_NONE,
        &desc,  D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr, IID_PPV_ARGS (&uploadBuffer.p))
    ); SK_D3D12_SetDebugName (  uploadBuffer.p,
          L"ImGui D3D12 Texture Upload Buffer" );

    void        *mapped = nullptr;
    D3D12_RANGE  range  = { 0, uploadSize };

    ThrowIfFailed (uploadBuffer->Map (0, &range, &mapped));

    for ( int y = 0; y < height; y++ )
    {
      memcpy ( (void*) ((uintptr_t) mapped + y * uploadPitch),
                                    pixels + y * width * 4,
                                                 width * 4 );
    }

    uploadBuffer->Unmap (0, &range);

    D3D12_TEXTURE_COPY_LOCATION
      srcLocation                                    = { };
      srcLocation.pResource                          = uploadBuffer;
      srcLocation.Type                               = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
      srcLocation.PlacedFootprint.Footprint.Format   = DXGI_FORMAT_R8G8B8A8_UNORM;
      srcLocation.PlacedFootprint.Footprint.Width    = width;
      srcLocation.PlacedFootprint.Footprint.Height   = height;
      srcLocation.PlacedFootprint.Footprint.Depth    = 1;
      srcLocation.PlacedFootprint.Footprint.RowPitch = uploadPitch;

    D3D12_TEXTURE_COPY_LOCATION
      dstLocation                  = { };
      dstLocation.pResource        = pTexture;
      dstLocation.Type             = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
      dstLocation.SubresourceIndex = 0;

    D3D12_RESOURCE_BARRIER
      barrier                        = { };
      barrier.Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
      barrier.Flags                  = D3D12_RESOURCE_BARRIER_FLAG_NONE;
      barrier.Transition.pResource   = pTexture;
      barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
      barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
      barrier.Transition.StateAfter  = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE |
                                       D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;

    ThrowIfFailed (
      _imgui_d3d12.pDevice->CreateFence (
        0, D3D12_FENCE_FLAG_NONE,
                  IID_PPV_ARGS (&pFence.p))
     ); SK_D3D12_SetDebugName  ( pFence.p,
     L"ImGui D3D12 Texture Upload Fence");

    D3D12_COMMAND_QUEUE_DESC
      queueDesc          = { };
      queueDesc.Type     = D3D12_COMMAND_LIST_TYPE_DIRECT;
      queueDesc.Flags    = D3D12_COMMAND_QUEUE_FLAG_NONE;
      queueDesc.NodeMask = 1;

    ThrowIfFailed (
      _imgui_d3d12.pDevice->CreateCommandQueue (
           &queueDesc, IID_PPV_ARGS (&cmdQueue.p))
      ); SK_D3D12_SetDebugName (      cmdQueue.p,
        L"ImGui D3D12 Texture Upload Cmd Queue");

    ThrowIfFailed (
      _imgui_d3d12.pDevice->CreateCommandAllocator (
        D3D12_COMMAND_LIST_TYPE_DIRECT,
                    IID_PPV_ARGS (&cmdAlloc.p))
    ); SK_D3D12_SetDebugName (     cmdAlloc.p,
      L"ImGui D3D12 Texture Upload Cmd Allocator");

    ThrowIfFailed (
      _imgui_d3d12.pDevice->CreateCommandList (
        0, D3D12_COMMAND_LIST_TYPE_DIRECT,
           cmdAlloc, nullptr,
                  IID_PPV_ARGS (&cmdList.p))
    ); SK_D3D12_SetDebugName (   cmdList.p,
    L"ImGui D3D12 Texture Upload Cmd List");

    cmdList->CopyTextureRegion ( &dstLocation, 0, 0, 0,
                                 &srcLocation, nullptr );
    cmdList->ResourceBarrier   ( 1, &barrier           );

    ThrowIfFailed (                     cmdList->Close ());

                   cmdQueue->ExecuteCommandLists (1,
                             (ID3D12CommandList* const*)
                                       &cmdList);

    ThrowIfFailed (
      cmdQueue->Signal (pFence,                       1));
                        pFence->SetEventOnCompletion (1,
                                  hEvent.m_h);
    SK_WaitForSingleObject       (hEvent.m_h, INFINITE);

    // Create texture view
    D3D12_SHADER_RESOURCE_VIEW_DESC
      srvDesc                           = { };
      srvDesc.Format                    = DXGI_FORMAT_R8G8B8A8_UNORM;
      srvDesc.ViewDimension             = D3D12_SRV_DIMENSION_TEXTURE2D;
      srvDesc.Texture2D.MipLevels       = desc.MipLevels;
      srvDesc.Texture2D.MostDetailedMip = 0;
      srvDesc.Shader4ComponentMapping   = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

    _imgui_d3d12.pDevice->CreateShaderResourceView (
      pTexture, &srvDesc, _imgui_d3d12.hFontSrvCpuDescHandle
    );

    _imgui_d3d12.pFontTexture.Attach (
      pTexture.Detach ()
    );

    SK_D3D12_SetDebugName (
      _imgui_d3d12.pFontTexture.p, L"ImGui D3D12 FontTexture"
    );

    io.Fonts->TexID =
      (ImTextureID)_imgui_d3d12.hFontSrvGpuDescHandle.ptr;
  }

  catch (const SK_ComException& e) {
    SK_LOG0 ( ( L" Exception: %hs [%ws]", e.what (), __FUNCTIONW__ ),
                L"ImGuiD3D12" );
  };
}

bool
ImGui_ImplDX12_CreateDeviceObjects (void)
{
  if (! _imgui_d3d12.pDevice)
    return false;

  try
  {
    if (_imgui_d3d12.pPipelineState)
      ImGui_ImplDX12_InvalidateDeviceObjects ();

    // Create the root signature
    D3D12_DESCRIPTOR_RANGE
      descRange                                   = { };
      descRange.RangeType                         = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
      descRange.NumDescriptors                    = 1;
      descRange.BaseShaderRegister                = 0;
      descRange.RegisterSpace                     = 0;
      descRange.OffsetInDescriptorsFromTableStart = 0;

    D3D12_ROOT_PARAMETER
      param [3]                                     = { };

      param [0].ParameterType                       = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
      param [0].Constants.ShaderRegister            = 0;
      param [0].Constants.RegisterSpace             = 0;
      param [0].Constants.Num32BitValues            = 24;//16;
      param [0].ShaderVisibility                    = D3D12_SHADER_VISIBILITY_VERTEX;

      param [1].ParameterType                       = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
      param [1].DescriptorTable.NumDescriptorRanges = 1;
      param [1].DescriptorTable.pDescriptorRanges   = &descRange;
      param [1].ShaderVisibility                    = D3D12_SHADER_VISIBILITY_PIXEL;

      param [2].ParameterType                       = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
      param [2].Constants.ShaderRegister            = 0; // b0
      param [2].Constants.Num32BitValues            = 4;
      param [2].ShaderVisibility                    = D3D12_SHADER_VISIBILITY_PIXEL;

    D3D12_STATIC_SAMPLER_DESC
      staticSampler = { };

      staticSampler.Filter           = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
      staticSampler.AddressU         = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
      staticSampler.AddressV         = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
      staticSampler.AddressW         = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
      staticSampler.MipLODBias       = 0.f;
      staticSampler.MaxAnisotropy    = 0;
      staticSampler.ComparisonFunc   = D3D12_COMPARISON_FUNC_ALWAYS;
      staticSampler.BorderColor      = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
      staticSampler.MinLOD           = 0.f;
      staticSampler.MaxLOD           = 0.f;
      staticSampler.ShaderRegister   = 0;
      staticSampler.RegisterSpace    = 0;
      staticSampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    D3D12_ROOT_SIGNATURE_DESC
      desc                   = { };

      desc.NumParameters     = _countof (param);
      desc.pParameters       =           param;
      desc.NumStaticSamplers =  1;
      desc.pStaticSamplers   = &staticSampler;

      desc.Flags             =
      ( D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
        D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS       |
        D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS     |
        D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS );

    SK_ComPtr <ID3DBlob>
                  pBlob;

    ThrowIfFailed (
      D3D12SerializeRootSignature ( &desc,
               D3D_ROOT_SIGNATURE_VERSION_1,
                 &pBlob, nullptr)
    );

    ThrowIfFailed (
      _imgui_d3d12.pDevice->CreateRootSignature (
        0, pBlob->GetBufferPointer (),
           pBlob->GetBufferSize    (),
                  IID_PPV_ARGS (&_imgui_d3d12.pRootSignature.p))
    ); SK_D3D12_SetDebugName (   _imgui_d3d12.pRootSignature.p,
                                L"ImGui D3D12 Root Signture");

    D3D12_GRAPHICS_PIPELINE_STATE_DESC
      psoDesc                       = { };
      psoDesc.NodeMask              = 1;
      psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
      psoDesc.pRootSignature        = _imgui_d3d12.pRootSignature;
      psoDesc.SampleMask            = UINT_MAX;
      psoDesc.NumRenderTargets      = 1;
      psoDesc.RTVFormats [0]        = _imgui_d3d12.RTVFormat;
      psoDesc.SampleDesc.Count      = 1;
      psoDesc.Flags                 = D3D12_PIPELINE_STATE_FLAG_NONE;

      psoDesc.VS = {
                imgui_d3d11_vs_bytecode,
        sizeof (imgui_d3d11_vs_bytecode) /
        sizeof (imgui_d3d11_vs_bytecode) [0]
                   };

    // Create the input layout
    static D3D12_INPUT_ELEMENT_DESC local_layout [] = {
      { "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT,       0, static_cast<UINT>((size_t)(&((ImDrawVert*)0)->pos)), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
      { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,       0, static_cast<UINT>((size_t)(&((ImDrawVert*)0)->uv)),  D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
      { "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, static_cast<UINT>((size_t)(&((ImDrawVert*)0)->col)), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    };

      psoDesc.InputLayout = {       local_layout, 3   };

      psoDesc.PS = {
                imgui_d3d11_ps_bytecode,
        sizeof (imgui_d3d11_ps_bytecode) /
        sizeof (imgui_d3d11_ps_bytecode) [0]
                   };

    // Create the blending setup
    D3D12_BLEND_DESC&
      blendDesc                                        = psoDesc.BlendState;
      blendDesc.AlphaToCoverageEnable                  = false;
      blendDesc.RenderTarget [0].BlendEnable           = true;
      blendDesc.RenderTarget [0].SrcBlend              = D3D12_BLEND_SRC_ALPHA;
      blendDesc.RenderTarget [0].DestBlend             = D3D12_BLEND_INV_SRC_ALPHA;
      blendDesc.RenderTarget [0].BlendOp               = D3D12_BLEND_OP_ADD;
      blendDesc.RenderTarget [0].SrcBlendAlpha         = D3D12_BLEND_ONE;//INV_SRC_ALPHA;
      blendDesc.RenderTarget [0].DestBlendAlpha        = D3D12_BLEND_ZERO;
      blendDesc.RenderTarget [0].BlendOpAlpha          = D3D12_BLEND_OP_ADD;
      blendDesc.RenderTarget [0].RenderTargetWriteMask = //D3D12_COLOR_WRITE_ENABLE_ALL;
                                                         D3D12_COLOR_WRITE_ENABLE_RED   |
                                                         D3D12_COLOR_WRITE_ENABLE_GREEN |
                                                         D3D12_COLOR_WRITE_ENABLE_BLUE;

    // Create the rasterizer state
    D3D12_RASTERIZER_DESC&
      rasterDesc                       = psoDesc.RasterizerState;
      rasterDesc.FillMode              = D3D12_FILL_MODE_SOLID;
      rasterDesc.CullMode              = D3D12_CULL_MODE_NONE;
      rasterDesc.FrontCounterClockwise = FALSE;
      rasterDesc.DepthBias             = D3D12_DEFAULT_DEPTH_BIAS;
      rasterDesc.DepthBiasClamp        = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
      rasterDesc.SlopeScaledDepthBias  = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
      rasterDesc.DepthClipEnable       = true;
      rasterDesc.MultisampleEnable     = FALSE;
      rasterDesc.AntialiasedLineEnable = FALSE;
      rasterDesc.ForcedSampleCount     = 0;
      rasterDesc.ConservativeRaster    = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

    // Create depth-stencil State
    D3D12_DEPTH_STENCIL_DESC&
      stencilDesc                              = psoDesc.DepthStencilState;
      stencilDesc.DepthEnable                  = false;
      stencilDesc.DepthWriteMask               = D3D12_DEPTH_WRITE_MASK_ZERO;
      stencilDesc.DepthFunc                    = D3D12_COMPARISON_FUNC_ALWAYS;
      stencilDesc.StencilEnable                = false;
      stencilDesc.FrontFace.StencilFailOp      =
      stencilDesc.FrontFace.StencilDepthFailOp =
      stencilDesc.FrontFace.StencilPassOp      = D3D12_STENCIL_OP_KEEP;
      stencilDesc.FrontFace.StencilFunc        = D3D12_COMPARISON_FUNC_ALWAYS;
      stencilDesc.BackFace                     =
      stencilDesc.FrontFace;

    ThrowIfFailed (
      _imgui_d3d12.pDevice->CreateGraphicsPipelineState (
        &psoDesc, IID_PPV_ARGS (&_imgui_d3d12.pPipelineState.p))
    ); SK_D3D12_SetDebugName (   _imgui_d3d12.pPipelineState.p,
                                 L"ImGui D3D12 Pipeline State");

    ImGui_ImplDX12_CreateFontsTexture ();

    return true;
  }

  catch (const SK_ComException& e)
  {
    SK_LOG0 ( ( L" Exception: %hs [%ws]", e.what (), __FUNCTIONW__ ),
                L"ImGuiD3D12" );

    return false;
  }
}

void
ImGui_ImplDX12_InvalidateDeviceObjects (void)
{
  if (! _imgui_d3d12.pDevice)
    return;

  _imgui_d3d12.pRootSignature.Release ();
  _imgui_d3d12.pPipelineState.Release ();

  // We copied g_pFontTextureView to io.Fonts->TexID so let's clear that as well.
  _imgui_d3d12.pFontTexture.Release ();

  ImGui::GetIO ().Fonts->TexID = nullptr;

  for ( auto& frame : _imgui_d3d12.frame_heaps )
  {
    frame.Ib.Release (); frame.Ib.size = 0;
    frame.Vb.Release (); frame.Vb.size = 0;
  }
}

bool
ImGui_ImplDX12_Init ( ID3D12Device*               device,
                      int                         num_frames_in_flight,
                      DXGI_FORMAT                 rtv_format,
                      D3D12_CPU_DESCRIPTOR_HANDLE font_srv_cpu_desc_handle,
                      D3D12_GPU_DESCRIPTOR_HANDLE font_srv_gpu_desc_handle,
                      HWND                        hwnd )
{
  SK_LOG0 ( ( L"(+) Acquiring D3D12 Render Context: Device=%08xh, SwapChain: {%lu x %hs, HWND=%08xh}",
                device, num_frames_in_flight,
                                      SK_DXGI_FormatToStr (rtv_format).data (),
                                      hwnd ),
              L"D3D12BkEnd" );

  _imgui_d3d12.pDevice               = device;
  _imgui_d3d12.RTVFormat             = rtv_format;
  _imgui_d3d12.hFontSrvCpuDescHandle = font_srv_cpu_desc_handle;
  _imgui_d3d12.hFontSrvGpuDescHandle = font_srv_gpu_desc_handle;
  _imgui_d3d12.hWndSwapChain         = hwnd;

  // Create buffers with a default size (they will later be grown as needed)
  for ( auto& frame : _imgui_d3d12.frame_heaps )
  {
    frame.Ib.Release ();
    frame.Vb.Release ();

    frame.Vb.size = 25000;
    frame.Ib.size = 50000;
  }

  return true;
}

void
ImGui_ImplDX12_Shutdown (void)
{
  ImGui_ImplDX12_InvalidateDeviceObjects ();

  if (_imgui_d3d12.pDevice.p != nullptr)
  {
    SK_LOG0 ( ( L"(-) Releasing D3D12 Render Context: Device=%08xh, SwapChain: {%hs, HWND=%08xh}",
                                            _imgui_d3d12.pDevice.p,
                       SK_DXGI_FormatToStr (_imgui_d3d12.RTVFormat).data (),
                                            _imgui_d3d12.hWndSwapChain),
                L"D3D12BkEnd" );
  }

  _imgui_d3d12.pDevice.Release ();
  _imgui_d3d12.hFontSrvCpuDescHandle.ptr = 0;
  _imgui_d3d12.hFontSrvGpuDescHandle.ptr = 0;
}

void
SK_ImGui_User_NewFrame (void);

#include <SpecialK/window.h>

void
ImGui_ImplDX12_NewFrame (void)
{
  if ( _imgui_d3d12.pDevice                   == nullptr ||
       _imgui_d3d12.hFontSrvCpuDescHandle.ptr == 0       ||
       _imgui_d3d12.hFontSrvGpuDescHandle.ptr == 0 ) return;

  if (! _imgui_d3d12.pPipelineState)
    ImGui_ImplDX12_CreateDeviceObjects ();

  static auto& rb =
    SK_GetCurrentRenderBackend ();

  if (! rb.device)
    return;

  SK_ReleaseAssert (rb.device == _imgui_d3d12.pDevice);

  //// Start the frame
  SK_ImGui_User_NewFrame ();
}


SK_LazyGlobal <SK_ImGui_ResourcesD3D12> SK_ImGui_D3D12;

extern void
SK_D3D12_AddMissingPipelineState ( ID3D12Device        *pDevice,
                                   ID3D12PipelineState *pPipelineState );

using D3D12GraphicsCommandList_SetPipelineState_pfn = void
(STDMETHODCALLTYPE *)( ID3D12GraphicsCommandList*,
                       ID3D12PipelineState* );
      D3D12GraphicsCommandList_SetPipelineState_pfn
      D3D12GraphicsCommandList_SetPipelineState_Original = nullptr;

void
STDMETHODCALLTYPE
D3D12GraphicsCommandList_SetPipelineState_Detour (
  ID3D12GraphicsCommandList *This,
  ID3D12PipelineState       *pPipelineState )
{
  SK_LOG_FIRST_CALL

  if (pPipelineState != nullptr)
  {
    // We do not actually care what this is, only that it exists.
    UINT size_ = DxilContainerHashSize;

    if ( FAILED ( pPipelineState->GetPrivateData (
                    SKID_D3D12KnownVtxShaderDigest,
                                            &size_, nullptr ) ) )
    {
      SK_ComPtr <ID3D12Device>               pDevice;
      if ( SUCCEEDED (
             This->GetDevice (IID_PPV_ARGS (&pDevice.p))) )
      { SK_D3D12_AddMissingPipelineState (   pDevice.p, pPipelineState ); }
    }

    UINT64 current_frame =
      SK_GetFramesDrawn ();

    pPipelineState->SetPrivateData (
      SKID_D3D12LastFrameUsed, sizeof (current_frame),
                                      &current_frame );

    UINT size    = sizeof (bool);
    bool disable = false;

    pPipelineState->GetPrivateData (
      SKID_D3D12DisablePipelineState, &size, &disable );
              This->SetPrivateData (
      SKID_D3D12DisablePipelineState,  sizeof (bool),
                                             &disable );
  }

  D3D12GraphicsCommandList_SetPipelineState_Original ( This,
                                                        pPipelineState );
}


using D3D12GraphicsCommandList_DrawInstanced_pfn = void
(STDMETHODCALLTYPE *)( ID3D12GraphicsCommandList*,
                       UINT,UINT,UINT,UINT );
      D3D12GraphicsCommandList_DrawInstanced_pfn
      D3D12GraphicsCommandList_DrawInstanced_Original = nullptr;
using D3D12GraphicsCommandList_DrawIndexedInstanced_pfn = void
(STDMETHODCALLTYPE *)( ID3D12GraphicsCommandList*,
                       UINT,UINT,UINT,INT,UINT );
      D3D12GraphicsCommandList_DrawIndexedInstanced_pfn
      D3D12GraphicsCommandList_DrawIndexedInstanced_Original = nullptr;
using D3D12GraphicsCommandList_ExecuteIndirect_pfn = void
(STDMETHODCALLTYPE *)( ID3D12GraphicsCommandList*,
                       ID3D12CommandSignature*,
                       UINT,  ID3D12Resource*,
                       UINT64,ID3D12Resource*,
                       UINT64 );
      D3D12GraphicsCommandList_ExecuteIndirect_pfn
      D3D12GraphicsCommandList_ExecuteIndirect_Original = nullptr;

void
STDMETHODCALLTYPE
D3D12GraphicsCommandList_DrawInstanced_Detour (
  ID3D12GraphicsCommandList *This,
  UINT                       VertexCountPerInstance,
  UINT                       InstanceCount,
  UINT                       StartVertexLocation,
  UINT                       StartInstanceLocation )
{
  SK_LOG_FIRST_CALL

  UINT size    = sizeof (bool);
  bool disable = false;

  if ( SUCCEEDED ( This->GetPrivateData ( SKID_D3D12DisablePipelineState, &size, &disable ) ) )
  {
    if (disable)
      return;
  }

  return
    D3D12GraphicsCommandList_DrawInstanced_Original (
      This, VertexCountPerInstance, InstanceCount,
               StartVertexLocation, StartInstanceLocation );
}

void
STDMETHODCALLTYPE
D3D12GraphicsCommandList_DrawIndexedInstanced_Detour (
  ID3D12GraphicsCommandList *This,
  UINT                       IndexCountPerInstance,
  UINT                       InstanceCount,
  UINT                       StartIndexLocation,
  INT                        BaseVertexLocation,
  UINT                       StartInstanceLocation )
{
  SK_LOG_FIRST_CALL

  UINT size    = sizeof (bool);
  bool disable = false;

  if ( SUCCEEDED ( This->GetPrivateData ( SKID_D3D12DisablePipelineState, &size, &disable ) ) )
  {
    if (disable)
      return;
  }

  return
    D3D12GraphicsCommandList_DrawIndexedInstanced_Original (
      This, IndexCountPerInstance, InstanceCount, StartIndexLocation,
                           BaseVertexLocation, StartInstanceLocation );
}

void
STDMETHODCALLTYPE
D3D12GraphicsCommandList_ExecuteIndirect_Detour (
  ID3D12GraphicsCommandList *This,
  ID3D12CommandSignature    *pCommandSignature,
  UINT                       MaxCommandCount,
  ID3D12Resource            *pArgumentBuffer,
  UINT64                     ArgumentBufferOffset,
  ID3D12Resource            *pCountBuffer,
  UINT64                     CountBufferOffset )
{
  SK_LOG_FIRST_CALL

  UINT size    = sizeof (bool);
  bool disable = false;

  if ( SUCCEEDED ( This->GetPrivateData ( SKID_D3D12DisablePipelineState, &size, &disable ) ) )
  {
    if (disable)
      return;
  }

  return
    D3D12GraphicsCommandList_ExecuteIndirect_Original (
      This, pCommandSignature, MaxCommandCount, pArgumentBuffer,
        ArgumentBufferOffset, pCountBuffer, CountBufferOffset );
}

void
_InitDrawCommandHooks (ID3D12GraphicsCommandList* pCmdList)
{
  if (D3D12GraphicsCommandList_DrawInstanced_Original == nullptr)
  {
    SK_CreateVFTableHook2 ( L"ID3D12GraphicsCommandList::DrawInstanced",
                            *(void***)*(&pCmdList), 12,
                              D3D12GraphicsCommandList_DrawInstanced_Detour,
                    (void **)&D3D12GraphicsCommandList_DrawInstanced_Original );
  }

  if (D3D12GraphicsCommandList_DrawIndexedInstanced_Original == nullptr)
  {
    SK_CreateVFTableHook2 ( L"ID3D12GraphicsCommandList::DrawIndexedInstanced",
                            *(void***)*(&pCmdList), 13,
                              D3D12GraphicsCommandList_DrawIndexedInstanced_Detour,
                    (void **)&D3D12GraphicsCommandList_DrawIndexedInstanced_Original );
  }

  if (D3D12GraphicsCommandList_SetPipelineState_Original == nullptr)
  {
    SK_CreateVFTableHook2 ( L"ID3D12GraphicsCommandList::SetPipelineState",
                            *(void***)*(&pCmdList), 25,
                               D3D12GraphicsCommandList_SetPipelineState_Detour,
                     (void **)&D3D12GraphicsCommandList_SetPipelineState_Original );
  }

  // 26 ResourceBarrier
  // 27 ExecuteBundle
  // 28 SetDescriptorHeaps
  // 29 SetComputeRootSignature
  // 30 SetGraphicsRootSignature
  // 31 SetComputeRootDescriptorTable
  // 32 SetGraphicsRootDescriptorTable
  // 33 SetComputeRoot32BitConstant
  // 34 SetGraphicsRoot32BitConstant
  // 35 SetComputeRoot32BitConstants
  // 36 SetGraphicsRoot32BitConstants
  // 37 SetComputeRootConstantBufferView
  // 38 SetGraphicsRootConstantBufferView
  // 39 SetComputeRootShaderResourceView
  // 40 SetGraphicsRootShaderResourceView
  // 41 SetComputeRootUnorderedAccessView
  // 42 SetGraphicsRootUnorderedAccessView
  // 43 IASetIndexBuffer
  // 44 IASetVertexBuffers
  // 45 SOSetTargets
  // 46 OMSetRenderTargets
  // 47 ClearDepthStencilView
  // 48 ClearRenderTargetView
  // 49 ClearUnorderedAccessViewUint
  // 50 ClearUnorderedAccessViewFloat
  // 51 DiscardResource
  // 52 BeginQuery
  // 53 EndQuery
  // 54 ResolveQueryData
  // 55 SetPredication
  // 56 SetMarker
  // 57 BeginEvent
  // 58 EndEvent
  // 59 ExecuteIndirect

  if (D3D12GraphicsCommandList_ExecuteIndirect_Original == nullptr)
  {
    SK_CreateVFTableHook2 ( L"ID3D12GraphicsCommandList::ExecuteIndirect",
                            *(void***)*(&pCmdList), 59,
                               D3D12GraphicsCommandList_ExecuteIndirect_Detour,
                     (void **)&D3D12GraphicsCommandList_ExecuteIndirect_Original );
  }

  SK_ApplyQueuedHooks ();
}


/// --------------- UGLY COMPAT HACK ----------------------
using D3D12GraphicsCommandList_CopyTextureRegion_pfn = void
(STDMETHODCALLTYPE *)( ID3D12GraphicsCommandList*,
                const  D3D12_TEXTURE_COPY_LOCATION*,
                       UINT, UINT, UINT,
                const  D3D12_TEXTURE_COPY_LOCATION*,
                const  D3D12_BOX* );
      D3D12GraphicsCommandList_CopyTextureRegion_pfn
      D3D12GraphicsCommandList_CopyTextureRegion_Original = nullptr;

using D3D12GraphicsCommandList_CopyResource_pfn = void
(STDMETHODCALLTYPE *)( ID3D12GraphicsCommandList*,
                       ID3D12Resource*,ID3D12Resource* );
      D3D12GraphicsCommandList_CopyResource_pfn
      D3D12GraphicsCommandList_CopyResource_Original = nullptr;

extern bool SK_D3D12_IsTextureInjectionNeeded (void);

// Workaround for Guardians of the Galaxy in HDR
//
void
STDMETHODCALLTYPE
D3D12GraphicsCommandList_CopyResource_Detour (
        ID3D12GraphicsCommandList *This,
        ID3D12Resource            *pDstResource,
        ID3D12Resource            *pSrcResource )
{
  if (__SK_HDR_16BitSwap)
  {
    auto src_desc = pSrcResource->GetDesc (),
         dst_desc = pDstResource->GetDesc ();

    if (dst_desc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE2D &&
        dst_desc.Format    == DXGI_FORMAT_R16G16B16A16_FLOAT)
    {
      auto typelessSrc = DirectX::MakeTypeless (src_desc.Format),
           typelessDst = DirectX::MakeTypeless (dst_desc.Format);
      
      if ( typelessSrc != typelessDst &&
             DirectX::BitsPerPixel (src_desc.Format) !=
             DirectX::BitsPerPixel (dst_desc.Format) )
      {
        if (This->GetType () == D3D12_COMMAND_LIST_TYPE_DIRECT)
        {
          SK_ComQIPtr <IDXGISwapChain>
              pSwap (  SK_GetCurrentRenderBackend ().swapchain  );
          DXGI_SWAP_CHAIN_DESC swapDesc = { };
          if (pSwap)
              pSwap->GetDesc (&swapDesc);

          if ( pSwap.p == nullptr ||
               ( src_desc.Width  == swapDesc.BufferDesc.Width  &&
                 src_desc.Height == swapDesc.BufferDesc.Height &&
                (src_desc.Format == swapDesc.BufferDesc.Format ||
                 dst_desc.Format == swapDesc.BufferDesc.Format) )
             )
          {
            //// We're copying to the SwapChain, so we can use SK's Blitter to copy an incompatible format
            extern void SK_D3D12_HDR_CopyBuffer (ID3D12GraphicsCommandList*, ID3D12Resource*);
                        SK_D3D12_HDR_CopyBuffer (This, pSrcResource);
          }
        }

        return;
      }
    }
  }

  return
    D3D12GraphicsCommandList_CopyResource_Original (
      This, pDstResource, pSrcResource
    );
}

// Workaround for Control in HDR
void
STDMETHODCALLTYPE
D3D12GraphicsCommandList_CopyTextureRegion_Detour (
         ID3D12GraphicsCommandList   *This,
  const  D3D12_TEXTURE_COPY_LOCATION *pDst,
         UINT                         DstX,
         UINT                         DstY,
         UINT                         DstZ,
  const  D3D12_TEXTURE_COPY_LOCATION *pSrc,
  const  D3D12_BOX                   *pSrcBox )
{
  D3D12_RESOURCE_DESC
    src_desc = pSrc->pResource->GetDesc (),
    dst_desc = pDst->pResource->GetDesc ();

#if 1
  static bool
    bUseInjection =
      SK_D3D12_IsTextureInjectionNeeded ();

  if (                                           bUseInjection &&
      D3D12_RESOURCE_DIMENSION_BUFFER    == src_desc.Dimension &&
      D3D12_RESOURCE_DIMENSION_TEXTURE2D == dst_desc.Dimension &&
      pDst->SubresourceIndex             == 0 &&
      pSrc->SubresourceIndex             == 0 && pSrcBox == nullptr &&
                                    DstX == 0 &&
                                    DstY == 0 && DstZ == 0 )
  {
    UINT size   = 1U;
    bool ignore = false;

    pSrc->pResource->GetPrivateData (SKID_D3D12IgnoredTextureCopy, &size, &ignore);

    if (! ignore)
    {
      size = 1U;
      pDst->pResource->GetPrivateData (SKID_D3D12IgnoredTextureCopy, &size, &ignore);
    }

    if (! ignore)
    {
      extern void
        SK_D3D12_CopyTexRegion_Dump (ID3D12GraphicsCommandList* This, ID3D12Resource* pResource);
        SK_D3D12_CopyTexRegion_Dump (This, pDst->pResource);
    }
  }
#endif

  if (__SK_HDR_16BitSwap)
  {
    // Format override siliness in D3D12
    static volatile LONG lSizeSkips   = 0;
    static volatile LONG lFormatSkips = 0;

    if (D3D12_RESOURCE_DIMENSION_TEXTURE2D == src_desc.Dimension)
    {
      static auto& rb =
        SK_GetCurrentRenderBackend ();

      SK_ComQIPtr <IDXGISwapChain>
          pSwap (  rb.swapchain  );
      DXGI_SWAP_CHAIN_DESC swapDesc = { };
      if (pSwap)
          pSwap->GetDesc (&swapDesc);

      //
      // SwapChain Copies:   Potentially fixable using shader-based copy
      //
      if ( pSwap.p == nullptr ||
           ( src_desc.Width  == swapDesc.BufferDesc.Width  &&
             src_desc.Height == swapDesc.BufferDesc.Height &&
            (src_desc.Format == swapDesc.BufferDesc.Format ||
             dst_desc.Format == swapDesc.BufferDesc.Format) )
         )
      {
        if ( dst_desc.Dimension == D3D12_RESOURCE_DIMENSION_BUFFER &&
             dst_desc.Height    == 1                               && DstX == 0 && DstY    == 0
                                                                   && DstZ == 0 && pSrcBox == nullptr &&
             // Is the destination buffer too small?
             dst_desc.Width     <  src_desc.Width   *
                                   src_desc.Height  *
            DirectX::BitsPerPixel (src_desc.Format) / 8 )
        {
          InterlockedIncrement (&lSizeSkips);

          return;
        }

        else if ( dst_desc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE2D )
        {
          auto typelessSrc = DirectX::MakeTypeless (src_desc.Format),
               typelessDst = DirectX::MakeTypeless (dst_desc.Format);

          if ( typelessSrc != typelessDst     &&
               DirectX::BitsPerPixel (src_desc.Format) !=
               DirectX::BitsPerPixel (dst_desc.Format) )
          {
            // We're copying -to- the SwapChain, so we can use SK's Blitter to copy an incompatible format
            if (This->GetType () == D3D12_COMMAND_LIST_TYPE_DIRECT && typelessSrc != DXGI_FORMAT_R16G16B16A16_TYPELESS)
            {
              extern void SK_D3D12_HDR_CopyBuffer (ID3D12GraphicsCommandList*, ID3D12Resource*);
                          SK_D3D12_HDR_CopyBuffer (This, pSrc->pResource);

              return;
            }

            //
            // Either some unrelated copy, or the engine is copying back -from- the SwapChain
            //
            //  * This case is not currently implemented (rarely used)
            //

            InterlockedIncrement (&lFormatSkips);

            return;
          }         
        }
      }

      auto typelessFootprintSrc = DirectX::MakeTypeless (pSrc->PlacedFootprint.Footprint.Format),
           typelessFootprintDst = DirectX::MakeTypeless (pDst->PlacedFootprint.Footprint.Format);

      //
      // Handle situations where engine uses some, but not all, modified swapchain properties
      // 
      //  (e.g. knows the format is different, but computes size using the original format)
      //
      if ( typelessFootprintSrc == DXGI_FORMAT_R16G16B16A16_TYPELESS ||
           typelessFootprintDst == DXGI_FORMAT_R16G16B16A16_TYPELESS )
      {
        const UINT src_row_pitch =      pSrc->PlacedFootprint.Footprint.Width *
          (static_cast <UINT> (DirectX::BitsPerPixel (typelessFootprintSrc) / 8));
        const UINT dst_row_pitch =      pDst->PlacedFootprint.Footprint.Width *
          (static_cast <UINT> (DirectX::BitsPerPixel (typelessFootprintDst) / 8));

        if ( pSrc->PlacedFootprint.Footprint.RowPitch < src_row_pitch ||
             pDst->PlacedFootprint.Footprint.RowPitch < dst_row_pitch )
        {
          SK_LOGi0 (
            L"Skipping invalid CopyTextureRegion:"
            L" (SrcPitch: Requested = %lu, Valid >= %lu),"
            L" (DstPitch: Requested = %lu, Valid >= %lu) - SrcFmt: (%hs | %hs) /"
            L" DstFmt: (% hs | % hs)",
              pSrc->PlacedFootprint.Footprint.RowPitch, src_row_pitch,
              pDst->PlacedFootprint.Footprint.RowPitch, dst_row_pitch,
                SK_DXGI_FormatToStr (pSrc->PlacedFootprint.Footprint.Format).data (),
                SK_DXGI_FormatToStr (                       src_desc.Format).data (),
                SK_DXGI_FormatToStr (pDst->PlacedFootprint.Footprint.Format).data (),
                SK_DXGI_FormatToStr (                       dst_desc.Format).data ()
          );

          InterlockedIncrement (&lSizeSkips);

          //
          // TODO: Implement a copy-from-swapchain to temporary surface w/ format conversion
          //         and allow this mismatched format subregion copy to read from it.
          //

          return;
        }
      }
    }
  }

  D3D12GraphicsCommandList_CopyTextureRegion_Original (
    This, pDst, DstX, DstY, DstZ, pSrc, pSrcBox
  );
}

void
_InitCopyTextureRegionHook (ID3D12GraphicsCommandList* pCmdList)
{
  if (D3D12GraphicsCommandList_CopyTextureRegion_Original == nullptr)
  {
    SK_CreateVFTableHook2 ( L"ID3D12GraphicsCommandList::CopyTextureRegion",
                            *(void***)*(&pCmdList), 16,
                               D3D12GraphicsCommandList_CopyTextureRegion_Detour,
                     (void **)&D3D12GraphicsCommandList_CopyTextureRegion_Original );
  }

  if (D3D12GraphicsCommandList_CopyResource_Original == nullptr)
  {
    SK_CreateVFTableHook2 ( L"ID3D12GraphicsCommandList::CopyResource",
                            *(void***)*(&pCmdList), 17,
                               D3D12GraphicsCommandList_CopyResource_Detour,
                     (void **)&D3D12GraphicsCommandList_CopyResource_Original );
  }
}
/// --------------- UGLY COMPAT HACK ----------------------


void
SK_D3D12_HDR_CopyBuffer (ID3D12GraphicsCommandList *pCommandList, ID3D12Resource* pResource)
{
  static auto& rb =
    SK_GetCurrentRenderBackend ();

  if (! _d3d12_rbk->_pCommandQueue.p)
    return;

  SK_ComPtr <ID3D12Device>
             pD3D12Device;

  if (FAILED (_d3d12_rbk->_pSwapChain->GetDevice (IID_PPV_ARGS (&pD3D12Device.p))))
    return;

  UINT swapIdx =
    _d3d12_rbk->_pSwapChain->GetCurrentBackBufferIndex ();

  DXGI_SWAP_CHAIN_DESC1          swapDesc = { };
  _d3d12_rbk->_pSwapChain->GetDesc1
                               (&swapDesc);
  if ( _imgui_d3d12.RTVFormat != swapDesc.Format || swapIdx > _d3d12_rbk->frames_.size () )
  {
    return;
  }

  SK_D3D12_RenderCtx::FrameCtx& stagingFrame =
    _d3d12_rbk->frames_ [swapIdx];

  SK_ReleaseAssert (stagingFrame.fence.p != nullptr);

  if (stagingFrame.fence == nullptr)
    return;

  HDR_LUMINANCE
    cbuffer_luma = {
      {  __SK_HDR_Luma,
         __SK_HDR_Exp, 1.0f,
                       1.0f
      }
    };

  // Passthrough mode so we can reuse the HDR shader to blit incompatible image formats
  static auto constexpr TONEMAP_CopyResource = 255;

  static HDR_COLORSPACE_PARAMS
    cbuffer_cspace              = { };
    cbuffer_cspace.uiToneMapper = TONEMAP_CopyResource;

  static FLOAT         kfBlendFactors [] = { 0.0f, 0.0f, 0.0f, 0.0f };

  pCommandList->SetGraphicsRootSignature          ( _d3d12_rbk->pHDRSignature            );
  pCommandList->SetPipelineState                  ( _d3d12_rbk->pHDRPipeline             );
  pCommandList->SetGraphicsRoot32BitConstants     ( 0, 4,  &cbuffer_luma,   0            );
  pCommandList->SetGraphicsRoot32BitConstants     ( 1, 16, &cbuffer_cspace, 0            );
  pCommandList->OMSetBlendFactor                  ( kfBlendFactors                       );
  pCommandList->IASetPrimitiveTopology            ( D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP );

  _d3d12_rbk->
          _pDevice->CreateShaderResourceView ( pResource, nullptr,
                                               stagingFrame.hdr.hBufferCopy_CPU
                                             );

  SK_D3D12_StateTransition barriers [] = {
    { D3D12_RESOURCE_STATE_COPY_DEST,     D3D12_RESOURCE_STATE_RENDER_TARGET },
    { D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COPY_DEST     }
  };

  barriers [0].Transition.pResource = stagingFrame.pRenderOutput.p;
  barriers [1].Transition.pResource = stagingFrame.pRenderOutput.p;

  pCommandList->ResourceBarrier                   ( 1, &barriers [0]);
  pCommandList->SetDescriptorHeaps                ( 1, &_d3d12_rbk->descriptorHeaps.pHDR_CopyAssist.p );
  pCommandList->SetGraphicsRootDescriptorTable    ( 2,  stagingFrame.hdr.hBufferCopy_GPU              );
  pCommandList->OMSetRenderTargets                ( 1, &stagingFrame.hRenderOutput, FALSE, nullptr    );
  pCommandList->RSSetViewports                    ( 1, &stagingFrame.hdr.vp                           );
  pCommandList->RSSetScissorRects                 ( 1, &stagingFrame.hdr.scissor                      );
  pCommandList->DrawInstanced                     ( 3, 1, 0, 0                                        );
  pCommandList->ResourceBarrier                   ( 1, &barriers [1]);
}

void
SK_D3D12_RenderCtx::present (IDXGISwapChain3 *pSwapChain)
{
  if (! _pDevice.p || frames_.empty ())
  {
    if (! init (pSwapChain, _pCommandQueue.p))
      return;
  }

  if (! _pCommandQueue.p)
    return;

  if (! SK_D3D12_HasDebugName (_pCommandQueue.p))
  {
    static UINT unique_d3d12_qid = 0UL;

    SK_D3D12_SetDebugName (    _pCommandQueue.p,
             SK_FormatStringW ( L"[Game] D3D12 SwapChain CmdQueue %d",
                                  unique_d3d12_qid++ ).c_str ()
                          );
  }

  SK_ComPtr <ID3D12Device>
             pD3D12Device;

  if (FAILED (pSwapChain->GetDevice (IID_PPV_ARGS (&pD3D12Device.p))))
    return;

  if (! pD3D12Device.IsEqualObject (_pDevice.p))
    return;

  UINT swapIdx =
    _pSwapChain->GetCurrentBackBufferIndex ();

  DXGI_SWAP_CHAIN_DESC1          swapDesc = { };
  pSwapChain->GetDesc1         (&swapDesc);
  if ((_imgui_d3d12.RTVFormat != swapDesc.Format &&
       _imgui_d3d12.RTVFormat != DXGI_FORMAT_UNKNOWN) || swapIdx > frames_.size () )
  {
    static bool          once = false;
    if (! std::exchange (once, true))
    {
      SK_LOG0 ( ( L"ImGui Expects SwapChain Format %hs, but Got %hs... "
                  L"no attempt to draw will be made.",
                    SK_DXGI_FormatToStr (_imgui_d3d12.RTVFormat).data (),
                    SK_DXGI_FormatToStr (       swapDesc.Format).data () ),
                  __SK_SUBSYSTEM__ );
    }

    return;
  }

  FrameCtx& stagingFrame =
    frames_ [swapIdx];

  SK_ReleaseAssert (stagingFrame.fence.p != nullptr);

  if (stagingFrame.fence == nullptr)
    return;

  auto pCommandList =
    stagingFrame.pCmdList.p;

  // Make sure all commands for this command allocator have finished executing before reseting it
  if (stagingFrame.fence->GetCompletedValue () < stagingFrame.fence.value)
  {
    if ( SUCCEEDED ( stagingFrame.fence->SetEventOnCompletion (
                     stagingFrame.fence.value,
                     stagingFrame.fence.event                 )
                   )
       )
    {
      // Event is automatically reset after this wait is released
      SK_WaitForSingleObject (stagingFrame.fence.event, INFINITE);
    }
  }

  // Screenshot may have left this in a recording state
  if (! stagingFrame.bCmdListRecording)
  {
    stagingFrame.pCmdAllocator->Reset ();

    if (! stagingFrame.begin_cmd_list ())
    {
      SK_ReleaseAssert (!"Command List Cannot Begin");
      return;
    }
  }

  static auto& rb =
    SK_GetCurrentRenderBackend ();

  ///bool hdr_display =
  ///  (rb.isHDRCapable () && (rb.framebuffer_flags & SK_FRAMEBUFFER_FLAG_HDR));

  SK_RunOnce (
    _InitDrawCommandHooks (pCommandList)
  );

  static std::error_code ec;
  static bool inject_textures =
    std::filesystem::exists   (SK_Resource_GetRoot () / LR"(inject/textures/)", ec) &&
  (!std::filesystem::is_empty (SK_Resource_GetRoot () / LR"(inject/textures/)", ec));

  if (config.textures.dump_on_load || inject_textures)
  {
    SK_RunOnce (
      _InitCopyTextureRegionHook (pCommandList)
    );
  }

  bool bHDR =
    ( __SK_HDR_16BitSwap &&
       stagingFrame.hdr.pSwapChainCopy.p != nullptr &&
       stagingFrame.hdr.pSwapChainCopy.p->GetDesc ().Format == DXGI_FORMAT_R16G16B16A16_FLOAT &&
                        pHDRPipeline.p   != nullptr &&
                        pHDRSignature.p  != nullptr );

  if (bHDR)
  {
    SK_RunOnce (
      _InitCopyTextureRegionHook (pCommandList)
    );

    // Don't let user disable HDR re-processing
    bool                                                                          _enable = false;
    pHDRPipeline->SetPrivateData (SKID_D3D12DisablePipelineState, sizeof (bool), &_enable);
    SK_RunOnce (_criticalVertexShaders.insert (pHDRPipeline));


    HDR_LUMINANCE
      cbuffer_luma = {
        {  __SK_HDR_Luma,
           __SK_HDR_Exp,
          (__SK_HDR_HorizCoverage / 100.0f) * 2.0f - 1.0f,
          (__SK_HDR_VertCoverage  / 100.0f) * 2.0f - 1.0f
        }
      };

    static HDR_COLORSPACE_PARAMS
      cbuffer_cspace                       = { };

      cbuffer_cspace.uiToneMapper          =   __SK_HDR_tonemap;
      cbuffer_cspace.hdrSaturation         =   __SK_HDR_Saturation;
      cbuffer_cspace.hdrGamutExpansion     =   __SK_HDR_Gamut;
      cbuffer_cspace.sdrLuminance_NonStd   =   __SK_HDR_user_sdr_Y * 1.0_Nits;
      cbuffer_cspace.sdrIsImplicitlysRGB   =   __SK_HDR_Bypass_sRGB != 1;
      cbuffer_cspace.visualFunc [0]        = (uint32_t)__SK_HDR_visualization;
      cbuffer_cspace.visualFunc [1]        = (uint32_t)__SK_HDR_visualization;
      cbuffer_cspace.visualFunc [2]        = (uint32_t)__SK_HDR_visualization;

      cbuffer_cspace.hdrLuminance_MaxAvg   = __SK_HDR_tonemap == 2 ?
                                      rb.working_gamut.maxAverageY != 0.0f ?
                                      rb.working_gamut.maxAverageY         : rb.display_gamut.maxAverageY
                                                                   :         rb.display_gamut.maxAverageY;
      cbuffer_cspace.hdrLuminance_MaxLocal = __SK_HDR_tonemap == 2 ?
                                      rb.working_gamut.maxLocalY != 0.0f ?
                                      rb.working_gamut.maxLocalY         : rb.display_gamut.maxLocalY
                                                                   :       rb.display_gamut.maxLocalY;
      cbuffer_cspace.hdrLuminance_Min      = rb.display_gamut.minY * 1.0_Nits;
      cbuffer_cspace.currentTime           = (float)SK_timeGetTime ();

      extern float                       __SK_HDR_PQBoost0;
      extern float                       __SK_HDR_PQBoost1;
      extern float                       __SK_HDR_PQBoost2;
      extern float                       __SK_HDR_PQBoost3;
      cbuffer_cspace.pqBoostParams [0] = __SK_HDR_PQBoost0;
      cbuffer_cspace.pqBoostParams [1] = __SK_HDR_PQBoost1;
      cbuffer_cspace.pqBoostParams [2] = __SK_HDR_PQBoost2;
      cbuffer_cspace.pqBoostParams [3] = __SK_HDR_PQBoost3;

    static FLOAT         kfBlendFactors [] = { 0.0f, 0.0f, 0.0f, 0.0f };

    pCommandList->SetGraphicsRootSignature          ( pHDRSignature                                  );
    pCommandList->SetPipelineState                  ( pHDRPipeline                                   );
    pCommandList->SetGraphicsRoot32BitConstants     ( 0, 4,  &cbuffer_luma,   0                      );
    pCommandList->SetGraphicsRoot32BitConstants     ( 1, 16, &cbuffer_cspace, 0                      );

    pCommandList->OMSetBlendFactor                  ( kfBlendFactors                                 );
    pCommandList->IASetPrimitiveTopology            ( D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP           );

    // pRenderOutput is expected to promote from STATE_PRESENT to STATE_COPY_SOURCE without a barrier.
    pCommandList->CopyResource                      (     stagingFrame.hdr.pSwapChainCopy.p,
                                                          stagingFrame.     pRenderOutput.p          );
    pCommandList->ResourceBarrier                   ( 2,  stagingFrame.hdr.barriers.process          );
    pCommandList->SetDescriptorHeaps                ( 1, &descriptorHeaps.pHDR.p                     );
    pCommandList->SetGraphicsRootDescriptorTable    ( 2,  stagingFrame.hdr.hSwapChainCopy_GPU        );
    pCommandList->OMSetRenderTargets                ( 1, &stagingFrame.hRenderOutput, FALSE, nullptr );
    pCommandList->RSSetViewports                    ( 1, &stagingFrame.hdr.vp                        );
    pCommandList->RSSetScissorRects                 ( 1, &stagingFrame.hdr.scissor                   );
    pCommandList->DrawInstanced                     ( 3, 1, 0, 0                                     );
    pCommandList->ResourceBarrier                   ( 1,  stagingFrame.hdr.barriers.copy_end         );
  }

  else
    transition_state (pCommandList, stagingFrame.pRenderOutput, D3D12_RESOURCE_STATE_PRESENT,
                                                                D3D12_RESOURCE_STATE_RENDER_TARGET,
                                                                D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES);

  // Queue-up Pre-SK OSD Screenshots
  SK_Screenshot_ProcessQueue  (SK_ScreenshotStage::BeforeGameHUD, rb); // Before Game HUD (meaningless in D3D12)
  SK_Screenshot_ProcessQueue  (SK_ScreenshotStage::BeforeOSD,     rb);

  extern void SK_D3D12_WriteResources (void);
              SK_D3D12_WriteResources ();

  SK_D3D12_CommitUploadQueue (pCommandList);

  extern DWORD SK_ImGui_DrawFrame ( DWORD dwFlags, void* user    );
               SK_ImGui_DrawFrame (       0x00,          nullptr );

  // Queue-up Post-SK OSD Screenshots
  SK_Screenshot_ProcessQueue  (SK_ScreenshotStage::EndOfFrame, rb);

  //if (! bHDR)
    transition_state   (pCommandList, stagingFrame.pRenderOutput, D3D12_RESOURCE_STATE_RENDER_TARGET,
                                                                  D3D12_RESOURCE_STATE_PRESENT,
                                                                  D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES);

  stagingFrame.exec_cmd_list ();

  if ( const UINT64 sync_value = stagingFrame.fence.value + 1;
         SUCCEEDED ( _pCommandQueue->Signal (
                                 stagingFrame.fence.p,
                    sync_value              )
                   )
     )
  {
    stagingFrame.fence.value =
      sync_value;
  }

  SK_RunOnce (SK_ApplyQueuedHooks ());
}

bool
SK_D3D12_RenderCtx::FrameCtx::begin_cmd_list (const SK_ComPtr <ID3D12PipelineState>& state)
{
  if (bCmdListRecording)
  {
    if (state != nullptr) // Update pipeline state if requested
      pCmdList->SetPipelineState (state.p);

    return true;
  }

  // Reset command list using current command allocator and put it into the recording state
  bCmdListRecording =
    SUCCEEDED (pCmdList->Reset (pCmdAllocator.p, state.p));

  if (bCmdListRecording)
      pCmdList->SetPredication (nullptr, 0, D3D12_PREDICATION_OP_EQUAL_ZERO);

  return
    bCmdListRecording;
}

void
SK_D3D12_RenderCtx::FrameCtx::exec_cmd_list (void)
{
  assert (bCmdListRecording);

  if (FAILED (pCmdList->Close ()))
    return;

  bCmdListRecording = false;

  ID3D12CommandList* const cmd_lists [] = {
    pCmdList.p
  };

  // If we are doing this in the wrong order (i.e. during teardown),
  //   do not attempt to execute the command list, just close it.
  //
  //  Failure to skip execution will result in device removal
  if (pRoot->_pSwapChain->GetCurrentBackBufferIndex () == iBufferIdx)
  {
    pRoot->_pCommandQueue->ExecuteCommandLists (
      ARRAYSIZE (cmd_lists),
                 cmd_lists
    );
  }

  else
  {
    _d3d12_rbk->release (pRoot->_pSwapChain.p);
  }
}

bool
SK_D3D12_RenderCtx::FrameCtx::wait_for_gpu (void)
{
  // Flush command list, to avoid it still referencing resources that may be destroyed after this call
  if (bCmdListRecording)
  {
    exec_cmd_list ();
  }

  // Increment fence value to ensure it has not been signaled before
  const UINT64 sync_value =
    fence.value + 1;

  if (! fence.event)
    return false;

  if ( FAILED (
         pRoot->_pCommandQueue->Signal (
           fence.p, sync_value         )
              )
     )
  {
    return false; // Cannot wait on fence if signaling was not successful
  }

  if ( SUCCEEDED (
         fence->SetEventOnCompletion (
           sync_value, fence.event   )
                 )
     )
  {
    SK_WaitForSingleObject (fence.event, INFINITE);
  }

  // Update CPU side fence value now that it is guaranteed to have come through
  fence.value =
    sync_value;

  return true;
}

SK_D3D12_RenderCtx::FrameCtx::~FrameCtx (void)
{
  // Execute and wait for any cmds on the current pending swap,
  //   everything else can be destroyed with no sync.
  if (                       this->pRoot          == nullptr ||
       ! SK_ValidatePointer (this->pRoot->_pSwapChain, true) ||
                             this->pRoot->_pSwapChain->GetCurrentBackBufferIndex () != iBufferIdx )
  {
    bCmdListRecording = false;
  }

  wait_for_gpu               ();

  pCmdList.Release           ();
  pCmdAllocator.Release      ();
  bCmdListRecording     = false;

  pRenderOutput.Release      ();
  hdr.pSwapChainCopy.Release ();

  fence.Release ();
  fence.value  = 0;

  if (fence.event != 0)
  {
    SK_CloseHandle (fence.event);
                    fence.event = 0;
  }
}

#include <d3d12sdklayers.h>
#include <SpecialK/render/dxgi/dxgi_swapchain.h>

void
SK_D3D12_RenderCtx::release (IDXGISwapChain *pSwapChain)
{
  //SK_ComPtr <IDXGISwapChain> pSwapChain_ (pSwapChain);
  //
  //UINT _size =
  //      sizeof (LPVOID);
  //
  //SK_ComPtr <IUnknown> pUnwrapped;
  //
  //if ( pSwapChain != nullptr &&
  //     SUCCEEDED (
  //       pSwapChain->GetPrivateData (
  //         IID_IUnwrappedDXGISwapChain, &_size,
  //            &pUnwrapped
  //               )                   )
  //   )
  //{
  //}

  if ( (_pSwapChain.p != nullptr && pSwapChain == nullptr) ||
        _pSwapChain.IsEqualObject  (pSwapChain)            )//||
        //_pSwapChain.IsEqualObject  (pUnwrapped) )
  {
    if (SK_IsDebuggerPresent ())
    {
      SK_ComQIPtr <ID3D12DebugDevice>
                        pDebugDevice (_pDevice.p);

      if (pDebugDevice)
          pDebugDevice->ReportLiveDeviceObjects ( D3D12_RLDO_SUMMARY | D3D12_RLDO_DETAIL|
                                                  D3D12_RLDO_IGNORE_INTERNAL );
    }

    SK_D3D12_EndFrame (SK_TLS_Bottom ());


    ImGui_ImplDX12_Shutdown ();

    ///// 1 frame delay for re-init
    ///frame_delay.fetch_add (1);

    // Steam overlay is releasing references to the SwapChain it did not acquire (!!)
    if (! SK_ValidatePointer (_pSwapChain.p, true))
                              _pSwapChain.p = nullptr;

    if (! SK_ValidatePointer (_pDevice.p, true))
                              _pDevice.p = nullptr;

    frames_.clear ();

    // Do this after closing the command lists (frames_.clear ())
    pHDRPipeline.Release                 ();
    pHDRSignature.Release                ();

    descriptorHeaps.pBackBuffers.Release ();
    descriptorHeaps.pImGui.Release       ();
    descriptorHeaps.pHDR.Release         ();

    _pSwapChain.Release ();
    _pDevice.Release    ();
  }
}

bool
SK_D3D12_RenderCtx::init (IDXGISwapChain3 *pSwapChain, ID3D12CommandQueue *pCommandQueue)
{
  // This is the first time we've seen this device (unless something really funky's going on)
  if ( _pCommandQueue.p == nullptr &&
        pCommandQueue   != nullptr )
  {
    _pCommandQueue =
     pCommandQueue;
  }


  // Delay (re-)init for issues with Ubisoft games
  if ( frame_delay.fetch_sub (1) > 0 ) return false;
  else frame_delay.exchange  (0);


  if (_pDevice.p == nullptr)
  {
    if ( _pCommandQueue.p != nullptr && SUCCEEDED (
           _pCommandQueue->GetDevice ( IID_PPV_ARGS (&_pDevice.p) )
                                                  )
       )
    {
      _pSwapChain =
       pSwapChain;

      static auto& rb =
        SK_GetCurrentRenderBackend ();

      if (rb.swapchain == nullptr)
      {
        rb.swapchain           = pSwapChain;
        rb.device              = _pDevice.p;
        rb.d3d12.command_queue = _pCommandQueue.p;
        rb.api                 = SK_RenderAPI::D3D12;
      }
    }
  }

  if (! _pSwapChain.p)
  {
    return false;
  }

  else
  {
    DXGI_SWAP_CHAIN_DESC1  swapDesc1 = { };
    pSwapChain->GetDesc1 (&swapDesc1);

    SK_ReleaseAssert (swapDesc1.BufferCount > 0);
    frames_.resize   (swapDesc1.BufferCount);

    try
    {
      ThrowIfFailed (
        _pDevice->CreateDescriptorHeap (
          std::array < D3D12_DESCRIPTOR_HEAP_DESC,                1 >
            {          D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,    swapDesc1.BufferCount,
                       D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE, 0 }.data (),
                                       IID_PPV_ARGS (&descriptorHeaps.pImGui.p)));
                              SK_D3D12_SetDebugName ( descriptorHeaps.pImGui.p,
                                        L"ImGui D3D12 Descriptor Heap" );
      ThrowIfFailed (
        _pDevice->CreateDescriptorHeap (
          std::array < D3D12_DESCRIPTOR_HEAP_DESC,                1 >
            {          D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,    swapDesc1.BufferCount,
                       D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE, 0 }.data (),
                                       IID_PPV_ARGS (&descriptorHeaps.pHDR.p)));
                              SK_D3D12_SetDebugName ( descriptorHeaps.pHDR.p,
                                        L"SK D3D12 HDR Descriptor Heap" );

      ThrowIfFailed (
        _pDevice->CreateDescriptorHeap (
          std::array < D3D12_DESCRIPTOR_HEAP_DESC,                1 >
            {          D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,    swapDesc1.BufferCount,
                       D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE, 0 }.data (),
                                       IID_PPV_ARGS (&descriptorHeaps.pHDR_CopyAssist.p)));
                              SK_D3D12_SetDebugName ( descriptorHeaps.pHDR_CopyAssist.p,
                                        L"SK D3D12 HDR Copy Descriptor Heap" );
      ThrowIfFailed (
        _pDevice->CreateDescriptorHeap (
          std::array < D3D12_DESCRIPTOR_HEAP_DESC,                1 >
            {          D3D12_DESCRIPTOR_HEAP_TYPE_RTV,            swapDesc1.BufferCount,
                       D3D12_DESCRIPTOR_HEAP_FLAG_NONE,           0 }.data (),
                                       IID_PPV_ARGS (&descriptorHeaps.pBackBuffers.p)));
                              SK_D3D12_SetDebugName ( descriptorHeaps.pBackBuffers.p,
                                        L"SK D3D12 Backbuffer Descriptor Heap" );

      const auto  rtvDescriptorSize =
        _pDevice->GetDescriptorHandleIncrementSize (D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
      const auto  srvDescriptorSize =
        _pDevice->GetDescriptorHandleIncrementSize (D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

      auto        rtvHandle         =
        descriptorHeaps.pBackBuffers->GetCPUDescriptorHandleForHeapStart ();

      int bufferIdx = 0;

      for ( auto& frame : frames_ )
      {
        frame.iBufferIdx =
               bufferIdx++;

        ThrowIfFailed (
          _pDevice->CreateFence (0, D3D12_FENCE_FLAG_NONE,          IID_PPV_ARGS (&frame.fence.p)));
        ThrowIfFailed (
          _pDevice->CreateCommandAllocator (
                                    D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS (&frame.pCmdAllocator.p)));

        SK_ComQIPtr <ID3D12Device4>
                          pDevice4 (_pDevice);

        if (! pDevice4) {
          ThrowIfFailed (
            _pDevice->CreateCommandList ( 0,
                                    D3D12_COMMAND_LIST_TYPE_DIRECT,                frame.pCmdAllocator.p, nullptr,
                                                                    IID_PPV_ARGS (&frame.pCmdList.p)));
          ThrowIfFailed (                                                          frame.pCmdList->Close ());
        } else {
          ThrowIfFailed ( pDevice4->CreateCommandList1 ( 0,
                                    D3D12_COMMAND_LIST_TYPE_DIRECT,
                                    D3D12_COMMAND_LIST_FLAG_NONE,   IID_PPV_ARGS (&frame.pCmdList.p)));
        }

        ThrowIfFailed (
          _pSwapChain->GetBuffer (frame.iBufferIdx,                 IID_PPV_ARGS (&frame.pRenderOutput.p)));

        frame.hRenderOutput.ptr =
                  rtvHandle.ptr + frame.iBufferIdx * rtvDescriptorSize;

        _pDevice->CreateRenderTargetView ( frame.pRenderOutput, nullptr,
                                           frame.hRenderOutput );

        ThrowIfFailed (
          _pDevice->CreateCommittedResource (
            std::array < D3D12_HEAP_PROPERTIES, 1 >  {
                         D3D12_HEAP_TYPE_DEFAULT, D3D12_CPU_PAGE_PROPERTY_UNKNOWN,
                                                  D3D12_MEMORY_POOL_UNKNOWN,
                                              0, 1   }.data (),
                         D3D12_HEAP_FLAG_NONE,
            std::array < D3D12_RESOURCE_DESC,   1 > {
                         D3D12_RESOURCE_DIMENSION_TEXTURE2D,
                                         0,
                           swapDesc1.Width, swapDesc1.Height,
                                         1,                1,
                           swapDesc1.Format, { 1, 0 }, D3D12_TEXTURE_LAYOUT_UNKNOWN,
                                                       D3D12_RESOURCE_FLAG_NONE }.data (),
                           D3D12_RESOURCE_STATE_COPY_DEST, nullptr,
                                                IID_PPV_ARGS (&frame.hdr.pSwapChainCopy.p)));

        frame.hdr.vp.Width       = static_cast <FLOAT> (swapDesc1.Width);
        frame.hdr.vp.Height      = static_cast <FLOAT> (swapDesc1.Height);
        frame.hdr.scissor.right  =                      swapDesc1.Width;
        frame.hdr.scissor.bottom =                      swapDesc1.Height;

        frame.hdr.hSwapChainCopy_CPU.ptr =
          descriptorHeaps.pHDR->GetCPUDescriptorHandleForHeapStart ().ptr +
                                   srvDescriptorSize                      * frame.iBufferIdx;

        frame.hdr.hSwapChainCopy_GPU.ptr =
          descriptorHeaps.pHDR->GetGPUDescriptorHandleForHeapStart ().ptr +
                                   srvDescriptorSize                      * frame.iBufferIdx;

        frame.hdr.hBufferCopy_CPU.ptr =
          descriptorHeaps.pHDR_CopyAssist->GetCPUDescriptorHandleForHeapStart ().ptr +
                                   srvDescriptorSize                      * frame.iBufferIdx;

        frame.hdr.hBufferCopy_GPU.ptr =
          descriptorHeaps.pHDR_CopyAssist->GetGPUDescriptorHandleForHeapStart ().ptr +
                                   srvDescriptorSize                      * frame.iBufferIdx;

        _pDevice->CreateShaderResourceView ( frame.hdr.pSwapChainCopy.p, nullptr,
                                             frame.hdr.hSwapChainCopy_CPU
        );

        // As long as HDR processing comes first, we can implicitly transition the swapchain from
        //   STATE_PRESENT to D3D12_RESOURCE_STATE_COPY_SOURCE
        frame.hdr.barriers.copy_end [0].Transition.pResource = frame.hdr.pSwapChainCopy.p;
        frame.hdr.barriers.process  [0].Transition.pResource = frame.hdr.pSwapChainCopy.p;
        frame.hdr.barriers.process  [1].Transition.pResource =     frame.pRenderOutput.p;

        frame.pRoot       = this;

        frame.fence.value = 0;
        frame.fence.event =
          SK_CreateEvent ( nullptr, FALSE, FALSE, nullptr );

        struct {        ID3D12Object *pObj;
                 const std::wstring   kName;
        } _debugObjects [] =
          {
            { frame.pRenderOutput.p,      L"SK D3D12 SwapChain Buffer" },
            { frame.hdr.pSwapChainCopy.p, L"SK D3D12 HDR Copy Buffer"  },
            { frame.pCmdAllocator.p,      L"SK D3D12 CmdAllocator"     },
            { frame.pCmdList.p,           L"SK D3D12 CmdList"          },
            { frame.fence.p,              L"SK D3D12 Fence"            },
          };

        for ( auto& _obj : _debugObjects )
        {
          SK_D3D12_SetDebugName (
            _obj.pObj,
            _obj.kName + std::to_wstring (frame.iBufferIdx)
          );
        }

        frame.bCmdListRecording = false;
      }

      // Create the root signature
      D3D12_DESCRIPTOR_RANGE
        srv_range                    = {};
        srv_range.RangeType          = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
        srv_range.NumDescriptors     = 2;
        srv_range.BaseShaderRegister = 0; // t0, t1 (texLastFrame0)

      D3D12_ROOT_PARAMETER
        param [3]                                     = { };

        param [0].ParameterType                       = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
        param [0].Constants.ShaderRegister            = 0;
        param [0].Constants.RegisterSpace             = 0;
        param [0].Constants.Num32BitValues            = 4;// cbuffer vertexBuffer : register (b0)
        param [0].ShaderVisibility                    = D3D12_SHADER_VISIBILITY_VERTEX;

        param [1].ParameterType                       = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
        param [1].Constants.ShaderRegister            = 0;
        param [1].Constants.RegisterSpace             = 0;
        param [1].Constants.Num32BitValues            = 16;// cbuffer colorSpaceTransform : register (b0)
        param [1].ShaderVisibility                    = D3D12_SHADER_VISIBILITY_PIXEL;

        param [2].ParameterType                       = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        param [2].DescriptorTable.NumDescriptorRanges = 1;
        param [2].DescriptorTable.pDescriptorRanges   = &srv_range;
        param [2].ShaderVisibility                    = D3D12_SHADER_VISIBILITY_PIXEL;

      D3D12_STATIC_SAMPLER_DESC
        staticSampler = { };

        staticSampler.Filter           = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
        staticSampler.AddressU         = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        staticSampler.AddressV         = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        staticSampler.AddressW         = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        staticSampler.MipLODBias       = 0.f;
        staticSampler.MaxAnisotropy    = 0;
        staticSampler.ComparisonFunc   = D3D12_COMPARISON_FUNC_ALWAYS;
        staticSampler.BorderColor      = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
        staticSampler.MinLOD           = 0.f;
        staticSampler.MaxLOD           = 0.f;
        staticSampler.ShaderRegister   = 0;
        staticSampler.RegisterSpace    = 0;
        staticSampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

      D3D12_ROOT_SIGNATURE_DESC
        desc                   = { };

        desc.NumParameters     = _countof (param);
        desc.pParameters       =           param;
        desc.NumStaticSamplers =  1;
        desc.pStaticSamplers   = &staticSampler;

        desc.Flags             =
        ( D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS       |
          D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS     |
          D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS );

      SK_ComPtr <ID3DBlob>
                    pBlob;

      ThrowIfFailed (
        D3D12SerializeRootSignature (
          &desc, D3D_ROOT_SIGNATURE_VERSION_1,
            &pBlob.p, nullptr )     );

      ThrowIfFailed (
        _pDevice->CreateRootSignature (
          0, pBlob->GetBufferPointer (),
             pBlob->GetBufferSize    (),
                 IID_PPV_ARGS (&pHDRSignature.p))
      ); SK_D3D12_SetDebugName (pHDRSignature.p, L"SK HDR Root Signature");

      D3D12_GRAPHICS_PIPELINE_STATE_DESC
        psoDesc                       = { };
        psoDesc.NodeMask              = 1;
        psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        psoDesc.pRootSignature        = pHDRSignature;
        psoDesc.SampleMask            = UINT_MAX;
        psoDesc.NumRenderTargets      = 1;
        psoDesc.RTVFormats [0]        = swapDesc1.Format;
        psoDesc.SampleDesc.Count      = 1;
        psoDesc.Flags                 = D3D12_PIPELINE_STATE_FLAG_NONE;

        psoDesc.VS = { colorutil_vs_bytecode,
               sizeof (colorutil_vs_bytecode) /
               sizeof (colorutil_vs_bytecode) [0]
                     };

        psoDesc.PS = { uber_hdr_shader_ps_bytecode,
               sizeof (uber_hdr_shader_ps_bytecode) /
               sizeof (uber_hdr_shader_ps_bytecode) [0]
                     };

      // Create the blending setup
      D3D12_BLEND_DESC&
        blendDesc                                        = psoDesc.BlendState;
        blendDesc.AlphaToCoverageEnable                  = false;
        blendDesc.RenderTarget [0].BlendEnable           = true;
        blendDesc.RenderTarget [0].SrcBlend              = D3D12_BLEND_ONE;
        blendDesc.RenderTarget [0].DestBlend             = D3D12_BLEND_ZERO;
        blendDesc.RenderTarget [0].BlendOp               = D3D12_BLEND_OP_ADD;
        blendDesc.RenderTarget [0].SrcBlendAlpha         = D3D12_BLEND_ONE;//INV_SRC_ALPHA;
        blendDesc.RenderTarget [0].DestBlendAlpha        = D3D12_BLEND_ZERO;
        blendDesc.RenderTarget [0].BlendOpAlpha          = D3D12_BLEND_OP_ADD;
        blendDesc.RenderTarget [0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_RED   |
                                                           D3D12_COLOR_WRITE_ENABLE_GREEN |
                                                           D3D12_COLOR_WRITE_ENABLE_BLUE;

      // Create the rasterizer state
      D3D12_RASTERIZER_DESC&
        rasterDesc                       = psoDesc.RasterizerState;
        rasterDesc.FillMode              = D3D12_FILL_MODE_SOLID;
        rasterDesc.CullMode              = D3D12_CULL_MODE_NONE;
        rasterDesc.FrontCounterClockwise = FALSE;
        rasterDesc.DepthBias             = D3D12_DEFAULT_DEPTH_BIAS;
        rasterDesc.DepthBiasClamp        = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
        rasterDesc.SlopeScaledDepthBias  = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
        rasterDesc.DepthClipEnable       = true;
        rasterDesc.MultisampleEnable     = FALSE;
        rasterDesc.AntialiasedLineEnable = FALSE;
        rasterDesc.ForcedSampleCount     = 0;
        rasterDesc.ConservativeRaster    = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

      // Create depth-stencil State
      D3D12_DEPTH_STENCIL_DESC&
        depthDesc                              = psoDesc.DepthStencilState;
        depthDesc.DepthEnable                  = false;
        depthDesc.DepthWriteMask               = D3D12_DEPTH_WRITE_MASK_ZERO;
        depthDesc.DepthFunc                    = D3D12_COMPARISON_FUNC_NEVER;
        depthDesc.StencilEnable                = false;
        depthDesc.FrontFace.StencilFailOp      =
        depthDesc.FrontFace.StencilDepthFailOp =
        depthDesc.FrontFace.StencilPassOp      = D3D12_STENCIL_OP_KEEP;
        depthDesc.FrontFace.StencilFunc        = D3D12_COMPARISON_FUNC_NEVER;
        depthDesc.BackFace                     =
        depthDesc.FrontFace;

      ThrowIfFailed (
        _pDevice->CreateGraphicsPipelineState (
          &psoDesc, IID_PPV_ARGS (&pHDRPipeline.p)
       ));SK_D3D12_SetDebugName  ( pHDRPipeline.p, L"SK HDR Pipeline State" );

      HWND                   hWnd = HWND_BROADCAST;
      _pSwapChain->GetHwnd (&hWnd);

      if (
        ImGui_ImplDX12_Init ( _pDevice,
                              swapDesc1.BufferCount,
                                swapDesc1.Format,
          descriptorHeaps.pImGui->GetCPUDescriptorHandleForHeapStart (),
          descriptorHeaps.pImGui->GetGPUDescriptorHandleForHeapStart (), hWnd)
         )
      {
        ImGui_ImplDX12_CreateDeviceObjects ();

        return true;
      }
    }

    catch (const SK_ComException& e)
    {
      SK_LOG0 ( ( L"SK D3D12 Init Failed: %hs", e.what () ),
                  __SK_SUBSYSTEM__ );
    }
  }

  return false;
}

SK_LazyGlobal <SK_D3D12_RenderCtx> _d3d12_rbk;



template <typename _T>
HRESULT
SK_D3D12_GET_OBJECT_NAME_N ( ID3D12Object *pObject,
                             UINT         *pBytes,
                             _T           *pName = nullptr );

template <>
HRESULT
SK_D3D12_GET_OBJECT_NAME_N ( ID3D12Object *pObject,
                             UINT         *pBytes,
                             char         *pName )
{
  if ( (! pObject) ||
       (! pBytes ) )
    return E_POINTER;

  return
    pObject->GetPrivateData (
      WKPDID_D3DDebugObjectName,
        pBytes,
          pName );
}

template <>
HRESULT
SK_D3D12_GET_OBJECT_NAME_N ( ID3D12Object *pObject,
                             UINT         *pBytes,
                             wchar_t      *pName )
{
  if ( (! pObject) ||
       (! pBytes ) )
    return E_POINTER;

  return
    pObject->GetPrivateData (
      WKPDID_D3DDebugObjectNameW,
        pBytes,
          pName );
}

bool
SK_D3D12_HasDebugName (ID3D12Object* pD3D12Obj)
{
  UINT uiNameLen = 0;

  if ( FAILED ( pD3D12Obj->GetPrivateData (WKPDID_D3DDebugObjectNameW, &uiNameLen, nullptr) ) &&
       FAILED ( pD3D12Obj->GetPrivateData (WKPDID_D3DDebugObjectName,  &uiNameLen, nullptr) ) )
    return false;

  return
    ( uiNameLen > 0 );
}

template <typename _T>
std::basic_string <_T>
SK_D3D12_GetDebugName (ID3D12Object* pD3D12Obj)
{
  if (pD3D12Obj != nullptr)
  {
    UINT bufferLen = 0;

    if ( SUCCEEDED (
           SK_D3D12_GET_OBJECT_NAME_N <_T> ( pD3D12Obj,
                   &bufferLen, nullptr )
         )
       )
    {
      if (bufferLen >= sizeof (_T))
      {
        std::basic_string <_T>
          name (
            bufferLen / sizeof (_T),
                               (_T)0 );

        if ( SUCCEEDED (
               SK_D3D12_GET_OBJECT_NAME_N <_T> ( pD3D12Obj,
                       &bufferLen, name.data () )
             )
           )
        {
          return name;
        }
      }
    }
  }

  return
    std::basic_string <_T> (0);
}

void
SK_D3D12_SetDebugName (       ID3D12Object* pD3D12Obj,
                        const std::wstring&     kName )
{
  if (pD3D12Obj != nullptr && kName.size () > 0)
  {
#if 1
    D3D_SET_OBJECT_NAME_N_W ( pD3D12Obj,
                   static_cast <UINT> ( kName.size () ),
                                        kName.data ()
                            );
    std::string utf8_copy =
      SK_WideCharToUTF8 (kName);

    D3D_SET_OBJECT_NAME_N_A ( pD3D12Obj,
                   static_cast <UINT> ( (utf8_copy).size () ),
                                        (utf8_copy).data ()
                            );

    SK_LOGi0 (
      L"Created D3D12 Object: %ws", kName.c_str ()
    );
#else
    pD3D12Obj->SetName ( kName.c_str () );
#endif
  }
}
