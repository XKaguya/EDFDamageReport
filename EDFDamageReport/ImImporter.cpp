// Most of the ImGui Code is from Discord @KittopiaCreator.

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
#include "Memory.h"

HWND hwnd = nullptr;
ID3D11Device* d3dDevice = nullptr;
ID3D11DeviceContext* d3dContext = nullptr;
bool swapChainOccluded = false;
HHOOK messageHook = NULL;
bool init = false;
bool done = false;
ImGuiIO* io = nullptr;

float DEFAULT_POSITION_POS_X = 50.0f;
float DEFAULT_POSITION_POS_Y = 760.0f;
float FIXED_FONT_SIZE = 38.0f / 1.5f;

void MainLoop();

extern DamageSummary g_damageSummary;
extern uintptr_t baseAddress;
extern SignatureTable g_SignatureTable;

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
    ID3D11DeviceContext** ppImmediateContext);

D3D11CreateDeviceFn OriginalD3D11CreateDevice = nullptr;

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
    ID3D11DeviceContext** ppImmediateContext)
{
    DebugOutputFormat("[ImImporter] Hooked_D3D11CreateDevice called");
    HRESULT HookResult = OriginalD3D11CreateDevice(pAdapter, DriverType, Software, Flags, pFeatureLevels, FeatureLevels, SDKVersion, ppDevice, pFeatureLevel, ppImmediateContext);

    if (SUCCEEDED(HookResult)) 
    {
        DebugOutputFormat("[ImImporter] D3D11CreateDevice succeeded");
        if (ppDevice && *ppDevice) 
        {
            d3dDevice = *ppDevice;
            d3dDevice->AddRef();
            DebugOutputFormat("[ImImporter] d3dDevice acquired: 0x%p", d3dDevice);
        }
        if (ppImmediateContext && *ppImmediateContext) {
            d3dContext = *ppImmediateContext;
            d3dContext->AddRef();
            DebugOutputFormat("[ImImporter] d3dContext acquired: 0x%p", d3dContext);
        }
    }
    else
    {
        DebugOutputFormat("[ImImporter] D3D11CreateDevice failed: HRESULT=0x%08X", HookResult);
    }

    return HookResult;
}

static bool showDamageSummary = false;
static std::string cachedDamageSummary;
static bool cacheValid = false;
static float fontSize = 18.0f;
static ImFont* dynamicFont = nullptr;
static bool fontDirty = true;
static bool realTimeUpdate = false;

void ReloadFont(float size) {
    ImGuiIO& io = ImGui::GetIO();
    io.Fonts->Clear();
    dynamicFont = io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\CascadiaMono.ttf", size);
    if (!dynamicFont) dynamicFont = io.Fonts->AddFontDefault();
    ImGui_ImplDX11_InvalidateDeviceObjects();
    ImGui_ImplDX11_CreateDeviceObjects();
    fontDirty = false;
}

void MainLoop()
{
    if (!init || done)
    {
        DebugOutputFormat("[ImImporter] MainLoop skipped: init=%d, done=%d", init, done);
        return;
    }

    if (fontDirty) {
        ReloadFont(fontSize);
    }

    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    ImGuiIO& io = ImGui::GetIO();
    io.WantCaptureMouse = true;
    io.WantCaptureKeyboard = true;

    const ImGuiViewport* main_viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(ImVec2(main_viewport->WorkPos.x + 650, main_viewport->WorkPos.y + 20), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(550, 680), ImGuiCond_FirstUseEver);
    ImGui::Begin("Damage Summary Plugin");

    if (ImGui::SliderFloat("Font Size", &fontSize, 10.0f, 48.0f, "%.1f")) {
        fontDirty = true;
    }
    ImGui::Checkbox("Real-time Update", &realTimeUpdate);

    if (ImGui::Button(showDamageSummary ? "Hide Damage Summary" : "Show Damage Summary"))
    {
        showDamageSummary = !showDamageSummary;
        cacheValid = false;
    }

    if (showDamageSummary && ImGui::Button("Refresh Data"))
    {
        cacheValid = false;
    }

    if (showDamageSummary)
    {
        if (realTimeUpdate || !cacheValid) {
            cachedDamageSummary = g_damageSummary.formatAllDamagesSummary();
            cacheValid = true;
        }

        if (dynamicFont) ImGui::PushFont(dynamicFont);
        ImGui::TextWrapped(cachedDamageSummary.c_str());
        if (dynamicFont) ImGui::PopFont();
    }

    std::vector<std::string> recentAttacks = g_damageSummary.getFormattedAttacks();
    ImGui::Separator();
    ImGui::Text("Recent 5 Attacks:");
    if (dynamicFont) ImGui::PushFont(dynamicFont);
    for (const auto& record : recentAttacks) {
        ImGui::TextWrapped("%s", record.c_str());
    }
    if (dynamicFont) ImGui::PopFont();

    ImGui::End();
    ImGui::Render();
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
}

