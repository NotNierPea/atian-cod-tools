#include <includes.hpp>
#include <tools/fastfile/linkers/linker_bo4.hpp>
#include <utils/compress_utils.hpp>
#define STB_IMAGE_IMPLEMENTATION
#define STB_DXT_IMPLEMENTATION
#define STB_IMAGE_STATIC
#define STB_DXT_STATIC
#include <stb_image.h>
#include <stb_dxt.h>

/*
 * Images, in the fastfile (resident) or streamed from an xpak written next to it.
 *
 * image,name.png          resident, one mip in the fastfile
 * image,name.png,stream   streamed, the mips are in zone/<xpak>.xpak, the fastfile only has the header
 * >image.stream=true      streams every image of the zone (",resident" keeps one in the fastfile)
 * >xpak.name=<name>       name of the xpak, the fastfile name by default
 *
 * A streamed image is cut in up to 4 parts, part 0 is the smallest mips, each next part adds the next bigger
 * mip. The game finds the xpak with the keyvaluepairs asset named after the zone (key xpak_read), added here,
 * and matches the parts with the xpak index by key (XPak_BuildAdjacencyInfo).
 */
namespace {
    using namespace fastfile::linker::bo4;
    enum GfxPixelFormat : int32_t {
        GFX_PF_INVALID = 0x0,

        // for normal non-compressed images
        GFX_PF_R8G8B8A8_T1 = 28,
        GFX_PF_R8G8B8A8_T2 = 29,
        GFX_PF_R8G8B8A8_T3 = 31,
        GFX_PF_R8G8B8A8_T4 = 87,

        // for compressed images
        GFX_PF_BC1 = 72,
        GFX_PF_BC3 = 78,
    };
    enum MapType : byte {
        MAPTYPE_NONE = 0x0,
        MAPTYPE_2D = 0x1,
        MAPTYPE_2D_ARRAY = 0x2,
        MAPTYPE_3D = 0x3,
        MAPTYPE_CUBE = 0x4,
        MAPTYPE_CUBE_ARRAY = 0x5,
        MAPTYPE_COUNT = 0x6,
    };

    // flags: 0-6 xpak slot, 7 compressed, 8 valid, the rest adjacency, all set by the game
    struct XPakEntryInfo {
        uint64_t key;
        int64_t offset;
        uint64_t size;
        uint64_t flags;
    };
    static_assert(sizeof(XPakEntryInfo) == 0x20);

    struct GfxStreamedPartInfo {
        XPakEntryInfo xpakEntry;
        // (bytes of this part and all the smaller ones << 4) | level count
        uint32_t levelCountAndSize;
        uint16_t width;
        uint16_t height;
    };
    static_assert(sizeof(GfxStreamedPartInfo) == 0x28);

    struct GfxImage {
        void* unk0;
        void* dxgiAdapter;
        uintptr_t unk10;
        byte* pixels;
        XHash name;
        GfxStreamedPartInfo* streamedParts;
        uint64_t unk38;
        uint64_t unk40;
        uint64_t unk48;
        uint64_t unk50;
        uint32_t imageFlags;
        uint32_t unk5c;
        uint32_t totalSize;
        GfxPixelFormat imageFormat;
        uint16_t width;
        uint16_t height;
        uint16_t depth;
        uint16_t unk6e;
        byte unk70;
        byte unk71;
        uint16_t unk72;
        uint16_t unk74;
        byte unk76;
        byte alignment;
        byte unk78;
        MapType mapType;
        byte levelCount;
        byte await_stream;
        byte streaming;
        byte streamedParts_count;
        byte unk7e;
        byte unk7f;
        uint64_t unk80;
    };
    static_assert(sizeof(GfxImage) == 0x88);

    struct KeyValuePair {
        uint64_t keyHash;
        const char* value;
    };

