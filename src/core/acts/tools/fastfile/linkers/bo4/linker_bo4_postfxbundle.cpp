#include <includes.hpp>
#include <tools/fastfile/linkers/linker_bo4.hpp>

/*
 * Postfx bundles (postfx::playpostfxbundle on the client), the bo4 postfxbundle unlinker's json as it is: its plain
 * bytes as hex, its pointers apart. The retail layout (the unlinker's, read off the loaded zones):
 *
 * the bundle 0x38: name (XHash), flags 0x10..0x1D, stage count (u8 0x1E), curve count (u8 0x1F), stages (0x20), curves
 * (0x28), 0x30 none; a stage 0x30: length (float), material (0x08), flags 0x10..0x24, constant count (u8 0x25), 0x26..
 * 0x27, constants (0x28); a constant 0x20 (its name, an XHash at 0x08); a curve 0x10: kind (u8), key count (u8), 6
 * bytes, keys (0x08), a key 0x18.
 *
 * In the zone: the bundle (temp), then (virtual) its stages, each stage's material (by name) and constants, its curves,
 * each curve's keys.
 *
 * {
 *     "name": "pstfx_zm_wormhole",
 *     "flags": "0000000001010001000001000002",                    // 14 bytes
 *     "stages": [ { "length": 0.1, "material": "#hash_16026d20e691b59d", "flags": "00...", "flags26": "0000",
 *                   "constants": [ { "name": "#hash_7510748a05c0520d", "raw": "<0x20 bytes>" } ] } ],
 *     "curves": [ { "kind": 1, "pad": "<6 bytes>", "keys": [ "<0x18 bytes>", ... ] } ]
 * }
 */
namespace {
    using namespace fastfile::linker::bo4;

    struct PostfxCurve {
        uint8_t kind;
        uint8_t numKeys;
        uint8_t pad[6];
        uintptr_t keys;
    };
    static_assert(sizeof(PostfxCurve) == 0x10);

    struct PostfxStage {
        float length;
        uint32_t pad04;
        void* material;
        byte flags[0x15];
        uint8_t numConsts;
        byte flags26[2];
        uintptr_t constant;
    };
    static_assert(sizeof(PostfxStage) == 0x30);

    struct PostfxBundleDef {
        XHash name;
        byte flags[0xE];
        uint8_t numStages;
        uint8_t numCurves;
        uintptr_t stage;
        uintptr_t curve;
        uint64_t unk30;
    };
    static_assert(sizeof(PostfxBundleDef) == 0x38);

    constexpr size_t CONSTANT_SIZE{ 0x20 };
    constexpr size_t KEY_SIZE{ 0x18 };

    // a hex string into out (exactly len bytes), false when it isn't one
    bool ReadHex(const rapidjson::Value* v, byte* out, size_t len) {
        if (!v || !v->IsString() || v->GetStringLength() != len * 2) {
            return false;
        }
        const char* s{ v->GetString() };
        for (size_t i = 0; i < len; i++) {
            auto nib = [](char c) { return c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1; };
            int hi{ nib(s[i * 2]) }, lo{ nib(s[i * 2 + 1]) };
            if (hi < 0 || lo < 0) {
                return false;
            }
            out[i] = (byte)(hi << 4 | lo);
        }
        return true;
    }

    const rapidjson::Value* Member(const rapidjson::Value& v, const char* name) {
        if (!v.IsObject()) {
            return nullptr;
        }
        auto it{ v.FindMember(name) };
        return it == v.MemberEnd() ? nullptr : &it->value;
    }

    class XAssetLinkerImpl : public XAssetLinker {
      public:
        using XAssetLinker::XAssetLinker;

