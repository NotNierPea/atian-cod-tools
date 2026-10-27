#include <includes.hpp>
#include <tools/fastfile/linkers/linker_bo4.hpp>

/*
 * col_map (clipMap_t, retail 0x2D8, the loader sub_142E37BA0, the layout in bo4_unlinker_map_col.cpp): a map's
 * collision. For now the least the game takes: the world model (cmodel 0), one node and one empty leaf over the map's bounds, no
 * brushes, triangles, static models or dyn ents (the ground can be a script model with a collmap). CM_LoadMap copies the
 * box brush and box model per trace thread. The rope pool is runtime memory, the 375 u16 table zeros as in the game's,
 * the box brush/model, the extra brush after the brushes and the leaf are the game's with no surface flags.
 *
 * {
 *     "name": "maps/zm/enh_test.d3dbsp",   // maps/<mode>/<map>.d3dbsp
 *     "mins": [ -4096, -4096, -1024 ],
 *     "maxs": [ 4096, 4096, 4096 ],
 *     "floor": { "mins": [ -512, -512, -64 ], "maxs": [ 512, 512, 0 ] }   // optional, a solid box brush
 * }
 * "brushes": [ { "mins": [..], "maxs": [..], "contents": 0x10000 } ] adds box brushes after the floor (contents 0x100001,
 * solid, by default; 0x10000 player clip only, which AI and bullets go through).
 * "convexBrushes": "maps/my_brushes.json" adds convex brushes after those: a json array of
 *   { "mins": [..], "maxs": [..], "planes": [ [nx, ny, nz, dist], .. ], "verts": [ [x, y, z], .. ], "contents": .. }
 * (the planes past the bounds, outward, a point inside when n.p <= dist; the verts its corners). A level's collision as
 * world brushes: the players' ground stays the world (each script model with a collmap is its own ground entity, the
 * viewmodel's sprint restarted at each change) and the traces find them through a tree (a model's collmap has none).
 * Their sides point into ClipInfo's planes (0x14: normal, dist, u16 surface material index) and their verts into its brush verts
 * (0xC, block 8: on pc the current block), aliases as the loader (sub_142E160C0) takes them.
 * The floor: one axial brush (0x40: sides 0, verts 8, bounds 0x10/0x20, contents 0x1C, axial faces' flags 0x2C, u16
 * verts 0x38, u8 sides 0x3A), without sides or verts as the game's box brush (its traces use the bounds). ClipInfo's
 * 0x20 list is the leaf brush nodes (count at 4: < 0 an inner node, its brush indices (u32) after it), node 0 none:
 * node 1 lists brush 0 and the leaf points at it (0x28), with the brush's contents (4).
 * The leaf brush nodes (ClipInfo's 0x10 array, cLeafBrushNode_t 0x20: axis 0, count 4, contents 8, 0x10 a leaf's brush
 * list (u32 indices) or a split's dist, range 0x14, u32 child offsets 0x18 / 0x1C in nodes from it: retail's are u32,
 * the old elf's u16 pair at 0x18 crashed its traces (sub_142882440)): a count over 0 is a leaf,
 * 0 a split (CM_TestInLeafBrushNode_r: a box past dist on the axis goes to child 0, under it to child 1, across to
 * both), under 0 a split whose own brushes are the leaf right after it (traced first). With the convex brushes node 1
 * is the root of a tree of at most LEAF_BRUSHES a leaf, the brushes across a split kept at it (copied into both
 * children, the big walls and floors made thousands of nodes).
 */
namespace {
    using namespace fastfile::linker::bo4;

    struct ClipBlob {
        byte* data;
        uint32_t size;
    };
    static_assert(sizeof(ClipBlob) == 0x10);

    struct ClipInfoRetail {
        uint32_t planeCount;
        byte* planes;
        uint32_t numMaterials;
        byte* materials;
        uint32_t count20;
        byte* array20;
        uint32_t numBrushVerts;
        byte* brushVerts;
        uint32_t nuinds;
        byte* uinds;
        uint32_t numBrushes;
        byte* brushes;
        uint32_t count60;
        byte* array60;
    };
    static_assert(sizeof(ClipInfoRetail) == 0x70);

