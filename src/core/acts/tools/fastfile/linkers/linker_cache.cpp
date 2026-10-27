#include <includes.hpp>
#include <tools/fastfile/linkers/linker_cache.hpp>
#include <xxhash.h>
#include <detours.h>
#include <thread>

namespace fastfile::linker::cache {
    namespace {
        decltype(&CreateFileW) createFileW{};
        decltype(&CreateFileA) createFileA{};
        decltype(&CreateFile2) createFile2{};
        decltype(&GetFileAttributesW) getFileAttributesW{};
        decltype(&GetFileAttributesA) getFileAttributesA{};
        decltype(&GetFileAttributesExW) getFileAttributesExW{};
        decltype(&GetFileAttributesExA) getFileAttributesExA{};
        decltype(&FindFirstFileExW) findFirstFileExW{};
        decltype(&FindFirstFileExA) findFirstFileExA{};
        decltype(&FindFirstFileW) findFirstFileW{};
        decltype(&FindFirstFileA) findFirstFileA{};

        std::mutex trackMutex{};
        bool tracking{};
        std::unordered_set<std::string> trackedRead{};
        std::unordered_set<std::string> trackedMissing{};
        std::unordered_set<std::string> trackedDirectories{};
        std::unordered_map<std::string, std::string> trackedWritten{};
        std::string outputRoot{};
        std::vector<std::string> ignoredRoots{};
        std::string trackedTaint{};
        thread_local bool inHook{};
        thread_local bool ignoredThread{};

        // the full lower case path, empty for a device, full: as given
        std::string NormalizePath(const wchar_t* name, std::string* fullPath = nullptr) {
            if (!name || !*name) {
                return {};
            }
            std::wstring_view nv{ name };
            if (nv.starts_with(L"\\\\?\\")) {
                nv.remove_prefix(4);
            } else if (nv.starts_with(L"\\\\.\\")) {
                return {};
            }
            std::wstring in{ nv };
            wchar_t buff[0x1000];
            DWORD len{ GetFullPathNameW(in.c_str(), (DWORD)ARRAYSIZE(buff), buff, nullptr) };
            if (!len || len >= ARRAYSIZE(buff)) {
                return {};
            }
            std::wstring full{ buff, len };
            if (full.size() < 3 || full[1] != L':') {
                return {}; // CONOUT$, pipes, network...
            }
            if (fullPath) {
                *fullPath = utils::WStrToStr(full);
            }
            for (wchar_t& c : full) {
                c = (wchar_t)towlower(c);
                if (c == L'/') {
                    c = L'\\';
                }
            }
            return utils::WStrToStr(full);
        }

        std::wstring Widen(const char* name) {
            if (!name) {
                return {};
            }
            int len{ MultiByteToWideChar(CP_ACP, 0, name, -1, nullptr, 0) };
            if (len <= 0) {
                return {};
            }
            std::wstring out((size_t)len, L'\0');
            MultiByteToWideChar(CP_ACP, 0, name, -1, out.data(), len);
            out.resize((size_t)len - 1);
            return out;
        }

        void TaintLocked(const std::string& why) {
            if (trackedTaint.empty()) {
                trackedTaint = why;
            }
        }

        // a file opened or looked for, found or not (err)
        void Track(const wchar_t* name, bool write, bool found, DWORD err, bool wildcard) {
            if (inHook || ignoredThread) {
                return;
            }
            inHook = true;
            utils::CloseEnd ce{ [] { inHook = false; } };
            std::lock_guard lg{ trackMutex };
            if (!tracking) {
                return;
            }
            std::string fullPath{};
            std::string path{ NormalizePath(name, &fullPath) };
            if (path.empty()) {
                return;
            }
            for (const std::string& root : ignoredRoots) {
                if (path.starts_with(root)) {
                    return;
                }
            }
            if (!outputRoot.empty() && path.starts_with(outputRoot)) {
                if (write && found) {
                    trackedWritten[path] = fullPath;
                }
                return;
            }
            if (wildcard) {
                TaintLocked(std::format("directory listed: {}", path));
                return;
            }
            if (write) {
                TaintLocked(std::format("file written: {}", path));
                return;
            }
            if (!found) {
                if (err == ERROR_FILE_NOT_FOUND || err == ERROR_PATH_NOT_FOUND || err == ERROR_INVALID_NAME) {
                    trackedMissing.insert(path);
                } else {
                    TaintLocked(std::format("can't open {}: error {}", path, err));
                }
                return;
            }
            std::wstring wpath{ utils::StrToWStr(path) };
            DWORD attr{ getFileAttributesW(wpath.c_str()) };
            if (attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY)) {
                trackedDirectories.insert(path);
            } else {
                trackedRead.insert(path);
            }
        }