        void Compute(BO4LinkContext& ctx, const char* id, BO4FFContext& ff) override {
            std::filesystem::path path{ ctx.linkCtx.input / id };
            std::filesystem::path rfpath{ path.filename() };
            rfpath.replace_extension();

            BO4Json cfg{ path };
            if (!cfg.SyncConfig(false)) {
                LOG_ERROR("Can't read postfxbundle {}", path.string());
                ctx.error = true;
                return;
            }
            const rapidjson::Value& root{ cfg.main };
            auto Fail = [&](const std::string& why) {
                LOG_ERROR("postfxbundle {}: {}", path.string(), why);
                ctx.error = true;
            };
            const rapidjson::Value* stages{ Member(root, "stages") };
            const rapidjson::Value* curves{ Member(root, "curves") };
            if ((stages && !stages->IsArray()) || (curves && !curves->IsArray())) {
                Fail("stages and curves take arrays");
                return;
            }
            size_t numStages{ stages ? stages->Size() : 0 };
            size_t numCurves{ curves ? curves->Size() : 0 };
            if (numStages > 0xFF || numCurves > 0xFF) {
                Fail("at most 255 stages and curves");
                return;
            }

            // the bundle (Load_PostfxBundleDef's)
            ff.data.PushStream(XFILE_BLOCK_TEMP);
            ff.data.Align(8);
            PostfxBundleDef& def{ ff.data.AllocStreamRef<PostfxBundleDef>() };
            def = {};
            std::string assetName{ cfg.GetString("name", rfpath.string().c_str()) };
            const uint64_t nameHash{ ctx.HashXHash(assetName, true) };
            def.name.name = nameHash;
            if (!ReadHex(Member(root, "flags"), def.flags, sizeof(def.flags))) {
                Fail("flags takes 14 bytes of hex");
                ff.data.PopStream();
                return;
            }
            def.numStages = (uint8_t)numStages;
            def.numCurves = (uint8_t)numCurves;
            def.stage = numStages ? fastfile::linker::memory::POINTER_NEXT : 0;
            def.curve = numCurves ? fastfile::linker::memory::POINTER_NEXT : 0;

            ff.data.PushStream(XFILE_BLOCK_VIRTUAL);
            size_t numConstants{};
            if (numStages) {
                ff.data.Align(8);
                PostfxStage* st{ ff.data.AllocStreamPtr<PostfxStage>(numStages) };
                for (size_t i = 0; i < numStages; i++) {
                    const rapidjson::Value& s{ (*stages)[(rapidjson::SizeType)i] };
                    PostfxStage& out{ st[i] };
                    out = {};
                    const rapidjson::Value* len{ Member(s, "length") };
                    out.length = len && len->IsNumber() ? (float)len->GetDouble() : 0.0f;
                    if (!ReadHex(Member(s, "flags"), out.flags, sizeof(out.flags)) ||
                        !ReadHex(Member(s, "flags26"), out.flags26, sizeof(out.flags26))) {
                        Fail(std::format("stage {}: flags takes 21 bytes of hex, flags26 2", i));
                        break;
                    }
                    // its material, by name
                    const rapidjson::Value* mat{ Member(s, "material") };
                    if (mat && mat->IsString() && mat->GetStringLength()) {
                        std::string ref{ mat->GetString() };
                        std::string refId{ std::format(",{}", ref[0] == '#' ? ref.substr(1) : ref) };
                        ctx.LinkAsset(XAssetType::ASSET_TYPE_MATERIAL, refId.data(), out.material, false, &ff);
                    }
                    // its constants
                    const rapidjson::Value* consts{ Member(s, "constants") };
                    size_t n{ consts && consts->IsArray() ? consts->Size() : 0 };
                    if (n > 0xFF) {
                        Fail(std::format("stage {}: at most 255 constants", i));
                        break;
                    }
                    out.numConsts = (uint8_t)n;
                    if (n) {
                        out.constant = fastfile::linker::memory::POINTER_NEXT;
                        ff.data.Align(8);
                        byte* c{ ff.data.AllocStreamPtr<byte>(CONSTANT_SIZE * n) };
                        for (size_t j = 0; j < n; j++) {
                            const rapidjson::Value& cv{ (*consts)[(rapidjson::SizeType)j] };
                            if (!ReadHex(Member(cv, "raw"), c + CONSTANT_SIZE * j, CONSTANT_SIZE)) {
                                Fail(std::format("stage {} constant {}: raw takes 32 bytes of hex", i, j));
                                break;
                            }
                            // (its name: the json's, else the raw one)
                            const rapidjson::Value* nm{ Member(cv, "name") };
                            if (nm && nm->IsString()) {
                                *(uint64_t*)(c + CONSTANT_SIZE * j + 8) = ctx.HashXHash(nm->GetString(), true);
                            }
                        }
                        numConstants += n;
                    }
                }
            }
            size_t numKeys{};
            if (numCurves && !ctx.error) {
                ff.data.Align(8);
                PostfxCurve* cu{ ff.data.AllocStreamPtr<PostfxCurve>(numCurves) };
                for (size_t i = 0; i < numCurves; i++) {
                    const rapidjson::Value& c{ (*curves)[(rapidjson::SizeType)i] };
                    PostfxCurve& out{ cu[i] };
                    out = {};
                    const rapidjson::Value* kind{ Member(c, "kind") };
                    out.kind = kind && kind->IsInt() ? (uint8_t)kind->GetInt() : 1;
                    if (!ReadHex(Member(c, "pad"), out.pad, sizeof(out.pad))) {
                        std::memset(out.pad, 0, sizeof(out.pad));
                    }
                    const rapidjson::Value* keys{ Member(c, "keys") };
                    size_t n{ keys && keys->IsArray() ? keys->Size() : 0 };
                    if (n > 0xFF) {
                        Fail(std::format("curve {}: at most 255 keys", i));
                        break;
                    }
                    out.numKeys = (uint8_t)n;
                    if (n) {
                        out.keys = fastfile::linker::memory::POINTER_NEXT;
                        ff.data.Align(8);
                        byte* k{ ff.data.AllocStreamPtr<byte>(KEY_SIZE * n) };
                        for (size_t j = 0; j < n; j++) {
                            if (!ReadHex(&(*keys)[(rapidjson::SizeType)j], k + KEY_SIZE * j, KEY_SIZE)) {
                                Fail(std::format("curve {} key {}: takes 24 bytes of hex", i, j));
                                break;
                            }
                        }
                        numKeys += n;
                    }
                }
            }
            ff.data.PopStream();
            ff.data.PopStream();
            LOG_INFO(
                "Added asset postfxbundle {} (hash_{:x}), {} stages, {} constants, {} curves, {} keys", assetName, nameHash,
                numStages, numConstants, numCurves, numKeys
            );
        }
    };

    utils::MapAdder<XAssetLinkerImpl, XAssetType, XAssetLinker> impl{ GetWorkers(), XAssetType::ASSET_TYPE_POSTFXBUNDLE };
} // namespace
