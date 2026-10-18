#include <includes.hpp>
#include <tools/fastfile/linkers/linker_bo4.hpp>

/*
 * Materials, from a game material dumped by the bo4 material unlinker (acts ff ... -a material): its blocks as they
 * are, the technique set and the images by name, in the json or in a "template" json. Images are references
 * ("#name") or images of the zone ("file.png[,stream]", from the images directory).
 *
 * {
 *     "name": "my_material",
 *     "template": "../templates/material_model.json",   // the dumped material, or its keys in this json
 *     "textures": { "colorMap": "my_color.png,stream", "normalMap": "#$identitynormalmap" },  // by slot (type)
 *     "replace": { "#old_image": "my_image.png" },      // an image everywhere it is used
 *     "values": { "colorTint": [1, 0.5, 0.5], "glossRange": [0, 8] },  // the constants of the template's techniques
 *     "samplerStates": { "colorSampler": { "filter": "nearest", "wrap": "clamp" }, "*": "0x13" },
 *     "cameraRegion": "gbuffer"                          // the pass, lit_trans for blending
 * }
 *
 * Names are hashed (h = 33h ^ (c | 0x20)), unk_<hex> or #<hex> are hashes as the dump writes them. The constants are
 * the technique arguments of type 0x81 the dump lists in "constantDefs": a float array at an offset in the constant
 * buffer of each technique that has it (the data of subs[technique].blocks[2]). Sampler states are GfxSamplerState:
 * filter bits 0-2 (disabled, nearest, linear, aniso2x, 4x, 8x, 16x, compare), mipmap 3-4 (disabled, nearest,
 * linear), wrap u 5-6, v 7-8, w 9-10 (wrap, clamp, border, mirror), border color 11 (black, white).
 */
namespace {
    using namespace fastfile::linker::bo4;

    struct MaterialTextureDef {
        void* image;
        byte data[0x18];
    };
    static_assert(sizeof(MaterialTextureDef) == 0x20);

    struct MaterialBindingImage {
        void* image;
        uint64_t unk8;
    };
    static_assert(sizeof(MaterialBindingImage) == 0x10);

    struct MaterialBinding {
        uint32_t* data;
        MaterialBindingImage* images;
        uint32_t dataCount;
        uint32_t imageCount;
    };
    static_assert(sizeof(MaterialBinding) == 0x18);

    // 8 blocks of 0x40 bytes, constant buffers made at load (sub_1435FC100)
    struct MaterialConstantSub {
        byte* blocks[8];
    };
    static_assert(sizeof(MaterialConstantSub) == 0x40);

    struct MaterialConstants {
        uint64_t unk0[6];
        MaterialConstantSub* subs[12];
        uint64_t datalen;
        byte* data;
    };
    static_assert(sizeof(MaterialConstants) == 0xa0);

    struct Material {
        XHash name;
        uint64_t unk10;
        uint64_t unk18;
        uint64_t unk20;
        uint64_t unk28;
        void* techniqueSet;
        MaterialTextureDef* textureTable;
        uint64_t* samplerTable;
        uint64_t unk48;
        MaterialBinding* bindings[8];
        MaterialConstants constants;
        byte textureCount;
        byte samplerCount;
        byte unk132;
        byte unk133;
    };
    static_assert(sizeof(Material) == 0x138);

    constexpr size_t CONSTANT_BLOCK_SIZE = 0x40;
    // the block of a technique's constants in its sub, the pixel shader's
    constexpr size_t CONSTANT_BLOCK_INDEX = 2;
    constexpr uint16_t SAMPLER_FILTER_MASK = 0x7;
    constexpr uint16_t SAMPLER_MIPMAP_SHIFT = 3;
    constexpr uint16_t SAMPLER_MIPMAP_MASK = 0x18;
    constexpr uint16_t SAMPLER_WRAP_U_SHIFT = 5;
    constexpr uint16_t SAMPLER_WRAP_V_SHIFT = 7;
    constexpr uint16_t SAMPLER_WRAP_W_SHIFT = 9;
    constexpr uint16_t SAMPLER_BORDER_WHITE = 0x800;

