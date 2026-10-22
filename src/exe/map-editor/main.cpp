#include "editor.hpp"
#include <backends/imgui_impl_dx11.h>
#include <backends/imgui_impl_win32.h>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace {
    ID3D11Device* device{};
    ID3D11DeviceContext* context{};
    IDXGISwapChain* swapChain{};
    ID3D11RenderTargetView* target{};
    UINT resizeWidth{};
    UINT resizeHeight{};

    void CreateTarget() {
        ID3D11Texture2D* back{};
        swapChain->GetBuffer(0, IID_PPV_ARGS(&back));
        if (back) {
            device->CreateRenderTargetView(back, nullptr, &target);
            back->Release();
        }
    }

    void ReleaseTarget() {
        if (target) {
            target->Release();
            target = nullptr;
        }
    }

    bool CreateDevice(HWND hwnd) {
        DXGI_SWAP_CHAIN_DESC sd{};
        sd.BufferCount = 2;
        sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        sd.BufferDesc.RefreshRate.Numerator = 60;
        sd.BufferDesc.RefreshRate.Denominator = 1;
        sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
        sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        sd.OutputWindow = hwnd;
        sd.SampleDesc.Count = 1;
        sd.Windowed = TRUE;
        sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

        const D3D_FEATURE_LEVEL levels[]{ D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0 };
        D3D_FEATURE_LEVEL level{};
        HRESULT hr{ D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, levels, 2,
            D3D11_SDK_VERSION, &sd, &swapChain, &device, &level, &context) };
        // no GPU driver: the software one
        if (hr == DXGI_ERROR_UNSUPPORTED) {
            hr = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0, levels, 2,
                D3D11_SDK_VERSION, &sd, &swapChain, &device, &level, &context);
        }
        if (FAILED(hr)) {
            return false;
        }
        CreateTarget();
        return true;
    }

    void ReleaseDevice() {
        ReleaseTarget();
        if (swapChain) { swapChain->Release(); swapChain = nullptr; }
        if (context) { context->Release(); context = nullptr; }
        if (device) { device->Release(); device = nullptr; }
    }

    LRESULT WINAPI WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
        if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wParam, lParam)) {
            return true;
        }
        switch (msg) {
        case WM_SIZE:
            if (wParam != SIZE_MINIMIZED) {
                resizeWidth = (UINT)LOWORD(lParam);
                resizeHeight = (UINT)HIWORD(lParam);
            }
            return 0;
        case WM_SYSCOMMAND:
            // no menu on alt
            if ((wParam & 0xfff0) == SC_KEYMENU) {
                return 0;
            }
            break;
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
        }
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }

    void SetupStyle(float scale) {
        ImGuiIO& io{ ImGui::GetIO() };
        const char* fonts[]{ "C:\\Windows\\Fonts\\segoeui.ttf", "C:\\Windows\\Fonts\\arial.ttf" };
        for (const char* font : fonts) {
            if (GetFileAttributesA(font) != INVALID_FILE_ATTRIBUTES && io.Fonts->AddFontFromFileTTF(font, 17.0f * scale)) {
                break;
            }
        }

        ImGui::StyleColorsDark();
        ImGuiStyle& style{ ImGui::GetStyle() };
        style.WindowRounding = 0;
        style.FrameRounding = 3;
        style.GrabRounding = 3;
        style.TabRounding = 3;
        style.FramePadding = ImVec2(8, 4);
        style.ItemSpacing = ImVec2(8, 6);
        ImVec4* colors{ style.Colors };
        colors[ImGuiCol_WindowBg] = ImVec4(0.11f, 0.11f, 0.12f, 1.00f);
        colors[ImGuiCol_ChildBg] = ImVec4(0.13f, 0.13f, 0.14f, 1.00f);
        colors[ImGuiCol_Header] = ImVec4(0.22f, 0.32f, 0.45f, 0.70f);
        colors[ImGuiCol_HeaderHovered] = ImVec4(0.26f, 0.38f, 0.54f, 0.80f);
        colors[ImGuiCol_HeaderActive] = ImVec4(0.28f, 0.42f, 0.60f, 1.00f);
        colors[ImGuiCol_Tab] = ImVec4(0.16f, 0.17f, 0.19f, 1.00f);
        colors[ImGuiCol_TabActive] = ImVec4(0.24f, 0.34f, 0.48f, 1.00f);
        colors[ImGuiCol_TabHovered] = ImVec4(0.28f, 0.40f, 0.56f, 1.00f);
        style.ScaleAllSizes(scale);
    }
}