    struct KeyValuePairs {
        XHash name;
        int32_t numVariables;
        KeyValuePair* keyValuePairs;
    };
    static_assert(sizeof(KeyValuePairs) == 0x20);

    constexpr uint32_t XPAK_MAGIC = 0x4950414B;
    constexpr uint16_t XPAK_VERSION_BO4 = 0xb;
    constexpr size_t XPAK_HEADER_SIZE = 0x200;
    // the game reads the index and the data by 0x8000
    constexpr size_t XPAK_READ_ALIGN = 0x8000;
    // an entry is chunk groups of 0x80 bytes headers, 30 chunks each
    constexpr size_t XPAK_GROUP_ALIGN = 0x80;
    constexpr size_t XPAK_GROUP_MAX_CHUNKS = 30;
    constexpr size_t XPAK_CHUNK_SIZE = 0x4000;
    constexpr uint32_t XPAK_CHUNK_RAW = 0;
    constexpr uint32_t XPAK_CHUNK_LZ4 = 3;
    constexpr size_t MAX_STREAMED_PARTS = 4;

    struct XPakSection {
        int64_t itemCount;
        int64_t offset;
        int64_t size;
    };

    struct XPakHeader {
        uint32_t magic;
        uint16_t unk4;
        uint16_t version;
        uint32_t revision;
        uint32_t reserved0;
        int64_t size;
        XPakSection dataSection;
        XPakSection indexSection;
        XPakSection referenceSection;
        XPakSection metaSection;
        char comment[0x188];
    };
    static_assert(sizeof(XPakHeader) == XPAK_HEADER_SIZE);

    struct XPakIndexEntry {
        uint64_t key;
        int64_t offset;
        // bit 63: compressed
        int64_t size;
    };
    static_assert(sizeof(XPakIndexEntry) == 0x18);

    struct XPakItem {
        uint64_t key;
        std::vector<byte> data;
    };

    // the streamed parts of a fastfile, written to its xpak in ComputeFinal
    std::unordered_map<BO4FFContext*, std::vector<XPakItem>> xpakItems{};

    // one entry: the data cut in chunks, lz4 when it is smaller, raw else
    void EncodeXPakEntry(const byte* data, size_t len, std::vector<byte>& out, bool& compressed) {
        compressed = false;
        size_t srcOffset{};
        while (srcOffset < len) {
            size_t groupStart{ out.size() };
            out.resize(groupStart + XPAK_GROUP_ALIGN);

            uint32_t chunks{};
            uint32_t destOffset{ (uint32_t)srcOffset };
            uint32_t cmds[XPAK_GROUP_MAX_CHUNKS]{};

            while (chunks < XPAK_GROUP_MAX_CHUNKS && srcOffset < len) {
                size_t chunkLen{ std::min(XPAK_CHUNK_SIZE, len - srcOffset) };
                const byte* chunk{ data + srcOffset };

                size_t outLen{ utils::compress::GetCompressSize(utils::compress::COMP_LZ4, chunkLen) };
                std::vector<byte> buff(outLen);
                bool lz4{ utils::compress::Compress(
                              (utils::compress::CompressionAlgorithm)(utils::compress::COMP_LZ4 |
                                                                      utils::compress::COMP_HIGH_COMPRESSION),
                              buff.data(),
                              &outLen,
                              chunk,
                              chunkLen
                          ) &&
                          outLen < chunkLen };

                if (lz4) {
                    out.insert(out.end(), buff.begin(), buff.begin() + outLen);
                    cmds[chunks] = (XPAK_CHUNK_LZ4 << 24) | (uint32_t)outLen;
                    compressed = true;
                } else {
                    out.insert(out.end(), chunk, chunk + chunkLen);
                    cmds[chunks] = (XPAK_CHUNK_RAW << 24) | (uint32_t)chunkLen;
                }
                srcOffset += chunkLen;
                chunks++;
            }

            uint32_t* header{ (uint32_t*)&out[groupStart] };
            header[0] = chunks;
            header[1] = destOffset;
            std::memcpy(&header[2], cmds, sizeof(cmds));

            out.resize(utils::Aligned<size_t>(out.size(), XPAK_GROUP_ALIGN));
        }
    }