    constexpr const char* SAMPLER_FILTERS[]{ "disabled", "nearest", "linear", "aniso2x", "aniso4x", "aniso8x", "aniso16x", "compare" };
    constexpr const char* SAMPLER_MIPMAPS[]{ "disabled", "nearest", "linear" };
    constexpr const char* SAMPLER_WRAPS[]{ "wrap", "clamp", "border", "mirror" };
    constexpr const char* CAMERA_REGIONS[]{
        "gbuffer_depth_hack", "gbuffer_depth_hack_decal", "gbuffer", "gbuffer_decal", "gbuffer_no_volume_decal",
        "lit_opaque", "lit_trans", "lit_trans_post_resolve", "lit_post_resolve", "water", "emissive_fx",
        "depth_hack_sss", "forward_sss", "depth_hack", "depth_hack_trans", "depth_hack_post_blur", "sonar",
        "oit", "depth_prime", "underwater",
    };

    // the hash of the names of textures, samplers and constants
    uint32_t MaterialHash(std::string_view name) {
        // unk_<hex> and #<hex> as the dump writes the unknown ones
        for (std::string_view prefix : { "unk_", "#" }) {
            if (name.starts_with(prefix)) {
                return (uint32_t)std::strtoul(std::string{ name.substr(prefix.size()) }.c_str(), nullptr, 16);
            }
        }
        uint32_t h{};
        for (char c : name) {
            h = (33 * h) ^ ((byte)c | 0x20);
        }
        return h;
    }

    // the index of a name in a list, -1 when it isn't in it
    template<size_t N>
    int NameIndex(const char* const (&names)[N], std::string_view name) {
        for (size_t i = 0; i < N; i++) {
            if (name == names[i]) {
                return (int)i;
            }
        }
        return -1;
    }

    int HexDigit(char c) {
        if (c >= '0' && c <= '9')
            return c - '0';
        if (c >= 'a' && c <= 'f')
            return c - 'a' + 10;
        if (c >= 'A' && c <= 'F')
            return c - 'A' + 10;
        return -1;
    }

    bool ReadHex(const rapidjson::Value* val, std::vector<byte>& out) {
        out.clear();
        if (!val || !val->IsString()) {
            return false;
        }
        const char* str{ val->GetString() };
        size_t len{ val->GetStringLength() };
        if (len & 1) {
            return false;
        }
        out.reserve(len / 2);
        for (size_t i = 0; i < len; i += 2) {
            int hi{ HexDigit(str[i]) };
            int lo{ HexDigit(str[i + 1]) };
            if (hi < 0 || lo < 0) {
                return false;
            }
            out.push_back((byte)((hi << 4) | lo));
        }
        return true;
    }

    const rapidjson::Value* Member(const rapidjson::Value& obj, const char* name) {
        if (!obj.IsObject()) {
            return nullptr;
        }
        auto it{ obj.FindMember(name) };
        return it == obj.MemberEnd() || it->value.IsNull() ? nullptr : &it->value;
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
                LOG_ERROR("Can't read {}", path.string());
                ctx.error = true;
                return;
            }
            std::vector<byte> buff{};
            auto Fail = [&ctx, &path](const std::string& what) {
                LOG_ERROR("Invalid material {}: {}", path.string(), what);
                ctx.error = true;
            };

            // the blocks of the game material: the template's or this json's
            std::string templatePath{ cfg.GetString("template", "") };
            BO4Json tcfg{ path.parent_path() / templatePath };
            if (!templatePath.empty() && !tcfg.SyncConfig(false)) {
                Fail(std::format("can't read template {}", templatePath));
                return;
            }
            rapidjson::Value& root{ templatePath.empty() ? cfg.main : tcfg.main };
            const rapidjson::Value& user{ cfg.main };

            if (!ReadHex(Member(root, "header"), buff) || buff.size() != sizeof(Material)) {
                Fail("bad header");
                return;
            }

            std::unordered_map<std::string, std::string> replace{};
            if (const rapidjson::Value* rep{ Member(user, "replace") }; rep && rep->IsObject()) {
                for (auto it{ rep->MemberBegin() }; it != rep->MemberEnd(); ++it) {
                    if (it->value.IsString()) {
                        replace[it->name.GetString()] = it->value.GetString();
                    }
                }
            }

