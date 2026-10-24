#include <includes.hpp>
#include <game_data.hpp>
#include <utils/enumlist.hpp>
#include <tools/fastfile/fastfile_handlers.hpp>
#include <tools/fastfile/fastfile_dump.hpp>
#include <tools/fastfile/fastfile_compiled_zone.hpp>
#include <tools/utils/data_utils.hpp>
#include <hook/module_mapper.hpp>
#include <hook/memory.hpp>
#include <hook/error.hpp>
#include <tools/fastfile/fastfile_asset_pool.hpp>
#include <games/bo4/pool.hpp>
#include <decryptutils.hpp>
#include <tools/fastfile/handlers/handler_game_bo4.hpp>
#include <tools/fastfile/handlers/handler_game_bo4_load.hpp>

namespace fastfile::handlers::bo4 {
    namespace {
        using namespace games::bo4::pool;
        using namespace fastfile::handlers::bo4;

        class BO4FFHandler;

        struct StreamPosInfo {
            byte* pos;
            XFileBlock index;
        };

        struct SafeLoc {
            const char* name;
            byte* buffer;
            size_t len;
        };

        struct {
            BO4FFHandler* handler{};
            void (*Load_XAsset)(bool atStreamStart, XAsset_0* asset){};

            fastfile::FastFileOption* opt{};
            std::vector<char*> xstrings{};
            fastfile::FastFileContext* ctx{};
            core::bytebuffer::ByteBuffer* reader{};
            byte* data{};
            size_t dataLen{};
            utils::OutFileCE* outAssetNames{};
            utils::OutFileCE* outXHash{};
            size_t loaded{};
            core::memory_allocator::MemoryAllocator allocator{};
            std::unique_ptr<fastfile::compiled_zone::CompiledZone> compiledZone{};
            std::unordered_map<uint64_t, void*> linkedAssets[XAssetType::ASSET_TYPE_COUNT]{};
            XAssetList_0 assetList{};
            bool isRunning{};
            utils::EnumList<XAssetType, XAssetType::ASSET_TYPE_COUNT> handleList{ games::bo4::pool::XAssetIdFromName };

            // replace data
            XBlock* g_streamBlocks{};
            byte* g_streamPos{};
            XFileBlock g_streamPosIndex{};
            byte* g_streamRead[XFileBlock::XFILE_BLOCK_COUNT2]{};
            byte* g_streamPosArray[XFileBlock::XFILE_BLOCK_COUNT2]{};
            int g_streamPosStackIdx{};
            XBlock g_loadblocks[XFileBlock::XFILE_BLOCK_COUNT2]{};
            std::vector<SafeLoc> safeLocs{};
            StreamPosInfo g_streamPosStack[64]{};
            // where a preload of this zone would end in XFILE_BLOCK_TEMP_PRELOAD: the Preload_* loaders push it where the
            // Load_* ones push XFILE_BLOCK_TEMP, and it doesn't rewind (aligned to 16 at a pop)
            size_t preloadPos{};
            // the game's module, for the return addresses of its loaders
            byte* gameBase{};
            // ACTS_OLD_ZONE: a zone of an older build (changelist 3310077, no .fd patch), see OldZoneSize
            bool oldZone{};
            // ACTS_STREAM_LOG=<file>: every Load_Stream of the loaded assets (the game's rva, size, stream position)
            std::ofstream streamLog{};
        } gcx{};

        struct {
            XAsset_0* lastAsset{};
            size_t assetIdx{};
        } errorCtx;
        void PrintErrorContext() {
            if (!gcx.isRunning) {
                return;
            }

            LOG_ERROR("ERROR WHEN DUMPING DATA");
            if (errorCtx.lastAsset) {
                LOG_ERROR(
                    "last asset: {} / {}",
                    XAssetNameFromId(errorCtx.lastAsset->type),
                    errorCtx.lastAsset->header
                );
                LOG_ERROR("index: {} / {}", errorCtx.assetIdx, gcx.assetList.assetCount);
                LOG_ERROR("gcx.g_streamPosStackIdx = {}", gcx.g_streamPosStackIdx);
            }
        }

        inline void DECLSPEC_NORETURN ThrowFastFileError_(const std::string& msg) {
            LOG_LVL(core::logs::LVL_ERROR, msg);
            PrintErrorContext();
            hook::error::DumpStackTraceFrom();
            throw std::runtime_error("fastfile error");
        }

#define ThrowFastFileError(...) ThrowFastFileError_(std::format(__VA_ARGS__))

        const char* XBlockLocPtr(void* ptr) {
            byte* p{ (byte*)ptr };
            for (size_t i = 0; i < XFILE_BLOCK_COUNT2; i++) {
                if (p >= gcx.ctx->blockSizes[i].data && p < gcx.ctx->blockSizes[i].data + gcx.ctx->blockSizes[i].size) {
                    size_t rloc{ (size_t)(p - gcx.ctx->blockSizes[i].data) };
                    return utils::va(
                        "%s+0x%llx/0x%llx",
                        XFileBlockName((XFileBlock)i),
                        rloc,
                        gcx.ctx->blockSizes[i].size
                    );
                }
            }
            return utils::va("%p", ptr);
        }

        byte* Mem_AllocAligned(const char* id, size_t size, size_t alignment, bool streamed) {
            static byte empty{};
            if (!size || streamed) {
                return &empty;
            }
            byte* d{ gcx.allocator.AllocAligned<byte>(size, alignment) };
            gcx.safeLocs.emplace_back(id, d, size);
            return d;
        }
    } // namespace

    void DB_AllocXBlocks(size_t* blockSize, XBlock* blocks) {
        gcx.safeLocs.clear();
        gcx.safeLocs.emplace_back("assetList", (byte*)&gcx.assetList, sizeof(gcx.assetList));
        // gcx.safeLocs.emplace_back("decompressed", gcx.data, gcx.dataLen);
        for (size_t i = 0; i < XFILE_BLOCK_COUNT; i++) {
            XFileBlock block{ (XFileBlock)i };

            blocks[block].data = nullptr;
            blocks[block].size = blockSize[block];

            if (!blocks[block].size) {
                continue;
            }

            switch (GetXFileBlockMemLocation(block)) {
            case XFILE_BLOCK_LOC_VIRTUAL:
                blocks[block].size += 0xF;
                blocks[block].data = Mem_AllocAligned(XFileBlockName(block), blocks[block].size, 0x10000, false);
                break;
            case XFILE_BLOCK_LOC_PHYSICAL:
            case XFILE_BLOCK_LOC_STREAM_CPU:
            case XFILE_BLOCK_LOC_STREAM_MMAP:
                blocks[block].data = Mem_AllocAligned(XFileBlockName(block), blocks[block].size, 0x10000, false);
                break;
            case XFILE_BLOCK_LOC_STREAM_DEV:
                blocks[block].data = Mem_AllocAligned(XFileBlockName(block), blocks[block].size, 0x10000, true);
                break;
            }
        }
    }

