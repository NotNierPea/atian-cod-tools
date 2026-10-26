#include <includes.hpp>
#include <tools/fastfile/linkers/linker_bo4.hpp>

/*
 * staticlevelfxlist (retail 0x30, the loader sub_142E2CF70, the layout in bo4_unlinker_map_staticlevelfx.cpp): a map's
 * placed effects, named as its col_map. CG_SpawnStaticLevelFXList (0x1409E8370) spawns each entry for the local client
 * when the level starts (sub_1409E7580, at most 11000) and the lens flares of the name list.
 *
 * An entry (0x50): its fx (by name here: an fx of the zone or a common one), origin, angles (degrees), start time (ms,
 * stock maps a random -30000..0: the loop already running), 1.0, the cull distance squared (0: always there, played
 * at once; else the game plays it only within it), byte 0x30 (a flag for the fx system, stock 0), byte 0x31 (played at
 * the start even with exploders; stock 1 for the plain ones), a mask (stock 0xF), a mode name (none: every mode; set,
 * only that one), the exploders that play it (0x28 records: their name, then 0x20 bytes, stock zeros).
 *
 * {
 *     "name": "maps/zm/enh_test.d3dbsp",
 *     "fx": [
 *         { "fx": "bfn_gp/light_0", "origin": [ 0, 0, 0 ], "angles": [ 0, 90, 0 ], "start": -5000, "cull": 3000 },
 *         { "fx": "#hash_412ac59596fcf377", "data": "<0x30 bytes from 0x08, a dump's>", "exploders": [ { "name": "x" } ] }
 *     ],
 *     "lensFlares": [ "name" ]
 * }
 * Optional entry keys: "flag" (byte 0x30), "now" (byte 0x31, default true), "mask" (default 15), "scale" (default 1),
 * "mode". An entry with "data" (a dump's plain part) keeps it as it is.
 */
namespace {
    using namespace fastfile::linker::bo4;

    struct StaticLevelFxRecord {
        const char* name;
        byte data[0x20];
    };
    static_assert(sizeof(StaticLevelFxRecord) == 0x28);

    struct StaticLevelFx {
        void* fx;
        float origin[3];
        float angles[3];
        int32_t startTime;
        float scale;
        float cullDistSq;
        uint32_t unk2c;
        byte flag30;
        byte now31;
        byte pad32[2];
        uint32_t mask;
        uint32_t numRecords;
        uint32_t unk3c;
        const char* mode;
        StaticLevelFxRecord* records;
    };
    static_assert(sizeof(StaticLevelFx) == 0x50);

    struct StaticLevelFxList {
        XHash name;
        uint32_t numFx;
        StaticLevelFx* fx;
        uint32_t numFlares;
        const char** flares;
    };
    static_assert(sizeof(StaticLevelFxList) == 0x30);

    struct Record {
        std::string name;
        std::vector<byte> data;
    };

    struct Entry {
        std::string fx;
        StaticLevelFx plain{};
        std::string mode;
        std::vector<Record> records;
    };

    std::vector<byte> Hex(const char* s) {
        std::vector<byte> out{};
        for (size_t i = 0; s[i] && s[i + 1]; i += 2) {
            char b[3]{ s[i], s[i + 1], 0 };
            out.push_back((byte)std::strtoul(b, nullptr, 16));
        }
        return out;
    }

    bool Floats(const rapidjson::Value& v, const char* key, float* out, size_t n) {
        auto it{ v.FindMember(key) };
        if (it == v.MemberEnd()) {
            return true;
        }
        if (!it->value.IsArray() || it->value.Size() != n) {
            return false;
        }
        for (rapidjson::SizeType i = 0; i < n; i++) {
            if (!it->value[i].IsNumber()) {
                return false;
            }
            out[i] = (float)it->value[i].GetDouble();
        }
        return true;
    }

    class XAssetLinkerImpl : public XAssetLinker {
      public:
        using XAssetLinker::XAssetLinker;

