#include <includes.hpp>
#include <tools/fastfile/linkers/linker_bo4.hpp>

/*
 * com_map (ComWorld, retail 0x60, the loader sub_142E16C10, the layout in bo4_unlinker_map_com.cpp): a map's lights and
 * light probes as the server knows them. For now none: no primary lights (0x258 each, the game's first one is a real
 * light, not a placeholder), exploders, sun volumes or probes. Com_LoadWorld wants isInUse set.
 *
 * {
 *     "name": "maps/zm/enh_test.d3dbsp"   // maps/<mode>/<map>.d3dbsp
 * }
 */
namespace {
    using namespace fastfile::linker::bo4;

    struct ComWorld {
        XHash name;
        bool isInUse;
        uint32_t primaryLightCount;
        void* primaryLights;
        uint32_t lightExploderCount;
        void* lightExploders;
        uint32_t sunVolumeCount;
        uint32_t* sunVolumeProbeCount;
        uint32_t probeCount;
        uint32_t* probeGuids;
        uint32_t probeExploderCount;
        void* probeExploders;
    };
    static_assert(sizeof(ComWorld) == 0x60);

    class XAssetLinkerImpl : public XAssetLinker {
      public:
        using XAssetLinker::XAssetLinker;

        void Compute(BO4LinkContext& ctx, const char* id, BO4FFContext& ff) override {
            std::filesystem::path path{ ctx.linkCtx.input / id };
            BO4Json cfg{ path };
            if (!cfg.SyncConfig(false)) {
                LOG_ERROR("Can't read com_map {}", path.string());
                ctx.error = true;
                return;
            }
            std::string assetName{ cfg.GetString("name", "") };
            if (assetName.empty()) {
                LOG_ERROR("com_map {}: no name (maps/<mode>/<map>.d3dbsp)", path.string());
                ctx.error = true;
                return;
            }

            ff.data.PushStream(XFILE_BLOCK_TEMP);
            ff.data.Align(8);
            ComWorld& com{ ff.data.AllocStreamRef<ComWorld>() };
            com = {};
            com.name.name = ctx.HashXHash(assetName, true);
            com.isInUse = true;
            ff.data.PopStream();

            LOG_INFO("Added asset com_map {} (hash_{:x}), no lights", assetName, com.name.name);
        }
    };

    utils::MapAdder<XAssetLinkerImpl, XAssetType, XAssetLinker> impl{ GetWorkers(), XAssetType::ASSET_TYPE_COMWORLD };
} // namespace
