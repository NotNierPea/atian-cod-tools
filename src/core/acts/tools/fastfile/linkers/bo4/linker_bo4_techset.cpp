#include <includes.hpp>
#include <tools/fastfile/linkers/linker_bo4.hpp>

/*
 * Technique sets (MaterialTechniqueSet 0xA8, its loader sub_142E24040) from acts' dumper (bo4_unlinker_techset.cpp),
 * lossless: the header in the temp block, then in the virtual block each technique (0x100) and its 4 draw methods in the
 * loader's order (sub_142E236C0): args 0x18, the 0x20 array, the shader state (sub_142E23C50: 0x40, then its 0x78
 * block, programs 2, 5, 3, 4, 6, 7, its 0xB0 block), the 0x30 block; then the technique's 0xF8 array. A program (0x28,
 * its blob aligned 256), a 0x78 or a 0xB0 block the source shared (the same "id" in the dump) is an offset to the first
 * one: the game makes its objects once for each, the 0xB0's input layout from its state's vertex shader, so blocks equal
 * in bytes but apart in the source stay apart.
 *
 * techset,path/to/techset.json
 */
namespace {
    using namespace fastfile::linker::bo4;

    constexpr size_t TECHSET_SIZE = 0xA8;
    constexpr size_t TECHNIQUE_SIZE = 0x100;
    constexpr size_t DRAW_METHOD_SIZE = 0x38;
    constexpr size_t STATE_SIZE = 0x40;
    constexpr size_t PROGRAM_SIZE = 0x28;
    // the shader state's programs in the loader's order
    constexpr size_t PROGRAM_ORDER[]{ 2, 5, 3, 4, 6, 7 };

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

    // the shared blocks of each fastfile by kind, techset and source address
    std::unordered_map<BO4FFContext*, std::unordered_map<std::string, fastfile::linker::memory::LinkerDataChunk*>> shared{};

    class XAssetLinkerImpl : public XAssetLinker {
      public:
        using XAssetLinker::XAssetLinker;

        bool error{};
        size_t written{};
        size_t reused{};
        // the techset's file, the source addresses' scope
        std::string scope{};

        // the key of a block the source shared, empty when the dump has no id (never shared)
        std::string Key(char kind, const rapidjson::Value* id) {
            return id && id->IsString() ? std::format("{}:{}:{}", kind, scope, id->GetString()) : std::string{};
        }

        void Fail(BO4LinkContext& ctx, const std::string& what) {
            LOG_ERROR("{}", what);
            ctx.error = true;
            error = true;
        }

        // a block of a known size, or an offset to the same block written before
        void Block(BO4FFContext& ff, const std::string& key, const std::vector<byte>& data, uint64_t& slot) {
            auto& known{ shared[&ff] };
            if (auto it{ key.empty() ? known.end() : known.find(key) }; it != known.end()) {
                slot = 0;
                it->second->refs.push_back((void**)&slot);
                reused++;
                return;
            }
            slot = fastfile::linker::memory::POINTER_NEXT;
            ff.data.Align(8);
            fastfile::linker::memory::LinkerDataChunk* chunk{ ff.data.WriteStream(data.data(), data.size()) };
            if (!key.empty()) {
                known[key] = chunk;
            }
            written++;
        }

        // a program (0x28) and its blob (aligned 256), or an offset to the same program
        void Program(BO4FFContext& ff, const std::string& key, std::vector<byte> header, const std::vector<byte>& blob, uint64_t& slot) {
            *(uint32_t*)(header.data() + 0x20) = (uint32_t)blob.size();
            *(uint64_t*)(header.data() + 0x18) = blob.empty() ? 0 : fastfile::linker::memory::POINTER_NEXT;
            auto& known{ shared[&ff] };
            if (auto it{ key.empty() ? known.end() : known.find(key) }; it != known.end()) {
                slot = 0;
                it->second->refs.push_back((void**)&slot);
                reused++;
                return;
            }
            slot = fastfile::linker::memory::POINTER_NEXT;
            ff.data.Align(8);
            fastfile::linker::memory::LinkerDataChunk* chunk{ ff.data.WriteStream(header.data(), header.size()) };
            if (!key.empty()) {
                known[key] = chunk;
            }
            if (!blob.empty()) {
                ff.data.Align(256);
                ff.data.WriteStream(blob.data(), blob.size());
            }
            written++;
        }

