#include <includes.hpp>
#include <tools/fastfile/linkers/linker_bo4.hpp>

/*
 * FX (FxEffectDef 0xA0, its loader sub_142E1A360) from acts' dumper (bo4_unlinker_fx.cpp), lossless: the header, its
 * elements (FxElemDef 0x280, sub_142E1A5E0) in the loader's order: two visual blocks (0x00, 0x40), the type's block
 * (0x80), five sample arrays (0x88..0xA8). Materials, models and images by name (in the zone before, or a common zone);
 * the light descriptions (0x268) and KLFs (0xD8) of the source zone inline the first time in the zone, by name after; the
 * dump's names with the top bit (another zone's) by name (two assets of a name in a zone crash its unload).
 *
 * fx,path/to/fx.json
 */
namespace {
    using namespace fastfile::linker::bo4;

    constexpr size_t FX_SIZE = 0xA0;
    constexpr size_t ELEM_SIZE = 0x280;
    constexpr size_t VISUAL_SIZE = 0x40;
    constexpr size_t LIGHT_SIZE = 0x268;
    constexpr size_t KLF_SIZE = 0xD8;
    constexpr size_t KLF_ENTRY_SIZE = 0x2D8;
    constexpr size_t BUFFER_SIZE = 0x40;

    int HexDigit(char c) {
        if (c >= '0' && c <= '9')
            return c - '0';
        if (c >= 'a' && c <= 'f')
            return c - 'a' + 10;
        if (c >= 'A' && c <= 'F')
            return c - 'A' + 10;
        return -1;
    }

    const rapidjson::Value* Member(const rapidjson::Value& obj, const char* name) {
        if (!obj.IsObject()) {
            return nullptr;
        }
        auto it{ obj.FindMember(name) };
        return it == obj.MemberEnd() || it->value.IsNull() ? nullptr : &it->value;
    }

    std::vector<byte> Hex(const rapidjson::Value* val) {
        std::vector<byte> out{};
        if (!val || !val->IsString() || (val->GetStringLength() & 1)) {
            return out;
        }
        const char* str{ val->GetString() };
        out.reserve(val->GetStringLength() / 2);
        for (size_t i = 0; i < val->GetStringLength(); i += 2) {
            out.push_back((byte)((HexDigit(str[i]) << 4) | HexDigit(str[i + 1])));
        }
        return out;
    }

    // a dumped name with the top bit: the asset is another zone's (by name), else the source zone's own
    bool IsRef(BO4LinkContext& ctx, const rapidjson::Value* val) {
        return val && val->IsString() && *val->GetString() && (ctx.HashXHash(val->GetString(), true) >> 63);
    }

    // a "#hash_x" name's value, 0 when none
    uint64_t RefHash(BO4LinkContext& ctx, const rapidjson::Value* val) {
        return val && val->IsString() && *val->GetString() ? ctx.HashXHash(val->GetString(), true) & 0x7FFFFFFFFFFFFFFFull : 0;
    }

    enum VisualKind { VK_DATA, VK_MATERIAL, VK_MODEL, VK_LIGHTDEF };

    VisualKind KindOf(byte type) {
        switch (type) {
        case 7: return VK_MODEL;
        case 8: return VK_LIGHTDEF;
        case 9: case 0xA: case 0xB: case 0xE: case 0xF: case 0x10: return VK_DATA;
        default: return VK_MATERIAL;
        }
    }

    // the light descriptions and KLFs linked inline in each fastfile
    std::unordered_map<BO4FFContext*, std::unordered_set<uint64_t>> inlined{};

    class XAssetLinkerImpl : public XAssetLinker {
      public:
        using XAssetLinker::XAssetLinker;

        bool error{};

        void Fail(BO4LinkContext& ctx, const std::string& what) {
            LOG_ERROR("{}", what);
            ctx.error = true;
            error = true;
        }