    void DB_InitStreams(XBlock* blocks) {
        gcx.g_streamBlocks = blocks;
        gcx.g_streamPos = blocks[XFILE_BLOCK_TEMP].data;
        gcx.g_streamPosIndex = XFILE_BLOCK_TEMP;
        gcx.g_streamPosArray[XFILE_BLOCK_TEMP] = nullptr;
        gcx.g_streamRead[XFILE_BLOCK_TEMP] = 0;

        for (size_t i = 1; i < XFILE_BLOCK_COUNT; i++) {
            gcx.g_streamRead[i] = 0;
            gcx.g_streamPosArray[i] = blocks[i].data;
        }
    }

    static void AssertCanWrite(void* ptr, size_t len) {
        if (!len) {
            return;
        }
        byte* p{ (byte*)ptr };
        for (const SafeLoc& loc : gcx.safeLocs) {
            if (p >= loc.buffer && p < loc.buffer + loc.len) {
                size_t rloc{ (size_t)(p - loc.buffer) };
                if (rloc + len > loc.len) {
                    ThrowFastFileError(
                        "AssertCanWrite: Can't read {}:0x{:x}+0x{:x}, remaining 0x{:x} at {}",
                        loc.name,
                        rloc,
                        len,
                        (int64_t)loc.len - rloc,
                        ptr
                    );
                }
                return;
            }
        }
        ThrowFastFileError("AssertCanWrite: Can't read 0x{:x} at {} (outside data)", len, ptr);
    }

    void DB_LoadXFileData(void* pos, size_t size) {
        // LOG_TRACE("{} DB_LoadXFileData({}, 0x{:x}) -> {}/0x{:x}", hook::library::CodePointer{ _ReturnAddress() },
        // XBlockLocPtr(pos), size);
        if (!gcx.reader->CanRead(size)) {
            hook::error::DumpStackTraceFrom();
        }
        byte* p;
        if (pos) {
            AssertCanWrite(pos, size);
            gcx.reader->Read(pos, size);
            p = (byte*)pos;
        } else {
            p = gcx.reader->ReadPtr<byte>(size);
        }

        size_t s{ std::min<size_t>(size, 0x40 * 8) };
        while (s > 0) {
            size_t ss{ std::min<size_t>(s, 0x40) };
            LOG_TRACE("{:03x} : {}", p - (byte*)pos, utils::data::AsHex(p, ss));
            p += ss;
            s -= ss;
        }
    }

    void DB_IncStreamPos(size_t size) {
        gcx.g_streamPos += size;
        if (gcx.g_streamPosIndex == XFILE_BLOCK_TEMP) {
            gcx.preloadPos += size;
        }
    }

    constexpr size_t LIGHT_PROBE_SIZE = 0x144;
    constexpr size_t OLD_LIGHT_PROBE_SIZE = 0x148;

    // the size a load of the game has in a zone of an older build, by the loader's return address (rva)
    size_t OldZoneSize(size_t rva, size_t size) {
        switch (rva) {
        case 0x2E25DD1: // PhysGeomList's tree (sub_142E25CC0)
        case 0x2E36B77: // XModelCollSurf's tree (Load_XModel)
        case 0x2E37C3A: // clipMap_t's tree (sub_142E37BA0)
        case 0x2E37FA0: // clipMap_t's +0xA0 trees
            // counted in 8 byte units then
            return size * 8;
        case 0x2E1E371: // a lighting section entry's probes (sub_142E1E260)
            return size / LIGHT_PROBE_SIZE * OLD_LIGHT_PROBE_SIZE;
        default:
            return size;
        }
    }

    // the old data of a load made into today's layout where it was read (it got smaller or kept its size)
    void OldZoneConvert(size_t rva, byte* data, size_t size) {
        switch (rva) {
        case 0x2E2EA32:
            // a streamer world's 0x38 entries: +0x10..+0x34 were left with the linker's pointers, they are zeros now
            for (size_t i = 0; i < size / 0x38; i++) {
                std::memset(data + i * 0x38 + 0x10, 0, 0x24);
            }
            break;
        case 0x2E1E371:
            // a probe had a u32 at +0xB0 then, and +0x11C (now a flag, mostly 1) meant something else
            for (size_t i = 0; i < size / LIGHT_PROBE_SIZE; i++) {
                const byte* from{ data + i * OLD_LIGHT_PROBE_SIZE };
                byte* to{ data + i * LIGHT_PROBE_SIZE };
                std::memmove(to, from, 0xB0);
                std::memmove(to + 0xB0, from + 0xB4, 0x6C);
                *(uint32_t*)(to + 0x11C) = 1;
                std::memmove(to + 0x120, from + 0x124, 0x24);
            }
            break;
        }
    }

    bool Load_Stream(bool atStreamStart, void* ptr, size_t size) {
        size_t rva{ (size_t)((byte*)_ReturnAddress() - gcx.gameBase) };
        size_t newSize{ size };
        if (gcx.oldZone) {
            size = OldZoneSize(rva, size);
        }
        if (gcx.streamLog.is_open() && atStreamStart) {
            // and the offset in its block, which the linker's offsets to it (block << 60 | offset + 1) give
            const byte* blockData{ gcx.g_streamBlocks ? gcx.g_streamBlocks[gcx.g_streamPosIndex].data : nullptr };
            gcx.streamLog << std::hex << rva << " " << size << " " << gcx.reader->Loc() << " "
                          << (int)gcx.g_streamPosIndex << " " << (blockData ? (size_t)((byte*)ptr - blockData) : 0) << "\n";
        }
        LOG_TRACE(
            "Load_Stream({}, {}, {}) {}",
            atStreamStart ? "true" : "false",
            ptr,
            size,
            hook::library::CodePointer{ _ReturnAddress() }
        );
        if (!atStreamStart || !size) {
            return true;
        }

        switch (gcx.g_streamPosIndex) {
        case XFILE_BLOCK_TEMP:
        case XFILE_BLOCK_TEMP_PRELOAD:
        case XFILE_BLOCK_VIRTUAL:
        case XFILE_BLOCK_PHYSICAL:
        case XFILE_BLOCK_MEMMAPPED:
        case XFILE_BLOCK_MEMMAPPED2:
            DB_LoadXFileData(ptr, size);
            DB_IncStreamPos(size);
            if (gcx.oldZone) {
                OldZoneConvert(rva, (byte*)ptr, newSize);
            }
            return true;
        case XFILE_BLOCK_RUNTIME_VIRTUAL:
        case XFILE_BLOCK_RUNTIME_PHYSICAL:
            memset(ptr, 0, size);
            DB_IncStreamPos(size);
            return false;
        case XFILE_BLOCK_STREAMER:
        case XFILE_BLOCK_STREAMER_CPU:
            DB_IncStreamPos(size);
            return false;
        default:
            DB_IncStreamPos(size);
            return true;
        }
    }

    /*
     * The loaders of a zone of an older build (ACTS_OLD_ZONE) where a struct changed size: the old data is read where
     * the stream has it, then made into today's layout in memory of its own, so the stream and its offsets stay the
     * old build's. Research for shield-client's old_fastfiles.cpp, which does the same in game.
     */
    namespace old_zone {
        hook::library::Detour lightingDetour{};
        hook::library::Detour lightingSectionsDetour{};
        // the lighting asset being loaded (sub_142E214B0, 0x1F8): its sections at +0x18
        byte* lighting{};

