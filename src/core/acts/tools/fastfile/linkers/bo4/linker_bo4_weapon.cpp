#include <includes.hpp>
#include <tools/fastfile/linkers/linker_bo4.hpp>
#include <tools/fastfile/bo4_weapon_slots.hpp>

/*
 * Stock weapons as the bo4 weapon dumper writes them ("lossless": true, "header", "slots" by hex offset): the WeaponDef
 * as it was, its pointers and script strings written back in the loader's order (bo4_weapon_slots.hpp). An asset is
 * linked as the json names it: ",hash_x" for one of another zone (a reference), a path or "#hash_x" for one this
 * zone builds (as an aitype's). The tunables are inline. A file is the id or tables/weapon/<id>.json.
 */
namespace {
    using namespace fastfile::linker::bo4;
    using namespace fastfile::bo4weapon;

    std::vector<byte> Unhex(const rapidjson::Value* v) {
        std::vector<byte> out{};
        if (!v || !v->IsString()) {
            return out;
        }
        const char* s{ v->GetString() };
        size_t n{ v->GetStringLength() / 2 };
        out.resize(n);
        auto Nib = [](char c) -> byte { return (byte)(c <= '9' ? c - '0' : (c | 0x20) - 'a' + 10); };
        for (size_t i = 0; i < n; i++) {
            out[i] = (byte)(Nib(s[2 * i]) << 4 | Nib(s[2 * i + 1]));
        }
        return out;
    }

    const rapidjson::Value* Member(const rapidjson::Value& v, const char* name) {
        if (!v.IsObject()) {
            return nullptr;
        }
        auto it{ v.FindMember(name) };
        return it == v.MemberEnd() || it->value.IsNull() ? nullptr : &it->value;
    }

    bool IsPointer(uint64_t v) { return v == fastfile::linker::memory::POINTER_NEXT || (v >= 0x10000000000ull && v < 0x800000000000ull); }

    class XAssetLinkerImpl : public XAssetLinker {
      public:
        using XAssetLinker::XAssetLinker;

        uint32_t Scr(BO4FFContext& ff, const rapidjson::Value* v) {
            return v && v->IsString() ? ff.data.AddScrStringKeepEmpty(v->GetString()) : 0;
        }

        void Write(BO4FFContext& ff, const std::vector<byte>& data, size_t align, size_t size) {
            std::vector<byte> d{ data };
            d.resize(size);
            ff.data.Align(align);
            if (size) {
                ff.data.WriteStream(d.data(), d.size());
            }
        }

        // an asset as the json names it, null for none
        void Ref(BO4LinkContext& ctx, BO4FFContext& ff, uint32_t type, const rapidjson::Value* v, void*& ref) {
            ref = nullptr;
            if (v && v->IsString() && v->GetStringLength()) {
                ctx.LinkAsset((XAssetType)type, v->GetString(), ref, false, &ff);
            }
        }

        void Tunables(BO4LinkContext& ctx, BO4FFContext& ff, const rapidjson::Value& t, void*& ref) {
            std::vector<byte> header{ Unhex(Member(t, "header")) };
            header.resize(WEAPON_TUNABLES_SIZE);
            ref = (void*)fastfile::linker::memory::POINTER_NEXT;
            ff.data.PushStream(XFILE_BLOCK_TEMP);
            ff.data.Align(8);
            byte* a{ ff.data.AllocStreamPtr<byte>(WEAPON_TUNABLES_SIZE) };
            std::memcpy(a, header.data(), WEAPON_TUNABLES_SIZE);
            auto Had = [&header](size_t off) { return *(uint64_t*)&header[off] != 0; };
            for (size_t off : { 0x0, 0x8, 0x10, 0x18, 0x20, 0x28, 0x4C8 }) {
                *(uint64_t*)(a + off) = 0;
            }
            auto ScrAt = [&](uint32_t off) { *(uint32_t*)(a + off) = Scr(ff, Member(t, utils::va("scr%x", off))); };
            auto Next = [&](size_t off) { *(uint64_t*)(a + off) = fastfile::linker::memory::POINTER_NEXT; };
            auto Str = [&](size_t off, const char* key) {
                const rapidjson::Value* s{ Member(t, key) };
                if (Had(off) && s && s->IsString()) {
                    ff.data.AddXString(*(const char**)(a + off), s->GetString());
                }
            };
            auto Blob = [&](size_t off, const char* key, size_t size) {
                if (Had(off)) {
                    Next(off);
                    Write(ff, Unhex(Member(t, key)), 4, size);
                }
            };
            // sub_142E34C90's order
            ff.data.PushStream(XFILE_BLOCK_VIRTUAL);
            Blob(0x4C8, "data4c8", 0x10ull * *(uint64_t*)(a + 0x4C0));
            ScrAt(0x204);
            Str(0x18, "str18");
            ScrAt(0x1FC);
            ScrAt(0x200);
            Str(0x28, "str28");
            ScrAt(0x208);
            ScrAt(0x20C);
            Blob(0x8, "data8", 0xA0);
            Blob(0x10, "data10", 0xA0);
            Blob(0x0, "data0", 0x54);
            Str(0x20, "str20");
            ff.data.PopStream();
            ff.data.PopStream();
        }

