#pragma once

#include <Windows.h>
#include <string>
using namespace std;

uintptr_t ScanSignature(HANDLE hProcess, const char* moduleName, const string& signature);

template <typename T>
T get_struct_value(uintptr_t base, int offset) {
    return *reinterpret_cast<T*>(reinterpret_cast<char*>(base) + offset);
}

struct SignatureTable {
    const char* DispatchDamage;
    const char* InternalExit;
    const char* InternalInitialize;
    const char* CreatePlayer;
    uintptr_t DispatchDamagePointer;
    uintptr_t InternalExitPointer;
    uintptr_t InternalInitializePointer;
    uintptr_t CreatePlayerPointer;
};

struct MemoryPattern {
    BYTE byte;
    bool wildcard;
};