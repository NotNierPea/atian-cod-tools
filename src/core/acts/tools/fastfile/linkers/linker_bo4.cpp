#include <includes.hpp>
#include <tools/fastfile/linkers/linker_bo4.hpp>
#include <core/preprocessor.hpp>

namespace fastfile::linker::bo4 {
    static BO4LinkContext* linkContext{};

    BO4FFContext::BO4FFContext() : data{ XFILE_BLOCK_COUNT, XFILE_BLOCK_VIRTUAL } {
        data.SetMode(fastfile::linker::memory::LM_DATA);
        data.SetBlockType(XFILE_BLOCK_TEMP, fastfile::linker::memory::BLOCKTYPE_TEMP);
        data.SetBlockType(XFILE_BLOCK_TEMP_PRELOAD, fastfile::linker::memory::BLOCKTYPE_TEMP);
        data.SetBlockType(XFILE_BLOCK_RUNTIME_VIRTUAL, fastfile::linker::memory::BLOCKTYPE_RUNTIME);
        data.SetBlockType(XFILE_BLOCK_RUNTIME_PHYSICAL, fastfile::linker::memory::BLOCKTYPE_RUNTIME);
        data.SetBlockType(XFILE_BLOCK_VIRTUAL, fastfile::linker::memory::BLOCKTYPE_VIRTUAL);
        data.SetBlockType(XFILE_BLOCK_PHYSICAL, fastfile::linker::memory::BLOCKTYPE_VIRTUAL);
        data.SetBlockType(XFILE_BLOCK_STREAMER, fastfile::linker::memory::BLOCKTYPE_RUNTIME);
        data.SetBlockType(XFILE_BLOCK_STREAMER_CPU, fastfile::linker::memory::BLOCKTYPE_RUNTIME);
        data.SetBlockType(XFILE_BLOCK_MEMMAPPED, fastfile::linker::memory::BLOCKTYPE_UNKNOWN);
    }
    std::unordered_map<XAssetType, XAssetLinker*>& GetWorkers() {
        // could be an array like in bo3 linker
        static std::unordered_map<XAssetType, XAssetLinker*> workers{};
        return workers;
    }

    uint32_t BO4LinkContext::HashScr(const char* str) {
        if (!str) {
            return 0;
        }
        uint64_t r;
        if (hash::TryHashPattern(str, r)) {
            return (uint32_t)r; // nothing to add
        }

        r = hash::HashT89Scr(str);

        linkCtx.RegisterHash(r, str);

        return (uint32_t)r;
    }

    bool BO4FFContext::SetContains(const std::string& set, uint64_t key) {
        auto it{ sets.find(set) };
        bool r{ it != sets.end() && it->second.contains(key) };
        if (linkContext && linkContext->cache) {
            linkContext->cache->RecordSet(*this, set, key, false, r);
        }
        return r;
    }

    bool BO4FFContext::SetInsert(const std::string& set, uint64_t key) {
        bool r{ sets[set].insert(key).second };
        if (linkContext && linkContext->cache) {
            linkContext->cache->RecordSet(*this, set, key, true, r);
        }
        return r;
    }

    BO4FFContext& BO4LinkContext::GetFFContext(const char* prefix) {
        if (cache && linkDepth) {
            cache->Taint("prefixed fastfile");
        }
        BO4FFContext& sctx{ ffs[prefix] };
        if (!sctx.ffnameHash) {
            sctx.ffname = this->linkCtx.strs.CloneStr(utils::va("%s%s", prefix, mainFF.ffname));
            sctx.ffnameHash = HashXHash(sctx.ffname);
            sctx.data.SetMode(fastfile::linker::memory::LM_DATA);
        }
        return sctx;
    }

    uint64_t BO4LinkContext::HashXHash(const char* str, bool ignoreTop) {
        if (!str) {
            return 0;
        }
        if (ignoreTop && *str == '#') {
            str++;
        }
        uint64_t r;
        if (hash::TryHashPattern(str, r)) {
            return r; // nothing to add
        }

        r = hash::Hash64(str);

        linkCtx.RegisterHash(r, str);

        return r;
    }

