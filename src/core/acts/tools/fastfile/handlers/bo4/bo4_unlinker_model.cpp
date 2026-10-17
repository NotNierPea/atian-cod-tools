#include <includes.hpp>
#include <tools/fastfile/handlers/handler_game_bo4.hpp>
#include <utils/data_utils.hpp>

/*
 * xmodel and xmodelmesh templates for the bo4 linkers (acts ff ... -a xmodel,xmodelmesh): the blocks as they are,
 * the assets by name, the script strings as strings.
 */
namespace {
    using namespace fastfile::handlers::bo4;

    std::string Hex(const void* data, size_t len) {
        static const char* digits{ "0123456789abcdef" };
        std::string out{};
        out.reserve(len * 2);
        for (size_t i = 0; i < len; i++) {
            byte b{ ((const byte*)data)[i] };
            out += digits[b >> 4];
            out += digits[b & 0xF];
        }
        return out;
    }

    struct XModelMeshMaterials {
        uint16_t numMaterials;
        void** materials;
        float* himipInvSqRadii;
    };
    static_assert(sizeof(XModelMeshMaterials) == 0x18);

    struct XModelBoneNameIndex {
        ScrString_t name;
        uint16_t index;
        uint16_t pad;
    };
    static_assert(sizeof(XModelBoneNameIndex) == 8);

    struct __declspec(align(8)) XModel {
        XHash name;
        ScrString_t* boneNames;
        XModelBoneNameIndex* boneNameIndices;
        byte* parentList;
        uint64_t* quats;
        byte* trans;
        byte* partClassification;
        byte* baseMat;
        byte* boneInfo;
        void* collSurfs;
        void* physPreset;
        void* collmaps;
        void* physConstraints;
        void* submodels;
        void* meshes[8];
        XModelMeshMaterials* meshMaterials;
        byte* unkC0;
        uint32_t numLods;
        byte lodParams[0x40];
        float mins[3];
        float maxs[3];
        uint32_t unk124;
        uint32_t unk128;
        uint32_t unk12c;
        uint64_t usage;
        uint32_t contents;
        float radius;
        int32_t numCollSurfs;
        uint16_t numCosmeticBones;
        uint16_t unk146;
        int8_t highestShadowLod;
        int8_t highestRenderLod;
        int8_t numSubmodels;
        byte numBones;
        byte numRootBones;
        byte numCollmaps;
        byte unk14e;
        byte unk14f;
    };
    static_assert(sizeof(XModel) == 0x150);

    struct XSurfaceShared;

    struct XSurface {
        byte tileMode;
        byte unk1;
        byte vertListCount;
        byte flags;
        uint16_t vertCount;
        uint16_t triCount;
        uint32_t baseVertIndex;
        uint32_t baseIndexOffset;
        XSurfaceShared* shared;
        byte* vertList;
        uint64_t unk20;
        byte* partBits;
    };
    static_assert(sizeof(XSurface) == 0x30);

    struct XModelMesh {
        XHash name;
        XSurface* surfs;
        XSurfaceShared* shared;
        byte* partBits;
        byte xpakEntry[0x20];
        uint32_t unk48;
        uint16_t numSurfs;
        uint16_t unk4e;
        uint64_t unk50;
    };
    static_assert(sizeof(XModelMesh) == 0x58);

    constexpr size_t XSURFACE_SHARED_SIZE = 0x200;
    constexpr size_t PART_BITS_SIZE = 0x30;

    class ModelWorker : public Worker {
        using Worker::Worker;

