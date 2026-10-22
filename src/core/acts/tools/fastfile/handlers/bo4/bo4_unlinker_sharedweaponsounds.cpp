#include <includes.hpp>
#include <tools/fastfile/handlers/handler_game_bo4.hpp>
#include <utils/data_utils.hpp>

namespace {
    using namespace fastfile::handlers::bo4;

    // the layout isn't mapped: the asset's first bytes as hex (its alias names are XHashes in it)
    constexpr size_t SHARED_WEAPON_SOUNDS_DUMP_SIZE = 0x800;

    class ImplWorker : public Worker {
        using Worker::Worker;

        void Unlink(fastfile::FastFileOption& opt, void* ptr) {
            XHash& name{ *(XHash*)ptr };

            std::filesystem::path outFile{ opt.m_output / "bo4" / "source" / "tables" / "sharedweaponsounds" /
                                           fastfile::GetCurrentContext().ffname /
                                           std::format("{}.json", hashutils::ExtractTmp("file", name)) };
            std::filesystem::create_directories(outFile.parent_path());
            BO4JsonWriter json{};

            LOG_OPT_INFO("Dump sharedweaponsounds {}", outFile.string());

            json.BeginObject();
            json.WriteFieldValueXHash("name", name);
            json.WriteFieldValueString("raw", utils::data::AsHex(ptr, SHARED_WEAPON_SOUNDS_DUMP_SIZE));
            json.EndObject();

            if (!json.WriteToFile(outFile)) {
                LOG_ERROR("Error when dumping {}", outFile.string());
            }
        }
    };

    // superseded by the lossless dump of bo4_unlinker_ai_tables.cpp (the weapon linker reads it back)
    [[maybe_unused]] ImplWorker* unused{};
} // namespace
