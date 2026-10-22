#include <includes.hpp>
#include <tools/fastfile/handlers/handler_game_bo4.hpp>

/*
 * Technique sets (MaterialTechniqueSet 0xA8, its loader sub_142E24040), the techniques' names and, lossless for
 * linker_bo4_techset.cpp, every block in the file's state (acts stubs the shader post-loaders and the code constant
 * remap sub_1435FC320, bo4.json nullscans):
 *   technique 0x100: name, 4 draw methods (0x38) at 0x10, 8 * u8@0xF6 at 0xF8
 *   draw method: 0xC * (u8@1 + u8@2) args at 0x18, 8 * u8@0x12 at 0x20, a shader state (0x40) at 0x28, 0x18 (+0x2000
 *     at its 0x10) at 0x30
 *   shader state: a 0x78 block at 0, a 0xB0 block at 8, six programs (0x28, their blob at 0x18, its size u32@0x20) at
 *     0x10..0x38
 * The 0x78 and 0xB0 blocks and the programs have their address as "id": the file shares them by offset, and the game makes
 * its objects once for each (the 0xB0's input layout from its state's vertex shader), so the linker shares the same ones.
 */
namespace {
    using namespace fastfile::handlers::bo4;

    constexpr size_t TECHSET_SIZE = 0xA8;
    constexpr size_t TECHNIQUE_SIZE = 0x100;
    constexpr size_t DRAW_METHOD_SIZE = 0x38;
    constexpr size_t STATE_SIZE = 0x40;
    constexpr size_t PROGRAM_SIZE = 0x28;

    std::string Hex(const void* data, size_t len) {
        static const char* digits{ "0123456789abcdef" };
        std::string out{};
        out.reserve(len * 2);
        for (size_t i = 0; i < len; i++) {
            byte b{ ((const byte*)data)[i] };
            out += digits[b >> 4];
            out += digits[b & 0xF];
        }
        return out;
    }

    // a block of a pointer, null when there's none
    void WriteBlock(BO4JsonWriter& json, const char* key, const void* ptr, size_t len) {
        json.WriteFieldNameString(key);
        if (ptr) {
            json.WriteValueString(Hex(ptr, len));
        } else {
            json.WriteValueNull();
        }
    }

    // a shared block's address, its identity in the dump
    void WriteId(BO4JsonWriter& json, const char* key, const void* ptr) {
        if (ptr) {
            json.WriteFieldValueString(key, std::format("{:x}", (uintptr_t)ptr));
        }
    }

    void WriteState(BO4JsonWriter& json, const byte* s) {
        json.BeginObject();
        json.WriteFieldValueString("header", Hex(s, STATE_SIZE));
        WriteBlock(json, "decl", *(void**)s, 0x78);
        WriteId(json, "declId", *(void**)s);
        WriteBlock(json, "b0", *(void**)(s + 8), 0xB0);
        WriteId(json, "b0Id", *(void**)(s + 8));
        json.WriteFieldNameString("programs");
        json.BeginArray();
        for (size_t k = 0; k < 8; k++) {
            const byte* p{ k >= 2 ? *(const byte**)(s + 8 * k) : nullptr };
            if (!p) {
                json.WriteValueNull();
                continue;
            }
            json.BeginObject();
            json.WriteFieldValueString("header", Hex(p, PROGRAM_SIZE));
            WriteId(json, "id", p);
            WriteBlock(json, "blob", *(void**)(p + 0x18), *(uint32_t*)(p + 0x20));
            json.EndObject();
        }
        json.EndArray();
        json.EndObject();
    }

