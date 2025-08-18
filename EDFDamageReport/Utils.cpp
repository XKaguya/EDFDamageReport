#pragma once

#include <Windows.h>
#include <exception>
#include <cstring>
#include <cmath>
#include <xmmintrin.h>
#include <fstream>
#include "Utils.h"
#include <mutex>
#include <cstdarg>
#include <cstdio>

static std::mutex g_OutputMutex;
static const char* g_LogFilePath = "EDFDamageReport.log";
#define DEBUG_OUTPUT(msg) do { if (false) OutputMessage(msg); } while(0)

void OutputMessage(const char* message) {
    std::lock_guard<std::mutex> lock(g_OutputMutex);
    OutputDebugStringA(message);

    std::ifstream checkFile(g_LogFilePath, std::ios::binary | std::ios::ate);
    if (checkFile.is_open()) {
        std::streamsize size = checkFile.tellg();
        checkFile.close();
        if (size >= 1048576) {
            std::ofstream clearFile(g_LogFilePath, std::ios::trunc | std::ios::out);
        }
    }

    std::ofstream logFile(g_LogFilePath, std::ios::app | std::ios::out);
    if (logFile.is_open()) {
        logFile << message << std::endl;
    }
}

void DebugOutputFormat(const char* fmt, ...)
{
    extern bool g_enableDebugOutput;
    if (!g_enableDebugOutput) return;
    char buf[512];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    OutputMessage(buf);
}

void ClearLogFile()
{
    std::lock_guard<std::mutex> lock(g_OutputMutex);
    std::ofstream logFile(g_LogFilePath, std::ios::trunc | std::ios::out);
}

// From Discord @KittopiaCreator. Much thanks.
std::string AttemptReadRTTI(uintptr_t pointer) {
    DebugOutputFormat("[RTTI DEBUG] Input pointer: 0x%p",
        reinterpret_cast<void*>(pointer)
    );

    try {
        if (pointer == 0) {
            DEBUG_OUTPUT("[RTTI] Input pointer is NULL");
            return "NULL";
        }

        uintptr_t ptr_obj_locator = *(uintptr_t*)pointer - 8;

        DebugOutputFormat("[RTTI DEBUG] RTTI Locator: 0x%p",
            reinterpret_cast<void*>(ptr_obj_locator)
        );

        if (ptr_obj_locator == 0) {
            DEBUG_OUTPUT("[RTTI] RTTI Locator is NULL");
            return "NULL";
        }

        uintptr_t rtti_base = *(uintptr_t*)ptr_obj_locator;
        DebugOutputFormat("[RTTI DEBUG] RTTI Base Pointer: 0x%p",
            reinterpret_cast<void*>(rtti_base)
        );

        unsigned int type_descriptor_offset = *(unsigned int*)(rtti_base + 0xC);
        DebugOutputFormat("[RTTI DEBUG] Type Descriptor Offset: 0x%X",
            type_descriptor_offset
        );

        uintptr_t base_offset = *(uintptr_t*)(rtti_base + 0x14);
        DebugOutputFormat("[RTTI DEBUG] Base Offset: 0x%llX",
            static_cast<unsigned long long>(base_offset)
        );

        uintptr_t* class_name_ptr = (uintptr_t*)(rtti_base - base_offset + type_descriptor_offset + 0x10 + 0x04);
        DebugOutputFormat("[RTTI DEBUG] Class Name Pointer Calc: "
            "rtti_base(0x%p) - base_offset(0x%llX) + td_offset(0x%X) + 0x10 + 0x04 = 0x%p",
            reinterpret_cast<void*>(rtti_base),
            static_cast<unsigned long long>(base_offset),
            type_descriptor_offset,
            reinterpret_cast<void*>(class_name_ptr)
        );


        if (class_name_ptr == nullptr || *class_name_ptr == 0) {
            DebugOutputFormat(
                "[RTTI] Class name pointer is invalid: 0x%p",
                reinterpret_cast<void*>(class_name_ptr)
            );
            return "NULL";
        }

        char* str = (char*)class_name_ptr;
        std::string rawName = str;
        DebugOutputFormat(
            "[RTTI DEBUG] Raw class name: %s",
            rawName.c_str()
        );

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

        DebugOutputFormat(
            "[RTTI] Final class name: %s",
            prettifiedName.c_str()
        );

        return prettifiedName;
    }
    catch (...) {
        DEBUG_OUTPUT("[RTTI ERROR] Exception caught during processing");
        return "EXCEPTION";
    }
}