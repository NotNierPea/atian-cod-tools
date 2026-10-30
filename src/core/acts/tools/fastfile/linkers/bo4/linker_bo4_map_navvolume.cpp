#include <includes.hpp>
#include <tools/fastfile/linkers/linker_bo4.hpp>

/*
 * navvolume (retail 0x50, the loader sub_142E245C0, the layout in bo4_unlinker_map_navvolume.cpp): the map's Havok nav
 * volumes, named as the map (not maps/...), its four packfiles linked as they are from files (the dumper's blob<i>.bin),
 * aligned on 32. Like the navmesh's, its pointer loader (sub_142E24690) pushes no block: the header is in the current
 * block (VIRTUAL).
 *
 * {
 *     "name": "zm_park",
 *     "blobs": [ "navvolume/blob0.bin", "navvolume/blob1.bin", "navvolume/blob2.bin", "navvolume/blob3.bin" ]
 * }
 */
namespace {
    using namespace fastfile::linker::bo4;

    struct NavVolumeBlob {
        int32_t size;
        byte* data;
    };
    static_assert(sizeof(NavVolumeBlob) == 0x10);

    struct NavVolumeRetail {
        XHash name;
        NavVolumeBlob blobs[4];
    };
    static_assert(sizeof(NavVolumeRetail) == 0x50);

    class XAssetLinkerImpl : public XAssetLinker {
      public:
        using XAssetLinker::XAssetLinker;

        void Compute(BO4LinkContext& ctx, const char* id, BO4FFContext& ff) override {
            std::filesystem::path path{ ctx.linkCtx.input / id };
            BO4Json cfg{ path };
            if (!cfg.SyncConfig(false)) {
                LOG_ERROR("Can't read navvolume {}", path.string());
                ctx.error = true;
                return;
            }
            std::string assetName{ cfg.GetString("name", "") };
            if (assetName.empty()) {
                LOG_ERROR("navvolume {}: no name (the map's)", path.string());
                ctx.error = true;
                return;
            }
            std::string blobs[4]{};
            auto blobsIt{ cfg.main.FindMember("blobs") };
            for (rapidjson::SizeType i = 0; i < 4; i++) {
                const rapidjson::Value* v{ blobsIt != cfg.main.MemberEnd() && blobsIt->value.IsArray() && i < blobsIt->value.Size() ? &blobsIt->value[i] : nullptr };
                if (!v || v->IsNull()) {
                    continue;
                }
                if (!v->IsString() || !utils::ReadFile(path.parent_path() / v->GetString(), blobs[i])) {
                    LOG_ERROR("navvolume {}: can't read {}", path.string(), v->IsString() ? v->GetString() : "?");
                    ctx.error = true;
                    return;
                }
            }

            ff.data.PushStream(XFILE_BLOCK_VIRTUAL);
            ff.data.Align(8);
            NavVolumeRetail& vol{ ff.data.AllocStreamRef<NavVolumeRetail>() };
            vol = {};
            vol.name.name = ctx.HashXHash(assetName, true);
            for (size_t i = 0; i < 4; i++) {
                vol.blobs[i].size = (int32_t)blobs[i].size();
                vol.blobs[i].data = blobs[i].empty() ? nullptr : (byte*)fastfile::linker::memory::POINTER_NEXT;
            }
            for (const std::string& blob : blobs) {
                if (!blob.empty()) {
                    ff.data.Align(32);
                    ff.data.WriteStream(blob.data(), blob.size());
                }
            }
            ff.data.PopStream();

            LOG_INFO("Added asset navvolume {} (hash_{:x}), packfiles {} {} {} {}", assetName, vol.name.name,
                blobs[0].size(), blobs[1].size(), blobs[2].size(), blobs[3].size());
        }
    };

    utils::MapAdder<XAssetLinkerImpl, XAssetType, XAssetLinker> impl{ GetWorkers(), XAssetType::ASSET_TYPE_NAVVOLUME };
} // namespace
