#include <windows.h>
#include <d3d11.h>
#include <tchar.h>
#include <chrono>

#include "../../L3_state/app_state.h"
#include "ui_panels.h"
#include "../../L3_state/presets.h"

#include "../../../thirdparty/imgui/imgui.h"
#include "../../../thirdparty/imgui/backends/imgui_impl_win32.h"
#include "../../../thirdparty/imgui/backends/imgui_impl_dx11.h"
#include "../../../thirdparty/implot/implot.h"

// --- DX11 Global Variables ---
static ID3D11Device*            g_pd3dDevice = nullptr;
static ID3D11DeviceContext*     g_pd3dDeviceContext = nullptr;
static IDXGISwapChain*          g_pSwapChain = nullptr;
static ID3D11RenderTargetView*  g_mainRenderTargetView = nullptr;

// Helper functions for DX11
bool CreateDeviceD3D(HWND hWnd);
void CleanupDeviceD3D();
void CreateRenderTarget();
void CleanupRenderTarget();
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// --- WIN32 ENTRY POINT & D3D DEVICE CODE ---
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    // Create Application Window
    WNDCLASSEXW wc = { sizeof(wc), CS_CLASSDC, WndProc, 0L, 0L, hInstance, nullptr, nullptr, nullptr, nullptr, L"GT_Thermal1D_WindowClass", nullptr };
    RegisterClassExW(&wc);
    HWND hwnd = CreateWindowW(wc.lpszClassName, L"GT-Thermal 1D System Simulator", WS_OVERLAPPEDWINDOW, 100, 100, 1280, 800, nullptr, nullptr, hInstance, nullptr);

    // Initialize Direct3D
    if (!CreateDeviceD3D(hwnd)) {
        CleanupDeviceD3D();
        UnregisterClassW(wc.lpszClassName, wc.hInstance);
        return 1;
    }

    // Show window
    ShowWindow(hwnd, SW_SHOWMAXIMIZED);
    UpdateWindow(hwnd);

    // Initialize ImGui and ImPlot
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImPlot::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;           // Enable Docking
    
    // Theme setup: Crisp, Professional Light Grey/White theme
    ImGui::StyleColorsLight();
    
    // Tweak Style colors to look like a desktop CAD
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 3.0f;
    style.FrameRounding = 2.0f;
    style.PopupRounding = 3.0f;
    style.GrabRounding = 2.0f;
    style.TabRounding = 3.0f;
    style.WindowBorderSize = 1.0f;
    style.FrameBorderSize = 1.0f;
    
    style.Colors[ImGuiCol_WindowBg]             = ImVec4(0.97f, 0.97f, 0.98f, 1.0f);
    style.Colors[ImGuiCol_ChildBg]              = ImVec4(1.00f, 1.00f, 1.00f, 1.0f);
    style.Colors[ImGuiCol_Border]               = ImVec4(0.69f, 0.71f, 0.76f, 1.0f);
    style.Colors[ImGuiCol_BorderShadow]         = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    style.Colors[ImGuiCol_FrameBg]              = ImVec4(1.00f, 1.00f, 1.00f, 1.0f);
    style.Colors[ImGuiCol_FrameBgHovered]       = ImVec4(0.90f, 0.93f, 0.97f, 1.0f);
    style.Colors[ImGuiCol_FrameBgActive]        = ImVec4(0.85f, 0.89f, 0.95f, 1.0f);
    style.Colors[ImGuiCol_TitleBg]              = ImVec4(0.86f, 0.88f, 0.92f, 1.0f);
    style.Colors[ImGuiCol_TitleBgActive]        = ImVec4(0.80f, 0.83f, 0.88f, 1.0f);
    style.Colors[ImGuiCol_MenuBarBg]            = ImVec4(0.86f, 0.88f, 0.92f, 1.0f);
    style.Colors[ImGuiCol_Header]               = ImVec4(0.90f, 0.93f, 0.97f, 1.0f);
    style.Colors[ImGuiCol_HeaderHovered]        = ImVec4(0.80f, 0.85f, 0.92f, 1.0f);
    style.Colors[ImGuiCol_HeaderActive]         = ImVec4(0.70f, 0.80f, 0.90f, 1.0f);
    style.Colors[ImGuiCol_Tab]                  = ImVec4(0.86f, 0.88f, 0.92f, 1.0f);
    style.Colors[ImGuiCol_TabHovered]           = ImVec4(0.97f, 0.97f, 0.98f, 1.0f);
    style.Colors[ImGuiCol_TabActive]            = ImVec4(0.97f, 0.97f, 0.98f, 1.0f);
    style.Colors[ImGuiCol_Button]               = ImVec4(0.95f, 0.96f, 0.97f, 1.0f);
    style.Colors[ImGuiCol_ButtonHovered]        = ImVec4(0.90f, 0.92f, 0.95f, 1.0f);
    style.Colors[ImGuiCol_ButtonActive]         = ImVec4(0.85f, 0.88f, 0.92f, 1.0f);

    // Setup Platform/Renderer Backends
    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);

    // Load initial preset
    LoadPreset("vehicle");

    // Main Win32 Event Loop
    bool done = false;
    ImVec4 clear_color = ImVec4(0.90f, 0.90f, 0.92f, 1.0f);
    
    auto last_time = std::chrono::high_resolution_clock::now();
    
    while (!done) {
        MSG msg;
        while (PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
            if (msg.message == WM_QUIT)
                done = true;
        }
        if (done) break;

        // Perform simulation steps in real-time if Play is active
        if (g_is_running) {
            auto current_time = std::chrono::high_resolution_clock::now();
            std::chrono::duration<double, std::milli> elapsed = current_time - last_time;
            
            // Run multiple steps if simulation speed is > 1
            for (int step = 0; step < g_sim_speed; ++step) {
                StepSimulation();
            }
            last_time = current_time;
        } else {
            last_time = std::chrono::high_resolution_clock::now();
        }

        // Start ImGui Frame
        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        // Render our application windows
        RenderUI();

        // Rendering
        ImGui::Render();
        const float clear_color_with_alpha[4] = { clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w };
        g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, nullptr);
        g_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView, clear_color_with_alpha);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

        g_pSwapChain->Present(1, 0); // 1 = Vsync ON
    }

    // Cleanup resources
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImPlot::DestroyContext();
    ImGui::DestroyContext();

    CleanupDeviceD3D();
    DestroyWindow(hwnd);
    UnregisterClassW(wc.lpszClassName, wc.hInstance);

    return 0;
}

