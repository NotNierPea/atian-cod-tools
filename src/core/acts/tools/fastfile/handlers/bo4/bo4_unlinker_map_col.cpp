#include <includes.hpp>
#include <tools/fastfile/handlers/bo4/bo4_unlinker_map.hpp>
#include <core/hashes/raw_file_extractor.hpp>
#include <tools/utils/data_utils.hpp>

/*
 * col_map (clipMap_t, retail 0x2D8, the loader sub_142E37BA0): the header and its arrays as they are, for the col_map
 * linker. The order is the old elf's without its inline ClipInfo, which moved to the end (sub_142E160C0, 0x70), the
 * counts before their arrays:
 *   0x18 a blob {data, size}; 0x20 static models (0x60, XModel at 0); 0x30 and 0x40 pointers to 0x200 objects
 *   (sub_142E17CD0); 0x50 nodes (0x18); 0x60 leafs (0x38); 0x70 verts (0xC); 0x80 tris (0xC); 0x90 partitions (0x24);
 *   0xA0 aabb trees (0x10: a blob each); 0xB0 cmodels (0x58); 0xC0 clusters; 0xC8 box_brush (0x40); 0xD0 box_model;
 *   0x12C u32s, 0x138 u16s; dyn ents (u16 counts at 0x148, lists from 0x150); 0x1B0 constraints; 0x1C0 ropes
 *   (0x11A0); 0x1D0 terrain; 0x1E0 32 script strings; 0x260 ClipInfo; 0x2D0 checksum.
 * ClipInfo: planes (0x14), materials (0x20, count at 4, u32s at 0x10), u32s, brush verts (0xC), uinds (u32), brushes
 * (count + 1, 0x40: sides at 0, verts at 8, u16 numverts at 0x38, u8 numsides at 0x3A), u64s.
 */