            // the images of the slots by name
            std::unordered_map<uint32_t, std::string> slotImages{};
            if (const rapidjson::Value* tex{ Member(user, "textures") }; tex && tex->IsObject()) {
                for (auto it{ tex->MemberBegin() }; it != tex->MemberEnd(); ++it) {
                    if (it->value.IsString()) {
                        slotImages[MaterialHash(it->name.GetString())] = it->value.GetString();
                    }
                }
            }

            // images of the zone are linked in place once, then referenced by name
            std::unordered_set<std::string> linkedImages{};
            std::function<void(std::string, void*&)> LinkImageName{};
            auto LinkImage = [&](const rapidjson::Value* ref, void*& ptr) {
                if (!ref || !ref->IsString() || !*ref->GetString()) {
                    ptr = nullptr;
                    return;
                }
                std::string val{ ref->GetString() };
                LinkImageName(val, ptr);
            };
            LinkImageName = [&](std::string val, void*& ptr) {
                if (auto it{ replace.find(val) }; it != replace.end()) {
                    val = it->second;
                }
                if (val[0] == '#') {
                    std::string refId{ std::format(",{}", val.substr(1)) };
                    ctx.LinkAsset(XAssetType::ASSET_TYPE_IMAGE, refId.data(), ptr, false, &ff);
                    return;
                }
                std::filesystem::path file{ val.substr(0, val.find(',')) };
                std::string stem{ file.stem().string() };
                if (linkedImages.contains(stem)) {
                    ctx.LinkAsset(XAssetType::ASSET_TYPE_IMAGE, std::format(",{}", stem).data(), ptr, false, &ff);
                    return;
                }
                linkedImages.insert(stem);
                ctx.LinkAsset(XAssetType::ASSET_TYPE_IMAGE, val.data(), ptr, false, &ff);
            };

            // Load_Material: header, name, constants, technique set, textures, samplers, bindings
            ff.data.PushStream(XFILE_BLOCK_TEMP);
            ff.data.Align(0x10);
            Material& mat{ ff.data.AllocStreamRef<Material>() };
            std::memcpy(&mat, buff.data(), sizeof(mat));

            std::string assetName{ cfg.GetString("name", rfpath.string().c_str()) };
            if (const rapidjson::Value* region{ Member(user, "cameraRegion") }; region && region->IsString()) {
                int idx{ NameIndex(CAMERA_REGIONS, region->GetString()) };
                if (idx < 0) {
                    Fail(std::format("unknown cameraRegion {}", region->GetString()));
                    return;
                }
                mat.unk133 = (byte)idx;
            }
            mat.name.name = ctx.HashXHash(assetName, true);
            mat.unk48 = 0;

            ff.data.PushStream(XFILE_BLOCK_VIRTUAL);