using UpdateRadarFuncType = void(__fastcall*)(__int64, __int64);
UpdateRadarFuncType OriginalUpdateRadar = nullptr;

void __fastcall Hook_UpdateRadar(__int64 a1, __int64 a2)
{
    OriginalUpdateRadar(a1, a2);

    MainLoop();
}

void InitDirectXDeviceHook() 
{
    DebugOutputFormat("[ImImporter] InitDirectXDeviceHook called");
    HMODULE hD3D11 = GetModuleHandleW(L"d3d11.dll");
    if (!hD3D11) {
        DebugOutputFormat("[ImImporter] Failed to get d3d11.dll module handle!");
        return;
    }

    void* pD3D11CreateDevice = GetProcAddress(hD3D11, "D3D11CreateDevice");
    if (!pD3D11CreateDevice)
    {
        DebugOutputFormat("[ImImporter] Failed to get D3D11CreateDevice address!");
        return;
    }

    if (MH_CreateHook(pD3D11CreateDevice, &Hooked_D3D11CreateDevice, reinterpret_cast<LPVOID*>(&OriginalD3D11CreateDevice)) != MH_OK)
    {
        DebugOutputFormat("[ImImporter] Failed to create hook for D3D11CreateDevice!");
        return;
    }

    if (MH_EnableHook(pD3D11CreateDevice) != MH_OK)
    {
        DebugOutputFormat("[ImImporter] Failed to enable hook for D3D11CreateDevice!");
        return;
    }
    DebugOutputFormat("[ImImporter] D3D11CreateDevice hook enabled");
}

void InitRadarHook()
{
    if (MH_CreateHook((LPVOID)g_SignatureTable.UpdateRadarPointer, &Hook_UpdateRadar, reinterpret_cast<LPVOID*>(&OriginalUpdateRadar)) != MH_OK)
    {
        DebugOutputFormat("Failed to create hook for UpdateRadar!");
        return;
    }

    if (MH_EnableHook((LPVOID)g_SignatureTable.UpdateRadarPointer) != MH_OK)
    {
        DebugOutputFormat("Failed to enable hook for UpdateRadar!");
        return;
    }
}

void UnHook() 
{
    DebugOutputFormat("[ImImporter] UnHook called");
    if (d3dDevice) 
    {
        DebugOutputFormat("[ImImporter] Releasing d3dDevice: 0x%p", d3dDevice);
        d3dDevice->Release();
        d3dDevice = nullptr;
    }
    if (d3dContext) 
    {
        DebugOutputFormat("[ImImporter] Releasing d3dContext: 0x%p", d3dContext);
        d3dContext->Release();
        d3dContext = nullptr;
    }
}

WNDPROC OriginalWndProc;

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

LRESULT __stdcall WndProc(const HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    if (ImGui_ImplWin32_WndProcHandler(hWnd, uMsg, wParam, lParam))
    {
        return true;
    }
    return CallWindowProc(OriginalWndProc, hWnd, uMsg, wParam, lParam);
}

void Start()
{
    DebugOutputFormat("[ImImporter] Start called");
    DebugOutputFormat("[ImImporter] Base address=0x%p", baseAddress);
    InitDirectXDeviceHook();
    InitRadarHook();
    while (d3dDevice == nullptr || d3dContext == nullptr)
    {
        DebugOutputFormat("[ImImporter] Waiting for d3dDevice/d3dContext...");
        Sleep(1000);
    }
    DebugOutputFormat("[ImImporter] d3dDevice=0x%p, d3dContext=0x%p", d3dDevice, d3dContext);

    HWND* hwndPTR = reinterpret_cast<HWND*>(baseAddress + 0x21360a8);
    while (*hwndPTR == nullptr)
    {
        DebugOutputFormat("[ImImporter] Waiting for hwnd...");
        Sleep(1000);
    }
    hwnd = *hwndPTR;
    DebugOutputFormat("[ImImporter] hwnd acquired: 0x%p", hwnd);

    OriginalWndProc = (WNDPROC)SetWindowLongPtr(hwnd, GWLP_WNDPROC, (LONG_PTR)WndProc);
    DebugOutputFormat("[ImImporter] WndProc hooked");

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    io = &ImGui::GetIO(); 
    (void)io;

    ImGui::StyleColorsDark();

    // Setup Platform/Renderer backends
    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX11_Init(d3dDevice, d3dContext);

    ImFontConfig config;
    config.GlyphRanges = io->Fonts->GetGlyphRangesChineseFull();

    init = true;
    MainLoop();
    DebugOutputFormat("[ImImporter] ImGui initialized, init=%d", init);
}

void Exit()
{
    DebugOutputFormat("[ImImporter] Exit called");
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    UnHook();
    MH_DisableHook(MH_ALL_HOOKS);
    DebugOutputFormat("[ImImporter] Exit finished");
}