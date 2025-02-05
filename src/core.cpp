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

#include <SpecialK/render/backend.h>
#include <SpecialK/render/d3d9/d3d9_backend.h>
#include <SpecialK/render/d3d11/d3d11_core.h>
#include <SpecialK/render/d3d12/d3d12_interfaces.h>
#include <SpecialK/render/dstorage/dstorage.h>
#include <SpecialK/render/dxgi/dxgi_util.h>
#include <SpecialK/render/ngx/ngx.h>
#include <reflex/pclstats.h>

#include <SpecialK/storefront/epic.h>

#include <SpecialK/nvapi.h>
#include <nvapi/NvApiDriverSettings.h>
#include <SpecialK/adl.h>
#include <SpecialK/popups/popup.h>


#include <SpecialK/commands/mem.inl>
#include <SpecialK/commands/update.inl>

#include <filesystem>


#ifdef _WIN64
#pragma comment (lib, R"(depends\lib\DirectXTex\x64\DirectXTex.lib)")
#pragma comment (lib, R"(depends\lib\MinHook\x64\libMinHook64.lib)")
#pragma comment (lib, R"(depends\lib\lzma\x64\libzma.lib)")
#else
#pragma comment (lib, R"(depends\lib\DirectXTex\Win32\DirectXTex.lib)")
#pragma comment (lib, R"(depends\lib\MinHook\Win32\libMinHook.lib)")
#pragma comment (lib, R"(depends\lib\lzma\Win32\libzma.lib)")
#endif

volatile HANDLE   hInitThread     = { INVALID_HANDLE_VALUE };
volatile DWORD    dwInitThreadId  =   1;

         BOOL     nvapi_init      = FALSE;
         HMODULE  backend_dll     = nullptr;

volatile LONG     __SK_Init       = FALSE;
         bool     __SK_bypass     = false;
   const wchar_t* __SK_BootedCore = L"";
  extern bool     __SK_RunDLL_Bypass;

  extern float    __target_fps;

         BOOL     __SK_DisableQuickHook = FALSE;


//#define _THREADED_BASIC_INIT

struct init_params_s {
  std::wstring  backend    = L"INVALID";
  void*         callback   =    nullptr;
  LARGE_INTEGER start_time {   0,   0 };
};


init_params_s&
SKX_GetInitParams (void)
{
  static init_params_s params = { };
  return               params;
};

const init_params_s&
SK_GetInitParams (void)
{
  return SKX_GetInitParams ();
};


wchar_t*
SKX_GetBackend (void)
{
  static wchar_t SK_Backend [128] = { };
  return         SK_Backend;
}

const wchar_t*
__stdcall
SK_GetBackend (void)
{
  return SKX_GetBackend ();
}

const wchar_t*
__stdcall
SK_SetBackend (const wchar_t* wszBackend)
{
  SK_ReleaseAssert (wszBackend != nullptr);

  if (wszBackend == nullptr)
    return L"InvalidPointer";

  wcsncpy_s ( SKX_GetBackend (), 127,
                wszBackend,      _TRUNCATE );

  return
    SKX_GetBackend ();
}

wchar_t*
SKX_GetRootPath (void)
{
  static wchar_t SK_RootPath [MAX_PATH + 2] = { };
  return         SK_RootPath;
}

const wchar_t*
__stdcall
SK_GetRootPath (void)
{
  return SKX_GetRootPath ();
}


wchar_t*
SKX_GetInstallPath (void)
{
  static wchar_t SK_InstallPath [MAX_PATH + 2] = { };
  return         SK_InstallPath;
}

const wchar_t*
__stdcall
SK_GetInstallPath (void)
{
  return SKX_GetInstallPath ();
}



wchar_t*
SKX_GetNaiveConfigPath (void)
{
  static wchar_t SK_ConfigPath [MAX_PATH + 2] = { };
  return         SK_ConfigPath;
}

__declspec (noinline)
const wchar_t*
__stdcall
SK_GetNaiveConfigPath (void)
{
  return SKX_GetNaiveConfigPath ();
}

//
// To be used internally only, by the time any plug-in has
//   been activated, Special K will have already established
//     this path and loaded its config.
//
void
__stdcall
SK_SetConfigPath (const wchar_t* path)
{
  if (path != nullptr)
  {
    wcsncpy_s ( SKX_GetNaiveConfigPath (), MAX_PATH,
                  path,                    _TRUNCATE );
  }

  SK_ReleaseAssert (path != nullptr);
}

void
__stdcall
SK_StartPerfMonThreads (void)
{
  // Handle edge-case that might re-spawn WMI threads during de-init.
  if (ReadAcquire (&__SK_DLL_Ending))
    return;

  auto SpawnMonitorThread =
  []( volatile HANDLE                 *phThread,
      const    wchar_t                *wszName,
               LPTHREAD_START_ROUTINE  pThunk ) ->
  bool
  {
    if ( INVALID_HANDLE_VALUE ==
           InterlockedCompareExchangePointer (phThread, nullptr, INVALID_HANDLE_VALUE)
       )
    {
      dll_log->LogEx (true, L"[ Perfmon. ] Spawning %ws...  ", wszName);

      InterlockedExchangePointer ( (void **)phThread,
        SK_Thread_CreateEx ( pThunk )
      );

      if (ReadPointerAcquire (phThread) != INVALID_HANDLE_VALUE)
      {
        dll_log->LogEx (false, L"tid=0x%04x\n",
                          GetThreadId (ReadPointerAcquire (phThread)));
        return true;
      }

      dll_log->LogEx (false, L"Failed!\n");
    }

    return false;
  };

  //
  // Spawn CPU Refresh Thread
  //
  if (config.cpu.show || ( SK_ImGui_Widgets->cpu_monitor != nullptr &&
                           SK_ImGui_Widgets->cpu_monitor->isActive () ))
  {
    SpawnMonitorThread ( &SK_WMI_CPUStats->hThread,
                         L"CPU Monitor",      SK_MonitorCPU      );
  }

  if (config.disk.show)
  {
    SpawnMonitorThread ( &SK_WMI_DiskStats->hThread,
                         L"Disk Monitor",     SK_MonitorDisk     );
  }

  if (config.pagefile.show)
  {
    SpawnMonitorThread ( &SK_WMI_PagefileStats->hThread,
                         L"Pagefile Monitor", SK_MonitorPagefile );
  }
}

void
SK_LoadGPUVendorAPIs (void)
{
  dll_log->LogEx (false, L"================================================"
                         L"===========================================\n" );

  // None of the GPU vendor-specific APIs work in WINE.
  //
  if (config.compatibility.using_wine)
    return;

  dll_log->Log (L"[  NvAPI   ] Initializing NVIDIA API           (NvAPI)...");

  SK_NvAPI_SetAppName         (       SK_GetFullyQualifiedApp () );
  SK_NvAPI_SetAppFriendlyName (
    app_cache_mgr->getAppNameFromID ( SK_Steam_GetAppID_NoAPI () ).c_str ()
                              );

  nvapi_init =
    sk::NVAPI::InitializeLibrary (sk::NVAPI::app_name.c_str ());

  dll_log->Log (L"[  NvAPI   ]              NvAPI Init         { %s }",
                                                     nvapi_init ? L"Success" :
                                                                  L"Failed ");

  if (nvapi_init)
  {
    if (config.apis.NvAPI.vulkan_bridge != SK_NoPreference)
    {
      SK_NvAPI_EnableVulkanBridge (config.apis.NvAPI.vulkan_bridge);
    }

    if (SK_GetModuleHandleW (L"_nvngx.dll") != nullptr)
    {
      SK_NGX_Init ();
    }

    const int num_sli_gpus =
      sk::NVAPI::CountSLIGPUs ();

    dll_log->Log ( L"[  NvAPI   ] >> NVIDIA Driver Version: %s",
                    sk::NVAPI::GetDriverVersion ().c_str () );

    const int gpu_count =
      sk::NVAPI::CountPhysicalGPUs ();

    dll_log->Log ( gpu_count > 1 ? L"[  NvAPI   ]  * Number of Installed NVIDIA GPUs: %i  "
                                   L"{ SLI: '%s' }"
                                 :
                                   L"[  NvAPI   ]  * Number of Installed NVIDIA GPUs: %i  { '%s' }",
                                     gpu_count > 1 ? num_sli_gpus :
                                                     num_sli_gpus + 1,
                                       sk::NVAPI::EnumGPUs_DXGI ()[0].Description );

    if (num_sli_gpus > 0)
    {
      DXGI_ADAPTER_DESC* sli_adapters =
        sk::NVAPI::EnumSLIGPUs ();

      int sli_gpu_idx = 0;

      while (*sli_adapters->Description != L'\0')
      {
        dll_log->Log ( L"[  NvAPI   ]   + SLI GPU %d: %s",
                         sli_gpu_idx++,
                           (sli_adapters++)->Description );
      }
    }

    //
    // Setup a framerate limiter and (if necessary) restart
    //
    bool restart = (! sk::NVAPI::SetFramerateLimit (0));

    //
    // Install SLI Override Settings
    //
    if (sk::NVAPI::CountSLIGPUs () && config.nvidia.sli.override)
    {
      if (! sk::NVAPI::SetSLIOverride
              ( SK_GetDLLRole (),
                  config.nvidia.sli.mode.c_str (),
                    config.nvidia.sli.num_gpus.c_str (),
                      config.nvidia.sli.compatibility.c_str ()
              )
         )
      {
        restart = true;
      }
    }

    if (! config.nvidia.bugs.snuffed_ansel)
    {
#if 0
      if (SK_NvAPI_DisableAnsel (SK_GetDLLRole ()))
      {
        restart = true;

        SK_MessageBox (
          L"To Avoid Potential Compatibility Issues, Special K has Disabled Ansel for this Game.\r\n\r\n"
          L"You may re-enable Ansel for this Game using the Help Menu in Special K's Control Panel.",
            L"Special K Compatibility Layer:  [ Ansel Disabled ]",
              MB_ICONWARNING | MB_OK
        );
      }
#endif

      config.nvidia.bugs.snuffed_ansel = true;

      SK_GetDLLConfig ()->write ();
    }

    if (restart)
    {
      dll_log->Log (L"[  Nv API  ] >> Restarting to apply NVIDIA driver settings <<");

      SK_ShellExecuteW ( GetDesktopWindow (),
                          L"OPEN",
                            SK_GetHostApp (),
                              nullptr,
                                nullptr,
                                  SW_SHOWDEFAULT );
      exit (0);
    }
  }

  // Not NVIDIA, maybe AMD?
  else
  {
    dll_log->Log (L"[DisplayLib] Initializing AMD Display Library (ADL)...");

    BOOL adl_init =
      SK_InitADL ();

    dll_log->Log   (L"[DisplayLib]              ADL   Init         { %s }",
                                                      adl_init ? L"Success" :
                                                                 L"Failed ");

    // Yes, AMD driver is in working order ...
    if (adl_init > 0)
    {
      dll_log->Log ( L"[DisplayLib]  * Number of Reported AMD Adapters: %i (%i active)",
                       SK_ADL_CountPhysicalGPUs (),
                         SK_ADL_CountActiveGPUs () );
    }
  }

  const HMODULE hMod =
    SK_GetModuleHandle (SK_GetHostApp ());

  if (hMod != nullptr)
  {
    const auto* dwOptimus =
      reinterpret_cast <DWORD *> (
        SK_GetProcAddress ( SK_GetHostApp (),
                              "NvOptimusEnablement" )
      );

    if (dwOptimus != nullptr)
    {
      dll_log->Log ( L"[Hybrid GPU]  NvOptimusEnablement..................: 0x%02X (%s)",
                       *dwOptimus,
                     ((*dwOptimus) & 0x1) ? L"Max Perf." :
                                            L"Don't Care" );
    }

    else
    {
      dll_log->Log (L"[Hybrid GPU]  NvOptimusEnablement..................: UNDEFINED");
    }

    const auto* dwPowerXpress =
      reinterpret_cast <DWORD *> (
        SK_GetProcAddress ( SK_GetHostApp (),
                              "AmdPowerXpressRequestHighPerformance" )
      );

    if (dwPowerXpress != nullptr)
    {
      dll_log->Log (L"[Hybrid GPU]  AmdPowerXpressRequestHighPerformance.: 0x%02X (%s)",
         *dwPowerXpress,
         (*dwPowerXpress & 0x1) ? L"High Perf." :
                                  L"Don't Care" );
    }

    else
      dll_log->Log (L"[Hybrid GPU]  AmdPowerXpressRequestHighPerformance.: UNDEFINED");

    dll_log->LogEx (false, L"================================================"
                           L"===========================================\n" );
  }
}

void SK_FetchBuiltinSounds (void)
{
  wchar_t           wszArchive  [MAX_PATH + 2] = { };
  static const auto wszDestination =
      std::filesystem::path (SK_GetInstallPath ()) /
                        LR"(Assets\Shared\Sounds\)";

  std::error_code                                      ec;
  if (! std::filesystem::exists (SK_GetInstallPath (), ec))
    return;

  static const std::filesystem::path
    predefined_sounds [] =
    {
      std::filesystem::path (wszDestination) / LR"(dream_theater.wav)",
      std::filesystem::path (wszDestination) / LR"(screenshot.wav)",
      std::filesystem::path (wszDestination) / LR"(psn_trophy.wav)",
      std::filesystem::path (wszDestination) / LR"(crash.wav)",
      std::filesystem::path (wszDestination) / LR"(xbox.wav)"
    };

  static bool incomplete_set = false;

  for ( auto& sound : predefined_sounds )
  {
    if (! std::filesystem::exists (sound, ec))
    {
      incomplete_set = true;
      break;
    }
  }

  if (! incomplete_set) return;

  SK_CreateDirectories (wszDestination.c_str ());

  wcsncpy_s   ( wszArchive,               MAX_PATH,
                wszDestination.c_str (), _TRUNCATE );
  PathAppendW ( wszArchive,          L"builtin.7z" );

  SK_RunOnce (
  SK_Network_EnqueueDownload (
    sk_download_request_s (wszArchive, R"(https://sk-data.special-k.info/sounds/builtin.7z)",
      []( const std::vector <uint8_t>&& data,
          const std::wstring_view       file ) -> bool
      {
        std::filesystem::path
                    full_path (file.data ());

        if ( FILE *fPackedSounds = _wfopen (full_path.c_str (), L"wb") ;
                   fPackedSounds != nullptr )
        {
          fwrite (data.data (), 1, data.size (), fPackedSounds);
          fclose (                               fPackedSounds);

          if ( SUCCEEDED (
                 SK_Decompress7zEx ( full_path.c_str (),
                                     full_path.parent_path (
                                            ).c_str (), nullptr )
                         )
             )
          {
            DeleteFileW (full_path.c_str ());

            incomplete_set = false;
          }
        }

        return true;
      }
    ), true // High Priority
  ));
}

void
__stdcall
SK_InitCore (std::wstring, void* callback)
{
  if (SK_IsRunDLLInvocation () || SK_GetCurrentGameID () == SK_GAME_ID::Launcher)
    return;

  using finish_pfn   = void (WINAPI *)  (void);
  using callback_pfn = void (WINAPI *)(_Releases_exclusive_lock_ (init_mutex) finish_pfn);

  const auto callback_fn =
    (callback_pfn)callback;

  init_mutex->lock ();

#ifdef _THREADED_BASIC_INIT
extern void BasicInit (void);
            BasicInit (    );
#endif

  SK_FetchBuiltinSounds ();

  switch (SK_GetCurrentGameID ())
  {
#ifdef _WIN64
    case SK_GAME_ID::NieRAutomata:
      SK_FAR_InitPlugin ();
      break;

    case SK_GAME_ID::BlueReflection:
      SK_IT_InitPlugin ();
      break;

    case SK_GAME_ID::Tales_of_Graces:
      extern void SK_TGFix_InitPlugin (void);
                  SK_TGFix_InitPlugin ();
      break;

    case SK_GAME_ID::DotHackGU:
      SK_DGPU_InitPlugin ();
      break;

    case SK_GAME_ID::NiNoKuni2:
      SK_NNK2_InitPlugin ();
      break;

    case SK_GAME_ID::Tales_of_Vesperia:
      SK_TVFix_InitPlugin ();
      break;

    case SK_GAME_ID::Sekiro:
      SK_Sekiro_InitPlugin ();
      break;

    case SK_GAME_ID::Ys_Eight:
      SK_YS8_InitPlugin ();
      break;
    case SK_GAME_ID::EldenRing:
      SK_ER_InitPlugin ();
      break;
    case SK_GAME_ID::Starfield:
    case SK_GAME_ID::Oblivion:
    case SK_GAME_ID::Fallout3:
    case SK_GAME_ID::FalloutNewVegas:
      SK_BGS_InitPlugin ();
      break;
    case SK_GAME_ID::LordsOfTheFallen2:
      SK_LOTF2_InitPlugin ();
      break;
    case SK_GAME_ID::StarOcean2R:
      SK_SO2R_InitPlugin ();
      break;
    case SK_GAME_ID::Metaphor:
      SK_Metaphor_InitPlugin ();
      break;
#else
    case SK_GAME_ID::SecretOfMana:
      SK_SOM_InitPlugin ();
      break;

    case SK_GAME_ID::DragonBallFighterZ:
    {
      wchar_t      wszPath       [MAX_PATH + 2] = { };
      wchar_t      wszWorkingDir [MAX_PATH + 2] = { };
      wcscpy      (wszWorkingDir, SK_GetHostPath  ());
      PathAppendW (wszWorkingDir, LR"(RED\Binaries\Win64\)");

      wcscpy      (wszPath,       wszWorkingDir);
      PathAppendW (wszPath,       L"RED-Win64-Shipping.exe");

      SK_ShellExecuteW (nullptr, L"open", wszPath, L"-eac-nop-loaded", wszWorkingDir, SW_SHOWNORMAL);
      ExitProcess      (0);
    } break;

    case SK_GAME_ID::ChronoCross:
    {
      SK_CC_InitPlugin ();
    } break;
#endif
  }

  // Setup the compatibility back end, which monitors loaded libraries,
  //   blacklists bad DLLs and detects render APIs...
  SK_EnumLoadedModules  (SK_ModuleEnum::PostLoad);
  SK_Memory_InitHooks   ();

  if (SK_GetDLLRole () != DLL_ROLE::DInput8)
  {
    if (SK_GetModuleHandle (L"dinput8.dll"))
      SK_Input_HookDI8  ();

    if (SK_GetModuleHandle (L"dinput.dll"))
      SK_Input_HookDI7  ();
  }

  SK_Input_Init ();

  if (sk::NVAPI::nv_hardware)
  {
    if (config.render.framerate.target_fps_bg > 0.0f)
    {
      SK_NvAPI_DRS_SetDWORD (VSYNCMODE_ID, VSYNCMODE_PASSIVE);
    }
  }

  void
     __stdcall SK_InitFinishCallback (void);
  callback_fn (SK_InitFinishCallback);
}

