#pragma once

#include "pch.h"
#include <Windows.h>
#include <exception>
#include <cstring>
#include <cmath>
#include <xmmintrin.h>
#include <fstream>
#include "Utils.h"
#include <mutex>

static std::mutex g_OutputMutex;

static const char* g_LogFilePath = "EDFDamageReport.log";

void OutputMessage(const char* message) {
    std::lock_guard<std::mutex> lock(g_OutputMutex);
    OutputDebugStringA(message);

    std::ofstream logFile(g_LogFilePath, std::ios::app | std::ios::out);
    if (logFile.is_open()) {
        logFile << message << std::endl;
    }
}

void ClearLogFile()
{
    std::lock_guard<std::mutex> lock(g_OutputMutex);
    std::ofstream logFile(g_LogFilePath, std::ios::trunc | std::ios::out);
}