        void State(BO4LinkContext& ctx, BO4FFContext& ff, const std::string& where, const rapidjson::Value& s) {
            std::vector<byte> header{ Hex(Member(s, "header")) };
            if (header.size() != STATE_SIZE) {
                Fail(ctx, std::format("{}: bad shader state", where));
                return;
            }
            std::vector<byte> decl{ Hex(Member(s, "decl")) };
            std::vector<byte> b0{ Hex(Member(s, "b0")) };
            if ((!decl.empty() && decl.size() != 0x78) || (!b0.empty() && b0.size() != 0xB0)) {
                Fail(ctx, std::format("{}: bad shader state block", where));
                return;
            }
            const rapidjson::Value* programs{ Member(s, "programs") };
            ff.data.Align(8);
            uint64_t* out{ ff.data.AllocStreamPtr<uint64_t>(STATE_SIZE / 8) };
            std::memcpy(out, header.data(), STATE_SIZE);
            for (size_t k = 0; k < 8; k++) {
                out[k] = 0;
            }
            if (!decl.empty()) {
                Block(ff, Key('d', Member(s, "declId")), decl, out[0]);
            }
            for (size_t k : PROGRAM_ORDER) {
                const rapidjson::Value* p{ programs && programs->IsArray() && k < programs->Size() ? &(*programs)[(rapidjson::SizeType)k] : nullptr };
                if (!p || !p->IsObject()) {
                    continue;
                }
                std::vector<byte> ph{ Hex(Member(*p, "header")) };
                if (ph.size() != PROGRAM_SIZE) {
                    Fail(ctx, std::format("{}: program {}: bad header", where, k));
                    return;
                }
                Program(ff, Key('p', Member(*p, "id")), ph, Hex(Member(*p, "blob")), out[k]);
            }
            if (!b0.empty()) {
                Block(ff, Key('b', Member(s, "b0Id")), b0, out[1]);
            }
        }

        void DrawMethod(BO4LinkContext& ctx, BO4FFContext& ff, const std::string& where, const rapidjson::Value& d, byte* dm) {
            std::vector<byte> args{ Hex(Member(d, "args")) };
            std::vector<byte> a20{ Hex(Member(d, "a20")) };
            const rapidjson::Value* state{ Member(d, "state") };
            const rapidjson::Value* s30{ Member(d, "s30") };
            if (!args.empty() && args.size() != 0xCull * ((size_t)dm[1] + dm[2])) {
                Fail(ctx, std::format("{}: args size {}", where, args.size()));
                return;
            }
            if (!a20.empty() && a20.size() != 8ull * dm[0x12]) {
                Fail(ctx, std::format("{}: 0x20 array size {}", where, a20.size()));
                return;
            }
            auto Next = [](bool has) { return has ? fastfile::linker::memory::POINTER_NEXT : (uint64_t)0; };
            *(uint64_t*)(dm + 0x18) = Next(!args.empty());
            *(uint64_t*)(dm + 0x20) = Next(!a20.empty());
            *(uint64_t*)(dm + 0x28) = Next(state && state->IsObject());
            *(uint64_t*)(dm + 0x30) = Next(s30 && s30->IsObject());
            if (!args.empty()) {
                ff.data.Align(4);
                ff.data.WriteStream(args.data(), args.size());
            }
            if (!a20.empty()) {
                ff.data.Align(4);
                ff.data.WriteStream(a20.data(), a20.size());
            }
            if (state && state->IsObject()) {
                State(ctx, ff, where, *state);
            }
            if (s30 && s30->IsObject()) {
                std::vector<byte> h{ Hex(Member(*s30, "header")) };
                std::vector<byte> data{ Hex(Member(*s30, "data")) };
                if (h.size() != 0x18 || (!data.empty() && data.size() != 0x2000)) {
                    Fail(ctx, std::format("{}: bad 0x30 block", where));
                    return;
                }
                *(uint64_t*)(h.data() + 0x10) = Next(!data.empty());
                ff.data.Align(8);
                ff.data.WriteStream(h.data(), h.size());
                if (!data.empty()) {
                    ff.data.WriteStream(data.data(), data.size());
                }
            }
        }

