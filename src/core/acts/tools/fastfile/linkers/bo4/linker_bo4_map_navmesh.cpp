#include <includes.hpp>
#include <tools/fastfile/linkers/linker_bo4.hpp>

/*
 * navmesh (retail 0x78, the loader sub_142E24430, the layout in bo4_unlinker_map_navmesh.cpp): the AI's Havok navmesh,
 * named as the map (not maps/...). Its five Havok packfiles are linked as they are from files (the dumper's blob<i>.bin,
 * a game map's navmesh until acts makes its own), aligned on 32, then the points (0xC each). sub_1437C6690 drops on a map
 * without one and loads the packfiles at 0x18, 0x48, 0x28, 0x38. unk70 is the float 14 in the game's. Unlike the other
 * world assets the header is in the current block (VIRTUAL), not TEMP, as navvolume's.
 *
 * {
 *     "name": "enh_test",
 *     "blobs": [ "navmesh/blob0.bin", "navmesh/blob1.bin", "navmesh/blob2.bin", "navmesh/blob3.bin", null ],
 *     "points": "navmesh/points.bin",   // optional
 *     "unk70": 14.0
 * }
 */
namespace {
    using namespace fastfile::linker::bo4;

    struct NavMeshBlob {
        int32_t size;
        byte* data;
    };
    static_assert(sizeof(NavMeshBlob) == 0x10);

    struct NavMeshRetail {
        XHash name;
        NavMeshBlob blobs[5];
        int32_t pointCount;
        byte* points;
        float unk70;
        uint32_t pad74;
    };
    static_assert(sizeof(NavMeshRetail) == 0x78);

    class XAssetLinkerImpl : public XAssetLinker {
      public:
        using XAssetLinker::XAssetLinker;

        void Compute(BO4LinkContext& ctx, const char* id, BO4FFContext& ff) override {
            std::filesystem::path path{ ctx.linkCtx.input / id };
            BO4Json cfg{ path };
            if (!cfg.SyncConfig(false)) {
                LOG_ERROR("Can't read navmesh {}", path.string());
                ctx.error = true;
                return;
            }
            std::string assetName{ cfg.GetString("name", "") };
            if (assetName.empty()) {
                LOG_ERROR("navmesh {}: no name (the map's)", path.string());
                ctx.error = true;
                return;
            }
            auto Read = [&](const rapidjson::Value* v, std::string& out) {
                if (!v || v->IsNull()) {
                    return true;
                }
                if (!v->IsString() || !utils::ReadFile(path.parent_path() / v->GetString(), out)) {
                    LOG_ERROR("navmesh {}: can't read {}", path.string(), v->IsString() ? v->GetString() : "?");
                    return false;
                }
                return true;
            };
            std::string blobs[5]{};
            auto blobsIt{ cfg.main.FindMember("blobs") };
            for (rapidjson::SizeType i = 0; i < 5; i++) {
                const rapidjson::Value* v{ blobsIt != cfg.main.MemberEnd() && blobsIt->value.IsArray() && i < blobsIt->value.Size() ? &blobsIt->value[i] : nullptr };
                if (!Read(v, blobs[i])) {
                    ctx.error = true;
                    return;
                }
            }
            std::string points{};
            auto pointsIt{ cfg.main.FindMember("points") };
            if (!Read(pointsIt != cfg.main.MemberEnd() ? &pointsIt->value : nullptr, points) || points.size() % 0xC) {
                ctx.error = true;
                return;
            }

            // the header in the current block, not TEMP: the navmesh pointer loader (sub_142E24550) pushes no block
            ff.data.PushStream(XFILE_BLOCK_VIRTUAL);
            ff.data.Align(8);
            NavMeshRetail& nav{ ff.data.AllocStreamRef<NavMeshRetail>() };
            nav = {};
            nav.name.name = ctx.HashXHash(assetName, true);
            for (size_t i = 0; i < 5; i++) {
                nav.blobs[i].size = (int32_t)blobs[i].size();
                nav.blobs[i].data = blobs[i].empty() ? nullptr : (byte*)fastfile::linker::memory::POINTER_NEXT;
            }
            nav.pointCount = (int32_t)(points.size() / 0xC);
            nav.points = points.empty() ? nullptr : (byte*)fastfile::linker::memory::POINTER_NEXT;
            nav.unk70 = (float)cfg.GetDouble("unk70", 14.0);

            for (const std::string& blob : blobs) {
                if (!blob.empty()) {
                    ff.data.Align(32);
                    ff.data.WriteStream(blob.data(), blob.size());
                }
            }
            if (!points.empty()) {
                ff.data.Align(4);
                ff.data.WriteStream(points.data(), points.size());
            }
            ff.data.PopStream();

            LOG_INFO("Added asset navmesh {} (hash_{:x}), packfiles {} {} {} {} {}, {} points", assetName, nav.name.name,
                blobs[0].size(), blobs[1].size(), blobs[2].size(), blobs[3].size(), blobs[4].size(), nav.pointCount);
        }
    };

    utils::MapAdder<XAssetLinkerImpl, XAssetType, XAssetLinker> impl{ GetWorkers(), XAssetType::ASSET_TYPE_NAVMESH };
} // namespace