        constexpr size_t SECTION_SIZE = 0x14C0;
        constexpr size_t OLD_SECTION_SIZE = 0x13D0;

        void Lighting(bool atStreamStart, byte* asset) {
            lighting = asset;
            lightingDetour.GetBase<void(bool, byte*)>()(atStreamStart, asset);
            lighting = nullptr;
        }

        // an old section had no 0x30 buffers (sub_142E1C410) at +0x10B0, nor in its 4 entries (0x28 then, 0x58 now) at +0x20
        void ExpandSection(const byte* from, byte* to) {
            std::memcpy(to, from, 0x10B0);
            for (size_t i = 0; i < 4; i++) {
                const byte* entry{ from + 0x10B0 + i * 0x28 };
                byte* out{ to + 0x10E0 + i * 0x58 };
                std::memcpy(out, entry, 0x20);
                std::memcpy(out + 0x50, entry + 0x20, 8);
            }
            std::memcpy(to + 0x1240, from + 0x1150, SECTION_SIZE - 0x1240);
        }

        // sub_142E1E260: the lighting's sections (0x14C0 each)
        void LightingSections(bool atStreamStart, uint64_t count, byte* sections) {
            auto original{ lightingSectionsDetour.GetBase<void(bool, uint64_t, byte*)>() };
            if (!atStreamStart) {
                original(atStreamStart, count, sections);
                return;
            }
            if (!Load_Stream(true, sections, OLD_SECTION_SIZE * count)) {
                return;
            }

            byte* expanded{ gcx.allocator.Alloc<byte>(SECTION_SIZE * count) };
            std::memset(expanded, 0, SECTION_SIZE * count);
            for (size_t i = 0; i < count; i++) {
                ExpandSection(sections + i * OLD_SECTION_SIZE, expanded + i * SECTION_SIZE);
            }
            LOG_INFO("old zone: lighting sections at block 4 +0x{:x} (0x{:x} old bytes), header refs +0x48 {:x} +0x50 {:x}",
                     (size_t)(sections - gcx.g_streamBlocks[4].data), OLD_SECTION_SIZE * count,
                     lighting ? *(uint64_t*)(lighting + 0x48) : 0, lighting ? *(uint64_t*)(lighting + 0x50) : 0);
            if (lighting && *(byte**)(lighting + 0x18) == sections) {
                *(byte**)(lighting + 0x18) = expanded;
            }
            original(false, count, expanded);
        }

        hook::library::Detour worldDetour{};

        constexpr size_t WORLD_SIZE = 0x1AB0;
        constexpr size_t OLD_WORLD_SIZE = 0x1A88;

        /*
         * An old GfxWorld: the 0x28 struct of sub_142E2E1B0 (texture, materials, images) was 0x18 then, with no images;
         * it is at +0x1808 and in the 0x68 struct at +0x1830 (sub_142E1E070, 0x50 then), whose u32s after it had no
         * zero at +0x3C nor after the bounds (+0x64)
         */
        void ExpandWorld(const byte* from, byte* to) {
            std::memcpy(to, from, 0x1820);
            std::memcpy(to + 0x1830, from + 0x1820, 0x10);
            std::memcpy(to + 0x1840, from + 0x1830, 0x18);
            std::memcpy(to + 0x1868, from + 0x1848, 4);
            std::memcpy(to + 0x1870, from + 0x184C, 0x24);
            std::memcpy(to + 0x1898, from + 0x1870, WORLD_SIZE - 0x1898);
        }

        // sub_142E1E830: the GfxWorld (gfx_map)
        char World(bool atStreamStart, byte* world) {
            auto original{ worldDetour.GetBase<char(bool, byte*)>() };
            if (!atStreamStart) {
                return original(atStreamStart, world);
            }
            if (!Load_Stream(true, world, OLD_WORLD_SIZE)) {
                return 0;
            }

            byte* expanded{ gcx.allocator.Alloc<byte>(WORLD_SIZE) };
            std::memset(expanded, 0, WORLD_SIZE);
            ExpandWorld(world, expanded);
            return original(false, expanded);
        }

        hook::library::Detour modelInfoDetour{};

        constexpr size_t MODEL_INFO_TABLE_SIZE = 0x4438;
        constexpr size_t OLD_MODEL_INFO_TABLE_SIZE = 0x4420;

        template<typename T>
        T Game(size_t rva) {
            return reinterpret_cast<T>(gcx.gameBase + rva);
        }

        /*
         * sub_142E17CD0 (0x200, a static model's and sub_142E37BA0's): its 0x4B entries of 0xE8 at +0x20 were 0x18 bytes
         * fewer in all. Every table seen was zeros, so where they were is unknown: the old bytes come first.
         */
        void ModelInfo(bool atStreamStart, byte* info) {
            auto fixString{ Game<void (*)(void*)>(0x2EBC430) };
            if (atStreamStart && !Load_Stream(true, info, 0x200)) {
                return;
            }
            if (*(byte**)(info + 0x18)) {
                byte* p{ (byte*)DB_AllocStreamPos(7) };
                *(byte**)(info + 0x18) = p;
                if (Load_Stream(true, p, 0x390)) {
                    for (size_t i = 0; i < 0x20; i++) {
                        fixString(p + i * 0x1C);
                    }
                }
            }
            if (*(byte**)(info + 0x20)) {
                byte* p{ (byte*)DB_AllocStreamPos(7) };
                byte* table{ gcx.allocator.Alloc<byte>(MODEL_INFO_TABLE_SIZE) };
                std::memset(table, 0, MODEL_INFO_TABLE_SIZE);
                *(byte**)(info + 0x20) = table;
                if (Load_Stream(true, p, OLD_MODEL_INFO_TABLE_SIZE)) {
                    std::memcpy(table, p, OLD_MODEL_INFO_TABLE_SIZE);
                    for (size_t i = 0; i < OLD_MODEL_INFO_TABLE_SIZE; i++) {
                        if (p[i]) {
                            LOG_WARNING("old zone: a model info table that isn't zeros, its layout is a guess");
                            break;
                        }
                    }
                    for (size_t i = 0; i < 0x4B; i++) {
                        fixString(table + 8 + i * 0xE8);
                    }
                }
            }
            if (*(byte**)(info + 0x30)) {
                byte* p{ (byte*)DB_AllocStreamPos(7) };
                *(byte**)(info + 0x30) = p;
                Game<void (*)(bool, uint64_t, byte*)>(0x2E37360)(true, info[0x1F6], p);
            }
            if (*(byte**)(info + 0x38)) {
                byte* p{ (byte*)DB_AllocStreamPos(1) };
                *(byte**)(info + 0x38) = p;
                Load_Stream(true, p, 2ull * info[0x1F6]);
            }
            if (*(byte**)(info + 0x40)) {
                byte* p{ (byte*)DB_AllocStreamPos(3) };
                *(byte**)(info + 0x40) = p;
                Load_Stream(true, p, 0x20ull * info[0x1F9]);
            }
            Game<void (*)(bool, byte*, byte*)>(0x2E17E70)(false, info + 0x50, info);
            if (*(byte**)(info + 0x1C8)) {
                byte* p{ (byte*)DB_AllocStreamPos(1) };
                *(byte**)(info + 0x1C8) = p;
                Load_Stream(true, p, 4ull * (*(uint32_t*)(info + 0x1D0) + 1));
            }
        }

