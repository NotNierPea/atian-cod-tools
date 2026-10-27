#pragma once
#include <core/memory_allocator.hpp>

namespace fastfile::linker::memory {
    constexpr size_t POINTER_NEXT = static_cast<size_t>(-1ll);
    constexpr size_t POINTER_NEXT_POINTER = static_cast<size_t>(-2ll);

    constexpr size_t LINKER_MAX_BLOCKS = 20;
    constexpr size_t LINKER_STACK_SIZE = 64;

    enum LinkerBlockType {
        BLOCKTYPE_UNKNOWN = 0,
        BLOCKTYPE_TEMP = 1,
        BLOCKTYPE_RUNTIME = 2,
        BLOCKTYPE_VIRTUAL = 3,
    };

    enum LinkerDataChunkType {
        CHUNKTYPE_DATA = 1,
        CHUNKTYPE_ALIGN = 2,
        CHUNKTYPE_POP = 3,
        CHUNKTYPE_PUSH = 4,
    };

    enum LinkerMode { LM_HEADER = 0, LM_DATA = 1, LM_COUNT };

    struct LinkerDataChunk {
        LinkerDataChunk* next{};
        LinkerDataChunk* nextZone{};
        LinkerDataChunk* last{};
        LinkerDataChunk* lastZone{};
        LinkerDataChunkType type;
        size_t align{};
        void* data{};
        size_t size{};
        std::vector<void**> refs{};

        template<typename T>
        T* As() {
            return (T*)data;
        }
    };

    struct LinkerBlock {
        LinkerDataChunk* last[LM_COUNT]{};
        LinkerDataChunk* data[LM_COUNT]{};
        LinkerBlockType type{};
        size_t finalSize{};
        // a temp block's data (LM_DATA) as if it didn't rewind at a pop, aligned to 16 there
        size_t totalSize{};
    };

    struct AssetData {
        size_t type;
        void* header;
        LinkerDataChunk* align{};
        LinkerDataChunk* insertedZone{};
    };

    class XBlockLinker;

    /*
     * What a linker gets while an asset is linked, for the link cache (linker_cache.hpp): the chunks in their order,
     * the script strings asked and the assets added. Taint is why it can't be replayed (empty if it can).
     */
    struct LinkerRecorder {
        struct ScrStringUse {
            std::string str;
            uint32_t index;
            bool keepEmpty;
        };
        struct AssetUse {
            size_t type;
            size_t alignChunk;
        };

        XBlockLinker* linker{};
        std::vector<LinkerDataChunk*> chunks{};
        // the block each chunk was added to
        std::vector<size_t> chunkBlocks{};
        std::unordered_map<LinkerDataChunk*, size_t> chunkIndex{};
        std::vector<ScrStringUse> scrStrings{};
        std::vector<AssetUse> assets{};
        std::string taint{};

        void Taint(const std::string& why) {
            if (taint.empty()) {
                taint = why;
            }
        }
    };

    // the recorder of the asset being linked, null if none
    LinkerRecorder* GetActiveRecorder();
    void SetActiveRecorder(LinkerRecorder* recorder);

    class XBlockLinker {
        core::memory_allocator::MemoryAllocator alloc{};
        // the index of each script string, + 1
        std::unordered_map<std::string_view, uint32_t> scrStringIndex{};

      public:
        LinkerBlock blocks[LINKER_MAX_BLOCKS]{};
        LinkerDataChunk* zone[LM_COUNT]{};
        LinkerDataChunk* zoneLast[LM_COUNT]{};
        LinkerMode linkerMode{};
        size_t blockStack[LINKER_STACK_SIZE]{};
        size_t blockStackIndex{};
        std::vector<AssetData*> assets{};
        std::vector<const char*> scrStrings{};
        // the last chunk added
        LinkerDataChunk* lastChunk{};
        size_t numblocks;
        size_t virtualBlock;

        // a xblock linker
        XBlockLinker(size_t numblocks, size_t virtualBlock);
        XBlockLinker(const XBlockLinker& other) = delete;
        XBlockLinker(XBlockLinker&& other) = delete;

        // set linker mode
        void SetMode(LinkerMode mode);
        // configure a block type
        void SetBlockType(size_t block, LinkerBlockType type);
        // push a block
        void PushStream(size_t block);
        // pop the block
        void PopStream();
        // align the stream
        LinkerDataChunk* Align(size_t align);
        // create a chunk
        LinkerDataChunk* CreateChunk(LinkerDataChunkType type, size_t align, void* data, size_t size);
        // add chunk to current block
        LinkerDataChunk* AddChunk(LinkerDataChunkType type, size_t align, void* data, size_t size);
        // alloc and write a stream
        LinkerDataChunk* WriteStream(const void* data, size_t size);
        // alloc a stream
        LinkerDataChunk* AllocStream(size_t size);
        // alloc runtime data
        LinkerDataChunk* AllocRuntime(size_t size);
        // link the blocks
        void Link(size_t* blocks);
        // write the data
        void WriteLinkedData(std::vector<byte>& data);
        // add a scr string
        uint32_t AddScrString(const char* value);
        // add a scr string, "" as a string (not 0)
        uint32_t AddScrStringKeepEmpty(const char* value);
        // add an asset
        AssetData* AddAsset(size_t type, LinkerDataChunk* align);
        // add a string
        void AddXString(const char*& value, const char* val);
        // a pointer to a chunk's data, set at Link
        void AddRef(LinkerDataChunk* chunk, void** ref);
        // the index of a script string, 0 if it isn't in the fastfile
        uint32_t FindScrString(const char* value) const;
        // the current block
        size_t GetCurrentBlock() const { return blockStack[blockStackIndex]; }

        template<typename Type>
        LinkerDataChunk* WriteStream(const Type& data) {
            return WriteStream(&data, sizeof(data));
        }
        template<typename Type>
        inline LinkerDataChunk* WriteStream(const std::vector<Type>& data) {
            return WriteStream(data.data(), data.size() * sizeof(Type));
        }
        template<typename Type>
        inline LinkerDataChunk* AllocStream() {
            return AllocStream(sizeof(Type));
        }
        template<typename Type>
        inline Type* AllocStreamPtr(size_t count = 1, fastfile::linker::memory::LinkerDataChunk** out = nullptr) {
            fastfile::linker::memory::LinkerDataChunk* stream{ AllocStream(sizeof(Type) * count) };
            if (out)
                *out = stream;
            return stream->As<Type>();
        }
        template<typename Type>
        inline Type& AllocStreamRef(fastfile::linker::memory::LinkerDataChunk** out = nullptr) {
            return *AllocStreamPtr<Type>(1, out);
        }
        LinkerDataChunk* WriteStream(const char* str) { return WriteStream(str, std::strlen(str) + 1); }
        LinkerDataChunk* WriteStream(const std::string& str) { return WriteStream(str.data(), str.size() + 1); }

        template<typename Type = void>
        void Align() {
            Align(sizeof(Type));
        }

        inline uint32_t AddScrString(const std::string& value) { return AddScrString(value.data()); }

      private:
        uint32_t AddScrStringRaw(const char* value);
        // get the current last pushed block
        LinkerBlock& GetBlock();
        // assert that a block is valid
        void AssertBlock(size_t block);
    };

} // namespace fastfile::linker::memory