    void WriteXPak(const std::filesystem::path& path, std::vector<XPakItem>& items) {
        std::sort(items.begin(), items.end(), [](const XPakItem& a, const XPakItem& b) { return a.key < b.key; });

        for (size_t i = 1; i < items.size(); i++) {
            if (items[i].key == items[i - 1].key) {
                throw std::runtime_error(std::format("Two streamed parts with the key 0x{:x}", items[i].key));
            }
        }

        std::vector<byte> file(XPAK_HEADER_SIZE);
        std::vector<XPakIndexEntry> index{};

        for (XPakItem& item : items) {
            file.resize(utils::Aligned<size_t>(file.size(), XPAK_GROUP_ALIGN));
            size_t start{ file.size() };
            bool compressed;
            EncodeXPakEntry(item.data.data(), item.data.size(), file, compressed);

            XPakIndexEntry& entry{ index.emplace_back() };
            entry.key = item.key;
            entry.offset = (int64_t)(start - XPAK_HEADER_SIZE);
            entry.size = (int64_t)(file.size() - start);
            if (compressed) {
                entry.size |= (int64_t)(1ull << 63);
            }
        }
        size_t dataEnd{ file.size() };

        // index sorted by key, the game walks it along the sorted parts
        file.resize(utils::Aligned<size_t>(file.size(), XPAK_READ_ALIGN));
        size_t indexOffset{ file.size() };
        size_t indexSize{ index.size() * sizeof(XPakIndexEntry) };
        file.resize(indexOffset + indexSize);
        std::memcpy(&file[indexOffset], index.data(), indexSize);
        file.resize(utils::Aligned<size_t>(file.size(), XPAK_READ_ALIGN));

        XPakHeader& header{ *(XPakHeader*)file.data() };
        header.magic = XPAK_MAGIC;
        header.version = XPAK_VERSION_BO4;
        header.size = (int64_t)file.size();
        header.dataSection.itemCount = (int64_t)index.size();
        header.dataSection.offset = XPAK_HEADER_SIZE;
        header.dataSection.size = (int64_t)(dataEnd - XPAK_HEADER_SIZE);
        header.indexSection.itemCount = (int64_t)index.size();
        header.indexSection.offset = (int64_t)indexOffset;
        header.indexSection.size = (int64_t)indexSize;
        snprintf(header.comment, sizeof(header.comment), "atian-cod-tools");

        std::filesystem::create_directories(path.parent_path());
        if (!utils::WriteFile(path, file)) {
            throw std::runtime_error(std::format("Can't write xpak {}", path.string()));
        }
        LOG_INFO("Written xpak {} with {} entries (0x{:x} bytes)", path.string(), index.size(), file.size());
    }

    // 4x4 blocks of an rgba picture, the edges padded with zeros
    void CompressBC(const byte* img, uint32_t x, uint32_t y, bool hasAlpha, std::vector<byte>& out) {
        uint32_t blockW{ (x + 3) / 4 };
        uint32_t blockH{ (y + 3) / 4 };
        size_t blockSize{ hasAlpha ? (size_t)16 : (size_t)8 };
        size_t start{ out.size() };
        out.resize(start + blockW * blockH * blockSize);

        for (uint32_t by = 0; by < blockH; by++) {
            for (uint32_t bx = 0; bx < blockW; bx++) {
                byte block[4 * 4 * 4]{}; // 4x4 RGBA, zero-padded for edge blocks
                for (uint32_t py = 0; py < 4; py++) {
                    uint32_t sy{ by * 4 + py };
                    if (sy >= y)
                        break;
                    for (uint32_t px = 0; px < 4; px++) {
                        uint32_t sx{ bx * 4 + px };
                        if (sx >= x)
                            break;
                        std::memcpy(&block[(py * 4 + px) * 4], &img[(sy * x + sx) * 4], 4);
                    }
                }
                stb_compress_dxt_block(
                    &out[start + (by * blockW + bx) * blockSize],
                    block,
                    hasAlpha ? 1 : 0,
                    STB_DXT_HIGHQUAL
                );
            }
        }
    }

