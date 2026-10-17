#include <includes.hpp>
#include <tools/fastfile/linkers/linker_bo4.hpp>

/*
 * Rigid single bone xmodels from obj files, on the templates the bo4 unlinker dumps (acts ff ... -a xmodel,xmodelmesh)
 * of a game model with one bone, one lod and one rigid surface (shared flags 8 or 9): what the geometry does not
 * give is kept from them. The meshes (<name>_lod<k>) are linked in place, resident or streamed from the xpak.
 *
 * {
 *     "name": "my_model",
 *     "templateModel": "templates/xmodel.json",
 *     "templateMesh": "templates/xmodelmesh.json",
 *     "obj": "models/my_model.obj",
 *     "upAxis": "z",            // "y" for obj files with y up
 *     "scale": 1.0,
 *     "bone": "tag_origin",
 *     "material": "materials/my_material.json" or "#game_material",   // the surfaces without their own
 *     "materials": { "usemtl_name": "materials/other.json" },        // a surface per usemtl of the obj
 *     "normals": "obj",         // "obj" (smoothed by smoothAngle where it has none), "smooth", "flat", "angle"
 *     "smoothAngle": 60,
 *     "vertexColors": true,     // the colors of the obj vertices (v x y z r g b) when it has them
 *     "lods": [ { "obj": "models/my_model_lod1.obj", "threshold": 2.5, "materials": { } } ],  // less detailed each
 *     "lodScale": 0.42,         // the lod distances of the model (areaScale), the template's by default
 *     "stream": false,          // mesh data in the xpak, per lod too
 *     "collision": "box",       // "box", "mesh" (bullets hit the triangles) or "none"
 *     "physics": false,         // true (the preset of the game's props) or "#preset": physicslaunch works on it
 *     "playerClip": true        // the collmap blocks the players, false by default on a physics model (jitter)
 * }
 *
 * Surfaces: one per material, cut at 0xFFFF vertices or 0x7FFF triangles, all in one buffer per lod. Lods: the game keeps the
 * least detailed first (meshes[0], the streamer falls back up) and switches by the projected size of the triangles:
 * each lod's average triangle area, or its "threshold".
 *
 * Collision, the values of the solid props of core_common: a collision surface for the bullets (script models are
 * traced against it, DObjGeomTraceline) and a collmap with a box brush of the bounds for the players
 * (SV_TraceCapsuleToEntity). The game's memmapped block is the current one on pc.
 */
namespace {
    using namespace fastfile::linker::bo4;

    struct XModelMeshMaterials {
        uint16_t numMaterials;
        void** materials;
        float* himipInvSqRadii;
    };
    static_assert(sizeof(XModelMeshMaterials) == 0x18);

    struct XModelBoneNameIndex {
        ScrString_t name;
        uint16_t index;
        uint16_t pad;
    };
    static_assert(sizeof(XModelBoneNameIndex) == 8);

    struct XModel {
        XHash name;
        ScrString_t* boneNames;
        XModelBoneNameIndex* boneNameIndices;
        byte* parentList;
        uint64_t* quats;
        byte* trans;
        byte* partClassification;
        byte* baseMat;
        byte* boneInfo;
        void* collSurfs;
        void* physPreset;
        void* collmaps;
        void* physConstraints;
        void* submodels;
        void* meshes[8];
        XModelMeshMaterials* meshMaterials;
        byte* unkC0;
        uint32_t numLods;
        byte lodParams[0x40];
        float mins[3];
        float maxs[3];
        uint32_t unk124;
        uint32_t unk128;
        uint32_t unk12c;
        uint64_t usage;
        uint32_t contents;
        float radius;
        int32_t numCollSurfs;
        uint16_t numCosmeticBones;
        uint16_t unk146;
        int8_t highestShadowLod;
        int8_t highestRenderLod;
        int8_t numSubmodels;
        byte numBones;
        byte numRootBones;
        byte numCollmaps;
        byte unk14e;
        byte unk14f;
    };
    static_assert(sizeof(XModel) == 0x150);

    struct XRigidVertList {
        uint16_t boneOffset;
        uint16_t vertCount;
        uint16_t triOffset;
        uint16_t triCount;
    };
    static_assert(sizeof(XRigidVertList) == 8);

    struct XSurfaceShared;

    struct XSurface {
        byte tileMode;
        byte unk1;
        byte vertListCount;
        byte flags;
        uint16_t vertCount;
        uint16_t triCount;
        uint32_t baseVertIndex;
        uint32_t baseIndexOffset;
        XSurfaceShared* shared;
        XRigidVertList* vertList;
        uint64_t unk20;
        byte* partBits;
    };
    static_assert(sizeof(XSurface) == 0x30);

    struct XPakEntryInfo {
        uint64_t key;
        int64_t offset;
        uint64_t size;
        uint64_t flags;
    };
    static_assert(sizeof(XPakEntryInfo) == 0x20);

    struct XModelMesh {
        XHash name;
        XSurface* surfs;
        XSurfaceShared* shared;
        byte* partBits;
        XPakEntryInfo xpakEntry;
        float avgRenderTriArea;
        uint16_t numSurfs;
        uint16_t lodIndex; // 0 the most detailed (the last slot)
        uint64_t unk50;
    };
    static_assert(sizeof(XModelMesh) == 0x58);

    // the start of the 0x200 block, the rest is made at load (sub_143733CD0)
    struct XSurfaceSharedHeader {
        uint32_t flags;
        uint32_t vertCount;
        uint32_t skinVertCount;
        uint32_t indexCount;
        uint32_t blendTargetVertCount;
        uint32_t unk14;
        uint32_t unk18;
        uint32_t unk1c;
        byte* data;
        uint32_t dataSize;
        uint32_t posOffset;
        uint32_t vertsOffset;
        uint32_t indicesOffset;
        uint32_t skinWeightsOffset;
        uint32_t unk3c;
        uint32_t unk40;
    };

    // plane: normal, distance; s = dot(svec, p) - svec.w, t the same, inside when s, t >= 0 and s + t <= 1
    struct XModelCollTri {
        float plane[4];
        float svec[4];
        float tvec[4];
    };
    static_assert(sizeof(XModelCollTri) == 0x30);

    struct XModelCollSurf {
        XModelCollTri* collTris;
        void* blob;
        uint32_t numCollTris;
        float mins[3];
        float maxs[3];
        uint32_t boneIdx;
        uint32_t contents;
        uint32_t surfFlags;
    };
    static_assert(sizeof(XModelCollSurf) == 0x38);

    struct BrushWrapper {
        void* sides;
        float* verts;
        float mins[3];
        uint32_t contents;
        float maxs[3];
        byte unk2c[0xC];
        uint16_t numverts;
        byte numsides;
        byte unk3b[5];
    };
    static_assert(sizeof(BrushWrapper) == 0x40);

    struct PhysGeomInfo {
        BrushWrapper* brush;
        void* info;
    };
    static_assert(sizeof(PhysGeomInfo) == 0x10);

