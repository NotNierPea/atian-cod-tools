#include <includes.hpp>
#include <tools/fastfile/handlers/bo4/bo4_unlinker_map.hpp>
#include <core/hashes/raw_file_extractor.hpp>
#include <tools/utils/data_utils.hpp>

/*
 * game_map (GameWorld, retail 0x60, the loader sub_142E1BA70): the header, its PathData (0x38 at 0x10, sub_142E25700:
 * nodes (nodeCount + 0x80, 0xA8 each, the extra ones for the nodes made at runtime), basenodes in the runtime block,
 * pathVis, the node tree), the tactical graph (0x48, 0x2738, sub_142E2F5B0) and the two 0x1E0008 tables at 0x50/0x58
 * summed up, for the game_map linker.
 */
namespace fastfile::handlers::bo4::map {
    using namespace games::bo4::pool;

    struct GameWorldRetail {
        XHash name;
        uint32_t nodeCount;
        uint32_t originalNodeCount;
        byte* nodes;
        byte* basenodes;
        uint32_t visBytes;
        byte* pathVis;
        uint32_t nodeTreeCount;
        byte* nodeTree;
        byte* tacGraph;
        byte* table50;
        byte* table58;
    };
    static_assert(sizeof(GameWorldRetail) == 0x60);

    constexpr size_t GAME_TABLE_SIZE = 0x1E0008;

    std::string GameHex(const void* data, size_t len) {
        static const char* digits{ "0123456789abcdef" };
        std::string out{};
        for (size_t i = 0; i < len; i++) {
            byte b{ ((const byte*)data)[i] };
            out += digits[b >> 4];
            out += digits[b & 0xF];
        }
        return out;
    }