namespace fastfile::handlers::bo4::map {
    using namespace games::bo4::pool;

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
        uint16_t unk148[4]; // the dyn ent counts
        void* dynEntDefList[2];
        byte* dynEntPoseList[2];
        byte* dynEntClientList[2];
        byte* dynEntServerList[2];
        byte* dynEntCollList[4];
        uint32_t numConstraints;
        void* constraints;
        uint32_t maxRopes;
        void* ropes;
        uint32_t terrainSurfaceCount;
        void* terrainSurfaces;
        ScrString_t miscModelTargetnameGroups[32];
        ClipInfoRetail info;
        uint32_t checksum;
        uint32_t pad2d4;
    };
    static_assert(sizeof(ClipMapRetail) == 0x2D8);
    static_assert(offsetof(ClipMapRetail, boxBrush) == 0xC8);
    static_assert(offsetof(ClipMapRetail, unk148) == 0x148);
    static_assert(offsetof(ClipMapRetail, numConstraints) == 0x1B0);
    static_assert(offsetof(ClipMapRetail, miscModelTargetnameGroups) == 0x1E0);
    static_assert(offsetof(ClipMapRetail, info) == 0x260);

    std::string ColHex(const void* data, size_t len) {
        static const char* digits{ "0123456789abcdef" };
        std::string out{};
        out.reserve(len * 2);
        for (size_t i = 0; i < len; i++) {
            byte b{ ((const byte*)data)[i] };
            out += digits[b >> 4];
            out += digits[b & 0xF];
        }
        return out;
    }

    class ColMapWorker : public Worker {
        void PreLoadWorker(fastfile::FastFileContext* ctx) override {
            if (!ctx) {
                return;
            }
            // maps/<mode>/<ff>.d3dbsp
            std::string gamemode{ ctx->ffname };
            size_t fdd{ gamemode.find('_') };
            if (fdd != std::string::npos) {
                gamemode.resize(fdd);
            }
            const char* str{ utils::va("maps/%s/%s.d3dbsp", gamemode.c_str(), ctx->ffname) };
            hashutils::AddPrecomputed(hash::Hash64(str), str, true);
        }

        void Unlink(fastfile::FastFileOption& opt, void* ptr) override {
            ClipMapRetail& asset{ *(ClipMapRetail*)ptr };
            BO4JsonWriter json{};
            auto Blob = [&json](const char* key, const void* data, size_t len) {
                if (data && len) {
                    json.WriteFieldValueString(key, ColHex(data, len));
                }
            };
            auto Count = [&json](const char* key, size_t value) {
                json.WriteFieldValueNumber(key, value);
            };

            json.BeginObject();
            json.WriteFieldValueXHash("name", asset.name);
            json.WriteFieldValueString("header", ColHex(&asset, sizeof(asset)));
            if (asset.blob18) {
                Count("blob18Size", asset.blob18->size);
                Blob("blob18", asset.blob18->data, asset.blob18->size);
            }

            Count("numStaticModels", asset.numStaticModels);
            json.WriteFieldNameString("staticModels");
            json.BeginArray();
            for (size_t i = 0; asset.staticModelList && i < asset.numStaticModels; i++) {
                byte* sm{ asset.staticModelList + 0x60 * i };
                json.BeginObject();
                void* model{ *(void**)sm };
                if (model) {
                    json.WriteFieldValueXHash("model", *(XHash*)model);
                }
                json.WriteFieldValueString("raw", ColHex(sm, 0x60));
                json.EndObject();
            }
            json.EndArray();

            auto Objects = [&](const char* key, uint32_t count, void** list) {
                Count(utils::va("%sCount", key), count);
                if (!list) {
                    return;
                }
                json.WriteFieldNameString(key);
                json.BeginArray();
                for (size_t i = 0; i < count; i++) {
                    json.WriteValueString(list[i] ? ColHex(list[i], 0x200) : "");
                }
                json.EndArray();
            };
            Objects("list30", asset.count30, asset.list30);
            Objects("list40", asset.count40, asset.list40);

            Count("numNodes", asset.numNodes);
            Blob("nodes", asset.nodes, 0x18ull * asset.numNodes);
            Count("numLeafs", asset.numLeafs);
            Blob("leafs", asset.leafs, 0x38ull * asset.numLeafs);
            Count("vertCount", asset.vertCount);
            Blob("verts", asset.verts, 0xCull * asset.vertCount);
            Count("triCount", asset.triCount);
            Blob("tris", asset.tris, 0xCull * asset.triCount);
            Count("partitionCount", asset.partitionCount);
            Blob("partitions", asset.partitions, 0x24ull * asset.partitionCount);
            Count("aabbTreeCount", asset.aabbTreeCount);
            json.WriteFieldNameString("aabbTrees");
            json.BeginArray();
            for (int32_t i = 0; asset.aabbTrees && i < asset.aabbTreeCount; i++) {
                json.WriteValueString(ColHex(asset.aabbTrees[i].data, asset.aabbTrees[i].size));
            }
            json.EndArray();
            Count("numSubModels", asset.numSubModels);
            Blob("cmodels", asset.cmodels, 0x58ull * asset.numSubModels);
            Count("numClusters", asset.numClusters);
            Count("clusterBytes", asset.clusterBytes);
            Blob("boxBrush", asset.boxBrush, 0x40);
            Count("count12C", asset.count12C);
            Blob("array130", asset.array130, 4ull * asset.count12C);
            Count("count138", asset.count138);
            Blob("array140", asset.array140, 2ull * asset.count138);

            json.WriteFieldNameString("dynEntCounts");
            json.BeginArray();
            for (uint16_t c : asset.unk148) {
                json.WriteValueNumber(c);
            }
            json.EndArray();
            Count("numConstraints", asset.numConstraints);
            Count("maxRopes", asset.maxRopes);
            Count("terrainSurfaceCount", asset.terrainSurfaceCount);

            json.WriteFieldNameString("miscModelTargetnameGroups");
            json.BeginArray();
            for (ScrString_t s : asset.miscModelTargetnameGroups) {
                json.WriteValueString(GetScrString(s));
            }
            json.EndArray();

            // the ClipInfo
            ClipInfoRetail& info{ asset.info };
            json.WriteFieldNameString("info");
            json.BeginObject();
            Count("planeCount", info.planeCount);
            Blob("planes", info.planes, 0x14ull * info.planeCount);
            Count("numMaterials", info.numMaterials);
            json.WriteFieldNameString("materials");
            json.BeginArray();
            for (size_t i = 0; info.materials && i < info.numMaterials; i++) {
                byte* m{ info.materials + 0x20 * i };
                int32_t n{ *(int32_t*)(m + 4) };
                uint32_t* list{ *(uint32_t**)(m + 0x10) };
                json.BeginObject();
                json.WriteFieldValueString("raw", ColHex(m, 0x20));
                if (n > 0 && list) {
                    json.WriteFieldValueString("list", ColHex(list, 4ull * n));
                }
                json.EndObject();
            }
            json.EndArray();
            Count("count20", info.count20);
            Blob("array20", info.array20, 4ull * info.count20);
            Count("numBrushVerts", info.numBrushVerts);
            Blob("brushVerts", info.brushVerts, 0xCull * info.numBrushVerts);
            Count("nuinds", info.nuinds);
            Blob("uinds", info.uinds, 4ull * info.nuinds);
            Count("numBrushes", info.numBrushes);
            json.WriteFieldNameString("brushes");
            json.BeginArray();
            for (size_t i = 0; info.brushes && i <= info.numBrushes; i++) {
                byte* b{ info.brushes + 0x40 * i };
                json.WriteValueString(ColHex(b, 0x40));
            }
            json.EndArray();
            Count("count60", info.count60);
            Blob("array60", info.array60, 8ull * info.count60);
            json.EndObject();
            Count("checksum", asset.checksum);
            json.EndObject();

            const char* name{ hashutils::ExtractPtr(asset.name) };
            if (!name) {
                name = utils::va("maps/hashed/file_%llx.d3dbsp", asset.name.name);
            }
            std::filesystem::path outFile{ opt.m_output / "bo4" / "source" / name / "col_map.json" };
            std::filesystem::create_directories(outFile.parent_path());
            LOG_OPT_INFO("Dump col_map {}", outFile.string());
            if (!json.WriteToFile(outFile)) {
                LOG_ERROR("Error when dumping {}", outFile.string());
            }
        }
    };

    utils::MapAdder<ColMapWorker, games::bo4::pool::XAssetType, Worker> implcol{
        GetWorkers(), games::bo4::pool::XAssetType::ASSET_TYPE_CLIPMAP
    };
} // namespace fastfile::handlers::bo4::map