// This God awful code is lockless and safe to call from anywhere, but please do not.
void
WaitForInit (void)
{
  constexpr auto _SpinMax = 32;

  LONG init =
    ReadAcquire (&__SK_Init);

  if (init != 0)
    return;

  const DWORD dwThreadId =
    GetCurrentThreadId ();

  while (ReadPointerAcquire (&hInitThread) != INVALID_HANDLE_VALUE)
  {
    const DWORD dwInitTid =
      ReadULongAcquire (&dwInitThreadId);

    if ( dwInitTid                == dwThreadId ||
         dwInitTid                == 0          ||
         ReadAcquire (&__SK_Init) == TRUE )
    {
      break;
    }

    for (int i = 0; i < _SpinMax && (ReadPointerAcquire (&hInitThread) != INVALID_HANDLE_VALUE); i++)
      YieldProcessor ();

    HANDLE hWait =
      ReadPointerAcquire (&hInitThread);

    if ( hWait == INVALID_HANDLE_VALUE )
      break;

    HANDLE hWaitArray [] = {
           hWait
    };

    const DWORD dwWaitStatus =
      MsgWaitForMultipleObjectsEx (1, hWaitArray, 16UL, QS_ALLINPUT, MWMO_INPUTAVAILABLE);

    if ( dwWaitStatus == WAIT_OBJECT_0 ||
         dwWaitStatus == WAIT_ABANDONED ) break;
  }
}


// Stuff might be unimplemented in Wine, limp home if it throws this exception
#define EXCEPTION_WINE_STUB 0x80000100

void SK_SEH_InitFinishCallback (void)
{
  __try
  {
    if (! (SK_GetDLLRole () & DLL_ROLE::DXGI))
      SK::DXGI::StartBudgetThread_NoAdapter ();
  }__except(GetExceptionCode()==EXCEPTION_WINE_STUB      ?
                                EXCEPTION_EXECUTE_HANDLER:
                                EXCEPTION_CONTINUE_SEARCH){ }
  __try
  {
    static const GUID  nil_guid = {     };
                 GUID* pGUID    = nullptr;

    // Make note of the system's original power scheme
    if ( ERROR_SUCCESS ==
           PowerGetActiveScheme ( nullptr,
                                    &pGUID )
       )
    {
      config.cpu.power_scheme_guid_orig = *pGUID;

      SK_LocalFree ((HLOCAL)pGUID);
    }

    // Apply a powerscheme override if one is set
    if (! IsEqualGUID (config.cpu.power_scheme_guid, nil_guid))
    {
      PowerSetActiveScheme (nullptr, &config.cpu.power_scheme_guid);
    }
  }__except(GetExceptionCode()==EXCEPTION_WINE_STUB      ?
                                EXCEPTION_EXECUTE_HANDLER:
                                EXCEPTION_CONTINUE_SEARCH){ }
  __try
  {
    SK_LoadGPUVendorAPIs ();
  }__except(GetExceptionCode()==EXCEPTION_WINE_STUB      ?
                                EXCEPTION_EXECUTE_HANDLER:
                                EXCEPTION_CONTINUE_SEARCH){ }
}

void
__stdcall
SK_InitFinishCallback (void)
{
  // Needed for CAPCOM's D3D12 games to not crash
  ModifyPrivilege (L"SeIncreaseBasePriorityPrivilege", TRUE);

  bool rundll_invoked =
    (StrStrIW (SK_GetHostApp (), L"Rundll32") != nullptr);

  if (rundll_invoked || SK_IsSuperSpecialK () || *__SK_BootedCore == L'\0')
  {
    init_mutex->unlock ();
    return;
  }


  SK_DeleteTemporaryFiles ();
  SK_DeleteTemporaryFiles (L"Version", L"*.old");

  SK::Framerate::Init ();

  static gsl::not_null <SK_ICommandProcessor *> cp (
    SK_GetCommandProcessor ()
  );

  extern int32_t SK_D3D11_amount_to_purge;
  cp->AddVariable (
    "VRAM.Purge",
      new SK_IVarStub <int32_t> (
        (int32_t *)&SK_D3D11_amount_to_purge
      )
  );

  cp->AddVariable (
    "GPU.StatPollFreq",
      new SK_IVarStub <float> (
        &config.gpu.interval
      )
  );

  cp->AddVariable (
    "ImGui.FontScale",
      new SK_IVarStub <float> (
        &config.imgui.scale
      )
  );

  struct MemoryManager : public SK_IVariableListener
  {
    MemoryManager (void)
    {
      SK_Thread_CreateEx ([](LPVOID pUserParam)->DWORD
      {
        SK_Thread_SetCurrentPriority (THREAD_PRIORITY_IDLE);

        auto pManager =
          (MemoryManager *)pUserParam;

        while ( WAIT_OBJECT_0 !=
                  WaitForSingleObject (__SK_DLL_TeardownEvent, 1000UL) )
        {
          PROCESS_MEMORY_COUNTERS pmc = {
            .cb = sizeof (PROCESS_MEMORY_COUNTERS)
          };

          if (! GetProcessMemoryInfo (SK_GetCurrentProcess (), &pmc, pmc.cb))
          {
            if ( WAIT_OBJECT_0 ==
                   WaitForSingleObject (__SK_DLL_TeardownEvent, 750UL) )
            {
              break;
            }
          }

          pManager->working_set = pmc.WorkingSetSize;

          MEMORYSTATUSEX
          msex          = {           };// Mmmm, sex.
          msex.dwLength = sizeof (msex);

          if (GlobalMemoryStatusEx (&msex))
          {
            if (msex.dwMemoryLoad >= 98)
            {
              SK_ImGui_CreateNotification (
                "RAM.HighLoad", SK_ImGui_Toast::Warning,
                  SK_FormatString ( "Total System RAM Usage:\t%d%%\r\n\r\n"
                                    "\t * Consider closing background software...",
                                    msex.dwMemoryLoad
                                  ).c_str (), "Critically Low System RAM", 20000,
                                SK_ImGui_Toast::UseDuration |
                                SK_ImGui_Toast::ShowTitle   |
                                SK_ImGui_Toast::ShowCaption |
                                SK_ImGui_Toast::ShowNewest );
            }
          }
        }

        SK_Thread_CloseSelf ();

        return 0;
      }, L"[SK] Memory Statistics", this);

      vram_scale_var =
        SK_CreateVar ( SK_IVariable::Float,
                         &config.render.dxgi.vram_budget_scale, this );
      working_set_var =
        SK_CreateVar ( SK_IVariable::LongInt, &working_set,     this );

      cp->AddVariable ("DXGI.VRAMBudgetScale",  vram_scale_var);
      cp->AddVariable (   "Memory.WorkingSet", working_set_var);
    }

    ~MemoryManager (void)
    {
      if (working_set_var != nullptr)
      {
        cp->RemoveVariable ("Memory.WorkingSet");
        delete working_set_var;
      }

      if (vram_scale_var != nullptr)
      {
        cp->RemoveVariable ("DXGI.VRAMBudgetScale");
        delete vram_scale_var;
      }
    }

    virtual bool OnVarChange (SK_IVariable* var, void* val = nullptr)
    {
      if (var != nullptr)
      {
        if (var->getValuePointer () == &working_set)
        {
          if (val != nullptr)
          {
            *(size_t *)val =
              SK_Memory_EmptyWorkingSet ();

            return true;
          }

          return false;
        }

        if (var->getValuePointer () == &config.render.dxgi.vram_budget_scale)
        {
          if (val != nullptr)
          {
            if (*(float *)val >= 0.1f &&
                *(float *)val <= 5.0f)
            {
              config.render.dxgi.vram_budget_scale =
                *(float *)val;

              extern HANDLE __SK_DXGI_BudgetChangeEvent;
              if (          __SK_DXGI_BudgetChangeEvent != INVALID_HANDLE_VALUE)
                  SetEvent (__SK_DXGI_BudgetChangeEvent);

              return true;
            }
          }

          return false;
        }
      }

      return false;
    }

  protected:
    uint64         working_set;

  private:
    SK_IVariable*  working_set_var;
    SK_IVariable*   vram_scale_var;
  } static memory_manager;

  SK_InitRenderBackends ();

  cp->AddCommand ("mem",       new skMemCmd    ());
  cp->AddCommand ("GetUpdate", new skUpdateCmd ());


  //
  // Game-Specific Stuff that I am not proud of
  //
  switch (SK_GetCurrentGameID ())
  {
#ifdef _WIN64
    case SK_GAME_ID::DarkSouls3:
      SK_DS3_InitPlugin ();
      break;
#else
    case SK_GAME_ID::Tales_of_Zestiria:
      SK_GetCommandProcessor ()->ProcessCommandFormatted (
        "TargetFPS %f",
          config.render.framerate.target_fps
      );
      break;
    default:
    {
      HMODULE hModEOSOVH =
        GetModuleHandleW (L"EOSOVH-Win32-Shipping.dll");

      if (hModEOSOVH)
      {
        MoveFileW (  SK_GetModuleFullName (hModEOSOVH).        c_str (),
                    (SK_GetModuleFullName (hModEOSOVH) + L"_").c_str () );
      }
    } break;
#endif
  }


  // Get rid of the game output log if the user doesn't want it...
  if (! config.system.game_output)
  {
    game_debug->close ();
    game_debug->silent = true;
  }


  const wchar_t* config_name =
    SK_GetBackend ();

  // Use a generic "SpecialK" name instead of the primary wrapped/hooked API name
  //   for this DLL when it is injected at run-time rather than a proxy DLL.
  if (SK_IsInjected ())
    config_name = L"SpecialK";

  SK_SaveConfig (config_name);

  SK_Console::getInstance ()->Start ();

  // Disable Windows 11 Dynamic Refresh Rate
  if (config.render.dxgi.disable_virtual_vbi)
    SK_DXGI_DisableVBlankVirtualization ();

  SK_Power_InitEffectiveModeCallbacks ();

  // SEH to handle Wine Stub functions
  SK_SEH_InitFinishCallback ();

  SK_EnableApplyQueuedHooks ();
  SK_ApplyQueuedHooks       ();

  dll_log->LogEx (false, L"------------------------------------------------"
                         L"-------------------------------------------\n" );
  dll_log->Log   (       L"[ SpecialK ] === Initialization Finished! ===   "
                         L"       (%6.2f ms)",
                           SK_DeltaPerfMS ( SK_GetInitParams ().start_time.QuadPart, 1 )
                 );
  dll_log->LogEx (false, L"------------------------------------------------"
                         L"-------------------------------------------\n" );

#ifdef _M_IX86
  typedef BOOL (WINAPI* K32EmptyWorkingSet_pfn)(HANDLE hProcess);
  static                K32EmptyWorkingSet_pfn
                        K32EmptyWorkingSet =
                       (K32EmptyWorkingSet_pfn) SK_GetProcAddress (
          L"kernel32", "K32EmptyWorkingSet" );

  if (K32EmptyWorkingSet != nullptr)
  {   K32EmptyWorkingSet (GetCurrentProcess ()); }
#endif

  init_mutex->unlock ();
}


DWORD
WINAPI
CheckVersionThread (LPVOID)
{
  SetCurrentThreadDescription (                   L"[SK] Auto-Update Worker"   );
  SetThreadPriority           ( SK_GetCurrentThread (), THREAD_PRIORITY_LOWEST );

  // If a local repository is present, use that.
  if (GetFileAttributes (LR"(Version\installed.ini)") == INVALID_FILE_ATTRIBUTES)
  {
    if (SK_FetchVersionInfo (L"SpecialK"))
    {
      // ↑ Check, but ↓ don't update unless running the global injector version
      if ( (SK_IsInjected () && (! SK_IsSuperSpecialK ())) )
      {
        SK_UpdateSoftware (L"SpecialK");
      }
    }
  }

  SK_Thread_CloseSelf ();

  return 0;
}


void BasicInit (void)
{
  // Cleanup any leftover temporary files from the last launch
  SK_DeleteTemporaryFiles ();

  // Add a notification that will not go away until a user reads it...
  SK_ImGui_CreateNotification (
    "Notification.HelloWorld", SK_ImGui_Toast::Success,
    "Notifications can be configured by right-clicking them.",
    "Special K Notification System Initialized Successfully",
      25000, SK_ImGui_Toast::ShowCaption |
             SK_ImGui_Toast::ShowTitle   |
             SK_ImGui_Toast::ShowOnce    |
             SK_ImGui_Toast::UseDuration );

  // Setup unhooked function pointers
  SK_PreInitLoadLibrary ();

  if (config.system.handle_crashes)
    SK::Diagnostics::CrashHandler::Init   ();
  SK::Diagnostics::CrashHandler::InitSyms ();

  // Setup widgets now, some of them do non-trivial things
  //   such as load INI settings for HDR...
  SK_Widget_InitEverything ();

  // This installs hooks for COM's CoCreateInstance, used for various old DirectX
  //   features in addition to Special K's WMI monitoring services
  SK_WMI_Init ();

  SK::EOS::Init (false);

  //// Do this from the startup thread [these functions queue, but don't apply]
  if (! config.input.dont_hook_core)
  {
    // TODO: Split this into Keyboard hooks and Mouse hooks
    //         and allow picking them individually
    SK_Input_PreInit        (); // Hook only symbols in user32 and kernel32
  }
  SK_HookWinAPI             ();
  SK_CPU_InstallHooks       ();
  SK_NvAPI_PreInitHDR       ();
  SK_NvAPI_InitializeHDR    ();
  SK_DStorage_Init          ();

  ////// For the global injector, when not started by SKIM, check its version
  ////if ( (SK_IsInjected () && (! SK_IsSuperSpecialK ())) )
  ////  SK_Thread_Create ( CheckVersionThread );

  if (config.dpi.disable_scaling)   SK_Display_DisableDPIScaling      (     );
  if (config.dpi.per_monitor.aware) SK_Display_SetMonitorDPIAwareness (false);

  SK_File_InitHooks    ();
  SK_Network_InitHooks ();

  if (config.system.display_debug_out)
    SK::Diagnostics::Debugger::SpawnConsole ();


   // Games that need plug-in initialization before Steam
   //
   switch (SK_GetCurrentGameID ())
   {
#ifdef _WIN64
     case SK_GAME_ID::FinalFantasyXVI:
       SK_FFXVI_InitPlugin ();
       break;
#endif
     case SK_GAME_ID::Launcher:
     default:
       break;
   }


  // Steam Overlay and SteamAPI Manipulation
  //
  if (! config.platform.silent)
  {
    config.steam.force_load_steamapi = false;

    // TODO: Rename -- this initializes critical sections and performance
    //                   counters needed by SK's SteamAPI back end.
    //
    SK_Steam_InitCommandConsoleVariables  ();

    // Non-Steam games have non-zero (negative) AppIDs, but cannot use SteamAPI
    if (SK::SteamAPI::AppID () + config.steam.appid > 0)
    {
      ///// Lazy-load SteamAPI into a process that doesn't use it; this brings
      /////   a number of general-purpose benefits (such as battery charge monitoring).
      bool kick_start = config.steam.force_load_steamapi;

      if (kick_start || (! SK_Steam_TestImports (SK_GetModuleHandle (nullptr))))
      {
        // Implicitly kick-start anything in SteamApps\common that does not import
        //   SteamAPI...
        if ((! kick_start) && config.steam.auto_inject)
        {
          if (StrStrIW (SK_GetHostPath (), LR"(SteamApps\common)") != nullptr)
          {
            // Only do this if the game doesn't have a copy of the DLL lying around somewhere,
            //   because if we init Special K's SteamAPI DLL, the game's will fail to init and
            //     the game won't be happy about that!
            kick_start =
              (! SK_Modules->LoadLibrary (SK_Steam_GetDLLPath ())) ||
                    config.steam.force_load_steamapi;
          }
        }

        if (kick_start)
          SK_Steam_KickStart (SK_Steam_GetDLLPath ());
      }

      SK_Steam_PreHookCore ();
    }
  }

  if (SK_COMPAT_IsFrapsPresent ())
      SK_COMPAT_UnloadFraps ();

  bool bEnable = SK_EnableApplyQueuedHooks  ();
  {
    SK_ApplyQueuedHooks ();
  }
  if (! bEnable) SK_DisableApplyQueuedHooks ();
}

DWORD
WINAPI
DllThread (LPVOID user)
{
  WriteULongNoFence (&dwInitThreadId, SK_Thread_GetCurrentId ());

  SetCurrentThreadDescription (                 L"[SK] Primary Initialization Thread" );
  SetThreadPriority           ( SK_GetCurrentThread (), THREAD_PRIORITY_TIME_CRITICAL );
  SetThreadPriorityBoost      ( SK_GetCurrentThread (), FALSE                         );

  if (config.compatibility.init_on_separate_thread && (! config.compatibility.init_sync_for_streamline))
                                                   //&& (! config.compatibility.init_sync_for_reshade))
  {
    auto* params =
      static_cast <init_params_s *> (user);
    
    SK_InitCore ( params->backend,
                  params->callback );
  }

  AppId64_t appid (
    SK_Steam_GetAppID_NoAPI ()
  );

  if (SK_IsInjected ())
  {
    DWORD dwPid =
      GetCurrentProcessId ();

    auto *pInjectMeta =
      SK_Inject_GetRecord (dwPid);

    if (pInjectMeta != nullptr)
    {
      pInjectMeta->process.id           = dwPid;
      pInjectMeta->platform.steam_appid = appid;

      SK_Inject_AuditRecord ( dwPid, pInjectMeta,
                            sizeof (*pInjectMeta) );
    }
  }


  if (ReadAcquire (&__SK_Init) == FALSE)
  {
    if (! InterlockedCompareExchangeAcquire (&__SK_Init, TRUE, FALSE))
    {
      SK_D3D_SetupShaderCompiler ();

      WritePointerRelease ( (volatile PVOID *)(&hInitThread),
                              INVALID_HANDLE_VALUE );
      WriteULongRelease   (                       &dwInitThreadId,
                              0                    );

      // Implicitly load ReShade (ReShade{32|64}.dll) if it exists
      SK_ReShade_LoadIfPresent  ();

      if (PathFileExistsW (L"fsr2streamline.asi"))
          SK_LoadLibraryW (L"fsr2streamline.asi");

      // Load user-defined DLLs (Lazy)
      SK_RunLHIfBitness ( 64, SK_LoadLazyImports64 (),
                              SK_LoadLazyImports32 () );

      if (config.system.handle_crashes)
        SK::Diagnostics::CrashHandler::Reinstall ();
    }
  }

  return 0;
}


enum SK_File_SearchStopCondition {
  FirstMatchFound,
  AllMatchesFound,
  LastMatchFound
};

