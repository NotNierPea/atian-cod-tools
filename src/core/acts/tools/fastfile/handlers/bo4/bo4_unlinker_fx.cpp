#include <includes.hpp>
#include <tools/fastfile/handlers/handler_game_bo4.hpp>

/*
 * FX (FxEffectDef 0xA0, its loader sub_142E1A360), lossless for linker_bo4_fx.cpp: the header (its elements at 0x30, the
 * looping, oneshot and emission counts at 0x24, 0x26, 0x28), each element (FxElemDef 0x280, sub_142E1A5E0): two visual
 * blocks at 0x00 and 0x40 by the element's type (0x264) and visual count (0x26E), the type's block at 0x80 (a trail 0x30
 * and its two arrays, or one byte), five sample arrays at 0x88..0xA8 (sizes from the counts at 0x26F..0x273). Materials
 * and models by name; the nested effects (header 0x90, element 0xC8..0xF8, type 0xE visuals: XHashes the loader swaps for
 * the effects, sub_142EBB5C0) are written back as their hashes.
 */
namespace {
    using namespace fastfile::handlers::bo4;

    constexpr size_t FX_SIZE = 0xA0;
    constexpr size_t ELEM_SIZE = 0x280;
    constexpr size_t VISUAL_SIZE = 0x40;

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

    // what a visual of an element's type holds (sub_142E1AAF0, sub_142E1A820): an asset pointer or data
    enum VisualKind { VK_DATA, VK_MATERIAL, VK_MODEL, VK_LIGHTDEF };

    VisualKind KindOf(byte type) {
        switch (type) {
        case 7: return VK_MODEL;
        case 8: return VK_LIGHTDEF;
        case 9: case 0xA: case 0xB: case 0xE: case 0xF: case 0x10: return VK_DATA;
        default: return VK_MATERIAL;
        }
    }

    // an asset's name, the first 8 bytes of its header, the top bit kept (a reference by name to another zone's asset)
    void WriteRef(BO4JsonWriter& json, const void* asset) {
        if (!asset) {
            json.WriteValueNull();
            return;
        }
        json.WriteValueString(std::format("#hash_{:x}", *(const uint64_t*)asset));
    }

    void WriteImageRef(BO4JsonWriter& json, const void* image);

    // a GPU buffer (0x40, sub_142E1DBE0): its data (an i32 by an i32 at 8, 0xC bytes) in the physical block
    void WriteBuffer(BO4JsonWriter& json, const char* key, const byte* b) {
        json.WriteFieldNameString(key);
        json.BeginObject();
        json.WriteFieldValueString("header", Hex(b, 0x40));
        if (*(void**)b) {
            json.WriteFieldValueString("data", Hex(*(void**)b, (size_t)*(int32_t*)(b + 8) * *(int32_t*)(b + 0xC)));
        }
        json.EndObject();
    }

    void WriteString(BO4JsonWriter& json, const char* key, const char* str) {
        json.WriteFieldNameString(key);
        if (str) {
            json.WriteValueString(str);
        } else {
            json.WriteValueNull();
        }
    }

    /*
     * A KLF (0xD8, sub_142E1AD40), inline in its effects: a string at 0, the name at 0x18 (the top bit for a reference by
     * name, nothing else then), entries at 0x28 (0x2D8 each, the
     * count u16 at 0xC0: a string, a buffer at 0x288, an image at 0x2C8), arrays at 0x30 (0x18C an entry) and 0x38 (8 by the
     * u16 at 0xC2) in the physical block, buffers at 0x40 and 0x80, an image at 0xC8
     */
    void WriteKlf(BO4JsonWriter& json, const byte* k) {
        json.BeginObject();
        json.WriteFieldValueString("header", Hex(k, 0xD8));
        WriteString(json, "string", *(const char**)k);
        uint16_t count{ *(uint16_t*)(k + 0xC0) };
        json.WriteFieldNameString("entries");
        json.BeginArray();
        const byte* entries{ *(const byte**)(k + 0x28) };
        for (size_t i = 0; entries && i < count; i++) {
            const byte* e{ entries + 0x2D8 * i };
            json.BeginObject();
            json.WriteFieldValueString("header", Hex(e, 0x2D8));
            WriteString(json, "string", *(const char**)e);
            WriteBuffer(json, "buffer", e + 0x288);
            json.WriteFieldNameString("image");
            WriteImageRef(json, *(void**)(e + 0x2C8));
            json.EndObject();
        }
        json.EndArray();
        if (*(void**)(k + 0x30)) {
            json.WriteFieldValueString("a30", Hex(*(void**)(k + 0x30), 0x18Cull * count));
        }
        if (*(void**)(k + 0x38)) {
            json.WriteFieldValueString("a38", Hex(*(void**)(k + 0x38), 8ull * *(uint16_t*)(k + 0xC2)));
        }
        WriteBuffer(json, "buffer40", k + 0x40);
        WriteBuffer(json, "buffer80", k + 0x80);
        json.WriteFieldNameString("image");
        WriteImageRef(json, *(void**)(k + 0xC8));
        json.EndObject();
    }