        // an asset by name, its header (the ptr loader's DB_AllocStreamPos) aligned as align
        void ByName(BO4LinkContext& ctx, BO4FFContext& ff, XAssetType type, uint64_t name, void*& ref, size_t align = 8) {
            if (!name) {
                ref = nullptr;
                return;
            }
            if (align > 8) {
                ff.data.PushStream(XFILE_BLOCK_TEMP);
                ff.data.Align(align);
                ff.data.PopStream();
            }
            ctx.LinkAsset(type, std::format(",hash_{:x}", name).c_str(), ref, false, &ff);
        }

        // a light description (sub_142E1D1F0: 0x268, aligned 16, no pointers) inline once
        void Light(BO4LinkContext& ctx, BO4FFContext& ff, const rapidjson::Value& root, const rapidjson::Value* nameVal, void*& ref) {
            uint64_t name{ RefHash(ctx, nameVal) };
            if (!name || IsRef(ctx, nameVal) || inlined[&ff].contains(name)) {
                ByName(ctx, ff, XAssetType::ASSET_TYPE_LIGHT_DESCRIPTION, name, ref, 16);
                return;
            }
            const rapidjson::Value* lights{ Member(root, "lightDefs") };
            std::vector<byte> data{ Hex(lights ? Member(*lights, std::format("hash_{:x}", name).c_str()) : nullptr) };
            if (data.size() != LIGHT_SIZE) {
                Fail(ctx, std::format("fx: light description hash_{:x} has no data", name));
                return;
            }
            inlined[&ff].insert(name);
            ref = (void*)fastfile::linker::memory::POINTER_NEXT;
            ff.data.PushStream(XFILE_BLOCK_TEMP);
            ff.data.Align(16);
            ff.data.WriteStream(data.data(), data.size());
            ff.data.PopStream();
        }

        // a GPU buffer's data (sub_142E1DBE0) in the physical block, aligned 256
        void BufferData(BO4FFContext& ff, const rapidjson::Value* buf) {
            std::vector<byte> data{ Hex(buf ? Member(*buf, "data") : nullptr) };
            if (data.empty()) {
                return;
            }
            ff.data.PushStream(XFILE_BLOCK_PHYSICAL);
            ff.data.Align(0x100);
            ff.data.WriteStream(data.data(), data.size());
            ff.data.PopStream();
        }

        // a buffer's header in its parent: the data pointer kept as written, the runtime part cleared
        void BufferHeader(BO4LinkContext& ctx, byte* out, const rapidjson::Value* buf) {
            std::vector<byte> h{ Hex(buf ? Member(*buf, "header") : nullptr) };
            if (h.size() != BUFFER_SIZE) {
                std::memset(out, 0, BUFFER_SIZE);
                return;
            }
            std::memcpy(out, h.data(), BUFFER_SIZE);
            *(uint64_t*)out = Member(*buf, "data") ? fastfile::linker::memory::POINTER_NEXT : 0;
            std::memset(out + 0x10, 0, BUFFER_SIZE - 0x10);
        }

        void String(BO4FFContext& ff, const rapidjson::Value* str) {
            if (str && str->IsString()) {
                ff.data.WriteStream(str->GetString(), str->GetStringLength() + 1);
            }
        }

