#pragma once

#include <d3d11.h>
#include <tchar.h>
#include <string>
#include <stdio.h>
#include <stdlib.h>
#include <windows.h>
#include <psapi.h>
#include <iostream>

#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"
#include "Utils.h"
#include "MinHook.h"
#include "Player.h"

class DamageSummary;

extern DamageSummary g_damageSummary;

extern HWND hwnd;
extern ID3D11Device* d3dDevice;
extern ID3D11DeviceContext* d3dContext;
extern bool swapChainOccluded;
extern HHOOK messageHook;
extern bool init;
extern bool done;
extern ImGuiIO* io;

extern float DEFAULT_POSITION_POS_X;
extern float DEFAULT_POSITION_POS_Y;
extern float FIXED_FONT_SIZE;

typedef HRESULT(WINAPI* D3D11CreateDeviceFn)(
    IDXGIAdapter* pAdapter,
    D3D_DRIVER_TYPE DriverType,
    HMODULE Software,
    UINT Flags,
    const D3D_FEATURE_LEVEL* pFeatureLevels,
    UINT FeatureLevels,
    UINT SDKVersion,
    ID3D11Device** ppDevice,
    D3D_FEATURE_LEVEL* pFeatureLevel,
    ID3D11DeviceContext** ppImmediateContext
    );

HRESULT WINAPI Hooked_D3D11CreateDevice(
    IDXGIAdapter* pAdapter,
    D3D_DRIVER_TYPE DriverType,
    HMODULE Software,
    UINT Flags,
    const D3D_FEATURE_LEVEL* pFeatureLevels,
    UINT FeatureLevels,
    UINT SDKVersion,
    ID3D11Device** ppDevice,
    D3D_FEATURE_LEVEL* pFeatureLevel,
    ID3D11DeviceContext** ppImmediateContext
);

void InitDirectXDeviceHook();
void UnHook();
void MainLoop();

LRESULT __stdcall WndProc(const HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

void Start();
void Exit();