        void Compute(BO4LinkContext& ctx, const char* id, BO4FFContext& ff) override {
            std::filesystem::path path{ ctx.linkCtx.input / id };
            BO4Json cfg{ path };
            if (!cfg.SyncConfig(false)) {
                LOG_ERROR("Can't read staticlevelfxlist {}", path.string());
                ctx.error = true;
                return;
            }
            std::string assetName{ cfg.GetString("name", "") };
            if (assetName.empty()) {
                LOG_ERROR("staticlevelfxlist {}: no name (maps/<mode>/<map>.d3dbsp)", path.string());
                ctx.error = true;
                return;
            }

            std::vector<Entry> entries{};
            std::vector<std::string> flares{};
            const rapidjson::Value& root{ cfg.main };
            auto fxIt{ root.FindMember("fx") };
            if (fxIt != root.MemberEnd() && fxIt->value.IsArray()) {
                for (const rapidjson::Value& v : fxIt->value.GetArray()) {
                    Entry& e{ entries.emplace_back() };
                    auto name{ v.FindMember("fx") };
                    if (!v.IsObject() || name == v.MemberEnd() || !name->value.IsString()) {
                        LOG_ERROR("staticlevelfxlist {}: entry {} has no fx", path.string(), entries.size() - 1);
                        ctx.error = true;
                        return;
                    }
                    e.fx = name->value.GetString();
                    StaticLevelFx& p{ e.plain };
                    auto data{ v.FindMember("data") };
                    if (data != v.MemberEnd() && data->value.IsString()) {
                        std::vector<byte> raw{ Hex(data->value.GetString()) };
                        if (raw.size() != 0x30) {
                            LOG_ERROR("staticlevelfxlist {}: entry {}: data isn't 0x30 bytes", path.string(), entries.size() - 1);
                            ctx.error = true;
                            return;
                        }
                        std::memcpy((byte*)&p + 8, raw.data(), raw.size());
                    } else {
                        p.scale = 1.0f;
                        p.now31 = 1;
                        p.mask = 0xF;
                        if (!Floats(v, "origin", p.origin, 3) || !Floats(v, "angles", p.angles, 3)) {
                            LOG_ERROR("staticlevelfxlist {}: entry {}: bad origin or angles", path.string(), entries.size() - 1);
                            ctx.error = true;
                            return;
                        }
                        auto num = [&v](const char* key, double def) {
                            auto it{ v.FindMember(key) };
                            return it != v.MemberEnd() && it->value.IsNumber() ? it->value.GetDouble() : def;
                        };
                        p.startTime = (int32_t)num("start", 0);
                        p.scale = (float)num("scale", 1.0);
                        double cull{ num("cull", 0) };
                        p.cullDistSq = (float)(cull * cull);
                        p.flag30 = (byte)num("flag", 0);
                        auto now{ v.FindMember("now") };
                        if (now != v.MemberEnd() && now->value.IsBool()) {
                            p.now31 = now->value.GetBool() ? 1 : 0;
                        }
                        p.mask = (uint32_t)num("mask", 0xF);
                    }
                    auto mode{ v.FindMember("mode") };
                    if (mode != v.MemberEnd() && mode->value.IsString()) {
                        e.mode = mode->value.GetString();
                    }
                    auto exps{ v.FindMember("exploders") };
                    if (exps != v.MemberEnd() && exps->value.IsArray()) {
                        for (const rapidjson::Value& r : exps->value.GetArray()) {
                            Record& rec{ e.records.emplace_back() };
                            if (r.IsString()) {
                                rec.name = r.GetString();
                            } else if (r.IsObject()) {
                                auto n{ r.FindMember("name") };
                                if (n != r.MemberEnd() && n->value.IsString()) {
                                    rec.name = n->value.GetString();
                                }
                                auto d{ r.FindMember("data") };
                                if (d != r.MemberEnd() && d->value.IsString()) {
                                    rec.data = Hex(d->value.GetString());
                                }
                            }
                            rec.data.resize(0x20);
                        }
                    }
                }
            }
            auto flIt{ root.FindMember("lensFlares") };
            if (flIt != root.MemberEnd() && flIt->value.IsArray()) {
                for (const rapidjson::Value& v : flIt->value.GetArray()) {
                    if (v.IsString()) {
                        flares.emplace_back(v.GetString());
                    }
                }
            }
            if (entries.size() > 11000) {
                LOG_ERROR("staticlevelfxlist {}: {} entries, the game takes 11000", path.string(), entries.size());
                ctx.error = true;
                return;
            }

            ff.data.PushStream(XFILE_BLOCK_TEMP);
            ff.data.Align(8);
            StaticLevelFxList& list{ ff.data.AllocStreamRef<StaticLevelFxList>() };
            list = {};
            list.name.name = ctx.HashXHash(assetName, true);
            list.numFx = (uint32_t)entries.size();
            list.fx = entries.empty() ? nullptr : (StaticLevelFx*)fastfile::linker::memory::POINTER_NEXT;
            list.numFlares = (uint32_t)flares.size();
            list.flares = flares.empty() ? nullptr : (const char**)fastfile::linker::memory::POINTER_NEXT;

            // sub_142E2CF70's order: the entries, each one's fx, mode and records (their names after them), the flares
            ff.data.PushStream(XFILE_BLOCK_VIRTUAL);
            if (!entries.empty()) {
                ff.data.Align(8);
                std::vector<StaticLevelFx> out{};
                for (Entry& e : entries) {
                    StaticLevelFx p{ e.plain };
                    p.fx = (void*)fastfile::linker::memory::POINTER_NEXT;
                    p.numRecords = (uint32_t)e.records.size();
                    p.unk3c = 0;
                    p.mode = e.mode.empty() ? nullptr : (const char*)fastfile::linker::memory::POINTER_NEXT;
                    p.records = e.records.empty() ? nullptr : (StaticLevelFxRecord*)fastfile::linker::memory::POINTER_NEXT;
                    out.push_back(p);
                }
                ff.data.WriteStream(out.data(), out.size() * sizeof(StaticLevelFx));
                for (Entry& e : entries) {
                    // the fx inline as a by-name asset (",name": the zone's own or a common zone's, resolved at load)
                    void* ref{};
                    std::string fxName{ std::format(",hash_{:x}", ctx.HashXHash(e.fx, true) & 0x7FFFFFFFFFFFFFFFull) };
                    ctx.LinkAsset(XAssetType::ASSET_TYPE_FX, fxName.c_str(), ref, false, &ff);
                    if (!e.mode.empty()) {
                        ff.data.WriteStream(e.mode.c_str(), e.mode.size() + 1);
                    }
                    if (!e.records.empty()) {
                        ff.data.Align(8);
                        std::vector<StaticLevelFxRecord> recs(e.records.size());
                        for (size_t i = 0; i < e.records.size(); i++) {
                            recs[i].name = e.records[i].name.empty() ? nullptr : (const char*)fastfile::linker::memory::POINTER_NEXT;
                            std::memcpy(recs[i].data, e.records[i].data.data(), sizeof(recs[i].data));
                        }
                        ff.data.WriteStream(recs.data(), recs.size() * sizeof(StaticLevelFxRecord));
                        for (Record& r : e.records) {
                            if (!r.name.empty()) {
                                ff.data.WriteStream(r.name.c_str(), r.name.size() + 1);
                            }
                        }
                    }
                }
            }
            if (!flares.empty()) {
                ff.data.Align(8);
                std::vector<uintptr_t> ptrs(flares.size(), (uintptr_t)fastfile::linker::memory::POINTER_NEXT);
                ff.data.WriteStream(ptrs.data(), ptrs.size() * sizeof(uintptr_t));
                for (const std::string& f : flares) {
                    ff.data.WriteStream(f.c_str(), f.size() + 1);
                }
            }
            ff.data.PopStream();
            ff.data.PopStream();

            LOG_INFO("Added asset staticlevelfxlist {} (hash_{:x}), {} effect(s), {} lens flare(s)", assetName, list.name.name,
                     entries.size(), flares.size());
        }
    };

    utils::MapAdder<XAssetLinkerImpl, XAssetType, XAssetLinker> impl{ GetWorkers(), XAssetType::ASSET_TYPE_STATIC_LEVEL_FX_LIST };
} // namespace
