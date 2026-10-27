#include <includes.hpp>
#include <tools/fastfile/linkers/linker_bo4.hpp>
#include <tools/fastfile/linkers/linker_cache.hpp>
#include <utils/compress_utils.hpp>
#include <condition_variable>
#include <deque>
#include <thread>

/*
 * The link cache of the bo4 linker, a directory a zone and output (--cache-dir, by default
 * %LOCALAPPDATA%/acts/link_cache/<ff>_<hash>):
 *
 * <key>.lce    a top asset of the zone: its key (acts' build, the zone's options, type, id), the files it read (or
 *              looked for) with their size, time and hash, then (lz4) what it linked: the chunks of the fastfile data
 *              with their pointers, the script strings with their indices, the assets added, the hashes registered,
 *              what it found and added in the fastfile's sets (assets linked once, by name after), its xpak entries
 *              and the files it wrote in the output (a sound bank's sabs), written again when replayed.
 * strings.bin  the script strings of the last link, given again first: an asset's indices stay the same when one
 *              before it changes (the strings no asset asks anymore stay, unused).
 *
 * An asset is replayed if its files are the same (size and time, else content) and it would find the same in the
 * fastfile (sets, the indices of its strings), else linked and recorded again. Only the types whose linkers keep
 * nothing between the assets but the sets are cached, an asset linking another kind is linked each time. An asset that
 * writes a file out of the output, lists a directory or links in another fastfile isn't cached either. The files in
 * acts' directory are acts' (its data), not tracked. --no-cache links without it,
 * --rebuild-cache links all and stores them again.
 */
namespace fastfile::linker::bo4 {
    namespace {
        constexpr uint64_t ENTRY_MAGIC = 0x3145484341434C41;    // ALCACHE1
        constexpr uint64_t STRINGS_MAGIC = 0x3152545343434C41;  // ALCCSTR1
        constexpr uint64_t COMPUTED_MAGIC = 0x3150434143434C41; // ALCCACP1
        constexpr const char* CACHE_VERSION = "1";
        // the entries waiting to be written, past it the linking waits
        constexpr size_t MAX_PENDING_WRITES = 0x40000000;

        bool IsCacheable(XAssetType type) {
            switch (type) {
            case XAssetType::ASSET_TYPE_IMAGE:
            case XAssetType::ASSET_TYPE_XMODEL:
            case XAssetType::ASSET_TYPE_XMODELMESH:
            case XAssetType::ASSET_TYPE_MATERIAL:
            case XAssetType::ASSET_TYPE_PHYSPRESET:
            case XAssetType::ASSET_TYPE_XANIM:
            case XAssetType::ASSET_TYPE_FX:
            case XAssetType::ASSET_TYPE_GFXWORLD:
            case XAssetType::ASSET_TYPE_CLIPMAP:
            case XAssetType::ASSET_TYPE_COMWORLD:
            case XAssetType::ASSET_TYPE_GAMEWORLD:
            case XAssetType::ASSET_TYPE_LIGHTING:
            case XAssetType::ASSET_TYPE_SOUND:
                return true;
            default:
                return false;
            }
        }

        struct SetOp {
            std::string set;
            uint64_t key;
            bool insert;
            bool result;
        };

        struct XPakRecord {
            uint64_t key;
            std::vector<byte> data;
        };

        struct Entry {
            std::string key{};
            std::vector<cache::FileDep> deps{};
            cache::LinkerRecord record{};
            std::vector<std::pair<uint64_t, std::string>> hashes{};
            std::vector<SetOp> sets{};
            std::vector<XPakRecord> xpak{};
            // the files written, their path in the output
            std::vector<std::pair<std::string, std::vector<byte>>> outputs{};

            size_t Size() const {
                size_t s{ record.data.size() };
                for (const XPakRecord& x : xpak) {
                    s += x.data.size();
                }
                for (const auto& [p, d] : outputs) {
                    s += d.size();
                }
                return s;
            }

