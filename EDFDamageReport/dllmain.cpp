#pragma once

#include <fstream>
#include <Windows.h>
#include <cstdio>
#include <cstdint>
#include <exception>
#include <atomic>
#include <rttidata.h>
#include <deque>
#include <chrono>
#include <mutex>
#include <unordered_map>
#include <string>
#include <tuple>
#include <cstddef>
#include <iostream>
#include <vector>
#include <algorithm>
#include <codecvt>
#include <sstream>

#include "MinHook.h"
#include "Memory.h"
#include "Utils.h"
#include "Player.h"
#include "ImImporter.h"
#include "PluginAPI.h"

extern "C" {
    BOOL __declspec(dllexport) EML4_Load(PluginInfo* pluginInfo) {
        return false;
    }

    BOOL __declspec(dllexport) EML5_Load(PluginInfo* pluginInfo) {
        return false;
    }

    BOOL __declspec(dllexport) EML6_Load(PluginInfo* pluginInfo) {
        pluginInfo->infoVersion = PluginInfo::MaxInfoVer;
        pluginInfo->name = "EDF6 Damage Summary";
        pluginInfo->version = PLUG_VER(1, 0, 0, 0);
        return true;
    }
}

typedef __int64 (__fastcall* DispatchDamage_Type)(__int64 damageTarget, __int64 damageMessage);
typedef __int64(__fastcall* InternalExit_Type)(__int64 a1, MissionResult missionResult);
typedef __int64(__fastcall* InternalInitialize_Type)(__int64 a1, int a2);
typedef void(__fastcall* CreatePlayer_Type)(__int64 a1, void* a2, unsigned int a3);

uintptr_t baseAddress = 0;
uintptr_t sceneObjectTypeDescriptor = 0;
uintptr_t soldierBaseTypeDescriptor = 0;

DispatchDamage_Type OriginalDispatchDamage = nullptr;
InternalExit_Type OriginalInternalExit = nullptr;
InternalInitialize_Type OriginalInternalInitialize = nullptr;
CreatePlayer_Type OriginalCreatePlayer = nullptr;

SignatureTable g_SignatureTable;
std::vector<uintptr_t> g_playerPointers;
PlayerList g_playerList;
RecentAttackLogger g_attackLogger;
DamageSummary g_damageSummary(&g_playerList, &g_attackLogger);
bool g_enableDebugOutput = false;

ENUM_MAP(MissionResult)

#define DEBUG_OUTPUT(msg) do { if (g_enableDebugOutput) OutputMessage(msg); } while(0)

uintptr_t GetPlayer(int playerID)
{
    try
    {
        char logBuffer[256];

        uintptr_t MissionMgr = *(uintptr_t*)(baseAddress + 0x20b28a8);
        if (!MissionMgr) {
            snprintf(logBuffer, sizeof(logBuffer), "[HOOK] Error: MissionMgr is null");
            OutputMessage(logBuffer);
            return 0;
        }

        uintptr_t MissionScriptASImplement = get_struct_value<uintptr_t>(MissionMgr, 0x40);
        if (!MissionScriptASImplement) {
            snprintf(logBuffer, sizeof(logBuffer), "[HOOK] Error: MissionScriptASImplement is null (MissionMgr=0x%llX)",
                (unsigned long long)MissionMgr);
            OutputMessage(logBuffer);
            return 0;
        }

        uintptr_t MissionContext = get_struct_value<uintptr_t>(MissionScriptASImplement, 0xE8);
        if (!MissionContext) {
            snprintf(logBuffer, sizeof(logBuffer), "[HOOK] Error: MissionContext is null (ASImplement=0x%llX)",
                (unsigned long long)MissionScriptASImplement);
            OutputMessage(logBuffer);
            return 0;
        }

        const int BASE_OFFSET = 0x10 * 0x10;
        const int PLAYER_STRUCT_SIZE = 0x10;

        uintptr_t playerPtr = MissionContext + BASE_OFFSET + (playerID * PLAYER_STRUCT_SIZE);
        uintptr_t playerPointer = *(uintptr_t*)playerPtr;

        DebugOutputFormat("[HOOK] Debug Info: Mgr=0x%llX, ASImp=0x%llX, Ctx=0x%llX, Player[%d]=0x%llX",
            (unsigned long long)MissionMgr,
            (unsigned long long)MissionScriptASImplement,
            (unsigned long long)MissionContext,
            playerID,
            (unsigned long long)playerPointer
        );

        return playerPointer;
    }
    catch (...)
    {
        OutputMessage("[HOOK] Fatal: Exception in GetPlayer");
        return 0;
    }
}

