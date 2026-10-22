#include <includes.hpp>
#include <tools/fastfile/linkers/linker_bo4.hpp>

/*
 * entitylist (retail 0x20, the loader sub_142E18FD0 / sub_142E219B0): a map's entities as key values, named as its
 * col_map (GScr_LoadScriptsForEntities and the struct spawn look it up by CM_Name). A map without one drops in
 * DB_CreateDefaultEntry (error 3733923913, "Uniform 99 Divebomb Karma"). Entries 0x30: count, key values, model
 * index, a u32 (0 on the game's worldspawn), origin at 0x18, angles at 0x24 (the elf's has no u32, its origin at 0x14).
 * Key values 0x20: key string, value (a string for type 2, a vector for 3), u16 type at 0x18, a byte at 0x1A (set on the
 * game's). The strings follow the key values inline, the key then the string value. The keys sorted as the game's.
 *
 * {
 *     "name": "maps/zm/enh_test.d3dbsp",
 *     "entities": [
 *         { "classname": "worldspawn", "gravity": "800", "skyboxmodel": "skybox_default_day" },
 *         { "classname": "script_struct", "origin": [ 0, 0, 64 ], "angles": [ 0, 90, 0 ], "targetname": "initial_spawn_points" }
 *     ]
 * }
 * A value of 3 numbers is a vector, the value of a key in HASH_KEYS (model, prop_model, scriptbundlename, script_sound,
 * classnamepayloadhash: the game's are hashes) a hash of its string ("hash_x" for a hash), any other value a string.
 */
namespace {
    using namespace fastfile::linker::bo4;

    enum SpawnVarKeyType : uint16_t {
        SVKT_STRING = 2,
        SVKT_VECTOR = 3,
        SVKT_XHASH = 4,
    };

    // the keys the game's entities have as hashes (zm_towers' entitylist)
    const char* HASH_KEYS[]{ "model", "prop_model", "scriptbundlename", "script_sound", "classnamepayloadhash" };

    struct SpawnVarKeyValue {
        const char* key;
        union {
            const char* string;
            float vector[3];
            byte pad[0x10];
        } value;
        uint16_t type;
        bool unk1a;
        byte pad1b[5];
    };
    static_assert(sizeof(SpawnVarKeyValue) == 0x20);

    struct LinkerSpawnVar {
        uint32_t numSpawnVars;
        SpawnVarKeyValue* spawnVars;
        int32_t modelIndex;
        uint32_t unk14;
        float origin[3];
        float angles[3];
    };
    static_assert(sizeof(LinkerSpawnVar) == 0x30);

    struct EntityList {
        XHash name;
        uint64_t count;
        LinkerSpawnVar* spawnVars;
    };
    static_assert(sizeof(EntityList) == 0x20);

    struct KeyValue {
        std::string key{};
        std::string string{};
        float vector[3]{};
        bool isVector{};
        uint64_t hash{};
        bool isHash{};
    };

    class XAssetLinkerImpl : public XAssetLinker {
      public:
        using XAssetLinker::XAssetLinker;