    struct PhysGeomList {
        uint32_t count;
        uint32_t contents;
        PhysGeomInfo* geoms;
        void* blob;
    };
    static_assert(sizeof(PhysGeomList) == 0x18);

    struct Collmap {
        PhysGeomList* geomList;
        uint16_t boneIndex;
        byte pad[6];
        uint32_t count;
        uint32_t unk14;
        uint64_t* list;
    };
    static_assert(sizeof(Collmap) == 0x20);

    constexpr uint32_t COLLSURF_CONTENTS = 1;
    // the player clip props of the game: the players move with the mask 0x2818011 (ClientThink_real), 0x400
    // (the other common collmap) isn't in it
    constexpr uint32_t COLLMAP_CONTENTS = 0x20010000;
    constexpr float COLLSURF_BOUNDS_PADDING = 0.001f;

    constexpr size_t XSURFACE_SHARED_SIZE = 0x200;
    constexpr size_t PART_BITS_SIZE = 0x30;
    constexpr size_t BONE_INFO_SIZE = 0x2C;
    constexpr size_t BASE_MAT_SIZE = 0x20;
    // shared flags: bit 0 streamed, bit 3 rigid, no skin weights, bits 5-8 extra streams
    constexpr uint32_t SHARED_STREAMED = 1;
    constexpr uint32_t SHARED_EXTRA_STREAMS = 0x40 | 0x80 | 0x20 | 0x100;
    constexpr size_t SHARED_DATA_OFFSET = 0x20;
    constexpr size_t SHARED_BLEND_COUNT_OFFSET = 0x1E0;

    int HexDigit(char c) {
        if (c >= '0' && c <= '9')
            return c - '0';
        if (c >= 'a' && c <= 'f')
            return c - 'a' + 10;
        if (c >= 'A' && c <= 'F')
            return c - 'A' + 10;
        return -1;
    }

    bool ReadHex(const rapidjson::Value* val, std::vector<byte>& out) {
        out.clear();
        if (!val || !val->IsString() || (val->GetStringLength() & 1)) {
            return false;
        }
        const char* str{ val->GetString() };
        for (size_t i = 0; i < val->GetStringLength(); i += 2) {
            int hi{ HexDigit(str[i]) };
            int lo{ HexDigit(str[i + 1]) };
            if (hi < 0 || lo < 0) {
                return false;
            }
            out.push_back((byte)((hi << 4) | lo));
        }
        return true;
    }

    const rapidjson::Value* Member(const rapidjson::Value& obj, const char* name) {
        if (!obj.IsObject()) {
            return nullptr;
        }
        auto it{ obj.FindMember(name) };
        return it == obj.MemberEnd() || it->value.IsNull() ? nullptr : &it->value;
    }

    struct Vec3 {
        float x, y, z;

        Vec3 operator+(const Vec3& o) const { return { x + o.x, y + o.y, z + o.z }; }
        Vec3 operator-(const Vec3& o) const { return { x - o.x, y - o.y, z - o.z }; }
        Vec3 operator*(float f) const { return { x * f, y * f, z * f }; }
        float Dot(const Vec3& o) const { return x * o.x + y * o.y + z * o.z; }
        Vec3 Cross(const Vec3& o) const { return { y * o.z - z * o.y, z * o.x - x * o.z, x * o.y - y * o.x }; }
        float Length() const { return std::sqrt(Dot(*this)); }
        Vec3 Normalized() const {
            float l{ Length() };
            return l > 1e-8f ? *this * (1.0f / l) : Vec3{ 0, 0, 1 };
        }
    };

    // triangles of a surface: 0xFFFF drew nothing in game (a signed count), the game's own stay under 0x8000
    constexpr size_t MAX_SURFACE_TRIS = 0x7FFF;

    struct MeshVertex {
        Vec3 pos;
        float u, v;
        Vec3 normal;
        uint32_t color;
        Vec3 tangent;
    };

    // a surface of the game: one material, at most 0xFFFF vertices, 16 bits indices, clockwise
    struct MeshSurface {
        std::string material;
        std::vector<MeshVertex> verts{};
        std::vector<uint16_t> indices{};
    };

    // what an obj has, the faces in the game's space and winding (triangles, clockwise)
    struct ObjCorner {
        int pos;
        int uv;
        int normal;
    };
    struct ObjTri {
        ObjCorner c[3];
        uint32_t group;
    };
    struct ObjData {
        std::vector<Vec3> positions{};
        std::vector<uint32_t> colors{}; // per position, 0 when the obj has none
        std::vector<std::pair<float, float>> uvs{};
        std::vector<Vec3> normals{};
        std::vector<ObjTri> tris{};
        std::vector<std::string> groups{}; // usemtl names
        bool hasColors{};
    };

    uint32_t PackColor(float r, float g, float b, float a) {
        auto c = [](float f) { return (uint32_t)std::lround(std::clamp(f, 0.0f, 1.0f) * 255.0f); };
        return c(r) | (c(g) << 8) | (c(b) << 16) | (c(a) << 24);
    }

    bool ReadObj(const std::filesystem::path& path, bool yUp, float scale, ObjData& obj) {
        std::string data{};
        if (!utils::ReadFile(path, data)) {
            LOG_ERROR("Can't read {}", path.string());
            return false;
        }
        auto Convert = [yUp, scale](Vec3 v) { return (yUp ? Vec3{ v.x, -v.z, v.y } : v) * scale; };
        auto Resolve = [](int idx, size_t count) { return idx < 0 ? (int)count + idx : idx - 1; };
        uint32_t group{};
        obj.groups.emplace_back();

        std::istringstream is{ data };
        std::string line;
        size_t lineIdx{};
        while (std::getline(is, line)) {
            lineIdx++;
            std::istringstream ls{ line };
            std::string op;
            ls >> op;
            if (op == "v") {
                Vec3 v{};
                ls >> v.x >> v.y >> v.z;
                obj.positions.push_back(Convert(v));
                // vertex colors as blender and others write them: v x y z r g b
                float r, g, b;
                if (ls >> r >> g >> b) {
                    obj.colors.push_back(PackColor(r, g, b, 1));
                    obj.hasColors = true;
                } else {
                    obj.colors.push_back(0xFFFFFFFF);
                }
            } else if (op == "vt") {
                float u{}, v{};
                ls >> u >> v;
                obj.uvs.emplace_back(u, v);
            } else if (op == "vn") {
                Vec3 n{};
                ls >> n.x >> n.y >> n.z;
                obj.normals.push_back(Convert(n).Normalized());
            } else if (op == "usemtl") {
                std::string name{};
                std::getline(ls >> std::ws, name);
                while (!name.empty() && (name.back() == '\r' || name.back() == ' ')) {
                    name.pop_back();
                }
                auto it{ std::find(obj.groups.begin(), obj.groups.end(), name) };
                group = (uint32_t)(it - obj.groups.begin());
                if (it == obj.groups.end()) {
                    obj.groups.push_back(name);
                }
            } else if (op == "f") {
                std::vector<ObjCorner> face{};
                std::string token;
                while (ls >> token) {
                    int vi{}, ti{ 0 }, ni{ 0 };
                    size_t s1{ token.find('/') };
                    vi = std::atoi(token.substr(0, s1).c_str());
                    if (s1 != std::string::npos) {
                        size_t s2{ token.find('/', s1 + 1) };
                        size_t tlen{ s2 == std::string::npos ? std::string::npos : s2 - s1 - 1 };
                        std::string t{ token.substr(s1 + 1, tlen) };
                        if (!t.empty())
                            ti = std::atoi(t.c_str());
                        if (s2 != std::string::npos)
                            ni = std::atoi(token.substr(s2 + 1).c_str());
                    }
                    ObjCorner c{ Resolve(vi, obj.positions.size()), ti ? Resolve(ti, obj.uvs.size()) : -1,
                                 ni ? Resolve(ni, obj.normals.size()) : -1 };
                    if (c.pos < 0 || c.pos >= (int)obj.positions.size() || c.uv >= (int)obj.uvs.size() ||
                        c.normal >= (int)obj.normals.size()) {
                        LOG_ERROR("{}:{}: bad face index", path.string(), lineIdx);
                        return false;
                    }
                    face.push_back(c);
                }
                // obj faces are counter clockwise, the game's clockwise
                for (size_t i = 2; i < face.size(); i++) {
                    obj.tris.push_back({ { face[0], face[i], face[i - 1] }, group });
                }
            }
        }
        if (obj.tris.empty()) {
            LOG_ERROR("{}: no faces", path.string());
            return false;
        }
        return true;
    }

