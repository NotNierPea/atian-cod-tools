#include <includes.hpp>
#include <tools/fastfile/linkers/linker_bo4.hpp>

/*
 * lighting (GfxWorldLighting, retail 0x1F8, the loader sub_142E214B0): the renderer world's light data, named as the
 * d3dbsp. gfx_map points to it at 0x17F8 and loads it inline (linker_bo4_map_gfx.cpp links it there); nothing looks it
 * up by name. The streamer's world start (sub_143720AF0) reads the list at 0x10/0x18 without a null check.
 * Header in TEMP, the rest in VIRTUAL. Arrays (count, pointer): 0x10/0x18, 0x20/0x28 (0x10), 0x90/0x98 (0x1D0, images),
 * 0xA0..0x138, 0x158/0x160 (0x168), 0x168/0x170 (0x10). Images (type 9): 4 groups of 3 at 0x30, 4 at 0x178 (inline
 * 0x80 struct, loaded last). The first group and the 4 at 0x178 are the same in the frontend and zm_towers: global
 * images. The images are linked by name to the game's.
 *
 * The lighting states (0x10/0x18, 0x14C0 each, sub_142E1E260): cgame picks one each frame (sub_140E46980) and its
 * readers take it as set, the world needs one. A state from a game map's (the dumper's lighting_state<i>.bin): its
 * plain values to 0x10A0, the pointers after (probes, models, images...) none. The 4 pointers at 0xF10 + 0x80 * i can't
 * be null, the load writes their stream key's 0x38 (sub_1422B7AF0): each an object (0x70, sub_142E1E130) with only its
 * stream key (0x48, asset type 0x9F, sub_142E2E0C0) without data. Each frame reads an atlas of one (0x58 or 0x68,
 * sub_1435F75D0, its 0x28): with "objectImage" an object is the frontend's, its atlases without their u16 list.
 * The list at 0x90/0x98 (0x1D0 each, after the images at 0x30): the frame takes one (sub_1436825F0, the first when none
 * matches) without a count check. One element in the game's maps: zeros but 4 objects at 0x10 + 0x70 * j + 0x60 (one
 * shared there), with the load's write in their key (sub_1422B7B00). "list90": one, its objects as the states'.
 * The 4 records at 0x1240 (0x30, sub_142E1D5D0): 3 images, the streamed data handle at 0x18 (0x48, sub_142E2E380), u32s
 * at 0x20/0x28. The streamer's start reads the handle's id of one (sub_143727120, sub_14372A060) and a handle without
 * data has none (-1, sub_143729A70): each a handle of type 0 (1 points the 3 images' data in it, sub_143729BA0),
 * resident (0x39 = 0, the data in block 5), 16 bytes. The images are the "recordImages", the game's.
 * The records are the light grid: 3 volume images (3D) the models take their ambient light from. The game's maps put
 * them inline (pointer -1, a 0x88 GfxImage without pixels, sub_142E1D5D0), the low qualities 1x1x1 RGBA32F whose texel
 * is in the handle's data (type 1, 0x30). The frontend's aren't loaded in a match: with "lightGrid" (the 3 texels,
 * 12 floats) each record has its own 3 images, a handle of type 1 with them and the u32 list (0x20/0x28) of one 0.
 * The 4 lightmap sets at 0x10E0 (0x58 by quality, sub_142E1E260's loop): images count 0 and list 8, 0x144s at 0x10
 * ([0x50] + 1), 0x25Cs at 0x18 ([0x54]), inline 0x30 at 0x20. The streamer's start takes an image of the one picked
 * (sub_1436116F0). As the frontend's low qualities: the "lightmapImages" (2) and "lightmapData" (2 0x144s) in all 4.
 * The sets are the reflection probes, their images the probes' cubemaps (inline as the records', sub_142E1D140). The lit
 * shaders take the model's ambient as the grid times the probe array (t8, a cube array at mip 6), the set's first image
 * while the probe array isn't ready (sub_14367CEB0). With "probeColor" (RGB) the images are ours, each a cube array of
 * one 1x1 RGBA32F cube of that colour (mapType 5, sub_1435B9A60 makes a cube array view of it).
 * The exposure is automatic (sub_143699F10): it measures the light at the camera with the state's sun volume (0x13A8,
 * 0x40 by quality: probe count 0, a light level 4, probes 8, bounds 0x10-0x24, sub_143660E50), the level at 4 when the
 * camera is outside or there are no probes. 0 is the most exposure there is, a too bright sky: "exposure" is that level
 * (zm_towers 2511.2, the frontend 0.205), in all 4.
 * The sky (0x1308, 0x28 by quality, sub_142E1DED0): the model 0x00, a rotation 0x08 (zm_towers 258) and a distance 0x0C
 * (99999, the frontend 8000), the image 0x18 (zm_towers' a lat-long sky), a lens flare 0x20 (KLF, zm_towers' sun). The
 * streamer keeps the model's mesh and images in (sub_143715220). "sky": { "model", "image", "rotation", "distance",
 * "lensFlare" } for the 4 qualities, or an array of them by quality.
 *
 * {
 *     "name": "maps/zm/enh_test.d3dbsp",
 *     "states": [ "gfx/lighting_state0.bin" ],
 *     "recordImages": [ "hash_166d6ffe2807ab51", "hash_59d29182468b73ea", "hash_708399d5eb727933" ],
 *     "lightmapImages": [ "hash_795361a2816b9223", "hash_795360a2816b9070" ],
 *     "lightmapData": "gfx/lighting_lightmap_144.bin",
 *     "objectImage": "hash_e4117858d2aa27b",
 *     "list90": true,
 *     "images30": [ "hash_2a12fae3f8cef40", ... ],   // up to 12
 *     "images178": [ "hash_5c02282b50567cdd", ... ]  // up to 4
 * }
 */