std::unordered_set <std::wstring>
SK_RecursiveFileSearchEx ( const wchar_t* wszDir,
                           const wchar_t* wszSearchExt,
 std::unordered_set <std::wstring_view>& cwsFileNames,
       std::vector        <
         std::pair          < std::wstring, bool >
                          >&&             preferred_dirs = { },
              SK_File_SearchStopCondition stop_condition = FirstMatchFound )
{
  std::unordered_set <
    std::wstring
  > found_set;

  for ( auto& preferred : preferred_dirs )
  {
    if ( preferred.second == false )
    {
      preferred.second = true;

      const DWORD dwAttribs  =
        GetFileAttributesW (preferred.first.c_str ());

      if (  dwAttribs != INVALID_FILE_ATTRIBUTES &&
          !(dwAttribs &  FILE_ATTRIBUTE_DIRECTORY) )
      {
        found_set.emplace (preferred.first);

        if (stop_condition == FirstMatchFound)
        {
          return found_set;
        }
      }

      PathRemoveFileSpec (
        preferred.first.data ()
      );

      std::unordered_set <std::wstring>&& recursive_finds =
        SK_RecursiveFileSearchEx ( preferred.first.c_str (), wszSearchExt,
                                                            cwsFileNames,
                                                       { }, stop_condition );

      if (! recursive_finds.empty () )
      {
        if (stop_condition == FirstMatchFound)
        {
          return recursive_finds;
        }
      }
    }
  }

  wchar_t   wszPath [MAX_PATH + 2] = { };
  swprintf (wszPath, LR"(%s\*)", wszDir);

  WIN32_FIND_DATA fd          = {   };
  HANDLE          hFind       =
    FindFirstFileW ( wszPath, &fd);

  if (hFind == INVALID_HANDLE_VALUE) { return found_set; }

  std::vector <std::wstring> dirs_to_traverse;

  do
  {
    if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
    {
      if ( *fd.cFileName == L'.' )
      {
        const wchar_t* wszNext =
          SK_CharNextW (fd.cFileName);

        if (    wszNext != nullptr &&
             ((*wszNext == L'.'  ) ||
              (*wszNext == L'\0' ))   )
        {
        //dll_log.Log (L"%ws\\%ws is a special directory", wszDir, fd.cFileName);
          continue;
        }
      }

      dirs_to_traverse.emplace_back (fd.cFileName);
    }

    wchar_t wszFoundFile      [MAX_PATH + 2] = { };
    wchar_t wszFoundFileLower [MAX_PATH + 2] = { };
    wchar_t wszFoundExtension [     16     ] = { };

    if ( _wsplitpath_s (
           fd.cFileName, nullptr, 0,
                         nullptr, 0,
                         wszFoundFile, MAX_PATH,
                         wszFoundExtension, 16
                       ) != 0
       )
    {
    //dll_log.Log (L"%ws did not split", fd.cFileName);
      continue;
    }

    if (_wcsicmp (wszFoundExtension, wszSearchExt) != 0)
    {
    //dll_log.Log (L"%ws is wrong extension    { Search Extension: %ws, First File To Check: %ws }",
    //             wszFoundExtension, wszSearchExt, cwsFileNames.begin ()->c_str () );
      continue;
    }

    wcscpy (
      wszFoundFileLower,
        wszFoundFile
    );

    wchar_t* pwszFileLower = wszFoundFileLower;
    while ( *pwszFileLower != L'\0' )
    {       *pwszFileLower = towlower     (*pwszFileLower);
             pwszFileLower = SK_CharNextW ( pwszFileLower);
    }

    if (! cwsFileNames.count (wszFoundFileLower))
    {
      //dll_log.Log (L"%ws is not contained  { Search Extension: %ws, First File To Check: %ws }",
      //             wszFoundFileLower, wszSearchExt, cwsFileNames.begin ()->c_str () );

      continue;
    }

    else
    {
      std::wstring found (wszDir);
                   found += LR"(\)";
                   found += wszFoundFile;
                   found += wszFoundExtension;

      found_set.emplace (found);

      //dll_log.Log (L"Add to Found Set: %s", found.c_str ());

      if (stop_condition == FirstMatchFound)
      {
        FindClose (hFind);
        return found_set;
      }
    }
  } while (FindNextFile (hFind, &fd));

  FindClose (hFind);


  for ( auto& dir : dirs_to_traverse )
  {
    wchar_t   wszDescend [MAX_PATH + 2] = { };
    swprintf (wszDescend, LR"(%s\%s)", wszDir, dir.c_str () );

    const std::unordered_set <std::wstring>&& recursive_finds =
      SK_RecursiveFileSearchEx ( wszDescend,  wszSearchExt,
                                             cwsFileNames,
                                         { }, stop_condition );

    if (! recursive_finds.empty ())
    {
      for ( auto& found_entry : recursive_finds )
      {
        found_set.emplace (found_entry);

        if (stop_condition == FirstMatchFound)
          return found_set;
      }
    }
  }


  return found_set;
}

std::wstring
SK_RecursiveFileSearch ( const wchar_t* wszDir,
                         const wchar_t* wszFile )
{
  dll_log->Log ( L"Recursive File Search for '%ws', beginning in '%ws'",
                   SK_ConcealUserDir (std::wstring (wszFile).data ()),
                   SK_ConcealUserDir (std::wstring (wszDir).data  ()) );

  std::wstring extension (
    PathFindExtensionW     (wszFile)
                         ),
               filename    (wszFile);

  PathRemoveExtensionW (filename.data ());
  PathFindFileNameW    (filename.data ());

  std::unordered_set <std::wstring_view> file_pattern = {
    filename.data ()
  };

  const std::unordered_set <std::wstring> matches =
    SK_RecursiveFileSearchEx (
      wszDir, extension.c_str (),
        file_pattern, {{wszFile, false}}, FirstMatchFound
    );

  if (! matches.empty ())
  {
    dll_log->Log ( L"Success!  [%ws]",
                   SK_ConcealUserDir (std::wstring (*matches.begin ()).data ()) );
  }

  else
  {
    dll_log->Log ( L"No Such File [%ws] Exists",
                   SK_ConcealUserDir (std::wstring (*matches.begin ()).data ()) );
  }

  return matches.empty () ?
                      L"" : *matches.begin ();
}


bool
__stdcall
SK_HasGlobalInjector (void)
{
  static int
      last_test = 0;
  if (last_test == 0)
  {
    wchar_t     wszBasePath [MAX_PATH + 2] = { };
    wcsncpy_s ( wszBasePath, MAX_PATH,
          SK_GetInstallPath (), _TRUNCATE );

    lstrcatW (wszBasePath, SK_RunLHIfBitness ( 64, L"SpecialK64.dll",
                                                   L"SpecialK32.dll" ));

    bool result = (GetFileAttributesW (wszBasePath) != INVALID_FILE_ATTRIBUTES);
    last_test   = result ?
                       1 : -1;
  }

  return (last_test != -1);
}

extern std::pair <std::queue <DWORD>, BOOL> __stdcall SK_BypassInject (void);

const wchar_t*
__stdcall
SK_GetDebugSymbolPath (void)
{
  static volatile LONG    __init                            = 0;
  static          wchar_t wszDbgSymbols [MAX_PATH * 3 + 1] = { };

  if (ReadAcquire (&__init) == 2)
    return wszDbgSymbols;

  if (! InterlockedCompareExchange (&__init, 1, 0))
  {
    if (! crash_log->initialized)
    {
      crash_log->flush_freq = 0;
      crash_log->lockless   = true;
      crash_log->init       (L"logs/crash.log", L"wt+,ccs=UTF-8");
    }


    std::filesystem::path symbol_file =
      SK_GetModuleFullName (skModuleRegistry::Self ());

    wchar_t wszSelfName    [MAX_PATH + 2] = { };
    wchar_t wszGenericName [MAX_PATH + 2] = { };
    wcsncpy_s ( wszSelfName,           MAX_PATH,
                 symbol_file.c_str (), _TRUNCATE );


    PathRemoveExtensionW (wszSelfName);
    lstrcatW             (wszSelfName, L".pdb");

              wcsncpy_s ( wszGenericName, MAX_PATH,
                          wszSelfName,    _TRUNCATE );

    PathRemoveFileSpecW ( wszGenericName );
    PathAppendW         ( wszGenericName,
      SK_RunLHIfBitness ( 64, L"SpecialK64.pdb",
                              L"SpecialK32.pdb" ) );

    bool generic_symbols =
      ( INVALID_FILE_ATTRIBUTES !=
          GetFileAttributesW (wszGenericName) );

    if (generic_symbols)
    {
      symbol_file = wszGenericName;
    }

    else
    {
      symbol_file = wszSelfName;
    }

    // Not the correct way of validating the existence of this DLL's symbol file,
    //   but it is good enough for now ...
    if ( (! generic_symbols)     &&
         INVALID_FILE_ATTRIBUTES == GetFileAttributesW (wszSelfName) )
    {
      static wchar_t
           wszCurrentPath [MAX_PATH * 3 + 1] = { };
      if (*wszCurrentPath == L'\0')
      {
        SymGetSearchPathW ( GetCurrentProcess (),
                              wszCurrentPath,
                                MAX_PATH * 3 );
      }

      std::wstring dir (wszCurrentPath); dir += L";";
                   dir.append (SK_GetInstallPath ());

      wcsncpy_s ( wszDbgSymbols,  MAX_PATH * 3,
                    dir.c_str (), _TRUNCATE );

      symbol_file  = SK_GetInstallPath ();
      symbol_file /=
        SK_RunLHIfBitness ( 64, L"SpecialK64.pdb",
                                L"SpecialK32.pdb" );
    }

    else
    {
      static wchar_t                           wszCurrentPath [MAX_PATH * 3 + 1] = { };
      SymGetSearchPathW (GetCurrentProcess (), wszCurrentPath, MAX_PATH * 3);

      wcsncpy_s ( wszDbgSymbols,  MAX_PATH * 3,
                  wszCurrentPath, _TRUNCATE );
    }


    // Strip the username from the logged path
    static wchar_t wszDbgSymbolsEx  [MAX_PATH * 3 + 1] = { };
       wcsncpy_s ( wszDbgSymbolsEx,  MAX_PATH * 3,
                      wszDbgSymbols, _TRUNCATE );

    SK_ConcealUserDir (wszDbgSymbolsEx);
    crash_log->Log (L"DebugHelper Symbol Search Path......: %ws", wszDbgSymbolsEx);

    std::wstring
      stripped (symbol_file);
      stripped =
        SK_ConcealUserDir (stripped.data ());


    if (GetFileAttributesW (symbol_file.c_str ()) != INVALID_FILE_ATTRIBUTES)
    {
      crash_log->Log ( L"Special K Debug Symbols Loaded From.: %ws",
                         stripped.c_str () );
    }

    else
    {
      crash_log->Log ( L"Unable to load Special K Debug Symbols ('%ws'), "
                       L"crash log will not be accurate.",
                         stripped.c_str () );

      SymSetExtendedOption (SYMOPT_EX_NEVERLOADSYMBOLS, TRUE);
    }

    wcsncpy_s ( wszDbgSymbols,        MAX_PATH * 3,
                symbol_file.c_str (), _TRUNCATE );

    PathRemoveFileSpec (wszDbgSymbols);

    InterlockedIncrementRelease (&__init);
  }

  else
    SK_Thread_SpinUntilAtomicMin (&__init, 2);

  return
    wszDbgSymbols;
}

void
__stdcall
SK_EstablishRootPath (void)
{
  wchar_t   wszConfigPath [MAX_PATH + 2] = { };
  GetCurrentDirectory     (MAX_PATH,
            wszConfigPath);
  lstrcatW (wszConfigPath, LR"(\)");

  // File permissions don't permit us to store logs in the game's directory,
  //   so implicitly turn on the option to relocate this stuff.
  if (! SK_File_CanUserWriteToPath (wszConfigPath))
  {
    config.system.central_repository = true;
  }

  else if (PathFileExistsW (L"SpecialK.central"))
  {
    config.system.central_repository = true;
  }

  RtlZeroMemory (
    wszConfigPath, sizeof (wchar_t) * (MAX_PATH + 2)
  );

  if (SK_GetModuleName (SK_GetDLL ())._Equal (
    SK_RunLHIfBitness (64, L"SpecialK64.dll",
                           L"SpecialK32.dll")))
  {
    try {
      std::filesystem::path path =
        SK_GetModuleFullName (SK_GetDLL ());

      wcsncpy_s ( SKX_GetInstallPath (), MAX_PATH,
                    path.parent_path ().wstring ().
                                          c_str (), _TRUNCATE );
    }

    catch (const std::exception& e)
    {
      std::ignore = e;
#ifdef DEBUG
      _debugbreak ();
#endif
    }
  }

  if (*SK_GetInstallPath () == L'\0')
  {
    bool bEnvironmentDefinedPath = false;
    bool bRegistryDefinedPath    = false;

    wchar_t wszInstallPath [MAX_PATH + 2] = { };
    ULONG   ulPathLen   =   MAX_PATH;

    if ( GetEnvironmentVariableW (
           L"SPECIALK_PATH", wszInstallPath, ulPathLen
         ) != 0
       )
    {
      bEnvironmentDefinedPath = true;

      wcsncpy_s ( SKX_GetInstallPath (), MAX_PATH,
                      wszInstallPath,   _TRUNCATE );

      // Couldn't create the directory, try something else
      if (! SK_CreateDirectoriesEx (SKX_GetInstallPath (), false))
      {
        bEnvironmentDefinedPath = false;
      }
    }

    if (! bEnvironmentDefinedPath)
    {
      if ( CRegKey
             hkInstallPath; ERROR_SUCCESS ==
             hkInstallPath.Open ( HKEY_CURRENT_USER,
                  LR"(Software\Kaldaien\Special K)" )
          )
      {
        if ( ERROR_SUCCESS ==
               hkInstallPath.QueryStringValue ( L"Path",
              wszInstallPath,                  &ulPathLen )
           )
        {
          bRegistryDefinedPath = true;

          wcsncpy_s ( SKX_GetInstallPath (), MAX_PATH,
                          wszInstallPath,   _TRUNCATE );

          // Couldn't create the directory, try something else
          if (! SK_CreateDirectoriesEx (SKX_GetInstallPath (), false))
          {
            bRegistryDefinedPath = false;
          }
        }
      }
    }

    // Fallback to ol' trusty
    if (! (bRegistryDefinedPath || bEnvironmentDefinedPath))
    {
      swprintf ( SKX_GetInstallPath (), LR"(%s\My Mods\SpecialK)",
                 SK_GetDocumentsDir ().c_str () );
    }
  }

  // Store config profiles in a centralized location rather than
  //   relative to the game's executable
  //
  if (config.system.central_repository)
  {
    if (! SK_IsSuperSpecialK ())
    {
      wcsncpy_s (SKX_GetRootPath    (),  MAX_PATH,
                 SKX_GetInstallPath (), _TRUNCATE);
    }

    else
    {
      GetCurrentDirectory (MAX_PATH, SKX_GetRootPath ());
    }

    wcsncpy_s ( wszConfigPath,      MAX_PATH,
                SKX_GetRootPath (), _TRUNCATE  );
    lstrcatW  ( wszConfigPath, LR"(\Profiles\)");
    lstrcatW  ( wszConfigPath, SK_GetHostApp ());
  }


  // Relative to game's executable path
  //
  else
  {
    if (! SK_IsSuperSpecialK ())
    {
      wcsncpy_s ( SKX_GetRootPath (), MAX_PATH,
                  SK_GetHostPath  (), _TRUNCATE );
    }

    else
    {
      GetCurrentDirectory (MAX_PATH, SKX_GetRootPath ());
    }

    wcsncpy_s ( wszConfigPath,      MAX_PATH,
                SKX_GetRootPath (), _TRUNCATE );
  }


  // Not using the ShellW API because at this (init) stage,
  //   we can only reliably use Kernel32 functions.
  lstrcatW (SKX_GetRootPath (), LR"(\)");
  lstrcatW (wszConfigPath,      LR"(\)");

  SK_SetConfigPath (wszConfigPath);
}

