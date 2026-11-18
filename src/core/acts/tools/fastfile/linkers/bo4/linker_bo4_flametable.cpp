#include <includes.hpp>
#include <tools/fastfile/linkers/linker_bo4.hpp>

/*
 * Flame tables, a flamethrower's flameTableFirstPerson/ThirdPerson (the weapon links them by name). The json is the
 * bo4 flametable unlinker's: the 0x240 header as hex, the name, 8 materials by name, 4 sounds:
 *
 * {
 *     "name": "#hash_7c9e63bac4fc46ba",
 *     "header": "...",
 *     "fire": "#material", "smoke": ..., "heat": ..., "drips": ...,
 *     "streamFuel": ..., "streamFuel2": ..., "streamFlame": ..., "streamFlame2": ...,
 *     "flameOffLoopSound": "#alias", "flameIgniteSound": ..., "flameOnLoopSound": ..., "flameCooldownSound": ...
 * }
 *
 * The loader (sub_142E19730): the header, then in the virtual block the 8 materials (each a temp material pointer).
 */
namespace {
    using namespace fastfile::linker::bo4;

    constexpr size_t FLAMETABLE_SIZE = 0x240;
    constexpr size_t FLAMETABLE_NAME = 0x1B0;
    constexpr size_t FLAMETABLE_MATERIALS = 0x1C0;
    constexpr size_t FLAMETABLE_SOUNDS = 0x200;
    const char* MATERIAL_NAMES[]{ "fire", "smoke", "heat", "drips", "streamFuel", "streamFuel2", "streamFlame", "streamFlame2" };
    const char* SOUND_NAMES[]{ "flameOffLoopSound", "flameIgniteSound", "flameOnLoopSound", "flameCooldownSound" };

    std::vector<byte> Unhex(const char* s, size_t len) {
        std::vector<byte> out(len / 2);
        auto Nib = [](char c) -> byte { return (byte)(c <= '9' ? c - '0' : (c | 0x20) - 'a' + 10); };
        for (size_t i = 0; i < out.size(); i++) {
            out[i] = (byte)(Nib(s[2 * i]) << 4 | Nib(s[2 * i + 1]));
        }
        return out;
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
                LOG_ERROR("Can't read flametable {}", path.string());
                ctx.error = true;
                return;
            }
            const rapidjson::Value& root{ cfg.main };
            auto Str = [&root](const char* key) -> std::string {
                auto it{ root.FindMember(key) };
                return it != root.MemberEnd() && it->value.IsString() ? it->value.GetString() : "";
            };

            std::string hex{ Str("header") };
            if (hex.size() != FLAMETABLE_SIZE * 2) {
                LOG_ERROR("flametable {}: the header isn't 0x{:x} bytes", path.string(), FLAMETABLE_SIZE);
                ctx.error = true;
                return;
            }
            std::vector<byte> header{ Unhex(hex.data(), hex.size()) };

            // Load_FlameTable: the header (8 aligned)
            ff.data.PushStream(XFILE_BLOCK_TEMP);
            ff.data.Align(8);
            byte* a{ ff.data.AllocStreamPtr<byte>(FLAMETABLE_SIZE) };
            std::memcpy(a, header.data(), FLAMETABLE_SIZE);
            std::string assetName{ Str("name") };
            if (assetName.empty()) {
                assetName = rfpath.string();
            }
            uint64_t nameHash{ ctx.HashXHash(assetName, true) };
            ((XHash*)(a + FLAMETABLE_NAME))->name = nameHash;
            for (size_t i = 0; i < 4; i++) {
                std::string s{ Str(SOUND_NAMES[i]) };
                ((XHash*)(a + FLAMETABLE_SOUNDS))[i].name = s.empty() ? 0 : ctx.HashXHash(s, true);
            }

            // the materials by name
            ff.data.PushStream(XFILE_BLOCK_VIRTUAL);
            void** mats{ (void**)(a + FLAMETABLE_MATERIALS) };
            size_t linked{};
            for (size_t i = 0; i < 8; i++) {
                mats[i] = nullptr;
                std::string m{ Str(MATERIAL_NAMES[i]) };
                if (m.empty()) {
                    continue;
                }
                std::string refId{ std::format(",{}", m[0] == '#' ? m.substr(1) : m) };
                ctx.LinkAsset(XAssetType::ASSET_TYPE_MATERIAL, refId.data(), mats[i], false, &ff);
                linked++;
            }
            ff.data.PopStream();

            ff.data.PopStream();
            LOG_INFO("Added asset flametable {} (hash_{:x}), {} materials", assetName, nameHash, linked);
        }
    };

    utils::MapAdder<XAssetLinkerImpl, XAssetType, XAssetLinker> impl{ GetWorkers(), XAssetType::ASSET_TYPE_FLAMETABLE };
} // namespace