            std::vector<byte> Serialize() const {
                cache::Writer payload{};
                record.Write(payload);
                payload.Write<uint64_t>(hashes.size());
                for (const auto& [v, s] : hashes) {
                    payload.Write(v);
                    payload.WriteString(s);
                }
                payload.Write<uint64_t>(sets.size());
                for (const SetOp& op : sets) {
                    payload.WriteString(op.set);
                    payload.Write(op.key);
                    payload.Write(op.insert);
                    payload.Write(op.result);
                }
                payload.Write<uint64_t>(xpak.size());
                for (const XPakRecord& x : xpak) {
                    payload.Write(x.key);
                    payload.WriteBytes(x.data.data(), x.data.size());
                }
                payload.Write<uint64_t>(outputs.size());
                for (const auto& [p, d] : outputs) {
                    payload.WriteString(p);
                    payload.WriteBytes(d.data(), d.size());
                }

                cache::Writer w{};
                w.Write(ENTRY_MAGIC);
                w.WriteString(key);
                w.Write<uint64_t>(deps.size());
                for (const cache::FileDep& d : deps) {
                    w.WriteString(d.path);
                    w.Write(d.exists);
                    w.Write(d.directory);
                    w.Write(d.size);
                    w.Write(d.mtime);
                    w.Write(d.hash);
                }
                std::vector<byte> comp{};
                if (!utils::compress::CompressBuffer(utils::compress::COMP_LZ4, payload.data, comp)) {
                    throw std::runtime_error("can't compress a link cache entry");
                }
                w.Write<uint64_t>(payload.data.size());
                w.WriteBytes(comp.data(), comp.size());
                return std::move(w.data);
            }

            // the header (key and files), the rest in payload
            void ReadHeader(cache::Reader& r, std::vector<byte>& compressed, size_t& rawSize) {
                if (r.Read<uint64_t>() != ENTRY_MAGIC) {
                    throw std::runtime_error("bad magic");
                }
                key = r.ReadString();
                deps.resize((size_t)r.Read<uint64_t>());
                for (cache::FileDep& d : deps) {
                    d.path = r.ReadString();
                    d.exists = r.Read<bool>();
                    d.directory = r.Read<bool>();
                    d.size = r.Read<uint64_t>();
                    d.mtime = r.Read<uint64_t>();
                    d.hash = r.Read<uint64_t>();
                }
                rawSize = (size_t)r.Read<uint64_t>();
                compressed = r.ReadBytes();
            }

            void ReadPayload(const std::vector<byte>& compressed, size_t rawSize) {
                std::vector<byte> raw(rawSize);
                int r{ utils::compress::Decompress2(
                    utils::compress::COMP_LZ4,
                    raw.data(),
                    raw.size(),
                    compressed.data(),
                    compressed.size()
                ) };
                if (r < 0 || (size_t)r != rawSize) {
                    throw std::runtime_error("can't decompress");
                }
                cache::Reader p{ raw.data(), raw.size() };
                record.Read(p);
                hashes.resize((size_t)p.Read<uint64_t>());
                for (auto& [v, s] : hashes) {
                    v = p.Read<uint64_t>();
                    s = p.ReadString();
                }
                sets.resize((size_t)p.Read<uint64_t>());
                for (SetOp& op : sets) {
                    op.set = p.ReadString();
                    op.key = p.Read<uint64_t>();
                    op.insert = p.Read<bool>();
                    op.result = p.Read<bool>();
                }
                xpak.resize((size_t)p.Read<uint64_t>());
                for (XPakRecord& x : xpak) {
                    x.key = p.Read<uint64_t>();
                    x.data = p.ReadBytes();
                }
                outputs.resize((size_t)p.Read<uint64_t>());
                for (auto& [path, d] : outputs) {
                    path = p.ReadString();
                    d = p.ReadBytes();
                }
                if (!p.End()) {
                    throw std::runtime_error("trailing data");
                }
            }
        };

        // the entries written by a thread while linking
        class EntryWriter {
            std::mutex mutex{};
            std::condition_variable cv{};
            std::deque<std::pair<std::filesystem::path, std::unique_ptr<Entry>>> jobs{};
            size_t pending{};
            bool stop{};
            size_t failed{};
            std::thread thread;

