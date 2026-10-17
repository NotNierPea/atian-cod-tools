#include <includes.hpp>
#include <tools/fastfile/handlers/handler_game_bo4.hpp>

namespace {
    constexpr uint32_t MaterialHash(const char* name) {
        uint32_t h{};

        while (*name) {
            h = (33 * h) ^ (*(name++) | 0x20);
        }

        return h;
    }

    struct MaterialHashLookup {
        std::unordered_map<uint32_t, const char*> map{};

        void Init() {
            if (!map.empty())
                return;

            static const char* names[]{
                "colorMap",       "detailMap",     "glossMap",         "heightMap",     "thermalHeatmap",
                "normalMap",      "revealMap",     "specColorMap",     "tintMask",      "$white_ao",
                "$white_diffuse", "$white_gloss",  "$white_reveal",    "detailNormal1", "detailNormal2",
                "detailNormal3",  "detailNormal4", "detailNormalMask", "aoMap",         "lookupTexture",
                "heatLookup",     "transGlossMap", "tintMask",         "glossBodyMap",  "thicknessMap",
                "diffuseMap",     "edgeFadeMap",
            };

            for (const char* n : names) {
                map[MaterialHash(n)] = n;
            }
        }
    } hashLookup;

    const char* LookupHash(uint32_t hash) {
        hashLookup.Init();
        auto it{ hashLookup.map.find(hash) };
        if (it == hashLookup.map.end())
            return nullptr;
        return it->second;
    }

    std::string Hex(const void* data, size_t len) {
        static const char* digits{ "0123456789abcdef" };
        std::string out{};
        out.reserve(len * 2);
        for (size_t i = 0; i < len; i++) {
            byte b{ ((const byte*)data)[i] };
            out += digits[b >> 4];
            out += digits[b & 0xF];
        }
        return out;
    }

    using namespace fastfile::handlers::bo4;
    class ImplWorker : public Worker {
        using Worker::Worker;

        std::unordered_set<uint32_t> unktypes{};