    void WriteTechnique(BO4JsonWriter& json, const byte* t) {
        json.BeginObject();
        json.WriteFieldValueString("header", Hex(t, TECHNIQUE_SIZE));
        WriteBlock(json, "f8", *(void**)(t + 0xF8), 8ull * t[0xF6]);
        json.WriteFieldNameString("drawMethods");
        json.BeginArray();
        for (size_t d = 0; d < 4; d++) {
            const byte* dm{ t + 0x10 + DRAW_METHOD_SIZE * d };
            json.BeginObject();
            WriteBlock(json, "args", *(void**)(dm + 0x18), 0xCull * ((size_t)dm[1] + dm[2]));
            WriteBlock(json, "a20", *(void**)(dm + 0x20), 8ull * dm[0x12]);
            json.WriteFieldNameString("state");
            if (const byte* s{ *(const byte**)(dm + 0x28) }) {
                WriteState(json, s);
            } else {
                json.WriteValueNull();
            }
            json.WriteFieldNameString("s30");
            if (const byte* s30{ *(const byte**)(dm + 0x30) }) {
                json.BeginObject();
                json.WriteFieldValueString("header", Hex(s30, 0x18));
                WriteBlock(json, "data", *(void**)(s30 + 0x10), 0x2000);
                json.EndObject();
            } else {
                json.WriteValueNull();
            }
            json.EndObject();
        }
        json.EndArray();
        json.EndObject();
    }

    class ImplWorker : public Worker {
        using Worker::Worker;

        void Unlink(fastfile::FastFileOption& opt, void* ptr) {
            struct MaterialDrawMethod {
                uint64_t unk0;
                uint64_t unk8;
                uint64_t unk10;
                uint64_t* unk18;
                uint64_t* unk20;
                uint64_t unk28;
                uint64_t unk30;
            };
            static_assert(sizeof(MaterialDrawMethod) == 0x38);

            struct MaterialTechnique {
                XHash name;
                MaterialDrawMethod drawMethods[4];
                uint32_t unkf0;
                uint16_t unkf4;
                byte unkf8_count;
                uint64_t* unkf8;
            };
            static_assert(sizeof(MaterialTechnique) == 0x100);

            struct MaterialTechniqueSet {
                XHash name;
                XHash unk10;
                uint64_t unk18;
                MaterialTechnique* techniques[16];
            };
            static_assert(sizeof(MaterialTechniqueSet) == 0xa8);

            MaterialTechniqueSet* asset{ (MaterialTechniqueSet*)ptr };

            std::filesystem::path outFile{ opt.m_output / "bo4" / "graphics" / "techset" /
                                           std::format("{}.json", hashutils::ExtractTmp("file", asset->name)) };
            std::filesystem::create_directories(outFile.parent_path());
            BO4JsonWriter json{};

            LOG_OPT_INFO("Dump techset {}", outFile.string());

            json.BeginObject();
            json.WriteFieldValueXHash("name", asset->name);
            json.WriteFieldValueXHash("unk10", asset->unk10);
            json.WriteFieldValueHash("unk18", asset->unk18);
            json.WriteFieldNameString("techniques");
            json.BeginArray();
            for (size_t i = 0; i < ACTS_ARRAYSIZE(asset->techniques); i++) {
                MaterialTechnique* tech = asset->techniques[i];
                if (!tech) {
                    json.WriteValueNull();
                    continue;
                }
                json.BeginObject();
                json.WriteFieldValueHash("name", tech->name);
                json.WriteFieldValueNumber("unkf0", tech->unkf0);
                json.WriteFieldValueNumber("unkf4", tech->unkf4);
                json.WriteFieldNameString("unkf8");
                json.BeginArray();
                for (size_t j = 0; j < tech->unkf8_count; j++) {
                    json.WriteValueHash(tech->unkf8[j]);
                }
                json.EndArray();
                json.EndObject();
            }
            json.EndArray();

            // the lossless data, the techniques' blocks
            json.WriteFieldValueBool("lossless", true);
            json.WriteFieldValueString("header", Hex(asset, TECHSET_SIZE));
            json.WriteFieldNameString("data");
            json.BeginArray();
            for (size_t i = 0; i < ACTS_ARRAYSIZE(asset->techniques); i++) {
                if (asset->techniques[i]) {
                    WriteTechnique(json, (const byte*)asset->techniques[i]);
                } else {
                    json.WriteValueNull();
                }
            }
            json.EndArray();
            json.EndObject();

            if (!json.WriteToFile(outFile)) {
                LOG_ERROR("Error when dumping {}", outFile.string());
            }
        }
    };

    utils::MapAdder<ImplWorker, games::bo4::pool::XAssetType, Worker> impl{
        GetWorkers(), games::bo4::pool::XAssetType::ASSET_TYPE_TECHNIQUE_SET, true
    };
} // namespace