            void Run() {
                cache::IgnoreThread();
                std::unique_lock lock{ mutex };
                while (true) {
                    cv.wait(lock, [this] { return stop || !jobs.empty(); });
                    if (jobs.empty()) {
                        return;
                    }
                    auto [path, entry]{ std::move(jobs.front()) };
                    jobs.pop_front();
                    lock.unlock();
                    size_t size{ entry->Size() };
                    try {
                        std::vector<byte> data{ entry->Serialize() };
                        entry.reset();
                        std::filesystem::path tmp{ path };
                        tmp += ".tmp";
                        if (!utils::WriteFile(tmp, data) ||
                            !MoveFileExW(tmp.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING)) {
                            throw std::runtime_error(std::format("can't write {}", path.string()));
                        }
                    } catch (std::exception& e) {
                        LOG_WARNING("Link cache: {}", e.what());
                        lock.lock();
                        failed++;
                        lock.unlock();
                    }
                    lock.lock();
                    pending -= size;
                    cv.notify_all();
                }
            }

          public:
            EntryWriter() : thread([this] { Run(); }) {}
            ~EntryWriter() { Wait(); }

            void Add(std::filesystem::path path, std::unique_ptr<Entry> entry) {
                std::unique_lock lock{ mutex };
                cv.wait(lock, [this] { return pending < MAX_PENDING_WRITES; });
                pending += entry->Size();
                jobs.emplace_back(std::move(path), std::move(entry));
                cv.notify_all();
            }

            // the entries written, the count of failed ones
            size_t Wait() {
                {
                    std::lock_guard lg{ mutex };
                    stop = true;
                    cv.notify_all();
                }
                if (thread.joinable()) {
                    thread.join();
                }
                return failed;
            }
        };

        struct TypeStats {
            size_t replayed{};
            size_t linked{};
            size_t uncached{};
        };

        class BO4LinkCacheImpl : public BO4LinkCache {
            BO4LinkContext& ctx;
            std::filesystem::path dir;
            std::string globalKey{};
            bool rebuild;
            cache::FileStates files{};
            EntryWriter writer{};
            std::unordered_set<std::string> used{};
            std::map<std::string, TypeStats> stats{};

            struct Recording {
                BO4FFContext* ff{};
                std::string type{};
                std::string id{};
                std::string key{};
                std::string file{};
                memory::LinkerRecorder recorder{};
                std::vector<std::pair<uint64_t, std::string>> hashes{};
                std::vector<SetOp> sets{};
                std::vector<XPakRecord> xpak{};
                std::string taint{};
            };
            std::unique_ptr<Recording> rec{};
            // why the last asset tried wasn't replayed
            std::string missReason{};

            std::string KeyOf(XAssetType type, const char* id, BO4FFContext& ff, std::string& file) {
                std::string key{
                    std::format("{}\n{}\n{}\n{}", globalKey, games::bo4::pool::XAssetNameFromId(type), id, ff.ffname)
                };
                file = cache::HexKey(
                           cache::HashBytes(key.data(), key.size(), 0),
                           cache::HashBytes(key.data(), key.size(), 1)
                       ) +
                       ".lce";
                return key;
            }

            std::filesystem::path StringsPath() const { return dir / "strings.bin"; }