void GetAllPlayers()
{
    g_playerPointers.clear();
    g_playerList.resetPlayers();

    for (int i = 0; i <= 3; i++)
    {
        uintptr_t playerPointer = GetPlayer(i);
        if (playerPointer)
        {
            bool exists = std::find(g_playerPointers.begin(), g_playerPointers.end(), playerPointer) != g_playerPointers.end();

            if (!exists)
            {
                g_playerPointers.push_back(playerPointer);
                g_playerList.addOrUpdatePlayer(i, "INVALID", playerPointer);
            }
        }
    }
}

__int64 __fastcall Hooked_InternalInitialize(__int64 a1, int a2)
{
    OutputMessage("[HOOK] New mission has started.");

    g_damageSummary.resetAll();
    g_playerList.resetPlayers();

    return OriginalInternalInitialize(a1, a2);
}

void __fastcall Hooked_CreatePlayer(__int64 a1, void* a2, unsigned int a3)
{
    OriginalCreatePlayer(a1, a2, a3);
    GetAllPlayers();
}

__int64 __fastcall Hooked_InternalExit(__int64 a1, MissionResult missionResult)
{
    DebugOutputFormat("[HOOK] Game has ended with result: %s", MissionResultToString(missionResult));
    OutputMessage(g_damageSummary.formatAllDamagesSummary().c_str());

    return OriginalInternalExit(a1, missionResult);
}

__int64 __fastcall Hooked_DispatchDamage(__int64 damageTarget, __int64 damageMessage)
{
    // Safety protocol
    if (sceneObjectTypeDescriptor == 0 || soldierBaseTypeDescriptor == 0)
    {
        OutputMessage("[HOOK] FATAL ERROR: sceneObjectTypeDescriptor or soldierBaseTypeDescriptor is null. The offset mismatch.");
        return OriginalDispatchDamage(damageTarget, damageMessage);
    }

    void* damageDealerPointer = get_struct_value<void*>(damageMessage, 0x10);
    float damageValue = get_struct_value<float>(damageMessage, 0x50);
    std::string targetName = AttemptReadRTTI((uintptr_t)damageTarget);

    // Damage Event does not triggered by SoldierBaseType.
    uintptr_t cast = (uintptr_t)__RTDynamicCast((void*)damageDealerPointer, 0, (void*)sceneObjectTypeDescriptor, (void*)soldierBaseTypeDescriptor, false);
    if (!cast)
    {
        return OriginalDispatchDamage(damageTarget, damageMessage);
    }

    // Damage Event does not triggered by Player.
    bool exists = std::find(g_playerPointers.begin(), g_playerPointers.end(), reinterpret_cast<uintptr_t>(damageDealerPointer)) != g_playerPointers.end();
    if (!exists)
    {
        return OriginalDispatchDamage(damageTarget, damageMessage);
    }

    // Is Friendly Damage.
    bool isFriendly = std::find(g_playerPointers.begin(), g_playerPointers.end(), damageTarget) != g_playerPointers.end();
    if (isFriendly)
    {
        g_damageSummary.addFriendlyDamage(reinterpret_cast<uintptr_t>(damageDealerPointer), damageTarget, damageValue);
	}

    DebugOutputFormat("[HOOK] Target Pointer: 0x%p | Damage Message Pointer: 0x%p | Damage Dealer Pointer: 0x%p | Damage Value: %f | Target Name: %s",
        reinterpret_cast<void*>(damageTarget),
        reinterpret_cast<void*>(damageMessage),
        damageDealerPointer,
        damageValue,
        targetName.c_str()
    );

    g_damageSummary.addDamage((uintptr_t)damageDealerPointer, damageTarget, damageValue);

    return OriginalDispatchDamage(damageTarget, damageMessage);
}