        void Compute(BO4LinkContext& ctx, const char* id, BO4FFContext& ff) override {
            std::filesystem::path path{ ctx.linkCtx.input / id };
            BO4Json cfg{ path };
            if (!cfg.SyncConfig(false)) {
                Fail(ctx, std::format("Can't read techset {}", path.string()));
                return;
            }
            const rapidjson::Value& root{ cfg.main };
            std::vector<byte> header{ Hex(Member(root, "header")) };
            const rapidjson::Value* data{ Member(root, "data") };
            if (header.size() != TECHSET_SIZE || !data || !data->IsArray() || data->Size() != 16) {
                Fail(ctx, std::format("techset {}: not a lossless dump", path.string()));
                return;
            }
            uint64_t name{ ctx.HashXHash(cfg.GetString("name", path.stem().string().c_str()), true) & 0x7FFFFFFFFFFFFFFFull };
            written = 0;
            reused = 0;
            scope = path.lexically_normal().string();

            ff.data.PushStream(XFILE_BLOCK_TEMP);
            ff.data.Align(8);
            byte* ts{ ff.data.AllocStreamPtr<byte>(TECHSET_SIZE) };
            std::memcpy(ts, header.data(), TECHSET_SIZE);
            *(uint64_t*)ts = name;
            for (size_t i = 0; i < 16; i++) {
                *(uint64_t*)(ts + 0x28 + 8 * i) = (*data)[(rapidjson::SizeType)i].IsObject() ? fastfile::linker::memory::POINTER_NEXT : 0;
            }

            ff.data.PushStream(XFILE_BLOCK_VIRTUAL);
            size_t techniques{};
            for (size_t i = 0; i < 16 && !error; i++) {
                const rapidjson::Value& t{ (*data)[(rapidjson::SizeType)i] };
                if (!t.IsObject()) {
                    continue;
                }
                std::string where{ std::format("techset {}: technique {}", path.filename().string(), i) };
                std::vector<byte> th{ Hex(Member(t, "header")) };
                if (th.size() != TECHNIQUE_SIZE) {
                    Fail(ctx, std::format("{}: bad header", where));
                    break;
                }
                std::vector<byte> f8{ Hex(Member(t, "f8")) };
                if (!f8.empty() && f8.size() != 8ull * th[0xF6]) {
                    Fail(ctx, std::format("{}: 0xF8 array size {}", where, f8.size()));
                    break;
                }
                ff.data.Align(8);
                byte* tech{ ff.data.AllocStreamPtr<byte>(TECHNIQUE_SIZE) };
                std::memcpy(tech, th.data(), TECHNIQUE_SIZE);
                *(uint64_t*)(tech + 0xF8) = f8.empty() ? 0 : fastfile::linker::memory::POINTER_NEXT;
                const rapidjson::Value* dms{ Member(t, "drawMethods") };
                for (size_t d = 0; d < 4 && !error; d++) {
                    byte* dm{ tech + 0x10 + DRAW_METHOD_SIZE * d };
                    const rapidjson::Value* dv{ dms && dms->IsArray() && d < dms->Size() ? &(*dms)[(rapidjson::SizeType)d] : nullptr };
                    if (!dv || !dv->IsObject()) {
                        for (size_t o : { (size_t)0x18, (size_t)0x20, (size_t)0x28, (size_t)0x30 }) {
                            *(uint64_t*)(dm + o) = 0;
                        }
                        continue;
                    }
                    DrawMethod(ctx, ff, std::format("{}: draw method {}", where, d), *dv, dm);
                }
                if (!f8.empty()) {
                    ff.data.Align(8);
                    ff.data.WriteStream(f8.data(), f8.size());
                }
                techniques++;
            }
            ff.data.PopStream();
            ff.data.PopStream();
            if (!error) {
                LOG_INFO("Added asset techset {} (hash_{:x}), lossless, {} technique(s), {} block(s) written, {} shared", path.filename().string(), name, techniques, written, reused);
            }
        }
    };

    utils::MapAdder<XAssetLinkerImpl, XAssetType, XAssetLinker> impl{ GetWorkers(), XAssetType::ASSET_TYPE_TECHNIQUE_SET };
} // namespace
