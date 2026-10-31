#include <includes.hpp>
#include <tools/fastfile/linkers/linker_bo4.hpp>
#include <tools/fastfile/linkers/bo4/gltf.hpp>
#include <tools/fastfile/linkers/bo4/game_skeleton.hpp>
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
 *     "obj": "models/my_model.obj",   // or "gltf": "models/my_model.gltf" (.glb), skinned with its bones
 *     "upAxis": "z",            // "y" for obj files with y up
 *     "scale": 1.0,
 *     "bone": "tag_origin",
 *     "material": "materials/my_material.json" or "#game_material",   // the surfaces without their own
 *     "materials": { "usemtl_name": "materials/other.json" },        // a surface per usemtl of the obj
 *     "normals": "obj",         // "obj" (smoothed by smoothAngle where it has none), "smooth", "flat", "angle"
 *     "smoothAngle": 60,
 *     "skeletonPositions": "gltf", // with "skeleton": its bones where the glTF's joints are, their orientations kept
 *     "vertexColors": true,     // the colors of the obj vertices (v x y z r g b) when it has them
 *     "lods": [ { "obj": "models/my_model_lod1.obj", "threshold": 2.5, "materials": { } } ],  // less detailed each
 *     "lodScale": 0.42,         // the lod distances of the model (areaScale), the template's by default
 *     "stream": false,          // mesh data in the xpak, per lod too
 *     "collision": "hull",      // "hull" (convex), "box", "mesh" (a hull, the bullets hit the triangles) or "none"
 *     "collisionObj": "models/my_model_coll.obj",  // the shape of the collision instead of the model, a convex
 *                               // piece per object (o or g) of it
 *     "hullMaxSides": 44,       // the planes of a hull past its bounds, the game's props stay under 46
 *     "collisionPieces": 64,    // the most collisionObj objects (acts' default; the counts are 32 bits): a level's
 *                               // collision in one model, the viewmodel's sprint restarted at each ground entity change
 *     "physics": false,         // true (the game's props' preset), "#preset" or "physics/my_preset.json" (ours)
 *     "playerClip": true,       // the collmap blocks the players, false by default on a physics model (jitter)
 *     "skeleton": "skeletons/body.json",  // a game model's json of the acts dumper: its bones, for its animations
 *     "extraJoints": { "j_my_jaw": "j_head" },  // with "skeleton": glTF joints it hasn't, new cosmetic bones on those
 *     "hitboxes": "skeleton",   // with a skeleton: its bone hit boxes (boneInfo), or "mesh" (the vertices of each)
 *     "hitLocations": [ [ "head", 2 ], [ "spine", 5 ] ]  // without a skeleton: a bone's hit location (its
 *                               // partClassification) is the first whose text is in its name, others the template's
 *                               // (0: none, the bullets pass it)
 * }
 *
 * glTF: the meshes at rest in the game's space (y up converted, translations times scale), skinned to the joints of
 * the first skin (the bones, the roots first; at most 4 weights a vertex) or rigid on the closest joint above them. The
 * bones have no rotation at rest, the xanim linker samples its animations the same way (gltf::Skeleton). Without a
 * skin the model has one bone ("bone"). The lods take the model's bones.
 *
 * Game skeleton ("skeleton"): the bones of a game model, exactly (the dumper writes its glTF next to the json, a
 * mannequin skinned to it to model on): the glTF joints are matched by name, the meshes must be at its bind pose. The
 * game's animations then play on the model (bones by name). A mesh without a skin is rigid on "bone". The hit boxes
 * (boneInfo, in each bone's frame, the hit locations of partClassification) are the skeleton's by default.
 *
 * Surfaces: one per material, cut at 0xFFFF vertices or 0x7FFF triangles, all in one buffer per lod. Lods: the game keeps the
 * least detailed first (meshes[0], the streamer falls back up) and switches by the projected size of the triangles:
 * each lod's average triangle area, or its "threshold".
 *
 * Collision, the values of the solid props of core_common: a collision surface for the bullets (script models are
 * traced against it, DObjGeomTraceline) and a collmap for the players (SV_TraceCapsuleToEntity) and the physics, a
 * brush per convex piece: the convex hull of the model (or of each object of collisionObj) or its box. A skinned model
 * has a collision surface per bone (the bullets follow the animation), the players collide with the rest pose (the
 * game takes the first collmap at the entity). The game's memmapped block is the current one on pc.
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
        uint64_t flags;    // MESH_SKINNED on 8729 of the 9300 skinned meshes of core_common, 0x10 or 0 rigid
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
    // the weight (of 255) a bone needs on a vertex for the vertex to be in its collision hull
    constexpr byte COLLISION_BONE_WEIGHT = 64;

    constexpr size_t XSURFACE_SHARED_SIZE = 0x200;
    constexpr size_t PART_BITS_SIZE = 0x30;
    constexpr size_t BONE_INFO_SIZE = 0x2C;
    constexpr size_t BASE_MAT_SIZE = 0x20;
    // shared flags: bit 0 streamed, bit 3 rigid, no skin weights, bits 5-8 extra streams
    constexpr uint32_t SHARED_STREAMED = 1;
    constexpr uint32_t SHARED_RIGID = 8;
    // XSurface::tileMode of the game's skinned surfaces (no vertex list)
    constexpr byte SURFACE_SKINNED = 0xA2;
    // XModelMesh::flags of the game's skinned meshes
    constexpr uint64_t MESH_SKINNED = 8;
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
        uint16_t bones[4]{};
        uint8_t weights[4]{ 0xFF, 0, 0, 0 };
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
        // a skinned mesh: 4 bones and weights (sum 255) per position
        std::vector<std::array<uint16_t, 4>> skinBones{};
        std::vector<std::array<uint8_t, 4>> skinWeights{};
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

    // up to 4 weights a vertex, the biggest first, 255 in all (the rounding on the first)
    void QuantizeSkin(std::vector<std::pair<int, double>>& in, std::array<uint16_t, 4>& bones, std::array<uint8_t, 4>& weights) {
        std::vector<std::pair<int, double>> merged{};
        for (const auto& [b, w] : in) {
            auto it{ std::find_if(merged.begin(), merged.end(), [b](const auto& e) { return e.first == b; }) };
            if (it != merged.end()) {
                it->second += w;
            } else if (w > 0) {
                merged.emplace_back(b, w);
            }
        }
        std::sort(merged.begin(), merged.end(), [](const auto& a, const auto& b) { return a.second > b.second; });
        merged.resize(std::min<size_t>(merged.size(), 4));
        double sum{};
        for (const auto& e : merged) {
            sum += e.second;
        }
        bones = {};
        weights = {};
        if (merged.empty() || sum <= 0) {
            weights[0] = 0xFF;
            return;
        }
        int total{};
        for (size_t k = 0; k < merged.size(); k++) {
            bones[k] = (uint16_t)merged[k].first;
            weights[k] = (uint8_t)std::lround(merged[k].second / sum * 255.0);
            total += weights[k];
        }
        weights[0] = (uint8_t)std::clamp(weights[0] + 255 - total, 0, 255);
    }

    // a game skeleton's bind position (the model's space)
    Vec3 BindPosition(const gltf::GameSkeleton& game, size_t b) {
        double q[4], t[3];
        game.Bind(b, q, t);
        return { (float)t[0], (float)t[1], (float)t[2] };
    }

    // a model space vector in a game skeleton bone's bind frame (the inverse of its baseMat rotation)
    Vec3 ToBoneFrame(const gltf::GameSkeleton& game, size_t b, const Vec3& v) {
        double q[4], t[3];
        game.Bind(b, q, t);
        Vec3 u{ (float)-q[0], (float)-q[1], (float)-q[2] };
        Vec3 c{ u.Cross(v) };
        return v + (c * (float)q[3] + u.Cross(c)) * 2.0f;
    }

    /*
     * The meshes of a glTF at rest in the game's space: skinned to the joints of its first skin (skel, the bones) or,
     * a mesh outside of it, rigid on the closest joint above it. A primitive's material is its group (usemtl). With a
     * game skeleton the joints are its bones of the same name (or of the closest joint above with one).
     */
    bool ReadGltf(const std::filesystem::path& path, float scale, ObjData& obj, gltf::Skeleton& skel, std::string& err,
                  const gltf::GameSkeleton* game = nullptr) {
        gltf::Document doc{};
        if (!doc.Load(path, err)) {
            return false;
        }
        int skin{ doc.skins.empty() ? -1 : 0 };
        skel = {};
        if (!skel.Build(doc, skin, scale, err)) {
            return false;
        }
        std::vector<int> boneMap(skel.nodes.size());
        for (size_t b = 0; b < boneMap.size(); b++) {
            boneMap[b] = (int)b;
        }
        if (game) {
            std::vector<std::string> unmatched{};
            float moved{};
            std::string movedBone{};
            for (size_t b = 0; b < skel.nodes.size(); b++) {
                int idx{ -1 };
                for (int p = (int)b; p >= 0 && idx < 0; p = skel.parents[p]) {
                    idx = game->Index(skel.names[p]);
                }
                if (game->Index(skel.names[b]) < 0) {
                    unmatched.push_back(skel.names[b]);
                } else {
                    // the joint where the game's bone is, or the mesh isn't at the bind pose
                    double q[4], t[3];
                    skel.ToGame(skel.restWorld[b], q, t);
                    float d{ (Vec3{ (float)t[0], (float)t[1], (float)t[2] } - BindPosition(*game, (size_t)idx)).Length() };
                    if (d > moved) {
                        moved = d;
                        movedBone = skel.names[b];
                    }
                }
                boneMap[b] = std::max(0, idx);
            }
            if (!unmatched.empty()) {
                std::string list{};
                for (const std::string& n : unmatched) {
                    list += (list.empty() ? "" : ", ") + n;
                }
                LOG_WARNING("{}: joints not in the skeleton, their vertices on the bone above: {}", path.filename().string(), list);
            }
            if (moved > 0.5f) {
                LOG_WARNING("{}: joint {} is {} units from the skeleton's bone, is the scale right?", path.filename().string(), movedBone, moved);
            }
        }
        auto Map = [&boneMap](int b) { return b >= 0 && b < (int)boneMap.size() ? boneMap[b] : 0; };
        std::vector<gltf::Mat4> locals(doc.nodes.size());
        for (size_t i = 0; i < doc.nodes.size(); i++) {
            locals[i] = doc.LocalMatrix((int)i);
        }
        std::vector<gltf::Mat4> world{ doc.WorldMatrices(locals) };
        auto BoneOf = [&skel](int node) {
            auto it{ std::find(skel.nodes.begin(), skel.nodes.end(), node) };
            return it == skel.nodes.end() ? -1 : (int)(it - skel.nodes.begin());
        };
        // the skinning matrix of each joint of the skin at rest
        std::vector<gltf::Mat4> skinMats{};
        std::vector<int> jointBone{};
        if (skin >= 0) {
            const gltf::Skin& sk{ doc.skins[skin] };
            for (size_t j = 0; j < sk.joints.size(); j++) {
                gltf::Mat4 ib{};
                std::copy(sk.inverseBind.begin() + 16 * j, sk.inverseBind.begin() + 16 * (j + 1), ib.m);
                skinMats.push_back(world[sk.joints[j]] * ib);
                jointBone.push_back(BoneOf(sk.joints[j]));
            }
        }
        obj = {};
        obj.groups.emplace_back();
        std::vector<std::pair<int, double>> influences{};
        for (size_t n = 0; n < doc.nodes.size(); n++) {
            const gltf::Node& node{ doc.nodes[n] };
            if (node.mesh < 0 || node.mesh >= (int)doc.meshes.size()) {
                continue;
            }
            bool skinned{ skin >= 0 && node.skin == skin };
            int rigidBone{ 0 };
            for (int p = (int)n; !skinned && p >= 0 && skin >= 0; p = doc.nodes[p].parent) {
                if (BoneOf(p) >= 0) {
                    rigidBone = BoneOf(p);
                    break;
                }
            }
            for (const gltf::Primitive& prim : doc.meshes[node.mesh].primitives) {
                auto git{ std::find(obj.groups.begin(), obj.groups.end(), prim.material) };
                uint32_t group{ (uint32_t)(git - obj.groups.begin()) };
                if (git == obj.groups.end()) {
                    obj.groups.push_back(prim.material);
                }
                size_t vcount{ prim.positions.size() / 3 };
                int base{ (int)obj.positions.size() };
                int nbase{ (int)obj.normals.size() };
                int ubase{ (int)obj.uvs.size() };
                bool hasNormals{ prim.normals.size() == vcount * 3 };
                bool hasUvs{ prim.uvs.size() == vcount * 2 };
                bool vskin{ skinned && prim.joints.size() == vcount * 4 && prim.weights.size() == vcount * 4 };
                for (size_t i = 0; i < vcount; i++) {
                    double p[3]{ prim.positions[i * 3], prim.positions[i * 3 + 1], prim.positions[i * 3 + 2] };
                    double nn[3]{};
                    if (hasNormals) {
                        nn[0] = prim.normals[i * 3];
                        nn[1] = prim.normals[i * 3 + 1];
                        nn[2] = prim.normals[i * 3 + 2];
                    }
                    double wp[3]{}, wn[3]{};
                    influences.clear();
                    if (vskin) {
                        for (int k = 0; k < 4; k++) {
                            uint32_t j{ prim.joints[i * 4 + k] };
                            double w{ prim.weights[i * 4 + k] };
                            if (w <= 0 || j >= skinMats.size()) {
                                continue;
                            }
                            double tp[3], tn[3];
                            skinMats[j].Apply(p, tp);
                            skinMats[j].Apply(nn, tn, 0);
                            for (int c = 0; c < 3; c++) {
                                wp[c] += tp[c] * w;
                                wn[c] += tn[c] * w;
                            }
                            influences.emplace_back(Map(std::max(0, jointBone[j])), w);
                        }
                    }
                    if (influences.empty()) {
                        world[n].Apply(p, wp);
                        world[n].Apply(nn, wn, 0);
                        influences.emplace_back(Map(rigidBone), 1.0);
                    }
                    double gp[3], gn[3];
                    skel.ToGamePoint(wp, gp);
                    skel.ToGameVector(wn, gn);
                    obj.positions.push_back({ (float)gp[0], (float)gp[1], (float)gp[2] });
                    if (hasNormals) {
                        obj.normals.push_back(Vec3{ (float)gn[0], (float)gn[1], (float)gn[2] }.Normalized());
                    }
                    if (hasUvs) {
                        // glTF v goes down like the game's, the obj convention goes up
                        obj.uvs.emplace_back(prim.uvs[i * 2], 1.0f - prim.uvs[i * 2 + 1]);
                    }
                    if (prim.colors.size() == vcount * 4) {
                        obj.colors.push_back(PackColor(prim.colors[i * 4], prim.colors[i * 4 + 1], prim.colors[i * 4 + 2], prim.colors[i * 4 + 3]));
                        obj.hasColors = true;
                    } else {
                        obj.colors.push_back(0xFFFFFFFF);
                    }
                    if (skin >= 0) {
                        std::array<uint16_t, 4> b{};
                        std::array<uint8_t, 4> w{};
                        QuantizeSkin(influences, b, w);
                        obj.skinBones.push_back(b);
                        obj.skinWeights.push_back(w);
                    }
                }
                // glTF faces are counter clockwise, the game's clockwise
                for (size_t t = 0; t + 2 < prim.indices.size(); t += 3) {
                    auto Corner = [&](uint32_t i) {
                        return ObjCorner{ base + (int)i, hasUvs ? ubase + (int)i : -1, hasNormals ? nbase + (int)i : -1 };
                    };
                    obj.tris.push_back({ { Corner(prim.indices[t]), Corner(prim.indices[t + 2]), Corner(prim.indices[t + 1]) }, group, 0 });
                }
            }
        }
        if (obj.tris.empty()) {
            err = "no triangles";
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
            std::map<std::array<uint32_t, 13>, uint16_t> known{};
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
                    if (!obj.skinBones.empty()) {
                        std::memcpy(v.bones, obj.skinBones[c.pos].data(), sizeof(v.bones));
                        std::memcpy(v.weights, obj.skinWeights[c.pos].data(), sizeof(v.weights));
                    }
                    std::array<uint32_t, 13> key{};
                    std::memcpy(key.data(), &v, sizeof(float) * 9);
                    key[9] = v.color;
                    key[10] = v.bones[0] | ((uint32_t)v.bones[1] << 16);
                    key[11] = v.bones[2] | ((uint32_t)v.bones[3] << 16);
                    std::memcpy(&key[12], v.weights, 4);
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
    constexpr int64_t MAX_COLLISION_PIECES_LIMIT = 32768;
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

    /*
     * A rotation as the game packs it (xmodel bind quats, xanim keys): the largest component dropped, its index in bits
     * 0-1 (x 0, w 1, z 2, y 3), the three after it (x y z w cyclic) as signed 20 bits of 1/sqrt(2) in bits 2-61, the
     * sign of the dropped one in bit 63.
     */
    uint64_t PackQuat(const double qin[4]) {
        double q[4]{ qin[0], qin[1], qin[2], qin[3] };
        double n{ std::sqrt(q[0] * q[0] + q[1] * q[1] + q[2] * q[2] + q[3] * q[3]) };
        int a{};
        for (int i = 0; i < 4; i++) {
            q[i] = n > 0 ? q[i] / n : (i == 3 ? 1 : 0);
            if (std::abs(q[i]) > std::abs(q[a])) {
                a = i;
            }
        }
        static constexpr uint64_t AXIS_INDEX[4]{ 0, 3, 2, 1 };
        uint64_t v{ AXIS_INDEX[a] };
        for (int k = 0; k < 3; k++) {
            double c{ q[(a + 1 + k) % 4] * std::numbers::sqrt2 * (1 << 19) };
            int64_t ic{ std::clamp<int64_t>(std::llround(c), -(1 << 19), (1 << 19) - 1) };
            v |= ((uint64_t)ic & 0xFFFFF) << (2 + 20 * k);
        }
        if (q[a] < 0) {
            v |= 1ull << 63;
        }
        return v;
    }

    /*
     * The fastfile's sets (BO4FFContext::sets) of the material jsons linked, a json once a zone, by name after (two
     * models on one material linked it twice, two assets of a name: the game unloaded one and streamed into the other's
     * freed parts), and of the lossless models' meshes linked in a zone (by name)
     */
    constexpr const char* LINKED_MATERIALS = "xmodel.materials";
    constexpr const char* LINKED_MESHES = "xmodel.meshes";

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
        uint32_t indicesOffset{}, posOffset{}, vertsOffset{}, skinOffset{};
        uint32_t vertCount{}, indexCount{};
        float triArea{};
        float threshold{};
        bool stream{};
        bool skinned{};
    };

    // indices, positions, vertices of all the surfaces, the skin weights of a skinned one, each part on 16 bytes
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
        if (lod.skinned) {
            // 4 weights (sum 255), 4 model bone indices
            std::vector<byte> skin{};
            for (const MeshSurface& s : lod.surfs) {
                for (const MeshVertex& v : s.verts) {
                    skin.insert(skin.end(), v.weights, v.weights + 4);
                    skin.insert(skin.end(), (const byte*)v.bones, (const byte*)v.bones + sizeof(v.bones));
                }
            }
            lod.skinOffset = Append(skin.data(), skin.size());
        }
        lod.vertCount = (uint32_t)(positions.size() / 3);
        lod.indexCount = (uint32_t)indices.size();
        lod.triArea = tris ? (float)(area / tris) : 0;
    }

    /*
     * Stock models as the bo4 dumper writes them ("lossless": true), in the loaders' order (Load_XModel sub_142E366B0,
     * Load_XModelMesh sub_142E36F90, XSurfaceShared sub_142E37470): the blocks as they were, the meshes inline from
     * their json (../xmodelmesh/<name>.json), the materials, physpreset and physconstraints by name (the common zones'),
     * the script strings by name. A pointer is written when the dumped one wasn't null. A streamed mesh's data goes in
     * the zone's xpak under its key.
     */
    namespace lossless {
        auto Next = [](byte* p, size_t off) { *(uintptr_t*)(p + off) = fastfile::linker::memory::POINTER_NEXT; };

        std::vector<byte> Hex(const rapidjson::Value& v, const char* key) {
            std::vector<byte> out{};
            ReadHex(Member(v, key), out);
            return out;
        }

        uint32_t Scr(BO4FFContext& ff, const rapidjson::Value* v) {
            return v && v->IsString() ? ff.data.AddScrStringKeepEmpty(v->GetString()) : 0;
        }

        void Write(BO4FFContext& ff, const std::vector<byte>& data, size_t align, size_t size) {
            std::vector<byte> d{ data };
            d.resize(size);
            ff.data.Align(align);
            if (size) {
                ff.data.WriteStream(d.data(), d.size());
            }
        }

        // a "#name" or "#hash_x" of the dumper, the top bit (a default reference) off
        std::string RefName(const rapidjson::Value* v) {
            if (!v || !v->IsString()) {
                return {};
            }
            std::string s{ v->GetString() };
            if (!s.empty() && s[0] == '#') {
                s = s.substr(1);
            }
            if (s.rfind("hash_", 0) == 0) {
                s = std::format("hash_{:x}", std::strtoull(s.data() + 5, nullptr, 16) & hash::MASK63);
            }
            return s;
        }

        // the file of a dumped asset: file_<hash> or its name
        std::string FileName(const std::string& ref) {
            return ref.rfind("hash_", 0) == 0 ? "file_" + ref.substr(5) : ref;
        }

        // the 0x1E0 strings of a shared block, after it (count at 0x1E0)
        void SharedStrings(BO4FFContext& ff, byte* sh, const rapidjson::Value& m) {
            uint32_t count{ *(uint32_t*)(sh + 0x1E0) };
            *(uint32_t*)(sh + 0x1E4) = Scr(ff, Member(m, "sharedStr"));
            const rapidjson::Value* l4{ Member(m, "sharedStrings") };
            *(uint64_t*)(sh + 0x1E8) = 0;
            if (l4 && l4->IsArray()) {
                Next(sh, 0x1E8);
                ff.data.Align(4);
                uint32_t* out{ ff.data.AllocStreamPtr<uint32_t>(count) };
                for (uint32_t i = 0; i < count; i++) {
                    out[i] = i < l4->Size() ? Scr(ff, &(*l4)[i]) : 0;
                }
            }
            std::vector<byte> l8{ Hex(m, "sharedEntries") };
            const rapidjson::Value* l8s{ Member(m, "sharedEntryStrings") };
            *(uint64_t*)(sh + 0x1F0) = 0;
            if (!l8.empty()) {
                Next(sh, 0x1F0);
                l8.resize(count * 8ull);
                for (uint32_t i = 0; i < count; i++) {
                    *(uint32_t*)&l8[i * 8ull] = l8s && l8s->IsArray() && i < l8s->Size() ? Scr(ff, &(*l8s)[i]) : 0;
                }
                Write(ff, l8, 4, l8.size());
            }
        }

        // a shared block: header, data (physical, or the streamer for a streamed mesh), strings
        fastfile::linker::memory::LinkerDataChunk* Shared(BO4FFContext& ff, const std::vector<byte>& raw, const std::vector<byte>& data, const rapidjson::Value& m) {
            ff.data.Align(0x10);
            fastfile::linker::memory::LinkerDataChunk* chunk{};
            byte* sh{ ff.data.AllocStreamPtr<byte>(XSURFACE_SHARED_SIZE, &chunk) };
            std::memcpy(sh, raw.data(), std::min(raw.size(), XSURFACE_SHARED_SIZE));
            uint32_t flags{ *(uint32_t*)sh };
            uint32_t dataSize{ *(uint32_t*)(sh + 0x28) };
            bool had{ *(uint64_t*)(sh + SHARED_DATA_OFFSET) != 0 };
            *(uint64_t*)(sh + SHARED_DATA_OFFSET) = had ? fastfile::linker::memory::POINTER_NEXT : 0;
            if (had) {
                if (flags & SHARED_STREAMED) {
                    ff.data.PushStream(XFILE_BLOCK_STREAMER);
                    ff.data.Align(0x100);
                    ff.data.AllocRuntime(dataSize);
                    ff.data.PopStream();
                } else {
                    ff.data.PushStream(XFILE_BLOCK_PHYSICAL);
                    Write(ff, data, 0x100, dataSize);
                    ff.data.PopStream();
                }
            }
            SharedStrings(ff, sh, m);
            return chunk;
        }

        void Mesh(BO4LinkContext& ctx, BO4FFContext& ff, const std::filesystem::path& file, void*& ref) {
            BO4Json cfg{ file };
            if (!cfg.SyncConfig(false)) {
                LOG_ERROR("Can't read xmodelmesh {}", file.string());
                ctx.error = true;
                ref = nullptr;
                return;
            }
            const rapidjson::Value& m{ cfg.main };
            std::vector<byte> header{ Hex(m, "header") };
            header.resize(sizeof(XModelMesh));

            ref = (void*)fastfile::linker::memory::POINTER_NEXT;
            ff.data.PushStream(XFILE_BLOCK_TEMP);
            ff.data.Align(8);
            byte* mesh{ ff.data.AllocStreamPtr<byte>(sizeof(XModelMesh)) };
            std::memcpy(mesh, header.data(), sizeof(XModelMesh));
            *(uint64_t*)(mesh + 8) = 0;
            auto Had = [&header](size_t off) { return *(uint64_t*)&header[off] != 0; };
            for (size_t off : { 0x10, 0x18, 0x20 }) {
                *(uint64_t*)(mesh + off) = 0;
            }
            // the xpak entry: its key only, the game finds the rest
            uint64_t key{ *(uint64_t*)(mesh + 0x28) };
            std::memset(mesh + 0x30, 0, 0x18);
            uint16_t numSurfs{ *(uint16_t*)(mesh + 0x4C) };

            ff.data.PushStream(XFILE_BLOCK_VIRTUAL);
            if (Had(0x20)) {
                Next(mesh, 0x20);
                Write(ff, Hex(m, "partBits"), 4, PART_BITS_SIZE);
            }
            fastfile::linker::memory::LinkerDataChunk* sharedChunk{};
            std::vector<byte> sharedRaw{ Hex(m, "shared") };
            if (Had(0x18) && !sharedRaw.empty()) {
                Next(mesh, 0x18);
                std::vector<byte> data{ Hex(m, "data") };
                sharedChunk = Shared(ff, sharedRaw, data, m);
                uint32_t flags{ *(uint32_t*)sharedRaw.data() };
                if (flags & SHARED_STREAMED) {
                    std::vector<byte> streamed{ Hex(m, "streamData") };
                    if (streamed.empty() || !key) {
                        LOG_ERROR("xmodelmesh {}: streamed without its data", file.string());
                        ctx.error = true;
                    } else {
                        AddXPakItem(ff, key, std::move(streamed));
                    }
                }
            }
            const rapidjson::Value* surfs{ Member(m, "surfs") };
            if (Had(0x10) && surfs && surfs->IsArray()) {
                Next(mesh, 0x10);
                ff.data.Align(8);
                byte* out{ ff.data.AllocStreamPtr<byte>(sizeof(XSurface) * numSurfs) };
                std::memset(out, 0, sizeof(XSurface) * numSurfs);
                for (size_t i = 0; i < numSurfs && i < surfs->Size(); i++) {
                    std::vector<byte> h{ Hex((*surfs)[(rapidjson::SizeType)i], "header") };
                    std::memcpy(out + sizeof(XSurface) * i, h.data(), std::min(h.size(), sizeof(XSurface)));
                }
                for (size_t i = 0; i < numSurfs && i < surfs->Size(); i++) {
                    const rapidjson::Value& sv{ (*surfs)[(rapidjson::SizeType)i] };
                    byte* surf{ out + sizeof(XSurface) * i };
                    bool hadShared{ *(uint64_t*)(surf + 0x10) != 0 };
                    bool hadVerts{ *(uint64_t*)(surf + 0x18) != 0 };
                    bool hadBits{ *(uint64_t*)(surf + 0x28) != 0 };
                    *(uint64_t*)(surf + 0x10) = 0;
                    *(uint64_t*)(surf + 0x18) = 0;
                    *(uint64_t*)(surf + 0x20) = 0;
                    *(uint64_t*)(surf + 0x28) = 0;
                    const rapidjson::Value* meshShared{ Member(sv, "meshShared") };
                    if (hadShared) {
                        if (meshShared && meshShared->IsBool() && meshShared->GetBool() && sharedChunk) {
                            // an offset to the mesh's shared block
                            ff.data.AddRef(sharedChunk, (void**)(surf + 0x10));
                        } else {
                            Next(surf, 0x10);
                            Shared(ff, Hex(sv, "shared"), {}, sv);
                        }
                    }
                    if (hadBits) {
                        Next(surf, 0x28);
                        Write(ff, Hex(sv, "partBits"), 4, PART_BITS_SIZE);
                    }
                    if (hadVerts) {
                        Next(surf, 0x18);
                        Write(ff, Hex(sv, "vertList"), 2, surf[2] * 8ull);
                    }
                }
            }
            ff.data.PopStream();
            ff.data.PopStream();
        }

        void Model(BO4LinkContext& ctx, BO4FFContext& ff, const rapidjson::Value& root, const std::filesystem::path& path, const std::string& assetName) {
            std::vector<byte> header{ Hex(root, "header") };
            if (header.size() != sizeof(XModel)) {
                LOG_ERROR("xmodel {}: bad header", assetName);
                ctx.error = true;
                return;
            }
            ff.data.PushStream(XFILE_BLOCK_TEMP);
            byte* a{ ff.data.AllocStreamPtr<byte>(sizeof(XModel)) };
            std::memcpy(a, header.data(), sizeof(XModel));
            auto Had = [&header](size_t off) { return *(uint64_t*)&header[off] != 0; };
            *(uint64_t*)(a + 8) = 0;
            for (size_t off = 0x10; off <= 0xC0; off += 8) {
                *(uint64_t*)(a + off) = 0;
            }
            size_t numBones{ a[0x14B] };
            size_t cosmetic{ *(uint16_t*)(a + 0x144) };
            size_t bones{ numBones + cosmetic };
            size_t parents{ bones - a[0x14C] };

            ff.data.PushStream(XFILE_BLOCK_VIRTUAL);
            if (Had(0x10)) {
                Next(a, 0x10);
                const rapidjson::Value* names{ Member(root, "boneNames") };
                ff.data.Align(4);
                uint32_t* out{ ff.data.AllocStreamPtr<uint32_t>(bones) };
                for (size_t i = 0; i < bones; i++) {
                    out[i] = names && names->IsArray() && i < names->Size() ? Scr(ff, &(*names)[(rapidjson::SizeType)i]) : 0;
                }
            }
            if (Had(0x18)) {
                Next(a, 0x18);
                const rapidjson::Value* idx{ Member(root, "boneNameIndices") };
                ff.data.Align(4);
                byte* out{ ff.data.AllocStreamPtr<byte>(8 * numBones) };
                std::memset(out, 0, 8 * numBones);
                for (size_t i = 0; i < numBones && idx && idx->IsArray() && i < idx->Size(); i++) {
                    const rapidjson::Value& e{ (*idx)[(rapidjson::SizeType)i] };
                    *(uint32_t*)(out + 8 * i) = Scr(ff, Member(e, "name"));
                    const rapidjson::Value* index{ Member(e, "index") };
                    *(uint16_t*)(out + 8 * i + 4) = index && index->IsNumber() ? (uint16_t)index->GetUint() : 0;
                }
            }
            auto Blob = [&](size_t off, const char* key, size_t align, size_t size) {
                if (Had(off)) {
                    Next(a, off);
                    Write(ff, Hex(root, key), align, size);
                }
            };
            Blob(0x20, "parentList", 1, parents);
            Blob(0x28, "quats", 8, parents * 8);
            Blob(0x30, "trans", 4, parents * 0x10);
            Blob(0x38, "partClassification", 1, bones);
            Blob(0x40, "baseMat", 0x10, bones * BASE_MAT_SIZE);

            // the meshes, inline
            const rapidjson::Value* meshes{ Member(root, "meshes") };
            std::filesystem::path meshDir{ path.parent_path().parent_path() / "xmodelmesh" };
            for (size_t i = 0; i < 8; i++) {
                if (!Had(0x78 + 8 * i)) {
                    continue;
                }
                std::string ref{ meshes && meshes->IsArray() && i < meshes->Size() ? RefName(&(*meshes)[(rapidjson::SizeType)i]) : "" };
                void*& mref{ *(void**)(a + 0x78 + 8 * i) };
                // a mesh once in the zone, then by its name (the game's models share them: a weapon's and its upgrade's)
                if (!ref.empty() && ff.SetContains(LINKED_MESHES, ::hash::Hash64(ref.data()))) {
                    std::string refId{ "," + ref };
                    ctx.LinkAsset(XAssetType::ASSET_TYPE_XMODELMESH, refId.data(), mref, false, &ff);
                    continue;
                }
                Mesh(ctx, ff, meshDir / (FileName(ref) + ".json"), mref);
                if (!ref.empty()) {
                    ff.SetInsert(LINKED_MESHES, ::hash::Hash64(ref.data()));
                }
            }

            // the materials of each lod: entries, then each its materials (by name) and himip floats
            uint32_t numLods{ *(uint32_t*)(a + 0xC8) };
            const rapidjson::Value* mms{ Member(root, "meshMaterials") };
            if (Had(0xB8)) {
                Next(a, 0xB8);
                ff.data.Align(8);
                byte* out{ ff.data.AllocStreamPtr<byte>(0x18ull * numLods) };
                std::memset(out, 0, 0x18ull * numLods);
                for (size_t i = 0; i < numLods && mms && mms->IsArray() && i < mms->Size(); i++) {
                    std::vector<byte> e{ Hex((*mms)[(rapidjson::SizeType)i], "entry") };
                    std::memcpy(out + 0x18 * i, e.data(), std::min<size_t>(e.size(), 0x18));
                }
                for (size_t i = 0; i < numLods && mms && mms->IsArray() && i < mms->Size(); i++) {
                    const rapidjson::Value& mm{ (*mms)[(rapidjson::SizeType)i] };
                    byte* e{ out + 0x18 * i };
                    uint16_t count{ *(uint16_t*)e };
                    bool hadMats{ *(uint64_t*)(e + 8) != 0 };
                    bool hadHimip{ *(uint64_t*)(e + 0x10) != 0 };
                    *(uint64_t*)(e + 8) = 0;
                    *(uint64_t*)(e + 0x10) = 0;
                    if (hadMats) {
                        Next(e, 8);
                        const rapidjson::Value* mats{ Member(mm, "materials") };
                        ff.data.Align(8);
                        void** arr{ ff.data.AllocStreamPtr<void*>(count) };
                        for (size_t j = 0; j < count; j++) {
                            arr[j] = nullptr;
                        }
                        for (size_t j = 0; j < count; j++) {
                            std::string mat{ mats && mats->IsArray() && j < mats->Size() ? RefName(&(*mats)[(rapidjson::SizeType)j]) : "" };
                            if (!mat.empty()) {
                                ctx.LinkAsset(XAssetType::ASSET_TYPE_MATERIAL, ("," + mat).data(), arr[j], false, &ff);
                            }
                        }
                    }
                    if (hadHimip) {
                        Next(e, 0x10);
                        Write(ff, Hex(mm, "himipInvSqRadii"), 4, count * 4ull);
                    }
                }
            }
            // the collision surfaces (0x38 each, the count i32 at 0x140), then the triangles of each, as the obj path's
            if (Had(0x50)) {
                const rapidjson::Value* css{ Member(root, "collSurfs") };
                size_t count{ css && css->IsArray() ? css->Size() : 0 };
                if (!count || count != (size_t)*(int32_t*)(a + 0x140)) {
                    LOG_ERROR("xmodel {}: {} collision surfaces in the json for a count of {}", assetName, count, *(int32_t*)(a + 0x140));
                    ctx.error = true;
                } else {
                    Next(a, 0x50);
                    ff.data.Align(8);
                    byte* out{ ff.data.AllocStreamPtr<byte>(0x38 * count) };
                    std::vector<std::vector<byte>> tris(count);
                    for (size_t i = 0; i < count; i++) {
                        const rapidjson::Value& cs{ (*css)[(rapidjson::SizeType)i] };
                        std::vector<byte> h{ Hex(cs, "header") };
                        if (h.size() != 0x38 || Member(cs, "blob")) {
                            LOG_ERROR("xmodel {}: collision surface {}: bad header or a blob (not supported)", assetName, i);
                            ctx.error = true;
                            return;
                        }
                        tris[i] = Hex(cs, "tris");
                        if (tris[i].size() != *(uint32_t*)&h[0x10] * 0x30ull) {
                            LOG_ERROR("xmodel {}: collision surface {}: {} triangle bytes for {} triangles", assetName, i, tris[i].size(), *(uint32_t*)&h[0x10]);
                            ctx.error = true;
                            return;
                        }
                        std::memcpy(out + 0x38 * i, h.data(), 0x38);
                        *(uint64_t*)(out + 0x38 * i) = tris[i].empty() ? 0 : (uint64_t)fastfile::linker::memory::POINTER_NEXT;
                        *(uint64_t*)(out + 0x38 * i + 8) = 0;
                    }
                    for (const std::vector<byte>& t : tris) {
                        if (!t.empty()) {
                            ff.data.Align(4);
                            ff.data.WriteStream(t.data(), t.size());
                        }
                    }
                }
            }
            Blob(0x48, "boneInfo", 4, numBones * BONE_INFO_SIZE);
            Blob(0xC0, "unkC0", 4, 0x30);
            auto Ref = [&](size_t off, XAssetType type, const char* key) {
                std::string name{ RefName(Member(root, key)) };
                if (Had(off) && !name.empty()) {
                    ctx.LinkAsset(type, ("," + name).data(), *(void**)(a + off), false, &ff);
                }
            };
            Ref(0x58, XAssetType::ASSET_TYPE_PHYSPRESET, "physPreset");
            if (Had(0x60)) {
                LOG_ERROR("xmodel {}: collmaps aren't supported in lossless models yet", assetName);
                ctx.error = true;
            }
            Ref(0x68, XAssetType::ASSET_TYPE_PHYSCONSTRAINTS, "physConstraints");
            // the submodels (0x28 each, the loader's last block): a model by name (sub_142E6B190) and its tag at 8
            if (Had(0x70)) {
                Next(a, 0x70);
                size_t count{ (size_t)(int8_t)a[0x14A] };
                const rapidjson::Value* subs{ Member(root, "submodels") };
                ff.data.Align(8);
                byte* out{ ff.data.AllocStreamPtr<byte>(0x28 * count) };
                std::memset(out, 0, 0x28 * count);
                for (size_t i = 0; i < count && subs && subs->IsArray() && i < subs->Size(); i++) {
                    const rapidjson::Value& sub{ (*subs)[(rapidjson::SizeType)i] };
                    std::vector<byte> raw{ Hex(sub, "raw") };
                    byte* e{ out + 0x28 * i };
                    std::memcpy(e, raw.data(), std::min<size_t>(raw.size(), 0x28));
                    *(uint64_t*)e = 0;
                    std::string model{ RefName(Member(sub, "model")) };
                    if (!model.empty()) {
                        ctx.LinkAsset(XAssetType::ASSET_TYPE_XMODEL, ("," + model).data(), *(void**)e, false, &ff);
                    }
                    *(uint32_t*)(e + 8) = Scr(ff, Member(sub, "str"));
                }
            }
            ff.data.PopStream();
            ff.data.PopStream();
            LOG_INFO("Added asset xmodel {} (hash_{:x}), lossless, {} lod(s), {} bone(s)", assetName, *(uint64_t*)a, numLods, numBones);
        }
    } // namespace lossless

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
            // a stock model from the dumper
            if (Member(cfg.main, "lossless")) {
                lossless::Model(ctx, ff, cfg.main, path, cfg.GetString("name", rfpath.string().c_str()));
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
            int64_t maxPieces{ cfg.GetInteger("collisionPieces", (int64_t)MAX_COLLISION_PIECES) };
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
            if (maxPieces < 1 || maxPieces > MAX_COLLISION_PIECES_LIMIT) {
                Fail(std::format("collisionPieces must be 1 to {}", MAX_COLLISION_PIECES_LIMIT));
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

            // a game skeleton for the game's animations
            std::string skeletonPath{ cfg.GetString("skeleton", "") };
            std::string hitboxes{ cfg.GetString("hitboxes", "skeleton") };
            gltf::GameSkeleton gameSkel{};
            if (!skeletonPath.empty()) {
                std::string err{};
                if (!gameSkel.Load(dir / skeletonPath, err)) {
                    Fail(std::format("skeleton {}: {}", skeletonPath, err));
                    return;
                }
                if (!gameSkel.AddExtraJoints(cfg.main, err)) {
                    Fail(err);
                    return;
                }
                if (hitboxes != "skeleton" && hitboxes != "mesh") {
                    Fail(std::format("unknown hitboxes '{}', skeleton or mesh", hitboxes));
                    return;
                }
            }
            bool useGameSkel{ !skeletonPath.empty() };

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
            gltf::Skeleton skel{};
            for (size_t k = 0; k < lodCfgs.size(); k++) {
                const rapidjson::Value& lc{ *lodCfgs[k] };
                LodGeom& lod{ lods[k] };
                const rapidjson::Value* objCfg{ Member(lc, "obj") };
                const rapidjson::Value* gltfCfg{ Member(lc, "gltf") };
                ObjData obj{};
                if (gltfCfg && gltfCfg->IsString()) {
                    std::string err{};
                    gltf::Skeleton lodSkel{};
                    if (!ReadGltf(dir / gltfCfg->GetString(), scale, obj, lodSkel, err, useGameSkel ? &gameSkel : nullptr)) {
                        Fail(std::format("lod {}: {}: {}", k, gltfCfg->GetString(), err));
                        return;
                    }
                    if (!k) {
                        skel = lodSkel;
                    } else if (!useGameSkel && lodSkel.names != skel.names) {
                        Fail(std::format("lod {}: its bones aren't the model's", k));
                        return;
                    }
                } else if (!objCfg || !objCfg->IsString() || !ReadObj(dir / objCfg->GetString(), upAxis == "y", scale, obj)) {
                    Fail(std::format("lod {}: can't read obj", k));
                    return;
                }
                if (useGameSkel && obj.skinBones.empty()) {
                    // rigid on one of the skeleton's bones
                    int b{ gameSkel.Index(boneName) };
                    if (b < 0) {
                        Fail(std::format("lod {}: bone {} isn't in the skeleton", k, boneName));
                        return;
                    }
                    obj.skinBones.assign(obj.positions.size(), { (uint16_t)b, 0, 0, 0 });
                    obj.skinWeights.assign(obj.positions.size(), { 255, 0, 0, 0 });
                }
                lod.skinned = !obj.skinBones.empty();
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
                    if (pieceShapes.size() > (size_t)maxPieces) {
                        Fail(std::format("collisionObj: at most {} objects (collisionPieces)", maxPieces));
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

            /*
             * Collision surfaces of the bullets, in the model's space at rest, each on a bone the trace follows
             * (XModelTraceLineAnimated): the pieces' faces or the triangles of the most detailed lod on bone 0, or for a
             * skinned model (without collisionObj) one per bone, of the vertices it moves the most (their hull, box or
             * triangles). The players collide with the collmap at the entity only (SV_TraceCapsuleToEntity takes the
             * first collmap), it doesn't follow the bones.
             */
            struct CollGroup {
                uint32_t bone{};
                std::vector<XModelCollTri> tris{};
                Vec3 mins{}, maxs{};
            };
            std::vector<CollGroup> collGroups{};
            float collArea{};
            size_t collTriCount{};
            auto AddCollTri = [&](CollGroup& g, const Vec3& a, const Vec3& b, const Vec3& c) {
                XModelCollTri tri;
                if (MakeCollTri(a, b, c, tri)) {
                    if (g.tris.empty()) {
                        g.mins = g.maxs = a;
                    }
                    g.tris.push_back(tri);
                    collArea += (b - a).Cross(c - a).Length() * 0.5f;
                    collTriCount++;
                    for (const Vec3& p : { a, b, c }) {
                        GrowBounds(g.mins, g.maxs, p);
                    }
                }
            };
            auto Dominant = [](const MeshVertex& v) {
                int k{};
                for (int i = 1; i < 4; i++) {
                    if (v.weights[i] > v.weights[k]) {
                        k = i;
                    }
                }
                return (uint32_t)v.bones[k];
            };
            bool perBone{ lod0.skinned && collisionObj.empty() && collision != "none" };
            if (perBone) {
                std::map<uint32_t, std::vector<Vec3>> bonePoints{};
                std::map<uint32_t, CollGroup> groups{};
                for (const MeshSurface& s : lod0.surfs) {
                    if (collision == "mesh") {
                        for (size_t i = 0; i < s.indices.size(); i += 3) {
                            const MeshVertex& a{ s.verts[s.indices[i]] };
                            const MeshVertex& b{ s.verts[s.indices[i + 1]] };
                            const MeshVertex& c{ s.verts[s.indices[i + 2]] };
                            uint32_t ba{ Dominant(a) }, bb{ Dominant(b) }, bc{ Dominant(c) };
                            uint32_t bone{ bb == bc ? bb : ba };
                            CollGroup& g{ groups[bone] };
                            g.bone = bone;
                            AddCollTri(g, a.pos, c.pos, b.pos);
                        }
                    } else {
                        // each bone moving a vertex enough has it, the hulls of the bones overlap where they blend
                        for (const MeshVertex& v : s.verts) {
                            bonePoints[Dominant(v)].push_back(v.pos);
                            for (int i = 0; i < 4; i++) {
                                if (v.weights[i] >= COLLISION_BONE_WEIGHT && v.bones[i] != Dominant(v)) {
                                    bonePoints[v.bones[i]].push_back(v.pos);
                                }
                            }
                        }
                    }
                }
                for (auto& [bone, points] : bonePoints) {
                    CollisionPiece bp{};
                    if (collision == "box" || !HullPiece(points, (size_t)hullMaxSides, bp)) {
                        bp = BoxPiece(points);
                    }
                    CollGroup& g{ groups[bone] };
                    g.bone = bone;
                    for (const std::array<Vec3, 3>& t : bp.tris) {
                        AddCollTri(g, t[0], t[1], t[2]);
                    }
                }
                for (auto& [bone, g] : groups) {
                    if (!g.tris.empty()) {
                        collGroups.push_back(std::move(g));
                    }
                }
            } else {
                CollGroup& g{ collGroups.emplace_back() };
                if (collision == "mesh") {
                    // the mesh is clockwise
                    for (const MeshSurface& s : lod0.surfs) {
                        for (size_t i = 0; i < s.indices.size(); i += 3) {
                            AddCollTri(g, s.verts[s.indices[i]].pos, s.verts[s.indices[i + 2]].pos, s.verts[s.indices[i + 1]].pos);
                        }
                    }
                } else {
                    for (const CollisionPiece& piece : pieces) {
                        for (const std::array<Vec3, 3>& t : piece.tris) {
                            AddCollTri(g, t[0], t[1], t[2]);
                        }
                    }
                }
                if (g.tris.empty()) {
                    collGroups.clear();
                }
            }
            bool hasCollision{ !collGroups.empty() && !pieces.empty() };
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
                collArea /= (float)collTriCount;
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
            // the bones: the glTF skin's or one
            // "skeletonPositions": "gltf": the game's bones where the glTF's joints of their names are (the model's own
            // proportions), their orientations the game's
            if (useGameSkel && std::string{ cfg.GetString("skeletonPositions", "game") } == "gltf") {
                std::vector<std::optional<std::array<double, 3>>> pos(gameSkel.Count());
                size_t placed{};
                for (size_t b = 0; b < skel.nodes.size(); b++) {
                    int idx{ gameSkel.Index(skel.names[b]) };
                    if (idx < 0) {
                        continue;
                    }
                    double q[4], t[3];
                    skel.ToGame(skel.restWorld[b], q, t);
                    pos[idx] = std::array<double, 3>{ t[0], t[1], t[2] };
                    placed++;
                }
                gameSkel.Reposition(pos);
                LOG_INFO("{}: {} of the skeleton's {} bones at the glTF's joints", assetName, placed, gameSkel.Count());
            } else if (useGameSkel && std::string{ cfg.GetString("skeletonPositions", "game") } != "game") {
                Fail("unknown skeletonPositions, game or gltf");
                return;
            }
            size_t numBones{ useGameSkel ? gameSkel.numMain : skel.nodes.empty() ? 1 : skel.nodes.size() };
            if (numBones > 0xFF) {
                Fail("more than 255 bones");
                return;
            }
            model.numBones = (byte)numBones;
            model.numRootBones = (byte)(useGameSkel ? gameSkel.numRoots : skel.nodes.empty() ? 1 : skel.numRoots);
            model.numCosmeticBones = (uint16_t)(useGameSkel ? gameSkel.numCosmetic : 0);
            model.numCollSurfs = hasCollision ? (int32_t)collGroups.size() : 0;
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

            // bones (Load_XModel: names, name indices sorted at load, parents, quats, trans, classification, baseMat)
            std::vector<std::string> boneNames{ useGameSkel ? gameSkel.names : skel.nodes.empty() ? std::vector<std::string>{ boneName } : skel.names };
            size_t allBones{ boneNames.size() };
            std::vector<std::array<double, 3>> boneWorld(allBones);
            for (size_t b = 0; b < allBones; b++) {
                if (useGameSkel) {
                    Vec3 p{ BindPosition(gameSkel, b) };
                    boneWorld[b] = { p.x, p.y, p.z };
                } else if (b < skel.nodes.size()) {
                    double q[4], t[3];
                    skel.ToGame(skel.restWorld[b], q, t);
                    boneWorld[b] = { t[0], t[1], t[2] };
                }
            }
            model.boneNames = (ScrString_t*)fastfile::linker::memory::POINTER_NEXT;
            ff.data.Align(4);
            ScrString_t* boneStrs{ ff.data.AllocStreamPtr<ScrString_t>(allBones) };
            for (size_t b = 0; b < allBones; b++) {
                ctx.AddScrString(boneNames[b], boneStrs[b], ff);
            }
            model.boneNameIndices = (XModelBoneNameIndex*)fastfile::linker::memory::POINTER_NEXT;
            ff.data.Align(4);
            XModelBoneNameIndex* boneIndices{ ff.data.AllocStreamPtr<XModelBoneNameIndex>(numBones) };
            for (size_t b = 0; b < numBones; b++) {
                boneIndices[b] = {};
                boneIndices[b].index = (uint16_t)b;
                ctx.AddScrString(boneNames[b], boneIndices[b].name, ff);
            }
            size_t numChildren{ allBones - model.numRootBones };
            if (useGameSkel && numChildren) {
                // the skeleton's as they are
                model.parentList = (byte*)fastfile::linker::memory::POINTER_NEXT;
                ff.data.WriteStream(gameSkel.parentList.data(), gameSkel.parentList.size());
                model.quats = (uint64_t*)fastfile::linker::memory::POINTER_NEXT;
                ff.data.Align(8);
                ff.data.WriteStream(gameSkel.quats.data(), gameSkel.quats.size());
                model.trans = (byte*)fastfile::linker::memory::POINTER_NEXT;
                ff.data.Align(4);
                ff.data.WriteStream(gameSkel.trans.data(), gameSkel.trans.size());
            } else if (numChildren) {
                // the parent as the distance back, the bind pose local to the parent (no rotation at rest)
                model.parentList = (byte*)fastfile::linker::memory::POINTER_NEXT;
                std::vector<byte> parentList{};
                for (size_t b = model.numRootBones; b < numBones; b++) {
                    parentList.push_back((byte)(b - skel.parents[b]));
                }
                ff.data.WriteStream(parentList.data(), parentList.size());
                model.quats = (uint64_t*)fastfile::linker::memory::POINTER_NEXT;
                ff.data.Align(8);
                double identity[4]{ 0, 0, 0, 1 };
                for (size_t b = model.numRootBones; b < numBones; b++) {
                    ff.data.WriteStream<uint64_t>(PackQuat(identity));
                }
                // packed float3 per bone, the 16 bytes a bone the loader reads padded with zeros
                model.trans = (byte*)fastfile::linker::memory::POINTER_NEXT;
                ff.data.Align(4);
                std::vector<float> trans(4 * numChildren);
                for (size_t b = model.numRootBones; b < numBones; b++) {
                    const auto& w{ boneWorld[b] };
                    const auto& pw{ boneWorld[skel.parents[b]] };
                    size_t i{ 3 * (b - model.numRootBones) };
                    trans[i] = (float)(w[0] - pw[0]);
                    trans[i + 1] = (float)(w[1] - pw[1]);
                    trans[i + 2] = (float)(w[2] - pw[2]);
                }
                ff.data.WriteStream(trans.data(), trans.size() * sizeof(float));
            }

            model.partClassification = (byte*)fastfile::linker::memory::POINTER_NEXT;
            if (useGameSkel) {
                ff.data.WriteStream(gameSkel.partClassification.data(), gameSkel.partClassification.size());
            } else {
                // the bones' hit locations: the first "hitLocations" text in the bone's name, else the template's
                const rapidjson::Value* hitLocs{ Member(cfg.main, "hitLocations") };
                for (size_t b = 0; b < numBones; b++) {
                    byte loc{ partClassification[0] };
                    if (hitLocs && hitLocs->IsArray() && b < boneNames.size()) {
                        for (const rapidjson::Value& h : hitLocs->GetArray()) {
                            if (h.IsArray() && h.Size() == 2 && h[0].IsString() && h[1].IsInt() &&
                                boneNames[b].find(h[0].GetString()) != std::string::npos) {
                                loc = (byte)h[1].GetInt();
                                break;
                            }
                        }
                    }
                    ff.data.WriteStream(&loc, 1);
                }
            }

            // the bind pose in the model's space: quat, trans, transWeight
            model.baseMat = (byte*)fastfile::linker::memory::POINTER_NEXT;
            ff.data.Align(0x10);
            if (useGameSkel) {
                ff.data.WriteStream(gameSkel.baseMat.data(), gameSkel.baseMat.size());
            }
            for (size_t b = 0; !useGameSkel && b < numBones; b++) {
                std::vector<byte> bm{ baseMat };
                float* f{ (float*)bm.data() };
                f[0] = f[1] = f[2] = 0;
                f[3] = 1;
                f[4] = (float)boneWorld[b][0];
                f[5] = (float)boneWorld[b][1];
                f[6] = (float)boneWorld[b][2];
                f[7] = 2;
                ff.data.WriteStream(bm.data(), bm.size());
            }

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
                if (lod.skinned) {
                    mesh.flags = MESH_SKINNED;
                }
                mesh.xpakEntry = {};

                ff.data.PushStream(XFILE_BLOCK_VIRTUAL);

                // the bones used, msb first in 32 bits words, the root always
                auto PartBits = [](const MeshSurface& surf, std::vector<byte>& bits) {
                    if (bits.size() != PART_BITS_SIZE) {
                        bits.assign(PART_BITS_SIZE, 0);
                    }
                    auto Set = [&bits](uint32_t b) { ((uint32_t*)bits.data())[b >> 5] |= 0x80000000u >> (b & 31); };
                    Set(0);
                    for (const MeshVertex& v : surf.verts) {
                        for (int i = 0; i < 4; i++) {
                            if (v.weights[i]) {
                                Set(v.bones[i]);
                            }
                        }
                    }
                };
                std::vector<byte> lodPartBits{ meshPartBits };
                if (lod.skinned) {
                    lodPartBits.assign(PART_BITS_SIZE, 0);
                    for (const MeshSurface& surf : lod.surfs) {
                        PartBits(surf, lodPartBits);
                    }
                }
                if (lodPartBits.size() == PART_BITS_SIZE) {
                    mesh.partBits = (byte*)fastfile::linker::memory::POINTER_NEXT;
                    ff.data.Align(4);
                    ff.data.WriteStream(lodPartBits.data(), lodPartBits.size());
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
                // a skinned mesh has no rigid flag and a weight per vertex
                if (lod.skinned) {
                    sh.flags &= ~SHARED_RIGID;
                }
                sh.vertCount = lod.vertCount;
                sh.skinVertCount = lod.skinned ? lod.vertCount : 0;
                sh.indexCount = lod.indexCount;
                sh.blendTargetVertCount = 0;
                sh.dataSize = (uint32_t)lod.data.size();
                sh.posOffset = lod.posOffset;
                sh.vertsOffset = lod.vertsOffset;
                sh.indicesOffset = lod.indicesOffset;
                sh.skinWeightsOffset = lod.skinned ? lod.skinOffset : 0;
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
                    surf.vertListCount = lod.skinned ? 0 : 1;
                    if (lod.skinned) {
                        surf.tileMode = SURFACE_SKINNED;
                    }
                    surf.vertCount = (uint16_t)lod.surfs[i].verts.size();
                    surf.triCount = (uint16_t)(lod.surfs[i].indices.size() / 3);
                    surf.baseVertIndex = baseVert;
                    surf.baseIndexOffset = baseIndex;
                    surf.unk20 = 0;
                    baseVert += surf.vertCount;
                    baseIndex += (uint32_t)lod.surfs[i].indices.size();
                    // offset of the shared block in the virtual block, set at link
                    surf.shared = nullptr;
                    ff.data.AddRef(sharedChunk, (void**)&surf.shared);
                    surf.partBits = lod.skinned || surfPartBits.size() == PART_BITS_SIZE ? (byte*)fastfile::linker::memory::POINTER_NEXT : nullptr;
                    surf.vertList = lod.skinned ? nullptr : (XRigidVertList*)fastfile::linker::memory::POINTER_NEXT;
                }
                for (size_t i = 0; i < lod.surfs.size(); i++) {
                    if (surfs[i].partBits) {
                        std::vector<byte> bits{ surfPartBits };
                        if (lod.skinned) {
                            bits.clear();
                            PartBits(lod.surfs[i], bits);
                        }
                        ff.data.Align(4);
                        ff.data.WriteStream(bits.data(), bits.size());
                    }
                    if (surfs[i].vertList) {
                        ff.data.Align(2);
                        XRigidVertList& vertList{ ff.data.AllocStreamRef<XRigidVertList>() };
                        vertList = { 0, surfs[i].vertCount, 0, surfs[i].triCount };
                    }
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
            // a material of a json once in the zone, then by its name
            for (size_t slot = 0; slot < numLods; slot++) {
                const LodGeom& lod{ lods[numLods - 1 - slot] };
                ff.data.Align(8);
                void** mats{ ff.data.AllocStreamPtr<void*>(lod.materials.size()) };
                for (size_t i = 0; i < lod.materials.size(); i++) {
                    const std::string& material{ lod.materials[i] };
                    if (material[0] == '#') {
                        std::string refId{ std::format(",{}", material.substr(1)) };
                        ctx.LinkAsset(XAssetType::ASSET_TYPE_MATERIAL, refId.data(), mats[i], false, &ff);
                    } else if (!ff.SetInsert(LINKED_MATERIALS, ::hash::Hash64(material.data()))) {
                        std::string refId{ std::format(",{}", MaterialAssetName(ctx, material)) };
                        ctx.LinkAsset(XAssetType::ASSET_TYPE_MATERIAL, refId.data(), mats[i], false, &ff);
                    } else {
                        ctx.LinkAsset(XAssetType::ASSET_TYPE_MATERIAL, material.data(), mats[i], false, &ff);
                    }
                }
                ff.data.Align(4);
                for (size_t i = 0; i < lod.materials.size(); i++) {
                    ff.data.WriteStream<float>(radius > 0 ? 1.0f / (radius * radius) : 1.0f);
                }
            }

            // collision surfaces of the bullets: the array, then the triangles of each
            if (hasCollision) {
                model.collSurfs = (void*)fastfile::linker::memory::POINTER_NEXT;
                ff.data.Align(8);
                XModelCollSurf* css{ ff.data.AllocStreamPtr<XModelCollSurf>(collGroups.size()) };
                for (size_t i = 0; i < collGroups.size(); i++) {
                    XModelCollSurf& cs{ css[i] };
                    const CollGroup& g{ collGroups[i] };
                    cs = {};
                    cs.collTris = (XModelCollTri*)fastfile::linker::memory::POINTER_NEXT;
                    cs.numCollTris = (uint32_t)g.tris.size();
                    Vec3 csMins{ g.mins - Vec3{ 1, 1, 1 } * COLLSURF_BOUNDS_PADDING };
                    Vec3 csMaxs{ g.maxs + Vec3{ 1, 1, 1 } * COLLSURF_BOUNDS_PADDING };
                    std::memcpy(cs.mins, &csMins, sizeof(cs.mins));
                    std::memcpy(cs.maxs, &csMaxs, sizeof(cs.maxs));
                    cs.boneIdx = g.bone;
                    cs.contents = COLLSURF_CONTENTS;
                }
                for (const CollGroup& g : collGroups) {
                    // DB_PushStreamPos(XFILE_BLOCK_MEMMAPPED) keeps the current block on pc (DB_EnableMemMappedData)
                    ff.data.Align(4);
                    ff.data.WriteStream(g.tris.data(), g.tris.size() * sizeof(g.tris[0]));
                }
            }

            // bone info: mins, maxs, center, radius squared, the rest from the template
            model.boneInfo = (byte*)fastfile::linker::memory::POINTER_NEXT;
            ff.data.Align(4);
            if (useGameSkel && hitboxes == "skeleton") {
                // the skeleton's hit boxes, no collmap per bone (the last byte)
                for (size_t b = 0; b < numBones; b++) {
                    std::vector<byte> info{ gameSkel.boneInfo.begin() + BONE_INFO_SIZE * b, gameSkel.boneInfo.begin() + BONE_INFO_SIZE * (b + 1) };
                    info[BONE_INFO_SIZE - 1] = 0xFF;
                    ff.data.WriteStream(info.data(), info.size());
                }
            } else if (!lod0.skinned) {
                float* bi{ (float*)boneInfo.data() };
                std::memcpy(&bi[0], &mins, sizeof(mins));
                std::memcpy(&bi[3], &maxs, sizeof(maxs));
                std::memcpy(&bi[6], &center, sizeof(center));
                bi[9] = half.Dot(half);
                for (size_t b = 0; b < numBones; b++) {
                    ff.data.WriteStream(boneInfo.data(), boneInfo.size());
                }
            } else {
                // the vertices a bone moves, in its frame (the game skeleton's bones are rotated)
                std::vector<Vec3> bmins(numBones, Vec3{ 1e30f, 1e30f, 1e30f }), bmaxs(numBones, Vec3{ -1e30f, -1e30f, -1e30f });
                for (const MeshSurface& surf : lod0.surfs) {
                    for (const MeshVertex& v : surf.verts) {
                        for (int i = 0; i < 4; i++) {
                            if (v.weights[i] && v.bones[i] < numBones) {
                                const auto& w{ boneWorld[v.bones[i]] };
                                Vec3 rel{ v.pos - Vec3{ (float)w[0], (float)w[1], (float)w[2] } };
                                if (useGameSkel) {
                                    rel = ToBoneFrame(gameSkel, v.bones[i], rel);
                                }
                                GrowBounds(bmins[v.bones[i]], bmaxs[v.bones[i]], rel);
                            }
                        }
                    }
                }
                for (size_t b = 0; b < numBones; b++) {
                    std::vector<byte> info{ boneInfo };
                    float* bi{ (float*)info.data() };
                    Vec3 bmin{ bmins[b] }, bmax{ bmaxs[b] };
                    if (bmin.x > bmax.x) {
                        bmin = bmax = {};
                    }
                    Vec3 c{ (bmin + bmax) * 0.5f };
                    Vec3 h{ (bmax - bmin) * 0.5f };
                    std::memcpy(&bi[0], &bmin, sizeof(bmin));
                    std::memcpy(&bi[3], &bmax, sizeof(bmax));
                    std::memcpy(&bi[6], &c, sizeof(c));
                    bi[9] = h.Dot(h);
                    ff.data.WriteStream(info.data(), info.size());
                }
            }

            // the part bits of the bones the meshes use (the union of the meshes' on all the game's models), the root
            // always: the template's has only the root
            if (lod0.skinned) {
                unkC0.assign(PART_BITS_SIZE, 0);
                auto Set = [&unkC0](uint32_t b) { ((uint32_t*)unkC0.data())[b >> 5] |= 0x80000000u >> (b & 31); };
                Set(0);
                for (const LodGeom& lod : lods) {
                    for (const MeshSurface& surf : lod.surfs) {
                        for (const MeshVertex& v : surf.verts) {
                            for (int i = 0; i < 4; i++) {
                                if (v.weights[i]) {
                                    Set(v.bones[i]);
                                }
                            }
                        }
                    }
                }
            }
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
                "Added asset xmodel {} (hash_{:x}), {} vertices, {} triangles, {} surface(s), {} lod(s), {} bone(s){}{}{}", assetName,
                model.name.name, lod0.vertCount, lod0.indexCount / 3, lod0.surfs.size(), numLods, numBones,
                lod0.skinned ? ", skinned" : "", lod0.stream ? ", streamed mesh" : "", physPreset.empty() ? "" : ", physics"
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
