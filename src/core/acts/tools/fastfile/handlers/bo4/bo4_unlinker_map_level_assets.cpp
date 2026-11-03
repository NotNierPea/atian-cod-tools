#include <includes.hpp>
#include <tools/fastfile/handlers/bo4/bo4_unlinker_map.hpp>
#include <tools/fastfile/handlers/bo4/bo4_unlinker_scriptbundle.hpp>
#include <core/hashes/raw_file_extractor.hpp>
#include <tools/utils/data_utils.hpp>

/*
 * maptableentrylevelassets (type 93, retail 0x68, loader sub_142E22800 / sub_142E22610): a map's minimap pictures, named
 * as the map (linker_bo4_map_level_assets.cpp). The images at 0x20 / 0x28 / 0x30 (the dev ELF's compassMapImage,
 * compassMapImageFull, compassMapImageSpawn), the u32 list at 0x18 (count 0x14), the image list at 0x40 (count 0x38) and
 * the bundle at 0x48 retail added.
 */
namespace fastfile::handlers::bo4::map {
    using namespace games::bo4::pool;

    struct MapTableEntryLevelAssetsRetail {
        XHash name;
        uint32_t unk10;
        uint32_t count14;
        uint32_t* list18;
        GfxImage* compassMapImage;
        GfxImage* compassMapImageFull;
        GfxImage* compassMapImageSpawn;
        uint32_t count38;
        GfxImage** images40;
        scriptbundle::SB_ObjectsArray bundle;
    };
    static_assert(sizeof(MapTableEntryLevelAssetsRetail) == 0x68);

    class LevelAssetsWorker : public Worker {
        void Unlink(fastfile::FastFileOption& opt, void* ptr) override {
            MapTableEntryLevelAssetsRetail& asset{ *(MapTableEntryLevelAssetsRetail*)ptr };
            BO4JsonWriter json{};
            json.BeginObject();
            json.WriteFieldValueXHash("name", asset.name);
            json.WriteFieldValueNumber("unk10", asset.unk10);
            json.WriteFieldNameString("list18");
            json.BeginArray();
            for (uint32_t i = 0; asset.list18 && i < asset.count14; i++) {
                json.WriteValueNumber(asset.list18[i]);
            }
            json.EndArray();
            json.WriteFieldValueXAsset("compassMap", XAssetType::ASSET_TYPE_IMAGE, asset.compassMapImage);
            json.WriteFieldValueXAsset("compassMapFull", XAssetType::ASSET_TYPE_IMAGE, asset.compassMapImageFull);
            json.WriteFieldValueXAsset("compassMapSpawn", XAssetType::ASSET_TYPE_IMAGE, asset.compassMapImageSpawn);
            json.WriteFieldNameString("images40");
            json.BeginArray();
            for (uint32_t i = 0; asset.images40 && i < asset.count38; i++) {
                json.WriteValueXAsset(XAssetType::ASSET_TYPE_IMAGE, asset.images40[i]);
            }
            json.EndArray();
            scriptbundle::WriteObject(json, "bundle", asset.bundle);
            json.EndObject();

            const char* name{ hashutils::ExtractPtr(asset.name) };
            if (!name) {
                name = utils::va("file_%llx", asset.name.name);
            }
            std::filesystem::path outFile{ opt.m_output / "bo4" / "source" / "tables" / "maptableentrylevelassets" / std::format("{}.json", name) };
            std::filesystem::create_directories(outFile.parent_path());
            LOG_OPT_INFO("Dump maptableentrylevelassets {}", outFile.string());
            if (!json.WriteToFile(outFile)) {
                LOG_ERROR("Error when dumping {}", outFile.string());
            }
        }
    };

    utils::MapAdder<LevelAssetsWorker, games::bo4::pool::XAssetType, Worker> impllevelassets{
        GetWorkers(), games::bo4::pool::XAssetType::ASSET_TYPE_MAPTABLEENTRY_LEVEL_ASSETS
    };
} // namespace fastfile::handlers::bo4::map
