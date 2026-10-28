#include <includes.hpp>
#include <tools/fastfile/linkers/linker_bo4.hpp>

/*
 * game_map (GameWorld, retail 0x60, the loader sub_142E1BA70, the layout in bo4_unlinker_map_game.cpp): a map's path
 * nodes for the AI. For now none, as the game's frontend world: no nodes but the 0x80 the game adds at runtime (the
 * nodes array is nodeCount + 0x80, 0xA8 each), the base nodes in the runtime block, one node tree leaf (axis -1) without
 * nodes and an empty tactical graph (0x2738 of zeros). The two 0x1E0008 tables at 0x50/0x58 are null in the game's.
 *
 * {
 *     "name": "maps/zm/enh_test.d3dbsp",  // maps/<mode>/<map>.d3dbsp
 *     "nodes": [                          // optional: the map's path nodes, in the order of its node_ entities
 *         { "type": 20, "origin": [ x, y, z ], "yaw": 0, "extents": [ 24, 32, 28.5 ], "flags": 32796, "partner": 1, "mantle": -1,
 *           "connect": 1, "cost": 1, "samples": [ [ x, y, z ], ... ] }
 *     ]
 * }
 * The game spawns each node_ entity into the next baked node (sub_14405DE40: nodes + 0xA8 * a counter, no check
 * against nodeCount), its keys over the node's fields (origin, angles, the strings, movementtype_*, cost_modifier,
 * connect_navmesh_regions), so the map's node entities need as many baked nodes. A node (zm_towers' negotiation
 * volumes): 0x00 its samples (vec3s on the floor along its back, the u16 count at 0x70), 0x08 half extents, 0x14 origin,
 * 0x20 its yaw's cos / sin, 0x28 type, 0x40 flags (0x801c on a volume), 0x54 its traversal's other end ("partner": the
 * game pairs the -1 ones by their baked target / targetname strings at load, sub_14406F970, before the entities spawn
 * and fill those, so a pair without baked strings stays -1 and the first traversal raised 0x75FFFD17), 0x58 its mantle (-1), 0x5C cost, 0x64
 * yaw, 0x68 / 0x6C -1, 0x72 0xffff, 0x74 connect_navmesh_regions. The visibility 2 bits a pair of nodes, none here; the
 * node tree one leaf of them all.
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

            // the baked nodes
            struct Node {
                byte data[PATHNODE_SIZE]{};
                std::vector<float> samples{};
            };
            std::vector<Node> baked{};
            auto nodesIt{ cfg.main.FindMember("nodes") };
            if (nodesIt != cfg.main.MemberEnd() && nodesIt->value.IsArray()) {
                for (const rapidjson::Value& n : nodesIt->value.GetArray()) {
                    Node& node{ baked.emplace_back() };
                    auto Vec = [&](const char* key, float* out, size_t count) {
                        auto it{ n.FindMember(key) };
                        if (it != n.MemberEnd() && it->value.IsArray()) {
                            for (rapidjson::SizeType i = 0; i < it->value.Size() && i < count; i++) {
                                out[i] = (float)it->value[i].GetDouble();
                            }
                        }
                    };
                    auto Num = [&](const char* key, double def) {
                        auto it{ n.FindMember(key) };
                        return it != n.MemberEnd() && it->value.IsNumber() ? it->value.GetDouble() : def;
                    };
                    byte* d{ node.data };
                    Vec("extents", (float*)(d + 0x08), 3);
                    Vec("origin", (float*)(d + 0x14), 3);
                    float yaw{ (float)Num("yaw", 0) };
                    *(float*)(d + 0x20) = std::cos(yaw * 3.14159265358979f / 180.0f);
                    *(float*)(d + 0x24) = std::sin(yaw * 3.14159265358979f / 180.0f);
                    *(uint32_t*)(d + 0x28) = (uint32_t)Num("type", 1);
                    *(uint32_t*)(d + 0x40) = (uint32_t)Num("flags", 0);
                    *(int32_t*)(d + 0x54) = (int32_t)Num("partner", -1);
                    *(int32_t*)(d + 0x58) = (int32_t)Num("mantle", -1);
                    *(float*)(d + 0x5C) = (float)Num("cost", 1);
                    *(float*)(d + 0x64) = yaw;
                    *(int32_t*)(d + 0x68) = -1;
                    *(int32_t*)(d + 0x6C) = -1;
                    *(uint16_t*)(d + 0x72) = 0xFFFF;
                    *(uint32_t*)(d + 0x74) = (uint32_t)Num("connect", 0);
                    auto samplesIt{ n.FindMember("samples") };
                    if (samplesIt != n.MemberEnd() && samplesIt->value.IsArray()) {
                        for (const rapidjson::Value& s : samplesIt->value.GetArray()) {
                            for (rapidjson::SizeType i = 0; i < 3; i++) {
                                node.samples.push_back((float)s[i].GetDouble());
                            }
                        }
                    }
                    *(uint16_t*)(d + 0x70) = (uint16_t)(node.samples.size() / 3);
                    if (!node.samples.empty()) {
                        *(uint64_t*)d = fastfile::linker::memory::POINTER_NEXT;
                    }
                }
            }
            size_t nodeCount{ baked.size() };
            // 2 bits a pair of nodes (zm_towers': 436 nodes, 23708 bytes)
            size_t visBytes{ (nodeCount * (nodeCount ? nodeCount - 1 : 0) + 7) / 8 };

            ff.data.PushStream(XFILE_BLOCK_TEMP);
            ff.data.Align(8);
            GameWorldRetail& game{ ff.data.AllocStreamRef<GameWorldRetail>() };
            game = {};
            game.name.name = ctx.HashXHash(assetName, true);
            game.nodeCount = (uint32_t)nodeCount;
            game.originalNodeCount = (uint32_t)nodeCount;
            game.nodes = (byte*)fastfile::linker::memory::POINTER_NEXT;
            game.basenodes = (byte*)fastfile::linker::memory::POINTER_NEXT;
            game.visBytes = (uint32_t)visBytes;
            game.pathVis = visBytes ? (byte*)fastfile::linker::memory::POINTER_NEXT : nullptr;
            game.nodeTreeCount = 1;
            game.nodeTree = (byte*)fastfile::linker::memory::POINTER_NEXT;
            game.tacGraph = (byte*)fastfile::linker::memory::POINTER_NEXT;

            // the loader's order (sub_142E25700, then the tactical graph), each on its alignment
            ff.data.PushStream(XFILE_BLOCK_VIRTUAL);
            ff.data.Align(8);
            std::vector<byte> nodes(PATHNODE_SIZE * (nodeCount + RUNTIME_NODES));
            for (size_t i = 0; i < nodeCount; i++) {
                std::memcpy(&nodes[PATHNODE_SIZE * i], baked[i].data, PATHNODE_SIZE);
            }
            ff.data.WriteStream(nodes.data(), nodes.size());
            // each node's samples after the nodes, in node order
            for (const Node& node : baked) {
                if (!node.samples.empty()) {
                    ff.data.Align(4);
                    ff.data.WriteStream(node.samples.data(), node.samples.size() * sizeof(float));
                }
            }

            ff.data.PushStream(XFILE_BLOCK_RUNTIME_VIRTUAL);
            ff.data.Align(16);
            ff.data.AllocRuntime(BASENODE_SIZE * (nodeCount + RUNTIME_NODES));
            ff.data.PopStream();

            if (visBytes) {
                std::vector<byte> vis(visBytes);
                ff.data.WriteStream(vis.data(), vis.size());
            }

            // a leaf (axis < 0) of all the nodes
            ff.data.Align(8);
            byte tree[NODE_TREE_SIZE]{};
            *(int32_t*)tree = -1;
            *(int32_t*)(tree + 8) = (int32_t)nodeCount;
            if (nodeCount) {
                *(uint64_t*)(tree + 0x10) = fastfile::linker::memory::POINTER_NEXT;
            }
            ff.data.WriteStream(tree, sizeof(tree));
            if (nodeCount) {
                ff.data.Align(2);
                std::vector<uint16_t> list(nodeCount);
                for (size_t i = 0; i < nodeCount; i++) {
                    list[i] = (uint16_t)i;
                }
                ff.data.WriteStream(list.data(), list.size() * sizeof(uint16_t));
            }

            ff.data.Align(8);
            std::vector<byte> tacGraph(TACTICAL_GRAPH_SIZE);
            ff.data.WriteStream(tacGraph.data(), tacGraph.size());
            ff.data.PopStream();

            ff.data.PopStream();

            LOG_INFO("Added asset game_map {} (hash_{:x}), {} path nodes", assetName, game.name.name, nodeCount);
        }
    };

    utils::MapAdder<XAssetLinkerImpl, XAssetType, XAssetLinker> impl{ GetWorkers(), XAssetType::ASSET_TYPE_GAMEWORLD };
} // namespace
