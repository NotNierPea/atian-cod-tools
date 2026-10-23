#include "scene.hpp"
#include <array>
#include <cmath>
#include <fstream>
#include <regex>
#include <set>
#include <sstream>

/*
 * Moving objects: where each is saved. A map's editor_bindings.json (maps/ or its folder) binds objects to the json its
 * tools build them from, so a move survives the tools running again:
 *
 * {
 *     "generated": { "maps/entitylist.json": "why a direct edit there is lost" },
 *     "run": { "room": [ "tools/build_room.py", "tools/build_crafting.py" ] },
 *     "bindings": [
 *         { "kind": "entity", "match": { "targetname": "enh_perk_statue_0" },
 *           "targets": [ { "file": "maps/room.json", "path": [ "perks", 0 ], "origin": "xy", "yaw": "yaw" } ],
 *           "run": "room" },
 *         { "kind": "static", "match": { "model": "enh_ferriswheel" }, "direct": true,
 *           "targets": [ { "file": "maps/placements.json", "path": [ "ferriswheel" ], "origin": "xyz" } ],
 *           "gsc": [ { "file": "scripts/zm/enh_test.gsc", "function": "ferris_wheel_layout", "field": "origin" } ] },
 *         { "kind": "static", "match": { "model": "enh_room_walls" }, "locked": "why it can't move" }
 *     ]
 * }
 *
 * A target's "origin" is "xy" (z left as it is) or "xyz", in its "originKey" ("origin"; "" the object is the array),
 * "yaw" its yaw number's key, "angles" its [pitch, yaw, roll] key. "index" picks the n-th object matching. An object
 * without a binding is saved into its own file (gfx_map.json's staticModels, entitylist.json's entities).
 *
 * "config": [ { "file": "maps/room.json", "path": [ "windows", 0 ], "keys": [ ... ], "run": "room", "note": "..." } ]
 * adds to what the 3D view's config panel shows and edits for the object (a locked object's real source): "keys" only
 * those keys of the object, "run" what runs after saving it (the binding's "run" by default).
 *
 * A relative target writes changes of numbers instead (along / across its wall, zKey, scale; "yawKey" the turn's, the
 * object alone turning); with "origin" / "yaw" too its own place is written as well (a stone moved, turned and grown).
 * "each": { "file": "maps/room.json", "path": [ "stones" ] } makes a binding per element of that list, "{n}" in its
 * strings the index (a number when it's the whole string), "{key}" the element's value (a window's "{name}").
 * "duplicate": { "file", "path", "select": "enh_stone_{n}" }: Ctrl+D adds a stone of the object's box to that list
 * ({ origin (bottom middle), yaw, width, depth, height }), "select" the copy's label once rebuilt; "removable": Delete
 * takes the first target out of its list. Without a binding Ctrl+D copies the object's own gfx_map / entitylist entry.
 */
namespace editor::scene {
    namespace {
        std::set<fs::path> backedUp{};

        json ReadJsonFile(const fs::path& p) {
            std::ifstream is{ p };
            return is ? json::parse(is, nullptr, false) : json{};
        }

        bool Backup(const fs::path& p) {
            std::error_code ec;
            if (!backedUp.contains(p) && fs::exists(p, ec)) {
                fs::copy_file(p, fs::path{ p }.concat(".bak"), fs::copy_options::overwrite_existing, ec);
                backedUp.insert(p);
            }
            return true;
        }

        bool WriteText(const fs::path& p, const std::string& text) {
            Backup(p);
            std::ofstream os{ p, std::ios::binary };
            if (!os) return false;
            os.write(text.data(), (std::streamsize)text.size());
            return true;
        }

        // a coordinate as the tools write them: whole numbers whole, else 2 decimals
        json Num(float v) {
            double r{ std::round(v * 100.0) / 100.0 };
            if (std::abs(r - std::round(r)) < 1e-6) return (int64_t)std::llround(r);
            return r;
        }

        // the value at a path of keys / indices, made when missing
        json* Walk(json& root, const json& path) {
            json* at{ &root };
            if (!path.is_array()) return at;
            for (const json& key : path) {
                if (key.is_string()) {
                    if (!at->is_object()) return nullptr;
                    at = &(*at)[key.get<std::string>()];
                }
                else if (key.is_number_integer()) {
                    size_t i{ key.get<size_t>() };
                    if (!at->is_array() || i >= at->size()) return nullptr;
                    at = &(*at)[i];
                }
                else return nullptr;
            }
            return at;
        }

        std::string PathText(const json& path) {
            std::string s{};
            for (const json& k : path) s += k.is_string() ? "." + k.get<std::string>() : "[" + k.dump() + "]";
            return s;
        }

        bool Matches(const Binding& b, const Instance& inst) {
            if (b.kind == "static" && inst.kind != Kind::STATIC_MODEL) return false;
            if (b.kind == "entity" && inst.kind == Kind::STATIC_MODEL) return false;
            if (!b.match.is_object()) return false;
            for (auto& [k, v] : b.match.items()) {
                auto it{ inst.data.find(k) };
                if (it == inst.data.end()) return false;
                if (v.is_string() && v.get<std::string>() == "*") continue;
                if (*it != v && it->dump() != v.dump()) return false;
            }
            return true;
        }