    uint64_t BO4LinkContext::HashPathName(const std::filesystem::path& path) {
        std::filesystem::path p{ path };
        if (p.has_extension()) {
            p.replace_extension();
        }
        std::string fn{ p.filename().string() };
        uint64_t r;
        if (!hash::TryHashPattern(fn.data(), r)) {
            fn = path.string();
            r = HashXHash(fn.data());
        }
        LOG_TRACE("Hash path {} -> 0x{:x}", path.string(), r);
        return r;
    }

    BO4LinkContext& GetLinkContext() {
        if (!linkContext) {
            throw std::runtime_error("No BO4LinkContext");
        }
        return *linkContext;
    }

    class FFLinkerBO4 : public FFLinker {
      public:
        FFLinkerBO4() : FFLinker("Bo4", "Bo4 fastfile linker") {}

        void Link(FastFileLinkerContext& ctx) override {
            BO4LinkContext bo4ctx{ ctx };
            linkContext = &bo4ctx;
            utils::CloseEnd linkContextCE{ [] { linkContext = nullptr; } };
            bo4ctx.mainFF.ffname = ctx.mainFFName;
            bo4ctx.mainFF.ffnameHash = bo4ctx.HashXHash(ctx.mainFFName);
            std::unique_ptr<BO4LinkCache> cache{ CreateLinkCache(bo4ctx) };
            bo4ctx.cache = cache.get();
            utils::CloseEnd cacheCE{ [&bo4ctx] { bo4ctx.cache = nullptr; } };
            if (cache) {
                cache->Start(bo4ctx);
            }

            // the time of each step, logged at the end (the top assets with the ones they link)
            struct StepTime {
                double seconds{};
                size_t count{};
            };
            std::map<std::string, StepTime> times{};
            auto Elapsed = [](std::chrono::steady_clock::time_point start) {
                return std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
            };

            for (auto& [t, d] : ctx.zone.assets) {
                XAssetType type{ games::bo4::pool::XAssetIdFromName(t.data()) };
                if (type == XAssetType::ASSET_TYPE_COUNT) {
                    bo4ctx.error = true;
                    LOG_ERROR("Can't find asset type: {}", t);
                    continue;
                }

                StepTime& time{ times[t] };
                for (fastfile::zone::AssetData& assetData : d) {
                    std::chrono::steady_clock::time_point start{ std::chrono::steady_clock::now() };
                    void* ref{};
                    bo4ctx.zoneAsset = true;
                    bo4ctx.LinkAsset(type, assetData.value, ref, true, &bo4ctx.mainFF);
                    assetData.handled = true;
                    time.seconds += Elapsed(start);
                    time.count++;
                }
            }

            // final linking for all the fastfiles
            for (auto& [type, linker] : GetWorkers()) {
                std::chrono::steady_clock::time_point start{ std::chrono::steady_clock::now() };
                linker->ComputeFinal(bo4ctx, bo4ctx.mainFF);
                for (auto& [ffname, ff] : bo4ctx.ffs) {
                    linker->ComputeFinal(bo4ctx, ff);
                }
                double seconds{ Elapsed(start) };
                if (seconds >= 0.01) {
                    StepTime& time{ times[std::format("final {}", games::bo4::pool::XAssetNameFromId(type))] };
                    time.seconds += seconds;
                    time.count++;
                }
            }

            if (bo4ctx.error) {
                throw std::runtime_error("Error when linking fast file data");
            }
            if (cache) {
                std::chrono::steady_clock::time_point start{ std::chrono::steady_clock::now() };
                cache->Finish(bo4ctx);
                bo4ctx.cache = nullptr;
                times["final link cache"] = { Elapsed(start), 1 };
            }

            auto LinkFF = [&bo4ctx](BO4FFContext& ff, std::vector<byte>& data, size_t* blocks) {
                ff.data.SetMode(fastfile::linker::memory::LM_HEADER);

                // add to ctx.data the assets/strings headers
                XAssetList assetlist{};

                // write header
                assetlist.stringList.count = (int)ff.data.scrStrings.size() + 1; // +1 for the null
                assetlist.stringList.strings = fastfile::linker::memory::POINTER_NEXT;

                if (ff.data.assets.size()) {
                    assetlist.assetCount = (int)ff.data.assets.size();
                    assetlist.assets = fastfile::linker::memory::POINTER_NEXT;
                }
                // DB_LoadXFile copies the list in a local, it takes no room in the virtual block: the offset
                // pointers into it (data chunk refs) would be 0x20 too far
                ff.data.PushStream(XFILE_BLOCK_TEMP);
                ff.data.WriteStream(assetlist);
                ff.data.PopStream();

                ff.data.PushStream(XFILE_BLOCK_VIRTUAL);

                ff.data.PushStream(XFILE_BLOCK_VIRTUAL);
                // write string ref array
                ff.data.Align<void*>();

                ff.data.WriteStream<void*>(nullptr); // empty str
                for (size_t i = 0; i < ff.data.scrStrings.size(); i++) {
                    ff.data.WriteStream<void*>((void*)fastfile::linker::memory::POINTER_NEXT);
                }

                // write strings
                for (const char* str : ff.data.scrStrings) {
                    ff.data.Align<char>();
                    ff.data.WriteStream(str);
                }
                ff.data.PopStream();

                if (ff.data.assets.size()) {
                    ff.data.Align(8); // GetAlignment_XAsset

                    struct XAsset {
                        games::bo4::pool::XAssetType type;
                        uintptr_t header; // XAssetHeader
                    };
                    // write asset array

                    XAsset tmpAsset{};
                    for (const fastfile::linker::memory::AssetData* asset : ff.data.assets) {
                        tmpAsset.type = (games::bo4::pool::XAssetType)asset->type;
                        tmpAsset.header = (uintptr_t)asset->header;
                        ff.data.WriteStream(tmpAsset);
                    }
                }
                ff.data.PopStream();

                ff.data.Link(blocks);

                /*
                 * A map zone (a clipMap in it) gets the block the lobby's preload loads it into: the Preload_* loaders
                 * push XFILE_BLOCK_TEMP_PRELOAD where the Load_* ones push XFILE_BLOCK_TEMP, and it doesn't rewind (the
                 * level's temp data stays there until DB_FinishPreloadedZone). Empty, the preload wrote the clipMap_t at
                 * null. Stock levels' are the temp data so counted plus 0x1D8 to 0x248 (zm_towers, zm_mansion,
                 * zm_office, zm_zodt8 read back).
                 */
                constexpr size_t PRELOAD_MARGIN = 0x1000;
                for (const fastfile::linker::memory::AssetData* asset : ff.data.assets) {
                    if (asset->type == games::bo4::pool::ASSET_TYPE_CLIPMAP) {
                        blocks[XFILE_BLOCK_TEMP_PRELOAD] = std::max<size_t>(
                            blocks[XFILE_BLOCK_TEMP_PRELOAD], ff.data.blocks[XFILE_BLOCK_TEMP].totalSize + PRELOAD_MARGIN
                        );
                        LOG_INFO("Preload block of {}: 0x{:x}", ff.ffname, blocks[XFILE_BLOCK_TEMP_PRELOAD]);
                        break;
                    }
                }

                ff.data.WriteLinkedData(data);

                LOG_INFO(
                    "Fastfile {} data linked with {} asset(s) and {} string(s)",
                    ff.ffname,
                    assetlist.assetCount,
                    assetlist.stringList.count
                );
            };

            std::chrono::steady_clock::time_point linkStart{ std::chrono::steady_clock::now() };
            fastfile::FastFile& mff{ ctx.fastfiles.emplace_back() };
            mff.ffname = ctx.mainFFName;
            LinkFF(bo4ctx.mainFF, mff.linkedData, mff.blockSizes);

            for (auto& [k, ff] : bo4ctx.ffs) {
                fastfile::FastFile& mff{ ctx.fastfiles.emplace_back() };
                mff.ffname = ff.ffname;
                LinkFF(ff, mff.linkedData, mff.blockSizes);
            }
            times["final link data"] = { Elapsed(linkStart), 1 };

            std::vector<std::pair<std::string, StepTime>> sorted{ times.begin(), times.end() };
            std::sort(sorted.begin(), sorted.end(), [](const auto& a, const auto& b) {
                return a.second.seconds > b.second.seconds;
            });
            for (const auto& [name, time] : sorted) {
                if (time.seconds >= 0.05) {
                    LOG_INFO("Link time {:>8.2f}s {} ({})", time.seconds, name, time.count);
                }
            }
        }
    };

