#include <includes.hpp>
#include <tools/fastfile/linkers/linker_memory.hpp>

namespace fastfile::linker::memory {
    namespace {
        LinkerRecorder* activeRecorder{};
    }

    LinkerRecorder* GetActiveRecorder() { return activeRecorder; }
    void SetActiveRecorder(LinkerRecorder* recorder) { activeRecorder = recorder; }

    XBlockLinker::XBlockLinker(size_t numblocks, size_t virtualBlock)
        : numblocks(numblocks), virtualBlock(virtualBlock) {
        if (numblocks > LINKER_MAX_BLOCKS) {
            throw std::runtime_error(std::format("numblocks={} > LINKER_MAX_BLOCKS={}", numblocks, LINKER_MAX_BLOCKS));
        }
        SetBlockType(virtualBlock, BLOCKTYPE_VIRTUAL);
    }

    void XBlockLinker::SetMode(LinkerMode mode) {
        if (mode == linkerMode) {
            return; // ignore
        }
        if (blockStackIndex) {
            throw std::runtime_error(
                "XBlockLinker::SetMode: Can't set linker mode with non empty chunks, missing PopStream()?"
            );
        }
        if (activeRecorder) {
            activeRecorder->Taint("linker mode set");
        }
        linkerMode = mode;
    }

    void XBlockLinker::SetBlockType(size_t block, LinkerBlockType type) {
        AssertBlock(block);
        blocks[block].type = type;
    }

    LinkerDataChunk* XBlockLinker::AddChunk(LinkerDataChunkType type, size_t align, void* data, size_t size) {
        LinkerDataChunk* chunk{ CreateChunk(type, align, data, size) };
        LinkerBlock& block{ GetBlock() };
        if (!block.data[linkerMode]) {
            // first
            block.data[linkerMode] = chunk;
            block.last[linkerMode] = chunk;
        } else {
            block.last[linkerMode]->next = chunk;
            chunk->last = block.last[linkerMode];
            block.last[linkerMode] = chunk;
        }
        lastChunk = chunk;
        if (activeRecorder) {
            if (activeRecorder->linker != this) {
                activeRecorder->Taint("data linked in another fastfile");
            } else if (linkerMode != LM_DATA) {
                activeRecorder->Taint("header data linked");
            } else {
                activeRecorder->chunkIndex[chunk] = activeRecorder->chunks.size();
                activeRecorder->chunks.push_back(chunk);
                activeRecorder->chunkBlocks.push_back(blockStack[blockStackIndex]);
            }
        }
        return chunk;
    }
    LinkerDataChunk* XBlockLinker::CreateChunk(LinkerDataChunkType type, size_t align, void* data, size_t size) {
        LinkerDataChunk* chunk{ alloc.New<LinkerDataChunk>() };
        chunk->type = type;
        chunk->align = align;
        chunk->data = data;
        chunk->size = size;
        return chunk;
    }

    void XBlockLinker::PushStream(size_t block) {
        AssertBlock(block);
        if (blockStackIndex + 1 == ACTS_ARRAYSIZE(blockStack)) {
            throw std::runtime_error("XBlockLinker::PushStreamPos: Stack overflow");
        }
        blockStack[++blockStackIndex] = block;
        LOG_TRACE("XBlockLinker::PushStream({}) {}", blockStack[blockStackIndex], blockStackIndex);
        AddChunk(LinkerDataChunkType::CHUNKTYPE_PUSH, 0, nullptr, 0);
    }

    void XBlockLinker::PopStream() {
        if (blockStackIndex == 0) {
            throw std::runtime_error("XBlockLinker::PopStreamPos: Stack underflow");
        }
        LOG_TRACE("XBlockLinker::PopStream({}) {}", blockStack[blockStackIndex], blockStackIndex);
        AddChunk(LinkerDataChunkType::CHUNKTYPE_POP, 0, nullptr, 0);
        blockStackIndex--;
    }

    LinkerBlock& XBlockLinker::GetBlock() {
        if (blockStackIndex == 0) {
            throw std::runtime_error("XBlockLinker::GetBlock: No block set");
        }
        return blocks[blockStack[blockStackIndex]];
    }

    LinkerDataChunk* XBlockLinker::Align(size_t align) {
        LOG_TRACE("XBlockLinker::Align(0x{:x}) {}({})", align, blockStack[blockStackIndex], blockStackIndex);
        return AddChunk(LinkerDataChunkType::CHUNKTYPE_ALIGN, align, nullptr, 0);
    }