        // a KLF (sub_142E1AD40: 0xD8 aligned 16, then its data in virtual and physical) inline once
        void Klf(BO4LinkContext& ctx, BO4FFContext& ff, const rapidjson::Value& k, uint64_t uniqueName, void*& ref) {
            std::vector<byte> header{ Hex(Member(k, "header")) };
            if (header.size() != KLF_SIZE) {
                Fail(ctx, "fx: bad klf header");
                return;
            }
            // the name at 0x18 (pool xoffsets), the top bit for another zone's KLF
            uint64_t raw{ *(uint64_t*)(header.data() + 0x18) };
            uint64_t name{ raw & 0x7FFFFFFFFFFFFFFFull };
            if (!name) {
                name = uniqueName;
            }
            if ((raw >> 63) || inlined[&ff].contains(name)) {
                ByName(ctx, ff, XAssetType::ASSET_TYPE_KLF, name, ref, 16);
                return;
            }
            inlined[&ff].insert(name);
            *(uint64_t*)(header.data() + 0x18) = name;

            const rapidjson::Value* entries{ Member(k, "entries") };
            size_t count{ entries && entries->IsArray() ? entries->Size() : 0 };
            if (count != *(uint16_t*)(header.data() + 0xC0)) {
                Fail(ctx, "fx: klf entry count");
                return;
            }
            ref = (void*)fastfile::linker::memory::POINTER_NEXT;
            ff.data.PushStream(XFILE_BLOCK_TEMP);
            ff.data.Align(16);
            byte* out{ ff.data.AllocStreamPtr<byte>(KLF_SIZE) };
            std::memcpy(out, header.data(), KLF_SIZE);
            auto Next = [](bool has) { return has ? fastfile::linker::memory::POINTER_NEXT : (uint64_t)0; };
            *(uint64_t*)out = Next(Member(k, "string"));
            *(uint64_t*)(out + 0x28) = Next(count);
            *(uint64_t*)(out + 0x30) = Next(Member(k, "a30"));
            *(uint64_t*)(out + 0x38) = Next(Member(k, "a38"));
            BufferHeader(ctx, out + 0x40, Member(k, "buffer40"));
            BufferHeader(ctx, out + 0x80, Member(k, "buffer80"));
            *(uint64_t*)(out + 0xC8) = 0;

            ff.data.PushStream(XFILE_BLOCK_VIRTUAL);
            String(ff, Member(k, "string"));
            if (count) {
                ff.data.Align(16);
                byte* arr{ ff.data.AllocStreamPtr<byte>(KLF_ENTRY_SIZE * count) };
                for (size_t i = 0; i < count; i++) {
                    const rapidjson::Value& e{ (*entries)[(rapidjson::SizeType)i] };
                    std::vector<byte> eh{ Hex(Member(e, "header")) };
                    byte* o{ arr + KLF_ENTRY_SIZE * i };
                    if (eh.size() != KLF_ENTRY_SIZE) {
                        Fail(ctx, "fx: bad klf entry");
                        return;
                    }
                    std::memcpy(o, eh.data(), KLF_ENTRY_SIZE);
                    *(uint64_t*)o = Next(Member(e, "string"));
                    BufferHeader(ctx, o + 0x288, Member(e, "buffer"));
                    *(uint64_t*)(o + 0x2C8) = 0;
                }
                for (size_t i = 0; i < count; i++) {
                    const rapidjson::Value& e{ (*entries)[(rapidjson::SizeType)i] };
                    byte* o{ arr + KLF_ENTRY_SIZE * i };
                    String(ff, Member(e, "string"));
                    BufferData(ff, Member(e, "buffer"));
                    ByName(ctx, ff, XAssetType::ASSET_TYPE_IMAGE, RefHash(ctx, Member(e, "image")), *(void**)(o + 0x2C8));
                }
            }
            for (const char* key : { "a30", "a38" }) {
                std::vector<byte> data{ Hex(Member(k, key)) };
                if (data.empty()) {
                    continue;
                }
                ff.data.PushStream(XFILE_BLOCK_PHYSICAL);
                ff.data.Align(16);
                ff.data.WriteStream(data.data(), data.size());
                ff.data.PopStream();
            }
            BufferData(ff, Member(k, "buffer40"));
            BufferData(ff, Member(k, "buffer80"));
            ByName(ctx, ff, XAssetType::ASSET_TYPE_IMAGE, RefHash(ctx, Member(k, "image")), *(void**)(out + 0xC8));
            ff.data.PopStream();
            ff.data.PopStream();
        }

