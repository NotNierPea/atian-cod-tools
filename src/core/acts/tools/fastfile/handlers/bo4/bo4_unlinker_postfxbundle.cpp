#include <includes.hpp>
#include <tools/fastfile/handlers/handler_game_bo4.hpp>

/*
 * Postfx bundles, the full screen effects the client scripts play (postfx::playpostfxbundle: zm_fasttravel's
 * pstfx_zm_wormhole). The retail layout, read off the loaded zones (zm_red's and zm_common's: their arrays one after
 * another as these counts say; the dev build's cod_Release.elf PostfxBundleDef is an older, other one):
 *
 * the bundle 0x38: its name (XHash), flags (0x10..0x1D), its stage count (u8 0x1E) and curve count (u8 0x1F), its
 * stages (0x20), its curves (0x28), 0x30 none in the game's;
 * a stage 0x30: its length (float 0x00), its material (0x08), flags (0x10..0x27), its constant count (u8 0x25), its
 * constants (0x28);
 * a constant 0x20: its name (XHash 0x08: the material's constant it drives), its curves and channels (0x18..);
 * a curve 0x10: a kind (u8 0x00), its key count (u8 0x01), its keys (0x08), each key 0x18.
 *
 * In the zone: the bundle, then (virtual) its stages, each stage's material (by name) and constants, its curves, each
 * curve's keys. The json is the linker's (linker_bo4_postfxbundle.cpp): the plain bytes as hex, its pointers apart.
 */
namespace {
    using namespace fastfile::handlers::bo4;

    struct PostfxCurve {
        uint8_t kind;
        uint8_t numKeys;
        uint8_t pad[6];
        byte* keys;
    };
    static_assert(sizeof(PostfxCurve) == 0x10);

    struct PostfxStage {
        float length;
        uint32_t pad04;
        void* material;
        byte flags[0x15];
        uint8_t numConsts;
        byte flags26[2];
        byte* constant;
    };
    static_assert(sizeof(PostfxStage) == 0x30);

    struct PostfxBundleDef {
        XHash name;
        byte flags[0xE];
        uint8_t numStages;
        uint8_t numCurves;
        PostfxStage* stage;
        PostfxCurve* curve;
        uint64_t unk30;
    };
    static_assert(sizeof(PostfxBundleDef) == 0x38);

    constexpr size_t CONSTANT_SIZE{ 0x20 };
    constexpr size_t KEY_SIZE{ 0x18 };

    std::string Hex(const void* ptr, size_t len) {
        std::string out{};
        for (size_t i = 0; i < len; i++) {
            out += std::format("{:02x}", ((const byte*)ptr)[i]);
        }
        return out;
    }

    class ImplWorker : public Worker {
        using Worker::Worker;

        void Unlink(fastfile::FastFileOption& opt, void* ptr) {
            PostfxBundleDef* asset{ (PostfxBundleDef*)ptr };

            std::filesystem::path outFile{ opt.m_output / "bo4" / "source" / "tables" / "postfxbundle" /
                                           fastfile::GetCurrentContext().ffname /
                                           std::format("{}.json", hashutils::ExtractTmp("file", asset->name)) };
            std::filesystem::create_directories(outFile.parent_path());
            BO4JsonWriter json{};

            // (the layout's check: the arrays one after another as the counts say)
            byte* next{ (byte*)asset->stage + sizeof(PostfxStage) * asset->numStages };
            bool contiguous{ true };
            for (size_t i = 0; asset->stage && i < asset->numStages; i++) {
                PostfxStage& s{ asset->stage[i] };
                if (s.constant) {
                    contiguous &= s.constant == next;
                    next = s.constant + CONSTANT_SIZE * s.numConsts;
                }
            }
            if (asset->curve) {
                contiguous &= (byte*)asset->curve == next;
                next = (byte*)asset->curve + sizeof(PostfxCurve) * asset->numCurves;
                for (size_t i = 0; i < asset->numCurves; i++) {
                    PostfxCurve& c{ asset->curve[i] };
                    if (c.keys) {
                        contiguous &= c.keys == next;
                        next = c.keys + KEY_SIZE * c.numKeys;
                    }
                }
            }
            LOG_OPT_INFO(
                "Dump postfxbundle {} ({} stages, {} curves{})", outFile.string(), asset->numStages, asset->numCurves,
                contiguous ? "" : ", NOT CONTIGUOUS: another layout"
            );
            if (!contiguous || asset->unk30) {
                LOG_WARNING("postfxbundle {}: its layout isn't the one known (unk30 {:x})", outFile.string(), asset->unk30);
            }

            json.BeginObject();
            json.WriteFieldValueXHash("name", asset->name);
            json.WriteFieldValueString("flags", Hex(asset->flags, sizeof(asset->flags)));
            json.WriteFieldNameString("stages");
            json.BeginArray();
            for (size_t i = 0; asset->stage && i < asset->numStages; i++) {
                PostfxStage& s{ asset->stage[i] };
                json.BeginObject();
                json.WriteFieldValueNumber("length", s.length);
                json.WriteFieldValueXAsset("material", games::bo4::pool::ASSET_TYPE_MATERIAL, s.material);
                json.WriteFieldValueString("flags", Hex(s.flags, sizeof(s.flags)));
                json.WriteFieldValueString("flags26", Hex(s.flags26, sizeof(s.flags26)));
                json.WriteFieldNameString("constants");
                json.BeginArray();
                for (size_t j = 0; s.constant && j < s.numConsts; j++) {
                    byte* c{ s.constant + CONSTANT_SIZE * j };
                    json.BeginObject();
                    json.WriteFieldValueXHash("name", *(XHash*)(c + 8));
                    json.WriteFieldValueString("raw", Hex(c, CONSTANT_SIZE));
                    json.EndObject();
                }
                json.EndArray();
                json.EndObject();
            }
            json.EndArray();
            json.WriteFieldNameString("curves");
            json.BeginArray();
            for (size_t i = 0; asset->curve && i < asset->numCurves; i++) {
                PostfxCurve& c{ asset->curve[i] };
                json.BeginObject();
                json.WriteFieldValueNumber("kind", c.kind);
                json.WriteFieldValueString("pad", Hex(c.pad, sizeof(c.pad)));
                if (c.keys) {
                    json.WriteFieldNameString("keys");
                    json.BeginArray();
                    for (size_t j = 0; j < c.numKeys; j++) {
                        json.WriteValueString(Hex(c.keys + KEY_SIZE * j, KEY_SIZE));
                    }
                    json.EndArray();
                }
                json.EndObject();
            }
            json.EndArray();
            json.EndObject();

            if (!json.WriteToFile(outFile)) {
                LOG_ERROR("Error when dumping {}", outFile.string());
            }
        }
    };

    utils::MapAdder<ImplWorker, games::bo4::pool::XAssetType, Worker> impl{
        GetWorkers(), games::bo4::pool::XAssetType::ASSET_TYPE_POSTFXBUNDLE
    };
} // namespace
