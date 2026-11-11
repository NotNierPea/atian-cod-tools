#include <includes.hpp>
#include <tools/fastfile/linkers/linker_bo4.hpp>
#include <tools/fastfile/linkers/bo4/linker_bo4_scriptbundle.hpp>

/*
 * Sprays and gestures of the wheel: items (sprayorgestureitem, asset type 0xA5) and the list (sprayorgesturelist,
 * 0xA6), the bo4 sprayorgesture unlinker's json. The retail layouts (their loaders in the retail dump):
 *
 * SprayOrGestureItem, 0x60 (sub_142E2BB40): 0x00 name, 0x10 type (XHash "gesture" or "tag"), 0x20 the type's data,
 * 0x40 its bundle (SB_ObjectsArray, sub_142E296C0). A gesture's data (sub_142E1BE60): 0x20 gesture, 0x28 gesture2 (the
 * one for role templates whose +0x30 +0xB8 is 1), u32 0x30 sub count, 0x38 subs (0x20: gesture, gesture2, u32 role
 * count, roles XHash[]: per role ones). A tag's: 0x20 its material.
 * SprayOrGestureList, 0x20 (sub_142E2BD00): 0x00 name, u32 0x10 count, 0x18 the items.
 *
 * In the zone: the header (temp), then (virtual) the gestures (by name), the subs, the bundle; a list's items by name
 * (the game's list is core_common's #hash_7c6bb932b4895a9c, read by item index: the wheel's slots).
 *
 * { "name": "my_gesture", "type": "gesture", "gesture": "ges_x", "gesture2": "ges_x", "bundle": { ... } }
 * { "name": "#hash_7c6bb932b4895a9c", "items": [ "#hash_...", "my_gesture" ] }
 */
namespace {
    using namespace fastfile::linker::bo4;

    const rapidjson::Value* Member(const rapidjson::Value& v, const char* name) {
        if (!v.IsObject()) {
            return nullptr;
        }
        auto it{ v.FindMember(name) };
        return it == v.MemberEnd() ? nullptr : &it->value;
    }

    std::string RefId(const char* name) {
        return std::format(",{}", name[0] == '#' ? name + 1 : name);
    }

    class SprayOrGestureItemLinker : public XAssetLinker {
      public:
        using XAssetLinker::XAssetLinker;

        void Compute(BO4LinkContext& ctx, const char* id, BO4FFContext& ff) override {
            std::filesystem::path path{ ctx.linkCtx.input / id };
            std::filesystem::path rfpath{ path.filename() };
            rfpath.replace_extension();

            BO4Json cfg{ path };
            if (!cfg.SyncConfig(false)) {
                LOG_ERROR("Can't read sprayorgestureitem {}", path.string());
                ctx.error = true;
                return;
            }
            rapidjson::Value& root{ cfg.main };
            std::string assetName{ cfg.GetString("name", rfpath.string().c_str()) };
            std::string type{ cfg.GetString("type", "gesture") };
            bool gesture{ type == "gesture" || type == "#gesture" };
            bool tag{ type == "tag" || type == "#tag" };
            if (!gesture && !tag) {
                LOG_ERROR("sprayorgestureitem {}: type {} isn't gesture or tag", path.string(), type);
                ctx.error = true;
                return;
            }

            ff.data.PushStream(XFILE_BLOCK_TEMP);
            ff.data.Align(8);
            byte* item{ ff.data.AllocStreamPtr<byte>(0x60) };
            std::memset(item, 0, 0x60);
            const uint64_t nameHash{ ctx.HashXHash(assetName, true) };
            *(uint64_t*)item = nameHash;
            *(uint64_t*)(item + 0x10) = ctx.HashXHash(type[0] == '#' ? type.substr(1) : type);

            ff.data.PushStream(XFILE_BLOCK_VIRTUAL);
            // the type's references by name, in the loader's order
            auto Ref = [&](const char* key, size_t off, XAssetType refType) {
                const rapidjson::Value* v{ Member(root, key) };
                if (!v || !v->IsString() || !v->GetStringLength()) {
                    return;
                }
                std::string refId{ RefId(v->GetString()) };
                ctx.LinkAsset(refType, refId.data(), *(void**)(item + off), false, &ff);
            };
            if (gesture) {
                Ref("gesture", 0x20, XAssetType::ASSET_TYPE_GESTURE);
                Ref("gesture2", 0x28, XAssetType::ASSET_TYPE_GESTURE);
                if (const rapidjson::Value* subs{ Member(root, "sub") }; subs && subs->IsArray() && subs->Size()) {
                    LOG_ERROR("sprayorgestureitem {}: per role gestures (sub) aren't linked yet", path.string());
                    ctx.error = true;
                }
            } else {
                Ref("material", 0x20, XAssetType::ASSET_TYPE_MATERIAL);
            }

            // its bundle (display name, icon, isdefault...)
            if (rapidjson::Value* bundle{ (rapidjson::Value*)Member(root, "bundle") }; bundle && bundle->IsObject()) {
                auto& array{ *(scriptbundle::SB_ObjectsArray*)(item + 0x40) };
                if (!scriptbundle::WriteArray(ctx, *bundle, array, ff)) {
                    LOG_ERROR("sprayorgestureitem {}: bad bundle", path.string());
                    ctx.error = true;
                }
            }
            ff.data.PopStream();
            ff.data.PopStream();
            LOG_INFO("Added asset sprayorgestureitem {} (hash_{:x}), type {}", assetName, nameHash, type);
        }
    };