        std::string StrOf(const json& j, const char* key) {
            auto it{ j.find(key) };
            return it != j.end() && it->is_string() ? it->get<std::string>() : std::string{};
        }

        float NumOr(const json& obj, const std::string& key, float def) {
            auto it{ obj.find(key) };
            return it != obj.end() && it->is_number() ? it->get<float>() : def;
        }

        // a turn, -180 to 180
        float Turn(float d) {
            d = std::fmod(d, 360.0f);
            return d > 180 ? d - 360 : d <= -180 ? d + 360 : d;
        }

        // an "each" binding for one element of its list: "{n}" its index, "{key}" the element's values
        json Substitute(const json& j, size_t n, const json& element) {
            if (j.is_string()) {
                const std::string& s{ j.get_ref<const std::string&>() };
                if (s == "{n}") return n;
                std::string out{};
                for (size_t i = 0; i < s.size(); i++) {
                    size_t close{ s[i] == '{' ? s.find('}', i) : std::string::npos };
                    std::string key{ close == std::string::npos ? "" : s.substr(i + 1, close - i - 1) };
                    auto it{ element.is_object() && !key.empty() ? element.find(key) : element.end() };
                    if (key == "n") out += std::to_string(n);
                    else if (element.is_object() && it != element.end() && it->is_string()) out += it->get<std::string>();
                    else if (element.is_object() && it != element.end() && it->is_number()) out += it->dump();
                    else {
                        out += s[i];
                        continue;
                    }
                    i = close;
                }
                return out;
            }
            json out{ j };
            if (out.is_array() || out.is_object()) {
                for (auto it{ out.begin() }; it != out.end(); ++it) *it = Substitute(*it, n, element);
            }
            return out;
        }

        // a relative target's object read when the bindings load: along's axis from its wall, the grown sizes
        void ResolveRelative(Target& t) {
            json root{ ReadJsonFile(t.file) };
            const json* obj{ root.is_null() || root.is_discarded() ? nullptr : Walk(root, t.path) };
            if (!obj || !obj->is_object()) {
                Logf(LogLevel::WARN, "3d view: bindings: no %s in %s, its moves won't save", PathText(t.path).c_str(), ToUtf8(t.file.filename()).c_str());
                return;
            }
            std::string wall{ StrOf(*obj, t.wallKey.c_str()) };
            t.axis = wall == "south" || wall == "north" ? 0 : wall == "west" || wall == "east" ? 1 : -1;
            // the scale numbers by world axis: along / across the wall, or x / y / z as written
            if (t.scaleRoles.is_object()) {
                auto role = [&](const char* name) { return StrOf(t.scaleRoles, name); };
                if (t.axis >= 0) {
                    if (!role("along").empty()) t.scaleKey[t.axis] = role("along");
                    if (!role("across").empty()) t.scaleKey[1 - t.axis] = role("across");
                }
                for (int k = 0; k < 3; k++) {
                    const char* names[3]{ "x", "y", "z" };
                    if (!role(names[k]).empty()) t.scaleKey[k] = role(names[k]);
                }
            }
        }

        // "s.<field> = ( x, y, z );" in the function's body
        bool PatchGsc(std::string& text, const std::string& function, const std::string& field, const float v[3]) {
            size_t at{ text.find("function " + function + "(") };
            if (at == std::string::npos) return false;
            size_t end{ text.find("\n}", at) };
            std::string key{ "s." + field + " =" };
            size_t line{ text.find(key, at) };
            if (line == std::string::npos || (end != std::string::npos && line > end)) return false;
            size_t eol{ text.find(';', line) };
            if (eol == std::string::npos) return false;
            char buff[160];
            snprintf(buff, sizeof(buff), "s.%s = ( %g, %g, %g )", field.c_str(), std::round(v[0] * 100) / 100, std::round(v[1] * 100) / 100, std::round(v[2] * 100) / 100);
            text.replace(line, eol - line, buff);
            return true;
        }
    }

    namespace {
        fs::path FindBindingsFile(const Project& project) {
            std::error_code ec;
            for (const fs::path& p : { project.dir / "maps" / "editor_bindings.json", project.dir / "editor_bindings.json" }) {
                if (fs::is_regular_file(p, ec)) return p;
            }
            return {};
        }

        // a gsc layout function's vectors: "s.<field> = ( x, y, z );" and "s.<field> = array( ( ... ), ... );"
        using Layout = std::map<std::string, std::vector<std::array<float, 3>>>;
        Layout ParseLayout(const fs::path& gsc, const std::string& function) {
            Layout out{};
            std::ifstream is{ gsc, std::ios::binary };
            std::stringstream ss;
            ss << is.rdbuf();
            std::string text{ ss.str() };
            size_t at{ text.find("function " + function + "(") };
            if (at == std::string::npos) return out;
            size_t end{ text.find("\n}", at) };
            std::string body{ text.substr(at, end == std::string::npos ? std::string::npos : end - at) };
            static const std::regex field{ R"(s\.(\w+)\s*=\s*(array\s*\(.*?\)\s*|\([^;]*?\))\s*;)" };
            static const std::regex vec{ R"(\(\s*([-+0-9.eE]+)\s*,\s*([-+0-9.eE]+)\s*,\s*([-+0-9.eE]+)\s*\))" };
            for (std::sregex_iterator it{ body.begin(), body.end(), field }, last{}; it != last; ++it) {
                std::string value{ (*it)[2].str() };
                std::vector<std::array<float, 3>> vs{};
                for (std::sregex_iterator v{ value.begin(), value.end(), vec }; v != last; ++v) {
                    vs.push_back({ std::stof((*v)[1].str()), std::stof((*v)[2].str()), std::stof((*v)[3].str()) });
                }
                if (!vs.empty()) out[(*it)[1].str()] = vs;
            }
            return out;
        }

