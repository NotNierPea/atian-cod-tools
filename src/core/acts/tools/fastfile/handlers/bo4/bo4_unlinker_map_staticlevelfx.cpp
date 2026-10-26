#include <includes.hpp>
#include <tools/fastfile/handlers/bo4/bo4_unlinker_map.hpp>
#include <core/hashes/raw_file_extractor.hpp>
#include <tools/utils/data_utils.hpp>

/*
 * staticlevelfxlist (retail 0x30, the loader sub_142E2CF70): a map's placed effects. The header: the entries (0x50,
 * sub_142E2CD70) at 0x18 of the count at 0x10, strings at 0x28 of the count at 0x20. An entry: its effect at 0 (an fx
 * asset, inline or another zone's by name), the plain part, a string at 0x40, a list of 0x28 records (a string at 0)
 * at 0x48 of the count at 0x38. Dumped with the plain parts as hex for the staticlevelfxlist linker.
 */
namespace fastfile::handlers::bo4::map {
    using namespace games::bo4::pool;

    struct StaticLevelFxRecord {
        const char* str;
        byte data[0x20];
    };
    static_assert(sizeof(StaticLevelFxRecord) == 0x28);

    struct StaticLevelFx {
        XHash* fx;
        byte data[0x30];
        uint32_t numRecords;
        uint32_t unk3c;
        const char* str;
        StaticLevelFxRecord* records;
    };
    static_assert(sizeof(StaticLevelFx) == 0x50);

    struct StaticLevelFxList {
        XHash name;
        uint32_t numFx;
        StaticLevelFx* fx;
        uint32_t numStrings;
        const char** strings;
    };
    static_assert(sizeof(StaticLevelFxList) == 0x30);

    std::string SlfHex(const void* data, size_t len) {
        static const char* digits{ "0123456789abcdef" };
        std::string out{};
        for (size_t i = 0; i < len; i++) {
            byte b{ ((const byte*)data)[i] };
            out += digits[b >> 4];
            out += digits[b & 0xF];
        }
        return out;
    }

    class StaticLevelFxListWorker : public Worker {
        void Unlink(fastfile::FastFileOption& opt, void* ptr) override {
            StaticLevelFxList& asset{ *(StaticLevelFxList*)ptr };
            BO4JsonWriter json{};
            json.BeginObject();
            json.WriteFieldValueXHash("name", asset.name);
            json.WriteFieldValueString("header", SlfHex(&asset, sizeof(asset)));
            json.WriteFieldNameString("strings");
            json.BeginArray();
            for (size_t i = 0; i < asset.numStrings; i++) {
                json.WriteValueString(asset.strings && asset.strings[i] ? asset.strings[i] : "");
            }
            json.EndArray();
            json.WriteFieldNameString("fx");
            json.BeginArray();
            for (size_t i = 0; i < asset.numFx; i++) {
                StaticLevelFx& fx{ asset.fx[i] };
                json.BeginObject();
                if (fx.fx) {
                    json.WriteFieldValueXHash("fx", *fx.fx);
                }
                json.WriteFieldValueString("data", SlfHex(fx.data, sizeof(fx.data)));
                json.WriteFieldValueNumber("unk3c", fx.unk3c);
                if (fx.str) {
                    json.WriteFieldValueString("str", fx.str);
                }
                if (fx.records && fx.numRecords) {
                    json.WriteFieldNameString("records");
                    json.BeginArray();
                    for (size_t j = 0; j < fx.numRecords; j++) {
                        json.BeginObject();
                        if (fx.records[j].str) {
                            json.WriteFieldValueString("str", fx.records[j].str);
                        }
                        json.WriteFieldValueString("data", SlfHex(fx.records[j].data, sizeof(fx.records[j].data)));
                        json.EndObject();
                    }
                    json.EndArray();
                }
                json.EndObject();
            }
            json.EndArray();
            json.EndObject();

            const char* name{ hashutils::ExtractPtr(asset.name) };
            if (!name) {
                name = utils::va("maps/hashed/file_%llx.d3dbsp", asset.name.name);
            }
            std::filesystem::path outFile{ opt.m_output / "bo4" / "source" / name / "staticlevelfxlist.json" };
            std::filesystem::create_directories(outFile.parent_path());
            LOG_OPT_INFO("Dump staticlevelfxlist {}", outFile.string());
            if (!json.WriteToFile(outFile)) {
                LOG_ERROR("Error when dumping {}", outFile.string());
            }
        }
    };

    utils::MapAdder<StaticLevelFxListWorker, games::bo4::pool::XAssetType, Worker> implstaticlevelfxlist{
        GetWorkers(), games::bo4::pool::XAssetType::ASSET_TYPE_STATIC_LEVEL_FX_LIST
    };
} // namespace fastfile::handlers::bo4::map
