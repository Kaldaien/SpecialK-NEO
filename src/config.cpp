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

#ifndef __SK_SUBSYSTEM__
#define __SK_SUBSYSTEM__ L"  Config  "
#endif

#include <SpecialK/nvapi.h>
#include <SpecialK/render/d3d11/d3d11_core.h>

#include <SpecialK/storefront/epic.h>
#include <filesystem>

#ifndef __cpp_lib_format
#define __cpp_lib_format
#endif

#include <format>

#define D3D11_RAISE_FLAG_DRIVER_INTERNAL_ERROR 1

iSK_INI* dll_ini      = nullptr;
iSK_INI* osd_ini      = nullptr;
iSK_INI* input_ini    = nullptr;
iSK_INI* platform_ini = nullptr;
iSK_INI* macro_ini    = nullptr;
iSK_INI* notify_ini   = nullptr;

SK_LazyGlobal <SK_AppCache_Manager> app_cache_mgr;

SK_LazyGlobal <std::unordered_map <wstring_hash, BYTE>> humanKeyNameToVirtKeyCode;
SK_LazyGlobal <std::unordered_map <BYTE, wchar_t [32]>> virtKeyCodeToHumanKeyName;
SK_LazyGlobal <std::unordered_map <BYTE, wchar_t [32]>> virtKeyCodeToFullyLocalizedKeyName;

UINT SK_RecursiveMove ( const wchar_t* wszOrigDir,
                        const wchar_t* wszDestDir,
                              bool     replace );

bool
__stdcall
SK_IsCurrentGame (SK_GAME_ID game_id)
{
  return
    SK_GetCurrentGameID () == game_id;
}

SK_GAME_ID
__stdcall
SK_GetCurrentGameID (void)
{
  static SK_GAME_ID current_game =
         SK_GAME_ID::UNKNOWN_GAME;

  static bool first_check = true;
  if         (first_check)
  {
    struct constexpr_game_s
    {
      size_t     hash;
      SK_GAME_ID id;

      constexpr constexpr_game_s ( const wchar_t* name,
                                       SK_GAME_ID game_id )
      {
        hash = hash_lower (name);
        id   = game_id;
      }

      static constexpr
      SK_GAME_ID get ( const std::initializer_list <constexpr_game_s>& games,
                                                              size_t _hashval )
      {
        for (const auto& game : games)
        {
          if (game.hash == _hashval)
            return game.id;
        }

        return
          SK_GAME_ID::UNKNOWN_GAME;
      }

      using list_type =
        std::initializer_list <constexpr_game_s>;
    };

    static constexpr
      constexpr_game_s::list_type
        _games =
        {
          { L"Tyranny.exe",                            SK_GAME_ID::Tyranny                      },
          { L"TidesOfNumenera.exe",                    SK_GAME_ID::TidesOfNumenera              },
          { L"MassEffectAndromeda.exe",                SK_GAME_ID::MassEffect_Andromeda         },
          { L"MadMax.exe",                             SK_GAME_ID::MadMax                       },
          { L"Dreamfall Chapters.exe",                 SK_GAME_ID::Dreamfall_Chapters           },
          { L"TheWitness.exe",                         SK_GAME_ID::TheWitness                   },
          { L"Obduction-Win64-Shipping.exe",           SK_GAME_ID::Obduction                    },
          { L"witcher3.exe",                           SK_GAME_ID::TheWitcher3                  },
          { L"re7.exe",                                SK_GAME_ID::ResidentEvil7                },
          { L"DDDA.exe",                               SK_GAME_ID::DragonsDogma                 },
          { L"eqgame.exe",                             SK_GAME_ID::EverQuest                    },
          { L"GE2RB.exe",                              SK_GAME_ID::GodEater2RageBurst           },
          { L"ge3.exe",                                SK_GAME_ID::GodEater3                    },
          { L"WatchDogs2.exe",                         SK_GAME_ID::WatchDogs2                   },
          { L"NieRAutomata.exe",                       SK_GAME_ID::NieRAutomata                 },
          { L"Warframe.x64.exe",                       SK_GAME_ID::Warframe_x64                 },
          { L"LEGOLCUR_DX11.exe",                      SK_GAME_ID::LEGOCityUndercover           },
          { L"Sacred.exe",                             SK_GAME_ID::Sacred                       },
          { L"sacred2.exe",                            SK_GAME_ID::Sacred2                      },
          { L"FF9.exe",                                SK_GAME_ID::FinalFantasy9                },
          { L"FinchGame.exe",                          SK_GAME_ID::EdithFinch                   },
          { L"FFX.exe",                                SK_GAME_ID::FinalFantasyX_X2             },
          { L"FFX-2.exe",                              SK_GAME_ID::FinalFantasyX_X2             },
          { L"FFX&X-2_Will.exe",                       SK_GAME_ID::FinalFantasyX_X2             },
          { L"ffxiiiimg.exe",                          SK_GAME_ID::FinalFantasy13               },
          { L"FFXiiiLauncher.exe",                     SK_GAME_ID::Launcher                     },
          { L"DP.exe",                                 SK_GAME_ID::DeadlyPremonition            },
          { L"GG2Game.exe",                            SK_GAME_ID::GalGun_Double_Peace          },
          { L"Ys7.exe",                                SK_GAME_ID::YS_Seven                     },
          { L"TOS.exe",                                SK_GAME_ID::Tales_of_Symphonia           },
          { L"Tales of Zestiria.exe",                  SK_GAME_ID::Tales_of_Zestiria            },
          { L"TOV_DE.exe",                             SK_GAME_ID::Tales_of_Vesperia            },
          { L"Tales of Arise.exe",                     SK_GAME_ID::Tales_of_Arise               },
          { L"Tales of Graces f Remastered.exe",       SK_GAME_ID::Tales_of_Graces              },
          { L"Life is Strange - Before the Storm.exe", SK_GAME_ID::LifeIsStrange_BeforeTheStorm },
          { L"EoCApp.exe",                             SK_GAME_ID::DivinityOriginalSin          },
          { L"Hob.exe",                                SK_GAME_ID::Hob                          },
          { L"DukeForever.exe",                        SK_GAME_ID::DukeNukemForever             },
          { L"BLUE_REFLECTION.exe",                    SK_GAME_ID::BlueReflection               },
          { L"Zero Escape.exe",                        SK_GAME_ID::ZeroEscape                   },
          { L"hackGU.exe",                             SK_GAME_ID::DotHackGU                    },
          { L"WOFF.exe",                               SK_GAME_ID::WorldOfFinalFantasy          },
          { L"StarOceanTheLastHope.exe",               SK_GAME_ID::StarOcean4                   },
          { L"SO2R.exe",                               SK_GAME_ID::StarOcean2R                  },
          { L"LEGOMARVEL2_DX11.exe",                   SK_GAME_ID::LEGOMarvelSuperheroes2       },
          { L"okami.exe",                              SK_GAME_ID::Okami                        },
          { L"DuckTales.exe",                          SK_GAME_ID::DuckTalesRemastered          },
          { L"mafia3.exe",                             SK_GAME_ID::Mafia3                       },
          { L"Owlboy.exe",                             SK_GAME_ID::Owlboy                       },
          { L"DarkSoulsIII.exe",                       SK_GAME_ID::DarkSouls3                   },
          { L"Fallout4.exe",                           SK_GAME_ID::Fallout4                     },
          { L"dis1_st.exe",                            SK_GAME_ID::DisgaeaPC                    },
          { L"Secret_of_Mana.exe",                     SK_GAME_ID::SecretOfMana                 },
          { L"DBFighterZ.exe",                         SK_GAME_ID::DragonBallFighterZ           },
          { L"Nino2.exe",                              SK_GAME_ID::NiNoKuni2                    },
          { L"FarCry4.exe",                            SK_GAME_ID::FarCry4                      },
          { L"FarCry5.exe",                            SK_GAME_ID::FarCry5                      },
          { L"Chrono Trigger.exe",                     SK_GAME_ID::ChronoTrigger                },
          { L"ys8.exe",                                SK_GAME_ID::Ys_Eight                     },
          { L"PillarsOfEternityII.exe",                SK_GAME_ID::PillarsOfEternity2           },
          { L"Yakuza0.exe",                            SK_GAME_ID::Yakuza0                      },
          { L"YakuzaKiwami.exe",                       SK_GAME_ID::YakuzaKiwami                 },
          { L"YakuzaKiwami2.exe",                      SK_GAME_ID::YakuzaKiwami2                },
          { L"LikeADragonGaiden.exe",                  SK_GAME_ID::YakuzaLikeADragonGaiden      },
          { L"likeadragon8.exe",                       SK_GAME_ID::YakuzaInfiniteWealth         },
          { L"MonsterHunterWorld.exe",                 SK_GAME_ID::MonsterHunterWorld           },
          { L"MonsterHunterRise.exe",                  SK_GAME_ID::MonsterHunterRise            },
          { L"Shenmue.exe",                            SK_GAME_ID::Shenmue                      },
          { L"Shenmue2.exe",                           SK_GAME_ID::Shenmue                      },
          { L"SteamLauncher.exe",                      SK_GAME_ID::Launcher                     },
          { L"DRAGON QUEST XI.exe",                    SK_GAME_ID::DragonQuestXI                },
          { L"ACOdyssey.exe",                          SK_GAME_ID::AssassinsCreed_Odyssey       },
          { L"ACOrigins.exe",                          SK_GAME_ID::AssassinsCreed_Odyssey       },
          { L"JustCause3.exe",                         SK_GAME_ID::JustCause3                   },
          { L"ed8.exe",                                SK_GAME_ID::TrailsOfColdSteel            },
          { L"sekiro.exe",                             SK_GAME_ID::Sekiro                       },
          { L"Octopath_Traveler-Win64-Shipping.exe",   SK_GAME_ID::OctopathTraveler             },
          { L"SonicMania.exe",                         SK_GAME_ID::SonicMania                   },
          { L"P4G.exe",                                SK_GAME_ID::Persona4                     },
          { L"P5R.exe",                                SK_GAME_ID::Persona5                     },
          { L"HorizonZeroDawn.exe",                    SK_GAME_ID::HorizonZeroDawn              },
          { L"HorizonForbiddenWest.exe",               SK_GAME_ID::HorizonForbiddenWest         },
          { L"bg3.exe",                                SK_GAME_ID::BaldursGate3                 },
          { L"bg3_dx11.exe",                           SK_GAME_ID::BaldursGate3                 },
          { L"Atelier_Ryza_2.exe",                     SK_GAME_ID::AtelierRyza2                 },
          { L"nioh2.exe",                              SK_GAME_ID::Nioh2                        },
          { L"HuniePop 2 - Double Date.exe",           SK_GAME_ID::HuniePop2                    },
          { L"NieR Replicant ver.1.22474487139.exe",   SK_GAME_ID::NieR_Sqrt_1_5                },
          { L"re8.exe",                                SK_GAME_ID::ResidentEvil8                },
          { L"Legend of Mana.exe",                     SK_GAME_ID::LegendOfMana                 },
          { L"FarCry6.exe",                            SK_GAME_ID::FarCry6                      },
          { L"Ryujinx.exe",                            SK_GAME_ID::Ryujinx                      },
          { L"yuzu.exe",                               SK_GAME_ID::yuzu                         },
          { L"cemu.exe",                               SK_GAME_ID::cemu                         },
          { L"rpcs3.exe",                              SK_GAME_ID::RPCS3                        },
          { L"ForzaHorizon5.exe",                      SK_GAME_ID::ForzaHorizon5                },
          { L"forza_gaming.desktop.x64_release_final"
            L".exe",                                   SK_GAME_ID::ForzaMotorsport              },
          { L"HaloInfinite.exe",                       SK_GAME_ID::HaloInfinite                 },
          { L"start_protected_game.exe",               SK_GAME_ID::EasyAntiCheat                },
          { L"eldenring.exe",                          SK_GAME_ID::EldenRing                    },
          { L"wonderlands.exe",                        SK_GAME_ID::TinyTinasWonderlands         },
          { L"ELEX2.exe",                              SK_GAME_ID::Elex2                        },
          { L"CHRONOCROSS.exe",                        SK_GAME_ID::ChronoCross                  },
          { L"CHRONOCROSS_LAUNCHER.exe",               SK_GAME_ID::Launcher                     },
          { L"DivaMegaMix.exe",                        SK_GAME_ID::HatsuneMikuDIVAMegaMix       },
        //{ L"smt3hd.exe",                             SK_GAME_ID::ShinMegamiTensei3            },
          { L"TheQuarry-Win64-Shipping.exe",           SK_GAME_ID::TheQuarry                    },
          { L"GenshinImpact.exe",                      SK_GAME_ID::GenshinImpact                },
          { L"PathOfExileSteam.exe",                   SK_GAME_ID::PathOfExile                  },
          { L"PathOfExile.exe",                        SK_GAME_ID::PathOfExile                  },
          { L"Disgaea5.exe",                           SK_GAME_ID::Disgaea5                     },
          { L"SOUL HACKERS2.exe",                      SK_GAME_ID::SoulHackers2                 },
          { L"MMBN_LC1.exe",                           SK_GAME_ID::MegaManBattleNetwork         },
          { L"MMBN_LC2.exe",                           SK_GAME_ID::MegaManBattleNetwork         },
          { L"StarRail.exe",                           SK_GAME_ID::HonkaiStarRail               },
          { L"ffxiv_dx11.exe",                         SK_GAME_ID::FinalFantasyXIV              },
          { L"NMS.exe",                                SK_GAME_ID::NoMansSky                    },
          { L"Diablo IV.exe",                          SK_GAME_ID::DiabloIV                     },
          { L"CoDSP.exe",                              SK_GAME_ID::CallOfDuty                   },
          { L"CoDMP.exe",                              SK_GAME_ID::CallOfDuty                   },
          { L"RiftApart.exe",                          SK_GAME_ID::RatchetAndClank_RiftApart    },
          { L"Sam2017.exe",                            SK_GAME_ID::SeriousSamFusion2017         },
          { L"Sam2017_Unrestricted.exe",               SK_GAME_ID::SeriousSamFusion2017         },
          { L"Starfield.exe",                          SK_GAME_ID::Starfield                    },
          { L"Oblivion.exe",                           SK_GAME_ID::Oblivion                     },
          { L"FalloutNV.exe",                          SK_GAME_ID::FalloutNewVegas              },
          { L"Fallout3.exe",                           SK_GAME_ID::Fallout3                     },
          { L"GECK.exe",                               SK_GAME_ID::GECK                         },
          { L"CreationKit.exe",                        SK_GAME_ID::CreationKit                  },
          { L"TESConstructionSet.exe",                 SK_GAME_ID::ConstructionSet              },
          { L"LOTF2.exe",                              SK_GAME_ID::EasyAntiCheat                },
          { L"LOTF2-Win64-Shipping.exe",               SK_GAME_ID::LordsOfTheFallen2            },
          { L"AlanWake2.exe",                          SK_GAME_ID::AlanWake2                    },
          { L"Cyberpunk2077.exe",                      SK_GAME_ID::Cyberpunk2077                },
          { L"CrashReport.exe",                        SK_GAME_ID::CrashReport                  },
          { L"StreetFighter6.exe",                     SK_GAME_ID::StreetFighter6               },
          { L"Stardew Valley.exe",                     SK_GAME_ID::StardewValley                },
          { L"DOOMEternalx64vk.exe",                   SK_GAME_ID::DOOMEternal                  },
          { L"anuket_x64.exe",                         SK_GAME_ID::Blood                        },
          { L"BatmanAK.exe",                           SK_GAME_ID::BatmanArkhamKnight           },
          { L"Noita.exe",                              SK_GAME_ID::Noita                        },
          { L"P3R.exe",                                SK_GAME_ID::Persona3                     },
          { L"granblue_fantasy_relink.exe",            SK_GAME_ID::GranblueFantasyRelink        },
          { L"wrath-sdl.exe",                          SK_GAME_ID::WrathAeonOfRuin              },
          { L"dd2.exe",                                SK_GAME_ID::DragonsDogma                 },
          { L"Harold Halibut.exe",                     SK_GAME_ID::HaroldHalibut                },
          { L"KingdomCome.exe",                        SK_GAME_ID::KingdomComeDeliverance       },
          { L"GoW.exe",                                SK_GAME_ID::GodOfWar                     },
          { L"Talos2-Win64-Shipping.exe",              SK_GAME_ID::TalosPrinciple2              },
          { L"CrashBandicootNSaneTrilogy.exe",         SK_GAME_ID::CrashBandicootNSaneTrilogy   },
          { L"Outlaws.exe",                            SK_GAME_ID::StarWarsOutlaws              },
          { L"Outlaws_Plus.exe",                       SK_GAME_ID::StarWarsOutlaws              },
          { L"shadPS4.exe",                            SK_GAME_ID::ShadPS4                      },
          { L"GoWR.exe",                               SK_GAME_ID::GodOfWarRagnarok             },
          { L"METAPHOR.exe",                           SK_GAME_ID::Metaphor                     },
          { L"SONIC_X_SHADOW_GENERATIONS.exe",         SK_GAME_ID::SonicXShadowGenerations      },
          { L"SONIC_GENERATIONS.exe",                  SK_GAME_ID::SonicGenerations             },
          { L"BS1R.exe",                               SK_GAME_ID::BrokenSword                  },
          { L"ysx.exe",                                SK_GAME_ID::YsX                          },
          { L"MonsterHunterWilds.exe",                 SK_GAME_ID::MonsterHunterWilds           },
          { L"MonsterHunterWildsBeta.exe",             SK_GAME_ID::MonsterHunterWilds           },
          { L"Dragon Age The Veilguard.exe",           SK_GAME_ID::DragonAgeTheVeilguard        },
          { L"tomb123.exe",                            SK_GAME_ID::TombRaider123Remastered      },
          { L"Stalker2-WinGDK-Shipping.exe",           SK_GAME_ID::Stalker2                     },
          { L"Stalker2-Win64-Shipping.exe",            SK_GAME_ID::Stalker2                     },
          { L"vlc.exe",                                SK_GAME_ID::vlc                          },
          { L"ZenlessZoneZero.exe",                    SK_GAME_ID::ZenlessZoneZero              },
        };

    first_check  = false;
    current_game =
      constexpr_game_s::get (
               _games, hash_lower (SK_GetHostApp ())
                            );

    // For games that can't be matched using a single executable filename
    if (current_game == SK_GAME_ID::UNKNOWN_GAME)
    {
#ifdef _M_AMD64
      auto app_id =
        SK_Steam_GetAppID_NoAPI ();

      if (app_id == 1382330)
        current_game = SK_GAME_ID::Persona5Strikers;
         

      if ( StrStrIW ( SK_GetHostApp (), L"ffxv" ) ==
                      SK_GetHostApp () )
      {
        if ( StrStrIW ( SK_GetHostApp (), L"ffxv_" ) )
        {
          current_game = SK_GAME_ID::FinalFantasyXV;
        }

        else if ( StrStrIW ( SK_GetHostApp (), L"ffxvi" ) )
        {
          // Streamline shenanigans
          config.compatibility.init_sync_for_streamline = true;

          // Game has native PlayStation support
          config.input.gamepad.xinput.emulate = false;

          config.render.dxgi.fake_fullscreen_mode       = true;
          config.render.dstorage.submit_threads         = 2;
          config.system.auto_load_asi_files             = true;

          current_game = SK_GAME_ID::FinalFantasyXVI;
        }
      }

      else if ( StrStrIW ( SK_GetHostApp (), L"ff7remake" ) )
      {
        current_game = SK_GAME_ID::FinalFantasy7Remake;
      }

      else if ( StrStrW ( SK_GetHostApp (), L"GoW" ) )
      {
        current_game = SK_GAME_ID::GodOfWar;
      }

      // Basically, _every single Yakuza game ever_ releases more references than it acquires...
      //   so just assume this is never going to get better and anything with Yakuza in the name
      //     needs help counting.
      else if ( StrStrIW ( SK_GetHostApp (), L"Yakuza" ) )
      {
        current_game = SK_GAME_ID::YakuzaUnderflow;
      }

      else if ( StrStrIW ( SK_GetHostApp  (), L"game"           ) )
      {    if ( StrStrIW ( SK_GetHostPath (), L"GalGun Returns" ) )
        {
          current_game = SK_GAME_ID::GalGunReturns;
        }

        else if ( StrStrIW ( SK_GetFullyQualifiedApp (),
                             L"P5S" ) )
        {
          current_game = SK_GAME_ID::Persona5Strikers;
          config.apis.d3d9.hook                    = false;
          config.apis.d3d9ex.hook                  = false;
          config.input.keyboard.override_alt_f4    = true;
          config.input.keyboard.catch_alt_f4       = true;
          config.window.background_render          = true;
        }

        else
        {
          // Monster Hunter Stories 2
          if (app_id == 1277400 ||
              app_id == 1412570)
          {
            current_game = SK_GAME_ID::MonsterHunterStories2;

            config.apis.d3d9.hook                 = false;
            config.apis.d3d9ex.hook               = false;

            config.input.keyboard.catch_alt_f4    = true;
            config.input.keyboard.override_alt_f4 = true;
          }
        }
      }
      else
#endif
      if (StrStrIW ( SK_GetHostApp (), L"Launcher" ))
      {
        current_game =
          SK_GAME_ID::Launcher;

        if (! config.compatibility.using_wine)
        {
          if (config.system.log_level == 0)
            config.system.silent = true;
        }
      }

      else
      {
        if ( const wchar_t *wszFinalFantasy7 = StrStrIW (SK_GetHostApp (), L"FF7_");
                            wszFinalFantasy7 != nullptr )
        {
          wchar_t              region [2] = { };
          auto matches = _snwscanf (wszFinalFantasy7,
                            wcslen (wszFinalFantasy7), L"FF7_%wc%wc.exe)",
                              &region [0],
                              &region [1]);

          if ( matches == 2 && region [0] != (region [0] + region [1]) )
          {
            current_game =
              SK_GAME_ID::FinalFantasy7;
          }
        }
      }
    }

    else
    {
      // CAPCOM games crash at start if we don't kill this
      if (current_game == SK_GAME_ID::CrashReport)
      {
        if (StrStrIW (SK_VerifyTrust_GetCodeSignature (SK_GetHostApp ()).subject.c_str (), L"CAPCOM"))
        {
          if ( IDYES == SK_MessageBox (
                L"Game may crash at startup unless CrashReport.exe is disabled."
                L"\r\n\r\n\tDisable Crash Report?",
                  L"Crash Report Must Be Disabled",
                    MB_YESNO |
                    MB_ICONQUESTION
               )
             )
          {
            // Properly disable this crap
            MoveFileW        (L"CrashReport.exe", L"CrashReport_Disabled.exe");
            TerminateProcess (GetCurrentProcess (), 0x0);
          }
        }

        // Self-Identify as Launcher and hope for the best...
        current_game = SK_GAME_ID::Launcher;
      }

      if (current_game == SK_GAME_ID::EasyAntiCheat)
      {
        std::error_code                                          ec;
        if (! std::filesystem::exists (L"SpecialK.AllowEAC",     ec))
        {
          if (std::filesystem::exists (L"eldenring.exe",         ec))
          {
            SK_SEH_LaunchEldenRing ();
          }

          else if (std::filesystem::exists (L"armoredcore6.exe", ec))
          {
            SK_SEH_LaunchArmoredCoreVI ();
          }

          else if (std::filesystem::exists (LR"(LOTF2\Binaries\Win64\LOTF2-Win64-Shipping.exe)", ec))
          {
            SK_SEH_LaunchLordsOfTheFallen2 ();
          }
        }
      }
    }
  }

  return
    current_game;
}


auto LoadKeybind =
  [](SK_ConfigSerializedKeybind* binding) ->
    auto
    {
      if (binding != nullptr)
      {
        auto ret =
          binding->param;

        if (ret != nullptr)
        {
          if (! static_cast <sk::iParameter *> (ret)->load  ())
          {
            // If the key exists without a value, skip assigning the default
            if (! static_cast <sk::iParameter *> (ret)->empty ())
            {
              binding->parse ();
              ret->store     (binding->human_readable);
            }
          }

          binding->human_readable = ret->get_value ();
          binding->parse ();
        }

        return ret;
      }

      return
        (sk::ParameterStringW *)nullptr;
    };


struct {
  struct {
    sk::ParameterBool*    show                    = nullptr;
  } time;

  struct {
    sk::ParameterBool*    show                    = nullptr;
  } title;

  struct {
    sk::ParameterBool*    show                    = nullptr;
    sk::ParameterFloat*   interval                = nullptr;
  } io;

  struct {
    sk::ParameterBool*    show                    = nullptr;
    sk::ParameterBool*    frametime               = nullptr;
    sk::ParameterBool*    advanced                = nullptr;
    sk::ParameterBool*    compact                 = nullptr;
    sk::ParameterBool*    compact_vrr             = nullptr;
    sk::ParameterBool*    framenumber             = nullptr;
    sk::ParameterInt*     timing_method           = nullptr;
  } fps;

  struct {
    sk::ParameterBool*    show                    = nullptr;
  } memory;

  struct {
    sk::ParameterBool*    show                    = nullptr;
  } SLI;

  struct {
    sk::ParameterBool*    show                    = nullptr;
    sk::ParameterFloat*   interval                = nullptr;
    sk::ParameterBool*    simple                  = nullptr;
  } cpu;

  struct {
    sk::ParameterBool*    show                    = nullptr;
    sk::ParameterBool*    print_slowdown          = nullptr;
    sk::ParameterFloat*   interval                = nullptr;
  } gpu;

  struct {
    sk::ParameterBool*    show                    = nullptr;
    sk::ParameterFloat*   interval                = nullptr;
    sk::ParameterInt*     type                    = nullptr;
  } disk;

  struct {
    sk::ParameterBool*    show                    = nullptr;
    sk::ParameterFloat*   interval                = nullptr;
  } pagefile;

  struct {
    sk::ParameterBool*    show                    = nullptr;
    sk::ParameterBool*    show_output_res         = nullptr;
    sk::ParameterBool*    show_quality            = nullptr;
    sk::ParameterBool*    show_preset             = nullptr;
    sk::ParameterBool*    show_fg                 = nullptr;
  } dlss;
} monitoring;

struct {
  struct {
    sk::ParameterBool*    trace_reads             = nullptr;
    sk::ParameterStringW  ignore_reads            = {     };
    sk::ParameterBool*    trace_writes            = nullptr;
    sk::ParameterStringW  ignore_writes           = {     };
  } file;
} reverse_engineering;

struct {
  struct {
    sk::ParameterFloat*   duration                = nullptr;
  } version_banner;


  sk::ParameterBool*      show                    = nullptr;

  struct {
    sk::ParameterInt*     red                     = nullptr;
    sk::ParameterInt*     green                   = nullptr;
    sk::ParameterInt*     blue                    = nullptr;
  } text;

  struct {
    sk::ParameterFloat*   scale                   = nullptr;
    sk::ParameterInt*     pos_x                   = nullptr;
    sk::ParameterInt*     pos_y                   = nullptr;
  } viewport;

  struct {
    sk::ParameterBool*    remember                = nullptr;
  } state;
} osd;

struct {
  sk::ParameterFloat*     scale                   = nullptr;
  sk::ParameterBool*      show_eula               = nullptr;
  sk::ParameterBool*      show_playtime           = nullptr;
  sk::ParameterBool*      mac_style_menu          = nullptr;
  sk::ParameterBool*      show_gsync_status       = nullptr;
  sk::ParameterBool*      show_input_apis         = nullptr;
  sk::ParameterBool*      disable_alpha           = nullptr;
  sk::ParameterBool*      antialias_lines         = nullptr;
  sk::ParameterBool*      antialias_contours      = nullptr;
  sk::ParameterBool*      center_cursor_on_overlay= nullptr;
  sk::ParameterBool*      nav_moves_mouse         = nullptr;
} imgui;

struct {
  struct {
    sk::ParameterInt64*   appid                   = nullptr;
    sk::ParameterInt*     init_delay              = nullptr;
    sk::ParameterBool*    auto_pump               = nullptr;
    sk::ParameterBool*    block_stat_callback     = nullptr;
    sk::ParameterBool*    filter_stat_callbacks   = nullptr;
    sk::ParameterBool*    load_early              = nullptr;
    sk::ParameterBool*    early_overlay           = nullptr;
    sk::ParameterBool*    force_load              = nullptr;
    sk::ParameterBool*    auto_inject             = nullptr;
    sk::ParameterStringW* dll_path                = nullptr;
    sk::ParameterBool*    crapcom_mode            = nullptr;
  } system;

  struct {
    sk::ParameterInt*     online_status           = nullptr;
  } social;

  struct {
    sk::ParameterBool*    spoof_BLoggedOn         = nullptr;
  } drm;

  struct {
    sk::ParameterStringW* blacklist               = nullptr;
  } cloud;

  struct
  {
    sk::ParameterInt*     throttle                = nullptr;
  } callbacks;

  struct
  {
    sk::ParameterBool*    smart_capture           = nullptr;
  } screenshots;
} steam;

struct {
  struct
  {
    sk::ParameterBool*    warned_online           = nullptr;
  } system;
} eos;

struct
{
  struct {
    sk::ParameterStringW* sound_file              = nullptr;
    sk::ParameterBool*    play_sound              = nullptr;
    sk::ParameterBool*    take_screenshot         = nullptr;
    sk::ParameterBool*    fetch_friend_stats      = nullptr;

    struct {
    sk::ParameterBool*    show                    = nullptr;
    sk::ParameterBool*    show_title              = nullptr;
    sk::ParameterBool*    animate                 = nullptr;
    sk::ParameterStringW* origin                  = nullptr;
    sk::ParameterFloat*   inset                   = nullptr;
    sk::ParameterInt*     duration                = nullptr;
    } popup;
  } achievements;

  struct {
    sk::ParameterBool*    silent                  = nullptr;
  } log;

  struct {
    sk::ParameterFloat*   hdr_luminance           = nullptr;
    sk::ParameterBool*    no_draw                 = nullptr;
  } overlay;

  struct {
    sk::ParameterStringW* notify_corner           = nullptr;
    sk::ParameterBool*    reuse_overlay_pause     = nullptr;
  } system;
} platform;

struct {
  sk::ParameterBool*      include_osd_default     = nullptr;
  sk::ParameterBool*      keep_png_copy           = nullptr;
  sk::ParameterBool*      play_sound              = nullptr;
  sk::ParameterBool*      copy_to_clipboard       = nullptr;
  sk::ParameterBool*      allow_hdr_clipboard     = nullptr;
  sk::ParameterBool*      embed_nickname          = nullptr;
  sk::ParameterStringW*   override_path           = nullptr;
  sk::ParameterStringW*   filename_format         = nullptr;
  sk::ParameterInt*       compression_quality     = nullptr;
  sk::ParameterBool*      compatibility_mode      = nullptr;
  sk::ParameterInt*       clipboard_hdr_format    = nullptr;

  struct {
    sk::ParameterBool*    use_avif                = nullptr;
    sk::ParameterInt*     yuv_subsampling         = nullptr;
    sk::ParameterInt*     scrgb_bit_depth         = nullptr;
    sk::ParameterInt*     compression_speed       = nullptr;
    //bool                full_range              =  true;
    //int                 max_threads             =     3;
  } avif;

  struct {
    sk::ParameterBool*    store_hdr               = nullptr;
    sk::ParameterInt*     st2084_bits             = nullptr;
  } png;

  struct {
    sk::ParameterBool*    use_jxl                 = nullptr;
  } jxl;
} screenshots;

struct {
  struct
  {
    sk::ParameterFloat*   hdr_luminance           = nullptr;
  } overlay;
} uplay;

struct {
  struct
  {
    sk::ParameterFloat*   hdr_luminance           = nullptr;
  } overlay;
} discord;

struct {
  struct
  {
    sk::ParameterFloat*   hdr_luminance           = nullptr;
  } overlay;
} rtss;

struct {
  struct
  {
    sk::ParameterFloat*   hdr_luminance           = nullptr;
  } overlay;
  sk::ParameterBool*      draw_first              = nullptr;
} reshade_cfg;

struct {
  sk::ParameterBool*      per_monitor_aware       = nullptr;
  sk::ParameterBool*      per_monitor_all_threads = nullptr;
  sk::ParameterBool*      disable                 = nullptr;
} dpi;

struct {
  sk::ParameterBool*      minimize_latency        = nullptr;
} sound;

struct {
  struct {
    sk::ParameterBool*    override                = nullptr;
    sk::ParameterStringW* compatibility           = nullptr;
    sk::ParameterStringW* num_gpus                = nullptr;
    sk::ParameterStringW* mode                    = nullptr;
  } sli;

  struct {
    sk::ParameterBool*    disable                 = nullptr;
    sk::ParameterBool*    disable_hdr             = nullptr;
    sk::ParameterInt*     vulkan_bridge           = nullptr;
  } api;

  struct
  {
    sk::ParameterBool*    snuffed_ansel           = nullptr;
    sk::ParameterBool*    bypass_ansel            = nullptr;
    sk::ParameterBool*    streamline_compat       = nullptr;
  } bugs;

  struct
  {
    sk::ParameterBool*    enable                  = nullptr;
    sk::ParameterBool*    low_latency             = nullptr;
    sk::ParameterBool*    low_latency_boost       = nullptr;
    sk::ParameterBool*    marker_optimization     = nullptr;
    sk::ParameterInt*     engagement_policy       = nullptr;
    sk::ParameterBool*    override_native         = nullptr;
    sk::ParameterBool*    use_limiter             = nullptr;
    sk::ParameterBool*    combined_limiter        = nullptr;
    sk::ParameterBool*    disable_native          = nullptr;
    sk::ParameterBool*    show_detailed_widget    = nullptr;
  } reflex;

  struct
  {
    sk::ParameterBool*    force_dlaa              = nullptr;
    sk::ParameterInt*     use_sharpening          = nullptr;
    sk::ParameterFloat*   forced_sharpness        = nullptr;
    sk::ParameterBool*    auto_redirect_dll       = nullptr;
    sk::ParameterInt*     forced_preset           = nullptr;
    sk::ParameterInt*     forced_auto_exposure    = nullptr;
    sk::ParameterInt*     forced_alpha_upscale    = nullptr;
    sk::ParameterBool*    show_active_features    = nullptr;
    sk::ParameterFloat*   performance_scale       = nullptr;
    sk::ParameterFloat*   balanced_scale          = nullptr;
    sk::ParameterFloat*   quality_scale           = nullptr;
    sk::ParameterFloat*   ultra_performance_scale = nullptr;
    sk::ParameterFloat*   ultra_quality_scale     = nullptr;
    sk::ParameterFloat*   dynamic_resolution_min  = nullptr;
    sk::ParameterFloat*   dynamic_resolution_max  = nullptr;
    sk::ParameterInt*     override_appid          = nullptr;
    sk::ParameterInt*     extra_pixels            = nullptr;
    sk::ParameterBool*    disable_ota_updates     = nullptr;
    sk::ParameterBool*    allow_scrgb             = nullptr;
    sk::ParameterBool*    spoof_feature_support   = nullptr;
  } dlss;
} nvidia;

struct {
  struct {
    sk::ParameterBool*    disable                 = nullptr;
  } adl;
} amd;

struct {
  struct {
    sk::ParameterBool*    disable_perfdata        = nullptr;
  } d3dkmt;
} microsoft;

struct {
  sk::ParameterInt*       location                = nullptr;
  sk::ParameterBool*      silent                  = nullptr;
} notifications;

sk::ParameterFloat*       mem_reserve             = nullptr;
sk::ParameterBool*        debug_output            = nullptr;
sk::ParameterBool*        debug_wait              = nullptr;
sk::ParameterBool*        game_output             = nullptr;
sk::ParameterBool*        handle_crashes          = nullptr;
sk::ParameterBool*        silent_crash            = nullptr;
sk::ParameterBool*        crash_suppression       = nullptr;
sk::ParameterBool*        prefer_fahrenheit       = nullptr;
sk::ParameterInt*         log_level               = nullptr;
sk::ParameterBool*        trace_libraries         = nullptr;
sk::ParameterBool*        strict_compliance       = nullptr;
sk::ParameterBool*        silent                  = nullptr;
sk::ParameterFloat*       init_delay              = nullptr;
sk::ParameterBool*        return_to_skif          = nullptr;
sk::ParameterInt*         skif_autostop_behavior  = nullptr;
sk::ParameterBool*        auto_load_asi_files     = nullptr;
sk::ParameterStringW*     version                 = nullptr;
                       // Version at last boot

struct {
  struct {
    sk::ParameterStringW* target_fps              = nullptr;
    sk::ParameterStringW* target_fps_bg           = nullptr;
    sk::ParameterFloat*   last_refresh_rate       = nullptr;
    sk::ParameterStringW* last_monitor_path       = nullptr;
    sk::ParameterInt*     prerender_limit         = nullptr;
    sk::ParameterInt*     present_interval        = nullptr;
    sk::ParameterInt*     sync_interval_clamp     = nullptr;
    sk::ParameterInt*     tearing_mode            = nullptr;
    sk::ParameterInt*     buffer_count            = nullptr;
    sk::ParameterInt*     max_delta_time          = nullptr;
    sk::ParameterBool*    flip_discard            = nullptr;
    sk::ParameterBool*    flip_sequential         = nullptr;
    sk::ParameterBool*    disable_flip_model      = nullptr;
    sk::ParameterFloat*   refresh_rate            = nullptr;
    sk::ParameterStringW* rescan_ratio            = nullptr;
    sk::ParameterBool*    wait_for_vblank         = nullptr;
    sk::ParameterBool*    allow_dwm_tearing       = nullptr;
    sk::ParameterBool*    sleepless_window        = nullptr;
    sk::ParameterBool*    sleepless_render        = nullptr;
    sk::ParameterBool*    enable_mmcss            = nullptr;
    sk::ParameterInt*     override_cpu_count      = nullptr;
    sk::ParameterInt*     enforcement_policy      = nullptr;
    sk::ParameterBool*    drop_late_frames        = nullptr;
    sk::ParameterBool*    auto_low_latency        = nullptr;
    sk::ParameterBool*    auto_vrr_triggered      = nullptr;
    sk::ParameterBool*    auto_low_latency_ex     = nullptr;
    sk::ParameterBool*    auto_low_latency_optin  = nullptr;
    sk::ParameterBool*    auto_low_latency_reapply= nullptr;
    sk::ParameterBool*    enable_etw_tracing      = nullptr;
    sk::ParameterBool*    use_amd_mwaitx          = nullptr;

    struct
    {
      sk::ParameterInt*   render_ahead            = nullptr;
    } control;

    struct
    {
      sk::ParameterInt*     offset                = nullptr;
      sk::ParameterFloat*   resync                = nullptr;
      sk::ParameterInt*     error                 = nullptr;
      sk::ParameterFloat*   bias                  = nullptr;
      sk::ParameterBool*    auto_bias             = nullptr;
      sk::ParameterFloat*   max_auto_bias         = nullptr;
      sk::ParameterStringW* auto_bias_target      = nullptr;
    } latent_sync;
  } framerate;

  struct {
    sk::ParameterInt*     adapter_override        = nullptr;
    sk::ParameterStringW* max_res                 = nullptr;
    sk::ParameterStringW* min_res                 = nullptr;
    sk::ParameterFloat*   min_refresh             = nullptr;
    sk::ParameterFloat*   max_refresh             = nullptr;
    sk::ParameterInt*     swapchain_wait          = nullptr;
    sk::ParameterStringW* scaling_mode            = nullptr;
    sk::ParameterStringW* exception_mode          = nullptr;
    sk::ParameterStringW* scanline_order          = nullptr;
    sk::ParameterStringW* rotation                = nullptr;
    sk::ParameterBool*    test_present            = nullptr;
    sk::ParameterBool*    debug_layer             = nullptr;
    sk::ParameterBool*    enhanced_depth          = nullptr;
    sk::ParameterBool*    deferred_isolation      = nullptr;
    sk::ParameterInt*     msaa_samples            = nullptr;
    sk::ParameterBool*    skip_present_test       = nullptr;
    sk::ParameterInt*     srgb_behavior           = nullptr;
    sk::ParameterBool*    low_spec_mode           = nullptr;
    sk::ParameterBool*    hide_hdr_support        = nullptr;
    sk::ParameterInt*     hdr_metadata_override   = nullptr;
    sk::ParameterBool*    enable_factory_cache    = nullptr;
    sk::ParameterBool*    skip_redundant_modes    = nullptr;
    sk::ParameterBool*    temporary_dwm_hdr       = nullptr;
    sk::ParameterBool*    disable_virtual_vbi     = nullptr;
    sk::ParameterBool*    clear_buffers_after_flip= nullptr;
    sk::ParameterFloat*   warn_if_vram_exceeds    = nullptr;
    sk::ParameterBool*    allow_d3d12_footguns    = nullptr;
    sk::ParameterBool*    fake_fullscreen_mode    = nullptr;
    sk::ParameterFloat*   vram_budget_scale       = nullptr;
  } dxgi;

  struct {
    sk::ParameterBool*    disable_bypass_io       = nullptr;
    sk::ParameterBool*    disable_telemetry       = nullptr;
    sk::ParameterBool*    disable_gpu_decomp      = nullptr;
    sk::ParameterBool*    force_file_buffering    = nullptr;
    sk::ParameterInt*     submit_threads          = nullptr;
    sk::ParameterInt*     cpu_decomp_threads      = nullptr;
    sk::ParameterBool*    hook_dstorage           = nullptr;
  } dstorage;

  struct {
    sk::ParameterBool*    enable_debug            = nullptr;
    sk::ParameterBool*    break_on_error          = nullptr;
    sk::ParameterBool*    prefer_10bpc            = nullptr;
    sk::ParameterBool*    upgrade_zbuffer         = nullptr;
  } gl;

  struct {
    sk::ParameterBool*    force_d3d9ex            = nullptr;
    sk::ParameterBool*    impure                  = nullptr;
    sk::ParameterBool*    enable_texture_mods     = nullptr;
    sk::ParameterBool*    enable_flipex           = nullptr;
    sk::ParameterBool*    use_d3d9on12            = nullptr;
  } d3d9;

  struct {
    sk::ParameterInt*     max_anisotropy          = nullptr;
    sk::ParameterBool*    force_anisotropic       = nullptr;
    sk::ParameterFloat*   force_lod_bias          = nullptr;
  } d3d12;

  struct {
    sk::ParameterBool*    draw_in_vidcap          = nullptr;
    sk::ParameterFloat*   hdr_luminance           = nullptr;
  } osd;

  struct {
    sk::ParameterBool*    enable_32bpc            = nullptr;
    sk::ParameterBool*    remaster_8bpc_as_unorm  = nullptr;
    sk::ParameterBool*    remaster_subnative_unorm= nullptr;
    sk::ParameterInt*     last_used_colorspace    = nullptr;
  } hdr;
} render;

struct {
  sk::ParameterBool*
                    multimonitor_focus_is_focused = nullptr;
  sk::ParameterBool*      multimonitor_focus_mode = nullptr;
  sk::ParameterBool*      force_fullscreen        = nullptr;
  sk::ParameterBool*      force_windowed          = nullptr;
  sk::ParameterBool*      confirm_mode_changes    = nullptr;
  sk::ParameterBool*      save_monitor_prefs      = nullptr;
  sk::ParameterBool*      warn_no_mpo_planes      = nullptr;
  sk::ParameterBool*      save_resolution         = nullptr;
  sk::ParameterBool*      allow_refresh_change    = nullptr;
  sk::ParameterStringW*   override_resolution     = nullptr;
  sk::ParameterFloat*     override_refresh        = nullptr;
  sk::ParameterBool*      force_10bpc_sdr         = nullptr;
  sk::ParameterBool*      aspect_ratio_stretch    = nullptr;
} display;

struct {
  struct {
    sk::ParameterBool*    clamp_lod_bias          = nullptr;
  } d3d9;

  struct {
    sk::ParameterBool*    use_l3_hash             = nullptr;
    sk::ParameterBool*    precise_hash            = nullptr;
    sk::ParameterBool*    inject                  = nullptr;
    sk::ParameterBool*    injection_keeps_format  = nullptr;
    sk::ParameterBool*    gen_mips                = nullptr;
    sk::ParameterBool*    cache                   = nullptr;
  } d3d11;

  struct {
    sk::ParameterInt*     min_evict               = nullptr;
    sk::ParameterInt*     max_evict               = nullptr;
    sk::ParameterInt*     min_size                = nullptr;
    sk::ParameterInt*     max_size                = nullptr;
    sk::ParameterInt*     min_entries             = nullptr;
    sk::ParameterInt*     max_entries             = nullptr;
    sk::ParameterBool*    ignore_non_mipped       = nullptr;
    sk::ParameterBool*    allow_staging           = nullptr;
    sk::ParameterBool*    allow_unsafe_refs       = nullptr;
    sk::ParameterBool*    manage_residency        = nullptr;
  } cache;

    sk::ParameterStringW* res_root                = nullptr;
    sk::ParameterBool*    dump_on_load            = nullptr;
} texture;

struct {
  struct
  {
    sk::ParameterBool*    catch_alt_f4            = nullptr;
    sk::ParameterBool*    bypass_alt_f4           = nullptr;
    sk::ParameterInt*     disabled_to_game        = nullptr;
  } keyboard;

  struct
  {
    sk::ParameterInt*     disabled_to_game        = nullptr;
  } mouse;

  struct {
    sk::ParameterBool*    manage                  = nullptr;
    sk::ParameterBool*    keys_activate           = nullptr;
    sk::ParameterBool*    gamepad_deactivates     = nullptr;
    sk::ParameterFloat*   timeout                 = nullptr;
    sk::ParameterBool*    ui_capture              = nullptr;
    sk::ParameterBool*    hw_cursor               = nullptr;
    sk::ParameterBool*    block_invisible         = nullptr;
    sk::ParameterBool*    fix_synaptics           = nullptr;
  } cursor;

  struct {
    sk::ParameterBool*    disable_hid             = nullptr;
    sk::ParameterBool*    disable_winmm           = nullptr;
    sk::ParameterBool*    rehook_xinput           = nullptr;
    sk::ParameterBool*    haptic_ui               = nullptr;
    sk::ParameterBool*    disable_rumble          = nullptr;
    sk::ParameterBool*    hook_dinput8            = nullptr;
    sk::ParameterBool*    hook_dinput7            = nullptr;
    sk::ParameterBool*    hook_hid                = nullptr;
    sk::ParameterBool*    hook_xinput             = nullptr;
    sk::ParameterBool*    hook_scepad             = nullptr;
    sk::ParameterBool*    hook_raw_input          = nullptr;
    sk::ParameterBool*    hook_game_input         = nullptr;
    sk::ParameterBool*    hook_windows_gaming     = nullptr;
    sk::ParameterBool*    hook_winmm              = nullptr;
    sk::ParameterBool*    allow_steam_winmm       = nullptr;

    struct {
      sk::ParameterInt*   ui_slot                 = nullptr;
      sk::ParameterInt*   placeholders            = nullptr;
      sk::
        ParameterStringW* assignment              = nullptr;
      sk::
        ParameterStringW* disable_slots           = nullptr;
      sk::ParameterBool*  hook_setstate           = nullptr;
      sk::ParameterBool*  auto_slot_assign        = nullptr;
      sk::ParameterBool*  blackout_api            = nullptr;
      sk::ParameterBool*  emulate                 = nullptr;
      sk::ParameterFloat* deadzone                = nullptr;
    } xinput;

    struct {
      sk::ParameterBool*  blackout_gamepads       = nullptr;
      sk::ParameterBool*  blackout_mice           = nullptr;
      sk::ParameterBool*  blackout_keyboards      = nullptr;
      sk::ParameterBool*  block_enum_devices      = nullptr;
    } dinput;

    struct {
      sk::ParameterBool*    disable_touchpad      = nullptr;
      sk::ParameterBool*    share_clicks_touch    = nullptr;
      sk::ParameterBool*    mute_applies_to_game  = nullptr;
      sk::ParameterBool*    enhanced_ps_button    = nullptr;
      sk::ParameterBool*    power_save_mode       = nullptr;
      sk::ParameterInt*     led_color_r           = nullptr;
      sk::ParameterInt*     led_color_g           = nullptr;
      sk::ParameterInt*     led_color_b           = nullptr;
      sk::ParameterInt*     led_brightness        = nullptr;
      sk::ParameterInt*     show_ds4_as_ds4_v2    = nullptr;
      sk::ParameterInt*     hide_ds4_v2_pid       = nullptr;
      sk::ParameterInt*     hide_ds_edge_pid      = nullptr;
      sk::ParameterBool*    enable_full_bluetooth = nullptr;
      sk::ParameterStringW* left_fn_bind          = nullptr;
      sk::ParameterStringW* right_fn_bind         = nullptr;
      sk::ParameterStringW* left_paddle_bind      = nullptr;
      sk::ParameterStringW* right_paddle_bind     = nullptr;
      sk::ParameterStringW* touch_click_bind      = nullptr;
    } scepad;

    struct {
      sk::ParameterBool*  disable                 = nullptr;
    } steam;

    struct {
      sk::ParameterInt*   max_allowed_buffers     = nullptr;
    } hid;

    sk::ParameterInt*     disabled_to_game        = nullptr;
    sk::ParameterBool*    bt_input_only           = nullptr;
    sk::ParameterFloat*   low_battery_warning     = nullptr;
    sk::ParameterBool*    blocks_screensaver      = nullptr;
    sk::ParameterFloat*   left_impulse_strength   = nullptr;
    sk::ParameterFloat*   right_impulse_strength  = nullptr;
    sk::ParameterFloat*   left_resist_strength    = nullptr;
    sk::ParameterFloat*   right_resist_strength   = nullptr;
    sk::ParameterFloat*   left_resist_start       = nullptr;
    sk::ParameterFloat*   right_resist_start      = nullptr;
  } gamepad;
} input;

struct {
  sk::ParameterBool*      enable_mem_alloc_trace  = nullptr;
  sk::ParameterBool*      enable_file_io_trace    = nullptr;
} threads;

struct {
  sk::ParameterBool*      borderless              = nullptr;
  sk::ParameterBool*      center                  = nullptr;

  struct {
    sk::ParameterStringW* x                       = nullptr;
    sk::ParameterStringW* y                       = nullptr;
  } offset;

  sk::ParameterBool*      background_render       = nullptr;
  sk::ParameterBool*      background_mute         = nullptr;
  sk::ParameterBool*      confine_cursor          = nullptr;
  sk::ParameterBool*      unconfine_cursor        = nullptr;
  sk::ParameterBool*      persistent_drag         = nullptr;
  sk::ParameterBool*      fullscreen              = nullptr;
  sk::ParameterStringW*   override                = nullptr;
  sk::ParameterBool*      multi_monitor_mode      = nullptr;
  sk::ParameterBool*      fix_mouse_coords        = nullptr;
  sk::ParameterInt*       always_on_top           = nullptr;
  sk::ParameterInt*       preferred_monitor_id    = nullptr; // Used if exact match cannot be found
  sk::ParameterStringW*   preferred_monitor_exact = nullptr;
  sk::ParameterBool*      disable_screensaver     = nullptr;
  sk::ParameterBool*      fullscreen_no_saver     = nullptr;
  sk::ParameterBool*      manage_screensaver      = nullptr;
  sk::ParameterBool*      dont_hook_wndproc       = nullptr;
  sk::ParameterBool*      activate_at_start       = nullptr;
  sk::ParameterBool*      treat_fg_as_active      = nullptr;
} window;

struct {
  struct {
    sk::ParameterBool*    raise_always            = nullptr;
    sk::ParameterBool*    raise_in_fg             = nullptr;
    sk::ParameterBool*    raise_in_bg             = nullptr;
    sk::ParameterBool*    highest_priority        = nullptr;
    sk::ParameterBool*    deny_foreign_change     = nullptr;
    sk::ParameterInt*     min_render_priority     = nullptr;
    sk::ParameterInt64*   cpu_affinity_mask       = nullptr;
    sk::ParameterBool*    perf_cores_only         = nullptr;
  } priority;
} scheduling;


struct {
  sk::ParameterStringW*   power_scheme_guid       = nullptr;
} cpu;

struct {
  sk::ParameterBool*      rehook_loadlibrary      = nullptr;
  sk::ParameterBool*      disable_nv_bloat        = nullptr;
  sk::ParameterBool*      using_wine              = nullptr;
  sk::ParameterBool*      allow_dxdiagn           = nullptr;
  sk::ParameterBool*      auto_large_address      = nullptr; // 32-bit only
  sk::ParameterBool*      async_init              = nullptr;
  sk::ParameterBool*      reshade_mode            = nullptr;
  sk::ParameterBool*      fsr3_mode               = nullptr;
  sk::ParameterBool*      allow_fake_streamline   = nullptr;
  sk::ParameterInt*       sdl_sanity_level        = nullptr;
  struct {
    sk::ParameterInt*     allow_wgi               = nullptr;
    sk::ParameterInt*     allow_raw_input         = nullptr;
    sk::ParameterInt*     allow_direct_input      = nullptr;
    sk::ParameterInt*     allow_xinput            = nullptr;
    sk::ParameterInt*     allow_hid               = nullptr;
    sk::ParameterInt*     allow_all_ps_bt_features= nullptr;
    sk::ParameterFloat*   switch_led_brightness   = nullptr;
    sk::ParameterInt*     use_joystick_thread     = nullptr;
    sk::ParameterInt*     poll_sentinel           = nullptr;
  } sdl;
} compatibility;

struct {
  struct {
    sk::ParameterBool*    hook                    = nullptr;
    sk::ParameterBool*    debug                   = nullptr;
  }
#ifdef _M_IX86
      ddraw, d3d8,
#endif
      d3d9,  d3d9ex,
      d3d11, d3d12,
#ifdef _M_AMD64
      Vulkan,
#endif
      OpenGL;

  struct {
    sk::ParameterInt*     enable                  = nullptr;
  }   dxvk9;

  sk::ParameterInt*       last_known              = nullptr;
} apis;

bool hook_xinput_orig    = true;
bool hook_scepad_orig    = true;
bool hook_wgi_orig       = true;
bool hook_raw_input_orig = true;
bool hook_dinput7_orig   = true;
bool hook_dinput8_orig   = true;
bool hook_hid_orig       = true;
bool hook_winmm_orig     = true;

bool
SK_LoadConfig (const std::wstring& name)
{
  if (SK_GetHostAppUtil ()->isBlacklisted ())
  {
    return false;
  }

  ULARGE_INTEGER
    useable  = { }, // Amount the current user's quota allows
    capacity = { },
    free     = { };

  wchar_t                         wszStartupDir [MAX_PATH + 2] = { };
  GetCurrentDirectoryW (MAX_PATH, wszStartupDir);

  // Check Disk Space for non-Microsoft Store Games
  const BOOL bDiskSpace = (nullptr == StrStrIW (wszStartupDir, L"WindowsApps")) &&
    GetDiskFreeSpaceExW (
      wszStartupDir,
        &useable, &capacity, &free );

  if ( SK_GetCurrentGameID () == SK_GAME_ID::Launcher ||
        (bDiskSpace && useable.QuadPart < 1024ull * 1024ull * 5ull) )
  { if ( bDiskSpace && useable.QuadPart < 1024ull * 1024ull * 5ull )
    {
      UINT uiDriveNum =
        PathGetDriveNumberW (wszStartupDir);

      bool bRet =
        SK_LoadConfigEx (name);

      if (config.system.global_inject_delay < 0.05f)
      {
        SK_RunOnce (
          SK_ImGui_WarningWithTitle (
            SK_FormatStringW (
              L"Special K Requires 5+ MiB of Free Storage on Drive %hc:\\ \t ( %ws )",
                ('A' + (char)uiDriveNum), wszStartupDir ).c_str (),
              L"Insufficient Storage"
          )
        );

        config.system.global_inject_delay = 0.05f;
      }

      return bRet;
    }

    config.system.global_inject_delay = 0.25f;

    return
      (config.system.silent = true);
  }

  return
    SK_LoadConfigEx (name);
}


class SK_ScopedLocale {
public:
  SK_ScopedLocale (const wchar_t *wszLocale)
  {
    if (wszLocale != nullptr)
    {
      prev_locale_policy =
        _configthreadlocale (_ENABLE_PER_THREAD_LOCALE);

      if (prev_locale_policy != -1)
      {
        if (auto orig = _wsetlocale (LC_ALL, wszLocale);
                 orig != nullptr)
        {
          orig_locale = orig;
        }
      }
    }
  }

  ~SK_ScopedLocale (void)
  {
    if (prev_locale_policy != -1)
    {
      if (! orig_locale.empty ())
        _wsetlocale (LC_ALL, orig_locale.c_str ());

      _configthreadlocale (prev_locale_policy);
    }
  }

private:
  std::wstring orig_locale        = L"";
  int          prev_locale_policy = -1;
};


__declspec (noinline)
const wchar_t*
__stdcall
SK_GetConfigPathEx (bool reset)
{
  if ((! SK_IsInjected ()) && (! config.system.central_repository))
    return SK_GetNaiveConfigPath ();

  static volatile LONG init = FALSE;

  if (! InterlockedCompareExchange (&init, 1, 0))
  {
    InterlockedIncrement (&init);
    app_cache_mgr->loadAppCacheForExe ( SK_GetFullyQualifiedApp () );
  }

  else
    SK_Thread_SpinUntilAtomicMin (&init, 2);

  static wchar_t
    cached_path [MAX_PATH + 2] = { };

  if (reset || InterlockedCompareExchange (&init, 3, 2) == 2)
  {
    wcscpy_s ( cached_path, MAX_PATH,
               app_cache_mgr->getConfigPathFromAppPath (
                 SK_GetFullyQualifiedApp ()
               ).c_str ()
           );

    InterlockedIncrement (&init);
  }

  else
    SK_Thread_SpinUntilAtomicMin (&init, 3);

  return
    cached_path;
}

__declspec (noinline)
const wchar_t*
__stdcall
SK_GetConfigPath (void)
{
  return
    SK_GetConfigPathEx ();
}



template <class _Tp>
_Tp*
SK_CreateINIParameter ( const wchar_t *wszDescription,
                        iSK_INI       *pINIFile,
                        const wchar_t *wszSection,
                        const wchar_t *wszKey )
{
  using value_type =
   _Tp::value_type;

  static_assert (std::is_polymorphic <_Tp> ());

  auto ret =
    dynamic_cast <_Tp *> (
      g_ParameterFactory->create_parameter <value_type> (
          wszDescription )
    );

  if (ret != nullptr)
      ret->register_to_ini (pINIFile, wszSection, wszKey);

  return ret;
};


bool
SK_LoadConfigEx (std::wstring name, bool create)
{
  if (SK_GetHostAppUtil ()->isBlacklisted ())
  {
    return false;
  }

  try
  {
  if (name.empty ())
  {
    if (SK_IsInjected ())
      name = L"SpecialK";
    else
      name = SK_GetBackend ();
  }

  static std::wstring orig_name = name;

  if (name.empty ()) name = orig_name;

  SK_ScopedLocale _locale (L"en_us.utf8");

  // Load INI File
  std::wstring full_name;
  std::wstring custom_name; // User may have custom prefs
  std::wstring master_name;

  std::wstring   osd_config, platform_config,
               macro_config,    input_config,
              notify_config;

  full_name = // For paths with :, do not prepend the config root
    name.find (L':') != std::wstring::npos ?
      SK_FormatStringW ( L"%s.ini",             name.c_str () ) :
      SK_FormatStringW ( L"%s%s.ini",
                           SK_GetConfigPath (), name.c_str () );

  std::wstring undecorated_name (name);

  if ( undecorated_name.find (L"default_") != std::wstring::npos &&
       undecorated_name.find (L':')        == std::wstring::npos )
  {
      undecorated_name.erase ( undecorated_name.find (L"default_"),
                                        std::wstring (L"default_").length () );
  }

  custom_name =
    SK_FormatStringW ( L"%scustom_%s.ini",
                       SK_GetConfigPath (), undecorated_name.c_str () );

  if (create)
    SK_CreateDirectories ( full_name.c_str () );

  SK_RenderBackend& rb =
    SK_GetCurrentRenderBackend ();


  static LONG         init     = FALSE;
  static bool         empty    = true;
  static std::wstring last_try = name;

  // Allow a second load attempt using a different name
  if (last_try != name)
  {
    init     = FALSE;
    last_try = std::move (name);
  }


  master_name        =
    std::wstring (SK_GetInstallPath ()) + LR"(\Global\master_)" + undecorated_name + L".ini";

  osd_config         =
    std::wstring (SK_GetInstallPath ()) + LR"(\Global\osd.ini)";

  input_config       =
    std::wstring (SK_GetInstallPath ()) + LR"(\Global\input.ini)";

  platform_config    =
    std::wstring (SK_GetInstallPath ()) + LR"(\Global\platform.ini)";

  std::wstring migrate_platform_config;

  macro_config       =
    std::wstring (SK_GetInstallPath ()) + LR"(\Global\macros.ini)";

  notify_config      =
    std::wstring (SK_GetInstallPath ()) + LR"(\Global\notifications.ini)";

  if (init == FALSE || dll_ini == nullptr)
  {
    SK_RunOnce (     dll_ini   =
      SK_CreateINI (full_name.c_str ()));

    init = -1;

    if (! dll_ini)
    {
      SK_ReleaseAssert (false && L"Out Of Memory");
      init = FALSE; return false;
    }

    dll_ini->set_encoding (iSK_INI::INI_UTF16LE);
    dll_ini->reload       (full_name.c_str ());


    empty   =
      dll_ini->get_sections ().empty ();


    SK_CreateDirectories (osd_config.c_str ());

    SK_RunOnce (
    {
      osd_ini         =
        SK_CreateINI (osd_config.c_str ());
      input_ini       =
        SK_CreateINI (input_config.c_str ());
      platform_ini    =
        SK_CreateINI (platform_config.c_str ());
      macro_ini       =
        SK_CreateINI (macro_config.c_str ());
      notify_ini       =
        SK_CreateINI (notify_config.c_str ());
    });


    osd_ini->reload      ();
    input_ini->reload    ();
    platform_ini->reload ();
    macro_ini->reload    ();
    notify_ini->reload   ();


auto DeclKeybind =
  [](const SK_ConfigSerializedKeybind* binding, iSK_INI* ini, const wchar_t* sec) ->
    auto
    {
      auto* ret =
       dynamic_cast <sk::ParameterStringW *>
        (g_ParameterFactory->create_parameter <std::wstring> (L"DESCRIPTION HERE"));

      if (ret != nullptr && binding != nullptr)
      {
        ret->register_to_ini ( ini, sec, binding->short_name );
      }

      return ret;
    };

// TODO:  Covert this to a template
#define ConfigEntry(param,descrip,ini,sec,key) {   \
  reinterpret_cast <sk::iParameter **> (&(param)), \
  std::type_index (              typeid ((param))),\
                    (descrip),  (ini),             \
                    (sec),      (key)              \
}

#define Keybind(bind,descrip,ini,sec) {             \
  reinterpret_cast <sk::iParameter **>  ((bind)),   \
  std::type_index (              typeid ((bind))),  \
                    (descrip),  (ini),              \
                    (sec),      ((bind)->short_name)\
}

  //
  // Create Parameters
  //
  struct param_decl_s {
           sk::iParameter **parameter_   = nullptr;
          std::type_index   type_;
    const wchar_t          *description_ = nullptr;
          iSK_INI          *ini_         = nullptr;
    const wchar_t          *section_     = nullptr;
    const wchar_t          *key_         = nullptr;
  };

  static const std::initializer_list <param_decl_s> params_to_build
  //// nb: If you want any hope of reading this table, turn line wrapping off.
  //
  {
    ConfigEntry (osd.version_banner.duration,            L"How long to display version info at startup, 0=disable)",   osd_ini,         L"SpecialK.VersionBanner",L"Duration"),
    ConfigEntry (osd.show,                               L"OSD Visibility",                                            osd_ini,         L"SpecialK.OSD",          L"Show"),

    ConfigEntry (osd.text.red,                           L"OSD Color (Red)",                                           osd_ini,         L"SpecialK.OSD",          L"TextColorRed"),
    ConfigEntry (osd.text.green,                         L"OSD Color (Green)",                                         osd_ini,         L"SpecialK.OSD",          L"TextColorGreen"),
    ConfigEntry (osd.text.blue,                          L"OSD Color (Blue)",                                          osd_ini,         L"SpecialK.OSD",          L"TextColorBlue"),

    ConfigEntry (osd.viewport.pos_x,                     L"OSD Position (X)",                                          osd_ini,         L"SpecialK.OSD",          L"PositionX"),
    ConfigEntry (osd.viewport.pos_y,                     L"OSD Position (Y)",                                          osd_ini,         L"SpecialK.OSD",          L"PositionY"),

    ConfigEntry (osd.viewport.scale,                     L"OSD Scale",                                                 osd_ini,         L"SpecialK.OSD",          L"Scale"),

    ConfigEntry (osd.state.remember,                     L"Remember status monitoring state",                          osd_ini,         L"SpecialK.OSD",          L"RememberMonitoringState"),
    ConfigEntry (render.osd.hdr_luminance,               L"OSD's Luminance (cd.m^-2) in HDR games",                    osd_ini,         L"SpecialK.OSD",          L"HDRLuminance"),

    ConfigEntry (monitoring.SLI.show,                    L"Show SLI Monitoring",                                       osd_ini,         L"Monitor.SLI",           L"Show"),

    ConfigEntry (uplay.overlay.hdr_luminance,            L"Make the uPlay Overlay visible in HDR mode!",               osd_ini,         L"uPlay.Overlay",         L"Luminance_scRGB"),
    ConfigEntry (rtss.overlay.hdr_luminance,             L"Make the RTSS Overlay visible in HDR mode!",                osd_ini,         L"RTSS.Overlay",          L"Luminance_scRGB"),
    ConfigEntry (discord.overlay.hdr_luminance,          L"Make the Discord Overlay visible in HDR mode!",             osd_ini,         L"Discord.Overlay",       L"Luminance_scRGB"),
    ConfigEntry (reshade_cfg.overlay.hdr_luminance,      L"Make the ReShade Overlay visible in HDR mode!",             osd_ini,         L"ReShade.Overlay",       L"Luminance_scRGB"),

    ConfigEntry (display.confirm_mode_changes,           L"Show Confirmation Dialog when Changing Display Modes",      osd_ini,         L"Display.Settings",      L"ConfirmChanges"),
    ConfigEntry (display.save_monitor_prefs,             L"Remember Monitor Preferences for the Current Game",         dll_ini,         L"Display.Monitor",       L"RememberPreference"),
    ConfigEntry (display.save_resolution,                L"Remember Monitor Resolution for the Current Game" ,         dll_ini,         L"Display.Monitor",       L"RememberResolution"),
    ConfigEntry (display.override_resolution,            L"Apply Resolution Override for the Current Game",            dll_ini,         L"Display.Monitor",       L"ResolutionForMonitor"),
    ConfigEntry (display.override_refresh,               L"Apply Refresh Override for the Current Game",               dll_ini,         L"Display.Monitor",       L"RefreshRateForMonitor"),
    ConfigEntry (display.warn_no_mpo_planes,             L"Warn user if Multiplane Overlays support is missing",       dll_ini,         L"Display.Monitor",       L"WarnIfNoOverlayPlanes"),

    // Performance Monitoring  (Global Settings)
    //////////////////////////////////////////////////////////////////////////

    ConfigEntry (monitoring.io.show,                     L"Show IO Monitoring",                                        osd_ini,         L"Monitor.IO",            L"Show"),
    ConfigEntry (monitoring.io.interval,                 L"IO Monitoring Interval",                                    osd_ini,         L"Monitor.IO",            L"Interval"),

    ConfigEntry (monitoring.disk.show,                   L"Show Disk Monitoring",                                      osd_ini,         L"Monitor.Disk",          L"Show"),
    ConfigEntry (monitoring.disk.interval,               L"Disk Monitoring Interval",                                  osd_ini,         L"Monitor.Disk",          L"Interval"),
    ConfigEntry (monitoring.disk.type,                   L"Disk Monitoring Type (0 = Physical, 1 = Logical)",          osd_ini,         L"Monitor.Disk",          L"Type"),

    ConfigEntry (monitoring.cpu.show,                    L"Show CPU Monitoring",                                       osd_ini,         L"Monitor.CPU",           L"Show"),
    ConfigEntry (monitoring.cpu.interval,                L"CPU Monitoring Interval (seconds)",                         osd_ini,         L"Monitor.CPU",           L"Interval"),
    ConfigEntry (monitoring.cpu.simple,                  L"Minimal CPU Info",                                          osd_ini,         L"Monitor.CPU",           L"Simple"),

    ConfigEntry (monitoring.gpu.show,                    L"Show GPU Monitoring",                                       osd_ini,         L"Monitor.GPU",           L"Show"),
    ConfigEntry (monitoring.gpu.interval,                L"GPU Monitoring Interval (msecs)",                           osd_ini,         L"Monitor.GPU",           L"Interval"),
    ConfigEntry (monitoring.gpu.print_slowdown,          L"Print GPU Slowdown Reason (NVIDA GPUs)",                    osd_ini,         L"Monitor.GPU",           L"PrintSlowdown"),

    ConfigEntry (monitoring.pagefile.show,               L"Show Pagefile Monitoring",                                  osd_ini,         L"Monitor.Pagefile",      L"Show"),
    ConfigEntry (monitoring.pagefile.interval,           L"Pagefile Monitoring Interval (seconds)",                    osd_ini,         L"Monitor.Pagefile",      L"Interval"),

    ConfigEntry (monitoring.dlss.show,                   L"Show DLSS Resolution Information",                          osd_ini,         L"Monitor.DLSS",          L"Show"),
    ConfigEntry (monitoring.dlss.show_output_res,        L"Print DLSS Output Resolution",                              osd_ini,         L"Monitor.DLSS",          L"ShowOutputResolution"),
    ConfigEntry (monitoring.dlss.show_quality,           L"Print DLSS Quality Level",                                  osd_ini,         L"Monitor.DLSS",          L"ShowQuality"),
    ConfigEntry (monitoring.dlss.show_preset,            L"Print DLSS Preset",                                         osd_ini,         L"Monitor.DLSS",          L"ShowPreset"),
    ConfigEntry (monitoring.dlss.show_fg,                L"Print DLSS Frame Generation Status",                        osd_ini,         L"Monitor.DLSS",          L"ShowFrameGeneration"),

    ConfigEntry (monitoring.memory.show,                 L"Show Memory Monitoring",                                    osd_ini,         L"Monitor.Memory",        L"Show"),
    ConfigEntry (monitoring.fps.show,                    L"Show Framerate Monitoring",                                 osd_ini,         L"Monitor.FPS",           L"Show"),
    ConfigEntry (monitoring.fps.frametime,               L"Show Frametime in Framerate Counter",                       osd_ini,         L"Monitor.FPS",           L"DisplayFrametime"),
    ConfigEntry (monitoring.fps.advanced,                L"Show Advanced Statistics in Framerate Counter",             osd_ini,         L"Monitor.FPS",           L"AdvancedStatistics"),
    ConfigEntry (monitoring.fps.compact,                 L"Show FRAPS-like ('120') Statistics in Framerate Counter",   osd_ini,         L"Monitor.FPS",           L"CompactStatistics"),
    ConfigEntry (monitoring.fps.compact_vrr,             L"Show VRR Status in Compact Mode",                           osd_ini,         L"Monitor.FPS",           L"CompactIncludesVRR"),
    ConfigEntry (monitoring.fps.framenumber,             L"Show Frame Number",                                         osd_ini,         L"Monitor.FPS",           L"DisplayFrameNumber"),
    ConfigEntry (monitoring.fps.timing_method,           L"How to measure frame intervals for the framepacing widget.",osd_ini,         L"Monitor.FPS",           L"FrametimeMethod"),
    ConfigEntry (monitoring.time.show,                   L"Show System Clock",                                         osd_ini,         L"Monitor.Time",          L"Show"),
    ConfigEntry (monitoring.title.show,                  L"Show Special K Title",                                      osd_ini,         L"Monitor.Title",         L"Show"),

    ConfigEntry (prefer_fahrenheit,                      L"Prefer Fahrenheit Units",                                   osd_ini,         L"SpecialK.OSD",          L"PreferFahrenheit"),

    ConfigEntry (imgui.scale,                            L"ImGui Scale",                                               osd_ini,         L"ImGui.Global",          L"FontScale"),
    ConfigEntry (imgui.show_playtime,                    L"Display Playing Time in Config UI",                         osd_ini,         L"ImGui.Global",          L"ShowPlaytime"),
    ConfigEntry (imgui.show_gsync_status,                L"Show G-Sync Status on Control Panel",                       osd_ini,         L"ImGui.Global",          L"ShowGSyncStatus"),
    ConfigEntry (imgui.mac_style_menu,                   L"Use Mac-style Menu Bar",                                    osd_ini,         L"ImGui.Global",          L"UseMacStyleMenu"),
    ConfigEntry (imgui.show_input_apis,                  L"Show Input APIs currently in-use",                          osd_ini,         L"ImGui.Global",          L"ShowActiveInputAPIs"),
    ConfigEntry (imgui.center_cursor_on_overlay,         L"Center the mouse cursor when opening SK's overlay",         osd_ini,         L"ImGui.Global",          L"CenterCursorOnOverlayToggle"),
    ConfigEntry (imgui.nav_moves_mouse,                  L"Keyboard/Gamepad selection changes move the mouse cursor",  osd_ini,         L"ImGui.Global",          L"NavigationMovesMouseCursor"),

    ConfigEntry (screenshots.keep_png_copy,              L"Keep a .PNG compressed copy of each screenshot?",           osd_ini,         L"Screenshot.System",     L"KeepLosslessPNG"),
    ConfigEntry (screenshots.play_sound,                 L"Play a Sound when triggering Screenshot Capture",           osd_ini,         L"Screenshot.System",     L"PlaySoundOnCapture"),
    ConfigEntry (screenshots.copy_to_clipboard,          L"Copy an LDR/HDR copy to the Windows Clipboard",             osd_ini,         L"Screenshot.System",     L"CopyToClipboard"),
    ConfigEntry (screenshots.embed_nickname,             L"Add Steam/Epic nickname as Author to Screenshot Metadata",  osd_ini,         L"Screenshot.System",     L"AuthorMetadata"),
    ConfigEntry (screenshots.override_path,              L"Where to store screenshots (if non-empty)",                 osd_ini,         L"Screenshot.System",     L"OverridePath"),
    ConfigEntry (screenshots.filename_format,            L"wcsftime format; Non-Standard Specifier: %G = <Game Name>", osd_ini,         L"Screenshot.System",     L"FilenameFormat"),
    ConfigEntry (screenshots.compression_quality,        L"Compression Quality: 0=Worst, 100=Lossless",                osd_ini,         L"Screenshot.System",     L"Quality"),
    ConfigEntry (screenshots.compatibility_mode,         L"Use less advanced encoding in JPEG XR and AVIF for compat.",osd_ini,         L"Screenshot.System",     L"CompatibilityMode"),
    ConfigEntry (screenshots.jxl.use_jxl,                L"Use JPEG XL file format for HDR screenshots",               osd_ini,         L"Screenshot.System",     L"UseJPEGXL"),
    ConfigEntry (screenshots.avif.use_avif,              L"Use AVIF file format for HDR screenshots",                  osd_ini,         L"Screenshot.System",     L"UseAVIF"),
    ConfigEntry (screenshots.avif.yuv_subsampling,       L"Chroma Subsampling (444, 422, 420, 400)",                   osd_ini,         L"Screenshot.AVIF",       L"SubsampleYUV"),
    ConfigEntry (screenshots.avif.scrgb_bit_depth,       L"Bits to use for scRGB to PQ encoded images",                osd_ini,         L"Screenshot.AVIF",       L"scRGBtoPQBits"),
    ConfigEntry (screenshots.avif.compression_speed,     L"Compression Speed: 0=Slowest (Smallest File), 10=Fastest",  osd_ini,         L"Screenshot.AVIF",       L"Speed"),
    ConfigEntry (screenshots.png.store_hdr,              L"Use HDR PNG file format for HDR screenshots",               osd_ini,         L"Screenshot.HDR",        L"StorePNG"),
    ConfigEntry (screenshots.allow_hdr_clipboard,        L"Use HDR for Windows Clipboard screenshots",                 osd_ini,         L"Screenshot.HDR",        L"AllowClipboardHDR"),
    ConfigEntry (screenshots.png.st2084_bits,            L"Use n-bit Quantization to save Disk Space",                 osd_ini,         L"Screenshot.HDR",        L"MaxST2084QuantizedBits"),
    ConfigEntry (screenshots.clipboard_hdr_format,       L"Use PNG or AVIF for HDR clipboard screenshots",             osd_ini,         L"Screenshot.HDR",        L"HDRClipboardFormat"),
    Keybind (&config.render.keys.hud_toggle,             L"Toggle Game's HUD",                                         osd_ini,         L"Game.HUD"),
    Keybind (&config.osd.keys.console_toggle,            L"Toggle SK's Command Console",                               osd_ini,         L"OSD.System"),
    Keybind (&config.screenshots.game_hud_free_keybind,  L"Take a screenshot without the HUD",                         osd_ini,         L"Screenshot.System"),
    Keybind (&config.screenshots.sk_osd_free_keybind,    L"Take a screenshot without SK's OSD",                        osd_ini,         L"Screenshot.System"),
    Keybind (&config.screenshots.
                               sk_osd_insertion_keybind, L"Take a screenshot and insert SK's OSD",                     osd_ini,         L"Screenshot.System"),
    Keybind (&config.screenshots.no_3rd_party_keybind,   L"Take a screenshot before third-party overlays",             osd_ini,         L"Screenshot.System"),
    Keybind (&config.screenshots.clipboard_only_keybind, L"Take a screenshot and copy it to the clipboard only",       osd_ini,         L"Screenshot.System"),
    Keybind (&config.screenshots.snipping_keybind,       L"Snip a screenshot and copy it to the clipboard",            osd_ini,         L"Screenshot.System"),

    Keybind (&config.monitors.multimonitor_focus_keybind,L"Toggle Multi-Monitor Focus Mode",                           osd_ini,         L"Display.Monitor"),
    Keybind (&config.monitors.monitor_primary_keybind,   L"Move Game to Primary Monitor",                              osd_ini,         L"Display.Monitor"),
    Keybind (&config.monitors.monitor_next_keybind,      L"Move Game to Next Monitor",                                 osd_ini,         L"Display.Monitor"),
    Keybind (&config.monitors.monitor_prev_keybind,      L"Move Game to Previous Monitor",                             osd_ini,         L"Display.Monitor"),
    Keybind (&config.monitors.monitor_toggle_hdr,        L"Toggle HDR on Selected Monitor",                            osd_ini,         L"Display.Monitor"),

    Keybind ( &config.render.framerate.latent_sync.
                             tearline_move_down_keybind, L"Move Tear Location Down 1 Scanline",                        osd_ini,         L"LatentSync.Control"),
    Keybind ( &config.render.framerate.latent_sync.
                               tearline_move_up_keybind, L"Move Tear Location Up 1 Scanline",                          osd_ini,         L"LatentSync.Control"),
    Keybind ( &config.render.framerate.latent_sync.
                                  timing_resync_keybind, L"Request a Monitor Timing Resync",                           osd_ini,         L"LatentSync.Control"),
    Keybind ( &config.render.framerate.latent_sync.
                               toggle_fcat_bars_keybind, L"Toggle FCAT Tearing Visualizer",                            osd_ini,         L"LatentSync.Control"),

    Keybind ( &config.sound.game_mute_keybind,           L"Toggle Mute for the Game",                                  osd_ini,         L"Sound.Mixing"),
    Keybind ( &config.sound.game_volume_up_keybind,      L"Increase Game Volume 10%",                                  osd_ini,         L"Sound.Mixing"),
    Keybind ( &config.sound.game_volume_down_keybind,    L"Decrease Game Volume 10%",                                  osd_ini,         L"Sound.Mixing"),

    Keybind ( &config.widgets.hide_all_widgets_keybind,  L"Temporarily hide all widgets",                              osd_ini,         L"Widgets.Global"),
    Keybind ( &config.reshade.toggle_overlay_keybind,    L"Toggle ReShade Overlay (Add-On version)",                   osd_ini,         L"ReShade.AddOn"),
    Keybind ( &config.reshade.inject_reshade_keybind,    L"Inject ReShade (6.0+) as a Global PlugIn",                  osd_ini,         L"ReShade.AddOn"),


    // Input
    //////////////////////////////////////////////////////////////////////////

    ConfigEntry (input.keyboard.catch_alt_f4,            L"If the game does not handle Alt+F4, offer a replacement",   dll_ini,         L"Input.Keyboard",        L"CatchAltF4"),
    ConfigEntry (input.keyboard.bypass_alt_f4,           L"Forcefully disable a game's Alt+F4 handler",                dll_ini,         L"Input.Keyboard",        L"BypassAltF4Handler"),
    ConfigEntry (input.keyboard.disabled_to_game,        L"Completely stop all keyboard input from reaching the Game", dll_ini,         L"Input.Keyboard",        L"DisabledToGame"),

    ConfigEntry (input.mouse.disabled_to_game,           L"Completely stop all mouse input from reaching the Game",    dll_ini,         L"Input.Mouse",           L"DisabledToGame"),

    ConfigEntry (input.cursor.manage,                    L"Manage Cursor Visibility (due to inactivity)",              dll_ini,         L"Input.Cursor",          L"Manage"),
    ConfigEntry (input.cursor.keys_activate,             L"Keyboard Input Activates Cursor",                           dll_ini,         L"Input.Cursor",          L"KeyboardActivates"),
    ConfigEntry (input.cursor.gamepad_deactivates,       L"Gamepad Input Deactivates Cursor",                          dll_ini,         L"Input.Cursor",          L"GamepadDeactivates"),
    ConfigEntry (input.cursor.timeout,                   L"Inactivity Timeout (in milliseconds)",                      dll_ini,         L"Input.Cursor",          L"Timeout"),
    ConfigEntry (input.cursor.ui_capture,                L"Forcefully Capture Mouse Cursor in UI Mode",                dll_ini,         L"Input.Cursor",          L"ForceCaptureInUI"),
    ConfigEntry (input.cursor.hw_cursor,                 L"Use a Hardware Cursor for Special K's UI Features",         dll_ini,         L"Input.Cursor",          L"UseHardwareCursor"),
    ConfigEntry (input.cursor.block_invisible,           L"Block Mouse Input if Hardware Cursor is Invisible",         dll_ini,         L"Input.Cursor",          L"BlockInvisibleCursorInput"),
    ConfigEntry (input.cursor.fix_synaptics,             L"Fix Synaptic Touchpad Scroll",                              dll_ini,         L"Input.Cursor",          L"FixSynapticsTouchpadScroll"),

    ConfigEntry (input.gamepad.disabled_to_game,         L"Disable ALL Gamepad Input (across all APIs)",               dll_ini,         L"Input.Gamepad",         L"DisabledToGame"),
    ConfigEntry (input.gamepad.disable_hid,              L"Disable HID Input (prevent double-input if XInput is used)",dll_ini,         L"Input.Gamepad",         L"DisableHID"),
    ConfigEntry (input.gamepad.disable_winmm,            L"Disable WinMM Joystick Input",                              dll_ini,         L"Input.Gamepad",         L"DisableWinMM"),
    ConfigEntry (input.gamepad.haptic_ui,                L"Give tactile feedback on gamepads when navigating the UI",  dll_ini,         L"Input.Gamepad",         L"AllowHapticUI"),
    ConfigEntry (input.gamepad.hook_windows_gaming,      L"Install hooks for Windows.Gaming.Input",                    dll_ini,         L"Input.Gamepad",         L"EnableWindowsGamingInput"),
    ConfigEntry (input.gamepad.hook_raw_input,           L"Install hooks for RawInput and process WM_INPUT messages",  dll_ini,         L"Input.Gamepad",         L"EnableRawInput"),
    ConfigEntry (input.gamepad.hook_dinput8,             L"Install hooks for DirectInput 8",                           dll_ini,         L"Input.Gamepad",         L"EnableDirectInput8"),
    ConfigEntry (input.gamepad.hook_dinput7,             L"Install hooks for DirectInput 7",                           dll_ini,         L"Input.Gamepad",         L"EnableDirectInput7"),
    ConfigEntry (input.gamepad.hook_hid,                 L"Install hooks for HID",                                     dll_ini,         L"Input.Gamepad",         L"EnableHID"),
    ConfigEntry (input.gamepad.hook_game_input,          L"Install hooks for GameInput",                               dll_ini,         L"Input.Gamepad",         L"HookGameInput"),
    ConfigEntry (input.gamepad.hook_winmm,               L"Install hooks for joyGet* APIs",                            dll_ini,         L"Input.Gamepad",         L"HookWinMM"),
    ConfigEntry (input.gamepad.allow_steam_winmm,        L"Use Steam-manipulated version of WinMM input",              dll_ini,         L"Input.Gamepad",         L"AllowSteamWinMM"),
    ConfigEntry (input.gamepad.disable_rumble,           L"Disable Rumble from ALL SOURCES (across all APIs)",         dll_ini,         L"Input.Gamepad",         L"DisableRumble"),
    ConfigEntry (input.gamepad.blocks_screensaver,       L"Gamepad activity will block screensaver activation",        dll_ini,         L"Input.Gamepad",         L"BlocksScreenSaver"),
    ConfigEntry (input.gamepad.right_impulse_strength,   L"Scale the Right Impulse Triggers in GameInput games",       dll_ini,         L"Input.Gamepad",         L"RightImpulseStrength"),
    ConfigEntry (input.gamepad.left_impulse_strength,    L"Scale the Left Impulse Triggers in GameInput games",        dll_ini,         L"Input.Gamepad",         L"LeftImpulseStrength"),
    ConfigEntry (input.gamepad.right_resist_strength,    L"Apply a constant Right Trigger resistance on DualSense",    dll_ini,         L"Input.Gamepad",         L"RightTriggerResistance"),
    ConfigEntry (input.gamepad.left_resist_strength,     L"Apply a constant Left Trigger resistance on DualSense",     dll_ini,         L"Input.Gamepad",         L"LeftTriggerResistance"),
    ConfigEntry (input.gamepad.right_resist_start,       L"Ratio of Right Trigger pull before resistance applies",     dll_ini,         L"Input.Gamepad",         L"RightTriggerResistsAt"),
    ConfigEntry (input.gamepad.left_resist_start,        L"Ratio of Left Trigger pull before resistance applies",      dll_ini,         L"Input.Gamepad",         L"LeftTriggerResistsAt"),
    ConfigEntry (input.gamepad.bt_input_only,            L"Prevent Bluetooth Output (PlayStation DirectInput compat.)",dll_ini,         L"Input.Gamepad",         L"BluetoothInputOnly"),
    ConfigEntry (input.gamepad.hid.max_allowed_buffers,  L"Maximum allowed HID buffers; 32=NS default, 8=SK default,"
                                                         L" this will lower latency at the expense of possibly missed"
                                                         L" inputs...",                                                dll_ini,         L"Input.Gamepad",         L"MaxHIDPollingBuffers"),

    ConfigEntry (input.gamepad.hook_xinput,              L"Install hooks for XInput",                                  dll_ini,         L"Input.XInput",          L"Enable"),
    ConfigEntry (input.gamepad.rehook_xinput,            L"Re-install XInput hooks if hookchain is modified",          dll_ini,         L"Input.XInput",          L"Rehook"),
    ConfigEntry (input.gamepad.xinput.ui_slot,           L"XInput Controller that owns the config UI",                 dll_ini,         L"Input.XInput",          L"UISlot"),
    ConfigEntry (input.gamepad.xinput.placeholders,      L"XInput Controller Slots to Fake Connectivity On",           dll_ini,         L"Input.XInput",          L"PlaceholderMask"),
    ConfigEntry (input.gamepad.xinput.assignment,        L"Re-Assign XInput Slots",                                    dll_ini,         L"Input.XInput",          L"SlotReassignment"),
    ConfigEntry (input.gamepad.xinput.disable_slots,     L"Disable Devices Connected to Specific XInput Slots",        dll_ini,         L"Input.XInput",          L"DisableSlots"),
    ConfigEntry (input.gamepad.xinput.hook_setstate,     L"Hook vibration; fix third-party created feedback loops",    dll_ini,         L"Input.XInput",          L"HookSetState"),
    ConfigEntry (input.gamepad.xinput.auto_slot_assign,  L"Switch a game hard-coded to use Slot 0 to an active pad",   dll_ini,         L"Input.XInput",          L"AutoSlotAssign"),
    ConfigEntry (input.gamepad.xinput.blackout_api,      L"Prevent game from seeing XInput at all, useful if a game "
                                                         L"supports native SONY input and XInput.",                    dll_ini,         L"Input.XInput",          L"HideAllDevices"),
    ConfigEntry (input.gamepad.xinput.emulate,           L"For non-Xbox controllers, translate HID to XInput",         dll_ini,         L"Input.XInput",          L"EnableEmulation"),
    ConfigEntry (input.gamepad.xinput.deadzone,          L"In HID->XInput, filter analog values below this threshold", dll_ini,         L"Input.XInput",          L"DeadzonePercent"),
    ConfigEntry (input.gamepad.dinput.blackout_gamepads, L"Prevent game from seeing DirectInput gamepads",             dll_ini,         L"Input.DInput",          L"HideGamepads"),
    ConfigEntry (input.gamepad.dinput.blackout_mice,     L"Prevent game from seeing DirectInput mice",                 dll_ini,         L"Input.DInput",          L"HideMice"),
    ConfigEntry (input.gamepad.dinput.blackout_keyboards,L"Prevent game from seeing DirectInput keyboards",            dll_ini,         L"Input.DInput",          L"HideKeyboards"),
    ConfigEntry (input.gamepad.dinput.block_enum_devices,L"Prevent Steam Input from utterly destroying performance",   dll_ini,         L"Input.DInput",          L"PreventEnumDevices"),

    ConfigEntry (input.gamepad.hook_scepad,              L"Install hooks for libScePad",                               dll_ini,         L"Input.libScePad",       L"Enable"),
    ConfigEntry (input.gamepad.scepad.disable_touchpad,  L"Disable Touchpad Input",                                    dll_ini,         L"Input.libScePad",       L"DisableTouchpad"),
    ConfigEntry (input.gamepad.scepad.share_clicks_touch,L"Share Button can be used as Touchpad Click",                input_ini,       L"Input.libScePad",       L"ShareClicksTouchpad"),
    ConfigEntry (input.gamepad.scepad.
                                   mute_applies_to_game, L"Mute Button on DualSense will Mute the Game",               input_ini,       L"Input.libScePad",       L"MuteButtonAppliesToGame"),
    ConfigEntry (input.gamepad.scepad.enhanced_ps_button,L"PlayStation / Home Button activates SK's control panel and "
                                                         L"may be used for special button combos (e.g. trigger sshot)",input_ini,       L"Input.libScePad",       L"AdvancedPlayStationButton"),
    ConfigEntry (input.gamepad.scepad.power_save_mode,   L"Reduced power for Audio/Gyro/Touchpad on Bluetooth",        input_ini,       L"Input.libScePad",       L"EnableBluetoothPowerSaving"),
    ConfigEntry (input.gamepad.scepad.led_color_r,       L"Force Red LED Color [0,255] or -1 for No Override",         input_ini,       L"Input.libScePad",       L"LEDColor_R"),
    ConfigEntry (input.gamepad.scepad.led_color_g,       L"Force Green LED Color [0,255] or -1 for No Override",       input_ini,       L"Input.libScePad",       L"LEDColor_G"),
    ConfigEntry (input.gamepad.scepad.led_color_b,       L"Force Blue LED Color [0,255] or -1 for No Override",        input_ini,       L"Input.libScePad",       L"LEDColor_B"),
    ConfigEntry (input.gamepad.scepad.led_brightness,    L"Force LED brightness [0,1,2,3] or -1 for No Override",      input_ini,       L"Input.libScePad",       L"LEDBrightness"),
    ConfigEntry (input.gamepad.scepad.
                                   enable_full_bluetooth,L"Allow SK to use all available features over Bluetooth",     input_ini,       L"Input.libScePad",       L"EnableFullBluetoothSupport"),
    ConfigEntry (input.gamepad.scepad.show_ds4_as_ds4_v2,L"Cause games to see DualShock 4 v1 as DualShock 4 v2",       dll_ini,         L"Input.libScePad",       L"IdentifyDualShock4AsDualShock4v2"),
    ConfigEntry (input.gamepad.scepad.hide_ds4_v2_pid,   L"Cause games to see DualShock 4 v2 as DualShock 4",          dll_ini,         L"Input.libScePad",       L"IdentifyDualShock4v2AsDualShock4"),
    ConfigEntry (input.gamepad.scepad.hide_ds_edge_pid,  L"Cause games to see DualSense Edge as DualSense",            dll_ini,         L"Input.libScePad",       L"IdentifyDualSenseEdgeAsDualSense"),
    ConfigEntry (input.gamepad.scepad.left_fn_bind,      L"Keyboard Input to Generate when Left Function is Pressed",  dll_ini,         L"Input.libScePad",       L"LeftFunction"),
    ConfigEntry (input.gamepad.scepad.right_fn_bind,     L"Keyboard Input to Generate when Right Function is Pressed", dll_ini,         L"Input.libScePad",       L"RightFunction"),
    ConfigEntry (input.gamepad.scepad.left_paddle_bind,  L"Keyboard Input to Generate when Left Paddle is Pressed",    dll_ini,         L"Input.libScePad",       L"LeftPaddle"),
    ConfigEntry (input.gamepad.scepad.right_paddle_bind, L"Keyboard Input to Generate when Right Paddle is Pressed",   dll_ini,         L"Input.libScePad",       L"RightPaddle"),
    ConfigEntry (input.gamepad.scepad.touch_click_bind,  L"Keyboard Input to Generate when Touch Pad is Clicked",      dll_ini,         L"Input.libScePad",       L"TouchpadClick"),

    ConfigEntry (input.gamepad.low_battery_warning,      L"Percentage when SK will warn controller batteries are low", input_ini,       L"Input.Battery",         L"WarnIfPercentIsBelow"),

 //DEPRECATED  (                                                                                                                       L"Input.XInput",          L"DisableRumble"),

    ConfigEntry (input.gamepad.steam.disable,            L"Disable Steam Input Always (only works for flat API)",      dll_ini,         L"Input.Steam",           L"Disable"),

    // Thread Monitoring
    //////////////////////////////////////////////////////////////////////////

    ConfigEntry (threads.enable_mem_alloc_trace,         L"Trace per-Thread Memory Allocation in Threads Widget",      dll_ini,         L"Threads.Analyze",       L"MemoryAllocation"),
    ConfigEntry (threads.enable_file_io_trace,           L"Trace per-Thread File I/O Activity in Threads Widget",      dll_ini,         L"Threads.Analyze",       L"FileActivity"),

    // Window Management
    //////////////////////////////////////////////////////////////////////////

    ConfigEntry (window.borderless,                      L"Borderless Window Mode",                                    dll_ini,         L"Window.System",         L"Borderless"),
    ConfigEntry (window.center,                          L"Center the Window",                                         dll_ini,         L"Window.System",         L"Center"),
    ConfigEntry (window.background_render,               L"Render While Window is in Background",                      dll_ini,         L"Window.System",         L"RenderInBackground"),
    ConfigEntry (window.background_mute,                 L"Mute While Window is in Background",                        dll_ini,         L"Window.System",         L"MuteInBackground"),
    ConfigEntry (window.offset.x,                        L"X Offset (Percent or Absolute)",                            dll_ini,         L"Window.System",         L"XOffset"),
    ConfigEntry (window.offset.y,                        L"Y Offset (Percent or Absolute)",                            dll_ini,         L"Window.System",         L"YOffset"),
    ConfigEntry (window.confine_cursor,                  L"Confine the Mouse Cursor to the Game Window",               dll_ini,         L"Window.System",         L"ConfineCursor"),
    ConfigEntry (window.unconfine_cursor,                L"Unconfine the Mouse Cursor from the Game Window",           dll_ini,         L"Window.System",         L"UnconfineCursor"),
    ConfigEntry (window.persistent_drag,                 L"Remember where the window is dragged to",                   dll_ini,         L"Window.System",         L"PersistentDragPos"),
    ConfigEntry (window.fullscreen,                      L"Make the Game Window Fill the Screen (scale to fit)",       dll_ini,         L"Window.System",         L"Fullscreen"),
    ConfigEntry (window.override,                        L"Force the Client Region to this Size in Windowed Mode",     dll_ini,         L"Window.System",         L"OverrideRes"),
    ConfigEntry (window.multi_monitor_mode,              L"Allow Resolution Overrides that Span Multiple Monitors",    dll_ini,         L"Window.System",         L"MultiMonitorMode"),
    ConfigEntry (window.fix_mouse_coords,                L"Re-Compute Mouse Coordinates for Resized Windows",          dll_ini,         L"Window.System",         L"FixMouseCoords"),
    ConfigEntry (window.always_on_top,                   L"Prevent (0) or Force (1) a game's window Always-On-Top",    dll_ini,         L"Window.System",         L"AlwaysOnTop"),
    ConfigEntry (window.disable_screensaver,             L"Prevent the Windows Screensaver from activating",           dll_ini,         L"Window.System",         L"DisableScreensaver"),
    ConfigEntry (window.fullscreen_no_saver,             L"Prevent the Windows Screensaver in (Borderless) Fullscreen",dll_ini,         L"Window.System",         L"DisableFullscreenSaver"),
    ConfigEntry (window.manage_screensaver,              L"Allow Screensaver to work, by Disabling any Game Overrides",dll_ini,         L"Window.System",         L"FullyManageScreenSaver"),
    ConfigEntry (window.preferred_monitor_id,            L"GDI Monitor ID of Preferred Monitor",                       dll_ini,         L"Window.System",         L"PreferredMonitor"),
    ConfigEntry (window.preferred_monitor_exact,         L"CCD Display Path (invariant) of Preferred Monitor",         dll_ini,         L"Window.System",         L"PreferredMonitorExact"),
    ConfigEntry (window.dont_hook_wndproc,               L"Disable WndProc / ClassProc hooks (wrap instead of hook)",  dll_ini,         L"Window.System",         L"DontHookWndProc"),
    ConfigEntry (window.activate_at_start,               L"Activate window after 15 frames (fixes games that think"
                                                         L" they are running in the background)",                      dll_ini,         L"Window.System",         L"ActivateAtStart"),
    ConfigEntry (window.treat_fg_as_active,              L"The game treats the foreground window (rather than focus),"
                                                         L" as the active application [for background render feature]",dll_ini,         L"Window.System",         L"TreatForegroundAsActive"),

    // Compatibility
    //////////////////////////////////////////////////////////////////////////

    ConfigEntry (compatibility.disable_nv_bloat,         L"Disable All NVIDIA BloatWare (GeForce Experience)",         dll_ini,         L"Compatibility.General", L"DisableBloatWare_NVIDIA"),
    ConfigEntry (compatibility.rehook_loadlibrary,       L"Rehook LoadLibrary When RTSS/Steam/ReShade hook it",        dll_ini,         L"Compatibility.General", L"RehookLoadLibrary"),
    ConfigEntry (compatibility.using_wine,               L"Disable Functionality Not Compatible With WINE",            dll_ini,         L"Compatibility.General", L"UsingWINE"),
    ConfigEntry (compatibility.allow_dxdiagn,            L"Disable Unnecessary DxDiagnostic BLOAT in Some Games",      dll_ini,         L"Compatibility.General", L"AllowDxDiagn"),
#ifdef _M_IX86
    ConfigEntry (compatibility.auto_large_address,       L"Opt-in for Automatic Large Address Aware Patch on Crash",   dll_ini,         L"Compatibility.General", L"AutoLargeAddressPatch"),
#endif
    ConfigEntry (compatibility.async_init,               L"Runs hook initialization on a separate thread; high safety",dll_ini,         L"Compatibility.General", L"AsyncInit"),
    ConfigEntry (compatibility.reshade_mode,             L"Initializes hooks in a way that ReShade will not interfere",dll_ini,         L"Compatibility.General", L"ReShadeMode"),
    ConfigEntry (compatibility.fsr3_mode,                L"Avoid hooks on CreateSwapChainForHwnd",                     dll_ini,         L"Compatibility.General", L"FSR3Mode"),
    ConfigEntry (compatibility.allow_fake_streamline,    L"Allow invalid stuff, that might let fake DLSS3 mods work.", dll_ini,         L"Compatibility.General", L"AllowFakeStreamline"),
    ConfigEntry (compatibility.sdl_sanity_level,         L"Set Default (1) or Override (2) SDL input/window behavior.",dll_ini,         L"Compatibility.General", L"SDLSanityLevel"),
    // Refer to SDL_hints.h, only the most useful options are exposed here...
    ConfigEntry (compatibility.sdl.allow_wgi,            L"SDL_JOYSTICK_WGI",                                          dll_ini,         L"Compatibility.SDL",     L"SDL_JOYSTICK_WGI"),
    ConfigEntry (compatibility.sdl.allow_raw_input,      L"SDL_JOYSTICK_RAWINPUT",                                     dll_ini,         L"Compatibility.SDL",     L"SDL_JOYSTICK_RAWINPUT"),
    ConfigEntry (compatibility.sdl.allow_direct_input,   L"SDL_DIRECTINPUT_ENABLED",                                   dll_ini,         L"Compatibility.SDL",     L"SDL_DIRECTINPUT_ENABLED"),
    ConfigEntry (compatibility.sdl.allow_xinput,         L"SDL_XINPUT_ENABLED",                                        dll_ini,         L"Compatibility.SDL",     L"SDL_XINPUT_ENABLED"),
    ConfigEntry (compatibility.sdl.allow_hid,            L"SDL_JOYSTICK_HIDAPI",                                       dll_ini,         L"Compatibility.SDL",     L"SDL_JOYSTICK_HIDAPI"),
    ConfigEntry (compatibility.sdl.
                                allow_all_ps_bt_features,L"SDL_JOYSTICK_HIDAPI_PS4_RUMBLE",                            dll_ini,         L"Compatibility.SDL",     L"FullPlayStationBluetoothSupport"),
    ConfigEntry (compatibility.sdl.switch_led_brightness,L"SDL_JOYSTICK_HIDAPI_JOYCON_HOME_LED",                       dll_ini,         L"Compatibility.SDL",     L"SDL_JOYSTICK_HIDAPI_JOYCON_HOME_LED"),
    ConfigEntry (compatibility.sdl.use_joystick_thread,  L"SDL_JOYSTICK_THREAD",                                       dll_ini,         L"Compatibility.SDL",     L"SDL_JOYSTICK_THREAD"),
    ConfigEntry (compatibility.sdl.poll_sentinel,        L"SDL_POLL_SENTINEL",                                         dll_ini,         L"Compatibility.SDL",     L"SDL_POLL_SENTINEL"),

    ConfigEntry (apis.last_known,                        L"Last Known Render API",                                     dll_ini,         L"API.Hook",              L"LastKnown"),

#ifdef _M_IX86
    ConfigEntry (apis.ddraw.hook,                        L"Enable DirectDraw Hooking",                                 dll_ini,         L"API.Hook",              L"ddraw"),
    ConfigEntry (apis.d3d8.hook,                         L"Enable Direct3D 8 Hooking",                                 dll_ini,         L"API.Hook",              L"d3d8"),
#endif

    ConfigEntry (apis.d3d9.hook,                         L"Enable Direct3D 9 Hooking",                                 dll_ini,         L"API.Hook",              L"d3d9"),
    ConfigEntry (apis.d3d9ex.hook,                       L"Enable Direct3D 9Ex Hooking",                               dll_ini,         L"API.Hook",              L"d3d9ex"),
    ConfigEntry (apis.dxvk9.enable,                      L"Enable Native DXVK (D3D9)",                                 dll_ini,         L"API.Hook",              L"dxvk9"),
    ConfigEntry (apis.d3d11.hook,                        L"Enable Direct3D 11 Hooking",                                dll_ini,         L"API.Hook",              L"d3d11"),
    ConfigEntry (apis.d3d12.hook,                        L"Enable Direct3D 12 Hooking",                                dll_ini,         L"API.Hook",              L"d3d12"),

#ifdef _M_AMD64
    ConfigEntry (apis.Vulkan.hook,                       L"Enable Vulkan Hooking",                                     dll_ini,         L"API.Hook",              L"Vulkan"),
#endif

    ConfigEntry (apis.OpenGL.hook,                       L"Enable OpenGL Hooking",                                     dll_ini,         L"API.Hook",              L"OpenGL"),
    ConfigEntry (apis.OpenGL.debug,                      L"Enable OpenGL Debugging",                                   dll_ini,         L"OpenGL.System",         L"EnableDebug"),

    // Misc.
    //////////////////////////////////////////////////////////////////////////

      // Hidden setting (it will be read, but not written -- setting this is discouraged and I intend to phase it out)
    ConfigEntry (mem_reserve,                            L"Memory Reserve Percentage",                                 dll_ini,         L"Manage.Memory",         L"ReservePercent"),


    // General Mod System Settings
    //////////////////////////////////////////////////////////////////////////

    ConfigEntry (silent,                                 L"Log Silence",                                               dll_ini,         L"SpecialK.System",       L"Silent"),
    ConfigEntry (strict_compliance,                      L"Strict DLL Loader Compliance",                              dll_ini,         L"SpecialK.System",       L"StrictCompliant"),
    ConfigEntry (trace_libraries,                        L"Trace DLL Loading (needed for dynamic API detection)",      dll_ini,         L"SpecialK.System",       L"TraceLoadLibrary"),
    ConfigEntry (log_level,                              L"Log Verbosity (0=General, 5=Insane Debug)",                 dll_ini,         L"SpecialK.System",       L"LogLevel"),
    ConfigEntry (handle_crashes,                         L"Use Custom Crash Handler",                                  dll_ini,         L"SpecialK.System",       L"UseCrashHandler"),
    ConfigEntry (silent_crash,                           L"Disable Crash Sound",                                       dll_ini,         L"SpecialK.System",       L"NoCrashSound"),
    ConfigEntry (crash_suppression,                      L"Try to Recover from Exceptions that Would Cause a Crash",   dll_ini,         L"SpecialK.System",       L"EnableCrashSuppression"),
    ConfigEntry (debug_wait,                             L"Halt Special K Initialization Until Debugger is Attached",  dll_ini,         L"SpecialK.System",       L"WaitForDebugger"),
    ConfigEntry (debug_output,                           L"Print Application's Debug Output in real-time",             dll_ini,         L"SpecialK.System",       L"DebugOutput"),
    ConfigEntry (game_output,                            L"Log Application's Debug Output",                            dll_ini,         L"SpecialK.System",       L"GameOutput"),
    ConfigEntry (init_delay,                             L"Delay Global Injection Initialization for x-many Seconds",  dll_ini,         L"SpecialK.System",       L"GlobalInjectDelay"),
    ConfigEntry (return_to_skif,                         L"At Application Exit, make SKIF the new Foreground Window",  dll_ini,         L"SpecialK.System",       L"ReturnToSKIF"),
    ConfigEntry (auto_load_asi_files,                    L"Automatically load .asi files from the game's directory",   dll_ini,         L"SpecialK.System",       L"AutoLoadASIFiles"),
    ConfigEntry (version,                                L"The last version that wrote the config file",               dll_ini,         L"SpecialK.System",       L"Version"),


    ConfigEntry (display.force_fullscreen,               L"Force Fullscreen Mode",                                     dll_ini,         L"Display.Output",        L"ForceFullscreen"),
    ConfigEntry (display.force_windowed,                 L"Force Windowed Mode",                                       dll_ini,         L"Display.Output",        L"ForceWindowed"),
    ConfigEntry (display.force_10bpc_sdr,                L"Force 10-bpc (SDR) Output",                                 dll_ini,         L"Display.Output",        L"Force10bpcSDR"),
    ConfigEntry (display.aspect_ratio_stretch,           L"Fill monitor background (eg. black bars) in windowed mode", dll_ini,         L"Display.Output",        L"AspectRatioStretch"),
    ConfigEntry (display.multimonitor_focus_mode,        L"Displays black background on all except the game's monitor",dll_ini,         L"Display.Output",        L"MultiMonitorADHDRelief"),
    ConfigEntry (display.multimonitor_focus_is_focused,  L"Whenever the game loses input focus, ADHD mode turns off",  osd_ini,         L"Display.Monitor",       L"MultiMonitorFocusIsFocused"),
    ConfigEntry (display.allow_refresh_change,           L"Allow Current Game to change Refresh Rate",                 dll_ini,         L"Display.Output",        L"AllowRefreshRateChanges"),


    // Framerate Limiter
    //////////////////////////////////////////////////////////////////////////

    ConfigEntry (render.framerate.target_fps,            L"Framerate Target (negative signed values are non-limiting)",dll_ini,         L"Render.FrameRate",      L"TargetFPS"),
    ConfigEntry (render.framerate.target_fps_bg,         L"Framerate Target (window in background;  0.0 = same as fg)",dll_ini,         L"Render.FrameRate",      L"BackgroundFPS"),
    ConfigEntry (render.framerate.last_refresh_rate,     L"Refresh rate the last time framerate limit was configured.",dll_ini,         L"Render.FrameRate",      L"LastRefreshRate"),
    ConfigEntry (render.framerate.last_monitor_path,     L"The monitor the last time framerate limit was configured.", dll_ini,         L"Render.FrameRate",      L"LastMonitorPath"),
    ConfigEntry (render.framerate.wait_for_vblank,       L"Limiter Will Wait for VBLANK",                              dll_ini,         L"Render.FrameRate",      L"WaitForVBLANK"),
    ConfigEntry (render.framerate.buffer_count,          L"Number of Backbuffers in the Swapchain",                    dll_ini,         L"Render.FrameRate",      L"BackBufferCount"),
    ConfigEntry (render.framerate.present_interval,      L"Presentation Interval (VSYNC)",                             dll_ini,         L"Render.FrameRate",      L"PresentationInterval"),
    ConfigEntry (render.framerate.sync_interval_clamp,   L"Maximum Sync Interval (Clamp VSYNC)",                       dll_ini,         L"Render.FrameRate",      L"SyncIntervalClamp"),
    ConfigEntry (render.framerate.tearing_mode,          L"Tearing Mode (Always On/Off or Adaptive)",                  dll_ini,         L"Render.FrameRate",      L"TearingMode"),
    ConfigEntry (render.framerate.prerender_limit,       L"Maximum Frames to Render-Ahead",                            dll_ini,         L"Render.FrameRate",      L"PreRenderLimit"),
    ConfigEntry (render.framerate.sleepless_render,      L"Sleep Free Render Thread",                                  dll_ini,         L"Render.FrameRate",      L"SleeplessRenderThread"),
    ConfigEntry (render.framerate.sleepless_window,      L"Sleep Free Window Thread",                                  dll_ini,         L"Render.FrameRate",      L"SleeplessWindowThread"),
    ConfigEntry (render.framerate.enable_mmcss,          L"Enable Multimedia Class Scheduling for FPS Limiter Sleep",  dll_ini,         L"Render.FrameRate",      L"EnableMMCSS"),
    ConfigEntry (render.framerate.refresh_rate,          L"Fullscreen Refresh Rate",                                   dll_ini,         L"Render.FrameRate",      L"RefreshRate"),
    ConfigEntry (render.framerate.rescan_ratio,          L"Fullscreen Rational Scan Rate (precise refresh rate)",      dll_ini,         L"Render.FrameRate",      L"RescanRatio"),

    ConfigEntry (render.framerate.enforcement_policy,    L"Place Framerate Limiter Wait Before/After Present, etc.",   dll_ini,         L"Render.FrameRate",      L"LimitEnforcementPolicy"),
    ConfigEntry (render.framerate.enable_etw_tracing,    L"Use ETW tracing (PresentMon) for extra latency/flip info",  dll_ini,         L"Render.FrameRate",      L"EnableETWTracing"),
    ConfigEntry (render.framerate.use_amd_mwaitx,        L"Use AMD Power-Saving Instructions for Busy-Wait",           dll_ini,         L"Render.FrameRate",      L"UseAMDMWAITX"),

    ConfigEntry (render.framerate.control.render_ahead,  L"Maximum number of CPU-side frames to work ahead of GPU.",   dll_ini,         L"FrameRate.Control",     L"MaxRenderAheadFrames"),
    ConfigEntry (render.framerate.override_cpu_count,    L"Number of CPU cores to tell the game about",                dll_ini,         L"FrameRate.Control",     L"OverrideCPUCoreCount"),
    ConfigEntry (render.framerate.latent_sync.offset,    L"Offset in Scanlines from Top of Screen to Steer Tearing",   dll_ini,         L"FrameRate.LatentSync",  L"TearlineOffset"),
    ConfigEntry (render.framerate.latent_sync.resync,    L"Frequency (in seconds) to Resync Timing",                   dll_ini,         L"FrameRate.LatentSync",  L"ResyncFrequency"),
    ConfigEntry (render.framerate.latent_sync.error,     L"Expected Error (in QPC ticks) of Refresh Rate Calculation", dll_ini,         L"FrameRate.LatentSync",  L"RoundingError"),
    ConfigEntry (render.framerate.latent_sync.bias,      L"Controls Distribution of Idle Time Per-Delayed Frame",      dll_ini,         L"FrameRate.LatentSync",  L"DelayBias"),
    ConfigEntry (render.framerate.latent_sync.auto_bias, L"Automatically Sets Delay Bias For Minimum Latency",         dll_ini,         L"FrameRate.LatentSync",  L"AutoBias"),
      ConfigEntry (render.framerate.latent_sync.
                                       auto_bias_target, L"Target input latency (in milliseconds or %) for auto-bias", dll_ini,         L"FrameRate.LatentSync",  L"AutoBiasTarget"),
    ConfigEntry (render.framerate.latent_sync.
                                          max_auto_bias, L"Maximum percentage to bias towards low input latency",      dll_ini,         L"FrameRate.LatentSync",  L"MaxAutoBias"),

    ConfigEntry (render.framerate.allow_dwm_tearing,     L"Enable DWM Tearing (Windows 10+)",                          dll_ini,         L"Render.DXGI",           L"AllowTearingInDWM"),
    ConfigEntry (render.framerate.drop_late_frames,      L"Enable Flip Model to Render (and drop) frames at rates >"
                                                         L"refresh rate with VSYNC enabled (similar to NV Fast Sync).",dll_ini,         L"Render.DXGI",           L"DropLateFrames"),
    ConfigEntry (render.framerate.auto_low_latency,      L"If G-Sync is seen supported, automatically optimize the"
                                                         L"limiter for low-latency.",                                  dll_ini,         L"Render.DXGI",           L"AutoLowLatency"),
    ConfigEntry (render.framerate.auto_vrr_triggered,    L"Indicates that Auto VRR activated and has turned off",      dll_ini,         L"Render.DXGI",           L"AutoLowLatencyTriggered"),
    ConfigEntry (render.framerate.auto_low_latency_ex,   L"Auto Low-Latency Mode may add stutter to get lower latency",input_ini,       L"Input.AutoLowLatency",  L"UltraLowLatency"),
    ConfigEntry (render.framerate.auto_low_latency_optin,L"Global policy applied when starting a game the first time", input_ini,       L"Input.AutoLowLatency",  L"DefaultPolicy"),
    ConfigEntry (render.framerate.
                                auto_low_latency_reapply,L"Global policy to reapply AutoVRR if display/refresh change",input_ini,       L"Input.AutoLowLatency",  L"AutoReapply"),

    ConfigEntry (nvidia.reflex.enable,                   L"Enable NVIDIA Reflex Integration w/ SK's limiter",          dll_ini,         L"NVIDIA.Reflex",         L"Enable"),
    ConfigEntry (nvidia.reflex.low_latency,              L"Low Latency Mode",                                          dll_ini,         L"NVIDIA.Reflex",         L"LowLatency"),
    ConfigEntry (nvidia.reflex.low_latency_boost,        L"Reflex Boost (lower-latency power scaling)",                dll_ini,         L"NVIDIA.Reflex",         L"LowLatencyBoost"),
    ConfigEntry (nvidia.reflex.marker_optimization,      L"Train Reflex using Latency Markers for Optimization",       dll_ini,         L"NVIDIA.Reflex",         L"OptimizeByMarkers"),
    ConfigEntry (nvidia.reflex.engagement_policy,        L"When to apply Reflex's magic",                              dll_ini,         L"NVIDIA.Reflex",         L"EngagementPolicy"),
    ConfigEntry (nvidia.reflex.override_native,          L"Use SK's Reflex Mode options instead of the game's",        dll_ini,         L"NVIDIA.Reflex",         L"OverrideNativeMode"),
    ConfigEntry (nvidia.reflex.use_limiter,              L"Use Reflex's framerate limiter (SK's target) instead of SK",dll_ini,         L"NVIDIA.Reflex",         L"UseFramerateLimiter"),
    ConfigEntry (nvidia.reflex.combined_limiter,         L"Use Reflex's framerate limiter (AND SK's)",                 dll_ini,         L"NVIDIA.Reflex",         L"CombineFramerateLimiters"),
    ConfigEntry (nvidia.reflex.disable_native,           L"Disable a game's native Reflex implementation",             dll_ini,         L"NVIDIA.Reflex",         L"DisableNative"),
    ConfigEntry (nvidia.reflex.show_detailed_widget,     L"Show detailed stage timing pipeline diagram on widget",     osd_ini,         L"NVIDIA.Reflex",         L"ShowDetailsInWidget"),

    ConfigEntry (nvidia.dlss.force_dlaa,                 L"Force DLAA in games that do not normally support it",       dll_ini,         L"NVIDIA.DLSS",           L"ForceDLAA"),
    ConfigEntry (nvidia.dlss.use_sharpening,             L"Override DLSS Sharpening Mode",                             dll_ini,         L"NVIDIA.DLSS",           L"UseSharpening"),
    ConfigEntry (nvidia.dlss.forced_sharpness,           L"Sharpness Value to Use",                                    dll_ini,         L"NVIDIA.DLSS",           L"ForcedSharpness"),
    ConfigEntry (nvidia.dlss.auto_redirect_dll,          L"Always load SK's Plug-In DLSS DLL instead of the game's",   dll_ini,         L"NVIDIA.DLSS",           L"AutoRedirectDLL"),
    ConfigEntry (nvidia.dlss.forced_preset,              L"Override DLSS Perf/Quality Level's Preset",                 dll_ini,         L"NVIDIA.DLSS",           L"ForcePreset"),
    ConfigEntry (nvidia.dlss.forced_auto_exposure,       L"Override DLSS Auto Exposure",                               dll_ini,         L"NVIDIA.DLSS",           L"ForcedAutoExposure"),
    ConfigEntry (nvidia.dlss.forced_alpha_upscale,       L"Override DLSS Alpha Upscaling (3.7.0+)",                    dll_ini,         L"NVIDIA.DLSS",           L"ForceAlphaUpscale"),
    ConfigEntry (nvidia.dlss.performance_scale,          L"Custom scale factor (if != 0.0f) to use for Performance",   dll_ini,         L"NVIDIA.DLSS",           L"CustomPerformanceScale"),
    ConfigEntry (nvidia.dlss.balanced_scale,             L"Custom scale factor (if != 0.0f) to use for Balanced",      dll_ini,         L"NVIDIA.DLSS",           L"CustomBalancedScale"),
    ConfigEntry (nvidia.dlss.quality_scale,              L"Custom scale factor (if != 0.0f) to use for Quality",       dll_ini,         L"NVIDIA.DLSS",           L"CustomQualityScale"),
    ConfigEntry (nvidia.dlss.ultra_performance_scale,    L"Custom scale factor (if != 0.0f) to use for Ultra Perf.",   dll_ini,         L"NVIDIA.DLSS",           L"CustomUltraPerfScale"),
    ConfigEntry (nvidia.dlss.dynamic_resolution_min,     L"Minimum Dynamic Resolution (scale) used by custom scales",  dll_ini,         L"NVIDIA.DLSS",           L"CustomMinDynamicRes"),
    ConfigEntry (nvidia.dlss.dynamic_resolution_max,     L"Maximum Dynamic Resolution (scale) used by custom scales",  dll_ini,         L"NVIDIA.DLSS",           L"CustomMaxDynamicRes"),
    ConfigEntry (nvidia.dlss.override_appid,             L"Spoof AppId for compatibility",                             dll_ini,         L"NVIDIA.DLSS",           L"OverrideAppId"),
    ConfigEntry (nvidia.dlss.extra_pixels,               L"Add extra pixels when forcing DLAA",                        dll_ini,         L"NVIDIA.DLSS",           L"ExtraPixelsForDLAA"),
    ConfigEntry (nvidia.dlss.disable_ota_updates,        L"Disable OTA updates (i.e. NVIDIA phone-home every launch)", dll_ini,         L"NVIDIA.DLSS",           L"DisableOTAUpdates"),
    ConfigEntry (nvidia.dlss.show_active_features,       L"Show the in-use features in the DLSS settings tab",         osd_ini,         L"NVIDIA.DLSS",           L"ShowActiveFeatures"),
    ConfigEntry (nvidia.dlss.allow_scrgb,                L"Allow scRGB even if DLSS-G DLLs are detected",              dll_ini,         L"NVIDIA.DLSS",           L"AllowSCRGBinDLSSG"),
    ConfigEntry (nvidia.dlss.spoof_feature_support,      L"Report all NGX (D3D11/D3D12) features supported on all HW.",dll_ini,         L"NVIDIA.DLSS",           L"SpoofFeatureSupport"),

    ConfigEntry (render.hdr.enable_32bpc,                L"Experimental - Use 32bpc for HDR",                          dll_ini,         L"SpecialK.HDR",          L"Enable128BitPipeline"),
    ConfigEntry (render.hdr.remaster_8bpc_as_unorm,      L"Do not use Floating-Point RTs when re-mastering 8-bpc+ RTs",dll_ini,         L"SpecialK.HDR",          L"Keep8BpcRemastersUNORM"),
    ConfigEntry (render.hdr.remaster_subnative_unorm,    L"Do not use FP RTs when re-mastering reduced resolution RTS",dll_ini,         L"SpecialK.HDR",          L"KeepSubnativeRemastersUNORM"),
    ConfigEntry (render.hdr.last_used_colorspace,        L"Last Used DXGI Colorspace; auto-enables HDR features...",   dll_ini,         L"SpecialK.HDR",          L"LastUsedColorSpace"),

    ConfigEntry (render.osd.draw_in_vidcap,              L"Changes hook order in order to allow recording the OSD.",   dll_ini,         L"Render.OSD",            L"ShowInVideoCapture"),

    // OpenGL
    //////////////////////////////////////////////////////////////////////////
    ConfigEntry (render.gl.enable_debug,                 L"Enable OpenGL Debug Output (in legacy OpenGL games)",       dll_ini,         L"OpenGL.System",         L"EnableDebug"),
    ConfigEntry (render.gl.prefer_10bpc,                 L"Use 10-bpc in SDR if the game's pixel type allows it",      dll_ini,         L"OpenGL.System",         L"Prefer10bpc"),
    ConfigEntry (render.gl.upgrade_zbuffer,              L"Use the most precise Z-buffer possible (usually 24-bit)",   dll_ini,         L"OpenGL.System",         L"UpgradeZBuffer"),

    // D3D9
    //////////////////////////////////////////////////////////////////////////

    ConfigEntry (render.d3d9.force_d3d9ex,               L"Force D3D9Ex Context",                                      dll_ini,         L"Render.D3D9",           L"ForceD3D9Ex"),
    ConfigEntry (render.d3d9.impure,                     L"Force PURE device off",                                     dll_ini,         L"Render.D3D9",           L"ForceImpure"),
    ConfigEntry (render.d3d9.enable_texture_mods,        L"Enable Texture Modding Support",                            dll_ini,         L"Render.D3D9",           L"EnableTextureMods"),
    ConfigEntry (render.d3d9.enable_flipex,              L"Enable D3D9Ex FlipEx SwapEffect",                           dll_ini,         L"Render.D3D9",           L"EnableFlipEx"),
    ConfigEntry (render.d3d9.use_d3d9on12,               L"Use D3D9On12 instead of normal driver",                     dll_ini,         L"Render.D3D9",           L"UseD3D9On12"),


    // D3D10/11/12
    //////////////////////////////////////////////////////////////////////////

    ConfigEntry (render.framerate.max_delta_time,        L"Maximum Frame Delta Time",                                  dll_ini,         L"Render.DXGI",           L"MaxDeltaTime"),
    ConfigEntry (render.framerate.flip_discard,          L"Use Flip Discard - Windows 10+",                            dll_ini,         L"Render.DXGI",           L"UseFlipDiscard"),
    ConfigEntry (render.framerate.flip_sequential,       L"Force Sequential (requires UseFlipDiscard or native Flip)", dll_ini,         L"Render.DXGI",           L"ForceFlipSequential"),
    ConfigEntry (render.framerate.disable_flip_model,    L"Disable Flip Model - Fix AMD Drivers in Yakuza0",           dll_ini,         L"Render.DXGI",           L"DisableFlipModel"),

    ConfigEntry (render.dxgi.adapter_override,           L"Override DXGI Adapter",                                     dll_ini,         L"Render.DXGI",           L"AdapterOverride"),
    ConfigEntry (render.dxgi.max_res,                    L"Maximum Resolution To Report",                              dll_ini,         L"Render.DXGI",           L"MaxRes"),
    ConfigEntry (render.dxgi.min_res,                    L"Minimum Resolution To Report",                              dll_ini,         L"Render.DXGI",           L"MinRes"),
    ConfigEntry (render.dxgi.max_refresh,                L"Maximum Refresh To Report",                                 dll_ini,         L"Render.DXGI",           L"MaxRefresh"),
    ConfigEntry (render.dxgi.min_refresh,                L"Minimum Refresh To Report",                                 dll_ini,         L"Render.DXGI",           L"MinRefresh"),

    ConfigEntry (render.dxgi.swapchain_wait,             L"Time to wait in msec. for SwapChain",                       dll_ini,         L"Render.DXGI",           L"SwapChainWait"),
    ConfigEntry (render.dxgi.scaling_mode,               L"Scaling Preference (DontCare | Centered | Stretched"
                                                         L" | Unspecified)",                                           dll_ini,         L"Render.DXGI",           L"Scaling"),
    ConfigEntry (render.dxgi.exception_mode,             L"D3D11 Exception Handling (DontCare | Raise | Ignore)",      dll_ini,         L"Render.DXGI",           L"ExceptionMode"),

    ConfigEntry (render.dxgi.debug_layer,                L"DXGI Debug Layer Support",                                  dll_ini,         L"Render.DXGI",           L"EnableDebugLayer"),
    ConfigEntry (render.dxgi.scanline_order,             L"Scanline Order (DontCare | Progressive | LowerFieldFirst |"
                                                         L" UpperFieldFirst )",                                        dll_ini,         L"Render.DXGI",           L"ScanlineOrder"),
    ConfigEntry (render.dxgi.rotation,                   L"Screen Rotation (DontCare | Identity | 90 | 180 | 270 )",   dll_ini,         L"Render.DXGI",           L"Rotation"),
    ConfigEntry (render.dxgi.test_present,               L"Test SwapChain Presentation Before Actually Presenting",    dll_ini,         L"Render.DXGI",           L"TestSwapChainPresent"),
    ConfigEntry (render.dxgi.enhanced_depth,             L"Use 32-bit Depth + 8-bit Stencil + 24-bit Padding",         dll_ini,         L"Render.DXGI",           L"Use64BitDepthStencil"),
    ConfigEntry (render.dxgi.deferred_isolation,         L"Isolate D3D11 Deferred Context Queues instead of Tracking"
                                                         L" in Immediate Mode.",                                       dll_ini,         L"Render.DXGI",           L"IsolateD3D11DeferredContexts"),
    ConfigEntry (render.dxgi.msaa_samples,               L"Override ON-SCREEN Multisample Antialiasing Level;-1=None", dll_ini,         L"Render.DXGI",           L"OverrideMSAA"),
    ConfigEntry (render.dxgi.skip_present_test,          L"Nix the swapchain present flag: DXGI_PRESENT_TEST to "
                                                         L"workaround bad third-party software that doesn't handle it"
                                                         L" correctly.",                                               dll_ini,         L"Render.DXGI",           L"SkipSwapChainPresentTest"),
    ConfigEntry (render.dxgi.srgb_behavior,              L"How to handle sRGB SwapChains when we have to kill them for"
                                                         L" Flip Model support (-1=Passthrough, 0=Strip, 1=Apply)",    dll_ini,         L"Render.DXGI",           L"sRGBBypassBehavior"),
    ConfigEntry (render.dxgi.low_spec_mode,              L"Disable D3D11 Render Mods (for slight perf. increase)",     dll_ini,         L"Render.DXGI",           L"LowSpecMode"),
    ConfigEntry (render.dxgi.hide_hdr_support,           L"Prevent games from detecting monitor HDR support",          dll_ini,         L"Render.DXGI",           L"HideHDRSupport"),
    ConfigEntry (render.dxgi.hdr_metadata_override,      L"Override the HDR header metadata type set by a game",       dll_ini,         L"Render.DXGI",           L"HDRMetadataType"),
    ConfigEntry (render.dxgi.enable_factory_cache,       L"Cache DXGI Factories to reduce display mode list overhead", dll_ini,         L"Render.DXGI",           L"UseFactoryCache"),
    ConfigEntry (render.dxgi.skip_redundant_modes,       L"Try to keep resolution setting changes to a minimum",       dll_ini,         L"Render.DXGI",           L"SkipRedundantModeChanges"),
    ConfigEntry (render.dxgi.temporary_dwm_hdr,          L"Temporarily Enable DWM-based HDR while the game runs",      dll_ini,         L"Render.DXGI",           L"TemporaryDesktopHDRMode"),
    ConfigEntry (render.dxgi.disable_virtual_vbi,        L"Disable Dynamic Refresh Rate (VBLANK Virtualization)",      dll_ini,         L"Render.DXGI",           L"DisableVirtualizedBlanking"),
    ConfigEntry (render.dxgi.clear_buffers_after_flip,   L"Clear the SwapChain Backbuffer every frame",                dll_ini,         L"Render.DXGI",           L"ClearFlipModelBackbuffers"),
    ConfigEntry (render.dxgi.warn_if_vram_exceeds,       L"Warn if VRAM used exceeds this % of available VRAM",        dll_ini,         L"Render.DXGI",           L"WarnIfUsedVRAMPercentExceeds"),
    ConfigEntry (render.dxgi.allow_d3d12_footguns,       L"Feel like shooting your foot with unsafe d3d12 settings..?",dll_ini,         L"Render.DXGI",           L"AllowD3D12FootGuns"),
    ConfigEntry (render.dxgi.fake_fullscreen_mode,       L"Lie to games and tell them they're in FSE, all the while, "
                                                         L"they are actually running a fullscreen borderless window.", dll_ini,         L"Render.DXGI",           L"FakeFullscreenMode"),
    ConfigEntry (render.dxgi.vram_budget_scale,          L"Multiplier for reported VRAM budget for D3D12 era engines.",dll_ini,         L"Render.DXGI",           L"VRAMBudgetScale"),

    ConfigEntry (render.d3d12.max_anisotropy,            L"Maximum Anisotropic Filter Level",                          dll_ini,         L"Render.D3D12",          L"MaxAnisotropy"),
    ConfigEntry (render.d3d12.force_anisotropic,         L"Forced Anisotropic Filtering",                              dll_ini,         L"Render.D3D12",          L"ForceAnisotropic"),
    ConfigEntry (render.d3d12.force_lod_bias,            L"Forced LOD Bias",                                           dll_ini,         L"Render.D3D12",          L"ForceLODBias"),

    ConfigEntry (render.dstorage.disable_bypass_io,      L"Disable DirectStorage BypassIO",                            dll_ini,         L"Render.DStorage",       L"DisableBypassIO"),
    ConfigEntry (render.dstorage.disable_telemetry,      L"Disable DirectStorage Telemetry",                           dll_ini,         L"Render.DStorage",       L"DisableTelemetry"),
    ConfigEntry (render.dstorage.disable_gpu_decomp,     L"Disable DirectStorage (1.2) GPU Decompression",             dll_ini,         L"Render.DStorage",       L"DisableGPUDecompression"),
    ConfigEntry (render.dstorage.force_file_buffering,   L"Force DirectStorage File Buffering",                        dll_ini,         L"Render.DStorage",       L"ForceFileBuffering"),
    ConfigEntry (render.dstorage.submit_threads,         L"Override default number of DirectStorage Submit threads",   dll_ini,         L"Render.DStorage",       L"NumberOfSubmitThreads"),
    ConfigEntry (render.dstorage.cpu_decomp_threads,     L"Override default number of CPU Decompression threads",      dll_ini,         L"Render.DStorage",       L"NumberOfCPUDecompThreads"),
    ConfigEntry (render.dstorage.hook_dstorage,          L"Hook DirectStorage for additional features",                dll_ini,         L"Render.DStorage",       L"EnableHooks"),

    ConfigEntry (texture.d3d9.clamp_lod_bias,            L"Clamp Negative LOD Bias",                                   dll_ini,         L"Textures.D3D9",         L"ClampNegativeLODBias"),
    ConfigEntry (texture.d3d11.cache,                    L"Cache Textures",                                            dll_ini,         L"Textures.D3D11",        L"Cache"),
    ConfigEntry (texture.d3d11.use_l3_hash,              L"Adds L3 to Hierarchical Cache;  L3=Fmt,  L2=Mips,  L1=Res", dll_ini,         L"Textures.D3D11",        L"CacheUsingL3Hash"),
    ConfigEntry (texture.d3d11.precise_hash,             L"Precise Hash Generation",                                   dll_ini,         L"Textures.D3D11",        L"PreciseHash"),

    ConfigEntry (texture.d3d11.inject,                   L"Inject Textures",                                           dll_ini,         L"Textures.D3D11",        L"Inject"),
    ConfigEntry (texture.d3d11.injection_keeps_format,   L"Allow image format to change during texture injection",     dll_ini,         L"Textures.D3D11",        L"InjectionKeepsFormat"),
    ConfigEntry (texture.d3d11.gen_mips,                 L"Create complete mipmap chain for textures without them",    dll_ini,         L"Textures.D3D11",        L"GenerateMipmaps"),
    ConfigEntry (texture.res_root,                       L"Resource Root",                                             dll_ini,         L"Textures.General",      L"ResourceRoot"),
    ConfigEntry (texture.dump_on_load,                   L"Dump Textures while Loading",                               dll_ini,         L"Textures.General",      L"DumpOnFirstLoad"),
    ConfigEntry (texture.cache.min_entries,              L"Minimum Cached Textures",                                   dll_ini,         L"Textures.Cache",        L"MinEntries"),
    ConfigEntry (texture.cache.max_entries,              L"Maximum Cached Textures",                                   dll_ini,         L"Textures.Cache",        L"MaxEntries"),
    ConfigEntry (texture.cache.min_evict,                L"Minimum Textures to Evict",                                 dll_ini,         L"Textures.Cache",        L"MinEvict"),
    ConfigEntry (texture.cache.max_evict,                L"Maximum Textures to Evict",                                 dll_ini,         L"Textures.Cache",        L"MaxEvict"),
    ConfigEntry (texture.cache.min_size,                 L"Minimum Data Size to Evict",                                dll_ini,         L"Textures.Cache",        L"MinSizeInMiB"),
    ConfigEntry (texture.cache.max_size,                 L"Maximum Data Size to Evict",                                dll_ini,         L"Textures.Cache",        L"MaxSizeInMiB"),

    ConfigEntry (texture.cache.ignore_non_mipped,        L"Ignore textures without mipmaps?",                          dll_ini,         L"Textures.Cache",        L"IgnoreNonMipmapped"),
    ConfigEntry (texture.cache.allow_staging,            L"Enable texture caching/dumping/injecting staged textures",  dll_ini,         L"Textures.Cache",        L"AllowStaging"),
    ConfigEntry (texture.cache.allow_unsafe_refs,        L"For games with broken resource reference counting, allow"
                                                         L" textures to be cached anyway (needed for injection).",     dll_ini,         L"Textures.Cache",        L"AllowUnsafeRefCounting"),
    ConfigEntry (texture.cache.manage_residency,         L"Actively manage D3D11 teture residency",                    dll_ini,         L"Textures.Cache",        L"ManageResidency"),


    ConfigEntry (nvidia.api.disable,                     L"Disable NvAPI",                                             dll_ini,         L"NVIDIA.API",            L"Disable"),
    ConfigEntry (nvidia.api.disable_hdr,                 L"Prevent Game from Using NvAPI HDR Features",                dll_ini,         L"NVIDIA.API",            L"DisableHDR"),
    ConfigEntry (nvidia.api.vulkan_bridge,               L"Enable/Disable Vulkan DXGI Layer",                          dll_ini,         L"NVIDIA.API",            L"EnableVulkanBridge"),
    ConfigEntry (nvidia.bugs.snuffed_ansel,              L"By default, Special K disables Ansel at first launch, but"
                                                         L" users have an option under 'Help|..' to turn it back on.", dll_ini,         L"NVIDIA.Bugs",           L"AnselSleepsWithFishes"),
    ConfigEntry (nvidia.bugs.bypass_ansel,               L"Forcefully block nvcamera{64}.dll",                         dll_ini,         L"NVIDIA.Bugs",           L"DisableAnselShimLoader"),
    ConfigEntry (nvidia.bugs.streamline_compat,          L"Alternate DXGI/D3D11/D3D12 Hook Implementation for NV BUG", dll_ini,         L"NVIDIA.Bugs",           L"StreamlineCompatibilityMode"),
    ConfigEntry (nvidia.sli.compatibility,               L"SLI Compatibility Bits",                                    dll_ini,         L"NVIDIA.SLI",            L"CompatibilityBits"),
    ConfigEntry (nvidia.sli.num_gpus,                    L"SLI GPU Count",                                             dll_ini,         L"NVIDIA.SLI",            L"NumberOfGPUs"),
    ConfigEntry (nvidia.sli.mode,                        L"SLI Mode",                                                  dll_ini,         L"NVIDIA.SLI",            L"Mode"),
    ConfigEntry (nvidia.sli.override,                    L"Override Driver Defaults",                                  dll_ini,         L"NVIDIA.SLI",            L"Override"),

    ConfigEntry (amd.adl.disable,                        L"Disable AMD's ADL library",                                 dll_ini,         L"AMD.ADL",               L"Disable"),
    ConfigEntry (microsoft.d3dkmt.disable_perfdata,      L"Disable Microsoft's D3DKMT Performance Data",               dll_ini,         L"Microsoft.D3DKMT",      L"DisablePerfData"),

    ConfigEntry (notifications.location,                 L"Corner to display notifications, 0=Top-Left,1=Top-Right,"
                                                         L"2=Bottom-Left,3=Bottom-Right",                              notify_ini,      L"Notification.System",   L"Location"),
    ConfigEntry (notifications.silent,                   L"Will not draw notifications until user requests them.",     notify_ini,      L"Notification.System",   L"Silent"),

    ConfigEntry (reshade_cfg.draw_first,                 L"Draw ReShade before SK's overlay in AddOn capable versions",dll_ini,         L"ReShade.System",        L"DrawFirst"),

    ConfigEntry (imgui.show_eula,                        L"Show Software EULA",                                        dll_ini,         L"SpecialK.System",       L"ShowEULA"),
    ConfigEntry (imgui.disable_alpha,                    L"Disable Alpha Transparency (reduce flicker)",               dll_ini,         L"ImGui.Render",          L"DisableAlpha"),
    ConfigEntry (imgui.antialias_lines,                  L"Reduce Aliasing on (but dim) Line Edges",                   dll_ini,         L"ImGui.Render",          L"AntialiasLines"),
    ConfigEntry (imgui.antialias_contours,               L"Reduce Aliasing on (but widen) Window Borders",             dll_ini,         L"ImGui.Render",          L"AntialiasContours"),

    ConfigEntry (dpi.disable,                            L"Disable DPI Scaling",                                       dll_ini,         L"DPI.Scaling",           L"Disable"),
    ConfigEntry (dpi.per_monitor_aware,                  L"Windows 8.1+ Monitor DPI Awareness (needed for UE4)",       dll_ini,         L"DPI.Scaling",           L"PerMonitorAware"),
    ConfigEntry (dpi.per_monitor_all_threads,            L"Further fixes required for UE4",                            dll_ini,         L"DPI.Scaling",           L"MonitorAwareOnAllThreads"),

    ConfigEntry (reverse_engineering.file.trace_reads,   L"Log file read activity to logs/file_read.log",              dll_ini,         L"FileIO.Trace",          L"LogReads"),
    ConfigEntry (reverse_engineering.file.ignore_reads,  L"Don't log activity for files in this list",                 dll_ini,         L"FileIO.Trace",          L"IgnoreReads"),
    ConfigEntry (reverse_engineering.file.trace_writes,  L"Log file write activity to logs/file_write.log",            dll_ini,         L"FileIO.Trace",          L"LogWrites"),
    ConfigEntry (reverse_engineering.file.ignore_writes, L"Don't log activity for files in this list",                 dll_ini,         L"FileIO.Trace",          L"IgnoreWrites"),

    ConfigEntry (cpu.power_scheme_guid,                  L"Power Policy (GUID) to Apply At Application Start",         dll_ini,         L"CPU.Power",             L"PowerSchemeGUID"),

    ConfigEntry (scheduling.priority.raise_always,       L"Always boost process priority to Above Normal",             dll_ini,         L"Scheduler.Boost",       L"AlwaysRaisePriority"),
    ConfigEntry (scheduling.priority.raise_in_bg,        L"Boost process priority to Above Normal in Background",      dll_ini,         L"Scheduler.Boost",       L"RaisePriorityInBackground"),
    ConfigEntry (scheduling.priority.raise_in_fg,        L"Boost process priority to Above Normal in Foreground",      dll_ini,         L"Scheduler.Boost",       L"RaisePriorityInForeground"),
    ConfigEntry (scheduling.priority.highest_priority,   L"Boost process priority to High instead of Above Normal",    dll_ini,         L"Scheduler.Boost",       L"RaisePriorityToHigh"),
    ConfigEntry (scheduling.priority.deny_foreign_change,L"Do not allow third-party apps to change priority",          dll_ini,         L"Scheduler.Boost",       L"DenyForeignChanges"),
    ConfigEntry (scheduling.priority.min_render_priority,L"Minimum priority for a game's render thread",               dll_ini,         L"Scheduler.Boost",       L"MinimumRenderThreadPriority"),
    ConfigEntry (scheduling.priority.cpu_affinity_mask,  L"Mask of CPU cores the process is eligible for scheduling.", dll_ini,         L"Scheduler.System",      L"ProcessorAffinityMask"),
    ConfigEntry (scheduling.priority.perf_cores_only,    L"Limit the scheduler to Intel P-Cores",                      dll_ini,         L"Scheduler.System",      L"PerformanceCoresOnly"),

    ConfigEntry (sound.minimize_latency,                 L"Minimize Audio Latency while Game is Running",              dll_ini,         L"Sound.Mixing",          L"MinimizeLatency"),

    // Control the behavior of SKIF rather than the other way around
    ConfigEntry (skif_autostop_behavior,                 L"Control when SKIF auto-stops, 0=Never, 1=AtStart, 2=AtExit",platform_ini,    L"SKIF.System",           L"AutoStopBehavior"),


    // The one odd-ball Steam achievement setting that can be specified per-game
    ConfigEntry (platform.achievements.sound_file,       L"Achievement Sound File",                                    dll_ini,         L"Platform.Achievements", L"SoundFile"),

    // Steam Achievement Enhancements  (Global Settings)
    //////////////////////////////////////////////////////////////////////////

    ConfigEntry (platform.achievements.play_sound,       L"Silence is Bliss?",                                         platform_ini,    L"Platform.Achievements", L"PlaySound"),
    ConfigEntry (platform.achievements.take_screenshot,  L"Precious Memories",                                         platform_ini,    L"Platform.Achievements", L"TakeScreenshot"),
    ConfigEntry (platform.achievements.
                                    fetch_friend_stats,  L"Friendly Competition",                                      platform_ini,    L"Platform.Achievements", L"FetchFriendStats"),
    ConfigEntry (platform.achievements.popup.origin,     L"Achievement Popup Position",                                platform_ini,    L"Platform.Achievements", L"PopupOrigin"),
    ConfigEntry (platform.achievements.popup.animate,    L"Achievement Notification Animation",                        platform_ini,    L"Platform.Achievements", L"AnimatePopup"),
    ConfigEntry (platform.achievements.popup.show_title, L"Achievement Popup Includes Game Title?",                    platform_ini,    L"Platform.Achievements", L"ShowPopupTitle"),
    ConfigEntry (platform.achievements.popup.inset,      L"Achievement Notification Inset X",                          platform_ini,    L"Platform.Achievements", L"PopupInset"),
    ConfigEntry (platform.achievements.popup.duration,   L"Achievement Popup Duration (in ms)",                        platform_ini,    L"Platform.Achievements", L"PopupDuration"),

    ConfigEntry (platform.system.notify_corner,          L"Overlay Notification Position  (non-Big Picture Mode)",     dll_ini,         L"Platform.System",       L"NotifyCorner"),
    ConfigEntry (platform.system.reuse_overlay_pause,    L"Pause Overlay Aware games when control panel is visible",   dll_ini,         L"Platform.System",       L"ReuseOverlayPause"),

    ConfigEntry (steam.system.appid,                     L"Steam AppID",                                               dll_ini,         L"Steam.System",          L"AppID"),
    ConfigEntry (steam.system.init_delay,                L"Delay SteamAPI initialization if the game doesn't do it",   dll_ini,         L"Steam.System",          L"AutoInitDelay"),
    ConfigEntry (steam.system.auto_pump,                 L"Should we force the game to run Steam callbacks?",          dll_ini,         L"Steam.System",          L"AutoPumpCallbacks"),
    ConfigEntry (steam.system.block_stat_callback,       L"Block the User Stats Receipt Callback?",                    dll_ini,         L"Steam.System",          L"BlockUserStatsCallback"),
    ConfigEntry (steam.system.filter_stat_callbacks,     L"Filter Unrelated Data from the User Stats Receipt Callback",dll_ini,         L"Steam.System",          L"FilterExternalDataFromCallbacks"),
    ConfigEntry (steam.system.load_early,                L"Load the Steam Client DLL Early?",                          dll_ini,         L"Steam.System",          L"PreLoadSteamClient"),
    ConfigEntry (steam.system.early_overlay,             L"Load the Steam Overlay Early",                              dll_ini,         L"Steam.System",          L"PreLoadSteamOverlay"),
    ConfigEntry (steam.system.force_load,                L"Forcefully load steam_api{64}.dll",                         dll_ini,         L"Steam.System",          L"ForceLoadSteamAPI"),
    ConfigEntry (steam.system.auto_inject,               L"Automatically load steam_api{64}.dll into any game whose "
                                                         L"path includes SteamApps\\common, but doesn't use steam_api",dll_ini,         L"Steam.System",          L"AutoInjectSteamAPI"),
    ConfigEntry (steam.social.online_status,             L"Always apply a social state (defined by EPersonaState) at"
                                                         L" application start",                                        dll_ini,         L"Steam.Social",          L"OnlineStatus"),
    ConfigEntry (steam.system.dll_path,                  L"Path to a known-working SteamAPI dll for this game.",       dll_ini,         L"Steam.System",          L"SteamPipeDLL"),
    ConfigEntry (steam.callbacks.throttle,               L"-1=Unlimited, 0-oo=Upper bound limit to SteamAPI rate",     dll_ini,         L"Steam.System",          L"CallbackThrottle"),
    ConfigEntry (platform.overlay.no_draw,               L"Disable Steam Overlay using 'SteamNoOverlayUIDrawing'",     dll_ini,         L"Steam.System",          L"NoDrawOverlay"),
    ConfigEntry (steam.system.crapcom_mode,              L"Special mode to workaround CAPCOM DRM without memory leaks",dll_ini,         L"Steam.System",          L"BypassCRAPCOM"),

    // This option is per-game, since it has potential compatibility issues...
    ConfigEntry (steam.screenshots.smart_capture,        L"Enhanced screenshot speed and HUD options; D3D11-only.",    dll_ini,         L"Steam.Screenshots",     L"EnableSmartCapture"),
    ConfigEntry (screenshots.include_osd_default,        L"Should a screenshot triggered BY Steam include SK's OSD?",  platform_ini,    L"Steam.Screenshots",     L"DefaultKeybindCapturesOSD"),

    ConfigEntry (eos.system.warned_online,               L"Has user been told about EOS incompatibility?",             dll_ini,         L"Platform.System",       L"WarnedEOSIncompat"),

    // These are all system-wide for all Steam games
    ConfigEntry (platform.overlay.hdr_luminance,         L"Make the Steam Overlay visible in HDR mode!",               platform_ini,    L"Platform.Overlay",      L"Luminance_scRGB"),

    // Swashbucklers pay attention
    //////////////////////////////////////////////////////////////////////////

    ConfigEntry (platform.log.silent,                    L"Makes steam_api.log go away [DISABLES STEAMAPI FEATURES]",  dll_ini,         L"Steam.Log",             L"Silent"),
    ConfigEntry (steam.cloud.blacklist,                  L"CSV list of files to block from cloud sync.",               dll_ini,         L"Steam.Cloud",           L"FilesNotToSync"),
    ConfigEntry (steam.drm.spoof_BLoggedOn,              L"Fix For Stupid Games That Don't Know How DRM Works",        dll_ini,         L"Steam.DRMWorks",        L"SpoofBLoggedOn"),
  };


  for ( auto& decl : params_to_build )
  {
    if ( decl.type_ == std::type_index ( typeid ( sk::ParameterBool* ) ) )
    {
      if (*decl.parameter_ == nullptr)
          *decl.parameter_ =
        SK_CreateINIParameter <sk::ParameterBool> (decl.description_, decl.ini_, decl.section_, decl.key_);

      continue;
    }

    if ( decl.type_ == std::type_index ( typeid ( sk::ParameterInt* ) ) )
    {
      if (*decl.parameter_ == nullptr)
          *decl.parameter_ =
        SK_CreateINIParameter <sk::ParameterInt> (decl.description_, decl.ini_, decl.section_, decl.key_);

      continue;
    }

    if ( decl.type_ == std::type_index ( typeid ( sk::ParameterInt64* ) ) )
    {
      if (*decl.parameter_ == nullptr)
          *decl.parameter_ =
        SK_CreateINIParameter <sk::ParameterInt64> (decl.description_, decl.ini_, decl.section_, decl.key_);

      continue;
    }

    if ( decl.type_ == std::type_index ( typeid ( sk::ParameterFloat* ) ) )
    {
      if (*decl.parameter_ == nullptr)
          *decl.parameter_ =
        SK_CreateINIParameter <sk::ParameterFloat> (decl.description_, decl.ini_, decl.section_, decl.key_);

      continue;
    }

    if ( decl.type_ == std::type_index ( typeid ( sk::ParameterStringW* ) ) )
    {
      if (*decl.parameter_ == nullptr)
          *decl.parameter_ =
        SK_CreateINIParameter <sk::ParameterStringW> (decl.description_, decl.ini_, decl.section_, decl.key_);

      continue;
    }

    if ( decl.type_ == std::type_index ( typeid ( sk::ParameterVec2f* ) ) )
    {
      if (*decl.parameter_ == nullptr)
          *decl.parameter_ =
        SK_CreateINIParameter <sk::ParameterVec2f> (decl.description_, decl.ini_, decl.section_, decl.key_);

      continue;
    }

    if ( decl.type_ == std::type_index ( typeid ( SK_ConfigSerializedKeybind* ) ) )
    {
      if (((SK_ConfigSerializedKeybind *)decl.parameter_)->param == nullptr)
          ((SK_ConfigSerializedKeybind *)decl.parameter_)->param =
        DeclKeybind ( (SK_ConfigSerializedKeybind *)decl.parameter_, decl.ini_, decl.section_ );

      continue;
    }
  }

  const iSK_INI::_TSectionMap& sections =
    dll_ini->get_sections ();

  auto sec =
    sections.cbegin ();

  int import_idx = 0;

  auto& host_executable =
    imports->host_executable;

  host_executable.hLibrary     = GetModuleHandle     (nullptr);
  host_executable.product_desc = SK_GetDLLVersionStr (SK_GetModuleFullName (host_executable.hLibrary).c_str ());


  SK_KeyboardMacros->clear   ();


  if (GetFileAttributesW (master_name.c_str ()) != INVALID_FILE_ATTRIBUTES)
  {
    dll_ini->import_file (master_name.c_str ());
  }

  if (GetFileAttributesW (custom_name.c_str ()) != INVALID_FILE_ATTRIBUTES)
  {
    dll_ini->import_file (custom_name.c_str ());
  }

  sec = sections.cbegin ();

  SK_RunOnce (
    while (sec != sections.cend ())
    {
      auto& import_ =
        imports->imports
           [import_idx];

      if (sec->first.find (L"Import.") != std::wstring::npos)
      {
        const wchar_t* wszNext =
          wcschr (sec->first.c_str (), L'.');

        if (StrStrIW (SK_CharNextW (wszNext), L"ReShade"))
        {
          config.compatibility.init_sync_for_reshade = true;
        }

        import_.name =
          wszNext != nullptr       ?
            SK_CharNextW (wszNext) : L"";

        import_.filename =
           dynamic_cast <sk::ParameterStringW *>
               (g_ParameterFactory->create_parameter <std::wstring> (
                  L"Import Filename")
               );
        import_.filename->register_to_ini (
          dll_ini,
            sec->first,
              L"Filename" );

        import_.when =
           dynamic_cast <sk::ParameterStringW *>
               (g_ParameterFactory->create_parameter <std::wstring> (
                  L"Import Timeframe")
               );
        import_.when->register_to_ini (
          dll_ini,
            sec->first,
              L"When" );

        import_.role =
           dynamic_cast <sk::ParameterStringW *>
               (g_ParameterFactory->create_parameter <std::wstring> (
                  L"Import Role")
               );
        import_.role->register_to_ini (
          dll_ini,
            sec->first,
              L"Role" );

        import_.architecture =
           dynamic_cast <sk::ParameterStringW *>
               (g_ParameterFactory->create_parameter <std::wstring> (
                  L"Import Architecture")
               );
        import_.architecture->register_to_ini (
          dll_ini,
            sec->first,
              L"Architecture" );

        import_.blacklist =
           dynamic_cast <sk::ParameterStringW *>
               (g_ParameterFactory->create_parameter <std::wstring> (
                  L"Blacklisted Executables")
               );
        import_.blacklist->register_to_ini (
          dll_ini,
            sec->first,
              L"Blacklist" );

        import_.mode =
           dynamic_cast <sk::ParameterStringW *>
               (g_ParameterFactory->create_parameter <std::wstring> (
                  L"Plug-In Mode (application defined)")
               );
        import_.mode->register_to_ini (
          dll_ini,
            sec->first,
              L"Mode" );

        static_cast <sk::iParameter *> (import_.filename    )->load ();
        static_cast <sk::iParameter *> (import_.when        )->load ();
        static_cast <sk::iParameter *> (import_.blacklist   )->load ();
        static_cast <sk::iParameter *> (import_.mode        )->load ();

        if (! static_cast <sk::iParameter *> (import_.role        )->load ())
                                              import_.role->set_value         (L"Any");
        if (! static_cast <sk::iParameter *> (import_.architecture)->load ())
                                              import_.architecture->set_value (L"Any");

        import_.hLibrary = nullptr;

        if (++import_idx > SK_MAX_IMPORTS)
          break;
      }

      ++sec;
    }
  );

  sec = sections.cbegin ();

  while (sec != sections.cend ())
  {
    if (sec->first.find (L"Macro.") != std::wstring::npos)
    {
      for ( auto &[key_name, command] : sec->second.keys )
      {
        SK_KeyCommand
          cmd = { .binding = { .human_readable = key_name },
                  .command =                      command };

          cmd.binding.parse ();

        SK_KeyboardMacros->emplace (
                     cmd.binding.masked_code,
          std::move (cmd)
        );
      }
    }

    ++sec;
  }



  //
  // Load Global Macro Table
  //
  if (macro_ini->get_sections ().empty ())
  {
    macro_ini->import ( L"[Macro.SpecialK_OSD_Toggles]\n"
                        L"Ctrl+Shift+O=OSD.Show toggle\n"
                        L"Ctrl+Shift+M=OSD.Memory.Show toggle\n"
                        L"Ctrl+Shift+T=OSD.Clock.Show toggle\n"
                        L"Ctrl+Shift+I=OSD.IOPS.Show toggle\n"
                        L"Ctrl+Shift+F=OSD.FPS.Show toggle\n"
                        L"Ctrl+Shift+G=OSD.GPU.Show toggle\n"
                        L"Ctrl+Shift+R=OSD.Shaders.Show toggle\n"
                        L"Ctrl+Alt+Shift+D=OSD.Disk.Show toggle\n"
                        L"Ctrl+Alt+Shift+P=OSD.Pagefile.Show toggle\n"
                        L"Ctrl+Alt+Shift+S=OSD.SLI.Show toggle\n"
                        L"Ctrl+Shift+C=OSD.CPU.Show toggle\n"
                        L"Alt+Shift+D=OSD.DLSS.Show toggle\n\n"
                      );
                        //L"[Macro.SpecialK_CommandConsole]\n"
                        //L"Ctrl+Shift+Tab=Console.Show toggle\n\n" );
    macro_ini->write ();
  }

  auto& macro_sections =
    macro_ini->get_sections ();

  sec =
    macro_sections.begin ();

  while (sec != macro_sections.cend ())
  {
    for ( auto &[key_name, command] : sec->second.keys )
    {
      SK_KeyCommand
        cmd = { .binding = { .human_readable = key_name },
                .command =                      command };

        cmd.binding.parse ();

      SK_KeyboardMacros->emplace (
                   cmd.binding.masked_code,
        std::move (cmd)
      );
    }

    ++sec;
  }


  config.window.border_override    = true;


                                            //
  config.system.trace_load_library = true;  // Generally safe even with the
                                            //   worst third-party software;
                                            //
                                            //  NEEDED for injector API detect


  config.system.strict_compliance  = false; // Will deadlock in DLLs that call
                                            //   LoadLibrary from DllMain
                                            //
                                            //  * NVIDIA Ansel, MSI Nahimic,
                                            //      Razer *, RTSS (Sometimes)
                                            //

  // Default = Don't Care
  config.render.dxgi.exception_mode = SK_NoPreference;
  config.render.dxgi.scaling_mode   = SK_NoPreference;


  //
  // Application Compatibility Overrides
  // ===================================
  //
  if (SK_GetCurrentGameID () != SK_GAME_ID::UNKNOWN_GAME)
  {
    switch (SK_GetCurrentGameID ())
    {
      case SK_GAME_ID::Tyranny:
        // Cannot auto-detect API?!
        config.apis.dxgi.d3d11.hook       = false;
        config.apis.OpenGL.hook           = false;
        config.steam.filter_stat_callback = true; // Will stop running SteamAPI when it receives
                                                  //   data it didn't ask for
        break;


      case SK_GAME_ID::TidesOfNumenera:
        // API Auto-Detect Broken (0.7.43)
        //
        //   => Auto-Detection Thinks Game is OpenGL
        //
        config.apis.d3d9.hook       = true;
        config.apis.d3d9ex.hook     = false;
        config.apis.dxgi.d3d11.hook = false;
        config.apis.OpenGL.hook     = false;
        break;


      case SK_GAME_ID::MassEffect_Andromeda:
        // Disable Exception Handling Instead of Crashing at Shutdown
        config.render.dxgi.exception_mode      = D3D11_RAISE_FLAG_DRIVER_INTERNAL_ERROR;

        // Not a Steam game :(
        config.platform.silent                 = true;

        config.system.strict_compliance        = false; // Uses NVIDIA Ansel, so this won't work!

        config.apis.d3d9.hook                  = false;
        config.apis.d3d9ex.hook                = false;
        config.apis.OpenGL.hook                = false;

        config.input.ui.capture_hidden         = false; // Mouselook is a bitch

        config.textures.d3d11.cache            = true;
        config.textures.cache.ignore_nonmipped = true;
        config.textures.cache.max_size         = 4096;
        break;


      case SK_GAME_ID::MadMax:
        break;


      case SK_GAME_ID::Dreamfall_Chapters:
        config.system.trace_load_library       = true;
        config.system.strict_compliance        = false;

        // Chances are good that we will not catch SteamAPI early enough to hook callbacks, so
        //   auto-pump.
        config.steam.auto_pump_callbacks       = true;
        config.steam.preload_client            = true;
        config.steam.filter_stat_callback      = true; // Will stop running SteamAPI when it receives
                                                    //   data it didn't ask for

        config.apis.dxgi.d3d11.hook            = true;
        config.apis.d3d9.hook                  = true;
        config.apis.d3d9ex.hook                = true;
        config.apis.OpenGL.hook                = false;
        break;


      case SK_GAME_ID::TheWitness:
        config.system.trace_load_library    = true;
        config.system.strict_compliance     = false; // Uses Ansel

        config.render.d3d11.track_map_and_unmap = false;

        // Game has mouselook problems without this
        config.input.ui.capture_mouse       = true;
        break;


      case SK_GAME_ID::ResidentEvil7:
      case SK_GAME_ID::Obduction:
        config.system.trace_load_library = true;  // Need to catch SteamAPI DLL load
        config.system.strict_compliance  = false; // Cannot block threads while loading DLLs
                                                  //   (uses an incorrectly written DLL)
        break;


      case SK_GAME_ID::TheWitcher3:
        config.system.strict_compliance   = false; // Uses NVIDIA Ansel, so this won't work!
        config.steam.filter_stat_callback = true;  // Will stop running SteamAPI when it receives
                                                   //   data it didn't ask for

        config.apis.d3d9.hook             = false;
        config.apis.d3d9ex.hook           = false;
        config.apis.OpenGL.hook           = false;

        config.textures.cache.ignore_nonmipped = true; // Invalid use of immutable textures
        break;

      case SK_GAME_ID::KingdomComeDeliverance:
        config.textures.cache.ignore_nonmipped = true;
        config.textures.d3d11.cache            = false; // Fix grass artifacts
        break;

      case SK_GAME_ID::DragonsDogma2:
        config.nvidia.dlss.compat.extra_pixels = -2;
        break;

      case SK_GAME_ID::DragonsDogma:
        // BROKEN (by GeForce Experience)
        //
        // TODO: Debug the EXACT cause of NVIDIA's Deadlock
        //
        config.compatibility.disable_nv_bloat = true;  // PREVENT DEADLOCK CAUSED BY NVIDIA!

        config.system.trace_load_library      = true;  // Need to catch NVIDIA Bloat DLLs
        config.system.strict_compliance       = false; // Cannot block threads while loading DLLs
                                                       //   (uses an incorrectly written DLL)

        config.steam.auto_pump_callbacks      = false;
        config.steam.preload_client           = true;

        config.apis.d3d9.hook                 = true;
        config.apis.dxgi.d3d11.hook           = false;
        config.apis.d3d9ex.hook               = false;
        config.apis.OpenGL.hook               = false;
        config.window.dont_hook_wndproc       = false;
        window.dont_hook_wndproc->store (config.window.dont_hook_wndproc);
        break;


      case SK_GAME_ID::EverQuest:
        // Fix-up rare issues during Server Select -> Game
        //config.compatibility.d3d9.rehook_reset = true;
        break;


      case SK_GAME_ID::GodEater3:
        //Does not support XInput hot-plugging, needs Special K loving :)
        config.input.gamepad.xinput.placehold [0] = true;
        break;

      case SK_GAME_ID::GodEater2RageBurst:
        //Does not support XInput hot-plugging, needs Special K loving :)
        config.input.gamepad.xinput.placehold [0] = true;

        config.apis.d3d9.hook                     = true;
        config.apis.d3d9ex.hook                   = true;
        config.apis.dxgi.d3d11.hook               = false;
        config.apis.OpenGL.hook                   = false;
        break;


      case SK_GAME_ID::WatchDogs2:
        //Does not support XInput hot-plugging, needs Special K loving :)
        config.input.gamepad.xinput.placehold [0] = true;
        break;


      case SK_GAME_ID::NieRAutomata:
        // Maximize compatibility with 3rd party injectors that corrupt hooks
        //config.render.dxgi.slow_state_cache    = false;
        //SK_DXGI_SlowStateCache                 = config.render.dxgi.slow_state_cache;
        config.render.dxgi.scaling_mode         = DXGI_MODE_SCALING_UNSPECIFIED;
        config.render.dxgi.fake_fullscreen_mode = true;
        config.window.background_render         = true;

        // Prevent VRR disable when game plays cutscenes
        config.render.framerate.sync_interval_clamp  =     1;
        break;


      case SK_GAME_ID::Warframe_x64:
        config.apis.d3d9.hook       = false;
        config.apis.d3d9ex.hook     = false;
        config.apis.dxgi.d3d11.hook = true;
        break;


      case SK_GAME_ID::LEGOCityUndercover:
        // Prevent the game from deadlocking its message pump in fullscreen
        config.render.dxgi.safe_fullscreen       = true;
        config.render.framerate.sleepless_window = true; // Fix framerate limiter
        break;


      case SK_GAME_ID::Sacred2:
        config.display.force_windowed      = true; // Fullscreen is not particularly well
                                                   //   supported in this game
        [[fallthrough]];
      case SK_GAME_ID::Sacred:
        config.render.dxgi.safe_fullscreen = true; // dgVoodoo compat
        config.steam.force_load_steamapi   = true; // Not safe in all games, but it is here.
        break;


      case SK_GAME_ID::FinalFantasy9:
        // Don't auto-pump callbacks
        config.steam.auto_pump_callbacks = false;
        config.apis.OpenGL.hook          = false; // Not an OpenGL game, API auto-detect is borked
        break;


      case SK_GAME_ID::EdithFinch:
        config.render.framerate.sleepless_window = true;
        break;


      case SK_GAME_ID::FinalFantasyX_X2:
        // Don't auto-pump callbacks
        //  Excessively lengthy startup is followed by actual SteamAPI init eventually...
        config.steam.auto_pump_callbacks = false;
        break;

#ifdef _M_IX86
      case SK_GAME_ID::DeadlyPremonition:
        config.steam.force_load_steamapi       = true;
        config.apis.d3d9.hook                  = true;
        config.apis.d3d9ex.hook                = false;
        config.apis.d3d8.hook                  = false;
        break;
#endif

#ifdef _M_AMD64
      case SK_GAME_ID::LifeIsStrange_BeforeTheStorm:
        config.apis.d3d9.hook       = false;
        config.apis.d3d9ex.hook     = false;
        config.apis.OpenGL.hook     = false;
        config.apis.Vulkan.hook     = false;
        config.apis.dxgi.d3d11.hook = true;
        config.apis.dxgi.d3d12.hook = false;
        break;

      case SK_GAME_ID::DivinityOriginalSin:
        config.textures.cache.ignore_nonmipped  = true;
        config.render.d3d11.track_map_and_unmap = false;
        break;
#endif

      case SK_GAME_ID::Hob:
        // Fails to start Steamworks correctly, we have to kickstart it
        config.steam.force_load_steamapi = true;
        break;

#ifdef _M_IX86
      case SK_GAME_ID::DukeNukemForever:
        // The mouse cursor's coordinate space is limited to 1920x1080 even at 4K, which
        //   has the unfortunate side effect of reducing aiming precision when the game
        //     isn't using HID.
        config.window.unconfine_cursor = true; // Remap the coordinates and increase precision

        // The graphics engine doesn't import any render APIs directly aside from ddraw.dll,
        //   which is obviously not the correct API. D3D9 and only D3D9 is the correct API.
        config.apis.d3d9.hook          = true;
        config.apis.ddraw.hook         = false;
        config.apis.d3d8.hook          = false;
        config.apis.dxgi.d3d11.hook    = false;
        config.apis.OpenGL.hook        = false;
        break;
#endif

      case SK_GAME_ID::DotHackGU:
      //config.textures.d3d11.generate_mips = true;
        break;

      case SK_GAME_ID::WorldOfFinalFantasy:
      {
        config.window.borderless                 = true;
        config.window.fullscreen                 = true;
        config.window.offset.x.absolute          = SK_NoPreference;
        config.window.offset.y.absolute          = SK_NoPreference;
        config.window.center                     = false;
        config.render.framerate.buffer_count     =  3;
        config.render.framerate.target_fps       = -30.0f;
        config.render.framerate.flip_discard     = true;
        config.render.framerate.present_interval = 2;
        config.render.framerate.pre_render_limit = 2;
        config.render.framerate.sleepless_window = false;
        config.input.cursor.manage               = true;
        config.input.cursor.timeout              = 0;

        HMONITOR hMonitor =
          MonitorFromWindow ( HWND_DESKTOP,
                                MONITOR_DEFAULTTOPRIMARY );

        MONITORINFO mi   = {         };
        mi.cbSize        = sizeof (mi);
        GetMonitorInfo (hMonitor, &mi);

        config.window.res.override.x = mi.rcMonitor.right  - mi.rcMonitor.left;
        config.window.res.override.y = mi.rcMonitor.bottom - mi.rcMonitor.top;
      } break;

      case SK_GAME_ID::StarOcean4:
        // Prevent the game from layering windows always on top.
        config.window.always_on_top             = PreventAlwaysOnTop;
        config.window.disable_screensaver       = true;
        config.textures.d3d11.uncompressed_mips = true;
        config.textures.d3d11.cache_gen_mips    = false;
        break;

      case SK_GAME_ID::Okami:
        config.render.dxgi.deferred_isolation   = true;
        break;

      case SK_GAME_ID::Mafia3:
        config.steam.force_load_steamapi        = true;
        break;

      // (0.8.58 - 1/1/18) -> API autodetect doesn't like XNA games
      case SK_GAME_ID::Owlboy:
        config.apis.d3d9.hook       = true;
        config.apis.d3d9ex.hook     = true;
        config.apis.OpenGL.hook     = false;
        config.apis.dxgi.d3d11.hook = false;
        break;

      // (0.9.1 - 2/15/18) -> SteamAPI init happens before CBT hook injects SpecialK
      case SK_GAME_ID::SecretOfMana:
        config.steam.init_delay = 1; // Non-zero value causes forced injection
        break;

      case SK_GAME_ID::FinalFantasyXV:
        // On many systems, people have third-party software that is behaving
        //   incorrectly when the game issues DXGI_PRESENT_TEST; so disable
        //     this feature to improve performance and compat.
        config.render.dxgi.present_test_skip  = false;
        config.render.dxgi.deferred_isolation = false;

        // Replace sleep calls that would normally block the message queue with
        //   calls that wakeup and dispatch these events.
        config.render.framerate.sleepless_window = false;

        config.textures.d3d11.cache           = true;
        config.textures.cache.max_entries     = 16384; // Uses a ton of small textures

        // Don't show the cursor, ever, because the game doesn't use it.
        config.input.cursor.manage            = true;
        config.input.cursor.timeout           = 0;

        config.threads.enable_file_io_trace   = true;

        config.steam.preload_overlay          = true;
        config.render.framerate.flip_discard  = false;

        SK_D3D11_DeclHUDShader_Vtx (0x3be1c239);
        SK_D3D11_DeclHUDShader_Vtx (0x466e477c);
        SK_D3D11_DeclHUDShader_Vtx (0x588dea7a);
        SK_D3D11_DeclHUDShader_Vtx (0xf15a90ab);

        InterlockedExchange (&SK_SteamAPI_CallbackRateLimit, 10);
        break;

      case SK_GAME_ID::NiNoKuni2:
      {
        // Game will break itself if it sees a DualSense controller using DirectInput
        config.input.gamepad.dinput.blackout_gamepads  = true;
        config.input.gamepad.dinput.block_enum_devices = true;
        config.render.dxgi.fake_fullscreen_mode = true; // For HDR to work
        config.window.treat_fg_as_active        = true;
        config.window.background_render         = true;

        uintptr_t hdr_branch_addr =
          (uintptr_t)SK_Debug_GetImageBaseAddr () + 0x469c4e;

        if (SK_IsAddressExecutable ((void *)hdr_branch_addr, true))
        {
          // Unconditional jump needed, or the game will infinitely set HDR mode
          if (*(uint8_t *)hdr_branch_addr == 0x74)
          {
            SK_LOGi0 (L"Patching NiNoKuni2's NvAPI HDR Bug...");

            DWORD dwOrigProt =                              PAGE_EXECUTE_READ;
            if (VirtualProtect ((LPVOID)hdr_branch_addr, 1, PAGE_EXECUTE_READWRITE, &dwOrigProt))
            {               *(uint8_t *)hdr_branch_addr = 0xeb;
                VirtualProtect ((LPVOID)hdr_branch_addr, 1, dwOrigProt,             &dwOrigProt);
            }
          }
        }

        // Evaluate deferred command lists for state tracking
        config.render.dxgi.deferred_isolation   = true;
      } break;

      case SK_GAME_ID::PillarsOfEternity2:
        config.textures.cache.ignore_nonmipped    = true;
        config.textures.cache.residency_managemnt = false;
        config.apis.OpenGL.hook                   = false;
        config.steam.appid                        = 560130;
        config.steam.auto_pump_callbacks          = true;
        config.steam.auto_inject                  = true;
        config.steam.force_load_steamapi          = true;
        config.steam.dll_path                     = LR"(PillarsOfEternityII_Data\Plugins\steam_api64.dll)";
        break;

      case SK_GAME_ID::DragonBallFighterZ:
        WinExec (R"(RED\Binaries\Win64\RED-Win64-Shipping.exe)", SW_SHOWNORMAL);
        exit (0);
        break;

#ifdef _M_AMD64
      case SK_GAME_ID::FarCry5:
      {
        // Game shares buggy XInput code with Watch_Dogs2
        config.input.gamepad.xinput.placehold [0] = true;
      } break;
      case SK_GAME_ID::FarCry4:
      {
        // It is not possible to use flip model in this game due to dxdiagn,
        //   Windows 11 can successfully upgrade the game however.
        config.render.framerate.disable_flip =  true;
        config.render.framerate.flip_discard = false;
      } break;
#endif

      case SK_GAME_ID::ChronoTrigger:
        // Don't accidentally hook the D3D9 device used for video playback
        config.apis.d3d9.hook   = false;
        config.apis.d3d9ex.hook = false;
        break;

      case SK_GAME_ID::Ys_Eight:
        config.textures.d3d11.uncompressed_mips = true;
        config.textures.d3d11.cache_gen_mips    = false;
        break;

      case SK_GAME_ID::TrailsOfColdSteel:
        SK_D3D11_DeclHUDShader_Vtx (0x497dc49d);
        SK_D3D11_DeclHUDShader_Vtx (0x671ca0fa);
        SK_D3D11_DeclHUDShader_Vtx (0x75cf58de);
        break;

      case SK_GAME_ID::Sekiro:
        config.window.activate_at_start           = true;
        config.render.dxgi.fake_fullscreen_mode   = true;
        config.window.background_render           = true;
        config.input.gamepad.xinput.placehold [0] = true;
        config.input.gamepad.xinput.placehold [1] = true;
        config.input.gamepad.xinput.placehold [2] = true;
        config.input.gamepad.xinput.placehold [3] = true;
        config.render.framerate.buffer_count      = 3;
        config.render.framerate.pre_render_limit  = 4;

        SK_D3D11_DeclHUDShader_Vtx (0x15888ef2);
        SK_D3D11_DeclHUDShader_Vtx (0x1893edbd);
        SK_D3D11_DeclHUDShader_Vtx (0x26dc61b1);
        SK_D3D11_DeclHUDShader_Vtx (0x3d205776);
        SK_D3D11_DeclHUDShader_Vtx (0x4c8dd6ec);
        SK_D3D11_DeclHUDShader_Vtx (0x5e74f0b4);
        SK_D3D11_DeclHUDShader_Vtx (0xc4ee4c93);
        SK_D3D11_DeclHUDShader_Vtx (0xd931a68c);
        SK_D3D11_DeclHUDShader_Vtx (0xdb921b64);
        SK_D3D11_DeclHUDShader_Vtx (0xe15a43f4);
        SK_D3D11_DeclHUDShader_Vtx (0xf497bad8);
        SK_D3D11_DeclHUDShader_Vtx (0x6fd3fed7);
        config.render.dxgi.deferred_isolation = true;

        // ReShade will totally crash if it is permitted to hook D3D9
        config.apis.d3d9.hook                 = false;
        config.apis.d3d9ex.hook               = false;

        apis.d3d9.hook->store   (config.apis.d3d9.  hook);
        apis.d3d9ex.hook->store (config.apis.d3d9ex.hook);
        apis.OpenGL.hook->store (config.apis.OpenGL.hook);
        break;

      case SK_GAME_ID::SonicMania:
      {
        config.apis.d3d9ex.hook = false;
      } break;

      case SK_GAME_ID::Tales_of_Zestiria:
        config.window.activate_at_start = true;
        break;

      case SK_GAME_ID::BaldursGate3:
      {
        // Game has native support for DualSense, but not DualSense Edge
        config.input.gamepad.scepad.hide_ds_edge_pid = true;
        config.input.gamepad.xinput.emulate          = false;

        // The Vulkan executable is simply bg3.exe,
        //   D3D11 is bg3_dx11.exe
        bool bVulkan =
          StrStrIW (SK_GetHostApp (), L"bg3.exe");

        if (bVulkan)
        {
          config.apis.NvAPI.vulkan_bridge = 1;
        }

        else
        {
          SK_D3D11_DeclHUDShader_Vtx (0x5dcbb1c5);
          SK_D3D11_DeclHUDShader_Vtx (0x73199e2e);
          SK_D3D11_DeclHUDShader_Vtx (0xa8226121);
          SK_D3D11_DeclHUDShader_Vtx (0xc9ce2d35);
          SK_D3D11_DeclHUDShader_Vtx (0xd2650529);
        }

        config.window.activate_at_start = true;
      } break;

      case SK_GAME_ID::Persona4:
      {
        config.window.treat_fg_as_active          = true;
        config.dpi.per_monitor.aware              = true;
        config.textures.cache.ignore_nonmipped    = true;
        config.compatibility.impersonate_debugger = true;
        config.apis.d3d9.hook                     = false;
        config.apis.d3d9ex.hook                   = false;
        config.apis.OpenGL.hook                   = false;

        apis.d3d9.hook->store   (config.apis.d3d9.  hook);
        apis.d3d9ex.hook->store (config.apis.d3d9ex.hook);
        apis.OpenGL.hook->store (config.apis.OpenGL.hook);
      } break;

      case SK_GAME_ID::ChronoCross:
      {
        if (! SK_PE32_IsLargeAddressAware ())
              SK_PE32_MakeLargeAddressAwareCopy ();

        config.threads.enable_dynamic_spinlocks = true;
      } break;

      case SK_GAME_ID::FinalFantasy13:
      {
        config.compatibility.allow_dxdiagn            = false;
        config.compatibility.auto_large_address_patch = false;
      } break;

      case SK_GAME_ID::FalloutNewVegas:
      {
        config.window.treat_fg_as_active      = true;
      } break;

#ifdef _M_AMD64
      case SK_GAME_ID::WutheringWaves:
      {
        // Work-around anti-cheat
        config.compatibility.disable_debug_features =  true;
        config.system.handle_crashes                = false;
      } break;

      case SK_GAME_ID::GenshinImpact:
      {
        // Work-around anti-cheat
        config.compatibility.disable_debug_features =  true;
        config.system.handle_crashes                = false;

        // Game requires sRGB Passthrough for proper SDR color
        config.render.dxgi.srgb_behavior = 0;

        // Game has a funny IAT with DLL dependencies it does not need
        config.apis.OpenGL.hook          = false;
        config.apis.d3d9.hook            = false;
        config.apis.d3d9ex.hook          = false;

        // Game now has native PlayStation support
        config.input.gamepad.xinput.emulate = false;
      } break;

      case SK_GAME_ID::PathOfExile:
      {
        // Last remaining game that requires an override, its engine
        //   freaks the hell out of the SwapChain has a _TYPELESS
        //     format even though all API calls are successful.
        config.render.dxgi.srgb_behavior = 1;
      } break;

      case SK_GAME_ID::HatsuneMikuDIVAMegaMix:
      {
        config.render.dxgi.ignore_thread_flags   =  true;
        config.threads.enable_dynamic_spinlocks  =  true;

        // Game tries to get an R8_UNORM view of the SwapChain,
        //   this would normally trigger a warning in SK.
        config.render.dxgi.suppress_rtv_mismatch =  true;

        // Defaults for latency an framepacing; important
        config.render.framerate.sleepless_render =  true;
        config.render.framerate.sleepless_window =  true;
        config.window.background_render          =  true;

        // Fix bad stuff introduced in first patch
        config.window.borderless                 =  true;
        config.window.fullscreen                 =  true;
        config.display.force_windowed            =  true;
        config.window.always_on_top              =  PreventAlwaysOnTop;

        // 1.00: 0x2C3201
        // 1.01: 0x2C37B1, File Location: 0x2C2801
        // 1.02: 0x2C3681, File Location: 0x2C26D0
        // 1.03: 0x2C36C1, File Location: 0x2C26F0
        auto win_ver_check_addr =
          ((uintptr_t)SK_Debug_GetImageBaseAddr () + 0x2C36C1);
        auto win_ver_check_pattern = "\x48\x8B\xCF";

        DWORD dwOrigProt =                                     PAGE_EXECUTE_READ;
        if (VirtualProtect ((LPVOID)(win_ver_check_addr-3), 8, PAGE_EXECUTE_READWRITE, &dwOrigProt))
        {
          if ( 0 ==
                 std::memcmp ( (LPCVOID)(win_ver_check_addr -  3),
                               (LPCVOID) win_ver_check_pattern, 3 ) )
          {
            memcpy        (    (LPVOID) win_ver_check_addr,    "\x90\x90\x90\x90\x90", 5);
            VirtualProtect(    (LPVOID)(win_ver_check_addr-3),                         8, dwOrigProt,
                                                                                         &dwOrigProt);

            plugin_mgr->begin_frame_fns.emplace (
              SK_HatsuneMiku_BeginFrame
            );
          }
        }
      } break;
#endif

      case SK_GAME_ID::Launcher:
      {
        config.system.silent = true;
      } break;


#ifdef _M_AMD64
      case SK_GAME_ID::GalGunReturns:
        config.input.keyboard.override_alt_f4    = true;
        break;

      case SK_GAME_ID::Persona5Strikers:
        config.apis.d3d9.hook                    = false;
        config.apis.d3d9ex.hook                  = false;
        config.input.keyboard.override_alt_f4    = true;
        config.input.keyboard.catch_alt_f4       = true;
        config.window.background_render          = true;
        break;

      case SK_GAME_ID::HorizonZeroDawn:
        // Game uses UI mouse cursor position for mouselook,
        //   it needs to be clipped because developers don't
        //     know about relative input (e.g. RawInput).
        config.input.mouse.ignore_small_clips     = false;
        config.compatibility.impersonate_debugger = false;
        break;

      case SK_GAME_ID::HorizonForbiddenWest:
      {
#if 0
        if (SK_IsInjected () && ((! PathFileExists (L"dxgi.dll")) &&
                                 (! PathFileExists (L"d3d12.dll"))))
        {
          wchar_t      wszProfileSKinny [MAX_PATH] = {};
          PathAppendW (wszProfileSKinny, SK_GetConfigPath ());
          PathAppendW (wszProfileSKinny,    L"SKinny.ignore");

          if (! (PathFileExists (L"SKinny.ignore") ||
                 PathFileExists (wszProfileSKinny)))
          {
            if (IDYES ==
                SK_MessageBox (
                  L"Special K has Compatibility Issues with this Game\r\n\r\n"
                  L" * Please use Local Injection or SKinny\r\n\r\n"
                  L"Click Yes for more info on SKinny.", L"Special K Incompatibility",
                    MB_YESNO|MB_ICONWARNING))
            {
              SK_Util_OpenURI (L"https://github.com/SpecialKO/SKinny/releases", SW_RESTORE);
            }
          }
        }
#endif
      } break;

      case SK_GAME_ID::Yakuza0:
      {
        if (! IsProcessDPIAware ())
        {
          SK_Display_ForceDPIAwarenessUsingAppCompat (true);
          SK_Display_SetMonitorDPIAwareness          (false);

          // Only do this for Steam games, the Microsoft Store Yakuza games
          //   are chronically DPI unaware and broken
          if (StrStrIW (SK_GetHostPath (), L"SteamApps"))
            SK_RestartGame ();
        }

        ///// Engine has a problem with its texture management that
        /////   makes texture caching / modding impossible.
        config.textures.d3d11.cache               = false;
        config.textures.cache.allow_unsafe_refs   =  true;
        config.render.dxgi.deferred_isolation     =  true; // Needed for DoF disable
        config.window.borderless                  =  true;
        config.window.fullscreen                  =  true;
        config.input.keyboard.override_alt_f4     =  true;
        config.input.keyboard.catch_alt_f4        =  true;
        config.platform.silent                    =  true; // Game crashes w/ SteamAPI

        // Game's busted without this
        config.display.force_windowed             =  true;
      }
      break;

      case SK_GAME_ID::YakuzaKiwami2:
        config.apis.d3d9.hook                     =  false;
        config.apis.d3d9ex.hook                   =  false;

        SK_Display_ForceDPIAwarenessUsingAppCompat (true);
        SK_Display_SetMonitorDPIAwareness          (false);

        dll_ini->import (L"[Import.ReShade64_Custom]\n"
                         L"Architecture=x64\n"
                         L"Role=Unofficial\n"
                         L"When=PlugIn\n"
                         L"Filename=ReShade64.dll\n");

        SK_D3D11_DeclHUDShader_Vtx (0x062173ec);
        SK_D3D11_DeclHUDShader_Vtx (0x48dd4bc3);
        SK_D3D11_DeclHUDShader_Vtx (0x54c0d366);
        SK_D3D11_DeclHUDShader_Vtx (0xb943178b);
                                                 [[fallthrough]];

      case SK_GAME_ID::YakuzaKiwami:             [[fallthrough]];
      case SK_GAME_ID::YakuzaUnderflow:
      {
        config.render.dxgi.fake_fullscreen_mode   = true;

        if (! IsProcessDPIAware ())
        {
          // Oly do this for Steam games, the Microsoft Store Yakuza games
          //   are chronically DPI unaware and broken
          if (StrStrIW (SK_GetHostPath (), L"SteamApps"))
            SK_RestartGame ();
        }

        config.textures.d3d11.cache               =  false;
        config.window.background_render           =   true;
        config.window.always_on_top               =  PreventAlwaysOnTop;
        config.render.dxgi.deferred_isolation     =  false;
        config.platform.reuse_overlay_pause       =  false;
        config.window.borderless                  =   true;
        config.window.fullscreen                  =   true;
        config.input.keyboard.override_alt_f4     =   true;
      }
      break;

      case SK_GAME_ID::YakuzaInfiniteWealth:
      case SK_GAME_ID::YakuzaLikeADragonGaiden:
      {
        config.render.dxgi.fake_fullscreen_mode   = true;
        config.window.background_render           = true;
        config.render.dxgi.hooks.
                            create_swapchain4hwnd = false;
      }
      break;

      case SK_GAME_ID::DragonQuestXI:
        config.apis.d3d9.hook                     = false;
        config.apis.d3d9ex.hook                   = false;
        config.apis.OpenGL.hook                   = false;
        config.apis.Vulkan.hook                   = false;
        config.apis.dxgi.d3d12.hook               = false;
        config.render.framerate.enable_mmcss      = true;

        apis.d3d9.hook->store   (config.apis.d3d9.  hook);
        apis.d3d9ex.hook->store (config.apis.d3d9ex.hook);
        apis.OpenGL.hook->store (config.apis.OpenGL.hook);
        break;

      case SK_GAME_ID::AssassinsCreed_Odyssey:
        config.apis.d3d9.hook                     = false;
        config.apis.d3d9ex.hook                   = false;
        config.apis.OpenGL.hook                   = false;
        config.apis.Vulkan.hook                   = false;
        config.render.framerate.enable_mmcss      = true;
        config.textures.cache.residency_managemnt = false;
        config.render.framerate.buffer_count      = 3;
        config.render.framerate.pre_render_limit  = 4;
        config.textures.cache.max_size            = 5120;
        break;

      case SK_GAME_ID::AssassinsCreed_Valhalla:
        config.apis.d3d9.hook                    = false;
        config.apis.d3d9ex.hook                  = false;
        config.apis.OpenGL.hook                  = false;
        config.apis.Vulkan.hook                  = false;
        config.apis.dxgi.d3d11.hook              = false;
        config.apis.dxgi.d3d12.hook              =  true;
        config.render.framerate.flip_discard     =  true;
        config.render.framerate.swapchain_wait   =     1;
        config.render.framerate.buffer_count     =     6;
        config.render.framerate.pre_render_limit =     6;
        config.render.framerate.present_interval =     1;
        config.render.framerate.sleepless_render =  true;
        config.render.framerate.sleepless_window =  true;
        break;

      case SK_GAME_ID::Shenmue:
        config.textures.d3d11.generate_mips       = true;
        config.textures.d3d11.uncompressed_mips   = true;
        config.textures.d3d11.cache_gen_mips      = true;
        config.render.framerate.target_fps        = 30.0f;
        break;

      case SK_GAME_ID::CallOfCthulhu:
      {
        SK_D3D11_DeclHUDShader_Pix (0x28c1a0d6);
      } break;

      case SK_GAME_ID::JustCause3:
      {
        // This game has multiple windows, we can't hook the wndproc
        config.window.dont_hook_wndproc   = true;
        //config.system.display_debug_out = true;
        config.steam.force_load_steamapi  = true;
        config.steam.auto_inject          = true;
        config.steam.auto_pump_callbacks  = true;
        config.platform.silent            = true;
      } break;

      case SK_GAME_ID::Tales_of_Vesperia:
      {
        config.window.treat_fg_as_active        = true;
        config.input.ui.use_hw_cursor           = false;
        config.textures.d3d11.uncompressed_mips = true;
        config.textures.d3d11.cache_gen_mips    = true;
        config.render.d3d11.wrap_d3d11_dev_ctx  = true;
        config.render.dxgi.deferred_isolation   = true; // For texture mods / HUD tracking
      } break;

      case SK_GAME_ID::Tales_of_Graces:
      {
        // Avoid the game's native DualSense support so rumble will work
        config.input.gamepad.xinput.emulate                    = true;
        config.input.gamepad.disable_hid                       = true;
        config.input.gamepad.scepad.alias_trackpad_share       = true;
        // XInput has a number of advantages over WGI and Unity will fallback
        //   to XInput 1.3 if we deny it access to Windows.Gaming.Input interfaces.
        config.input.gamepad.windows_gaming_input.blackout_api = true;
      //config.render.d3d11.trace_sampler_init                 = true;
        config.render.framerate.target_fps                     = 0.0f;
        config.render.framerate.buffer_count                   =    3;
        config.render.framerate.pre_render_limit               =    2;
        config.render.framerate.sleepless_render               = true;
        config.render.framerate.sleepless_window               = true;
        config.render.d3d12.max_anisotropy                     =    9;
        config.render.d3d12.force_anisotropic                  =false;
      } break;

      case SK_GAME_ID::Tales_of_Arise:
      {
        // Fix for 4K TVs
        config.render.dxgi.res.max.x = 3840;
        config.render.dxgi.res.max.y = 2160;

        config.window.borderless = true;
        config.window.center     = true;
        config.window.fullscreen = true;

        void WINAPI
          SK_D3D11_SetResourceRoot (const wchar_t *root);
          SK_D3D11_SetResourceRoot (config.textures.d3d11.res_root.c_str ());

        if (PathFileExistsW ((std::filesystem::path (SK_D3D11_res_root->c_str ()) / LR"(inject\textures\CDE62E66.dds)").c_str ()))
        {
          void *pSteamInput001 =
            SK_ScanAligned ("SteamInput001", 13, "SteamInput001", 16);

          if (pSteamInput001 != nullptr)
          {
            DWORD                                                            dwOriginal = 0;
            if (VirtualProtect (pSteamInput001, 13, PAGE_EXECUTE_READWRITE, &dwOriginal))
            {
              SK_ImGui_Warning (L"Steam Input Disabled...");
                        memcpy (pSteamInput001, "SteamInputDie", 13);
                VirtualProtect (pSteamInput001, 13, dwOriginal,             &dwOriginal);
            }
          }

          config.input.gamepad.xinput.emulate         = true;
          config.input.gamepad.steam.disabled_to_game = true;
          config.input.gamepad.xinput.placehold [0]   = true;
        }
      } break;

      case SK_GAME_ID::AtelierRyza2:
      {
        config.render.framerate.flip_discard  = true;
        config.render.dxgi.deferred_isolation = true; // For texture mods / HUD tracking

        // Don't accidentally hook the D3D9 device used for video playback
        config.apis.d3d9.hook   = false;
        config.apis.d3d9ex.hook = false;

        SK_D3D11_DeclHUDShader_Vtx (0x1a7704f4);
      } break;

      case SK_GAME_ID::Nioh2:
      {
        // Kill the win32 message box, SK's in-game confirmation is gamepad friendly
        config.input.keyboard.override_alt_f4    = true;
        config.render.framerate.flip_discard     = true;
        config.render.framerate.swapchain_wait   = 1;
        config.render.framerate.sleepless_window = true;
        config.render.dxgi.fake_fullscreen_mode  = true;
      } break;

      case SK_GAME_ID::OctopathTraveler:
      {
        // It's a Denuvo game, so it may take a while to start...
        //   but the game DOES run its own callbacks so don't worry.
        config.steam.auto_pump_callbacks         = false;
        config.window.borderless                 =  true;
        config.window.fullscreen                 =  true;
        config.window.center                     = false;

        config.render.framerate.buffer_count      =     3;
        config.render.framerate.target_fps        =    60;
        config.render.framerate.pre_render_limit  =     4;
        config.render.framerate.sleepless_render  =  true;
        config.render.framerate.sleepless_window  =  true;

        config.window.treat_fg_as_active          =  true;
        config.window.background_render           =  true;
        config.input.ui.use_hw_cursor             = false;
        config.input.cursor.manage                =  true;
        config.input.cursor.timeout               =     0;
        config.input.cursor.keys_activate         = false;

        config.apis.d3d9.hook                     = false;
        config.apis.d3d9ex.hook                   = false;
        config.apis.OpenGL.hook                   = false;

        config.threads.enable_file_io_trace       =  true;
        
        SK_OPT_InitPlugin ();

        apis.d3d9.hook->store   (config.apis.d3d9.  hook);
        apis.d3d9ex.hook->store (config.apis.d3d9ex.hook);
        apis.OpenGL.hook->store (config.apis.OpenGL.hook);
      } break;

      case SK_GAME_ID::HuniePop2:
      {
        // Unity engine game is detected as GL due to no imports
        config.apis.OpenGL.hook                      = false;
        config.apis.d3d9.hook                        = false;
        config.textures.cache.ignore_nonmipped       =  true;

        apis.d3d9.hook->store   (config.apis.d3d9.  hook);
        apis.d3d9ex.hook->store (config.apis.d3d9ex.hook);
        apis.OpenGL.hook->store (config.apis.OpenGL.hook);
      } break;

      case SK_GAME_ID::LegendOfMana:
      {
        config.render.framerate.sleepless_window     =  true;
        config.render.framerate.sleepless_render     =  true;

        config.render.framerate.buffer_count         =     3;
        config.render.framerate.swapchain_wait       =     1;
        config.render.framerate.pre_render_limit     =     2;

        config.render.framerate.flip_discard         =  true;
        config.render.framerate.target_fps           =    60;

        config.dpi.disable_scaling                   =  true;
        config.dpi.per_monitor.aware                 =  true;
        config.dpi.per_monitor.aware_on_all_threads  =  true;

        SK_InjectMemory ( (void *)((uintptr_t)SK_Debug_GetImageBaseAddr () + 0x29A230),
                            "\x90\x90",
                              2,
                                PAGE_EXECUTE_READWRITE );

        // Disable GPU power saving mode using Reflex, otherwise game stutters
        //
        config.nvidia.reflex.enable                   =  true;
        config.nvidia.reflex.enforcement_site         =     2;
        config.nvidia.reflex.low_latency              =  true;
        config.nvidia.reflex.low_latency_boost        =  true;
      } break;


      // You knowe it's a bad port when it requires all of these overrides (!!)
      case SK_GAME_ID::NieR_Sqrt_1_5:
      {
        // Prevent VRR disable when game plays cutscenes
        config.render.framerate.sync_interval_clamp  =     1;

        config.textures.d3d11.cache                  =  true;
        config.textures.cache.allow_staging          =  true;
        config.textures.cache.max_size               =    64; // SmaLL cache; engine's leaky
        config.apis.OpenGL.hook                      = false;
        config.apis.d3d9.hook                        = false;
        config.apis.d3d9ex.hook                      = false;
        config.input.cursor.keys_activate            = false;
        config.input.cursor.manage                   = false; // Automagic
        config.input.cursor.timeout                  =   750;
        config.input.ui.use_hw_cursor                = false;
        config.input.ui.capture_hidden               = false;
        config.input.ui.capture_mouse                = false;
        config.render.framerate.sleepless_window     =  true;
        config.render.framerate.sleepless_render     = false; // Reshade Problems
        config.render.framerate.max_delta_time       =     1;
        config.render.framerate.buffer_count         =     4;
        config.render.framerate.swapchain_wait       =     0;
        config.render.framerate.pre_render_limit     =     3;
        config.render.framerate.target_fps           =    60;
        config.render.framerate.drop_late_flips      =  true;
        config.render.framerate.flip_discard         =  true;
        config.input.gamepad.disable_hid             =  true;
        config.input.gamepad.xinput.emulate          =  true;
        config.input.gamepad.xinput.placehold [0]    =  true;
        config.input.gamepad.xinput.auto_slot_assign =  true;
        config.threads.enable_file_io_trace          =  true;
        config.steam.preload_overlay                 = false; // Set to false because of loss of rumble
        config.window.background_render              =  true;
        config.render.dxgi.fake_fullscreen_mode      =  true;

        SK_D3D11_DeclHUDShader_Vtx (0x3e464f00);

        config.render.dxgi.deferred_isolation        =  true; // Enable render mods
        config.render.dxgi.low_spec_mode             =  true; // Reduce state tracking overhead

        config.input.keyboard.catch_alt_f4           =  true;
        config.input.keyboard.override_alt_f4        =  true;

        config.nvidia.reflex.enable                  =  true;
        config.nvidia.reflex.enforcement_site        =     2;
        config.nvidia.reflex.low_latency             =  true;
        config.nvidia.reflex.low_latency_boost       =  true;

        config.system.suppress_crashes               =  true;

        apis.d3d9.hook->store   (config.apis.d3d9.  hook);
        apis.d3d9ex.hook->store (config.apis.d3d9ex.hook);
        apis.OpenGL.hook->store (config.apis.OpenGL.hook);
      } break;

      case SK_GAME_ID::ResidentEvil8:
      { config.render.framerate.sleepless_window     =  true;
        config.render.framerate.sleepless_render     = false;
        config.render.dxgi.use_factory_cache         =  true;
        config.render.framerate.max_delta_time       =     1;
        config.system.suppress_crashes               =  true;
        config.compatibility.impersonate_debugger    = false;
      } break;

      // Nintendo Switch Emulators ( OpenGL / Vulkan )
      case SK_GAME_ID::yuzu:
      case SK_GAME_ID::Ryujinx:
      case SK_GAME_ID::cemu:    // Wii U
      case SK_GAME_ID::RPCS3:   // PS3
      case SK_GAME_ID::ShadPS4: // PS4
      {
        config.steam.appid                = 0;
        config.platform.silent            = true;
        config.apis.d3d9.hook             = false;
        config.apis.d3d9ex.hook           = false;
        config.apis.OpenGL.hook           = true;
        config.apis.Vulkan.hook           = true;
        config.apis.last_known            = SK_RenderAPI::OpenGL;

        apis.d3d9.hook->store   (config.apis.d3d9.      hook);
        apis.d3d9ex.hook->store (config.apis.d3d9ex.    hook);
        apis.OpenGL.hook->store (config.apis.OpenGL.    hook);
        apis.Vulkan.hook->store (config.apis.Vulkan.    hook);
        apis.last_known->store  ((int)config.apis.last_known);

        config.window.always_on_top       = PreventAlwaysOnTop;
        config.apis.NvAPI.vulkan_bridge   = 1;

        SK_Vulkan_DisableThirdPartyLayers ();

        if (SK_GetCurrentGameID () == SK_GAME_ID::yuzu && (! SK_IsInjected ()))
        {
          static SK_AutoHandle hHookTeardown (
            OpenEvent ( EVENT_ALL_ACCESS, FALSE,
                SK_RunLHIfBitness (32, LR"(Local\SK_GlobalHookTeardown32)",
                                       LR"(Local\SK_GlobalHookTeardown64)") )
                                      );

          if (! hHookTeardown.m_h)
          {
            SK_MessageBox ( L"Please Manually Start Special K Injection",
                            L"Yuzu requires Special K's Injection Service",
                              MB_OK | MB_ICONASTERISK );

            SK_SelfDestruct ();
          }
        }
      } break;

      case SK_GAME_ID::HaloInfinite:
        // Prevent VRR disable when using game's framerate limiter
        config.render.framerate.sync_interval_clamp = 1;
        break;

      case SK_GAME_ID::SeriousSamFusion2017:
        // Prevent crash on launch due to 'Signature verification of executable failed.'
        config.window.dont_hook_wndproc = true;
        break;

      case SK_GAME_ID::ForzaHorizon5:
      case SK_GAME_ID::ForzaMotorsport:
      {
        config.input.gamepad.dualsense.trigger_effect_l = playstation_trigger_effect::Vibration;
        config.input.gamepad.dualsense.trigger_effect_r = playstation_trigger_effect::Vibration;
        if (SK_IsInjected () && ((! PathFileExists (L"dxgi.dll")) &&
                                 (! PathFileExists (L"d3d12.dll"))))
        {
          wchar_t      wszProfileSKinny [MAX_PATH] = {};
          PathAppendW (wszProfileSKinny, SK_GetConfigPath ());
          PathAppendW (wszProfileSKinny,    L"SKinny.ignore");

          if (! (PathFileExists (L"SKinny.ignore") ||
                 PathFileExists (wszProfileSKinny)))
          {
            if (IDYES ==
                SK_MessageBox (
                  L"Special K has Compatibility Issues with this Game\r\n\r\n"
                  L" * Please use Local Injection or SKinny\r\n\r\n"
                  L"Click Yes for more info on SKinny.", L"Special K Incompatibility",
                    MB_YESNO|MB_ICONWARNING))
            {
              SK_Util_OpenURI (L"https://github.com/SpecialKO/SKinny/releases", SW_RESTORE);
            }
          }
        }

        // Prevent VRR disable when using game's framerate limiter
        config.render.framerate.sync_interval_clamp = 1;

        // Work around DRM / Anti-Debug Quirks
        config.compatibility.disable_debug_features =  true;
        config.window.dont_hook_wndproc             =  true;

        // Prevent double-input
        config.input.gamepad.dinput.blackout_gamepads
                                                    =  true;
        config.input.gamepad.xinput.emulate         =  true;
        config.input.cursor.manage                  =  true; // Mouse cursor doesn't auto-hide
        config.input.gamepad.impulse_strength_r     =  0.3f; // Impulse triggers are a bit STRONG...
        config.input.gamepad.impulse_strength_l     =  0.3f; // Impulse triggers are a bit STRONG...
        config.input.gamepad.xinput.placehold [0]   =  true;
        config.input.gamepad.xinput.placehold [1]   = false;
        config.input.gamepad.xinput.placehold [2]   = false;
        config.input.gamepad.xinput.placehold [3]   = false;
      } break;

      case SK_GAME_ID::StarWarsOutlaws:
      {
        // Avoid anti-debug stuff
        SK_GetCurrentRenderBackend ().windows.capcom = true;
      } break;

      case SK_GAME_ID::FinalFantasy7Remake:
      {
        SK_D3D11_DeclHUDShader_Vtx (0x38a98690);
        SK_D3D11_DeclHUDShader_Vtx (0x9d5f74c0);
        SK_D3D11_DeclHUDShader_Pix (0xa9fd37df);

        config.input.cursor.manage               = true;
        config.input.cursor.timeout              = 0; // Mouse cursor? What's that?

        config.render.framerate.buffer_count     = 3;
        config.render.framerate.pre_render_limit = 3;
        config.render.framerate.max_delta_time   = 1;
        config.render.framerate.sleepless_render = true;
        config.render.framerate.sleepless_window = true;

        config.apis.d3d9.hook   = false;
        config.apis.d3d9ex.hook = false;
        config.apis.OpenGL.hook = false;
      } break;

      case SK_GAME_ID::DyingLight2:
        config.input.mouse.ignore_small_clips = true;
        break;

      case SK_GAME_ID::EldenRing:
        // Minimize chance of games screwing up achievements
        app_cache_mgr->setFriendPreference (SK_AppCache_Manager::Disable);

        // Keep mouse inside game window while playing and prevent
        // input when game is not focused (background_render)
        config.window.confine_cursor             = true;
        config.window.background_render          = true;
        config.apis.dxgi.d3d11.hook              = false;
        config.apis.OpenGL.hook                  = false;
        config.render.framerate.sleepless_window = false;
        config.render.dxgi.fake_fullscreen_mode  = true;
        break;

      case SK_GAME_ID::TinyTinasWonderlands:
        config.apis.d3d9.hook   = false;
        config.apis.d3d9ex.hook = false;
        config.apis.OpenGL.hook = false;
        break;

      case SK_GAME_ID::Elex2:
        config.textures.cache.ignore_nonmipped = true;

        SK_D3D11_DeclHUDShader_Vtx (0x2725bc3e); // Primary UI
        SK_D3D11_DeclHUDShader_Vtx (0xcdb10248); // Radar
        break;

      case SK_GAME_ID::TheQuarry:
        config.threads.enable_dynamic_spinlocks = true;

        SK_D3D11_DeclHUDShader_Vtx (0x71532076);
        SK_D3D11_DeclHUDShader_Vtx (0x90c7c88b);
        break;

      case SK_GAME_ID::MonsterHunterRise:
        // DLAA produces a black screen w/o this
        config.nvidia.dlss.compat.extra_pixels   = -2;
        // Allow non-D presets
        config.nvidia.dlss.compat.override_appid = 0x24480451;
        break;
#endif
      case SK_GAME_ID::Disgaea5:
        config.render.framerate.flip_discard = false;
        break;

      case SK_GAME_ID::SoulHackers2:
        SK_D3D11_DeclHUDShader_Vtx (0x10e0c21c);
        SK_D3D11_DeclHUDShader_Vtx (0xb70aff71);
        SK_D3D11_DeclHUDShader_Vtx (0xd7fb1989);
        break;

      case SK_GAME_ID::MegaManBattleNetwork:
        config.compatibility.init_on_separate_thread = false;
        config.display.force_windowed                = true;
        SK_GetCurrentRenderBackend ().windows.capcom = true;
        break;

      case SK_GAME_ID::ZenlessZoneZero:
        // Work-around anti-cheat
        config.compatibility.disable_debug_features =  true;
        config.system.handle_crashes                = false;
        break;

      case SK_GAME_ID::HonkaiStarRail:
        // Work-around anti-cheat
        config.compatibility.disable_debug_features =  true;
        config.system.handle_crashes                = false;
        // Game has native PlayStation support
        config.input.gamepad.xinput.emulate         = false;
        break;

      case SK_GAME_ID::FinalFantasyXIV:
        config.display.force_windowed = true;
        config.window.borderless      = true;
        config.window.fullscreen      = true;
        break;

      case SK_GAME_ID::NoMansSky:
        config.apis.NvAPI.vulkan_bridge = 1;
        break;

      case SK_GAME_ID::SonicGenerations:
      case SK_GAME_ID::SonicXShadowGenerations:
        // Do not enable Sleepless options in this game, it will cause problems
        config.render.framerate.sleepless_render = false;
        config.render.framerate.sleepless_window = false;
        config.textures.d3d11.cache              = false;
        config.input.cursor.manage               =  true;
        config.input.cursor.timeout              =   500;
        break;

      case SK_GAME_ID::YsX:
        // Reduce stutter on plugging and unplugging devices
        config.compatibility.sdl.allow_direct_input = 0;
        config.compatibility.sdl.allow_wgi          = 0;
        config.compatibility.sdl.allow_raw_input    = 0;
        config.textures.d3d11.cache                 = false;
        // Cache is pointless, the game has an equivalent feature
        break;

      case SK_GAME_ID::BrokenSword:
        // Has really bad timing code that will cause major frame drops w/o.
        config.render.framerate.max_delta_time      = 15;
        break;

      case SK_GAME_ID::MonsterHunterWilds:
        config.steam.crapcom_mode                   = true;
        config.render.dstorage.enable_hooks         = false;
        break;

      case SK_GAME_ID::DragonAgeTheVeilguard:
        config.input.ui.capture_mouse               = true;
        break;

      // Game requires special sRGB treatment.
      case SK_GAME_ID::TombRaider123Remastered:
        config.render.dxgi.srgb_behavior            = 0;
        config.render.gl.prefer_10bpc               = false;
        break;

      case SK_GAME_ID::Stalker2:
        // Stupid game requires Fullscreen Exclusive (in D3D12) for HDR
        config.render.dxgi.fake_fullscreen_mode     = true;
        config.input.gamepad.xinput.emulate         = true;
        // GameInput has poor support for non-Xbox controllers...
        break;

      case SK_GAME_ID::Metaphor:
        config.compatibility.init_on_separate_thread   = false;
        config.priority.perf_cores_only                = true;
        config.window.background_render                = true;
        config.window.fullscreen_no_saver              = true;
        config.input.keyboard.override_alt_f4          = true; // Oh lord, kill that buggy exit confirmation
        config.render.framerate.sleepless_render       = true;
        config.render.framerate.sleepless_window       = true;
        config.input.gamepad.xinput.emulate            = true; // XInput-only
        config.render.hdr.remaster_8bpc_as_unorm       = true;
        config.render.hdr.remaster_subnative_as_unorm  = true;
        config.textures.cache.allow_staging            = false;
        config.render.dxgi.deferred_isolation          = false; // Needed for correct texture caching on staging uploads

        config.render.d3d12.force_anisotropic          = true;
        config.render.d3d12.force_lod_bias             = 0.0f;

        SK_D3D11_DeclHUDShader_Vtx (0x187097b5);
        SK_D3D11_DeclHUDShader_Vtx (0x8973ba49);

        // Scheduling fixes still needed.
        config.compatibility.allow_dxdiagn             = true;

        SK_RunOnce
        (
          // Auto-load Metaphor Fix if it is present
          if (PathFileExistsW (L"MetaphorFix.asi")
              && LoadLibraryW (L"MetaphorFix.asi"))
          {
            SK_ImGui_CreateNotification (
              "PlugIn.Load", SK_ImGui_Toast::Success,
                 "MetaphorFix.asi",
                   "Special K Plug-In Loaded",
                   5000, SK_ImGui_Toast::UseDuration |
                         SK_ImGui_Toast::ShowCaption |
                         SK_ImGui_Toast::ShowTitle );
          }
        );
        break;

      case SK_GAME_ID::DiabloIV:
        config.window.dont_hook_wndproc = true;
        break;

      case SK_GAME_ID::CallOfDuty:
        // Disable D3D9 and D3D12 for OpenGL-IK to take precedence
        config.apis.d3d9.hook       = false;
        config.apis.d3d9ex.hook     = false;
        config.apis.dxgi.d3d12.hook = false;
        break;

      // Pain in the ass Nixxes port
      case SK_GAME_ID::RatchetAndClank_RiftApart:
        // Game has native PlayStation support
        config.input.gamepad.xinput.emulate          = false;
        config.compatibility.init_on_separate_thread = false;
        // Workaround Nixxes quirks
        break;

      case SK_GAME_ID::BatmanArkhamKnight:
        config.input.gamepad.scepad.hide_ds4_v2_pid = SK_Enabled;
        break;

      case SK_GAME_ID::Starfield:
        config.compatibility.reshade_mode = false;
        break;

      case SK_GAME_ID::StardewValley:
        // Game needs to be told that it is OpenGL, or it won't inject...
        config.compatibility.
            init_on_separate_thread = false;
        config.apis.last_known      = SK_RenderAPI::OpenGL;
        config.apis.dxgi.d3d11.hook = true;
        config.apis.OpenGL.hook     = true;
        apis.last_known->store     ((int)config.apis.last_known     );
        apis.OpenGL.hook->store    (     config.apis.OpenGL.hook    );
        apis.d3d11.hook->store     (     config.apis.dxgi.d3d11.hook);
        break;

      case SK_GAME_ID::Cyberpunk2077:
        // Game now has native PlayStation support
        config.input.gamepad.xinput.emulate = false;
        break;

      case SK_GAME_ID::DOOMEternal:
        config.apis.NvAPI.vulkan_bridge   = 1;
        config.system.global_inject_delay = 0.0f;
        break;

      case SK_GAME_ID::GranblueFantasyRelink:
        config.input.gamepad.xinput.emulate           = true;
        config.input.gamepad.steam.disabled_to_game   = true;
        config.window.background_render               = true;
        config.window.activate_at_start               = true;
        config.window.always_on_top                   =    2;
        config.render.dxgi.fake_fullscreen_mode       = true;
        config.input.cursor.manage                    = true;
        config.input.cursor.gamepad_deactivates       = true;
        config.input.cursor.timeout                   =    0;
        config.window.disable_screensaver             = true;
        config.render.hdr.remaster_8bpc_as_unorm      = true;
        config.render.hdr.remaster_subnative_as_unorm = true;
        config.render.dxgi.deferred_isolation         = true; // For render mods
        break;

      case SK_GAME_ID::WrathAeonOfRuin:
        config.window.activate_at_start  = true;
        config.window.treat_fg_as_active = true;
        break;

      case SK_GAME_ID::AlanWake2:
      {
        config.apis.OpenGL.hook = false;
        config.apis.d3d9.hook   = false;
        config.apis.d3d9ex.hook = false;
        config.apis.Vulkan.hook = false;

        void *pOverlayCheck =
          (void *)((uintptr_t)SK_Debug_GetImageBaseAddr () + 0x1E74B09);

        DWORD                                                          dwOriginal = 0;
        if (VirtualProtect (pOverlayCheck, 5, PAGE_EXECUTE_READWRITE, &dwOriginal))
        { if (! memcmp (    pOverlayCheck, "\xE8\x42\xB5\x1E\xFF", 5))
          {
            SK_LOGi0 (L"Disabled Alan Wake 2 Overlay Check");
            memcpy (        pOverlayCheck, "\x90\x90\x90\x90\x90", 5);
          }
        } VirtualProtect (  pOverlayCheck, 5, dwOriginal, &dwOriginal);

        plugin_mgr->first_frame_fns.emplace (
        [](IUnknown *, UINT, UINT) -> HRESULT
        {
          if (  GetModuleHandleW (L"RTSSHooks64.dll") != nullptr &&
              (! PathFileExistsW (L"SpecialK.RTSSWarned")) )
          {
            SK_ImGui_Warning (
              L"RTSS disables the Epic overlay, which is required to activate this game.\r\n\r\n"
              L"\t>> This warning will not be shown again."
            );

            FILE *fWarned =
              fopen ("SpecialK.RTSSWarned", "w");

            if (         fWarned != nullptr)
            { fputc  (0, fWarned);
              fclose (   fWarned);
            }
          }

          if ( ! GetModuleHandleW (L"EOSOVH-Win64-Shipping.dll") &&
              (! PathFileExistsW  (L"SpecialK.RTSSWarned")) )
          {
            SK_ImGui_Warning (
              L"The EOS Overlay is required once to activate this game.\r\n\r\n"
              L"\t>> This warning will not be shown again."
            );

            FILE *fWarned =
              fopen ("SpecialK.EOSWarned", "w");

            if (         fWarned != nullptr)
            { fputc  (0, fWarned);
              fclose (   fWarned);
            }
          }

          return S_OK;
        });

      } break;

      case SK_GAME_ID::GodOfWar:
      {
        // Game has native PlayStation support
        config.input.gamepad.xinput.emulate = false;
      } break;

      case SK_GAME_ID::GodOfWarRagnarok:
      {
        // Game has native PlayStation support
        config.input.gamepad.xinput.emulate = false;
        // Window management tweaks to assist this game in keeping
        //   the Windows task bar away in borderless mode
        config.window.always_on_top     = SmartAlwaysOnTop;
        config.window.center            = true;
        config.window.borderless        = true;
        config.window.background_render = true;
      } break;

      case SK_GAME_ID::TalosPrinciple2:
      {
        // Speed-up initialization by skipping these APIs
        //   (why the game loads their DLLs is anyone's guess)
        config.apis.d3d9.hook   = false;
        config.apis.d3d9ex.hook = false;
        config.apis.OpenGL.hook = false;
      } break;

      case SK_GAME_ID::CrashBandicootNSaneTrilogy:
      {
        // Requires synchronous init or the game will get GDI Copy
        config.compatibility.init_on_separate_thread = false;
      } break;

      case SK_GAME_ID::Transistor:
      {
        config.apis.Vulkan.translate = 1; // Bridge it
        config.apis.OpenGL.hook      = false;
        config.apis.dxgi.d3d11.hook  = true;
        config.apis.dxgi.d3d12.hook  = true;
      } break;
    }
  }

  if (! apis.last_known->load ((int &)config.apis.last_known))
    config.apis.last_known = SK_RenderAPI::Reserved;
  else // Implicitly count 1 context if the last launch used OpenGL
    if (config.apis.last_known == SK_RenderAPI::OpenGL)
      SK_GL_ContextCount++;

  SK_RunOnce (
    config.apis.last_last_known =
         config.apis.last_known
  );


#ifdef _M_IX86
  apis.ddraw.hook->load  (config.apis.ddraw.hook);
  apis.d3d8.hook->load   (config.apis.d3d8.hook);
#endif

  apis.dxvk9.enable->load (config.apis.d3d9.native_dxvk);

  if (! apis.d3d9.hook->load (config.apis.d3d9.hook))
    config.apis.d3d9.hook = true;

  if (! apis.d3d9ex.hook->load (config.apis.d3d9ex.hook))
    config.apis.d3d9ex.hook = true;
  else
  {
    // D3D9Ex cannot exist without D3D9...
    if (config.apis.d3d9ex.hook)
        config.apis.d3d9.hook = true;
  }

  if (! apis.d3d11.hook->load  (config.apis.dxgi.d3d11.hook))
    config.apis.dxgi.d3d11.hook = true;

  if (apis.d3d12.hook->load (config.apis.dxgi.d3d12.hook))
  {
    // We need to enable D3D11 hooking for D3D12 to work reliably
    if (config.apis.dxgi.d3d12.hook)
        config.apis.dxgi.d3d11.hook = true;
  }

  if (! apis.OpenGL.hook->load (config.apis.OpenGL.hook))
    config.apis.OpenGL.hook = true;

#ifdef _M_AMD64
  apis.Vulkan.hook->load (config.apis.Vulkan.hook);
#endif

  init = TRUE;
}


  //
  // Load Parameters
  //
  compatibility.sdl_sanity_level->load      (config.compatibility.sdl_sanity_level);
  compatibility.allow_fake_streamline->load (config.compatibility.allow_fake_streamline);
  compatibility.fsr3_mode->load             (config.compatibility.fsr3_mode);
  compatibility.reshade_mode->load          (config.compatibility.reshade_mode);
  compatibility.async_init->load            (config.compatibility.init_on_separate_thread);
  compatibility.disable_nv_bloat->load      (config.compatibility.disable_nv_bloat);
  compatibility.rehook_loadlibrary->load    (config.compatibility.rehook_loadlibrary);
  compatibility.using_wine->load            (config.compatibility.using_wine);
  compatibility.allow_dxdiagn->load         (config.compatibility.allow_dxdiagn);
                                             
  compatibility.sdl.allow_wgi->load         (config.compatibility.sdl.allow_wgi);
  compatibility.sdl.allow_raw_input->load   (config.compatibility.sdl.allow_raw_input); 
  compatibility.sdl.allow_direct_input->load(config.compatibility.sdl.allow_direct_input);
  compatibility.sdl.allow_xinput->load      (config.compatibility.sdl.allow_xinput);
  compatibility.sdl.allow_hid->load         (config.compatibility.sdl.allow_hid);
  compatibility.sdl.allow_all_ps_bt_features 
                                     ->load (config.compatibility.sdl.allow_all_ps_bt_features);
  compatibility.sdl.switch_led_brightness    
                                     ->load (config.compatibility.sdl.switch_led_brightness);
  compatibility.sdl.use_joystick_thread
                                     ->load (config.compatibility.sdl.use_joystick_thread);
  compatibility.sdl.poll_sentinel->load     (config.compatibility.sdl.poll_sentinel);

#ifdef _M_IX86
  compatibility.auto_large_address->load (config.compatibility.auto_large_address_patch);
#endif

  // D3D9Ex cannot exist without D3D9...
  if (config.apis.d3d9ex.hook)
      config.apis.d3d9.hook = true;

  // We need to enable D3D11 hooking for D3D12 to work reliably
  if (config.apis.dxgi.d3d12.hook)
      config.apis.dxgi.d3d11.hook = true;


  osd.version_banner.duration->load      (config.version_banner.duration);
  osd.state.remember->load               (config.osd.remember_state);

  imgui.scale->load                      (config.imgui.scale);
  imgui.show_eula->load                  (config.imgui.show_eula);
  imgui.show_playtime->load              (config.platform.show_playtime);
  imgui.show_gsync_status->load          (config.apis.NvAPI.gsync_status);
  imgui.mac_style_menu->load             (config.imgui.use_mac_style_menu);
  imgui.show_input_apis->load            (config.imgui.show_input_apis);
  imgui.center_cursor_on_overlay->load   (config.input.ui.center_cursor);
  imgui.nav_moves_mouse->load            (config.input.ui.nav_moves_mouse);

  imgui.disable_alpha->load              (config.imgui.render.disable_alpha);
  imgui.antialias_lines->load            (config.imgui.render.antialias_lines);
  imgui.antialias_contours->load         (config.imgui.render.antialias_contours);


  config.imgui.show_eula =
    app_cache_mgr->getLicenseRevision () !=
         SK_LICENSE_REVISION;


  if (((sk::iParameter *)monitoring.io.show)->load     () && config.osd.remember_state)
    config.io.show =     monitoring.io.show->get_value ();
                         monitoring.io.interval->load  (config.io.interval);

  monitoring.fps.show->load          (config.fps.show);
  monitoring.fps.frametime->load     (config.fps.frametime);
  monitoring.fps.framenumber->load   (config.fps.framenumber);
  monitoring.fps.advanced->load      (config.fps.advanced);
  monitoring.fps.compact->load       (config.fps.compact);
  monitoring.fps.compact_vrr->load   (config.fps.compact_vrr);
  monitoring.fps.timing_method->load (config.fps.timing_method);

  if (((sk::iParameter *)monitoring.memory.show)->load     () && config.osd.remember_state)
       config.mem.show = monitoring.memory.show->get_value ();
  mem_reserve->load (config.mem.reserve);

  if (((sk::iParameter *)monitoring.cpu.show)->load     () && config.osd.remember_state)
    config.cpu.show =    monitoring.cpu.show->get_value ();
                         monitoring.cpu.interval->load  (config.cpu.interval);
                         monitoring.cpu.simple->load    (config.cpu.simple);

  monitoring.gpu.show->load           (config.gpu.show);
  monitoring.gpu.print_slowdown->load (config.gpu.print_slowdown);
  monitoring.gpu.interval->load       (config.gpu.interval);

  if (((sk::iParameter *)monitoring.disk.show)->load     () && config.osd.remember_state)
    config.disk.show =   monitoring.disk.show->get_value ();
                         monitoring.disk.interval->load  (config.disk.interval);
                         monitoring.disk.type->load      (config.disk.type);

  monitoring.pagefile.interval->load (config.pagefile.interval);

  config.cpu.interval      = std::clamp (config.cpu.interval,      0.125f, 2.5f);
  config.gpu.interval      = std::clamp (config.gpu.interval,      0.125f, 2.5f);
  config.disk.interval     = std::clamp (config.disk.interval,     0.125f, 2.5f);
  config.pagefile.interval = std::clamp (config.pagefile.interval, 0.125f, 2.5f);
  config.io.interval       = std::clamp (config.io.interval,       0.125f, 2.5f);

  monitoring.dlss.show->load            (config.dlss.show);
  monitoring.dlss.show_output_res->load (config.dlss.show_output_res);
  monitoring.dlss.show_quality->load    (config.dlss.show_quality);
  monitoring.dlss.show_preset->load     (config.dlss.show_preset);
  monitoring.dlss.show_fg->load         (config.dlss.show_fg);

  monitoring.title.show->load (config.title.show);
  monitoring.time.show->load  (config.time.show);
  monitoring.SLI.show->load   (config.sli.show);

  if (nvidia.api.disable->load (config.apis.NvAPI.enable))
     config.apis.NvAPI.enable = (! nvidia.api.disable->get_value ());

  nvidia.api.disable_hdr->load    (config.apis.NvAPI.disable_hdr);
  nvidia.api.vulkan_bridge->load  (config.apis.NvAPI.vulkan_bridge);
  nvidia.bugs.snuffed_ansel->load (config.nvidia.bugs.snuffed_ansel);
  nvidia.bugs.bypass_ansel->load  (config.nvidia.bugs.bypass_ansel);
  nvidia.bugs.streamline_compat
                           ->load (config.compatibility.reshade_mode);

  if (amd.adl.disable->load (config.apis.ADL.enable))
     config.apis.ADL.enable = (! amd.adl.disable->get_value ());

  if (microsoft.d3dkmt.disable_perfdata->load (config.apis.D3DKMT.enable_perfdata))
     config.apis.D3DKMT.enable_perfdata = (! microsoft.d3dkmt.disable_perfdata->get_value ());

  reshade_cfg.draw_first->load              (config.reshade.draw_first);

  notifications.location->load              (config.notifications.location);
  notifications.silent->load                (config.notifications.silent);

  display.force_fullscreen->load            (config.display.force_fullscreen);
  display.force_windowed->load              (config.display.force_windowed);
  display.force_10bpc_sdr->load             (config.render.output.force_10bpc);
  display.aspect_ratio_stretch->load        (config.display.aspect_ratio_stretch);
  display.multimonitor_focus_is_focused
                                     ->load (config.display.focus_mode_if_focused);
  display.multimonitor_focus_mode->load     (config.display.focus_mode);
  display.confirm_mode_changes->load        (config.display.confirm_mode_changes);
  display.save_monitor_prefs->load          (config.display.save_monitor_prefs);
  display.save_resolution->load             (config.display.resolution.save);
  display.warn_no_mpo_planes->load          (config.display.warn_no_mpo_planes);

  if (config.display.resolution.save)
  {
    std::wstring                             override_str;
    display.override_resolution->load       (override_str);

    if ( 2 ==
           _snwscanf ( override_str.c_str  (),
                       override_str.length (), L"%ux%u",
                         &config.display.resolution.override.x,
                         &config.display.resolution.override.y ) )
    {
      ;
    }

    display.override_refresh->load          (config.display.refresh_rate);
  } display.allow_refresh_change->load      (config.display.allow_refresh_change);

  if (config.apis.NvAPI.vulkan_bridge)
  {
    SK_Vulkan_DisableThirdPartyLayers ();
  }

  std::wstring target_fps;

  if (render.framerate.target_fps->load (target_fps))
  {
    if (target_fps.find (L'/') != std::wstring::npos)
    {
      int numerator = 1, denominator = 1;

      swscanf (target_fps.c_str (), L"%i/%i", &numerator, &denominator);

      if (denominator != 0)
      {
        config.render.framerate.target_fps = static_cast <float> (
          (rb.windows.device.getDevCaps ().res.refresh * numerator) / denominator
        );
      }
    }

    else
    {
      swscanf (target_fps.c_str (), L"%f", &config.render.framerate.target_fps);
    }
  }

  std::wstring target_fps_bg;

  if (render.framerate.target_fps_bg->load (target_fps_bg))
  {
    if (target_fps_bg.find (L'/') != std::wstring::npos)
    {
      int numerator = 1, denominator = 1;

      swscanf (target_fps_bg.c_str (), L"%i/%i", &numerator, &denominator);

      if (denominator != 0)
      {
        config.render.framerate.target_fps_bg = static_cast <float> (
          (rb.windows.device.getDevCaps ().res.refresh * numerator) / denominator
        );
      }
    }

    else
    {
      swscanf (target_fps_bg.c_str (), L"%f", &config.render.framerate.target_fps_bg);
    }
  }

  render.framerate.last_refresh_rate->load    (config.render.framerate.last_refresh_rate);
  render.framerate.last_monitor_path->load    (config.render.framerate.last_monitor_path);
  render.framerate.sleepless_render->load     (config.render.framerate.sleepless_render);
  render.framerate.sleepless_window->load     (config.render.framerate.sleepless_window);
  render.framerate.enable_mmcss->load         (config.render.framerate.enable_mmcss);
  render.framerate.use_amd_mwaitx->load       (config.render.framerate.use_amd_mwaitx);

  if (! SK_CPU_HasMWAITX) // Turn off if CPU does not support
    config.render.framerate.use_amd_mwaitx = false;

  __target_fps    = config.render.framerate.target_fps;
  __target_fps_bg = config.render.framerate.target_fps_bg;

//  render.framerate.control.
//                  render_ahead->load        (config.render.framerate.max_render_ahead);
  render.framerate.override_cpu_count->load (config.render.framerate.override_num_cpus);

  render.framerate.latent_sync.offset->load   (config.render.framerate.latent_sync.scanline_offset);
  render.framerate.latent_sync.resync->load   (config.render.framerate.latent_sync.scanline_resync);
  render.framerate.latent_sync.error->load    (config.render.framerate.latent_sync.scanline_error);
  render.framerate.latent_sync.bias->load     (config.render.framerate.latent_sync.delay_bias);
  render.framerate.latent_sync.auto_bias->load(config.render.framerate.latent_sync.auto_bias);
  render.framerate.latent_sync.max_auto_bias
                                       ->load (config.render.framerate.latent_sync.max_auto_bias);

  std::wstring auto_bias_target;

  if (render.framerate.latent_sync.auto_bias_target->load (auto_bias_target))
  {
    if (auto_bias_target.find (L'%') != std::wstring::npos)
    {
      config.render.framerate.latent_sync.auto_bias_target.ms = 0.0f;
      swscanf (auto_bias_target.c_str (), L"%f%%", &config.render.framerate.latent_sync.auto_bias_target.percent);
      config.render.framerate.latent_sync.auto_bias_target.percent /= 100.0f;
    }

    else
    {
      config.render.framerate.latent_sync.auto_bias_target.percent = 0.0f;
      swscanf (auto_bias_target.c_str (), L"%f", &config.render.framerate.latent_sync.auto_bias_target.ms);
    }
  }

  render.osd.draw_in_vidcap->load            (config.render.osd. draw_in_vidcap);

  if (render.osd.hdr_luminance->load         (config.render.osd.hdr_luminance))
    rb.ui_luminance =                         config.render.osd.hdr_luminance;

  // D3D9/11
  //

  nvidia.sli.compatibility->load             (config.nvidia.sli.compatibility);
  nvidia.sli.mode->load                      (config.nvidia.sli.mode);
  nvidia.sli.num_gpus->load                  (config.nvidia.sli.num_gpus);
  nvidia.sli.override->load                  (config.nvidia.sli.override);

  nvidia.reflex.enable->load                 (config.nvidia.reflex.enable);
  nvidia.reflex.low_latency->load            (config.nvidia.reflex.low_latency);
  nvidia.reflex.low_latency_boost->load      (config.nvidia.reflex.low_latency_boost);
  nvidia.reflex.marker_optimization->load    (config.nvidia.reflex.marker_optimization);
  nvidia.reflex.engagement_policy->load      (config.nvidia.reflex.enforcement_site);
  nvidia.reflex.override_native->load        (config.nvidia.reflex.override);
  nvidia.reflex.use_limiter->load            (config.nvidia.reflex.use_limiter);
  nvidia.reflex.combined_limiter->load       (config.nvidia.reflex.combined_limiter);
  nvidia.reflex.disable_native->load         (config.nvidia.reflex.disable_native);
  nvidia.reflex.show_detailed_widget->load   (config.nvidia.reflex.show_detailed_widget);

  nvidia.dlss.force_dlaa->load               (config.nvidia.dlss.force_dlaa);
  nvidia.dlss.use_sharpening->load           (config.nvidia.dlss.use_sharpening);
  nvidia.dlss.forced_sharpness->load         (config.nvidia.dlss.forced_sharpness);
  nvidia.dlss.auto_redirect_dll->load        (config.nvidia.dlss.auto_redirect_dlss);
  nvidia.dlss.forced_preset->load            (config.nvidia.dlss.forced_preset);
  nvidia.dlss.forced_auto_exposure->load     (config.nvidia.dlss.forced_auto_exposure);
  nvidia.dlss.forced_alpha_upscale->load     (config.nvidia.dlss.forced_alpha_upscale);
  nvidia.dlss.performance_scale->load        (config.nvidia.dlss.scale.performance);
  nvidia.dlss.balanced_scale->load           (config.nvidia.dlss.scale.balanced);
  nvidia.dlss.quality_scale->load            (config.nvidia.dlss.scale.quality);
  nvidia.dlss.ultra_performance_scale->load  (config.nvidia.dlss.scale.ultra_performance);
  nvidia.dlss.dynamic_resolution_min->load   (config.nvidia.dlss.scale.dynamic_min);
  nvidia.dlss.dynamic_resolution_max->load   (config.nvidia.dlss.scale.dynamic_max);
  nvidia.dlss.extra_pixels->load             (config.nvidia.dlss.compat.extra_pixels);
  nvidia.dlss.override_appid->load           (config.nvidia.dlss.compat.override_appid);
  nvidia.dlss.disable_ota_updates->load      (config.nvidia.dlss.disable_ota_updates);
  nvidia.dlss.show_active_features->load     (config.nvidia.dlss.show_active_features);
  nvidia.dlss.allow_scrgb->load              (config.nvidia.dlss.allow_scrgb);
  nvidia.dlss.spoof_feature_support->load    (config.nvidia.dlss.spoof_support);

  render.hdr.enable_32bpc->load              (config.render.hdr.enable_32bpc);
  render.hdr.remaster_8bpc_as_unorm->load    (config.render.hdr.remaster_8bpc_as_unorm);
  render.hdr.remaster_subnative_unorm->load  (config.render.hdr.remaster_subnative_as_unorm);
  render.hdr.last_used_colorspace->load      (config.render.hdr.last_used_colorspace);

  render.framerate.wait_for_vblank->load     (config.render.framerate.wait_for_vblank);
  render.framerate.buffer_count->load        (config.render.framerate.buffer_count);
  render.framerate.prerender_limit->load     (config.render.framerate.pre_render_limit);
  render.framerate.present_interval->load    (config.render.framerate.present_interval);
  render.framerate.sync_interval_clamp->load (config.render.framerate.sync_interval_clamp);
  render.framerate.tearing_mode->load        (config.render.framerate.tearing_mode);

  if (render.framerate.refresh_rate)
  {
    render.framerate.refresh_rate->load      (config.render.framerate.refresh_rate);
  }

  if (render.framerate.rescan_ratio)
  {
    render.framerate.rescan_ratio->load      (config.render.framerate.rescan_ratio);

    if (! config.render.framerate.rescan_ratio.empty ())
    {
      swscanf (config.render.framerate.rescan_ratio.c_str (), L"%i/%i", (INT*)&config.render.framerate.rescan_.Numerator,
                                                                        (INT*)&config.render.framerate.rescan_.Denom);
    }

    if ( config.render.framerate.rescan_.Numerator != (-1) &&
         config.render.framerate.rescan_.Denom     !=   0 )
    {
      config.render.framerate.refresh_rate =
        sk::narrow_cast <float> (config.render.framerate.rescan_.Numerator) /
        sk::narrow_cast <float> (config.render.framerate.rescan_.Denom);
    }
  }

  scheduling.priority.raise_always->load        (config.priority.raise_always);
  scheduling.priority.raise_in_bg->load         (config.priority.raise_bg);
  scheduling.priority.raise_in_fg->load         (config.priority.raise_fg);
  scheduling.priority.highest_priority->load    (config.priority.highest_priority);
  scheduling.priority.deny_foreign_change->load (config.priority.deny_foreign_change);
  scheduling.priority.min_render_priority->load (config.priority.minimum_render_prio);
  scheduling.priority.cpu_affinity_mask->load   (config.priority.cpu_affinity_mask);

  if (config.priority.cpu_affinity_mask != -1)
  {
    SK_SetProcessAffinityMask (GetCurrentProcess (), (DWORD_PTR)config.priority.cpu_affinity_mask);
  }

  scheduling.priority.perf_cores_only->load     (config.priority.perf_cores_only);

  if (config.priority.raise_always)
    SetPriorityClass (GetCurrentProcess (), ABOVE_NORMAL_PRIORITY_CLASS);

  render.framerate.enforcement_policy->load (config.render.framerate.enforcement_policy);
  render.framerate.enable_etw_tracing->load (config.render.framerate.enable_etw_tracing);

  // Check for PresentMon ETW Trace Permissions if Enabled
  if (config.render.framerate.enable_etw_tracing)
  {
    static BOOL  pfuAccessToken = FALSE;
    static BYTE  pfuSID [SECURITY_MAX_SID_SIZE];
    static DWORD pfuSize = sizeof (pfuSID);

    SK_RunOnce (CreateWellKnownSid   (WELL_KNOWN_SID_TYPE::WinBuiltinPerfLoggingUsersSid, NULL, &pfuSID, &pfuSize));
    SK_RunOnce (CheckTokenMembership (NULL, &pfuSID, &pfuAccessToken));

    if (pfuAccessToken)
    {
      config.render.framerate.supports_etw_trace = true;
    }
  }

  render.d3d9.force_d3d9ex->load        (config.render.d3d9.force_d3d9ex);
  render.d3d9.impure->load              (config.render.d3d9.force_impure);
  render.d3d9.enable_flipex->load       (config.render.d3d9.enable_flipex);
  render.d3d9.use_d3d9on12->load        (config.render.d3d9.use_d3d9on12);
  render.d3d9.enable_texture_mods->load (config.textures.d3d9_mod);
  texture.d3d9.clamp_lod_bias->load     (config.textures.clamp_lod_bias);


  // OpenGL
  //
  if (! render.gl.enable_debug->load (config.render.gl.debug.enable))
#ifdef DEBUG
    config.render.gl.debug.enable = true;
#else
    config.render.gl.debug.enable = false;
#endif
  render.gl.prefer_10bpc->load    (config.render.gl.prefer_10bpc);
  render.gl.upgrade_zbuffer->load (config.render.gl.upgrade_zbuffer);


  // DXGI
  //
  render.framerate.max_delta_time->load   (config.render.framerate.max_delta_time);

  if (render.framerate.flip_discard->load (config.render.framerate.flip_discard) && config.render.framerate.flip_discard)
  {
    config.render.framerate.disable_flip = false;
  }

  render.framerate.allow_dwm_tearing->load (config.render.dxgi.allow_tearing);
  render.framerate.flip_sequential->load   (config.render.framerate.flip_sequential);

  render.framerate.drop_late_frames->load  (config.render.framerate.drop_late_flips);
  render.framerate.auto_low_latency_optin->
                                     load  (config.render.framerate.auto_low_latency.policy.global_opt);
  render.framerate.
             auto_low_latency_reapply->load(config.render.framerate.auto_low_latency.policy.auto_reapply);

  bool auto_low_latency_preset =
  render.framerate.auto_low_latency->load  (config.render.framerate.auto_low_latency.waiting);
  render.framerate.auto_low_latency_ex->
                                     load  (config.render.framerate.auto_low_latency.policy.ultra_low_latency);

  if (! auto_low_latency_preset)
    config.render.framerate.auto_low_latency.waiting = config.render.framerate.auto_low_latency.policy.global_opt;

  render.framerate.auto_vrr_triggered->load(config.render.framerate.auto_low_latency.triggered);

  if (render.framerate.disable_flip_model->load (config.render.framerate.disable_flip))
  {
    if (config.render.framerate.disable_flip)
    {
      if (config.render.framerate.flip_discard)
        SK_ImGui_Warning (L"Conflicting Flip Model Overrides Detected (Disable and Force)");

      config.render.framerate.flip_discard = false;
      config.render.dxgi.allow_tearing     = false;
    }
  }

  render.dxgi.adapter_override->load (config.render.dxgi.adapter_override);

  if (((sk::iParameter *)render.dxgi.max_res)->load ())
  {
    swscanf ( render.dxgi.max_res->get_value_str ().c_str (),
                L"%ux%u",
                &config.render.dxgi.res.max.x,
                  &config.render.dxgi.res.max.y );
  }
  if (((sk::iParameter *)render.dxgi.min_res)->load ())
  {
    swscanf ( render.dxgi.min_res->get_value_str ().c_str (),
                L"%ux%u",
                &config.render.dxgi.res.min.x,
                  &config.render.dxgi.res.min.y );
  }

  render.dxgi.max_refresh->load (config.render.dxgi.refresh.max);
  render.dxgi.min_refresh->load (config.render.dxgi.refresh.min);

  //
  // MUST establish sanity
  //
  //  * If both limits exist, then ( max.x >= min.x  &&  max.y >= min.y )
  //
  bool sane = true;

  if ( config.render.dxgi.res.max.x != 0 &&
       config.render.dxgi.res.min.x >
       config.render.dxgi.res.max.x ) sane = false;
  if ( config.render.dxgi.res.min.x != 0 &&
       config.render.dxgi.res.max.x <
       config.render.dxgi.res.min.x ) sane = false;

  if ( config.render.dxgi.res.max.y != 0 &&
       config.render.dxgi.res.min.y >
       config.render.dxgi.res.max.y ) sane = false;
  if ( config.render.dxgi.res.min.y != 0 &&
       config.render.dxgi.res.max.y <
       config.render.dxgi.res.min.y ) sane = false;

  // Discard the lower-bound and hope for the best
  if (! sane) { config.render.dxgi.res.min.x = 0;
                config.render.dxgi.res.min.y = 0;

    // Correct handling involves actually querying supported resolutions
    //   from the display device, but that's not practical at the time
    //     the config file is parsed.
  };



  if (((sk::iParameter *)render.dxgi.scaling_mode)->load ())
  {
    std::wstring&& mode =
      render.dxgi.scaling_mode->get_value_str ();

    if (! _wcsicmp (mode.c_str (), L"Unspecified"))
    {
      config.render.dxgi.scaling_mode =
        DXGI_MODE_SCALING_UNSPECIFIED;
    }

    else if (! _wcsicmp (mode.c_str (), L"Centered"))
    {
      config.render.dxgi.scaling_mode =
        DXGI_MODE_SCALING_CENTERED;
    }

    else if (! _wcsicmp (mode.c_str (), L"Stretched"))
    {
      config.render.dxgi.scaling_mode =
        DXGI_MODE_SCALING_STRETCHED;
    }
  }

  if (((sk::iParameter *)render.dxgi.scanline_order)->load ())
  {
    std::wstring&& order =
      render.dxgi.scanline_order->get_value_str ();

    if (! _wcsicmp (order.c_str (), L"Unspecified"))
    {
      config.render.dxgi.scanline_order =
        DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
    }

    else if (! _wcsicmp (order.c_str (), L"Progressive"))
    {
      config.render.dxgi.scanline_order =
        DXGI_MODE_SCANLINE_ORDER_PROGRESSIVE;
    }

    else if (! _wcsicmp (order.c_str (), L"LowerFieldFirst"))
    {
      config.render.dxgi.scanline_order =
        DXGI_MODE_SCANLINE_ORDER_LOWER_FIELD_FIRST;
    }

    else if (! _wcsicmp (order.c_str (), L"UpperFieldFirst"))
    {
      config.render.dxgi.scanline_order =
        DXGI_MODE_SCANLINE_ORDER_UPPER_FIELD_FIRST;
    }

    // If a user specifies Interlaced, default to Lower Field First
    else if (! _wcsicmp (order.c_str (), L"Interlaced"))
    {
      config.render.dxgi.scanline_order =
        DXGI_MODE_SCANLINE_ORDER_LOWER_FIELD_FIRST;
    }
  }

  render.dxgi.vram_budget_scale->load    (config.render.dxgi.vram_budget_scale);
  render.dxgi.fake_fullscreen_mode->load (config.render.dxgi.fake_fullscreen_mode);
  render.dxgi.allow_d3d12_footguns->load (config.render.dxgi.allow_d3d12_footguns);
  render.dxgi.debug_layer->load          (config.render.dxgi.debug_layer);

  if (((sk::iParameter *)render.dxgi.exception_mode)->load ())
  {
    std::wstring&& except_mode =
      render.dxgi.exception_mode->get_value_str ();

    if (! _wcsicmp (except_mode.c_str (), L"Raise"))
    {
      #define D3D11_RAISE_FLAG_DRIVER_INTERNAL_ERROR 1
      config.render.dxgi.exception_mode = D3D11_RAISE_FLAG_DRIVER_INTERNAL_ERROR;
    }

    else if (! _wcsicmp (except_mode.c_str (), L"Ignore"))
    {
      config.render.dxgi.exception_mode = 0;
    }
    else
      config.render.dxgi.exception_mode = SK_NoPreference;
  }

  render.dxgi.test_present->load         (config.render.dxgi.test_present);
  render.dxgi.swapchain_wait->load       (config.render.framerate.swapchain_wait);

  render.dxgi.enhanced_depth->load       (config.render.dxgi.enhanced_depth);
  render.dxgi.deferred_isolation->load   (config.render.dxgi.deferred_isolation);
  render.dxgi.skip_present_test->load    (config.render.dxgi.present_test_skip);
  render.dxgi.msaa_samples->load         (config.render.dxgi.msaa_samples);
  render.dxgi.srgb_behavior->load        (config.render.dxgi.srgb_behavior);
  render.dxgi.low_spec_mode->load        (config.render.dxgi.low_spec_mode);
  render.dxgi.hide_hdr_support->load     (config.render.dxgi.hide_hdr_support);
  render.dxgi.hdr_metadata_override->load(config.render.dxgi.hdr_metadata_override);
  render.dxgi.temporary_dwm_hdr->load    (config.render.dxgi.temporary_dwm_hdr);
  render.dxgi.disable_virtual_vbi->load  (config.render.dxgi.disable_virtual_vbi);
  render.dxgi.clear_buffers_after_flip->
                                    load (config.render.dxgi.clear_flipped_chain);
  render.dxgi.enable_factory_cache->load (config.render.dxgi.use_factory_cache);
  render.dxgi.skip_redundant_modes->load (config.render.dxgi.skip_mode_changes);
  render.dxgi.warn_if_vram_exceeds->load (config.render.dxgi.warn_if_vram_exceeds);

  // Tie wrapping device contexts to the costly Deferred Isolation setting, as
  //   a last-ditch effort to get render mod tools working despite perf. overhead.
  if (config.render.dxgi.deferred_isolation)
    config.render.d3d11.wrap_d3d11_dev_ctx = true;

  render.d3d12.max_anisotropy->load      (config.render.d3d12.max_anisotropy);
  render.d3d12.force_anisotropic->load   (config.render.d3d12.force_anisotropic);
  render.d3d12.force_lod_bias->load      (config.render.d3d12.force_lod_bias);

  config.render.d3d12.force_lod_bias = // TODO: What are the actual limits?
    std::clamp (config.render.d3d12.force_lod_bias, -32.0f, 32.0f);

  render.dstorage.disable_bypass_io->load(config.render.dstorage.disable_bypass_io);
  render.dstorage.disable_telemetry->load(config.render.dstorage.disable_telemetry);
  render.dstorage.disable_gpu_decomp->
                                    load (config.render.dstorage.disable_gpu_decomp);
  render.dstorage.force_file_buffering->
                                    load (config.render.dstorage.force_file_buffering);
  render.dstorage.submit_threads->  load (config.render.dstorage.submit_threads);
  render.dstorage.cpu_decomp_threads->
                                    load (config.render.dstorage.cpu_decomp_threads);
  render.dstorage.hook_dstorage->   load (config.render.dstorage.enable_hooks);

  texture.d3d11.cache->load              (config.textures.d3d11.cache);
  texture.d3d11.use_l3_hash->load        (config.textures.d3d11.use_l3_hash);
  texture.d3d11.precise_hash->load       (config.textures.d3d11.precise_hash);
  texture.d3d11.inject->load             (config.textures.d3d11.inject);
        texture.res_root->load           (config.textures.d3d11.res_root);

  SK_RunOnce (config.textures.d3d11.orig_cache = config.textures.d3d11.cache);

  texture.d3d11.injection_keeps_format->
                                   load  (config.textures.d3d11.injection_keeps_fmt);
             texture.dump_on_load->load  (config.textures.d3d11.dump);
             texture.dump_on_load->load  (config.textures.dump_on_load);

  texture.d3d11.gen_mips->load           (config.textures.d3d11.generate_mips);

  texture.cache.max_entries->load        (config.textures.cache.max_entries);
  texture.cache.min_entries->load        (config.textures.cache.min_entries);
  texture.cache.max_evict->load          (config.textures.cache.max_evict);
  texture.cache.min_evict->load          (config.textures.cache.min_evict);
  texture.cache.max_size->load           (config.textures.cache.max_size);
  texture.cache.min_size->load           (config.textures.cache.min_size);
  texture.cache.ignore_non_mipped->load  (config.textures.cache.ignore_nonmipped);
  texture.cache.allow_staging->load      (config.textures.cache.allow_staging);
  texture.cache.allow_unsafe_refs->load  (config.textures.cache.allow_unsafe_refs);
  texture.cache.manage_residency->load   (config.textures.cache.residency_managemnt);

  if (config.render.dxgi.adapter_override != SK_NoPreference)
    SK_DXGI_SetPreferredAdapter (config.render.dxgi.adapter_override);

  input.keyboard.catch_alt_f4->load      (config.input.keyboard.catch_alt_f4);
  input.keyboard.bypass_alt_f4->load     (config.input.keyboard.override_alt_f4);
  input.keyboard.disabled_to_game->load  (config.input.keyboard.disabled_to_game);
  config.input.keyboard.
                    org_disabled_to_game= config.input.keyboard.disabled_to_game;

  input.mouse.disabled_to_game->load     (config.input.mouse.disabled_to_game);
  config.input.mouse.
                 org_disabled_to_game =   config.input.mouse.disabled_to_game;

  input.cursor.manage->load              (config.input.cursor.manage);
  input.cursor.keys_activate->load       (config.input.cursor.keys_activate);
  input.cursor.gamepad_deactivates->load (config.input.cursor.gamepad_deactivates);

                            float fTimeout;
  if (input.cursor.timeout->load (fTimeout))
    config.input.cursor.timeout = (int)(1000.0 * fTimeout);

  input.cursor.ui_capture->load          (config.input.ui.capture);
  input.cursor.hw_cursor->load           (config.input.ui.use_hw_cursor);
  input.cursor.block_invisible->load     (config.input.ui.capture_hidden);
  input.cursor.fix_synaptics->load       (config.input.mouse.fix_synaptics);

  input.gamepad.disabled_to_game->load   (config.input.gamepad.disabled_to_game);
  input.gamepad.disable_hid->load        (config.input.gamepad.disable_hid);
  input.gamepad.disable_winmm->load      (config.input.gamepad.disable_winmm);
  input.gamepad.rehook_xinput->load      (config.input.gamepad.rehook_xinput);
  input.gamepad.hook_xinput->load        (config.input.gamepad.hook_xinput);
  input.gamepad.hook_scepad->load        (config.input.gamepad.hook_scepad);

  // Hidden INI values; they're loaded, but never written
  input.gamepad.hook_winmm->load         (config.input.gamepad.hook_winmm);
  input.gamepad.hook_game_input->load    (config.input.gamepad.hook_game_input);
  input.gamepad.allow_steam_winmm->load  (config.input.gamepad.allow_steam_winmm);
  input.gamepad.hook_windows_gaming->load(config.input.gamepad.hook_windows_gaming);
  input.gamepad.hook_raw_input->load     (config.input.gamepad.hook_raw_input);
  input.gamepad.hook_dinput8->load       (config.input.gamepad.hook_dinput8);
  input.gamepad.hook_dinput7->load       (config.input.gamepad.hook_dinput7);
  input.gamepad.hook_hid->load           (config.input.gamepad.hook_hid);

  SK_RunOnce (hook_xinput_orig    = config.input.gamepad.hook_xinput);
  SK_RunOnce (hook_scepad_orig    = config.input.gamepad.hook_scepad);
  SK_RunOnce (hook_wgi_orig       = config.input.gamepad.hook_windows_gaming);
  SK_RunOnce (hook_raw_input_orig = config.input.gamepad.hook_raw_input);
  SK_RunOnce (hook_dinput7_orig   = config.input.gamepad.hook_dinput7);
  SK_RunOnce (hook_dinput8_orig   = config.input.gamepad.hook_dinput8);
  SK_RunOnce (hook_hid_orig       = config.input.gamepad.hook_hid);
  SK_RunOnce (hook_winmm_orig     = config.input.gamepad.hook_winmm);

  input.gamepad.haptic_ui->load          (config.input.gamepad.haptic_ui);

  int placeholder_mask;

  if (input.gamepad.xinput.placeholders->load (placeholder_mask))
  {
    config.input.gamepad.xinput.placehold [0] = ( placeholder_mask & 0x1 );
    config.input.gamepad.xinput.placehold [1] = ( placeholder_mask & 0x2 );
    config.input.gamepad.xinput.placehold [2] = ( placeholder_mask & 0x4 );
    config.input.gamepad.xinput.placehold [3] = ( placeholder_mask & 0x8 );
  }

  input.gamepad.hid.max_allowed_buffers->load  (config.input.gamepad.hid.max_allowed_buffers);
  input.gamepad.bt_input_only->load            (config.input.gamepad.bt_input_only);
  input.gamepad.disable_rumble->load           (config.input.gamepad.disable_rumble);
  input.gamepad.blocks_screensaver->load       (config.input.gamepad.blocks_screensaver);
  input.gamepad.left_impulse_strength->load    (config.input.gamepad.impulse_strength_l);
  input.gamepad.right_impulse_strength->load   (config.input.gamepad.impulse_strength_r);
  input.gamepad.left_resist_strength->load     (config.input.gamepad.dualsense.resist_strength_l);
  input.gamepad.right_resist_strength->load    (config.input.gamepad.dualsense.resist_strength_r);
  input.gamepad.left_resist_start->load        (config.input.gamepad.dualsense.resist_start_l);
  input.gamepad.right_resist_start->load       (config.input.gamepad.dualsense.resist_start_r);
  input.gamepad.xinput.hook_setstate->load     (config.input.gamepad.xinput.hook_setstate);
  input.gamepad.xinput.auto_slot_assign->load  (config.input.gamepad.xinput.auto_slot_assign);
  input.gamepad.xinput.blackout_api->load      (config.input.gamepad.xinput.blackout_api);
  input.gamepad.xinput.emulate->load           (config.input.gamepad.xinput.emulate);
  input.gamepad.xinput.deadzone->load          (config.input.gamepad.xinput.deadzone);
  input.gamepad.dinput.blackout_gamepads->load (config.input.gamepad.dinput.blackout_gamepads);
  input.gamepad.dinput.blackout_mice->load     (config.input.gamepad.dinput.blackout_mice);
  input.gamepad.dinput.blackout_keyboards->load(config.input.gamepad.dinput.blackout_keyboards);
  input.gamepad.dinput.block_enum_devices->load(config.input.gamepad.dinput.block_enum_devices);

  auto& scratch_mem =
    SK_TLS_Bottom ()->scratch_memory->log.formatted_output;

  if (((sk::iParameter *)input.gamepad.xinput.assignment)->load ())
  {
    sk::ParameterStringW* pAssignment =
      input.gamepad.xinput.assignment;

    wchar_t *wszAssign = scratch_mem.alloc (
               pAssignment->get_value ().length () + 2, true );

    memcpy ( wszAssign, pAssignment->get_value ().c_str (),
                        pAssignment->get_value ().length () * sizeof (wchar_t) );

    wchar_t* wszBuf = nullptr;
    wchar_t* wszTok =
      std::wcstok (wszAssign, L",", &wszBuf);

    if (wszTok == nullptr)
    {
      config.input.gamepad.xinput.assignment [0] = 0;
      config.input.gamepad.xinput.assignment [1] = 1;
      config.input.gamepad.xinput.assignment [2] = 2;
      config.input.gamepad.xinput.assignment [3] = 3;
    }

    int idx = 0;

    while (wszTok != nullptr && idx < 4)
    {
      config.input.gamepad.xinput.assignment [idx++] =
        _wtoi (wszTok);

      wszTok =
        std::wcstok (nullptr, L",", &wszBuf);
    }
  }

  if (((sk::iParameter *)input.gamepad.xinput.disable_slots)->load ())
  {
    sk::ParameterStringW* pDisable =
      input.gamepad.xinput.disable_slots;

    wchar_t *wszDisable = scratch_mem.alloc (
        pDisable->get_value ().length () + 2, true );

    memcpy ( wszDisable, pDisable->get_value ().c_str (),
                         pDisable->get_value ().length () * sizeof (wchar_t) );

    wchar_t* wszBuf = nullptr;
    wchar_t* wszTok =
      std::wcstok (wszDisable, L",", &wszBuf);

    if (wszTok == nullptr)
    {
      config.input.gamepad.xinput.disable [0] = false;
      config.input.gamepad.xinput.disable [1] = false;
      config.input.gamepad.xinput.disable [2] = false;
      config.input.gamepad.xinput.disable [3] = false;
    }

    while (wszTok != nullptr)
    {
      config.input.gamepad.xinput.disable [std::clamp (_wtoi (wszTok), 0, 3)] =
        true;

      wszTok =
        std::wcstok (nullptr, L",", &wszBuf);
    }
  }

  input.gamepad.hook_scepad->load                 (config.input.gamepad.hook_scepad);
  input.gamepad.scepad.disable_touchpad->load     (config.input.gamepad.scepad.disable_touch);
  input.gamepad.scepad.share_clicks_touch->load   (config.input.gamepad.scepad.share_clicks_touch);
  input.gamepad.scepad.mute_applies_to_game->load (config.input.gamepad.scepad.mute_applies_to_game);
  input.gamepad.scepad.enhanced_ps_button->load   (config.input.gamepad.scepad.enhanced_ps_button);
  input.gamepad.scepad.power_save_mode->load      (config.input.gamepad.scepad.power_save_mode);
  input.gamepad.scepad.led_color_r->load          (config.input.gamepad.scepad.led_color_r);
  input.gamepad.scepad.led_color_g->load          (config.input.gamepad.scepad.led_color_g);
  input.gamepad.scepad.led_color_b->load          (config.input.gamepad.scepad.led_color_b);
  input.gamepad.scepad.led_brightness->load       (config.input.gamepad.scepad.led_brightness);
  input.gamepad.scepad.show_ds4_as_ds4_v2->load   (config.input.gamepad.scepad.show_ds4_v1_as_v2);
  input.gamepad.scepad.hide_ds4_v2_pid->load      (config.input.gamepad.scepad.hide_ds4_v2_pid);
  input.gamepad.scepad.hide_ds_edge_pid->load     (config.input.gamepad.scepad.hide_ds_edge_pid);
  input.gamepad.scepad.enable_full_bluetooth->load(config.input.gamepad.scepad.enable_full_bluetooth);
  input.gamepad.scepad.left_fn_bind->load         (config.input.gamepad.scepad.left_fn);
  input.gamepad.scepad.left_paddle_bind->load     (config.input.gamepad.scepad.left_paddle);
  input.gamepad.scepad.right_paddle_bind->load    (config.input.gamepad.scepad.right_paddle);
  input.gamepad.scepad.right_fn_bind->load        (config.input.gamepad.scepad.right_fn);
  input.gamepad.scepad.touch_click_bind->load     (config.input.gamepad.scepad.touch_click);

  input.gamepad.low_battery_warning->load         (config.input.gamepad.low_battery_percent);

  input.gamepad.xinput.ui_slot->load   ((int &)config.input.gamepad.xinput.ui_slot);
  input.gamepad.steam.disable->load    (config.input.gamepad.steam.disabled_to_game);


  threads.enable_mem_alloc_trace->load (config.threads.enable_mem_alloc_trace);
  threads.enable_file_io_trace->load   (config.threads.enable_file_io_trace);

  window.borderless->load              (config.window.borderless);

  window.center->load                  (config.window.center);
  window.background_render->load       (config.window.background_render);
  window.background_mute->load         (config.window.background_mute);

  // "Fake Fullscreen" requires background rendering
  if (config.render.dxgi.fake_fullscreen_mode)
      config.window.background_render = true;

  std::wstring offset;

  if (window.offset.x->load (offset))
  {
    if (offset.find (L'%') != std::wstring::npos)
    {
      config.window.offset.x.absolute = 0;
      swscanf (offset.c_str (), L"%f%%", &config.window.offset.x.percent);
      config.window.offset.x.percent /= 100.0f;
    }

    else
    {
      config.window.offset.x.percent = 0.0f;
      swscanf (offset.c_str (), L"%i", &config.window.offset.x.absolute);
    }
  }

  if (window.offset.y->load (offset))
  {
    if (offset.find (L'%') != std::wstring::npos)
    {
      config.window.offset.y.absolute = 0;
      swscanf (offset.c_str (), L"%f%%", &config.window.offset.y.percent);
      config.window.offset.y.percent /= 100.0f;
    }

    else
    {
      config.window.offset.y.percent = 0.0f;
      swscanf (offset.c_str (), L"%i", &config.window.offset.y.absolute);
    }
  }

  window.confine_cursor->load      (config.window.confine_cursor);
  window.unconfine_cursor->load    (config.window.unconfine_cursor);
  window.persistent_drag->load     (config.window.persistent_drag);
  window.fullscreen->load          (config.window.fullscreen);
  window.fix_mouse_coords->load    (config.window.res.override.fix_mouse);
  window.always_on_top->load       (config.window.always_on_top);
  window.disable_screensaver->load (config.window.disable_screensaver);
  window.fullscreen_no_saver->load (config.window.fullscreen_no_saver);
  window.manage_screensaver->load  (config.window.manage_screensaver);
  window.dont_hook_wndproc->load   (config.window.dont_hook_wndproc);
  window.activate_at_start->load   (config.window.activate_at_start);
  window.treat_fg_as_active->load  (config.window.treat_fg_as_active);

  if (config.window.fullscreen && (! config.window.borderless))
  {
    SK_LOGi0 (L"Invalid Combination of Fullscreen + Not Borderless, forcing Borderless!");
    config.window.borderless = true;
  }


  // Oh boy, let the fun begin :)
  //
  //   Use CCD API to map precise VidPn path names to much easier to work with
  //     GDI monitor names
  //
  window.preferred_monitor_exact->load (config.display.monitor_path_ccd);

  bool found_exact = false;

  extern LONG
  __stdcall
  SK_ChangeDisplaySettingsEx ( _In_ LPCWSTR   lpszDeviceName,
                               _In_ DEVMODEW *lpDevMode,
                                    HWND      hWnd,
                               _In_ DWORD     dwFlags,
                               _In_ LPVOID    lParam );

  if (! config.display.monitor_path_ccd.empty ())
  {
    static UINT32 uiNumPaths =  128;
    static UINT32 uiNumModes = 1024;

    static DISPLAYCONFIG_PATH_INFO *pathArray = nullptr;
    static DISPLAYCONFIG_MODE_INFO *modeArray = nullptr;

    static std::map <std::wstring, HMONITOR>
                      _PathDeviceToHMONITOR;

    if (_PathDeviceToHMONITOR.empty ())
    {
      if ( ERROR_SUCCESS ==
             GetDisplayConfigBufferSizes ( QDC_ONLY_ACTIVE_PATHS, &uiNumPaths,          &uiNumModes )
                                                                && uiNumPaths <= 128 &&  uiNumModes <= 1024 )
      {
        SK_TLS *pTLS =
          SK_TLS_Bottom ();

        pathArray = (DISPLAYCONFIG_PATH_INFO *)pTLS->scratch_memory->ccd.display_paths.alloc (uiNumPaths);
        modeArray = (DISPLAYCONFIG_MODE_INFO *)pTLS->scratch_memory->ccd.display_modes.alloc (uiNumModes);

        if ( ERROR_SUCCESS != QueryDisplayConfig ( QDC_ONLY_ACTIVE_PATHS, &uiNumPaths, pathArray,
                                                                          &uiNumModes, modeArray, nullptr ) )
        {
          SK_ReleaseAssert (! "QueryDisplayConfig (QDC_ONLY_ACTIVE_PATHS");
        }
      }
    }

    static BOOL bEnumerated =
    EnumDisplayMonitors (nullptr, nullptr, [](HMONITOR hMonitor, HDC hDC, LPRECT lpRect, LPARAM lParam) -> BOOL
    {
      std::ignore = hDC;
      std::ignore = lpRect;
      std::ignore = lParam;

      MONITORINFOEXW
        mi        = {         };
        mi.cbSize = sizeof (mi);

      if ( GetMonitorInfoW (hMonitor, &mi) && pathArray != nullptr &&
                                              modeArray != nullptr )
      {
        float bestIntersectArea = -1.0f;

        int ax1 = mi.rcMonitor.left,
            ax2 = mi.rcMonitor.right;
        int ay1 = mi.rcMonitor.top,
            ay2 = mi.rcMonitor.bottom;

        DISPLAYCONFIG_PATH_INFO *pVidPn = nullptr;

        for (UINT32 pathIdx = 0; pathIdx < uiNumPaths; ++pathIdx)
        {
          auto *path =
            &pathArray [pathIdx];

          if ( path != nullptr &&
              (path->flags & DISPLAYCONFIG_PATH_ACTIVE) )
          {
            DISPLAYCONFIG_SOURCE_MODE *pSourceMode =
              &modeArray [path->sourceInfo.modeInfoIdx].sourceMode;

            RECT rect =
              { pSourceMode->position.x,
                pSourceMode->position.y,
                pSourceMode->position.x + sk::narrow_cast <LONG> (pSourceMode->width),
                pSourceMode->position.y + sk::narrow_cast <LONG> (pSourceMode->height) };

            if (! IsRectEmpty (&rect))
            {
              int bx1 = rect.left;
              int by1 = rect.top;
              int bx2 = rect.right;
              int by2 = rect.bottom;

              int intersectArea =
                ComputeIntersectionArea (ax1, ay1, ax2, ay2, bx1, by1, bx2, by2);

              if (intersectArea > bestIntersectArea)
              {
                pVidPn            = path;
                bestIntersectArea =
                  static_cast <float> (intersectArea);
              }
            }
          }
        }

        if (pVidPn != nullptr)
        {
          DISPLAYCONFIG_TARGET_DEVICE_NAME
            getTargetName                  = { };
            getTargetName.header.type      = DISPLAYCONFIG_DEVICE_INFO_GET_TARGET_NAME;
            getTargetName.header.size      =  sizeof (DISPLAYCONFIG_TARGET_DEVICE_NAME);
            getTargetName.header.adapterId = pVidPn->targetInfo.adapterId;
            getTargetName.header.id        = pVidPn->targetInfo.id;

          if ( ERROR_SUCCESS == SK_DisplayConfigGetDeviceInfo ( (DISPLAYCONFIG_DEVICE_INFO_HEADER *)&getTargetName ) )
          {
            _PathDeviceToHMONITOR [getTargetName.monitorDevicePath] = hMonitor;
          }
        }
      }

      return TRUE;
    }, 0);

    MONITORINFOEXW
    mi        = {         };
    mi.cbSize = sizeof (mi);

    HMONITOR hMonitor =
      _PathDeviceToHMONITOR [config.display.monitor_path_ccd];

    if (                  hMonitor != nullptr &&
         GetMonitorInfoW (hMonitor, &mi) )
    {
      int monitor_idx = 0;

      if ( 1 == swscanf (StrStrIW (mi.szDevice, LR"(\DISPLAY)"),
                                                LR"(\DISPLAY%i)", &monitor_idx) )
      {
        config.display.monitor_idx =        monitor_idx;
        window.preferred_monitor_id->store (monitor_idx);

        config.display.monitor_handle = hMonitor;
        rb.next_monitor               = hMonitor;

        found_exact = true;

        SK_Display_ApplyDesktopResolution (mi);
      }
    }
  }

  // Prefer to use an exact VidPn path name, because they are persistent.
  //
  //  * However, if all the user has is a monitor index preference, give that a shot.
  //
  if (! found_exact)
  {
    window.preferred_monitor_id->load (config.display.monitor_idx);

    if (config.display.monitor_idx != 0 && config.display.monitor_handle == nullptr)
    {
      EnumDisplayMonitors ( nullptr, nullptr,
      [](HMONITOR hMonitor, HDC hDC, LPRECT lpRect, LPARAM lParam)
   -> BOOL
      {
        SK_RenderBackend& rb =
          SK_GetCurrentRenderBackend ();

        std::ignore = hDC;
        std::ignore = lpRect;
        std::ignore = lParam;

        MONITORINFOEXW
          mi        = {         };
          mi.cbSize = sizeof (mi);

        if (GetMonitorInfoW (hMonitor, &mi))
        {
          if (config.display.monitor_idx > 0)
          {
            if (StrStrIW (mi.szDevice, SK_FormatStringW (LR"(\DISPLAY%i)", config.display.monitor_idx).c_str ()) != nullptr)
            {
              config.display.monitor_handle = hMonitor;
              rb.next_monitor               = hMonitor;

              SK_Display_ApplyDesktopResolution (mi);

              return FALSE;
            }
          }

          else if (config.display.monitor_idx == SK_NoPreference)
          {
            if (mi.dwFlags & MONITORINFOF_PRIMARY)
            {
              config.display.monitor_handle = hMonitor;
              rb.next_monitor               = hMonitor;

              SK_Display_ApplyDesktopResolution (mi);

              return FALSE;
            }
          }
        }

        return TRUE;
      }, 0);
    }

    // No preference established, so handle should be null
    else if ( config.display.monitor_idx   == 0 )
    {         config.display.monitor_handle = 0;
      if ( (! config.display.resolution.override.isZero ()) ||
              config.display.refresh_rate > 0.0f )
      {
        EnumDisplayMonitors ( nullptr,
                              nullptr,
        [](HMONITOR hMonitor, HDC, LPRECT, LPARAM)
     -> BOOL
        {
          SK_RenderBackend& rb =
            SK_GetCurrentRenderBackend ();

          MONITORINFOEXW
            mi = { };
            mi.cbSize = sizeof (MONITORINFOEXW);

          if (GetMonitorInfoW (hMonitor, &mi))
          {
            if (mi.dwFlags & MONITORINFOF_PRIMARY)
            {
              config.display.monitor_handle = hMonitor;
              rb.next_monitor               = hMonitor;

              SK_Display_ApplyDesktopResolution (mi);

              return FALSE;
            }
          }

          // Keep looking, we only want the primary monitor
          return TRUE;
        }, 0);
      }
    }
  }

  class DisplayListener : public SK_IVariableListener
  {
  public:
    DisplayListener (void)
    {
      auto cmd =
        SK_GetCommandProcessor ();

      cmd->AddVariable ( "Display.RefreshRate",
        SK_CreateVar (SK_IVariable::Float, &config.display.refresh_rate, this)
      );
    }

    virtual bool OnVarChange (SK_IVariable* var, void* val = nullptr)
    {
      const SK_RenderBackend& rb =
        SK_GetCurrentRenderBackend ();

      if (val != nullptr && var != nullptr )
      {
        if (var->getValuePointer () == &config.display.refresh_rate)
        {
          float refresh = *(float *)val;

          refresh =
            std::clamp (refresh, 0.0f, 1000.0f);

          MONITORINFOEXW
            mi = { };
            mi.cbSize = sizeof (MONITORINFOEXW);

          // Remove any overrides
          if (refresh == 0.0f)
          {
            config.display.refresh_rate = 0.0f;
            return true;
          }

          if (GetMonitorInfoW (rb.monitor, &mi))
          {
            float original =
              std::exchange (config.display.refresh_rate, refresh);

            if (SK_Display_ApplyDesktopResolution (mi))
              return true;

            // Oh no!
            config.display.refresh_rate = original;
          }
        }
      }

      return true;
    }
  } static display_control;

  if (((sk::iParameter *)window.override)->load ())
  {
    swscanf ( window.override->get_value_str ().c_str (),
                L"%ux%u",
                &config.window.res.override.x,
                  &config.window.res.override.y );
  }
  window.multi_monitor_mode->load (config.window.multi_monitor_mode);


  reverse_engineering.file.trace_reads->load  (config.file_io.trace_reads);
  reverse_engineering.file.trace_writes->load (config.file_io.trace_writes);

  if (config.file_io.trace_reads)
  {
    extern iSK_Logger *read_log;

    if (read_log == nullptr)
        read_log = SK_CreateLog (LR"(logs\file_reads.log)");
  }

  if (config.file_io.trace_writes)
  {
    extern iSK_Logger *write_log;

    if (write_log == nullptr)
        write_log = SK_CreateLog (LR"(logs\file_writes.log)");
  }

  if (((sk::iParameter *)&reverse_engineering.file.ignore_reads)->load ())
  {
    std::unique_ptr <wchar_t> wszCSV (
      _wcsdup (reverse_engineering.file.ignore_reads.get_value ().c_str ())
    );

    wchar_t* wszBuf = nullptr;
    wchar_t* wszTok =
      std::wcstok (wszCSV.get (), L",", &wszBuf);

    while (wszTok != nullptr)
    {
      if (*wszTok != L'>')
        config.file_io.ignore_reads->single_file.insert   (              wszTok);
      else
        config.file_io.ignore_reads->entire_thread.insert (SK_CharNextW (wszTok));

      wszTok =
        std::wcstok (nullptr, L",", &wszBuf);
    }
  }

  if (((sk::iParameter *)&reverse_engineering.file.ignore_writes)->load ())
  {
    std::unique_ptr <wchar_t> wszCSV (
      _wcsdup (reverse_engineering.file.ignore_writes.get_value ().c_str ())
    );

    wchar_t* wszBuf = nullptr;
    wchar_t* wszTok =
      std::wcstok (wszCSV.get (), L",", &wszBuf);

    while (wszTok != nullptr)
    {
      if (*wszTok != L'>')
        config.file_io.ignore_writes->single_file.insert   (              wszTok);
      else
        config.file_io.ignore_writes->entire_thread.insert (SK_CharNextW (wszTok));

      wszTok =
        std::wcstok (nullptr, L",", &wszBuf);
    }
  }


  std::wstring tmp;
  cpu.power_scheme_guid->load (tmp);

  if (! tmp.empty ())
  {
    CLSIDFromString  (tmp.c_str (), &config.cpu.power_scheme_guid);
  }

  dpi.disable->load                 (config.dpi.disable_scaling);
  dpi.per_monitor_aware->load       (config.dpi.per_monitor.aware);
  dpi.per_monitor_all_threads->load (config.dpi.per_monitor.aware_on_all_threads);

  sound.minimize_latency->load      (config.sound.minimize_latency);

  platform.achievements.play_sound->load         (config.platform.achievements.play_sound);
  platform.achievements.sound_file->load         (config.platform.achievements.sound_file);
  platform.achievements.take_screenshot->load    (config.platform.achievements.take_screenshot);
  platform.achievements.fetch_friend_stats->load (config.platform.achievements.pull_friend_stats);
  platform.achievements.popup.animate->load      (config.platform.achievements.popup.animate);
  platform.achievements.popup.show_title->load   (config.platform.achievements.popup.show_title);

  if (((sk::iParameter *)platform.achievements.popup.origin)->load ())
  {
    config.platform.achievements.popup.origin =
      SK_Steam_PopupOriginWStrToEnum (
        platform.achievements.popup.origin->get_value ().c_str ()
      );
  }

  else
  {
    config.platform.achievements.popup.origin = 3;
  }

  platform.achievements.popup.inset->load    (config.platform.achievements.popup.inset);
  platform.achievements.popup.duration->load (config.platform.achievements.popup.duration);

  if (config.platform.achievements.popup.duration == 0)
  {
    config.platform.achievements.popup.show        = false;
    config.platform.achievements.pull_friend_stats = false;
    config.platform.achievements.pull_global_stats = false;
  }


  if (((sk::iParameter *)steam.cloud.blacklist)->load ())
  {
    std::unique_ptr <wchar_t> wszCSV (
        _wcsdup (steam.cloud.blacklist->get_value ().c_str ())
    );

    wchar_t* wszBuf = nullptr;
    wchar_t* wszTok =
      std::wcstok (wszCSV.get (), L",", &wszBuf);

    while (wszTok != nullptr)
    {
      config.steam.cloud.blacklist.emplace (SK_WideCharToUTF8 (wszTok));

      wszTok =
        std::wcstok (nullptr, L",", &wszBuf);
    }
  }


  platform.log.silent->load                   (config.platform.silent);
  steam.drm.spoof_BLoggedOn->load             (config.steam.spoof_BLoggedOn);
  eos.system.warned_online->load              (config.epic.warned_online);

  // We may already know the AppID before loading the game's config.
  if (config.steam.appid == 0)
      steam.system.appid->load                (config.steam.appid);

  if (config.steam.appid != 0)
  {
    if (config.steam.appid != SPECIAL_KILLER_APPID)
    {
#if 0
      // Non-Steam Games: Ignore this to prevent general weirdness.
      if ( ( static_cast <size_t>  (std::numeric_limits <int>::max () - 1) ) <=
             static_cast <AppId_t> (config.steam.appid) )

      {   config.steam.appid        = 0;
          steam.system.appid->store ( 0 );
      }

      else
#endif
      {
        SetEnvironmentVariableA ( "SteamGameId",
                 std::format ("{}", config.steam.appid).c_str () );
      }
    }

    // Special K's AppID belongs to Special K, not this game!
    else
    {
      config.steam.appid               = 0;
      steam.system.appid->store       (  0  );
      steam.system.auto_inject->store (false);
      steam.system.force_load->store  (false);
    }
  }

  steam.system.init_delay->load               (config.steam.init_delay);
  steam.system.auto_pump->load                (config.steam.auto_pump_callbacks);
  steam.system.block_stat_callback->load      (config.steam.block_stat_callback);
  steam.system.filter_stat_callbacks->load    (config.steam.filter_stat_callback);
  steam.system.load_early->load               (config.steam.preload_client);
  steam.system.early_overlay->load            (config.steam.preload_overlay);
  steam.system.force_load->load               (config.steam.force_load_steamapi);
  steam.system.auto_inject->load              (config.steam.auto_inject);

  int                                 throttle = SK_NoPreference;
  if (steam.callbacks.throttle->load (throttle))
  {
    InterlockedExchange ( &SK_SteamAPI_CallbackRateLimit,
                                      throttle );
  }

  if (platform.overlay.no_draw->load (config.steam.disable_overlay))
  {
    if (config.steam.disable_overlay)
    {
      SetEnvironmentVariable (
        L"SteamNoOverlayUIDrawing", L"1"
      );

      config.steam.disable_overlay = true;
    }
  }

  platform.system.reuse_overlay_pause->load   (config.platform.reuse_overlay_pause);
  platform.overlay.hdr_luminance->load        (config.platform.overlay_hdr_luminance);
  uplay.overlay.hdr_luminance->load           (config.uplay.overlay_luminance);
  rtss.overlay.hdr_luminance->load            (config.rtss.overlay_luminance);
  discord.overlay.hdr_luminance->load         (config.discord.overlay_luminance);

  steam.screenshots.smart_capture->load       (config.steam.screenshots.enable_hook);
  screenshots.include_osd_default->load       (config.screenshots.show_osd_by_default);
  screenshots.keep_png_copy->load             (config.screenshots.png_compress);
  screenshots.play_sound->load                (config.screenshots.play_sound);
  screenshots.copy_to_clipboard->load         (config.screenshots.copy_to_clipboard);
  screenshots.embed_nickname->load            (config.screenshots.embed_nickname);
  screenshots.override_path->load             (config.screenshots.override_path);
  screenshots.filename_format->load           (config.screenshots.filename_format);

  screenshots.compression_quality->load       (config.screenshots.compression_quality);
  screenshots.compatibility_mode->load        (config.screenshots.compatibility_mode);

  screenshots.jxl.use_jxl->load               (config.screenshots.use_jxl);

  screenshots.avif.use_avif->load             (config.screenshots.use_avif);
  screenshots.avif.yuv_subsampling->load      (config.screenshots.avif.yuv_subsampling);
  screenshots.avif.scrgb_bit_depth->load      (config.screenshots.avif.scrgb_bit_depth);
  screenshots.avif.compression_speed->load    (config.screenshots.avif.compression_speed);

  screenshots.png.store_hdr->load             (config.screenshots.use_hdr_png);
  screenshots.allow_hdr_clipboard->load       (config.screenshots.allow_hdr_clipboard);
  screenshots.png.st2084_bits->load           (config.screenshots.max_st2084_bits);
  screenshots.clipboard_hdr_format->load      (config.screenshots.clipboard_hdr_format);

  LoadKeybind (&config.render.keys.hud_toggle);
  LoadKeybind (&config.osd.keys.console_toggle);
  LoadKeybind (&config.screenshots.game_hud_free_keybind);
  LoadKeybind (&config.screenshots.sk_osd_free_keybind);
  LoadKeybind (&config.screenshots.sk_osd_insertion_keybind);
  LoadKeybind (&config.screenshots.no_3rd_party_keybind);
  LoadKeybind (&config.screenshots.clipboard_only_keybind);
  LoadKeybind (&config.screenshots.snipping_keybind);

  LoadKeybind (&config.monitors.monitor_primary_keybind);
  LoadKeybind (&config.monitors.monitor_next_keybind);
  LoadKeybind (&config.monitors.monitor_prev_keybind);
  LoadKeybind (&config.monitors.monitor_toggle_hdr);
  LoadKeybind (&config.monitors.multimonitor_focus_keybind);

  LoadKeybind (&config.render.framerate.latent_sync.tearline_move_up_keybind);
  LoadKeybind (&config.render.framerate.latent_sync.tearline_move_down_keybind);
  LoadKeybind (&config.render.framerate.latent_sync.timing_resync_keybind);
  LoadKeybind (&config.render.framerate.latent_sync.toggle_fcat_bars_keybind);

  LoadKeybind (&config.sound.game_mute_keybind);
  LoadKeybind (&config.sound.game_volume_up_keybind);
  LoadKeybind (&config.sound.game_volume_down_keybind);

  LoadKeybind (&config.widgets.hide_all_widgets_keybind);
  LoadKeybind (&config.reshade.toggle_overlay_keybind);
  LoadKeybind (&config.reshade.inject_reshade_keybind);


  if (config.steam.dll_path.empty ())
  {
    std::filesystem::path steam_plugin (
      SK_GetInstallPath ()
    );

    steam_plugin /= LR"(PlugIns\ThirdParty\Steamworks)";
    steam_plugin /= SK_RunLHIfBitness (32, L"steam_api_sk.dll",
                                           L"steam_api_sk64.dll");

    HMODULE hModSteam =
      GetModuleHandleW (
        SK_RunLHIfBitness (32, L"steam_api.dll",
                               L"steam_api64.dll"));

    std::wstring wszModPath =
      SK_GetModuleFullName (hModSteam);

    // Setup sane initial values
    config.steam.dll_path = hModSteam != 0 &&
      PathFileExistsW (wszModPath.c_str ()) ?
                       wszModPath           : steam_plugin.wstring ();
  }

  if (! steam.system.dll_path->empty ())
    steam.system.dll_path->load (config.steam.dll_path);

  steam.system.crapcom_mode->load (config.steam.crapcom_mode);


  bool global_override = false;

  if (platform_ini->contains_section (L"Steam.Social"))
  {
    if (platform_ini->get_section (L"Steam.Social").contains_key (L"OnlineStatus"))
    {
      swscanf ( platform_ini->get_section (L"Steam.Social").
                              get_value   (L"OnlineStatus").c_str (),
                  L"%d", &config.steam.online_status );
      global_override = true;
    }
  }

  // If no global value is set, this can be established per-game.
  if (! global_override)
    steam.social.online_status->load (config.steam.online_status);


  if (((sk::iParameter *)platform.system.notify_corner)->load ())
  {
    config.platform.notify_corner =
      SK_Steam_PopupOriginWStrToEnum (
        platform.system.notify_corner->get_value ().c_str ()
    );
  }

  osd.show->load           (config.osd.show);

  osd.text.red->load       (config.osd.red);
  osd.text.green->load     (config.osd.green);
  osd.text.blue->load      (config.osd.blue);

  osd.viewport.pos_x->load (config.osd.pos_x);
  osd.viewport.pos_y->load (config.osd.pos_y);
  osd.viewport.scale->load (config.osd.scale);


  silent->load              (config.system.silent);
  trace_libraries->load     (config.system.trace_load_library);
  strict_compliance->load   (config.system.strict_compliance);
  log_level->load           (config.system.log_level);
  sk::logs::base_log_lvl  =  config.system.log_level;
  prefer_fahrenheit->load   (config.system.prefer_fahrenheit);
  handle_crashes->load      (config.system.handle_crashes);
  silent_crash->load        (config.system.silent_crash);
  crash_suppression->load   (config.system.suppress_crashes);
  debug_wait->load          (config.system.wait_for_debugger);
  debug_output->load        (config.system.display_debug_out);
  game_output->load         (config.system.game_output);
  init_delay->load          (config.system.global_inject_delay);
  return_to_skif->load      (config.system.return_to_skif);
  auto_load_asi_files->load (config.system.auto_load_asi_files);

  // This is slow as hell thanks to the Steam overlay, so it
  //   should only ever be done on the first launch...
  static bool do_win_verify_trust =
    (SK_Steam_GetAppID_NoAPI () != 0 && config.system.first_run) || SK_IsAdmin ();

  SK_RunOnce (
    if (version->load (config.system.version)) {
                       config.system.first_run = false;
                       do_win_verify_trust     = false;
    }
  );

  skif_autostop_behavior->load (config.skif.auto_stop_behavior);




  ///// Disable Smart Screenshot capture if address space is constrained
  ///if (! SK_PE32_IsLargeAddressAware ())
  ///{
  ///  if (config.steam.screenshots.enable_hook)
  ///  {   config.steam.screenshots.enable_hook = false;
  ///    SK_ImGui_Warning ( L"Executable is not Large Address Aware, disabling Smart Screenshots." );
  ///  }
  ///}


  SK_Resource_SetRoot (config.textures.d3d11.res_root.c_str ());


  //
  // EMERGENCY OVERRIDES
  //
  // ...


  //
  // If DXGI ever fails to create a SwapChain complaining about WS_EX_TOPMOST,
  //   it may be necessary to force this off for that game.
  //
  static bool bDisallowAlwaysOnTop =
    (SK_GetCurrentGameID () == SK_GAME_ID::GodOfWarRagnarok);

  if (bDisallowAlwaysOnTop)
  {
    if (config.window.always_on_top !=  SmartAlwaysOnTop)
        config.window.always_on_top = PreventAlwaysOnTop; // DXGI SwapChain creation may fail without this
  }


  static bool scanned = false;

  if ((! scanned) && (! config.window.res.override.isZero ()))
  {
    scanned = true;

    if (SK_GetCurrentGameID () != SK_GAME_ID::UNKNOWN_GAME)
    {
      switch (SK_GetCurrentGameID ())
      {
      //case SK_GAME_ID::GalGunReturns:
        case SK_GAME_ID::GalGun_Double_Peace:
        case SK_GAME_ID::DuckTalesRemastered:
        {
          SK_Thread_Create ([](LPVOID) ->
          DWORD
          {
            void
            SK_ResHack_PatchGame (uint32_t w, uint32_t h);
            SK_ResHack_PatchGame (1920, 1080);
            SK_Thread_CloseSelf  ();

            return 0;
          });
        } break;


        case SK_GAME_ID::YS_Seven:
        {
          SK_Thread_Create ([] (LPVOID) ->
          DWORD
          {
            void
            SK_ResHack_PatchGame2 (uint32_t w, uint32_t h);
            SK_ResHack_PatchGame2 (1920, 1080);
            SK_Thread_CloseSelf   ();

            return 0;
          });
        } break;
      }
    }
  }

  // The name of this config file is different in 0.10.x, but want to load existing values
  //   if the user has any; that involves renaming the file after loading it.
  if (! migrate_platform_config.empty ())
    platform_ini->rename (migrate_platform_config.c_str ());




  //
  // DRM Workarounds
  //
  bool bHasCrapcomDRM = false;

  // Only do DRM checks when we have a fully parsed INI file
  //
  //  * To be clear, we are checking for DRM that needs workarounds (CAPCOM, mostly)!
  if ( dll_ini != nullptr &&
       osd_ini != nullptr && do_win_verify_trust )
  {
    static DWORD dwVerifyTrustStartTime = SK_timeGetTime ();
    static auto code_sig =
      SK_VerifyTrust_GetCodeSignature (SK_GetFullyQualifiedApp ());

    if (StrStrIW (code_sig.subject.c_str (), L"COGNOSPHERE") ||
        StrStrIW (code_sig.subject.c_str (), L"KURO TECHNOLOGY"))
    {
      config.compatibility.disable_debug_features = true;
    }

    if (StrStrIW (code_sig.subject.c_str (), L"CAPCOM"))
    //LR"(Private Organization, JP, 1200-01-077023, JP, Osaka, Osaka-shi, "CAPCOM CO., LTD.", "CAPCOM CO., LTD.")"
    {
      static constexpr
      SYSTEMTIME crapcom_drm_epoch =
      { // We think the really bad DRM stuff started ~2018
        .wYear = 2018, .wMonth = 1,
                       .wDay   = 1
      };

      FILETIME                                   ft_drm_epoch;
      SystemTimeToFileTime (&crapcom_drm_epoch, &ft_drm_epoch);

      //if (1 == CompareFileTime (&code_sig.valid_beginning, &ft_drm_epoch))
      {
        bHasCrapcomDRM = true;

        SK_GetCurrentRenderBackend ().windows.capcom = bHasCrapcomDRM;
      }
    }

    if (bHasCrapcomDRM)
    {
  #ifdef _M_IX86
      static constexpr wchar_t *wszSteamAPIDll    =              L"steam_api.dll";
      static constexpr wchar_t *wszKaldaienAPIDll = L"kaldaien_api/steam_api.dll";
  #else
      static constexpr wchar_t *wszSteamAPIDll    =              L"steam_api64.dll";
      static constexpr wchar_t *wszKaldaienAPIDll = L"kaldaien_api/steam_api64.dll";
  #endif
  
      // Do not use CRAPCOM DRM workaround on Steam Deck
      if (config.compatibility.using_wine)
          config.platform.silent = true;
  
      if (PathFileExistsW (wszSteamAPIDll))
      {
        if (! config.platform.silent)
        {     config.platform.silent =
                !((PathFileExistsW (                  wszSteamAPIDll)
                 &&PathFileExistsW (               wszKaldaienAPIDll))||
                  (PathFileExistsW (                  wszSteamAPIDll)&&
                   SK_CreateDirectories (L"kaldaien_api/") &&
                   CopyFile        (        wszSteamAPIDll,
                                         wszKaldaienAPIDll, FALSE)));
        }
  
        if (! config.platform.silent )
        {if(! config.steam.crapcom_mode )
          {   config.steam.auto_inject         =    true;
              config.steam.auto_pump_callbacks =    true;
              config.steam.force_load_steamapi =    true;
              config.steam.preload_client      =    true;
              config.steam.preload_overlay     =    true;
              config.steam.init_delay          =      -1;
              config.platform.silent           =   false;
              config.steam.dll_path            =  wszKaldaienAPIDll;
          }   config.steam.crapcom_mode        =    true;
        }else{config.steam.crapcom_mode        =   false;
              config.steam.dll_path            =     L"";}
      }
  
      else
      {
        config.steam.preload_client  = true;
        config.steam.preload_overlay = true;
  
        // Setup to use SK's own Steamworks DLL because Enigma Protector packs
        //   the game's SteamAPI DLL into its encrypted payload
        if ((! config.platform.silent) && config.steam.dll_path.empty ())
          SteamAPI_ManualDispatch_Init_Detour ();
      }
    }
  
    // Time this, because the Steam overlay is doing some bad stuff with
    //   WinVerifyTrust that causes games to take an extremely excessive
    //     amount of time to start! (2025)
#ifdef DEBUG
    static DWORD
        dwVerifyTrustEndTime = SK_timeGetTime ();
    if (dwVerifyTrustEndTime - dwVerifyTrustStartTime > 250UL)
    {
      SK_LOGi0 ( L"WinVerifyTrust took %d ms.",
                    dwVerifyTrustEndTime -
                    dwVerifyTrustStartTime );
    }
#endif
  }


  // Config opted-in to debugger wait
  if (config.system.wait_for_debugger)
  {
    SK_ApplyQueuedHooks ();

    if (      ! SK_IsDebuggerPresent ())
    { while ((! SK_IsDebuggerPresent ()))
      {
        bool  _break = false;

        RtlAcquirePebLock_Detour ();
              SK_SleepEx (50, TRUE);
              _break =
                SK_Debug_CheckDebugFlagInPEB ();
        RtlReleasePebLock_Detour ();

        if (_break)
          break;
      }

      __debugbreak ();
    }
  }

  return (! empty);
  }

  catch (const std::exception& e)
  {
    SK_LOGi0 (
      L"Exception %hs during INI Parse", e.what ()
    );
  }

  return false;
}

void
SK_ResHack_PatchGame ( uint32_t width,
                       uint32_t height )
{
  static unsigned int replacements = 0;

  struct
  {
    struct
    {
      uint32_t w, h;
    } pattern;

    struct
    {
      uint32_t w = config.window.res.override.x,
               h = config.window.res.override.y;
    } replacement;
  } res_mod;

  res_mod.pattern.w = width;
  res_mod.pattern.h = height;

        uint32_t* pOut     = nullptr;
  const void*     pPattern = &res_mod.pattern;

  for (int i = 0 ; i < 3; i++)
  {
    pOut =
    pOut != nullptr ?
      static_cast <uint32_t *> (
        SK_ScanAlignedEx ( pPattern, 8, nullptr, pOut, 8 )
      )             : pOut;


    if (pOut != nullptr)
    {
      if ( SK_InjectMemory ( pOut,
                               &res_mod.replacement.w,
                                8,
                                  PAGE_READWRITE )
         )
      {
        ++replacements;
      }

      pOut += 8;
    }

    else
    {
      dll_log->Log ( L"[GalGunHACK] ** %lu Resolution Replacements Made  ==>  "
                                          L"( %lux%lu --> %lux%lu )",
                       replacements,
                         width, height,
                           res_mod.replacement.w, res_mod.replacement.h );
      break;
    }
  }
}


void
SK_ResHack_PatchGame3 ( uint32_t width,
                        uint32_t height )
{
  static unsigned int replacements = 0;

        char orig [] = "\xC7\x05\x00\x00\x00\x00"
                       "\x00\x00\x00\x00"
                       "\xC7\x05\x00\x00\x00\x00"
                       "\x00\x00\x00\x00";

  const char* mask = "\xFF\xFF\x00\x00\x00\x00"
                     "\xFF\xFF\xFF\xFF"
                     "\xFF\xFF\x00\x00\x00\x00"
                     "\xFF\xFF\xFF\xFF";

  *((uint32_t *)(uint8_t*)(orig +  6)) = width;
  *((uint32_t *)(uint8_t*)(orig + 16)) = height;

  uint8_t* pOut = nullptr;

  for (int i = 0 ; i < 5; i++)
  {
    pOut =
    pOut != nullptr           ?
      static_cast <uint8_t *> (
        SK_ScanAlignedEx (orig, 20, mask, pOut, 1)
      )                       : pOut;

    if (pOut != nullptr)
    {
      uint8_t data [20] = { };

      memcpy (data, pOut, 20);

      *((uint32_t *)(uint8_t *)(data +  6)) = (uint32_t)(config.window.res.override.x);
      *((uint32_t *)(uint8_t *)(data + 16)) = (uint32_t)(config.window.res.override.y);

      if ( SK_InjectMemory ( pOut,
                               data,
                                 20,
                                   PAGE_EXECUTE_READWRITE )
         )
      {
        ++replacements;
      }

      pOut += 20;
    }

    if (pOut == nullptr)
    {
      dll_log->Log ( L"[Resolution] ** %lu Resolution Replacements Made  ==>  "
                                       L"( %lux%lu --> %lux%lu )",
                       replacements,
                         width, height,
                           config.window.res.override.x, config.window.res.override.y );
      break;
    }
  }
}

void
SK_ResHack_PatchGame2 ( uint32_t width,
                        uint32_t height )
{
  static unsigned int replacements = 0;

  uint32_t orig [2] = { 0x00000000,
                        0x00000000 };

  *(orig + 0) = width;
  *(orig + 1) = height;

  uint32_t *pOut = nullptr;

  for (int i = 0 ; i < 5; i++)
  {
    pOut =
    pOut != nullptr            ?
      static_cast <uint32_t *> (
        SK_ScanAlignedEx (orig, 8, nullptr, pOut, 4)
      )                        : pOut;

    if (pOut != nullptr)
    {
      struct {
        uint32_t w = sk::narrow_cast <uint32_t> (config.window.res.override.x),
                 h = sk::narrow_cast <uint32_t> (config.window.res.override.y);
      } out_data = { };


      if ( SK_InjectMemory ( pOut,
                               &out_data.w,
                                 8,
                                   PAGE_READWRITE )
         )
      {
        ++replacements;
      }

      pOut += 8;
    }

    if (pOut == nullptr)
    {
      dll_log->Log ( L"[Resolution] ** %lu Resolution Replacements Made  ==>  "
                                       L"( %lux%lu --> %lux%lu )",
                       replacements,
                         width, height,
                           config.window.res.override.x, config.window.res.override.y );
      break;
    }
  }
}

bool
SK_DeleteConfig (std::wstring name)
{
  if (name.empty ())
  {
    if (SK_IsInjected ())
      name = L"SpecialK";
    else
      name = SK_GetBackend ();
  }


  wchar_t wszFullName [ MAX_PATH + 2 ] = { };

  if ( name.find (L'/' ) == std::wstring::npos &&
       name.find (L'\\') == std::wstring::npos )
    lstrcatW (wszFullName, SK_GetConfigPath ());

  lstrcatW (wszFullName,   name.c_str ());

  if (name.find (L".ini") == std::wstring::npos)
    lstrcatW (wszFullName, L".ini");


  return (DeleteFileW (wszFullName) != FALSE);
}

void
SK_SaveConfig ( std::wstring name,
                bool         close_config )
{
  // Ignore this :)
  if (SK_GetCurrentGameID () == SK_GAME_ID::Launcher || SK_GetHostAppUtil ()->isBlacklisted ())
  {
    return;
  }

  if (name.empty ())
  {
    if (SK_IsInjected ())
      name = L"SpecialK";
    else
      name = SK_GetBackend ();
  }

  //
  // Shutting down before initialization would be damn near fatal if we didn't catch this! :)
  //
  if ( dll_ini == nullptr ||
       osd_ini == nullptr    )
    return;

  SK_ScopedLocale _locale (L"en_us.utf8");

  const SK_RenderBackend& rb =
    SK_GetCurrentRenderBackend ();

  // Temp hack to handle GLDX11
  if (SK_GL_OnD3D11)
    config.apis.last_known = SK_RenderAPI::OpenGL;


  // Don't write these to INI by default, they're rarely needed and planned to be removed
//compatibility.fsr3_mode->store              (config.compatibility.fsr3_mode);
//compatibility.reshade_mode->store           (config.compatibility.reshade_mode);
  compatibility.async_init->store             (config.compatibility.init_on_separate_thread);
  compatibility.disable_nv_bloat->store       (config.compatibility.disable_nv_bloat);
  compatibility.rehook_loadlibrary->store     (config.compatibility.rehook_loadlibrary);
  compatibility.using_wine->store             (config.compatibility.using_wine);
  compatibility.allow_dxdiagn->store          (config.compatibility.allow_dxdiagn);
  compatibility.allow_fake_streamline->store  (config.compatibility.allow_fake_streamline);
  compatibility.sdl_sanity_level->store       (config.compatibility.sdl_sanity_level);

  compatibility.sdl.allow_xinput->store       (config.compatibility.sdl.allow_xinput);
  compatibility.sdl.allow_direct_input->store (config.compatibility.sdl.allow_direct_input);
  compatibility.sdl.allow_wgi->store          (config.compatibility.sdl.allow_wgi);
  compatibility.sdl.allow_raw_input->store    (config.compatibility.sdl.allow_raw_input); 
  compatibility.sdl.allow_hid->store          (config.compatibility.sdl.allow_hid);
  compatibility.sdl.switch_led_brightness    
                                       ->store(config.compatibility.sdl.switch_led_brightness);
  compatibility.sdl.use_joystick_thread->store(config.compatibility.sdl.use_joystick_thread);
  compatibility.sdl.poll_sentinel->store      (config.compatibility.sdl.poll_sentinel);
  compatibility.sdl.allow_all_ps_bt_features 
                                       ->store(config.compatibility.sdl.allow_all_ps_bt_features);

#ifdef _M_IX86
  compatibility.auto_large_address->store     (config.compatibility.auto_large_address_patch);
#endif

  monitoring.memory.show->set_value           (config.mem.show);
//mem_reserve->store                          (config.mem.reserve);

  monitoring.fps.show->store                  (config.fps.show);
  monitoring.fps.compact->store               (config.fps.compact);
  monitoring.fps.compact_vrr->store           (config.fps.compact_vrr);
  monitoring.fps.advanced->store              (config.fps.advanced);
  monitoring.fps.frametime->store             (config.fps.frametime);
  monitoring.fps.framenumber->store           (config.fps.framenumber);
  monitoring.fps.timing_method->store         (config.fps.timing_method);

  monitoring.io.show->set_value               (config.io.show);
  monitoring.io.interval->store               (config.io.interval);

  monitoring.cpu.show->set_value              (config.cpu.show);
  monitoring.cpu.interval->store              (config.cpu.interval);
  monitoring.cpu.simple->store                (config.cpu.simple);

  monitoring.gpu.show->store                  (config.gpu.show);
  monitoring.gpu.print_slowdown->store        (config.gpu.print_slowdown);
  monitoring.gpu.interval->store              (config.gpu.interval);

  monitoring.disk.show->set_value             (config.disk.show);
  monitoring.disk.interval->store             (config.disk.interval);
  monitoring.disk.type->store                 (config.disk.type);

  monitoring.pagefile.show->set_value         (config.pagefile.show);
  monitoring.pagefile.interval->store         (config.pagefile.interval);

  monitoring.dlss.show->store                 (config.dlss.show);
  monitoring.dlss.show_output_res->store      (config.dlss.show_output_res);
  monitoring.dlss.show_quality->store         (config.dlss.show_quality);
  monitoring.dlss.show_preset->store          (config.dlss.show_preset);
  monitoring.dlss.show_fg->store              (config.dlss.show_fg);

  if (! (nvapi_init && sk::NVAPI::nv_hardware && sk::NVAPI::CountSLIGPUs () > 1))
    config.sli.show = false;

  monitoring.SLI.show->store                  (config.sli.show);
  monitoring.time.show->store                 (config.time.show);
  monitoring.title.show->store                (config.title.show);

  osd.version_banner.duration->store          (config.version_banner.duration);
  osd.show->store                             (config.osd.show);
  osd.text.red->store                         (config.osd.red);
  osd.text.green->store                       (config.osd.green);
  osd.text.blue->store                        (config.osd.blue);
  osd.viewport.pos_x->store                   (config.osd.pos_x);
  osd.viewport.pos_y->store                   (config.osd.pos_y);
  osd.viewport.scale->store                   (config.osd.scale);
  osd.state.remember->store                   (config.osd.remember_state);

  imgui.scale->store                          (config.imgui.scale);
  imgui.show_eula->store                      (config.imgui.show_eula);
  imgui.show_playtime->store                  (config.platform.show_playtime);
  imgui.show_gsync_status->store              (config.apis.NvAPI.gsync_status);
  imgui.mac_style_menu->store                 (config.imgui.use_mac_style_menu);
  imgui.show_input_apis->store                (config.imgui.show_input_apis);
  imgui.disable_alpha->store                  (config.imgui.render.disable_alpha);
  imgui.antialias_lines->store                (config.imgui.render.antialias_lines);
  imgui.antialias_contours->store             (config.imgui.render.antialias_contours);
  imgui.center_cursor_on_overlay->store       (config.input.ui.center_cursor);
  imgui.nav_moves_mouse->store                (config.input.ui.nav_moves_mouse);

  apis.last_known->store                      (static_cast <int> (config.apis.last_known));

#ifdef _M_IX86
  if (config.apis.ddraw.hook_next != SK_NoPreference) config.apis.ddraw.hook = config.apis.ddraw.hook_next != 0;
  if (config.apis.d3d8.hook_next  != SK_NoPreference) config.apis.d3d8.hook  = config.apis.d3d8.hook_next  != 0;

  apis.ddraw.hook->store                      (config.apis.ddraw.hook);
  apis.d3d8.hook->store                       (config.apis.d3d8.hook);
#endif
  // Change the settings now if the user changed them from the control panel
  if (config.apis.d3d9.hook_next       != SK_NoPreference) config.apis.d3d9.hook       = config.apis.d3d9.hook_next       != 0;
  if (config.apis.d3d9ex.hook_next     != SK_NoPreference) config.apis.d3d9ex.hook     = config.apis.d3d9ex.hook_next     != 0;
  if (config.apis.dxgi.d3d11.hook_next != SK_NoPreference) config.apis.dxgi.d3d11.hook = config.apis.dxgi.d3d11.hook_next != 0;
  if (config.apis.dxgi.d3d12.hook_next != SK_NoPreference) config.apis.dxgi.d3d12.hook = config.apis.dxgi.d3d12.hook_next != 0;
  if (config.apis.OpenGL.hook_next     != SK_NoPreference) config.apis.OpenGL.hook     = config.apis.OpenGL.hook_next     != 0;

  apis.d3d9.hook->store                       (config.apis.d3d9.hook);
  apis.d3d9ex.hook->store                     (config.apis.d3d9ex.hook);
  apis.dxvk9.enable->store                    (config.apis.d3d9.native_dxvk);
  apis.d3d11.hook->store                      (config.apis.dxgi.d3d11.hook);
  apis.d3d12.hook->store                      (config.apis.dxgi.d3d12.hook);
  apis.OpenGL.hook->store                     (config.apis.OpenGL.hook);
#ifdef _M_AMD64
  if (config.apis.Vulkan.hook_next != SK_NoPreference) config.apis.Vulkan.hook = config.apis.Vulkan.hook_next != 0;

  apis.Vulkan.hook->store                     (config.apis.Vulkan.hook);
#endif

  nvidia.api.disable_hdr->store               (config.apis.NvAPI.disable_hdr);
  nvidia.api.vulkan_bridge->store             (config.apis.NvAPI.vulkan_bridge);
  nvidia.bugs.snuffed_ansel->store            (config.nvidia.bugs.snuffed_ansel);
  nvidia.bugs.bypass_ansel->store             (config.nvidia.bugs.bypass_ansel);

  input.keyboard.catch_alt_f4->store          (config.input.keyboard.catch_alt_f4);
  input.keyboard.bypass_alt_f4->store         (config.input.keyboard.override_alt_f4);
  input.keyboard.disabled_to_game->store      (config.input.keyboard.org_disabled_to_game);

  input.mouse.disabled_to_game->store         (config.input.mouse.org_disabled_to_game);

  input.cursor.manage->store                  (config.input.cursor.manage);
  input.cursor.keys_activate->store           (config.input.cursor.keys_activate);
  input.cursor.gamepad_deactivates->store     (config.input.cursor.gamepad_deactivates);
  input.cursor.timeout->store                 (static_cast <float> (config.input.cursor.timeout) / 1000.0f);
  input.cursor.ui_capture->store              (config.input.ui.capture);
  input.cursor.hw_cursor->store               (config.input.ui.use_hw_cursor);
  input.cursor.block_invisible->store         (config.input.ui.capture_hidden);
  input.cursor.fix_synaptics->store           (config.input.mouse.fix_synaptics);

  input.gamepad.disabled_to_game->store       (config.input.gamepad.disabled_to_game);
  input.gamepad.disable_hid->store            (config.input.gamepad.disable_hid);
  input.gamepad.disable_winmm->store          (config.input.gamepad.disable_winmm);
  input.gamepad.rehook_xinput->store          (config.input.gamepad.rehook_xinput);
  input.gamepad.haptic_ui->store              (config.input.gamepad.haptic_ui);

  if (config.input.gamepad.hook_dinput8 != hook_dinput8_orig)
             input.gamepad.hook_dinput8->store (config.input.gamepad.hook_dinput8);
  if (config.input.gamepad.hook_dinput7 != hook_dinput7_orig)
             input.gamepad.hook_dinput7->store (config.input.gamepad.hook_dinput7);
  if (config.input.gamepad.hook_windows_gaming != hook_wgi_orig)
             input.gamepad.hook_windows_gaming->
                                         store (config.input.gamepad.hook_windows_gaming);
  if (config.input.gamepad.hook_raw_input != hook_raw_input_orig)
             input.gamepad.hook_raw_input->
                                         store (config.input.gamepad.hook_raw_input);
  if (config.input.gamepad.hook_hid != hook_hid_orig)
             input.gamepad.hook_hid->store     (config.input.gamepad.hook_hid);
  //if (config.input.gamepad.hook_winmm != hook_winmm_orig)
  //           input.gamepad.hook_winmm->store     (config.input.gamepad.hook_winmm);
  if (config.input.gamepad.hook_scepad != hook_scepad_orig)
             input.gamepad.hook_scepad->store  (config.input.gamepad.hook_scepad);
  if (config.input.gamepad.hook_xinput != hook_xinput_orig)
             input.gamepad.hook_xinput->store  (config.input.gamepad.hook_xinput);

  int placeholder_mask = 0x0;

  placeholder_mask |= (config.input.gamepad.xinput.placehold [0] ? 0x1 : 0x0);
  placeholder_mask |= (config.input.gamepad.xinput.placehold [1] ? 0x2 : 0x0);
  placeholder_mask |= (config.input.gamepad.xinput.placehold [2] ? 0x4 : 0x0);
  placeholder_mask |= (config.input.gamepad.xinput.placehold [3] ? 0x8 : 0x0);

  input.gamepad.xinput.placeholders->store    (placeholder_mask);
  input.gamepad.xinput.ui_slot->store         (config.input.gamepad.xinput.ui_slot);
  input.gamepad.steam.disable->store          (config.input.gamepad.steam.disabled_to_game);

  // Turn off auto-slot assignment if the UI slot is invalid
  if (config.input.gamepad.xinput.ui_slot < 0 || config.input.gamepad.xinput.ui_slot > 3)
      config.input.gamepad.xinput.auto_slot_assign = false;

  std::wstring xinput_assign;
  std::wstring xinput_disable;

  xinput_assign.reserve  (16);
  xinput_disable.reserve (16);

  for (int i = 0; i < 4; i++)
  {
    xinput_assign += std::to_wstring (
      config.input.gamepad.xinput.assignment [i]
    );

    if (i != 3)
      xinput_assign += L",";
  }

  for (int i = 0; i < 4; i++)
  {
    if (config.input.gamepad.xinput.disable [i])
    {
      xinput_disable += std::to_wstring (i);
      xinput_disable += L",";
    }
  }

  if (xinput_disable.data ()[xinput_disable.length () - 1] == L',')
      xinput_disable.resize (xinput_disable.length () - 1);

  input.gamepad.xinput.assignment->store           (xinput_assign);
  input.gamepad.xinput.disable_slots->store        (xinput_disable);
  input.gamepad.hid.max_allowed_buffers->store     (config.input.gamepad.hid.max_allowed_buffers);
  input.gamepad.bt_input_only->store               (config.input.gamepad.bt_input_only);
  input.gamepad.disable_rumble->store              (config.input.gamepad.disable_rumble);
  input.gamepad.blocks_screensaver->store          (config.input.gamepad.blocks_screensaver);
  input.gamepad.left_impulse_strength->store       (config.input.gamepad.impulse_strength_l);
  input.gamepad.right_impulse_strength->store      (config.input.gamepad.impulse_strength_r);
  input.gamepad.left_resist_strength->store        (config.input.gamepad.dualsense.resist_strength_l);
  input.gamepad.right_resist_strength->store       (config.input.gamepad.dualsense.resist_strength_r);
  input.gamepad.left_resist_start->store           (config.input.gamepad.dualsense.resist_start_l);
  input.gamepad.right_resist_start->store          (config.input.gamepad.dualsense.resist_start_r);
  input.gamepad.xinput.hook_setstate->store        (config.input.gamepad.xinput.hook_setstate);
  input.gamepad.xinput.auto_slot_assign->store     (config.input.gamepad.xinput.auto_slot_assign);
  input.gamepad.xinput.blackout_api->store         (config.input.gamepad.xinput.blackout_api);
  input.gamepad.xinput.emulate->store              (config.input.gamepad.xinput.emulate);
  input.gamepad.xinput.deadzone->store             (config.input.gamepad.xinput.deadzone);
  input.gamepad.dinput.blackout_gamepads->store    (config.input.gamepad.dinput.blackout_gamepads);
  input.gamepad.dinput.blackout_mice->store        (config.input.gamepad.dinput.blackout_mice);
  input.gamepad.dinput.blackout_keyboards->store   (config.input.gamepad.dinput.blackout_keyboards);
  input.gamepad.dinput.block_enum_devices->store   (config.input.gamepad.dinput.block_enum_devices);

  input.gamepad.hook_scepad->store                 (config.input.gamepad.hook_scepad);
  input.gamepad.scepad.disable_touchpad->store     (config.input.gamepad.scepad.disable_touch);
  input.gamepad.scepad.share_clicks_touch->store   (config.input.gamepad.scepad.share_clicks_touch);
  input.gamepad.scepad.mute_applies_to_game->store (config.input.gamepad.scepad.mute_applies_to_game);
  input.gamepad.scepad.enhanced_ps_button->store   (config.input.gamepad.scepad.enhanced_ps_button);
  input.gamepad.scepad.power_save_mode->store      (config.input.gamepad.scepad.power_save_mode);
  input.gamepad.scepad.led_color_r->store          (config.input.gamepad.scepad.led_color_r);
  input.gamepad.scepad.led_color_g->store          (config.input.gamepad.scepad.led_color_g);
  input.gamepad.scepad.led_color_b->store          (config.input.gamepad.scepad.led_color_b);
  input.gamepad.scepad.led_brightness->store       (config.input.gamepad.scepad.led_brightness);
  input.gamepad.scepad.show_ds4_as_ds4_v2->store   (config.input.gamepad.scepad.show_ds4_v1_as_v2);
  input.gamepad.scepad.hide_ds4_v2_pid->store      (config.input.gamepad.scepad.hide_ds4_v2_pid);
  input.gamepad.scepad.hide_ds_edge_pid->store     (config.input.gamepad.scepad.hide_ds_edge_pid);
  input.gamepad.scepad.enable_full_bluetooth->store(config.input.gamepad.scepad.enable_full_bluetooth);
  input.gamepad.scepad.left_fn_bind->store         (config.input.gamepad.scepad.left_fn);
  input.gamepad.scepad.left_paddle_bind->store     (config.input.gamepad.scepad.left_paddle);
  input.gamepad.scepad.right_paddle_bind->store    (config.input.gamepad.scepad.right_paddle);
  input.gamepad.scepad.right_fn_bind->store        (config.input.gamepad.scepad.right_fn);
  input.gamepad.scepad.touch_click_bind->store     (config.input.gamepad.scepad.touch_click);

  input.gamepad.low_battery_warning->store         (config.input.gamepad.low_battery_percent);


  threads.enable_mem_alloc_trace->store            (config.threads.enable_mem_alloc_trace);
  threads.enable_file_io_trace->store              (config.threads.enable_file_io_trace);

  window.borderless->store                         (config.window.borderless);
  window.center->store                             (config.window.center);
  window.background_render->store                  (config.window.background_render);
  window.background_mute->store                    (config.window.background_mute);
  if (config.window.offset.x.absolute != 0)
  {
    wchar_t   wszAbsolute [16] = { };
    swprintf (wszAbsolute, L"%i", config.window.offset.x.absolute);

    window.offset.x->store (wszAbsolute);
  }

  else
  {
      wchar_t wszPercent [16] = { };
    swprintf (wszPercent, L"%08.6f", 100.0 * config.window.offset.x.percent);

    SK_RemoveTrailingDecimalZeros (wszPercent);
    lstrcatW                      (wszPercent, L"%");
    window.offset.x->store        (wszPercent);
  }

  if (config.window.offset.y.absolute != 0)
  {
    wchar_t   wszAbsolute [16] = { };
    swprintf (wszAbsolute, L"%i", config.window.offset.y.absolute);

    window.offset.y->store (wszAbsolute);
  }

  else
  {
      wchar_t wszPercent [16] = { };
    swprintf (wszPercent, L"%08.6f", 100.0 * config.window.offset.y.percent);

    SK_RemoveTrailingDecimalZeros (wszPercent);
    lstrcatW                      (wszPercent, L"%");
    window.offset.y->store        (wszPercent);
  }

  window.confine_cursor->store                (config.window.confine_cursor);
  window.unconfine_cursor->store              (config.window.unconfine_cursor);
  window.persistent_drag->store               (config.window.persistent_drag);
  window.fullscreen->store                    (config.window.fullscreen);
  window.fix_mouse_coords->store              (config.window.res.override.fix_mouse);
  window.always_on_top->store                 (config.window.always_on_top);
  window.disable_screensaver->store           (config.window.disable_screensaver);
  window.fullscreen_no_saver->store           (config.window.fullscreen_no_saver);
  window.manage_screensaver->store            (config.window.manage_screensaver);
  window.dont_hook_wndproc->store             (config.window.dont_hook_wndproc);
  window.activate_at_start->store             (config.window.activate_at_start);
  window.treat_fg_as_active->store            (config.window.treat_fg_as_active);

#ifdef _VALIDATE_MONITOR_IDX
  if (config.display.monitor_handle != 0)
  {
    if (config.display.monitor_idx > 0)
    {
      MONITORINFOEXW
        mi        = {         };
        mi.cbSize = sizeof (mi);

      int current_idx = 0;

      if (GetMonitorInfoW (config.display.monitor_handle, &mi))
      {
        wchar_t *wszDisplay =
          StrStrIW (mi.szDevice, LR"(\DISPLAY)");

        if (wszDisplay != nullptr)
        {
          int scount =
            swscanf (wszDisplay, LR"(\DISPLAY%i)", &current_idx);

          SK_ReleaseAssert (scount == 1);
        }
      }
    }
  }
#endif

  if (config.display.monitor_handle != 0)
  {
    window.preferred_monitor_exact->store (config.display.monitor_path_ccd);
    window.preferred_monitor_id->store    (config.display.monitor_idx);
  }

  else
  {
    window.preferred_monitor_id->store    (0);
  }

  window.override->store (
                        std::format ( L"{}x{}", config.window.res.override.x,
                                                config.window.res.override.y ) );
  window.multi_monitor_mode->store             (config.window.multi_monitor_mode);

  display.force_fullscreen->store              (config.display.force_fullscreen);
  display.force_windowed->store                (config.display.force_windowed);
  display.force_10bpc_sdr->store               (config.render.output.force_10bpc);
  display.aspect_ratio_stretch->store          (config.display.aspect_ratio_stretch);
  display.multimonitor_focus_is_focused->store (config.display.focus_mode_if_focused);
  display.multimonitor_focus_mode->store       (config.display.focus_mode);
  display.confirm_mode_changes->store          (config.display.confirm_mode_changes);
  display.save_monitor_prefs->store            (config.display.save_monitor_prefs);
  display.save_resolution->store               (config.display.resolution.save);
  display.warn_no_mpo_planes->store            (config.display.warn_no_mpo_planes);
  display.allow_refresh_change->store          (config.display.allow_refresh_change);

  if ((! config.display.resolution.override.isZero ()) || config.display.resolution.save)
  {
    if (config.display.resolution.save)
    {
      display.override_resolution->store (
                       std::format ( L"{}x{}", config.display.resolution.override.x,
                                               config.display.resolution.override.y )
      );
      display.override_refresh->store         (config.display.refresh_rate);
    }

    else
    {
      display.override_resolution->store (L"0x0");
      display.override_refresh->store    (  0.0f);
    }
  }

  wchar_t   wszTargetFps   [16] = { };
  wchar_t   wszTargetFpsBg [16] = { };
  swprintf (wszTargetFps,   L"%f", config.render.framerate.target_fps);
  swprintf (wszTargetFpsBg, L"%f", config.render.framerate.target_fps_bg);

  render.framerate.target_fps->store         (wszTargetFps);//__target_fps);
  render.framerate.target_fps_bg->store      (wszTargetFpsBg);//__target_fps_bg);
  render.framerate.last_monitor_path->store  (config.render.framerate.last_monitor_path);
  render.framerate.last_refresh_rate->store  (config.render.framerate.last_refresh_rate);
  render.framerate.sleepless_render->store   (config.render.framerate.sleepless_render);
  render.framerate.sleepless_window->store   (config.render.framerate.sleepless_window);
  render.framerate.enable_mmcss->store       (config.render.framerate.enable_mmcss);
  render.framerate.use_amd_mwaitx->store     (config.render.framerate.use_amd_mwaitx);

  render.framerate.override_cpu_count->store (config.render.framerate.override_num_cpus);

  if (  SK_IsInjected ()                       ||
      ( SK_GetDLLRole () & DLL_ROLE::DInput8 ) ||
      ( SK_GetDLLRole () & DLL_ROLE::D3D9    ) ||
      ( SK_GetDLLRole () & DLL_ROLE::D3D8    ) ||
      ( SK_GetDLLRole () & DLL_ROLE::DDraw   ) ||
      ( SK_GetDLLRole () & DLL_ROLE::DXGI    ) )
  {
    render.framerate.wait_for_vblank->store  (config.render.framerate.wait_for_vblank);
    render.framerate.prerender_limit->store  (config.render.framerate.pre_render_limit);
    render.framerate.buffer_count->store     (config.render.framerate.buffer_count);

    scheduling.priority.raise_always->store        (config.priority.raise_always);
    scheduling.priority.raise_in_bg->store         (config.priority.raise_bg);
    scheduling.priority.raise_in_fg->store         (config.priority.raise_fg);
    scheduling.priority.highest_priority->store    (config.priority.highest_priority);
    scheduling.priority.deny_foreign_change->store (config.priority.deny_foreign_change);
    scheduling.priority.min_render_priority->store (config.priority.minimum_render_prio);
    scheduling.priority.cpu_affinity_mask->store   (config.priority.cpu_affinity_mask);
    scheduling.priority.perf_cores_only->store     (config.priority.perf_cores_only);

    if (render.framerate.rescan_ratio != nullptr)
    {
      render.framerate.rescan_ratio->store    (config.render.framerate.rescan_ratio);

      if (! config.render.framerate.rescan_ratio.empty ())
      {
        swscanf ( config.render.framerate.rescan_ratio.c_str (), L"%i/%i",
          (INT *)&config.render.framerate.rescan_.Numerator,
          (INT *)&config.render.framerate.rescan_.Denom);
      }

      if ( config.render.framerate.rescan_.Numerator != static_cast <UINT> (-1) &&
           config.render.framerate.rescan_.Denom     !=                      0 )
      {
        config.render.framerate.refresh_rate =
            sk::narrow_cast <float> (config.render.framerate.rescan_.Numerator) /
            sk::narrow_cast <float> (config.render.framerate.rescan_.Denom);
      }
    }

    if (render.framerate.refresh_rate != nullptr)
      render.framerate.refresh_rate->store        (config.render.framerate.refresh_rate);

    render.framerate.present_interval->store      (config.render.framerate.present_interval);
    render.framerate.sync_interval_clamp->store   (config.render.framerate.sync_interval_clamp);
    render.framerate.tearing_mode->store          (config.render.framerate.tearing_mode);
    render.framerate.enforcement_policy->store    (config.render.framerate.enforcement_policy);
    render.framerate.enable_etw_tracing->store    (config.render.framerate.enable_etw_tracing);

    render.framerate.latent_sync.offset->store    (config.render.framerate.latent_sync.scanline_offset);
    render.framerate.latent_sync.resync->store    (config.render.framerate.latent_sync.scanline_resync);
    render.framerate.latent_sync.error->store     (config.render.framerate.latent_sync.scanline_error);
    render.framerate.latent_sync.bias->store      (config.render.framerate.latent_sync.delay_bias);
    render.framerate.latent_sync.auto_bias->store (config.render.framerate.latent_sync.auto_bias);
    render.framerate.latent_sync.max_auto_bias
                                          ->store (config.render.framerate.latent_sync.max_auto_bias);

    if (config.render.framerate.latent_sync.auto_bias_target.ms != 0.0f)
    {
      wchar_t   wszMilliseconds [16] = { };
      swprintf (wszMilliseconds, L"%f", config.render.framerate.latent_sync.auto_bias_target.ms);

      render.framerate.latent_sync.auto_bias_target->store (wszMilliseconds);
    }

    else
    {
      wchar_t   wszPercent [16] = { };
      swprintf (wszPercent, L"%08.6f", 100.0f * config.render.framerate.latent_sync.auto_bias_target.percent);

      SK_RemoveTrailingDecimalZeros                        (wszPercent);
      lstrcatW                                             (wszPercent, L"%");
      render.framerate.latent_sync.auto_bias_target->store (wszPercent);
    }

    texture.d3d9.clamp_lod_bias->store              (config.textures.clamp_lod_bias);

    // SLI only works in Direct3D
    //  + Keep these out of the INI on non-SLI systems for simplicity
    if (nvapi_init && sk::NVAPI::nv_hardware && sk::NVAPI::CountSLIGPUs ())
    {
      nvidia.sli.compatibility->store               (config.nvidia.sli.compatibility);
      nvidia.sli.mode->store                        (config.nvidia.sli.mode);
      nvidia.sli.num_gpus->store                    (config.nvidia.sli.num_gpus);
      nvidia.sli.override->store                    (config.nvidia.sli.override);
    }

    render.framerate.auto_low_latency->store        (config.render.framerate.auto_low_latency.waiting);
    render.framerate.auto_vrr_triggered->store      (config.render.framerate.auto_low_latency.triggered);
    render.framerate.auto_low_latency_ex->store     (config.render.framerate.auto_low_latency.policy.ultra_low_latency);
    render.framerate.auto_low_latency_optin->store  (config.render.framerate.auto_low_latency.policy.global_opt);
    render.framerate.auto_low_latency_reapply->store(config.render.framerate.auto_low_latency.policy.auto_reapply);

    if (  SK_IsInjected ()                       ||
        ( SK_GetDLLRole () & DLL_ROLE::DInput8 ) ||
        ( SK_GetDLLRole () & DLL_ROLE::D3D8    ) ||
        ( SK_GetDLLRole () & DLL_ROLE::DDraw   ) ||
        ( SK_GetDLLRole () & DLL_ROLE::DXGI    ) )
    {
      nvidia.reflex.enable->store                 (config.nvidia.reflex.enable);
      nvidia.reflex.low_latency->store            (config.nvidia.reflex.low_latency);
      nvidia.reflex.low_latency_boost->store      (config.nvidia.reflex.low_latency_boost);
      nvidia.reflex.engagement_policy->store      (config.nvidia.reflex.enforcement_site);
      nvidia.reflex.marker_optimization->store    (config.nvidia.reflex.marker_optimization);
      nvidia.reflex.override_native->store        (config.nvidia.reflex.override);
      nvidia.reflex.use_limiter->store            (config.nvidia.reflex.use_limiter);
      nvidia.reflex.combined_limiter->store       (config.nvidia.reflex.combined_limiter);
      nvidia.reflex.disable_native->store         (config.nvidia.reflex.disable_native);
      nvidia.reflex.show_detailed_widget->store   (config.nvidia.reflex.show_detailed_widget);
      nvidia.dlss.force_dlaa->store               (config.nvidia.dlss.force_dlaa);
      nvidia.dlss.use_sharpening->store           (config.nvidia.dlss.use_sharpening);
      nvidia.dlss.forced_sharpness->store         (config.nvidia.dlss.forced_sharpness);
      nvidia.dlss.auto_redirect_dll->store        (config.nvidia.dlss.auto_redirect_dlss);
      nvidia.dlss.forced_preset->store            (config.nvidia.dlss.forced_preset);
      nvidia.dlss.forced_auto_exposure->store     (config.nvidia.dlss.forced_auto_exposure);
      nvidia.dlss.forced_alpha_upscale->store     (config.nvidia.dlss.forced_alpha_upscale);
      nvidia.dlss.performance_scale->store        (config.nvidia.dlss.scale.performance);
      nvidia.dlss.balanced_scale->store           (config.nvidia.dlss.scale.balanced);
      nvidia.dlss.quality_scale->store            (config.nvidia.dlss.scale.quality);
      nvidia.dlss.ultra_performance_scale->store  (config.nvidia.dlss.scale.ultra_performance);
      nvidia.dlss.dynamic_resolution_min->store   (config.nvidia.dlss.scale.dynamic_min);
      nvidia.dlss.dynamic_resolution_max->store   (config.nvidia.dlss.scale.dynamic_max);
      nvidia.dlss.extra_pixels->store             (config.nvidia.dlss.compat.extra_pixels);
      nvidia.dlss.override_appid->store           (config.nvidia.dlss.compat.override_appid);
      nvidia.dlss.disable_ota_updates->store      (config.nvidia.dlss.disable_ota_updates);
      nvidia.dlss.show_active_features->store     (config.nvidia.dlss.show_active_features);
      nvidia.dlss.allow_scrgb->store              (config.nvidia.dlss.allow_scrgb);
      nvidia.dlss.spoof_feature_support->store    (config.nvidia.dlss.spoof_support);
      render.framerate.max_delta_time->store      (config.render.framerate.max_delta_time);
      render.framerate.flip_discard->store        (config.render.framerate.flip_discard);
      render.framerate.flip_sequential->store     (config.render.framerate.flip_sequential);
      render.framerate.disable_flip_model->store  (config.render.framerate.disable_flip);
      render.framerate.allow_dwm_tearing->store   (config.render.dxgi.allow_tearing);
      render.framerate.drop_late_frames->store    (config.render.framerate.drop_late_flips);
      if                                          (config.render.hdr.enable_32bpc)
      render.hdr.enable_32bpc->store              (config.render.hdr.enable_32bpc);
      render.hdr.remaster_8bpc_as_unorm->store    (config.render.hdr.remaster_8bpc_as_unorm);
      render.hdr.remaster_subnative_unorm->store  (config.render.hdr.remaster_subnative_as_unorm);
      render.hdr.last_used_colorspace->store      (config.render.hdr.last_used_colorspace);

      texture.d3d11.cache->store                  (config.textures.d3d11.cache);
      texture.d3d11.use_l3_hash->store            (config.textures.d3d11.use_l3_hash);
      texture.d3d11.precise_hash->store           (config.textures.d3d11.precise_hash);
      texture.d3d11.inject->store                 (config.textures.d3d11.inject);
      texture.d3d11.injection_keeps_format->store (config.textures.d3d11.injection_keeps_fmt);
      texture.d3d11.gen_mips->store               (config.textures.d3d11.generate_mips);

      texture.cache.max_entries->store            (config.textures.cache.max_entries);
      texture.cache.min_entries->store            (config.textures.cache.min_entries);
      texture.cache.max_evict->store              (config.textures.cache.max_evict);
      texture.cache.min_evict->store              (config.textures.cache.min_evict);
      texture.cache.max_size->store               (config.textures.cache.max_size);
      texture.cache.min_size->store               (config.textures.cache.min_size);

      texture.cache.ignore_non_mipped->store      (config.textures.cache.ignore_nonmipped);
      texture.cache.allow_staging->store          (config.textures.cache.allow_staging);
      texture.cache.allow_unsafe_refs->store      (config.textures.cache.allow_unsafe_refs);
      texture.cache.manage_residency->store       (config.textures.cache.residency_managemnt);

      render.dxgi.max_res->store (
                           std::format ( L"{}x{}", config.render.dxgi.res.max.x,
                                                   config.render.dxgi.res.max.y ) );
      render.dxgi.min_res->store (
                           std::format ( L"{}x{}", config.render.dxgi.res.min.x,
                                                   config.render.dxgi.res.min.y ) );

      render.dxgi.max_refresh->store              (config.render.dxgi.refresh.max);
      render.dxgi.min_refresh->store              (config.render.dxgi.refresh.min);

      render.dxgi.swapchain_wait->store           (config.render.framerate.swapchain_wait);

      switch (config.render.dxgi.scaling_mode)
      {
        case DXGI_MODE_SCALING_UNSPECIFIED:
          render.dxgi.scaling_mode->store (L"Unspecified");
          break;
        case DXGI_MODE_SCALING_CENTERED:
          render.dxgi.scaling_mode->store (L"Centered");
          break;
        case DXGI_MODE_SCALING_STRETCHED:
          render.dxgi.scaling_mode->store (L"Stretched");
          break;
        default:
          render.dxgi.scaling_mode->store (L"DontCare");
          break;
      }

      switch (config.render.dxgi.scanline_order)
      {
        case DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED:
          render.dxgi.scanline_order->store (L"Unspecified");
          break;
        case DXGI_MODE_SCANLINE_ORDER_PROGRESSIVE:
          render.dxgi.scanline_order->store (L"Progressive");
          break;
        case DXGI_MODE_SCANLINE_ORDER_LOWER_FIELD_FIRST:
          render.dxgi.scanline_order->store (L"LowerFieldFirst");
          break;
        case DXGI_MODE_SCANLINE_ORDER_UPPER_FIELD_FIRST:
          render.dxgi.scanline_order->store (L"UpperFieldFirst");
          break;
        default:
          render.dxgi.scanline_order->store (L"DontCare");
          break;
      }

      switch (config.render.dxgi.exception_mode)
      {
        case D3D11_RAISE_FLAG_DRIVER_INTERNAL_ERROR:
          render.dxgi.exception_mode->store (L"Raise");
          break;
        case 0:
          render.dxgi.exception_mode->store (L"Ignore");
          break;
        default:
          render.dxgi.exception_mode->store (L"DontCare");
          break;
      }

      render.dxgi.vram_budget_scale->store    (config.render.dxgi.vram_budget_scale);
      render.dxgi.fake_fullscreen_mode->store (config.render.dxgi.fake_fullscreen_mode);
      render.dxgi.debug_layer->store          (config.render.dxgi.debug_layer);
      render.dxgi.allow_d3d12_footguns->store (config.render.dxgi.allow_d3d12_footguns);
      render.dxgi.enhanced_depth->store       (config.render.dxgi.enhanced_depth);
      render.dxgi.deferred_isolation->store   (config.render.dxgi.deferred_isolation);
      render.dxgi.skip_present_test->store    (config.render.dxgi.present_test_skip);
      render.dxgi.msaa_samples->store         (config.render.dxgi.msaa_samples);
      render.dxgi.srgb_behavior->store        (config.render.dxgi.srgb_behavior);
      render.dxgi.low_spec_mode->store        (config.render.dxgi.low_spec_mode);
      render.dxgi.hide_hdr_support->store     (config.render.dxgi.hide_hdr_support);
      render.dxgi.hdr_metadata_override->store(config.render.dxgi.hdr_metadata_override);
      render.dxgi.temporary_dwm_hdr->store    (config.render.dxgi.temporary_dwm_hdr);
      render.dxgi.disable_virtual_vbi->store  (config.render.dxgi.disable_virtual_vbi);
      render.dxgi.clear_buffers_after_flip->
                                        store (config.render.dxgi.clear_flipped_chain);
      render.dxgi.enable_factory_cache->store (config.render.dxgi.use_factory_cache);
      render.dxgi.skip_redundant_modes->store (config.render.dxgi.skip_mode_changes);
      render.dxgi.warn_if_vram_exceeds->store (config.render.dxgi.warn_if_vram_exceeds);

      render.d3d12.max_anisotropy->store      (config.render.d3d12.max_anisotropy);
      render.d3d12.force_anisotropic->store   (config.render.d3d12.force_anisotropic);
      render.d3d12.force_lod_bias->store      (config.render.d3d12.force_lod_bias);

      render.dstorage.disable_bypass_io->store(config.render.dstorage.disable_bypass_io);
      render.dstorage.disable_telemetry->store(config.render.dstorage.disable_telemetry);
      render.dstorage.disable_gpu_decomp->
                                        store (config.render.dstorage.disable_gpu_decomp);
      render.dstorage.force_file_buffering->
                                        store (config.render.dstorage.force_file_buffering);
      render.dstorage.submit_threads->  store (config.render.dstorage.submit_threads);
      render.dstorage.cpu_decomp_threads->
                                        store (config.render.dstorage.cpu_decomp_threads);
      render.dstorage.hook_dstorage->   store (config.render.dstorage.enable_hooks);
    }

    if ( SK_IsInjected () || ( SK_GetDLLRole () & DLL_ROLE::D3D9    ) ||
                             ( SK_GetDLLRole () & DLL_ROLE::DInput8 ) )
    {
      render.d3d9.force_d3d9ex->store         (config.render.d3d9.force_d3d9ex);
      render.d3d9.enable_flipex->store        (config.render.d3d9.enable_flipex);
      render.d3d9.enable_texture_mods->store  (config.textures.d3d9_mod);
      render.d3d9.use_d3d9on12->store         (config.render.d3d9.use_d3d9on12);
    }
  }

  if (  SK_IsInjected () ||
      ( SK_GetDLLRole () & DLL_ROLE::OpenGL ) )
  {
    render.gl.enable_debug->store    (config.render.gl.debug.enable);
    render.gl.prefer_10bpc->store    (config.render.gl.prefer_10bpc);
    render.gl.upgrade_zbuffer->store (config.render.gl.upgrade_zbuffer);
  }

  // Don't write this setting unless an AddOn capable version of ReShade is loaded
  if (config.reshade.is_addon)
    reshade_cfg.draw_first->store             (config.reshade.draw_first);

  notifications.location->store               (config.notifications.location);
  notifications.silent->store                 (config.notifications.silent);

  if (SK_GetFramesDrawn ())
  {
    render.osd.draw_in_vidcap->store          (config.render.osd.draw_in_vidcap);

    config.render.osd.hdr_luminance = rb.ui_luminance;
    render.osd.hdr_luminance->store  (rb.ui_luminance);
  }

  texture.res_root->store                     (config.textures.d3d11.res_root);
  texture.dump_on_load->store                 (config.textures.dump_on_load);


  // Keep this setting hidden (not to be sneaky; but to prevent overwhelming users with
  //   extremely esoteric options -- this is needed for a lot of settings =P)
  if ((! ( config.file_io.trace_reads  || config.file_io.trace_writes )) ||
         ( config.file_io.ignore_reads->entire_thread.size  () +
           config.file_io.ignore_reads->single_file.size    () ) == 0 ||
         ( config.file_io.ignore_writes->entire_thread.size () +
           config.file_io.ignore_writes->single_file.size   () ) == 0
     )
  {
    dll_ini->remove_section (L"FileIO.Trace");
  }

  else
  {
    if (config.file_io.trace_reads)
      reverse_engineering.file.trace_reads->store  (config.file_io.trace_reads);
    if (config.file_io.trace_writes)
      reverse_engineering.file.trace_writes->store (config.file_io.trace_writes);
  }


  static const GUID empty_guid = { };

  if (! IsEqualGUID (config.cpu.power_scheme_guid, empty_guid))
  {
                                                   wchar_t wszGuid [40] = { L'\0' };
    StringFromGUID2 ( config.cpu.power_scheme_guid,        wszGuid, 40);
                             cpu.power_scheme_guid->store (wszGuid);
  }
  else
    dll_ini->get_section (L"CPU.Power").remove_key (L"PowerSchemeGUID");


  dpi.disable->store                              (config.dpi.disable_scaling);
  dpi.per_monitor_aware->store                    (config.dpi.per_monitor.aware);
  dpi.per_monitor_all_threads->store              (config.dpi.per_monitor.aware_on_all_threads);

  sound.minimize_latency->store                   (config.sound.minimize_latency);


  platform.achievements.sound_file->store         (config.platform.achievements.sound_file);
  platform.achievements.play_sound->store         (config.platform.achievements.play_sound);
  platform.achievements.take_screenshot->store    (config.platform.achievements.take_screenshot);
  platform.achievements.fetch_friend_stats->store (config.platform.achievements.pull_friend_stats);
  platform.achievements.popup.origin->store       (
    SK_Steam_PopupOriginToWStr (config.platform.achievements.popup.origin)
  );
  platform.achievements.popup.inset->store        (config.platform.achievements.popup.inset);

  if (! config.platform.achievements.popup.show)
  {
    config.platform.achievements.popup.duration = 0;
  }

  platform.achievements.popup.duration->store     (config.platform.achievements.popup.duration);
  platform.achievements.popup.animate->store      (config.platform.achievements.popup.animate);
  platform.achievements.popup.show_title->store   (config.platform.achievements.popup.show_title);

  if (config.steam.appid == 0)
  {
    if (SK::SteamAPI::AppID () != 0 &&
        SK::SteamAPI::AppID () != 1)
    {
      config.steam.appid = SK::SteamAPI::AppID ();
    }
  }


  // Special K's AppID belongs to Special K, not this game!
  if (config.steam.appid == SPECIAL_KILLER_APPID)
  {   config.steam.appid               = 0;
      config.steam.auto_inject         = false;
      config.steam.force_load_steamapi = false;
      config.steam.dll_path            = L"";
  }

  steam.system.appid->store                    (config.steam.appid);
  steam.system.init_delay->store               (config.steam.init_delay);
  steam.system.auto_pump->store                (config.steam.auto_pump_callbacks);
  steam.system.block_stat_callback->store      (config.steam.block_stat_callback);
  steam.system.filter_stat_callbacks->store    (config.steam.filter_stat_callback);
  steam.system.load_early->store               (config.steam.preload_client);
  steam.system.early_overlay->store            (config.steam.preload_overlay);
  steam.system.force_load->store               (config.steam.force_load_steamapi);
  steam.system.auto_inject->store              (config.steam.auto_inject);
  steam.system.dll_path->store                 (config.steam.dll_path);
  steam.system.crapcom_mode->store             (config.steam.crapcom_mode);
  platform.overlay.no_draw->store              (config.steam.disable_overlay);

  steam.callbacks.throttle->store              (ReadAcquire (&SK_SteamAPI_CallbackRateLimit));

  steam.social.online_status->store            (config.steam.online_status);

  steam.drm.spoof_BLoggedOn->store             (config.steam.spoof_BLoggedOn);
  eos.system.warned_online->store              (config.epic.warned_online);

  platform.system.notify_corner->store         (
                    SK_Steam_PopupOriginToWStr (config.platform.notify_corner));
  platform.system.reuse_overlay_pause->store   (config.platform.reuse_overlay_pause);
  platform.log.silent->store                   (config.platform.silent);
  platform.overlay.hdr_luminance->store        (config.platform.overlay_hdr_luminance);

  steam.screenshots.smart_capture->store       (config.steam.screenshots.enable_hook);

  screenshots.include_osd_default->store       (config.screenshots.show_osd_by_default);
  screenshots.keep_png_copy->store             (config.screenshots.png_compress);
  screenshots.play_sound->store                (config.screenshots.play_sound);
  screenshots.copy_to_clipboard->store         (config.screenshots.copy_to_clipboard);
  screenshots.embed_nickname->store            (config.screenshots.embed_nickname);
  screenshots.override_path->store             (config.screenshots.override_path);
  screenshots.filename_format->store           (config.screenshots.filename_format);

  screenshots.compression_quality->store       (config.screenshots.compression_quality);
  screenshots.compatibility_mode->store        (config.screenshots.compatibility_mode);

  screenshots.png.store_hdr->store             (config.screenshots.use_hdr_png);
  screenshots.allow_hdr_clipboard->store       (config.screenshots.allow_hdr_clipboard);
  screenshots.png.st2084_bits->store           (config.screenshots.max_st2084_bits);
  screenshots.clipboard_hdr_format->store      (config.screenshots.clipboard_hdr_format);

  screenshots.jxl.use_jxl->store               (config.screenshots.use_jxl);
  screenshots.avif.use_avif->store             (config.screenshots.use_avif);
  screenshots.avif.yuv_subsampling->store      (config.screenshots.avif.yuv_subsampling);
  screenshots.avif.scrgb_bit_depth->store      (config.screenshots.avif.scrgb_bit_depth);
  screenshots.avif.compression_speed->store    (config.screenshots.avif.compression_speed);

  uplay.overlay.hdr_luminance->store           (config.uplay.overlay_luminance);
  discord.overlay.hdr_luminance->store         (config.discord.overlay_luminance);
  rtss.overlay.hdr_luminance->store            (config.rtss.overlay_luminance);

  silent->store                                (config.system.silent);
  log_level->store                             (config.system.log_level);
  prefer_fahrenheit->store                     (config.system.prefer_fahrenheit);


  nvidia.api.disable->store                    (! config.apis.NvAPI.enable);
  amd.adl.disable->store                       (! config.apis.ADL.enable);
  microsoft.d3dkmt.disable_perfdata->store     (! config.apis.D3DKMT.enable_perfdata);


  // Don't store this setting at shutdown  (it may have been turned off automatically)
  if (__SK_DLL_Ending == false)
  {
    handle_crashes->store                      (config.system.handle_crashes);
    crash_suppression->store                   (config.system.suppress_crashes);
  }

  silent_crash->store(config.system.silent_crash);

  game_output->store                           (config.system.game_output);

  // Only add this to the INI file if it differs from default
  if (config.system.display_debug_out != debug_output->get_value ())
  {
    debug_output->store                        (config.system.display_debug_out);
  }

  debug_wait->store                            (config.system.wait_for_debugger);
  trace_libraries->store                       (config.system.trace_load_library);
  strict_compliance->store                     (config.system.strict_compliance);
  init_delay->store                            (config.system.global_inject_delay);
  return_to_skif->store                        (config.system.return_to_skif);
  auto_load_asi_files->store                   (config.system.auto_load_asi_files);
  version->store                               (SK_GetVersionStrW ());

  if (! SK_IsInjected ())
  {
    HKEY     hKey;
    LSTATUS lsKey =
      RegCreateKeyW (HKEY_CURRENT_USER,
                       LR"(SOFTWARE\Kaldaien\Special K\Local)",
                         &hKey);
    if (ERROR_SUCCESS == lsKey)
    {
      LSTATUS lsRegSet = RegSetValueExW (
             hKey, SK_GetModuleFullName (SK_GetDLL ()).c_str (),
                0, REG_SZ, (LPBYTE)        SK_GetVersionStrW (),
                            (DWORD)wcslen (SK_GetVersionStrW ()) * sizeof (wchar_t) );

      if (lsRegSet != ERROR_SUCCESS)
      {
        SK_LOGi0 (
          L"Failed to store local injection record in system registry! Err=%x",
          lsRegSet );
      }

      RegCloseKey (hKey);
    }
  }

  skif_autostop_behavior->store                (config.skif.auto_stop_behavior);


  wchar_t wszFullName [ MAX_PATH + 2 ] = { };

  lstrcatW (wszFullName, SK_GetConfigPath ());
  lstrcatW (wszFullName,       name.c_str ());
  lstrcatW (wszFullName,             L".ini");

  if (dll_ini != nullptr)
      dll_ini->write ( wszFullName );

  SK_ImGui_Widgets->SaveConfig ();

  if (     osd_ini)      osd_ini->write ();
  if (   input_ini)    input_ini->write ();
  if (platform_ini) platform_ini->write ();
  if (   macro_ini)    macro_ini->write ();
  if (  notify_ini)   notify_ini->write ();



  if (close_config)
  {
    if (dll_ini != nullptr)
    {
      delete dll_ini;
             dll_ini = nullptr;
    }

    if (osd_ini != nullptr)
    {
      delete osd_ini;
             osd_ini = nullptr;
    }

    if (input_ini != nullptr)
    {
      delete input_ini;
             input_ini = nullptr;
    }

    if (platform_ini != nullptr)
    {
      delete platform_ini;
             platform_ini = nullptr;
    }

    if (macro_ini != nullptr)
    {
      delete macro_ini;
             macro_ini = nullptr;
    }
  }
}

constexpr UINT
SK_MakeKeyMask ( const SHORT vKey,
                 const bool  ctrl,
                 const bool  shift,
                 const bool  alt )
{
  return
    static_cast <UINT> (
      ( vKey | ( (ctrl != 0) <<  9 ) |
               ( (shift!= 0) << 10 ) |
               ( (alt  != 0) << 11 ) )
    );
}

void
SK_KeyMap_StandardizeNames (wchar_t* wszNameToFormalize)
{
  if (wszNameToFormalize == nullptr)
    return;

  wchar_t*                  pwszName = wszNameToFormalize;
                CharUpperW (pwszName);
   pwszName = SK_CharNextW (pwszName);

  bool lower = true;

  while ( pwszName != nullptr &&
         *pwszName != L'\0' )
  {
    if (lower) CharLowerW (pwszName);
    else       CharUpperW (pwszName);

    lower =
      (! iswspace (*pwszName));

    pwszName =
      SK_CharNextW (pwszName);
  }
}
void
SK_Keybind::update (void)
{
  human_readable.clear ();

  wchar_t* key_name =
    (*virtKeyCodeToHumanKeyName)[static_cast <BYTE>(vKey & 0xFF)];

  if (*key_name == L'\0')
  {
    ctrl           = false;
    alt            = false;
    shift          = false;
    masked_code    = 0x0;
    human_readable = L"<Not Bound>";

    return;
  }

  std::queue <std::wstring> words;

  if (ctrl)
    words.emplace (L"Ctrl");

  if (alt)
    words.emplace (L"Alt");

  if (shift)
    words.emplace (L"Shift");

  words.emplace (key_name);

  while (! words.empty ())
  {
    human_readable += words.front ();
    words.pop ();

    if (! words.empty ())
      human_readable += L"+";
  }

  masked_code =
    SK_MakeKeyMask (vKey & 0xFF, ctrl, shift, alt);
}

void
SK_Keybind::parse (void)
{
  vKey = 0x00;

  static bool init = false;

  static auto& humanToVirtual = humanKeyNameToVirtKeyCode.get          ();
  static auto& virtualToHuman = virtKeyCodeToHumanKeyName.get          ();
  static auto& virtualToLocal = virtKeyCodeToFullyLocalizedKeyName.get ();

  static const auto _PushVirtualToHuman =
  [] (BYTE vKey_, const wchar_t* wszHumanName)
  {
    if (! wszHumanName)
      return;

    auto& pair_builder =
      virtualToHuman [vKey_];

    wcsncpy_s ( pair_builder, 32,
                wszHumanName, _TRUNCATE );
  };

  static const auto _PushVirtualToLocal =
  [] (BYTE vKey_, const wchar_t* wszHumanName)
  {
    if (! wszHumanName)
      return;

    auto& pair_builder =
      virtualToLocal [vKey_];

    wcsncpy_s ( pair_builder, 32,
                wszHumanName, _TRUNCATE );
  };

  static const auto _PushHumanToVirtual =
  [] (const wchar_t* wszHumanName, BYTE vKey_)
  {
    if (! wszHumanName)
      return;

    humanToVirtual.emplace (
      hash_string (wszHumanName),
        vKey_
    );
  };

  if (! init)
  {
    for (int i = 0; i < 0xFF; i++)
    {
      wchar_t name [32] = { };

      switch (i)
      {
        case VK_F1:          wcscat (name, L"F1");           break;
        case VK_F2:          wcscat (name, L"F2");           break;
        case VK_F3:          wcscat (name, L"F3");           break;
        case VK_F4:          wcscat (name, L"F4");           break;
        case VK_F5:          wcscat (name, L"F5");           break;
        case VK_F6:          wcscat (name, L"F6");           break;
        case VK_F7:          wcscat (name, L"F7");           break;
        case VK_F8:          wcscat (name, L"F8");           break;
        case VK_F9:          wcscat (name, L"F9");           break;
        case VK_F10:         wcscat (name, L"F10");          break;
        case VK_F11:         wcscat (name, L"F11");          break;
        case VK_F12:         wcscat (name, L"F12");          break;
        case VK_F13:         wcscat (name, L"F13");          break;
        case VK_F14:         wcscat (name, L"F14");          break;
        case VK_F15:         wcscat (name, L"F15");          break;
        case VK_F16:         wcscat (name, L"F16");          break;
        case VK_F17:         wcscat (name, L"F17");          break;
        case VK_F18:         wcscat (name, L"F18");          break;
        case VK_F19:         wcscat (name, L"F19");          break;
        case VK_F20:         wcscat (name, L"F20");          break;
        case VK_F21:         wcscat (name, L"F21");          break;
        case VK_F22:         wcscat (name, L"F22");          break;
        case VK_F23:         wcscat (name, L"F23");          break;
        case VK_F24:         wcscat (name, L"F24");          break;
        case VK_PRINT:       wcscat (name, L"Print Screen"); break;
        case VK_SCROLL:      wcscat (name, L"Scroll Lock");  break;
        case VK_PAUSE:       wcscat (name, L"Pause Break");  break;

        default:
        {
          unsigned int scanCode =
            ( MapVirtualKey (i, 0) & 0xFFU );
          unsigned short int temp      =  0;

          bool asc = (i <= 32);

          if (! asc && i != VK_DIVIDE)
          {
                                     BYTE buf [256] = { };
             asc = ToAscii ( i, scanCode, buf, &temp, 1 );
          }

          scanCode             <<= 16U;
          scanCode   |= ( 0x1U <<  25U  );

          if (! asc)
            scanCode |= ( 0x1U << 24U   );

          GetKeyNameText ( scanCode,
                             name,
                               32 );

          SK_KeyMap_StandardizeNames (name);
        } break;
      }


      if ( i != VK_CONTROL  && i != VK_MENU     &&
           i != VK_SHIFT    && i != VK_OEM_PLUS && i != VK_OEM_MINUS &&
           i != VK_LSHIFT   && i != VK_RSHIFT   &&
           i != VK_LCONTROL && i != VK_RCONTROL &&
           i != VK_LMENU    && i != VK_RMENU    && i != VK_ADD   && // Num Plus
           i != VK_BACK     && i != VK_HOME     && i != VK_END   &&
           i != VK_DELETE   && i != VK_INSERT   && i != VK_PRIOR &&
           i != VK_NEXT
         )
      {
        _PushHumanToVirtual (name, sk::narrow_cast <BYTE> (i));
        _PushVirtualToHuman (      sk::narrow_cast <BYTE> (i), name);
      }

      _PushVirtualToLocal   (      sk::narrow_cast <BYTE> (i), name);
    }

    _PushHumanToVirtual (L"Plus",        sk::narrow_cast <BYTE> (VK_OEM_PLUS));
    _PushHumanToVirtual (L"Minus",       sk::narrow_cast <BYTE> (VK_OEM_MINUS));
    _PushHumanToVirtual (L"Ctrl",        sk::narrow_cast <BYTE> (VK_CONTROL));
    _PushHumanToVirtual (L"Alt",         sk::narrow_cast <BYTE> (VK_MENU));
    _PushHumanToVirtual (L"Shift",       sk::narrow_cast <BYTE> (VK_SHIFT));
    _PushHumanToVirtual (L"Left Shift",  sk::narrow_cast <BYTE> (VK_LSHIFT));
    _PushHumanToVirtual (L"Right Shift", sk::narrow_cast <BYTE> (VK_RSHIFT));
    _PushHumanToVirtual (L"Left Alt",    sk::narrow_cast <BYTE> (VK_LMENU));
    _PushHumanToVirtual (L"Right Alt",   sk::narrow_cast <BYTE> (VK_RMENU));
    _PushHumanToVirtual (L"Left Ctrl",   sk::narrow_cast <BYTE> (VK_LCONTROL));
    _PushHumanToVirtual (L"Right Ctrl",  sk::narrow_cast <BYTE> (VK_RCONTROL));
    _PushHumanToVirtual (L"Backspace",   sk::narrow_cast <BYTE> (VK_BACK));
    _PushHumanToVirtual (L"Home",        sk::narrow_cast <BYTE> (VK_HOME));
    _PushHumanToVirtual (L"End",         sk::narrow_cast <BYTE> (VK_END));
    _PushHumanToVirtual (L"Insert",      sk::narrow_cast <BYTE> (VK_INSERT));
    _PushHumanToVirtual (L"Delete",      sk::narrow_cast <BYTE> (VK_DELETE));
    _PushHumanToVirtual (L"Page Up",     sk::narrow_cast <BYTE> (VK_PRIOR));
    _PushHumanToVirtual (L"Page Down",   sk::narrow_cast <BYTE> (VK_NEXT));

    _PushVirtualToHuman (sk::narrow_cast <BYTE> (VK_CONTROL),   L"Ctrl");
    _PushVirtualToHuman (sk::narrow_cast <BYTE> (VK_MENU),      L"Alt");
    _PushVirtualToHuman (sk::narrow_cast <BYTE> (VK_SHIFT),     L"Shift");
    _PushVirtualToHuman (sk::narrow_cast <BYTE> (VK_OEM_PLUS),  L"Plus");
    _PushVirtualToHuman (sk::narrow_cast <BYTE> (VK_OEM_MINUS), L"Minus");
    _PushVirtualToHuman (sk::narrow_cast <BYTE> (VK_LSHIFT),    L"Left Shift");
    _PushVirtualToHuman (sk::narrow_cast <BYTE> (VK_RSHIFT),    L"Right Shift");
    _PushVirtualToHuman (sk::narrow_cast <BYTE> (VK_LMENU),     L"Left Alt");
    _PushVirtualToHuman (sk::narrow_cast <BYTE> (VK_RMENU),     L"Right Alt");
    _PushVirtualToHuman (sk::narrow_cast <BYTE> (VK_LCONTROL),  L"Left Ctrl");
    _PushVirtualToHuman (sk::narrow_cast <BYTE> (VK_RCONTROL),  L"Right Ctrl");
    _PushVirtualToHuman (sk::narrow_cast <BYTE> (VK_BACK),      L"Backspace");
    _PushVirtualToHuman (sk::narrow_cast <BYTE> (VK_HOME),      L"Home");
    _PushVirtualToHuman (sk::narrow_cast <BYTE> (VK_END),       L"End");
    _PushVirtualToHuman (sk::narrow_cast <BYTE> (VK_INSERT),    L"Insert");
    _PushVirtualToHuman (sk::narrow_cast <BYTE> (VK_DELETE),    L"Delete");
    _PushVirtualToHuman (sk::narrow_cast <BYTE> (VK_PRIOR),     L"Page Up");
    _PushVirtualToHuman (sk::narrow_cast <BYTE> (VK_NEXT),      L"Page Down");

    _PushHumanToVirtual (L"Num Plus", sk::narrow_cast <BYTE> (VK_ADD));
    _PushVirtualToHuman (             sk::narrow_cast <BYTE> (VK_ADD), L"Num Plus");

    init = true;
  }

  wchar_t   wszKeyBind [128] = { };
  lstrcatW (wszKeyBind, human_readable.c_str ());

  wchar_t* wszBuf = nullptr;
  wchar_t* wszTok = std::wcstok (wszKeyBind, L"+", &wszBuf);

  vKey  = 0x0;
  ctrl  = false;
  alt   = false;
  shift = false;

  if (wszTok == nullptr)
  {
    if (*wszKeyBind != L'\0')
    {
      SK_KeyMap_StandardizeNames (wszKeyBind);

      if (*wszKeyBind != L'\0')
      {
        vKey =
          humanToVirtual [hash_string (wszKeyBind)];
      }
    }
  }

  while (wszTok != nullptr)
  {
    SK_KeyMap_StandardizeNames (wszTok);

    if (*wszTok != L'\0')
    {
      BYTE vKey_ =
        humanToVirtual [hash_string (wszTok)];

      if (vKey_ == VK_CONTROL)
        ctrl  = true;
      else if (vKey_ == VK_SHIFT)
        shift = true;
      else if (vKey_ == VK_MENU)
        alt   = true;
      else
        vKey = vKey_;
    }

    wszTok =
      std::wcstok (nullptr, L"+", &wszBuf);
  }

  masked_code =
    SK_MakeKeyMask (vKey & 0xFFU, ctrl, shift, alt);
}





bool
SK_AppCache_Manager::loadAppCacheForExe (const wchar_t* wszExe)
{
  ///SK_LOG0 ( ( L" [!] SK_AppCache_Manager::loadAppCacheForExe (%ws)", wszExe ),
  ///            L" AppCache " );

  //
  // "AppCaches" are basically fast-path lookups for a game's user-friendly
  //   name that avoid using SteamAPI, the Steam Client or exhaustive app-
  //     manifest file searches for games we have seen before.
  //
  const wchar_t* wszPath =
    StrStrIW ( wszExe, LR"(SteamApps\common\)" );

  if (wszPath != nullptr)
  {
    auto wszRelStr =
      SK_CharNextW (
          StrStrIW (
      SK_CharNextW (
          StrStrIW ( wszPath, LR"(\)" )
                   ),         LR"(\)" )
                   );

    std::wstring wszRelPath = L"";

    if (wszRelStr != nullptr)
    {
      wszRelPath = wszRelStr;
      wszRelPath += L'\0';

      PathRemoveFileSpecW (
        wszRelPath.data ()
      );

      //SK_LOG0 ( ( L" Relative Path: %ws ", wszRelPath.data () ),
      //            L" AppCache " );
    }

    wchar_t            wszAppCache [MAX_PATH + 2] = { };
    std::wstring_view
      app_cache_view ( wszAppCache, MAX_PATH + 2 );

    SK_FormatStringViewW (
      app_cache_view, LR"(%ws\Profiles\AppCache\%s\SpecialK.AppCache)",
        SK_GetInstallPath (),
          wszRelPath.data ()
                         );

    // It may be necessary to write the INI immediately after creating it,
    //   but usually not.
    bool need_to_create =
      (! std::filesystem::exists (wszAppCache));

    if ( need_to_create )
      SK_CreateDirectories (wszAppCache);

    if (app_cache_db == nullptr)
        app_cache_db =
      SK_CreateINI         (wszAppCache);
    else
    {
      if (_wcsicmp (app_cache_db->get_filename (), wszAppCache))
                    app_cache_db->rename          (wszAppCache);

      app_cache_db->reload ();
    }

    if (app_cache_db != nullptr)
    {
      loadDepotCache ();

      if ( need_to_create )
        app_cache_db->write ();
    }
  }

  // The app name is literally in the command line, but it is best we
  //   read the .egstore manifest for consistency

  else if ( StrStrIA (GetCommandLineA (), "-epicapp=") ||
                         PathFileExistsW (L".egstore") ||
                      PathFileExistsW (L"../.egstore") ||
                      StrStrIW (wszExe, L"Epic Games") )

    // uPlay's launcher does not forward the command line, so fallback
    //   to a directory check if "-epicapp=" is undefined.

  {
    // Epic games might have multiple manifests.
    //
    //  * Break-out early after finding the first one...
    //
    bool found_manifest = false;

    try {
      std::filesystem::path path =
        std::filesystem::path (wszExe).lexically_normal ();

      while (! std::filesystem::equivalent ( path.parent_path    (),
                                             path.root_directory () ) )
      {
        if (found_manifest)
          break;

        if (std::filesystem::is_directory (path / L".egstore"))
        {
          for ( const auto& file : std::filesystem::directory_iterator (path / L".egstore") )
          {
            if (found_manifest)
              break;

            if (! file.is_regular_file ())
              continue;

            if (file.path ().extension ().compare (L".mancpn") == 0)
            {
              if (std::fstream mancpn (file.path (), std::fstream::in);
                               mancpn.is_open ())
              {
                char                     szLine [512] = { };
                while (! mancpn.getline (szLine, 511).eof ())
                {
                  if (StrStrIA (szLine, "\"AppName\"") != nullptr)
                  {
                    char       szEpicApp [65] = { };
                    const char  *substr =     StrStrIA (szLine, ":");
                    strncpy_s (szEpicApp, 64, StrStrIA (substr, "\"") + 1, _TRUNCATE);
                     *strrchr (szEpicApp, '"') = '\0';

                    wchar_t            wszAppCache [MAX_PATH + 2] = { };
                    std::wstring_view
                      app_cache_view ( wszAppCache, MAX_PATH + 2 );

                    SK_FormatStringViewW (
                      app_cache_view, LR"(%ws\Profiles\AppCache\#EpicApps\%hs\SpecialK.AppCache)",
                                         SK_GetInstallPath (), szEpicApp
                                         );

                    // It may be necessary to write the INI immediately after creating it,
                    //   but usually not.
                    bool need_to_create =
                      (! std::filesystem::exists (wszAppCache));

                    if ( need_to_create )
                      SK_CreateDirectories (wszAppCache);

                    if (app_cache_db == nullptr)
                        app_cache_db =
                      SK_CreateINI         (wszAppCache);
                    else
                    {
                      if (_wcsicmp (app_cache_db->get_filename (), wszAppCache))
                                    app_cache_db->rename          (wszAppCache);

                      app_cache_db->reload ();
                    }

                    if (app_cache_db != nullptr)
                    {
                      if ( need_to_create )
                        app_cache_db->write ();
                    }

                    path = LR"(\)";

                    found_manifest = true;

                    break;
                  }
                }
              }
            }
          }
        }

        path =
          path.parent_path ().lexically_normal ();
      }
    }

    catch (const std::exception& e)
    {
      SK_LOGs0 ( L" AppCache ",
                 L"Appcache Parse Failure: %hs during Epic Name Lookup",
                                             e.what () );
    }
  }

  if (app_cache_db != nullptr)
    return true;

  return false;
}

AppId64_t
SK_AppCache_Manager::getAppIDFromPath (const wchar_t* wszPath) const
{
  if (app_cache_db == nullptr)
    return 0;

  iSK_INISection&
    fwd_map =
      app_cache_db->get_section (L"AppID_Cache.FwdMap");

  const wchar_t* wszSteamApps =
    StrStrIW ( wszPath, LR"(SteamApps\common\)" );

  if (wszSteamApps != nullptr)
  {
    const wchar_t* wszRelPath =
    {
      SK_CharNextW (
          StrStrIW (
      SK_CharNextW (
          StrStrIW ( wszSteamApps, LR"(\)" )
                   ),              LR"(\)"
        )
      )
    };

    if (            fwd_map.contains_key (wszRelPath)           )
      return _wtoi (fwd_map.get_value    (wszRelPath).c_str ());
  }

  return 0;
}

std::wstring
SK_AppCache_Manager::getAppNameFromID (AppId64_t uiAppID) const
{
  if (app_cache_db == nullptr)
    return L"";

  const iSK_INISection&
    name_map =
      app_cache_db->get_section (L"AppID_Cache.Names");

  if ( std::wstring               app_id_as_wstr = std::to_wstring (uiAppID) ;
           name_map.contains_key (app_id_as_wstr) )
    return name_map.get_cvalue   (app_id_as_wstr);

  return L"";
}

std::wstring
SK_AppCache_Manager::getAppNameFromPath (const wchar_t* wszPath) const
{
  if (wszPath == nullptr)
    return L"";

  const AppId64_t uiAppID =
    getAppIDFromPath (wszPath);

  if (uiAppID != 0)
  {
    return getAppNameFromID (uiAppID);
  }

  CRegKey hkAppCacheRoot;
          hkAppCacheRoot.Open (HKEY_CURRENT_USER, LR"(Software\Kaldaien\Special K\Profiles)");

  if ((intptr_t)hkAppCacheRoot.m_hKey > 0)
  {
    std::filesystem::path path =
      std::filesystem::path (wszPath).lexically_normal ();

    try
    {
      wchar_t wszProfileName [MAX_PATH + 2] = { };

      while (! std::filesystem::equivalent ( path.parent_path    (),
                                             path.root_directory () ) )
      {
        *wszProfileName = L'\0';

        ULONG ulProfileLen = MAX_PATH;

        if ( ERROR_SUCCESS ==
               hkAppCacheRoot.QueryStringValue ( path.c_str (),
                                                 wszProfileName,
                                                   &ulProfileLen )
           )
        {
          return
            wszProfileName;
        }

        path =
          path.parent_path ().lexically_normal ();
      }
    }

    catch (const std::exception& e)
    {
      SK_LOGs0 ( L" AppCache ",
                 L"AppCache Parse Failure: %hs during Profile Name Lookup",
                                 e.what () );
    }
  }

  return L"";
}

bool
SK_AppCache_Manager::addAppToCache ( const wchar_t* wszFullPath,
                                     const wchar_t*,
                                     const wchar_t* wszAppName,
                                          AppId64_t uiAppID )
{
  if ( wszFullPath == nullptr ||
       wszAppName  == nullptr )
    return false;

  if (app_cache_db == nullptr)
    return false;

  try
  {
    if (StrStrIW (wszFullPath, LR"(SteamApps\common\)") == nullptr)
        return false;

    iSK_INISection& rev_map =
      app_cache_db->get_section (L"AppID_Cache.RevMap");
    iSK_INISection& fwd_map =
      app_cache_db->get_section (L"AppID_Cache.FwdMap");
    iSK_INISection& name_map =
      app_cache_db->get_section (L"AppID_Cache.Names");


    std::wstring wszRelativeCopy (
      std::wstring (wszFullPath) + L'\0'
                                 );

    const wchar_t* wszRelPath =
    {
      SK_CharNextW (
          StrStrIW (
      SK_CharNextW (
          StrStrIW (
          StrStrIW ( wszRelativeCopy.data (),
                                       LR"(SteamApps\common\)" ),
                                       LR"(\)"  )
                   ),                  LR"(\)"
                )
            )
    };

    wchar_t         wszAppID [32] = { };
    std::format_to (wszAppID, L"{:0}", uiAppID);

    if (wszRelPath != nullptr)
    {
      if (fwd_map.contains_key (wszRelPath))
        fwd_map.get_value      (wszRelPath) = wszAppID;
      else
        fwd_map.add_key_value  (wszRelPath,   wszAppID);


      if (rev_map.contains_key (wszAppID))
        rev_map.get_value      (wszAppID) = wszRelPath;
      else
        rev_map.add_key_value  (wszAppID,   wszRelPath);
    }


    if (name_map.contains_key (wszAppID))
      name_map.get_value      (wszAppID) = wszAppName;
    else
      name_map.add_key_value  (wszAppID,   wszAppName);

    app_cache_db->write ();

    return true;
  }

  catch (const std::exception& e)
  {
    SK_LOGs0 ( L" AppCache ",
               L"Exception: %hs during addAppToCache (...)", e.what () );
  }

  return false;
}

bool
SK_AppCache_Manager::addAppToCache ( const wchar_t* wszFullPath,
                                     const wchar_t* wszHostApp,
                                     const wchar_t* wszAppName,
                                     const char*    szEpicApp )
{
  std::ignore = wszFullPath;

  if (app_cache_db == nullptr)
    return false;

  iSK_INISection& rev_map =
    app_cache_db->get_section (L"EpicApp_Cache.RevMap");
  iSK_INISection& fwd_map =
    app_cache_db->get_section (L"EpicApp_Cache.FwdMap");
  iSK_INISection& name_map =
    app_cache_db->get_section (L"EpicApp_Cache.Names");

  const std::wstring  wszEpicApp (
    SK_UTF8ToWideChar (szEpicApp)
  );


  if (fwd_map.contains_key (wszHostApp))
    fwd_map.get_value      (wszHostApp) = wszFullPath;
  else
    fwd_map.add_key_value  (wszHostApp,   wszFullPath);

  if (rev_map.contains_key (wszFullPath))
    rev_map.get_value      (wszFullPath) = wszHostApp;
  else
    rev_map.add_key_value  (wszFullPath,   wszHostApp);

  if (name_map.contains_key (wszEpicApp))
    name_map.get_value      (wszEpicApp) = wszAppName;
  else
    name_map.add_key_value  (wszEpicApp,   wszAppName);


  app_cache_db->write ();


  return true;
}

std::wstring
SK_AppCache_Manager::getConfigPathFromAppPath (const wchar_t* wszPath) const
{
  if ( auto uiAppID  = getAppIDFromPath (wszPath) ;
            uiAppID != 0 )
  {
    return
      getConfigPathForAppID (uiAppID);
  }

  std::filesystem::path path =
    std::filesystem::path (wszPath).lexically_normal ();

  try
  {
    while (! std::filesystem::equivalent ( path.parent_path    (),
                                           path.root_directory () ) )
    {
      if (std::filesystem::is_directory (path / L".egstore"))
      {
        for ( const auto& file : std::filesystem::directory_iterator (path / L".egstore") )
        {
          if (! file.is_regular_file ())
            continue;

          if (file.path ().extension ().compare (L".mancpn") == 0)
          {
            if (std::fstream mancpn (file.path (), std::fstream::in);
                             mancpn.is_open ())
            {
              char                     szLine [512] = { };
              while (! mancpn.getline (szLine, 511).eof ())
              {
                if (StrStrIA (szLine, "\"AppName\"") != nullptr)
                {
                  char       szEpicApp [65] = { };
                  const char  *substr =     StrStrIA (szLine, ":");
                  strncpy_s (szEpicApp, 64, StrStrIA (substr, "\"") + 1, _TRUNCATE);
                   *strrchr (szEpicApp, '"') = '\0';

                   auto ret =
                    getConfigPathForEpicApp (szEpicApp);

                   if (! ret.empty ())
                     return ret;
                }
              }
            }
          }
        }
      }

      path =
        path.parent_path ().lexically_normal ();
    }
  }

  catch (const std::exception& e)
  {
    SK_LOGs0 ( L" AppCache ",
               L"Appcache Parse Failure: %hs during Epic Name Lookup",
                               e.what () );
  }

  CRegKey hkAppCacheRoot;
          hkAppCacheRoot.Open (HKEY_CURRENT_USER, LR"(Software\Kaldaien\Special K\Profiles)");

  if ((intptr_t)hkAppCacheRoot.m_hKey > 0)
  {
    path =
      std::filesystem::path (wszPath).lexically_normal ();

    try
    {
      wchar_t wszProfileName [MAX_PATH + 2] = { };

      while (! std::filesystem::equivalent ( path.parent_path    (),
                                             path.root_directory () ) )
      {
        *wszProfileName = L'\0';

        ULONG ulProfileLen = MAX_PATH;

        if ( ERROR_SUCCESS ==
               hkAppCacheRoot.QueryStringValue ( path.c_str (),
                                                 wszProfileName,
                                                   &ulProfileLen )
           )
        {
          auto ret =
            getConfigPathForGenericApp (wszProfileName);

          // ret should never be empty
          SK_ReleaseAssert (! ret.empty ());
          
          if (! ret.empty ())
            return ret;

          break;
        }

        path =
          path.parent_path ();
      }
    }

    catch (const std::exception& e)
    {
      SK_LOGs0 ( L" AppCache ",
                 L"AppCache Parse Failure: %hs during Profile Name Lookup",
                                 e.what () );
    }
  }

  return
    SK_GetNaiveConfigPath ();
}

std::wstring
SK_AppCache_Manager::getConfigPathFromCmdLine (const wchar_t* wszCmdLine) const
{
  if (wszCmdLine != nullptr)
  {
    if (const wchar_t *wszEpicAppSwitch  = StrStrIW (wszCmdLine, L"-epicapp=");
                       wszEpicAppSwitch != nullptr)
    {
      wchar_t wszEpicApp [65] = { };

      if (1 == swscanf (wszEpicAppSwitch, L"-epicapp=%ws ", wszEpicApp))
      {
        return
          getConfigPathForEpicApp (
            SK_WideCharToUTF8 (wszEpicApp).c_str ()
          );
      }
    }
  }

  return
    SK_GetNaiveConfigPath ();
}


static const
  std::unordered_set <wchar_t>
    invalid_file_chars =
    {
      L'\\', L'/', L':',
      L'*',  L'?', L'\"',
      L'<',  L'>', L'|',
    //L'&',

      //
      // Obviously a period is not an invalid character,
      //   but three of them in a row messes with
      //     Windows Explorer and some Steam games use
      //       ellipsis in their titles.
      //
      L'.'
    };

std::wstring
SK_AppCache_Manager::getAppNameFromEpicApp (const char* szEpicApp) const
{
  if (app_cache_db == nullptr || szEpicApp == nullptr)
    return L"";

  const iSK_INISection&
    name_map =
      app_cache_db->get_section (L"EpicApp_Cache.Names");

  const std::wstring app_name_as_wstr =
    SK_UTF8ToWideChar (szEpicApp);

  if (     name_map.contains_key (app_name_as_wstr) )
    return name_map.get_cvalue   (app_name_as_wstr);

  return L"";
}

std::wstring
SK_AppCache_Manager::getConfigPathForEpicApp (const char* szEpicApp) const
{
  // The cache is broken right now, we can do a full manifest lookup in as little time though.
  std::ignore = szEpicApp;

  static bool recursing = false;

  // If no AppCache (probably not a Steam game), or opting-out of central repo,
  //   then don't parse crap and just use the traditional path.
  if ( app_cache_db == nullptr || (! config.system.central_repository) )
    return SK_GetNaiveConfigPath ();

  std::wstring path = SK_GetNaiveConfigPath ();
  std::wstring name =
    SK_UTF8ToWideChar (SK::EOS::AppName ());

  // Non-trivial name = custom path, remove the old-style <program.exe>
  if (! name.empty ())
  {
    std::wstring original_dir (path);

    size_t pos = 0;
    if (  (pos = path.find (SK_GetHostApp (), pos)) != std::wstring::npos)
    {
      std::wstring       host_app (SK_GetHostApp ());
      path.replace (pos, host_app.length (), L"\0");
    }

    std::erase_if ( name,      [](wchar_t tval) {
      return invalid_file_chars.contains (tval);} );

    path =
      SK_FormatStringW ( LR"(%s\%s\)",
                         path.c_str (),
                         name.c_str () );

    SK_StripTrailingSlashesW (path.data ());

    if (recursing)
      return path;

    std::error_code                                  err;
    if (std::filesystem::is_directory (original_dir, err))
    {
      std::wstring old_ini =
                   dll_ini != nullptr ? dll_ini->get_filename ()
                                      : L"";

      recursing         = true;
      SK_GetConfigPathEx (true);
      SK_LoadConfigEx    (L"" );
      recursing         = false;

      // We've already parsed/written the new file, delete the old one
      if ((! old_ini.empty ())  &&
             dll_ini != nullptr &&
          _wcsicmp ( old_ini.c_str (), dll_ini->get_filename ()))
        DeleteFileW (old_ini.c_str ());

      SK_RecursiveMove ( original_dir.c_str (),
                                 path.c_str (), false );
    }
  }

  return
    path;
}

std::wstring
SK_AppCache_Manager::getConfigPathForGenericApp (const wchar_t* wszGenericAppName) const
{
  static bool recursing = false;

  // If no AppCache (probably not a Steam game), or opting-out of central repo,
  //   then don't parse crap and just use the traditional path.
  if (! config.system.central_repository)
    return SK_GetNaiveConfigPath ();

  std::wstring path = SK_GetNaiveConfigPath ();
  std::wstring name = wszGenericAppName;

  // Non-trivial name = custom path, remove the old-style <program.exe>
  if (! name.empty ())
  {
    std::wstring original_dir (path);

    size_t pos = 0;
    if (  (pos = path.find (SK_GetHostApp (), pos)) != std::wstring::npos)
    {
      std::wstring       host_app (SK_GetHostApp ());
      path.replace (pos, host_app.length (), L"\0");
    }

    std::erase_if ( name,      [](wchar_t tval) {
      return invalid_file_chars.contains (tval);} );

    path =
      SK_FormatStringW ( LR"(%s\%s\)",
                         path.c_str (),
                         name.c_str () );

    SK_StripTrailingSlashesW (path.data ());

    if (recursing)
      return path;

    std::error_code                                  err;
    if (std::filesystem::is_directory (original_dir, err))
    {
      std::wstring old_ini =
                   dll_ini != nullptr ? dll_ini->get_filename ()
                                      : L"";

      recursing         = true;
      SK_GetConfigPathEx (true);
      SK_LoadConfigEx    (L"" );
      recursing         = false;

      // We've already parsed/written the new file, delete the old one
      if ((! old_ini.empty ())  &&
             dll_ini != nullptr &&
          _wcsicmp ( old_ini.c_str (), dll_ini->get_filename ()))
        DeleteFileW (old_ini.c_str ());

      SK_RecursiveMove ( original_dir.c_str (),
                                 path.c_str (), false );
    }
  }

  return
    path;
}

std::wstring
SK_AppCache_Manager::getConfigPathForAppID (AppId64_t uiAppID) const
{
  static bool recursing = false;

  // If no AppCache (probably not a Steam game), or opting-out of central repo,
  //   then don't parse crap and just use the traditional path.
  if ( app_cache_db == nullptr || (! config.system.central_repository) )
    return SK_GetNaiveConfigPath ();

  std::wstring path = SK_GetNaiveConfigPath ();
  std::wstring name ( getAppNameFromID      (uiAppID) );

  // Non-trivial name = custom path, remove the old-style <program.exe>
  if (! name.empty ())
  {
    std::wstring original_dir (path);

    size_t pos = 0;
    if (  (pos = path.find (SK_GetHostApp (), pos)) != std::wstring::npos)
    {
      std::wstring       host_app (SK_GetHostApp ());
      path.replace (pos, host_app.length (), L"\0");
    }

    std::erase_if ( name,      [](wchar_t tval) {
      return invalid_file_chars.contains (tval);} );

    path =
      SK_FormatStringW ( LR"(%s\%s\)",
                         path.c_str (),
                         name.c_str () );

    SK_StripTrailingSlashesW (path.data ());

    if (recursing)
      return path;

    std::error_code                                  err;
    if (std::filesystem::is_directory (original_dir, err))
    {
      std::wstring old_ini =
                   dll_ini != nullptr ? dll_ini->get_filename ()
                                      : L"";

      recursing         = true;
      SK_GetConfigPathEx (true);
      SK_LoadConfigEx    (L"" );
      recursing         = false;

      // We've already parsed/written the new file, delete the old one
      if ((! old_ini.empty ())  &&
             dll_ini != nullptr &&
          _wcsicmp ( old_ini.c_str (), dll_ini->get_filename ()))
        DeleteFileW (old_ini.c_str ());

      SK_RecursiveMove ( original_dir.c_str (),
                                 path.c_str (), false );
    }
  }

  return
    path;
}

bool
SK_AppCache_Manager::saveAppCache (bool close)
{
  if (app_cache_db != nullptr)
  {
    storeDepotCache ();

    app_cache_db->write ();

    if (close)
    {
      delete
        std::exchange (app_cache_db, nullptr);
    }

    return true;
  }

  return false;
}


time_t
SK_AppCache_Manager::setFriendOwnership ( uint64_t                       friend_,
                                          SK_AppCache_Manager::Ownership owns   )
{
  if (app_cache_db == nullptr)
    return 0;

  auto& sec =
    app_cache_db->get_section (L"Friend.Ownership");

  static_assert ( sizeof (time_t) ==
                  sizeof (unsigned long long) );

  SK_AppCache_Manager::Ownership ownership    = SK_AppCache_Manager::Unknown;
  time_t                         last_updated = 0ULL;

  time_t update_time;
  time (&update_time);

  if (sec.contains_key (std::to_wstring (friend_).c_str ()))
  {
    std::wstring friend_status =
      sec.get_value (std::to_wstring (friend_).c_str ());

    if ( 2 ==
           std::swscanf ( friend_status.c_str (),
                            L"%li {%lli}",
                              &ownership, (int64_t *)&last_updated
                        )
       )
    {
      if (ownership != owns)
      {
        last_updated = update_time;

        sec.add_key_value ( std::to_wstring (friend_).c_str (),
                              SK_FormatStringW (
                                L"%li {%llu}",
                                  owns,
                                    last_updated
                              ).c_str ()
                          );
      }

      return last_updated;
    }
  }

  last_updated = update_time;

  sec.add_key_value ( std::to_wstring (friend_).c_str (),
                        SK_FormatStringW (
                          L"%li {%llu}",
                            owns,
                              last_updated
                        ).c_str ()
                    );

  return last_updated;
}

SK_AppCache_Manager::Ownership
SK_AppCache_Manager::getFriendOwnership ( uint64_t friend_,
                                          time_t*  updated )
{
  if (app_cache_db != nullptr)
  {
    auto& sec =
      app_cache_db->get_section (L"Friend.Ownership");

    SK_AppCache_Manager::Ownership ownership    = SK_AppCache_Manager::Unknown;
    time_t                         last_updated = 0ULL;

    static_assert ( sizeof (time_t) ==
                    sizeof (unsigned long long) );

    if (sec.contains_key (std::to_wstring (friend_).c_str ()))
    {
      std::wstring friend_status =
        sec.get_value    (std::to_wstring (friend_).c_str ());

      if ( 2 ==
             std::swscanf ( friend_status.c_str (),
                              L"%li {%lli}",
                                &ownership,
                                  &last_updated
                          )
         )
      {
        if (updated != nullptr)
           *updated  = last_updated;

        return
          SK_AppCache_Manager::Ownership (ownership);
      }
    }
  }

  if (updated != nullptr)
    *updated = 0;

  return
    SK_AppCache_Manager::Unknown;
}

time_t
SK_AppCache_Manager::setFriendAchievPct (uint64_t friend_, float new_percent)
{
  if (app_cache_db == nullptr)
    return 0;

  auto& sec =
    app_cache_db->get_section (L"Friend.Achievements");

  static_assert ( sizeof (time_t) ==
                  sizeof (unsigned long long) );

   float percent      = 0.0f;
  time_t last_updated = 0ULL;

  time_t update_time;
  time (&update_time);

  if (sec.contains_key (std::to_wstring (friend_).c_str ()))
  {
    std::wstring friend_status =
      sec.get_value    (std::to_wstring (friend_).c_str ());

    if ( 2 ==
           std::swscanf ( friend_status.c_str (),
                            L"%f {%lli}",
                              &percent, (int64_t *)&last_updated
                        )
       )
    {
      if (new_percent != percent)
      {
        last_updated = update_time;

        sec.add_key_value ( std::to_wstring (friend_).c_str (),
                              SK_FormatStringW (
                                L"%f {%lli}",
                                  new_percent,
                                    (int64_t)last_updated
                              ).c_str ()
                          );
      }

      return last_updated;
    }
  }

  last_updated = update_time;

  sec.add_key_value ( std::to_wstring (friend_).c_str (),
                        SK_FormatStringW (
                          L"%f {%lli}",
                            new_percent,
                              (int64_t)last_updated
                        ).c_str ()
                    );

  return last_updated;
}

float
SK_AppCache_Manager::getFriendAchievPct (uint64_t friend_, time_t* updated)
{
  if (app_cache_db != nullptr)
  {
    auto& sec =
      app_cache_db->get_section (L"Friend.Achievements");

    float  percent      = 0.0f;
    time_t last_updated = 0ULL;

    static_assert ( sizeof (time_t) ==
                    sizeof (unsigned long long) );

    time_t update_time;
    time (&update_time);

    if (sec.contains_key (std::to_wstring (friend_).c_str ()))
    {
      std::wstring friend_status =
        sec.get_value    (std::to_wstring (friend_).c_str ());

      if ( 2 ==
             std::swscanf ( friend_status.c_str (),
                              L"%f {%lli}",
                                &percent,
                                  &last_updated
                          )
         )
      {
        if (updated != nullptr)
           *updated  = last_updated;

        return
          percent;
      }
    }
  }

  if (updated != nullptr)
    time (updated);

  return
    0.0f;
}

void
SK_AppCache_Manager::setFriendPreference (FriendPreference set)
{
  if (app_cache_db == nullptr)
    return;

  auto& pref =
    app_cache_db->get_section (L"Friend.Achievements").
                    get_value (L"FetchFromServer");

  switch (set)
  {
    case Enable:
      pref = L"true";
      break;
    case Disable:
      pref = L"false";
      break;
    case Default:
    default:
      pref = L"UseGlobalPreference";
      break;
  }

  app_cache_db->write ();
}

bool
SK_AppCache_Manager::wantFriendStats (void)
{
  if (! config.platform.achievements.pull_friend_stats)
    return false;

  bool global_pref =
    config.platform.achievements.pull_friend_stats;

  if (app_cache_db == nullptr)
    return global_pref;

  auto& sec =
    app_cache_db->get_section (L"Friend.Achievements");

  if (sec.contains_key (L"FetchFromServer"))
  {
    const std::wstring& val =
      sec.get_value    (L"FetchFromServer");

    if (val._Equal (L"UseGlobalPreference"))
      return global_pref;

    return
      SK_IsTrue (val.c_str ());
  }

  sec.add_key_value ( L"FetchFromServer",
                        L"UseGlobalPreference" );

  return
    global_pref;
}

int
SK_AppCache_Manager::getLicenseRevision (void)
{
  bool must_show =
    config.imgui.show_eula;

  if (app_cache_db == nullptr)
    return must_show ? 0 : SK_LICENSE_REVISION;

  auto& sec =
    app_cache_db->get_section (L"License.Info");

  if (sec.contains_key (L"LastRevision"))
  {
    const std::wstring& val =
      sec.get_value    (L"LastRevision");

    return must_show ? 0 :
      _wtoi (val.c_str ());
  }

  return 0;
}

void
SK_AppCache_Manager::setLicenseRevision (int rev)
{
  if (app_cache_db == nullptr)
    return;

  auto& sec =
    app_cache_db->get_section (L"License.Info");

  sec.add_key_value ( L"LastRevision",
                       std::to_wstring (rev).c_str () );
}

int
SK_AppCache_Manager::migrateProfileData (LPVOID)
{
  // TODO
  return 0;
}


SK_API
SK_RenderAPI
__stdcall
SK_Render_GetAPIHookMask (void)
{
  if (config.apis.last_known != SK_RenderAPI::Reserved)
  {
    return static_cast <SK_RenderAPI> (config.apis.last_known);
  }

  int mask = 0;

#ifdef _M_IX86
  if (config.apis.d3d8.hook)       mask |= static_cast <int> (SK_RenderAPI::D3D8);
  if (config.apis.ddraw.hook)      mask |= static_cast <int> (SK_RenderAPI::DDraw);
#endif
  if (config.apis.d3d9.hook)       mask |= static_cast <int> (SK_RenderAPI::D3D9);
  if (config.apis.d3d9ex.hook)     mask |= static_cast <int> (SK_RenderAPI::D3D9Ex);
  if (config.apis.dxgi.d3d11.hook) mask |= static_cast <int> (SK_RenderAPI::D3D11);
  if (config.apis.dxgi.d3d12.hook) mask |= static_cast <int> (SK_RenderAPI::D3D12);
  if (config.apis.OpenGL.hook)     mask |= static_cast <int> (SK_RenderAPI::OpenGL);
#ifdef _M_AMD64
  if (config.apis.Vulkan.hook)     mask |= static_cast <int> (SK_RenderAPI::Vulkan);
#endif

  return
    static_cast <SK_RenderAPI> (mask);
}

sk_config_t config;

namespace sk
{
  namespace logs
  {
    int base_log_lvl = 0;
  };
};


void
sk_config_t::utility_functions_s::save_async (void)
{
  SK_RunOnce (
    SK_Thread_CreateEx ([](LPVOID) -> DWORD
    {
      HANDLE hEvents [] = {
        __SK_DLL_TeardownEvent,
        config.utility.hSignalAsyncSave
      };

      DWORD  dwWaitState  = WAIT_FAILED;
      while (dwWaitState != WAIT_OBJECT_0)
      {
        dwWaitState =
          WaitForMultipleObjectsEx (2, hEvents, FALSE, INFINITE, FALSE);

        if (dwWaitState == WAIT_OBJECT_0)
          break;

        if (dwWaitState == WAIT_OBJECT_0 + 1)
        {
          const wchar_t* config_name = SK_GetBackend ();

          if (SK_IsInjected ())
            config_name = L"SpecialK";

          SK_SaveConfig (config_name);

          // Throttle this stuff
          if ( WAIT_OBJECT_0 ==
                 WaitForSingleObject (__SK_DLL_TeardownEvent, 150UL) )
            break;
        }
      }

      SK_Thread_CloseSelf ();

      return 0;
    }, L"[SK] Config Save Thread");
  );

  SetEvent (config.utility.hSignalAsyncSave);
}

iSK_INI* SK_GetNotifyINI (void)
{
  return notify_ini;
}



SK_LazyGlobal <sk_config_t::file_trace_s::ignore_files_s> sk_config_t::file_trace_s::ignore_reads;
SK_LazyGlobal <sk_config_t::file_trace_s::ignore_files_s> sk_config_t::file_trace_s::ignore_writes;