        // "field" or "field[i]" of a layout
        bool LayoutVec(const Layout& layout, const std::string& term, float out[3]) {
            std::string name{ term };
            size_t index{};
            if (size_t b{ term.find('[') }; b != std::string::npos) {
                name = term.substr(0, b);
                index = (size_t)std::atoi(term.c_str() + b + 1);
            }
            auto it{ layout.find(name) };
            if (it == layout.end() || index >= it->second.size()) return false;
            std::copy(it->second[index].begin(), it->second[index].end(), out);
            return true;
        }
    }

    /*
     * "scriptModels": [ { "model": "enh_sawblade_trap", "gsc": "scripts/zm/enh_test.gsc", "function": "sawblade_layout",
     *     "origin": [ "origin" ], "angles": "angles" }, { "model": "enh_ending_orb", "entity": { "targetname": "enh_ending" },
     *     "offset": [ 0, 0, 56 ] } ]: "origin" a sum of the layout's fields ("carts[2]" an array's), "offset" added, "angles"
     * a field or [ p, y, r ]; with "entity" the first matching entity's origin and angles. Shown, not moved: they follow
     * what they're built from ("note" says what).
     */
    void Scene::AddScriptModels(const Project& project, const std::function<int(const std::string&, fs::path&)>& meshFor) {
        fs::path file{ FindBindingsFile(project) };
        if (file.empty()) return;
        json root{ ReadJsonFile(file) };
        if (!root.is_object() || !root.contains("scriptModels") || !root["scriptModels"].is_array()) return;
        std::map<std::pair<fs::path, std::string>, Layout> layouts{};
        size_t added{};
        for (const json& spec : root["scriptModels"]) {
            Instance inst{};
            inst.kind = Kind::SPAWNED;
            inst.name = StrOf(spec, "model");
            if (inst.name.empty()) continue;
            std::string where{};
            if (spec.contains("entity") && spec["entity"].is_object()) {
                Binding probe{};
                probe.match = spec["entity"];
                const Instance* anchor{};
                for (const Instance& e : instances) {
                    if (e.kind != Kind::STATIC_MODEL && Matches(probe, e)) {
                        anchor = &e;
                        break;
                    }
                }
                if (!anchor) {
                    Logf(LogLevel::WARN, "3d view: script model %s: no entity %s", inst.name.c_str(), spec["entity"].dump().c_str());
                    continue;
                }
                std::copy(anchor->origin, anchor->origin + 3, inst.origin);
                std::copy(anchor->angles, anchor->angles + 3, inst.angles);
                inst.source = anchor->source;
                where = "at " + anchor->label;
            }
            else {
                fs::path gsc{ (project.dir / FromUtf8(StrOf(spec, "gsc"))).lexically_normal() };
                std::string function{ StrOf(spec, "function") };
                auto key{ std::make_pair(gsc, function) };
                auto lit{ layouts.find(key) };
                if (lit == layouts.end()) lit = layouts.emplace(key, ParseLayout(gsc, function)).first;
                const Layout& layout{ lit->second };
                bool ok{ true };
                if (spec.contains("origin") && spec["origin"].is_array()) {
                    for (const json& term : spec["origin"]) {
                        float v[3]{};
                        if (!term.is_string() || !LayoutVec(layout, term.get<std::string>(), v)) {
                            ok = false;
                            break;
                        }
                        for (int k = 0; k < 3; k++) inst.origin[k] += v[k];
                    }
                }
                if (spec.contains("angles") && spec["angles"].is_string()) LayoutVec(layout, spec["angles"].get<std::string>(), inst.angles);
                if (!ok) {
                    Logf(LogLevel::WARN, "3d view: script model %s: %s() hasn't its origin's fields", inst.name.c_str(), function.c_str());
                    continue;
                }
                inst.source = gsc;
                where = "from " + function + "()";
            }
            if (spec.contains("offset") && spec["offset"].is_array() && spec["offset"].size() >= 3) {
                for (int k = 0; k < 3; k++) inst.origin[k] += spec["offset"][k].is_number() ? spec["offset"][k].get<float>() : 0;
            }
            if (spec.contains("angles") && spec["angles"].is_array() && spec["angles"].size() >= 3) {
                for (int k = 0; k < 3; k++) inst.angles[k] = spec["angles"][k].is_number() ? spec["angles"][k].get<float>() : 0;
            }
            inst.label = inst.name + " (map script, " + where + ")";
            std::string note{ StrOf(spec, "note") };
            inst.locked = "spawned by the map script " + where + (note.empty() ? "" : ": " + note);
            inst.mesh = meshFor(inst.name, inst.model);
            if (inst.mesh >= 0) {
                std::copy(meshes[inst.mesh].mins, meshes[inst.mesh].mins + 3, inst.mins);
                std::copy(meshes[inst.mesh].maxs, meshes[inst.mesh].maxs + 3, inst.maxs);
            }
            else {
                for (int k = 0; k < 3; k++) {
                    inst.mins[k] = k == 2 ? 0 : -8;
                    inst.maxs[k] = k == 2 ? 16 : 8;
                }
            }
            inst.data = spec;
            instances.push_back(std::move(inst));
            added++;
        }
        Logf(LogLevel::INFO, "3d view: %zu script spawned model(s)", added);
    }

