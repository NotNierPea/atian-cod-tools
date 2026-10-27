#pragma once
#include <tools/fastfile/linkers/linker_memory.hpp>

/*
 * Link cache: an asset linked before is replayed from what it gave the linker (its chunks, script strings, assets and
 * pointers) when the files it read are the same. The files are those the process opened while it was linked (file
 * opens and attribute queries hooked), checked by size and time, else by content.
 */
namespace fastfile::linker::cache {
    // a file an asset read (or looked for), its state then
    struct FileDep {
        std::string path;
        bool exists{};
        bool directory{};
        uint64_t size{};
        uint64_t mtime{};
        uint64_t hash{};
    };

    // the files opened while tracked, why the asset can't be cached if not empty
    struct TrackedFiles {
        std::vector<std::string> read{};
        // the files written in the output (their full path as opened)
        std::vector<std::string> written{};
        std::vector<std::string> missing{};
        std::vector<std::string> directories{};
        std::string taint{};
    };

    // track the files the process opens until EndTracking
    void BeginTracking();
    TrackedFiles EndTracking();
    // the file tracking hooks, false if they can't be set
    bool InitTracking();
    // the files this thread opens aren't tracked (the cache's own)
    void IgnoreThread();
    /*
     * The files under output are the link's: the ones written are kept (TrackedFiles::written), the others not
     * tracked. The ones under ignored aren't tracked (acts' own data, the build's).
     */
    void SetTrackingRoots(const std::filesystem::path& output, const std::vector<std::filesystem::path>& ignored);

    // the current state of the files, read once a run
    class FileStates {
        struct State {
            bool exists{};
            bool directory{};
            uint64_t size{};
            uint64_t mtime{};
            uint64_t hash{};
            bool hashed{};
        };
        std::unordered_map<std::string, State> states{};

        State& Get(const std::string& path, bool hash);

      public:
        // the state of a file now
        bool Snapshot(const std::string& path, FileDep& dep);
        // the file is as it was
        bool Matches(FileDep& dep);
    };

    class Writer {
      public:
        std::vector<byte> data{};

        template<typename T>
        void Write(const T& v) {
            static_assert(std::is_trivially_copyable_v<T>);
            utils::WriteValue(data, &v, sizeof(v));
        }
        void WriteBytes(const void* ptr, size_t len) {
            Write<uint64_t>(len);
            utils::WriteValue(data, ptr, len);
        }
        void WriteString(const std::string& str) { WriteBytes(str.data(), str.size()); }
    };

    class Reader {
        const byte* data;
        size_t size;
        size_t pos{};

      public:
        Reader(const byte* data, size_t size) : data(data), size(size) {}

        const byte* ReadRaw(size_t len) {
            if (len > size - pos) {
                throw std::runtime_error("link cache entry too short");
            }
            const byte* r{ data + pos };
            pos += len;
            return r;
        }
        template<typename T>
        T Read() {
            static_assert(std::is_trivially_copyable_v<T>);
            T v;
            std::memcpy(&v, ReadRaw(sizeof(T)), sizeof(T));
            return v;
        }
        std::string ReadString() {
            size_t len{ (size_t)Read<uint64_t>() };
            const byte* r{ ReadRaw(len) };
            return std::string{ (const char*)r, len };
        }
        std::vector<byte> ReadBytes() {
            size_t len{ (size_t)Read<uint64_t>() };
            const byte* r{ ReadRaw(len) };
            return std::vector<byte>{ r, r + len };
        }
        bool End() const { return pos == size; }
    };

    // what an asset linked, replayable
    struct LinkerRecord {
        struct Chunk {
            uint8_t type;
            uint8_t block;
            uint8_t hasData;
            uint64_t align;
            uint64_t size;
        };
        struct Ref {
            uint32_t chunk;
            uint32_t at;
            uint64_t offset;
        };
        std::vector<Chunk> chunks{};
        // the data of the chunks with data, in order
        std::vector<byte> data{};
        std::vector<Ref> refs{};
        std::vector<memory::LinkerRecorder::ScrStringUse> scrStrings{};
        std::vector<memory::LinkerRecorder::AssetUse> assets{};

        // from a recorder once the asset is linked, false (taint) if it can't be replayed
        bool Build(memory::LinkerRecorder& recorder, std::string& taint);
        void Write(Writer& w) const;
        void Read(Reader& r);
        // the script strings would get their indices again
        bool CanReplay(const memory::XBlockLinker& linker) const;
        void Replay(memory::XBlockLinker& linker) const;
    };

    // func(i) for i < count on the cores, the first exception thrown again
    void ParallelFor(size_t count, const std::function<void(size_t)>& func);

    std::string HexKey(uint64_t a, uint64_t b);
    uint64_t HashBytes(const void* data, size_t len, uint64_t seed = 0);
    // a file and its time, a key of the acts build
    std::string GetModuleIdentity();
    // the directory of the acts build
    std::filesystem::path GetModuleDirectory();
} // namespace fastfile::linker::cache
