#include <includes.hpp>
#include <tools/fastfile/linkers/linker_bo4.hpp>
#include <tools/fastfile/bo4_ai_tables.hpp>

/*
 * The AI tables (bo4_ai_tables.hpp): animmappingtable, animselectortable, animstatemachine, behaviortree,
 * behaviorstatemachine, footsteptable and surfacesounddef, from the bo4 unlinker's json. An aitype links them by name
 * ("behaviorTree": "#hash_13a7f138b939d440"); the file is the name or tables/<type>/<name>.json:
 *
 * tables/behaviortree/hash_13a7f138b939d440.json
 * tables/animmappingtable/hash_13b1ea38b9421ce0.json
 *
 * The json has the struct's fields by name, the arrays as lists and "unk_<offset>" lists of the 4 byte words no field
 * names. The sub assets (a node's behaviorstatemachine, the footsteptable's surfacesounddefs) are inline objects.
 */
namespace {
    using namespace fastfile::linker::bo4;
    using namespace fastfile::bo4ai;

    // the inline assets of each fastfile by type, by name
    std::unordered_map<BO4FFContext*, std::unordered_map<XAssetType, std::unordered_set<uint64_t>>> inlineAssets{};

    class XAssetLinkerImpl : public XAssetLinker {
        XAssetType type;

      public:
        XAssetLinkerImpl(XAssetType type) : XAssetLinker(), type(type) {}

        void WriteFields(BO4LinkContext& ctx, BO4FFContext& ff, const Struct& s, byte* p, const rapidjson::Value& v) {
            auto FieldValue = [&v, &s](const Field& f) -> const rapidjson::Value* {
                if (s.flat) {
                    return &v;
                }
                if (!v.IsObject()) {
                    return nullptr;
                }
                auto it{ v.FindMember(f.name) };
                return it == v.MemberEnd() ? nullptr : &it->value;
            };

            // the values first, then the data in the loader's order
            for (const Field& f : s.fields) {
                const rapidjson::Value* fv{ FieldValue(f) };
                if (!fv || fv->IsNull()) {
                    continue;
                }
                byte* d{ p + f.offset };
                switch (f.kind) {
                case FK_XHASH:
                    if (fv->IsString()) {
                        *(uint64_t*)d = ctx.HashXHash(fv->GetString(), true);
                    }
                    break;
                case FK_SCR:
                    if (fv->IsString()) {
                        *(uint32_t*)d = ff.data.AddScrString(fv->GetString());
                    }
                    break;
                case FK_I32: *(int32_t*)d = fv->IsNumber() ? fv->GetInt() : 0; break;
                case FK_U32: *(uint32_t*)d = fv->IsNumber() ? fv->GetUint() : 0; break;
                case FK_F32: *(float*)d = fv->IsNumber() ? (float)fv->GetDouble() : 0; break;
                case FK_U8: *d = fv->IsNumber() ? (byte)fv->GetUint() : 0; break;
                case FK_BOOL: *d = fv->IsBool() ? fv->GetBool() : fv->IsNumber() && fv->GetInt(); break;
                default: break;
                }
            }
            if (!s.flat && v.IsObject()) {
                for (auto m{ v.MemberBegin() }; m != v.MemberEnd(); ++m) {
                    const char* key{ m->name.GetString() };
                    if (std::strncmp(key, "unk_", 4) || !m->value.IsArray()) {
                        continue;
                    }
                    uint32_t off{ (uint32_t)std::strtoul(key + 4, nullptr, 16) };
                    for (const rapidjson::Value& w : m->value.GetArray()) {
                        if (off + 4 > s.size) {
                            LOG_ERROR("{}: {} past the end", s.name, key);
                            ctx.error = true;
                            break;
                        }
                        *(uint32_t*)(p + off) = w.IsNumber() ? (uint32_t)w.GetUint64() : 0;
                        off += 4;
                    }
                }
            }

            for (const Field& f : s.fields) {
                const rapidjson::Value* fv{ FieldValue(f) };
                byte* d{ p + f.offset };
                if (f.kind == FK_ARRAY) {
                    if (!fv || !fv->IsArray() || fv->Empty()) {
                        continue;
                    }
                    uint32_t count{ fv->Size() };
                    *(uintptr_t*)d = fastfile::linker::memory::POINTER_NEXT;
                    *(int32_t*)(p + f.countOffset) = (int32_t)count;
                    ff.data.Align(f.align);
                    byte* arr{ ff.data.AllocStreamPtr<byte>((size_t)f.sub->size * count) };
                    std::memset(arr, 0, (size_t)f.sub->size * count);
                    for (uint32_t i = 0; i < count; i++) {
                        WriteFields(ctx, ff, *f.sub, arr + (size_t)i * f.sub->size, (*fv)[i]);
                    }
                } else if (f.kind == FK_ASSET) {
                    for (uint32_t i = 0; i < f.fixedCount; i++) {
                        const rapidjson::Value* av{ fv };
                        if (av && f.fixedCount > 1) {
                            av = av->IsArray() && i < av->Size() ? &(*av)[i] : nullptr;
                        }
                        uintptr_t& ref{ ((uintptr_t*)d)[i] };
                        if (!av || !av->IsObject()) {
                            ref = 0;
                            continue;
                        }
                        // an asset already inline in the fastfile by its name (the loader adds each inline one: twice the same
                        // was two assets of a name, the game's zones name the second)
                        if (const rapidjson::Value* nv{ av->HasMember("name") ? &(*av)["name"] : nullptr }; nv && nv->IsString() && *nv->GetString()) {
                            uint64_t nameHash{ ctx.HashXHash(nv->GetString(), true) };
                            if (!inlineAssets[&ff][f.assetType].insert(nameHash).second) {
                                ctx.LinkAsset(f.assetType, std::format(",hash_{:x}", nameHash).c_str(), reinterpret_cast<void*&>(ref), false, &ff);
                                continue;
                            }
                        }
                        // inline: the header in temp, its data in virtual
                        ref = fastfile::linker::memory::POINTER_NEXT;
                        ff.data.PushStream(XFILE_BLOCK_TEMP);
                        ff.data.Align(8);
                        byte* hdr{ ff.data.AllocStreamPtr<byte>(f.sub->size) };
                        std::memset(hdr, 0, f.sub->size);
                        ff.data.PushStream(XFILE_BLOCK_VIRTUAL);
                        WriteFields(ctx, ff, *f.sub, hdr, *av);
                        ff.data.PopStream();
                        ff.data.PopStream();
                    }
                }
            }
        }