namespace {
    using namespace fastfile::linker::bo4;

    constexpr size_t LIGHTING_SIZE = 0x1F8;
    constexpr size_t STATE_SIZE = 0x14C0;
    constexpr size_t STATE_PLAIN_END = 0x10A0;
    constexpr size_t STATE_OBJECT_SIZE = 0x70;
    constexpr size_t STREAM_KEY_SIZE = 0x48;
    constexpr size_t RECORDS = 0x1240;
    constexpr size_t RECORD_HANDLE_SIZE = 0x48;
    constexpr size_t RECORD_DATA_SIZE = 0x10;
    constexpr size_t LIGHTMAPS = 0x10E0;
    constexpr size_t LIGHTMAP_ENTRY_SIZE = 0x144;
    constexpr size_t LIST90_SIZE = 0x1D0;
    constexpr uint32_t ATLAS_LIST_COUNT = 0x55554;
    // the sun shadow data's parts, each on 64 KB (sub_1422B7C30): its u32s, the 0x58 atlas's u16s, its image's pixels
    constexpr size_t Align64K(size_t v) { return (v + 0xFFFF) & ~(size_t)0xFFFF; }
    constexpr size_t ShadowListOffset(uint32_t nodes) { return Align64K(4 * (size_t)nodes); }
    constexpr size_t ShadowImageOffset(uint32_t nodes) { return ShadowListOffset(nodes) + Align64K(2 * (size_t)ATLAS_LIST_COUNT); }
    constexpr size_t ShadowDataSize(uint32_t nodes) { return Align64K(ShadowImageOffset(nodes) + 2 * (size_t)ATLAS_LIST_COUNT); }
    /*
     * The sparse shadow tree's leaf for "nothing in the way": bit 30 a leaf, its occluder depth float(word << 2) = 131072
     * in sun space (the lit shaders' far path: shadowed when it's under the receiver's depth); 0 reads as an inner node
     * down to a depth 0 leaf, shadowed
     */
    constexpr uint32_t SHADOW_LIT_LEAF = 0x52000000;
    constexpr size_t IMAGE_SIZE = 0x88;
    constexpr size_t LIGHT_GRID_FLOATS = 12;
    constexpr size_t PROBE_FACES = 6;
    constexpr size_t SUN_VOLUMES = 0x13A8;
    constexpr size_t SKY_ENTRIES = 0x1308;
    constexpr size_t SKY_ENTRY_SIZE = 0x28;
    // the game's low quality grid handles have it at 0x18
    constexpr uint64_t LIGHT_GRID_KEY = 0xef176b74f5d7d291;

    class XAssetLinkerImpl : public XAssetLinker {
      public:
        using XAssetLinker::XAssetLinker;