    class SprayOrGestureListLinker : public XAssetLinker {
      public:
        using XAssetLinker::XAssetLinker;

        void Compute(BO4LinkContext& ctx, const char* id, BO4FFContext& ff) override {
            std::filesystem::path path{ ctx.linkCtx.input / id };
            std::filesystem::path rfpath{ path.filename() };
            rfpath.replace_extension();

            BO4Json cfg{ path };
            if (!cfg.SyncConfig(false)) {
                LOG_ERROR("Can't read sprayorgesturelist {}", path.string());
                ctx.error = true;
                return;
            }
            const rapidjson::Value* items{ Member(cfg.main, "items") };
            if (!items || !items->IsArray()) {
                LOG_ERROR("sprayorgesturelist {}: no items", path.string());
                ctx.error = true;
                return;
            }
            std::string assetName{ cfg.GetString("name", rfpath.string().c_str()) };

            ff.data.PushStream(XFILE_BLOCK_TEMP);
            ff.data.Align(8);
            byte* list{ ff.data.AllocStreamPtr<byte>(0x20) };
            std::memset(list, 0, 0x20);
            const uint64_t nameHash{ ctx.HashXHash(assetName[0] == '#' ? assetName.substr(1) : assetName, true) };
            *(uint64_t*)list = nameHash;
            *(uint32_t*)(list + 0x10) = (uint32_t)items->Size();

            ff.data.PushStream(XFILE_BLOCK_VIRTUAL);
            if (items->Size()) {
                *(void**)(list + 0x18) = (void*)fastfile::linker::memory::POINTER_NEXT;
                ff.data.Align(8);
                void** refs{ ff.data.AllocStreamPtr<void*>(items->Size()) };
                size_t i{};
                for (const rapidjson::Value& v : items->GetArray()) {
                    refs[i] = nullptr;
                    if (v.IsString() && v.GetStringLength()) {
                        std::string refId{ RefId(v.GetString()) };
                        ctx.LinkAsset(XAssetType::ASSET_TYPE_SPRAYORGESTUREITEM, refId.data(), refs[i], false, &ff);
                    }
                    i++;
                }
            }
            ff.data.PopStream();
            ff.data.PopStream();
            LOG_INFO("Added asset sprayorgesturelist {} (hash_{:x}), {} items", assetName, nameHash, items->Size());
        }
    };

    utils::MapAdder<SprayOrGestureItemLinker, XAssetType, XAssetLinker> implItem{ GetWorkers(), XAssetType::ASSET_TYPE_SPRAYORGESTUREITEM };
    utils::MapAdder<SprayOrGestureListLinker, XAssetType, XAssetLinker> implList{ GetWorkers(), XAssetType::ASSET_TYPE_SPRAYORGESTURELIST };
} // namespace