    // half size rgba, 2x2 box filter
    std::vector<byte> HalfSize(const std::vector<byte>& img, uint32_t x, uint32_t y, uint32_t nx, uint32_t ny) {
        std::vector<byte> out(nx * ny * 4);
        for (uint32_t dy = 0; dy < ny; dy++) {
            for (uint32_t dx = 0; dx < nx; dx++) {
                uint32_t sx0{ std::min(dx * 2, x - 1) };
                uint32_t sx1{ std::min(dx * 2 + 1, x - 1) };
                uint32_t sy0{ std::min(dy * 2, y - 1) };
                uint32_t sy1{ std::min(dy * 2 + 1, y - 1) };
                for (uint32_t c = 0; c < 4; c++) {
                    uint32_t v{ (uint32_t)img[(sy0 * x + sx0) * 4 + c] + img[(sy0 * x + sx1) * 4 + c] +
                                img[(sy1 * x + sx0) * 4 + c] + img[(sy1 * x + sx1) * 4 + c] };
                    out[(dy * nx + dx) * 4 + c] = (byte)((v + 2) / 4);
                }
            }
        }
        return out;
    }

    class XAssetLinkerImpl : public XAssetLinker {
      public:
        using XAssetLinker::XAssetLinker;

        void ComputeFinal(BO4LinkContext& ctx, BO4FFContext& ff) override {
            auto it{ xpakItems.find(&ff) };
            if (it == xpakItems.end()) {
                return;
            }
            std::vector<XPakItem> items{ std::move(it->second) };
            xpakItems.erase(it);
            if (items.empty()) {
                return;
            }

            // a prefixed fastfile (en_, ...) gets its own xpak
            const char* xpakName{ &ff == &ctx.mainFF ? ctx.linkCtx.zone.GetConfig("xpak.name", ff.ffname) : ff.ffname };

            WriteXPak(ctx.linkCtx.opt.m_output / "zone" / std::format("{}.xpak", xpakName), items);

            // XPak_AddPackfilesForZone: values of xpak_read in the keyvaluepairs named after the zone
            ff.data.PushStream(XFILE_BLOCK_TEMP);
            ff.data.AddAsset(XAssetType::ASSET_TYPE_KEYVALUEPAIRS, ff.data.Align(8));

            KeyValuePairs& table{ ff.data.AllocStreamRef<KeyValuePairs>() };
            table.name.name = ff.ffnameHash;
            table.numVariables = 1;
            table.keyValuePairs = (KeyValuePair*)fastfile::linker::memory::POINTER_NEXT;

            ff.data.PushStream(XFILE_BLOCK_VIRTUAL);
            ff.data.Align(8);
            KeyValuePair* kvp{ ff.data.AllocStreamPtr<KeyValuePair>(1) };
            kvp->keyHash = ::hash::HashKVP("xpak_read");
            ff.data.AddXString(kvp->value, xpakName);
            ff.data.PopStream();

            ff.data.PopStream();

            LOG_INFO("Added asset keyvaluepairs {} (xpak_read={})", ff.ffname, xpakName);
        }

