#include <includes.hpp>
#include <tools/fastfile/handlers/handler_game_bo4.hpp>
#include <tools/fastfile/handlers/bo4/bo4_unlinker_scriptbundle.hpp>
#include <core/hashes/raw_file_extractor.hpp>
#include <tools/utils/data_utils.hpp>

namespace {
    using namespace fastfile::handlers::bo4;
    using namespace games::bo4::pool;
    struct Ragdoll;

    // see: 6969A0     ; void __fastcall Scr_function_d7c3cf6c(scriptInstance_t inst)

    struct PlayerOutfitUnk68 {
        XHash desc;
        bool unk10;
        XHash unk18;
        GfxImage* image;
        ScriptBundle* scriptbundle;
        XModel* model;
        uint32_t unk48_count;
        Material** unk48;
        uint64_t unk50;
    };
    static_assert(sizeof(PlayerOutfitUnk68) == 0x58);

    struct PlayerOutfitUnk78 {
        XHash desc;
        bool unk10;
        XHash unk18;
        GfxImage* image;
        XModel* model;
        uint32_t unk40_count;
        Material** unk40;
        uint32_t unk50_count;
        Material** unk50;
        uint64_t unk58;
    };
    static_assert(sizeof(PlayerOutfitUnk78) == 0x60);

    struct PlayerOutfit {
        XHash name;
        uint64_t unk10;
        XHash desc;
        GfxImage* unk28;
        uint64_t unk30;
        PlayerFXTable* playerFXTable;
        TagFxSet* tagFX;
        Ragdoll* ragdoll;
        FxEffectDef* fxEffect;
        ScrString_t tag;
        uint32_t unk5c;
        uint32_t unk68_count;
        uint32_t unk64;
        PlayerOutfitUnk68* unk68;
        uint32_t unk78_count;
        uint32_t unk74;
        PlayerOutfitUnk78* unk78;
        uint32_t unk88_count;
        uint32_t unk84;
        void* unk88;
        uint32_t unk98_count;
        uint32_t unk94;
        void* unk98;
        uint32_t unka8_count;
        uint32_t unka4;
        void* unka8;
        uint32_t unkb8_count;
        uint32_t unkb4;
        void* unkb8;
        uint32_t unkc8_count;
        uint32_t unkc4;
        void* unkc8;
        uint32_t unkd8_count;
        uint32_t unkd4;
        void* unkd8;
        uint32_t unke8_count;
        uint32_t unke4;
        void* unke8;
        uint32_t unkf8_count;
        uint32_t unkf4;
        void* unkf8;
    };
    static_assert(sizeof(PlayerOutfit) == 0x100);