    void BO4LinkContext::LinkAsset(XAssetType type, const char* id, void*& ref, bool addAsset, BO4FFContext* ff) {
        // assign default ff
        if (!ff) {
            ff = &mainFF;
        }

        bool zoneAsset{ this->zoneAsset };
        this->zoneAsset = false;
        LOG_DEBUG(
            "BO4LinkContext::LinkAsset(type={}, id={}, addAsset={}, ff={})",
            games::bo4::pool::XAssetNameFromId(type),
            !id ? "nullptr" : id,
            addAsset ? "true" : "false",
            ff->ffname
        );
        if (!id) {
            ref = nullptr;
            return; // empty id = no asset to link
        }

        // ignore hash identifier
        const char* real{ id };
        if (*id == '#') {
            id++;
        }
        bool defaultAsset{ *id == ',' };
        if (defaultAsset) {
            id++;
        }

        uint64_t hash{ hash::Hash64(id) };

        /*
         * An asset already in this fastfile is linked again by its name (an empty asset the game resolves to it): two
         * of one name in a zone break its unload, the pool keeps a freed one (two aitypes' inline aiFxBundle left a
         * scriptbundle dangling, the frontend's spawn then crashed walking the scriptbundles). Only for the types with
         * an empty asset (a size and a name in it).
         */
        if (!defaultAsset && !ff->SetInsert("assets", hash ^ ((uint64_t)type * 0x9E3779B97F4A7C15ull))) {
            size_t len{ games::bo4::pool::GetAssetSize(type) };
            std::vector<byte> probe(len);
            if (len && games::bo4::pool::GetAssetName(type, probe.data(), len)) {
                LOG_DEBUG("{} {} already in {}, linked by its name", games::bo4::pool::XAssetNameFromId(type), id, ff->ffname);
                defaultAsset = true;
            }
        }
        /*
        auto ait{ ff->allocatedAssets.find(hash) };

        if (ait != ff->allocatedAssets.end()) {
                // we already know this asset, we can link it there

                // ait->second->refs.push_back(&ref); // fixme
                LOG_INFO("Reuse existing {} {}", games::bo4::pool::XAssetNameFromId(type), id);
                return;
        }
        */

        ref = (void*)fastfile::linker::memory::POINTER_NEXT;
        ff->data.PushStream(XFILE_BLOCK_TEMP);

        fastfile::linker::memory::LinkerDataChunk* align{ ff->data.Align(8) }; // GetAlignment_XAsset

        fastfile::linker::memory::LinkerDataChunk* computedRef{};
        fastfile::linker::memory::AssetData* asset{};
        if (!defaultAsset) {
            std::unordered_map<XAssetType, XAssetLinker*>& workers{ GetWorkers() };
            auto it{ workers.find(type) };

            if (it == workers.end()) {
                LOG_ERROR("Can't find asset linker for type {}", games::bo4::pool::XAssetNameFromId(type));
                error = true;
                return;
            }

            // add the asset to the list if required
            if (addAsset && !it->second->isGrouped) {
                asset = ff->data.AddAsset(type, align);
            }
            // a top asset replayed from the link cache or recorded in it
            bool top{ cache && !linkDepth && zoneAsset };
            if (cache && linkDepth) {
                cache->Nested(type);
            }
            bool recording{};
            try {
                if (top && cache->Replay(type, real, *ff)) {
                    ff->data.PopStream();
                    return;
                }
                recording = top && cache->Begin(type, real, *ff);
                linkDepth++;
                utils::CloseEnd depthCE{ [this] { linkDepth--; } };
                bool errorBefore{ error };
                it->second->Compute(*this, id, *ff);

                if (recording) {
                    recording = false;
                    cache->End(!errorBefore && !error);
                }

                if (computedRef) {
                    ff->allocatedAssets[hash] = computedRef;
                }
            } catch (std::runtime_error& e) {
                if (recording) {
                    cache->End(false);
                }
                error = true;
                LOG_ERROR("Can't link asset {},{} : {}", games::bo4::pool::XAssetNameFromId(type), id, e.what());
            }
        } else {
            // create empty asset

            size_t len{ games::bo4::pool::GetAssetSize(type) };

            if (!len) {
                LOG_ERROR("Can't link empty asset {}: '{}'", games::bo4::pool::XAssetNameFromId(type), real);
                error = true;
                return;
            }

            if (addAsset) {
                asset = ff->data.AddAsset(type, align);
            }

            computedRef = ff->data.AllocStream(len);
            void* emptyAsset{ computedRef->As<void>() };
            XHash* h{ games::bo4::pool::GetAssetName(type, emptyAsset, len) };

            if (!h) {
                LOG_ERROR(
                    "Can't link asset {} with invalid name location: '{}'",
                    games::bo4::pool::XAssetNameFromId(type),
                    real
                );
                error = true;
                return;
            }
            // we add the flag 63 to mark it as default
            h->name = HashPathName(id) | ~hash::MASK63;

            LOG_INFO("Add default asset {} {} (hash_{:x})", games::bo4::pool::XAssetNameFromId(type), id, h->name);
        }
        ff->data.PopStream();
    }

