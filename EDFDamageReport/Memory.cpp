#include "pch.h"
#include <Windows.h>
#include <vector>
#include <string>
#include <Psapi.h>
#include <memory>
#include <cctype>
#include "Memory.h"
using namespace std;

#pragma comment(lib, "psapi.lib")

vector<MemoryPattern> HexToPattern(const string& hex) {
    vector<MemoryPattern> pattern;
    string buffer;

    for (size_t i = 0; i < hex.size(); ++i) {
        char c = hex[i];

        if (isspace(c)) {
            if (!buffer.empty()) {
                if (buffer == "??") {
                    pattern.push_back({ 0, true });
                }
                else {
                    BYTE byte = static_cast<BYTE>(strtoul(buffer.c_str(), nullptr, 16));
                    pattern.push_back({ byte, false });
                }
                buffer.clear();
            }
        }
        else {
            buffer += c;
        }
    }

    if (!buffer.empty()) {
        if (buffer == "??") {
            pattern.push_back({ 0, true });
        }
        else {
            BYTE byte = static_cast<BYTE>(strtoul(buffer.c_str(), nullptr, 16));
            pattern.push_back({ byte, false });
        }
    }

    return pattern;
}

DWORD_PTR FindPattern(HANDLE hProcess, DWORD_PTR baseAddr, size_t size, const vector<MemoryPattern>& pattern) {
    if (pattern.empty() || size < pattern.size()) {
        return 0;
    }

    BYTE* buffer = new BYTE[size];

    SIZE_T bytesRead;
    if (!ReadProcessMemory(hProcess, reinterpret_cast<LPCVOID>(baseAddr), buffer, size, &bytesRead)) {
        delete[] buffer;
        return 0;
    }

    const size_t scanEnd = min(static_cast<size_t>(bytesRead), size - pattern.size());

    for (size_t i = 0; i <= scanEnd; ++i) {
        bool match = true;
        for (size_t j = 0; j < pattern.size(); ++j) {
            if (!pattern[j].wildcard && buffer[i + j] != pattern[j].byte) {
                match = false;
                break;
            }
        }
        if (match) {
            DWORD_PTR result = baseAddr + i;
            delete[] buffer;
            return result;
        }
    }

    delete[] buffer;
    return 0;
}

MODULEINFO GetModuleInfo(HANDLE hProcess, const char* moduleName) {
    HMODULE hModule = GetModuleHandleA(moduleName);
    MODULEINFO modInfo = { 0 };

    if (hModule) {
        GetModuleInformation(hProcess, hModule, &modInfo, sizeof(MODULEINFO));
    }

    return modInfo;
}

uintptr_t ScanSignature(HANDLE hProcess, const char* moduleName, const string& signature) {
    MODULEINFO modInfo = GetModuleInfo(hProcess, moduleName);
    if (modInfo.SizeOfImage == 0)
    {
        return 0;
    }

    vector<MemoryPattern> pattern = HexToPattern(signature);

    return FindPattern(hProcess, (uintptr_t)modInfo.lpBaseOfDll, modInfo.SizeOfImage, pattern);
}