        void Compute(BO4LinkContext& ctx, const char* id, BO4FFContext& ff) override {
            std::filesystem::path path{ ctx.linkCtx.input / id };
            BO4Json cfg{ path };
            if (!cfg.SyncConfig(false)) {
                LOG_ERROR("Can't read lighting {}", path.string());
                ctx.error = true;
                return;
            }
            std::string assetName{ cfg.GetString("name", "") };
            if (assetName.empty()) {
                LOG_ERROR("lighting {}: no name (maps/<mode>/<map>.d3dbsp)", path.string());
                ctx.error = true;
                return;
            }
            auto ReadNames = [&](const char* key, std::vector<std::string>& out, size_t max) {
                auto it{ cfg.main.FindMember(key) };
                if (it == cfg.main.MemberEnd() || !it->value.IsArray()) {
                    return true;
                }
                for (auto& v : it->value.GetArray()) {
                    if (!v.IsString() || out.size() == max) {
                        LOG_ERROR("lighting {}: {} takes up to {} image names", path.string(), key, max);
                        return false;
                    }
                    out.emplace_back(v.GetString());
                }
                return true;
            };
            std::vector<std::string> images30{};
            std::vector<std::string> images178{};
            std::vector<std::string> recordImages{};
            std::vector<std::string> lightmapImages{};
            if (!ReadNames("images30", images30, 12) || !ReadNames("images178", images178, 4) || !ReadNames("recordImages", recordImages, 3)
                || !ReadNames("lightmapImages", lightmapImages, 64)) {
                ctx.error = true;
                return;
            }
            // the image of the objects' atlases (0x58/0x68), without it the objects have only their stream key
            std::string objectImage{ cfg.GetString("objectImage", "") };
            // the 0x90 list (0x1D0 each): one element, as the frontend's and zm_towers'
            bool list90{ cfg.GetBool("list90", false) };
            // the states' sun shadow data: an empty one in the xpak (see WriteObject), its 0x58 atlas image from this
            // template (a game atlas image's 0x88, the dumper's lighting_state<i>_obj<j>_58_image.bin); needs "objectImage"
            std::string shadowImageFile{ cfg.GetString("shadowImage", "") };
            std::string shadowImage{};
            if (!shadowImageFile.empty() && (!utils::ReadFile(path.parent_path() / shadowImageFile, shadowImage) || shadowImage.size() != IMAGE_SIZE)) {
                LOG_ERROR("lighting {}: can't read {} or it isn't 0x{:x} bytes", path.string(), shadowImageFile, IMAGE_SIZE);
                ctx.error = true;
                return;
            }
            std::string lightmapData{};
            std::string lightmapDataFile{ cfg.GetString("lightmapData", "") };
            if (!lightmapDataFile.empty() && (!utils::ReadFile(path.parent_path() / lightmapDataFile, lightmapData) || lightmapData.empty()
                || lightmapData.size() % LIGHTMAP_ENTRY_SIZE)) {
                LOG_ERROR("lighting {}: can't read {} or it isn't 0x{:x} entries", path.string(), lightmapDataFile, LIGHTMAP_ENTRY_SIZE);
                ctx.error = true;
                return;
            }

            // the light grid's 3 texels (RGBA each), see above
            std::vector<float> lightGrid{};
            auto gridIt{ cfg.main.FindMember("lightGrid") };
            if (gridIt != cfg.main.MemberEnd()) {
                if (!gridIt->value.IsArray() || gridIt->value.Size() != LIGHT_GRID_FLOATS) {
                    LOG_ERROR("lighting {}: lightGrid takes {} numbers", path.string(), LIGHT_GRID_FLOATS);
                    ctx.error = true;
                    return;
                }
                for (auto& v : gridIt->value.GetArray()) {
                    lightGrid.push_back(v.IsNumber() ? v.GetFloat() : 0.0f);
                }
            }
            const size_t recordImageCount{ lightGrid.empty() ? recordImages.size() : 3 };
            // the probes' colour (RGB), see above
            std::vector<float> probeColor{};
            auto probeIt{ cfg.main.FindMember("probeColor") };
            if (probeIt != cfg.main.MemberEnd()) {
                if (!probeIt->value.IsArray() || probeIt->value.Size() != 3) {
                    LOG_ERROR("lighting {}: probeColor takes 3 numbers", path.string());
                    ctx.error = true;
                    return;
                }
                for (auto& v : probeIt->value.GetArray()) {
                    probeColor.push_back(v.IsNumber() ? v.GetFloat() : 0.0f);
                }
            }
            const size_t probeImageCount{ probeColor.empty() ? lightmapImages.size() : std::max<size_t>(lightmapImages.size(), 1) };
            // the light level the exposure takes without sun volume probes, see above
            const float exposure{ (float)cfg.GetDouble("exposure", 0.0) };
            /*
             * the sky (see above) by quality: an object for the 4 entries or an array of up to 4 (the last one repeated):
             * its model and image by name, rotation and distance, "lensFlare" the sun's KLF as the gfx_map dumper writes it
             * (lighting_state<i>_sky<j>_klf.json, inline once, its images by name)
             */
            struct SkyEntry {
                std::string model{};
                std::string image{};
                float rotation{};
                float distance{ 8000 };
                std::shared_ptr<rapidjson::Document> lensFlare{};
            };
            SkyEntry sky[4]{};
            auto ReadSky = [&](const rapidjson::Value& s, SkyEntry& out) {
                out.model = s.HasMember("model") && s["model"].IsString() ? s["model"].GetString() : "";
                out.image = s.HasMember("image") && s["image"].IsString() ? s["image"].GetString() : "";
                out.rotation = s.HasMember("rotation") && s["rotation"].IsNumber() ? s["rotation"].GetFloat() : 0;
                out.distance = s.HasMember("distance") && s["distance"].IsNumber() ? s["distance"].GetFloat() : 8000;
                if (s.HasMember("lensFlare") && s["lensFlare"].IsString()) {
                    std::string data{};
                    out.lensFlare = std::make_shared<rapidjson::Document>();
                    if (!utils::ReadFile(path.parent_path() / s["lensFlare"].GetString(), data) || out.lensFlare->Parse(data.data(), data.size()).HasParseError()) {
                        LOG_ERROR("lighting {}: can't read the lens flare {}", path.string(), s["lensFlare"].GetString());
                        return false;
                    }
                }
                return true;
            };
            auto skyIt{ cfg.main.FindMember("sky") };
            if (skyIt != cfg.main.MemberEnd() && skyIt->value.IsObject()) {
                if (!ReadSky(skyIt->value, sky[0])) {
                    ctx.error = true;
                    return;
                }
                for (size_t j = 1; j < 4; j++) {
                    sky[j] = sky[0];
                }
            } else if (skyIt != cfg.main.MemberEnd() && skyIt->value.IsArray() && !skyIt->value.Empty()) {
                for (size_t j = 0; j < 4; j++) {
                    if (j >= skyIt->value.Size()) {
                        sky[j] = sky[j - 1];
                    } else if (!skyIt->value[(rapidjson::SizeType)j].IsObject() || !ReadSky(skyIt->value[(rapidjson::SizeType)j], sky[j])) {
                        LOG_ERROR("lighting {}: bad sky entry {}", path.string(), j);
                        ctx.error = true;
                        return;
                    }
                }
            }

            std::vector<std::string> states{};
            // each state's tree size by quality (the u32s of its objects' shadow data)
            std::vector<std::array<uint32_t, 4>> shadowNodes{};
            auto statesIt{ cfg.main.FindMember("states") };
            if (statesIt != cfg.main.MemberEnd() && statesIt->value.IsArray()) {
                for (auto& v : statesIt->value.GetArray()) {
                    std::string data{};
                    if (!v.IsString() || !utils::ReadFile(path.parent_path() / v.GetString(), data) || data.size() != STATE_SIZE) {
                        LOG_ERROR("lighting {}: a state isn't a 0x{:x} file", path.string(), STATE_SIZE);
                        ctx.error = true;
                        return;
                    }
                    // the plain values only, then the objects and the records' handles (and images)
                    std::memset(data.data() + STATE_PLAIN_END, 0, STATE_SIZE - STATE_PLAIN_END);
                    /*
                     * The sun shadow block by quality (0xEA0 + 0x80 * j, its object at 0x70; the view's copy at +0x22E0): the
                     * sun matrix 0x10-0x48, the tree's root grid at u32 0x04 of the data (zm_towers' its part 1, 0x01ADB033),
                     * cut 0x08 times a side in its cells, 0x50 x 0x54 cells (floats) of 128 * 0x58 units, 0x5C the depth
                     * range. The shaders' far path reads root[floor(y * cut) * x cells * cut + floor(x * cut)]: with
                     * "shadowImage" the root is at 0 and the data is that many lit leaves (a read past the data is 0, shadowed)
                     */
                    std::array<uint32_t, 4> nodes{ 1, 1, 1, 1 };
                    if (!shadowImage.empty()) {
                        for (size_t j = 0; j < 4; j++) {
                            byte* block{ (byte*)data.data() + 0xEA0 + 0x80 * j };
                            uint64_t cells{ (uint64_t)std::max(0.0f, *(float*)(block + 0x50)) * (uint64_t)std::max(0.0f, *(float*)(block + 0x54)) };
                            uint64_t cut{ *(uint32_t*)(block + 0x08) };
                            nodes[j] = (uint32_t)std::clamp<uint64_t>(cells * cut * cut, 1, 0x1000000);
                            *(uint32_t*)(block + 0x04) = 0;
                        }
                    }
                    shadowNodes.push_back(nodes);
                    for (size_t j = 0; j < 4; j++) {
                        *(uint64_t*)(data.data() + 0xF10 + 0x80 * j) = fastfile::linker::memory::POINTER_NEXT;
                        byte* record{ (byte*)data.data() + RECORDS + 0x30 * j };
                        for (size_t k = 0; k < recordImageCount; k++) {
                            *(uint64_t*)(record + 8 * k) = fastfile::linker::memory::POINTER_NEXT;
                        }
                        *(uint64_t*)(record + 0x18) = fastfile::linker::memory::POINTER_NEXT;
                        if (!lightGrid.empty()) {
                            *(uint64_t*)(record + 0x20) = 1;
                            *(uint64_t*)(record + 0x28) = fastfile::linker::memory::POINTER_NEXT;
                        }

                        *(float*)(data.data() + SUN_VOLUMES + 0x40 * j + 4) = exposure;

                        byte* skyEntry{ (byte*)data.data() + SKY_ENTRIES + SKY_ENTRY_SIZE * j };
                        if (!sky[j].model.empty()) {
                            *(uint64_t*)skyEntry = fastfile::linker::memory::POINTER_NEXT;
                            *(float*)(skyEntry + 0x8) = sky[j].rotation;
                            *(float*)(skyEntry + 0xC) = sky[j].distance;
                        }
                        if (!sky[j].image.empty()) {
                            *(uint64_t*)(skyEntry + 0x18) = fastfile::linker::memory::POINTER_NEXT;
                        }
                        if (sky[j].lensFlare) {
                            *(uint64_t*)(skyEntry + 0x20) = fastfile::linker::memory::POINTER_NEXT;
                        }

                        byte* lightmap{ (byte*)data.data() + LIGHTMAPS + 0x58 * j };
                        if (probeImageCount) {
                            *(uint64_t*)lightmap = probeImageCount;
                            *(uint64_t*)(lightmap + 8) = fastfile::linker::memory::POINTER_NEXT;
                        }
                        if (!lightmapData.empty()) {
                            *(uint64_t*)(lightmap + 0x10) = fastfile::linker::memory::POINTER_NEXT;
                            *(uint32_t*)(lightmap + 0x50) = (uint32_t)(lightmapData.size() / LIGHTMAP_ENTRY_SIZE - 1);
                        }
                    }
                    states.emplace_back(std::move(data));
                }
            }

            // the groups at 0x30 are the grid's images by quality as the records': the first state's, in place of "images30"
            if (!lightGrid.empty() && !states.empty()) {
                images30.clear();
                for (size_t j = 0; j < 4; j++) {
                    for (size_t k = 0; k < 3; k++) {
                        images30.emplace_back(std::format("{}_lightgrid0_{}_{}", assetName, j, k));
                    }
                }
            }

            ff.data.PushStream(XFILE_BLOCK_TEMP);
            ff.data.Align(8);
            byte* header{ ff.data.AllocStreamPtr<byte>(LIGHTING_SIZE) };
            std::memset(header, 0, LIGHTING_SIZE);
            const uint64_t nameHash{ ctx.HashXHash(assetName, true) };
            ((XHash*)header)->name = nameHash;
            if (!states.empty()) {
                *(uint32_t*)(header + 0x10) = (uint32_t)states.size();
                *(uint64_t*)(header + 0x18) = fastfile::linker::memory::POINTER_NEXT;
            }
            if (list90) {
                *(uint32_t*)(header + 0x90) = 1;
                *(uint64_t*)(header + 0x98) = fastfile::linker::memory::POINTER_NEXT;
            }

            // an object (0x70, sub_142E1E130): its stream key (in TEMP), with "objectImage" the frontend's (see above)
            auto WriteObject = [&](const std::string& keyName, byte owner, uint32_t nodes) {
                const size_t shadowListOffset{ ShadowListOffset(nodes) };
                const size_t shadowImageOffset{ ShadowImageOffset(nodes) };
                const size_t shadowDataSize{ ShadowDataSize(nodes) };
                ff.data.Align(16);
                byte object[STATE_OBJECT_SIZE]{};
                *(uint64_t*)object = fastfile::linker::memory::POINTER_NEXT;
                if (!objectImage.empty()) {
                    // as the frontend's: 2 u32s (count 0x1C 1), the atlas 0x58/0x68, the 4x1 texture 0x60
                    *(uint64_t*)(object + 0x8) = fastfile::linker::memory::POINTER_NEXT;
                    *(uint64_t*)(object + 0x10) = fastfile::linker::memory::POINTER_NEXT;
                    // the u32s in the shadow data (0x18) and the parts (0x1C, their offsets 0x8 and counts 0x10)
                    *(uint32_t*)(object + 0x18) = shadowImage.empty() ? 1 : nodes;
                    *(uint32_t*)(object + 0x1C) = 1;
                    *(uint64_t*)(object + 0x58) = fastfile::linker::memory::POINTER_NEXT;
                    *(uint64_t*)(object + 0x60) = fastfile::linker::memory::POINTER_NEXT;
                    *(uint64_t*)(object + 0x68) = fastfile::linker::memory::POINTER_NEXT;
                }
                ff.data.WriteStream(object, sizeof(object));

                /*
                 * the game's: 0x44 64, streamed (flags 0), no data. The owner (0x46) picks how the key is let go at the unload
                 * (sub_1422B7B90): 2 a state's (the object at its owner + 0x70), 3 the 0x90 list's (at + 0x60); the wrong one
                 * reads a null object there and crashes leaving the map
                 */
                ff.data.PushStream(XFILE_BLOCK_TEMP);
                ff.data.Align(8);
                byte key[STREAM_KEY_SIZE]{};
                ((XHash*)key)->name = ctx.HashXHash(keyName, true);
                *(uint16_t*)(key + 0x44) = 0x40;
                key[0x46] = owner;
                /*
                 * With "shadowImage" the key has the sun shadow data, as zm_towers' low qualities: streamed (flags 0) from the
                 * xpak into the streamer block (sub_142E2DFE0), 64 KB aligned. Once in, its callback (sub_1422B7C30) makes the
                 * 0x58 atlas's list and image out of it and a GPU buffer of its u32s at the object's 0x28, which the lighting
                 * pass binds (sub_1435F8D20), and the frame takes the 0x58 atlas (sub_1435F75D0). The data: the u32s (the
                 * tree's root grid of lit leaves, see the states), the atlas's 0x55554 u16s and its image's (0xFFFF).
                 */
                std::vector<byte> shadowData{};
                if (!shadowImage.empty()) {
                    shadowData.resize(shadowDataSize);
                    std::fill_n((uint32_t*)shadowData.data(), nodes, SHADOW_LIT_LEAF);
                    std::fill_n((uint16_t*)(shadowData.data() + shadowListOffset), ATLAS_LIST_COUNT, (uint16_t)0xFFFF);
                    std::fill_n((uint16_t*)(shadowData.data() + shadowImageOffset), ATLAS_LIST_COUNT, (uint16_t)0xFFFF);
                    *(uint64_t*)(key + 0x10) = ::hash::Hash64(keyName.data());
                    *(uint64_t*)(key + 0x30) = fastfile::linker::memory::POINTER_NEXT;
                    *(uint32_t*)(key + 0x40) = (uint32_t)shadowDataSize;
                }
                ff.data.WriteStream(key, sizeof(key));
                if (!shadowImage.empty()) {
                    ff.data.PushStream(XFILE_BLOCK_VIRTUAL);
                    ff.data.PushStream(XFILE_BLOCK_STREAMER);
                    ff.data.Align(0x10000);
                    ff.data.AllocRuntime(shadowDataSize);
                    ff.data.PopStream();
                    ff.data.PopStream();
                    AddXPakItem(ff, *(uint64_t*)(key + 0x10), std::move(shadowData));
                }
                ff.data.PopStream();

                if (objectImage.empty()) {
                    return;
                }
                // the loader's order (sub_142E1E130): 0x8, 0x10, 0x58, 0x60, 0x68
                ff.data.Align(4);
                ff.data.WriteStream<uint32_t>(0);
                ff.data.Align(4);
                ff.data.WriteStream<uint32_t>(shadowImage.empty() ? 0 : nodes);
                auto Atlas = [&]() {
                    // the frontend's 0x58 (sub_142E1D2E0): 512x512, 1/512, 0x55554 u16s, the image, 393216. The streamer
                    // reads the u16s as a grid (width at 0, sub_14372B030 from sub_14372EC80) where a state's values put the
                    // camera in it: a list of zeros, the frontend's 0x58 has none, its 0x68 has one
                    ff.data.Align(8);
                    byte atlas[0x30]{};
                    *(uint32_t*)(atlas + 0x0) = 0x200;
                    *(uint32_t*)(atlas + 0x4) = 0x200;
                    *(uint32_t*)(atlas + 0x8) = 0x3B004020;
                    *(uint32_t*)(atlas + 0xC) = 0x3B004020;
                    *(uint32_t*)(atlas + 0x10) = ATLAS_LIST_COUNT;
                    *(uint64_t*)(atlas + 0x18) = fastfile::linker::memory::POINTER_NEXT;
                    *(uint64_t*)(atlas + 0x20) = fastfile::linker::memory::POINTER_NEXT;
                    *(float*)(atlas + 0x28) = 393216.0f;
                    ff.data.WriteStream(atlas, sizeof(atlas));
                    ff.data.Align(2);
                    // no page resident (0xFFFF, as zm_towers' 0x68): zeros pointed every page at the cache's first tile,
                    // which with a state's sun shadow values shadowed everything
                    std::vector<uint16_t> list(ATLAS_LIST_COUNT, 0xFFFF);
                    ff.data.WriteStream(list.data(), list.size() * sizeof(list[0]));
                    void* ref{};
                    ctx.LinkAsset(XAssetType::ASSET_TYPE_IMAGE, std::format(",{}", objectImage).data(), ref, false, &ff);
                };
                if (shadowImage.empty()) {
                    Atlas();
                } else {
                    // the 0x58 atlas of the shadow data: no list (the callback points it in the data), its own image inline
                    // (sub_142E1D2E0: -1, its 0x88 in TEMP, the pixels the callback's)
                    ff.data.Align(8);
                    byte atlas[0x30]{};
                    *(uint32_t*)(atlas + 0x0) = 0x200;
                    *(uint32_t*)(atlas + 0x4) = 0x200;
                    *(uint32_t*)(atlas + 0x8) = 0x3B004020;
                    *(uint32_t*)(atlas + 0xC) = 0x3B004020;
                    *(uint32_t*)(atlas + 0x10) = ATLAS_LIST_COUNT;
                    *(uint64_t*)(atlas + 0x20) = fastfile::linker::memory::POINTER_NEXT;
                    *(float*)(atlas + 0x28) = 393216.0f;
                    ff.data.WriteStream(atlas, sizeof(atlas));
                    ff.data.PushStream(XFILE_BLOCK_TEMP);
                    ff.data.Align(8);
                    std::string image{ shadowImage };
                    std::memset(image.data(), 0, 0x20);
                    *(uint64_t*)(image.data() + 0x20) = ctx.HashXHash(keyName + "_image", true);
                    ff.data.WriteStream(image.data(), image.size());
                    ff.data.PopStream();
                }
                // the texture (sub_142E1DBE0): data 4x1 bytes in block 5 on 256
                ff.data.Align(16);
                byte texture[0x40]{};
                *(uint64_t*)texture = fastfile::linker::memory::POINTER_NEXT;
                *(uint32_t*)(texture + 0x8) = 4;
                *(uint32_t*)(texture + 0xC) = 1;
                ff.data.WriteStream(texture, sizeof(texture));
                ff.data.PushStream(XFILE_BLOCK_PHYSICAL);
                ff.data.Align(0x100);
                // the sun shadow data's fallback while its stream isn't in (the lighting pass binds it, sub_1435F8D20):
                // the game maps' 0x52000000 (a float far away, no occluder); 0 shadowed everything
                ff.data.WriteStream<uint32_t>(0x52000000);
                ff.data.PopStream();
                Atlas();
            };

            ff.data.PushStream(XFILE_BLOCK_VIRTUAL);
            // the states, then for each: its 4 objects (each followed by its stream key, in TEMP), its 4 records
            if (!states.empty()) {
                ff.data.Align(16);
                for (const std::string& state : states) {
                    ff.data.WriteStream(state.data(), state.size());
                }
                for (size_t i = 0; i < states.size(); i++) {
                    for (size_t j = 0; j < 4; j++) {
                        WriteObject(std::format("{}_lighting_state{}_{}", assetName, i, j), 2, shadowNodes[i][j]);
                    }
                    // the lightmap sets: the 0x144s, then the image list and its images (each in TEMP)
                    for (size_t j = 0; j < 4; j++) {
                        if (!lightmapData.empty()) {
                            ff.data.Align(16);
                            ff.data.WriteStream(lightmapData.data(), lightmapData.size());
                        }
                        if (probeImageCount) {
                            ff.data.Align(8);
                            std::vector<uint64_t> list(probeImageCount, fastfile::linker::memory::POINTER_NEXT);
                            ff.data.WriteStream(list.data(), list.size() * sizeof(list[0]));
                        }
                        if (probeColor.empty()) {
                            for (const std::string& image : lightmapImages) {
                                void* ref{};
                                ctx.LinkAsset(XAssetType::ASSET_TYPE_IMAGE, std::format(",{}", image).data(), ref, false, &ff);
                            }
                            continue;
                        }
                        // our own: inline (header in TEMP), a cube array of one 1x1 RGBA32F cube (mapType 5, the view the
                        // lit shaders' probe array takes), resident (its 6 faces in block 5 on 256)
                        for (size_t k = 0; k < probeImageCount; k++) {
                            ff.data.PushStream(XFILE_BLOCK_TEMP);
                            ff.data.Align(8);
                            byte image[IMAGE_SIZE]{};
                            ((XHash*)(image + 0x20))->name = ctx.HashXHash(std::format("{}_probe{}_{}_{}", assetName, i, j, k), true);
                            *(uint64_t*)(image + 0x18) = fastfile::linker::memory::POINTER_NEXT;
                            *(uint32_t*)(image + 0x58) = 0x4;
                            *(uint32_t*)(image + 0x60) = PROBE_FACES * 0x10;
                            *(uint32_t*)(image + 0x64) = 2;
                            *(uint16_t*)(image + 0x68) = 1;
                            *(uint16_t*)(image + 0x6A) = 1;
                            *(uint16_t*)(image + 0x6C) = 1;
                            image[0x79] = 5;
                            image[0x7A] = 1;
                            ff.data.WriteStream(image, sizeof(image));
                            ff.data.PopStream();

                            ff.data.PushStream(XFILE_BLOCK_PHYSICAL);
                            ff.data.Align(0x100);
                            for (size_t f = 0; f < PROBE_FACES; f++) {
                                float texel[4]{ probeColor[0], probeColor[1], probeColor[2], 1.0f };
                                ff.data.WriteStream(texel, sizeof(texel));
                            }
                            ff.data.PopStream();
                        }
                    }
                    for (size_t j = 0; j < 4; j++) {
                        uint64_t firstImage{};
                        if (lightGrid.empty()) {
                            for (const std::string& image : recordImages) {
                                void* ref{};
                                ctx.LinkAsset(XAssetType::ASSET_TYPE_IMAGE, std::format(",{}", image).data(), ref, false, &ff);
                            }
                        } else {
                            // the 3 images inline (in TEMP), as the game's low quality ones: 1x1x1 RGBA32F 3D, no pixels
                            for (size_t k = 0; k < 3; k++) {
                                ff.data.PushStream(XFILE_BLOCK_TEMP);
                                ff.data.Align(8);
                                byte image[IMAGE_SIZE]{};
                                uint64_t imageName{ ctx.HashXHash(std::format("{}_lightgrid{}_{}_{}", assetName, i, j, k), true) };
                                if (!k) {
                                    firstImage = imageName;
                                }
                                ((XHash*)(image + 0x20))->name = imageName;
                                *(uint32_t*)(image + 0x58) = 0xB;
                                *(uint32_t*)(image + 0x60) = 0x10;
                                *(uint32_t*)(image + 0x64) = 2;
                                *(uint16_t*)(image + 0x68) = 1;
                                *(uint16_t*)(image + 0x6A) = 1;
                                *(uint16_t*)(image + 0x6C) = 1;
                                *(uint16_t*)(image + 0x6E) = 0x40;
                                image[0x70] = 1;
                                *(uint16_t*)(image + 0x72) = 1;
                                *(uint16_t*)(image + 0x74) = 0x100;
                                image[0x77] = 4;
                                image[0x79] = 3;
                                image[0x7A] = 1;
                                image[0x7C] = 2;
                                ff.data.WriteStream(image, sizeof(image));
                                ff.data.PopStream();
                            }
                        }
                        // the handle: no id yet (-1), resident, its data in block 5; type 1 points the images in it
                        ff.data.Align(8);
                        byte handle[RECORD_HANDLE_SIZE]{};
                        const size_t dataSize{ lightGrid.empty() ? RECORD_DATA_SIZE : lightGrid.size() * sizeof(float) };
                        ((XHash*)handle)->name = lightGrid.empty() ? ctx.HashXHash(std::format("{}_lighting_record{}_{}", assetName, i, j), true) : firstImage;
                        *(uint32_t*)(handle + 0x10) = 0xFFFFFFFF;
                        if (!lightGrid.empty()) {
                            *(uint64_t*)(handle + 0x18) = LIGHT_GRID_KEY;
                            handle[0x38] = 1;
                        }
                        *(uint32_t*)(handle + 0x3C) = (uint32_t)dataSize;
                        *(uint64_t*)(handle + 0x40) = fastfile::linker::memory::POINTER_NEXT;
                        ff.data.WriteStream(handle, sizeof(handle));
                        ff.data.PushStream(XFILE_BLOCK_PHYSICAL);
                        ff.data.Align(0x8000);
                        if (lightGrid.empty()) {
                            byte recordData[RECORD_DATA_SIZE]{};
                            ff.data.WriteStream(recordData, sizeof(recordData));
                        } else {
                            ff.data.WriteStream(lightGrid.data(), dataSize);
                        }
                        ff.data.PopStream();
                        // the u32 list after the handle (0x28): one 0
                        if (!lightGrid.empty()) {
                            ff.data.Align(4);
                            ff.data.WriteStream<uint32_t>(0);
                        }
                    }
                    // the sky entries after the records (sub_142E1DED0): each its model, then its image, by name, then its KLF
                    for (size_t j = 0; j < 4; j++) {
                        void* ref{};
                        if (!sky[j].model.empty()) {
                            ctx.LinkAsset(XAssetType::ASSET_TYPE_XMODEL, std::format(",{}", sky[j].model).data(), ref, false, &ff);
                        }
                        if (!sky[j].image.empty()) {
                            ctx.LinkAsset(XAssetType::ASSET_TYPE_IMAGE, std::format(",{}", sky[j].image).data(), ref, false, &ff);
                        }
                        if (sky[j].lensFlare && !LinkKlf(ctx, ff, *sky[j].lensFlare, ref)) {
                            LOG_ERROR("lighting {}: bad lens flare in sky entry {}", path.string(), j);
                            ctx.error = true;
                        }
                    }
                }
            }
            // the images by name, in the loader's order: the game's own, the name with the default flag
            for (size_t i = 0; i < images30.size(); i++) {
                ctx.LinkAsset(XAssetType::ASSET_TYPE_IMAGE, std::format(",{}", images30[i]).data(), *(void**)(header + 0x30 + i * 8), false, &ff);
            }
            // the 0x90 list after the images at 0x30: one element (0x1D0), zeros but its 4 objects (at 0x10 + 0x70 * j + 0x60)
            if (list90) {
                ff.data.Align(16);
                byte element[LIST90_SIZE]{};
                for (size_t j = 0; j < 4; j++) {
                    *(uint64_t*)(element + 0x10 + 0x70 * j + 0x60) = fastfile::linker::memory::POINTER_NEXT;
                }
                ff.data.WriteStream(element, sizeof(element));
                for (size_t j = 0; j < 4; j++) {
                    WriteObject(std::format("{}_lighting_list90_{}", assetName, j), 3, 1);
                }
            }
            for (size_t i = 0; i < images178.size(); i++) {
                ctx.LinkAsset(XAssetType::ASSET_TYPE_IMAGE, std::format(",{}", images178[i]).data(), *(void**)(header + 0x178 + i * 8), false, &ff);
            }
            ff.data.PopStream();
            ff.data.PopStream();

            LOG_INFO("Added asset lighting {} (hash_{:x}), {} states, {} images", assetName, nameHash, states.size(), images30.size() + images178.size());
        }
    };

    utils::MapAdder<XAssetLinkerImpl, XAssetType, XAssetLinker> impl{ GetWorkers(), XAssetType::ASSET_TYPE_LIGHTING };
} // namespace
