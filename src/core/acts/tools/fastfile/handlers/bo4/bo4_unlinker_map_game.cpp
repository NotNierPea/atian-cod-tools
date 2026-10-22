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
        }
    };

    utils::MapAdder<GameMapWorker, games::bo4::pool::XAssetType, Worker> implgame{
        GetWorkers(), games::bo4::pool::XAssetType::ASSET_TYPE_GAMEWORLD
    };
} // namespace fastfile::handlers::bo4::map
