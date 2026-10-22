#include <includes.hpp>
#include <tools/fastfile/handlers/handler_game_bo4.hpp>
#include <tools/fastfile/bo4_ai_tables.hpp>

// the AI tables (bo4_ai_tables.hpp), dumped as the linker reads them back
namespace {
    using namespace fastfile::handlers::bo4;
    using namespace fastfile::bo4ai;

    void DumpStruct(BO4JsonWriter& json, const Struct& s, const byte* p);

    void DumpValue(BO4JsonWriter& json, const Field& f, const byte* p) {
        const byte* v{ p + f.offset };
        switch (f.kind) {
        case FK_XHASH: {
            uint64_t h{ *(uint64_t*)v };
            if (h) {
                // the name only when the linker hashes it back to the same value (the dictionary has other hash types)
                const char* str{ hashutils::ExtractPtr(h) };
                if (str && hash::Hash64(str) == h) {
                    json.WriteValueString(std::format("#{}", str));
                } else {
                    json.WriteValueString(std::format("#hash_{:x}", h));
                }
            } else {
                json.WriteValueNull();
            }
            break;
        }
        case FK_SCR: {
            ScrString_t s{};
            std::memcpy(&s, v, sizeof(s));
            if (*(uint32_t*)v) {
                json.WriteValueScrString(s);
            } else {
                json.WriteValueNull();
            }
            break;
        }
        case FK_I32: json.WriteValueNumber(*(int32_t*)v); break;
        case FK_U32: json.WriteValueNumber(*(uint32_t*)v); break;
        case FK_F32: json.WriteValueNumber(*(float*)v); break;
        case FK_U8: json.WriteValueNumber((uint32_t)*v); break;
        case FK_BOOL: json.WriteValueBool(*v != 0); break;
        case FK_ARRAY: {
            const byte* arr{ *(const byte**)v };
            int32_t count{ *(int32_t*)(p + f.countOffset) };
            json.BeginArray();
            if (arr) {
                for (int32_t i = 0; i < count; i++) {
                    DumpStruct(json, *f.sub, arr + (size_t)i * f.sub->size);
                }
            }
            json.EndArray();
            break;
        }
        case FK_ASSET: {
            if (f.fixedCount > 1) {
                json.BeginArray();
            }
            for (uint32_t i = 0; i < f.fixedCount; i++) {
                const byte* a{ ((const byte**)v)[i] };
                if (a) {
                    DumpStruct(json, *f.sub, a);
                } else {
                    json.WriteValueNull();
                }
            }
            if (f.fixedCount > 1) {
                json.EndArray();
            }
            break;
        }
        }
    }

    void DumpStruct(BO4JsonWriter& json, const Struct& s, const byte* p) {
        if (s.flat) {
            DumpValue(json, s.fields[0], p);
            return;
        }
        json.BeginObject();
        std::vector<bool> covered(s.size);
        for (const Field& f : s.fields) {
            json.WriteFieldNameString(f.name);
            DumpValue(json, f, p);
            for (uint32_t i = 0; i < FieldSize(f); i++) {
                covered[f.offset + i] = true;
            }
            if (f.kind == FK_ARRAY) {
                for (uint32_t i = 0; i < 4; i++) {
                    covered[f.countOffset + i] = true;
                }
            }
        }
        // the bytes no field covers, by 4 bytes, when they aren't all 0
        for (uint32_t o = 0; o < s.size; o += 4) {
            if (covered[o]) {
                continue;
            }
            uint32_t e{ o };
            while (e < s.size && !covered[e]) {
                e += 4;
            }
            bool zero{ true };
            for (uint32_t i = o; i < e; i++) {
                zero &= p[i] == 0;
            }
            if (!zero) {
                json.WriteFieldNameString(utils::va("unk_%x", o));
                json.BeginArray();
                for (uint32_t i = o; i < e; i += 4) {
                    json.WriteValueNumber(*(uint32_t*)(p + i));
                }
                json.EndArray();
            }
            o = e - 4;
        }
        json.EndObject();
    }

    class ImplWorker : public Worker {
        XAssetType type;

      public:
        ImplWorker(XAssetType type) : Worker(), type(type) {}

        void Unlink(fastfile::FastFileOption& opt, void* ptr) override {
            // looked up here, the schemas are another file's globals
            const AssetSchema* schema{ FindSchema(type) };
            uint64_t name{ *(uint64_t*)ptr };
            const char* n{ hashutils::ExtractPtr(name) };
            if (!n) {
                n = utils::va("hash_%llx", name);
            }
            std::filesystem::path outFile{ opt.m_output / "bo4" / "source" / "tables" / schema->dir / std::format("{}.json", n) };
            std::filesystem::create_directories(outFile.parent_path());
            LOG_OPT_INFO("Dump {} {}", schema->dir, outFile.string());

            BO4JsonWriter json{};
            DumpStruct(json, *schema->header, (const byte*)ptr);
            if (!json.WriteToFile(outFile)) {
                LOG_ERROR("Error when dumping {}", outFile.string());
            }
        }
    };

    using games::bo4::pool::XAssetType;
    utils::MapAdder<ImplWorker, XAssetType, Worker> amt{ GetWorkers(), XAssetType::ASSET_TYPE_ANIMMAPPINGTABLE, XAssetType::ASSET_TYPE_ANIMMAPPINGTABLE };
    utils::MapAdder<ImplWorker, XAssetType, Worker> ast{ GetWorkers(), XAssetType::ASSET_TYPE_ANIMSELECTORTABLESET, XAssetType::ASSET_TYPE_ANIMSELECTORTABLESET };
    utils::MapAdder<ImplWorker, XAssetType, Worker> asm_{ GetWorkers(), XAssetType::ASSET_TYPE_ANIMSTATEMACHINE, XAssetType::ASSET_TYPE_ANIMSTATEMACHINE };
    utils::MapAdder<ImplWorker, XAssetType, Worker> bt{ GetWorkers(), XAssetType::ASSET_TYPE_BEHAVIORTREE, XAssetType::ASSET_TYPE_BEHAVIORTREE };
    utils::MapAdder<ImplWorker, XAssetType, Worker> bsm{ GetWorkers(), XAssetType::ASSET_TYPE_BEHAVIORSTATEMACHINE, XAssetType::ASSET_TYPE_BEHAVIORSTATEMACHINE };
    utils::MapAdder<ImplWorker, XAssetType, Worker> fst{ GetWorkers(), XAssetType::ASSET_TYPE_FOOTSTEP_TABLE, XAssetType::ASSET_TYPE_FOOTSTEP_TABLE };
    utils::MapAdder<ImplWorker, XAssetType, Worker> ssd{ GetWorkers(), XAssetType::ASSET_TYPE_SURFACESOUNDDEF, XAssetType::ASSET_TYPE_SURFACESOUNDDEF };
    utils::MapAdder<ImplWorker, XAssetType, Worker> sws{ GetWorkers(), XAssetType::ASSET_TYPE_SHAREDWEAPONSOUNDS, XAssetType::ASSET_TYPE_SHAREDWEAPONSOUNDS };
    utils::MapAdder<ImplWorker, XAssetType, Worker> esi{ GetWorkers(), XAssetType::ASSET_TYPE_ENTITYSOUNDIMPACTS, XAssetType::ASSET_TYPE_ENTITYSOUNDIMPACTS };
} // namespace