bool
__stdcall
SK_StartupCore (const wchar_t* backend, void* callback)
{
  if ( backend == nullptr || callback == nullptr )
  {
    return false;
  }

  // Not a saved INI setting; use an alternate initialization
  //   strategy when Streamline is detected...
  config.compatibility.init_sync_for_streamline =
    config.compatibility.allow_fake_streamline == false &&
    (SK_GetModuleHandleW (L"sl.interposer.dll") != 0);

 try
 {
  if ( SK_GetProcAddress ( L"NtDll",
                            "wine_get_version" ) != nullptr )
  {
    config.compatibility.using_wine = true;
  }

  InstructionSet::deferredInit ();

  // .NET Applications might be inside of managed code, so perform
  //   initialization on a deferred native thread unless WaitForDebugger
  //     is configured.
  const static auto
    _NeedImplicitDelay =
     [&]()
      { return (! config.system.wait_for_debugger) &&
                SK_IsModuleLoaded (L"MSCOREE.dll"); };

  // If Global Injection Delay, block initialization thread until the delay period ends
  if (SK_IsInjected () && (SK_Inject_GetInjectionDelayInSeconds () > 0.0f || _NeedImplicitDelay ()))
  {
    struct packaged_params_s {
      std::wstring backend  = L""; // Persistent copy
      void*        callback =                nullptr;
      HANDLE       thread   =   INVALID_HANDLE_VALUE;
      HANDLE       parent   =   INVALID_HANDLE_VALUE;
      DWORD        resume   =                      0; // Number of times we Suspended
      CONTEXT      ctx      = {                    };
    } static  delay_params  { backend,  callback };

    if (delay_params.thread == INVALID_HANDLE_VALUE)
    {
      if (! DuplicateHandle ( GetCurrentProcess (), SK_TLS_Bottom ()->debug.handle,
                                nullptr,              &delay_params.parent,
                                  THREAD_ALL_ACCESS, FALSE, DUPLICATE_CLOSE_SOURCE |
                                                            DUPLICATE_SAME_ACCESS ) )
      {             delay_params.parent  = INVALID_HANDLE_VALUE; }
      if ((intptr_t)delay_params.parent <= 0)
                    delay_params.parent  = INVALID_HANDLE_VALUE;

      delay_params.thread =
      SK_Thread_CreateEx ([](LPVOID) -> DWORD
      {
        DWORD dwMilliseconds =
          std::max (1UL, sk::narrow_cast <DWORD> (config.system.global_inject_delay * 1000.0f));

        SK_SleepEx (dwMilliseconds, FALSE);

        if (delay_params.parent != INVALID_HANDLE_VALUE)
        {
          const DWORD dwCount =
            SuspendThread (    delay_params.parent );

          if ( dwCount > 0 &&
               dwCount < MAXIMUM_SUSPEND_COUNT )
          {
            if ( GetThreadContext (
                               delay_params.parent,
                              &delay_params.ctx   )
               )             ++delay_params.resume;
            else ResumeThread (delay_params.parent);
          }
        }

        SK_StartupCore        (delay_params.backend.c_str (),
                               delay_params.callback);

        if (delay_params.parent != INVALID_HANDLE_VALUE)
        {
          if (                 delay_params.resume > 0 )
          {
            if ( SetThreadContext (
                               delay_params.parent,
                              &delay_params.ctx   )
               ) ResumeThread (delay_params.parent);
          } else OutputDebugStringA (
             "[SK Init] Unable to Restore Suspended Thread Context");

          SK_CloseHandle (delay_params.parent);
        }

        SK_Thread_CloseSelf ();

        return 0;
      }, L"[SK] Global Injection Delay", &delay_params);

      return true;
    }
  }

  __SK_BootedCore = backend;

  // Before loading any config files, test the game's environment variables
  //  to determine if the Steam client has given us the AppID without having
  //    to initialize SteamAPI first.
  SK_Steam_GetAppID_NoAPI ();


  // Allow users to centralize all files if they want
  //
  //   Stupid hack, if the application is running with a different working-directory than
  //     the executable -- compensate!
  wchar_t          wszCentralPathVFile [MAX_PATH + 2] = { };
  SK_PathCombineW (wszCentralPathVFile, SK_GetHostPath (), L"SpecialK.central");

  if ( SK_IsInjected   () ||
       PathFileExistsW (wszCentralPathVFile) ||
       PathFileExistsW (L"SpecialK.central") )
  {
    config.system.central_repository = true;
  }

  SK_EstablishRootPath   ();
  SK_CreateDirectoriesEx (SK_GetConfigPath (), false);

  ///SK_Config_CreateSymLinks ();

  const bool rundll_invoked =
    (StrStrIW (SK_GetHostApp (), L"Rundll32") != nullptr);
  const bool skim           =
    rundll_invoked || SK_IsSuperSpecialK () || __SK_RunDLL_Bypass;

  __SK_bypass |= skim;


  auto& init_ =
    SKX_GetInitParams ();

  init_          = {     };
  init_.backend  = backend;
  init_.callback = callback;

  SK_SetBackend (backend);

  bool blacklist = false;

  // Injection Compatibility Menu
  if ( (sk::narrow_cast <USHORT> (SK_GetAsyncKeyState (VK_SHIFT  )) & 0x8000) != 0 &&
       (sk::narrow_cast <USHORT> (SK_GetAsyncKeyState (VK_CONTROL)) & 0x8000) != 0 )
  {
    WriteRelease (&__SK_Init, -1);
                   __SK_bypass = true;

    SK_BypassInject ();
  }

  else
  {
    blacklist =
      SK_IsInjected    () &&
       PathFileExistsW (SK_GetBlacklistFilename ());

    switch (SK_GetCurrentGameID ())
    {
      case SK_GAME_ID::EasyAntiCheat:
        blacklist = true;
        break;

      default:
        break;
    }

    if (! blacklist)
    {
      // Dave the Diver launches crashpad_handler.dll as a secondary application,
      //  we want to know immediately if anything else does this (dll as exe).
      SK_ReleaseAssert (StrStrIW (SK_GetHostApp (), L".dll") == nullptr);

      wchar_t                log_fname [MAX_PATH + 2] = { };
      SK_PathCombineW      ( log_fname, L"logs",
                                SK_IsInjected () ?
                                        L"SpecialK" : backend);
      PathAddExtensionW ( log_fname,    L".log" );

      dll_log->init (log_fname, L"wS+,ccs=UTF-8");
      dll_log->Log  ( L"%s.log created\t\t(Special K  %s,  %hs)",
                        SK_IsInjected () ?
                             L"SpecialK" : backend,
                          SK_GetVersionStrW (),
                            __DATE__ );

      init_.start_time =
        SK_QueryPerf ();

      game_debug->init (L"logs/game_output.log", L"w");
      game_debug->lockless = true;


      // Apply game netcode killswitch, intended to prevent stuttering in
      //   some games that do weird stuff with network (e.g. telemetry)
      if (config.network.disable_winsock)
      {
        SK_WinSock_GoOffline ();
        SK_ApplyQueuedHooks  ();
      }
    }
  }


  if (skim || blacklist)
  {
    return true;
  }


  budget_log->init ( LR"(logs\dxgi_budget.log)", L"wc+,ccs=UTF-8" );

  dll_log->LogEx (false,
    L"------------------------------------------------------------------------"
    L"-------------------\n");

  std::wstring   module_name   = SK_GetModuleName (SK_GetDLL ());
  const wchar_t* wszModuleName = module_name.c_str ();

  dll_log->Log ( L">> (%s) [%s] <<",
                   SK_GetHostApp (),
                     wszModuleName );

  const wchar_t* config_name = backend;

  if (SK_IsInjected ())
    config_name = L"SpecialK";

  LARGE_INTEGER liStartConfig =
    SK_CurrentPerf ();

  dll_log->LogEx ( true, L"Loading user preferences from %s.ini... ",
                     config_name );
  if (SK_LoadConfig (config_name))
  {
    dll_log->LogEx ( false, L"done! (%6.2f ms)\n",
      SK_DeltaPerfMS (liStartConfig.QuadPart, 1) );
  }

  else
  {
    dll_log->LogEx (false, L"failed!\n");

    std::filesystem::path default_global_name (
      SK_FormatStringW (
        LR"(%ws\Global\default_%ws)",
          SK_GetInstallPath (), config_name ) );

    std::wstring default_name (
      SK_FormatStringW ( L"%s%s%s",
                           SK_GetConfigPath (),
                             L"default_",
                               config_name )
    );

    std::wstring default_global_ini (default_global_name.wstring () + L".ini");
    std::wstring default_ini        (default_name                   + L".ini");

    if (GetFileAttributesW (default_global_ini.c_str ()) != INVALID_FILE_ATTRIBUTES)
    {
      dll_log->LogEx ( true,
                       L"Loading global default preferences from %s.ini... ",
                         default_global_name.c_str () );

      if (! SK_LoadConfig (default_global_name))
        dll_log->LogEx (false, L"failed!\n");
      else
        dll_log->LogEx (false, L"done!\n");
    }

    if (GetFileAttributesW (default_ini.c_str ()) != INVALID_FILE_ATTRIBUTES)
    {
      dll_log->LogEx ( true,
                       L"Loading default preferences from %s.ini... ",
                         default_name.c_str () );

      if (! SK_LoadConfig (default_name))
        dll_log->LogEx (false, L"failed!\n");
      else
        dll_log->LogEx (false, L"done!\n");
    }

    // If no INI file exists, write one immediately.
    dll_log->LogEx (true,  L"  >> Writing base INI file, because none existed... ");

    // Fake a frame so that the config file writes
    WriteULong64Release
                   (&SK_RenderBackend::frames_drawn, 1);
    SK_SaveConfig  (config_name);
    SK_LoadConfig  (config_name);
    WriteULong64Release
                   (&SK_RenderBackend::frames_drawn, 0);

    dll_log->LogEx (false, L"done!\n");
  }

  SK_ReShadeAddOn_Init ();

  SK_RunOnce (
  {
    SK_Display_HookModeChangeAPIs ();
    SK_ApplyQueuedHooks           ();
  });

  dll_log->LogEx (false,
    L"----------------------------------------------------------------------"
    L"---------------------\n");

  // If the module name is this, then we need to load the system-wide DLL...
  wchar_t     wszProxyName [MAX_PATH + 2] = { };
  wcsncpy_s ( wszProxyName, MAX_PATH,
              backend,     _TRUNCATE );
  PathAddExtensionW (
              wszProxyName, L".dll"  );

#ifndef _WIN64
  //
  // TEMP HACK: dgVoodoo2
  //
  if (SK_GetDLLRole () == DLL_ROLE::D3D8)
  {
    if (SK_IsInjected () && config.apis.translated == SK_RenderAPI::None)
    {
      config.compatibility.init_on_separate_thread = false;
    }

    swprintf (wszProxyName, LR"(%s\PlugIns\ThirdParty\dgVoodoo\d3d8.dll)",
                              SK_GetInstallPath ());
  }

  else if (SK_GetDLLRole () == DLL_ROLE::DDraw)
  {
    if (SK_IsInjected () && config.apis.translated == SK_RenderAPI::None)
    {
      config.compatibility.init_on_separate_thread = false;
    }

    swprintf (wszProxyName, LR"(%s\PlugIns\ThirdParty\dgVoodoo\ddraw.dll)",
                              SK_GetInstallPath ());
  }
#endif


  wchar_t wszBackendDLL  [MAX_PATH + 2] = { }, wszBackendPriv [MAX_PATH + 2] = { };
  wchar_t wszWorkDir     [MAX_PATH + 2] = { }, wszWorkDirPriv [MAX_PATH + 2] = { };

  wcsncpy_s ( wszBackendDLL,             MAX_PATH,
              SK_GetSystemDirectory (), _TRUNCATE );
  wcsncpy_s ( wszBackendPriv,            MAX_PATH,
              wszBackendDLL,            _TRUNCATE );

  GetCurrentDirectoryW (                 MAX_PATH,
              wszWorkDir                          );
  wcsncpy_s ( wszWorkDirPriv,            MAX_PATH,
              wszWorkDir,               _TRUNCATE );

  dll_log->Log (L" Working Directory:          %s", SK_ConcealUserDir (wszWorkDirPriv));
  dll_log->Log (L" System Directory:           %s", SK_ConcealUserDir (wszBackendPriv));

  PathAppendW       ( wszBackendDLL, backend );
  PathAddExtensionW ( wszBackendDLL, L".dll" );

  wchar_t* dll_name = wszBackendDLL;

  if (SK_Path_wcsicmp (wszProxyName, wszModuleName))
           dll_name =  wszProxyName;

  bool load_proxy = false;

  if (! SK_IsInjected ())
  {
    static constexpr auto architecture =
      SK_RunLHIfBitness (64, L"x64", L"Win32");

    for (auto& import : imports->imports)
    {
      if (                         import.role != nullptr &&
           0 == _wcsicmp (backend, import.role->get_value ().c_str ()) &&
                           import.architecture != nullptr &&
           0 == _wcsicmp (architecture,
                           import.architecture->get_value ().c_str ()) )
      {
        dll_log->LogEx (true, L" Loading proxy %s.dll:    ", backend);
        wcsncpy_s ( dll_name,                                MAX_PATH,
                    import.filename->get_value ().c_str (), _TRUNCATE );

        load_proxy = true;
        break;
      }
    }
  }

  if (! load_proxy)
    dll_log->LogEx (true, L" Loading default %s.dll: ", backend);

  // Pre-Load the original DLL into memory
  if (dll_name != wszBackendDLL)
  {
    SK_Modules->LoadLibraryLL (wszBackendDLL);
  }

  backend_dll =
    SK_Modules->LoadLibraryLL (dll_name);

  if (backend_dll != nullptr)
    dll_log->LogEx (false, L" (%s)\n",         SK_ConcealUserDir (dll_name));
  else
    dll_log->LogEx (false, L" FAILED (%s)!\n", SK_ConcealUserDir (dll_name));

  dll_log->LogEx (false,
    L"----------------------------------------------------------------------"
    L"---------------------\n");


  if (! __SK_bypass)
  {
    SK_D3D11_InitMutexes ();
    SK_ImGui_Init        ();

#ifndef _THREADED_BASIC_INIT
    BasicInit ();

    if ((! config.compatibility.init_on_separate_thread) ||
           config.compatibility.init_sync_for_streamline)// ||
           //config.compatibility.init_sync_for_reshade)
    {
      bool gl = false, vulkan = false, d3d9  = false, d3d11 = false, d3d12 = false,
         dxgi = false, d3d8   = false, ddraw = false, glide = false;

      SK_TestRenderImports (
        SK_GetModuleHandle (nullptr),
          &gl, &vulkan,
            &d3d9, &dxgi, &d3d11, &d3d12,
              &d3d8, &ddraw, &glide );

      dxgi  |=  SK_IsModuleLoaded (L"dxgi.dll");
      d3d11 |=  SK_IsModuleLoaded (L"d3d11.dll");
      d3d12 |=  SK_IsModuleLoaded (L"d3d12.dll");
      d3d9  |=  SK_IsModuleLoaded (L"d3d9.dll");
      gl    |= (SK_IsModuleLoaded (L"OpenGL32.dll") && !SK_IsModuleLoaded (L"EOSOVH-Win64-Shipping.dll"));
    //gl    |=  SK_IsModuleLoaded (L"gdi32.dll");
    //gl    |=  SK_IsModuleLoaded (L"gdi32full.dll");

      if ( ( dxgi || d3d11 || d3d12 ||
             d3d8 || ddraw ) && ( config.apis.dxgi.d3d11.hook
                               || config.apis.dxgi.d3d12.hook ) )
      {
        SK_DXGI_QuickHook ();
      }

      if (d3d9 && (config.apis.d3d9.hook || config.apis.d3d9ex.hook))
      {
        SK_D3D9_QuickHook ();
      }

      auto *params = &init_;

      SK_InitCore ( params->backend,
                    params->callback );
    }
#endif

    SK_EnumLoadedModules (SK_ModuleEnum::PreLoad);
  }

  if (config.system.silent)
  {        dll_log->silent = true;

    std::wstring log_fnameW (
      SK_IsInjected () ? L"SpecialK"
                       : backend
    );           log_fnameW += L".log";
    DeleteFile ( log_fnameW.c_str () );
  }

  else
    dll_log->silent = false;

  if (! __SK_bypass)
  {
    auto _AutoLoadASIMods = [&](void)
    {
      if (! config.system.auto_load_asi_files)
        return;

      using namespace std::filesystem;

      std::error_code                                                ec;
      directory_iterator           working_dir (wszWorkDir,          ec);
      recursive_directory_iterator profile_dir (SK_GetConfigPath (), ec);
      
      std::vector <directory_entry>        files;
      for (const auto& file : working_dir) files.emplace_back (file);
      for (const auto& file : profile_dir) files.emplace_back (file);
      for (const auto& file :              files )
      {
        const auto& path =
               file.path ();

        if (          file.is_regular_file (ec) &&
           !_wcsicmp (path.extension ().c_str (), L".asi"))
        {
          const auto& path_filename = path.filename ();

          // It's already loaded...
          if (GetModuleHandleW (path_filename.c_str ()))
            continue;

          const auto filename      = path_filename.wstring  ();
          const auto filename_utf8 = path_filename.u8string ();

          dll_log->LogEx (
            true, L"[ SpecialK ]  * Loading Early ASI PlugIn: '%ws' from '%ws' ... ",
              filename.c_str (),
            SK_StripUserNameFromPathW (path.parent_path ().wstring ().data ())
          );

          const HMODULE hModASI =
            SK_Modules->LoadLibraryLL (
             path.wstring ().c_str () );

          if (hModASI != skModuleRegistry::INVALID_MODULE)
          {
            dll_log->LogEx (false, L"success!\n");

            SK_ImGui_CreateNotification (
              "PlugIn.Load", SK_ImGui_Toast::Success,
                (const char *)filename_utf8.c_str (),
                        "Special K ASI Plug-In Loaded",
                          5000, SK_ImGui_Toast::UseDuration |
                                SK_ImGui_Toast::ShowCaption |
                                SK_ImGui_Toast::ShowTitle );
          }
          
          else
          {
            _com_error err (HRESULT_FROM_WIN32 (GetLastError ()));

            dll_log->LogEx (false, L"failed: 0x%04X (%s)!\n",
                            err.WCode (), err.ErrorMessage () );
          }
        }
      }
    };

    _AutoLoadASIMods ();

    switch (SK_GetCurrentGameID ())
    {
#ifdef _M_AMD64
      case SK_GAME_ID::StarOcean4:
        plugin_mgr->config_fns.emplace (SK_SO4_PlugInCfg);
        break;

      case SK_GAME_ID::AssassinsCreed_Odyssey:
        SK_ACO_PlugInInit ();
        break;
      case SK_GAME_ID::MonsterHunterWorld:
        SK_MHW_PlugInInit ();
        break;

      case SK_GAME_ID::DragonQuestXI:
        SK_DQXI_PlugInInit ();
        break;

      case SK_GAME_ID::Shenmue:
        SK_SM_PlugInInit ();
        break;

      case SK_GAME_ID::FinalFantasyXV:
        SK_FFXV_InitPlugin ();
        break;

      case SK_GAME_ID::PillarsOfEternity2:
        plugin_mgr->config_fns.emplace (SK_POE2_PlugInCfg);
        break;

      case SK_GAME_ID::LifeIsStrange_BeforeTheStorm:
        plugin_mgr->config_fns.emplace (SK_LSBTS_PlugInCfg);
        break;

      case SK_GAME_ID::Okami:
        plugin_mgr->config_fns.emplace (SK_Okami_PlugInCfg);
        break;

      case SK_GAME_ID::Yakuza0:
      case SK_GAME_ID::YakuzaKiwami:
      case SK_GAME_ID::YakuzaKiwami2:
      case SK_GAME_ID::YakuzaUnderflow:
        SK_Yakuza0_PlugInInit ();
        break;
#endif

      // May be 32-bit or 64-bit depending on which patch user is running
      case SK_GAME_ID::Persona4:
        SK_Persona4_InitPlugin ();
        break;

#ifdef _M_AMD64
      case SK_GAME_ID::NieR_Sqrt_1_5:
        SK_NIER_RAD_InitPlugin ();

        plugin_mgr->config_fns.emplace (SK_NIER_RAD_PlugInCfg);
        break;

      case SK_GAME_ID::FinalFantasy7Remake:
        SK_FF7R_InitPlugin ();
        break;

      case SK_GAME_ID::EldenRing:
        break;
#endif
    }

    if (config.steam.preload_overlay)
    {
      SK_Steam_LoadOverlayEarly ();
    }

    bool gl = false, vulkan = false, d3d9  = false, d3d11 = false, d3d12 = false,
       dxgi = false, d3d8   = false, ddraw = false, glide = false;

    SK_TestRenderImports (
      SK_GetModuleHandle (nullptr),
        &gl, &vulkan,
          &d3d9, &dxgi, &d3d11, &d3d12,
            &d3d8, &ddraw, &glide );

    dxgi  |=  SK_IsModuleLoaded (L"dxgi.dll");
    d3d11 |=  SK_IsModuleLoaded (L"d3d11.dll");
    d3d12 |=  SK_IsModuleLoaded (L"d3d12.dll");
    d3d9  |=  SK_IsModuleLoaded (L"d3d9.dll");
    gl    |= (SK_IsModuleLoaded (L"OpenGL32.dll") && !SK_IsModuleLoaded (L"EOSOVH-Win64-Shipping.dll"));
  //gl    |=  SK_IsModuleLoaded (L"gdi32.dll");
  //gl    |=  SK_IsModuleLoaded (L"gdi32full.dll");

    if ( ( dxgi || d3d11 || d3d12 ||
           d3d8 || ddraw ) && ( config.apis.dxgi.d3d11.hook
                             || config.apis.dxgi.d3d12.hook ) )
    {
      SK_DXGI_QuickHook ();
    }

    if (d3d9 && (config.apis.d3d9.hook || config.apis.d3d9ex.hook))
    {
      SK_D3D9_QuickHook ();
    }

    void SK_Streamline_InitBypass (void);
         SK_Streamline_InitBypass ();

    InterlockedExchangePointer (
      const_cast <void **> (&hInitThread),
      SK_Thread_CreateEx ( DllThread, nullptr,
                               &init_ )
    ); // Avoid the temptation to wait on this thread

    if (int32_t hooks_queued = (int32_t)ReadULongAcquire (&SK_MinHook_HooksQueuedButNotApplied);
                hooks_queued > 0)
    {
      SK_ApplyQueuedHooks ();
    }
  }

  return true;
 }
 catch (const std::exception& e)
 {
   SK_LOG0 ( ( L"Exception '%hs' during StartupCore (...)", e.what () ),
               L" SpecialK " );
   return false;
 }
}


struct SK_DummyWindows {
  struct window_s {
    HWND hWnd   = 0;
    BOOL active = FALSE;

    struct {
      HWND    hWnd    = 0;
      BOOL    unicode = FALSE;
      WNDPROC wndproc = nullptr;
    } parent;
  };

  std::unordered_map <HWND, window_s> list;
  std::set           <HWND>           unique;
  std::recursive_mutex                lock;
};