        void Compute(BO4LinkContext& ctx, const char* id, BO4FFContext& ff) override {
            std::filesystem::path path{ ctx.linkCtx.input / id };
            BO4Json cfg{ path };
            if (!cfg.SyncConfig(false)) {
                LOG_ERROR("Can't read entitylist {}", path.string());
                ctx.error = true;
                return;
            }
            std::string assetName{ cfg.GetString("name", "") };
            auto entitiesIt{ cfg.main.FindMember("entities") };
            if (assetName.empty() || entitiesIt == cfg.main.MemberEnd() || !entitiesIt->value.IsArray()) {
                LOG_ERROR("entitylist {}: needs a name (maps/<mode>/<map>.d3dbsp) and entities", path.string());
                ctx.error = true;
                return;
            }

            // the key values of each entity, sorted by key
            std::vector<std::vector<KeyValue>> entities{};
            for (const rapidjson::Value& e : entitiesIt->value.GetArray()) {
                if (!e.IsObject()) {
                    LOG_ERROR("entitylist {}: an entity isn't an object", path.string());
                    ctx.error = true;
                    return;
                }
                std::vector<KeyValue>& kvs{ entities.emplace_back() };
                for (auto& m : e.GetObj()) {
                    KeyValue& kv{ kvs.emplace_back() };
                    kv.key = m.name.GetString();
                    const rapidjson::Value& v{ m.value };
                    if (v.IsArray() && v.Size() == 3 && v[0].IsNumber() && v[1].IsNumber() && v[2].IsNumber()) {
                        kv.isVector = true;
                        for (rapidjson::SizeType i = 0; i < 3; i++) {
                            kv.vector[i] = (float)v[i].GetDouble();
                        }
                    } else if (v.IsString()) {
                        kv.string = v.GetString();
                        for (const char* k : HASH_KEYS) {
                            if (kv.key == k) {
                                kv.isHash = true;
                                kv.hash = ctx.HashXHash(kv.string.c_str());
                            }
                        }
                    } else if (v.IsNumber()) {
                        kv.string = v.IsInt64() ? std::to_string(v.GetInt64()) : std::format("{}", v.GetDouble());
                    } else {
                        LOG_ERROR("entitylist {}: {} isn't a string, number or vector", path.string(), kv.key);
                        ctx.error = true;
                        return;
                    }
                }
                std::sort(kvs.begin(), kvs.end(), [](const KeyValue& a, const KeyValue& b) { return a.key < b.key; });
            }

            ff.data.PushStream(XFILE_BLOCK_TEMP);
            ff.data.Align(8);
            EntityList& list{ ff.data.AllocStreamRef<EntityList>() };
            list = {};
            list.name.name = ctx.HashXHash(assetName, true);
            list.count = entities.size();
            list.spawnVars = entities.empty() ? nullptr : (LinkerSpawnVar*)fastfile::linker::memory::POINTER_NEXT;

            ff.data.PushStream(XFILE_BLOCK_VIRTUAL);
            if (!entities.empty()) {
                ff.data.Align(8);
                LinkerSpawnVar* vars{ ff.data.AllocStreamPtr<LinkerSpawnVar>(entities.size()) };
                for (size_t i = 0; i < entities.size(); i++) {
                    LinkerSpawnVar& var{ vars[i] };
                    var = {};
                    var.numSpawnVars = (uint32_t)entities[i].size();
                    var.spawnVars = entities[i].empty() ? nullptr : (SpawnVarKeyValue*)fastfile::linker::memory::POINTER_NEXT;
                    var.modelIndex = -1;
                    for (const KeyValue& kv : entities[i]) {
                        if (kv.isVector && kv.key == "origin") {
                            std::memcpy(var.origin, kv.vector, sizeof(var.origin));
                        } else if (kv.isVector && kv.key == "angles") {
                            std::memcpy(var.angles, kv.vector, sizeof(var.angles));
                        }
                    }
                }
                // per entity its key values, then the strings of each (the key, the string value)
                for (const std::vector<KeyValue>& kvs : entities) {
                    if (kvs.empty()) {
                        continue;
                    }
                    ff.data.Align(8);
                    SpawnVarKeyValue* out{ ff.data.AllocStreamPtr<SpawnVarKeyValue>(kvs.size()) };
                    for (size_t j = 0; j < kvs.size(); j++) {
                        out[j] = {};
                        out[j].key = (const char*)fastfile::linker::memory::POINTER_NEXT;
                        out[j].unk1a = true;
                        if (kvs[j].isVector) {
                            out[j].type = SVKT_VECTOR;
                            std::memcpy(out[j].value.vector, kvs[j].vector, sizeof(kvs[j].vector));
                        } else if (kvs[j].isHash) {
                            // an XHash { name, string }
                            out[j].type = SVKT_XHASH;
                            std::memcpy(out[j].value.pad, &kvs[j].hash, sizeof(kvs[j].hash));
                        } else {
                            out[j].type = SVKT_STRING;
                            out[j].value.string = (const char*)fastfile::linker::memory::POINTER_NEXT;
                        }
                    }
                    for (const KeyValue& kv : kvs) {
                        ff.data.WriteStream(kv.key.c_str(), kv.key.size() + 1);
                        if (!kv.isVector && !kv.isHash) {
                            ff.data.WriteStream(kv.string.c_str(), kv.string.size() + 1);
                        }
                    }
                }
            }
            ff.data.PopStream();
            ff.data.PopStream();

            LOG_INFO("Added asset entitylist {} (hash_{:x}), {} entities", assetName, list.name.name, entities.size());
        }
    };

    utils::MapAdder<XAssetLinkerImpl, XAssetType, XAssetLinker> impl{ GetWorkers(), XAssetType::ASSET_TYPE_ENTITY_LIST };
} // namespace
