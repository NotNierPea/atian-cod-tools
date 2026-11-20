#include <includes.hpp>
#include <tools/fastfile/linkers/linker_bo4.hpp>

/*
 * com_map (ComWorld, retail 0x60, the loader sub_142E16C10, the layout in bo4_unlinker_map_com.cpp): a map's lights and
 * light probes as the server knows them. Com_LoadWorld wants isInUse set. No exploders, sun volumes or probes.
 *
 * The primary lights (0x10/0x18, 0x258 each on 16, VIRTUAL, no pointers but an XHash at 0x220): the map's static lights,
 * which cgame copies into its scene lights at the start (R_InitPrimaryLights, at most 3000). The renderer draws a sun
 * volume's own list of them (the lighting states' 0x10A0/0x10A8, linker_bo4_map_lighting.cpp's "primaryLights"), each
 * one boxed (0x1D0/0x1DC) against the view, its umbraId (0x1F0) 0 (not in an umbra tome). The record is the effects'
 * light description's (GfxConfig_Light, its 0x10 name off): type 0x40 (4 ball, 2 box), guid 0x44, origin 0x48, wldDir
 * 0x54 (back toward the light), rvec 0x60, uvec 0x6C, the axis 0x78 (rvec, uvec, -wldDir), colour 0xA8 (linear, times its
 * intensity), cosHalfFov 0xBC (-1 an omni), cut_on 0xC4, radius 0xCC, bulb 0xD8, the shadow 0x110 (quality, 1 none),
 * 0x116 (spot shadows) and 0x120 (update), the cookie 0x128-0x1A7, lightStateMask 0x1B4, the cull falloff and cutoff
 * 0x1B8/0x1BC, cullRadius 0x1C0, its box's middle 0x1C4, min 0x1D0, max 0x1DC, umbraId 0x1F0, exploderDisabled 0x218.
 * "primaryLightsRaw": the records as the dumper writes them (hex).
 *
 * {
 *     "name": "maps/zm/enh_test.d3dbsp",   // maps/<mode>/<map>.d3dbsp
 *     "primaryLightsRaw": [ "<0x258 bytes hex>", ... ]
 * }
 */
namespace {
    using namespace fastfile::linker::bo4;

    constexpr size_t PRIMARY_LIGHT_SIZE = 0x258;
    constexpr size_t PRIMARY_LIGHTS_MAX = 3000;

    struct ComWorld {
        XHash name;
        bool isInUse;
        uint32_t primaryLightCount;
        void* primaryLights;
        uint32_t lightExploderCount;
        void* lightExploders;
        uint32_t sunVolumeCount;
        uint32_t* sunVolumeProbeCount;
        uint32_t probeCount;
        uint32_t* probeGuids;
        uint32_t probeExploderCount;
        void* probeExploders;
    };
    static_assert(sizeof(ComWorld) == 0x60);

    bool ReadHex(const char* str, size_t len, std::vector<byte>& out) {
        if (len & 1) {
            return false;
        }
        auto Nibble = [](char c) -> int {
            if (c >= '0' && c <= '9') return c - '0';
            if (c >= 'a' && c <= 'f') return c - 'a' + 10;
            if (c >= 'A' && c <= 'F') return c - 'A' + 10;
            return -1;
        };
        for (size_t i = 0; i < len; i += 2) {
            int hi{ Nibble(str[i]) };
            int lo{ Nibble(str[i + 1]) };
            if (hi < 0 || lo < 0) {
                return false;
            }
            out.push_back((byte)(hi << 4 | lo));
        }
        return true;
    }

    class XAssetLinkerImpl : public XAssetLinker {
      public:
        using XAssetLinker::XAssetLinker;

        void Compute(BO4LinkContext& ctx, const char* id, BO4FFContext& ff) override {
            std::filesystem::path path{ ctx.linkCtx.input / id };
            BO4Json cfg{ path };
            if (!cfg.SyncConfig(false)) {
                LOG_ERROR("Can't read com_map {}", path.string());
                ctx.error = true;
                return;
            }
            std::string assetName{ cfg.GetString("name", "") };
            if (assetName.empty()) {
                LOG_ERROR("com_map {}: no name (maps/<mode>/<map>.d3dbsp)", path.string());
                ctx.error = true;
                return;
            }

            std::vector<byte> lights{};
            auto lightsIt{ cfg.main.FindMember("primaryLightsRaw") };
            if (lightsIt != cfg.main.MemberEnd() && lightsIt->value.IsArray()) {
                for (auto& v : lightsIt->value.GetArray()) {
                    size_t start{ lights.size() };
                    if (!v.IsString() || !ReadHex(v.GetString(), v.GetStringLength(), lights) || lights.size() - start != PRIMARY_LIGHT_SIZE) {
                        LOG_ERROR("com_map {}: primary light {} isn't 0x{:x} bytes of hex", path.string(), start / PRIMARY_LIGHT_SIZE, PRIMARY_LIGHT_SIZE);
                        ctx.error = true;
                        return;
                    }
                    // (no string behind the XHash)
                    std::memset(lights.data() + start + 0x228, 0, 8);
                }
            }
            const size_t lightCount{ lights.size() / PRIMARY_LIGHT_SIZE };
            if (lightCount > PRIMARY_LIGHTS_MAX) {
                LOG_ERROR("com_map {}: {} primary lights, cgame takes {}", path.string(), lightCount, PRIMARY_LIGHTS_MAX);
                ctx.error = true;
                return;
            }

            ff.data.PushStream(XFILE_BLOCK_TEMP);
            ff.data.Align(8);
            ComWorld& com{ ff.data.AllocStreamRef<ComWorld>() };
            com = {};
            com.name.name = ctx.HashXHash(assetName, true);
            com.isInUse = true;
            if (lightCount) {
                com.primaryLightCount = (uint32_t)lightCount;
                com.primaryLights = (void*)fastfile::linker::memory::POINTER_NEXT;
            }
            // the loader's arrays in VIRTUAL (DB_PushStreamPos(4))
            ff.data.PushStream(XFILE_BLOCK_VIRTUAL);
            if (lightCount) {
                ff.data.Align(16);
                ff.data.WriteStream(lights.data(), lights.size());
            }
            ff.data.PopStream();
            ff.data.PopStream();

            LOG_INFO("Added asset com_map {} (hash_{:x}), {} primary light(s)", assetName, ctx.HashXHash(assetName, true), lightCount);
        }
    };

    utils::MapAdder<XAssetLinkerImpl, XAssetType, XAssetLinker> impl{ GetWorkers(), XAssetType::ASSET_TYPE_COMWORLD };
} // namespace