SK_LazyGlobal <SK_DummyWindows> dummy_windows;

LRESULT
WINAPI
DummyWindowProc (_In_  HWND   hWnd,
                 _In_  UINT   uMsg,
                 _In_  WPARAM wParam,
                 _In_  LPARAM lParam)
{
  SK_DummyWindows::window_s* pWindow = nullptr;

  {
    std::scoped_lock <std::recursive_mutex> auto_lock (dummy_windows->lock);

    if (dummy_windows->unique.count (hWnd))
      pWindow = &dummy_windows->list [hWnd];
  }

  if (pWindow != nullptr && IsWindow (pWindow->parent.hWnd))
  {
    MSG msg;
    msg.hwnd    = pWindow->parent.hWnd;
    msg.message = uMsg;
    msg.lParam  = lParam;
    msg.wParam  = wParam;

    bool SK_ImGui_HandlesMessage (MSG * lpMsg, bool /*remove*/, bool /*peek*/);
    if  (SK_ImGui_HandlesMessage (&msg, false, false))
      return ImGui_WndProcHandler (pWindow->parent.hWnd, uMsg, wParam, lParam);
  }

  return
    DefWindowProcW (hWnd, uMsg, wParam, lParam);
};

HWND
SK_Win32_CreateDummyWindow (HWND hWndParent)
{
  std::scoped_lock <std::recursive_mutex>
         auto_lock (dummy_windows->lock);

  static WNDCLASSW wc = {
    CS_HREDRAW | CS_VREDRAW | CS_OWNDC,
    DummyWindowProc,
    0x00, 0x00,
    SK_GetDLL        (                           ),
    LoadIcon         (nullptr, IDI_APPLICATION   ),
    nullptr,
    static_cast <HBRUSH> (
      GetStockObject (         NULL_BRUSH        )
                         ),
    nullptr,
    L"Special K Dummy Window Class"
  };
  static WNDCLASSW wc_existing = { };

  if ( RegisterClassW (&wc) ||
       GetClassInfo   ( SK_GetDLL (),
                          L"Special K Dummy Window Class",
                            &wc_existing ) )
  {
    RECT                          rect = { };
    if (IsWindow (hWndParent))
      GetWindowRect (hWndParent, &rect);

    HWND hWnd =
      CreateWindowExW ( WS_EX_NOACTIVATE | WS_EX_NOPARENTNOTIFY,
                            L"Special K Dummy Window Class",
                            L"Special K Dummy Window",
                            //IsWindow (hWndParent) ? WS_CHILD : WS_CLIPSIBLINGS,
                                      WS_POPUP        | WS_DISABLED |
                                      WS_CLIPSIBLINGS | WS_CLIPCHILDREN,
                                              rect.left,               rect.top,
                                 rect.right - rect.left, rect.bottom - rect.top,
                        HWND_DESKTOP/*hWndParent*/, nullptr,
                                    SK_GetDLL (),   nullptr );

    if (hWnd != SK_HWND_DESKTOP)
    {
      if (dummy_windows->unique.emplace (hWnd).second)
      {
        SK_DummyWindows::window_s window;
        window.hWnd        = hWnd;
        window.active      = true;
        window.parent.hWnd = hWndParent;

        window.parent.unicode = IsWindowUnicode            (window.parent.hWnd);
        window.parent.wndproc = (WNDPROC)GetWindowLongPtrW (window.parent.hWnd, GWLP_WNDPROC);

        dummy_windows->list [hWnd] = window;

        if (hWndParent != 0 && IsWindow (hWndParent))
        {
          SK_Thread_CreateEx ([](LPVOID user)->DWORD
          {
            HWND hWnd = (HWND)user;

            SK_ShowWindowAsync (hWnd, SW_HIDE);

            MSG                     msg = { };
            while (SK_GetMessageW (&msg, 0, 0, 0))
            {
              SK_TranslateMessage (&msg);
              SK_DispatchMessageW (&msg);

              if (msg.message == WM_DESTROY && msg.hwnd == hWnd)
              {
                SK_Win32_CleanupDummyWindow (hWnd);
                break;
              }
            }

            SK_Thread_CloseSelf ();

            return 0;
          }, L"[SK] Dummy Window Proc", (LPVOID)hWnd);
        }
      }
    }

    else
      SK_ReleaseAssert (!"CreateDummyWindow Failed");

    return hWnd;
  }

  else
  {
    dll_log->Log (L"Window Class Registration Failed!");
  }

  return nullptr;
}

void
SK_Win32_CleanupDummyWindow (HWND hwnd)
{
  std::scoped_lock <std::recursive_mutex>
         auto_lock (dummy_windows->lock);

  std::vector <HWND> cleaned_windows;

  auto& wnd_list =
    dummy_windows->list;

  if (wnd_list.count (hwnd))
  {
    auto& window = wnd_list [hwnd];

    if (DestroyWindow (window.hWnd))
    {
      //if (IsWindow (     window.parent.hWnd))
      //  SetActiveWindow (window.parent.hWnd);
    }
    cleaned_windows.emplace_back (window.hWnd);
  }

  else if (hwnd == 0)
  {
    for (auto& it : wnd_list)
    {
      if (DestroyWindow (it.second.hWnd))
      {
        //if (it.second.parent.hWnd != 0 && IsWindow (it.second.parent.hWnd))
        //  SetActiveWindow (it.second.parent.hWnd);
      }
      cleaned_windows.emplace_back (it.second.hWnd);
    }
  }

  for ( auto& it : cleaned_windows )
    if (wnd_list.count (it))
        wnd_list.erase (it);

  if (wnd_list.empty ())
    UnregisterClassW ( L"Special K Dummy Window Class", SK_GetDLL () );
}

void
SK_Log_CleanupLogs (void)
{
  if (steam_log->lines <= 4)
      steam_log->lines =  0;

  if (epic_log->lines < 2)
      epic_log->lines = 0;

  if (game_debug->lines < 2)
      game_debug->lines = 0;

  if (budget_log->lines < 2)
      budget_log->lines = 0;

  budget_log->close ();
  game_debug->close ();
  epic_log->close   ();
  steam_log->close  ();
  tex_log->close    ();
}

bool
SK_Inject_IsWindowSKIF (HWND hWnd)
{
  DWORD                               dwPid = 0x0;
  SK_GetWindowThreadProcessId (hWnd, &dwPid);

  if (dwPid == 0 ||
      dwPid == GetCurrentProcessId ())
  {
    return false;
  }

  SK_AutoHandle hProcess (
    OpenProcess ( PROCESS_QUERY_LIMITED_INFORMATION,
                    FALSE, dwPid )
  );

  if (! hProcess.isValid ())
    return false;

  wchar_t wszImageName [MAX_PATH] = {};
  DWORD    dwImageNameLen         = MAX_PATH;

  if (QueryFullProcessImageNameW (hProcess, 0, wszImageName, &dwImageNameLen))
  {
    bool is_skif =
      StrStrIW (wszImageName, L"SKIF") != nullptr;

    if (! is_skif)
    { // Most likely any false positive is SKIV, because it shares window code.
      SK_RunOnce (
        SK_LOGs0 ( L"InjectUtil",
          L"Tested window (%x) belongs to '%ws', NOT SKIF",
            hWnd, wszImageName );
      );
    }

    return is_skif;
  }

  return false;
}

void
SK_Inject_PostHeartbeatToSKIF (void)
{
  static HWND hWndSKIF =
    FindWindow (L"SK_Injection_Frontend", nullptr);

  if (hWndSKIF == nullptr)
  {
    // This isn't the correct window since SKIF was overhauled, but
    //   it at least gives us the process that owns the window we need...
    hWndSKIF =
      FindWindow (L"SKIF_ImGuiWindow", nullptr);
  
    DWORD                                   dwPidOfSKIF = 0;
    SK_GetWindowThreadProcessId (hWndSKIF, &dwPidOfSKIF);
  
    EnumWindows ( []( HWND   hWnd,
                      LPARAM lParam ) -> BOOL
    {
      DWORD                                   dwPID = 0;
      if (SK_GetWindowThreadProcessId (hWnd, &dwPID))
      {
        if (dwPID != (DWORD)lParam)
          return TRUE;
  
        if (SK_GetWindowLongPtrW (hWnd, GWL_EXSTYLE) & WS_EX_APPWINDOW)
        {
          // What we thought was SKIF, is not actually SKIF...?
          if (! SK_Inject_IsWindowSKIF (hWnd))
            return TRUE;

          // Success, we found the app window!
          hWndSKIF = hWnd;
  
          SK_SetWindowLongPtrW (   hWnd, GWL_EXSTYLE,
            SK_GetWindowLongPtrW ( hWnd, GWL_EXSTYLE ) & ~WS_EX_NOACTIVATE );
  
          return FALSE;
        }
      }
  
      return TRUE;
    }, (LPARAM)dwPidOfSKIF);
  }

  // If done constantly (true), it helps with alt-tab
  if (SK_Inject_IsHookActive ()) 
  { // If done only when hook is running, it fixes periodic VRR loss,
    //   but alt-tab can still flicker.
    if (hWndSKIF != game_window.hWnd && IsWindow (hWndSKIF))
    {
      DWORD                                   dwPid = 0x0;
      SK_GetWindowThreadProcessId (hWndSKIF, &dwPid);
    
      if ( dwPid != 0x0 &&
           dwPid != GetCurrentProcessId () )
      {
        static DWORD
            dwLastHeartbeat = 0;
        if (dwLastHeartbeat < SK_timeGetTime () - 133)
        {   dwLastHeartbeat = SK_timeGetTime ();
          if ( SK_GetForegroundWindow () == hWndSKIF &&
                    SK_Inject_IsWindowSKIF (hWndSKIF) )
          {
            // Wake SKIF up and make it redraw
            PostMessage (hWndSKIF, WM_NULL, 0x0, 0x0);
          }
        }
      }

      else
      {
        hWndSKIF = game_window.hWnd;
      }
    }

    // SKIF's not running, we'll re-check every 1250 ms
    else
    {
      static DWORD
          dwLastWindowCheck = 0;
      if (dwLastWindowCheck < SK_timeGetTime () - 1250)
      {
        hWndSKIF = nullptr;
      }

      else
      {
        // Deliberately wrong window, it will be the same PID
        hWndSKIF = game_window.hWnd;
      }
    }
  }
}

void
SK_Inject_ReturnToSKIF (void)
{
  static HWND hWndExisting =
    FindWindow (L"SK_Injection_Frontend", nullptr);

  if (hWndExisting == nullptr)
  {
    // This isn't the correct window since SKIF was overhauled, but
    //   it at least gives us the process that owns the window we need...
    hWndExisting =
      FindWindow (L"SKIF_ImGuiWindow", nullptr);
  
    DWORD                                       dwPidOfSKIF = 0;
    SK_GetWindowThreadProcessId (hWndExisting, &dwPidOfSKIF);
  
    EnumWindows ( []( HWND   hWnd,
                      LPARAM lParam ) -> BOOL
    {
      DWORD                                   dwPID = 0;
      if (SK_GetWindowThreadProcessId (hWnd, &dwPID))
      {
        if (dwPID != (DWORD)lParam)
          return TRUE;
  
        if (SK_GetWindowLongPtrW (hWnd, GWL_EXSTYLE) & WS_EX_APPWINDOW)
        {
          // What we thought was SKIF, is not actually SKIF...?
          if (! SK_Inject_IsWindowSKIF (hWnd))
            return TRUE;

          // Success, we found the app window!
          hWndExisting = hWnd;
  
          SK_SetWindowLongPtrW (   hWnd, GWL_EXSTYLE,
            SK_GetWindowLongPtrW ( hWnd, GWL_EXSTYLE ) & ~WS_EX_NOACTIVATE );
  
          return FALSE;
        }
      }
  
      return TRUE;
    }, (LPARAM)dwPidOfSKIF);
  }
  
  if (hWndExisting != nullptr && IsWindow (hWndExisting))
  {
    DWORD                                       dwPid = 0x0;
    SK_GetWindowThreadProcessId (hWndExisting, &dwPid);
  
    if ( dwPid != 0x0 &&
         dwPid != GetCurrentProcessId () &&
           SK_Inject_IsWindowSKIF (hWndExisting) )
    {
#define        WM_SKIF_REPOSITION     WM_USER + 0x4096
constexpr UINT WM_SKIF_EVENT_SIGNAL = WM_USER + 0x3000;

      AllowSetForegroundWindow     (dwPid);
      PostMessage                  (hWndExisting, WM_SKIF_EVENT_SIGNAL, (WPARAM)hWndExisting, 0x0);
      PostMessage                  (hWndExisting, WM_SKIF_REPOSITION, 0x0, 0x0);

      if (!    SetForegroundWindow (hWndExisting))
        SK_RealizeForegroundWindow (hWndExisting);

      if (SK_Inject_GetFocusWindow (            ) == nullptr)
          SK_Inject_SetFocusWindow (hWndExisting);

      if (                IsIconic (hWndExisting))
        SK_ShowWindowAsync         (hWndExisting, SW_RESTORE);
      else
        SK_ShowWindowAsync         (hWndExisting, SW_SHOW);
    }
  }
}

bool
__stdcall
SK_ShutdownCore (const wchar_t* backend)
{
  const auto
  bNoCanWait =
    RtlpWaitCouldDeadlock ();

  if (bNoCanWait)
  {
    //SK_LOGi0 (L"Wait Could Deadlock!");
  }

  SK_Inject_BroadcastExitNotify ();
  SK_DisableApplyQueuedHooks    ();

  if (        __SK_DLL_TeardownEvent != nullptr)
    SetEvent (__SK_DLL_TeardownEvent);

  // Fast path for DLLs that were never really attached.
  if (! __SK_DLL_AttachTime)
  {
    if (dll_log.isAllocated ())
        dll_log->Log (L"SK_ShutdownCore (...) on uninitialized DLL");

    SK_MinHook_UnInit ();

    return
      true;
  }

  if (__SK_bypass)
    return true;

  SK_PrintUnloadedDLLs (&dll_log.get ());

  dll_log->LogEx ( false,
                L"========================================================="
                L"========= (End  Unloads) ================================"
                L"==================================\n" );

  dll_log->Log (L"[ SpecialK ] *** Initiating DLL Shutdown ***");

  SK_Win32_DestroyBackgroundWindow (); // Destroy the aspect ratio stretch window

  SK::Diagnostics::Debugger::CloseConsole ();

  auto &rb =
    SK_GetCurrentRenderBackend ();

  rb.resetTemporaryDisplayChanges ();

  if (sk::NVAPI::nv_hardware)
  {
    dll_log->LogEx  (true, L"[  Reflex  ] Shutting down PCL Stats...                   ");

    DWORD dwTime =
       SK_timeGetTime ();

    PCLSTATS_SHUTDOWN ();

    dll_log->LogEx  (false, L"done! (%4u ms)\n",            SK_timeGetTime () - dwTime);
  }

  dll_log->LogEx    (true, L"[ ETWTrace ] Shutting down ETW Trace Providers...         ");

  DWORD dwTime =
       SK_timeGetTime ();

  if (SK_ETW_EndTracing ())
    dll_log->LogEx  (false, L"done! (%4u ms)\n",            SK_timeGetTime () - dwTime); else
    dll_log->LogEx  (false, L"fail! (%4u ms -> Timeout)\n", SK_timeGetTime () - dwTime);

  const wchar_t* config_name = backend;

  if (SK_IsInjected ())
  {
    config_name = L"SpecialK";
  }

  if (sk::NVAPI::app_name.find (L"ds3t.exe") == std::wstring::npos)
  {
    dll_log->LogEx       (true,  L"[ SpecialK ] Saving user preferences to"
                                 L" %10s.ini... ", config_name);
    dwTime =
          SK_timeGetTime (           );
    SK_SaveConfig        (config_name);
    dll_log->LogEx       (false, L"done! (%4u ms)\n", SK_timeGetTime () - dwTime);
  }


  dll_log->LogEx    (true, L"[   ImGui  ] Shutting down ImGui...                       ");

  dwTime =
    SK_timeGetTime ();

  if (ImGui::GetCurrentContext () != nullptr)
  {
    ImGui::Shutdown (
      //ImGui::GetCurrentContext ()
    );
  }

  dll_log->LogEx    (false, L"done! (%4u ms)\n",            SK_timeGetTime () - dwTime);


  __SKX_WinHook_InstallInputHooks (nullptr);

  SK_Win32_CleanupDummyWindow ();

  // No more exit rumble, please :)
  SK_XInput_Enable (FALSE);

  if (config.window.background_mute)
    SK_SetGameMute (false);

  if (config.window.confine_cursor)
    SK_ClipCursor (nullptr);

  // These games do not handle resolution correctly
  switch (SK_GetCurrentGameID ())
  {
    case SK_GAME_ID::DarkSouls3:
    case SK_GAME_ID::Fallout4:
    case SK_GAME_ID::FinalFantasyX_X2:
    case SK_GAME_ID::DisgaeaPC:
      if (ChangeDisplaySettingsA_Original != nullptr)
          ChangeDisplaySettingsA_Original (nullptr, CDS_RESET);
      break;

#ifdef _M_AMD64
    case SK_GAME_ID::MonsterHunterWorld:
    {
      SK_MHW_PlugIn_Shutdown ();
    } break;
#endif
  }

  SK_AutoClose_LogEx (dll_log, dll);

  if (SK_GetFramesDrawn () > 0)
  {
    // If an override was applied at runtime, reset it
    SK_Steam_ForceInputAppId (0);

    if (SK_Inject_GetFocusWindow (       ) == game_window.hWnd)
        SK_Inject_SetFocusWindow (nullptr);

    if (config.system.return_to_skif)
    {
      SK_Inject_ReturnToSKIF ();
    }
  }

  SK_Power_StopEffectiveModeCallbacks ();

  SK::DXGI::ShutdownBudgetThread ();

  dll_log->LogEx    (true, L"[ GPU Stat ] Shutting down Performance Monitor Thread...  ");

  dwTime =
    SK_timeGetTime ();
  SK_EndGPUPolling ();

  dll_log->LogEx    (false, L"done! (%4u ms)\n", SK_timeGetTime () - dwTime);

  SK_Console::getInstance ()->End ();

  auto ShutdownWMIThread =
  []( volatile HANDLE&  hSignal,
      volatile HANDLE&  hThread,
        const wchar_t* wszName )
  {
    if ( wszName == nullptr ||
         ( ReadPointerAcquire (&hSignal) == INVALID_HANDLE_VALUE &&
           ReadPointerAcquire (&hThread) == INVALID_HANDLE_VALUE ) )
    {
      return;
    }

    wchar_t   wszFmtName [32] = { };
    lstrcatW (wszFmtName, wszName);
    lstrcatW (wszFmtName, L"...");

    dll_log->LogEx (true, L"[ Perfmon. ] Shutting down %-30s ", wszFmtName);

    DWORD dwTime_WMIShutdown =
         SK_timeGetTime ();

    if (hThread != INVALID_HANDLE_VALUE)
    {
      // Signal the thread to shutdown
      if ( hSignal != INVALID_HANDLE_VALUE &&
            SignalObjectAndWait (hSignal, hThread, 66UL, FALSE)
                        != WAIT_OBJECT_0 )  // Give 66 milliseconds, and
      {                                     // then we're killing
        SK_TerminateThread (hThread, 0x00); // the thing!
                            hThread = INVALID_HANDLE_VALUE;
      }

      if (hThread != INVALID_HANDLE_VALUE)
      {
        if (SK_CloseHandle (hThread))
                            hThread = INVALID_HANDLE_VALUE;
      }
    }

    if (hSignal != INVALID_HANDLE_VALUE)
    {
      if (SK_CloseHandle (hSignal))
                          hSignal = INVALID_HANDLE_VALUE;
    }

    dll_log->LogEx ( false, L"done! (%4u ms)\n",
                    SK_timeGetTime () - dwTime_WMIShutdown );
  };

  static auto& cpu_stats      = *SK_WMI_CPUStats;
  static auto& disk_stats     = *SK_WMI_DiskStats;
  static auto& pagefile_stats = *SK_WMI_PagefileStats;

  ShutdownWMIThread (cpu_stats .hShutdownSignal,
                     cpu_stats.hThread,               L"CPU Monitor"     );
  ShutdownWMIThread (disk_stats.hShutdownSignal,
                     disk_stats.hThread,              L"Disk Monitor"    );
  ShutdownWMIThread (pagefile_stats.hShutdownSignal,
                     pagefile_stats.hThread,          L"Pagefile Monitor");

  PowerSetActiveScheme (
    nullptr,
      &config.cpu.power_scheme_guid_orig
  );

  if (! SK_Debug_IsCrashing ())
  {
    SK::Framerate::Shutdown ();

    dll_log->LogEx             (true, L"[ WMI Perf ] Shutting down WMI WbemLocator...             ");
    dwTime = SK_timeGetTime    ();
    SK_WMI_Shutdown            ();
    dll_log->LogEx             (false, L"done! (%4u ms)\n", SK_timeGetTime () - dwTime);

    if (! config.platform.silent)
    {
      dll_log->LogEx           (true, L"[ SpecialK ] Shutting down SteamAPI integration...        ");
      dwTime = SK_timeGetTime  ();
      SK::SteamAPI::Shutdown   ();
      dll_log->LogEx           (false, L"done! (%4u ms)\n", SK_timeGetTime () - dwTime);
    }

    // Due to hooks on NvAPI methods, best to just leave the library loaded
#if 0
    if (nvapi_init)
    {
      dll_log->LogEx           (true, L"[  NV API  ] Unloading NVAPI Library...                   ");
      dwTime = SK_timeGetTime  ();
      sk::NVAPI::UnloadLibrary ();
      dll_log->LogEx           (false, L"done! (%4u ms)\n", SK_timeGetTime () - dwTime);
    }
#endif

    if (! plugin_mgr->exit_game_fns.empty ())
    {
      dll_log->LogEx           (true, L"[ SpecialK ] Shutting down game specific plug-ins...      ");
      dwTime = SK_timeGetTime  ();
      // Invoke any plug-in's exit callback
      for ( auto exit_plugin_fn : plugin_mgr->exit_game_fns )
      {
        exit_plugin_fn ();
      }
      dll_log->LogEx           (false, L"done! (%4u ms)\n", SK_timeGetTime () - dwTime);
    }

    // Unload imports before shutting down MinHook
    SK_UnloadImports           ();

    dll_log->LogEx             (true, L"[ SpecialK ] Shutting down MinHook...                     ");

    dwTime = SK_timeGetTime    ();
    SK_MinHook_UnInit          ();
    dll_log->LogEx             (false, L"done! (%4u ms)\n", SK_timeGetTime () - dwTime);

    dll_log->LogEx             (true, L"[ SpecialK ] Closing secondary logs...                    ");

    dwTime = SK_timeGetTime    ();
    SK_Log_CleanupLogs         ();
    SK_ReShadeAddOn_CleanupConfigAndLogs
                               ();
    dll_log->LogEx             (false, L"done! (%4u ms)\n", SK_timeGetTime () - dwTime);
  }


  dll_log->Log (L"[ SpecialK ] Custom %s.dll Detached (pid=0x%04x)",
    backend, GetCurrentProcessId ());


  if (! SK_Debug_IsCrashing ())
  {
    if (config.system.handle_crashes)
      SK::Diagnostics::CrashHandler::Shutdown ();
  }

  WriteRelease (&__SK_Init, -2);

#ifdef _SK_CONSISTENCY_CHECK
  // Teardown static globals
  __SK_LazyRiver.atExit ();
#endif

  return true;
}