        void Compute(BO4LinkContext& ctx, const char* id, BO4FFContext& ff) override {
            std::filesystem::path path{ ctx.linkCtx.input / id };
            if (!std::filesystem::is_regular_file(path)) {
                std::filesystem::path p2{ ctx.linkCtx.input / "tables" / "weapon" / std::format("{}.json", id) };
                if (std::filesystem::is_regular_file(p2)) {
                    path = p2;
                }
            }
            BO4Json cfg{ path };
            if (!cfg.SyncConfig(false)) {
                LOG_ERROR("Can't read weapon {}", path.string());
                ctx.error = true;
                return;
            }
            const rapidjson::Value& root{ cfg.main };
            const rapidjson::Value* slots{ Member(root, "slots") };
            std::vector<byte> header{ Unhex(Member(root, "header")) };
            if (!Member(root, "lossless") || !slots || header.size() != WEAPON_DEF_SIZE) {
                LOG_ERROR("weapon {}: only the dumper's lossless weapons can be linked", path.string());
                ctx.error = true;
                return;
            }
            auto Had = [&header](size_t off) { return *(uint64_t*)&header[off] != 0; };
            auto SlotValue = [slots](uint32_t off) { return Member(*slots, utils::va("%x", off)); };

            ff.data.PushStream(XFILE_BLOCK_TEMP);
            byte* a{ ff.data.AllocStreamPtr<byte>(WEAPON_DEF_SIZE) };
            std::memcpy(a, header.data(), WEAPON_DEF_SIZE);
            auto Next = [&](size_t off) { *(uint64_t*)(a + off) = fastfile::linker::memory::POINTER_NEXT; };

            // the pointers of the table, written as they're linked; the structs it doesn't write must be null
            for (const Slot& s : SLOTS) {
                switch (s.kind) {
                case SK_ASSET:
                case SK_ASSET_ARRAY:
                case SK_SCR_ARRAY:
                case SK_BLOB:
                case SK_XSTRING:
                case SK_TUNABLES:
                    *(uint64_t*)(a + s.offset) = 0;
                    break;
                case SK_ASSET_FIXED:
                    std::memset(a + s.offset, 0, 8ull * s.count);
                    break;
                case SK_STRUCT5:
                    for (uint32_t i = 0; i < s.count; i++) {
                        std::memset(a + s.offset + 0x20 * i, 0, 0x20);
                    }
                    break;
                case SK_UNSUPPORTED:
                    if (IsPointer(*(uint64_t*)(a + s.offset))) {
                        LOG_ERROR("weapon {}: the struct at 0x{:x} isn't supported", path.string(), s.offset);
                        ctx.error = true;
                        return;
                    }
                    break;
                default:
                    break;
                }
            }

            ff.data.PushStream(XFILE_BLOCK_VIRTUAL);
            uint32_t variants{ (uint32_t)std::max(0, *(int32_t*)(a + 0x1350)) };
            for (const Slot& s : SLOTS) {
                const rapidjson::Value* v{ SlotValue(s.offset) };
                uint32_t count{ SlotCount(s, a) };
                byte* d{ a + s.offset };
                switch (s.kind) {
                case SK_ASSET:
                    if (Had(s.offset)) {
                        Ref(ctx, ff, s.type, v, *(void**)d);
                    }
                    break;
                case SK_ASSET_FIXED:
                    for (uint32_t i = 0; i < s.count; i++) {
                        Ref(ctx, ff, s.type, v && v->IsArray() && i < v->Size() ? &(*v)[i] : nullptr, ((void**)d)[i]);
                    }
                    break;
                case SK_ASSET_ARRAY:
                    if (Had(s.offset)) {
                        Next(s.offset);
                        ff.data.Align(8);
                        void** arr{ ff.data.AllocStreamPtr<void*>(count) };
                        for (uint32_t i = 0; i < count; i++) {
                            arr[i] = nullptr;
                        }
                        for (uint32_t i = 0; i < count; i++) {
                            Ref(ctx, ff, s.type, v && v->IsArray() && i < v->Size() ? &(*v)[i] : nullptr, arr[i]);
                        }
                    }
                    break;
                case SK_SCR:
                    *(uint32_t*)d = Scr(ff, v);
                    break;
                case SK_SCR_ARRAY:
                    if (Had(s.offset)) {
                        Next(s.offset);
                        ff.data.Align(4);
                        uint32_t* out{ ff.data.AllocStreamPtr<uint32_t>(count) };
                        for (uint32_t i = 0; i < count; i++) {
                            out[i] = v && v->IsArray() && i < v->Size() ? Scr(ff, &(*v)[i]) : 0;
                        }
                    }
                    break;
                case SK_BLOB:
                    if (Had(s.offset)) {
                        Next(s.offset);
                        Write(ff, Unhex(v), s.align, (size_t)count * s.elemSize);
                    }
                    break;
                case SK_XSTRING:
                    if (Had(s.offset)) {
                        ff.data.AddXString(*(const char**)d, v && v->IsString() ? v->GetString() : "");
                    }
                    break;
                case SK_STRUCT5:
                    // sub_142E31E80: per entry its models, strings and 2 x 0xC per variant
                    for (uint32_t i = 0; i < s.count; i++) {
                        const byte* orig{ header.data() + s.offset + 0x20 * i };
                        byte* e{ d + 0x20 * i };
                        const rapidjson::Value* ev{ v && v->IsArray() && i < v->Size() ? &(*v)[i] : nullptr };
                        if (*(uint64_t*)orig) {
                            *(uint64_t*)e = fastfile::linker::memory::POINTER_NEXT;
                            const rapidjson::Value* models{ ev ? Member(*ev, "models") : nullptr };
                            ff.data.Align(8);
                            void** arr{ ff.data.AllocStreamPtr<void*>(variants) };
                            for (uint32_t k = 0; k < variants; k++) {
                                arr[k] = nullptr;
                            }
                            for (uint32_t k = 0; k < variants; k++) {
                                Ref(ctx, ff, XAssetType::ASSET_TYPE_XMODEL, models && models->IsArray() && k < models->Size() ? &(*models)[k] : nullptr, arr[k]);
                            }
                        }
                        if (*(uint64_t*)(orig + 8)) {
                            *(uint64_t*)(e + 8) = fastfile::linker::memory::POINTER_NEXT;
                            const rapidjson::Value* strs{ ev ? Member(*ev, "strings") : nullptr };
                            ff.data.Align(4);
                            uint32_t* out{ ff.data.AllocStreamPtr<uint32_t>(variants) };
                            for (uint32_t k = 0; k < variants; k++) {
                                out[k] = strs && strs->IsArray() && k < strs->Size() ? Scr(ff, &(*strs)[k]) : 0;
                            }
                        }
                        for (uint32_t k = 0; k < 2; k++) {
                            if (*(uint64_t*)(orig + 0x10 + 8 * k)) {
                                *(uint64_t*)(e + 0x10 + 8 * k) = fastfile::linker::memory::POINTER_NEXT;
                                Write(ff, Unhex(ev ? Member(*ev, k ? "data18" : "data10") : nullptr), 4, 0xCull * variants);
                            }
                        }
                    }
                    break;
                case SK_TUNABLES:
                    if (Had(s.offset) && v && v->IsObject()) {
                        Tunables(ctx, ff, *v, *(void**)d);
                    }
                    break;
                default:
                    break;
                }
            }
            ff.data.PopStream();
            ff.data.PopStream();
            LOG_INFO("Added asset weapon {} (hash_{:x}), lossless", path.filename().string(), *(uint64_t*)a);
        }
    };

    utils::MapAdder<XAssetLinkerImpl, XAssetType, XAssetLinker> impl{ GetWorkers(), XAssetType::ASSET_TYPE_WEAPON };
} // namespace