    struct ClipMapRetail {
        XHash name;
        uint64_t unk10;
        ClipBlob* blob18;
        uint32_t numStaticModels;
        byte* staticModelList;
        uint32_t count30;
        void** list30;
        uint32_t count40;
        void** list40;
        uint32_t numNodes;
        byte* nodes;
        uint32_t numLeafs;
        byte* leafs;
        uint32_t vertCount;
        byte* verts;
        int32_t triCount;
        byte* tris;
        int32_t partitionCount;
        byte* partitions;
        int32_t aabbTreeCount;
        ClipBlob* aabbTrees;
        uint32_t numSubModels;
        byte* cmodels;
        uint32_t numClusters;
        uint32_t clusterBytes;
        byte* boxBrush;
        byte boxModel[0x58];
        uint32_t unk128;
        uint32_t count12C;
        uint32_t* array130;
        uint32_t count138;
        uint16_t* array140;
        uint16_t dynEntCount[4];
        void* dynEntDefList[2];
        byte* dynEntPoseList[2];
        byte* dynEntClientList[2];
        byte* dynEntServerList[2];
        byte* dynEntCollList[4];
        uint32_t numConstraints;
        void* constraints;
        uint32_t maxRopes;
        byte* ropes;
        uint32_t terrainSurfaceCount;
        void* terrainSurfaces;
        ScrString_t miscModelTargetnameGroups[32];
        ClipInfoRetail info;
        uint32_t checksum;
        uint32_t pad2d4;
    };
    static_assert(sizeof(ClipMapRetail) == 0x2D8);
    static_assert(offsetof(ClipMapRetail, boxBrush) == 0xC8);
    static_assert(offsetof(ClipMapRetail, dynEntCount) == 0x148);
    static_assert(offsetof(ClipMapRetail, maxRopes) == 0x1C0);
    static_assert(offsetof(ClipMapRetail, info) == 0x260);

    constexpr size_t LEAF_SIZE = 0x38;
    constexpr size_t CMODEL_SIZE = 0x58;
    constexpr size_t BRUSH_SIZE = 0x40;
    constexpr size_t MATERIAL_SIZE = 0x20;
    constexpr size_t PLANE_SIZE = 0x14;
    constexpr size_t LEAF_BRUSHES = 8;
    constexpr int TREE_DEPTH = 48;
    constexpr size_t ROPE_SIZE = 0x11A0;
    // the game's maps (32 frontend, 35 zm_towers)
    constexpr uint32_t MAX_ROPES = 32;
    // the u16 table at 0x140, zeros in all the game's maps
    constexpr uint32_t TABLE_140_COUNT = 375;
    // the client's own dyn ents (gibs, createdynentandlaunch, sub_142F07460): added after the map's (0 here, the base at
    // 0x128) in dyn ent list 0, so the list has room for them as the frontend's (500 empty defs, their runtime lists)
    constexpr uint16_t CLIENT_DYNENT_SLOTS = 500;
    constexpr size_t DYNENT_DEF_SIZE = 0x188;
    constexpr size_t DYNENT_POSE_SIZE = 0xB0;
    constexpr size_t DYNENT_CLIENT_SIZE = 0x120;
    constexpr size_t DYNENT_COLL_SIZE = 0x20;

    /*
     * A brush without sides: the game's box brush and the brush after the brushes. The six u16 at 0x2C are its axial faces'
     * surface flags, indices in ClipInfo's u64 array (the load swaps them for indices in a table of all of them, sub_142877AB0).
     */
    void TemplateBrush(byte* brush, uint16_t axialMaterial) {
        std::memset(brush, 0, BRUSH_SIZE);
        *(uint32_t*)(brush + 0x1C) = 0xFFFFFFFF;
        for (size_t i = 0; i < 6; i++) {
            *(uint16_t*)(brush + 0x2C + 2 * i) = axialMaterial;
        }
    }