        void Unlink(fastfile::FastFileOption& opt, void* ptr) override {
            XModel* asset{ (XModel*)ptr };

            std::filesystem::path outFile{ opt.m_output / "bo4" / "graphics" / "xmodel" /
                                           std::format("{}.json", hashutils::ExtractTmp("file", asset->name)) };
            std::filesystem::create_directories(outFile.parent_path());
            BO4JsonWriter json{};

            LOG_OPT_INFO("Dump xmodel {}", outFile.string());

            size_t bones{ (size_t)asset->numBones + asset->numCosmeticBones };
            size_t parents{ bones - asset->numRootBones };

            json.BeginObject();
            json.WriteFieldValueXHash("name", asset->name);
            json.WriteFieldValueString("header", Hex(asset, sizeof(*asset)));

            json.WriteFieldNameString("boneNames");
            json.BeginArray();
            for (size_t i = 0; asset->boneNames && i < bones; i++) {
                json.WriteValueString(GetScrString(asset->boneNames[i]));
            }
            json.EndArray();

            json.WriteFieldNameString("boneNameIndices");
            json.BeginArray();
            for (size_t i = 0; asset->boneNameIndices && i < asset->numBones; i++) {
                json.BeginObject();
                json.WriteFieldValueString("name", GetScrString(asset->boneNameIndices[i].name));
                json.WriteFieldValueNumber("index", asset->boneNameIndices[i].index);
                json.EndObject();
            }
            json.EndArray();

            auto Blob = [&json](const char* name, const void* data, size_t len) {
                if (data && len) {
                    json.WriteFieldValueString(name, Hex(data, len));
                }
            };
            Blob("parentList", asset->parentList, parents);
            Blob("quats", asset->quats, parents * 8);
            Blob("trans", asset->trans, parents * 0x10);
            Blob("partClassification", asset->partClassification, bones);
            Blob("baseMat", asset->baseMat, bones * 0x20);
            Blob("boneInfo", asset->boneInfo, asset->numBones * 0x2Cull);
            Blob("unkC0", asset->unkC0, 0x30);

            json.WriteFieldNameString("meshes");
            json.BeginArray();
            for (void* mesh : asset->meshes) {
                if (mesh) {
                    json.WriteValueHash(games::bo4::pool::GetAssetName(
                                            games::bo4::pool::XAssetType::ASSET_TYPE_XMODELMESH, mesh
                    )->name);
                } else {
                    json.WriteValueNull();
                }
            }
            json.EndArray();

            json.WriteFieldNameString("meshMaterials");
            json.BeginArray();
            for (size_t i = 0; asset->meshMaterials && i < asset->numLods; i++) {
                XModelMeshMaterials* mm{ asset->meshMaterials + i };
                json.BeginObject();
                json.WriteFieldValueString("entry", Hex(mm, sizeof(*mm)));
                json.WriteFieldNameString("materials");
                json.BeginArray();
                for (size_t j = 0; mm->materials && j < mm->numMaterials; j++) {
                    void* mat{ mm->materials[j] };
                    if (mat) {
                        json.WriteValueHash(games::bo4::pool::GetAssetName(
                                                games::bo4::pool::XAssetType::ASSET_TYPE_MATERIAL, mat
                        )->name);
                    } else {
                        json.WriteValueNull();
                    }
                }
                json.EndArray();
                Blob("himipInvSqRadii", mm->himipInvSqRadii, mm->numMaterials * 4ull);
                json.EndObject();
            }
            json.EndArray();

            json.WriteFieldValueXAsset(
                "physPreset",
                games::bo4::pool::XAssetType::ASSET_TYPE_PHYSPRESET,
                asset->physPreset
            );
            json.WriteFieldValueXAsset(
                "physConstraints",
                games::bo4::pool::XAssetType::ASSET_TYPE_PHYSCONSTRAINTS,
                asset->physConstraints
            );
            json.WriteFieldValueNumber("numCollSurfs", asset->numCollSurfs);
            json.WriteFieldValueNumber("numCollmaps", (int)asset->numCollmaps);

            // collision surfaces, 0x38 each: tris (0x30 each, count at 0x10), a blob {data, size}
            json.WriteFieldNameString("collSurfs");
            json.BeginArray();
            for (int i = 0; asset->collSurfs && i < asset->numCollSurfs; i++) {
                byte* cs{ (byte*)asset->collSurfs + 0x38ull * i };
                byte* tris{ *(byte**)cs };
                byte* blob{ *(byte**)(cs + 8) };
                uint32_t numTris{ *(uint32_t*)(cs + 0x10) };
                json.BeginObject();
                json.WriteFieldValueString("header", Hex(cs, 0x38));
                Blob("tris", tris, numTris * 0x30ull);
                if (blob) {
                    json.WriteFieldValueString("blobHeader", Hex(blob, 0x10));
                    Blob("blob", *(byte**)blob, *(uint32_t*)(blob + 8));
                }
                json.EndObject();
            }
            json.EndArray();

            // collmaps, 0x20 each: geom list, {count at 0x10, 8 bytes each at 0x18}
            json.WriteFieldNameString("collmaps");
            json.BeginArray();
            for (int i = 0; asset->collmaps && i < asset->numCollmaps; i++) {
                byte* cm{ (byte*)asset->collmaps + 0x20ull * i };
                byte* geomList{ *(byte**)cm };
                json.BeginObject();
                json.WriteFieldValueString("header", Hex(cm, 0x20));
                Blob("list", *(byte**)(cm + 0x18), *(uint32_t*)(cm + 0x10) * 8ull);
                if (geomList) {
                    // count, geoms (0x10 each: brush, 0x40 bytes), blob {data, size}
                    json.WriteFieldValueString("geomList", Hex(geomList, 0x18));
                    uint32_t count{ *(uint32_t*)geomList };
                    byte* geoms{ *(byte**)(geomList + 8) };
                    byte* blob{ *(byte**)(geomList + 0x10) };
                    json.WriteFieldNameString("geoms");
                    json.BeginArray();
                    for (uint32_t j = 0; geoms && j < count; j++) {
                        byte* geom{ geoms + 0x10ull * j };
                        byte* brush{ *(byte**)geom };
                        json.BeginObject();
                        Blob("info", *(byte**)(geom + 8), 0x40);
                        if (brush) {
                            json.WriteFieldValueString("brush", Hex(brush, 0x40));
                            Blob("sides", *(byte**)brush, brush[0x3A] * 0x14ull);
                            Blob("verts", *(byte**)(brush + 8), *(uint16_t*)(brush + 0x38) * 0xCull);
                        }
                        json.EndObject();
                    }
                    json.EndArray();
                    if (blob) {
                        json.WriteFieldValueString("blobHeader", Hex(blob, 0x10));
                        Blob("blob", *(byte**)blob, *(uint32_t*)(blob + 8));
                    }
                }
                json.EndObject();
            }
            json.EndArray();
            json.WriteFieldValueNumber("numSubmodels", (int)asset->numSubmodels);
            json.WriteFieldValueNumber("numLods", asset->numLods);
            json.WriteFieldValueNumber("numBones", (int)asset->numBones);
            json.EndObject();

            if (!json.WriteToFile(outFile)) {
                LOG_ERROR("Error when dumping {}", outFile.string());
            }
        }
    };

