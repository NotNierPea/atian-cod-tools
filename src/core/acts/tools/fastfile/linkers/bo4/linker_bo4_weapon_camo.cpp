#include <includes.hpp>
#include <tools/fastfile/linkers/linker_bo4.hpp>

/*
 * Weapon camos (zone line: weaponcamo,tables/weaponcamo/my_camos.json). The keys are the bo4 weaponcamo unlinker's, so a
 * dumped game camo links again as it is:
 *
 * {
 *     "name": "my_camos",                   // the asset name, the file name by default
 *     "count": 400,                         // the number of sets, by default one past the last
 *     "camoMaterials": [ set, ... ],        // a set at each index (a camo index of the game is the set index + 1),
 *                                           // or { "398": set, ... } for sets at those indices, the others empty
 * }
 *
 * set: { "materials": [ camo material, ... ] }, a camo material:
 * {
 *     "baseMaterials": [ { "material": "#hash_..", "mask": "#hash_.." }, ... ], // one a channel, {} for none
 *     "camoMaterialChannels": [ channel, ... ],
 *     "template": 2,                        // shield-client: the camo index whose per-weapon sets this one is laid on
 *     "unk1a": 0, "unk1c": 0                // the rest of the header ("template" is unk1a)
 * }
 *
 * a channel:
 * {
 *     "camoMaterial": "materials/my_camo.json", // a material json of the zone, or a game one by name ("#name")
 *     "detailMap": "#image",                // an image file of the zone or a game one by name, none by default
 *     "translationX": 0, "translationY": 0, "scaleX": 1, "scaleY": 1, "rotation": 0,
 *     "normalBlend": 1, "glossBlend": 1, "detailHeight": 0, "detailScale": [0, 0], "albedoTint": 0,
 *     "flags": 14,                          // 2, 4, 8: the base material's color, normal, gloss textures go to the
 *                                           // camo material (sub_140EAC410), all of it to the shader as well
 *     "flags2": 0, "unk3f": 0
 * }
 */
namespace {
    using namespace fastfile::linker::bo4;

    struct CamoMaterialChannel {
        void* camoMaterial;
        float translationX;
        float translationY;
        float scaleX;
        float scaleY;
        float rotation;
        float normalBlend;
        float glossBlend;
        uint32_t albedoTint;
        void* detailMap;
        float detailHeight;
        float detailScale[2];
        uint16_t flags;
        uint8_t flags2;
        uint8_t unk3f;
    };
    static_assert(sizeof(CamoMaterialChannel) == 0x40);

    struct CamoMaterial {
        void** baseMaterials;
        void** baseMaterialMasks;
        CamoMaterialChannel* camoMaterialChannels;
        uint16_t numBaseMaterials;
        uint16_t unk1a;
        uint32_t unk1c;
    };
    static_assert(sizeof(CamoMaterial) == 0x20);

    struct WeaponCamoMaterialSet {
        CamoMaterial* materials;
        uint32_t numMaterials;
    };
    static_assert(sizeof(WeaponCamoMaterialSet) == 0x10);

    struct WeaponCamo {
        XHash name;
        WeaponCamoMaterialSet* camoMaterials;
        uint32_t numCamoMaterials;
    };
    static_assert(sizeof(WeaponCamo) == 0x20);

    constexpr size_t MAX_SETS = 0x200; // 9 bits of camo in the render options

    const rapidjson::Value* Member(const rapidjson::Value& obj, const char* name) {
        if (!obj.IsObject()) {
            return nullptr;
        }
        auto it{ obj.FindMember(name) };
        return it == obj.MemberEnd() ? nullptr : &it->value;
    }

    float Float(const rapidjson::Value& obj, const char* name, float def) {
        const rapidjson::Value* v{ Member(obj, name) };
        return v && v->IsNumber() ? (float)v->GetDouble() : def;
    }

    uint64_t Integer(const rapidjson::Value& obj, const char* name, uint64_t def) {
        const rapidjson::Value* v{ Member(obj, name) };
        if (!v) {
            return def;
        }
        if (v->IsUint64()) {
            return v->GetUint64();
        }
        if (v->IsInt64()) {
            return (uint64_t)v->GetInt64();
        }
        if (v->IsString()) {
            return std::strtoull(v->GetString(), nullptr, 0);
        }
        return def;
    }

