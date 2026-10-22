#include "scene.hpp"
#include <gltf.hpp>
#include <array>
#include <fstream>
#include <sstream>

namespace editor::scene {
    namespace {
        namespace gltf = fastfile::linker::bo4::gltf;

        json ReadJson(const fs::path& p) {
            std::ifstream is{ p };
            return is ? json::parse(is, nullptr, false) : json{};
        }

        std::string Str(const json& j, const char* key) {
            auto it{ j.find(key) };
            return it != j.end() && it->is_string() ? it->get<std::string>() : std::string{};
        }

        bool ReadVec3(const json& j, const char* key, float out[3]) {
            auto it{ j.find(key) };
            if (it == j.end() || !it->is_array() || it->size() < 3) return false;
            for (int i = 0; i < 3; i++) out[i] = (*it)[i].is_number() ? (*it)[i].get<float>() : 0;
            return true;
        }

        fs::path FirstFile(std::initializer_list<fs::path> candidates) {
            std::error_code ec;
            for (const fs::path& p : candidates) {
                if (fs::is_regular_file(p, ec)) return p.lexically_normal();
            }
            return {};
        }

        // a material reference of an xmodel json: from the zone's folder (the map's, or the model's fastfile's)
        fs::path MaterialJson(const std::string& ref, const fs::path& mapDir, const fs::path& xmodelDir) {
            if (ref.empty() || ref[0] == '#') return {};
            fs::path r{ FromUtf8(ref) };
            return FirstFile({ mapDir / r, xmodelDir.parent_path() / r, xmodelDir / r });
        }

        // a material json's color map: "textures": { "colorMap": "x.png,options" }, an image of the map's
        fs::path ColorMap(const fs::path& material, const fs::path& mapDir) {
            if (material.empty()) return {};
            json m{ ReadJson(material) };
            if (!m.is_object() || !m.contains("textures") || !m["textures"].is_object()) return {};
            std::string map{ Str(m["textures"], "colorMap") };
            map = map.substr(0, map.find(','));
            if (map.empty() || map[0] == '#') return {};
            fs::path name{ FromUtf8(map) };
            fs::path dir{ material.parent_path() };
            return FirstFile({ mapDir / "images" / name, dir / name, dir.parent_path() / "images" / name, mapDir / name });
        }

        // triangles without normals: their face's
        void FaceNormals(Mesh& mesh, size_t firstIndex) {
            for (size_t i = firstIndex; i + 2 < mesh.indices.size(); i += 3) {
                Vertex& a{ mesh.vertices[mesh.indices[i]] };
                Vertex& b{ mesh.vertices[mesh.indices[i + 1]] };
                Vertex& c{ mesh.vertices[mesh.indices[i + 2]] };
                float u[3]{ b.pos[0] - a.pos[0], b.pos[1] - a.pos[1], b.pos[2] - a.pos[2] };
                float v[3]{ c.pos[0] - a.pos[0], c.pos[1] - a.pos[1], c.pos[2] - a.pos[2] };
                float n[3]{ u[1] * v[2] - u[2] * v[1], u[2] * v[0] - u[0] * v[2], u[0] * v[1] - u[1] * v[0] };
                float len{ std::sqrt(n[0] * n[0] + n[1] * n[1] + n[2] * n[2]) };
                if (len <= 0) continue;
                for (Vertex* p : { &a, &b, &c }) {
                    for (int k = 0; k < 3; k++) p->normal[k] = n[k] / len;
                }
            }
        }