        hook::library::Detour worldPartsDetour{};

        constexpr size_t WORLD_PART_SIZE = 0x50;
        constexpr size_t OLD_WORLD_PART_SIZE = 0x40;

        // sub_142E1E070 (GfxWorld +0x1830, inline): its 0x50 parts were 0x40, the sub_142E2E1B0 struct at +8 0x18 then
        void WorldParts(bool atStreamStart, byte* parts) {
            auto loadMaterials{ Game<void (*)(bool, byte*)>(0x2E2E1B0) };
            if (atStreamStart) {
                LOG_WARNING("old zone: sub_142E1E070 loaded by itself, its old size isn't known");
            }
            uint64_t count{ *(uint64_t*)parts };
            if (*(byte**)(parts + 8)) {
                byte* p{ (byte*)DB_AllocStreamPos(7) };
                byte* expanded{ gcx.allocator.Alloc<byte>(WORLD_PART_SIZE * count) };
                std::memset(expanded, 0, WORLD_PART_SIZE * count);
                *(byte**)(parts + 8) = expanded;
                if (Load_Stream(true, p, OLD_WORLD_PART_SIZE * count)) {
                    for (size_t i = 0; i < count; i++) {
                        const byte* from{ p + i * OLD_WORLD_PART_SIZE };
                        byte* to{ expanded + i * WORLD_PART_SIZE };
                        std::memcpy(to, from, 0x20);
                        std::memcpy(to + 0x30, from + 0x20, 0x20);
                        loadMaterials(false, to + 8);
                    }
                }
            }
            loadMaterials(false, parts + 0x10);
        }

        hook::library::Detour clipMapDetour{};

        constexpr size_t CLIPMAP_SIZE = 0x2D8;
        constexpr size_t OLD_CLIPMAP_SIZE = 0x2C0;

        /*
         * An old clipMap_t had no counts and arrays at +0x128..+0x147 (u32 0x128, u32 0x12C, its 4 * (n + 1) at 0x130,
         * u32 0x138, its 2 * n at 0x140): a u16 at +0x128, then the four u16 counts of +0x148 at +0x12A
         */
        void ExpandClipMap(const byte* from, byte* to) {
            std::memcpy(to, from, 0x128);
            std::memcpy(to + 0x148, from + 0x12A, 8);
            std::memcpy(to + 0x150, from + 0x138, CLIPMAP_SIZE - 0x150);
        }

        // sub_142E37BA0: the clipMap_t (col_map)
        char ClipMap(bool atStreamStart, byte* clipMap) {
            auto original{ clipMapDetour.GetBase<char(bool, byte*)>() };
            if (!atStreamStart) {
                return original(atStreamStart, clipMap);
            }
            if (!Load_Stream(true, clipMap, OLD_CLIPMAP_SIZE)) {
                return 0;
            }

            byte* expanded{ gcx.allocator.Alloc<byte>(CLIPMAP_SIZE) };
            std::memset(expanded, 0, CLIPMAP_SIZE);
            ExpandClipMap(clipMap, expanded);
            return original(false, expanded);
        }

        hook::library::Detour streamerWorldDetour{};

        constexpr size_t STREAMER_WORLD_SIZE = 0x100;
        constexpr size_t OLD_STREAMER_WORLD_SIZE = 0xF0;

        /*
         * sub_142E2E760: the StreamerWorld (streamerworld). Its {count, array} pairs at +0xC8.. were at +0xB8: one of the
         * pairs at +0x98 / +0xA8 / +0xB8 wasn't there, all three empty in the old zones seen, so the one at +0xB8 it is
         */
        char StreamerWorld(bool atStreamStart, byte* world) {
            auto original{ streamerWorldDetour.GetBase<char(bool, byte*)>() };
            if (!atStreamStart) {
                return original(atStreamStart, world);
            }
            if (!Load_Stream(true, world, OLD_STREAMER_WORLD_SIZE)) {
                return 0;
            }

            byte* expanded{ gcx.allocator.Alloc<byte>(STREAMER_WORLD_SIZE) };
            std::memset(expanded, 0, STREAMER_WORLD_SIZE);
            std::memcpy(expanded, world, 0xB8);
            std::memcpy(expanded + 0xC8, world + 0xB8, OLD_STREAMER_WORLD_SIZE - 0xB8);
            return original(false, expanded);
        }

        void Create() {
            streamerWorldDetour.Create(gcx.gameBase + 0x2E2E760, (void*)StreamerWorld);
            clipMapDetour.Create(gcx.gameBase + 0x2E37BA0, (void*)ClipMap);
            lightingDetour.Create(gcx.gameBase + 0x2E214B0, (void*)Lighting);
            lightingSectionsDetour.Create(gcx.gameBase + 0x2E1E260, (void*)LightingSections);
            worldDetour.Create(gcx.gameBase + 0x2E1E830, (void*)World);
            modelInfoDetour.Create(gcx.gameBase + 0x2E17CD0, (void*)ModelInfo);
            worldPartsDetour.Create(gcx.gameBase + 0x2E1E070, (void*)WorldParts);
        }
    }

    void* DB_AllocStreamPos(int align) {
        LOG_TRACE("DB_AllocStreamPos({}) {}", align, hook::library::CodePointer{ _ReturnAddress() });
        byte* streamPos{ gcx.g_streamPos };
        byte* newLoc{ utils::Aligned(gcx.g_streamPos, (size_t)align + 1) };
        gcx.g_streamPos = newLoc;
        // add align padding
        gcx.g_streamRead[gcx.g_streamPosIndex] += newLoc - streamPos;
        if (gcx.g_streamPosIndex == XFILE_BLOCK_TEMP) {
            gcx.preloadPos = utils::Aligned(gcx.preloadPos, (size_t)align + 1);
        }
        return newLoc;
    }

    void DB_PopStreamPos() {
        LOG_TRACE("DB_PopStreamPos() {}", hook::library::CodePointer{ _ReturnAddress() });
        if (!gcx.g_streamPosStackIdx) {
            ThrowFastFileError("DB_PopStreamPos error: Empty pos stack");
        }

        int newidx{ --gcx.g_streamPosStackIdx };
        byte* pos;
        switch (gcx.g_streamPosIndex) {
        case XFILE_BLOCK_TEMP:
            pos = gcx.g_streamPosStack[newidx].pos;
            gcx.g_streamPos = pos;
            gcx.preloadPos = utils::Aligned(gcx.preloadPos, (size_t)0x10);
            break;
        case XFILE_BLOCK_TEMP_PRELOAD:
            pos = utils::Aligned(gcx.g_streamPos, 0x10);
            gcx.g_streamPos = pos;
            break;
        default:
            pos = gcx.g_streamPos;
            break;
        }

        XFileBlock index{ gcx.g_streamPosStack[newidx].index };
        if (index != gcx.g_streamPosIndex) {
            gcx.g_streamPosArray[gcx.g_streamPosIndex] = pos;
            gcx.g_streamPos = gcx.g_streamPosArray[index];
            gcx.g_streamPosIndex = index;
        }
    }