    // "(x, y)" as the unlinker writes it, or [x, y]
    void Vec2(const rapidjson::Value& obj, const char* name, float(&out)[2]) {
        out[0] = out[1] = 0;
        const rapidjson::Value* v{ Member(obj, name) };
        if (!v) {
            return;
        }
        if (v->IsArray()) {
            for (rapidjson::SizeType i = 0; i < 2 && i < v->Size(); i++) {
                out[i] = (*v)[i].IsNumber() ? (float)(*v)[i].GetDouble() : 0;
            }
        } else if (v->IsString()) {
            std::sscanf(v->GetString(), " ( %f , %f )", &out[0], &out[1]);
        }
    }

    // a game asset by name: "#hash_..", "#name" or ",name", the flag bit of a dumped name off
    std::string RefName(const char* val) {
        std::string s{ *val == '#' || *val == ',' ? val + 1 : val };
        if (s.starts_with("hash_") && s.size() == 21) {
            uint64_t h{ std::strtoull(s.data() + 5, nullptr, 16) & 0x7FFFFFFFFFFFFFFFull };
            return std::format("hash_{:x}", h);
        }
        return s;
    }

    class XAssetLinkerImpl : public XAssetLinker {
        // the material jsons linked in a zone, a repeat is linked by its name
        std::unordered_map<BO4FFContext*, std::unordered_set<std::string>> zoneMaterials{};
        std::unordered_map<BO4FFContext*, std::unordered_set<std::string>> zoneImages{};

      public:
        using XAssetLinker::XAssetLinker;

