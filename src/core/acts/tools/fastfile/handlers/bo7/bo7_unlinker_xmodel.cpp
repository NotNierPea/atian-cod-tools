#include <includes.hpp>
#include <tools/fastfile/handlers/handler_game_bo7.hpp>

namespace {
    using namespace fastfile::handlers::bo7;
    struct Scriptable;
    struct CharCollBounds;
    struct XSurface;

    struct XModelSurfs {
        uint64_t name;
        XSurface* surf;
        uint64_t unk10;
        uint64_t unk18;
        uint16_t numsurfs;
        uint32_t unk24;
        byte __pad[0x20];
    };
    static_assert(sizeof(XModelSurfs) == 0x48);

    struct XModelLodInfo {
        XModelSurfs* surf;
        XSurface* surfs;
        byte __pad[0x38];
    };
    static_assert(sizeof(XModelLodInfo) == 0x48);

    struct XModel {
        uint64_t name;
        const char* unk08;
        uint16_t unk10;
        uint8_t lodCount;
        uint8_t unk13;
        uint32_t unk14;
        uint64_t unk18;
        uint64_t unk20;
        uint64_t unk28;
        uint64_t unk30;
        uint64_t unk38;
        uint64_t unk40;
        uint64_t unk48;
        uint64_t unk50;
        uint64_t unk58;
        uint64_t unk60;
        uint64_t unk68;
        Scriptable* scriptable;
        CharCollBounds* charCollBounds;
        uint64_t unk80;
        uint64_t unk88;
        XModelLodInfo* lodsData;
        uint64_t unk98;
        uint64_t unka0;
        uint64_t unka8;
        uint64_t unkb0;
        uint64_t unkb8;
        MaterialAnimation* materialAnimation;
        uint64_t unkc8;
        uint64_t unkd0;
        uint64_t unkd8;
    };
    static_assert(sizeof(XModel) == 0xe0);

    /*
    class ImplWorker : public Worker {
        using Worker::Worker;
        void PostAllXFileLoading(fastfile::FastFileOption& opt) override {
            // DB_ModelFixup_FixXModelSurf
            // we do that here because we don't control if the xmodel surfaces will be loaded before or after the xmodel
            // data
            size_t xmld{};
            fastfile::ForEachAsset(
                fastfile::handlers::bo7::GetAssetNames().GetExePoolId(SatHashAssetType::SATH_ASSET_XMODEL),
                [&xmld](compatibility::scobalula::csi::XAsset64Mem& mem) {
                    XModel& model{ *(XModel*)mem.header };
                    xmld++;
                    if (model.lodsData) {
                        for (size_t i = 0; i < model.lodCount; i++) {
                            XModelLodInfo& lodInfo{ model.lodsData[i] };

                            if (!lodInfo.surf) {
                                continue; // probably default xmodel
                            }
                            if (!lodInfo.surf->surf) {
                                if (lodInfo.surf->numsurfs) {
                                    LOG_WARNING(
                                        "missing surface for {}[{}]/{} {}",
                                        hashutils::ExtractTmp("hash", mem.id),
                                        hashutils::ExtractTmp("hash", model.name),
                                        hashutils::ExtractTmp("hash", lodInfo.surf->name),
                                        (int)lodInfo.surf->numsurfs
                                    );
                                }
                                continue; // not available
                            }
                            LOG_INFO(
                                "surface {}[{}]/{} {}",
                                hashutils::ExtractTmp("hash", mem.id),
                                hashutils::ExtractTmp("hash", model.name),
                                hashutils::ExtractTmp("hash", lodInfo.surf->name),
                                (int)lodInfo.surf->numsurfs
                            );
                            lodInfo.surfs = lodInfo.surf->surf;
                        }
                    }
                }
            );
            LOG_INFO("patch {} xmodels", xmld);
        }
    };

    utils::MapAdder<ImplWorker, SatHashAssetType, Worker> impl{ GetWorkers(),
                                                                SatHashAssetType::SATH_ASSET_XMODEL,
                                                                sizeof(XModel) };
        */
} // namespace