void
SK_FrameCallback ( SK_RenderBackend& rb,
                   ULONG64           frames_drawn =
                                       SK_GetFramesDrawn () )
{
  void
  ActivateWindow ( HWND hWnd,
                   bool active          = false,
                   HWND hWndDeactivated = 0 );

  switch (frames_drawn)
  {
    // First frame
    //
    case 0:
    {
      ActivateWindow (game_window.hWnd, SK_GetForegroundWindow () == game_window.hWnd);

      // Notify anything that was waiting for injection into this game
      SK_Inject_BroadcastInjectionNotify ();

      wchar_t *wszDescription = nullptr;

      if ( SUCCEEDED ( GetCurrentThreadDescription (&wszDescription) )         &&
                                                     wszDescription != nullptr &&
                                            wcslen ( wszDescription )
         )
      {
        if (StrStrIW (wszDescription, L"[GAME] Primary Render Thread") == nullptr)
        {
          SetCurrentThreadDescription (
            SK_FormatStringW ( L"[GAME] Primary Render < %s >",
                       wszDescription ).c_str ()
                                      );
        }
      }

      else
      {
        SetCurrentThreadDescription (L"[GAME] Primary Render Thread");
      }

      SK_LocalFree (wszDescription);


      if ( (static_cast <int> (rb.api) & static_cast <int> (SK_RenderAPI::D3D11)) ||
           (static_cast <int> (rb.api) & static_cast <int> (SK_RenderAPI::D3D12)) )
      {
        auto hdr =
          SK_HDR_GetWidget ();

        if (hdr != nullptr)
            hdr->run ();
      }

      // Maybe make this into an option, but for now just get this the hell out
      //   of there almost no software should be shipping with FP exceptions,
      //     it causes compatibility problems.
      _controlfp (MCW_EM, MCW_EM);


      // Load user-defined DLLs (Late)
      SK_RunLHIfBitness ( 64, SK_LoadLateImports64 (),
                              SK_LoadLateImports32 () );

      if (ReadAcquire64 (&SK_SteamAPI_CallbackRunCount) < 1)
      {
        // Steam Init: Better late than never

        SK_Steam_TestImports (SK_GetModuleHandle (nullptr));
      }

      if (config.system.handle_crashes)
        SK::Diagnostics::CrashHandler::Reinstall ();

      __target_fps = config.render.framerate.target_fps;
    } break;


    // Grace period frame, just ignore it
    //
    case 1:
    case 2:
      break;


    // 2+ frames drawn
    //
    default:
    {
      ActivateWindow (game_window.hWnd, SK_GetForegroundWindow () == game_window.hWnd);

      if (game_window.active)
        SK_RunOnce (SK_Steam_ProcessWindowActivation (game_window.active));

      auto priority =
        SK_Thread_GetCurrentPriority ();

      static int _last_priority = config.priority.minimum_render_prio;

      // Adjust render thread priority if user wants it
      if (       priority <  config.priority.minimum_render_prio ||
           _last_priority != config.priority.minimum_render_prio )
      {
        SK_Thread_SetCurrentPriority (config.priority.minimum_render_prio);

        _last_priority = priority;
      }

      // This is going to fail on Steam Deck EVERY time, so limit the number of attempts...
      //   try for up to 15 seconds, once every 250 ms.
      static DWORD last_init_try   = 0;
      static int wasapi_init_tries = 0;
      if (       wasapi_init_tries < 60 &&
                   last_init_try   < SK_timeGetTime () - 250UL)
      {
        if (SK_WASAPI_Init ()) wasapi_init_tries = INT_MAX;

        else
        {
          wasapi_init_tries++;
            last_init_try = SK_timeGetTime ();
        }
      }


#pragma region HDR Late Injection HACK
      // Very late attempt to change the colorspace if we were not
      //   around to catch the initial SwapChain creation...
      static bool _10BitHDR = false,
                  _16BitHDR = false,
                       _SDR = true;

      if (  __SK_HDR_16BitSwap != _16BitHDR ||
            __SK_HDR_10BitSwap != _10BitHDR ||
          !(__SK_HDR_16BitSwap || __SK_HDR_10BitSwap) != _SDR )
      {
        if (SK_ComQIPtr <IDXGISwapChain3> pSwapChain3 (rb.swapchain);
                                          pSwapChain3 != nullptr && __SK_HDR_UserForced)
        {
          if (
            SUCCEEDED (
              pSwapChain3->SetColorSpace1 ( __SK_HDR_16BitSwap ? DXGI_COLOR_SPACE_RGB_FULL_G10_NONE_P709 :
                                            __SK_HDR_10BitSwap ? DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020
                                                               : DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709 )
                      )
             )
          {
            _10BitHDR = __SK_HDR_10BitSwap;
            _16BitHDR = __SK_HDR_16BitSwap;
            _SDR      = !(__SK_HDR_10BitSwap || __SK_HDR_16BitSwap);
          }
        }
      }
#pragma endregion


      // Delayed Init  (Elden Ring vs. Flawless Widescreen compat hack)
      //                 * Also fix Steam Input in CAPCOM games
      if (frames_drawn > 15)
      {
        // Horizon: Forbidden West Hack
        // ----------------------------
        //   Since we have to delay injection as a compatibility hack for Nixxes
        //     wonky Streamline implementation not to crash the game, do some stuff
        //       that we normally would have caught during app startup.
        SK_RunOnce (
        if ( rb.isHDRCapable () && SK_IsModuleLoaded (L"sl.interposer.dll") && 
                                   SK_IsModuleLoaded (L"EOSOVH-Win64-Shipping.dll") )
        {
          SK_ImGui_CreateNotification (
            "EOSOVH.Warning", SK_ImGui_Toast::Warning,
            "If the game crashes while turning on HDR or switching display modes, consider "
            "deleting EOSOVH-Win64-Shipping.dll from Epic Games\\Launcher\\Portal\\Extras\\Overlay",
            "Known Incompatibility Detected", 5000UL, SK_ImGui_Toast::UseDuration |
                                                      SK_ImGui_Toast::ShowTitle   |
                                                      SK_ImGui_Toast::ShowCaption );
        }
        if (SK_GetCurrentGameID () == SK_GAME_ID::HorizonForbiddenWest)
        {
          SK_ComQIPtr <IDXGISwapChain>
              pSwapChain (rb.swapchain);
          if (pSwapChain != nullptr)
          {
            DXGI_SWAP_CHAIN_DESC  swapDesc = { };
            pSwapChain->GetDesc (&swapDesc);
            //
            // We likely will miss the initial call to SetColorSpace1, so make it again...
            //   this will trigger our own hook and get things rolling.
            //
            //  * Game never uses 10-bpc in SDR, sadly... but that's convenient for this.
            //
            if (swapDesc.BufferDesc.Format == DXGI_FORMAT_R10G10B10A2_UNORM)
            {
              if (! __SK_HDR_16BitSwap)
                    __SK_HDR_10BitSwap = true;

              SK_ComQIPtr <IDXGISwapChain4>
                  pSwap4 (pSwapChain);
              if (pSwap4 != nullptr)
                  pSwap4->SetColorSpace1 (DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020);
            }
          }
        });

        if (rb.windows.sdl && (! SK_HID_PlayStationControllers.empty ()))
        {
          static ULONG64 toggle_frame = 0ULL;
          static bool    toggling     = false;
          static DWORD   time_start   = SK_timeGetTime ();

          if ((! toggling) && (! config.input.gamepad.disable_hid) && (frames_drawn > 666 || SK_timeGetTime () - time_start > 4000UL))
          {
            toggling = true;

            config.input.gamepad.disable_hid = true;

            if (toggle_frame == 0)
            {   toggle_frame  = frames_drawn;
              SK_Win32_NotifyDeviceChange (false, false);
            }
          }

          else if (toggling)
          {
            if (frames_drawn > toggle_frame + 60)
            {
              SK_RunOnce (
                config.input.gamepad.disable_hid = false;
                SK_Win32_NotifyDeviceChange (!config.input.gamepad.xinput.blackout_api, !config.input.gamepad.disable_hid)
              );
            }
          }
        }

        if (game_window.WndProc_Original != nullptr)
        {
          if (game_window.hWnd != 0)
          {

            if (config.window.activate_at_start || config.window.background_render)
            {
#define SK_SAFE_CALLWNDPROC(Msg,wParam,lParam) \
 SK_COMPAT_SafeCallProc (&game_window, game_window.hWnd, (Msg), (WPARAM)(wParam), (LPARAM)(lParam));

              // Activate the game window one time
              //   (workaround wonkiness from splash screens, etc.)
              SK_RunOnce (
              {
                DWORD                                                                        dwProcId = 0x0;
                if (GetCurrentThreadId () == SK_GetWindowThreadProcessId (game_window.hWnd, &dwProcId))
                {
                  SK_SAFE_CALLWNDPROC (WM_ACTIVATEAPP,                      TRUE, 0);
                  SK_SAFE_CALLWNDPROC (WM_ACTIVATE,    MAKEWPARAM (WA_ACTIVE, 0), 0);
                  SK_SAFE_CALLWNDPROC (WM_SETFOCUS,                            0, 0);
                }

                else
                {
                  SendMessageTimeout (game_window.hWnd, WM_ACTIVATEAPP, TRUE, 0, SMTO_ABORTIFHUNG | SMTO_BLOCK, 100UL, nullptr);
                  SendMessageTimeout (game_window.hWnd, WM_ACTIVATE,
                                                   MAKEWPARAM (WA_ACTIVE, 0), 0, SMTO_ABORTIFHUNG | SMTO_BLOCK, 100UL, nullptr);
                  SendMessageTimeout (game_window.hWnd, WM_SETFOCUS,       0, 0, SMTO_ABORTIFHUNG | SMTO_BLOCK, 100UL, nullptr);
                }

                // Activate the window
                //if (IsIconic         (game_window.hWnd))
                //  SK_ShowWindowAsync (game_window.hWnd, SW_SHOWNORMAL);
                if (IsIconic         (game_window.hWnd))
                  SK_ShowWindowAsync (game_window.hWnd, SW_RESTORE);
                else
                  SK_ShowWindowAsync (game_window.hWnd, SW_SHOW);

                if (!    SetForegroundWindow (game_window.hWnd))
                  SK_RealizeForegroundWindow (game_window.hWnd);
              });
            }

            // If user wants position / style overrides, kick them off on the first
            //   frame after a window procedure has been established.
            //
            //  (nb: Must be implemented asynchronously)
            //
            SK_RunOnce (
            {
              SK_Window_RepositionIfNeeded ();

              game_window.active |=
                (SK_GetForegroundWindow () == game_window.hWnd);
            });
          }
        }

        SK_RunOnce (SK_Input_HookScePad ());

        if (rb.api != SK_RenderAPI::D3D11  &&
            rb.api != SK_RenderAPI::D3D9Ex &&
            rb.api != SK_RenderAPI::D3D9)
        {
          rb.gsync_state.update ();
        }

        SK_RunOnce (rb.gsync_state.update (true));
      }
    } break;
  }
}

std::atomic_int __SK_RenderThreadCount = 0;

void
SK_MMCS_EndBufferSwap (void)
{
  auto* task  =
    SK_MMCS_GetTaskForThreadIDEx (
      SK_Thread_GetCurrentId (), "[GAME] Render Thread",
                                  "Games", "DisplayPostProcessing" );

  if (task != nullptr)
      task->disassociateWithTask ();

//SK_Thread_SetCurrentPriority (
//  SK_TLS_Bottom ()->win32->thread_prio
//);
}

void
SK_MMCS_BeginBufferSwap (void)
{
//SK_TLS_Bottom ()->win32->thread_prio =
//  SK_Thread_GetCurrentPriority (                             );
//  SK_Thread_SetCurrentPriority (THREAD_PRIORITY_TIME_CRITICAL);

  static concurrency::concurrent_unordered_set <DWORD> render_threads;

  static const auto&
    game_id = SK_GetCurrentGameID ();

  auto tid =
    SK_Thread_GetCurrentId ();

  SK_MMCS_TaskEntry* task = nullptr;

  if ( ! render_threads.count (tid) )
  {
    __SK_RenderThreadCount++;

    static bool first = true;
                task  =
      SK_MMCS_GetTaskForThreadIDEx ( tid,                       first ?
                                       "[GAME] Primary Render Thread" :
                                       "[GAME] Ancillary Render Thread",
                                        "Games", "DisplayPostProcessing" );

    if ( task != nullptr )
    {
      if (game_id != SK_GAME_ID::AssassinsCreed_Odyssey)
      {
        //if (first)
          task->setPriority (AVRT_PRIORITY_CRITICAL);
        //else
          //task->setPriority (AVRT_PRIORITY_HIGH); // Assymetric priority is not desirable
      }

      else
        task->setPriority (AVRT_PRIORITY_LOW);

      render_threads.insert (tid);

      first = false;
    }
  }

  else
  {
    task =
      SK_MMCS_GetTaskForThreadIDEx ( tid, "[GAME] Render Thread",
                                           "Games", "DisplayPostProcessing" );

    if (task != nullptr)
        task->reassociateWithTask ();
  }
}

__declspec (noinline)
void
__stdcall
SK_BeginBufferSwapEx (BOOL bWaitOnFail)
{
  void SK_Render_CountVBlanks (void);
       SK_Render_CountVBlanks ();

  if (config.render.framerate.enable_mmcss)
  {
    SK_MMCS_BeginBufferSwap ();
  }

  SK_RenderBackend& rb =
    SK_GetCurrentRenderBackend ();

  static SK_RenderAPI LastKnownAPI =
    SK_RenderAPI::Reserved;

  if ( (int)rb.api        &
       (int)SK_RenderAPI::D3D11 )
  {
    SK_D3D11_BeginFrame ();
  }

  else if ( (int)rb.api &
            (int)SK_RenderAPI::D3D12 )
  {
    SK_D3D12_BeginFrame ();
  }

  rb.driverSleepNV      (0);
  rb.setLatencyMarkerNV (RENDERSUBMIT_END);

  if (config.render.framerate.enforcement_policy == 0 && rb.swapchain.p != nullptr)
  {
    SK::Framerate::Tick ( bWaitOnFail, 0.0, { 0,0 }, rb.swapchain.p );
  }

  rb.present_staging.begin_overlays.time.QuadPart =
    SK_QueryPerf ().QuadPart;

  // Invoke any plug-in's frame begin callback
  for ( auto begin_frame_fn : plugin_mgr->begin_frame_fns )
  {
    begin_frame_fn ();
  }

  // Handle init. actions that depend on the number of frames
  //   drawn...
  SK_FrameCallback (rb);

  if (config.render.framerate.enforcement_policy == 1 && rb.swapchain.p != nullptr)
  {
    SK::Framerate::Tick ( bWaitOnFail, 0.0, { 0,0 }, rb.swapchain.p );
  }

  if (SK_Steam_PiratesAhoy () && (! SK_ImGui_Active ()))
  {
    SK_ImGui_Toggle ();
  }

  LastKnownAPI =
    rb.api;

  rb.present_staging.submit.time =
    SK_QueryPerf ();

  if ( config.render.framerate.enforcement_policy == 4 ||
       config.render.framerate.enforcement_policy <  0 )
  {
    if (rb.swapchain.p != nullptr)
      SK::Framerate::Tick ( bWaitOnFail, 0.0, { 0,0 }, rb.swapchain.p );
  }
}

