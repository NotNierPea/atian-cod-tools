#include "editor.hpp"
#include "scene.hpp"
#include <fstream>
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

        // the user's theme
        ImGuiStyle& style{ ImGui::GetStyle() };
        ImVec4* colors{ style.Colors };
        colors[ImGuiCol_Text] = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
        colors[ImGuiCol_TextDisabled] = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);
        colors[ImGuiCol_WindowBg] = ImVec4(0.10f, 0.10f, 0.10f, 0.85f);
        colors[ImGuiCol_ChildBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
        colors[ImGuiCol_PopupBg] = ImVec4(0.19f, 0.19f, 0.19f, 0.92f);
        colors[ImGuiCol_Border] = ImVec4(0.19f, 0.19f, 0.19f, 0.29f);
        colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.24f);
        colors[ImGuiCol_FrameBg] = ImVec4(0.05f, 0.05f, 0.05f, 0.54f);
        colors[ImGuiCol_FrameBgHovered] = ImVec4(0.19f, 0.19f, 0.19f, 0.54f);
        colors[ImGuiCol_FrameBgActive] = ImVec4(0.20f, 0.22f, 0.23f, 1.00f);
        colors[ImGuiCol_TitleBg] = ImVec4(0.00f, 0.00f, 0.00f, 1.00f);
        colors[ImGuiCol_TitleBgActive] = ImVec4(0.06f, 0.06f, 0.06f, 1.00f);
        colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.00f, 0.00f, 0.00f, 1.00f);
        colors[ImGuiCol_MenuBarBg] = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
        colors[ImGuiCol_ScrollbarBg] = ImVec4(0.05f, 0.05f, 0.05f, 0.54f);
        colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.34f, 0.34f, 0.34f, 0.54f);
        colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.40f, 0.40f, 0.40f, 0.54f);
        colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.56f, 0.56f, 0.56f, 0.54f);
        colors[ImGuiCol_CheckMark] = ImVec4(0.33f, 0.67f, 0.86f, 1.00f);
        colors[ImGuiCol_SliderGrab] = ImVec4(0.34f, 0.34f, 0.34f, 0.54f);
        colors[ImGuiCol_SliderGrabActive] = ImVec4(0.56f, 0.56f, 0.56f, 0.54f);
        colors[ImGuiCol_Button] = ImVec4(0.05f, 0.05f, 0.05f, 0.54f);
        colors[ImGuiCol_ButtonHovered] = ImVec4(0.19f, 0.19f, 0.19f, 0.54f);
        colors[ImGuiCol_ButtonActive] = ImVec4(0.20f, 0.22f, 0.23f, 1.00f);
        colors[ImGuiCol_Header] = ImVec4(0.00f, 0.00f, 0.00f, 0.52f);
        colors[ImGuiCol_HeaderHovered] = ImVec4(0.00f, 0.00f, 0.00f, 0.36f);
        colors[ImGuiCol_HeaderActive] = ImVec4(0.20f, 0.22f, 0.23f, 0.33f);
        colors[ImGuiCol_Separator] = ImVec4(0.28f, 0.28f, 0.28f, 0.29f);
        colors[ImGuiCol_SeparatorHovered] = ImVec4(0.44f, 0.44f, 0.44f, 0.29f);
        colors[ImGuiCol_SeparatorActive] = ImVec4(0.40f, 0.44f, 0.47f, 1.00f);
        colors[ImGuiCol_ResizeGrip] = ImVec4(0.28f, 0.28f, 0.28f, 0.29f);
        colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.44f, 0.44f, 0.44f, 0.29f);
        colors[ImGuiCol_ResizeGripActive] = ImVec4(0.40f, 0.44f, 0.47f, 1.00f);
        colors[ImGuiCol_Tab] = ImVec4(0.00f, 0.00f, 0.00f, 0.52f);
        colors[ImGuiCol_TabHovered] = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
        colors[ImGuiCol_TabActive] = ImVec4(0.20f, 0.20f, 0.20f, 0.36f);
        colors[ImGuiCol_TabUnfocused] = ImVec4(0.00f, 0.00f, 0.00f, 0.52f);
        colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
        colors[ImGuiCol_PlotLines] = ImVec4(1.00f, 0.00f, 0.00f, 1.00f);
        colors[ImGuiCol_PlotLinesHovered] = ImVec4(1.00f, 0.00f, 0.00f, 1.00f);
        colors[ImGuiCol_PlotHistogram] = ImVec4(1.00f, 0.00f, 0.00f, 1.00f);
        colors[ImGuiCol_PlotHistogramHovered] = ImVec4(1.00f, 0.00f, 0.00f, 1.00f);
        colors[ImGuiCol_TextSelectedBg] = ImVec4(0.26f, 0.59f, 0.98f, 0.35f);
        colors[ImGuiCol_DragDropTarget] = ImVec4(0.33f, 0.67f, 0.86f, 1.00f);
        colors[ImGuiCol_NavHighlight] = ImVec4(1.00f, 0.00f, 0.00f, 1.00f);
        colors[ImGuiCol_NavWindowingHighlight] = ImVec4(1.00f, 0.00f, 0.00f, 0.70f);
        colors[ImGuiCol_NavWindowingDimBg] = ImVec4(1.00f, 0.00f, 0.00f, 0.20f);
        colors[ImGuiCol_ModalWindowDimBg] = ImVec4(1.00f, 0.00f, 0.00f, 0.35f);

        style.WindowPadding = ImVec2(8.00f, 8.00f);
        style.FramePadding = ImVec2(5.00f, 2.00f);
        style.ItemSpacing = ImVec2(6.00f, 6.00f);
        style.ItemInnerSpacing = ImVec2(6.00f, 6.00f);
        style.TouchExtraPadding = ImVec2(0.00f, 0.00f);
        style.IndentSpacing = 25;
        style.ScrollbarSize = 15;
        style.GrabMinSize = 10;
        style.WindowBorderSize = 1;
        style.ChildBorderSize = 1;
        style.PopupBorderSize = 1;
        style.FrameBorderSize = 1;
        style.TabBorderSize = 1;
        style.WindowRounding = 7;
        style.ChildRounding = 4;
        style.FrameRounding = 3;
        style.PopupRounding = 4;
        style.ScrollbarRounding = 9;
        style.GrabRounding = 3;
        style.TabRounding = 4;
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

    // --configtest: the config panel's save on a copy of room.json (acts-map-editor-configtest.json): a keys-only entry
    // changed and saved, the rest of the file kept
    int ConfigTest() {
        if (int ret{ SelfTest() }) return ret;
        namespace scene = editor::scene;
        editor::fs::path copy{ editor::ExeDir() / "acts-map-editor-configtest.json" };
        std::error_code ec;
        editor::fs::copy_file(editor::GetApp().project.dir / "maps" / "room.json", copy, editor::fs::copy_options::overwrite_existing, ec);
        if (ec) return 1;
        auto read = [&]() { std::ifstream is{ copy }; return editor::json::parse(is, nullptr, false); };
        editor::json before{ read() };
        scene::ConfigEntry spawn{ copy, editor::json::array(), "", {}, { "spawnDistance", "spawnSpread", "spawnType" } };
        scene::ConfigEntry window{ copy, editor::json::array({ "windows", 0 }), "", {}, {} };
        std::string error{};
        editor::json v{ scene::ReadConfig(spawn, error) };
        editor::Logf(editor::LogLevel::INFO, "configtest: spawn entry %s %s", v.dump().c_str(), error.c_str());
        v["spawnDistance"] = v["spawnDistance"].get<int>() + 1;
        v.erase("spawnType");
        bool ok{ scene::SaveConfig(spawn, v, error) };
        editor::json w{ scene::ReadConfig(window, error) };
        w["width"] = 120;
        ok = scene::SaveConfig(window, w, error) && ok;
        editor::json after{ read() };
        // what should differ: spawnDistance + 1, spawnType gone, windows[0].width 120
        editor::json expect{ before };
        expect["spawnDistance"] = before["spawnDistance"].get<int>() + 1;
        expect.erase("spawnType");
        expect["windows"][0]["width"] = 120;
        bool same{ after == expect };
        editor::Logf(editor::LogLevel::INFO, "configtest: saves %s, file %s (%s)", ok ? "ok" : error.c_str(), same ? "as expected" : "DIFFERS", editor::ToUtf8(copy).c_str());
        if (!same) editor::Logf(editor::LogLevel::ERR, "configtest: %s", after.dump().substr(0, 600).c_str());
        return ok && same ? 0 : 1;
    }

    // --grouptest: the window, its sill, the walls and the floor moved / scaled / turned (rendered), a dry-run save
    // (nothing written); --copytest: ctrl+D on the window's sill (room.json gets a stone, the scripts listed, not run)
    int GroupTestMain(bool copy = false) {
        if (int ret{ SelfTest() }) return ret;
        const D3D_FEATURE_LEVEL levels[]{ D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0 };
        if (FAILED(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, levels, 2, D3D11_SDK_VERSION, &device, nullptr, &context)) &&
            FAILED(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0, levels, 2, D3D11_SDK_VERSION, &device, nullptr, &context))) {
            return 1;
        }
        editor::SetViewportContext(device, context);
        bool ok{ copy ? editor::CopyTest(editor::GetApp()) : editor::GroupTest(editor::GetApp(), editor::ExeDir()) };
        editor::ShutdownViewport();
        ReleaseDevice();
        return ok ? 0 : 1;
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
        // the west wall-buy's chalk image, its alpha checked
        editor::DumpImage(app.project.dir / "stock_props" / "image" / "hash_29068fa57694f60f.json", editor::ExeDir() / "acts-map-editor-chalk.png");
        editor::DumpImage(app.project.dir / "stock_props" / "image" / "hash_798eac7ea3c7fa92.json", editor::ExeDir() / "acts-map-editor-sky.png", true);
        bool ok{ editor::RenderToFile(app, editor::ExeDir() / "acts-map-editor-view.png", 1280, 720, 0) };
        ok = editor::RenderToFile(app, editor::ExeDir() / "acts-map-editor-all.png", 1280, 720, 1) && ok;
        ok = editor::RenderToFile(app, editor::ExeDir() / "acts-map-editor-top.png", 1280, 720, 2) && ok;
        editor::DumpSky(editor::ExeDir() / "acts-map-editor-sky-shown.png");
        // looking up at the sky from the north west corner (the user's screenshot), then the other way
        editor::SetViewCamera(-1098, -2169, 240, 71, -29);
        ok = editor::RenderToFile(app, editor::ExeDir() / "acts-map-editor-sky-up.png", 1280, 720, -1) && ok;        editor::SetViewCamera(-1098, -2169, 240, 251, -20);
        ok = editor::RenderToFile(app, editor::ExeDir() / "acts-map-editor-sky-back.png", 1280, 720, -1) && ok;
        // from the east, low (the user's second screenshot: the horizon strip's land and the stair steps)
        editor::SetViewCamera(3575, -1082, 555, 180, -12);
        ok = editor::RenderToFile(app, editor::ExeDir() / "acts-map-editor-sky-east.png", 2000, 970, -1) && ok;
        // the west wall-buy's chalk, face on from the room
        editor::SetViewCamera(-250, 0, 56, 180, 0);
        ok = editor::RenderToFile(app, editor::ExeDir() / "acts-map-editor-chalk-view.png", 800, 450, -1) && ok;
        // the south window's barrier boards, from the room and from outside
        editor::SetViewCamera(0, -200, 60, 270, 5);
        ok = editor::RenderToFile(app, editor::ExeDir() / "acts-map-editor-barrier.png", 960, 540, -1) && ok;
        editor::SetViewCamera(30, -460, 70, 100, 8);
        ok = editor::RenderToFile(app, editor::ExeDir() / "acts-map-editor-barrier-out.png", 960, 540, -1) && ok;
        // the sawblade trap (spawned by the map script) by the south wall
        editor::SetViewCamera(300, -60, 150, 216, 26);
        ok = editor::RenderToFile(app, editor::ExeDir() / "acts-map-editor-sawblade.png", 960, 540, -1) && ok;
        // close ups of the script models (the dumped stock ones)
        for (int i = 0; i < 4; i++) {
            char name[64];
            snprintf(name, sizeof(name), "acts-map-editor-model%d.png", i);
            if (!editor::RenderToFile(app, editor::ExeDir() / name, 640, 480, 3 + i)) break;
        }
        editor::ShutdownViewport();
        ReleaseDevice();
        return ok ? 0 : 1;
    }

    // --shot x y z yaw pitch [image json]: the 3D view from that camera into acts-map-editor-shot.png, the image's colours
    // (HDR) into acts-map-editor-image.png
    int ShotTest(const wchar_t* cmdline) {
        if (int ret{ SelfTest() }) return ret;
        float cam[5]{};
        wchar_t image[1024]{};
        int read{ swscanf_s(wcsstr(cmdline, L"--shot") + 6, L"%f %f %f %f %f %1023ls", &cam[0], &cam[1], &cam[2], &cam[3], &cam[4], image, (unsigned)_countof(image)) };
        if (read < 5) {
            editor::Log(editor::LogLevel::ERR, "--shot x y z yaw pitch [image json]");
            return 1;
        }
        const D3D_FEATURE_LEVEL levels[]{ D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0 };
        if (FAILED(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, levels, 2, D3D11_SDK_VERSION, &device, nullptr, &context)) &&
            FAILED(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0, levels, 2, D3D11_SDK_VERSION, &device, nullptr, &context))) {
            return 1;
        }
        editor::SetViewportContext(device, context);
        editor::App& app{ editor::GetApp() };
        if (read > 5) editor::DumpImage(image, editor::ExeDir() / "acts-map-editor-image.png", true);
        editor::SetViewCamera(cam[0], cam[1], cam[2], cam[3], cam[4]);
        bool ok{ editor::RenderToFile(app, editor::ExeDir() / "acts-map-editor-shot.png", 1280, 720, -1) };
        editor::ShutdownViewport();
        ReleaseDevice();
        return ok ? 0 : 1;
    }
}