    /*
     * The surfaces of an obj: a group per usemtl, cut in surfaces of 0xFFFF vertices. Normals: "obj" (the file's,
     * smoothed by angle where it has none), "smooth", "flat", or smoothed between faces within smoothAngle degrees.
     */
    void BuildSurfaces(const ObjData& obj, const std::string& normalsMode, float smoothAngle, bool colors,
                       std::vector<MeshSurface>& out) {
        size_t triCount{ obj.tris.size() };
        // face normals, area weighted (clockwise: the cross product of e2, e1 points out)
        std::vector<Vec3> faceNormals(triCount);
        for (size_t t = 0; t < triCount; t++) {
            const ObjTri& tri{ obj.tris[t] };
            Vec3 a{ obj.positions[tri.c[0].pos] }, b{ obj.positions[tri.c[1].pos] }, c{ obj.positions[tri.c[2].pos] };
            faceNormals[t] = (c - a).Cross(b - a);
        }
        // faces around a position, the positions welded by value (some exporters write a vertex per corner)
        std::map<std::array<uint32_t, 3>, uint32_t> weld{};
        std::vector<uint32_t> welded(obj.positions.size());
        for (size_t i = 0; i < obj.positions.size(); i++) {
            std::array<uint32_t, 3> key{};
            std::memcpy(key.data(), &obj.positions[i], sizeof(Vec3));
            welded[i] = weld.emplace(key, (uint32_t)weld.size()).first->second;
        }
        std::vector<std::vector<uint32_t>> around(weld.size());
        for (size_t t = 0; t < triCount; t++) {
            for (const ObjCorner& c : obj.tris[t].c) {
                around[welded[c.pos]].push_back((uint32_t)t);
            }
        }
        bool flat{ normalsMode == "flat" };
        bool fromObj{ normalsMode == "obj" };
        float cosLimit{ normalsMode == "smooth" ? -2.0f : std::cos(smoothAngle * 3.14159265f / 180.0f) };
        auto CornerNormal = [&](size_t t, const ObjCorner& c) {
            if (fromObj && c.normal >= 0) {
                return obj.normals[c.normal];
            }
            Vec3 fn{ faceNormals[t].Normalized() };
            if (flat) {
                return fn;
            }
            Vec3 sum{};
            for (uint32_t o : around[welded[c.pos]]) {
                if (faceNormals[o].Normalized().Dot(fn) >= cosLimit) {
                    sum = sum + faceNormals[o];
                }
            }
            return sum.Normalized();
        };

        // per group, the corners deduplicated by value, cut at 0xFFFF vertices
        for (uint32_t g = 0; g < obj.groups.size(); g++) {
            MeshSurface* surf{};
            std::map<std::array<uint32_t, 10>, uint16_t> known{};
            for (size_t t = 0; t < triCount; t++) {
                const ObjTri& tri{ obj.tris[t] };
                if (tri.group != g) {
                    continue;
                }
                // 16 bits vertex counts, the game's surfaces stay under 0x8000 triangles (31346 at most in core_common)
                if (!surf || surf->verts.size() > 0xFFFF - 3 || surf->indices.size() / 3 >= MAX_SURFACE_TRIS) {
                    surf = &out.emplace_back();
                    surf->material = obj.groups[g];
                    known.clear();
                }
                for (const ObjCorner& c : tri.c) {
                    MeshVertex v{};
                    v.pos = obj.positions[c.pos];
                    // obj v goes up, the game's goes down
                    v.u = c.uv >= 0 ? obj.uvs[c.uv].first : 0;
                    v.v = c.uv >= 0 ? 1.0f - obj.uvs[c.uv].second : 0;
                    v.normal = CornerNormal(t, c);
                    v.color = colors ? obj.colors[c.pos] : 0xFFFFFFFF;
                    std::array<uint32_t, 10> key{};
                    std::memcpy(key.data(), &v, sizeof(float) * 9);
                    key[9] = v.color;
                    auto it{ known.find(key) };
                    if (it == known.end()) {
                        surf->verts.push_back(v);
                        it = known.emplace(key, (uint16_t)(surf->verts.size() - 1)).first;
                    }
                    surf->indices.push_back(it->second);
                }
            }
        }

        // tangents from the uvs, against the normals
        for (MeshSurface& surf : out) {
            std::vector<Vec3> tangents(surf.verts.size());
            for (size_t i = 0; i < surf.indices.size(); i += 3) {
                MeshVertex& a{ surf.verts[surf.indices[i]] };
                MeshVertex& b{ surf.verts[surf.indices[i + 1]] };
                MeshVertex& c{ surf.verts[surf.indices[i + 2]] };
                Vec3 e1{ b.pos - a.pos };
                Vec3 e2{ c.pos - a.pos };
                float du1{ b.u - a.u }, dv1{ b.v - a.v }, du2{ c.u - a.u }, dv2{ c.v - a.v };
                float det{ du1 * dv2 - du2 * dv1 };
                Vec3 t{ std::abs(det) > 1e-12f ? (e1 * dv2 - e2 * dv1) * (1.0f / det) : e1 };
                for (size_t j = 0; j < 3; j++) {
                    tangents[surf.indices[i + j]] = tangents[surf.indices[i + j]] + t;
                }
            }
            for (size_t i = 0; i < surf.verts.size(); i++) {
                MeshVertex& v{ surf.verts[i] };
                Vec3 t{ tangents[i] - v.normal * v.normal.Dot(tangents[i]) };
                if (t.Dot(t) < 1e-12f) {
                    t = std::abs(v.normal.z) < 0.9f ? Vec3{ 0, 0, 1 }.Cross(v.normal) : Vec3{ 1, 0, 0 }.Cross(v.normal);
                }
                v.tangent = t.Normalized();
            }
        }
    }