    bool Binding::Rotates() const {
        if (!locked.empty()) return false;
        if (direct) return true;
        for (const Target& t : targets) if (!t.yaw.empty() || !t.angles.empty()) return true;
        return false;
    }

    bool Binding::FullAngles() const {
        if (direct) return true;
        for (const Target& t : targets) if (!t.angles.empty()) return true;
        return false;
    }

    bool Binding::MovesZ() const {
        if (direct) return true;
        for (const Target& t : targets) if (t.origin == "xyz" || !t.zKey.empty()) return true;
        return false;
    }

    const Target* Binding::Group() const {
        if (!locked.empty()) return nullptr;
        for (const Target& t : targets) if (t.Relative()) return &t;
        return nullptr;
    }

    void Scene::LoadBindings(const Project& project) {
        bindings.clear();
        generated.clear();
        runGroups.clear();
        bindingsFile.clear();
        std::error_code ec;
        for (const fs::path& p : { project.dir / "maps" / "editor_bindings.json", project.dir / "editor_bindings.json" }) {
            if (fs::is_regular_file(p, ec)) {
                bindingsFile = p;
                break;
            }
        }
        if (!bindingsFile.empty()) {
            json root{ ReadJsonFile(bindingsFile) };
            if (!root.is_object()) {
                Logf(LogLevel::ERR, "%s isn't valid json: every object saves into its own file", ToUtf8(bindingsFile).c_str());
            }
            else {
                if (root.contains("generated") && root["generated"].is_object()) {
                    for (auto& [k, v] : root["generated"].items()) {
                        generated[(project.dir / FromUtf8(k)).lexically_normal()] = v.is_string() ? v.get<std::string>() : "";
                    }
                }
                if (root.contains("run") && root["run"].is_object()) {
                    for (auto& [k, v] : root["run"].items()) {
                        if (!v.is_array()) continue;
                        for (const json& s : v) if (s.is_string()) runGroups[k].push_back(s.get<std::string>());
                    }
                }
                if (root.contains("bindings") && root["bindings"].is_array()) {
                    auto parse = [&](const json& j) {
                        Binding b{};
                        b.kind = StrOf(j, "kind");
                        b.match = j.contains("match") ? j["match"] : json::object();
                        b.index = j.contains("index") && j["index"].is_number_integer() ? j["index"].get<int>() : -1;
                        b.locked = StrOf(j, "locked");
                        b.note = StrOf(j, "note");
                        b.direct = j.contains("direct") && j["direct"].is_boolean() && j["direct"].get<bool>();
                        if (j.contains("targets") && j["targets"].is_array()) {
                            for (const json& t : j["targets"]) {
                                Target target{};
                                target.file = (project.dir / FromUtf8(StrOf(t, "file"))).lexically_normal();
                                target.path = t.contains("path") ? t["path"] : json::array();
                                target.origin = StrOf(t, "origin");
                                if (t.contains("originKey") && t["originKey"].is_string()) target.originKey = t["originKey"].get<std::string>();
                                target.yaw = StrOf(t, "yaw");
                                target.angles = StrOf(t, "angles");
                                target.along = StrOf(t, "along");
                                if (t.contains("wallKey") && t["wallKey"].is_string()) target.wallKey = t["wallKey"].get<std::string>();
                                target.zKey = StrOf(t, "zKey");
                                target.zDefault = t.contains("zDefault") && t["zDefault"].is_number() ? t["zDefault"].get<float>() : 0.0f;
                                target.across = StrOf(t, "across");
                                target.yawKey = StrOf(t, "yawKey");
                                auto strings = [&](const char* key, std::vector<std::string>& out) {
                                    if (!t.contains(key) || !t[key].is_array()) return;
                                    for (const json& f : t[key]) if (f.is_string()) out.push_back(f.get<std::string>());
                                };
                                strings("follow", target.follow);
                                strings("zAlso", target.zAlso);
                                if (t.contains("scale") && t["scale"].is_object()) {
                                    target.scaleRoles = t["scale"];
                                    target.scaleDouble = t["scale"].value("double", false);
                                    target.scaleXY = t["scale"].value("xy", false);
                                }
                                if (t.contains("defaults") && t["defaults"].is_object()) {
                                    for (auto& [k, v] : t["defaults"].items()) if (v.is_number()) target.defaults[k] = v.get<float>();
                                }
                                if (target.Relative() || target.scaleRoles.is_object()) ResolveRelative(target);
                                b.targets.push_back(std::move(target));
                            }
                        }
                        if (j.contains("gsc") && j["gsc"].is_array()) {
                            for (const json& g : j["gsc"]) {
                                b.gsc.push_back({ (project.dir / FromUtf8(StrOf(g, "file"))).lexically_normal(), StrOf(g, "function"), StrOf(g, "field") });
                            }
                        }
                        // a run group's name, a script, or a list of them
                        auto runOf = [&](const json& v) {
                            std::vector<std::string> out{};
                            auto add = [&](const std::string& r) {
                                auto it{ runGroups.find(r) };
                                if (it != runGroups.end()) out.insert(out.end(), it->second.begin(), it->second.end());
                                else out.push_back(r);
                            };
                            if (v.is_string()) add(v.get<std::string>());
                            else if (v.is_array()) {
                                for (const json& r : v) if (r.is_string()) add(r.get<std::string>());
                            }
                            return out;
                        };
                        if (j.contains("run")) b.run = runOf(j["run"]);
                        // what the config panel shows besides the targets (a locked object's real source)
                        if (j.contains("config") && j["config"].is_array()) {
                            for (const json& c : j["config"]) {
                                ConfigEntry e{};
                                e.file = (project.dir / FromUtf8(StrOf(c, "file"))).lexically_normal();
                                e.path = c.contains("path") && c["path"].is_array() ? c["path"] : json::array();
                                if (c.contains("keys") && c["keys"].is_array()) {
                                    for (const json& k : c["keys"]) if (k.is_string()) e.keys.push_back(k.get<std::string>());
                                }
                                e.what = StrOf(c, "note");
                                if (e.what.empty()) e.what = "what the map's tools place it from";
                                e.run = c.contains("run") ? runOf(c["run"]) : b.run;
                                b.config.push_back(std::move(e));
                            }
                        }
                        // Ctrl+D's list, Delete
                        if (j.contains("duplicate") && j["duplicate"].is_object()) {
                            const json& d{ j["duplicate"] };
                            b.copyFile = (project.dir / FromUtf8(StrOf(d, "file"))).lexically_normal();
                            b.copyPath = d.contains("path") && d["path"].is_array() ? d["path"] : json::array();
                            b.copySelect = StrOf(d, "select");
                        }
                        b.removable = j.contains("removable") && j["removable"].is_boolean() && j["removable"].get<bool>();
                        bindings.push_back(std::move(b));
                    };
                    for (const json& j : root["bindings"]) {
                        if (!j.contains("each") || !j["each"].is_object()) {
                            parse(j);
                            continue;
                        }
                        // one per element of its list ("duplicate" kept as written: its "{n}" is the copy's)
                        const json& each{ j["each"] };
                        json listRoot{ ReadJsonFile((project.dir / FromUtf8(StrOf(each, "file"))).lexically_normal()) };
                        json* list{ listRoot.is_object() ? Walk(listRoot, each.contains("path") ? each["path"] : json::array()) : nullptr };
                        if (!list || !list->is_array()) continue;
                        json body{ j };
                        body.erase("each");
                        body.erase("duplicate");
                        for (size_t n = 0; n < list->size(); n++) {
                            json one{ Substitute(body, n, (*list)[n]) };
                            if (j.contains("duplicate")) one["duplicate"] = j["duplicate"];
                            parse(one);
                        }
                    }
                }
            }
        }

        // each object the first binding matching it (its n-th match with "index")
        for (size_t bi = 0; bi < bindings.size(); bi++) {
            int count{};
            for (Instance& inst : instances) {
                if (inst.kind == Kind::SPAWNED || !Matches(bindings[bi], inst)) continue;
                int n{ count++ };
                if (inst.binding >= 0) continue;
                if (bindings[bi].index < 0 || bindings[bi].index == n) inst.binding = (int)bi;
            }
        }
        size_t bound{};
        for (const Instance& inst : instances) if (inst.binding >= 0) bound++;
        if (!bindingsFile.empty()) {
            Logf(LogLevel::INFO, "3d view: %s: %zu bindings, %zu of %zu objects bound", ToUtf8(bindingsFile.filename()).c_str(), bindings.size(), bound, instances.size());
        }
    }

