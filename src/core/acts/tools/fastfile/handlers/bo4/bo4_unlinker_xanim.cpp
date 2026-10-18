#include <includes.hpp>
#include <tools/fastfile/handlers/handler_game_bo4.hpp>

/*
 * XAnimParts (retail 0x130, the loader sub_142E35510): the header and its streams as they are, for the xanim linker.
 * Offsets of the counts from the loader: bone names 0x10E, notifies 0x110 (u16) / 0x125 / 0x126 (u8), frames 0x114
 * (u16, indices are u16 from 256), data byte 0xE4, int 0xE8, short 0xEC, random byte 0xF4, int 0xF8, short 0xDC,
 * indices 0xD0, extra channel 0xF0.
 */
namespace {
    using namespace fastfile::handlers::bo4;

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

    template<typename T>
    T At(const byte* p, size_t off) {
        return *(const T*)(p + off);
    }

    class ImplWorker : public Worker {
        using Worker::Worker;

        void Unlink(fastfile::FastFileOption& opt, void* ptr) override {
            byte* a{ (byte*)ptr };
            XHash* name{ (XHash*)(a + 0x70) };

            std::filesystem::path outFile{ opt.m_output / "bo4" / "graphics" / "xanim" /
                                           std::format("{}.json", hashutils::ExtractTmp("file", name->name)) };
            std::filesystem::create_directories(outFile.parent_path());
            BO4JsonWriter json{};
            LOG_OPT_INFO("Dump xanim {}", outFile.string());

            auto Blob = [&json](const char* key, const void* data, size_t len) {
                if (data && len) {
                    json.WriteFieldValueString(key, Hex(data, len));
                }
            };

            uint16_t numBones{ At<uint16_t>(a, 0x10E) };
            uint16_t numframes{ At<uint16_t>(a, 0x114) };
            json.BeginObject();
            json.WriteFieldValueXHash("name", *name);
            json.WriteFieldValueString("header", Hex(a, 0x130));
            json.WriteFieldValueNumber("numBones", numBones);
            json.WriteFieldValueNumber("numframes", numframes);
            json.WriteFieldValueString("branchName", GetScrString(At<ScrString_t>(a, 0xA8)));
            json.WriteFieldValueNumber("framerate", At<float>(a, 0xC0));
            json.WriteFieldValueNumber("frequency", At<float>(a, 0xC4));

            ScrString_t* names{ At<ScrString_t*>(a, 0x08) };
            json.WriteFieldNameString("names");
            json.BeginArray();
            for (size_t i = 0; names && i < numBones; i++) {
                json.WriteValueString(GetScrString(names[i]));
            }
            json.EndArray();

            Blob("dataByte", At<void*>(a, 0x58), At<uint32_t>(a, 0xE4));
            Blob("dataInt", At<void*>(a, 0x80), 4ull * At<uint32_t>(a, 0xE8));
            Blob("dataShort", At<void*>(a, 0x90), 2ull * At<uint32_t>(a, 0xEC));
            Blob("randomDataByte", At<void*>(a, 0x68), At<uint32_t>(a, 0xF4));
            Blob("randomDataInt", At<void*>(a, 0x88), 4ull * At<uint32_t>(a, 0xF8));
            Blob("randomDataShort", At<void*>(a, 0x98), 2ull * At<uint32_t>(a, 0xDC));
            Blob("extraChannelData", At<void*>(a, 0x60), At<uint32_t>(a, 0xF0));
            Blob("indices", At<void*>(a, 0xA0), (numframes >= 0x100 ? 2ull : 1ull) * At<uint32_t>(a, 0xD0));

            // notifies: 0x28 each, the names as strings
            auto Notifies = [&](const char* key, byte* list, size_t count) {
                json.WriteFieldNameString(key);
                json.BeginArray();
                for (size_t i = 0; list && i < count; i++) {
                    byte* n{ list + 0x28 * i };
                    json.BeginObject();
                    json.WriteFieldValueString("raw", Hex(n, 0x28));
                    json.WriteFieldValueString("type", GetScrString(At<ScrString_t>(n, 0x10)));
                    json.WriteFieldValueString("param1", GetScrString(At<ScrString_t>(n, 0x14)));
                    json.WriteFieldValueString("param2", GetScrString(At<ScrString_t>(n, 0x18)));
                    json.WriteFieldValueString("param3", GetScrString(At<ScrString_t>(n, 0x1C)));
                    json.WriteFieldValueNumber("time", At<float>(n, 0x20));
                    json.EndObject();
                }
                json.EndArray();
            };
            Notifies("notify", At<byte*>(a, 0x38), At<uint16_t>(a, 0x110));
            Notifies("startupNotify", At<byte*>(a, 0x40), At<byte>(a, 0x125));
            Notifies("shutdownNotify", At<byte*>(a, 0x48), At<byte>(a, 0x126));

            /*
             * The delta part (root motion, the loader sub_142E35180): trans, quat2 (yaw), quat. Each a header (u16 key
             * count - 1, trans: byte smallTrans), then one key (trans float3, quat2 4 bytes, quat 8 bytes) or the keys:
             * trans {mins, size, frames}, the others {frames}, the frame indices inline (u8, u16 from 256 frames), the
             * keys at frames (trans 3 or 6 bytes, quat2 4, quat 8).
             */
            byte* delta{ At<byte*>(a, 0x18) };
            if (delta) {
                json.WriteFieldNameString("deltaPart");
                json.BeginObject();
                size_t indexSize{ numframes >= 0x100 ? 2ull : 1ull };
                const char* keys[]{ "trans", "quat2", "quat" };
                for (size_t i = 0; i < 3; i++) {
                    byte* p{ At<byte*>(delta, 8 * i) };
                    if (!p) {
                        continue;
                    }
                    uint16_t size{ At<uint16_t>(p, 0) };
                    json.WriteFieldNameString(keys[i]);
                    json.BeginObject();
                    json.WriteFieldValueNumber("size", size);
                    json.WriteFieldValueString("header", Hex(p, 8));
                    if (!size) {
                        json.WriteFieldValueString("frame0", Hex(p + 8, i == 0 ? 12 : i == 1 ? 4 : 8));
                    } else {
                        size_t keyCount{ (size_t)size + 1 };
                        size_t head{ i == 0 ? 0x20ull : 8ull };
                        byte* frames{ At<byte*>(p, 8 + head - 8) };
                        if (i == 0) {
                            json.WriteFieldValueString("minsSize", Hex(p + 8, 0x18));
                        }
                        json.WriteFieldValueString("indices", Hex(p + 8 + head, keyCount * indexSize));
                        size_t keySize{ i == 0 ? (p[2] ? 3ull : 6ull) : i == 1 ? 4ull : 8ull };
                        if (frames) {
                            json.WriteFieldValueString("frames", Hex(frames, keyCount * keySize));
                        }
                    }
                    json.EndObject();
                }
                json.EndObject();
            }
            json.WriteFieldValueBool("streamInfo", At<void*>(a, 0xB0) != nullptr);
            json.EndObject();

            if (!json.WriteToFile(outFile)) {
                LOG_ERROR("Error when dumping {}", outFile.string());
            }
        }
    };

    utils::MapAdder<ImplWorker, games::bo4::pool::XAssetType, Worker> impl{
        GetWorkers(), games::bo4::pool::XAssetType::ASSET_TYPE_XANIM, true
    };
} // namespace
