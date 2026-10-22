#include "editor.hpp"
#include <misc/cpp/imgui_stdlib.h>
#include <algorithm>
#include <ShObjIdl.h>
#include <shellapi.h>

namespace editor {
    namespace {
        // is the game up: checked every 2 s, deploys wait for it to close
        bool GameRunning() {
            static double next{};
            static bool running{};
            double now{ ImGui::GetTime() };
            if (now >= next) {
                running = IsProcessRunning(L"BlackOps4.exe");
                next = now + 2.0;
            }
            return running;
        }

        void OpenMapDialog(App& app) {
            fs::path dir{};
            if (PickFolder(dir, L"The map's folder (its .zone file in it)")) {
                app.project.Open(dir);
                app.lighting = {};
            }
        }

        void DrawMenu(App& app) {
            if (!ImGui::BeginMenuBar()) return;
            if (ImGui::BeginMenu("File")) {
                if (ImGui::MenuItem("Open map folder...")) OpenMapDialog(app);
                if (ImGui::BeginMenu("Recent maps", !GetSettings().recentMaps.empty())) {
                    for (const fs::path& p : std::vector<fs::path>{ GetSettings().recentMaps }) {
                        if (ImGui::MenuItem(ToUtf8(p).c_str())) {
                            app.project.Open(p);
                            app.lighting = {};
                        }
                    }
                    ImGui::EndMenu();
                }
                if (ImGui::MenuItem("Reload", "F5", false, app.project.IsOpen())) {
                    app.project.Reload();
                    app.lighting = {};
                }
                ImGui::Separator();
                if (ImGui::MenuItem("Settings...")) app.showSettings = true;
                ImGui::Separator();
                if (ImGui::MenuItem("Exit")) app.quit = true;
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Build", app.project.IsOpen())) {
                if (ImGui::MenuItem("Link", "F7", false, !app.build.Busy())) app.build.Link(app.project, false);
                if (ImGui::MenuItem("Deploy", nullptr, false, !app.build.Busy())) app.build.Deploy(app.project);
                if (ImGui::MenuItem("Link and deploy", "F8", false, !app.build.Busy())) app.build.Link(app.project, true);
                ImGui::Separator();
                if (ImGui::MenuItem("Open the stage folder")) OpenInShell(GetSettings().StageFor(app.project.ZoneName()));
                ImGui::EndMenu();
            }
            ImGui::EndMenuBar();
        }

        void DrawToolbar(App& app) {
            Project& p{ app.project };
            ImGui::AlignTextToFramePadding();
            ImGui::Text("Map");
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(0.75f, 0.88f, 1.0f, 1), "%s", p.Name().c_str());
            if (p.zones.size() > 1) {
                ImGui::SameLine();
                ImGui::SetNextItemWidth(220);
                std::string current{ ToUtf8(p.zones[p.zoneIndex].filename()) };
                if (ImGui::BeginCombo("##zone", current.c_str())) {
                    for (size_t i = 0; i < p.zones.size(); i++) {
                        if (ImGui::Selectable(ToUtf8(p.zones[i].filename()).c_str(), i == p.zoneIndex)) {
                            p.zoneIndex = i;
                            p.Reload();
                            app.lighting = {};
                        }
                    }
                    ImGui::EndCombo();
                }
            }
            ImGui::SameLine(0, 24);
            ImGui::BeginDisabled(app.build.Busy());
            if (ImGui::Button("Link")) app.build.Link(p, false);
            ImGui::SameLine();
            if (ImGui::Button("Deploy")) app.build.Deploy(p);
            ImGui::SameLine();
            if (ImGui::Button("Link & deploy")) app.build.Link(p, true);
            ImGui::EndDisabled();
            ImGui::SameLine();
            if (app.build.step == Build::Step::LINKING) {
                ImGui::TextColored(ImVec4(1, 0.8f, 0.35f, 1), "linking %s... %.0f s", app.build.zoneName.c_str(), app.build.Seconds());
                ImGui::SameLine();
                if (ImGui::SmallButton("Stop")) app.build.process.Kill();
            }
            else if (app.build.step == Build::Step::DEPLOYING) {
                ImGui::TextColored(ImVec4(1, 0.8f, 0.35f, 1), "deploying... %.0f s", app.build.Seconds());
            }
            if (GameRunning()) {
                ImGui::SameLine(0, 24);
                ImGui::TextColored(ImVec4(1, 0.45f, 0.4f, 1), "Black Ops 4 is running");
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("Deploys need it closed: the game holds its zone files open.");
            }
        }