__declspec (noinline)
void
__stdcall
SK_BeginBufferSwap (void)
{
  return
    SK_BeginBufferSwapEx (TRUE);
}

// Todo, move out of here
void
SK_Input_PollKeyboard (void)
{
  constexpr ULONGLONG poll_interval = 1ULL;

  //
  // Do not poll the keyboard while the game window is inactive
  //
  bool skip = true;

  if ( SK_IsGameWindowActive () )
    skip = false;

  if (skip)
    return;

  static ULONGLONG last_osd_scale { 0ULL };
  static ULONGLONG last_poll      { 0ULL };
  static ULONGLONG last_drag      { 0ULL };

  SYSTEMTIME    stNow;
  FILETIME      ftNow;
  LARGE_INTEGER ullNow;

  GetSystemTime        (&stNow);
  SystemTimeToFileTime (&stNow, &ftNow);

  ullNow.HighPart = ftNow.dwHighDateTime;
  ullNow.LowPart  = ftNow.dwLowDateTime;

  static bool toggle_drag = false;

  static const auto& io =
    ImGui::GetIO ();

  if ( io.KeysDown [VK_CONTROL] &&
       io.KeysDown [  VK_SHIFT] &&
       io.KeysDown [ VK_SCROLL] )
  {
    if (! toggle_drag)
    {
      config.window.drag_lock =
        (! config.window.drag_lock);
    }

    toggle_drag = true;

    if (config.window.drag_lock)
    {
      ClipCursor (nullptr);
    }
  }

  else
  {
    toggle_drag = false;
  }

  if (config.window.drag_lock)
  {
    SK_CenterWindowAtMouse (config.window.persistent_drag);
  }


  if (ullNow.QuadPart - last_osd_scale > 25ULL * poll_interval)
  {
    if (io.KeysDown [config.osd.keys.expand [0]] &&
        io.KeysDown [config.osd.keys.expand [1]] &&
        io.KeysDown [config.osd.keys.expand [2]])
    {
      last_osd_scale = ullNow.QuadPart;
      SK_ResizeOSD (+0.1f);
    }

    if (io.KeysDown [config.osd.keys.shrink [0]] &&
        io.KeysDown [config.osd.keys.shrink [1]] &&
        io.KeysDown [config.osd.keys.shrink [2]])
    {
      last_osd_scale = ullNow.QuadPart;
      SK_ResizeOSD (-0.1f);
    }
  }

  static bool toggle_time = false;
  if (io.KeysDown [config.time.keys.toggle [0]] &&
      io.KeysDown [config.time.keys.toggle [1]] &&
      io.KeysDown [config.time.keys.toggle [2]])
  {
    if (! toggle_time)
    {
      config.time.show =
        (! config.time.show);
    }

    toggle_time = true;
  }

  else
  {
    toggle_time = false;
  }
}

void
SK_BackgroundRender_EndFrame (void)
{
  // Side-effect:  Evaluates Steam AppId Override
  SK_ImGui_WantGamepadCapture ();

  const SK_RenderBackend& rb =
    SK_GetCurrentRenderBackend ();

  static bool background_last_frame = false;
  static bool fullscreen_last_frame = false;
  static bool first_frame           = true;

  if (            first_frame ||
       (background_last_frame != config.window.background_render) )
  {

    if (SK_GetCurrentGameID () == SK_GAME_ID::Hello_Kitty_Island_Adventure)
    {
      config.window.background_render       = false;
      config.input.gamepad.xinput.emulate   = false;
      config.nvidia.reflex.enforcement_site = 0; // Reduce sutter
    }

    if (first_frame && ( SK_GetModuleHandleW (L"SDL2.dll") ||
                         SK_GetModuleHandleW (L"SDL3.dll")) )
    {
      // A few games change the window class name from the default...
      //   assume that the game uses SDL if one of its DLLs are loaded.
      SK_GetCurrentRenderBackend ().windows.sdl = true;

      // A more reasonable check might be for a thread named SDL_joystick,
      //   but that seems to be spawned unpredictably late.
    }

    first_frame = false;

    // Does not indicate the window was IN the background, but that it
    //   was rendering in a special mode that would allow the game to
    //     continue running while it is in the background.
    background_last_frame =
        config.window.background_render;
    if (config.window.background_render)
    {
      HWND     hWndGame  = SK_GetGameWindow ();
      LONG_PTR lpStyleEx =
        SK_GetWindowLongPtrW ( hWndGame,
                                 GWL_EXSTYLE );

      LONG_PTR lpStyleExNew =
            // Add style to ensure the game shows in the taskbar ...
        ( ( lpStyleEx |  WS_EX_APPWINDOW  )
                      & ~WS_EX_TOOLWINDOW );
                     // And remove one that prevents taskbar activation...

      if (IsIconic               ( hWndGame ))
           ShowWindowAsync       ( hWndGame, SW_SHOWNORMAL );
      else ShowWindowAsync       ( hWndGame, SW_SHOW       );

      if (lpStyleExNew != lpStyleEx)
      {
        SK_SetWindowLongPtrW     ( hWndGame, GWL_EXSTYLE,
                                              lpStyleExNew );
      }

      SK_RealizeForegroundWindow ( hWndGame                );
    }
  }

  //if (config.display.aspect_ratio_stretch || config.display.focus_mode)
  //{
  //  if (SK_Win32_BackgroundHWND != 0 && IsIconic (game_window.hWnd) && !IsIconic (SK_Win32_BackgroundHWND))
  //  {
  //    ShowWindow (SK_Win32_BackgroundHWND, SW_FORCEMINIMIZE);
  //  }
  //}

#if 0
  else
  {
    // Check if we can correctly handle input such as mousewheel
    //   and keyboard bindings that rely on window focus info.
    if (SK_GetFramesDrawn () > 30)
    {
      SK_RunOnce (
        if (! game_window.mouse.can_track)
        {
          SK_ImGui_WarningWithTitle (
            L"Special K cannot manage window state and "
            L"some input functionality will not work."
            L"\r\n\r\n"
            L" * This is likely caused by a third-party overlay."
            L"\r\n\r\n"
            L"Please restart the game.",
            L"Window Management Is Not Working Correctly"
          );
        }
      );
    }
  }
#endif

  fullscreen_last_frame =
        rb.isTrueFullscreen ();
  if (! fullscreen_last_frame)
  {
    bool implicit_smart_always_on_top =
      (config.window.always_on_top == NoPreferenceOnTop && rb.isFakeFullscreen ());

    static bool last_foreground = false;

    static const
    DWORD     dwProcessId = GetCurrentProcessId ();
    DWORD dwForegroundPid = 0x0;
    DWORD dwForegroundTid = 0x0;
    HWND   hForegroundWnd = SK_GetForegroundWindow ();

    struct {
      HWND   hWnd = 0;
      DWORD dwPid = 0;
      DWORD dwTid = 0;
    } static cached_window;

    if (hForegroundWnd != game_window.hWnd)
    { // This is an expensive call, only do it if GetForegroundWindow
      //   suggest its necessary
      if (cached_window.hWnd != hForegroundWnd)
      {
        cached_window.dwTid =
          SK_GetWindowThreadProcessId (hForegroundWnd, &dwForegroundPid);
        cached_window.hWnd  =          hForegroundWnd;
        cached_window.dwPid =                           dwForegroundPid;
      }
      else
      {
        dwForegroundPid = cached_window.dwPid;
        dwForegroundTid = cached_window.dwTid;
      }
    }
    else dwForegroundPid = dwProcessId;

    if (dwForegroundPid != dwProcessId)
    {
      if (std::exchange (last_foreground, false))
      {
        GUITHREADINFO gti        = {                    };
                      gti.cbSize = sizeof (GUITHREADINFO);
        if ( SK_GetGUIThreadInfo (    dwForegroundTid,
                                             &gti ) )
        {
          static HWND        hWndLastFocus  = 0;
          if (               hWndLastFocus != gti.hwndFocus) {
            SK_Window_OnFocusChange (         gti.hwndFocus,
              std::exchange (hWndLastFocus,   gti.hwndFocus)
            );
          }
        }

        if (config.window.always_on_top == PreventAlwaysOnTop || implicit_smart_always_on_top)
          SK_DeferCommand ("Window.TopMost false");
      }
    }

    else
    {
      if ((! std::exchange (last_foreground, true)) && (config.window.always_on_top >= AlwaysOnTop || implicit_smart_always_on_top))
        SK_DeferCommand ("Window.TopMost true");
    }

    if (hForegroundWnd != game_window.hWnd)
    {
      //
      // If SKIF is in the foreground, and SK is set to background render mode,
      //   then post a message to SKIF to keep it drawing constantly so that VRR
      //     in the game does not disengage.
      //
      //  The minor overhead from SKIF drawing constantly is much less than the
      //    performance oddities caused by the game periodically losing DirectFlip.
      //
      SK_Inject_PostHeartbeatToSKIF ();
    }
  }
}

void
SK_SLI_UpdateStatus (IUnknown *device)
{
  if (config.sli.show && device != nullptr)
  {
    // Get SLI status for the frame we just displayed... this will show up
    //   one frame late, but this is the safest approach.
    if (nvapi_init && sk::NVAPI::CountSLIGPUs () > 0)
    {
      *SK_NV_sli_state =
        sk::NVAPI::GetSLIState (device);
    }
  }
}

void
SK_Battery_UpdateRemainingPowerForAllDevices (void)
{
  static auto lastTimeChecked =
    SK::ControlPanel::current_time - 1000;

  // Avoid expensive API at end-of-frame to minimize stutter
  if (lastTimeChecked >= SK::ControlPanel::current_time - 5000)
    return;

  SK_ImGui_ProcessGamepadStatusBar (false);

  static bool has_battery = true;

  SYSTEM_POWER_STATUS  sps = { };
  SYSTEM_BATTERY_STATE sbs = { };

  const NTSTATUS ntStatus =
    CallNtPowerInformation ( SystemBatteryState,
                             nullptr, 0,
                             &sbs, sizeof (sbs) );

  if (has_battery || (SUCCEEDED (ntStatus) && sbs.Rate != 0))
  {
    if (GetSystemPowerStatus (&sps))
    {
      has_battery   = (sps.BatteryFlag & 128) == 0;
      bool charging = (sps.BatteryFlag & 8  ) == 8;

      uint8_t battery_level =
        sps.ACLineStatus != 1 ?
          sps.BatteryLifePercent :
            charging ? 100 + sps.BatteryLifePercent :
                       ( (sps.BatteryFlag & 2) ||
                         (sps.BatteryFlag & 4) ) ? sps.BatteryLifePercent
                                                 : 255;

      if (battery_level < 255 || sbs.Rate != 0) // 255 = Running on AC (not charging)
      {
        if (battery_level  > 100)
            battery_level -= 100;

        if (battery_level  > 100) // If we're still > 100, it's because running on AC.
            battery_level  = 100;

        if (battery_level <= 25.0f)
        {
          SK_ImGui_CreateNotification (
            "Computer.Battery_Low", SK_ImGui_Toast::Warning,
            "Computer's Battery Level Is Critically Low",
            nullptr, 10000UL, SK_ImGui_Toast::UseDuration |
                              SK_ImGui_Toast::ShowCaption |
                              SK_ImGui_Toast::ShowOnce
          );
        }
      }
    }
  }

  lastTimeChecked = SK::ControlPanel::current_time;
}

void SK_RandomCrapThatShouldBeInPlugIns (void)
{
  // TODO: Add a per-frame callback for plug-ins, because this is stupid
  //
#ifdef _M_AMD64
  static const auto
          game_id = SK_GetCurrentGameID ();
  switch (game_id)
  {
    case SK_GAME_ID::Shenmue:
      WriteRelease (&__SK_SHENMUE_FinishedButNotPresented, 0L);
      break;
    case SK_GAME_ID::FinalFantasyXV:
      SK_FFXV_SetupThreadPriorities ();
      break;
    case SK_GAME_ID::FinalFantasy7Remake:
    {
      uintptr_t pBase =
        (uintptr_t)SK_Debug_GetImageBaseAddr ();

      static uint8_t orig_bytes_3304DC8 [6] = { 0x0 };
      static uint8_t orig_bytes_3304DD0 [6] = { 0x0 };
      static uint8_t orig_bytes_3304E03 [2] = { 0x0 };
      static uint8_t orig_bytes_1C0C42A [5] = { 0x0 };
      static uint8_t orig_bytes_1C0C425 [5] = { 0x0 };

      std::queue <DWORD> suspended;

      auto orig_se =
        SK_SEH_ApplyTranslator (SK_FilteringStructuredExceptionTranslator (EXCEPTION_ACCESS_VIOLATION));
      try
      {
        static bool        bPatchable    = false;
        static bool        bInit         = false;
        if (std::exchange (bInit, true) == false)
        {
          DWORD dwOldProtect = 0x0;

          VirtualProtect ((LPVOID)(pBase + 0x3304DC6), 2, PAGE_EXECUTE_READWRITE, &dwOldProtect);
          bPatchable = ( 0 == memcmp (
                          (LPVOID)(pBase + 0x3304DC6), "\x73\x3d", 2) );
          VirtualProtect ((LPVOID)(pBase + 0x3304DC6), 2, dwOldProtect,           &dwOldProtect);

          if (bPatchable)
          {
            VirtualProtect ((LPVOID)(pBase + 0x3304DC8), 6, PAGE_EXECUTE_READWRITE, &dwOldProtect);
            memcpy (                orig_bytes_3304DC8,
                            (LPVOID)(pBase + 0x3304DC8),                                        6);
            memcpy (        (LPVOID)(pBase + 0x3304DC8), "\x90\x90\x90\x90\x90\x90",            6);
            VirtualProtect ((LPVOID)(pBase + 0x3304DC8), 6, dwOldProtect,           &dwOldProtect);
            VirtualProtect ((LPVOID)(pBase + 0x3304DD0), 6, PAGE_EXECUTE_READWRITE, &dwOldProtect);
            memcpy (                orig_bytes_3304DD0,
                            (LPVOID)(pBase + 0x3304DD0),                                        6);
            memcpy (        (LPVOID)(pBase + 0x3304DD0), "\x90\x90\x90\x90\x90\x90",            6);
            VirtualProtect ((LPVOID)(pBase + 0x3304DD0), 6, dwOldProtect,           &dwOldProtect);

            VirtualProtect ((LPVOID)(pBase + 0x3304E03), 2, PAGE_EXECUTE_READWRITE, &dwOldProtect);
            memcpy (                orig_bytes_3304E03,
                            (LPVOID)(pBase + 0x3304E03),                                        2);
            memcpy (        (LPVOID)(pBase + 0x3304E03), "\x90\x90",                            2);
            VirtualProtect ((LPVOID)(pBase + 0x3304E03), 2, dwOldProtect,           &dwOldProtect);

#if 0
#if 1
            VirtualProtect ((LPVOID)(pBase + 0x1C0C42A), 5, PAGE_EXECUTE_READWRITE, &dwOldProtect);
            memcpy (                orig_bytes_1C0C42A,
                            (LPVOID)(pBase + 0x1C0C42A),                                        5);
            memcpy (        (LPVOID)(pBase + 0x1C0C42A), "\x90\x90\x90\x90\x90",                5);
            VirtualProtect ((LPVOID)(pBase + 0x1C0C42A), 5, dwOldProtect,           &dwOldProtect);
#endif
#if 1
            VirtualProtect ((LPVOID)(pBase + 0x1C0C425), 5, PAGE_EXECUTE_READWRITE, &dwOldProtect);
            memcpy (                orig_bytes_1C0C425,
                            (LPVOID)(pBase + 0x1C0C425),                                        5);
            memcpy (        (LPVOID)(pBase + 0x1C0C425), "\x90\x90\x90\x90\x90",                5);
            VirtualProtect ((LPVOID)(pBase + 0x1C0C425), 5, dwOldProtect,           &dwOldProtect);
#endif
#endif
          }
        }

        if (bPatchable && __target_fps > 0.0f)
        {
          **(float **)(pBase + 0x0590C750) =
                          __target_fps;
        }
      }

      catch (...)
      {
      }
      SK_SEH_RemoveTranslator (orig_se);
    } break;
    default:
      break;
  }
#endif
}

