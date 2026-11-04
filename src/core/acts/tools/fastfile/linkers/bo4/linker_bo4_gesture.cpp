#include <includes.hpp>
#include <tools/fastfile/linkers/linker_bo4.hpp>

/*
 * Gestures (GestureDef, asset type 18) and gesture tables (type 19), the bo4 gesture unlinker's json as it is. The
 * retail layouts (their loaders in the retail dump):
 *
 * GestureDef, 0x208 bytes (sub_142E1BC90): 0x000 name (XHash); 0x020..0x150 38 xanims; the rest plain values.
 * GestureTable, 0x68 bytes (sub_142E1C270): 0x000 name (XHash); 0x010..0x060 10 gestures; 0x060 8 plain bytes.
 *
 * In the zone (the loaders' order): the header (temp), then (virtual) the references in order, each by name: the zone
 * links the xanims before the gestures and the gestures before the tables.
 *
 * {
 *     "name": "gestable_grapple_idle",
 *     "raw": "<0x68 bytes>",                  // its pointers are set from the rest
 *     "gestures": { "10": "#hash_..." }       // a gesture's: "xanims": { "20": "#hash_..." }
 * }
 */
namespace {
    using namespace fastfile::linker::bo4;

    bool ReadHex(const rapidjson::Value* v, std::vector<byte>& out) {
        out.clear();
        if (!v || !v->IsString() || (v->GetStringLength() & 1)) {
            return false;
        }
        const char* s{ v->GetString() };
        auto nib = [](char c) { return c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1; };
        for (size_t i = 0; i < v->GetStringLength(); i += 2) {
            int hi{ nib(s[i]) }, lo{ nib(s[i + 1]) };
            if (hi < 0 || lo < 0) {
                return false;
            }
            out.push_back((byte)(hi << 4 | lo));
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

    // one asset: its header, its references by name (refsCount pointers from refs)
    void LinkOne(BO4LinkContext& ctx, const char* id, BO4FFContext& ff, const char* kind, size_t size, const char* refsName,
                 XAssetType refType, size_t refs, size_t refsCount) {
        std::filesystem::path path{ ctx.linkCtx.input / id };
        std::filesystem::path rfpath{ path.filename() };
        rfpath.replace_extension();

        BO4Json cfg{ path };
        if (!cfg.SyncConfig(false)) {
            LOG_ERROR("Can't read {} {}", kind, path.string());
            ctx.error = true;
            return;
        }
        const rapidjson::Value& root{ cfg.main };
        std::vector<byte> raw{};
        if (!ReadHex(Member(root, "raw"), raw) || raw.size() != size) {
            LOG_ERROR("{} {}: raw takes 0x{:x} bytes of hex", kind, path.string(), size);
            ctx.error = true;
            return;
        }
        const rapidjson::Value* names{ Member(root, refsName) };

        ff.data.PushStream(XFILE_BLOCK_TEMP);
        ff.data.Align(8);
        byte* def{ ff.data.AllocStreamPtr<byte>(size) };
        std::memcpy(def, raw.data(), size);
        std::string assetName{ cfg.GetString("name", rfpath.string().c_str()) };
        const uint64_t nameHash{ ctx.HashXHash(assetName, true) };
        *(uint64_t*)def = nameHash;
        *(uint64_t*)(def + 8) = 0;
        for (size_t i = 0; i < refsCount; i++) {
            *(uint64_t*)(def + refs + i * 8) = 0;
        }

        ff.data.PushStream(XFILE_BLOCK_VIRTUAL);
        size_t numRefs{};
        for (size_t i = 0; i < refsCount; i++) {
            size_t off{ refs + i * 8 };
            std::string key{ std::format("{:x}", off) };
            const rapidjson::Value* v{ names ? Member(*names, key.c_str()) : nullptr };
            if (!v || !v->IsString() || !v->GetStringLength()) {
                continue;
            }
            std::string ref{ v->GetString() };
            std::string refId{ std::format(",{}", ref[0] == '#' ? ref.substr(1) : ref) };
            ctx.LinkAsset(refType, refId.data(), *(void**)(def + off), false, &ff);
            numRefs++;
        }
        ff.data.PopStream();
        ff.data.PopStream();
        LOG_INFO("Added asset {} {} (hash_{:x}), {} {}", kind, assetName, nameHash, numRefs, refsName);
    }

    class GestureLinker : public XAssetLinker {
      public:
        using XAssetLinker::XAssetLinker;

        void Compute(BO4LinkContext& ctx, const char* id, BO4FFContext& ff) override {
            LinkOne(ctx, id, ff, "gesture", 0x208, "xanims", XAssetType::ASSET_TYPE_XANIM, 0x20, 38);
        }
    };

    class GestureTableLinker : public XAssetLinker {
      public:
        using XAssetLinker::XAssetLinker;

        void Compute(BO4LinkContext& ctx, const char* id, BO4FFContext& ff) override {
            LinkOne(ctx, id, ff, "gesturetable", 0x68, "gestures", XAssetType::ASSET_TYPE_GESTURE, 0x10, 10);
        }
    };

    utils::MapAdder<GestureLinker, XAssetType, XAssetLinker> implGesture{ GetWorkers(), XAssetType::ASSET_TYPE_GESTURE };
    utils::MapAdder<GestureTableLinker, XAssetType, XAssetLinker> implTable{ GetWorkers(), XAssetType::ASSET_TYPE_GESTURE_TABLE };
} // namespace
