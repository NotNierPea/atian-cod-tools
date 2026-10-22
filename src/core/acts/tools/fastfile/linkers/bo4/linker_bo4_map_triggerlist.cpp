#include <includes.hpp>
#include <tools/fastfile/linkers/linker_bo4.hpp>

/*
 * triggerlist (retail 0x50, the loader sub_142E303B0, the layout in bo4_unlinker_map_triggerlist.cpp): a map's trigger
 * volumes (info_volume zones, trigger_* entities) and their entities, named as its col_map. A model has hulls (bounds
 * relative to the entity's origin: mid point, half size; contents) and a hull slabs (planes for rotated volumes, none
 * here). The entities are key values as the entitylist's (linker_bo4_map_entitylist.cpp), their model index the volume.
 *
 * {
 *     "name": "maps/zm/enh_test.d3dbsp",
 *     "entities": [
 *         { "classname": "info_volume", "origin": [ 0, 0, 0 ], "targetname": "zone_room", "target": "zone_room_spawns",
 *           "box": { "mins": [ -320, -320, 0 ], "maxs": [ 320, 320, 160 ] }, "contents": 536870913 }
 *     ]
 * }
 * "box" (relative to the origin, axial) and "contents" (0x20000001 as the game's volumes) aren't key values.
 */
namespace {
    using namespace fastfile::linker::bo4;

    enum SpawnVarKeyType : uint16_t {
        SVKT_STRING = 2,
        SVKT_VECTOR = 3,
    };

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

    struct TriggerModel {
        uint32_t contents;
        uint16_t hullCount;
        uint16_t firstHull;
    };
    static_assert(sizeof(TriggerModel) == 8);

    struct TriggerHull {
        float midPoint[3];
        float halfSize[3];
        uint32_t contents;
        uint16_t slabCount;
        uint16_t firstSlab;
    };
    static_assert(sizeof(TriggerHull) == 0x20);

    struct TriggerList {
        XHash name;
        uint32_t modelsCount;
        TriggerModel* models;
        uint32_t hullCount;
        TriggerHull* hulls;
        uint32_t slabCount;
        void* slabs;
        uint64_t spawnVarsCount;
        LinkerSpawnVar* spawnVars;
    };
    static_assert(sizeof(TriggerList) == 0x50);

    constexpr uint32_t VOLUME_CONTENTS = 0x20000001;

    struct KeyValue {
        std::string key{};
        std::string string{};
        float vector[3]{};
        bool isVector{};
    };

    struct Entity {
        std::vector<KeyValue> kvs{};
        int32_t model{ -1 };
    };

    class XAssetLinkerImpl : public XAssetLinker {
      public:
        using XAssetLinker::XAssetLinker;