    // R10G10B10A2_UNORM, n * 0.5 + 0.5
    uint32_t PackUnitVector(const Vec3& v) {
        auto c = [](float f) {
            float x{ std::clamp(f * 0.5f + 0.5f, 0.0f, 1.0f) };
            return (uint32_t)std::lround(x * 1023.0f);
        };
        return c(v.x) | (c(v.y) << 10) | (c(v.z) << 20);
    }

    uint16_t FloatToHalf(float f) {
        uint32_t x;
        std::memcpy(&x, &f, 4);
        uint32_t sign{ (x >> 16) & 0x8000 };
        int32_t exp{ (int32_t)((x >> 23) & 0xFF) - 127 + 15 };
        uint32_t mant{ x & 0x7FFFFF };
        if (exp <= 0) {
            return (uint16_t)sign;
        }
        if (exp >= 31) {
            return (uint16_t)(sign | 0x7C00);
        }
        uint32_t h{ sign | ((uint32_t)exp << 10) | (mant >> 13) };
        if (mant & 0x1000) {
            h++; // round
        }
        return (uint16_t)h;
    }

    // counter clockwise from outside, the triangles are one sided
    bool MakeCollTri(const Vec3& p0, const Vec3& p1, const Vec3& p2, XModelCollTri& tri) {
        Vec3 e1{ p1 - p0 };
        Vec3 e2{ p2 - p0 };
        Vec3 cross{ e1.Cross(e2) };
        if (cross.Dot(cross) < 1e-12f) {
            return false;
        }
        Vec3 n{ cross.Normalized() };
        Vec3 sdir{ e2.Cross(n) };
        Vec3 tdir{ n.Cross(e1) };
        sdir = sdir * (1.0f / e1.Dot(sdir));
        tdir = tdir * (1.0f / e2.Dot(tdir));
        tri = { { n.x, n.y, n.z, n.Dot(p0) },
                { sdir.x, sdir.y, sdir.z, sdir.Dot(p0) },
                { tdir.x, tdir.y, tdir.z, tdir.Dot(p0) } };
        return true;
    }

    // corner i: x max if bit 0, y if bit 1, z if bit 2
    Vec3 BoxCorner(const Vec3& mins, const Vec3& maxs, int i) {
        return { i & 1 ? maxs.x : mins.x, i & 2 ? maxs.y : mins.y, i & 4 ? maxs.z : mins.z };
    }

    // faces of the box, counter clockwise from outside
    constexpr int BOX_FACES[6][4]{ { 0, 2, 3, 1 }, { 4, 5, 7, 6 }, { 0, 1, 5, 4 },
                                   { 1, 3, 7, 5 }, { 3, 2, 6, 7 }, { 2, 0, 4, 6 } };
    // the vertices of a box brush, in the order of the game's
    constexpr int BOX_BRUSH_VERTS[24]{ 1, 3, 2, 0, 4, 6, 7, 5, 4, 5, 1, 0, 5, 7, 3, 1, 2, 3, 7, 6, 0, 2, 6, 4 };

    // the default preset of the game's physics props (hash_550b7918facda46d on 278 core_common props)
    constexpr const char* DEFAULT_PHYS_PRESET = "hash_550b7918facda46d";
    // what the game's physics props have in their collmaps: no player clip, a player on a moving body jitters
    constexpr uint32_t COLLMAP_PHYSICS_CONTENTS = 0x20000400;
    // the player clip bit of COLLMAP_CONTENTS
    constexpr uint32_t COLLMAP_PLAYER_CLIP = 0x10000;
    // a collmap list entry: flags, then the contents in the high dword
    constexpr uint64_t COLLMAP_LIST_FLAGS = 0x000440A0ull;

    // the asset name of a material json, for its references after the first (the material linker's name)
    std::string MaterialAssetName(BO4LinkContext& ctx, const std::string& material) {
        std::filesystem::path path{ ctx.linkCtx.input / material };
        BO4Json cfg{ path };
        std::filesystem::path stem{ path.filename() };
        stem.replace_extension();
        if (!cfg.SyncConfig(false)) {
            return stem.string();
        }
        return cfg.GetString("name", stem.string().c_str());
    }

    // a lod of the model: its surfaces in one buffer, as XSurfaceShared wants it
    struct LodGeom {
        std::vector<MeshSurface> surfs{};
        std::vector<std::string> materials{}; // per surface, json path or #name
        std::vector<byte> data{};
        uint32_t indicesOffset{}, posOffset{}, vertsOffset{};
        uint32_t vertCount{}, indexCount{};
        float triArea{};
        float threshold{};
        bool stream{};
    };

    // indices, positions, vertices of all the surfaces, each part on 16 bytes
    void BuildLodData(LodGeom& lod) {
        auto Append = [&lod](const void* ptr, size_t len) {
            lod.data.resize(utils::Aligned<size_t>(lod.data.size(), 0x10));
            size_t off{ lod.data.size() };
            lod.data.insert(lod.data.end(), (const byte*)ptr, (const byte*)ptr + len);
            return (uint32_t)off;
        };
        std::vector<uint16_t> indices{};
        std::vector<float> positions{};
        std::vector<uint32_t> packed{};
        double area{};
        size_t tris{};
        for (const MeshSurface& s : lod.surfs) {
            indices.insert(indices.end(), s.indices.begin(), s.indices.end());
            for (const MeshVertex& v : s.verts) {
                positions.insert(positions.end(), { v.pos.x, v.pos.y, v.pos.z });
                packed.push_back(v.color);
                packed.push_back(FloatToHalf(v.u) | ((uint32_t)FloatToHalf(v.v) << 16));
                packed.push_back(PackUnitVector(v.normal));
                packed.push_back(PackUnitVector(v.tangent));
            }
            for (size_t i = 0; i < s.indices.size(); i += 3) {
                Vec3 a{ s.verts[s.indices[i]].pos }, b{ s.verts[s.indices[i + 1]].pos }, c{ s.verts[s.indices[i + 2]].pos };
                area += (b - a).Cross(c - a).Length() * 0.5;
                tris++;
            }
        }
        lod.indicesOffset = Append(indices.data(), indices.size() * sizeof(uint16_t));
        lod.posOffset = Append(positions.data(), positions.size() * sizeof(float));
        lod.vertsOffset = Append(packed.data(), packed.size() * sizeof(uint32_t));
        lod.vertCount = (uint32_t)(positions.size() / 3);
        lod.indexCount = (uint32_t)indices.size();
        lod.triArea = tris ? (float)(area / tris) : 0;
    }

