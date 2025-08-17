#pragma once

#include "pch.h"
#include <fstream>
#include <Windows.h>
#include <MinHook.h>
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

#include "Memory.h"
#include "Utils.h"
#include "Player.h"

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
DamageSummary g_damageSummary(&g_playerList);
bool g_enableDebugOutput = false;
bool g_hasGameEnded = false;
bool g_hasGameStarted = false;

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

        snprintf(logBuffer, sizeof(logBuffer),
            "[HOOK] Debug Info: Mgr=0x%llX, ASImp=0x%llX, Ctx=0x%llX, Player[%d]=0x%llX",
            (unsigned long long)MissionMgr,
            (unsigned long long)MissionScriptASImplement,
            (unsigned long long)MissionContext,
            playerID,
            (unsigned long long)playerPointer);
        OutputMessage(logBuffer);

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

// From Discord @KittopiaCreator. Much thanks.
std::string AttemptReadRTTI(uintptr_t pointer) {
    const uintptr_t currentFunctionAddr = reinterpret_cast<uintptr_t>(&AttemptReadRTTI);
    char debugBuffer[512];

    snprintf(debugBuffer, sizeof(debugBuffer),
        "[RTTI DEBUG] Function address: 0x%p, Input pointer: 0x%p",
        reinterpret_cast<void*>(currentFunctionAddr),
        reinterpret_cast<void*>(pointer));
    DEBUG_OUTPUT(debugBuffer);

    try {
        if (pointer == 0) {
            DEBUG_OUTPUT("[RTTI] Input pointer is NULL");
            return "NULL";
        }

        uintptr_t ptr_obj_locator = *(uintptr_t*)pointer - 8;
        snprintf(debugBuffer, sizeof(debugBuffer),
            "[RTTI DEBUG] RTTI Locator: 0x%p",
            reinterpret_cast<void*>(ptr_obj_locator));
        DEBUG_OUTPUT(debugBuffer);

        if (ptr_obj_locator == 0) {
            DEBUG_OUTPUT("[RTTI] RTTI Locator is NULL");
            return "NULL";
        }

        uintptr_t rtti_base = *(uintptr_t*)ptr_obj_locator;
        snprintf(debugBuffer, sizeof(debugBuffer),
            "[RTTI DEBUG] RTTI Base Pointer: 0x%p",
            reinterpret_cast<void*>(rtti_base));
        DEBUG_OUTPUT(debugBuffer);

        unsigned int type_descriptor_offset = *(unsigned int*)(rtti_base + 0xC);
        snprintf(debugBuffer, sizeof(debugBuffer),
            "[RTTI DEBUG] Type Descriptor Offset: 0x%X",
            type_descriptor_offset);
        DEBUG_OUTPUT(debugBuffer);

        uintptr_t base_offset = *(uintptr_t*)(rtti_base + 0x14);
        snprintf(debugBuffer, sizeof(debugBuffer),
            "[RTTI DEBUG] Base Offset: 0x%llX",
            static_cast<unsigned long long>(base_offset));
        DEBUG_OUTPUT(debugBuffer);

        uintptr_t* class_name_ptr = (uintptr_t*)(rtti_base - base_offset + type_descriptor_offset + 0x10 + 0x04);
        snprintf(debugBuffer, sizeof(debugBuffer),
            "[RTTI DEBUG] Class Name Pointer Calc: "
            "rtti_base(0x%p) - base_offset(0x%llX) + td_offset(0x%X) + 0x10 + 0x04 = 0x%p",
            reinterpret_cast<void*>(rtti_base),
            static_cast<unsigned long long>(base_offset),
            type_descriptor_offset,
            reinterpret_cast<void*>(class_name_ptr));
        DEBUG_OUTPUT(debugBuffer);

        if (class_name_ptr == nullptr || *class_name_ptr == 0) {
            snprintf(debugBuffer, sizeof(debugBuffer),
                "[RTTI] Class name pointer is invalid: 0x%p",
                reinterpret_cast<void*>(class_name_ptr));
            DEBUG_OUTPUT(debugBuffer);
            return "NULL";
        }

        char* str = (char*)class_name_ptr;
        std::string rawName = str;
        snprintf(debugBuffer, sizeof(debugBuffer),
            "[RTTI DEBUG] Raw class name: %s",
            rawName.c_str());
        DEBUG_OUTPUT(debugBuffer);

        size_t atPos = rawName.find("@");
        std::string prettifiedName = (atPos != std::string::npos)
            ? rawName.substr(0, atPos)
            : rawName;

        for (size_t i = 0; i < prettifiedName.size(); ++i) {
            if (static_cast<unsigned char>(prettifiedName[i]) < 0x20 ||
                static_cast<unsigned char>(prettifiedName[i]) > 0x7E) {
                prettifiedName = prettifiedName.substr(0, i);
                break;
            }
        }

        snprintf(debugBuffer, sizeof(debugBuffer),
            "[RTTI] Final class name: %s",
            prettifiedName.c_str());
        DEBUG_OUTPUT(debugBuffer);

        return prettifiedName;
    }
    catch (...) {
        DEBUG_OUTPUT("[RTTI ERROR] Exception caught during processing");
        return "EXCEPTION";
    }
}