        void Compute(BO4LinkContext& ctx, const char* id, BO4FFContext& ff) override {
            const AssetSchema* schema{ FindSchema(type) };
            std::filesystem::path path{ ctx.linkCtx.input / id };
            if (!std::filesystem::is_regular_file(path)) {
                std::filesystem::path p2{ ctx.linkCtx.input / "tables" / schema->dir / std::format("{}.json", id) };
                if (std::filesystem::is_regular_file(p2)) {
                    path = p2;
                }
            }

            BO4Json cfg{ path };
            if (!cfg.SyncConfig(false)) {
                LOG_ERROR("Can't read {} {}", schema->dir, path.string());
                ctx.error = true;
                return;
            }
            std::filesystem::path stem{ path.filename() };
            stem.replace_extension();
            std::string stemName{ stem.string() };

            ff.data.PushStream(XFILE_BLOCK_TEMP);
            byte* hdr{ ff.data.AllocStreamPtr<byte>(schema->header->size) };
            std::memset(hdr, 0, schema->header->size);
            ff.data.PushStream(XFILE_BLOCK_VIRTUAL);
            WriteFields(ctx, ff, *schema->header, hdr, cfg.main);
            ff.data.PopStream();
            ff.data.PopStream();

            // the name: the json's or the file's
            uint64_t& name{ *(uint64_t*)hdr };
            if (!name) {
                name = ctx.HashXHash(stemName, true);
            }
            LOG_INFO("Added asset {} {} (hash_{:x})", schema->dir, stemName, name);
        }
    };

    utils::MapAdder<XAssetLinkerImpl, XAssetType, XAssetLinker> amt{ GetWorkers(), XAssetType::ASSET_TYPE_ANIMMAPPINGTABLE, XAssetType::ASSET_TYPE_ANIMMAPPINGTABLE };
    utils::MapAdder<XAssetLinkerImpl, XAssetType, XAssetLinker> ast{ GetWorkers(), XAssetType::ASSET_TYPE_ANIMSELECTORTABLESET, XAssetType::ASSET_TYPE_ANIMSELECTORTABLESET };
    utils::MapAdder<XAssetLinkerImpl, XAssetType, XAssetLinker> asm_{ GetWorkers(), XAssetType::ASSET_TYPE_ANIMSTATEMACHINE, XAssetType::ASSET_TYPE_ANIMSTATEMACHINE };
    utils::MapAdder<XAssetLinkerImpl, XAssetType, XAssetLinker> bt{ GetWorkers(), XAssetType::ASSET_TYPE_BEHAVIORTREE, XAssetType::ASSET_TYPE_BEHAVIORTREE };
    utils::MapAdder<XAssetLinkerImpl, XAssetType, XAssetLinker> bsm{ GetWorkers(), XAssetType::ASSET_TYPE_BEHAVIORSTATEMACHINE, XAssetType::ASSET_TYPE_BEHAVIORSTATEMACHINE };
    utils::MapAdder<XAssetLinkerImpl, XAssetType, XAssetLinker> fst{ GetWorkers(), XAssetType::ASSET_TYPE_FOOTSTEP_TABLE, XAssetType::ASSET_TYPE_FOOTSTEP_TABLE };
    utils::MapAdder<XAssetLinkerImpl, XAssetType, XAssetLinker> ssd{ GetWorkers(), XAssetType::ASSET_TYPE_SURFACESOUNDDEF, XAssetType::ASSET_TYPE_SURFACESOUNDDEF };
    utils::MapAdder<XAssetLinkerImpl, XAssetType, XAssetLinker> sws{ GetWorkers(), XAssetType::ASSET_TYPE_SHAREDWEAPONSOUNDS, XAssetType::ASSET_TYPE_SHAREDWEAPONSOUNDS };
    utils::MapAdder<XAssetLinkerImpl, XAssetType, XAssetLinker> esi{ GetWorkers(), XAssetType::ASSET_TYPE_ENTITYSOUNDIMPACTS, XAssetType::ASSET_TYPE_ENTITYSOUNDIMPACTS };
} // namespace