        // an obj as acts' ReadObj reads it: y up turned z up (x, -z, y), scaled; its usemtl groups the parts
        bool ReadObj(const fs::path& path, bool yUp, float scale, Mesh& mesh, const std::map<std::string, fs::path>& textures, const fs::path& texture) {
            std::ifstream is{ path };
            if (!is) {
                mesh.error = "can't read " + ToUtf8(path);
                return false;
            }
            auto convert = [&](float x, float y, float z, float s) {
                return yUp ? std::array<float, 3>{ x * s, -z * s, y * s } : std::array<float, 3>{ x * s, y * s, z * s };
            };
            std::vector<std::array<float, 3>> pos{};
            std::vector<std::array<float, 3>> nrm{};
            std::vector<std::array<float, 2>> uvs{};
            Part part{ (uint32_t)mesh.indices.size(), 0, texture };
            auto closePart = [&]() {
                part.count = (uint32_t)mesh.indices.size() - part.start;
                if (part.count) mesh.parts.push_back(part);
            };
            bool hasNormals{ true };
            std::string line{};
            while (std::getline(is, line)) {
                std::istringstream ls{ line };
                std::string tag{};
                ls >> tag;
                if (tag == "v") {
                    float x{}, y{}, z{};
                    ls >> x >> y >> z;
                    pos.push_back(convert(x, y, z, scale));
                }
                else if (tag == "vn") {
                    float x{}, y{}, z{};
                    ls >> x >> y >> z;
                    nrm.push_back(convert(x, y, z, 1));
                }
                else if (tag == "vt") {
                    float u{}, v{};
                    ls >> u >> v;
                    uvs.push_back({ u, 1 - v });
                }
                else if (tag == "usemtl") {
                    std::string name{};
                    ls >> name;
                    closePart();
                    auto it{ textures.find(name) };
                    part = { (uint32_t)mesh.indices.size(), 0, it != textures.end() ? it->second : texture };
                }
                else if (tag == "f") {
                    std::vector<uint32_t> face{};
                    std::string tok{};
                    while (ls >> tok) {
                        int idx[3]{};
                        size_t s1{ tok.find('/') };
                        idx[0] = std::atoi(tok.c_str());
                        if (s1 != std::string::npos) {
                            size_t s2{ tok.find('/', s1 + 1) };
                            if (s2 != s1 + 1) idx[1] = std::atoi(tok.c_str() + s1 + 1);
                            if (s2 != std::string::npos) idx[2] = std::atoi(tok.c_str() + s2 + 1);
                        }
                        auto fix = [](int i, size_t n) -> int { return i < 0 ? (int)n + i : i - 1; };
                        Vertex vert{};
                        int p{ fix(idx[0], pos.size()) };
                        if (p < 0 || p >= (int)pos.size()) continue;
                        std::copy(pos[p].begin(), pos[p].end(), vert.pos);
                        int t{ idx[1] ? fix(idx[1], uvs.size()) : -1 };
                        if (t >= 0 && t < (int)uvs.size()) std::copy(uvs[t].begin(), uvs[t].end(), vert.uv);
                        int n{ idx[2] ? fix(idx[2], nrm.size()) : -1 };
                        if (n >= 0 && n < (int)nrm.size()) std::copy(nrm[n].begin(), nrm[n].end(), vert.normal);
                        else hasNormals = false;
                        face.push_back((uint32_t)mesh.vertices.size());
                        mesh.vertices.push_back(vert);
                    }
                    for (size_t k = 1; k + 1 < face.size(); k++) {
                        mesh.indices.insert(mesh.indices.end(), { face[0], face[k], face[k + 1] });
                    }
                }
            }
            closePart();
            if (!hasNormals) FaceNormals(mesh, 0);
            return true;
        }

        // a gltf with acts' reader, its nodes' transforms applied and turned to game space as the linker does
        bool ReadGltf(const fs::path& path, float scale, Mesh& mesh, const std::map<std::string, fs::path>& textures, const fs::path& texture) {
            gltf::Document doc{};
            std::string err{};
            if (!doc.Load(path, err)) {
                mesh.error = ToUtf8(path.filename()) + ": " + err;
                return false;
            }
            for (size_t n = 0; n < doc.nodes.size(); n++) {
                const gltf::Node& node{ doc.nodes[n] };
                if (node.mesh < 0 || node.mesh >= (int)doc.meshes.size()) continue;
                gltf::Mat4 world{ doc.WorldMatrix((int)n) };
                for (const gltf::Primitive& prim : doc.meshes[node.mesh].primitives) {
                    size_t count{ prim.positions.size() / 3 };
                    uint32_t base{ (uint32_t)mesh.vertices.size() };
                    size_t first{ mesh.indices.size() };
                    bool hasNormals{ prim.normals.size() == count * 3 };
                    bool hasUvs{ prim.uvs.size() == count * 2 };
                    for (size_t i = 0; i < count; i++) {
                        double p[3]{ prim.positions[i * 3], prim.positions[i * 3 + 1], prim.positions[i * 3 + 2] };
                        double wp[3]{};
                        world.Apply(p, wp);
                        Vertex v{};
                        v.pos[0] = (float)(wp[0] * scale);
                        v.pos[1] = (float)(-wp[2] * scale);
                        v.pos[2] = (float)(wp[1] * scale);
                        if (hasNormals) {
                            double nn[3]{ prim.normals[i * 3], prim.normals[i * 3 + 1], prim.normals[i * 3 + 2] };
                            double wn[3]{};
                            world.Apply(nn, wn, 0);
                            double len{ std::sqrt(wn[0] * wn[0] + wn[1] * wn[1] + wn[2] * wn[2]) };
                            if (len > 0) {
                                v.normal[0] = (float)(wn[0] / len);
                                v.normal[1] = (float)(-wn[2] / len);
                                v.normal[2] = (float)(wn[1] / len);
                            }
                        }
                        if (hasUvs) {
                            // glTF's v goes down as D3D's
                            v.uv[0] = prim.uvs[i * 2];
                            v.uv[1] = prim.uvs[i * 2 + 1];
                        }
                        mesh.vertices.push_back(v);
                    }
                    if (prim.indices.empty()) {
                        for (uint32_t i = 0; i < count; i++) mesh.indices.push_back(base + i);
                    }
                    else {
                        for (uint32_t i : prim.indices) mesh.indices.push_back(base + i);
                    }
                    if (!hasNormals) FaceNormals(mesh, first);
                    auto it{ textures.find(prim.material) };
                    mesh.parts.push_back({ (uint32_t)first, (uint32_t)(mesh.indices.size() - first), it != textures.end() ? it->second : texture });
                }
            }
            return true;
        }