    const Binding* Scene::BindingOf(const Instance& inst) const {
        return inst.binding >= 0 && inst.binding < (int)bindings.size() ? &bindings[inst.binding] : nullptr;
    }

    std::vector<ConfigEntry> Scene::ConfigOf(const Instance& inst) const {
        // a board: its barrier's
        if (inst.board > 0) {
            for (const Instance& b : instances) {
                if (b.board == 0 && b.kind != Kind::SPAWNED && b.source == inst.source && b.sourceIndex == inst.sourceIndex) return ConfigOf(b);
            }
        }
        std::vector<ConfigEntry> out{};
        if (const Binding* b{ BindingOf(inst) }) {
            for (const Target& t : b->targets) out.push_back({ t.file, t.path, "what its tools build it from", b->run });
            out.insert(out.end(), b->config.begin(), b->config.end());
        }
        if (inst.kind == Kind::STATIC_MODEL && inst.sourceIndex >= 0) {
            out.push_back({ inst.source, json::array({ "staticModels", inst.sourceIndex }), "its gfx_map entry", {} });
        }
        else if (inst.kind != Kind::SPAWNED && inst.sourceIndex >= 0) {
            out.push_back({ inst.source, json::array({ "entities", inst.sourceIndex }), "its entity", {} });
        }
        else if (inst.kind == Kind::SPAWNED && inst.board == 0 && !bindingsFile.empty()) {
            // a script model: its "scriptModels" entry
            json root{ ReadJsonFile(bindingsFile) };
            if (root.is_object() && root.contains("scriptModels") && root["scriptModels"].is_array()) {
                for (size_t i = 0; i < root["scriptModels"].size(); i++) {
                    if (root["scriptModels"][i] == inst.data) {
                        out.push_back({ bindingsFile, json::array({ "scriptModels", i }), "how the editor places it", {} });
                        break;
                    }
                }
            }
        }
        return out;
    }

