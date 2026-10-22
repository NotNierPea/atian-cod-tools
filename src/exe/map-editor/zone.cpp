#include "editor.hpp"
#include <algorithm>
#include <fstream>

namespace editor {
    namespace {
        std::string Trim(const std::string& s) {
            size_t b{ s.find_first_not_of(" \t\r\n") };
            if (b == std::string::npos) return {};
            size_t e{ s.find_last_not_of(" \t\r\n") };
            return s.substr(b, e - b + 1);
        }

        // "// <name>: begin ..." / "// <name>: end": a block a tool writes, its name, or nothing
        std::optional<std::string> Marker(const std::string& comment, const char* word) {
            std::string tag{ std::string{ ": " } + word };
            size_t at{ comment.find(tag) };
            if (at == std::string::npos) return std::nullopt;
            size_t after{ at + tag.size() };
            if (after < comment.size() && comment[after] != ' ' && comment[after] != '(') return std::nullopt;
            return Trim(comment.substr(0, at));
        }

        // the folders acts looks a type's names up in when they aren't paths from the zone's folder
        const std::map<std::string, std::vector<const char*>> TYPE_DIRS{
            { "image", { "images" } },
            { "material", { "materials" } },
            { "scriptbundle", { "scriptbundle" } },
            { "xmodel", { "xmodels", "models" } },
            { "xanim", { "anims" } },
            { "fx", { "fx" } },
        };

        void Resolve(ZoneEntry& entry, const fs::path& mapDir) {
            std::error_code ec;
            fs::path value{ FromUtf8(entry.value) };
            auto tryFile = [&](const fs::path& p) {
                if (fs::is_regular_file(p, ec)) {
                    entry.file = p.lexically_normal();
                    entry.state = EntryState::FILE;
                    return true;
                }
                return false;
            };
            if (tryFile(mapDir / value)) return;
            if (auto it{ TYPE_DIRS.find(entry.type) }; it != TYPE_DIRS.end()) {
                for (const char* dir : it->second) {
                    if (tryFile(mapDir / dir / value)) return;
                    if (!value.has_extension() && tryFile(mapDir / dir / fs::path{ value }.concat(".json"))) return;
                }
            }
            // a path with an extension that isn't there is missing, the rest are names (hashes, other zones' assets)
            entry.state = value.has_extension() ? EntryState::MISSING : EntryState::NAME;
        }
    }

    bool ZoneFile::Load(const fs::path& file, const fs::path& mapDir, std::string& error) {
        *this = {};
        path = file;
        std::ifstream is{ file };
        if (!is) {
            error = "can't read " + ToUtf8(file);
            return false;
        }

        ZoneBlock current{ "(start)", 0, false, {} };
        std::string lastComment{};
        size_t lastCommentLine{};
        std::string line{};
        size_t lineNum{};

        auto flush = [&]() {
            if (!current.entries.empty()) {
                blocks.push_back(std::move(current));
            }
            current = {};
        };

        while (std::getline(is, line)) {
            lineNum++;
            std::string t{ Trim(line) };
            if (t.empty()) {
                continue;
            }
            if (t.rfind("//", 0) == 0) {
                std::string comment{ Trim(t.substr(2)) };
                if (auto begin{ Marker(comment, "begin") }) {
                    flush();
                    current = { *begin, lineNum, true, {} };
                    continue;
                }
                if (Marker(comment, "end")) {
                    flush();
                    current = { "", lineNum, false, {} };
                    lastComment.clear();
                    continue;
                }
                // the first comment line over entries names them when no tool's block holds them
                if (!current.marked && (lastComment.empty() || lastCommentLine + 1 != lineNum)) {
                    lastComment = comment;
                }
                lastCommentLine = lineNum;
                continue;
            }
            if (t[0] == '>') {
                size_t eq{ t.find('=') };
                options.emplace_back(t.substr(1, eq == std::string::npos ? std::string::npos : eq - 1),
                    eq == std::string::npos ? "" : t.substr(eq + 1));
                continue;
            }

            size_t comma{ t.find(',') };
            if (comma == std::string::npos) {
                continue;
            }
            ZoneEntry entry{};
            entry.type = Trim(t.substr(0, comma));
            std::string rest{ t.substr(comma + 1) };
            size_t comma2{ rest.find(',') };
            entry.value = Trim(rest.substr(0, comma2));
            if (comma2 != std::string::npos) {
                entry.options = Trim(rest.substr(comma2 + 1));
            }
            entry.line = lineNum;
            Resolve(entry, mapDir);

            if (!current.marked && !lastComment.empty() && current.name != lastComment) {
                flush();
                current = { lastComment, lineNum, false, {} };
            }
            else if (!current.marked && current.name.empty()) {
                current.name = "(no comment)";
                current.line = lineNum;
            }
            typeCounts[entry.type]++;
            entries++;
            if (entry.state == EntryState::MISSING) {
                missing++;
            }
            current.entries.push_back(std::move(entry));
        }
        flush();
        return true;
    }

    std::string ZoneFile::Option(const char* key) const {
        for (const auto& [k, v] : options) {
            if (k == key) return v;
        }
        return {};
    }

    bool Project::Open(const fs::path& folder) {
        std::error_code ec;
        if (!fs::is_directory(folder, ec)) {
            error = "not a folder: " + ToUtf8(folder);
            Log(LogLevel::ERR, error);
            return false;
        }
        dir = folder;
        zones.clear();
        for (const fs::directory_entry& e : fs::directory_iterator{ folder, ec }) {
            if (e.is_regular_file() && e.path().extension() == ".zone") {
                zones.push_back(e.path());
            }
        }
        // the folder's own zone first
        std::sort(zones.begin(), zones.end(), [&](const fs::path& a, const fs::path& b) {
            bool am{ a.stem() == folder.filename() };
            bool bm{ b.stem() == folder.filename() };
            return am != bm ? am : a < b;
        });
        zoneIndex = 0;

        Settings& settings{ GetSettings() };
        settings.mapDir = folder;
        auto& recent{ settings.recentMaps };
        recent.erase(std::remove(recent.begin(), recent.end(), folder), recent.end());
        recent.insert(recent.begin(), folder);
        if (recent.size() > 8) recent.resize(8);
        settings.Save();

        Reload();
        return true;
    }

    void Project::Reload() {
        error.clear();
        generation++;
        if (zones.empty()) {
            zone = {};
            error = "no .zone file in " + ToUtf8(dir);
            Log(LogLevel::WARN, error);
            return;
        }
        zoneIndex = std::min(zoneIndex, zones.size() - 1);
        if (!zone.Load(zones[zoneIndex], dir, error)) {
            Log(LogLevel::ERR, error);
            return;
        }
        Logf(LogLevel::INFO, "%s: %zu entries in %zu blocks, %zu missing", ToUtf8(zones[zoneIndex].filename()).c_str(),
            zone.entries, zone.blocks.size(), zone.missing);
    }

    std::string Project::Name() const { return ToUtf8(dir.filename()); }

    std::string Project::ZoneName() const {
        std::string name{ zone.Option("name") };
        return name.empty() && !zones.empty() ? ToUtf8(zones[zoneIndex].stem()) : name;
    }
}