    LinkerDataChunk* XBlockLinker::WriteStream(const void* data, size_t size) {
        LinkerDataChunk* chunk{ AllocStream(size) };
        std::memcpy(chunk->data, data, size);
        return chunk;
    }

    LinkerDataChunk* XBlockLinker::AllocStream(size_t size) {
        if (GetBlock().type == BLOCKTYPE_RUNTIME) {
            throw std::runtime_error("XBlockLinker::AllocStream: Can't alloc data for a runtime xblock");
        }
        LinkerDataChunk* chunk{ AllocRuntime(size) };

        if (!zone[linkerMode]) {
            // first
            zone[linkerMode] = chunk;
            zoneLast[linkerMode] = chunk;
        } else {
            zoneLast[linkerMode]->nextZone = chunk;
            chunk->lastZone = zoneLast[linkerMode];
            zoneLast[linkerMode] = chunk;
        }

        chunk->data = alloc.Alloc(size);
        std::memset(chunk->data, 0, size);
        return chunk;
    }

    LinkerDataChunk* XBlockLinker::AllocRuntime(size_t size) {
        return AddChunk(LinkerDataChunkType::CHUNKTYPE_DATA, 0, nullptr, size);
    }

    uint32_t XBlockLinker::FindScrString(const char* value) const {
        auto it{ scrStringIndex.find(value) };
        return it == scrStringIndex.end() ? 0 : it->second;
    }

    uint32_t XBlockLinker::AddScrStringRaw(const char* value) {
        uint32_t index{ FindScrString(value) };
        if (!index) {
            const char* str{ alloc.CloneStr(value) };
            scrStrings.push_back(str);
            index = (uint32_t)scrStrings.size(); // +1 for the null str
            scrStringIndex[str] = index;
        }
        return index;
    }

    uint32_t XBlockLinker::AddScrString(const char* value) {
        if (!value || !*value) {
            return 0;
        }
        uint32_t index{ AddScrStringRaw(value) };
        if (activeRecorder && activeRecorder->linker == this) {
            activeRecorder->scrStrings.push_back({ value, index, false });
        }
        return index;
    }

    uint32_t XBlockLinker::AddScrStringKeepEmpty(const char* value) {
        if (!value) {
            return 0;
        }
        uint32_t index{ AddScrStringRaw(value) };
        if (activeRecorder && activeRecorder->linker == this) {
            activeRecorder->scrStrings.push_back({ value, index, true });
        }
        return index;
    }

    AssetData* XBlockLinker::AddAsset(size_t type, LinkerDataChunk* align) {
        LOG_DEBUG("XBlockLinker::AddAsset({})", type);
        AssetData* asset{ alloc.New<AssetData>() };
        asset->header = (void*)POINTER_NEXT;
        asset->type = type;
        asset->align = align;
        assets.emplace_back(asset);
        if (activeRecorder && activeRecorder->linker == this) {
            auto it{ activeRecorder->chunkIndex.find(align) };
            if (it == activeRecorder->chunkIndex.end()) {
                activeRecorder->Taint("asset added on data linked before");
            } else {
                activeRecorder->assets.push_back({ type, it->second });
            }
        }
        return asset;
    }

    void XBlockLinker::AddRef(LinkerDataChunk* chunk, void** ref) {
        chunk->refs.push_back(ref);
        if (activeRecorder && !activeRecorder->chunkIndex.contains(chunk)) {
            activeRecorder->Taint("pointer to data linked before");
        }
    }

    void XBlockLinker::AddXString(const char*& value, const char* val) {
        // todo: add relative string
        value = (const char*)POINTER_NEXT;
        this->WriteStream(val);
    }

    void XBlockLinker::AssertBlock(size_t block) {
        if (block >= numblocks) {
            throw std::runtime_error(
                std::format("XBlockLinker::AssertBlock: Invalid linker block {}: Too high", block)
            );
        }
    }

    static void ApplyRefs(LinkerDataChunk& chunk, size_t blockIndex, size_t size) {
        if (!chunk.refs.empty()) {
            // +1 because 0|0 != nullptr
            void* loc{ (void*)((size | ((uint64_t)blockIndex << 60)) + 1) };

            for (void** ref : chunk.refs) {
                *ref = loc;
            }
        }
    }