        // a visual's asset (sub_142E1AAF0 for one, sub_142E1A820 for each of an array)
        void VisualAsset(BO4LinkContext& ctx, BO4FFContext& ff, const rapidjson::Value& root, VisualKind kind, const rapidjson::Value* name, void*& ref) {
            switch (kind) {
            // Load_MaterialPtr (sub_142E239E0) aligns the header 16, the model and image ones 8
            case VK_MATERIAL: ByName(ctx, ff, XAssetType::ASSET_TYPE_MATERIAL, RefHash(ctx, name), ref, 16); break;
            case VK_MODEL: ByName(ctx, ff, XAssetType::ASSET_TYPE_XMODEL, RefHash(ctx, name), ref); break;
            case VK_LIGHTDEF: Light(ctx, ff, root, name, ref); break;
            default: break;
            }
        }

        void Compute(BO4LinkContext& ctx, const char* id, BO4FFContext& ff) override {
            std::filesystem::path path{ ctx.linkCtx.input / id };
            BO4Json cfg{ path };
            if (!cfg.SyncConfig(false)) {
                Fail(ctx, std::format("Can't read fx {}", path.string()));
                return;
            }
            const rapidjson::Value& root{ cfg.main };
            std::vector<byte> header{ Hex(Member(root, "header")) };
            if (header.size() != FX_SIZE) {
                Fail(ctx, std::format("fx {}: bad header", path.string()));
                return;
            }
            const rapidjson::Value* elemsCfg{ Member(root, "elems") };
            size_t count{ elemsCfg && elemsCfg->IsArray() ? elemsCfg->Size() : 0 };
            size_t expected{ (size_t)(*(int16_t*)(header.data() + 0x24) + *(int16_t*)(header.data() + 0x26) + *(int16_t*)(header.data() + 0x28)) };
            if (count != expected) {
                Fail(ctx, std::format("fx {}: {} elements for the counts' {}", path.string(), count, expected));
                return;
            }
            uint64_t name{ ctx.HashXHash(cfg.GetString("name", path.stem().string().c_str()), true) };

            ff.data.PushStream(XFILE_BLOCK_TEMP);
            byte* fx{ ff.data.AllocStreamPtr<byte>(FX_SIZE) };
            std::memcpy(fx, header.data(), FX_SIZE);
            *(uint64_t*)fx = name;
            *(uint64_t*)(fx + 0x30) = count ? fastfile::linker::memory::POINTER_NEXT : 0;

            ff.data.PushStream(XFILE_BLOCK_VIRTUAL);
            byte* elems{};
            if (count) {
                ff.data.Align(8);
                elems = ff.data.AllocStreamPtr<byte>(ELEM_SIZE * count);
            }
            // the headers and their pointers, then each element's data in the loader's order
            for (size_t i = 0; i < count; i++) {
                const rapidjson::Value& e{ (*elemsCfg)[(rapidjson::SizeType)i] };
                std::vector<byte> eh{ Hex(Member(e, "header")) };
                if (eh.size() != ELEM_SIZE) {
                    Fail(ctx, std::format("fx {}: element {}: bad header", path.string(), i));
                    return;
                }
                byte* o{ elems + ELEM_SIZE * i };
                std::memcpy(o, eh.data(), ELEM_SIZE);
                byte type{ o[0x264] };
                byte visualCount{ o[0x26E] };
                const rapidjson::Value* visuals{ Member(e, "visuals") };
                for (size_t v = 0; v < 2; v++) {
                    const rapidjson::Value* vis{ visuals && visuals->IsArray() && v < visuals->Size() ? &(*visuals)[(rapidjson::SizeType)v] : nullptr };
                    uint64_t& ptr{ *(uint64_t*)(o + 0x40 * v) };
                    if (type == 0xD) {
                        const rapidjson::Value* decal{ vis ? Member(*vis, "decal") : nullptr };
                        ptr = decal && decal->IsArray() && decal->Size() ? fastfile::linker::memory::POINTER_NEXT : 0;
                    } else if (visualCount > 1) {
                        ptr = vis && Member(*vis, "array") ? fastfile::linker::memory::POINTER_NEXT : 0;
                    } else if (KindOf(type) != VK_DATA) {
                        ptr = 0;
                    }
                }
                uint64_t& u{ *(uint64_t*)(o + 0x80) };
                if (type == 5) {
                    u = Member(e, "trail") ? fastfile::linker::memory::POINTER_NEXT : 0;
                } else if (type == 8 || type == 0xB) {
                    u = 0;
                } else {
                    u = Member(e, "byte80") ? fastfile::linker::memory::POINTER_NEXT : 0;
                }
                const rapidjson::Value* arrays{ Member(e, "arrays") };
                for (size_t a = 0; a < 5; a++) {
                    bool has{ arrays && arrays->IsArray() && a < arrays->Size() && (*arrays)[(rapidjson::SizeType)a].IsString() };
                    *(uint64_t*)(o + 0x88 + 8 * a) = has ? fastfile::linker::memory::POINTER_NEXT : 0;
                }
            }
            static const struct { size_t size, countOff; bool plusOne; } arraySizes[]{
                { 0x60, 0x26F, true }, { 8, 0x270, false }, { 0x68, 0x273, true }, { 8, 0x271, false }, { 0x30, 0x272, true },
            };
            for (size_t i = 0; i < count && !error; i++) {
                const rapidjson::Value& e{ (*elemsCfg)[(rapidjson::SizeType)i] };
                byte* o{ elems + ELEM_SIZE * i };
                byte type{ o[0x264] };
                byte visualCount{ o[0x26E] };
                VisualKind kind{ KindOf(type) };
                const rapidjson::Value* visuals{ Member(e, "visuals") };
                for (size_t v = 0; v < 2; v++) {
                    const rapidjson::Value* vis{ visuals && visuals->IsArray() && v < visuals->Size() ? &(*visuals)[(rapidjson::SizeType)v] : nullptr };
                    if (type == 0xD) {
                        // a decal (sub_142E1A820): two materials a visual, always an array
                        const rapidjson::Value* decal{ vis ? Member(*vis, "decal") : nullptr };
                        if (!decal || !decal->IsArray() || !decal->Size()) {
                            continue;
                        }
                        if (decal->Size() != visualCount) {
                            Fail(ctx, std::format("fx {}: element {}: {} decal visuals for {}", path.string(), i, decal->Size(), visualCount));
                            return;
                        }
                        ff.data.Align(8);
                        byte* out{ ff.data.AllocStreamPtr<byte>(0x10 * visualCount) };
                        std::memset(out, 0, 0x10 * visualCount);
                        for (size_t j = 0; j < visualCount; j++) {
                            const rapidjson::Value& pair{ (*decal)[(rapidjson::SizeType)j] };
                            for (size_t k = 0; k < 2; k++) {
                                const rapidjson::Value* n{ pair.IsArray() && k < pair.Size() ? &pair[(rapidjson::SizeType)k] : nullptr };
                                if (RefHash(ctx, n)) {
                                    ByName(ctx, ff, XAssetType::ASSET_TYPE_MATERIAL, RefHash(ctx, n), *(void**)(out + 0x10 * j + 8 * k), 16);
                                }
                            }
                        }
                    } else if (visualCount > 1) {
                        std::vector<byte> arr{ Hex(vis ? Member(*vis, "array") : nullptr) };
                        if (arr.empty()) {
                            continue;
                        }
                        if (arr.size() != VISUAL_SIZE * visualCount) {
                            Fail(ctx, std::format("fx {}: element {}: visual array size", path.string(), i));
                            return;
                        }
                        ff.data.Align(8);
                        byte* out{ ff.data.AllocStreamPtr<byte>(arr.size()) };
                        std::memcpy(out, arr.data(), arr.size());
                        const rapidjson::Value* assets{ Member(*vis, "assets") };
                        for (size_t j = 0; j < visualCount && kind != VK_DATA; j++) {
                            *(uint64_t*)(out + VISUAL_SIZE * j) = 0;
                        }
                        for (size_t j = 0; j < visualCount && kind != VK_DATA; j++) {
                            const rapidjson::Value* n{ assets && assets->IsArray() && j < assets->Size() ? &(*assets)[(rapidjson::SizeType)j] : nullptr };
                            VisualAsset(ctx, ff, root, kind, n, *(void**)(out + VISUAL_SIZE * j));
                        }
                    } else if (kind != VK_DATA) {
                        VisualAsset(ctx, ff, root, kind, vis ? Member(*vis, "asset") : nullptr, *(void**)(o + 0x40 * v));
                    }
                }
                if (type == 5) {
                    if (const rapidjson::Value* trail{ Member(e, "trail") }) {
                        std::vector<byte> th{ Hex(Member(*trail, "header")) };
                        std::vector<byte> verts{ Hex(Member(*trail, "verts")) };
                        std::vector<byte> inds{ Hex(Member(*trail, "inds")) };
                        if (th.size() != 0x30) {
                            Fail(ctx, std::format("fx {}: element {}: bad trail", path.string(), i));
                            return;
                        }
                        ff.data.Align(8);
                        byte* t{ ff.data.AllocStreamPtr<byte>(0x30) };
                        std::memcpy(t, th.data(), 0x30);
                        *(uint64_t*)(t + 0x18) = verts.empty() ? 0 : fastfile::linker::memory::POINTER_NEXT;
                        *(uint64_t*)(t + 0x28) = inds.empty() ? 0 : fastfile::linker::memory::POINTER_NEXT;
                        if (!verts.empty()) {
                            ff.data.Align(4);
                            ff.data.WriteStream(verts.data(), verts.size());
                        }
                        if (!inds.empty()) {
                            ff.data.Align(2);
                            ff.data.WriteStream(inds.data(), inds.size());
                        }
                    }
                } else if (type == 8) {
                    Light(ctx, ff, root, Member(e, "lightDef"), *(void**)(o + 0x80));
                } else if (type == 0xB) {
                    if (const rapidjson::Value* klf{ Member(e, "klf") }) {
                        uint64_t unique{ ctx.HashXHash(std::format("hash_{:x}/klf{}", name & 0x7FFFFFFFFFFFFFFFull, i).c_str()) & 0x7FFFFFFFFFFFFFFFull };
                        Klf(ctx, ff, *klf, unique, *(void**)(o + 0x80));
                    }
                } else {
                    std::vector<byte> b{ Hex(Member(e, "byte80")) };
                    if (!b.empty()) {
                        ff.data.WriteStream(b.data(), 1);
                    }
                }
                const rapidjson::Value* arrays{ Member(e, "arrays") };
                for (size_t a = 0; a < 5; a++) {
                    std::vector<byte> data{ Hex(arrays && arrays->IsArray() && a < arrays->Size() ? &(*arrays)[(rapidjson::SizeType)a] : nullptr) };
                    if (data.empty()) {
                        continue;
                    }
                    size_t len{ arraySizes[a].plusOne ? arraySizes[a].size * ((size_t)o[arraySizes[a].countOff] + 1)
                                                      : arraySizes[a].size * (size_t)o[arraySizes[a].countOff] + 8 };
                    if (data.size() != len) {
                        Fail(ctx, std::format("fx {}: element {}: array {} size {} for {}", path.string(), i, a, data.size(), len));
                        return;
                    }
                    ff.data.Align(4);
                    ff.data.WriteStream(data.data(), data.size());
                }
            }
            ff.data.PopStream();
            ff.data.PopStream();
            if (!error) {
                LOG_INFO("Added asset fx {} (hash_{:x}), lossless, {} element(s)", path.filename().string(), name, count);
            }
        }
    };

    utils::MapAdder<XAssetLinkerImpl, XAssetType, XAssetLinker> impl{ GetWorkers(), XAssetType::ASSET_TYPE_FX };
} // namespace

namespace fastfile::linker::bo4 {
    bool LinkKlf(BO4LinkContext& ctx, BO4FFContext& ff, const rapidjson::Value& klf, void*& ref) {
        XAssetLinkerImpl linker{};
        linker.Klf(ctx, ff, klf, 0, ref);
        return !linker.error;
    }
}