            bool Load(const std::string& key, const std::string& file, BO4FFContext& ff, Entry& entry) {
                std::filesystem::path path{ dir / file };
                std::vector<byte> data{};
                if (!utils::ReadFile(path, data)) {
                    missReason = "new";
                    return false;
                }
                std::vector<byte> compressed{};
                size_t rawSize{};
                try {
                    cache::Reader r{ data.data(), data.size() };
                    entry.ReadHeader(r, compressed, rawSize);
                } catch (std::exception& e) {
                    missReason = std::format("bad entry ({})", e.what());
                    return false;
                }
                if (entry.key != key) {
                    missReason = "key collision";
                    return false;
                }
                for (cache::FileDep& d : entry.deps) {
                    if (!files.Matches(d)) {
                        missReason = std::format("{} {}", d.exists ? "changed" : "added", d.path);
                        return false;
                    }
                }
                try {
                    entry.ReadPayload(compressed, rawSize);
                } catch (std::exception& e) {
                    missReason = std::format("bad entry ({})", e.what());
                    return false;
                }
                // what it found in the sets: the same now
                std::unordered_map<std::string, std::unordered_set<uint64_t>> added{};
                for (const SetOp& op : entry.sets) {
                    auto it{ ff.sets.find(op.set) };
                    bool present{ (it != ff.sets.end() && it->second.contains(op.key)) ||
                                  added[op.set].contains(op.key) };
                    if (op.insert) {
                        if (op.result == present) {
                            missReason = std::format(
                                "{} 0x{:x} {} in the fastfile",
                                op.set,
                                op.key,
                                present ? "already" : "not"
                            );
                            return false;
                        }
                        added[op.set].insert(op.key);
                    } else if (op.result != present) {
                        missReason =
                            std::format("{} 0x{:x} {} in the fastfile", op.set, op.key, present ? "already" : "not");
                        return false;
                    }
                }
                if (!entry.record.CanReplay(ff.data)) {
                    missReason = "script strings moved";
                    return false;
                }
                return true;
            }

          public:
            BO4LinkCacheImpl(BO4LinkContext& ctx, std::filesystem::path dir, std::string globalKey, bool rebuild)
                : ctx(ctx), dir(std::move(dir)), globalKey(std::move(globalKey)), rebuild(rebuild) {}

            ~BO4LinkCacheImpl() {
                if (rec) {
                    End(false);
                }
            }

            bool Replay(XAssetType type, const char* id, BO4FFContext& ff) override {
                if (!IsCacheable(type)) {
                    stats[games::bo4::pool::XAssetNameFromId(type)].uncached++;
                    return false;
                }
                if (rebuild) {
                    missReason = "rebuild";
                    return false;
                }
                std::string file{};
                std::string key{ KeyOf(type, id, ff, file) };
                Entry entry{};
                if (!Load(key, file, ff, entry)) {
                    return false;
                }
                // replayed as it was linked
                entry.record.Replay(ff.data);
                for (const SetOp& op : entry.sets) {
                    if (op.insert) {
                        ff.sets[op.set].insert(op.key);
                    }
                }
                for (const auto& [v, s] : entry.hashes) {
                    ctx.linkCtx.RegisterHash(v, s.data());
                }
                for (XPakRecord& x : entry.xpak) {
                    AddXPakItem(ff, x.key, std::move(x.data));
                }
                for (const auto& [p, d] : entry.outputs) {
                    std::filesystem::path out{ ctx.linkCtx.opt.m_output /
                                               std::filesystem::path{ utils::StrToWStr(p) } };
                    std::filesystem::create_directories(out.parent_path());
                    if (!utils::WriteFile(out, d)) {
                        throw std::runtime_error(std::format("link cache: can't write {}", out.string()));
                    }
                    LOG_INFO("Link cache: written {}", out.string());
                }
                used.insert(file);
                stats[games::bo4::pool::XAssetNameFromId(type)].replayed++;
                LOG_DEBUG("Link cache: replayed {} {}", games::bo4::pool::XAssetNameFromId(type), id);
                return true;
            }

            bool Begin(XAssetType type, const char* id, BO4FFContext& ff) override {
                if (!IsCacheable(type)) {
                    return false;
                }
                rec = std::make_unique<Recording>();
                rec->ff = &ff;
                rec->type = games::bo4::pool::XAssetNameFromId(type);
                rec->id = id;
                rec->key = KeyOf(type, id, ff, rec->file);
                rec->recorder.linker = &ff.data;
                if (missReason == "new" || missReason == "rebuild") {
                    LOG_DEBUG("Link cache: linking {} {} ({})", rec->type, id, missReason);
                } else {
                    LOG_INFO("Link cache: linking {} {} ({})", rec->type, id, missReason);
                }
                memory::SetActiveRecorder(&rec->recorder);
                ctx.linkCtx.hashRecord = &rec->hashes;
                cache::BeginTracking();
                return true;
            }

