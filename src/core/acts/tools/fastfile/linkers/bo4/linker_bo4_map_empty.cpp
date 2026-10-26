#include <includes.hpp>
#include <tools/fastfile/linkers/linker_bo4.hpp>

/*
 * The world assets a map can have empty, named as its col_map, their header only (the name, all else 0, no list), as
 * the game's frontend and zm_towers have some of them:
 *   glasses (0x58, sub_142E20150): breakable glass pieces, then the glass system's memory sizes
 *   cpu_occlusion_data (0x60, sub_142E16EB0, the layout in bo4_unlinker_cpu_cclusion_data.cpp): occluder volumes; the
 *     runtime arrays at 0x50/0x58 are skipped while null
 *   streamerworld (0x100, sub_142E2E760): the streamer's lists, all (count, pointer); gfx_map loads it inline at 0x1800
 *     (linker_bo4_map_gfx.cpp), nothing looks it up by name
 *   terraingfx (0xB8, sub_142E2C630): the terrain's drawing (lists, images); a missing one drops the level start
 *
 * {
 *     "name": "maps/zm/enh_test.d3dbsp"   // maps/<mode>/<map>.d3dbsp
 * }
 */
namespace {
    using namespace fastfile::linker::bo4;

    template<XAssetType Type, size_t Size>
    class EmptyWorldAssetLinker : public XAssetLinker {
      public:
        using XAssetLinker::XAssetLinker;

        void Compute(BO4LinkContext& ctx, const char* id, BO4FFContext& ff) override {
            const char* typeName{ XAssetNameFromId(Type) };
            std::filesystem::path path{ ctx.linkCtx.input / id };
            BO4Json cfg{ path };
            if (!cfg.SyncConfig(false)) {
                LOG_ERROR("Can't read {} {}", typeName, path.string());
                ctx.error = true;
                return;
            }
            std::string assetName{ cfg.GetString("name", "") };
            if (assetName.empty()) {
                LOG_ERROR("{} {}: no name (maps/<mode>/<map>.d3dbsp)", typeName, path.string());
                ctx.error = true;
                return;
            }

            ff.data.PushStream(XFILE_BLOCK_TEMP);
            ff.data.Align(8);
            byte* header{ ff.data.AllocStreamPtr<byte>(Size) };
            std::memset(header, 0, Size);
            XHash& name{ *(XHash*)header };
            name.name = ctx.HashXHash(assetName, true);
            ff.data.PopStream();

            LOG_INFO("Added asset {} {} (hash_{:x}), empty", typeName, assetName, name.name);
        }
    };

    utils::MapAdder<EmptyWorldAssetLinker<XAssetType::ASSET_TYPE_GLASSES, 0x58>, XAssetType, XAssetLinker> glasses{
        GetWorkers(), XAssetType::ASSET_TYPE_GLASSES
    };
    utils::MapAdder<EmptyWorldAssetLinker<XAssetType::ASSET_TYPE_CPU_OCCLUSION_DATA, 0x60>, XAssetType, XAssetLinker> cpuOcclusionData{
        GetWorkers(), XAssetType::ASSET_TYPE_CPU_OCCLUSION_DATA
    };
    utils::MapAdder<EmptyWorldAssetLinker<XAssetType::ASSET_TYPE_STREAMERWORLD, 0x100>, XAssetType, XAssetLinker> streamerWorld{
        GetWorkers(), XAssetType::ASSET_TYPE_STREAMERWORLD
    };
    utils::MapAdder<EmptyWorldAssetLinker<XAssetType::ASSET_TYPE_TERRAINGFX, 0xB8>, XAssetType, XAssetLinker> terrainGfx{
        GetWorkers(), XAssetType::ASSET_TYPE_TERRAINGFX
    };
} // namespace
