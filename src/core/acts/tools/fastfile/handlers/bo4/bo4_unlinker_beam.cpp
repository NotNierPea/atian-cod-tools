#include <includes.hpp>
#include <tools/fastfile/handlers/handler_game_bo4.hpp>

/*
 * Beams (BeamDef, asset type 99): the tubes and arcs effects draw between two points (an fx element of type 14 / 15
 * names one: zm_red's wormhole fx8_teleport_instant_1p draws its tunnel with beam8_zm_teleport_path). The dev build's
 * cod_Release.elf BeamDef (688 bytes) is an older layout; the retail one, 0x4D8 bytes, read off its loader
 * (sub_142E11E70 in the retail dump: the header, then the parts in this order):
 *
 * 0x000 name (XHash); 0x010, 0x018 materials (the beam's, the perpendicular card's); curves (a pointer, its key count a
 * u32 at +8, 8-byte keys: (time, value), 4 aligned) at BEAM_CURVES' offsets; 0x3F8..0x420 six effects; 0x438, 0x448
 * XHashes (no stream data); the rest plain values.
 *
 * The json is the linker's (linker_bo4_beam.cpp): the plain bytes as hex (its pointers too, the linker sets them), the
 * materials and effects by name with their offsets, the curves' keys as hex by offset.
 */
namespace {
    using namespace fastfile::handlers::bo4;

    constexpr size_t BEAM_SIZE{ 0x4D8 };
    constexpr size_t BEAM_MATERIALS[]{ 0x10, 0x18 };
    constexpr size_t BEAM_CURVES[]{ 0x68, 0x80, 0xC0, 0xD8, 0x120, 0x138, 0x168, 0x180, 0x198, 0x1B0, 0x200, 0x210,
                                    0x470, 0x488, 0x4A0, 0x4B8 };
    constexpr size_t BEAM_FX[]{ 0x3F8, 0x400, 0x408, 0x410, 0x418, 0x420 };

    std::string Hex(const void* ptr, size_t len) {
        std::string out{};
        for (size_t i = 0; i < len; i++) {
            out += std::format("{:02x}", ((const byte*)ptr)[i]);
        }
        return out;
    }

    class ImplWorker : public Worker {
        using Worker::Worker;

        void Unlink(fastfile::FastFileOption& opt, void* ptr) {
            byte* asset{ (byte*)ptr };
            XHash* name{ (XHash*)asset };
            std::filesystem::path outFile{ opt.m_output / "bo4" / "source" / "tables" / "beam" /
                                           fastfile::GetCurrentContext().ffname /
                                           std::format("{}.json", hashutils::ExtractTmp("file", name->name)) };
            std::filesystem::create_directories(outFile.parent_path());
            BO4JsonWriter json{};

            json.BeginObject();
            json.WriteFieldValueXHash("name", *name);
            json.WriteFieldValueString("raw", Hex(asset, BEAM_SIZE));
            json.WriteFieldNameString("materials");
            json.BeginObject();
            for (size_t off : BEAM_MATERIALS) {
                void* m{ *(void**)(asset + off) };
                if (m) {
                    json.WriteFieldValueXAsset(std::format("{:x}", off).c_str(), games::bo4::pool::ASSET_TYPE_MATERIAL, m);
                }
            }
            json.EndObject();
            json.WriteFieldNameString("fx");
            json.BeginObject();
            for (size_t off : BEAM_FX) {
                void* f{ *(void**)(asset + off) };
                if (f) {
                    json.WriteFieldValueXAsset(std::format("{:x}", off).c_str(), games::bo4::pool::ASSET_TYPE_FX, f);
                }
            }
            json.EndObject();
            json.WriteFieldNameString("curves");
            json.BeginObject();
            size_t numCurves{};
            for (size_t off : BEAM_CURVES) {
                byte* keys{ *(byte**)(asset + off) };
                uint32_t count{ *(uint32_t*)(asset + off + 8) };
                if (keys && count) {
                    std::string key{ std::format("{:x}", off) };
                    json.WriteFieldValueString(key.c_str(), Hex(keys, 8 * (size_t)count));
                    numCurves++;
                }
            }
            json.EndObject();
            json.EndObject();

            LOG_OPT_INFO("Dump beam {} ({} curves)", outFile.string(), numCurves);
            if (!json.WriteToFile(outFile)) {
                LOG_ERROR("Error when dumping {}", outFile.string());
            }
        }
    };

    utils::MapAdder<ImplWorker, games::bo4::pool::XAssetType, Worker> impl{
        GetWorkers(), games::bo4::pool::XAssetType::ASSET_TYPE_BEAM
    };
} // namespace