HRESULT
__stdcall
SK_EndBufferSwap (HRESULT hr, IUnknown* device, SK_TLS* pTLS)
{
  auto qpcTimeOfSwap =
    SK_QueryPerf ();

  SK_RenderBackend& rb =
    SK_GetCurrentRenderBackend ();

  rb.frame_delta.markFrame ();

  if (config.display.aspect_ratio_stretch || config.display.focus_mode)
  {
    SK_RunOnce (
      SK_Win32_CreateBackgroundWindow ()
    );
  }

  // Invoke any plug-in's frame end callback
  for ( auto end_frame_fn : plugin_mgr->end_frame_fns )
  {
    end_frame_fn ();
  }


  auto _FrameTick = [&](void) -> void
  {
    bool bWait =
      SUCCEEDED (hr);

    // Only implement waiting on successful Presents,
    //   unsuccessful Presents must return immediately
    SK::Framerate::Tick ( bWait, 0.0,
                        { 0,0 }, rb.swapchain.p );
    // Tock
  };

  if (config.render.framerate.enforcement_policy == 3 && rb.swapchain.p != nullptr)
  {
    _FrameTick ();
  }

  // Various required actions at the end of every frame in order to
  //   support the background render mode in most games.
  SK_BackgroundRender_EndFrame ();

  rb.updateActiveAPI ();

  // Determine Fullscreen Exclusive state in D3D9 / DXGI
  //
  SK_RenderBackendUtil_IsFullscreen ();

  if ( static_cast <int> (rb.api)  &
       static_cast <int> (SK_RenderAPI::D3D11) )
  {
    SK_NGX_UpdateDLSSGStatus ();

    // Clear any resources we were tracking for the shader mod subsystem
    SK_D3D11_EndFrame (pTLS);
  }

  else if ( static_cast <int> (rb.api) &
            static_cast <int> (SK_RenderAPI::D3D12) )
  {
    SK_NGX_UpdateDLSSGStatus ();

    SK_D3D12_EndFrame (pTLS);
  }

  SK_RandomCrapThatShouldBeInPlugIns ();

  rb.updateActiveAPI (
    config.apis.last_known =
        rb.api
  );

  rb.driverSleepNV (1);

  if (config.render.framerate.enforcement_policy == 2)
  {
    _FrameTick ();
  }

  SK_RunOnce (
    SK::DXGI::StartBudgetThread_NoAdapter ()
  );

  SK_SLI_UpdateStatus   (device);
  SK_Input_PollKeyboard (      );

  // Always refresh at the beginning of a frame rather than the end,
  //   a failure event may cause a lengthy delay, missing VBLANK.
  SK_XInput_DeferredStatusChecks ();


  SK_ScePad_PaceMaker ();


  InterlockedIncrementAcquire (
    &SK_RenderBackend::frames_drawn
  );

  SK_StartPerfMonThreads ();

  if (int32_t hooks_queued = (int32_t)ReadULongAcquire (&SK_MinHook_HooksQueuedButNotApplied);
              hooks_queued > 0)
  {
    SK_ApplyQueuedHooks ();
  }


  if (pTLS == nullptr)
      pTLS =
    SK_TLS_Bottom ();

  ULONG64 ullFramesPresented =
    InterlockedIncrement (&pTLS->render->frames_presented);

#ifdef _RENDER_THREAD_TRANSIENCE
  if (ullFramesPresented > rb.most_frames)
  {
    InterlockedExchange ( &rb.most_frames,
                       ullFramesPresented );
    InterlockedExchange ( &rb.thread,
                           SK_Thread_GetCurrentId () );
  }

  InterlockedExchange ( &rb.last_thread,
                              SK_Thread_GetCurrentId () );
#else
  (void)ullFramesPresented;

  if (! ReadULongAcquire (&rb.thread))
  {
    InterlockedExchange ( &rb.thread,
                            SK_Thread_GetCurrentId () );
  }
#endif


  if (__SK_LatentSyncPostDelay > 1LL)
  {
    // Only apply this if Latent Sync is enabled
    if ( config.render.framerate.present_interval == 0 &&
         config.render.framerate.target_fps        > 0.0f )
    {
      SK_AutoHandle hTimer (
        INVALID_HANDLE_VALUE
      );

      SK_Framerate_WaitUntilQPC (
        qpcTimeOfSwap.QuadPart + __SK_LatentSyncPostDelay,
                    hTimer.m_h  );
    }
  }


  if (config.render.framerate.enable_mmcss)
  {
    SK_MMCS_EndBufferSwap ();
  }


#ifdef __UNRELIABLE_WINDOW_ACTIVATION_WORKAROUND
  // Handle missed window activation events; most commonly this is
  //   because of games that use multiple windows.
  DWORD                                                 dwProcess;
  GetWindowThreadProcessId (SK_GetForegroundWindow (), &dwProcess);

  void
  ActivateWindow ( HWND hWnd,
                   bool active          = false,
                   HWND hWndDeactivated = 0 );

  if (GetCurrentProcessId () == dwProcess)
  {
    if (! game_window.active)
    {
      ActivateWindow (game_window.hWnd, true);
    }
  }

  else
  {
    if (game_window.active)
    {
      ActivateWindow (game_window.hWnd, false);
    }
  }
#endif


#if 0
  MONITORINFO
    mi        = {                  };
    mi.cbSize = sizeof (MONITORINFO);

  if (rb.next_monitor != rb.monitor || FALSE == GetMonitorInfo (rb.monitor, &mi))
  {
    if (rb.next_monitor == rb.monitor)
        rb.monitor       = 0;

    SK_Window_RepositionIfNeeded ();
  }
#else
  if ( rb.next_monitor != nullptr &&
       rb.next_monitor != rb.monitor )
  {
    SK_Window_RepositionIfNeeded ();
  }
#endif


  SK_Window_CreateTopMostFixupThread ();


  if (SK_DXGI_IsTrackingBudget ())
  {
    constexpr auto    _BudgetPollingIntervalMs = 250UL;
    static DWORD dwLastBudgetPoll              = 0UL;

    if ( SK::ControlPanel::current_time - dwLastBudgetPoll >
                                               _BudgetPollingIntervalMs )
    {
      // If not using the GPU monitoring services, initialize it manually
      SK_RunOnce (SK_GPU_InitSensorData ());

      dwLastBudgetPoll = SK::ControlPanel::current_time;

      SK_DXGI_SignalBudgetThread ();
    }

    if ( SK_GPU_GetVRAMUsed   (0) > 0 &&
         SK_GPU_GetVRAMBudget (0) > 0 &&
         config.render.dxgi.warn_if_vram_exceeds > 0.0f )
    {
      double percent_used = 100.0 *
        ( static_cast <double> (SK_GPU_GetVRAMUsed   (0)) /
          static_cast <double> (SK_GPU_GetVRAMBudget (0)) );

      if (percent_used > config.render.dxgi.warn_if_vram_exceeds)
      {
        if (! std::exchange (config.render.dxgi.warned_low_vram, true))
        {
          std::wstring used     =
            SK_File_SizeToStringF (SK_GPU_GetVRAMUsed   (0), 0, 2).data (),
                       capacity =
            SK_File_SizeToStringF (SK_GPU_GetVRAMBudget (0), 0, 2).data (),
                       quota    =
            SK_File_SizeToStringF (
                (uint64_t)(0.01 * config.render.dxgi.warn_if_vram_exceeds
                                * SK_GPU_GetVRAMBudget (0)), 0, 2).data (),
                       overage  =
            SK_File_SizeToStringF (SK_GPU_GetVRAMUsed   (0) -
                (uint64_t)(0.01 * config.render.dxgi.warn_if_vram_exceeds
                                * SK_GPU_GetVRAMBudget (0)), 0, 2).data ();

          double percent_over   =
                 percent_used - config.render.dxgi.warn_if_vram_exceeds;

          SK_ImGui_CreateNotification (
            "VRAM.OverQuota", SK_ImGui_Toast::Warning,
            SK_FormatString ( "VRAM Used:\t%ls\r\n"
                              "VRAM Quota:\t%0.1f%% of Available; %ls"
                              "\r\n\r\n\t\t %ls "
                              "Over Budget by %0.1f%%\t (%ls)\r\n\r\n "
                              " Configure VRAM Quotas by Right-Clicking the"
                              " VRAM Gauge", used.c_str (),
                    config.render.dxgi.warn_if_vram_exceeds,
                                    quota.c_str (), L"*", percent_over,
                                  overage.c_str ()
                             ).c_str (), "Insufficient VRAM", 20000,
                                         SK_ImGui_Toast::UseDuration |
                                         SK_ImGui_Toast::ShowTitle   |
                                         SK_ImGui_Toast::ShowCaption |
                                         SK_ImGui_Toast::ShowNewest
          );
        }
      }
    }
  }


#pragma region Yuzu Window Management Hacks
  if (! game_window.active)
  {
    void
    ActivateWindow ( HWND hWnd,
                     bool active          = false,
                     HWND hWndDeactivated = 0 );

    HWND hWndForeground =
      SK_GetForegroundWindow ();

    //if (hWndForeground      == rb.windows.device.parent && rb.windows.device.parent != 0) {
    //   game_window.hWnd      = rb.windows.device.parent;
    if (hWndForeground      == rb.windows.device.hwnd && rb.windows.device.hwnd != 0 && game_window.hWnd != hWndForeground) {
       game_window.hWnd      = rb.windows.device.hwnd;
       game_window.changed   = true;
       ActivateWindow (game_window.hWnd, true);
    }
    else if (hWndForeground == rb.windows.focus.hwnd && rb.windows.focus.hwnd   != 0 && game_window.hWnd != hWndForeground) {
       game_window.hWnd      = rb.windows.focus.hwnd;
       game_window.changed   = true;
       ActivateWindow (game_window.hWnd, true);
    }
  }
#pragma endregion

  void SK_ImGui_DrawGraph_Latency (bool predraw);
       SK_ImGui_DrawGraph_Latency (true);

  // While in the size/move modal loop, continually evaluate
  //   window size/position and unrestrict the mouse.
  if (game_window.size_move)
  {
    SK_Window_RepositionIfNeeded ();
  }

  SK_GetCurrentRenderBackend ().in_present_call = false;

  return hr;
}

DLL_ROLE&
SKX_GetDLLRole (void)
{
  static DLL_ROLE dll_role (DLL_ROLE::INVALID);
  return          dll_role;
}

extern "C"
DLL_ROLE
__stdcall
SK_GetDLLRole (void)
{
  return SKX_GetDLLRole ();
}


void
__cdecl
SK_SetDLLRole (DLL_ROLE role)
{
  SKX_GetDLLRole () = role;
}


DWORD SK_GetParentProcessId (void) // By Napalm @ NetCore2K
{
  ULONG_PTR pbi [6] = { };
  ULONG     ulSize  =  0 ;

  LONG (WINAPI *NtQueryInformationProcess)(HANDLE ProcessHandle,      ULONG ProcessInformationClass,
                                           PVOID  ProcessInformation, ULONG ProcessInformationLength,
                                           PULONG ReturnLength);
  *(FARPROC *)&NtQueryInformationProcess = SK_GetProcAddress (L"NtDll",
              "NtQueryInformationProcess");

  if (NtQueryInformationProcess != nullptr)
  {
    if ( NtQueryInformationProcess (
           SK_GetCurrentProcess (), 0, &pbi,
                                sizeof (pbi), &ulSize ) >= 0 &&
                                               ulSize   == sizeof (pbi)
       )
    {
      return
        (DWORD)pbi [5];
    }
  }

  return
    (DWORD)-1;
}

// Timeout after 500 ms
void
SK_WaitForParentToExit (void)
{
  SK_AutoHandle hProc (
    OpenProcess ( PROCESS_QUERY_INFORMATION, FALSE,
                    SK_GetParentProcessId () )
                );

  if (reinterpret_cast <intptr_t> (hProc.m_h) > 0)
  {
    for (int tries = 0 ; tries < 10 ; ++tries)
    {
      DWORD                           dwExitCode  = STILL_ACTIVE;
      GetExitCodeProcess (hProc.m_h, &dwExitCode);
      if (                            dwExitCode != STILL_ACTIVE)
      {
        break;
      }

      SK_SleepEx (50UL, FALSE);
    }
  }
}

// Stupid solution, but very effective way of re-launching a game as admin without
//   the Steam client throwing a temper tantrum.
void
CALLBACK
RunDLL_ElevateMe ( HWND  hwnd,        HINSTANCE hInst,
                   LPSTR lpszCmdLine, int       nCmdShow )
{
  UNREFERENCED_PARAMETER (hInst);

  if (lpszCmdLine == nullptr)
    return;

  hwnd     = HWND_DESKTOP;
  nCmdShow = SW_SHOWNORMAL;

  SK_WaitForParentToExit ();

  // Strip enclosing quotes
  if (*lpszCmdLine == '"')
     ++lpszCmdLine;

  std::string work_dir;
  char*      szArgs  = nullptr;
  char* szExecutable =
    StrStrIA (lpszCmdLine, ".exe");

  if (szExecutable != nullptr)
  {
    if (     *(szExecutable + 4) != '\0')
      szArgs = szExecutable + 5;

    *(szExecutable + 4) = '\0';

    work_dir =
      std::filesystem::path (lpszCmdLine).parent_path ().string ();
  }

  szExecutable = lpszCmdLine;

  if (szArgs == nullptr || (! StrStrIA (szArgs, "-epicapp=")))
  {
    SK_ShellExecuteA ( hwnd, "runas", szExecutable,
                         szArgs, work_dir.empty () ?
                                           nullptr :
                                 work_dir.c_str (), nCmdShow );
  }

  // Epic Game, needs launcher assistance to restart
  else
  {
    char *szAppId =
      StrStrIA (szArgs, "-epicapp=") + 9;
    char *szSpace =
      StrStrIA (szAppId, " ");

    if (szSpace != nullptr)
       *szSpace = '\0';

    const std::string epic_launch (
      SK_FormatString ("com.epicgames.launcher://apps/%s?action=launch&silent=true", szAppId)
    );

    SK_ShellExecuteA ( hwnd, "runas", epic_launch.c_str (), nullptr, nullptr, nCmdShow );
  }
}

void
CALLBACK
RunDLL_RestartGame ( HWND  hwnd,        HINSTANCE hInst,
                     LPSTR lpszCmdLine, int       nCmdShow )
{
  if (! lpszCmdLine)
    return;

  UNREFERENCED_PARAMETER (hInst);

  hwnd     = HWND_DESKTOP;
  nCmdShow = SW_SHOWNORMAL;

  SK_WaitForParentToExit ();

  // Strip enclosing quotes
  if (*lpszCmdLine == '"')
     ++lpszCmdLine;

  std::string work_dir;
  char*      szArgs  = nullptr;
  char* szExecutable =
    StrStrIA (lpszCmdLine, ".exe");

  if (szExecutable != nullptr)
  {
    if (     *(szExecutable + 4) != '\0')
      szArgs = szExecutable + 5;

    *(szExecutable + 4) = '\0';

    work_dir =
      std::filesystem::path (lpszCmdLine).parent_path ().string ();
  }

  szExecutable = lpszCmdLine;

  if (szArgs == nullptr || (! StrStrIA (szArgs, "-epicapp=")))
  {
    SK_ShellExecuteA ( hwnd, "open", szExecutable,
                         szArgs, work_dir.empty () ?
                                           nullptr :
                                 work_dir.c_str (), nCmdShow );
  }

  // Epic Game, needs launcher assistance to restart
  else
  {
    char *szAppId =
      StrStrIA (szArgs, "-epicapp=") + 9;
    char *szSpace =
      StrStrIA (szAppId, " ");

    if (szSpace != nullptr)
       *szSpace = '\0';

    const std::string epic_launch (
      SK_FormatString ("com.epicgames.launcher://apps/%s?action=launch&silent=true", szAppId)
    );

    SK_ShellExecuteA ( hwnd, "open", epic_launch.c_str (), nullptr, nullptr, nCmdShow );
  }
}


SK_LazyGlobal <SK_ImGui_WidgetRegistry> SK_ImGui_Widgets;

HANDLE
WINAPI
SK_CreateEvent (
  _In_opt_ LPSECURITY_ATTRIBUTES lpEventAttributes,
  _In_     BOOL                  bManualReset,
  _In_     BOOL                  bInitialState,
  _In_opt_ LPCWSTR               lpName )
{
  return
    CreateEventW (
      lpEventAttributes, bManualReset, bInitialState, lpName
    );
}

auto SK_Config_CreateSymLinks = [](void) ->
void
{
  if (config.system.central_repository)
  {
    // Create Symlink for end-user's convenience
    if ( GetFileAttributes ( ( std::wstring (SK_GetHostPath ()) +
                               std::wstring (LR"(\SpecialK\)")
                             ).c_str ()
                           ) == INVALID_FILE_ATTRIBUTES )
    {
      std::wstring link (SK_GetHostPath ());
                   link += LR"(\SpecialK\)";

      CreateSymbolicLink (
        link.c_str         (),
          SK_GetConfigPath (),
            SYMBOLIC_LINK_FLAG_DIRECTORY
      );
    }

    if ( GetFileAttributes ( ( std::wstring (SK_GetConfigPath ()) +
                               std::wstring (LR"(Game\)") ).c_str ()
                           ) == INVALID_FILE_ATTRIBUTES )
    {
      std::wstring link (SK_GetConfigPath ());
                   link += LR"(Game\)";

      CreateSymbolicLink (
        link.c_str         (),
          SK_GetHostPath   (),
            SYMBOLIC_LINK_FLAG_DIRECTORY
      );
    }
  }
};

bool
SK_API_IsDXGIBased (SK_RenderAPI api)
{
  switch (api)
  {
    case SK_RenderAPI::D3D10:
    case SK_RenderAPI::D3D11:
    case SK_RenderAPI::D3D12:
    case SK_RenderAPI::D3D8On11:
    case SK_RenderAPI::D3D8On12:
    case SK_RenderAPI::DDrawOn11:
    case SK_RenderAPI::DDrawOn12:
    case SK_RenderAPI::GlideOn11:
    case SK_RenderAPI::GlideOn12:
    case SK_RenderAPI::GLOn11:
      return true;
    default:
      return false;
  }
}

bool
SK_API_IsLayeredOnD3D12 (SK_RenderAPI api)
{
  switch (api)
  {
    case SK_RenderAPI::D3D12:
    case SK_RenderAPI::D3D8On12:
    case SK_RenderAPI::DDrawOn12:
    case SK_RenderAPI::GlideOn12:
      return true;
    default:
      return
        ( static_cast <UINT> (api) &
          static_cast <UINT> (SK_RenderAPI::D3D12) )
       == static_cast <UINT> (SK_RenderAPI::D3D12);
  }
}

bool
SK_API_IsLayeredOnD3D11 (SK_RenderAPI api)
{
  switch (api)
  {
    case SK_RenderAPI::D3D11:
    case SK_RenderAPI::D3D8On11:
    case SK_RenderAPI::DDrawOn11:
    case SK_RenderAPI::GlideOn11:
    case SK_RenderAPI::GLOn11:
      return true;
    default:
      return
        ( static_cast <UINT> (api) &
          static_cast <UINT> (SK_RenderAPI::D3D11) )
       == static_cast <UINT> (SK_RenderAPI::D3D11);
  }
}

bool
SK_API_IsLayeredOnD3D10 (SK_RenderAPI api)
{
  switch (api)
  {
    case SK_RenderAPI::D3D10:
      return true;
    default:
      return
        ( static_cast <UINT> (api) &
          static_cast <UINT> (SK_RenderAPI::D3D10) )
       == static_cast <UINT> (SK_RenderAPI::D3D10);
  }
}

bool
SK_API_IsDirect3D9 (SK_RenderAPI api)
{
  switch (api)
  {
    case SK_RenderAPI::D3D9:
    case SK_RenderAPI::D3D9Ex:
      return true;
    default:
      return false;
  }
}

bool
SK_API_IsGDIBased (SK_RenderAPI api)
{
  switch (api)
  {
    case SK_RenderAPI::OpenGL:
      return true;
    default:
      return false;
  }
}

bool
SK_API_IsPlugInBased (SK_RenderAPI api)
{
  switch (api)
  {
    case SK_RenderAPI::DDraw:
    case SK_RenderAPI::DDrawOn11:
    case SK_RenderAPI::DDrawOn12:
    case SK_RenderAPI::D3D8:
    case SK_RenderAPI::D3D8On11:
    case SK_RenderAPI::D3D8On12:
    case SK_RenderAPI::Glide:
    case SK_RenderAPI::GlideOn11:
    case SK_RenderAPI::GlideOn12:
      return true;
    default:
      return false;
  }
}

bool
SK_GetStoreOverlayState (bool bReal)
{
  static std::atomic_bool     s_LastState = false;
  static std::atomic<ULONG64> s_LastFrame = 0;

  if (s_LastFrame.load () < SK_GetFramesDrawn ())
  {
    bool ret =
      SK::SteamAPI::GetOverlayState (bReal) ||
      SK::EOS::     GetOverlayState (bReal) ||
      SK_ReShadeAddOn_IsOverlayActive ();

    s_LastState.store (ret);
    s_LastFrame.store (SK_GetFramesDrawn ());

    return ret;
  }

  return s_LastState.load ();
}

SK_LazyGlobal <iSK_Logger> dll_log;
SK_LazyGlobal <iSK_Logger> crash_log;
SK_LazyGlobal <iSK_Logger> budget_log;
SK_LazyGlobal <iSK_Logger> game_debug;
SK_LazyGlobal <iSK_Logger> tex_log;
SK_LazyGlobal <iSK_Logger> steam_log;
SK_LazyGlobal <iSK_Logger> epic_log;