    class ImplWorker : public Worker {
        void Unlink(fastfile::FastFileOption& opt, void* ptr) {

            PlayerOutfit& asset{ *(PlayerOutfit*)ptr };

            std::filesystem::path outFile{ opt.m_output / "bo4" / "source" / "tables" / "player" / "outfit" /
                                           std::format("{}.json", hashutils::ExtractTmp("file", asset.name.name)) };
            std::filesystem::create_directories(outFile.parent_path());

            BO4JsonWriter json{};

            LOG_OPT_INFO("Dump playeroutfit {}", outFile.string());

            json.BeginObject();

            json.WriteFieldValueXHash("name", asset.name);
            json.WriteFieldValueXHash("desc", asset.desc);
            json.WriteFieldValueXAsset("unk28", ASSET_TYPE_IMAGE, asset.unk28);
            json.WriteFieldValueScrString("tag", asset.tag);
            json.WriteFieldValueXAsset("playerFXTable", ASSET_TYPE_PLAYERFX, asset.playerFXTable);
            json.WriteFieldValueXAsset("tagFX", ASSET_TYPE_TAGFX, asset.tagFX);
            json.WriteFieldValueXAsset("fxEffect", ASSET_TYPE_FX, asset.fxEffect);
            json.WriteFieldValueXAsset("ragdoll", ASSET_TYPE_RAGDOLL, asset.ragdoll);
            json.WriteFieldValueUnknown("unk10", asset.unk10);
            json.WriteFieldValueUnknown("unk30", asset.unk30);

            if (asset.unk68) {
                json.WriteFieldNameString("unk68");
                json.BeginArray();
                for (size_t i = 0; i < asset.unk68_count; i++) {
                    PlayerOutfitUnk68& obj{ asset.unk68[i] };
                    json.BeginObject();
                    json.WriteFieldValueXHash("desc", obj.desc);
                    json.WriteFieldValueXHash("unk68:unk18", obj.unk18);
                    json.WriteFieldValueBool("unk68:unk10", obj.unk10);
                    json.WriteFieldValueXAsset("image", ASSET_TYPE_IMAGE, obj.image);
                    json.WriteFieldValueXAsset("scriptbundle", ASSET_TYPE_SCRIPTBUNDLE, obj.scriptbundle);
                    json.WriteFieldValueXAsset("model", ASSET_TYPE_XMODEL, obj.model);
                    json.WriteFieldValueXAssetArray("unk68:unk48", ASSET_TYPE_MATERIAL, obj.unk48_count, obj.unk48);
                    json.WriteFieldValueNumber("unk68:unk50", obj.unk50);
                    json.EndObject();
                }
                json.EndArray();
            }

            if (asset.unk78) {
                json.WriteFieldNameString("unk78");
                json.BeginArray();
                for (size_t i = 0; i < asset.unk78_count; i++) {
                    PlayerOutfitUnk78& obj{ asset.unk78[i] };
                    json.BeginObject();
                    json.WriteFieldValueXHash("desc", obj.desc);
                    json.WriteFieldValueXHash("unk78:unk18", obj.unk18);
                    json.WriteFieldValueBool("unk78:unk10", obj.unk10);
                    json.WriteFieldValueXAsset("image", ASSET_TYPE_IMAGE, obj.image);
                    json.WriteFieldValueXAsset("model", ASSET_TYPE_XMODEL, obj.model);
                    json.WriteFieldValueXAssetArray("unk78:unk48", ASSET_TYPE_MATERIAL, obj.unk40_count, obj.unk40);
                    json.WriteFieldValueXAssetArray("unk78:unk48", ASSET_TYPE_MATERIAL, obj.unk50_count, obj.unk50);
                    json.WriteFieldValueUnknown("unk78:unk58", obj.unk58);
                    json.EndObject();
                }
                json.EndArray();
            }
            // the 0x88 items (0x78, sub_142E14F70): an image at 0x28, a model alias at 0x30, 7 models from 0x38
            if (asset.unk88) {
                json.WriteFieldNameString("unk88");
                json.BeginArray();
                for (size_t i = 0; i < asset.unk88_count; i++) {
                    byte* obj{ (byte*)asset.unk88 + 0x78 * i };
                    json.BeginObject();
                    json.WriteFieldValueXHash("desc", *(XHash*)obj);
                    json.WriteFieldValueXAsset("image", ASSET_TYPE_IMAGE, *(GfxImage**)(obj + 0x28));
                    void* alias{ *(void**)(obj + 0x30) };
                    if (alias) {
                        json.WriteFieldValueXHash("alias", *(XHash*)alias);
                    }
                    json.WriteFieldNameString("models");
                    json.BeginArray();
                    for (size_t k = 0; k < 7; k++) {
                        void* m{ *(void**)(obj + 0x38 + 8 * k) };
                        if (m) {
                            json.WriteValueString(std::format("#hash_{:x}", *(uint64_t*)m & 0x7FFFFFFFFFFFFFFFull));
                        } else {
                            json.WriteValueNull();
                        }
                    }
                    json.EndArray();
                    json.EndObject();
                }
                json.EndArray();
            }
            // the 0x98 items (0xA8, sub_142E14F70): an image at 0x28, 14 models from 0x30
            // the 0xA8 items (0x78): like the 0x88 ones, an image at 0x28, 7 models from 0x38
            auto writeModels = [&json](const char* field, void* items, uint32_t count, size_t size, size_t models, size_t nmodels) {
                if (!items) {
                    return;
                }
                json.WriteFieldNameString(field);
                json.BeginArray();
                for (size_t i = 0; i < count; i++) {
                    byte* obj{ (byte*)items + size * i };
                    json.BeginObject();
                    json.WriteFieldValueXHash("desc", *(XHash*)obj);
                    json.WriteFieldValueXAsset("image", ASSET_TYPE_IMAGE, *(GfxImage**)(obj + 0x28));
                    json.WriteFieldNameString("models");
                    json.BeginArray();
                    for (size_t k = 0; k < nmodels; k++) {
                        void* m{ *(void**)(obj + models + 8 * k) };
                        if (m) {
                            json.WriteValueString(std::format("#hash_{:x}", *(uint64_t*)m & 0x7FFFFFFFFFFFFFFFull));
                        } else {
                            json.WriteValueNull();
                        }
                    }
                    json.EndArray();
                    json.EndObject();
                }
                json.EndArray();
            };
            writeModels("unk98", asset.unk98, asset.unk98_count, 0xA8, 0x30, 14);
            writeModels("unka8", asset.unka8, asset.unka8_count, 0x78, 0x38, 7);
            json.EndObject();

            if (!json.WriteToFile(outFile)) {
                LOG_ERROR("Error when dumping {}", outFile.string());
            }
        }
    };

    utils::MapAdder<ImplWorker, XAssetType, Worker> impl{ GetWorkers(), XAssetType::ASSET_TYPE_PLAYER_OUTFIT };
} // namespace