namespace {
    namespace fs = std::filesystem;

    // --ui-shot: the window drawn offscreen (the 3D view's tab, facing the south window from the room, a board of its
    // barrier selected: its config in the side panel) into acts-map-editor-ui.png; loadingShot: its loading screen
    int UiShot(const char* tab, const fs::path& out, int mode) {
        if (int ret{ SelfTest() }) return ret;
        const D3D_FEATURE_LEVEL levels[]{ D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0 };
        if (FAILED(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, levels, 2, D3D11_SDK_VERSION, &device, nullptr, &context)) &&
            FAILED(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0, levels, 2, D3D11_SDK_VERSION, &device, nullptr, &context))) {
            return 1;
        }
        editor::SetDevice(device);
        editor::SetViewportContext(device, context);
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io{ ImGui::GetIO() };
        io.IniFilename = nullptr;
        SetupStyle(1.0f);
        ImGui_ImplDX11_Init(device, context);

        const int W{ 1600 }, H{ 900 };
        D3D11_TEXTURE2D_DESC td{};
        td.Width = W;
        td.Height = H;
        td.MipLevels = 1;
        td.ArraySize = 1;
        td.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        td.SampleDesc.Count = 1;
        td.BindFlags = D3D11_BIND_RENDER_TARGET;
        ID3D11Texture2D* shot{};
        device->CreateTexture2D(&td, nullptr, &shot);
        device->CreateRenderTargetView(shot, nullptr, &target);

        editor::ShowTab(tab);
        editor::SetViewCamera(0, -170, 64, 270, 6);
        const float clear[4]{ 0.08f, 0.08f, 0.09f, 1.0f };
        // the frames the 3D tab's loading takes, then a few with the selection; mode 1 the loading's 4th frame, mode 2
        // a reload's 2nd frame (built aside: the old scene still drawn)
        int loaded{ -1 }, stop{ 600 };
        for (int frame = 0; frame < stop; frame++) {
            io.DisplaySize = ImVec2((float)W, (float)H);
            io.DeltaTime = 1.0f / 60;
            ImGui_ImplDX11_NewFrame();
            ImGui::NewFrame();
            editor::DrawApp();
            if (mode == 1 && frame == 3) stop = frame + 1;
            else if (loaded < 0 && frame > 0 && !editor::ViewportLoading()) {
                loaded = frame;
                editor::SelectObject("room_window_a_barrier board 3");
                stop = frame + (mode == 2 ? 5 : mode == 3 ? 90 : 3);
            }
            if (mode >= 2 && loaded >= 0 && frame == loaded + 2) editor::GetApp().project.Reload();
            ImGui::Render();
            context->OMSetRenderTargets(1, &target, nullptr);
            context->ClearRenderTargetView(target, clear);
            ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
        }

        td.Usage = D3D11_USAGE_STAGING;
        td.BindFlags = 0;
        td.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
        ID3D11Texture2D* staging{};
        device->CreateTexture2D(&td, nullptr, &staging);
        context->CopyResource(staging, shot);
        D3D11_MAPPED_SUBRESOURCE mapped{};
        bool ok{};
        if (SUCCEEDED(context->Map(staging, 0, D3D11_MAP_READ, 0, &mapped))) {
            std::vector<uint8_t> rgba((size_t)W * H * 4);
            for (int y = 0; y < H; y++) memcpy(rgba.data() + (size_t)y * W * 4, (const uint8_t*)mapped.pData + (size_t)y * mapped.RowPitch, (size_t)W * 4);
            context->Unmap(staging, 0);
            for (size_t i = 3; i < rgba.size(); i += 4) rgba[i] = 255;
            ok = editor::WritePng(out, W, H, rgba.data());
        }
        staging->Release();
        shot->Release();
        editor::ShutdownViewport();
        editor::ClearTextures();
        ImGui_ImplDX11_Shutdown();
        ImGui::DestroyContext();
        ReleaseDevice();
        return ok ? 0 : 1;
    }
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR cmdline, int) {
    if (cmdline && wcsstr(cmdline, L"--ui-shot")) {
        // "--ui-shot zone": the Zone tab; "loading": the 3D tab's loading screen; "reload": a reload under way;
        // "reloaded": after it (swapped in, the selection kept)
        bool zone{ wcsstr(cmdline, L"zone") != nullptr };
        int mode{ wcsstr(cmdline, L"loading") ? 1 : wcsstr(cmdline, L"reloaded") ? 3 : wcsstr(cmdline, L"reload") ? 2 : 0 };
        const char* names[]{ "acts-map-editor-ui.png", "acts-map-editor-ui-loading.png", "acts-map-editor-ui-reload.png", "acts-map-editor-ui-reloaded.png" };
        return UiShot(zone ? "Zone" : "3D view", editor::ExeDir() / (zone ? "acts-map-editor-ui-zone.png" : names[mode]), mode);
    }
    if (cmdline && wcsstr(cmdline, L"--grouptest")) {
        return GroupTestMain();
    }
    if (cmdline && wcsstr(cmdline, L"--shot")) {
        return ShotTest(cmdline);
    }
    if (cmdline && wcsstr(cmdline, L"--copytest")) {
        return GroupTestMain(true);
    }
    if (cmdline && wcsstr(cmdline, L"--configtest")) {
        return ConfigTest();
    }
    if (cmdline && wcsstr(cmdline, L"--savetest")) {
        if (int ret{ SelfTest() }) return ret;
        return editor::SaveTest(editor::GetApp()) ? 0 : 1;
    }
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
    // no DockingEnable / ViewportsEnable: acts' imgui is the master branch, not docking
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard | ImGuiConfigFlags_NavEnableGamepad;
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