    class XAssetLinkerImpl : public XAssetLinker {
      public:
        using XAssetLinker::XAssetLinker;

        void Compute(BO4LinkContext& ctx, const char* id, BO4FFContext& ff) override {
            std::filesystem::path path{ ctx.linkCtx.input / id };
            std::filesystem::path dir{ path.parent_path() };
            std::filesystem::path rfpath{ path.filename() };
            rfpath.replace_extension();

            auto Fail = [&ctx, &path](const std::string& what) {
                LOG_ERROR("Invalid xmodel {}: {}", path.string(), what);
                ctx.error = true;
            };

            BO4Json cfg{ path };
            if (!cfg.SyncConfig(false)) {
                Fail("can't read");
                return;
            }

            std::string templateModelPath{ cfg.GetString("templateModel", "") };
            std::string templateMeshPath{ cfg.GetString("templateMesh", "") };
            BO4Json tmodel{ dir / templateModelPath };
            BO4Json tmesh{ dir / templateMeshPath };
            if (templateModelPath.empty() || !tmodel.SyncConfig(false)) {
                Fail("can't read templateModel");
                return;
            }
            if (templateMeshPath.empty() || !tmesh.SyncConfig(false)) {
                Fail("can't read templateMesh");
                return;
            }

            std::string assetName{ cfg.GetString("name", rfpath.string().c_str()) };
            std::string upAxis{ cfg.GetString("upAxis", "z") };
            float scale{ (float)cfg.GetDouble("scale", 1.0) };
            const rapidjson::Value* streamCfg{ Member(cfg.main, "stream") };
            bool stream{ streamCfg && streamCfg->IsBool() && streamCfg->GetBool() };
            std::string boneName{ cfg.GetString("bone", "tag_origin") };
            std::string collision{ cfg.GetString("collision", "box") };
            std::string normalsMode{ cfg.GetString("normals", "obj") };
            float smoothAngle{ (float)cfg.GetDouble("smoothAngle", 60.0) };
            const rapidjson::Value* colorsCfg{ Member(cfg.main, "vertexColors") };
            bool vertexColors{ !colorsCfg || !colorsCfg->IsBool() || colorsCfg->GetBool() };
            if (collision != "box" && collision != "mesh" && collision != "none") {
                Fail(std::format("unknown collision '{}'", collision));
                return;
            }
            if (normalsMode != "obj" && normalsMode != "smooth" && normalsMode != "flat" && normalsMode != "angle") {
                Fail(std::format("unknown normals '{}', obj, smooth, flat or angle (smoothAngle)", normalsMode));
                return;
            }
            // physics: true for the default preset of the game's props, or "#preset"
            const rapidjson::Value* physicsCfg{ Member(cfg.main, "physics") };
            std::string physPreset{};
            if (physicsCfg && physicsCfg->IsBool() && physicsCfg->GetBool()) {
                physPreset = DEFAULT_PHYS_PRESET;
            } else if (physicsCfg && physicsCfg->IsString()) {
                physPreset = physicsCfg->GetString();
            }
            if (!physPreset.empty() && collision == "none") {
                Fail("physics needs a collision");
                return;
            }

            // the lods: the model's obj, then "lods", from the most detailed to the least
            std::vector<const rapidjson::Value*> lodCfgs{ &cfg.main };
            const rapidjson::Value* lodsCfg{ Member(cfg.main, "lods") };
            if (lodsCfg && lodsCfg->IsArray()) {
                for (const rapidjson::Value& l : lodsCfg->GetArray()) {
                    lodCfgs.push_back(&l);
                }
            }
            if (lodCfgs.size() > 8) {
                Fail("at most 8 lods");
                return;
            }

            std::vector<LodGeom> lods(lodCfgs.size());
            for (size_t k = 0; k < lodCfgs.size(); k++) {
                const rapidjson::Value& lc{ *lodCfgs[k] };
                LodGeom& lod{ lods[k] };
                const rapidjson::Value* objCfg{ Member(lc, "obj") };
                ObjData obj{};
                if (!objCfg || !objCfg->IsString() || !ReadObj(dir / objCfg->GetString(), upAxis == "y", scale, obj)) {
                    Fail(std::format("lod {}: can't read obj", k));
                    return;
                }
                BuildSurfaces(obj, normalsMode, normalsMode == "smooth" ? 180.0f : smoothAngle, vertexColors && obj.hasColors, lod.surfs);
                if (lod.surfs.size() > 0xFF) {
                    Fail(std::format("lod {}: too many surfaces", k));
                    return;
                }
                // materials: "materials" { usemtl: material } of the lod then of the model, "material" for the rest
                for (const MeshSurface& s : lod.surfs) {
                    std::string mat{};
                    for (const rapidjson::Value* src : { &lc, static_cast<const rapidjson::Value*>(&cfg.main) }) {
                        const rapidjson::Value* mats{ Member(*src, "materials") };
                        const rapidjson::Value* m{ mats ? Member(*mats, s.material.data()) : nullptr };
                        const rapidjson::Value* m1{ Member(*src, "material") };
                        if (m && m->IsString()) {
                            mat = m->GetString();
                        } else if (m1 && m1->IsString()) {
                            mat = m1->GetString();
                        }
                        if (!mat.empty()) {
                            break;
                        }
                    }
                    if (mat.empty()) {
                        Fail(std::format("lod {}: no material for '{}'", k, s.material));
                        return;
                    }
                    lod.materials.push_back(mat);
                }
                const rapidjson::Value* lstream{ Member(lc, "stream") };
                lod.stream = lstream && lstream->IsBool() ? lstream->GetBool() : stream;
                BuildLodData(lod);
                const rapidjson::Value* threshold{ Member(lc, "threshold") };
                lod.threshold = threshold && threshold->IsNumber() && k ? (float)threshold->GetDouble() : lod.triArea;
            }
            const LodGeom& lod0{ lods[0] };

            std::vector<byte> modelHeader{}, meshHeader{}, sharedHeader{}, surfHeader{};
            std::vector<byte> meshPartBits{}, surfPartBits{}, partClassification{}, baseMat{}, boneInfo{}, unkC0{};
            std::vector<byte> meshMaterialsEntry{};

            const rapidjson::Value& tm{ tmodel.main };
            const rapidjson::Value& ts{ tmesh.main };
            const rapidjson::Value* tsurfs{ Member(ts, "surfs") };
            const rapidjson::Value* tmm{ Member(tm, "meshMaterials") };
            if (!ReadHex(Member(tm, "header"), modelHeader) || modelHeader.size() != sizeof(XModel) ||
                !ReadHex(Member(ts, "header"), meshHeader) || meshHeader.size() != sizeof(XModelMesh) ||
                !ReadHex(Member(ts, "shared"), sharedHeader) || sharedHeader.size() != XSURFACE_SHARED_SIZE ||
                !tsurfs || !tsurfs->IsArray() || tsurfs->Size() != 1 ||
                !ReadHex(Member((*tsurfs)[0], "header"), surfHeader) || surfHeader.size() != sizeof(XSurface) ||
                !ReadHex(Member(tm, "baseMat"), baseMat) || baseMat.size() != BASE_MAT_SIZE ||
                !ReadHex(Member(tm, "boneInfo"), boneInfo) || boneInfo.size() != BONE_INFO_SIZE ||
                !ReadHex(Member(tm, "partClassification"), partClassification) || partClassification.size() != 1 ||
                !tmm || !tmm->IsArray() || tmm->Size() != 1 ||
                !ReadHex(Member((*tmm)[0], "entry"), meshMaterialsEntry) ||
                meshMaterialsEntry.size() != sizeof(XModelMeshMaterials)) {
                Fail("the templates are not of a one bone, one lod, one surface model");
                return;
            }
            ReadHex(Member(ts, "partBits"), meshPartBits);
            ReadHex(Member((*tsurfs)[0], "partBits"), surfPartBits);
            ReadHex(Member(tm, "unkC0"), unkC0);

            XSurfaceSharedHeader& tshared{ *(XSurfaceSharedHeader*)sharedHeader.data() };
            if ((tshared.flags & SHARED_EXTRA_STREAMS) || !(tshared.flags & 8)) {
                Fail(std::format("the template mesh isn't rigid with 16 bytes vertices (flags 0x{:x})", tshared.flags));
                return;
            }

            // bounds of the most detailed lod
            Vec3 mins{ lod0.surfs[0].verts[0].pos }, maxs{ mins };
            for (const MeshSurface& s : lod0.surfs) {
                for (const MeshVertex& v : s.verts) {
                    mins = { std::min(mins.x, v.pos.x), std::min(mins.y, v.pos.y), std::min(mins.z, v.pos.z) };
                    maxs = { std::max(maxs.x, v.pos.x), std::max(maxs.y, v.pos.y), std::max(maxs.z, v.pos.z) };
                }
            }
            Vec3 extent{ std::max(std::abs(mins.x), std::abs(maxs.x)), std::max(std::abs(mins.y), std::abs(maxs.y)),
                         std::max(std::abs(mins.z), std::abs(maxs.z)) };
            float radius{ extent.Length() };
            Vec3 center{ (mins + maxs) * 0.5f };
            Vec3 half{ (maxs - mins) * 0.5f };

            // collision triangles: the box of the bounds or the triangles of the most detailed lod
            std::vector<XModelCollTri> collTris{};
            float collArea{};
            auto AddCollTri = [&collTris, &collArea](const Vec3& a, const Vec3& b, const Vec3& c) {
                XModelCollTri tri;
                if (MakeCollTri(a, b, c, tri)) {
                    collTris.push_back(tri);
                    collArea += (b - a).Cross(c - a).Length() * 0.5f;
                }
            };
            if (collision == "box") {
                for (const int(&face)[4] : BOX_FACES) {
                    Vec3 q[4];
                    for (size_t i = 0; i < 4; i++) {
                        q[i] = BoxCorner(mins, maxs, face[i]);
                    }
                    AddCollTri(q[0], q[1], q[2]);
                    AddCollTri(q[0], q[2], q[3]);
                }
            } else if (collision == "mesh") {
                // the mesh is clockwise
                for (const MeshSurface& s : lod0.surfs) {
                    for (size_t i = 0; i < s.indices.size(); i += 3) {
                        AddCollTri(s.verts[s.indices[i]].pos, s.verts[s.indices[i + 2]].pos, s.verts[s.indices[i + 1]].pos);
                    }
                }
            }
            bool hasCollision{ !collTris.empty() };
            if (hasCollision) {
                collArea /= (float)collTris.size();
            }
            // a static prop blocks the players, a physics prop has the game's props' contents ("playerClip" to block)
            const rapidjson::Value* playerClipCfg{ Member(cfg.main, "playerClip") };
            bool playerClip{ playerClipCfg && playerClipCfg->IsBool() ? playerClipCfg->GetBool() : physPreset.empty() };
            uint32_t collmapContents{ (physPreset.empty() ? COLLMAP_CONTENTS : COLLMAP_PHYSICS_CONTENTS) |
                                      (playerClip ? COLLMAP_PLAYER_CLIP : 0) };

            // Load_XModel: header, bones, meshes, materials, collision surfaces, bone info, unkC0, collmaps
            ff.data.PushStream(XFILE_BLOCK_TEMP);
            XModel& model{ ff.data.AllocStreamRef<XModel>() };
            std::memcpy(&model, modelHeader.data(), sizeof(model));
            model.name.name = ctx.HashXHash(assetName, true);
            model.numBones = 1;
            model.numRootBones = 1;
            model.numCosmeticBones = 0;
            model.numCollSurfs = hasCollision ? 1 : 0;
            model.numCollmaps = hasCollision ? 1 : 0;
            model.contents = hasCollision ? COLLSURF_CONTENTS : 0;
            // the average area of the collision triangles, a trace skips a model under a minimum
            std::memcpy(&model.unk128, &collArea, sizeof(model.unk128));
            model.numSubmodels = 0;
            model.parentList = nullptr;
            model.quats = nullptr;
            model.trans = nullptr;
            model.collSurfs = nullptr;
            model.physPreset = nullptr;
            model.collmaps = nullptr;
            model.physConstraints = nullptr;
            model.submodels = nullptr;
            std::memcpy(model.mins, &mins, sizeof(model.mins));
            std::memcpy(model.maxs, &maxs, sizeof(model.maxs));
            model.radius = radius;

            // lods: the least detailed first (the game's order, the streamer falls back up), the triangle area of each
            // as the size it switches at (GfxLodModelParams), areaScale the model's lod distance scale
            size_t numLods{ lods.size() };
            model.numLods = (uint32_t)numLods;
            float* lodParams{ (float*)model.lodParams };
            for (size_t slot = 0; slot < 8; slot++) {
                lodParams[slot] = slot < numLods ? lods[numLods - 1 - slot].threshold : 0;
            }
            const rapidjson::Value* lodScale{ Member(cfg.main, "lodScale") };
            if (lodScale && lodScale->IsNumber()) {
                lodParams[8] = (float)lodScale->GetDouble();
            }
            model.highestRenderLod = (int8_t)(numLods - 1);
            model.highestShadowLod = (int8_t)(numLods - 1);

            ff.data.PushStream(XFILE_BLOCK_VIRTUAL);

            // bones
            model.boneNames = (ScrString_t*)fastfile::linker::memory::POINTER_NEXT;
            ff.data.Align(4);
            ScrString_t& bone{ ff.data.AllocStreamRef<ScrString_t>() };
            ctx.AddScrString(boneName, bone, ff);

            model.boneNameIndices = (XModelBoneNameIndex*)fastfile::linker::memory::POINTER_NEXT;
            ff.data.Align(4);
            XModelBoneNameIndex& boneIndex{ ff.data.AllocStreamRef<XModelBoneNameIndex>() };
            boneIndex = {};
            ctx.AddScrString(boneName, boneIndex.name, ff);

            model.partClassification = (byte*)fastfile::linker::memory::POINTER_NEXT;
            ff.data.WriteStream(partClassification.data(), partClassification.size());

            model.baseMat = (byte*)fastfile::linker::memory::POINTER_NEXT;
            ff.data.Align(0x10);
            ff.data.WriteStream(baseMat.data(), baseMat.size());

            // meshes in place, slot s the lod numLods - 1 - s, named <name>_lod<k>
            for (size_t slot = 0; slot < std::size(model.meshes); slot++) {
                if (slot >= numLods) {
                    model.meshes[slot] = nullptr;
                    continue;
                }
                size_t k{ numLods - 1 - slot };
                const LodGeom& lod{ lods[k] };
                model.meshes[slot] = (void*)fastfile::linker::memory::POINTER_NEXT;
                ff.data.PushStream(XFILE_BLOCK_TEMP);
                ff.data.Align(8);
                XModelMesh& mesh{ ff.data.AllocStreamRef<XModelMesh>() };
                std::memcpy(&mesh, meshHeader.data(), sizeof(mesh));
                mesh.name.name = ctx.HashXHash(std::format("{}_lod{}", assetName, k), true);
                mesh.numSurfs = (uint16_t)lod.surfs.size();
                mesh.avgRenderTriArea = lod.triArea;
                mesh.lodIndex = (uint16_t)k;
                mesh.xpakEntry = {};

                ff.data.PushStream(XFILE_BLOCK_VIRTUAL);

                if (meshPartBits.size() == PART_BITS_SIZE) {
                    mesh.partBits = (byte*)fastfile::linker::memory::POINTER_NEXT;
                    ff.data.Align(4);
                    ff.data.WriteStream(meshPartBits.data(), meshPartBits.size());
                } else {
                    mesh.partBits = nullptr;
                }

                // shared block of all the surfaces, they point to it
                mesh.shared = (XSurfaceShared*)fastfile::linker::memory::POINTER_NEXT;
                ff.data.Align(0x10);
                fastfile::linker::memory::LinkerDataChunk* sharedChunk{};
                byte* shared{ ff.data.AllocStreamPtr<byte>(XSURFACE_SHARED_SIZE, &sharedChunk) };
                std::memcpy(shared, sharedHeader.data(), XSURFACE_SHARED_SIZE);
                XSurfaceSharedHeader& sh{ *(XSurfaceSharedHeader*)shared };
                sh.flags = (sh.flags & ~SHARED_STREAMED) | (lod.stream ? SHARED_STREAMED : 0);
                sh.vertCount = lod.vertCount;
                sh.skinVertCount = 0;
                sh.indexCount = lod.indexCount;
                sh.blendTargetVertCount = 0;
                sh.dataSize = (uint32_t)lod.data.size();
                sh.posOffset = lod.posOffset;
                sh.vertsOffset = lod.vertsOffset;
                sh.indicesOffset = lod.indicesOffset;
                sh.skinWeightsOffset = 0;
                sh.data = (byte*)fastfile::linker::memory::POINTER_NEXT;
                // no blend targets
                std::memset(shared + SHARED_BLEND_COUNT_OFFSET, 0, XSURFACE_SHARED_SIZE - SHARED_BLEND_COUNT_OFFSET);

                if (lod.stream) {
                    // the xpak keys of the game's streamed meshes all have the top bit
                    mesh.xpakEntry.key = ::hash::Hash64(std::format("{}/mesh_lod{}", assetName, k).data()) | 0x8000000000000000ull;
                    ff.data.PushStream(XFILE_BLOCK_STREAMER);
                    ff.data.Align(0x100);
                    ff.data.AllocRuntime(lod.data.size());
                    ff.data.PopStream();
                    AddXPakItem(ff, mesh.xpakEntry.key, std::vector<byte>{ lod.data });
                } else {
                    ff.data.PushStream(XFILE_BLOCK_PHYSICAL);
                    ff.data.Align(0x100);
                    ff.data.WriteStream(lod.data.data(), lod.data.size());
                    ff.data.PopStream();
                }

                // surfaces, then for each its part bits and vertex list
                mesh.surfs = (XSurface*)fastfile::linker::memory::POINTER_NEXT;
                ff.data.Align(8);
                XSurface* surfs{ ff.data.AllocStreamPtr<XSurface>(lod.surfs.size()) };
                uint32_t baseVert{}, baseIndex{};
                for (size_t i = 0; i < lod.surfs.size(); i++) {
                    XSurface& surf{ surfs[i] };
                    std::memcpy(&surf, surfHeader.data(), sizeof(surf));
                    surf.vertListCount = 1;
                    surf.vertCount = (uint16_t)lod.surfs[i].verts.size();
                    surf.triCount = (uint16_t)(lod.surfs[i].indices.size() / 3);
                    surf.baseVertIndex = baseVert;
                    surf.baseIndexOffset = baseIndex;
                    surf.unk20 = 0;
                    baseVert += surf.vertCount;
                    baseIndex += (uint32_t)lod.surfs[i].indices.size();
                    // offset of the shared block in the virtual block, set at link
                    surf.shared = nullptr;
                    sharedChunk->refs.push_back((void**)&surf.shared);
                    surf.partBits = surfPartBits.size() == PART_BITS_SIZE ? (byte*)fastfile::linker::memory::POINTER_NEXT : nullptr;
                    surf.vertList = (XRigidVertList*)fastfile::linker::memory::POINTER_NEXT;
                }
                for (size_t i = 0; i < lod.surfs.size(); i++) {
                    if (surfs[i].partBits) {
                        ff.data.Align(4);
                        ff.data.WriteStream(surfPartBits.data(), surfPartBits.size());
                    }
                    ff.data.Align(2);
                    XRigidVertList& vertList{ ff.data.AllocStreamRef<XRigidVertList>() };
                    vertList = { 0, surfs[i].vertCount, 0, surfs[i].triCount };
                }

                ff.data.PopStream();
                ff.data.PopStream();
            }

            // materials per lod, in the slots' order: the pointers, the materials, their himip radii
            model.meshMaterials = (XModelMeshMaterials*)fastfile::linker::memory::POINTER_NEXT;
            ff.data.Align(8);
            XModelMeshMaterials* mms{ ff.data.AllocStreamPtr<XModelMeshMaterials>(numLods) };
            for (size_t slot = 0; slot < numLods; slot++) {
                std::memcpy(&mms[slot], meshMaterialsEntry.data(), sizeof(mms[slot]));
                mms[slot].numMaterials = (uint16_t)lods[numLods - 1 - slot].surfs.size();
                mms[slot].materials = (void**)fastfile::linker::memory::POINTER_NEXT;
                mms[slot].himipInvSqRadii = (float*)fastfile::linker::memory::POINTER_NEXT;
            }
            // a material of a json once in the model, then by its name
            std::unordered_set<std::string> linked{};
            for (size_t slot = 0; slot < numLods; slot++) {
                const LodGeom& lod{ lods[numLods - 1 - slot] };
                ff.data.Align(8);
                void** mats{ ff.data.AllocStreamPtr<void*>(lod.materials.size()) };
                for (size_t i = 0; i < lod.materials.size(); i++) {
                    const std::string& material{ lod.materials[i] };
                    if (material[0] == '#') {
                        std::string refId{ std::format(",{}", material.substr(1)) };
                        ctx.LinkAsset(XAssetType::ASSET_TYPE_MATERIAL, refId.data(), mats[i], false, &ff);
                    } else if (linked.contains(material)) {
                        std::string refId{ std::format(",{}", MaterialAssetName(ctx, material)) };
                        ctx.LinkAsset(XAssetType::ASSET_TYPE_MATERIAL, refId.data(), mats[i], false, &ff);
                    } else {
                        linked.insert(material);
                        ctx.LinkAsset(XAssetType::ASSET_TYPE_MATERIAL, material.data(), mats[i], false, &ff);
                    }
                }
                ff.data.Align(4);
                for (size_t i = 0; i < lod.materials.size(); i++) {
                    ff.data.WriteStream<float>(radius > 0 ? 1.0f / (radius * radius) : 1.0f);
                }
            }

            // collision surface of the bullets
            if (hasCollision) {
                model.collSurfs = (void*)fastfile::linker::memory::POINTER_NEXT;
                ff.data.Align(8);
                XModelCollSurf& cs{ ff.data.AllocStreamRef<XModelCollSurf>() };
                cs = {};
                cs.collTris = (XModelCollTri*)fastfile::linker::memory::POINTER_NEXT;
                cs.numCollTris = (uint32_t)collTris.size();
                Vec3 csMins{ mins - Vec3{ 1, 1, 1 } * COLLSURF_BOUNDS_PADDING };
                Vec3 csMaxs{ maxs + Vec3{ 1, 1, 1 } * COLLSURF_BOUNDS_PADDING };
                std::memcpy(cs.mins, &csMins, sizeof(cs.mins));
                std::memcpy(cs.maxs, &csMaxs, sizeof(cs.maxs));
                cs.boneIdx = 0;
                cs.contents = COLLSURF_CONTENTS;
                // DB_PushStreamPos(XFILE_BLOCK_MEMMAPPED) keeps the current block on pc (DB_EnableMemMappedData)
                ff.data.Align(4);
                ff.data.WriteStream(collTris.data(), collTris.size() * sizeof(collTris[0]));
            }

            // bone info: mins, maxs, center, radius squared, the rest from the template
            model.boneInfo = (byte*)fastfile::linker::memory::POINTER_NEXT;
            ff.data.Align(4);
            float* bi{ (float*)boneInfo.data() };
            std::memcpy(&bi[0], &mins, sizeof(mins));
            std::memcpy(&bi[3], &maxs, sizeof(maxs));
            std::memcpy(&bi[6], &center, sizeof(center));
            bi[9] = half.Dot(half);
            ff.data.WriteStream(boneInfo.data(), boneInfo.size());

            if (unkC0.size() == 0x30) {
                model.unkC0 = (byte*)fastfile::linker::memory::POINTER_NEXT;
                ff.data.Align(4);
                ff.data.WriteStream(unkC0.data(), unkC0.size());
            } else {
                model.unkC0 = nullptr;
            }

            // the physics preset, a game asset by name
            if (!physPreset.empty()) {
                std::string refId{ std::format(",{}", physPreset[0] == '#' ? physPreset.substr(1) : physPreset) };
                ctx.LinkAsset(XAssetType::ASSET_TYPE_PHYSPRESET, refId.data(), model.physPreset, false, &ff);
            }

            // collmap of the players: one box brush
            if (hasCollision) {
                model.collmaps = (void*)fastfile::linker::memory::POINTER_NEXT;
                ff.data.Align(8);
                Collmap& cm{ ff.data.AllocStreamRef<Collmap>() };
                cm = {};
                cm.boneIndex = 0xFFFF;
                cm.count = 1;
                cm.geomList = (PhysGeomList*)fastfile::linker::memory::POINTER_NEXT;
                cm.list = (uint64_t*)fastfile::linker::memory::POINTER_NEXT;

                ff.data.Align(8);
                PhysGeomList& geomList{ ff.data.AllocStreamRef<PhysGeomList>() };
                geomList = {};
                geomList.count = 1;
                geomList.contents = collmapContents;
                geomList.geoms = (PhysGeomInfo*)fastfile::linker::memory::POINTER_NEXT;

                ff.data.Align(8);
                PhysGeomInfo& geom{ ff.data.AllocStreamRef<PhysGeomInfo>() };
                geom = {};
                geom.brush = (BrushWrapper*)fastfile::linker::memory::POINTER_NEXT;

                ff.data.Align(0x10);
                BrushWrapper& brush{ ff.data.AllocStreamRef<BrushWrapper>() };
                brush = {};
                std::memcpy(brush.mins, &mins, sizeof(brush.mins));
                std::memcpy(brush.maxs, &maxs, sizeof(brush.maxs));
                brush.contents = collmapContents;
                brush.numverts = (uint16_t)std::size(BOX_BRUSH_VERTS);
                brush.verts = (float*)fastfile::linker::memory::POINTER_NEXT;
                std::vector<float> verts{};
                for (int corner : BOX_BRUSH_VERTS) {
                    Vec3 v{ BoxCorner(mins, maxs, corner) };
                    verts.insert(verts.end(), { v.x, v.y, v.z });
                }
                ff.data.Align(4);
                ff.data.WriteStream(verts.data(), verts.size() * sizeof(float));

                ff.data.Align(4);
                ff.data.WriteStream<uint64_t>(((uint64_t)collmapContents << 32) | COLLMAP_LIST_FLAGS);
            }

            ff.data.PopStream();

            ff.data.PopStream();

            size_t totalSurfs{};
            for (const LodGeom& lod : lods) {
                totalSurfs += lod.surfs.size();
            }
            LOG_INFO(
                "Added asset xmodel {} (hash_{:x}), {} vertices, {} triangles, {} surface(s), {} lod(s){}{}", assetName,
                model.name.name, lod0.vertCount, lod0.indexCount / 3, lod0.surfs.size(), numLods,
                lod0.stream ? ", streamed mesh" : "", physPreset.empty() ? "" : ", physics"
            );
            for (size_t k = 1; k < numLods; k++) {
                LOG_INFO("  lod {}: {} vertices, {} triangles, switch at triangle area {}", k, lods[k].vertCount, lods[k].indexCount / 3, lods[k].threshold);
            }
        }
    };

    utils::MapAdder<XAssetLinkerImpl, XAssetType, XAssetLinker> impl{ GetWorkers(), XAssetType::ASSET_TYPE_XMODEL };
} // namespace