        // the colors of the markers by classname
        void MarkerLook(const std::string& classname, const std::string& targetname, Instance& inst) {
            float size{ 12 };
            float c[3]{ 0.65f, 0.65f, 0.7f };
            auto set = [&](float r, float g, float b, float s) { c[0] = r; c[1] = g; c[2] = b; size = s; };
            if (targetname == "initial_spawn_points") set(0.3f, 0.95f, 0.4f, 16);
            else if (classname.rfind("actor_spawner", 0) == 0) set(1.0f, 0.3f, 0.3f, 20);
            else if (classname.rfind("zbarrier", 0) == 0) set(1.0f, 0.6f, 0.2f, 24);
            else if (classname.rfind("weapon_", 0) == 0) set(0.3f, 0.85f, 1.0f, 10);
            else if (targetname.find("spawn") != std::string::npos) set(0.35f, 0.55f, 1.0f, 12);
            else if (classname == "script_model") set(0.95f, 0.8f, 0.3f, 14);
            std::copy(c, c + 3, inst.color);
            for (int i = 0; i < 3; i++) {
                inst.mins[i] = i == 2 ? 0 : -size / 2;
                inst.maxs[i] = i == 2 ? size : size / 2;
            }
        }
    }

    fs::path FindXModel(const Project& project, const std::string& name) {
        if (name.empty()) return {};
        for (const ZoneBlock& b : project.zone.blocks) {
            for (const ZoneEntry& e : b.entries) {
                if (e.type == "xmodel" && e.state == EntryState::FILE && ToUtf8(e.file.stem()) == name) return e.file;
            }
        }
        fs::path file{ FromUtf8(name + ".json") };
        if (fs::path p{ FirstFile({ project.dir / "xmodels" / file }) }; !p.empty()) return p;
        // another fastfile's (the mod's enh_ff has enh_floor)
        std::error_code ec;
        for (const fs::directory_entry& d : fs::directory_iterator{ project.dir.parent_path(), ec }) {
            if (!d.is_directory(ec) || d.path() == project.dir) continue;
            if (fs::path p{ FirstFile({ d.path() / "xmodels" / file }) }; !p.empty()) return p;
        }
        return {};
    }

    bool LoadXModel(const fs::path& path, const fs::path& mapDir, Mesh& out) {
        out = {};
        json x{ ReadJson(path) };
        if (!x.is_object()) {
            out.error = "not valid json";
            return false;
        }
        fs::path dir{ path.parent_path() };
        float scale{ x.contains("scale") && x["scale"].is_number() ? x["scale"].get<float>() : 1.0f };
        bool yUp{ Str(x, "upAxis") == "y" };

        fs::path texture{ ColorMap(MaterialJson(Str(x, "material"), mapDir, dir), mapDir) };
        std::map<std::string, fs::path> textures{};
        if (x.contains("materials") && x["materials"].is_object()) {
            for (auto& [k, v] : x["materials"].items()) {
                if (v.is_string()) textures[k] = ColorMap(MaterialJson(v.get<std::string>(), mapDir, dir), mapDir);
            }
        }

        std::string obj{ Str(x, "obj") };
        std::string gl{ Str(x, "gltf") };
        if (obj.empty() && gl.empty() && x.contains("lods") && x["lods"].is_array() && !x["lods"].empty()) {
            obj = Str(x["lods"][0], "obj");
            gl = Str(x["lods"][0], "gltf");
        }
        bool ok{};
        if (!gl.empty()) ok = ReadGltf(dir / FromUtf8(gl), scale, out, textures, texture);
        else if (!obj.empty()) ok = ReadObj(dir / FromUtf8(obj), yUp, scale, out, textures, texture);
        else out.error = x.contains("lossless") ? "a stock model (lossless): not drawn yet" : "no obj or gltf";
        if (!ok || out.vertices.empty()) {
            if (out.error.empty()) out.error = "no triangles";
            return false;
        }
        for (int k = 0; k < 3; k++) {
            out.mins[k] = out.maxs[k] = out.vertices[0].pos[k];
        }
        for (const Vertex& v : out.vertices) {
            for (int k = 0; k < 3; k++) {
                out.mins[k] = std::min(out.mins[k], v.pos[k]);
                out.maxs[k] = std::max(out.maxs[k], v.pos[k]);
            }
        }
        return true;
    }

