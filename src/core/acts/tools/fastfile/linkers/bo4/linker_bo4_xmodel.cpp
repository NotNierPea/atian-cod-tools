#include <includes.hpp>
#include <tools/fastfile/linkers/linker_bo4.hpp>
#include <numbers>

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
 *     "collision": "hull",      // "hull" (convex), "box", "mesh" (a hull, the bullets hit the triangles) or "none"
 *     "collisionObj": "models/my_model_coll.obj",  // the shape of the collision instead of the model, a convex
 *                               // piece per object (o or g) of it
 *     "hullMaxSides": 44,       // the planes of a hull past its bounds, the game's props stay under 46
 *     "physics": false,         // true (the game's props' preset), "#preset" or "physics/my_preset.json" (ours)
 *     "playerClip": true        // the collmap blocks the players, false by default on a physics model (jitter)
 * }
 *
 * Surfaces: one per material, cut at 0xFFFF vertices or 0x7FFF triangles, all in one buffer per lod. Lods: the game keeps the
 * least detailed first (meshes[0], the streamer falls back up) and switches by the projected size of the triangles:
 * each lod's average triangle area, or its "threshold".
 *
 * Collision, the values of the solid props of core_common: a collision surface for the bullets (script models are
 * traced against it, DObjGeomTraceline) and a collmap for the players (SV_TraceCapsuleToEntity) and the physics, a
 * brush per convex piece: the convex hull of the model (or of each object of collisionObj) or its box. The game's
 * memmapped block is the current one on pc.
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
        uint32_t object;
    };
    struct ObjData {
        std::vector<Vec3> positions{};
        std::vector<uint32_t> colors{}; // per position, 0 when the obj has none
        std::vector<std::pair<float, float>> uvs{};
        std::vector<Vec3> normals{};
        std::vector<ObjTri> tris{};
        std::vector<std::string> groups{}; // usemtl names
        uint32_t numObjects{ 1 };          // o and g lines, the convex pieces of a collision obj
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
        uint32_t object{};
        bool objectHasFaces{};
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
            } else if (op == "o" || op == "g") {
                // a new object once the current one has faces
                if (objectHasFaces) {
                    object = obj.numObjects++;
                    objectHasFaces = false;
                }
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
                    obj.tris.push_back({ { face[0], face[i], face[i - 1] }, group, object });
                }
                objectHasFaces = true;
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

    // the default preset of the game's physics props (hash_550b7918facda46d on 278 core_common props)
    constexpr const char* DEFAULT_PHYS_PRESET = "hash_550b7918facda46d";
    // what the game's physics props have in their collmaps: no player clip, a player on a moving body jitters
    constexpr uint32_t COLLMAP_PHYSICS_CONTENTS = 0x20000400;
    // the player clip bit of COLLMAP_CONTENTS
    constexpr uint32_t COLLMAP_PLAYER_CLIP = 0x10000;
    // a collmap list entry: flags, then the contents in the high dword
    constexpr uint64_t COLLMAP_LIST_FLAGS = 0x000440A0ull;

    // a plane of a brush past its bounds (the type and sign bits are 0 in the game's)
    struct BrushPlane {
        float normal[3];
        float dist;
        uint32_t typeSignbits;
    };
    static_assert(sizeof(BrushPlane) == 0x14);

    // the game's physics props stay under 46 planes, the physics hull of a brush keeps 128 points (generate_brush_chull)
    constexpr size_t DEFAULT_HULL_MAX_SIDES = 44;
    constexpr size_t MIN_HULL_SIDES = 4;
    constexpr size_t MAX_HULL_SIDES = 128;
    constexpr size_t MAX_COLLISION_PIECES = 64;
    // the directions the points a hull is made of are picked in
    constexpr size_t HULL_DIRECTIONS = 1024;
    // the triangles of a hull in one plane: within ~0.5 degree
    constexpr double HULL_COPLANAR_COS = 0.99996;
    constexpr double HULL_AXIAL_COS = 0.99999;

    /*
     * A convex piece of the collmap, a brush: the planes of its bounds (implicit), its other planes and the windings of
     * all its faces with an area (the axial ones first in the order -z, +z, -y, +x, +y, -x, then the planes'), clockwise
     * from outside. The game traces the players against the planes and makes the physics shape of the points
     * (gjk_brush_t, an aabb when there's no plane), the windings are what the game's tools write.
     */
    struct CollisionPiece {
        Vec3 mins{}, maxs{};
        std::vector<BrushPlane> planes{};
        std::vector<Vec3> verts{};
        std::vector<std::array<Vec3, 3>> tris{}; // the faces, counter clockwise from outside (bullets)
    };

    struct HVec {
        double x, y, z;

        HVec operator+(const HVec& o) const { return { x + o.x, y + o.y, z + o.z }; }
        HVec operator-(const HVec& o) const { return { x - o.x, y - o.y, z - o.z }; }
        HVec operator*(double f) const { return { x * f, y * f, z * f }; }
        double Dot(const HVec& o) const { return x * o.x + y * o.y + z * o.z; }
        HVec Cross(const HVec& o) const { return { y * o.z - z * o.y, z * o.x - x * o.z, x * o.y - y * o.x }; }
        double Length() const { return std::sqrt(Dot(*this)); }
        HVec Normalized() const {
            double l{ Length() };
            return l > 0 ? *this * (1.0 / l) : HVec{};
        }
    };

    // a first tetrahedron of points: the farthest two of the axis extremes, the farthest from their line, then from
    // their plane, the first face looking away from the fourth point; false when they are flat
    bool InitialTetra(const std::vector<HVec>& pts, double eps, std::array<int, 4>& out) {
        int n{ (int)pts.size() };
        if (n < 4) {
            return false;
        }
        int ext[6]{};
        for (int i = 0; i < n; i++) {
            const HVec& p{ pts[i] };
            if (p.x < pts[ext[0]].x) ext[0] = i;
            if (p.x > pts[ext[1]].x) ext[1] = i;
            if (p.y < pts[ext[2]].y) ext[2] = i;
            if (p.y > pts[ext[3]].y) ext[3] = i;
            if (p.z < pts[ext[4]].z) ext[4] = i;
            if (p.z > pts[ext[5]].z) ext[5] = i;
        }
        int i0{ ext[0] }, i1{ ext[1] };
        double best{ -1 };
        for (int a = 0; a < 6; a++) {
            for (int b = a + 1; b < 6; b++) {
                double d{ (pts[ext[a]] - pts[ext[b]]).Length() };
                if (d > best) {
                    best = d;
                    i0 = ext[a];
                    i1 = ext[b];
                }
            }
        }
        if (best <= eps) {
            return false;
        }
        HVec line{ (pts[i1] - pts[i0]).Normalized() };
        int i2{ -1 };
        best = eps;
        for (int i = 0; i < n; i++) {
            double d{ (pts[i] - pts[i0]).Cross(line).Length() };
            if (d > best) {
                best = d;
                i2 = i;
            }
        }
        if (i2 < 0) {
            return false;
        }
        HVec pn{ (pts[i1] - pts[i0]).Cross(pts[i2] - pts[i0]).Normalized() };
        int i3{ -1 };
        best = eps;
        for (int i = 0; i < n; i++) {
            double d{ std::abs(pn.Dot(pts[i] - pts[i0])) };
            if (d > best) {
                best = d;
                i3 = i;
            }
        }
        if (i3 < 0) {
            return false;
        }
        if (pn.Dot(pts[i3] - pts[i0]) > 0) {
            std::swap(i1, i2);
        }
        out = { i0, i1, i2, i3 };
        return true;
    }

    // triangles of the convex hull of points (incremental), counter clockwise from outside; empty when they are flat
    std::vector<std::array<int, 3>> ConvexHull(const std::vector<HVec>& pts, double eps) {
        struct Face {
            std::array<int, 3> v;
            HVec n;
            double d;
            bool alive;
        };
        std::vector<Face> faces{};
        auto MakeFace = [&](int a, int b, int c) {
            HVec n{ (pts[b] - pts[a]).Cross(pts[c] - pts[a]).Normalized() };
            faces.push_back({ { a, b, c }, n, n.Dot(pts[a]), true });
        };
        std::array<int, 4> tetra{};
        if (!InitialTetra(pts, eps, tetra)) {
            return {};
        }
        int i0{ tetra[0] }, i1{ tetra[1] }, i2{ tetra[2] }, i3{ tetra[3] };
        int n{ (int)pts.size() };
        MakeFace(i0, i1, i2);
        MakeFace(i0, i3, i1);
        MakeFace(i1, i3, i2);
        MakeFace(i2, i3, i0);

        std::vector<size_t> visible{};
        std::set<std::pair<int, int>> edges{};
        for (int p = 0; p < n; p++) {
            if (p == i0 || p == i1 || p == i2 || p == i3) {
                continue;
            }
            visible.clear();
            for (size_t f = 0; f < faces.size(); f++) {
                if (faces[f].alive && faces[f].n.Dot(pts[p]) - faces[f].d > eps) {
                    visible.push_back(f);
                }
            }
            if (visible.empty()) {
                continue; // inside
            }
            // the horizon: the edges of the visible faces without their reverse, the new faces keep their direction
            edges.clear();
            for (size_t f : visible) {
                faces[f].alive = false;
                for (size_t e = 0; e < 3; e++) {
                    edges.emplace(faces[f].v[e], faces[f].v[(e + 1) % 3]);
                }
            }
            for (const auto& [a, b] : edges) {
                if (!edges.contains({ b, a })) {
                    MakeFace(a, b, p);
                }
            }
        }
        std::vector<std::array<int, 3>> out{};
        for (const Face& f : faces) {
            if (f.alive) {
                out.push_back(f.v);
            }
        }
        return out;
    }

    // the points of pts the farthest in the directions: the axes, the diagonals and count on a fibonacci sphere
    std::vector<HVec> SupportPoints(const std::vector<HVec>& pts, size_t count) {
        std::vector<HVec> dirs{
            { 0, 0, -1 }, { 0, 0, 1 }, { 0, -1, 0 }, { 0, 1, 0 }, { -1, 0, 0 }, { 1, 0, 0 },
        };
        for (int i = 0; i < 8; i++) {
            dirs.push_back(HVec{ i & 1 ? 1.0 : -1.0, i & 2 ? 1.0 : -1.0, i & 4 ? 1.0 : -1.0 }.Normalized());
        }
        const double golden{ std::numbers::pi * (3.0 - std::sqrt(5.0)) };
        for (size_t i = 0; i < count; i++) {
            double z{ 1.0 - 2.0 * (i + 0.5) / count };
            double r{ std::sqrt(std::max(0.0, 1.0 - z * z)) };
            dirs.push_back({ std::cos(golden * i) * r, std::sin(golden * i) * r, z });
        }
        std::vector<bool> used(pts.size());
        std::vector<HVec> out{};
        for (const HVec& d : dirs) {
            size_t bi{};
            double bd{ -std::numeric_limits<double>::infinity() };
            for (size_t i = 0; i < pts.size(); i++) {
                double v{ d.Dot(pts[i]) };
                if (v > bd) {
                    bd = v;
                    bi = i;
                }
            }
            if (!used[bi]) {
                used[bi] = true;
                out.push_back(pts[bi]);
            }
        }
        return out;
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

    void GrowBounds(Vec3& mins, Vec3& maxs, const Vec3& p) {
        mins = { std::min(mins.x, p.x), std::min(mins.y, p.y), std::min(mins.z, p.z) };
        maxs = { std::max(maxs.x, p.x), std::max(maxs.y, p.y), std::max(maxs.z, p.z) };
    }

    // a box brush of the bounds of points
    CollisionPiece BoxPiece(const std::vector<Vec3>& points) {
        CollisionPiece piece{};
        piece.mins = piece.maxs = points[0];
        for (const Vec3& p : points) {
            GrowBounds(piece.mins, piece.maxs, p);
        }
        for (int corner : BOX_BRUSH_VERTS) {
            piece.verts.push_back(BoxCorner(piece.mins, piece.maxs, corner));
        }
        for (const int(&face)[4] : BOX_FACES) {
            Vec3 q[4];
            for (size_t i = 0; i < 4; i++) {
                q[i] = BoxCorner(piece.mins, piece.maxs, face[i]);
            }
            piece.tris.push_back({ q[0], q[1], q[2] });
            piece.tris.push_back({ q[0], q[2], q[3] });
        }
        return piece;
    }

    // the plane normals of the convex hull of points, its triangles merged by plane, without the axial ones (the bounds);
    // SIZE_MAX when the points are flat, else their count
    size_t HullNormals(const std::vector<HVec>& pts, double eps, std::vector<HVec>* normals) {
        static const HVec axialNormals[6]{ { 0, 0, -1 }, { 0, 0, 1 }, { 0, -1, 0 }, { 1, 0, 0 }, { 0, 1, 0 }, { -1, 0, 0 } };
        std::vector<std::array<int, 3>> tris{ ConvexHull(pts, eps) };
        if (tris.empty()) {
            return SIZE_MAX;
        }
        // the biggest triangles first, the others join the plane of one within HULL_COPLANAR_COS
        auto Area = [&pts](const std::array<int, 3>& t) {
            return (pts[t[1]] - pts[t[0]]).Cross(pts[t[2]] - pts[t[0]]).Length();
        };
        std::sort(tris.begin(), tris.end(), [&Area](const auto& a, const auto& b) { return Area(a) > Area(b); });
        std::vector<HVec> groups{}; // area weighted
        for (const std::array<int, 3>& t : tris) {
            HVec cross{ (pts[t[1]] - pts[t[0]]).Cross(pts[t[2]] - pts[t[0]]) };
            HVec n{ cross.Normalized() };
            auto it{ std::find_if(groups.begin(), groups.end(), [&n](const HVec& g) { return g.Normalized().Dot(n) > HULL_COPLANAR_COS; }) };
            if (it == groups.end()) {
                groups.push_back(cross);
            } else {
                *it = *it + cross;
            }
        }
        size_t count{};
        for (const HVec& g : groups) {
            HVec n{ g.Normalized() };
            if (std::none_of(std::begin(axialNormals), std::end(axialNormals), [&n](const HVec& a) { return n.Dot(a) > HULL_AXIAL_COS; })) {
                count++;
                if (normals) {
                    normals->push_back(n);
                }
            }
        }
        return count;
    }

    // the part of a polygon behind a plane (Sutherland-Hodgman, the points within eps kept)
    std::vector<HVec> ClipWinding(const std::vector<HVec>& w, const HVec& n, double d, double eps) {
        std::vector<HVec> out{};
        for (size_t i = 0; i < w.size(); i++) {
            const HVec& p{ w[i] };
            const HVec& q{ w[(i + 1) % w.size()] };
            double dp{ n.Dot(p) - d };
            double dq{ n.Dot(q) - d };
            if (dp <= eps) {
                out.push_back(p);
            }
            if ((dp > eps && dq < -eps) || (dp < -eps && dq > eps)) {
                out.push_back(p + (q - p) * (dp / (dp - dq)));
            }
        }
        return out;
    }

    /*
     * The brush of planes around points: the bounds of the points (the axial planes), then each normal moved to the
     * farthest point, so the brush holds all of them and lies on them. The faces are the polygon of each plane clipped
     * by all the others (a plane without one is left out), their corners welded.
     */
    void BrushOfPlanes(const std::vector<HVec>& all, const HVec& lo, const HVec& hi, const std::vector<HVec>& normals,
                       double eps, CollisionPiece& piece) {
        std::vector<std::pair<HVec, double>> planes{
            { { 0, 0, -1 }, -lo.z }, { { 0, 0, 1 }, hi.z }, { { 0, -1, 0 }, -lo.y }, { { 1, 0, 0 }, hi.x },
            { { 0, 1, 0 }, hi.y }, { { -1, 0, 0 }, -lo.x },
        };
        for (const HVec& n : normals) {
            double d{ -std::numeric_limits<double>::infinity() };
            for (const HVec& p : all) {
                d = std::max(d, n.Dot(p));
            }
            planes.emplace_back(n, d);
        }
        HVec center{ (lo + hi) * 0.5 };
        double size{ (hi - lo).Length() * 2 + 1 };
        std::vector<HVec> corners{};
        auto Weld = [&corners, eps](const HVec& p) {
            for (const HVec& c : corners) {
                if ((c - p).Length() <= eps * 10) {
                    return c;
                }
            }
            corners.push_back(p);
            return p;
        };

        piece = {};
        piece.mins = { (float)lo.x, (float)lo.y, (float)lo.z };
        piece.maxs = { (float)hi.x, (float)hi.y, (float)hi.z };
        for (size_t i = 0; i < planes.size(); i++) {
            const auto& [n, d] { planes[i] };
            // a big square on the plane, counter clockwise from outside, clipped by the other planes
            HVec u{ (std::abs(n.x) < 0.9 ? HVec{ 1, 0, 0 } : HVec{ 0, 1, 0 }).Cross(n).Normalized() };
            HVec v{ n.Cross(u) };
            HVec o{ center + n * (d - n.Dot(center)) };
            std::vector<HVec> w{ o - u * size - v * size, o + u * size - v * size, o + u * size + v * size, o - u * size + v * size };
            for (size_t j = 0; j < planes.size() && w.size() >= 3; j++) {
                if (j != i) {
                    w = ClipWinding(w, planes[j].first, planes[j].second, eps);
                }
            }
            std::vector<HVec> welded{};
            for (const HVec& p : w) {
                HVec c{ Weld(p) };
                if (welded.empty() || (welded.back() - c).Length() > 0) {
                    welded.push_back(c);
                }
            }
            while (welded.size() > 1 && (welded.back() - welded.front()).Length() == 0) {
                welded.pop_back();
            }
            HVec area{};
            for (size_t k = 2; k < welded.size(); k++) {
                area = area + (welded[k - 1] - welded[0]).Cross(welded[k] - welded[0]);
            }
            if (welded.size() < 3 || area.Length() <= eps * eps) {
                continue; // a plane past a corner or an edge
            }
            // clockwise from outside
            std::vector<Vec3> winding{};
            for (auto it = welded.rbegin(); it != welded.rend(); it++) {
                winding.push_back({ (float)it->x, (float)it->y, (float)it->z });
            }
            if (i >= 6) {
                piece.planes.push_back({ { (float)n.x, (float)n.y, (float)n.z }, (float)d, 0 });
            }
            piece.verts.insert(piece.verts.end(), winding.begin(), winding.end());
            for (size_t k = 2; k < winding.size(); k++) {
                piece.tris.push_back({ winding[0], winding[k], winding[k - 1] });
            }
        }
    }

    /*
     * The convex hull of points as a brush of at most maxSides planes past its bounds. From a tetrahedron, the point the
     * farthest out of the hull is added while it keeps few enough planes (the points tried are the farthest ones in a
     * set of directions), then its planes are moved out to hold all the points: a box stays a box, a round shape becomes
     * the polyhedron around it. False when the points are flat.
     */
    bool HullPiece(const std::vector<Vec3>& points, size_t maxSides, CollisionPiece& piece) {
        std::vector<HVec> all{};
        all.reserve(points.size());
        HVec lo{ points[0].x, points[0].y, points[0].z }, hi{ lo };
        for (const Vec3& p : points) {
            all.push_back({ p.x, p.y, p.z });
            lo = { std::min(lo.x, (double)p.x), std::min(lo.y, (double)p.y), std::min(lo.z, (double)p.z) };
            hi = { std::max(hi.x, (double)p.x), std::max(hi.y, (double)p.y), std::max(hi.z, (double)p.z) };
        }
        double eps{ std::max(1e-9, (hi - lo).Length() * 1e-6) };
        std::vector<HVec> candidates{ SupportPoints(all, HULL_DIRECTIONS) };
        std::array<int, 4> tetra{};
        if (!InitialTetra(candidates, eps, tetra)) {
            return false;
        }
        std::vector<HVec> chosen{};
        std::vector<bool> used(candidates.size());
        for (int i : tetra) {
            chosen.push_back(candidates[i]);
            used[i] = true;
        }
        for (;;) {
            // the planes of the hull, the candidate the farthest out of them
            std::vector<std::array<int, 3>> tris{ ConvexHull(chosen, eps) };
            std::vector<std::pair<HVec, double>> planes{};
            for (const std::array<int, 3>& t : tris) {
                HVec n{ (chosen[t[1]] - chosen[t[0]]).Cross(chosen[t[2]] - chosen[t[0]]).Normalized() };
                planes.emplace_back(n, n.Dot(chosen[t[0]]));
            }
            size_t bi{ SIZE_MAX };
            double bd{ eps };
            for (size_t i = 0; i < candidates.size(); i++) {
                if (used[i]) {
                    continue;
                }
                double out{ -std::numeric_limits<double>::infinity() };
                for (const auto& [n, d] : planes) {
                    out = std::max(out, n.Dot(candidates[i]) - d);
                }
                if (out > bd) {
                    bd = out;
                    bi = i;
                }
            }
            if (bi == SIZE_MAX) {
                break; // the whole hull
            }
            used[bi] = true;
            chosen.push_back(candidates[bi]);
            if (HullNormals(chosen, eps, nullptr) > maxSides) {
                chosen.pop_back();
                break;
            }
        }
        std::vector<HVec> normals{};
        if (HullNormals(chosen, eps, &normals) == SIZE_MAX) {
            return false;
        }
        BrushOfPlanes(all, lo, hi, normals, eps, piece);
        return true;
    }

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
            std::string collision{ cfg.GetString("collision", "hull") };
            std::string collisionObj{ cfg.GetString("collisionObj", "") };
            int64_t hullMaxSides{ cfg.GetInteger("hullMaxSides", (int64_t)DEFAULT_HULL_MAX_SIDES) };
            std::string normalsMode{ cfg.GetString("normals", "obj") };
            float smoothAngle{ (float)cfg.GetDouble("smoothAngle", 60.0) };
            const rapidjson::Value* colorsCfg{ Member(cfg.main, "vertexColors") };
            bool vertexColors{ !colorsCfg || !colorsCfg->IsBool() || colorsCfg->GetBool() };
            if (collision != "hull" && collision != "box" && collision != "mesh" && collision != "none") {
                Fail(std::format("unknown collision '{}', hull, box, mesh or none", collision));
                return;
            }
            if (hullMaxSides < (int64_t)MIN_HULL_SIDES || hullMaxSides > (int64_t)MAX_HULL_SIDES) {
                Fail(std::format("hullMaxSides must be {} to {}", MIN_HULL_SIDES, MAX_HULL_SIDES));
                return;
            }
            if (normalsMode != "obj" && normalsMode != "smooth" && normalsMode != "flat" && normalsMode != "angle") {
                Fail(std::format("unknown normals '{}', obj, smooth, flat or angle (smoothAngle)", normalsMode));
                return;
            }
            // physics: true for the default preset of the game's props, "#preset" or a preset json
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

            // the convex pieces of the collmap: the most detailed lod or each object of collisionObj
            std::vector<CollisionPiece> pieces{};
            if (collision != "none") {
                std::vector<std::vector<Vec3>> pieceShapes{};
                if (!collisionObj.empty()) {
                    ObjData cobj{};
                    if (!ReadObj(dir / collisionObj, upAxis == "y", scale, cobj)) {
                        Fail("can't read collisionObj");
                        return;
                    }
                    pieceShapes.resize(cobj.numObjects);
                    std::vector<bool> added(cobj.positions.size() * cobj.numObjects);
                    for (const ObjTri& t : cobj.tris) {
                        for (const ObjCorner& c : t.c) {
                            if (!added[(size_t)c.pos * cobj.numObjects + t.object]) {
                                added[(size_t)c.pos * cobj.numObjects + t.object] = true;
                                pieceShapes[t.object].push_back(cobj.positions[c.pos]);
                            }
                        }
                    }
                    std::erase_if(pieceShapes, [](const std::vector<Vec3>& p) { return p.empty(); });
                    if (pieceShapes.size() > MAX_COLLISION_PIECES) {
                        Fail(std::format("collisionObj: at most {} objects", MAX_COLLISION_PIECES));
                        return;
                    }
                } else {
                    std::vector<Vec3>& shape{ pieceShapes.emplace_back() };
                    for (const MeshSurface& s : lod0.surfs) {
                        for (const MeshVertex& v : s.verts) {
                            shape.push_back(v.pos);
                        }
                    }
                }
                for (size_t i = 0; i < pieceShapes.size(); i++) {
                    CollisionPiece& piece{ pieces.emplace_back() };
                    if (collision == "box" || !HullPiece(pieceShapes[i], (size_t)hullMaxSides, piece)) {
                        if (collision != "box") {
                            LOG_WARNING("{}: collision piece {} is flat, a box instead of a hull", assetName, i);
                        }
                        piece = BoxPiece(pieceShapes[i]);
                    }
                    if (piece.verts.size() > 0xFFFF) {
                        Fail(std::format("collision piece {}: too many vertices", i));
                        return;
                    }
                }
            }

            // collision triangles of the bullets: the pieces' faces or the triangles of the most detailed lod
            std::vector<XModelCollTri> collTris{};
            float collArea{};
            Vec3 collMins{ mins }, collMaxs{ maxs };
            auto AddCollTri = [&](const Vec3& a, const Vec3& b, const Vec3& c) {
                XModelCollTri tri;
                if (MakeCollTri(a, b, c, tri)) {
                    collTris.push_back(tri);
                    collArea += (b - a).Cross(c - a).Length() * 0.5f;
                    for (const Vec3& p : { a, b, c }) {
                        GrowBounds(collMins, collMaxs, p);
                    }
                }
            };
            if (collision == "mesh") {
                // the mesh is clockwise
                for (const MeshSurface& s : lod0.surfs) {
                    for (size_t i = 0; i < s.indices.size(); i += 3) {
                        AddCollTri(s.verts[s.indices[i]].pos, s.verts[s.indices[i + 2]].pos, s.verts[s.indices[i + 1]].pos);
                    }
                }
            } else {
                for (const CollisionPiece& piece : pieces) {
                    for (const std::array<Vec3, 3>& t : piece.tris) {
                        AddCollTri(t[0], t[1], t[2]);
                    }
                }
            }
            bool hasCollision{ !collTris.empty() && !pieces.empty() };
            // the model holds its collision (a collisionObj bigger than the mesh)
            for (const CollisionPiece& piece : pieces) {
                GrowBounds(mins, maxs, piece.mins);
                GrowBounds(mins, maxs, piece.maxs);
            }
            extent = { std::max(std::abs(mins.x), std::abs(maxs.x)), std::max(std::abs(mins.y), std::abs(maxs.y)),
                       std::max(std::abs(mins.z), std::abs(maxs.z)) };
            radius = extent.Length();
            center = (mins + maxs) * 0.5f;
            half = (maxs - mins) * 0.5f;
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
                Vec3 csMins{ collMins - Vec3{ 1, 1, 1 } * COLLSURF_BOUNDS_PADDING };
                Vec3 csMaxs{ collMaxs + Vec3{ 1, 1, 1 } * COLLSURF_BOUNDS_PADDING };
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

            // the physics preset: a json linked once in the zone, then by name, or a game asset by name
            if (physPreset.ends_with(".json")) {
                static std::unordered_map<std::string, std::string> linkedPresets{};
                if (auto it{ linkedPresets.find(physPreset) }; it != linkedPresets.end()) {
                    ctx.LinkAsset(XAssetType::ASSET_TYPE_PHYSPRESET, std::format(",{}", it->second).data(), model.physPreset, false, &ff);
                } else {
                    BO4Json pcfg{ ctx.linkCtx.input / physPreset };
                    if (!pcfg.SyncConfig(false)) {
                        Fail(std::format("can't read physics {}", physPreset));
                        return;
                    }
                    linkedPresets[physPreset] = pcfg.GetString("name", std::filesystem::path{ physPreset }.stem().string().c_str());
                    ctx.LinkAsset(XAssetType::ASSET_TYPE_PHYSPRESET, physPreset.data(), model.physPreset, false, &ff);
                }
            } else if (!physPreset.empty()) {
                std::string refId{ std::format(",{}", physPreset[0] == '#' ? physPreset.substr(1) : physPreset) };
                ctx.LinkAsset(XAssetType::ASSET_TYPE_PHYSPRESET, refId.data(), model.physPreset, false, &ff);
            }

            // collmap of the players and the physics: a brush per piece
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
                geomList.count = (uint32_t)pieces.size();
                geomList.contents = collmapContents;
                geomList.geoms = (PhysGeomInfo*)fastfile::linker::memory::POINTER_NEXT;

                // the geoms, then each brush with its planes and vertices
                ff.data.Align(8);
                PhysGeomInfo* geoms{ ff.data.AllocStreamPtr<PhysGeomInfo>(pieces.size()) };
                for (size_t i = 0; i < pieces.size(); i++) {
                    geoms[i] = {};
                    geoms[i].brush = (BrushWrapper*)fastfile::linker::memory::POINTER_NEXT;
                }
                for (const CollisionPiece& piece : pieces) {
                    ff.data.Align(0x10);
                    BrushWrapper& brush{ ff.data.AllocStreamRef<BrushWrapper>() };
                    brush = {};
                    std::memcpy(brush.mins, &piece.mins, sizeof(brush.mins));
                    std::memcpy(brush.maxs, &piece.maxs, sizeof(brush.maxs));
                    brush.contents = collmapContents;
                    brush.numsides = (byte)piece.planes.size();
                    brush.numverts = (uint16_t)piece.verts.size();
                    if (!piece.planes.empty()) {
                        brush.sides = (void*)fastfile::linker::memory::POINTER_NEXT;
                        ff.data.Align(4);
                        ff.data.WriteStream(piece.planes.data(), piece.planes.size() * sizeof(BrushPlane));
                    }
                    brush.verts = (float*)fastfile::linker::memory::POINTER_NEXT;
                    ff.data.Align(4);
                    ff.data.WriteStream(piece.verts.data(), piece.verts.size() * sizeof(Vec3));
                }

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
            for (size_t i = 0; i < pieces.size(); i++) {
                LOG_INFO("  collision piece {}: {} plane(s), {} vertices", i, pieces[i].planes.size(), pieces[i].verts.size());
            }
            for (size_t k = 1; k < numLods; k++) {
                LOG_INFO("  lod {}: {} vertices, {} triangles, switch at triangle area {}", k, lods[k].vertCount, lods[k].indexCount / 3, lods[k].threshold);
            }
        }
    };

    utils::MapAdder<XAssetLinkerImpl, XAssetType, XAssetLinker> impl{ GetWorkers(), XAssetType::ASSET_TYPE_XMODEL };
} // namespace