        void Compute(BO4LinkContext& ctx, const char* id, BO4FFContext& ff) override {
            if (*id == '#')
                id++; // ignore start #

            // name.png[,stream|,resident]
            std::string idStr{ id };
            bool stream{ ctx.linkCtx.zone.GetConfigBool("image.stream", false) };
            size_t opt{ idStr.find_last_of(',') };
            if (opt != std::string::npos) {
                std::string optName{ idStr.substr(opt + 1) };
                if (optName == "stream" || optName == "streamed") {
                    stream = true;
                } else if (optName == "resident") {
                    stream = false;
                } else {
                    LOG_ERROR("Unknown image option '{}' for {}", optName, idStr);
                    ctx.error = true;
                    return;
                }
                idStr.resize(opt);
            }

            std::filesystem::path path{ ctx.linkCtx.input / "images" / idStr };
            std::filesystem::path filename{ path.filename() };
            filename.replace_extension();
            std::string name{ filename.string() };
            uint64_t hash{ ctx.HashXHash(name.data()) };
            std::string pathStr{ path.string() };

            // header
            ff.data.PushStream(XFILE_BLOCK_TEMP);
            GfxImage& gfx{ ff.data.AllocStreamRef<GfxImage>() };
            gfx.name.name = hash;

            LOG_TRACE("Processing image {} ({}) 0x{:x}", name, path.string(), hash);
            int x, y, channels;
            // always rgba, whatever the file has
            stbi_uc* img{ stbi_load(pathStr.data(), &x, &y, &channels, 4) };

            if (!img) {
                ctx.error = true;
                LOG_ERROR("Can't process image {}: {}", path.string(), stbi_failure_reason());
                return;
            }

            utils::CloseEnd imgce{ [img]() { stbi_image_free(img); } };

            gfx.unk0 = 0;
            gfx.mapType = MAPTYPE_2D;
            gfx.depth = 1;
            gfx.pixels = (byte*)fastfile::linker::memory::POINTER_NEXT;

            LOG_TRACE("Image: {}x{}x{}", x, y, channels);

            uint32_t blockW{ (uint32_t)((x + 3) / 4) };
            uint32_t blockH{ (uint32_t)((y + 3) / 4) };

            gfx.width = (uint16_t)(blockW * 4);
            gfx.height = (uint16_t)(blockH * 4);

            bool hasAlpha{ false };
            for (int i = 0; i < x * y && !hasAlpha; i++) {
                if (img[i * 4 + 3] != 255)
                    hasAlpha = true;
            }
            gfx.imageFormat = (GfxPixelFormat)(hasAlpha ? GFX_PF_BC3 : GFX_PF_BC1);

            if (!stream) {
                std::vector<byte> compressed{};
                CompressBC(img, (uint32_t)x, (uint32_t)y, hasAlpha, compressed);

                gfx.levelCount = 1;
                gfx.totalSize = (uint32_t)compressed.size();

                ff.data.PushStream(XFILE_BLOCK_PHYSICAL);
                // pixels, DB_AllocStreamPos(0xFF)
                ff.data.Align(0x100);
                ff.data.WriteStream(compressed.data(), gfx.totalSize);
                ff.data.PopStream();

                ff.data.PopStream();
                LOG_INFO("Added asset gfximage {} (hash_{:x})", idStr, hash);
                return;
            }

            // the picture padded to blocks of 4, the edges repeated
            uint32_t width{ gfx.width };
            uint32_t height{ gfx.height };
            std::vector<byte> top(width * height * 4);
            for (uint32_t py = 0; py < height; py++) {
                uint32_t sy{ std::min(py, (uint32_t)y - 1) };
                for (uint32_t px = 0; px < width; px++) {
                    uint32_t sx{ std::min(px, (uint32_t)x - 1) };
                    std::memcpy(&top[(py * width + px) * 4], &img[(sy * x + sx) * 4], 4);
                }
            }

            // mips down to 4 pixels on the small side, like the game's
            uint32_t levels{ 1 };
            while ((std::min(width, height) >> levels) >= 4) {
                levels++;
            }

            // each part is a texture of its own: it starts on blocks of 4 too
            uint32_t parts{ (uint32_t)std::min<size_t>(MAX_STREAMED_PARTS, levels) };
            while (parts > 1 && (((width >> (parts - 1)) & 3) || ((height >> (parts - 1)) & 3))) {
                parts--;
            }

            // mip 0 first, then the smaller ones
            std::vector<byte> pixels{};
            std::vector<size_t> mipOffsets{};
            {
                std::vector<byte> level{ top };
                uint32_t lx{ width };
                uint32_t ly{ height };
                for (uint32_t i = 0; i < levels; i++) {
                    mipOffsets.push_back(pixels.size());
                    CompressBC(level.data(), lx, ly, hasAlpha, pixels);
                    if (i + 1 < levels) {
                        uint32_t nx{ std::max(1u, lx >> 1) };
                        uint32_t ny{ std::max(1u, ly >> 1) };
                        level = HalfSize(level, lx, ly, nx, ny);
                        lx = nx;
                        ly = ny;
                    }
                }
            }
            size_t totalSize{ pixels.size() };

            if (totalSize >= (1ull << 28)) {
                LOG_ERROR("Image {} is too big to be streamed", idStr);
                ctx.error = true;
                return;
            }

            gfx.levelCount = (byte)levels;
            gfx.totalSize = (uint32_t)totalSize;
            gfx.streaming = 1;
            gfx.streamedParts_count = (byte)parts;
            gfx.streamedParts = (GfxStreamedPartInfo*)fastfile::linker::memory::POINTER_NEXT;

            GfxStreamedPartInfo partInfo[MAX_STREAMED_PARTS]{};
            std::vector<XPakItem>& items{ xpakItems[&ff] };
            size_t lastCumulative{};
            for (uint32_t k = 0; k < parts; k++) {
                // the biggest mip of this part
                uint32_t topMip{ parts - 1 - k };
                size_t cumulative{ totalSize - mipOffsets[topMip] };
                uint32_t partLevels{ levels - topMip };

                GfxStreamedPartInfo& part{ partInfo[k] };
                part.xpakEntry.key = ::hash::Hash64(std::format("{}/streamed_part_{}", name, k).data());
                part.levelCountAndSize = (uint32_t)(cumulative << 4) | partLevels;
                part.width = (uint16_t)(width >> topMip);
                part.height = (uint16_t)(height >> topMip);

                // what this part adds, the game puts it at totalSize - cumulative (Image_PartOffset)
                XPakItem& item{ items.emplace_back() };
                item.key = part.xpakEntry.key;
                item.data.assign(
                    pixels.begin() + (totalSize - cumulative), pixels.begin() + (totalSize - lastCumulative)
                );
                lastCumulative = cumulative;
            }

            ff.data.PushStream(XFILE_BLOCK_VIRTUAL);

            // pixels, in the streamer block, nothing in the file
            ff.data.PushStream(XFILE_BLOCK_STREAMER);
            ff.data.Align(0x10000);
            ff.data.AllocRuntime(totalSize);
            ff.data.PopStream();

            // streamed parts
            ff.data.Align(8);
            ff.data.WriteStream(partInfo, sizeof(partInfo[0]) * parts);

            ff.data.PopStream();

            ff.data.PopStream();
            LOG_INFO(
                "Added asset gfximage {} (hash_{:x}) streamed, {}x{} {} mips in {} parts",
                idStr,
                hash,
                width,
                height,
                levels,
                parts
            );
        }
    };

    utils::MapAdder<XAssetLinkerImpl, XAssetType, XAssetLinker> impl{ GetWorkers(), XAssetType::ASSET_TYPE_IMAGE };
} // namespace

namespace fastfile::linker::bo4 {
    void AddXPakItem(BO4FFContext& ff, uint64_t key, std::vector<byte>&& data) {
        XPakItem& item{ xpakItems[&ff].emplace_back() };
        item.key = key;
        item.data = std::move(data);
    }
} // namespace fastfile::linker::bo4
