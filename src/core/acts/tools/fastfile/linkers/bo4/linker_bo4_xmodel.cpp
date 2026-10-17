#include <includes.hpp>
#include <tools/fastfile/linkers/linker_bo4.hpp>

/*
 * Rigid single bone xmodels from an obj, on the templates the bo4 unlinker dumps (acts ff ... -a xmodel,xmodelmesh)
 * of a game model with one bone, one lod and one rigid surface (shared flags 8 or 9): what the geometry does not
 * give is kept from them. The mesh (<name>_lod0) is linked in place, resident or streamed from the xpak.
 *
 * {
 *     "name": "my_model",
 *     "templateModel": "templates/xmodel.json",
 *     "templateMesh": "templates/xmodelmesh.json",
 *     "obj": "models/my_model.obj",
 *     "upAxis": "z",            // "y" for obj files with y up
 *     "scale": 1.0,
 *     "bone": "tag_origin",
 *     "material": "materials/my_material.json" or "#game_material",
 *     "stream": false,          // mesh data in the xpak
 *     "collision": "box"        // "box", "mesh" (bullets hit the triangles) or "none"
 * }
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
        uint16_t unk4e;
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
    constexpr uint64_t COLLMAP_LIST_ENTRY = 0x20010000000440A0ull;
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

    struct MeshVertex {
        Vec3 pos;
        float u, v;
        Vec3 normal;
        Vec3 tangent;
    };

    struct ObjMesh {
        std::vector<MeshVertex> verts{};
        std::vector<uint16_t> indices{};
    };

    // triangulated, deduplicated, in the game's space and winding
    bool ReadObj(const std::filesystem::path& path, bool yUp, float scale, ObjMesh& mesh) {
        std::string data{};
        if (!utils::ReadFile(path, data)) {
            LOG_ERROR("Can't read {}", path.string());
            return false;
        }

        std::vector<Vec3> positions{};
        std::vector<std::pair<float, float>> uvs{};
        std::vector<Vec3> normals{};
        // position, uv, normal
        using VertexKey = std::array<uint32_t, 8>;
        std::map<VertexKey, uint16_t> known{};

        auto Convert = [yUp, scale](Vec3 v) { return (yUp ? Vec3{ v.x, -v.z, v.y } : v) * scale; };
        auto Resolve = [](int idx, size_t count) { return idx < 0 ? (int)count + idx : idx - 1; };

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
                positions.push_back(Convert(v));
            } else if (op == "vt") {
                float u{}, v{};
                ls >> u >> v;
                uvs.emplace_back(u, v);
            } else if (op == "vn") {
                Vec3 n{};
                ls >> n.x >> n.y >> n.z;
                normals.push_back(Convert(n).Normalized());
            } else if (op == "f") {
                std::vector<uint16_t> face{};
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
                    int p{ Resolve(vi, positions.size()) };
                    int t{ ti ? Resolve(ti, uvs.size()) : -1 };
                    int n{ ni ? Resolve(ni, normals.size()) : -1 };
                    if (p < 0 || p >= (int)positions.size() || t >= (int)uvs.size() || n >= (int)normals.size()) {
                        LOG_ERROR("{}:{}: bad face index", path.string(), lineIdx);
                        return false;
                    }
                    MeshVertex vert{};
                    vert.pos = positions[p];
                    // obj v goes up, the game's goes down
                    vert.u = t >= 0 ? uvs[t].first : 0;
                    vert.v = t >= 0 ? 1.0f - uvs[t].second : 0;
                    vert.normal = n >= 0 ? normals[n] : Vec3{};

                    // welded by value, exporters like wraith write 3 vertices per triangle
                    VertexKey key{};
                    std::memcpy(key.data(), &vert, sizeof(float) * key.size());
                    auto it{ known.find(key) };
                    if (it == known.end()) {
                        if (mesh.verts.size() >= 0xFFFF) {
                            LOG_ERROR("{}: too many vertices", path.string());
                            return false;
                        }
                        mesh.verts.push_back(vert);
                        it = known.emplace(key, (uint16_t)(mesh.verts.size() - 1)).first;
                    }
                    face.push_back(it->second);
                }
                // obj faces are counter clockwise, the game's clockwise
                for (size_t i = 2; i < face.size(); i++) {
                    mesh.indices.push_back(face[0]);
                    mesh.indices.push_back(face[i]);
                    mesh.indices.push_back(face[i - 1]);
                }
            }
        }

        if (mesh.indices.empty()) {
            LOG_ERROR("{}: no faces", path.string());
            return false;
        }

        // face normals where the obj has none, tangents from the uvs
        std::vector<Vec3> faceNormals(mesh.verts.size());
        std::vector<Vec3> tangents(mesh.verts.size());
        for (size_t i = 0; i < mesh.indices.size(); i += 3) {
            MeshVertex& a{ mesh.verts[mesh.indices[i]] };
            MeshVertex& b{ mesh.verts[mesh.indices[i + 1]] };
            MeshVertex& c{ mesh.verts[mesh.indices[i + 2]] };
            Vec3 e1{ b.pos - a.pos };
            Vec3 e2{ c.pos - a.pos };
            // clockwise: the cross product points inside
            Vec3 fn{ e2.Cross(e1) };
            float du1{ b.u - a.u }, dv1{ b.v - a.v }, du2{ c.u - a.u }, dv2{ c.v - a.v };
            float det{ du1 * dv2 - du2 * dv1 };
            Vec3 t{ std::abs(det) > 1e-12f ? (e1 * dv2 - e2 * dv1) * (1.0f / det) : e1 };
            for (size_t j = 0; j < 3; j++) {
                faceNormals[mesh.indices[i + j]] = faceNormals[mesh.indices[i + j]] + fn;
                tangents[mesh.indices[i + j]] = tangents[mesh.indices[i + j]] + t;
            }
        }
        for (size_t i = 0; i < mesh.verts.size(); i++) {
            MeshVertex& v{ mesh.verts[i] };
            if (v.normal.Dot(v.normal) < 0.5f) {
                v.normal = faceNormals[i].Normalized();
            }
            // gram-schmidt against the normal
            Vec3 t{ tangents[i] - v.normal * v.normal.Dot(tangents[i]) };
            if (t.Dot(t) < 1e-12f) {
                t = std::abs(v.normal.z) < 0.9f ? Vec3{ 0, 0, 1 }.Cross(v.normal) : Vec3{ 1, 0, 0 }.Cross(v.normal);
            }
            v.tangent = t.Normalized();
        }
        return true;
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
            std::string objPath{ cfg.GetString("obj", "") };
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
            std::string material{ cfg.GetString("material", "") };
            std::string collision{ cfg.GetString("collision", "box") };
            if (collision != "box" && collision != "mesh" && collision != "none") {
                Fail(std::format("unknown collision '{}'", collision));
                return;
            }

            ObjMesh obj{};
            if (objPath.empty() || !ReadObj(dir / objPath, upAxis == "y", scale, obj)) {
                Fail("can't read obj");
                return;
            }

            std::vector<byte> buff{};
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

            // indices, positions, vertices, each on 16 bytes
            std::vector<byte> data{};
            auto Append = [&data](const void* ptr, size_t len) {
                data.resize(utils::Aligned<size_t>(data.size(), 0x10));
                size_t off{ data.size() };
                data.insert(data.end(), (const byte*)ptr, (const byte*)ptr + len);
                return (uint32_t)off;
            };
            uint32_t indicesOffset{ Append(obj.indices.data(), obj.indices.size() * sizeof(uint16_t)) };
            std::vector<float> positions{};
            for (const MeshVertex& v : obj.verts) {
                positions.insert(positions.end(), { v.pos.x, v.pos.y, v.pos.z });
            }
            uint32_t posOffset{ Append(positions.data(), positions.size() * sizeof(float)) };
            std::vector<uint32_t> packed{};
            for (const MeshVertex& v : obj.verts) {
                packed.push_back(0xFFFFFFFF); // color
                packed.push_back(FloatToHalf(v.u) | ((uint32_t)FloatToHalf(v.v) << 16));
                packed.push_back(PackUnitVector(v.normal));
                packed.push_back(PackUnitVector(v.tangent));
            }
            uint32_t vertsOffset{ Append(packed.data(), packed.size() * sizeof(uint32_t)) };

            // bounds
            Vec3 mins{ obj.verts[0].pos }, maxs{ obj.verts[0].pos };
            for (const MeshVertex& v : obj.verts) {
                mins = { std::min(mins.x, v.pos.x), std::min(mins.y, v.pos.y), std::min(mins.z, v.pos.z) };
                maxs = { std::max(maxs.x, v.pos.x), std::max(maxs.y, v.pos.y), std::max(maxs.z, v.pos.z) };
            }
            Vec3 extent{ std::max(std::abs(mins.x), std::abs(maxs.x)), std::max(std::abs(mins.y), std::abs(maxs.y)),
                      std::max(std::abs(mins.z), std::abs(maxs.z)) };
            float radius{ extent.Length() };
            Vec3 center{ (mins + maxs) * 0.5f };
            Vec3 half{ (maxs - mins) * 0.5f };
            float triArea{};
            for (size_t i = 0; i < obj.indices.size(); i += 3) {
                Vec3 a{ obj.verts[obj.indices[i]].pos };
                Vec3 b{ obj.verts[obj.indices[i + 1]].pos };
                Vec3 c{ obj.verts[obj.indices[i + 2]].pos };
                triArea += (b - a).Cross(c - a).Length() * 0.5f;
            }
            triArea /= (float)(obj.indices.size() / 3);

            uint16_t vertCount{ (uint16_t)obj.verts.size() };
            uint16_t triCount{ (uint16_t)(obj.indices.size() / 3) };

            // collision triangles: the box of the bounds or the triangles of the mesh
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
                for (size_t i = 0; i < obj.indices.size(); i += 3) {
                    AddCollTri(
                        obj.verts[obj.indices[i]].pos, obj.verts[obj.indices[i + 2]].pos,
                        obj.verts[obj.indices[i + 1]].pos
                    );
                }
            }
            bool hasCollision{ !collTris.empty() };
            if (hasCollision) {
                collArea /= (float)collTris.size();
            }

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
            model.numLods = 1;
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

            // meshes, the mesh of the lod 0 in place
            for (size_t i = 0; i < std::size(model.meshes); i++) {
                if (i) {
                    model.meshes[i] = nullptr;
                    continue;
                }
                model.meshes[i] = (void*)fastfile::linker::memory::POINTER_NEXT;
                ff.data.PushStream(XFILE_BLOCK_TEMP);
                ff.data.Align(8);
                XModelMesh& mesh{ ff.data.AllocStreamRef<XModelMesh>() };
                std::memcpy(&mesh, meshHeader.data(), sizeof(mesh));
                mesh.name.name = ctx.HashXHash(std::format("{}_lod0", assetName), true);
                mesh.numSurfs = 1;
                mesh.avgRenderTriArea = triArea;
                mesh.xpakEntry = {};

                ff.data.PushStream(XFILE_BLOCK_VIRTUAL);

                if (meshPartBits.size() == PART_BITS_SIZE) {
                    mesh.partBits = (byte*)fastfile::linker::memory::POINTER_NEXT;
                    ff.data.Align(4);
                    ff.data.WriteStream(meshPartBits.data(), meshPartBits.size());
                } else {
                    mesh.partBits = nullptr;
                }

                // shared block, the surface points to it
                mesh.shared = (XSurfaceShared*)fastfile::linker::memory::POINTER_NEXT;
                ff.data.Align(0x10);
                fastfile::linker::memory::LinkerDataChunk* sharedChunk{};
                byte* shared{ ff.data.AllocStreamPtr<byte>(XSURFACE_SHARED_SIZE, &sharedChunk) };
                std::memcpy(shared, sharedHeader.data(), XSURFACE_SHARED_SIZE);
                XSurfaceSharedHeader& sh{ *(XSurfaceSharedHeader*)shared };
                sh.flags = (sh.flags & ~SHARED_STREAMED) | (stream ? SHARED_STREAMED : 0);
                sh.vertCount = vertCount;
                sh.skinVertCount = 0;
                sh.indexCount = (uint32_t)obj.indices.size();
                sh.blendTargetVertCount = 0;
                sh.dataSize = (uint32_t)data.size();
                sh.posOffset = posOffset;
                sh.vertsOffset = vertsOffset;
                sh.indicesOffset = indicesOffset;
                sh.skinWeightsOffset = 0;
                sh.data = (byte*)fastfile::linker::memory::POINTER_NEXT;
                // no blend targets
                std::memset(shared + SHARED_BLEND_COUNT_OFFSET, 0, XSURFACE_SHARED_SIZE - SHARED_BLEND_COUNT_OFFSET);

                if (stream) {
                    mesh.xpakEntry.key = ::hash::Hash64(std::format("{}/mesh_lod0", assetName).data());
                    ff.data.PushStream(XFILE_BLOCK_STREAMER);
                    ff.data.Align(0x100);
                    ff.data.AllocRuntime(data.size());
                    ff.data.PopStream();
                    AddXPakItem(ff, mesh.xpakEntry.key, std::vector<byte>{ data });
                } else {
                    ff.data.PushStream(XFILE_BLOCK_PHYSICAL);
                    ff.data.Align(0x100);
                    ff.data.WriteStream(data.data(), data.size());
                    ff.data.PopStream();
                }

                // surface
                mesh.surfs = (XSurface*)fastfile::linker::memory::POINTER_NEXT;
                ff.data.Align(8);
                XSurface& surf{ ff.data.AllocStreamRef<XSurface>() };
                std::memcpy(&surf, surfHeader.data(), sizeof(surf));
                surf.vertListCount = 1;
                surf.vertCount = vertCount;
                surf.triCount = triCount;
                surf.baseVertIndex = 0;
                surf.baseIndexOffset = 0;
                surf.unk20 = 0;
                // offset of the shared block in the virtual block, set at link
                surf.shared = nullptr;
                sharedChunk->refs.push_back((void**)&surf.shared);

                if (surfPartBits.size() == PART_BITS_SIZE) {
                    surf.partBits = (byte*)fastfile::linker::memory::POINTER_NEXT;
                    ff.data.Align(4);
                    ff.data.WriteStream(surfPartBits.data(), surfPartBits.size());
                } else {
                    surf.partBits = nullptr;
                }

                surf.vertList = (XRigidVertList*)fastfile::linker::memory::POINTER_NEXT;
                ff.data.Align(2);
                XRigidVertList& vertList{ ff.data.AllocStreamRef<XRigidVertList>() };
                vertList = { 0, vertCount, 0, triCount };

                ff.data.PopStream();
                ff.data.PopStream();
            }

            // materials of the lod 0
            model.meshMaterials = (XModelMeshMaterials*)fastfile::linker::memory::POINTER_NEXT;
            ff.data.Align(8);
            XModelMeshMaterials& mm{ ff.data.AllocStreamRef<XModelMeshMaterials>() };
            std::memcpy(&mm, meshMaterialsEntry.data(), sizeof(mm));
            mm.numMaterials = 1;
            mm.materials = (void**)fastfile::linker::memory::POINTER_NEXT;
            ff.data.Align(8);
            void*& mat{ ff.data.AllocStreamRef<void*>() };
            if (material.empty()) {
                Fail("no material");
                return;
            }
            if (material[0] == '#') {
                std::string refId{ std::format(",{}", material.substr(1)) };
                ctx.LinkAsset(XAssetType::ASSET_TYPE_MATERIAL, refId.data(), mat, false, &ff);
            } else {
                ctx.LinkAsset(XAssetType::ASSET_TYPE_MATERIAL, material.data(), mat, false, &ff);
            }
            mm.himipInvSqRadii = (float*)fastfile::linker::memory::POINTER_NEXT;
            ff.data.Align(4);
            ff.data.WriteStream<float>(radius > 0 ? 1.0f / (radius * radius) : 1.0f);

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
                geomList.contents = COLLMAP_CONTENTS;
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
                brush.contents = COLLMAP_CONTENTS;
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
                ff.data.WriteStream<uint64_t>(COLLMAP_LIST_ENTRY);
            }

            ff.data.PopStream();

            ff.data.PopStream();

            LOG_INFO(
                "Added asset xmodel {} (hash_{:x}), {} vertices, {} triangles{}",
                assetName,
                model.name.name,
                vertCount,
                triCount,
                stream ? ", streamed mesh" : ""
            );
        }
    };

    utils::MapAdder<XAssetLinkerImpl, XAssetType, XAssetLinker> impl{ GetWorkers(), XAssetType::ASSET_TYPE_XMODEL };
} // namespace