        bool IsWrite(DWORD access, DWORD disposition) {
            return (access & (GENERIC_WRITE | GENERIC_ALL | FILE_WRITE_DATA | FILE_APPEND_DATA | DELETE)) ||
                   disposition == CREATE_NEW || disposition == CREATE_ALWAYS || disposition == OPEN_ALWAYS ||
                   disposition == TRUNCATE_EXISTING;
        }

        bool HasWildcard(const wchar_t* name) { return name && (std::wcschr(name, L'*') || std::wcschr(name, L'?')); }

        HANDLE WINAPI CreateFileWHook(
            LPCWSTR name, DWORD access, DWORD share, LPSECURITY_ATTRIBUTES sa, DWORD disposition, DWORD flags,
            HANDLE templateFile
        ) {
            HANDLE h{ createFileW(name, access, share, sa, disposition, flags, templateFile) };
            DWORD err{ GetLastError() };
            if (tracking) {
                Track(name, IsWrite(access, disposition), h != INVALID_HANDLE_VALUE, err, false);
                SetLastError(err);
            }
            return h;
        }

        HANDLE WINAPI CreateFileAHook(
            LPCSTR name, DWORD access, DWORD share, LPSECURITY_ATTRIBUTES sa, DWORD disposition, DWORD flags,
            HANDLE templateFile
        ) {
            HANDLE h{ createFileA(name, access, share, sa, disposition, flags, templateFile) };
            DWORD err{ GetLastError() };
            if (tracking) {
                Track(Widen(name).c_str(), IsWrite(access, disposition), h != INVALID_HANDLE_VALUE, err, false);
                SetLastError(err);
            }
            return h;
        }

        HANDLE WINAPI CreateFile2Hook(
            LPCWSTR name, DWORD access, DWORD share, DWORD disposition, LPCREATEFILE2_EXTENDED_PARAMETERS params
        ) {
            HANDLE h{ createFile2(name, access, share, disposition, params) };
            DWORD err{ GetLastError() };
            if (tracking) {
                Track(name, IsWrite(access, disposition), h != INVALID_HANDLE_VALUE, err, false);
                SetLastError(err);
            }
            return h;
        }

        DWORD WINAPI GetFileAttributesWHook(LPCWSTR name) {
            DWORD r{ getFileAttributesW(name) };
            DWORD err{ GetLastError() };
            if (tracking) {
                Track(name, false, r != INVALID_FILE_ATTRIBUTES, err, false);
                SetLastError(err);
            }
            return r;
        }

        DWORD WINAPI GetFileAttributesAHook(LPCSTR name) {
            DWORD r{ getFileAttributesA(name) };
            DWORD err{ GetLastError() };
            if (tracking) {
                Track(Widen(name).c_str(), false, r != INVALID_FILE_ATTRIBUTES, err, false);
                SetLastError(err);
            }
            return r;
        }

        BOOL WINAPI GetFileAttributesExWHook(LPCWSTR name, GET_FILEEX_INFO_LEVELS level, LPVOID info) {
            BOOL r{ getFileAttributesExW(name, level, info) };
            DWORD err{ GetLastError() };
            if (tracking) {
                Track(name, false, r, err, false);
                SetLastError(err);
            }
            return r;
        }

        BOOL WINAPI GetFileAttributesExAHook(LPCSTR name, GET_FILEEX_INFO_LEVELS level, LPVOID info) {
            BOOL r{ getFileAttributesExA(name, level, info) };
            DWORD err{ GetLastError() };
            if (tracking) {
                Track(Widen(name).c_str(), false, r, err, false);
                SetLastError(err);
            }
            return r;
        }

        HANDLE WINAPI FindFirstFileExWHook(
            LPCWSTR name, FINDEX_INFO_LEVELS level, LPVOID data, FINDEX_SEARCH_OPS op, LPVOID filter, DWORD flags
        ) {
            HANDLE h{ findFirstFileExW(name, level, data, op, filter, flags) };
            DWORD err{ GetLastError() };
            if (tracking) {
                Track(name, false, h != INVALID_HANDLE_VALUE, err, HasWildcard(name));
                SetLastError(err);
            }
            return h;
        }

