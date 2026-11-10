#include <includes.hpp>
#include <tools/fastfile/linkers/linker_bo4.hpp>

/*
 * Physics constraints: the swinging bones of a model (an xmodel's "physConstraints", by name), e.g. Firebreak's bunny
 * ears (core_common hash_4a640d8b3aea4bf). Retail layout, read off that asset in the zone: a 0x30 header (name XHash,
 * u32 count, the entries' pointer at 0x28), the entries inline after, 0x1D8 each. An entry's script strings are at the
 * ELF's PhysConstraint offsets (targetname 0, target_ent1 0x10, target_bone1 0x14, target_ent2 0x20, target_bone2 0x24,
 * target_bone3 0x28); its values partly moved from the ELF's, so an entry is a game entry's bytes ("template", hex) with
 * fields set over them:
 *
 * {
 *     "name": "my_constraints",
 *     "entries": [{
 *         "template": "<0x1D8 bytes hex>",          // a game entry (the ear's: type 6 at 4, health limits at 0x64/0x68)
 *         "target_bone1": "j_my_bone",              // the bone it swings
 *         "floats": { "0x74": 8.0, "0x94": -40 },   // values at offsets (the ear's: distance 0x6C, damp 0x70,
 *                                                    // power 0x74, pitch limits 0x94/0x98, yaw 0x9C/0xA0, gravity 0xB8)
 *         "ints": { "0x4": 6 }
 *     }]
 * }
 */
namespace {
    using namespace fastfile::linker::bo4;

    constexpr size_t ENTRY_SIZE = 0x1D8;
    constexpr size_t MAX_ENTRIES = 50;

    struct PhysConstraintsHeader {
        XHash name;
        uint32_t count;
        uint32_t pad14;
        uint64_t unk18;
        uint64_t unk20;
        void* data;
    };
    static_assert(sizeof(PhysConstraintsHeader) == 0x30);

    class XAssetLinkerImpl : public XAssetLinker {
      public:
        using XAssetLinker::XAssetLinker;

        void Compute(BO4LinkContext& ctx, const char* id, BO4FFContext& ff) override {
            std::filesystem::path path{ ctx.linkCtx.input / id };
            std::filesystem::path rfpath{ path.filename() };
            rfpath.replace_extension();

            BO4Json cfg{ path };
            if (!cfg.SyncConfig(false)) {
                LOG_ERROR("Can't read physconstraints {}", path.string());
                ctx.error = true;
                return;
            }
            const rapidjson::Value& root{ cfg.main };
            auto entriesIt{ root.FindMember("entries") };
            if (entriesIt == root.MemberEnd() || !entriesIt->value.IsArray() || !entriesIt->value.Size() || entriesIt->value.Size() > MAX_ENTRIES) {
                LOG_ERROR("physconstraints {}: \"entries\", 1 to {} of them", path.string(), MAX_ENTRIES);
                ctx.error = true;
                return;
            }
            const rapidjson::Value& entries{ entriesIt->value };

            ff.data.PushStream(XFILE_BLOCK_TEMP);
            ff.data.Align(8);
            PhysConstraintsHeader& header{ ff.data.AllocStreamRef<PhysConstraintsHeader>() };
            header = {};
            std::string assetName{ cfg.GetString("name", rfpath.string().c_str()) };
            header.name.name = ctx.HashXHash(assetName, true);
            header.count = (uint32_t)entries.Size();

            ff.data.PushStream(XFILE_BLOCK_VIRTUAL);
            ff.data.Align(8);
            header.data = (void*)fastfile::linker::memory::POINTER_NEXT;
            byte* out{ ff.data.AllocStreamPtr<byte>(ENTRY_SIZE * entries.Size()) };
            std::memset(out, 0, ENTRY_SIZE * entries.Size());

            auto Offset = [](const char* key) { return (size_t)std::strtoull(key, nullptr, 0); };
            for (rapidjson::SizeType e = 0; e < entries.Size(); e++) {
                const rapidjson::Value& entry{ entries[e] };
                byte* dst{ out + ENTRY_SIZE * e };
                auto tmpl{ entry.FindMember("template") };
                if (tmpl != entry.MemberEnd() && tmpl->value.IsString()) {
                    std::string hex{ tmpl->value.GetString() };
                    for (size_t i = 0; i + 1 < hex.size() && i / 2 < ENTRY_SIZE; i += 2) {
                        dst[i / 2] = (byte)std::strtoul(hex.substr(i, 2).c_str(), nullptr, 16);
                    }
                }
                // its script strings: the template's are the game zone's indices, none of ours
                static constexpr std::pair<const char*, size_t> STRINGS[]{
                    { "targetname", 0 }, { "target_ent1", 0x10 }, { "target_bone1", 0x14 }, { "target_ent2", 0x20 },
                    { "target_bone2", 0x24 }, { "target_bone3", 0x28 },
                };
                for (const auto& [key, off] : STRINGS) {
                    *(uint32_t*)(dst + off) = 0;
                    auto s{ entry.FindMember(key) };
                    if (s != entry.MemberEnd() && s->value.IsString() && *s->value.GetString()) {
                        ctx.AddScrString(s->value.GetString(), *(ScrString_t*)(dst + off), ff);
                    }
                }
                auto floats{ entry.FindMember("floats") };
                if (floats != entry.MemberEnd() && floats->value.IsObject()) {
                    for (auto f{ floats->value.MemberBegin() }; f != floats->value.MemberEnd(); ++f) {
                        size_t off{ Offset(f->name.GetString()) };
                        if (off + 4 <= ENTRY_SIZE && f->value.IsNumber()) {
                            *(float*)(dst + off) = (float)f->value.GetDouble();
                        }
                    }
                }
                auto ints{ entry.FindMember("ints") };
                if (ints != entry.MemberEnd() && ints->value.IsObject()) {
                    for (auto f{ ints->value.MemberBegin() }; f != ints->value.MemberEnd(); ++f) {
                        size_t off{ Offset(f->name.GetString()) };
                        if (off + 4 <= ENTRY_SIZE && f->value.IsInt()) {
                            *(int32_t*)(dst + off) = f->value.GetInt();
                        }
                    }
                }
                // (pointers: the material handle and the runtime handle off)
                *(uint64_t*)(dst + 0xA8) = 0;
            }
            ff.data.PopStream();
            ff.data.PopStream();
            LOG_INFO("Added asset physconstraints {} (hash_{:x}), {} entr{}", assetName, header.name.name, entries.Size(), entries.Size() == 1 ? "y" : "ies");
        }
    };

    utils::MapAdder<XAssetLinkerImpl, XAssetType, XAssetLinker> impl{ GetWorkers(), XAssetType::ASSET_TYPE_PHYSCONSTRAINTS };
} // namespace