    // an effect pointer of a copied block back to its name (the handler's stubs and the zone's effects start with it)
    void UnresolveFx(byte* block, size_t off) {
        const byte* fx{ *(const byte**)(block + off) };
        if (fx) {
            *(uint64_t*)(block + off) = *(const uint64_t*)fx & 0x7FFFFFFFFFFFFFFFull;
        }
    }

    // an image's name, at 0x20 of the GfxImage
    void WriteImageRef(BO4JsonWriter& json, const void* image) {
        WriteRef(json, image ? (const byte*)image + 0x20 : nullptr);
    }

    class FxWorker : public Worker {
        using Worker::Worker;

        void Unlink(fastfile::FastFileOption& opt, void* ptr) override {
            byte* fx{ (byte*)ptr };
            uint64_t name{ *(uint64_t*)fx };
            std::filesystem::path outFile{ opt.m_output / "bo4" / "graphics" / "fx" / std::format("file_{:x}.json", name & 0x7FFFFFFFFFFFFFFFull) };
            std::filesystem::create_directories(outFile.parent_path());
            LOG_OPT_INFO("Dump fx {}", outFile.string());

            BO4JsonWriter json{};
            json.BeginObject();
            json.WriteFieldValueString("name", std::format("#hash_{:x}", name & 0x7FFFFFFFFFFFFFFFull));
            json.WriteFieldValueBool("lossless", true);
            byte header[FX_SIZE];
            std::memcpy(header, fx, FX_SIZE);
            UnresolveFx(header, 0x90);
            json.WriteFieldValueString("header", Hex(header, FX_SIZE));
            size_t count{ (size_t)(*(int16_t*)(fx + 0x24) + *(int16_t*)(fx + 0x26) + *(int16_t*)(fx + 0x28)) };
            byte* elems{ *(byte**)(fx + 0x30) };
            json.WriteFieldNameString("elems");
            json.BeginArray();
            for (size_t i = 0; elems && i < count; i++) {
                byte* e{ elems + ELEM_SIZE * i };
                byte type{ e[0x264] };
                byte visualCount{ e[0x26E] };
                VisualKind kind{ KindOf(type) };
                json.BeginObject();
                byte elemHeader[ELEM_SIZE];
                std::memcpy(elemHeader, e, ELEM_SIZE);
                for (size_t off : { (size_t)0xC8, (size_t)0xD8, (size_t)0xE8, (size_t)0xF8 }) {
                    UnresolveFx(elemHeader, off);
                }
                if (type == 0xE && visualCount <= 1) {
                    UnresolveFx(elemHeader, 0);
                    UnresolveFx(elemHeader, 0x40);
                }
                json.WriteFieldValueString("header", Hex(elemHeader, ELEM_SIZE));

                // the visual blocks: inline (one visual) or an array of 0x40 visuals; decals (0xD) two materials a visual
                json.WriteFieldNameString("visuals");
                json.BeginArray();
                for (size_t off : { (size_t)0, (size_t)0x40 }) {
                    json.BeginObject();
                    byte* v{ *(byte**)(e + off) };
                    if (type == 0xD) {
                        json.WriteFieldNameString("decal");
                        json.BeginArray();
                        for (size_t j = 0; v && j < visualCount; j++) {
                            json.BeginArray();
                            WriteRef(json, *(void**)(v + 0x10 * j));
                            WriteRef(json, *(void**)(v + 0x10 * j + 8));
                            json.EndArray();
                        }
                        json.EndArray();
                    } else if (visualCount > 1) {
                        if (v) {
                            std::vector<byte> arr(v, v + VISUAL_SIZE * visualCount);
                            for (size_t j = 0; type == 0xE && j < visualCount; j++) {
                                UnresolveFx(arr.data(), VISUAL_SIZE * j);
                            }
                            json.WriteFieldValueString("array", Hex(arr.data(), arr.size()));
                            json.WriteFieldNameString("assets");
                            json.BeginArray();
                            for (size_t j = 0; j < visualCount; j++) {
                                if (kind == VK_DATA) {
                                    json.WriteValueNull();
                                } else {
                                    WriteRef(json, *(void**)(v + VISUAL_SIZE * j));
                                }
                            }
                            json.EndArray();
                        }
                    } else if (kind != VK_DATA) {
                        json.WriteFieldNameString("asset");
                        WriteRef(json, v);
                    }
                    json.EndObject();
                }
                json.EndArray();

                // the type's block: a trail (5), a light def (8) or an impact (0xB) by name, else a byte
                byte* u{ *(byte**)(e + 0x80) };
                if (u) {
                    if (type == 5) {
                        json.WriteFieldNameString("trail");
                        json.BeginObject();
                        json.WriteFieldValueString("header", Hex(u, 0x30));
                        if (*(void**)(u + 0x18)) {
                            json.WriteFieldValueString("verts", Hex(*(void**)(u + 0x18), 0x14ull * *(int32_t*)(u + 0x14)));
                        }
                        if (*(void**)(u + 0x28)) {
                            json.WriteFieldValueString("inds", Hex(*(void**)(u + 0x28), 2ull * *(int32_t*)(u + 0x20)));
                        }
                        json.EndObject();
                    } else if (type == 8) {
                        json.WriteFieldNameString("lightDef");
                        WriteRef(json, u);
                    } else if (type == 0xB) {
                        json.WriteFieldNameString("klf");
                        WriteKlf(json, u);
                    } else {
                        json.WriteFieldValueString("byte80", Hex(u, 1));
                    }
                }

                // the sample arrays: element size, count byte (+1 or the count + 1 entries)
                static const struct { size_t off, size, countOff; bool plusOne; } arrays[]{
                    { 0x88, 0x60, 0x26F, true }, { 0x90, 8, 0x270, false }, { 0x98, 0x68, 0x273, true },
                    { 0xA0, 8, 0x271, false }, { 0xA8, 0x30, 0x272, true },
                };
                json.WriteFieldNameString("arrays");
                json.BeginArray();
                for (const auto& a : arrays) {
                    byte* data{ *(byte**)(e + a.off) };
                    if (!data) {
                        json.WriteValueNull();
                        continue;
                    }
                    size_t len{ a.plusOne ? a.size * ((size_t)e[a.countOff] + 1) : a.size * (size_t)e[a.countOff] + 8 };
                    json.WriteValueString(Hex(data, len));
                }
                json.EndArray();
                json.EndObject();
            }
            json.EndArray();

            // the light descriptions (0x268, sub_142E1D1F0, no pointers) of type 8 elements, inline in the linker
            std::map<uint64_t, byte*> lights{};
            for (size_t i = 0; elems && i < count; i++) {
                byte* e{ elems + ELEM_SIZE * i };
                if (e[0x264] != 8) {
                    continue;
                }
                for (size_t off : { (size_t)0, (size_t)0x40, (size_t)0x80 }) {
                    byte* l{ *(byte**)(e + off) };
                    if (off != 0x80 && e[0x26E] > 1) {
                        for (size_t j = 0; l && j < e[0x26E]; j++) {
                            byte* lj{ *(byte**)(l + VISUAL_SIZE * j) };
                            if (lj) {
                                lights[*(uint64_t*)lj & 0x7FFFFFFFFFFFFFFFull] = lj;
                            }
                        }
                    } else if (l) {
                        lights[*(uint64_t*)l & 0x7FFFFFFFFFFFFFFFull] = l;
                    }
                }
            }
            json.WriteFieldNameString("lightDefs");
            json.BeginObject();
            for (auto& [h, l] : lights) {
                json.WriteFieldValueString(std::format("hash_{:x}", h).c_str(), Hex(l, 0x268));
            }
            json.EndObject();
            json.EndObject();
            if (!json.WriteToFile(outFile)) {
                LOG_ERROR("Error when dumping {}", outFile.string());
            }
        }
    };

    utils::MapAdder<FxWorker, games::bo4::pool::XAssetType, Worker> impl{ GetWorkers(), games::bo4::pool::XAssetType::ASSET_TYPE_FX };
} // namespace

namespace fastfile::handlers::bo4 {
    void WriteKlfJson(BO4JsonWriter& json, const byte* klf) {
        WriteKlf(json, klf);
    }
}