        HANDLE WINAPI FindFirstFileExAHook(
            LPCSTR name, FINDEX_INFO_LEVELS level, LPVOID data, FINDEX_SEARCH_OPS op, LPVOID filter, DWORD flags
        ) {
            HANDLE h{ findFirstFileExA(name, level, data, op, filter, flags) };
            DWORD err{ GetLastError() };
            if (tracking) {
                std::wstring wname{ Widen(name) };
                Track(wname.c_str(), false, h != INVALID_HANDLE_VALUE, err, HasWildcard(wname.c_str()));
                SetLastError(err);
            }
            return h;
        }

        HANDLE WINAPI FindFirstFileWHook(LPCWSTR name, LPWIN32_FIND_DATAW data) {
            HANDLE h{ findFirstFileW(name, data) };
            DWORD err{ GetLastError() };
            if (tracking) {
                Track(name, false, h != INVALID_HANDLE_VALUE, err, HasWildcard(name));
                SetLastError(err);
            }
            return h;
        }

        HANDLE WINAPI FindFirstFileAHook(LPCSTR name, LPWIN32_FIND_DATAA data) {
            HANDLE h{ findFirstFileA(name, data) };
            DWORD err{ GetLastError() };
            if (tracking) {
                std::wstring wname{ Widen(name) };
                Track(wname.c_str(), false, h != INVALID_HANDLE_VALUE, err, HasWildcard(wname.c_str()));
                SetLastError(err);
            }
            return h;
        }
    } // namespace

    bool InitTracking() {
        static int init{};
        if (init) {
            return init > 0;
        }
        init = -1;
        HMODULE kb{ GetModuleHandleW(L"KernelBase.dll") };
        if (!kb) {
            LOG_WARNING("Link cache: no KernelBase.dll");
            return false;
        }
        struct Hook {
            void** orig;
            void* to;
            const char* name;
        };
        Hook hooks[]{
            { (void**)&createFileW, (void*)&CreateFileWHook, "CreateFileW" },
            { (void**)&createFileA, (void*)&CreateFileAHook, "CreateFileA" },
            { (void**)&createFile2, (void*)&CreateFile2Hook, "CreateFile2" },
            { (void**)&getFileAttributesW, (void*)&GetFileAttributesWHook, "GetFileAttributesW" },
            { (void**)&getFileAttributesA, (void*)&GetFileAttributesAHook, "GetFileAttributesA" },
            { (void**)&getFileAttributesExW, (void*)&GetFileAttributesExWHook, "GetFileAttributesExW" },
            { (void**)&getFileAttributesExA, (void*)&GetFileAttributesExAHook, "GetFileAttributesExA" },
            { (void**)&findFirstFileExW, (void*)&FindFirstFileExWHook, "FindFirstFileExW" },
            { (void**)&findFirstFileExA, (void*)&FindFirstFileExAHook, "FindFirstFileExA" },
            { (void**)&findFirstFileW, (void*)&FindFirstFileWHook, "FindFirstFileW" },
            { (void**)&findFirstFileA, (void*)&FindFirstFileAHook, "FindFirstFileA" },
        };
        for (Hook& h : hooks) {
            *h.orig = (void*)GetProcAddress(kb, h.name);
            if (!*h.orig) {
                LOG_WARNING("Link cache: no KernelBase!{}", h.name);
                return false;
            }
        }
        DetourTransactionBegin();
        DetourUpdateThread(GetCurrentThread());
        for (Hook& h : hooks) {
            DetourAttach(h.orig, h.to);
        }
        LONG err{ DetourTransactionCommit() };
        if (err != NO_ERROR) {
            LOG_WARNING("Link cache: can't hook the file functions: {}", err);
            return false;
        }
        init = 1;
        return true;
    }

    void IgnoreThread() { ignoredThread = true; }

    void SetTrackingRoots(const std::filesystem::path& output, const std::vector<std::filesystem::path>& ignored) {
        std::lock_guard lg{ trackMutex };
        auto Root = [](const std::filesystem::path& p) {
            std::string r{ NormalizePath(std::filesystem::absolute(p).wstring().c_str()) };
            if (!r.empty() && r.back() != '\\') {
                r += '\\';
            }
            return r;
        };
        outputRoot = Root(output);
        ignoredRoots.clear();
        for (const std::filesystem::path& p : ignored) {
            ignoredRoots.push_back(Root(p));
        }
    }

    void BeginTracking() {
        std::lock_guard lg{ trackMutex };
        trackedRead.clear();
        trackedMissing.clear();
        trackedDirectories.clear();
        trackedWritten.clear();
        trackedTaint.clear();
        tracking = true;
    }

    TrackedFiles EndTracking() {
        std::lock_guard lg{ trackMutex };
        tracking = false;
        TrackedFiles files{};
        files.read.assign(trackedRead.begin(), trackedRead.end());
        files.missing.assign(trackedMissing.begin(), trackedMissing.end());
        files.directories.assign(trackedDirectories.begin(), trackedDirectories.end());
        for (const auto& [lower, full] : trackedWritten) {
            files.written.push_back(full);
        }
        std::sort(files.written.begin(), files.written.end());
        files.taint = trackedTaint;
        std::sort(files.read.begin(), files.read.end());
        std::sort(files.missing.begin(), files.missing.end());
        std::sort(files.directories.begin(), files.directories.end());
        return files;
    }

    FileStates::State& FileStates::Get(const std::string& path, bool hash) {
        auto [it, added]{ states.try_emplace(path) };
        State& s{ it->second };
        std::wstring wpath{ utils::StrToWStr(path) };
        if (added) {
            WIN32_FILE_ATTRIBUTE_DATA data{};
            BOOL ok{
                (getFileAttributesExW ? getFileAttributesExW
                                      : &GetFileAttributesExW)(wpath.c_str(), GetFileExInfoStandard, &data)
            };
            if (ok) {
                s.exists = true;
                s.directory = (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
                s.size = ((uint64_t)data.nFileSizeHigh << 32) | data.nFileSizeLow;
                s.mtime = ((uint64_t)data.ftLastWriteTime.dwHighDateTime << 32) | data.ftLastWriteTime.dwLowDateTime;
            }
        }
        if (hash && !s.hashed && s.exists && !s.directory) {
            s.hashed = true;
            HANDLE h{ (createFileW ? createFileW : &CreateFileW)(
                wpath.c_str(),
                GENERIC_READ,
                FILE_SHARE_READ | FILE_SHARE_WRITE,
                nullptr,
                OPEN_EXISTING,
                FILE_FLAG_SEQUENTIAL_SCAN,
                nullptr
            ) };
            if (h == INVALID_HANDLE_VALUE) {
                // unreadable: never the same
                s.hash = ~s.size;
                s.mtime = ~0ull;
                return s;
            }
            utils::CloseEnd ce{ [h] { CloseHandle(h); } };
            XXH64_state_t* state{ XXH64_createState() };
            utils::CloseEnd ces{ [state] { XXH64_freeState(state); } };
            XXH64_reset(state, 0);
            std::vector<byte> buff(0x400000);
            DWORD read{};
            while (ReadFile(h, buff.data(), (DWORD)buff.size(), &read, nullptr) && read) {
                XXH64_update(state, buff.data(), read);
            }
            s.hash = XXH64_digest(state);
        }
        return s;
    }

    bool FileStates::Snapshot(const std::string& path, FileDep& dep) {
        State& s{ Get(path, true) };
        dep.path = path;
        dep.exists = s.exists;
        dep.directory = s.directory;
        dep.size = s.size;
        dep.mtime = s.mtime;
        dep.hash = s.hash;
        return true;
    }

    bool FileStates::Matches(FileDep& dep) {
        State& s{ Get(dep.path, false) };
        if (!dep.exists || !s.exists) {
            return dep.exists == s.exists;
        }
        if (dep.directory || s.directory) {
            return dep.directory == s.directory;
        }
        if (dep.size != s.size) {
            return false;
        }
        if (dep.mtime == s.mtime) {
            return true;
        }
        // rewritten, the same if its content is
        State& hs{ Get(dep.path, true) };
        if (hs.hash != dep.hash) {
            return false;
        }
        dep.mtime = hs.mtime;
        return true;
    }

    bool LinkerRecord::Build(memory::LinkerRecorder& recorder, std::string& taint) {
        if (!recorder.taint.empty()) {
            taint = recorder.taint;
            return false;
        }
        struct Range {
            uintptr_t start;
            size_t size;
            uint32_t chunk;
        };
        std::vector<Range> ranges{};
        chunks.clear();
        data.clear();
        refs.clear();
        for (size_t i = 0; i < recorder.chunks.size(); i++) {
            memory::LinkerDataChunk* c{ recorder.chunks[i] };
            Chunk& rc{ chunks.emplace_back() };
            rc.type = (uint8_t)c->type;
            rc.block = (uint8_t)recorder.chunkBlocks[i];
            rc.hasData = c->data != nullptr;
            rc.align = c->align;
            rc.size = c->size;
            if (c->data) {
                utils::WriteValue(data, c->data, c->size);
                ranges.push_back({ (uintptr_t)c->data, c->size, (uint32_t)i });
            }
        }
        std::sort(ranges.begin(), ranges.end(), [](const Range& a, const Range& b) { return a.start < b.start; });
        for (size_t i = 0; i < recorder.chunks.size(); i++) {
            for (void** ref : recorder.chunks[i]->refs) {
                uintptr_t at{ (uintptr_t)ref };
                auto it{ std::upper_bound(ranges.begin(), ranges.end(), at, [](uintptr_t v, const Range& r) {
                    return v < r.start;
                }) };
                if (it == ranges.begin() || at + sizeof(void*) > std::prev(it)->start + std::prev(it)->size) {
                    taint = "pointer outside of the asset's data";
                    return false;
                }
                const Range& r{ *std::prev(it) };
                refs.push_back({ (uint32_t)i, r.chunk, at - r.start });
            }
        }
        scrStrings = recorder.scrStrings;
        assets = recorder.assets;
        return true;
    }

    void LinkerRecord::Write(Writer& w) const {
        w.Write<uint64_t>(chunks.size());
        for (const Chunk& c : chunks) {
            w.Write(c);
        }
        w.WriteBytes(data.data(), data.size());
        w.Write<uint64_t>(refs.size());
        for (const Ref& r : refs) {
            w.Write(r);
        }
        w.Write<uint64_t>(scrStrings.size());
        for (const memory::LinkerRecorder::ScrStringUse& s : scrStrings) {
            w.WriteString(s.str);
            w.Write(s.index);
            w.Write(s.keepEmpty);
        }
        w.Write<uint64_t>(assets.size());
        for (const memory::LinkerRecorder::AssetUse& a : assets) {
            w.Write<uint64_t>(a.type);
            w.Write<uint64_t>(a.alignChunk);
        }
    }

    void LinkerRecord::Read(Reader& r) {
        chunks.resize((size_t)r.Read<uint64_t>());
        for (Chunk& c : chunks) {
            c = r.Read<Chunk>();
        }
        data = r.ReadBytes();
        refs.resize((size_t)r.Read<uint64_t>());
        for (Ref& ref : refs) {
            ref = r.Read<Ref>();
            if (ref.chunk >= chunks.size() || ref.at >= chunks.size() || !chunks[ref.at].hasData ||
                ref.offset + sizeof(void*) > chunks[ref.at].size) {
                throw std::runtime_error("link cache entry: bad ref");
            }
        }
        scrStrings.resize((size_t)r.Read<uint64_t>());
        for (memory::LinkerRecorder::ScrStringUse& s : scrStrings) {
            s.str = r.ReadString();
            s.index = r.Read<uint32_t>();
            s.keepEmpty = r.Read<bool>();
        }
        assets.resize((size_t)r.Read<uint64_t>());
        for (memory::LinkerRecorder::AssetUse& a : assets) {
            a.type = (size_t)r.Read<uint64_t>();
            a.alignChunk = (size_t)r.Read<uint64_t>();
            if (a.alignChunk >= chunks.size()) {
                throw std::runtime_error("link cache entry: bad asset");
            }
        }
        size_t dataSize{};
        for (const Chunk& c : chunks) {
            if (c.hasData) {
                dataSize += c.size;
            }
        }
        if (dataSize != data.size()) {
            throw std::runtime_error("link cache entry: bad data size");
        }
    }

    bool LinkerRecord::CanReplay(const memory::XBlockLinker& linker) const {
        // the indices it got: the strings already there at theirs, the new ones next
        uint32_t next{ (uint32_t)linker.scrStrings.size() };
        std::unordered_map<std::string_view, uint32_t> added{};
        for (const memory::LinkerRecorder::ScrStringUse& s : scrStrings) {
            if (!s.keepEmpty && s.str.empty()) {
                if (s.index) {
                    return false;
                }
                continue;
            }
            uint32_t index{ linker.FindScrString(s.str.data()) };
            if (!index) {
                auto it{ added.find(s.str) };
                index = it == added.end() ? 0 : it->second;
            }
            if (!index) {
                index = ++next;
                added[s.str] = index;
            }
            if (index != s.index) {
                return false;
            }
        }
        return true;
    }

    void LinkerRecord::Replay(memory::XBlockLinker& linker) const {
        std::vector<memory::LinkerDataChunk*> made(chunks.size());
        size_t dataPos{};
        for (size_t i = 0; i < chunks.size(); i++) {
            const Chunk& c{ chunks[i] };
            if (c.type != memory::CHUNKTYPE_PUSH && linker.GetCurrentBlock() != c.block) {
                throw std::runtime_error(
                    std::format("link cache replay: block {} != {}", linker.GetCurrentBlock(), c.block)
                );
            }
            switch (c.type) {
            case memory::CHUNKTYPE_PUSH:
                linker.PushStream(c.block);
                break;
            case memory::CHUNKTYPE_POP:
                linker.PopStream();
                break;
            case memory::CHUNKTYPE_ALIGN:
                linker.Align(c.align);
                break;
            case memory::CHUNKTYPE_DATA:
                if (c.hasData) {
                    memory::LinkerDataChunk* d{ linker.AllocStream(c.size) };
                    std::memcpy(d->data, data.data() + dataPos, c.size);
                    dataPos += c.size;
                } else {
                    linker.AllocRuntime(c.size);
                }
                break;
            default:
                throw std::runtime_error(std::format("link cache replay: bad chunk type {}", c.type));
            }
            made[i] = linker.lastChunk;
        }
        for (const Ref& r : refs) {
            made[r.chunk]->refs.push_back((void**)((byte*)made[r.at]->data + r.offset));
        }
        for (const memory::LinkerRecorder::AssetUse& a : assets) {
            linker.AddAsset(a.type, made[a.alignChunk]);
        }
        for (const memory::LinkerRecorder::ScrStringUse& s : scrStrings) {
            uint32_t index{ s.keepEmpty ? linker.AddScrStringKeepEmpty(s.str.data())
                                        : linker.AddScrString(s.str.data()) };
            if (index != s.index) {
                throw std::runtime_error(
                    std::format("link cache replay: script string {} at {} != {}", s.str, index, s.index)
                );
            }
        }
    }

    void ParallelFor(size_t count, const std::function<void(size_t)>& func) {
        size_t threads{ std::min<size_t>(count, std::max(1u, std::thread::hardware_concurrency())) };
        std::atomic<size_t> next{};
        std::mutex errMutex{};
        std::exception_ptr err{};
        auto Work = [&] {
            for (size_t i = next++; i < count; i = next++) {
                try {
                    func(i);
                } catch (...) {
                    std::lock_guard lg{ errMutex };
                    if (!err) {
                        err = std::current_exception();
                    }
                }
            }
        };
        std::vector<std::thread> pool{};
        for (size_t i = 1; i < threads; i++) {
            pool.emplace_back(Work);
        }
        Work();
        for (std::thread& t : pool) {
            t.join();
        }
        if (err) {
            std::rethrow_exception(err);
        }
    }

    std::string HexKey(uint64_t a, uint64_t b) { return std::format("{:016x}{:016x}", a, b); }

    uint64_t HashBytes(const void* data, size_t len, uint64_t seed) { return XXH64(data, len, seed); }

    std::filesystem::path GetModuleDirectory() {
        HMODULE mod{};
        wchar_t path[MAX_PATH]{};
        if (!GetModuleHandleExW(
                GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                (LPCWSTR)&GetModuleDirectory,
                &mod
            ) ||
            !GetModuleFileNameW(mod, path, MAX_PATH)) {
            return {};
        }
        return std::filesystem::path{ path }.parent_path();
    }

    std::string GetModuleIdentity() {
        HMODULE mod{};
        if (!GetModuleHandleExW(
                GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                (LPCWSTR)&GetModuleIdentity,
                &mod
            )) {
            return {};
        }
        wchar_t path[MAX_PATH]{};
        DWORD len{ GetModuleFileNameW(mod, path, MAX_PATH) };
        if (!len || len >= MAX_PATH) {
            return {};
        }
        WIN32_FILE_ATTRIBUTE_DATA data{};
        if (!GetFileAttributesExW(path, GetFileExInfoStandard, &data)) {
            return {};
        }
        return std::format(
            "{}:{:x}:{:x}:{:x}",
            utils::WStrToStr(path),
            data.nFileSizeLow,
            data.ftLastWriteTime.dwHighDateTime,
            data.ftLastWriteTime.dwLowDateTime
        );
    }
} // namespace fastfile::linker::cache
