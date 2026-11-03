#include <includes.hpp>
#include <tools/fastfile/linkers/linker_bo4.hpp>
#include <tools/fastfile/linkers/bo4/linker_bo4_scriptbundle.hpp>

/*
 * maptableentrylevelassets (type 93, retail 0x68, loader sub_142E22800 / sub_142E22610): a map's minimap, named as the
 * map. cgame's CG_MiniMapChanged finds it by the map's name (sv_mapname); a map without one draws the default image, an
 * empty minimap. Retail (bo4_unlinker_map_level_assets.cpp dumps it):
 *   0x00 XHash name
 *   0x10 u32 the minimap's range (stock 2800 on mp_nuketown_4, 4000 on the bigger maps; 0 left the HUD minimap black,
 *        its player icons gone with it)
 *   0x14 count / 0x18 floats: 3 heights (stock: about the map's floor and up: mp_urban 0 128 192, mp_nuketown_4 -64 16 300)
 *   0x20 compassMapImage (the HUD minimap), 0x28 compassMapImageFull (the scoreboard's map), 0x30 compassMapImageSpawn
 *   0x38 count / 0x40 images (sub_142E1D140): stock one, the minimap's size, linear (image flags 2, the others' 0xC3):
 *        likely a height map of the map
 *   0x48 a scriptbundle's objects array (0x20): the map's script bundle (GSC map::get_script_bundle, the native
 *        function_2717b55f): Blackout's death circles (deathCircleList: { var_47fd5ad2: a death circle bundle's name }),
 *        destinationLabelList, oow heights... MP's stock ~60 keys, empty without "bundle" (as a scriptbundle's json)
 * The dev ELF's MapTableEntryLevelAssets has the name and the 3 images only. Stock minimap images are 3072x1728 (16:9)
 * BC7, their minimap corners 16:9 too.
 *
 * {
 *     "name": "mp_room",                     // the map's name
 *     "range": 2800,
 *     "heights": [ 0, 128, 192 ],
 *     "compassMap": "mp_room_minimap",       // the images by name: one of the zone (linked before) or a loaded one
 *     "compassMapFull": "mp_room_minimap",
 *     "compassMapSpawn": "mp_room_minimap",
 *     "levelImages": [ "mp_room_minimap_height" ],
 *     "bundle": { "deathCircleList": [ { "hash_21bd79c527b6f57f:hash_47fd5ad2": "scriptbundle&wz_room_death_circles" } ] }
 * }
 */
namespace {
    using namespace fastfile::linker::bo4;

    constexpr size_t LEVEL_ASSETS_SIZE = 0x68;
    constexpr size_t LEVEL_ASSETS_RANGE = 0x10;
    constexpr size_t LEVEL_ASSETS_HEIGHT_COUNT = 0x14;
    constexpr size_t LEVEL_ASSETS_HEIGHTS = 0x18;
    constexpr size_t LEVEL_ASSETS_IMAGES = 0x20;
    constexpr size_t LEVEL_ASSETS_LEVEL_IMAGE_COUNT = 0x38;
    constexpr size_t LEVEL_ASSETS_LEVEL_IMAGES = 0x40;
    constexpr size_t LEVEL_ASSETS_BUNDLE = 0x48;
    const char* IMAGE_KEYS[]{ "compassMap", "compassMapFull", "compassMapSpawn" };

    class XAssetLinkerImpl : public XAssetLinker {
      public:
        using XAssetLinker::XAssetLinker;