    class XAssetLinkerImpl : public XAssetLinker {
      public:
        using XAssetLinker::XAssetLinker;

        void Compute(BO4LinkContext& ctx, const char* id, BO4FFContext& ff) override {
            std::filesystem::path path{ ctx.linkCtx.input / id };
            BO4Json cfg{ path };
            if (!cfg.SyncConfig(false)) {
                LOG_ERROR("Can't read col_map {}", path.string());
                ctx.error = true;
                return;
            }
            const rapidjson::Value& root{ cfg.main };
            auto Vec = [&root](const char* name, float(&out)[3], const float(&def)[3]) {
                auto it{ root.FindMember(name) };
                for (size_t i = 0; i < 3; i++) {
                    out[i] = it != root.MemberEnd() && it->value.IsArray() && i < it->value.Size() && it->value[(rapidjson::SizeType)i].IsNumber()
                        ? (float)it->value[(rapidjson::SizeType)i].GetDouble() : def[i];
                }
            };
            std::string assetName{ cfg.GetString("name", "") };
            if (assetName.empty()) {
                LOG_ERROR("col_map {}: no name (maps/<mode>/<map>.d3dbsp)", path.string());
                ctx.error = true;
                return;
            }
            float mins[3], maxs[3];
            Vec("mins", mins, { -4096, -4096, -1024 });
            Vec("maxs", maxs, { 4096, 4096, 4096 });
            auto floorIt{ root.FindMember("floor") };
            const bool hasFloor{ floorIt != root.MemberEnd() && floorIt->value.IsObject() };
            float floorMins[3]{ -512, -512, -64 };
            float floorMaxs[3]{ 512, 512, 0 };
            if (hasFloor) {
                auto FloorVec = [&](const char* name, float(&out)[3]) {
                    auto it{ floorIt->value.FindMember(name) };
                    for (rapidjson::SizeType i = 0; i < 3; i++) {
                        if (it != floorIt->value.MemberEnd() && it->value.IsArray() && i < it->value.Size() && it->value[i].IsNumber()) {
                            out[i] = it->value[i].GetFloat();
                        }
                    }
                };
                FloorVec("mins", floorMins);
                FloorVec("maxs", floorMaxs);
            }
            constexpr uint32_t BRUSH_CONTENTS = 0x00100001;
            // the box brushes: the floor, then "brushes" ({ mins, maxs, contents }, contents solid by default)
            struct Box {
                float mins[3];
                float maxs[3];
                uint32_t contents;
            };
            std::vector<Box> boxes{};
            if (hasFloor) {
                Box& b{ boxes.emplace_back() };
                std::memcpy(b.mins, floorMins, sizeof(b.mins));
                std::memcpy(b.maxs, floorMaxs, sizeof(b.maxs));
                b.contents = BRUSH_CONTENTS;
            }
            auto brushesIt{ root.FindMember("brushes") };
            if (brushesIt != root.MemberEnd() && brushesIt->value.IsArray()) {
                for (const rapidjson::Value& bv : brushesIt->value.GetArray()) {
                    Box& b{ boxes.emplace_back() };
                    b.contents = BRUSH_CONTENTS;
                    for (const char* k : { "mins", "maxs" }) {
                        auto it{ bv.FindMember(k) };
                        float* out{ k[1] == 'i' ? b.mins : b.maxs };
                        for (rapidjson::SizeType i = 0; i < 3; i++) {
                            out[i] = it != bv.MemberEnd() && it->value.IsArray() && i < it->value.Size() && it->value[i].IsNumber() ? it->value[i].GetFloat() : 0;
                        }
                    }
                    auto ct{ bv.FindMember("contents") };
                    if (ct != bv.MemberEnd() && ct->value.IsNumber()) {
                        b.contents = ct->value.GetUint();
                    } else if (ct != bv.MemberEnd() && ct->value.IsString()) {
                        b.contents = (uint32_t)std::strtoul(ct->value.GetString(), nullptr, 0);
                    }
                }
            }
            // the convex brushes: their planes past the bounds and corners
            struct Convex {
                float mins[3];
                float maxs[3];
                uint32_t contents;
                std::vector<std::array<float, 4>> planes{};
                std::vector<std::array<float, 3>> verts{};
            };
            std::vector<Convex> convex{};
            if (const char* file{ cfg.GetCString("convexBrushes") }; file && *file) {
                std::string buffer{};
                if (!utils::ReadFile(path.parent_path() / file, buffer)) {
                    LOG_ERROR("col_map {}: can't read convexBrushes {}", path.string(), file);
                    ctx.error = true;
                    return;
                }
                rapidjson::Document doc{};
                doc.Parse(buffer.data());
                if (doc.HasParseError() || !doc.IsArray()) {
                    LOG_ERROR("col_map {}: convexBrushes {} isn't a json array", path.string(), file);
                    ctx.error = true;
                    return;
                }
                auto Floats = [](const rapidjson::Value& v, float* out, size_t n) {
                    for (rapidjson::SizeType i = 0; i < n; i++) {
                        out[i] = v.IsArray() && i < v.Size() && v[i].IsNumber() ? v[i].GetFloat() : 0;
                    }
                };
                for (const rapidjson::Value& bv : doc.GetArray()) {
                    Convex& c{ convex.emplace_back() };
                    c.contents = BRUSH_CONTENTS;
                    if (auto it{ bv.FindMember("contents") }; it != bv.MemberEnd() && it->value.IsNumber()) {
                        c.contents = it->value.GetUint();
                    }
                    if (auto it{ bv.FindMember("mins") }; it != bv.MemberEnd()) Floats(it->value, c.mins, 3);
                    if (auto it{ bv.FindMember("maxs") }; it != bv.MemberEnd()) Floats(it->value, c.maxs, 3);
                    if (auto it{ bv.FindMember("planes") }; it != bv.MemberEnd() && it->value.IsArray()) {
                        for (const rapidjson::Value& pl : it->value.GetArray()) {
                            Floats(pl, c.planes.emplace_back().data(), 4);
                        }
                    }
                    if (auto it{ bv.FindMember("verts") }; it != bv.MemberEnd() && it->value.IsArray()) {
                        for (const rapidjson::Value& v : it->value.GetArray()) {
                            Floats(v, c.verts.emplace_back().data(), 3);
                        }
                    }
                    if (c.planes.size() > 0xFF || c.verts.size() > 0xFFFF) {
                        LOG_ERROR("col_map {}: a convex brush of {} planes / {} verts (at most 255 / 65535)", path.string(), c.planes.size(), c.verts.size());
                        ctx.error = true;
                        return;
                    }
                }
            }
            // every brush's bounds and contents, the boxes first
            struct BrushBounds {
                float mins[3];
                float maxs[3];
                uint32_t contents;
            };
            std::vector<BrushBounds> all{};
            for (const Box& b : boxes) {
                all.push_back({ { b.mins[0], b.mins[1], b.mins[2] }, { b.maxs[0], b.maxs[1], b.maxs[2] }, b.contents });
            }
            for (const Convex& c : convex) {
                all.push_back({ { c.mins[0], c.mins[1], c.mins[2] }, { c.maxs[0], c.maxs[1], c.maxs[2] }, c.contents });
            }
            uint32_t allContents{};
            for (const BrushBounds& b : all) {
                allContents |= b.contents;
            }

            // the leaf brush nodes from node 1 (node 0 is the game's empty one): a tree, pre-order (children after their
            // node, child 0 right after it)
            struct LeafNode {
                byte axis{};
                uint32_t contents{};
                float dist{};
                size_t child[2]{};
                std::vector<uint32_t> brushes{};
                bool leaf{};
                bool ownLeaf{}; // a split with its brushes in the leaf after it (count -1)
            };
            std::vector<LeafNode> tree{};
            std::function<size_t(std::vector<uint32_t>&, int)> Build = [&](std::vector<uint32_t>& idx, int depth) -> size_t {
                size_t me{ tree.size() };
                tree.emplace_back();
                uint32_t contents{};
                for (uint32_t i : idx) {
                    contents |= all[i].contents;
                }
                tree[me].contents = contents;
                auto Leaf = [&]() {
                    tree[me].leaf = true;
                    tree[me].brushes = idx;
                    return me;
                };
                if (idx.size() <= LEAF_BRUSHES || depth >= TREE_DEPTH) {
                    return Leaf();
                }
                float lo[3]{ FLT_MAX, FLT_MAX, FLT_MAX }, hi[3]{ -FLT_MAX, -FLT_MAX, -FLT_MAX };
                for (uint32_t i : idx) {
                    for (int a = 0; a < 3; a++) {
                        float c{ (all[i].mins[a] + all[i].maxs[a]) * 0.5f };
                        lo[a] = std::min(lo[a], c);
                        hi[a] = std::max(hi[a], c);
                    }
                }
                int axis{ hi[0] - lo[0] >= hi[1] - lo[1] && hi[0] - lo[0] >= hi[2] - lo[2] ? 0 : hi[1] - lo[1] >= hi[2] - lo[2] ? 1 : 2 };
                if (hi[axis] - lo[axis] < 1) {
                    return Leaf();
                }
                std::vector<float> centres{};
                for (uint32_t i : idx) {
                    centres.push_back((all[i].mins[axis] + all[i].maxs[axis]) * 0.5f);
                }
                std::nth_element(centres.begin(), centres.begin() + centres.size() / 2, centres.end());
                float dist{ centres[centres.size() / 2] };
                // past the split, under it, across it (kept here)
                std::vector<uint32_t> front{}, back{}, across{};
                for (uint32_t i : idx) {
                    if (all[i].mins[axis] >= dist) front.push_back(i);
                    else if (all[i].maxs[axis] <= dist) back.push_back(i);
                    else across.push_back(i);
                }
                if (front.empty() || back.empty()) {
                    return Leaf();
                }
                tree[me].axis = (byte)axis;
                tree[me].dist = dist;
                if (!across.empty()) {
                    tree[me].ownLeaf = true;
                    size_t own{ tree.size() };
                    tree.emplace_back();
                    tree[own].leaf = true;
                    tree[own].brushes = across;
                    for (uint32_t i : across) {
                        tree[own].contents |= all[i].contents;
                    }
                }
                size_t f{ Build(front, depth + 1) };
                size_t b{ Build(back, depth + 1) };
                tree[me].child[0] = f - me;
                tree[me].child[1] = b - me;
                return me;
            };
            if (!all.empty()) {
                std::vector<uint32_t> idx(all.size());
                for (uint32_t i = 0; i < (uint32_t)all.size(); i++) {
                    idx[i] = i;
                }
                Build(idx, 0);
            }
            size_t planeCount{}, vertCount{};
            for (const Convex& c : convex) {
                planeCount += c.planes.size();
                vertCount += c.verts.size();
            }
            float radius{};
            for (size_t i = 0; i < 3; i++) {
                float e{ std::max(std::abs(mins[i]), std::abs(maxs[i])) };
                radius += e * e;
            }
            radius = std::sqrt(radius);

            ff.data.PushStream(XFILE_BLOCK_TEMP);
            ff.data.Align(8);
            ClipMapRetail& cm{ ff.data.AllocStreamRef<ClipMapRetail>() };
            cm = {};
            cm.name.name = ctx.HashXHash(assetName, true);
            cm.blob18 = (ClipBlob*)fastfile::linker::memory::POINTER_NEXT;
            cm.numNodes = 1;
            cm.nodes = (byte*)fastfile::linker::memory::POINTER_NEXT;
            cm.numLeafs = 1;
            cm.leafs = (byte*)fastfile::linker::memory::POINTER_NEXT;
            cm.numSubModels = 1;
            cm.cmodels = (byte*)fastfile::linker::memory::POINTER_NEXT;
            cm.numClusters = 1;
            cm.boxBrush = (byte*)fastfile::linker::memory::POINTER_NEXT;
            // the game's box model: a leaf with no aabb (-1), inverted bounds, its material index at 0x48
            *(uint32_t*)(cm.boxModel + 0x20) = 0xFFFFFFFF;
            for (size_t i = 0; i < 3; i++) {
                *(float*)(cm.boxModel + 0x28 + 4 * i) = FLT_MAX;
                *(float*)(cm.boxModel + 0x34 + 4 * i) = -FLT_MAX;
            }
            cm.count12C = 1;
            cm.array130 = (uint32_t*)fastfile::linker::memory::POINTER_NEXT;
            cm.count138 = TABLE_140_COUNT;
            cm.array140 = (uint16_t*)fastfile::linker::memory::POINTER_NEXT;
            cm.unk128 = 0;
            cm.dynEntCount[0] = CLIENT_DYNENT_SLOTS;
            cm.dynEntDefList[0] = (void*)fastfile::linker::memory::POINTER_NEXT;
            cm.dynEntPoseList[0] = (byte*)fastfile::linker::memory::POINTER_NEXT;
            cm.dynEntClientList[0] = (byte*)fastfile::linker::memory::POINTER_NEXT;
            cm.dynEntCollList[0] = (byte*)fastfile::linker::memory::POINTER_NEXT;
            cm.maxRopes = MAX_ROPES;
            cm.ropes = (byte*)fastfile::linker::memory::POINTER_NEXT;
            cm.info.planeCount = (uint32_t)planeCount;
            cm.info.planes = planeCount ? (byte*)fastfile::linker::memory::POINTER_NEXT : nullptr;
            cm.info.numMaterials = (uint32_t)(1 + tree.size());
            cm.info.materials = (byte*)fastfile::linker::memory::POINTER_NEXT;
            cm.info.numBrushVerts = (uint32_t)vertCount;
            cm.info.brushVerts = vertCount ? (byte*)fastfile::linker::memory::POINTER_NEXT : nullptr;
            cm.info.numBrushes = (uint32_t)all.size();
            cm.info.brushes = (byte*)fastfile::linker::memory::POINTER_NEXT;
            cm.info.count60 = 1;
            cm.info.array60 = (byte*)fastfile::linker::memory::POINTER_NEXT;

            // the arrays in the loader's order, each on its alignment (DB_AllocStreamPos(n): n + 1)
            ff.data.PushStream(XFILE_BLOCK_VIRTUAL);

            /*
             * blob 0x18: the static models' tree (8 wide, sub_1404F1600), walked by the static model traces
             * (sub_14287D9D0) without a null check. A node: count at 0, leaf flag at 1; an empty root (count 0) visits
             * nothing. The loader puts the data in block 8, acts links no block 8: in VIRTUAL as the nodes and leafs
             */
            ff.data.Align(8);
            ClipBlob& blob{ ff.data.AllocStreamRef<ClipBlob>() };
            blob.data = (byte*)fastfile::linker::memory::POINTER_NEXT;
            blob.size = 0x10;
            byte emptyTree[0x10]{};
            ff.data.WriteStream(emptyTree, sizeof(emptyTree));

            /*
             * the node: the world's traces start at node 0 (CM_TraceThroughTree, sub_1428829E0). 0x18: plane normal 0,
             * dist 0xC, children int16 0x10/0x12 (< 0: leaf ~child), type 0x14 (0..2 axial). Axial on z, both sides leaf 0
             */
            ff.data.Align(4);
            byte node[0x18]{};
            *(float*)(node + 0x8) = 1;
            *(int16_t*)(node + 0x10) = -1;
            *(int16_t*)(node + 0x12) = -1;
            node[0x14] = 2;
            ff.data.WriteStream(node, sizeof(node));

            // the leaf: no aabb (-1), no contents, the map's bounds, no leaf brush node (0, the game's start at 1), cluster 0
            ff.data.Align(8);
            byte leaf[LEAF_SIZE]{};
            *(uint32_t*)leaf = 0xFFFFFFFF;
            if (!all.empty()) {
                *(uint32_t*)(leaf + 0x4) = allContents;
                *(uint64_t*)(leaf + 0x28) = 1;
            }
            std::memcpy(leaf + 0x0C, mins, sizeof(mins));
            std::memcpy(leaf + 0x18, maxs, sizeof(maxs));
            ff.data.WriteStream(leaf, sizeof(leaf));

            // the world model: bounds and radius, its leaf zeros as in the game's cmodel 0
            ff.data.Align(8);
            byte cmodel[CMODEL_SIZE]{};
            std::memcpy(cmodel, mins, sizeof(mins));
            std::memcpy(cmodel + 0xC, maxs, sizeof(maxs));
            std::memcpy(cmodel + 0x18, &radius, sizeof(radius));
            ff.data.WriteStream(cmodel, sizeof(cmodel));

            ff.data.Align(16);
            byte boxBrush[BRUSH_SIZE];
            TemplateBrush(boxBrush, 0);
            ff.data.WriteStream(boxBrush, sizeof(boxBrush));

            ff.data.Align(4);
            ff.data.WriteStream<uint32_t>(0);

            ff.data.Align(2);
            std::vector<uint16_t> table140(TABLE_140_COUNT);
            ff.data.WriteStream(table140.data(), table140.size() * sizeof(table140[0]));

            // dyn ent list 0: the empty defs, then its poses, client data and collision in runtime memory (block 2)
            ff.data.Align(8);
            std::vector<byte> defs(DYNENT_DEF_SIZE * CLIENT_DYNENT_SLOTS);
            ff.data.WriteStream(defs.data(), defs.size());
            auto Runtime = [&ff](size_t align, size_t size) {
                ff.data.PushStream(XFILE_BLOCK_RUNTIME_VIRTUAL);
                ff.data.Align(align);
                ff.data.AllocRuntime(size);
                ff.data.PopStream();
            };
            Runtime(8, DYNENT_POSE_SIZE * CLIENT_DYNENT_SLOTS);
            Runtime(8, DYNENT_CLIENT_SIZE * CLIENT_DYNENT_SLOTS);
            Runtime(4, DYNENT_COLL_SIZE * CLIENT_DYNENT_SLOTS);

            // the rope pool is runtime memory (DB_PushStreamPos(2)), not in the file
            ff.data.PushStream(XFILE_BLOCK_RUNTIME_VIRTUAL);
            ff.data.Align(8);
            ff.data.AllocRuntime(ROPE_SIZE * MAX_ROPES);
            ff.data.PopStream();

            // ClipInfo in the loader's order: the planes (each convex brush's a chunk its sides point to), the leaf brush
            // nodes (node 0 empty, the tree from 1) and their leaves' lists, the brush verts (each brush's a chunk), the
            // brushes and the one after them, one u64 of surface flags (none) for their faces
            std::vector<fastfile::linker::memory::LinkerDataChunk*> planeChunks(convex.size()), vertChunks(convex.size());
            if (planeCount) {
                ff.data.Align(4);
                for (size_t i = 0; i < convex.size(); i++) {
                    if (convex[i].planes.empty()) {
                        continue;
                    }
                    std::vector<byte> planes(PLANE_SIZE * convex[i].planes.size());
                    for (size_t k = 0; k < convex[i].planes.size(); k++) {
                        const auto& pl{ convex[i].planes[k] };
                        byte* o{ planes.data() + PLANE_SIZE * k };
                        // normal, dist, then the u16 surface material index the traces read (sub_142881E50: 0, the
                        // brushes' one material; not the old elf's type and signbits)
                        std::memcpy(o, pl.data(), 16);
                    }
                    planeChunks[i] = ff.data.WriteStream(planes.data(), planes.size());
                }
            }

            ff.data.Align(8);
            byte material[MATERIAL_SIZE]{};
            ff.data.WriteStream(material, sizeof(material));
            for (const LeafNode& n : tree) {
                byte node[MATERIAL_SIZE]{};
                node[0] = n.axis;
                *(uint32_t*)(node + 0x8) = n.contents;
                if (n.leaf) {
                    *(int32_t*)(node + 0x4) = (int32_t)n.brushes.size();
                    *(uint64_t*)(node + 0x10) = fastfile::linker::memory::POINTER_NEXT;
                } else {
                    *(int32_t*)(node + 0x4) = n.ownLeaf ? -1 : 0;
                    *(float*)(node + 0x10) = n.dist;
                    *(uint32_t*)(node + 0x18) = (uint32_t)n.child[0];
                    *(uint32_t*)(node + 0x1C) = (uint32_t)n.child[1];
                }
                ff.data.WriteStream(node, sizeof(node));
            }
            for (const LeafNode& n : tree) {
                if (n.leaf) {
                    ff.data.Align(4);
                    ff.data.WriteStream(n.brushes.data(), n.brushes.size() * sizeof(uint32_t));
                }
            }

            if (vertCount) {
                ff.data.Align(4);
                for (size_t i = 0; i < convex.size(); i++) {
                    if (!convex[i].verts.empty()) {
                        vertChunks[i] = ff.data.WriteStream(convex[i].verts.data(), convex[i].verts.size() * sizeof(convex[i].verts[0]));
                    }
                }
            }

            ff.data.Align(16);
            for (const Box& b : boxes) {
                byte brush[BRUSH_SIZE];
                TemplateBrush(brush, 0);
                std::memcpy(brush + 0x10, b.mins, sizeof(b.mins));
                *(uint32_t*)(brush + 0x1C) = b.contents;
                std::memcpy(brush + 0x20, b.maxs, sizeof(b.maxs));
                ff.data.WriteStream(brush, sizeof(brush));
            }
            for (size_t i = 0; i < convex.size(); i++) {
                const Convex& c{ convex[i] };
                byte* brush{ ff.data.AllocStreamPtr<byte>(BRUSH_SIZE) };
                TemplateBrush(brush, 0);
                std::memcpy(brush + 0x10, c.mins, sizeof(c.mins));
                *(uint32_t*)(brush + 0x1C) = c.contents;
                std::memcpy(brush + 0x20, c.maxs, sizeof(c.maxs));
                if (planeChunks[i]) {
                    ff.data.AddRef(planeChunks[i], (void**)brush);
                    brush[0x3A] = (byte)c.planes.size();
                }
                if (vertChunks[i]) {
                    ff.data.AddRef(vertChunks[i], (void**)(brush + 0x8));
                    *(uint16_t*)(brush + 0x38) = (uint16_t)c.verts.size();
                }
            }
            byte lastBrush[BRUSH_SIZE];
            TemplateBrush(lastBrush, 0);
            ff.data.WriteStream(lastBrush, sizeof(lastBrush));

            ff.data.Align(4);
            ff.data.WriteStream<uint64_t>(0);

            ff.data.PopStream();
            ff.data.PopStream();

            LOG_INFO(
                "Added asset col_map {} (hash_{:x}), bounds ({} {} {}) ({} {} {}), {}",
                assetName, cm.name.name, mins[0], mins[1], mins[2], maxs[0], maxs[1], maxs[2],
                std::format("{} box and {} convex brush(es), {} planes, {} verts, {} leaf brush nodes", boxes.size(), convex.size(), planeCount, vertCount, tree.size())
            );
        }
    };

    utils::MapAdder<XAssetLinkerImpl, XAssetType, XAssetLinker> impl{ GetWorkers(), XAssetType::ASSET_TYPE_CLIPMAP };
} // namespace
