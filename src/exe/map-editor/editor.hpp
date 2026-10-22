#pragma once
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <d3d11.h>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>
#include <nlohmann/json.hpp>
#include <imgui.h>

namespace editor {
    namespace fs = std::filesystem;
    using json = nlohmann::ordered_json;

    // settings.cpp: the editor's paths, acts-map-editor.json next to the exe
    struct Settings {
        fs::path mapDir{};  // the map's project folder, its <name>.zone in it
        fs::path acts{};    // acts.exe, the one next to the editor by default
        fs::path gameDir{}; // Black Ops 4's folder, project-bo4 in it
        fs::path stageDir{}; // the links before a deploy, <stage>/<zone name>
        std::vector<fs::path> recentMaps{};

        void Load();
        void Save() const;
        fs::path StageFor(const std::string& zone) const;
    };
    Settings& GetSettings();
    fs::path ExeDir();
    std::string ToUtf8(const fs::path& path);
    fs::path FromUtf8(const std::string& path);

    // the log: the editor's messages and the acts runs' output
    enum class LogLevel { INFO, WARN, ERR, PROCESS };
    void Log(LogLevel level, const std::string& line);
    void SetLogFile(const fs::path& path); // the lines written there too
    template<typename... Args>
    void Logf(LogLevel level, const char* fmt, Args... args) {
        char buff[2048];
        snprintf(buff, sizeof(buff), fmt, args...);
        Log(level, buff);
    }

    // zone.cpp: a zone file and the map's project folder
    enum class EntryState { FILE, MISSING, NAME };
    struct ZoneEntry {
        std::string type{};
        std::string value{};
        std::string options{};
        size_t line{};
        fs::path file{};
        EntryState state{};
    };
    struct ZoneBlock {
        std::string name{};
        size_t line{};
        bool marked{}; // between a "// <name>: begin" and "// <name>: end"
        std::vector<ZoneEntry> entries{};
    };
    struct ZoneFile {
        fs::path path{};
        std::vector<std::pair<std::string, std::string>> options{};
        std::vector<ZoneBlock> blocks{};
        std::map<std::string, size_t> typeCounts{};
        size_t entries{};
        size_t missing{};

        bool Load(const fs::path& path, const fs::path& mapDir, std::string& error);
        std::string Option(const char* key) const;
    };
    struct Project {
        fs::path dir{};
        std::vector<fs::path> zones{}; // its .zone files
        size_t zoneIndex{};
        ZoneFile zone{};
        std::string error{};
        size_t generation{}; // counts the reloads: what was built from the zone builds again

        bool Open(const fs::path& dir);
        void Reload();
        bool IsOpen() const { return !dir.empty(); }
        std::string Name() const;     // the folder's name
        std::string ZoneName() const; // the zone's >name, the fastfile's name
    };

    // process.cpp: a child process, its output read on a thread
    class Process {
    public:
        ~Process();
        bool Start(const std::wstring& cmdline, const fs::path& cwd);
        bool Running();
        int ExitCode() const { return exitCode; }
        void Kill();
    private:
        void Close();
        HANDLE process{};
        HANDLE readPipe{};
        std::thread reader{};
        int exitCode{};
    };
    bool IsProcessRunning(const wchar_t* exeName);

    // textures.cpp: image files in shader resource views, kept by path
    struct Texture {
        ID3D11ShaderResourceView* srv{};
        int width{};
        int height{};
        std::string error{};
    };
    void SetDevice(ID3D11Device* device);
    const Texture& GetTexture(const fs::path& path);
    void ClearTextures();
    bool LoadImagePixels(const fs::path& path, std::vector<uint8_t>& rgba, int& width, int& height);
    bool WritePng(const fs::path& path, int width, int height, const uint8_t* rgba);

    // viewport.cpp: the 3D view (its own render target, drawn into the panel)
    struct App;
    void SetViewportContext(ID3D11Device* device, ID3D11DeviceContext* context);
    void ShutdownViewport();
    bool RenderToFile(App& app, const fs::path& out, int width, int height, int view); // --render

    // build.cpp: link the zone with acts into the stage, deploy the stage to the game's project-bo4
    struct Build {
        enum class Step { IDLE, LINKING, DEPLOYING };
        Process process{};
        Step step{};
        bool deployAfter{};
        std::string zoneName{};
        fs::path stage{};
        std::thread worker{};
        std::atomic<bool> workerDone{};
        std::chrono::steady_clock::time_point started{};

        ~Build();
        void Link(Project& project, bool deploy);
        void Deploy(Project& project);
        void Update(Project& project);
        bool Busy() const { return step != Step::IDLE; }
        float Seconds() const;
    };

    // lighting.cpp: maps/lighting.json and the lighting states' known values
    struct LightingState {
        fs::path path{};
        std::vector<uint8_t> data{};
        bool dirty{};
    };
    struct Lighting {
        fs::path path{};
        json root{};
        std::vector<LightingState> states{};
        std::string error{};
        bool dirty{};
        bool loaded{};

        void Load(Project& project);
        bool Save();
        LightingState* State(size_t i) { return i < states.size() ? &states[i] : nullptr; }
    };

    // the panels (ui_*.cpp)
    struct App {
        Project project{};
        Build build{};
        Lighting lighting{};
        bool showSettings{};
        bool quit{};
    };
    App& GetApp();
    void DrawApp(); // the whole window, each frame
    void DrawZonePanel(App& app);
    void DrawFolderPanel(App& app);
    void DrawLightingPanel(App& app);
    void DrawViewportPanel(App& app);
    void DrawLogPanel();
    void DrawPreview(const fs::path& file);
    void SelectFile(const fs::path& file);
    const fs::path& SelectedFile();
    bool PickFolder(fs::path& out, const wchar_t* title);
    void OpenInShell(const fs::path& path);
    void ShowInExplorer(const fs::path& path);
    void HelpMarker(const char* text);
}