        void PreXFileLoading(fastfile::FastFileOption& opt, fastfile::FastFileContext& ctx) override {
            unktypes.clear();
        }
        void PostXFileLoading(fastfile::FastFileOption& opt, fastfile::FastFileContext& ctx) override {
            if (unktypes.size()) {
                std::filesystem::path outFile{ opt.m_output / "bo4" / "graphics" /
                                               std::format("unktext_{}.txt", ctx.ffname) };
                std::filesystem::create_directories(outFile.parent_path());
                utils::OutFileCE os{ outFile, true };

                for (uint32_t h : unktypes) {
                    os << std::hex << "0x" << h << "\n";
                }
            }
        }
        void Unlink(fastfile::FastFileOption& opt, void* ptr) override {
            struct MaterialTechniqueSet;

            // 8 blocks of 0x40 bytes
            struct MaterialConstantSub {
                byte* blocks[8];
            };
            static_assert(sizeof(MaterialConstantSub) == 0x40);

            struct MaterialUnkA0 {
                uint64_t unk0;
                uint64_t unk8;
                uint64_t unk10;
                uint64_t unk18;
                uint64_t unk20;
                uint64_t unk28;
                MaterialConstantSub* unk30[12];
                uint64_t datalen;
                byte* data;
            };
            static_assert(sizeof(MaterialUnkA0) == 0xa0);

            struct MaterialTextureDef {
                GfxImage* image;
                uint32_t nameHash;
                vec2_t uvScale;
                byte texSizeShift[2];
                uint16_t uvOffset[3];
                uint16_t usage;
                byte isMatureContent;
            };
            static_assert(sizeof(MaterialTextureDef) == 0x20);

            struct MaterialBindingImage {
                GfxImage* image;
                uint64_t unk8;
            };

            struct MaterialBinding {
                uint32_t* data;
                MaterialBindingImage* images;
                uint32_t dataCount;
                uint32_t imageCount;
            };
            static_assert(sizeof(MaterialBinding) == 0x18);

            struct Material {
                XHash name;
                uint64_t unk10;
                uint64_t unk18;
                uint64_t unk20;
                uint64_t unk28;
                MaterialTechniqueSet* techniqueSet;
                MaterialTextureDef* textureTable;
                uint64_t* samplerTable;
                uint64_t unk48;
                MaterialBinding* bindings[8];
                MaterialUnkA0 unk90;
                byte textureCount;
                byte samplerCount;
                byte unk132;
                byte unk133;
            };
            static_assert(sizeof(Material) == 0x138);

            Material* asset{ (Material*)ptr };

            std::filesystem::path outFile{ opt.m_output / "bo4" / "graphics" / "material" /
                                           std::format("{}.json", hashutils::ExtractTmp("file", asset->name)) };
            std::filesystem::create_directories(outFile.parent_path());
            BO4JsonWriter json{};

            LOG_OPT_INFO("Dump material {}", outFile.string());

            auto WriteImage = [&json](const char* name, GfxImage* img) {
                if (img) {
                    json.WriteFieldValueXAsset(name, games::bo4::pool::XAssetType::ASSET_TYPE_IMAGE, img);
                }
            };

            // a template for the bo4 material linker: the blocks as they are, the assets by name
            json.BeginObject();
            json.WriteFieldValueXHash("name", asset->name);
            json.WriteFieldValueXAsset(
                "techniqueSet",
                games::bo4::pool::XAssetType::ASSET_TYPE_TECHNIQUE_SET,
                asset->techniqueSet
            );
            json.WriteFieldValueString("header", Hex(asset, sizeof(*asset)));

            json.WriteFieldNameString("textureTable");
            json.BeginArray();
            for (size_t i = 0; i < asset->textureCount; i++) {
                MaterialTextureDef* def{ asset->textureTable + i };
                json.BeginObject();
                WriteImage("image", def->image);
                const char* h{ LookupHash(def->nameHash) };
                if (!h) {
                    unktypes.insert(def->nameHash);
                    h = utils::va("unk_%x", def->nameHash);
                }
                json.WriteFieldValueString("type", h);
                json.WriteFieldValueString("def", Hex(def, sizeof(*def)));
                json.EndObject();
            }
            json.EndArray();

            if (asset->samplerTable && asset->samplerCount) {
                json.WriteFieldValueString("samplers", Hex(asset->samplerTable, 8ull * asset->samplerCount));
            }

            json.WriteFieldNameString("constants");
            json.BeginObject();
            json.WriteFieldNameString("subs");
            json.BeginArray();
            for (MaterialConstantSub* sub : asset->unk90.unk30) {
                if (!sub) {
                    json.WriteValueNull();
                    continue;
                }
                json.BeginArray();
                for (byte* block : sub->blocks) {
                    if (block) {
                        json.WriteValueString(Hex(block, 0x40));
                    } else {
                        json.WriteValueNull();
                    }
                }
                json.EndArray();
            }
            json.EndArray();
            if (asset->unk90.data && asset->unk90.datalen) {
                json.WriteFieldValueString("data", Hex(asset->unk90.data, asset->unk90.datalen));
            }
            json.EndObject();

            json.WriteFieldNameString("bindings");
            json.BeginArray();
            for (MaterialBinding* bind : asset->bindings) {
                if (!bind) {
                    json.WriteValueNull();
                    continue;
                }
                json.BeginObject();
                json.WriteFieldValueString("block", Hex(bind, sizeof(*bind)));
                if (bind->data && bind->dataCount) {
                    json.WriteFieldValueString("data", Hex(bind->data, 4ull * bind->dataCount));
                }
                json.WriteFieldNameString("images");
                json.BeginArray();
                for (size_t i = 0; bind->images && i < bind->imageCount; i++) {
                    json.BeginObject();
                    WriteImage("image", bind->images[i].image);
                    json.WriteFieldValueString("entry", Hex(&bind->images[i], sizeof(bind->images[i])));
                    json.EndObject();
                }
                json.EndArray();
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
        GetWorkers(), games::bo4::pool::XAssetType::ASSET_TYPE_MATERIAL, true
    };
} // namespace