    size_t BO4LinkContext::LinkAssetArray(
        XAssetType type, const char* id, core::config::RapidJsonGeneric& cfg, void** array, size_t count,
        BO4FFContext* ff
    ) {
        if (cfg.IsNull()) {
            return 0;
        }
        if (!cfg.IsArray()) {
            LOG_ERROR("BO4LinkContext::LinkAssetArray: {} isn't an array", id);
            error = true;
            return 0;
        }
        auto arr{ cfg.GetArray() };
        if (arr.Size() > count) {
            LOG_ERROR("BO4LinkContext::LinkAssetArray: {} can't contain more than {} element(s)", id, count);
            error = true;
            return 0;
        }

        for (rapidjson::Value& v : arr) {
            if (!v.IsString()) {
                LOG_ERROR("BO4LinkContext::LinkAssetArray: {} can't contain a non string element", id);
                error = true;
                continue;
            }

            const char* str{ v.GetString() };
            LinkAsset(type, str, *array, false, ff);
            array++;
        }

        return arr.Size();
    }

    void BO4LinkContext::AddXHash(const char* val, XHash& value) { value.name = HashXHash(val, true); }

    size_t
    BO4LinkContext::AddXHashArray(const char* id, core::config::RapidJsonGeneric& cfg, XHash* array, size_t count) {
        if (cfg.IsNull()) {
            return 0;
        }
        if (!cfg.IsArray()) {
            LOG_ERROR("BO4LinkContext::AddXHashArray: {} isn't an array", id);
            error = true;
            return 0;
        }
        auto arr{ cfg.GetArray() };
        if (arr.Size() > count) {
            LOG_ERROR("BO4LinkContext::AddXHashArray: {} can't contain more than {} element(s)", id, count);
            error = true;
            return 0;
        }

        for (rapidjson::Value& v : arr) {
            if (!v.IsString()) {
                LOG_ERROR("BO4LinkContext::AddXHashArray: {} can't contain a non string element", id);
                error = true;
                continue;
            }

            const char* str{ v.GetString() };
            AddXHash(str, *array);
            array++;
        }

        return arr.Size();
    }