void SwitchHooks(BOOL disable)
{
    HMODULE hModule = GetModuleHandleW(L"EDF.dll");
    if (!hModule) {
        OutputMessage("[HOOK] GetModuleHandle failed");
        return;
    }

    baseAddress = (uintptr_t)hModule;
    sceneObjectTypeDescriptor = baseAddress + 0x2006450;
    soldierBaseTypeDescriptor = baseAddress + 0x2006428;

	ASSERT(sceneObjectTypeDescriptor != 0);
    ASSERT(soldierBaseTypeDescriptor != 0);
    ASSERT(g_SignatureTable.DispatchDamagePointer != 0);
    ASSERT(g_SignatureTable.InternalExitPointer != 0);
    ASSERT(g_SignatureTable.InternalInitializePointer != 0);
    ASSERT(g_SignatureTable.CreatePlayerPointer != 0);

	int hookCount = 4;
    HookEntry hooks[] = {
        {"DispatchDamage", g_SignatureTable.DispatchDamagePointer, &Hooked_DispatchDamage, &OriginalDispatchDamage},
        {"Internal_Exit", g_SignatureTable.InternalExitPointer, &Hooked_InternalExit, &OriginalInternalExit},
        {"Internal_Initialize", g_SignatureTable.InternalInitializePointer, &Hooked_InternalInitialize, &OriginalInternalInitialize},
        {"CreatePlayer", g_SignatureTable.CreatePlayerPointer, &Hooked_CreatePlayer, &OriginalCreatePlayer},
    };

    MH_STATUS status = MH_UNKNOWN;

    char logBuffer[512];

    if (!disable)
    {
        char* pos = logBuffer;
        int remain = sizeof(logBuffer);

        for (int i = 0; i < hookCount && remain > 0; i++) {
            int written = snprintf(pos, remain,
                "[HOOK] %s: Target=0x%p Hook=0x%p | ",
                hooks[i].name,
                reinterpret_cast<void*>(hooks[i].targetAddr),
                hooks[i].hookFunc);

            if (written > 0) {
                pos += written;
                remain -= written;
            }
        }
        OutputMessage(logBuffer);

        MEMORY_BASIC_INFORMATION mbi;
        if (!VirtualQuery(reinterpret_cast<LPCVOID>(hooks[0].targetAddr), &mbi, sizeof(mbi))) {
            OutputMessage("[HOOK] VirtualQuery failed");
            return;
        }

        if (!(mbi.Protect & (PAGE_EXECUTE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE))) {
            OutputMessage("[HOOK] Target address not executable");
            return;
        }

        status = MH_Initialize();
        if (status != MH_OK) {
            snprintf(logBuffer, sizeof(logBuffer),
                "[HOOK] MH_Initialize failed: %s", MH_StatusToString(status));
            OutputMessage(logBuffer);
            return;
        }

        for (int i = 0; i < hookCount; i++) {
            status = MH_CreateHook(
                (LPVOID)(hooks[i].targetAddr),
                hooks[i].hookFunc,
                reinterpret_cast<LPVOID*>(hooks[i].origFuncPtr)
            );

            if (status != MH_OK) {
                snprintf(logBuffer, sizeof(logBuffer),
                    "[HOOK] CreateHook (%s) failed: %s", hooks[i].name, MH_StatusToString(status));
                OutputMessage(logBuffer);
                return;
            }
        }

        for (int i = 0; i < hookCount; i++) {
            status = MH_EnableHook((LPVOID)hooks[i].targetAddr);

            if (status != MH_OK) {
                snprintf(logBuffer, sizeof(logBuffer),
                    "[HOOK] EnableHook (%s) failed: %s", hooks[i].name, MH_StatusToString(status));
                OutputMessage(logBuffer);
                return;
            }
        }

        OutputMessage("[HOOK] All hooks enabled");
    }
    else
    {
        for (int i = 0; i < hookCount; i++) {
            status = MH_DisableHook((LPVOID)hooks[i].targetAddr);

            if (status != MH_OK) {
                snprintf(logBuffer, sizeof(logBuffer),
                    "[HOOK] DisableHook (%s) failed: %s", hooks[i].name, MH_StatusToString(status));
                OutputMessage(logBuffer);
                return;
            }
        }

        OutputMessage("[HOOK] All hooks disabled");
    }
}

