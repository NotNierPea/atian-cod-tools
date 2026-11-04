#include <includes.hpp>
#include <tools/fastfile/handlers/handler_game_bo4.hpp>

/*
 * Gestures (GestureDef, asset type 18) and gesture tables (type 19): the first person arm and weapon motions BO4 plays
 * over the view model. A weapon names its gesture tables (WeaponDef 0x978, 8 of them): an offhand gadget's hold, fire
 * and cancel are gestures (the grapple gun's view anim list is empty, all its motion is gestures). The retail layouts,
 * read off their loaders in the retail dump:
 *
 * GestureDef, 0x208 bytes (sub_142E1BC90): 0x000 name (XHash); 0x020..0x150 38 xanims (three runs: 20, 9, 9, by name
 * or inline); the rest plain values.
 * GestureTable, 0x68 bytes (sub_142E1C270): 0x000 name (XHash); 0x010..0x060 10 gestures; 0x060 8 plain bytes.
 *
 * The json is the linkers' (linker_bo4_gesture.cpp): the plain bytes as hex (the pointers too, the linker sets them)
 * and the references by name by their offsets.
 */
namespace {
    using namespace fastfile::handlers::bo4;

    constexpr size_t GESTURE_SIZE{ 0x208 };
    constexpr size_t GESTURE_ANIMS{ 0x20 };
    constexpr size_t GESTURE_ANIMS_COUNT{ 38 };
    constexpr size_t TABLE_SIZE{ 0x68 };
    constexpr size_t TABLE_GESTURES{ 0x10 };
    constexpr size_t TABLE_GESTURES_COUNT{ 10 };

    std::string Hex(const void* ptr, size_t len) {
        std::string out{};
        for (size_t i = 0; i < len; i++) {
            out += std::format("{:02x}", ((const byte*)ptr)[i]);
        }
        return out;
    }

    // one asset as {name, raw, <refsName>: {offset: name}}
    void Dump(fastfile::FastFileOption& opt, void* ptr, const char* folder, size_t size, const char* refsName,
              games::bo4::pool::XAssetType refType, size_t refs, size_t refsCount) {
        byte* asset{ (byte*)ptr };
        XHash* name{ (XHash*)asset };
        std::filesystem::path outFile{ opt.m_output / "bo4" / "source" / "tables" / folder /
                                       fastfile::GetCurrentContext().ffname /
                                       std::format("{}.json", hashutils::ExtractTmp("file", name->name)) };
        std::filesystem::create_directories(outFile.parent_path());
        BO4JsonWriter json{};

        json.BeginObject();
        json.WriteFieldValueXHash("name", *name);
        json.WriteFieldValueString("raw", Hex(asset, size));
        json.WriteFieldNameString(refsName);
        json.BeginObject();
        size_t numRefs{};
        for (size_t i = 0; i < refsCount; i++) {
            size_t off{ refs + i * 8 };
            void* r{ *(void**)(asset + off) };
            if (r) {
                json.WriteFieldValueXAsset(std::format("{:x}", off).c_str(), refType, r);
                numRefs++;
            }
        }
        json.EndObject();
        json.EndObject();

        LOG_OPT_INFO("Dump {} {} ({} {})", folder, outFile.string(), numRefs, refsName);
        if (!json.WriteToFile(outFile)) {
            LOG_ERROR("Error when dumping {}", outFile.string());
        }
    }

    class GestureWorker : public Worker {
        using Worker::Worker;

        void Unlink(fastfile::FastFileOption& opt, void* ptr) {
            Dump(opt, ptr, "gesture", GESTURE_SIZE, "xanims", games::bo4::pool::ASSET_TYPE_XANIM, GESTURE_ANIMS, GESTURE_ANIMS_COUNT);
        }
    };

    class GestureTableWorker : public Worker {
        using Worker::Worker;

        void Unlink(fastfile::FastFileOption& opt, void* ptr) {
            Dump(opt, ptr, "gesturetable", TABLE_SIZE, "gestures", games::bo4::pool::ASSET_TYPE_GESTURE, TABLE_GESTURES, TABLE_GESTURES_COUNT);
        }
    };

    utils::MapAdder<GestureWorker, games::bo4::pool::XAssetType, Worker> implGesture{
        GetWorkers(), games::bo4::pool::XAssetType::ASSET_TYPE_GESTURE
    };
    utils::MapAdder<GestureTableWorker, games::bo4::pool::XAssetType, Worker> implTable{
        GetWorkers(), games::bo4::pool::XAssetType::ASSET_TYPE_GESTURE_TABLE
    };
} // namespace
