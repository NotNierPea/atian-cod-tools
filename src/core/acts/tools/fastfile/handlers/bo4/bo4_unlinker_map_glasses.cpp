#include <includes.hpp>
#include <tools/fastfile/handlers/bo4/bo4_unlinker_map.hpp>
#include <core/hashes/raw_file_extractor.hpp>
#include <tools/utils/data_utils.hpp>

/*
 * glasses (retail 0x58, the loader sub_142E20150): a map's breakable glass, the header, the glass pieces (0x90: the glass
 * def at 0, outline at 0x50 of the byte count at 0x49) and the 0x1C entries at 0x28 as they are, for the glasses linker.
 * After them the counts the glass system sizes its memory with (the elf's workMemorySize, maxGroups, maxShards...).
 */
namespace fastfile::handlers::bo4::map {
    using namespace games::bo4::pool;

    struct GlassesRetail {
        XHash name;
        uint32_t numGlasses;
        byte* glasses;
        uint32_t count20;
        byte* array28;
        uint32_t counts[10];
    };
    static_assert(sizeof(GlassesRetail) == 0x58);

    std::string GlassHex(const void* data, size_t len) {
        static const char* digits{ "0123456789abcdef" };
        std::string out{};
        for (size_t i = 0; i < len; i++) {
            byte b{ ((const byte*)data)[i] };
            out += digits[b >> 4];
            out += digits[b & 0xF];
        }
        return out;
    }

    class GlassesWorker : public Worker {
        void Unlink(fastfile::FastFileOption& opt, void* ptr) override {
            GlassesRetail& asset{ *(GlassesRetail*)ptr };
            BO4JsonWriter json{};
            json.BeginObject();
            json.WriteFieldValueXHash("name", asset.name);
            json.WriteFieldValueString("header", GlassHex(&asset, sizeof(asset)));
            json.WriteFieldValueNumber("numGlasses", asset.numGlasses);
            json.WriteFieldValueNumber("count20", asset.count20);
            json.WriteFieldNameString("counts");
            json.BeginArray();
            for (uint32_t c : asset.counts) {
                json.WriteValueNumber(c);
            }
            json.EndArray();
            if (asset.glasses && asset.numGlasses) {
                json.WriteFieldValueString("glass0", GlassHex(asset.glasses, 0x90));
            }
            if (asset.array28 && asset.count20) {
                json.WriteFieldValueString("array28", GlassHex(asset.array28, 0x1Cull * std::min<uint32_t>(asset.count20, 4)));
            }
            json.EndObject();

            const char* name{ hashutils::ExtractPtr(asset.name) };
            if (!name) {
                name = utils::va("maps/hashed/file_%llx.d3dbsp", asset.name.name);
            }
            std::filesystem::path outFile{ opt.m_output / "bo4" / "source" / name / "glasses.json" };
            std::filesystem::create_directories(outFile.parent_path());
            LOG_OPT_INFO("Dump glasses {}", outFile.string());
            if (!json.WriteToFile(outFile)) {
                LOG_ERROR("Error when dumping {}", outFile.string());
            }
        }
    };

    utils::MapAdder<GlassesWorker, games::bo4::pool::XAssetType, Worker> implglasses{
        GetWorkers(), games::bo4::pool::XAssetType::ASSET_TYPE_GLASSES
    };
} // namespace fastfile::handlers::bo4::map