    static size_t LoadChunkSize(size_t blockIndex, LinkerDataChunk* chunk, size_t startSize) {
        size_t size{ startSize };
        for (; chunk; chunk = chunk->next) {
            ApplyRefs(*chunk, blockIndex, size);

            switch (chunk->type) {
            case CHUNKTYPE_DATA:
                size += chunk->size;
                break;
            case CHUNKTYPE_ALIGN:
                size = utils::Aligned<size_t>(size, chunk->align);
                break;
            case CHUNKTYPE_POP:
            case CHUNKTYPE_PUSH:
                break;
            default:
                throw std::runtime_error(std::format("Invalid chunk type: {}", (int)chunk->type));
            }
        }

        return size;
    }
    static size_t LoadChunkSizeTemp(size_t blockIndex, LinkerDataChunk* chunk, size_t maxSize) {
        size_t size{};

        std::stack<size_t> lens{};

        for (; chunk; chunk = chunk->next) {
            if (!chunk->refs.empty()) {
                LOG_WARNING("LoadChunkSizeTemp found a ref in TEMP block {}, it is probably an error.", blockIndex);
            }
            ApplyRefs(*chunk, blockIndex, size);

            switch (chunk->type) {
            case CHUNKTYPE_DATA:
                size += chunk->size;
                break;
            case CHUNKTYPE_ALIGN:
                size = utils::Aligned<size_t>(size, chunk->align);
                break;
            case CHUNKTYPE_POP:
                if (size > maxSize) {
                    maxSize = size;
                }
                size = lens.top();
                lens.pop();
                break;
            case CHUNKTYPE_PUSH:
                lens.push(size);
                break;
            default:
                throw std::runtime_error(std::format("Invalid chunk type: {}", (int)chunk->type));
            }
        }

        if (size > maxSize) {
            maxSize = size;
        }
        return maxSize;
    }

    static size_t LoadChunkSizeTotal(LinkerDataChunk* chunk) {
        size_t size{};
        for (; chunk; chunk = chunk->next) {
            switch (chunk->type) {
            case CHUNKTYPE_DATA:
                size += chunk->size;
                break;
            case CHUNKTYPE_ALIGN:
                size = utils::Aligned<size_t>(size, chunk->align);
                break;
            case CHUNKTYPE_POP:
                size = utils::Aligned<size_t>(size, 0x10);
                break;
            default:
                break;
            }
        }
        return size;
    }

    void XBlockLinker::Link(size_t* blocks) {
        if (blockStackIndex) {
            throw std::runtime_error(std::format("XBlockLinker::Link: block stack not empty {}", blockStackIndex));
        }

        for (size_t i = 0; i < numblocks; i++) {
            LinkerBlock& block{ this->blocks[i] };

            switch (block.type) {
            case BLOCKTYPE_TEMP:
                block.totalSize = LoadChunkSizeTotal(block.data[LM_DATA]);
                block.finalSize =
                    LoadChunkSizeTemp(i, block.data[LM_DATA], LoadChunkSizeTemp(i, block.data[LM_HEADER], 0));
                break;
            case BLOCKTYPE_RUNTIME:
            case BLOCKTYPE_VIRTUAL:
                block.finalSize = LoadChunkSize(i, block.data[LM_DATA], LoadChunkSize(i, block.data[LM_HEADER], 0));
                break;
            case BLOCKTYPE_UNKNOWN:
                if (block.data[LM_HEADER] || block.data[LM_DATA]) {
                    throw std::runtime_error(
                        std::format("XBlockLinker::Link: block 0x{:x} was used, but the type isn't known", i)
                    );
                }
                break;
            default:
                throw std::runtime_error(
                    std::format("XBlockLinker::Link: unimplemented block {} type {}", i, (int)block.type)
                );
                break;
            }
            LOG_TRACE("Block size {}: 0x{:x}", i, block.finalSize);
            blocks[i] = block.finalSize;
        }
    }

    void XBlockLinker::WriteLinkedData(std::vector<byte>& data) {
        for (size_t i = 0; i < LM_COUNT; i++) {
            for (LinkerDataChunk* chunk = zone[i]; chunk; chunk = chunk->nextZone) {
                if (chunk->data && chunk->size) {
                    utils::WriteValue(data, chunk->data, chunk->size);
                }
            }
        }
    }
} // namespace fastfile::linker::memory