            void End(bool ok) override {
                if (!rec) {
                    return;
                }
                cache::TrackedFiles tracked{ cache::EndTracking() };
                memory::SetActiveRecorder(nullptr);
                ctx.linkCtx.hashRecord = nullptr;
                std::unique_ptr<Recording> r{ std::move(rec) };
                TypeStats& st{ stats[r->type] };
                st.linked++;
                if (!ok) {
                    return;
                }
                std::string taint{ r->taint.empty() ? tracked.taint : r->taint };
                std::unique_ptr<Entry> entry{ std::make_unique<Entry>() };
                if (taint.empty()) {
                    entry->record.Build(r->recorder, taint);
                }
                if (!taint.empty()) {
                    LOG_INFO("Link cache: {} {} can't be cached: {}", r->type, r->id, taint);
                    return;
                }
                entry->key = std::move(r->key);
                for (const std::string& p : tracked.read) {
                    files.Snapshot(p, entry->deps.emplace_back());
                }
                for (const std::string& p : tracked.directories) {
                    files.Snapshot(p, entry->deps.emplace_back());
                }
                for (const std::string& p : tracked.missing) {
                    cache::FileDep& d{ entry->deps.emplace_back() };
                    d.path = p;
                    d.exists = false;
                }
                entry->hashes = std::move(r->hashes);
                entry->sets = std::move(r->sets);
                entry->xpak = std::move(r->xpak);
                // the files written, under the output
                std::filesystem::path output{ std::filesystem::absolute(ctx.linkCtx.opt.m_output).lexically_normal() };
                for (const std::string& w : tracked.written) {
                    std::filesystem::path wp{ utils::StrToWStr(w) };
                    std::filesystem::path rel{ wp.lexically_relative(output) };
                    std::vector<byte> d{};
                    if (rel.empty() || rel.native().starts_with(L"..") || !utils::ReadFile(wp, d)) {
                        LOG_INFO("Link cache: {} {} can't be cached: can't keep {}", r->type, r->id, w);
                        return;
                    }
                    entry->outputs.emplace_back(utils::WStrToStr(rel.wstring()), std::move(d));
                }
                used.insert(r->file);
                writer.Add(dir / r->file, std::move(entry));
            }

            void Nested(XAssetType type) override {
                if (rec && !IsCacheable(type)) {
                    Taint(std::format("links a {}", games::bo4::pool::XAssetNameFromId(type)));
                }
            }

            void Taint(const std::string& why) override {
                if (rec && rec->taint.empty()) {
                    rec->taint = why;
                }
            }

            void RecordSet(BO4FFContext& ff, const std::string& set, uint64_t key, bool insert, bool result) override {
                if (!rec) {
                    return;
                }
                if (&ff != rec->ff) {
                    Taint("set of another fastfile");
                    return;
                }
                rec->sets.push_back({ set, key, insert, result });
            }

            void RecordXPakItem(BO4FFContext& ff, uint64_t key, const std::vector<byte>& data) override {
                if (!rec) {
                    return;
                }
                if (&ff != rec->ff) {
                    Taint("xpak of another fastfile");
                    return;
                }
                rec->xpak.push_back({ key, data });
            }

            std::string ComputedFile(const std::string& key, std::string& full) const {
                full = globalKey + "\ncomputed\n" + key;
                return cache::HexKey(
                           cache::HashBytes(full.data(), full.size(), 0),
                           cache::HashBytes(full.data(), full.size(), 1)
                       ) +
                       ".lcc";
            }