    json ReadConfig(const ConfigEntry& entry, std::string& error) {
        json root{ ReadJsonFile(entry.file) };
        if (root.is_discarded() || root.is_null()) {
            error = "can't read " + ToUtf8(entry.file.filename());
            return {};
        }
        const json* at{ &root };
        for (const json& key : entry.path) {
            if (key.is_string() && at->is_object() && at->contains(key.get<std::string>())) at = &(*at)[key.get<std::string>()];
            else if (key.is_number_integer() && at->is_array() && key.get<size_t>() < at->size()) at = &(*at)[key.get<size_t>()];
            else {
                error = "no " + PathText(entry.path) + " in " + ToUtf8(entry.file.filename());
                return {};
            }
        }
        if (entry.keys.empty() || !at->is_object()) return *at;
        json some{ json::object() };
        for (const std::string& k : entry.keys) {
            if (at->contains(k)) some[k] = (*at)[k];
        }
        return some;
    }

    bool SaveConfig(const ConfigEntry& entry, const json& value, std::string& error) {
        json root{ ReadJsonFile(entry.file) };
        if (root.is_discarded() || root.is_null()) {
            error = "can't read " + ToUtf8(entry.file.filename());
            return false;
        }
        json* at{ Walk(root, entry.path) };
        if (!at) {
            error = "no " + PathText(entry.path) + " in " + ToUtf8(entry.file.filename());
            return false;
        }
        if (entry.keys.empty() || !at->is_object() || !value.is_object()) *at = value;
        else {
            // only its keys: the rest of the object kept
            for (const std::string& k : entry.keys) {
                if (value.contains(k)) (*at)[k] = value[k];
                else at->erase(k);
            }
        }
        if (!WriteText(entry.file, root.dump(4) + "\n")) {
            error = "can't write " + ToUtf8(entry.file);
            return false;
        }
        Logf(LogLevel::INFO, "config saved: %s%s", ToUtf8(entry.file.filename()).c_str(), PathText(entry.path).c_str());
        return true;
    }

    size_t Scene::MovedCount() const {
        // a moved group counts once, spawned models not at all
        size_t n{};
        std::set<std::string> groups{};
        for (const Instance& inst : instances) {
            if (!inst.Moved() || inst.kind == Kind::SPAWNED) continue;
            const Binding* b{ BindingOf(inst) };
            const Target* g{ b ? b->Group() : nullptr };
            if (!g || groups.insert(ToUtf8(g->file) + PathText(g->path)).second) n++;
        }
        return n;
    }

