#include <includes.hpp>
#include <tools/fastfile/handlers/bo4/bo4_unlinker_map.hpp>
#include <core/hashes/raw_file_extractor.hpp>
#include <tools/utils/data_utils.hpp>

/*
 * navmesh (retail 0x78, the loader sub_142E24430): the AI's Havok navmesh, five Havok packfiles (size, data aligned on
 * 32) and points (0xC each), the header as it is. The packfiles go next to the json as blob<i>.bin, for the navmesh
 * linker. sub_1437C6690 drops when a map has none (error 1149580689, "Papa 341 Brimstone Crocodile") and loads the
 * packfiles at 0x18, 0x48, 0x28, 0x38.
 */
namespace fastfile::handlers::bo4::map {
    using namespace games::bo4::pool;

    struct NavMeshBlob {
        int32_t size;
        byte* data;
    };
    static_assert(sizeof(NavMeshBlob) == 0x10);

    struct NavMeshRetail {
        XHash name;
        NavMeshBlob blobs[5];
        int32_t pointCount;
        byte* points;
        uint64_t unk70;
    };
    static_assert(sizeof(NavMeshRetail) == 0x78);

    class NavMeshWorker : public Worker {
        void Unlink(fastfile::FastFileOption& opt, void* ptr) override {
            NavMeshRetail& asset{ *(NavMeshRetail*)ptr };
            const char* name{ hashutils::ExtractPtr(asset.name) };
            if (!name) {
                name = utils::va("maps/hashed/file_%llx.d3dbsp", asset.name.name);
            }
            std::filesystem::path outDir{ opt.m_output / "bo4" / "source" / name / "navmesh" };
            std::filesystem::create_directories(outDir);

            BO4JsonWriter json{};
            json.BeginObject();
            json.WriteFieldValueXHash("name", asset.name);
            std::string header{};
            for (size_t i = 0; i < sizeof(asset); i++) {
                header += std::format("{:02x}", ((const byte*)&asset)[i]);
            }
            json.WriteFieldValueString("header", header);
            json.WriteFieldNameString("blobs");
            json.BeginArray();
            for (size_t i = 0; i < std::size(asset.blobs); i++) {
                const NavMeshBlob& blob{ asset.blobs[i] };
                if (!blob.data || blob.size <= 0) {
                    json.WriteValueNull();
                    continue;
                }
                std::string file{ std::format("blob{}.bin", i) };
                utils::WriteFile(outDir / file, blob.data, (size_t)blob.size);
                json.WriteValueString(std::format("navmesh/{}", file));
            }
            json.EndArray();
            json.WriteFieldValueNumber("pointCount", asset.pointCount);
            if (asset.points && asset.pointCount > 0) {
                utils::WriteFile(outDir / "points.bin", asset.points, 0xCull * asset.pointCount);
                json.WriteFieldValueString("points", "navmesh/points.bin");
            }
            json.WriteFieldValueNumber("unk70", asset.unk70);
            json.EndObject();

            std::filesystem::path outFile{ outDir.parent_path() / "navmesh.json" };
            LOG_OPT_INFO("Dump navmesh {}", outFile.string());
            if (!json.WriteToFile(outFile)) {
                LOG_ERROR("Error when dumping {}", outFile.string());
            }
        }
    };

    utils::MapAdder<NavMeshWorker, games::bo4::pool::XAssetType, Worker> implnavmesh{
        GetWorkers(), games::bo4::pool::XAssetType::ASSET_TYPE_NAVMESH
    };
} // namespace fastfile::handlers::bo4::map