    class MeshWorker : public Worker {
        using Worker::Worker;

        void Unlink(fastfile::FastFileOption& opt, void* ptr) override {
            XModelMesh* asset{ (XModelMesh*)ptr };

            std::filesystem::path outFile{ opt.m_output / "bo4" / "graphics" / "xmodelmesh" /
                                           std::format("{}.json", hashutils::ExtractTmp("file", asset->name)) };
            std::filesystem::create_directories(outFile.parent_path());
            BO4JsonWriter json{};

            LOG_OPT_INFO("Dump xmodelmesh {}", outFile.string());

            json.BeginObject();
            json.WriteFieldValueXHash("name", asset->name);
            json.WriteFieldValueString("header", Hex(asset, sizeof(*asset)));
            if (asset->partBits) {
                json.WriteFieldValueString("partBits", Hex(asset->partBits, PART_BITS_SIZE));
            }
            if (asset->shared) {
                json.WriteFieldValueString("shared", Hex(asset->shared, XSURFACE_SHARED_SIZE));

                // flags (bit 0: streamed), data, dataSize: the data of a resident mesh is loaded
                uint32_t flags{ *(uint32_t*)asset->shared };
                byte* data{ *(byte**)((byte*)asset->shared + 0x20) };
                uint32_t dataSize{ *(uint32_t*)((byte*)asset->shared + 0x28) };
                if (!(flags & 1) && data && dataSize) {
                    json.WriteFieldValueString("data", Hex(data, dataSize));
                }
            }

            json.WriteFieldNameString("surfs");
            json.BeginArray();
            for (size_t i = 0; asset->surfs && i < asset->numSurfs; i++) {
                XSurface* surf{ asset->surfs + i };
                json.BeginObject();
                json.WriteFieldValueString("header", Hex(surf, sizeof(*surf)));
                // the shared block of the mesh or one of its own
                json.WriteFieldValueBool("meshShared", surf->shared == asset->shared);
                if (surf->shared && surf->shared != asset->shared) {
                    json.WriteFieldValueString("shared", Hex(surf->shared, XSURFACE_SHARED_SIZE));
                }
                if (surf->vertList && surf->vertListCount) {
                    json.WriteFieldValueString("vertList", Hex(surf->vertList, surf->vertListCount * 8ull));
                }
                if (surf->partBits) {
                    json.WriteFieldValueString("partBits", Hex(surf->partBits, PART_BITS_SIZE));
                }
                json.EndObject();
            }
            json.EndArray();
            json.EndObject();

            if (!json.WriteToFile(outFile)) {
                LOG_ERROR("Error when dumping {}", outFile.string());
            }
        }
    };

    utils::MapAdder<ModelWorker, games::bo4::pool::XAssetType, Worker> impl{
        GetWorkers(), games::bo4::pool::XAssetType::ASSET_TYPE_XMODEL, true
    };
    utils::MapAdder<MeshWorker, games::bo4::pool::XAssetType, Worker> implMesh{
        GetWorkers(), games::bo4::pool::XAssetType::ASSET_TYPE_XMODELMESH, true
    };
} // namespace