    bool DB_EnableMemMappedData() { return false; }

    void DB_PushStreamPos(XFileBlock index) {
        LOG_TRACE("DB_PushStreamPos({}) {}", XFileBlockName(index), hook::library::CodePointer{ _ReturnAddress() });
        XFileBlock prevIndex{ gcx.g_streamPosIndex };
        XFileBlock newIndex{ index };

        if (index == XFILE_BLOCK_MEMMAPPED) {
            if (!DB_EnableMemMappedData()) {
                newIndex = gcx.g_streamPosIndex;
            }
        }

        if (newIndex == XFILE_BLOCK_MEMMAPPED2) {
            newIndex = XFILE_BLOCK_MEMMAPPED;
        }

        int stackIndex{ gcx.g_streamPosStackIdx++ };
        if (stackIndex == ACTS_ARRAYSIZE(gcx.g_streamPosStack)) {
            ThrowFastFileError("DB_PushStreamPos error: Full pos stack");
        }
        gcx.g_streamPosStack[stackIndex].index = prevIndex;

        byte* pos;
        if (newIndex == prevIndex) {
            pos = gcx.g_streamPos;
        } else {
            gcx.g_streamPosIndex = newIndex;
            gcx.g_streamPosArray[prevIndex] = gcx.g_streamPos;
            pos = gcx.g_streamPosArray[newIndex];
            gcx.g_streamPos = pos;
        }
        gcx.g_streamPosStack[stackIndex].pos = pos;
    }

    const void** DB_InsertPointer() {
        LOG_TRACE("DB_InsertPointer() {}", hook::library::CodePointer{ _ReturnAddress() });
        XFileBlock index{ gcx.g_streamPosIndex };
        int posStackLoc{ gcx.g_streamPosStackIdx };
        gcx.g_streamPosStack[gcx.g_streamPosStackIdx].index = gcx.g_streamPosIndex;

        byte* pos;
        if (index == XFILE_BLOCK_VIRTUAL) {
            pos = gcx.g_streamPos;
        } else {
            // we swap to BLOCK_VIRTUAL to create the pointer
            gcx.g_streamPosArray[index] = gcx.g_streamPos;
            index = XFILE_BLOCK_VIRTUAL;
            pos = gcx.g_streamPosArray[XFILE_BLOCK_VIRTUAL];
            gcx.g_streamPosIndex = XFILE_BLOCK_VIRTUAL;
        }

        gcx.g_streamPosStackIdx = posStackLoc;
        gcx.g_streamPosStack[posStackLoc].pos = pos;

        // add alignment, but not the pointer data, maybe a mistake from them
        byte* oldPos{ pos };
        pos = utils::Aligned<void*>(pos);
        gcx.g_streamRead[XFILE_BLOCK_VIRTUAL] += pos - oldPos;
        gcx.g_streamPos = pos + sizeof(void*);

        XFileBlock currentIndex{ gcx.g_streamPosStack[posStackLoc].index };
        if (currentIndex != index) {
            gcx.g_streamPosArray[index] = gcx.g_streamPos;
            gcx.g_streamPosIndex = currentIndex;
            gcx.g_streamPos = gcx.g_streamPosArray[currentIndex];
        }
        return (const void**)pos;
    }

    void* DB_GetOffsetData(uint64_t ref) {
        XFileBlock block{ (XFileBlock)((ref - 1) >> 60) };
        uint64_t offset{ (ref - 1) & ~0xF000000000000000 };

        void* loc{ &gcx.g_streamBlocks[block].data[offset] };
        LOG_TRACE(
            "DB_GetOffsetData(0x{:x}, {}, 0x{:x}) -> {} {}",
            ref,
            XFileBlockName(block),
            offset,
            loc,
            hook::library::CodePointer{ _ReturnAddress() }
        );
        return loc;
    }

    void DB_ConvertOffsetToPointer(void** data) {
        LOG_TRACE("DB_ConvertOffsetToPointer() {}", hook::library::CodePointer{ _ReturnAddress() });
        *data = DB_GetOffsetData(*(uint64_t*)data);
    }

    void DB_ConvertOffsetToAlias(void** data) {
        LOG_TRACE("DB_ConvertOffsetToAlias() {}", hook::library::CodePointer{ _ReturnAddress() });
        // an inline marker or no block where the loader wants an alias: a stream of another layout (old builds without
        // their .fd)
        uint64_t ref{ *(uint64_t*)data };
        if (ref >= 0xFFFFFFFFFFFFFFFEull || ((ref - 1) >> 60) >= XFILE_BLOCK_COUNT
            || ((ref - 1) & ~0xF000000000000000ull) + 8 > gcx.g_streamBlocks[(ref - 1) >> 60].size) {
            throw std::runtime_error(std::format("DB_ConvertOffsetToAlias of 0x{:x} from {} at stream 0x{:x}", *(uint64_t*)data,
                                                 hook::library::CodePointer{ _ReturnAddress() }, gcx.reader->Loc()));
        }
        *data = *(void**)DB_GetOffsetData(*(uint64_t*)data);
    }

    void Load_XStringCustom(char** str) {
        size_t size;
        char* ptr{ gcx.reader->ReadString(&size) };
        LOG_TRACE(
            "{} Load_XStringCustom({}, 0x{:x}) -> 0x{:x} / {}",
            hook::library::CodePointer{ _ReturnAddress() },
            XBlockLocPtr((void*)str),
            size,
            gcx.reader->Loc(),
            acts::decryptutils::DecryptStringT8(ptr)
        );
        AssertCanWrite(*str, size + 1);
        std::memcpy(*str, ptr, size + 1);
        const char* ds{ acts::decryptutils::DecryptStringT8(*str) };
        std::memmove(*str, ds, std::strlen(ds) + 1);
        hashutils::AddPrecomputed(hash::Hash64(ds), ds, true);
        if (gcx.opt->dumpXStrings) {
            gcx.xstrings.push_back(*str);
        }
        DB_IncStreamPos(size + 1);
    }

    void Load_String(bool atStreamStart, char** handle) {
        if (*handle) {
            if (*handle == (char*)0xFFFFFFFFFFFFFFFF) {
                *handle = AllocStreamPos<char>();
                Load_XStringCustom(handle);
            } else {
                DB_ConvertOffsetToPointer((void**)handle);
            }
        }
    }

    void Load_SimpleAsset_Internal(void** header, games::bo4::pool::XAssetType type) {
        XAsset_0 newEntry{};
        newEntry.type = type;
        newEntry.header = *header;
        *header = DB_LinkXAssetEntry(&newEntry, false)->header;
    }