    void BO4LinkContext::AddScrString(const char* val, ScrString_t& value, BO4FFContext& ff) {
        value = (ScrString_t)ff.data.AddScrString(val);
    }

    size_t BO4LinkContext::AddScrStringArray(
        const char* id, core::config::RapidJsonGeneric& cfg, ScrString_t* array, size_t count, BO4FFContext& ff
    ) {
        if (cfg.IsNull()) {
            return 0;
        }
        if (!cfg.IsArray()) {
            LOG_ERROR("BO4LinkContext::AddXHashArray: {} isn't an array", id);
            error = true;
            return 0;
        }
        auto arr{ cfg.GetArray() };
        if (arr.Size() > count) {
            LOG_ERROR("BO4LinkContext::AddXHashArray: {} can't contain more than {} element(s)", id, count);
            error = true;
            return 0;
        }

        for (rapidjson::Value& v : arr) {
            if (!v.IsString()) {
                LOG_ERROR("BO4LinkContext::AddXHashArray: {} can't contain a non string element", id);
                error = true;
                continue;
            }

            const char* str{ v.GetString() };
            AddScrString(str, *array, ff);
            array++;
        }

        return arr.Size();
    }

    bool BO4LinkContext::Assert(bool expr, const char* msg) {
        if (!expr) {
            LOG_ERROR("BO4LinkContext::Assert: {}", msg);
            error = true;
        }
        return expr;
    }

    utils::ArrayAdder<FFLinkerBO4, FFLinker> impl{ GetLinkers() };
} // namespace fastfile::linker::bo4