            // constants
            const rapidjson::Value* constants{ Member(root, "constants") };
            const rapidjson::Value* subs{ constants ? Member(*constants, "subs") : nullptr };
            for (size_t i = 0; i < std::size(mat.constants.subs); i++) {
                const rapidjson::Value* sub{
                    subs && subs->IsArray() && i < subs->Size() ? &(*subs)[(rapidjson::SizeType)i] : nullptr
                };
                if (!sub || !sub->IsArray()) {
                    mat.constants.subs[i] = nullptr;
                    continue;
                }
                mat.constants.subs[i] = (MaterialConstantSub*)fastfile::linker::memory::POINTER_NEXT;
                ff.data.Align(8);
                MaterialConstantSub& csub{ ff.data.AllocStreamRef<MaterialConstantSub>() };
                for (size_t j = 0; j < std::size(csub.blocks); j++) {
                    const rapidjson::Value* block{ j < sub->Size() ? &(*sub)[(rapidjson::SizeType)j] : nullptr };
                    if (!block || block->IsNull()) {
                        csub.blocks[j] = nullptr;
                        continue;
                    }
                    if (!ReadHex(block, buff) || buff.size() != CONSTANT_BLOCK_SIZE) {
                        Fail("bad constant block");
                        return;
                    }
                    // the buffer made at load, set again by the game
                    std::memset(&buff[8], 0, 0x30);
                    csub.blocks[j] = (byte*)fastfile::linker::memory::POINTER_NEXT;
                    ff.data.Align(0x10);
                    ff.data.WriteStream(buff.data(), buff.size());
                }
            }
            if (constants && ReadHex(Member(*constants, "data"), buff) && !buff.empty()) {
                // the named values on the data, in each technique that has them
                if (const rapidjson::Value* values{ Member(user, "values") }; values && values->IsObject()) {
                    const rapidjson::Value* defs{ Member(root, "constantDefs") };
                    for (auto it{ values->MemberBegin() }; it != values->MemberEnd(); ++it) {
                        uint32_t hash{ MaterialHash(it->name.GetString()) };
                        std::vector<float> vals{};
                        if (it->value.IsNumber()) {
                            vals.push_back((float)it->value.GetDouble());
                        } else if (it->value.IsArray()) {
                            for (const rapidjson::Value& v : it->value.GetArray()) {
                                vals.push_back(v.IsNumber() ? (float)v.GetDouble() : 0.0f);
                            }
                        }
                        size_t found{};
                        for (size_t d = 0; defs && defs->IsArray() && d < defs->Size(); d++) {
                            const rapidjson::Value& def{ (*defs)[(rapidjson::SizeType)d] };
                            const rapidjson::Value* dname{ Member(def, "name") };
                            if (!dname || !dname->IsString() || MaterialHash(dname->GetString()) != hash) {
                                continue;
                            }
                            size_t tech{ (size_t)def["technique"].GetUint() };
                            size_t offset{ (size_t)def["offset"].GetUint() };
                            size_t size{ (size_t)def["size"].GetUint() };
                            const rapidjson::Value* sub{ subs && subs->IsArray() && tech < subs->Size() ? &(*subs)[(rapidjson::SizeType)tech] : nullptr };
                            const rapidjson::Value* block{ sub && sub->IsArray() && CONSTANT_BLOCK_INDEX < sub->Size() ? &(*sub)[(rapidjson::SizeType)CONSTANT_BLOCK_INDEX] : nullptr };
                            std::vector<byte> bh{};
                            if (!block || !ReadHex(block, bh) || bh.size() < 8) {
                                continue;
                            }
                            uint32_t bufOffset{ *(uint32_t*)bh.data() };
                            uint32_t bufSize{ *(uint32_t*)(bh.data() + 4) };
                            if (offset + size > bufSize || bufOffset + bufSize > buff.size()) {
                                Fail(std::format("constant {} out of its buffer", it->name.GetString()));
                                return;
                            }
                            std::memcpy(&buff[bufOffset + offset], vals.data(), std::min(size, vals.size() * sizeof(float)));
                            found++;
                        }
                        if (!found) {
                            std::string names{};
                            for (size_t d = 0; defs && defs->IsArray() && d < defs->Size(); d++) {
                                const rapidjson::Value* dname{ Member((*defs)[(rapidjson::SizeType)d], "name") };
                                if (dname && dname->IsString() && names.find(dname->GetString()) == std::string::npos) {
                                    names += std::format(" {}", dname->GetString());
                                }
                            }
                            Fail(std::format("no constant {} in the techniques (constantDefs:{})", it->name.GetString(), names.empty() ? " none, dump the template again" : names));
                            return;
                        }
                    }
                }
                mat.constants.datalen = buff.size();
                mat.constants.data = (byte*)fastfile::linker::memory::POINTER_NEXT;
                ff.data.PushStream(XFILE_BLOCK_PHYSICAL);
                ff.data.Align(0x100);
                ff.data.WriteStream(buff.data(), buff.size());
                ff.data.PopStream();
            } else {
                mat.constants.datalen = 0;
                mat.constants.data = nullptr;
            }

            // technique set, a reference to the game's
            const rapidjson::Value* techset{ Member(root, "techniqueSet") };
            if (!techset || !techset->IsString()) {
                Fail("no techniqueSet");
                return;
            }
            const char* techsetName{ techset->GetString() };
            if (*techsetName == '#') {
                techsetName++;
            }
            ctx.LinkAsset(
                XAssetType::ASSET_TYPE_TECHNIQUE_SET,
                std::format(",{}", techsetName).data(),
                mat.techniqueSet,
                false,
                &ff
            );