    XAsset_0* DB_LinkXAssetEntry(XAsset_0* xasset, bool allowOverride) {
        gcx.loaded++;

        if (gcx.opt->assertContainer) {
            // todo: add header name
            // bo4FFHandlerContext.pool.AddAssetHeader(xasset->type, 0, xasset->header);
        }

        XHash* hash{ xasset->header ? games::bo4::pool::GetAssetName(xasset->type, xasset->header, 0) : nullptr };

        // const char* assetName{ hash ? hashutils::ExtractTmp("hash", hash->hash) : "<unknown>" };
        //*bo4FFHandlerContext.osassets << "\n" << XAssetNameFromId(xasset->type) << "," << assetName;
        size_t assetSize{ games::bo4::pool::GetAssetSize(xasset->type) };
        if (!assetSize) {
            LOG_ERROR("Can't link {}: empty asset type", games::bo4::pool::XAssetNameFromId(xasset->type));
            return xasset;
        }
        void* baseHeader{ xasset->header };
        void* linkedHeader{ gcx.allocator.Alloc(assetSize) };

        if (xasset->header) {
            std::memcpy(linkedHeader, xasset->header, assetSize);

            xasset->header = linkedHeader;
        }
        bool defaultAsset{};
        uint64_t name{};

        if (hash && hash->name) {
            defaultAsset = (hash->name & ~hash::MASK63) != 0;
            if (defaultAsset)
                hash->name &= hash::MASK63;
            name = hash->name;
            if (gcx.opt->workflow == FFW_READER) {
                *gcx.outAssetNames << XAssetNameFromId(xasset->type) << "," << (defaultAsset ? "d" : "") << "#"
                                   << hashutils::ExtractTmp("hash", hash->name);
                **gcx.outAssetNames << std::endl;
            }

            gcx.linkedAssets[xasset->type][hash->name] = xasset->header;

            LOG_DEBUG(
                "Loading asset {}({})/{}({:x}) -> {}/{}",
                XAssetNameFromId(xasset->type),
                (int)xasset->type,
                hashutils::ExtractTmp("hash", hash->name),
                hash->name,
                xasset->header,
                XBlockLocPtr(baseHeader)
            );
        }

        fastfile::AddAssetHeader(name, &xasset->header, xasset->type, assetSize);

        if (gcx.opt->noAssetDump || (!gcx.handleList.Empty() && !gcx.handleList[xasset->type]))
            return xasset; // ignore
        if (!gcx.opt->onlyAssets.empty() && !gcx.opt->onlyAssets.contains(name)) {
            return xasset;
        }
        if (xasset->header && !defaultAsset) {
            auto& workers{ fastfile::handlers::bo4::GetWorkers() };
            auto it{ workers.find(xasset->type) };
            if (it != workers.end()) {
                try {
                    if (!it->second->ignoreFull || !gcx.handleList.Empty()) {
                        it->second->Unlink(*gcx.opt, xasset->header);
                    }
                } catch (std::runtime_error& e) {
                    LOG_ERROR(
                        "Can't dump asset asset {}/{}: {}",
                        XAssetNameFromId(xasset->type),
                        (void*)xasset->header,
                        e.what()
                    );
                }
            }
        }
        return xasset;
    }
    namespace {

        void* DB_FindXAssetHeader(XAssetType type, XHash* name, bool errorIfMissing, int waitTime) {
            // an effect by name (the nested effects, sub_142EBB5C0): a zeroed stub with its name, for the fx dumper
            if (type == XAssetType::ASSET_TYPE_FX && name && name->name) {
                static std::unordered_map<uint64_t, std::unique_ptr<uint64_t[]>> fxStubs{};
                std::unique_ptr<uint64_t[]>& stub{ fxStubs[name->name] };
                if (!stub) {
                    stub = std::make_unique<uint64_t[]>(0x20);
                    stub[0] = name->name;
                }
                return stub.get();
            }
            if (!name || name->name)
                return nullptr;
            auto it{ gcx.linkedAssets[type].find(name->name) };

            if (it != gcx.linkedAssets[type].end()) {
                return it->second;
            }

            LOG_WARNING("MISSING XASSET {} : {}", XAssetNameFromId(type), hashutils::ExtractTmp("hash", name->name));

            return nullptr;
        }

        template<size_t offset = 0>
        void EmptyStub() {
            LOG_TRACE("{} EmptyStub<0x{:x}>", hook::library::CodePointer{ _ReturnAddress() }, offset);
        }
        template<size_t offset = 0>
        void ErrorStub() {
            ThrowFastFileError("{} ErrorStub<0x{:x}>", hook::library::CodePointer{ _ReturnAddress() }, offset);
        }

        template<size_t off, bool error = false>
        void RemoveStub(hook::library::Library& lib) {
            if constexpr (error) {
                hook::memory::RedirectJmp(lib[off], &ErrorStub<off>);
            } else {
                hook::memory::RedirectJmp(lib[off], &EmptyStub<off>);
            }
        }

        void Load_XHash(XHash* hash) {
            if (gcx.outXHash && *hash) {
                *gcx.outXHash << hashutils::ExtractTmp("hash", hash->name) << "\n";
            }
        }

        class BO4FFHandler : public fastfile::FFHandler {
          public:
            BO4FFHandler()
                : fastfile::FFHandler("bo4", "Black Ops 4", compatibility::scobalula::csi::CordycepGame::CG_BO4) {
                gcx.handler = this;

                hook::error::AddErrorDumper(PrintErrorContext);
            }

            void Init(fastfile::FastFileOption& opt) override {
                gcx.opt = &opt;

                gcx.handleList.Clear();
                if (opt.assetTypes) {
                    gcx.handleList.LoadConfig(opt.assetTypes);
                }

                acts::game_data::GameData game{ "bo4" };
                commonFiles = game.GetCommonFastFiles();
                hook::module_mapper::Module& mod{
                    opt.GetGameModule(true, nullptr, false, game.GetModuleName(), "bo4")
                };
                hook::scan_container::ScanContainer& scan{ mod.GetScanContainer() };
                game.SetScanContainer(&scan);
                scan.Sync();

                scan.ignoreMissing = true;

                game.Get("fastfile.Load_XAsset", &gcx.Load_XAsset);
                gcx.gameBase = (byte*)**mod;
                gcx.oldZone = std::getenv("ACTS_OLD_ZONE") != nullptr;
                if (gcx.oldZone) {
                    old_zone::Create();
                }
                void* Load_XHash{};
                if (opt.dumpXHash) {
                    game.Get("fastfile.Load_XHash", &Load_XHash);
                }

                game.Redirect("fastfile.Load_Stream", Load_Stream);
                game.Redirect("fastfile.DB_PopStreamPos", DB_PopStreamPos);
                game.Redirect("fastfile.DB_PushStreamPos", DB_PushStreamPos);
                game.Redirect("fastfile.DB_IncStreamPos", DB_IncStreamPos);
                game.Redirect("fastfile.DB_AllocStreamPos", DB_AllocStreamPos);
                game.Redirect("fastfile.DB_ConvertOffsetToPointer", DB_ConvertOffsetToPointer);
                game.Redirect("fastfile.DB_ConvertOffsetToAlias", DB_ConvertOffsetToAlias);
                game.Redirect("fastfile.DB_GetOffsetData", DB_GetOffsetData);
                game.Redirect("fastfile.DB_InsertPointer", DB_InsertPointer);
                game.Redirect("fastfile.DB_AllocXBlocks", DB_AllocXBlocks);
                game.Redirect("fastfile.DB_InitStreams", DB_InitStreams);
                game.Redirect("fastfile.DB_LoadXFileData", DB_LoadXFileData);
                game.Redirect("fastfile.Load_XStringCustom", Load_XStringCustom);
                game.Redirect("fastfile.DB_LinkXAssetEntry", DB_LinkXAssetEntry);
                game.Redirect("fastfile.DB_FindXAssetHeader", DB_FindXAssetHeader);

                game.ApplyNullScans("fastfile");

                scan.Save();
                if (scan.foundMissing) {
                    throw std::runtime_error("Can't find some patterns");
                }

                if (opt.dumpXHash) {
                    // this one is quite heavy, so only when required
                    // Load_XHash is empty, so we can't jump from it
                    void* trampoline{ hook::memory::AllocateNear(Load_XHash, 0x10) };
                    hook::memory::RedirectJmp(trampoline, fastfile::handlers::bo4::Load_XHash, true);

                    size_t patchedXHash{};
                    std::vector<hook::library::ScanResult> calls{ scan.Scan("E8 ?? ?? ?? ??") };

                    for (hook::library::ScanResult& res : calls) {
                        if (res.GetRelative<int32_t>(1) == (byte*)Load_XHash) {
                            void* call{ res.GetPtr(1) };
                            int32_t delta{ (int32_t)((byte*)trampoline - (byte*)call - sizeof(delta)) };
                            hook::process::WriteMemSafe(call, &delta, sizeof(delta));
                            patchedXHash++;
                        }
                    }
                    LOG_TRACE("patched {}/{} Load_XHash call(s)", patchedXHash, calls.size());
                }
            }
            void Cleanup() override { gcx.isRunning = false; }

