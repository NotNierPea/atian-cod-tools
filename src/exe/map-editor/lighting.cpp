#include "editor.hpp"
#include <fstream>
#include <set>

namespace editor {
    namespace {
        // the files backed up this run (<file>.bak, once, before the first save over them)
        std::set<fs::path> backedUp{};

        bool ReadFile(const fs::path& p, std::vector<uint8_t>& out) {
            std::ifstream is{ p, std::ios::binary };
            if (!is) return false;
            out.assign(std::istreambuf_iterator<char>{ is }, {});
            return true;
        }

        bool WriteFile(const fs::path& p, const void* data, size_t size) {
            std::error_code ec;
            if (!backedUp.contains(p) && fs::exists(p, ec)) {
                fs::copy_file(p, fs::path{ p }.concat(".bak"), fs::copy_options::overwrite_existing, ec);
                backedUp.insert(p);
            }
            std::ofstream os{ p, std::ios::binary };
            if (!os) {
                Logf(LogLevel::ERR, "can't write %s", ToUtf8(p).c_str());
                return false;
            }
            os.write((const char*)data, (std::streamsize)size);
            return true;
        }

        // the zone's gfx_map names the lighting json, from the map's folder
        fs::path FindLightingJson(Project& project) {
            for (const ZoneBlock& b : project.zone.blocks) {
                for (const ZoneEntry& e : b.entries) {
                    if (e.type != "gfx_map" || e.state != EntryState::FILE) continue;
                    std::ifstream is{ e.file };
                    json gfx{ json::parse(is, nullptr, false) };
                    if (gfx.is_object() && gfx.contains("lighting") && gfx["lighting"].is_string()) {
                        return (project.dir / FromUtf8(gfx["lighting"].get<std::string>())).lexically_normal();
                    }
                }
            }
            return project.dir / "maps" / "lighting.json";
        }
    }

    void Lighting::Load(Project& project) {
        *this = {};
        loaded = true;
        path = FindLightingJson(project);
        std::ifstream is{ path };
        if (!is) {
            error = "no lighting json: " + ToUtf8(path) + " (the gfx_map's \"lighting\")";
            return;
        }
        root = json::parse(is, nullptr, false);
        if (!root.is_object()) {
            error = "not valid json: " + ToUtf8(path);
            root = {};
            return;
        }
        // its states, once each, from the json's folder
        if (root.contains("states") && root["states"].is_array()) {
            std::set<fs::path> seen{};
            for (const json& s : root["states"]) {
                if (!s.is_string()) continue;
                fs::path p{ (path.parent_path() / FromUtf8(s.get<std::string>())).lexically_normal() };
                if (!seen.insert(p).second) continue;
                LightingState state{};
                state.path = p;
                if (!ReadFile(p, state.data)) {
                    Logf(LogLevel::WARN, "can't read the lighting state %s", ToUtf8(p).c_str());
                }
                states.push_back(std::move(state));
            }
        }
        Logf(LogLevel::INFO, "lighting: %s, %zu state file(s)", ToUtf8(path).c_str(), states.size());
    }

    bool Lighting::Save() {
        if (root.is_null()) {
            return false;
        }
        std::string text{ root.dump(4) };
        bool ok{ WriteFile(path, text.data(), text.size()) };
        for (LightingState& s : states) {
            if (s.dirty && !s.data.empty()) {
                ok = WriteFile(s.path, s.data.data(), s.data.size()) && ok;
                s.dirty = false;
            }
        }
        if (ok) {
            dirty = false;
            Logf(LogLevel::INFO, "saved the lighting (%s), a .bak of each file kept", ToUtf8(path.filename()).c_str());
        }
        return ok;
    }
}
