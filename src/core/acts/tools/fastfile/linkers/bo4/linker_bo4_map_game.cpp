#include <includes.hpp>
#include <tools/fastfile/linkers/linker_bo4.hpp>

/*
 * game_map (GameWorld, retail 0x60, the loader sub_142E1BA70, the layout in bo4_unlinker_map_game.cpp): a map's path
 * nodes for the AI. For now none, as the game's frontend world: no nodes but the 0x80 the game adds at runtime (the
 * nodes array is nodeCount + 0x80, 0xA8 each), the base nodes in the runtime block, one node tree leaf (axis -1) without
 * nodes and an empty tactical graph (0x2738 of zeros). The two 0x1E0008 tables at 0x50/0x58 are null in the game's.
 *
 * {
 *     "name": "maps/zm/enh_test.d3dbsp"   // maps/<mode>/<map>.d3dbsp
 * }
 */
namespace {
    using namespace fastfile::linker::bo4;

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

    constexpr size_t PATHNODE_SIZE = 0xA8;
    constexpr size_t BASENODE_SIZE = 0x10;
    // nodes the game makes at runtime, after those of the map
    constexpr size_t RUNTIME_NODES = 0x80;
    constexpr size_t NODE_TREE_SIZE = 0x18;
    constexpr size_t TACTICAL_GRAPH_SIZE = 0x2738;

    class XAssetLinkerImpl : public XAssetLinker {
      public:
        using XAssetLinker::XAssetLinker;

        void Compute(BO4LinkContext& ctx, const char* id, BO4FFContext& ff) override {
            std::filesystem::path path{ ctx.linkCtx.input / id };
            BO4Json cfg{ path };
            if (!cfg.SyncConfig(false)) {
                LOG_ERROR("Can't read game_map {}", path.string());
                ctx.error = true;
                return;
            }
            std::string assetName{ cfg.GetString("name", "") };
            if (assetName.empty()) {
                LOG_ERROR("game_map {}: no name (maps/<mode>/<map>.d3dbsp)", path.string());
                ctx.error = true;
                return;
            }

            ff.data.PushStream(XFILE_BLOCK_TEMP);
            ff.data.Align(8);
            GameWorldRetail& game{ ff.data.AllocStreamRef<GameWorldRetail>() };
            game = {};
            game.name.name = ctx.HashXHash(assetName, true);
            game.nodes = (byte*)fastfile::linker::memory::POINTER_NEXT;
            game.basenodes = (byte*)fastfile::linker::memory::POINTER_NEXT;
            game.nodeTreeCount = 1;
            game.nodeTree = (byte*)fastfile::linker::memory::POINTER_NEXT;
            game.tacGraph = (byte*)fastfile::linker::memory::POINTER_NEXT;

            // the loader's order (sub_142E25700, then the tactical graph), each on its alignment
            ff.data.PushStream(XFILE_BLOCK_VIRTUAL);
            ff.data.Align(8);
            std::vector<byte> nodes(PATHNODE_SIZE * RUNTIME_NODES);
            ff.data.WriteStream(nodes.data(), nodes.size());

            ff.data.PushStream(XFILE_BLOCK_RUNTIME_VIRTUAL);
            ff.data.Align(16);
            ff.data.AllocRuntime(BASENODE_SIZE * RUNTIME_NODES);
            ff.data.PopStream();

            // a leaf (axis < 0) of no nodes
            ff.data.Align(8);
            byte tree[NODE_TREE_SIZE]{};
            *(int32_t*)tree = -1;
            ff.data.WriteStream(tree, sizeof(tree));

            ff.data.Align(8);
            std::vector<byte> tacGraph(TACTICAL_GRAPH_SIZE);
            ff.data.WriteStream(tacGraph.data(), tacGraph.size());
            ff.data.PopStream();

            ff.data.PopStream();

            LOG_INFO("Added asset game_map {} (hash_{:x}), no path nodes", assetName, game.name.name);
        }
    };

    utils::MapAdder<XAssetLinkerImpl, XAssetType, XAssetLinker> impl{ GetWorkers(), XAssetType::ASSET_TYPE_GAMEWORLD };
} // namespace