            void Handle(
                fastfile::FastFileOption& opt, core::bytebuffer::ByteBuffer& reader, fastfile::FastFileContext& ctx
            ) override {
                gcx.ctx = &ctx;
                gcx.reader = &reader;
                gcx.data = reader.Ptr();
                gcx.dataLen = reader.Remaining();
                gcx.isRunning = true;
                utils::CloseEnd end{ [] {
                    gcx.isRunning = false;
                    gcx.compiledZone = nullptr;
                    errorCtx.lastAsset = nullptr;
                } };

                if (opt.dumpCompiledZone) {
                    gcx.compiledZone = std::make_unique<fastfile::compiled_zone::CompiledZone>();
                }

                gcx.loaded = 0;
                gcx.allocator.FreeAll();
                gcx.linkedAssets->clear();

                size_t blockSizes[XFILE_BLOCK_COUNT]{};

                // alloc the blocks
                for (size_t i = 0; i < XFILE_BLOCK_COUNT; i++) {
                    blockSizes[i] = ctx.blockSizes[i].size;
                    LOG_DEBUG("Block size {} = 0x{:x}", XFileBlockName((XFileBlock)i), blockSizes[i]);
                }

                DB_AllocXBlocks(blockSizes, gcx.g_loadblocks);
                DB_InitStreams(gcx.g_loadblocks);
                gcx.preloadPos = 0;

                DB_PushStreamPos(XFILE_BLOCK_VIRTUAL);
                gcx.xstrings.clear();

                XAssetList_0& assetList{ gcx.assetList };
                DB_LoadXFileData(&assetList, sizeof(assetList));
                LOG_DEBUG(
                    "asset list: {} strings, {} assets, the list ends at 0x{:x}",
                    assetList.stringsCount,
                    assetList.assetCount,
                    gcx.reader->Loc()
                );
                DB_PushStreamPos(XFILE_BLOCK_VIRTUAL);

                gcx.opt->assetNames.clear();

                for (auto& [hashType, worker] : GetWorkers()) {
                    worker->PreLoadWorker(&ctx);
                }

                if (assetList.strings) {
                    assetList.strings = AllocStreamPos<char*>();

                    LOG_TRACE("Log strings... {}", (void*)assetList.strings);
                    std::filesystem::path outStrings{ gcx.opt->m_output / "bo4" / "source" / "tables" / "data" /
                                                      "strings" / ctx.GetFFType() / std::format("{}.txt", ctx.ffname) };
                    std::filesystem::create_directories(outStrings.parent_path());
                    utils::OutFileCE os{ outStrings };
                    if (!os) {
                        ThrowFastFileError("Can't open {}", outStrings.string());
                    }

                    if (Load_Stream(true, assetList.strings, sizeof(*assetList.strings) * assetList.stringsCount)) {
                        for (size_t i = 0; i < assetList.stringsCount; i++) {
                            Load_String(false, &assetList.strings[i]);
                            if (assetList.strings[i]) {
                                os << assetList.strings[i] << "\n";
                            }
                        }
                        LOG_OPT_INFO("Dump strings into {}", outStrings.string());
                    }
                }

                utils::OutFileCE osxhash{};
                if (opt.dumpXHash) {
                    std::filesystem::path outXHash{ gcx.opt->m_output / "bo4" / "source" / "tables" / "data" / "xhash" /
                                                    ctx.GetFFType() / std::format("{}.txt", ctx.ffname) };
                    std::filesystem::create_directories(outXHash.parent_path());
                    osxhash->open(outXHash.string());
                    if (!osxhash) {
                        ThrowFastFileError("Can't open {}", outXHash.string());
                    }
                    gcx.outXHash = &osxhash;
                    LOG_OPT_INFO("Dump xhashes into {}", outXHash.string());
                } else {
                    gcx.outXHash = {};
                }

                DB_PopStreamPos();

                if (assetList.assets) {
                    assetList.assets = AllocStreamPos<XAsset_0, 7>();
                    Load_Stream(true, assetList.assets, sizeof(*assetList.assets) * assetList.assetCount);

                    if (!opt.noAssetDump) {
                        for (auto& [t, w] : GetWorkers()) {
                            w->PreXFileLoading(opt, ctx);
                        }
                    }
                    std::filesystem::path outAssets{
                        gcx.opt->m_output / "bo4" / "source" / "tables" / "data" / "assets" / ctx.GetFFType() /
                        std::format("{}{}.csv", (opt.noAssetDump ? "unloaded_" : ""), ctx.ffname)
                    };
                    std::filesystem::create_directories(outAssets.parent_path());
                    utils::OutFileCE osa{ outAssets };
                    osa << "type,name\n";
                    gcx.outAssetNames = &osa;

                    // ACTS_ASSET_MAP=<file>: each asset's type and its start / end in the decompressed stream
                    const char* assetMapPath{ std::getenv("ACTS_ASSET_MAP") };
                    std::ofstream assetMap{};
                    if (assetMapPath && *assetMapPath) {
                        assetMap.open(assetMapPath);
                        assetMap << "index,type,start,end\n";
                    }
                    // ACTS_JUMP=a:b:offset,...: assets a..b-1 not loaded, the stream at offset for asset b (research on
                    // streams the loaders can't walk whole); ACTS_STOP=i: stop after asset i
                    struct AssetJump {
                        size_t from;
                        size_t to;
                        size_t offset;
                    };
                    std::vector<AssetJump> jumps{};
                    if (const char* jumpEnv{ std::getenv("ACTS_JUMP") }) {
                        std::string list{ jumpEnv };
                        size_t at{};
                        while (at < list.size()) {
                            size_t end{ list.find(',', at) };
                            std::string item{ list.substr(at, end == std::string::npos ? std::string::npos : end - at) };
                            AssetJump j{};
                            if (std::sscanf(item.c_str(), "%zu:%zu:%zi", &j.from, &j.to, &j.offset) == 3) {
                                jumps.push_back(j);
                            }
                            at = end == std::string::npos ? list.size() : end + 1;
                        }
                    }
                    const char* stopEnv{ std::getenv("ACTS_STOP") };
                    size_t stopAt{ stopEnv ? (size_t)std::strtoull(stopEnv, nullptr, 0) : SIZE_MAX };
                    if (const char* streamLogPath{ std::getenv("ACTS_STREAM_LOG") }) {
                        gcx.streamLog.open(streamLogPath);
                    }

                    for (size_t i = 0; i < assetList.assetCount && i <= stopAt; i++) {
                        bool skipped{};
                        for (const AssetJump& j : jumps) {
                            if (i >= j.from && i < j.to) {
                                skipped = true;
                            } else if (i == j.to) {
                                gcx.reader->Goto(j.offset);
                            }
                        }
                        if (skipped) {
                            continue;
                        }
                        XAsset_0* ass{ &assetList.assets[i] };
                        errorCtx.lastAsset = ass;
                        errorCtx.assetIdx = i;
                        const char* assType{ XAssetNameFromId(ass->type) };
                        LOG_DEBUG(
                            "{}/{} Load asset {} (0x{:x}) {}",
                            i,
                            assetList.assetCount,
                            assType,
                            (int)ass->type,
                            ass->header
                        );
                        if (!opt.noAssetDump || opt.testDump || opt.dumpCompiledZone) {
#ifndef CI_BUILD
                            // if (i >= 110100) core::logs::setlevel(core::logs::LVL_TRACE_PATH);
#endif
                            size_t start{ gcx.reader->Loc() };
                            if (assetMap.is_open()) {
                                assetMap << i << "," << assType << "," << start << "," << std::flush;
                            }
                            gcx.Load_XAsset(false, ass);
                            if (assetMap.is_open()) {
                                assetMap << gcx.reader->Loc() << "\n";
                            }
                            continue;
                        }

                        osa << assType << ",0x" << ass->header << "\n";
                    }
                    errorCtx.lastAsset = nullptr;

                    LOG_OPT_INFO("Dumped assets into {}", outAssets.string());
                    if (!opt.noAssetDump) {
                        for (auto& [t, w] : GetWorkers()) {
                            w->PostXFileLoading(opt, ctx);
                        }
                    }

                    // xstrings
                    if (opt.dumpXStrings) {
                        std::filesystem::path outStrings{ gcx.opt->m_output / "bo4" / "source" / "tables" / "data" /
                                                          "xstrings" / ctx.GetFFType() /
                                                          std::format("{}.txt", ctx.ffname) };
                        std::filesystem::create_directories(outStrings.parent_path());
                        utils::OutFileCE os{ outStrings, true };

                        for (char* xstr : gcx.xstrings) {
                            os << acts::decryptutils::DecryptStringT8(xstr) << "\n";
                        }
                        LOG_OPT_INFO("Dump xstrings into {}", outStrings.string());
                    }
                }

                DB_PopStreamPos();
                LOG_OPT_INFO("Loaded {} asset(s)", gcx.loaded);
                LOG_OPT_INFO(
                    "Preload block: 0x{:x} for a preload of these assets, 0x{:x} in the header",
                    gcx.preloadPos,
                    blockSizes[XFILE_BLOCK_TEMP_PRELOAD]
                );

                if (gcx.compiledZone) {
                    std::filesystem::path outCZ{ gcx.opt->m_output / "bo4" / "compiled" /
                                                 std::format("{}.zonec", ctx.ffname) };
                    gcx.compiledZone->DumpDebug(outCZ);
                }
            }