    bool Scene::Save(const Project& project, std::vector<std::string>& scripts, std::string& error, bool write) {
        std::map<fs::path, json> files{};
        std::map<fs::path, std::vector<std::pair<const GscPatch*, const Instance*>>> gscs{};
        auto load = [&](const fs::path& p) -> json& {
            auto it{ files.find(p) };
            if (it == files.end()) it = files.emplace(p, ReadJsonFile(p)).first;
            return it->second;
        };
        auto addScript = [&](const std::string& s) {
            if (std::find(scripts.begin(), scripts.end(), s) == scripts.end()) scripts.push_back(s);
        };

        // a group's relative changes written once (its members carry the same move), by object and number
        std::set<std::string> written{};
        for (const Instance& inst : instances) {
            // spawned models (a barrier's boards) follow what they're spawned from, never saved
            if (!inst.Moved() || inst.kind == Kind::SPAWNED) continue;
            const Binding* b{ BindingOf(inst) };
            if (b && !b->locked.empty()) continue;
            json angles{ json::array({ Num(inst.angles[0]), Num(inst.angles[1]), Num(inst.angles[2]) }) };

            // its own file: no binding, or "direct"
            if (!b || b->direct) {
                json& root{ load(inst.source) };
                const char* list{ inst.kind == Kind::STATIC_MODEL ? "staticModels" : "entities" };
                if (!root.is_object() || !root.contains(list) || !root[list].is_array() || inst.sourceIndex < 0 || inst.sourceIndex >= (int)root[list].size()) {
                    error = "can't find " + inst.label + " in " + ToUtf8(inst.source.filename());
                    return false;
                }
                json& obj{ root[list][inst.sourceIndex] };
                obj["origin"] = json::array({ Num(inst.origin[0]), Num(inst.origin[1]), Num(inst.origin[2]) });
                obj["angles"] = angles;
                // a static model's uniform scale (the scale mode)
                if (inst.kind == Kind::STATIC_MODEL && std::abs(inst.scale - inst.savedScale) > 1e-5f) {
                    obj["scale"] = std::round(inst.scale * 1000.0) / 1000.0;
                }
                Logf(LogLevel::INFO, "  %s: %s %s[%d]%s", inst.label.c_str(), ToUtf8(inst.source.filename()).c_str(), list, inst.sourceIndex,
                    std::abs(inst.scale - inst.savedScale) > 1e-5f ? (" scale " + std::to_string(inst.scale)).c_str() : "");
            }
            if (!b) continue;

            for (const Target& t : b->targets) {
                json& root{ load(t.file) };
                json* obj{ root.is_null() ? nullptr : Walk(root, t.path) };
                if (!obj) {
                    error = "no " + PathText(t.path) + " in " + ToUtf8(t.file) + " (for " + inst.label + ")";
                    return false;
                }
                if (t.Relative()) {
                    // its own move: what it followed of its group's (a sill with its window) is the group's to write
                    float d[3]{};
                    for (int k = 0; k < 3; k++) d[k] = inst.origin[k] - inst.savedOrigin[k] - inst.followed[k];
                    // the changes by number, summed (a sill lifted and grown: its top gets both), then written once
                    std::map<std::string, float> changes{};
                    auto add = [&](const std::string& key, float by) {
                        if (!key.empty() && std::abs(by) > 1e-4f) changes[key] += by;
                    };
                    if (t.axis >= 0) add(t.along, d[t.axis]);
                    if (t.AcrossAxis() >= 0) add(t.across, d[t.AcrossAxis()]);
                    add(t.zKey, d[2]);
                    for (const std::string& k : t.zAlso) add(k, d[2]);
                    add(t.yawKey, Turn(inst.angles[1] - inst.savedAngles[1]));
                    for (int k = 0; k < 3; k++) add(t.scaleKey[k], inst.scaleDelta[k] * (k < 2 && t.scaleDouble ? 2.0f : 1.0f));
                    std::string where{ ToUtf8(t.file) + PathText(t.path) + "." };
                    for (auto& [key, by] : changes) {
                        if (!written.insert(where + key).second) continue;
                        float v{ NumOr(*obj, key, t.Default(key)) + by };
                        (*obj)[key] = Num(key == t.yawKey ? Turn(v) : v);
                        Logf(LogLevel::INFO, "  %s: %s%s.%s %+g", inst.label.c_str(), ToUtf8(t.file.filename()).c_str(), PathText(t.path).c_str(), key.c_str(), by);
                    }
                    // a free one's own place too (a stone: its origin and yaw, its size above)
                    if (t.origin.empty() && t.yaw.empty() && t.angles.empty()) continue;
                }
                if (!t.origin.empty()) {
                    json& arr{ t.originKey.empty() ? *obj : (*obj)[t.originKey] };
                    size_t n{ t.origin == "xyz" ? 3u : 2u };
                    if (!arr.is_array()) arr = json::array();
                    while (arr.size() < n) arr.push_back(0);
                    for (size_t k = 0; k < n; k++) arr[k] = Num(inst.origin[k]);
                }
                if (!t.yaw.empty()) (*obj)[t.yaw] = Num(inst.angles[1]);
                if (!t.angles.empty()) (*obj)[t.angles] = angles;
                Logf(LogLevel::INFO, "  %s: %s%s", inst.label.c_str(), ToUtf8(t.file.filename()).c_str(), PathText(t.path).c_str());
            }
            for (const GscPatch& g : b->gsc) gscs[g.file].emplace_back(&g, &inst);
            for (const std::string& s : b->run) addScript(s);
        }

        for (auto& [path, root] : files) {
            if (root.is_null()) {
                error = "can't read " + ToUtf8(path);
                return false;
            }
            if (!write) continue;
            if (!WriteText(path, root.dump(4) + "\n")) {
                error = "can't write " + ToUtf8(path);
                return false;
            }
        }
        for (auto& [path, patches] : gscs) {
            std::ifstream is{ path, std::ios::binary };
            std::stringstream ss;
            ss << is.rdbuf();
            std::string text{ ss.str() };
            for (auto& [g, inst] : patches) {
                if (!PatchGsc(text, g->function, g->field, inst->origin)) {
                    error = "no s." + g->field + " in " + g->function + "() of " + ToUtf8(path.filename());
                    return false;
                }
                Logf(LogLevel::INFO, "  %s: %s %s() s.%s", inst->label.c_str(), ToUtf8(path.filename()).c_str(), g->function.c_str(), g->field.c_str());
            }
            if (write && !WriteText(path, text)) {
                error = "can't write " + ToUtf8(path);
                return false;
            }
        }
        return true;
    }

