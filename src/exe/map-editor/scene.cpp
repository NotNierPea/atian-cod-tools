#include "scene.hpp"
#include <gltf.hpp>
#include <array>
#include <cmath>
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

        // a material reference of an xmodel json: a path from the zone's folder (the map's, or the model's fastfile's);
        // "#name" a material asset of that name, its json materials/<name>.json there or in a sibling fastfile folder
        // (enh_floor's "#enh_cube_mat" is enh_ff's); "#hash_x" a game material (the dumped ones are LoadLossless')
        fs::path MaterialJson(const std::string& ref, const fs::path& mapDir, const fs::path& xmodelDir) {
            if (ref.empty() || ref.rfind("#hash_", 0) == 0) return {};
            if (ref[0] == '#') {
                fs::path file{ FromUtf8(ref.substr(1) + ".json") };
                if (fs::path p{ FirstFile({ mapDir / "materials" / file, xmodelDir.parent_path() / "materials" / file }) }; !p.empty()) return p;
                std::error_code ec;
                for (const fs::directory_entry& d : fs::directory_iterator{ mapDir.parent_path(), ec }) {
                    if (!d.is_directory(ec)) continue;
                    if (fs::path p{ FirstFile({ d.path() / "materials" / file }) }; !p.empty()) return p;
                }
                return {};
            }
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

        std::vector<uint8_t> Unhex(const json& j) {
            std::vector<uint8_t> out{};
            if (!j.is_string()) return out;
            const std::string& s{ j.get_ref<const std::string&>() };
            out.resize(s.size() / 2);
            auto nib = [](char c) -> uint8_t { return (uint8_t)(c <= '9' ? c - '0' : (c | 0x20) - 'a' + 10); };
            for (size_t i = 0; i < out.size(); i++) out[i] = (uint8_t)(nib(s[i * 2]) << 4 | nib(s[i * 2 + 1]));
            return out;
        }

        template<typename T>
        T Read(const std::vector<uint8_t>& d, size_t off) {
            T v{};
            if (off + sizeof(T) <= d.size()) memcpy(&v, d.data() + off, sizeof(T));
            return v;
        }

        float Half(uint16_t h) {
            uint32_t sign{ (uint32_t)(h & 0x8000) << 16 };
            uint32_t exp{ (uint32_t)(h >> 10) & 0x1Fu };
            uint32_t man{ h & 0x3FFu };
            uint32_t bits{};
            if (!exp) {
                if (man) {
                    // subnormal: normalized
                    exp = 127 - 15 + 1;
                    while (!(man & 0x400)) { man <<= 1; exp--; }
                    bits = sign | (exp << 23) | ((man & 0x3FF) << 13);
                }
                else bits = sign;
            }
            else if (exp == 31) bits = sign | 0x7F800000 | (man << 13);
            else bits = sign | ((exp - 15 + 127) << 23) | (man << 13);
            float f;
            memcpy(&f, &bits, 4);
            return f;
        }

        /*
         * A dumped material's look: its textureTable's colorMap (a dumped image's json) and its alpha. Opaque materials
         * have 0x40 at header +0x1C (gbuffer ones, and the wall-buy chalk whose color map's alpha cuts its lines out);
         * the rest (decals, camera region 26, lit_trans) blend.
         */
        struct MaterialLook {
            fs::path colorMap{};
            Alpha alpha{};
            bool mask{};
        };
        // the wall-buy chalk's techset: its color map is black, its alpha the weapon's shape the shader chalks
        constexpr const char* CHALK_TECHSET{ "#hash_1a4c5ae8fb889db2" };

        MaterialLook LosslessMaterial(const std::string& material, const AssetIndex& assets) {
            MaterialLook look{};
            fs::path file{ assets.Find(AssetType::MATERIAL, material) };
            if (file.empty()) return look;
            json m{ ReadJson(file) };
            look.mask = Str(m, "techniqueSet") == CHALK_TECHSET;
            std::vector<uint8_t> header{ Unhex(m["header"]) };
            look.alpha = header.size() > 0x1C && !(header[0x1C] & 0x40) ? Alpha::BLEND : Alpha::CUTOUT;
            if (m.contains("textureTable") && m["textureTable"].is_array()) {
                for (const json& t : m["textureTable"]) {
                    if (Str(t, "type") == "colorMap") look.colorMap = assets.Find(AssetType::IMAGE, Str(t, "image"));
                }
            }
            return look;
        }

        /*
         * A dumped stock model (acts' lossless xmodel json): its most detailed lod (the last one, the game keeps the least
         * detailed first) from its xmodelmesh json. The mesh's shared buffer ("data", or the xpak's "streamData") as its
         * XSurfaceShared header says: u16 indices at 0x34, float3 positions at 0x2C, vertices at 0x30 (16 bytes: color,
         * half2 uv, 10:10:10 normal, tangent; 24 / 40 with the 0x40 / 0x80 flags). Each surface (0x30): u16 vertex and
         * triangle counts at 4 / 6, its base vertex and its first index at 8 / 0xC; one material each.
         */
        bool LoadLossless(const json& x, const AssetIndex& assets, Mesh& mesh) {
            int lods{ x.contains("numLods") && x["numLods"].is_number() ? x["numLods"].get<int>() : 1 };
            if (!x.contains("meshes") || !x["meshes"].is_array()) {
                mesh.error = "a stock model without meshes";
                return false;
            }
            const json& meshes{ x["meshes"] };
            int slot{ -1 };
            for (int s = std::min(lods, (int)meshes.size()) - 1; s >= 0 && slot < 0; s--) {
                if (meshes[s].is_string()) slot = s;
            }
            if (slot < 0) {
                mesh.error = "a stock model without meshes";
                return false;
            }
            fs::path meshFile{ assets.Find(AssetType::XMODELMESH, meshes[slot].get<std::string>()) };
            if (meshFile.empty()) {
                mesh.error = "its mesh " + meshes[slot].get<std::string>() + " isn't dumped in the map's folder";
                return false;
            }
            json mj{ ReadJson(meshFile) };
            std::vector<uint8_t> shared{ Unhex(mj["shared"]) };
            std::vector<uint8_t> data{ Unhex(mj.contains("data") && mj["data"].is_string() ? mj["data"] : mj["streamData"]) };
            if (shared.size() < 0x3C || data.empty()) {
                mesh.error = "its mesh has no data";
                return false;
            }
            uint32_t flags{ Read<uint32_t>(shared, 0) };
            uint32_t vertCount{ Read<uint32_t>(shared, 4) };
            uint32_t posOffset{ Read<uint32_t>(shared, 0x2C) };
            uint32_t vertsOffset{ Read<uint32_t>(shared, 0x30) };
            uint32_t indicesOffset{ Read<uint32_t>(shared, 0x34) };
            size_t stride{ (size_t)((flags & 0x80) ? 40 : (flags & 0x40) ? 24 : 16) };
            if ((size_t)posOffset + vertCount * 12ull > data.size() || (size_t)vertsOffset + vertCount * stride > data.size()) {
                mesh.error = "its mesh's buffer is shorter than its header says";
                return false;
            }
            mesh.vertices.resize(vertCount);
            for (uint32_t v = 0; v < vertCount; v++) {
                Vertex& vert{ mesh.vertices[v] };
                memcpy(vert.pos, data.data() + posOffset + v * 12ull, 12);
                size_t at{ vertsOffset + v * stride };
                vert.uv[0] = Half(Read<uint16_t>(data, at + 4));
                vert.uv[1] = Half(Read<uint16_t>(data, at + 6));
                uint32_t n{ Read<uint32_t>(data, at + 8) };
                vert.normal[0] = (n & 0x3FF) / 1023.0f * 2 - 1;
                vert.normal[1] = ((n >> 10) & 0x3FF) / 1023.0f * 2 - 1;
                vert.normal[2] = ((n >> 20) & 0x3FF) / 1023.0f * 2 - 1;
            }

            std::vector<std::string> materials{};
            if (x.contains("meshMaterials") && x["meshMaterials"].is_array() && slot < (int)x["meshMaterials"].size()) {
                const json& mm{ x["meshMaterials"][slot] };
                if (mm.contains("materials") && mm["materials"].is_array()) {
                    for (const json& m : mm["materials"]) materials.push_back(m.is_string() ? m.get<std::string>() : std::string{});
                }
            }
            std::map<std::string, MaterialLook> looks{};
            const json& surfs{ mj["surfs"] };
            for (size_t s = 0; surfs.is_array() && s < surfs.size(); s++) {
                std::vector<uint8_t> sh{ Unhex(surfs[s]["header"]) };
                uint16_t sv{ Read<uint16_t>(sh, 4) };
                uint16_t tris{ Read<uint16_t>(sh, 6) };
                uint32_t baseVert{ Read<uint32_t>(sh, 8) };
                uint32_t baseIndex{ Read<uint32_t>(sh, 0xC) };
                uint32_t first{ (uint32_t)mesh.indices.size() };
                for (uint32_t k = 0; k < tris * 3u; k++) {
                    size_t at{ indicesOffset + (baseIndex + k) * 2ull };
                    if (at + 2 > data.size()) break;
                    uint32_t index{ baseVert + Read<uint16_t>(data, at) };
                    if (index >= vertCount || Read<uint16_t>(data, at) >= sv) {
                        mesh.indices.resize(first);
                        break;
                    }
                    mesh.indices.push_back(index);
                }
                mesh.indices.resize(first + (mesh.indices.size() - first) / 3 * 3);
                MaterialLook look{};
                if (s < materials.size() && !materials[s].empty()) {
                    auto it{ looks.find(materials[s]) };
                    if (it == looks.end()) it = looks.emplace(materials[s], LosslessMaterial(materials[s], assets)).first;
                    look = it->second;
                }
                if (mesh.indices.size() > first) mesh.parts.push_back({ first, (uint32_t)mesh.indices.size() - first, look.colorMap, look.alpha, look.mask });
            }
            if (mesh.indices.empty()) {
                mesh.error = "its mesh has no triangles";
                return false;
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

    std::string HashKey(const std::string& name) {
        std::string n{ name };
        if (!n.empty() && n[0] == '#') n.erase(0, 1);
        for (const char* prefix : { "hash_", "file_" }) {
            size_t len{ strlen(prefix) };
            if (n.size() > len && n.compare(0, len, prefix) == 0 && n.find_first_not_of("0123456789abcdefABCDEF", len) == std::string::npos) {
                std::string hex{ n.substr(len) };
                std::transform(hex.begin(), hex.end(), hex.begin(), [](unsigned char c) { return (char)std::tolower(c); });
                hex.erase(0, std::min(hex.find_first_not_of('0'), hex.size() - 1));
                return hex;
            }
        }
        // a plain name: acts' asset hash (fnv1a 64 of the lowercase name, 63 bits)
        uint64_t h{ 0xcbf29ce484222325ull };
        for (unsigned char c : n) {
            h ^= (uint64_t)std::tolower(c);
            h *= 0x100000001b3ull;
        }
        char buff[32];
        snprintf(buff, sizeof(buff), "%llx", (unsigned long long)(h & 0x7FFFFFFFFFFFFFFFull));
        return buff;
    }

    void AssetIndex::Build(const fs::path& mapDir) {
        for (auto& f : files) f.clear();
        static const std::pair<const char*, AssetType> dirs[]{
            { "xmodel", AssetType::XMODEL }, { "xmodelmesh", AssetType::XMODELMESH }, { "material", AssetType::MATERIAL }, { "image", AssetType::IMAGE } };
        std::error_code ec;
        for (auto it{ fs::recursive_directory_iterator{ mapDir, fs::directory_options::skip_permission_denied, ec } }; it != fs::recursive_directory_iterator{}; it.increment(ec)) {
            if (ec) break;
            if (!it->is_regular_file(ec) || it->path().extension() != ".json") continue;
            std::string dir{ ToUtf8(it->path().parent_path().filename()) };
            for (const auto& [name, type] : dirs) {
                if (dir == name) {
                    files[(int)type].emplace(HashKey(ToUtf8(it->path().stem())), it->path());
                    break;
                }
            }
        }
    }

    fs::path AssetIndex::Find(AssetType type, const std::string& name) const {
        if (name.empty()) return {};
        auto it{ files[(int)type].find(HashKey(name)) };
        return it == files[(int)type].end() ? fs::path{} : it->second;
    }

    fs::path FindXModel(const Project& project, const AssetIndex& assets, const std::string& name) {
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
        // a stock model dumped into the map's folder, by its name's hash
        return assets.Find(AssetType::XMODEL, name);
    }

    bool LoadXModel(const fs::path& path, const fs::path& mapDir, const AssetIndex& assets, Mesh& out) {
        out = {};
        json x{ ReadJson(path) };
        if (!x.is_object()) {
            out.error = "not valid json";
            return false;
        }
        if (x.contains("lossless") && x["lossless"].is_boolean() && x["lossless"].get<bool>()) {
            if (!LoadLossless(x, assets, out)) return false;
            for (int k = 0; k < 3; k++) out.mins[k] = out.maxs[k] = out.vertices[0].pos[k];
            for (const Vertex& v : out.vertices) {
                for (int k = 0; k < 3; k++) {
                    out.mins[k] = std::min(out.mins[k], v.pos[k]);
                    out.maxs[k] = std::max(out.maxs[k], v.pos[k]);
                }
            }
            return true;
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
        else out.error = "no obj or gltf";
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

    namespace {
        // quaternions (x, y, z, w) in game space; QMul(a, b) turns by b, then a
        using Quat = std::array<float, 4>;
        Quat QMul(const Quat& a, const Quat& b) {
            return { a[3] * b[0] + a[0] * b[3] + a[1] * b[2] - a[2] * b[1],
                     a[3] * b[1] - a[0] * b[2] + a[1] * b[3] + a[2] * b[0],
                     a[3] * b[2] + a[0] * b[1] - a[1] * b[0] + a[2] * b[3],
                     a[3] * b[3] - a[0] * b[0] - a[1] * b[1] - a[2] * b[2] };
        }
        Quat QAxis(int axis, float degrees) {
            float h{ degrees * 3.14159265f / 360 };
            Quat q{ 0, 0, 0, std::cos(h) };
            q[axis] = std::sin(h);
            return q;
        }
        // as the 3D view turns an object (WorldOf): its roll about x, then its pitch about y, then its yaw about z
        Quat QAngles(const float angles[3]) { return QMul(QAxis(2, angles[1]), QMul(QAxis(1, angles[0]), QAxis(0, angles[2]))); }
        void QRotate(const Quat& q, const float v[3], float out[3]) {
            float t[3]{ 2 * (q[1] * v[2] - q[2] * v[1]), 2 * (q[2] * v[0] - q[0] * v[2]), 2 * (q[0] * v[1] - q[1] * v[0]) };
            out[0] = v[0] + q[3] * t[0] + (q[1] * t[2] - q[2] * t[1]);
            out[1] = v[1] + q[3] * t[1] + (q[2] * t[0] - q[0] * t[2]);
            out[2] = v[2] + q[3] * t[2] + (q[0] * t[1] - q[1] * t[0]);
        }

        // a 64-bit packed quat: the largest component dropped (bits 0-1: 0 x, 1 w, 2 z, 3 y; bit 63 its sign), the next
        // three cyclically (x y z w) as signed 20 bits x 2^-19 / sqrt(2)
        Quat Quat64(uint64_t v) {
            static const int dropped[4]{ 0, 3, 2, 1 };
            int drop{ dropped[v & 3] };
            Quat q{};
            float sum{};
            for (int k = 0; k < 3; k++) {
                int32_t raw{ (int32_t)((v >> (2 + 20 * k)) & 0xFFFFF) };
                if (raw & 0x80000) raw -= 0x100000;
                float c{ raw * (1.0f / 524288.0f) * 0.70710678f };
                q[(drop + 1 + k) % 4] = c;
                sum += c * c;
            }
            q[drop] = std::sqrt(std::max(0.0f, 1 - sum)) * ((v >> 63) ? -1.0f : 1.0f);
            return q;
        }

        /*
         * An anim's root motion (its delta part) at its last frame: trans {header: u16 size, u8 small; minsSize: mins
         * float3, size float3; frames: (size + 1) keys of 3 u16 (3 u8 small), pos = mins + size x key}, quat (8-byte
         * packed keys) or quat2 (yaw: 4 bytes, z as a float, bit 30 set when w < 0). A size of 0 is a single frame.
         */
        bool LastRootMotion(const json& anim, float trans[3], Quat& rot) {
            auto dp{ anim.find("deltaPart") };
            if (dp == anim.end() || !dp->is_object()) return false;
            trans[0] = trans[1] = trans[2] = 0;
            rot = { 0, 0, 0, 1 };
            if (auto t{ dp->find("trans") }; t != dp->end() && t->is_object()) {
                std::vector<uint8_t> hdr{ Unhex(t->value("header", json{})) };
                std::vector<uint8_t> frames{ Unhex(t->value("frames", json{})) };
                std::vector<uint8_t> ms{ Unhex(t->value("minsSize", json{})) };
                uint16_t size{ Read<uint16_t>(hdr, 0) };
                bool byteKeys{ Read<uint8_t>(hdr, 2) != 0 };
                if (size == 0) {
                    const std::vector<uint8_t>& one{ frames.size() >= 12 ? frames : ms };
                    for (int k = 0; k < 3; k++) trans[k] = Read<float>(one, k * 4);
                }
                else {
                    size_t ks{ byteKeys ? 3u : 6u };
                    size_t n{ frames.size() / ks };
                    if (!n || ms.size() < 24) return false;
                    size_t at{ (n - 1) * ks };
                    for (int k = 0; k < 3; k++) {
                        float raw{ byteKeys ? (float)Read<uint8_t>(frames, at + k) : (float)Read<uint16_t>(frames, at + k * 2) };
                        trans[k] = Read<float>(ms, k * 4) + Read<float>(ms, 12 + k * 4) * raw;
                    }
                }
            }
            if (auto q{ dp->find("quat") }; q != dp->end() && q->is_object()) {
                std::vector<uint8_t> frames{ Unhex(q->value("frames", json{})) };
                if (frames.size() >= 8) rot = Quat64(Read<uint64_t>(frames, frames.size() - 8));
            }
            else if (auto q2{ dp->find("quat2") }; q2 != dp->end() && q2->is_object()) {
                std::vector<uint8_t> frames{ Unhex(q2->value("frames", json{})) };
                if (frames.size() >= 4) {
                    uint32_t raw{ Read<uint32_t>(frames, frames.size() - 4) };
                    float z{};
                    uint32_t bits{ raw & ~0x40000000u };
                    memcpy(&z, &bits, 4);
                    rot = { 0, 0, z, std::sqrt(std::max(0.0f, 1 - z * z)) * ((raw & 0x40000000u) ? -1.0f : 1.0f) };
                }
            }
            return true;
        }

        // a board on its barrier as it is now: its offset and turn in the barrier's space
        void PlaceBoard(Instance& board, const Instance& barrier) {
            Quat base{ QAngles(barrier.angles) };
            float off[3]{};
            QRotate(base, board.boardOffset, off);
            for (int k = 0; k < 3; k++) board.origin[k] = barrier.origin[k] + off[k];
            std::copy(barrier.angles, barrier.angles + 3, board.angles);
            Quat q{ QMul(base, { board.boardRot[0], board.boardRot[1], board.boardRot[2], board.boardRot[3] }) };
            std::copy(q.begin(), q.end(), board.rot);
        }
    }

    void Scene::PlaceBoards() {
        for (Instance& board : instances) {
            if (board.board <= 0) continue;
            for (const Instance& b : instances) {
                if (b.board == 0 && b.kind != Kind::SPAWNED && b.source == board.source && b.sourceIndex == board.sourceIndex) {
                    PlaceBoard(board, b);
                    break;
                }
            }
        }
    }

    /*
     * A zbarrier entity's boards (a window's wooden boards): the zone's zbarrier asset of its type, each board's model
     * (the entity's zbarrierboardModel<n>, else the asset's) where its board anim (the repair) leaves it: the anim's root
     * motion at its last frame, in the entity's space. Only boards whose anim is root motion alone (its one bone
     * tag_animate): the PaP's and the box's pieces are skinned and bone animated, they stay markers.
     */
    void Scene::AddBarrierBoards(const Project& project, const std::function<int(const std::string&, fs::path&)>& meshFor) {
        std::map<std::string, fs::path> barriers{}, anims{};
        for (const ZoneBlock& b : project.zone.blocks) {
            for (const ZoneEntry& e : b.entries) {
                if (e.state != EntryState::FILE) continue;
                if (e.type == "xanim") anims.emplace(HashKey(ToUtf8(e.file.stem())), e.file);
                else if (e.type == "zbarrier") {
                    json zb{ ReadJson(e.file) };
                    if (zb.is_object()) barriers.emplace(HashKey(Str(zb, "name")), e.file);
                }
            }
        }
        if (barriers.empty()) return;
        std::map<fs::path, json> loaded{};
        auto load = [&](const fs::path& p) -> const json& {
            auto it{ loaded.find(p) };
            if (it == loaded.end()) it = loaded.emplace(p, ReadJson(p)).first;
            return it->second;
        };
        size_t added{};
        size_t count{ instances.size() };
        for (size_t i = 0; i < count; i++) {
            const Instance barrier{ instances[i] };
            std::string classname{ Str(barrier.data, "classname") };
            if (barrier.kind == Kind::STATIC_MODEL || classname.rfind("zbarrier_", 0) != 0) continue;
            std::string type{ Str(barrier.data, "type") };
            if (type.empty()) type = classname.substr(9);
            auto bit{ barriers.find(HashKey(type)) };
            if (bit == barriers.end()) continue;
            const json& zb{ load(bit->second) };
            if (!zb.contains("boards") || !zb["boards"].is_array()) continue;
            std::string owner{ Str(barrier.data, "targetname") };
            if (owner.empty()) owner = classname;
            // all its boards or none (a few of the box's pieces alone would mislead)
            std::vector<Instance> boards{};
            bool all{ true };
            for (size_t n = 0; n < zb["boards"].size() && all; n++) {
                const json& board{ zb["boards"][n] };
                std::string model{ Str(barrier.data, ("zbarrierboardModel" + std::to_string(n + 1)).c_str()) };
                if (model.empty()) model = Str(board, "boardModel");
                if (!model.empty() && model[0] == ',') model.erase(0, 1);
                auto ait{ anims.find(HashKey(Str(board, "boardAnim"))) };
                float trans[3]{};
                Quat rot{};
                all = !model.empty() && ait != anims.end();
                if (all) {
                    const json& anim{ load(ait->second) };
                    all = anim.value("numBones", 0) <= 1 && LastRootMotion(anim, trans, rot);
                }
                if (!all) break;

                Instance inst{};
                inst.kind = Kind::SPAWNED;
                inst.name = model;
                inst.label = owner + " board " + std::to_string(n + 1) + " [" + model + "]";
                std::copy(trans, trans + 3, inst.boardOffset);
                std::copy(rot.begin(), rot.end(), inst.boardRot);
                PlaceBoard(inst, barrier);
                inst.useRot = true;
                inst.mesh = meshFor(model, inst.model);
                if (inst.mesh >= 0) {
                    std::copy(meshes[inst.mesh].mins, meshes[inst.mesh].mins + 3, inst.mins);
                    std::copy(meshes[inst.mesh].maxs, meshes[inst.mesh].maxs + 3, inst.maxs);
                }
                else {
                    for (int k = 0; k < 3; k++) {
                        inst.mins[k] = -4;
                        inst.maxs[k] = 4;
                    }
                }
                inst.locked = "a board of " + owner + ": it sits where its repair anim leaves it, it moves with its barrier";
                inst.source = barrier.source;
                inst.sourceIndex = barrier.sourceIndex;
                inst.data = barrier.data;
                inst.board = (int)n + 1;
                boards.push_back(std::move(inst));
            }
            if (!all) continue;
            added += boards.size();
            for (Instance& inst : boards) instances.push_back(std::move(inst));
        }
        if (added) Logf(LogLevel::INFO, "3d view: %zu barrier board(s)", added);
    }

    bool Instance::Moved() const {
        for (int k = 0; k < 3; k++) {
            if (std::abs(origin[k] - savedOrigin[k]) > 1e-4f || std::abs(angles[k] - savedAngles[k]) > 1e-4f) return true;
            if (std::abs(scaleDelta[k]) > 1e-4f) return true;
        }
        return std::abs(scale - savedScale) > 1e-5f;
    }

    void Scene::Build(const Project& project) {
        *this = {};
        assets.Build(project.dir);
        std::map<std::string, std::pair<int, fs::path>> byName{};
        // a model's mesh, loaded once, -1 when it can't be drawn
        auto meshFor = [&](const std::string& name, fs::path& model) -> int {
            auto it{ byName.find(name) };
            if (it != byName.end()) {
                model = it->second.second;
                return it->second.first;
            }
            int index{ -1 };
            fs::path json{ FindXModel(project, assets, name) };
            Mesh mesh{};
            if (json.empty()) {
                Logf(LogLevel::WARN, "3d view: no xmodel json for %s, a box", name.c_str());
            }
            else if (!LoadXModel(json, project.dir, assets, mesh)) {
                Logf(LogLevel::WARN, "3d view: %s: %s, a box", name.c_str(), mesh.error.c_str());
            }
            else {
                index = (int)meshes.size();
                meshes.push_back(std::move(mesh));
            }
            byName[name] = { index, json };
            model = json;
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
                int sourceIndex{};
                for (const json& sm : gfx["staticModels"]) {
                    Instance inst{};
                    inst.kind = Kind::STATIC_MODEL;
                    inst.sourceIndex = sourceIndex++;
                    inst.name = Str(sm, "model");
                    inst.label = inst.name;
                    ReadVec3(sm, "origin", inst.origin);
                    ReadVec3(sm, "angles", inst.angles);
                    inst.scale = sm.contains("scale") && sm["scale"].is_number() ? sm["scale"].get<float>() : 1.0f;
                    inst.mesh = meshFor(inst.name, inst.model);
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
                int sourceIndex{ -1 };
                for (const json& ent : list["entities"]) {
                    sourceIndex++;
                    std::string classname{ Str(ent, "classname") };
                    if (classname == "worldspawn") continue;
                    std::string targetname{ Str(ent, "targetname") };
                    // a weapon item's weapon names it
                    if (targetname.empty()) targetname = Str(ent, "classnamepayloadhash");
                    std::string noteworthy{ Str(ent, "script_noteworthy") };
                    Instance inst{};
                    inst.name = classname;
                    inst.label = classname + (targetname.empty() ? "" : " " + targetname) + (noteworthy.empty() ? "" : " (" + noteworthy + ")");
                    ReadVec3(ent, "origin", inst.origin);
                    ReadVec3(ent, "angles", inst.angles);
                    inst.sourceIndex = sourceIndex;
                    std::string model{ Str(ent, "model") };
                    inst.kind = model.empty() ? Kind::ENTITY : Kind::SCRIPT_MODEL;
                    inst.mesh = model.empty() ? -1 : meshFor(model, inst.model);
                    if (!model.empty()) inst.label += " [" + model + "]";
                    MarkerLook(classname, targetname, inst);
                    if (inst.mesh >= 0) {
                        // a model: its own look, the marker's color only in the list
                        std::copy(meshes[inst.mesh].mins, meshes[inst.mesh].mins + 3, inst.mins);
                        std::copy(meshes[inst.mesh].maxs, meshes[inst.mesh].maxs + 3, inst.maxs);
                        const float grey[3]{ 0.6f, 0.6f, 0.62f };
                        std::copy(grey, grey + 3, inst.color);
                    }
                    inst.source = entityList;
                    inst.data = ent;
                    instances.push_back(std::move(inst));
                }
            }
        }

        AddScriptModels(project, meshFor);
        AddBarrierBoards(project, meshFor);
        for (Instance& inst : instances) {
            std::copy(inst.origin, inst.origin + 3, inst.savedOrigin);
            std::copy(inst.angles, inst.angles + 3, inst.savedAngles);
            inst.savedScale = inst.scale;
        }
        LoadBindings(project);

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