    class GameMapWorker : public Worker {
        void Unlink(fastfile::FastFileOption& opt, void* ptr) override {
            GameWorldRetail& asset{ *(GameWorldRetail*)ptr };
            BO4JsonWriter json{};
            json.BeginObject();
            json.WriteFieldValueXHash("name", asset.name);
            json.WriteFieldValueString("header", GameHex(&asset, sizeof(asset)));
            json.WriteFieldValueNumber("nodeCount", asset.nodeCount);
            json.WriteFieldValueNumber("originalNodeCount", asset.originalNodeCount);
            if (asset.nodes) {
                json.WriteFieldValueString("node0", GameHex(asset.nodes, 0xA8));
                // the last spare node, as written
                json.WriteFieldValueString("nodeLast", GameHex(asset.nodes + 0xA8ull * (asset.nodeCount + 0x7F), 0xA8));
            }
            json.WriteFieldValueBool("basenodes", asset.basenodes != nullptr);
            json.WriteFieldValueNumber("visBytes", asset.visBytes);
            json.WriteFieldValueNumber("nodeTreeCount", asset.nodeTreeCount);
            if (asset.nodeTree) {
                json.WriteFieldValueString("nodeTree", GameHex(asset.nodeTree, 0x18ull * std::min<uint32_t>(asset.nodeTreeCount, 8)));
            }
            if (asset.tacGraph) {
                size_t nonZero{};
                for (size_t i = 0; i < 0x2738; i++) {
                    nonZero += asset.tacGraph[i] != 0;
                }
                json.WriteFieldValueNumber("tacGraphNonZero", nonZero);
                json.WriteFieldValueString("tacGraph", GameHex(asset.tacGraph, nonZero < 0x100 ? 0x2738 : 0x40));
            }
            auto Table = [&](const char* key, const byte* table) {
                if (!table) {
                    return;
                }
                size_t nonZero{};
                for (size_t i = 0; i < GAME_TABLE_SIZE; i++) {
                    nonZero += table[i] != 0;
                }
                json.WriteFieldNameString(key);
                json.BeginObject();
                json.WriteFieldValueNumber("nonZero", nonZero);
                json.WriteFieldValueString("head", GameHex(table, 0x40));
                json.WriteFieldValueString("at100000", GameHex(table + 0x100000, 0x40));
                json.WriteFieldValueString("tail", GameHex(table + GAME_TABLE_SIZE - 0x20, 0x20));
                json.EndObject();
            };
            Table("table50", asset.table50);
            Table("table58", asset.table58);
            json.EndObject();

            const char* name{ hashutils::ExtractPtr(asset.name) };
            if (!name) {
                name = utils::va("maps/hashed/file_%llx.d3dbsp", asset.name.name);
            }
            std::filesystem::path outFile{ opt.m_output / "bo4" / "source" / name / "game_map.json" };
            std::filesystem::create_directories(outFile.parent_path());
            LOG_OPT_INFO("Dump game_map {}", outFile.string());
            if (!json.WriteToFile(outFile)) {
                LOG_ERROR("Error when dumping {}", outFile.string());
            }

            // the raw path data beside it: nodes.bin (nodeCount + 0x80 nodes), links.bin (each node's links, 0xC by the
            // u16 count at 0x70, in node order), vis.bin, nodetree.bin (0x18 each) and nodetree_leaves.bin (a leaf's u16
            // node indices: count at 0x8, in tree order)
            std::filesystem::path dir{ outFile.parent_path() };
            if (asset.nodes) {
                size_t count{ asset.nodeCount + 0x80ull };
                utils::WriteFile(dir / "nodes.bin", asset.nodes, 0xA8 * count);
                std::vector<byte> links{};
                for (size_t i = 0; i < count; i++) {
                    const byte* node{ asset.nodes + 0xA8 * i };
                    const byte* ptr{ *(const byte* const*)node };
                    uint16_t n{ *(const uint16_t*)(node + 0x70) };
                    if (ptr && n) {
                        links.insert(links.end(), ptr, ptr + 0xCull * n);
                    }
                }
                utils::WriteFile(dir / "links.bin", links.data(), links.size());
            }
            if (asset.pathVis && asset.visBytes) {
                utils::WriteFile(dir / "vis.bin", asset.pathVis, asset.visBytes);
            }
            if (asset.nodeTree) {
                utils::WriteFile(dir / "nodetree.bin", asset.nodeTree, 0x18ull * asset.nodeTreeCount);
                std::vector<byte> leaves{};
                for (size_t i = 0; i < asset.nodeTreeCount; i++) {
                    const byte* e{ asset.nodeTree + 0x18 * i };
                    if (*(const int32_t*)e < 0 && *(const byte* const*)(e + 0x10)) {
                        const byte* list{ *(const byte* const*)(e + 0x10) };
                        leaves.insert(leaves.end(), list, list + 2ull * *(const int32_t*)(e + 8));
                    }
                }
                utils::WriteFile(dir / "nodetree_leaves.bin", leaves.data(), leaves.size());
            }

            // the tactical graph (sub_142E2F5B0): tacgraph.bin (its 0x2738 header: point count, the points (0x2C each)
            // at 0x08, their vis (0x320 each) at 0x10, the direction tables, a tree's count / nodes (0x18 each, a leaf's
            // u16 point list: count at 0x8, list at 0x10) at 0x2718 / 0x2720, and 0x50 records' count / array at
            // 0x2728 / 0x2730, each a u16 list: count at 0x34, list at 0x48), tacpoints.bin, tacvis.bin, tactree.bin +
            // tactree_leaves.bin, tacrecords.bin + tacrecords_lists.bin
            if (asset.tacGraph) {
                const byte* tg{ asset.tacGraph };
                utils::WriteFile(dir / "tacgraph.bin", tg, 0x2738);
                int32_t count{ *(const int32_t*)tg };
                const byte* tpoints{ *(const byte* const*)(tg + 0x08) };
                const byte* vpoints{ *(const byte* const*)(tg + 0x10) };
                if (count > 0 && tpoints) {
                    utils::WriteFile(dir / "tacpoints.bin", tpoints, 0x2Cull * count);
                }
                if (count > 0 && vpoints) {
                    utils::WriteFile(dir / "tacvis.bin", vpoints, 0x320ull * count);
                }
                int32_t treeCount{ *(const int32_t*)(tg + 0x2718) };
                const byte* tree{ *(const byte* const*)(tg + 0x2720) };
                if (treeCount > 0 && tree) {
                    utils::WriteFile(dir / "tactree.bin", tree, 0x18ull * treeCount);
                    std::vector<byte> leaves{};
                    for (int32_t i = 0; i < treeCount; i++) {
                        const byte* e{ tree + 0x18 * i };
                        if (*(const int32_t*)e < 0 && *(const byte* const*)(e + 0x10)) {
                            const byte* list{ *(const byte* const*)(e + 0x10) };
                            leaves.insert(leaves.end(), list, list + 2ull * *(const int32_t*)(e + 8));
                        }
                    }
                    utils::WriteFile(dir / "tactree_leaves.bin", leaves.data(), leaves.size());
                }
                int32_t recCount{ *(const int32_t*)(tg + 0x2728) };
                const byte* recs{ *(const byte* const*)(tg + 0x2730) };
                if (recCount > 0 && recs) {
                    utils::WriteFile(dir / "tacrecords.bin", recs, 0x50ull * recCount);
                    std::vector<byte> lists{};
                    for (int32_t i = 0; i < recCount; i++) {
                        const byte* e{ recs + 0x50 * i };
                        const byte* list{ *(const byte* const*)(e + 0x48) };
                        if (list) {
                            lists.insert(lists.end(), list, list + 2ull * *(const int32_t*)(e + 0x34));
                        }
                    }
                    utils::WriteFile(dir / "tacrecords_lists.bin", lists.data(), lists.size());
                }
            }
        }
    };

    utils::MapAdder<GameMapWorker, games::bo4::pool::XAssetType, Worker> implgame{
        GetWorkers(), games::bo4::pool::XAssetType::ASSET_TYPE_GAMEWORLD
    };
} // namespace fastfile::handlers::bo4::map