// --- DX11 HELPER FUNCTIONS ---
bool CreateDeviceD3D(HWND hWnd) {
    DXGI_SWAP_CHAIN_DESC sd;
    ZeroMemory(&sd, sizeof(sd));
    sd.BufferCount = 1;
    sd.BufferDesc.Width = 0;
    sd.BufferDesc.Height = 0;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hWnd;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    UINT createDeviceFlags = 0;
    D3D_FEATURE_LEVEL featureLevel;
    const D3D_FEATURE_LEVEL featureLevelArray[2] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0, };
    
    HRESULT hr = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, createDeviceFlags, featureLevelArray, 2,
        D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext);
    
    if (FAILED(hr)) return false;

    CreateRenderTarget();
    return true;
}

void CleanupDeviceD3D() {
    CleanupRenderTarget();
    if (g_pSwapChain) { g_pSwapChain->Release(); g_pSwapChain = nullptr; }
    if (g_pd3dDeviceContext) { g_pd3dDeviceContext->Release(); g_pd3dDeviceContext = nullptr; }
    if (g_pd3dDevice) { g_pd3dDevice->Release(); g_pd3dDevice = nullptr; }
}

void CreateRenderTarget() {
    ID3D11Texture2D* pBackBuffer;
    g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
    g_pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &g_mainRenderTargetView);
    pBackBuffer->Release();
}

void CleanupRenderTarget() {
    if (g_mainRenderTargetView) { g_mainRenderTargetView->Release(); g_mainRenderTargetView = nullptr; }
}

// Forward declare message handler from imgui_impl_win32.cpp
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;

    switch (msg) {
    case WM_SIZE:
        if (g_pd3dDevice != nullptr && wParam != SIZE_MINIMIZED) {
            CleanupRenderTarget();
            g_pSwapChain->ResizeBuffers(0, (UINT)LOWORD(lParam), (UINT)HIWORD(lParam), DXGI_FORMAT_UNKNOWN, 0);
            CreateRenderTarget();
        }
        return 0;
    case WM_SYSCOMMAND:
        if ((wParam & 0xFFF0) == SC_KEYMENU) // Disable ALT application menu shortcut
            return 0;
        break;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}
