#include "editor.hpp"
#include <fstream>

namespace editor {
    namespace {
        fs::path SettingsPath() { return ExeDir() / "acts-map-editor.json"; }

        fs::path ReadPath(const json& root, const char* key) {
            auto it{ root.find(key) };
            return it != root.end() && it->is_string() ? FromUtf8(it->get<std::string>()) : fs::path{};
        }
    }

    fs::path ExeDir() {
        wchar_t buff[MAX_PATH]{};
        GetModuleFileNameW(nullptr, buff, MAX_PATH);
        return fs::path{ buff }.parent_path();
    }

    std::string ToUtf8(const fs::path& path) {
        std::u8string s{ path.u8string() };
        return std::string{ s.begin(), s.end() };
    }

    fs::path FromUtf8(const std::string& path) {
        return fs::path{ std::u8string{ path.begin(), path.end() } };
    }

    Settings& GetSettings() {
        static Settings settings{};
        return settings;
    }

    fs::path Settings::StageFor(const std::string& zone) const {
        return (stageDir.empty() ? ExeDir() / "map-editor-stage" : stageDir) / zone;
    }

    void Settings::Load() {
        // defaults: the acts.exe built next to the editor, the game's usual folder
        acts = ExeDir() / "acts.exe";
        const fs::path game{ L"C:\\Program Files (x86)\\Call of Duty Black Ops 4" };
        std::error_code ec;
        if (fs::exists(game, ec)) {
            gameDir = game;
        }

        std::ifstream is{ SettingsPath() };
        if (!is) {
            return;
        }
        json root{ json::parse(is, nullptr, false) };
        if (!root.is_object()) {
            Log(LogLevel::WARN, "acts-map-editor.json isn't valid json, defaults used");
            return;
        }
        if (fs::path p{ ReadPath(root, "mapDir") }; !p.empty()) mapDir = p;
        if (fs::path p{ ReadPath(root, "acts") }; !p.empty()) acts = p;
        if (fs::path p{ ReadPath(root, "gameDir") }; !p.empty()) gameDir = p;
        if (fs::path p{ ReadPath(root, "stageDir") }; !p.empty()) stageDir = p;
        if (auto it{ root.find("python") }; it != root.end() && it->is_string() && !it->get<std::string>().empty()) python = it->get<std::string>();
        if (auto it{ root.find("recentMaps") }; it != root.end() && it->is_array()) {
            for (const json& r : *it) {
                if (r.is_string()) recentMaps.push_back(FromUtf8(r.get<std::string>()));
            }
        }
    }

    void Settings::Save() const {
        json root{};
        root["mapDir"] = ToUtf8(mapDir);
        root["acts"] = ToUtf8(acts);
        root["gameDir"] = ToUtf8(gameDir);
        root["stageDir"] = ToUtf8(stageDir);
        root["python"] = python;
        json recent{ json::array() };
        for (const fs::path& r : recentMaps) {
            recent.push_back(ToUtf8(r));
        }
        root["recentMaps"] = recent;
        std::ofstream os{ SettingsPath() };
        if (!os) {
            Log(LogLevel::ERR, "can't write acts-map-editor.json");
            return;
        }
        os << root.dump(4);
    }
}