    void Scene::Build(const Project& project) {
        *this = {};
        std::map<std::string, int> byName{};
        // a model's mesh, loaded once, -1 when it can't be drawn
        auto meshFor = [&](const std::string& name) -> int {
            auto it{ byName.find(name) };
            if (it != byName.end()) return it->second;
            int index{ -1 };
            fs::path json{ FindXModel(project, name) };
            Mesh mesh{};
            if (json.empty()) {
                Logf(LogLevel::WARN, "3d view: no xmodel json for %s, a box", name.c_str());
            }
            else if (!LoadXModel(json, project.dir, mesh)) {
                Logf(LogLevel::WARN, "3d view: %s: %s, a box", name.c_str(), mesh.error.c_str());
            }
            else {
                index = (int)meshes.size();
                meshes.push_back(std::move(mesh));
            }
            byName[name] = index;
            return index;
        };

        for (const ZoneBlock& b : project.zone.blocks) {
            for (const ZoneEntry& e : b.entries) {
                if (e.state != EntryState::FILE) continue;
                if (e.type == "gfx_map") gfxMap = e.file;
                else if (e.type == "entitylist") entityList = e.file;
            }
        }

        if (!gfxMap.empty()) {
            json gfx{ ReadJson(gfxMap) };
            if (gfx.contains("staticModels") && gfx["staticModels"].is_array()) {
                for (const json& sm : gfx["staticModels"]) {
                    Instance inst{};
                    inst.kind = Kind::STATIC_MODEL;
                    inst.name = Str(sm, "model");
                    inst.label = inst.name;
                    ReadVec3(sm, "origin", inst.origin);
                    ReadVec3(sm, "angles", inst.angles);
                    inst.scale = sm.contains("scale") && sm["scale"].is_number() ? sm["scale"].get<float>() : 1.0f;
                    inst.mesh = meshFor(inst.name);
                    if (inst.mesh >= 0) {
                        std::copy(meshes[inst.mesh].mins, meshes[inst.mesh].mins + 3, inst.mins);
                        std::copy(meshes[inst.mesh].maxs, meshes[inst.mesh].maxs + 3, inst.maxs);
                    }
                    else if (!ReadVec3(sm, "mins", inst.mins) || !ReadVec3(sm, "maxs", inst.maxs)) {
                        MarkerLook("", "", inst);
                    }
                    inst.source = gfxMap;
                    inst.data = sm;
                    instances.push_back(std::move(inst));
                }
            }
        }

        if (!entityList.empty()) {
            json list{ ReadJson(entityList) };
            if (list.contains("entities") && list["entities"].is_array()) {
                for (const json& ent : list["entities"]) {
                    std::string classname{ Str(ent, "classname") };
                    if (classname == "worldspawn") continue;
                    std::string targetname{ Str(ent, "targetname") };
                    std::string noteworthy{ Str(ent, "script_noteworthy") };
                    Instance inst{};
                    inst.name = classname;
                    inst.label = classname + (targetname.empty() ? "" : " " + targetname) + (noteworthy.empty() ? "" : " (" + noteworthy + ")");
                    ReadVec3(ent, "origin", inst.origin);
                    ReadVec3(ent, "angles", inst.angles);
                    std::string model{ Str(ent, "model") };
                    inst.kind = model.empty() ? Kind::ENTITY : Kind::SCRIPT_MODEL;
                    inst.mesh = model.empty() ? -1 : meshFor(model);
                    if (!model.empty()) inst.label += " [" + model + "]";
                    MarkerLook(classname, targetname, inst);
                    if (inst.mesh >= 0) {
                        std::copy(meshes[inst.mesh].mins, meshes[inst.mesh].mins + 3, inst.mins);
                        std::copy(meshes[inst.mesh].maxs, meshes[inst.mesh].maxs + 3, inst.maxs);
                    }
                    inst.source = entityList;
                    inst.data = ent;
                    instances.push_back(std::move(inst));
                }
            }
        }

        // the scene's bounds (each instance's box at its origin, unrotated)
        bool first{ true };
        for (const Instance& inst : instances) {
            for (int k = 0; k < 3; k++) {
                float lo{ inst.origin[k] + inst.mins[k] * inst.scale };
                float hi{ inst.origin[k] + inst.maxs[k] * inst.scale };
                mins[k] = first ? lo : std::min(mins[k], lo);
                maxs[k] = first ? hi : std::max(maxs[k], hi);
            }
            first = false;
        }
        size_t triangles{};
        for (const Mesh& m : meshes) triangles += m.indices.size() / 3;
        Logf(LogLevel::INFO, "3d view: %zu instances, %zu meshes (%zu triangles)", instances.size(), meshes.size(), triangles);
    }
}