            // textures
            const rapidjson::Value* textures{ Member(root, "textureTable") };
            size_t textureCount{ textures && textures->IsArray() ? textures->Size() : 0 };
            if (textureCount > 0xFF) {
                Fail("too many textures");
                return;
            }
            mat.textureCount = (byte)textureCount;
            if (textureCount) {
                mat.textureTable = (MaterialTextureDef*)fastfile::linker::memory::POINTER_NEXT;
                ff.data.Align(8);
                MaterialTextureDef* defs{ ff.data.AllocStreamPtr<MaterialTextureDef>(textureCount) };
                for (size_t i = 0; i < textureCount; i++) {
                    if (!ReadHex(Member((*textures)[(rapidjson::SizeType)i], "def"), buff) ||
                        buff.size() != sizeof(MaterialTextureDef)) {
                        Fail("bad texture def");
                        return;
                    }
                    std::memcpy(&defs[i], buff.data(), sizeof(defs[i]));
                }
                for (size_t i = 0; i < textureCount; i++) {
                    uint32_t slot{ *(uint32_t*)((byte*)&defs[i] + 8) };
                    if (auto it{ slotImages.find(slot) }; it != slotImages.end()) {
                        LinkImageName(it->second, defs[i].image);
                        slotImages.erase(it);
                    } else {
                        LinkImage(Member((*textures)[(rapidjson::SizeType)i], "image"), defs[i].image);
                    }
                }
                if (!slotImages.empty()) {
                    std::string names{};
                    for (size_t i = 0; i < textureCount; i++) {
                        const rapidjson::Value* type{ Member((*textures)[(rapidjson::SizeType)i], "type") };
                        names += std::format(" {}", type && type->IsString() ? type->GetString() : "?");
                    }
                    Fail(std::format("a texture isn't a slot of the template (slots:{})", names));
                    return;
                }
            } else {
                mat.textureTable = nullptr;
            }

            // samplers
            if (ReadHex(Member(root, "samplers"), buff) && !buff.empty()) {
                if ((buff.size() & 7) || buff.size() / 8 > 0xFF) {
                    Fail("bad samplers");
                    return;
                }
                mat.samplerCount = (byte)(buff.size() / 8);
                // the states by name, "*" for all
                if (const rapidjson::Value* states{ Member(user, "samplerStates") }; states && states->IsObject()) {
                    for (auto it{ states->MemberBegin() }; it != states->MemberEnd(); ++it) {
                        std::string_view sname{ it->name.GetString() };
                        uint32_t hash{ MaterialHash(sname) };
                        size_t found{};
                        for (size_t i = 0; i < mat.samplerCount; i++) {
                            uint32_t& shash{ *(uint32_t*)&buff[i * 8] };
                            uint16_t& state{ *(uint16_t*)&buff[i * 8 + 4] };
                            if (sname != "*" && shash != hash) {
                                continue;
                            }
                            found++;
                            const rapidjson::Value& v{ it->value };
                            if (v.IsString() || v.IsNumber()) {
                                state = (uint16_t)(v.IsNumber() ? v.GetUint() : std::strtoul(v.GetString(), nullptr, 0));
                                continue;
                            }
                            if (!v.IsObject()) {
                                continue;
                            }
                            auto Field = [&](const char* field, const auto& names, uint16_t mask, uint16_t shift) {
                                const rapidjson::Value* f{ Member(v, field) };
                                if (!f || !f->IsString()) {
                                    return true;
                                }
                                int idx{ NameIndex(names, f->GetString()) };
                                if (idx < 0) {
                                    Fail(std::format("sampler {}: unknown {} {}", sname, field, f->GetString()));
                                    return false;
                                }
                                state = (uint16_t)((state & ~mask) | (idx << shift));
                                return true;
                            };
                            if (!Field("filter", SAMPLER_FILTERS, SAMPLER_FILTER_MASK, 0) ||
                                !Field("mipmap", SAMPLER_MIPMAPS, SAMPLER_MIPMAP_MASK, SAMPLER_MIPMAP_SHIFT) ||
                                !Field("wrap", SAMPLER_WRAPS, 3 << SAMPLER_WRAP_U_SHIFT, SAMPLER_WRAP_U_SHIFT) ||
                                !Field("wrap", SAMPLER_WRAPS, 3 << SAMPLER_WRAP_V_SHIFT, SAMPLER_WRAP_V_SHIFT) ||
                                !Field("wrapU", SAMPLER_WRAPS, 3 << SAMPLER_WRAP_U_SHIFT, SAMPLER_WRAP_U_SHIFT) ||
                                !Field("wrapV", SAMPLER_WRAPS, 3 << SAMPLER_WRAP_V_SHIFT, SAMPLER_WRAP_V_SHIFT) ||
                                !Field("wrapW", SAMPLER_WRAPS, 3 << SAMPLER_WRAP_W_SHIFT, SAMPLER_WRAP_W_SHIFT)) {
                                return;
                            }
                            if (const rapidjson::Value* b{ Member(v, "borderWhite") }; b && b->IsBool()) {
                                state = (uint16_t)(b->GetBool() ? state | SAMPLER_BORDER_WHITE : state & ~SAMPLER_BORDER_WHITE);
                            }
                        }
                        if (!found) {
                            Fail(std::format("no sampler {} in the template (see its samplerStates)", sname));
                            return;
                        }
                    }
                }
                mat.samplerTable = (uint64_t*)fastfile::linker::memory::POINTER_NEXT;
                ff.data.Align(0x10);
                ff.data.WriteStream(buff.data(), buff.size());
            } else {
                mat.samplerCount = 0;
                mat.samplerTable = nullptr;
            }

