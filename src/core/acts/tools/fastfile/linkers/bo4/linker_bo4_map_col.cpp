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
 * The floor: one axial brush (0x40: sides 0, verts 8, bounds 0x10/0x20, contents 0x1C, axial faces' flags 0x2C, u16
 * verts 0x38, u8 sides 0x3A), without sides or verts as the game's box brush (its traces use the bounds). ClipInfo's
 * 0x20 list is the leaf brush nodes (count at 4: < 0 an inner node, its brush indices (u32) after it), node 0 none:
 * node 1 lists brush 0 and the leaf points at it (0x28), with the brush's contents (4).
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
            uint32_t allContents{};
            for (const Box& b : boxes) {
                allContents |= b.contents;
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
            cm.info.numMaterials = boxes.empty() ? 1 : 2;
            cm.info.materials = (byte*)fastfile::linker::memory::POINTER_NEXT;
            cm.info.numBrushes = (uint32_t)boxes.size();
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
            if (!boxes.empty()) {
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

            // ClipInfo: one material without anything, the brush after the brushes, one u64 of surface flags (none) for its faces
            ff.data.Align(8);
            byte material[MATERIAL_SIZE]{};
            ff.data.WriteStream(material, sizeof(material));
            if (!boxes.empty()) {
                // node 1: a leaf (axis 0) of all the brushes, its list after the nodes
                byte node[MATERIAL_SIZE]{};
                *(int32_t*)(node + 0x4) = (int32_t)boxes.size();
                *(uint32_t*)(node + 0x8) = allContents;
                *(uint64_t*)(node + 0x10) = fastfile::linker::memory::POINTER_NEXT;
                ff.data.WriteStream(node, sizeof(node));
                ff.data.Align(4);
                for (uint32_t i = 0; i < (uint32_t)boxes.size(); i++) {
                    ff.data.WriteStream<uint32_t>(i);
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
            byte lastBrush[BRUSH_SIZE];
            TemplateBrush(lastBrush, 0);
            ff.data.WriteStream(lastBrush, sizeof(lastBrush));

            ff.data.Align(4);
            ff.data.WriteStream<uint64_t>(0);

            ff.data.PopStream();
            ff.data.PopStream();

            LOG_INFO(
                "Added asset col_map {} (hash_{:x}), bounds ({} {} {}) ({} {} {}), {}",
                assetName, cm.name.name, mins[0], mins[1], mins[2], maxs[0], maxs[1], maxs[2], std::format("{} brush(es)", boxes.size())
            );
        }
    };

    utils::MapAdder<XAssetLinkerImpl, XAssetType, XAssetLinker> impl{ GetWorkers(), XAssetType::ASSET_TYPE_CLIPMAP };
} // namespace
