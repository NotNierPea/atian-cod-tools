#include <includes.hpp>
#include <tools/fastfile/linkers/linker_bo4.hpp>

/*
 * Materials, from a template dumped by the bo4 material unlinker (acts ff ... -a material): the blocks of a game
 * material as they are, the technique set and the images by name. Images are references ("#name") or images of
 * the zone ("file.png[,stream]", from the images directory), "replace" swaps them everywhere:
 *
 * {
 *     "name": "my_material",
 *     "replace": { "#old_image": "my_image.png,stream" },
 *     ... the dumped template ...
 * }
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
            rapidjson::Value& root{ cfg.main };

            std::vector<byte> buff{};
            auto Fail = [&ctx, &path](const char* what) {
                LOG_ERROR("Invalid material {}: {}", path.string(), what);
                ctx.error = true;
            };

            if (!ReadHex(Member(root, "header"), buff) || buff.size() != sizeof(Material)) {
                Fail("bad header");
                return;
            }

            std::unordered_map<std::string, std::string> replace{};
            if (const rapidjson::Value* rep{ Member(root, "replace") }; rep && rep->IsObject()) {
                for (auto it{ rep->MemberBegin() }; it != rep->MemberEnd(); ++it) {
                    if (it->value.IsString()) {
                        replace[it->name.GetString()] = it->value.GetString();
                    }
                }
            }

            // images of the zone are linked in place once, then referenced by name
            std::unordered_set<std::string> linkedImages{};
            auto LinkImage = [&](const rapidjson::Value* ref, void*& ptr) {
                if (!ref || !ref->IsString() || !*ref->GetString()) {
                    ptr = nullptr;
                    return;
                }
                std::string val{ ref->GetString() };
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
                    LinkImage(Member((*textures)[(rapidjson::SizeType)i], "image"), defs[i].image);
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