        void Compute(BO4LinkContext& ctx, const char* id, BO4FFContext& ff) override {
            std::filesystem::path path{ ctx.linkCtx.input / id };
            BO4Json cfg{ path };
            if (!cfg.SyncConfig(false)) {
                LOG_ERROR("Can't read triggerlist {}", path.string());
                ctx.error = true;
                return;
            }
            std::string assetName{ cfg.GetString("name", "") };
            if (assetName.empty()) {
                LOG_ERROR("triggerlist {}: no name (maps/<mode>/<map>.d3dbsp)", path.string());
                ctx.error = true;
                return;
            }

            std::vector<Entity> entities{};
            std::vector<TriggerHull> hulls{};
            auto entitiesIt{ cfg.main.FindMember("entities") };
            if (entitiesIt != cfg.main.MemberEnd() && entitiesIt->value.IsArray()) {
                for (const rapidjson::Value& e : entitiesIt->value.GetArray()) {
                    if (!e.IsObject()) {
                        continue;
                    }
                    Entity& ent{ entities.emplace_back() };
                    uint32_t contents{ VOLUME_CONTENTS };
                    auto ct{ e.FindMember("contents") };
                    if (ct != e.MemberEnd() && ct->value.IsNumber()) {
                        contents = ct->value.GetUint();
                    }
                    for (auto m{ e.MemberBegin() }; m != e.MemberEnd(); ++m) {
                        std::string key{ m->name.GetString() };
                        const rapidjson::Value& v{ m->value };
                        if (key == "contents") {
                            continue;
                        }
                        if (key == "box") {
                            // one axial hull: the mid point and half size of the box, relative to the origin
                            float mins[3]{}, maxs[3]{};
                            for (const char* k : { "mins", "maxs" }) {
                                auto it{ v.FindMember(k) };
                                float* out{ k[1] == 'i' ? mins : maxs };
                                for (rapidjson::SizeType i = 0; i < 3; i++) {
                                    out[i] = it != v.MemberEnd() && it->value.IsArray() && i < it->value.Size() && it->value[i].IsNumber() ? it->value[i].GetFloat() : 0;
                                }
                            }
                            TriggerHull& h{ hulls.emplace_back() };
                            h = {};
                            for (size_t i = 0; i < 3; i++) {
                                h.midPoint[i] = (mins[i] + maxs[i]) / 2;
                                h.halfSize[i] = (maxs[i] - mins[i]) / 2;
                            }
                            h.contents = contents;
                            ent.model = (int32_t)hulls.size() - 1;
                            continue;
                        }
                        KeyValue& kv{ ent.kvs.emplace_back() };
                        kv.key = key;
                        if (v.IsArray() && v.Size() == 3 && v[0].IsNumber() && v[1].IsNumber() && v[2].IsNumber()) {
                            kv.isVector = true;
                            for (rapidjson::SizeType i = 0; i < 3; i++) {
                                kv.vector[i] = (float)v[i].GetDouble();
                            }
                        } else if (v.IsString()) {
                            kv.string = v.GetString();
                        } else if (v.IsNumber()) {
                            kv.string = v.IsInt64() ? std::to_string(v.GetInt64()) : std::format("{}", v.GetDouble());
                        } else {
                            LOG_ERROR("triggerlist {}: {} isn't a string, number or vector", path.string(), kv.key);
                            ctx.error = true;
                            return;
                        }
                    }
                    std::sort(ent.kvs.begin(), ent.kvs.end(), [](const KeyValue& a, const KeyValue& b) { return a.key < b.key; });
                }
            }

            ff.data.PushStream(XFILE_BLOCK_TEMP);
            ff.data.Align(8);
            TriggerList& list{ ff.data.AllocStreamRef<TriggerList>() };
            list = {};
            list.name.name = ctx.HashXHash(assetName, true);
            // a model a hull
            list.modelsCount = (uint32_t)hulls.size();
            list.models = hulls.empty() ? nullptr : (TriggerModel*)fastfile::linker::memory::POINTER_NEXT;
            list.hullCount = (uint32_t)hulls.size();
            list.hulls = hulls.empty() ? nullptr : (TriggerHull*)fastfile::linker::memory::POINTER_NEXT;
            list.spawnVarsCount = entities.size();
            list.spawnVars = entities.empty() ? nullptr : (LinkerSpawnVar*)fastfile::linker::memory::POINTER_NEXT;

            // sub_142E303B0's order: models, hulls, slabs, the entities
            ff.data.PushStream(XFILE_BLOCK_VIRTUAL);
            if (!hulls.empty()) {
                ff.data.Align(4);
                TriggerModel* models{ ff.data.AllocStreamPtr<TriggerModel>(hulls.size()) };
                for (size_t i = 0; i < hulls.size(); i++) {
                    models[i].contents = hulls[i].contents;
                    models[i].hullCount = 1;
                    models[i].firstHull = (uint16_t)i;
                }
                ff.data.Align(4);
                ff.data.WriteStream(hulls.data(), hulls.size() * sizeof(TriggerHull));
            }
            if (!entities.empty()) {
                ff.data.Align(8);
                LinkerSpawnVar* vars{ ff.data.AllocStreamPtr<LinkerSpawnVar>(entities.size()) };
                for (size_t i = 0; i < entities.size(); i++) {
                    LinkerSpawnVar& var{ vars[i] };
                    var = {};
                    var.numSpawnVars = (uint32_t)entities[i].kvs.size();
                    var.spawnVars = entities[i].kvs.empty() ? nullptr : (SpawnVarKeyValue*)fastfile::linker::memory::POINTER_NEXT;
                    var.modelIndex = entities[i].model;
                    for (const KeyValue& kv : entities[i].kvs) {
                        if (kv.isVector && kv.key == "origin") {
                            std::memcpy(var.origin, kv.vector, sizeof(var.origin));
                        } else if (kv.isVector && kv.key == "angles") {
                            std::memcpy(var.angles, kv.vector, sizeof(var.angles));
                        }
                    }
                }
                for (const Entity& ent : entities) {
                    if (ent.kvs.empty()) {
                        continue;
                    }
                    ff.data.Align(8);
                    SpawnVarKeyValue* out{ ff.data.AllocStreamPtr<SpawnVarKeyValue>(ent.kvs.size()) };
                    for (size_t j = 0; j < ent.kvs.size(); j++) {
                        out[j] = {};
                        out[j].key = (const char*)fastfile::linker::memory::POINTER_NEXT;
                        out[j].unk1a = true;
                        if (ent.kvs[j].isVector) {
                            out[j].type = SVKT_VECTOR;
                            std::memcpy(out[j].value.vector, ent.kvs[j].vector, sizeof(ent.kvs[j].vector));
                        } else {
                            out[j].type = SVKT_STRING;
                            out[j].value.string = (const char*)fastfile::linker::memory::POINTER_NEXT;
                        }
                    }
                    for (const KeyValue& kv : ent.kvs) {
                        ff.data.WriteStream(kv.key.c_str(), kv.key.size() + 1);
                        if (!kv.isVector) {
                            ff.data.WriteStream(kv.string.c_str(), kv.string.size() + 1);
                        }
                    }
                }
            }
            ff.data.PopStream();
            ff.data.PopStream();

            LOG_INFO("Added asset triggerlist {} (hash_{:x}), {} entities, {} volume(s)", assetName, list.name.name, entities.size(), hulls.size());
        }
    };

    utils::MapAdder<XAssetLinkerImpl, XAssetType, XAssetLinker> impl{ GetWorkers(), XAssetType::ASSET_TYPE_TRIGGER_LIST };
} // namespace