        void Compute(BO4LinkContext& ctx, const char* id, BO4FFContext& ff) override {
            std::filesystem::path path{ ctx.linkCtx.input / id };
            std::filesystem::path rfpath{ path.filename() };
            rfpath.replace_extension();

            BO4Json cfg{ path };
            if (!cfg.SyncConfig(false)) {
                LOG_ERROR("Can't read weaponcamo {}", path.string());
                ctx.error = true;
                return;
            }
            auto Fail = [&ctx, &path](const std::string& what) {
                LOG_ERROR("Invalid weaponcamo {}: {}", path.string(), what);
                ctx.error = true;
            };
            const rapidjson::Value& root{ cfg.main };

            // the sets by index
            std::vector<const rapidjson::Value*> sets{};
            const rapidjson::Value* camoMaterials{ Member(root, "camoMaterials") };
            if (camoMaterials && camoMaterials->IsArray()) {
                for (const rapidjson::Value& set : camoMaterials->GetArray()) {
                    sets.push_back(&set);
                }
            } else if (camoMaterials && camoMaterials->IsObject()) {
                for (auto it{ camoMaterials->MemberBegin() }; it != camoMaterials->MemberEnd(); ++it) {
                    char* end{};
                    unsigned long idx{ std::strtoul(it->name.GetString(), &end, 0) };
                    if (*end || idx >= MAX_SETS) {
                        Fail(std::format("bad set index {}, 0 to {}", it->name.GetString(), MAX_SETS - 1));
                        return;
                    }
                    if (sets.size() <= idx) {
                        sets.resize(idx + 1);
                    }
                    sets[idx] = &it->value;
                }
            } else if (camoMaterials) {
                Fail("camoMaterials is no array or object");
                return;
            }
            size_t count{ (size_t)Integer(root, "count", sets.size()) };
            if (count < sets.size() || count > MAX_SETS) {
                Fail(std::format("count {} out of {} to {}", count, sets.size(), MAX_SETS));
                return;
            }
            sets.resize(count);

            auto LinkMaterial = [&](const rapidjson::Value* ref, void*& ptr) {
                if (!ref || !ref->IsString() || !*ref->GetString()) {
                    ptr = nullptr;
                    return;
                }
                std::string val{ ref->GetString() };
                std::unordered_set<std::string>& linked{ zoneMaterials[&ff] };
                if (val[0] == '#' || val[0] == ',') {
                    ctx.LinkAsset(XAssetType::ASSET_TYPE_MATERIAL, std::format(",{}", RefName(val.data())).data(), ptr, false, &ff);
                } else if (linked.contains(val)) {
                    BO4Json mcfg{ ctx.linkCtx.input / val };
                    std::filesystem::path stem{ std::filesystem::path{ val }.filename() };
                    stem.replace_extension();
                    std::string name{ mcfg.SyncConfig(false) ? mcfg.GetString("name", stem.string().c_str()) : stem.string() };
                    ctx.LinkAsset(XAssetType::ASSET_TYPE_MATERIAL, std::format(",{}", name).data(), ptr, false, &ff);
                } else {
                    linked.insert(val);
                    ctx.LinkAsset(XAssetType::ASSET_TYPE_MATERIAL, val.data(), ptr, false, &ff);
                }
            };
            auto LinkImage = [&](const rapidjson::Value* ref, void*& ptr) {
                if (!ref || !ref->IsString() || !*ref->GetString()) {
                    ptr = nullptr;
                    return;
                }
                std::string val{ ref->GetString() };
                std::unordered_set<std::string>& linked{ zoneImages[&ff] };
                std::string stem{ std::filesystem::path{ val.substr(0, val.find(',')) }.stem().string() };
                if (val[0] == '#' || val[0] == ',') {
                    ctx.LinkAsset(XAssetType::ASSET_TYPE_IMAGE, std::format(",{}", RefName(val.data())).data(), ptr, false, &ff);
                } else if (linked.contains(stem)) {
                    ctx.LinkAsset(XAssetType::ASSET_TYPE_IMAGE, std::format(",{}", stem).data(), ptr, false, &ff);
                } else {
                    linked.insert(stem);
                    ctx.LinkAsset(XAssetType::ASSET_TYPE_IMAGE, val.data(), ptr, false, &ff);
                }
            };

            // Load_WeaponCamo (sub_142E86B70): the header, the sets, each set's camo materials
            ff.data.PushStream(XFILE_BLOCK_TEMP);
            ff.data.Align(8);
            WeaponCamo& camo{ ff.data.AllocStreamRef<WeaponCamo>() };
            camo = {};
            std::string assetName{ cfg.GetString("name", rfpath.string().c_str()) };
            camo.name.name = ctx.HashXHash(assetName, true);
            camo.numCamoMaterials = (uint32_t)count;

            ff.data.PushStream(XFILE_BLOCK_VIRTUAL);
            size_t used{};
            if (count) {
                camo.camoMaterials = (WeaponCamoMaterialSet*)fastfile::linker::memory::POINTER_NEXT;
                ff.data.Align(8);
                WeaponCamoMaterialSet* setData{ ff.data.AllocStreamPtr<WeaponCamoMaterialSet>(count) };
                std::vector<const rapidjson::Value*> setMaterials(count);
                for (size_t i = 0; i < count; i++) {
                    const rapidjson::Value* mats{ sets[i] ? Member(*sets[i], "materials") : nullptr };
                    setData[i] = {};
                    if (mats && mats->IsArray() && mats->Size()) {
                        setData[i].materials = (CamoMaterial*)fastfile::linker::memory::POINTER_NEXT;
                        setData[i].numMaterials = mats->Size();
                        setMaterials[i] = mats;
                        used++;
                    }
                }

                for (size_t i = 0; i < count; i++) {
                    const rapidjson::Value* mats{ setMaterials[i] };
                    if (!mats) {
                        continue;
                    }
                    // sub_142E86DC0: all the headers, then for each its base materials, masks and channels
                    ff.data.Align(8);
                    CamoMaterial* cms{ ff.data.AllocStreamPtr<CamoMaterial>(mats->Size()) };
                    std::vector<size_t> channelCounts(mats->Size());
                    for (rapidjson::SizeType m = 0; m < mats->Size(); m++) {
                        const rapidjson::Value& mat{ (*mats)[m] };
                        const rapidjson::Value* bases{ Member(mat, "baseMaterials") };
                        const rapidjson::Value* channels{ Member(mat, "camoMaterialChannels") };
                        size_t n{ std::max<size_t>(
                            bases && bases->IsArray() ? bases->Size() : 0,
                            channels && channels->IsArray() ? channels->Size() : 0
                        ) };
                        if (n > 0xFFFF) {
                            Fail(std::format("set {} has too many channels", i));
                            return;
                        }
                        channelCounts[m] = n;
                        CamoMaterial& cm{ cms[m] };
                        cm = {};
                        cm.numBaseMaterials = (uint16_t)n;
                        cm.unk1a = (uint16_t)Integer(mat, "template", Integer(mat, "unk1a", 0));
                        cm.unk1c = (uint32_t)Integer(mat, "unk1c", 0);
                        if (n) {
                            cm.baseMaterials = (void**)fastfile::linker::memory::POINTER_NEXT;
                            cm.baseMaterialMasks = (void**)fastfile::linker::memory::POINTER_NEXT;
                            cm.camoMaterialChannels = (CamoMaterialChannel*)fastfile::linker::memory::POINTER_NEXT;
                        }
                    }

                    for (rapidjson::SizeType m = 0; m < mats->Size(); m++) {
                        size_t n{ channelCounts[m] };
                        if (!n) {
                            continue;
                        }
                        const rapidjson::Value& mat{ (*mats)[m] };
                        const rapidjson::Value* bases{ Member(mat, "baseMaterials") };
                        const rapidjson::Value* channels{ Member(mat, "camoMaterialChannels") };
                        auto Base = [&](size_t k) -> const rapidjson::Value* {
                            return bases && bases->IsArray() && k < bases->Size() ? &(*bases)[(rapidjson::SizeType)k] : nullptr;
                        };

                        ff.data.Align(8);
                        void** baseMats{ ff.data.AllocStreamPtr<void*>(n) };
                        for (size_t k = 0; k < n; k++) {
                            const rapidjson::Value* base{ Base(k) };
                            LinkMaterial(base ? Member(*base, "material") : nullptr, baseMats[k]);
                        }

                        ff.data.Align(8);
                        void** masks{ ff.data.AllocStreamPtr<void*>(n) };
                        for (size_t k = 0; k < n; k++) {
                            const rapidjson::Value* base{ Base(k) };
                            LinkImage(base ? Member(*base, "mask") : nullptr, masks[k]);
                        }

                        ff.data.Align(8);
                        CamoMaterialChannel* chans{ ff.data.AllocStreamPtr<CamoMaterialChannel>(n) };
                        for (size_t k = 0; k < n; k++) {
                            static const rapidjson::Value empty{ rapidjson::kObjectType };
                            const rapidjson::Value& ch{
                                channels && channels->IsArray() && k < channels->Size() ? (*channels)[(rapidjson::SizeType)k] : empty
                            };
                            CamoMaterialChannel& c{ chans[k] };
                            c = {};
                            c.translationX = Float(ch, "translationX", 0);
                            c.translationY = Float(ch, "translationY", 0);
                            c.scaleX = Float(ch, "scaleX", 1);
                            c.scaleY = Float(ch, "scaleY", 1);
                            c.rotation = Float(ch, "rotation", 0);
                            c.normalBlend = Float(ch, "normalBlend", 1);
                            c.glossBlend = Float(ch, "glossBlend", 1);
                            c.albedoTint = (uint32_t)Integer(ch, "albedoTint", 0);
                            c.detailHeight = Float(ch, "detailHeight", 0);
                            Vec2(ch, "detailScale", c.detailScale);
                            c.flags = (uint16_t)Integer(ch, "flags", 0);
                            c.flags2 = (uint8_t)Integer(ch, "flags2", 0);
                            c.unk3f = (uint8_t)Integer(ch, "unk3f", 0);
                            // each channel's material, then its detail map
                            LinkMaterial(Member(ch, "camoMaterial"), c.camoMaterial);
                            LinkImage(Member(ch, "detailMap"), c.detailMap);
                        }
                    }
                }
            }
            ff.data.PopStream();

            ff.data.PopStream();
            LOG_INFO("Added asset weaponcamo {} (hash_{:x}), {} sets, {} of them with camo materials", assetName, camo.name.name, count, used);
        }
    };

    utils::MapAdder<XAssetLinkerImpl, XAssetType, XAssetLinker> impl{ GetWorkers(), XAssetType::ASSET_TYPE_WEAPON_CAMO };
} // namespace