        void DrawSettings(App& app) {
            if (!app.showSettings) return;
            ImGui::SetNextWindowSize(ImVec2(760, 0), ImGuiCond_Appearing);
            if (ImGui::Begin("Settings", &app.showSettings, ImGuiWindowFlags_NoCollapse)) {
                Settings& s{ GetSettings() };
                auto pathRow = [](const char* label, fs::path& path, bool folder, const char* help) {
                    std::string v{ ToUtf8(path) };
                    ImGui::PushID(label);
                    ImGui::SetNextItemWidth(-260);
                    if (ImGui::InputText(label, &v)) path = FromUtf8(v);
                    ImGui::SameLine();
                    if (folder && ImGui::Button("Browse")) {
                        fs::path picked{};
                        if (PickFolder(picked, L"Choose a folder")) path = picked;
                    }
                    ImGui::SameLine();
                    HelpMarker(help);
                    ImGui::PopID();
                };
                pathRow("acts.exe", s.acts, false, "the acts that links the zone (fastfilelinker)");
                pathRow("Game folder", s.gameDir, true, "Black Ops 4's folder: deploys go to its project-bo4");
                pathRow("Stage folder", s.stageDir, true, "where links go before a deploy (empty: map-editor-stage next to the editor)");
                if (ImGui::Button("Save")) {
                    s.Save();
                    Log(LogLevel::INFO, "settings saved");
                }
            }
            ImGui::End();
        }
    }

    App& GetApp() {
        static App app{};
        return app;
    }

    void HelpMarker(const char* text) {
        ImGui::TextDisabled("(?)");
        if (ImGui::BeginItemTooltip()) {
            ImGui::PushTextWrapPos(ImGui::GetFontSize() * 30.0f);
            ImGui::TextUnformatted(text);
            ImGui::PopTextWrapPos();
            ImGui::EndTooltip();
        }
    }

    bool PickFolder(fs::path& out, const wchar_t* title) {
        HRESULT init{ CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE) };
        bool ok{};
        IFileOpenDialog* dialog{};
        if (SUCCEEDED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_ALL, IID_PPV_ARGS(&dialog)))) {
            DWORD options{};
            dialog->GetOptions(&options);
            dialog->SetOptions(options | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM);
            dialog->SetTitle(title);
            if (SUCCEEDED(dialog->Show(GetActiveWindow()))) {
                IShellItem* item{};
                if (SUCCEEDED(dialog->GetResult(&item))) {
                    PWSTR name{};
                    if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &name))) {
                        out = name;
                        ok = true;
                        CoTaskMemFree(name);
                    }
                    item->Release();
                }
            }
            dialog->Release();
        }
        if (SUCCEEDED(init)) CoUninitialize();
        return ok;
    }

    void OpenInShell(const fs::path& path) {
        ShellExecuteW(nullptr, L"open", path.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    }

    void ShowInExplorer(const fs::path& path) {
        std::wstring args{ L"/select,\"" + path.wstring() + L"\"" };
        ShellExecuteW(nullptr, L"open", L"explorer.exe", args.c_str(), nullptr, SW_SHOWNORMAL);
    }

    void DrawApp() {
        App& app{ GetApp() };
        app.build.Update(app.project);

        if (app.project.IsOpen() && !app.build.Busy()) {
            if (ImGui::IsKeyPressed(ImGuiKey_F5, false)) { app.project.Reload(); app.lighting = {}; }
            if (ImGui::IsKeyPressed(ImGuiKey_F7, false)) app.build.Link(app.project, false);
            if (ImGui::IsKeyPressed(ImGuiKey_F8, false)) app.build.Link(app.project, true);
        }

        const ImGuiViewport* vp{ ImGui::GetMainViewport() };
        ImGui::SetNextWindowPos(vp->WorkPos);
        ImGui::SetNextWindowSize(vp->WorkSize);
        ImGui::Begin("##main", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
            ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoBringToFrontOnFocus);
        DrawMenu(app);

        if (!app.project.IsOpen()) {
            ImGui::Spacing();
            ImGui::Text("Open a map's folder: the one with its .zone file (enh_test for enh_test.zone).");
            if (ImGui::Button("Open map folder...")) OpenMapDialog(app);
            for (const fs::path& p : std::vector<fs::path>{ GetSettings().recentMaps }) {
                if (ImGui::Selectable(ToUtf8(p).c_str())) app.project.Open(p);
            }
            ImGui::Separator();
            DrawLogPanel();
            ImGui::End();
            DrawSettings(app);
            return;
        }

        DrawToolbar(app);
        if (!app.project.error.empty()) {
            ImGui::TextColored(ImVec4(1, 0.45f, 0.4f, 1), "%s", app.project.error.c_str());
        }

        // the panels over the log, the log's height dragged by the bar between
        static float logHeight{ 220 };
        float contentHeight{ std::max(200.0f, ImGui::GetContentRegionAvail().y - logHeight - 8) };
        ImGui::BeginChild("##content", ImVec2(0, contentHeight));
        if (ImGui::BeginTabBar("##tabs")) {
            if (ImGui::BeginTabItem("Zone")) {
                DrawZonePanel(app);
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Folder")) {
                DrawFolderPanel(app);
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("3D view")) {
                DrawViewportPanel(app);
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Lighting & sky")) {
                DrawLightingPanel(app);
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }
        ImGui::EndChild();

        ImGui::InvisibleButton("##splitter", ImVec2(-1, 8));
        if (ImGui::IsItemHovered() || ImGui::IsItemActive()) ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
        if (ImGui::IsItemActive()) logHeight = std::clamp(logHeight - ImGui::GetIO().MouseDelta.y, 60.0f, vp->WorkSize.y - 260);
        ImGui::BeginChild("##logarea", ImVec2(0, 0));
        DrawLogPanel();
        ImGui::EndChild();

        ImGui::End();
        DrawSettings(app);
    }
}