            // bindings
            const rapidjson::Value* bindings{ Member(root, "bindings") };
            for (size_t i = 0; i < std::size(mat.bindings); i++) {
                const rapidjson::Value* bind{
                    bindings && bindings->IsArray() && i < bindings->Size() ? &(*bindings)[(rapidjson::SizeType)i]
                                                                           : nullptr
                };
                if (!bind || !bind->IsObject()) {
                    mat.bindings[i] = nullptr;
                    continue;
                }
                if (!ReadHex(Member(*bind, "block"), buff) || buff.size() != sizeof(MaterialBinding)) {
                    Fail("bad binding");
                    return;
                }
                mat.bindings[i] = (MaterialBinding*)fastfile::linker::memory::POINTER_NEXT;
                ff.data.Align(8);
                MaterialBinding& binding{ ff.data.AllocStreamRef<MaterialBinding>() };
                std::memcpy(&binding, buff.data(), sizeof(binding));

                if (ReadHex(Member(*bind, "data"), buff) && !buff.empty()) {
                    if (buff.size() & 3) {
                        Fail("bad binding data");
                        return;
                    }
                    binding.dataCount = (uint32_t)(buff.size() / 4);
                    binding.data = (uint32_t*)fastfile::linker::memory::POINTER_NEXT;
                    ff.data.Align(0x40);
                    ff.data.WriteStream(buff.data(), buff.size());
                } else {
                    binding.dataCount = 0;
                    binding.data = nullptr;
                }

                const rapidjson::Value* images{ Member(*bind, "images") };
                size_t imageCount{ images && images->IsArray() ? images->Size() : 0 };
                binding.imageCount = (uint32_t)imageCount;
                if (imageCount) {
                    binding.images = (MaterialBindingImage*)fastfile::linker::memory::POINTER_NEXT;
                    ff.data.Align(8);
                    MaterialBindingImage* entries{ ff.data.AllocStreamPtr<MaterialBindingImage>(imageCount) };
                    for (size_t j = 0; j < imageCount; j++) {
                        if (!ReadHex(Member((*images)[(rapidjson::SizeType)j], "entry"), buff) ||
                            buff.size() != sizeof(MaterialBindingImage)) {
                            Fail("bad binding image");
                            return;
                        }
                        std::memcpy(&entries[j], buff.data(), sizeof(entries[j]));
                    }
                    for (size_t j = 0; j < imageCount; j++) {
                        LinkImage(Member((*images)[(rapidjson::SizeType)j], "image"), entries[j].image);
                    }
                } else {
                    binding.images = nullptr;
                }
            }

            ff.data.PopStream();

            ff.data.PopStream();
            LOG_INFO("Added asset material {} (hash_{:x})", assetName, mat.name.name);
        }
    };

    utils::MapAdder<XAssetLinkerImpl, XAssetType, XAssetLinker> impl{ GetWorkers(), XAssetType::ASSET_TYPE_MATERIAL };
} // namespace
