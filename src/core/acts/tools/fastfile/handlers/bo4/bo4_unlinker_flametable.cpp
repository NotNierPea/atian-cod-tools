#include <includes.hpp>
#include <tools/fastfile/handlers/handler_game_bo4.hpp>

/*
 * Flame tables (a flamethrower's flameTableFirstPerson/ThirdPerson). Retail 0x240 (the loader sub_142E19730): the
 * values, the name at 0x1B0, 8 materials at 0x1C0, 4 sounds (XHash) at 0x200. The header goes as hex, the materials by
 * name, the linker writes them back.
 */
namespace {
    using namespace fastfile::handlers::bo4;

    constexpr size_t FLAMETABLE_SIZE = 0x240;
    constexpr size_t FLAMETABLE_NAME = 0x1B0;
    constexpr size_t FLAMETABLE_MATERIALS = 0x1C0;
    constexpr size_t FLAMETABLE_SOUNDS = 0x200;
    // dev names (cod_Release.elf)
    const char* MATERIAL_NAMES[]{ "fire", "smoke", "heat", "drips", "streamFuel", "streamFuel2", "streamFlame", "streamFlame2" };
    const char* SOUND_NAMES[]{ "flameOffLoopSound", "flameIgniteSound", "flameOnLoopSound", "flameCooldownSound" };

    std::string Hex(const void* data, size_t len) {
        static const char* digits{ "0123456789abcdef" };
        std::string out{};
        out.reserve(len * 2);
        for (size_t i = 0; i < len; i++) {
            out += digits[((const byte*)data)[i] >> 4];
            out += digits[((const byte*)data)[i] & 0xF];
        }
        return out;
    }

    class ImplWorker : public Worker {
        using Worker::Worker;

        void Unlink(fastfile::FastFileOption& opt, void* ptr) {
            byte* asset{ (byte*)ptr };
            XHash& name{ *(XHash*)(asset + FLAMETABLE_NAME) };

            std::filesystem::path outFile{ opt.m_output / "bo4" / "source" / "tables" / "flametable" /
                                           fastfile::GetCurrentContext().ffname /
                                           std::format("{}.json", hashutils::ExtractTmp("file", name)) };
            std::filesystem::create_directories(outFile.parent_path());
            BO4JsonWriter json{};

            LOG_OPT_INFO("Dump flametable {}", outFile.string());

            // the header without its pointers
            std::vector<byte> header(asset, asset + FLAMETABLE_SIZE);
            std::memset(header.data() + FLAMETABLE_MATERIALS, 0, 8 * 8);

            json.BeginObject();
            json.WriteFieldValueXHash("name", name);
            json.WriteFieldValueString("header", Hex(header.data(), header.size()));
            for (size_t i = 0; i < 8; i++) {
                void* m{ ((void**)(asset + FLAMETABLE_MATERIALS))[i] };
                if (m && !*games::bo4::pool::GetAssetName(games::bo4::pool::ASSET_TYPE_MATERIAL, m)) {
                    LOG_WARNING("flametable {}: the {} material has no name", hashutils::ExtractTmp("hash", name), MATERIAL_NAMES[i]);
                }
                json.WriteFieldValueXAsset(MATERIAL_NAMES[i], games::bo4::pool::ASSET_TYPE_MATERIAL, m);
            }
            for (size_t i = 0; i < 4; i++) {
                json.WriteFieldValueXHash(SOUND_NAMES[i], ((XHash*)(asset + FLAMETABLE_SOUNDS))[i]);
            }
            json.EndObject();

            if (!json.WriteToFile(outFile)) {
                LOG_ERROR("Error when dumping {}", outFile.string());
            }
        }
    };

    utils::MapAdder<ImplWorker, games::bo4::pool::XAssetType, Worker> impl{
        GetWorkers(), games::bo4::pool::XAssetType::ASSET_TYPE_FLAMETABLE
    };
} // namespace