__int64 __fastcall Hooked_InternalInitialize(__int64 a1, int a2)
{
    char logBuffer[50];
    snprintf(logBuffer, sizeof(logBuffer), "[HOOK] New mission has started.");
    OutputMessage(logBuffer);

    g_hasGameStarted = true;
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
    char logBuffer[100];

    snprintf(logBuffer, sizeof(logBuffer), "[HOOK] Game has ended with result: %s", MissionResultToString(missionResult));
    OutputMessage(logBuffer);
    return OriginalInternalExit(a1, missionResult);
}

__int64 __fastcall Hooked_DispatchDamage(__int64 damageTarget, __int64 damageMessage)
{
    // Safety protocol
    if (sceneObjectTypeDescriptor == 0 || soldierBaseTypeDescriptor == 0)
    {
        OutputMessage("sceneObjectTypeDescriptor or soldierBaseTypeDescriptor is null. The offset mismatch.");
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

    char logBuffer[1024];
    snprintf(logBuffer, sizeof(logBuffer),
        "[HOOK] Target Pointer: 0x%p | Damage Message Pointer: 0x%p | Damage Dealer Pointer: 0x%p | Damage Value: %f | Target Name: %s",
        reinterpret_cast<void*>(damageTarget),
        reinterpret_cast<void*>(damageMessage),
        damageDealerPointer,
        damageValue,
        targetName.c_str()
    );

    OutputMessage(logBuffer);

    g_damageSummary.addDamage((uintptr_t)damageDealerPointer, damageValue);
    OutputMessage(g_damageSummary.formatAllDamages().c_str());

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
        "48 8B C4 48 89 58 18 55 56 57 41 54 41 55 41 56 41 57 48 8D A8 38 FF FF FF 48 81 EC 90 01 00 00 0F 29 70 B8 0F 29 78 A8 44 0F 29 40 98 44 0F 29 48 88 48 8B 05 EF 73 AA 01 48 33 C4 48 89 45 40 4C 8B EA 48 8B F9 F6 41 18 04 0F 85 A5 09 00 00 C6 81 E9 02 00 00 00 48 8B 59 30 33 D2 4D 8D 65 10 4D 8D 4D 10 48 85 DB 74 2C 4C 8B 51 28 F0 FF 43 0C", // DispatchDamage
        "48 89 5C 24 08 57 48 83 EC 20 48 8B D9 8B FA 48 8D 0D 4A 91 F5 01 E8 75 ED F3 00 48 8B 0D 16 4C ED 01 33 D2 E8 D7 34 5A 00 48 8B 83 E8 00 00 00 33 D2 C7 80 B8 02 00 00 03 00 00 00 48 8B 83 E8 00 00 00 89 B8 B4 02 00 00 48 8B 0D 70 4A ED 01 E8 0B 51 5D 00", // Internal_Exit
        "48 8B 49 08 FF C2 E9", // Internal_Initialize
        "48 89 5C 24 20 55 56 57 41 56 41 57 48 8D 6C 24 E0 48 81 EC 20 01 00 00 48 8B 05 ?? ?? ?? ?? 48 33 C4 48 89 45 10 45 8B F0 48 8B F9", // CreatePlayer
    };

    g_SignatureTable.DispatchDamagePointer = ScanSignature(GetCurrentProcess(), "EDF.dll", g_SignatureTable.DispatchDamage);
    g_SignatureTable.InternalExitPointer = ScanSignature(GetCurrentProcess(), "EDF.dll", g_SignatureTable.InternalExit);
    g_SignatureTable.InternalInitializePointer = ScanSignature(GetCurrentProcess(), "EDF.dll", g_SignatureTable.InternalInitialize);
    g_SignatureTable.CreatePlayerPointer = ScanSignature(GetCurrentProcess(), "EDF.dll", g_SignatureTable.CreatePlayer);

    char buf[256];
    snprintf(buf, 256, "[HOOK] Pointer based on signatures: 0x%p 0x%p 0x%p 0x%p",
        g_SignatureTable.DispatchDamagePointer,
        g_SignatureTable.InternalExitPointer,
        g_SignatureTable.InternalInitializePointer,
        g_SignatureTable.CreatePlayerPointer
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
            break;
        }
        case DLL_PROCESS_DETACH:
            OutputMessage("[HOOK] DLL_PROCESS_DETACH");
            SwitchHooks(true);
            MH_Uninitialize();
            break;
    }
    return TRUE;
}