namespace {
    // --selftest: the saved map and its lighting loaded without a window, what was read in acts-map-editor-selftest.txt
    int SelfTest() {
        editor::SetLogFile(editor::ExeDir() / "acts-map-editor-selftest.txt");
        editor::GetSettings().Load();
        editor::App& app{ editor::GetApp() };
        if (!app.project.Open(editor::GetSettings().mapDir)) {
            return 1;
        }
        const editor::ZoneFile& zone{ app.project.zone };
        editor::Logf(editor::LogLevel::INFO, "zone name %s, %zu types", app.project.ZoneName().c_str(), zone.typeCounts.size());
        for (const editor::ZoneBlock& b : zone.blocks) {
            size_t missing{};
            for (const editor::ZoneEntry& e : b.entries) {
                if (e.state == editor::EntryState::MISSING) {
                    missing++;
                    editor::Logf(editor::LogLevel::WARN, "  missing line %zu: %s,%s", e.line, e.type.c_str(), e.value.c_str());
                }
            }
            editor::Logf(editor::LogLevel::INFO, "block line %zu%s: %s (%zu, %zu missing)", b.line, b.marked ? " [tool]" : "",
                b.name.c_str(), b.entries.size(), missing);
        }
        app.lighting.Load(app.project);
        editor::Logf(editor::LogLevel::INFO, "lighting error '%s', exposure %s", app.lighting.error.c_str(),
            app.lighting.root.contains("exposure") ? app.lighting.root["exposure"].dump().c_str() : "none");
        for (const editor::LightingState& s : app.lighting.states) {
            editor::Logf(editor::LogLevel::INFO, "state %s 0x%zx bytes", editor::ToUtf8(s.path).c_str(), s.data.size());
        }
        return 0;
    }

    // --render: the 3D view drawn without a window into acts-map-editor-view.png / -all.png / -top.png
    int RenderTest() {
        if (int ret{ SelfTest() }) return ret;
        const D3D_FEATURE_LEVEL levels[]{ D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0 };
        if (FAILED(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, levels, 2, D3D11_SDK_VERSION, &device, nullptr, &context)) &&
            FAILED(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0, levels, 2, D3D11_SDK_VERSION, &device, nullptr, &context))) {
            editor::Log(editor::LogLevel::ERR, "can't create a Direct3D 11 device");
            return 1;
        }
        editor::SetViewportContext(device, context);
        editor::App& app{ editor::GetApp() };
        bool ok{ editor::RenderToFile(app, editor::ExeDir() / "acts-map-editor-view.png", 1280, 720, 0) };
        ok = editor::RenderToFile(app, editor::ExeDir() / "acts-map-editor-all.png", 1280, 720, 1) && ok;
        ok = editor::RenderToFile(app, editor::ExeDir() / "acts-map-editor-top.png", 1280, 720, 2) && ok;
        editor::ShutdownViewport();
        ReleaseDevice();
        return ok ? 0 : 1;
    }
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR cmdline, int) {
    if (cmdline && wcsstr(cmdline, L"--render")) {
        return RenderTest();
    }
    if (cmdline && wcsstr(cmdline, L"--selftest")) {
        return SelfTest();
    }
    ImGui_ImplWin32_EnableDpiAwareness();
    float scale{ ImGui_ImplWin32_GetDpiScaleForMonitor(MonitorFromPoint(POINT{ 0, 0 }, MONITOR_DEFAULTTOPRIMARY)) };

    WNDCLASSEXW wc{ sizeof(wc), CS_CLASSDC, WndProc, 0, 0, instance, LoadIconA(nullptr, IDI_APPLICATION),
        LoadCursorA(nullptr, IDC_ARROW), nullptr, nullptr, L"ActsMapEditor", nullptr };
    RegisterClassExW(&wc);
    HWND hwnd{ CreateWindowW(wc.lpszClassName, L"acts map editor", WS_OVERLAPPEDWINDOW, 100, 100,
        (int)(1600 * scale), (int)(950 * scale), nullptr, nullptr, instance, nullptr) };

    if (!CreateDevice(hwnd)) {
        ReleaseDevice();
        UnregisterClassW(wc.lpszClassName, instance);
        MessageBoxW(nullptr, L"Can't create a Direct3D 11 device", L"acts map editor", MB_ICONERROR);
        return 1;
    }
    ShowWindow(hwnd, SW_SHOWDEFAULT);
    UpdateWindow(hwnd);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io{ ImGui::GetIO() };
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    static std::string iniPath{ editor::ToUtf8(editor::ExeDir() / "acts-map-editor.ini") };
    io.IniFilename = iniPath.c_str();
    SetupStyle(scale);
    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX11_Init(device, context);
    editor::SetDevice(device);
    editor::SetViewportContext(device, context);

    editor::GetSettings().Load();
    editor::App& app{ editor::GetApp() };
    if (!editor::GetSettings().mapDir.empty()) {
        app.project.Open(editor::GetSettings().mapDir);
    }

    const float clear[4]{ 0.08f, 0.08f, 0.09f, 1.0f };
    while (!app.quit) {
        MSG msg;
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
            if (msg.message == WM_QUIT) {
                app.quit = true;
            }
        }
        if (app.quit) {
            break;
        }

        if (resizeWidth && resizeHeight) {
            ReleaseTarget();
            swapChain->ResizeBuffers(0, resizeWidth, resizeHeight, DXGI_FORMAT_UNKNOWN, 0);
            resizeWidth = resizeHeight = 0;
            CreateTarget();
        }

        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();
        editor::DrawApp();
        ImGui::Render();

        context->OMSetRenderTargets(1, &target, nullptr);
        context->ClearRenderTargetView(target, clear);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
        swapChain->Present(1, 0);
    }

    editor::GetSettings().Save();
    editor::ShutdownViewport();
    editor::ClearTextures();
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    ReleaseDevice();
    DestroyWindow(hwnd);
    UnregisterClassW(wc.lpszClassName, instance);
    return 0;
}