            bool LoadComputed(const std::string& key, std::vector<byte>& value) override {
                if (rebuild || rec) {
                    return false;
                }
                std::string full{};
                std::string file{ ComputedFile(key, full) };
                std::vector<byte> data{};
                if (!utils::ReadFile(dir / file, data)) {
                    return false;
                }
                try {
                    cache::Reader r{ data.data(), data.size() };
                    if (r.Read<uint64_t>() != COMPUTED_MAGIC || r.ReadString() != full) {
                        return false;
                    }
                    size_t count{ (size_t)r.Read<uint64_t>() };
                    for (size_t i = 0; i < count; i++) {
                        cache::FileDep d{};
                        d.path = r.ReadString();
                        d.exists = r.Read<bool>();
                        d.directory = r.Read<bool>();
                        d.size = r.Read<uint64_t>();
                        d.mtime = r.Read<uint64_t>();
                        d.hash = r.Read<uint64_t>();
                        if (!files.Matches(d)) {
                            LOG_INFO("Link cache: computing {} ({} {})", key, d.exists ? "changed" : "added", d.path);
                            return false;
                        }
                    }
                    size_t rawSize{ (size_t)r.Read<uint64_t>() };
                    std::vector<byte> comp{ r.ReadBytes() };
                    value.resize(rawSize);
                    int dr{ utils::compress::Decompress2(
                        utils::compress::COMP_LZ4,
                        value.data(),
                        value.size(),
                        comp.data(),
                        comp.size()
                    ) };
                    if (dr < 0 || (size_t)dr != rawSize) {
                        return false;
                    }
                } catch (std::exception&) {
                    return false;
                }
                used.insert(file);
                LOG_DEBUG("Link cache: computed {} from the cache", key);
                return true;
            }

            bool BeginComputed() override {
                if (rec) {
                    return false;
                }
                cache::BeginTracking();
                return true;
            }

            void StoreComputed(const std::string& key, const std::vector<byte>* value) override {
                cache::TrackedFiles tracked{ cache::EndTracking() };
                if (!value) {
                    return;
                }
                if (!tracked.taint.empty()) {
                    LOG_INFO("Link cache: {} can't be cached: {}", key, tracked.taint);
                    return;
                }
                std::string full{};
                std::string file{ ComputedFile(key, full) };
                cache::Writer w{};
                w.Write(COMPUTED_MAGIC);
                w.WriteString(full);
                w.Write<uint64_t>(tracked.read.size() + tracked.directories.size() + tracked.missing.size());
                auto WriteDep = [&w](const cache::FileDep& d) {
                    w.WriteString(d.path);
                    w.Write(d.exists);
                    w.Write(d.directory);
                    w.Write(d.size);
                    w.Write(d.mtime);
                    w.Write(d.hash);
                };
                for (const std::vector<std::string>* list : { &tracked.read, &tracked.directories }) {
                    for (const std::string& p : *list) {
                        cache::FileDep d{};
                        files.Snapshot(p, d);
                        WriteDep(d);
                    }
                }
                for (const std::string& p : tracked.missing) {
                    cache::FileDep d{};
                    d.path = p;
                    WriteDep(d);
                }
                std::vector<byte> comp{};
                if (!utils::compress::CompressBuffer(utils::compress::COMP_LZ4, *value, comp)) {
                    return;
                }
                w.Write<uint64_t>(value->size());
                w.WriteBytes(comp.data(), comp.size());
                if (!utils::WriteFile(dir / file, w.data)) {
                    LOG_WARNING("Link cache: can't write {}", (dir / file).string());
                    return;
                }
                used.insert(file);
            }

            const std::filesystem::path& Directory() const override { return dir; }
            const std::string& Key() const override { return globalKey; }
            bool Rebuild() const override { return rebuild; }

            void Start(BO4LinkContext& ctx) override {
                std::filesystem::create_directories(dir);
                cache::SetTrackingRoots(ctx.linkCtx.opt.m_output, { cache::GetModuleDirectory() });
                LOG_INFO("Link cache: {}{}", dir.string(), rebuild ? " (rebuild)" : "");
                if (rebuild) {
                    return;
                }
                // the script strings of the last link, at their indices
                std::vector<byte> data{};
                if (!utils::ReadFile(StringsPath(), data)) {
                    return;
                }
                try {
                    cache::Reader r{ data.data(), data.size() };
                    if (r.Read<uint64_t>() != STRINGS_MAGIC || r.ReadString() != globalKey) {
                        return;
                    }
                    size_t count{ (size_t)r.Read<uint64_t>() };
                    std::vector<std::string> strs(count);
                    for (std::string& s : strs) {
                        s = r.ReadString();
                    }
                    if (!ctx.mainFF.data.scrStrings.empty()) {
                        return;
                    }
                    for (const std::string& s : strs) {
                        ctx.mainFF.data.AddScrStringKeepEmpty(s.data());
                    }
                    LOG_DEBUG("Link cache: {} script strings from the last link", count);
                } catch (std::exception& e) {
                    LOG_WARNING("Link cache: bad {}: {}", StringsPath().string(), e.what());
                }
            }

