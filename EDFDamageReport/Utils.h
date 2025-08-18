#pragma once

#include <string>
#include <cstdint>
#include <cassert>
#include <windows.h>
#include <xmmintrin.h>

void ClearLogFile();
void OutputMessage(const char* message);
void DebugOutputFormat(const char* fmt, ...);
std::string AttemptReadRTTI(uintptr_t pointer);

extern const char* g_LogFilePath;

struct HookEntry
{
    const char* name;
    uintptr_t targetAddr;
    void* hookFunc;
    void* origFuncPtr;
}; 

#define ASSERT(expr) do { \
    if (!(expr)) { \
        OutputMessage( \
            (std::string("[ASSERT FAIL] ") + #expr + \
            " at " + __FILE__ + ":" + std::to_string(__LINE__)).c_str() \
        ); \
        assert(expr); \
    } \
} while(0)

#define ENUM_MAP(ENUM_TYPE) \
    const char* ENUM_TYPE##ToString(ENUM_TYPE value) { \
        switch (value) { \
            MAP_CASE(ENUM_TYPE, MISSION_RESULT_UNKNOWN); \
            MAP_CASE(ENUM_TYPE, MISSION_RESULT_CLEAR); \
            MAP_CASE(ENUM_TYPE, MISSION_RESULT_ABORT); \
            MAP_CASE(ENUM_TYPE, MISSION_RESULT_RETRY); \
            MAP_CASE(ENUM_TYPE, MISSION_RESULT_LOBBY); \
            MAP_CASE(ENUM_TYPE, MISSION_RESULT_RETURNTITLE); \
            MAP_CASE(ENUM_TYPE, MISSION_RESULT_MAX); \
            default: return "INVALID_" #ENUM_TYPE; \
        } \
    }

#define MAP_CASE(ENUM_TYPE, VALUE) case VALUE: return #VALUE;

enum MissionResult {
	MISSION_RESULT_UNKNOWN,
	MISSION_RESULT_CLEAR,
	MISSION_RESULT_ABORT,
	MISSION_RESULT_RETRY,
	MISSION_RESULT_LOBBY,
	MISSION_RESULT_RETURNTITLE,
	MISSION_RESULT_MAX,
};