    bool Scene::Duplicate(const Instance& inst, std::vector<std::string>& scripts, Selection& select, std::string& error) {
        scripts.clear();
        select = {};
        if (inst.kind == Kind::SPAWNED) {
            error = inst.label + (inst.board > 0 ? " is a barrier's board" : " is spawned by the map script") + ": it can't be copied here";
            return false;
        }
        const Binding* b{ BindingOf(inst) };
        if (b && !b->copyFile.empty()) {
            // a stone of its box as it is now (scaled, grown by its scale handles, turned), stacked on it
            json root{ ReadJsonFile(b->copyFile) };
            json* list{ root.is_object() ? Walk(root, b->copyPath) : nullptr };
            if (list && list->is_null()) *list = json::array();
            if (!list || !list->is_array()) {
                error = "no list " + PathText(b->copyPath) + " in " + ToUtf8(b->copyFile.filename());
                return false;
            }
            const Target* g{ b->Group() };
            float size[3]{};
            for (int k = 0; k < 3; k++) {
                size[k] = (inst.maxs[k] - inst.mins[k]) * inst.scale;
                if (g && g->Scales()) size[k] += (g->scaleXY && k == 1 ? inst.scaleDelta[0] : inst.scaleDelta[k]) * (k < 2 && g->scaleDouble ? 2.0f : 1.0f);
                size[k] = std::max(1.0f, size[k]);
            }
            // its bottom middle: the box's middle turned by its yaw, its base
            float yaw{ inst.angles[1] * 3.14159265f / 180 };
            float mx{ (inst.mins[0] + inst.maxs[0]) * 0.5f * inst.scale }, my{ (inst.mins[1] + inst.maxs[1]) * 0.5f * inst.scale };
            float origin[3]{ inst.origin[0] + mx * std::cos(yaw) - my * std::sin(yaw), inst.origin[1] + mx * std::sin(yaw) + my * std::cos(yaw),
                inst.origin[2] + inst.mins[2] * inst.scale + size[2] };
            json stone{ json::object() };
            stone["origin"] = json::array({ Num(origin[0]), Num(origin[1]), Num(origin[2]) });
            stone["yaw"] = Num(Turn(inst.angles[1]));
            stone["width"] = Num(size[0]);
            stone["depth"] = Num(size[1]);
            stone["height"] = Num(size[2]);
            size_t n{ list->size() };
            list->push_back(stone);
            if (!WriteText(b->copyFile, root.dump(4) + "\n")) {
                error = "can't write " + ToUtf8(b->copyFile);
                return false;
            }
            scripts = b->run;
            select.label = std::regex_replace(b->copySelect, std::regex{ R"(\{n\})" }, std::to_string(n));
            Logf(LogLevel::INFO, "copied %s: %s%s[%zu], on top of it", inst.label.c_str(), ToUtf8(b->copyFile.filename()).c_str(), PathText(b->copyPath).c_str(), n);
            return true;
        }
        if (b && !(b->direct && b->targets.empty() && b->gsc.empty() && b->locked.empty())) {
            error = inst.label + " is placed by the map's tools" + (b->targets.empty() ? "" : " from " + ToUtf8(b->targets[0].file.filename()) +
                PathText(b->targets[0].path)) + ": add another there (its config)";
            return false;
        }
        // its own entry, beside it along x
        json root{ ReadJsonFile(inst.source) };
        const char* listName{ inst.kind == Kind::STATIC_MODEL ? "staticModels" : "entities" };
        if (!root.is_object() || !root.contains(listName) || !root[listName].is_array() || inst.sourceIndex < 0 || inst.sourceIndex >= (int)root[listName].size()) {
            error = "can't find " + inst.label + " in " + ToUtf8(inst.source.filename());
            return false;
        }
        json copy{ root[listName][inst.sourceIndex] };
        float step{ std::max(16.0f, std::ceil((inst.maxs[0] - inst.mins[0]) * inst.scale / 8) * 8) };
        if (copy.contains("origin") && copy["origin"].is_array() && !copy["origin"].empty() && copy["origin"][0].is_number()) {
            copy["origin"][0] = Num(copy["origin"][0].get<float>() + step);
        }
        root[listName].push_back(copy);
        if (!WriteText(inst.source, root.dump(4) + "\n")) {
            error = "can't write " + ToUtf8(inst.source);
            return false;
        }
        select.source = inst.source;
        select.sourceIndex = (int)root[listName].size() - 1;
        Logf(LogLevel::INFO, "copied %s: %s %s[%d], %g along x", inst.label.c_str(), ToUtf8(inst.source.filename()).c_str(), listName, select.sourceIndex, step);
        if (auto gen{ generated.find(inst.source.lexically_normal()) }; gen != generated.end()) {
            Logf(LogLevel::WARN, "  %s: the copy can go when that tool runs", gen->second.c_str());
        }
        return true;
    }

    bool Scene::Remove(const Instance& inst, std::vector<std::string>& scripts, std::string& error) {
        scripts.clear();
        const Binding* b{ BindingOf(inst) };
        const Target* t{ b && b->removable && !b->targets.empty() ? &b->targets[0] : nullptr };
        if (!t || !t->path.is_array() || t->path.empty() || !t->path.back().is_number_integer()) {
            error = inst.label + " can't be removed here (a stone can)";
            return false;
        }
        json root{ ReadJsonFile(t->file) };
        json parent{ t->path };
        size_t index{ parent.back().get<size_t>() };
        parent.erase(parent.size() - 1);
        json* list{ root.is_object() ? Walk(root, parent) : nullptr };
        if (!list || !list->is_array() || index >= list->size()) {
            error = "no " + PathText(t->path) + " in " + ToUtf8(t->file.filename());
            return false;
        }
        list->erase(index);
        if (!WriteText(t->file, root.dump(4) + "\n")) {
            error = "can't write " + ToUtf8(t->file);
            return false;
        }
        scripts = b->run;
        Logf(LogLevel::INFO, "removed %s: %s%s", inst.label.c_str(), ToUtf8(t->file.filename()).c_str(), PathText(t->path).c_str());
        return true;
    }
}
