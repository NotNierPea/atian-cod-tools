#include <includes.hpp>
#include <tools/fastfile/linkers/linker_bo4.hpp>

/*
 * Beams (BeamDef, asset type 99: the tubes and arcs effects draw, an fx element of type 14 naming one), the bo4 beam
 * unlinker's json as it is. The retail layout (its loader, sub_142E11E70 in the retail dump), 0x4D8 bytes:
 *
 * 0x000 name (XHash); 0x010, 0x018 materials; curves (a pointer, its key count a u32 at +8, 8-byte keys, 4 aligned) at
 * BEAM_CURVES' offsets; 0x3F8..0x420 six effects; the rest plain values (0x438, 0x448 XHashes).
 *
 * In the zone (the loader's order): the header (temp), then (virtual) the two materials, the first twelve curves, the
 * six effects, the last four curves. Materials and effects by name: the zone links them before.
 *
 * {
 *     "name": "beam8_zm_teleport_path",
 *     "raw": "<0x4D8 bytes>",                       // its pointers and counts are set from the rest
 *     "materials": { "10": "#hash_fc7f05ac1436141" },
 *     "fx": { "418": "#hash_2ec73246ddf52378" },
 *     "curves": { "68": "<keys: 8 bytes each>" }
 * }
 */
namespace {
    using namespace fastfile::linker::bo4;

    constexpr size_t BEAM_SIZE{ 0x4D8 };
    constexpr size_t BEAM_MATERIALS[]{ 0x10, 0x18 };
    constexpr size_t BEAM_CURVES[]{ 0x68, 0x80, 0xC0, 0xD8, 0x120, 0x138, 0x168, 0x180, 0x198, 0x1B0, 0x200, 0x210,
                                    0x470, 0x488, 0x4A0, 0x4B8 };
    // the curves the loader reads before the effects (the rest after them)
    constexpr size_t BEAM_CURVES_BEFORE_FX{ 12 };
    constexpr size_t BEAM_FX[]{ 0x3F8, 0x400, 0x408, 0x410, 0x418, 0x420 };

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

    // an object's member by an offset key ("10", "3f8"), or null
    const rapidjson::Value* AtOffset(const rapidjson::Value* obj, size_t off) {
        if (!obj || !obj->IsObject()) {
            return nullptr;
        }
        std::string key{ std::format("{:x}", off) };
        return Member(*obj, key.c_str());
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
                LOG_ERROR("Can't read beam {}", path.string());
                ctx.error = true;
                return;
            }
            const rapidjson::Value& root{ cfg.main };
            auto Fail = [&](const std::string& why) {
                LOG_ERROR("beam {}: {}", path.string(), why);
                ctx.error = true;
            };
            std::vector<byte> raw{};
            if (!ReadHex(Member(root, "raw"), raw) || raw.size() != BEAM_SIZE) {
                Fail(std::format("raw takes 0x{:x} bytes of hex", BEAM_SIZE));
                return;
            }
            const rapidjson::Value* materials{ Member(root, "materials") };
            const rapidjson::Value* fx{ Member(root, "fx") };
            const rapidjson::Value* curves{ Member(root, "curves") };
            std::vector<std::vector<byte>> keys(std::size(BEAM_CURVES));
            for (size_t i = 0; i < std::size(BEAM_CURVES); i++) {
                const rapidjson::Value* c{ AtOffset(curves, BEAM_CURVES[i]) };
                if (c && (!ReadHex(c, keys[i]) || keys[i].size() % 8)) {
                    Fail(std::format("curve {:x}: takes 8 bytes of hex a key", BEAM_CURVES[i]));
                    return;
                }
            }

            // the header (Load_BeamDef's)
            ff.data.PushStream(XFILE_BLOCK_TEMP);
            ff.data.Align(8);
            byte* def{ ff.data.AllocStreamPtr<byte>(BEAM_SIZE) };
            std::memcpy(def, raw.data(), BEAM_SIZE);
            std::string assetName{ cfg.GetString("name", rfpath.string().c_str()) };
            const uint64_t nameHash{ ctx.HashXHash(assetName, true) };
            *(uint64_t*)def = nameHash;
            *(uint64_t*)(def + 8) = 0;
            for (size_t off : BEAM_MATERIALS) {
                *(uint64_t*)(def + off) = 0;
            }
            for (size_t off : BEAM_FX) {
                *(uint64_t*)(def + off) = 0;
            }
            for (size_t i = 0; i < std::size(BEAM_CURVES); i++) {
                *(uint64_t*)(def + BEAM_CURVES[i]) = keys[i].empty() ? 0 : fastfile::linker::memory::POINTER_NEXT;
                *(uint32_t*)(def + BEAM_CURVES[i] + 8) = (uint32_t)(keys[i].size() / 8);
            }

            ff.data.PushStream(XFILE_BLOCK_VIRTUAL);
            // (a by-name reference each: ",<name>")
            auto Ref = [&](XAssetType type, const rapidjson::Value* v, size_t off) {
                if (!v || !v->IsString() || !v->GetStringLength()) {
                    return;
                }
                std::string ref{ v->GetString() };
                std::string refId{ std::format(",{}", ref[0] == '#' ? ref.substr(1) : ref) };
                ctx.LinkAsset(type, refId.data(), *(void**)(def + off), false, &ff);
            };
            auto Curves = [&](size_t from, size_t to) {
                for (size_t i = from; i < to; i++) {
                    if (!keys[i].empty()) {
                        ff.data.Align(4);
                        byte* k{ ff.data.AllocStreamPtr<byte>(keys[i].size()) };
                        std::memcpy(k, keys[i].data(), keys[i].size());
                    }
                }
            };
            size_t numRefs{};
            for (size_t off : BEAM_MATERIALS) {
                Ref(XAssetType::ASSET_TYPE_MATERIAL, AtOffset(materials, off), off);
                numRefs += AtOffset(materials, off) != nullptr;
            }
            Curves(0, BEAM_CURVES_BEFORE_FX);
            for (size_t off : BEAM_FX) {
                Ref(XAssetType::ASSET_TYPE_FX, AtOffset(fx, off), off);
                numRefs += AtOffset(fx, off) != nullptr;
            }
            Curves(BEAM_CURVES_BEFORE_FX, std::size(BEAM_CURVES));
            ff.data.PopStream();
            ff.data.PopStream();
            size_t numCurves{};
            for (auto& k : keys) {
                numCurves += !k.empty();
            }
            LOG_INFO("Added asset beam {} (hash_{:x}), {} curves, {} materials and effects", assetName, nameHash, numCurves, numRefs);
        }
    };

    utils::MapAdder<XAssetLinkerImpl, XAssetType, XAssetLinker> impl{ GetWorkers(), XAssetType::ASSET_TYPE_BEAM };
} // namespace