            void Finish(BO4LinkContext& ctx) override {
                size_t failed{ writer.Wait() };

                cache::Writer w{};
                w.Write(STRINGS_MAGIC);
                w.WriteString(globalKey);
                w.Write<uint64_t>(ctx.mainFF.data.scrStrings.size());
                for (const char* s : ctx.mainFF.data.scrStrings) {
                    w.WriteString(s);
                }
                if (!utils::WriteFile(StringsPath(), w.data)) {
                    LOG_WARNING("Link cache: can't write {}", StringsPath().string());
                }

                // the entries of the assets not in the zone anymore
                size_t removed{};
                std::error_code ec{};
                for (const std::filesystem::directory_entry& e : std::filesystem::directory_iterator{ dir, ec }) {
                    std::string name{ e.path().filename().string() };
                    if (((name.ends_with(".lce") || name.ends_with(".lcc")) && !used.contains(name)) ||
                        name.ends_with(".tmp")) {
                        std::filesystem::remove(e.path(), ec);
                        removed++;
                    }
                }

                TypeStats total{};
                for (const auto& [type, st] : stats) {
                    total.replayed += st.replayed;
                    total.linked += st.linked;
                    total.uncached += st.uncached;
                    LOG_DEBUG(
                        "Link cache: {}: {} replayed, {} linked, {} not cached",
                        type,
                        st.replayed,
                        st.linked,
                        st.uncached
                    );
                }
                LOG_INFO(
                    "Link cache: {} asset(s) replayed, {} linked, {} of types not cached{}{}",
                    total.replayed,
                    total.linked,
                    total.uncached,
                    removed ? std::format(", {} old entries removed", removed) : "",
                    failed ? std::format(", {} entries not written", failed) : ""
                );
            }
        };
    } // namespace

    std::unique_ptr<BO4LinkCache> CreateLinkCache(BO4LinkContext& ctx) {
        FastFileLinkerOption& opt{ ctx.linkCtx.opt };
        if (opt.noCache) {
            return nullptr;
        }
        std::string module{ cache::GetModuleIdentity() };
        if (module.empty()) {
            LOG_WARNING("Link cache: can't identify the acts build, linked without it");
            return nullptr;
        }
        if (!cache::InitTracking()) {
            LOG_WARNING("Link cache: can't track the files, linked without it");
            return nullptr;
        }
        std::filesystem::path zoneFile{ std::filesystem::absolute(ctx.linkCtx.zoneFile).lexically_normal() };
        std::filesystem::path output{ std::filesystem::absolute(opt.m_output).lexically_normal() };

        std::vector<std::string> cfgs{};
        for (const auto& [k, v] : ctx.linkCtx.zone.cfgs) {
            cfgs.push_back(std::format("{}={}", k, v ? v : ""));
        }
        std::sort(cfgs.begin(), cfgs.end());
        std::string globalKey{ std::format(
            "acts link cache {}\n{}\n{}\n{}\nplatform={} server={}\n",
            CACHE_VERSION,
            module,
            zoneFile.string(),
            ctx.mainFF.ffname,
            (int)opt.platform,
            opt.server
        ) };
        for (const std::string& c : cfgs) {
            globalKey += "cfg " + c + "\n";
        }
        for (const char* d : opt.defines) {
            globalKey += std::format("define {}\n", d);
        }

        std::filesystem::path dir{ opt.cacheDir };
        if (dir.empty()) {
            const char* local{ std::getenv("LOCALAPPDATA") };
            std::string where{ zoneFile.string() + "\n" + output.string() };
            dir = std::filesystem::path{ local ? local : "." } / "acts" / "link_cache" /
                  std::format("{}_{:016x}", ctx.mainFF.ffname, cache::HashBytes(where.data(), where.size()));
        }
        return std::make_unique<BO4LinkCacheImpl>(ctx, dir, std::move(globalKey), opt.rebuildCache);
    }
} // namespace fastfile::linker::bo4
