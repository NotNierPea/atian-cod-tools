#include <includes.hpp>
#include <tools/fastfile/handlers/bo4/bo4_unlinker_map.hpp>
#include <core/hashes/raw_file_extractor.hpp>
#include <tools/utils/data_utils.hpp>

/*
 * navvolume (retail 0x50, the loader sub_142E245C0): the map's Havok nav volumes (the dev build's NavVolumeData: small
 * settings, small mediator, big settings, big mediator), four packfiles (size, data aligned on 32). They go next to the
 * json as blob<i>.bin, for the navvolume linker.
 */
namespace fastfile::handlers::bo4::map {
    using namespace games::bo4::pool;

    struct NavVolumeBlob {
        int32_t size;
        byte* data;
    };
    static_assert(sizeof(NavVolumeBlob) == 0x10);

    struct NavVolumeRetail {
        XHash name;
        NavVolumeBlob blobs[4];
    };
    static_assert(sizeof(NavVolumeRetail) == 0x50);

    class NavVolumeWorker : public Worker {
        void Unlink(fastfile::FastFileOption& opt, void* ptr) override {
            NavVolumeRetail& asset{ *(NavVolumeRetail*)ptr };
            const char* name{ hashutils::ExtractPtr(asset.name) };
            if (!name) {
                name = utils::va("maps/hashed/file_%llx.d3dbsp", asset.name.name);
            }
            std::filesystem::path outDir{ opt.m_output / "bo4" / "source" / name / "navvolume" };
            std::filesystem::create_directories(outDir);

            BO4JsonWriter json{};
            json.BeginObject();
            json.WriteFieldValueXHash("name", asset.name);
            json.WriteFieldNameString("blobs");
            json.BeginArray();
            for (size_t i = 0; i < std::size(asset.blobs); i++) {
                const NavVolumeBlob& blob{ asset.blobs[i] };
                if (!blob.data || blob.size <= 0) {
                    json.WriteValueNull();
                    continue;
                }
                std::string file{ std::format("blob{}.bin", i) };
                utils::WriteFile(outDir / file, blob.data, (size_t)blob.size);
                json.WriteValueString(std::format("navvolume/{}", file));
            }
            json.EndArray();
            json.EndObject();

            std::filesystem::path outFile{ outDir.parent_path() / "navvolume.json" };
            LOG_OPT_INFO("Dump navvolume {}", outFile.string());
            if (!json.WriteToFile(outFile)) {
                LOG_ERROR("Error when dumping {}", outFile.string());
            }
        }
    };

    utils::MapAdder<NavVolumeWorker, games::bo4::pool::XAssetType, Worker> implnavvolume{
        GetWorkers(), games::bo4::pool::XAssetType::ASSET_TYPE_NAVVOLUME
    };
} // namespace fastfile::handlers::bo4::map