void PreInit()
{
    g_SignatureTable = {
        "48 8B C4 48 89 58 ?? 55 56 57 41 54 41 55 41 56 41 57 48 8D A8 ?? ?? ?? ?? 48 81 EC 90 01 00 00 0F 29 70 ?? 0F 29 78 ?? 44 0F 29 40 ?? 44 0F 29 48 ?? 48 8B 05 ?? ?? ?? ?? 48 33 C4 48 89 45 40 4C 8B EA 48 8B F9 F6 41 ?? 04 0F 85 ?? ?? ?? ?? C6 81 ?? ?? ?? ?? 00 48 8B 59 ?? 33 D2 4D 8D 65 ?? 4D 8D 4D ?? 48 85 DB 74 2C 4C 8B 51 ?? F0 FF 43 ?? ", // DispatchDamage
        "48 89 5C 24 08 57 48 83 EC 20 48 8B D9 8B FA 48 8D 0D ?? ?? ?? ?? E8 ?? ?? ?? ?? 48 8B 0D ?? ?? ?? ?? 33 D2 E8 ?? ?? ?? ?? 48 8B 83 ?? ?? ?? ?? 33 D2 C7 80 ?? ?? ?? ?? 03 00 00 00 48 8B 83 ?? ?? ?? ?? 89 B8 ?? ?? ?? ?? 48 8B 0D ?? ?? ?? ?? E8 ?? ?? ?? ?? ", // Internal_Exit
        "48 8B 49 08 FF C2 E9", // Internal_Initialize
        "48 89 5C 24 20 55 56 57 41 56 41 57 48 8D 6C 24 E0 48 81 EC 20 01 00 00 48 8B 05 ?? ?? ?? ?? 48 33 C4 48 89 45 10 45 8B F0 48 8B F9", // CreatePlayer
		"55 56 57 41 54 41 55 41 56 41 57 48 8D A8 ?? ?? ?? ?? 48 81 EC 20 02 00 00 0F 29 70 ?? 0F 29 78 ?? 4C 8B EA 4C 8B F1 80 B9 ?? ?? ?? ?? 00 74 15 C6 81 ?? ?? ?? ?? 00 80 B9 ?? ?? ?? ?? 00 74 2A", // UpdateRadar
    };

    g_SignatureTable.DispatchDamagePointer = ScanSignature(GetCurrentProcess(), "EDF.dll", g_SignatureTable.DispatchDamage);
    g_SignatureTable.InternalExitPointer = ScanSignature(GetCurrentProcess(), "EDF.dll", g_SignatureTable.InternalExit);
    g_SignatureTable.InternalInitializePointer = ScanSignature(GetCurrentProcess(), "EDF.dll", g_SignatureTable.InternalInitialize);
    g_SignatureTable.CreatePlayerPointer = ScanSignature(GetCurrentProcess(), "EDF.dll", g_SignatureTable.CreatePlayer);
	g_SignatureTable.UpdateRadarPointer = ScanSignature(GetCurrentProcess(), "EDF.dll", g_SignatureTable.UpdateRadar);

    char buf[256];
    snprintf(buf, 256, "[HOOK] Pointer based on signatures: 0x%p 0x%p 0x%p 0x%p",
        g_SignatureTable.DispatchDamagePointer,
        g_SignatureTable.InternalExitPointer,
        g_SignatureTable.InternalInitializePointer,
        g_SignatureTable.CreatePlayerPointer,
		g_SignatureTable.UpdateRadarPointer
    );
    OutputMessage(buf);
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    switch (ul_reason_for_call) {
        case DLL_PROCESS_ATTACH: {
            DisableThreadLibraryCalls(hModule);
            ClearLogFile();
            OutputMessage("[HOOK] DLL_PROCESS_ATTACH");
            PreInit();
            SwitchHooks(false);
            CreateThread(NULL, NULL, (LPTHREAD_START_ROUTINE)Start, NULL, NULL, NULL);
            break;
        }
        case DLL_PROCESS_DETACH:
            OutputMessage("[HOOK] DLL_PROCESS_DETACH");
            SwitchHooks(true);
			Exit();
            MH_Uninitialize();
            break;
    }
    return TRUE;
}