            size_t GetNumXAssetPools() override { return games::bo4::pool::ASSET_TYPE_COUNT; };

            const char* GetXAssetPoolName(size_t pool) override {
                return games::bo4::pool::XAssetNameFromId((games::bo4::pool::XAssetType)pool);
            }
        };

        utils::ArrayAdder<BO4FFHandler, fastfile::FFHandler> arr{ fastfile::GetHandlers() };
    } // namespace
    std::unordered_map<games::bo4::pool::XAssetType, Worker*>& GetWorkers() {
        static std::unordered_map<games::bo4::pool::XAssetType, Worker*> map{};
        return map;
    }

    const char* GetScrString(ScrString_t id) {
        if ((int)id >= gcx.assetList.stringsCount) {
            LOG_WARNING("Can't get scr string: {} >= {}", id, gcx.assetList.stringsCount);
            return utils::va("<invalid:0x%x>", id);
        }
        char* c{ gcx.assetList.strings[id] };
        if (!c)
            return "";
        if (!IsValidHandle(c))
            return GetValidString(c);
        return acts::decryptutils::DecryptStringT8(c);
    }

    const char* GetEModeName(eModes id) {
        static const char* names[]{
            "ZOMBIES",
            "MULTIPLAYER",
            "CAMPAIGN",
            "WARZONE",
        };

        if (id >= ACTS_ARRAYSIZE(names) || id < 0) {
            return "unknown";
        }
        return names[id];
    }

    const char* XFileBlockName(XFileBlock id) {
        static const char* names[]{ "TEMP",      "TEMP_PRELOAD", "RUNTIME_VIRTUAL", "RUNTIME_PHYSICAL",
                                    "VIRTUAL",   "PHYSICAL",     "STREAMER",        "STREAMER_CPU",
                                    "MEMMAPPED", "COUNT",        "MEMMAPPED2" };

        if (id >= ACTS_ARRAYSIZE(names) || id < 0) {
            return "unknown";
        }
        return names[id];
    }

    XFileBlockMemLocation GetXFileBlockMemLocation(XFileBlock id) {
        static XFileBlockMemLocation data[9]{
            XFILE_BLOCK_LOC_VIRTUAL,  XFILE_BLOCK_LOC_VIRTUAL,    XFILE_BLOCK_LOC_VIRTUAL,
            XFILE_BLOCK_LOC_PHYSICAL, XFILE_BLOCK_LOC_VIRTUAL,    XFILE_BLOCK_LOC_PHYSICAL,
            XFILE_BLOCK_LOC_STREAM,   XFILE_BLOCK_LOC_STREAM_CPU, XFILE_BLOCK_LOC_STREAM_MMAP,
        };
        return data[id];
    }

    bool IsValidHandle(const void* handle) {
        uintptr_t h{ (uintptr_t)handle };

        if (h < 0x1000000)
            return false;
        if (h >> 60)
            return false;
        return true; // maybe ok
    }

    const char* GetValidString(const char* handle, const char* defaultVal) {
        if (IsValidHandle(handle))
            return handle;
        return defaultVal ? defaultVal : utils::va("<invalid:0x%llx>", (uintptr_t)handle);
    }
} // namespace fastfile::handlers::bo4

std::ostream& operator<<(std::ostream& os, const fastfile::handlers::bo4::ScrString_t& scr) { return os << scr.id; }