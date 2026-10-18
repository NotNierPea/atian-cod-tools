#include <includes.hpp>
#include <tools/fastfile/linkers/linker_bo4.hpp>

/*
 * Physics presets, the values of a physics prop (an xmodel's "physics": "physics/my_preset.json"). The defaults are the
 * game's common prop preset (hash_550b7918facda46d), the keys the bo4 physpreset unlinker's:
 *
 * {
 *     "name": "my_preset",
 *     "mass": 0.003,                 // the game's props: 0.003 to 0.03
 *     "bounce": 0.4,
 *     "friction": 1,
 *     "damping_linear": 1,
 *     "damping_angular": 1,
 *     "bulletForceScale": 0,         // a shot pushes with dynEnt_bulletForce (0.8) times it
 *     "explosiveForceScale": 0,      // the same with the explosions (dynEnt_explodeForce)
 *     "canFloat": false,
 *     "gravityScale": 1,
 *     "centerOfMassOffset": [0, 0, 0],
 *     "buoyancyBoxMin": [0, 0, 0],
 *     "buoyancyBoxMax": [0, 0, 0],
 *     "trailFX": "#fx",              // game assets by name
 *     "impactFX": "#impact_fx",
 *     "impactSounds": "#hash_51bb5e55b97a23ce"   // the common props' impact sounds
 * }
 */
namespace {
    using namespace fastfile::linker::bo4;

    struct PhysPreset {
        XHash name;
        int32_t flags;
        float mass;
        float bounce;
        float friction;
        float damping_linear;
        float damping_angular;
        float bulletForceScale;
        float explosiveForceScale;
        int32_t canFloat;
        float gravityScale;
        float centerOfMassOffset[3];
        float buoyancyBoxMin[3];
        float buoyancyBoxMax[3];
        void* trailFX;
        void* impactFX;
        void* impactSounds;
    };
    static_assert(sizeof(PhysPreset) == 0x78);

    constexpr const char* DEFAULT_IMPACT_SOUNDS = "#hash_51bb5e55b97a23ce";

    class XAssetLinkerImpl : public XAssetLinker {
      public:
        using XAssetLinker::XAssetLinker;

        void Compute(BO4LinkContext& ctx, const char* id, BO4FFContext& ff) override {
            std::filesystem::path path{ ctx.linkCtx.input / id };
            std::filesystem::path rfpath{ path.filename() };
            rfpath.replace_extension();

            BO4Json cfg{ path };
            if (!cfg.SyncConfig(false)) {
                LOG_ERROR("Can't read physpreset {}", path.string());
                ctx.error = true;
                return;
            }
            const rapidjson::Value& root{ cfg.main };

            auto Float = [&root](const char* name, float def) {
                auto it{ root.FindMember(name) };
                return it != root.MemberEnd() && it->value.IsNumber() ? (float)it->value.GetDouble() : def;
            };
            auto Vec = [&root](const char* name, float(&out)[3]) {
                auto it{ root.FindMember(name) };
                for (size_t i = 0; i < 3; i++) {
                    out[i] = it != root.MemberEnd() && it->value.IsArray() && i < it->value.Size() && it->value[(rapidjson::SizeType)i].IsNumber()
                        ? (float)it->value[(rapidjson::SizeType)i].GetDouble() : 0.0f;
                }
            };

            // Load_PhysPreset: the header, its assets by name
            ff.data.PushStream(XFILE_BLOCK_TEMP);
            ff.data.Align(8);
            PhysPreset& preset{ ff.data.AllocStreamRef<PhysPreset>() };
            preset = {};
            std::string assetName{ cfg.GetString("name", rfpath.string().c_str()) };
            preset.name.name = ctx.HashXHash(assetName, true);
            preset.flags = (int32_t)cfg.GetInteger("flags", 0);
            preset.mass = Float("mass", 0.003f);
            preset.bounce = Float("bounce", 0.4f);
            preset.friction = Float("friction", 1.0f);
            preset.damping_linear = Float("damping_linear", 1.0f);
            preset.damping_angular = Float("damping_angular", 1.0f);
            preset.bulletForceScale = Float("bulletForceScale", 0.0f);
            preset.explosiveForceScale = Float("explosiveForceScale", 0.0f);
            auto canFloat{ root.FindMember("canFloat") };
            preset.canFloat = canFloat != root.MemberEnd() && (canFloat->value.IsBool() ? canFloat->value.GetBool() : canFloat->value.IsInt() && canFloat->value.GetInt());
            preset.gravityScale = Float("gravityScale", 1.0f);
            Vec("centerOfMassOffset", preset.centerOfMassOffset);
            Vec("buoyancyBoxMin", preset.buoyancyBoxMin);
            Vec("buoyancyBoxMax", preset.buoyancyBoxMax);

            ff.data.PushStream(XFILE_BLOCK_VIRTUAL);
            auto LinkRef = [&](XAssetType type, const char* key, const char* def, void*& ref) {
                std::string val{ cfg.GetString(key, def) };
                if (val.empty()) {
                    ref = nullptr;
                    return;
                }
                std::string refId{ std::format(",{}", val[0] == '#' ? val.substr(1) : val) };
                ctx.LinkAsset(type, refId.data(), ref, false, &ff);
            };
            LinkRef(XAssetType::ASSET_TYPE_FX, "trailFX", "", preset.trailFX);
            LinkRef(XAssetType::ASSET_TYPE_IMPACT_FX, "impactFX", "", preset.impactFX);
            LinkRef(XAssetType::ASSET_TYPE_IMPACT_SOUND, "impactSounds", DEFAULT_IMPACT_SOUNDS, preset.impactSounds);
            ff.data.PopStream();

            ff.data.PopStream();
            LOG_INFO(
                "Added asset physpreset {} (hash_{:x}), mass {}, bounce {}, friction {}, bullet force x{}, explosive force x{}",
                assetName, preset.name.name, preset.mass, preset.bounce, preset.friction, preset.bulletForceScale, preset.explosiveForceScale
            );
        }
    };

    utils::MapAdder<XAssetLinkerImpl, XAssetType, XAssetLinker> impl{ GetWorkers(), XAssetType::ASSET_TYPE_PHYSPRESET };
} // namespace