        void Compute(BO4LinkContext& ctx, const char* id, BO4FFContext& ff) override {
            std::filesystem::path path{ ctx.linkCtx.input / id };
            BO4Json cfg{ path };
            if (!cfg.SyncConfig(false)) {
                LOG_ERROR("Can't read maptableentrylevelassets {}", path.string());
                ctx.error = true;
                return;
            }
            std::string assetName{ cfg.GetString("name", "") };
            if (assetName.empty()) {
                LOG_ERROR("maptableentrylevelassets {}: no name (the map's)", path.string());
                ctx.error = true;
                return;
            }
            std::string images[std::size(IMAGE_KEYS)]{};
            for (size_t i = 0; i < std::size(IMAGE_KEYS); i++) {
                images[i] = cfg.GetString(IMAGE_KEYS[i], "");
            }
            std::vector<float> heights{};
            std::vector<std::string> levelImages{};
            auto heightsIt{ cfg.main.FindMember("heights") };
            if (heightsIt != cfg.main.MemberEnd() && heightsIt->value.IsArray()) {
                for (const rapidjson::Value& v : heightsIt->value.GetArray()) {
                    if (!v.IsNumber()) {
                        LOG_ERROR("maptableentrylevelassets {}: a height isn't a number", path.string());
                        ctx.error = true;
                        return;
                    }
                    heights.push_back((float)v.GetDouble());
                }
            }
            auto levelImagesIt{ cfg.main.FindMember("levelImages") };
            if (levelImagesIt != cfg.main.MemberEnd() && levelImagesIt->value.IsArray()) {
                for (const rapidjson::Value& v : levelImagesIt->value.GetArray()) {
                    if (!v.IsString()) {
                        LOG_ERROR("maptableentrylevelassets {}: a level image isn't a name", path.string());
                        ctx.error = true;
                        return;
                    }
                    levelImages.emplace_back(v.GetString());
                }
            }

            ff.data.PushStream(XFILE_BLOCK_TEMP);
            ff.data.Align(8);
            byte* header{ ff.data.AllocStreamPtr<byte>(LEVEL_ASSETS_SIZE) };
            std::memset(header, 0, LEVEL_ASSETS_SIZE);
            XHash& name{ *(XHash*)header };
            name.name = ctx.HashXHash(assetName, true);
            uint64_t nameHash{ name.name };
            *(uint32_t*)(header + LEVEL_ASSETS_RANGE) = (uint32_t)cfg.GetInteger("range", 0);
            *(uint32_t*)(header + LEVEL_ASSETS_HEIGHT_COUNT) = (uint32_t)heights.size();
            if (!heights.empty()) {
                *(uint64_t*)(header + LEVEL_ASSETS_HEIGHTS) = fastfile::linker::memory::POINTER_NEXT;
            }
            *(uint32_t*)(header + LEVEL_ASSETS_LEVEL_IMAGE_COUNT) = (uint32_t)levelImages.size();
            if (!levelImages.empty()) {
                *(uint64_t*)(header + LEVEL_ASSETS_LEVEL_IMAGES) = fastfile::linker::memory::POINTER_NEXT;
            }

            ff.data.PushStream(XFILE_BLOCK_VIRTUAL);
            // in the loader's order: the heights, the 3 images, the level images' pointers then each image
            if (!heights.empty()) {
                ff.data.Align(4);
                ff.data.WriteStream(heights.data(), heights.size() * sizeof(heights[0]));
            }
            size_t linked{};
            for (size_t i = 0; i < std::size(IMAGE_KEYS); i++) {
                if (images[i].empty()) {
                    continue;
                }
                ctx.LinkAsset(XAssetType::ASSET_TYPE_IMAGE, std::format(",{}", images[i]).data(), *(void**)(header + LEVEL_ASSETS_IMAGES + i * 8), false, &ff);
                linked++;
            }
            if (!levelImages.empty()) {
                ff.data.Align(8);
                void** refs{ ff.data.AllocStreamPtr<void*>(levelImages.size()) };
                for (size_t i = 0; i < levelImages.size(); i++) {
                    refs[i] = nullptr;
                    ctx.LinkAsset(XAssetType::ASSET_TYPE_IMAGE, std::format(",{}", levelImages[i]).data(), refs[i], false, &ff);
                }
            }
            // the bundle last, its field's place
            auto bundleIt{ cfg.main.FindMember("bundle") };
            bool bundle{ bundleIt != cfg.main.MemberEnd() };
            if (bundle && !fastfile::linker::bo4::scriptbundle::WriteArray(ctx, bundleIt->value, *(fastfile::linker::bo4::scriptbundle::SB_ObjectsArray*)(header + LEVEL_ASSETS_BUNDLE), ff)) {
                LOG_ERROR("maptableentrylevelassets {}: can't write its bundle", path.string());
                ctx.error = true;
                return;
            }
            ff.data.PopStream();
            ff.data.PopStream();

            LOG_INFO("Added asset maptableentrylevelassets {} (hash_{:x}), range {}, {} height(s), {} image(s), {} level image(s){}",
                assetName, nameHash, *(uint32_t*)(header + LEVEL_ASSETS_RANGE), heights.size(), linked, levelImages.size(), bundle ? ", a bundle" : "");
        }
    };

    utils::MapAdder<XAssetLinkerImpl, XAssetType, XAssetLinker> impl{ GetWorkers(), XAssetType::ASSET_TYPE_MAPTABLEENTRY_LEVEL_ASSETS };
} // namespace
