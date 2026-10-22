#include <includes.hpp>
#include <tools/fastfile/handlers/handler_game_bo4.hpp>
#include <tools/fastfile/handlers/bo4/bo4_xpak_reader.hpp>
#include <utils/data_utils.hpp>
#include <numbers>

/*
 * xmodel and xmodelmesh templates for the bo4 linkers (acts ff ... -a xmodel,xmodelmesh): the blocks as they are,
 * the assets by name, the script strings as strings. A model of several bones also gets its skeleton as a glTF (a
 * mannequin skinned to it) to model on, the xmodel linker's "skeleton" taking the json.
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

    // a script string, null for none ("" is a string)
    void WriteScr(BO4JsonWriter& json, ScrString_t s) {
        if (*(uint32_t*)&s) {
            json.WriteValueString(GetScrString(s));
        } else {
            json.WriteValueNull();
        }
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

    // the 64-bit bind quats: the index of the dropped largest axis, the three 20-bit axes after it, its sign at bit 63
    void UnpackQuat(uint64_t v, double q[4]) {
        static constexpr int INDEX_AXIS[4]{ 0, 3, 2, 1 };
        int a{ INDEX_AXIS[v & 3] };
        double sum{};
        for (int k = 0; k < 3; k++) {
            int64_t c{ (int64_t)((v >> (2 + 20 * k)) & 0xFFFFF) };
            if (c & 0x80000) {
                c -= 0x100000;
            }
            double f{ c / (double)(1 << 19) / std::numbers::sqrt2 };
            q[(a + 1 + k) % 4] = f;
            sum += f * f;
        }
        double big{ std::sqrt(std::max(0.0, 1 - sum)) };
        q[a] = (v >> 63) ? -big : big;
    }

    struct Frame {
        double q[4]{ 0, 0, 0, 1 };
        double t[3]{};

        void Rotate(const double v[3], double out[3]) const {
            // v + 2w (u x v) + 2 u x (u x v)
            const double* u{ q };
            double c[3]{ u[1] * v[2] - u[2] * v[1], u[2] * v[0] - u[0] * v[2], u[0] * v[1] - u[1] * v[0] };
            double cc[3]{ u[1] * c[2] - u[2] * c[1], u[2] * c[0] - u[0] * c[2], u[0] * c[1] - u[1] * c[0] };
            for (int i = 0; i < 3; i++) {
                out[i] = v[i] + 2 * (q[3] * c[i] + cc[i]);
            }
        }
    };

    // game space (z up) to glTF (y up): x y z is x z -y
    void GameToGltf(const double in[3], double out[3]) {
        out[0] = in[0];
        out[1] = in[2];
        out[2] = -in[1];
    }

    std::string Base64(const std::vector<byte>& data) {
        static const char* chars{ "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/" };
        std::string out{};
        out.reserve((data.size() + 2) / 3 * 4);
        for (size_t i = 0; i < data.size(); i += 3) {
            uint32_t v{ (uint32_t)data[i] << 16 };
            if (i + 1 < data.size()) v |= (uint32_t)data[i + 1] << 8;
            if (i + 2 < data.size()) v |= data[i + 2];
            out += chars[(v >> 18) & 63];
            out += chars[(v >> 12) & 63];
            out += i + 1 < data.size() ? chars[(v >> 6) & 63] : '=';
            out += i + 2 < data.size() ? chars[v & 63] : '=';
        }
        return out;
    }

    /*
     * The skeleton of a model as a glTF for modelling on it: every bone (cosmetic ones too) as a joint in its bind
     * pose, local to its parent (the cosmetic bones' parent list entries are absolute indices), the inverse bind
     * matrices of baseMat, and a mannequin skinned to it (a spike from each j_ bone to the next, a stub on the ends) to
     * see it move. Game units, glTF axes (the xmodel linker's "scale": 1). The xmodel linker's "skeleton" takes the
     * model's json to rebuild the bones exactly, the glTF joints are only matched by name.
     */
    void WriteSkeletonGltf(const std::filesystem::path& outFile, XModel* asset) {
        size_t numMain{ asset->numBones };
        size_t count{ numMain + asset->numCosmeticBones };
        size_t roots{ asset->numRootBones };
        if (count < 2 || !asset->boneNames || !asset->baseMat || roots > count) {
            return;
        }
        std::vector<int> parents(count, -1);
        std::vector<Frame> local(count), world(count);
        for (size_t b = 0; b < count; b++) {
            const float* bm{ (const float*)(asset->baseMat + 0x20 * b) };
            for (int i = 0; i < 4; i++) world[b].q[i] = bm[i];
            for (int i = 0; i < 3; i++) world[b].t[i] = bm[4 + i];
            if (b < roots) {
                local[b] = world[b];
                continue;
            }
            size_t i{ b - roots };
            byte p{ asset->parentList ? asset->parentList[i] : (byte)0 };
            parents[b] = b < numMain ? (int)(b - p) : (int)p;
            if (parents[b] < 0 || parents[b] >= (int)b) {
                parents[b] = 0;
            }
            if (asset->quats) UnpackQuat(asset->quats[i], local[b].q);
            if (asset->trans) {
                const float* t{ (const float*)(asset->trans + 12 * i) };
                for (int k = 0; k < 3; k++) local[b].t[k] = t[k];
            }
        }
        std::vector<std::string> names(count);
        for (size_t b = 0; b < count; b++) {
            names[b] = GetScrString(asset->boneNames[b]);
        }

        // the mannequin, game space: flat shaded spikes, each on one bone
        std::vector<float> positions{}, normals{};
        std::vector<uint16_t> joints{};
        std::vector<float> weights{};
        auto Tri = [&](const double* a, const double* b, const double* c, const double* inside, size_t bone) {
            double e1[3]{ b[0] - a[0], b[1] - a[1], b[2] - a[2] }, e2[3]{ c[0] - a[0], c[1] - a[1], c[2] - a[2] };
            double n[3]{ e1[1] * e2[2] - e1[2] * e2[1], e1[2] * e2[0] - e1[0] * e2[2], e1[0] * e2[1] - e1[1] * e2[0] };
            double out[3]{ a[0] - inside[0], a[1] - inside[1], a[2] - inside[2] };
            const double* v[3]{ a, b, c };
            if (n[0] * out[0] + n[1] * out[1] + n[2] * out[2] < 0) {
                std::swap(v[1], v[2]);
                for (double& x : n) x = -x;
            }
            double len{ std::sqrt(n[0] * n[0] + n[1] * n[1] + n[2] * n[2]) };
            if (len <= 0) {
                return;
            }
            double gn[3], gp[3], nn[3]{ n[0] / len, n[1] / len, n[2] / len };
            GameToGltf(nn, gn);
            for (const double* p : v) {
                GameToGltf(p, gp);
                positions.insert(positions.end(), { (float)gp[0], (float)gp[1], (float)gp[2] });
                normals.insert(normals.end(), { (float)gn[0], (float)gn[1], (float)gn[2] });
                joints.insert(joints.end(), { (uint16_t)bone, 0, 0, 0 });
                weights.insert(weights.end(), { 1.0f, 0, 0, 0 });
            }
        };
        auto Spike = [&](const double a[3], const double b[3], double radius, size_t bone) {
            double d[3]{ b[0] - a[0], b[1] - a[1], b[2] - a[2] };
            double len{ std::sqrt(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]) };
            if (len < 0.25) {
                return;
            }
            for (double& x : d) x /= len;
            double up[3]{ 0, 0, 1 };
            if (std::abs(d[2]) > 0.9) {
                up[0] = 1;
                up[2] = 0;
            }
            double u[3]{ d[1] * up[2] - d[2] * up[1], d[2] * up[0] - d[0] * up[2], d[0] * up[1] - d[1] * up[0] };
            double ul{ std::sqrt(u[0] * u[0] + u[1] * u[1] + u[2] * u[2]) };
            for (double& x : u) x /= ul;
            double w[3]{ d[1] * u[2] - d[2] * u[1], d[2] * u[0] - d[0] * u[2], d[0] * u[1] - d[1] * u[0] };
            double ring[4][3];
            double mid[3];
            for (int i = 0; i < 3; i++) mid[i] = a[i] + d[i] * len * 0.2;
            for (int k = 0; k < 4; k++) {
                double s{ k == 0 ? 1.0 : k == 2 ? -1.0 : 0.0 }, c{ k == 1 ? 1.0 : k == 3 ? -1.0 : 0.0 };
                for (int i = 0; i < 3; i++) ring[k][i] = mid[i] + (u[i] * s + w[i] * c) * radius;
            }
            for (int k = 0; k < 4; k++) {
                Tri(a, ring[k], ring[(k + 1) % 4], mid, bone);
                Tri(b, ring[(k + 1) % 4], ring[k], mid, bone);
            }
        };
        auto IsBody = [&](size_t b) { return b < numMain && names[b].starts_with("j_"); };
        std::vector<bool> hasChild(count);
        for (size_t b = roots; b < count; b++) {
            int p{ parents[b] };
            if (!IsBody(b) || p < 0 || !IsBody((size_t)p)) {
                continue;
            }
            double len{ std::hypot(world[b].t[0] - world[p].t[0], world[b].t[1] - world[p].t[1], world[b].t[2] - world[p].t[2]) };
            Spike(world[p].t, world[b].t, std::clamp(len * 0.15, 0.3, 2.5), (size_t)p);
            hasChild[p] = true;
        }
        for (size_t b = 0; b < count; b++) {
            bool body{ IsBody(b) };
            bool head{ names[b] == "j_head" };
            if ((body && hasChild[b] && !head) || (!body && !positions.empty())) {
                continue;
            }
            // the ends along the bone's x, the head a bigger one
            double len{ head ? 9.0 : 1.5 };
            double x[3]{ len, 0, 0 }, dx[3], end[3];
            world[b].Rotate(x, dx);
            for (int i = 0; i < 3; i++) end[i] = world[b].t[i] + dx[i];
            Spike(world[b].t, end, head ? 3.5 : 0.4, b);
        }
        if (positions.empty()) {
            // no j_ bones: a stub on each bone
            for (size_t b = 0; b < numMain; b++) {
                double x[3]{ 2, 0, 0 }, dx[3], end[3];
                world[b].Rotate(x, dx);
                for (int i = 0; i < 3; i++) end[i] = world[b].t[i] + dx[i];
                Spike(world[b].t, end, 0.4, b);
            }
        }
        size_t vertCount{ positions.size() / 3 };

        // the buffer: inverse binds, positions, normals, joints, weights
        std::vector<byte> buffer{};
        auto Put = [&buffer](const void* data, size_t len) {
            size_t off{ buffer.size() };
            buffer.insert(buffer.end(), (const byte*)data, (const byte*)data + len);
            return off;
        };
        std::vector<float> ibm(16 * count);
        for (size_t b = 0; b < count; b++) {
            // the inverse of the rigid bind matrix, glTF axes, column major: rotation transposed, -R^T t
            Frame g{};
            double gq[3]{ world[b].q[0], world[b].q[1], world[b].q[2] }, gqo[3];
            GameToGltf(gq, gqo);
            g.q[0] = gqo[0];
            g.q[1] = gqo[1];
            g.q[2] = gqo[2];
            g.q[3] = world[b].q[3];
            GameToGltf(world[b].t, g.t);
            double r[3][3];
            for (int c = 0; c < 3; c++) {
                double axis[3]{ c == 0 ? 1.0 : 0.0, c == 1 ? 1.0 : 0.0, c == 2 ? 1.0 : 0.0 };
                g.Rotate(axis, r[c]);  // r[c] = column c of R
            }
            float* m{ &ibm[16 * b] };
            for (int col = 0; col < 3; col++) {
                for (int row = 0; row < 3; row++) {
                    m[col * 4 + row] = (float)r[row][col];  // (R^T)[row][col] = R[col][row] = column row of R, entry col
                }
                m[col * 4 + 3] = 0;
            }
            for (int row = 0; row < 3; row++) {
                m[12 + row] = (float)-(r[row][0] * g.t[0] + r[row][1] * g.t[1] + r[row][2] * g.t[2]);
            }
            m[15] = 1;
        }
        size_t ibmOff{ Put(ibm.data(), ibm.size() * 4) };
        size_t posOff{ Put(positions.data(), positions.size() * 4) };
        size_t nrmOff{ Put(normals.data(), normals.size() * 4) };
        size_t jntOff{ Put(joints.data(), joints.size() * 2) };
        size_t wgtOff{ Put(weights.data(), weights.size() * 4) };

        float mins[3]{ 1e30f, 1e30f, 1e30f }, maxs[3]{ -1e30f, -1e30f, -1e30f };
        for (size_t i = 0; i < positions.size(); i++) {
            mins[i % 3] = std::min(mins[i % 3], positions[i]);
            maxs[i % 3] = std::max(maxs[i % 3], positions[i]);
        }

        std::string nodes{}, rootList{};
        for (size_t b = 0; b < count; b++) {
            double lt[3], lq[3]{ local[b].q[0], local[b].q[1], local[b].q[2] }, gq[3];
            GameToGltf(local[b].t, lt);
            GameToGltf(lq, gq);
            std::string children{};
            for (size_t c = b + 1; c < count; c++) {
                if (parents[c] == (int)b) {
                    children += std::format("{}{}", children.empty() ? "" : ",", c);
                }
            }
            if (parents[b] < 0) {
                rootList += std::format("{},", b);
            }
            nodes += std::format("{{\"name\":\"{}\",\"translation\":[{},{},{}],\"rotation\":[{},{},{},{}]{}}},\n",
                                 names[b], lt[0], lt[1], lt[2], gq[0], gq[1], gq[2], local[b].q[3],
                                 children.empty() ? "" : std::format(",\"children\":[{}]", children));
        }
        std::string jointList{};
        for (size_t b = 0; b < count; b++) {
            jointList += std::format("{}{}", b ? "," : "", b);
        }
        std::string modelName{ outFile.stem().string() };
        nodes += std::format("{{\"name\":\"{}_mannequin\",\"mesh\":0,\"skin\":0}}", modelName);

        std::string gltf{ std::format(
            "{{\"asset\":{{\"version\":\"2.0\",\"generator\":\"acts xmodel skeleton\"}},\n"
            "\"scene\":0,\"scenes\":[{{\"nodes\":[{}{}]}}],\n"
            "\"nodes\":[\n{}],\n"
            "\"skins\":[{{\"name\":\"{}\",\"joints\":[{}],\"inverseBindMatrices\":0}}],\n"
            "\"materials\":[{{\"name\":\"mannequin\"}}],\n"
            "\"meshes\":[{{\"name\":\"{}_mannequin\",\"primitives\":[{{\"attributes\":{{\"POSITION\":1,\"NORMAL\":2,\"JOINTS_0\":3,\"WEIGHTS_0\":4}},\"material\":0}}]}}],\n"
            "\"accessors\":[\n"
            "{{\"bufferView\":0,\"componentType\":5126,\"count\":{},\"type\":\"MAT4\"}},\n"
            "{{\"bufferView\":1,\"componentType\":5126,\"count\":{},\"type\":\"VEC3\",\"min\":[{},{},{}],\"max\":[{},{},{}]}},\n"
            "{{\"bufferView\":2,\"componentType\":5126,\"count\":{},\"type\":\"VEC3\"}},\n"
            "{{\"bufferView\":3,\"componentType\":5123,\"count\":{},\"type\":\"VEC4\"}},\n"
            "{{\"bufferView\":4,\"componentType\":5126,\"count\":{},\"type\":\"VEC4\"}}],\n"
            "\"bufferViews\":[\n"
            "{{\"buffer\":0,\"byteOffset\":{},\"byteLength\":{}}},\n"
            "{{\"buffer\":0,\"byteOffset\":{},\"byteLength\":{}}},\n"
            "{{\"buffer\":0,\"byteOffset\":{},\"byteLength\":{}}},\n"
            "{{\"buffer\":0,\"byteOffset\":{},\"byteLength\":{}}},\n"
            "{{\"buffer\":0,\"byteOffset\":{},\"byteLength\":{}}}],\n"
            "\"buffers\":[{{\"byteLength\":{},\"uri\":\"data:application/octet-stream;base64,{}\"}}]}}\n",
            rootList, count, nodes, modelName, jointList, modelName,
            count, vertCount, mins[0], mins[1], mins[2], maxs[0], maxs[1], maxs[2], vertCount, vertCount, vertCount,
            ibmOff, ibm.size() * 4, posOff, positions.size() * 4, nrmOff, normals.size() * 4,
            jntOff, joints.size() * 2, wgtOff, weights.size() * 4,
            buffer.size(), Base64(buffer)
        ) };
        if (!utils::WriteFile(outFile, gltf)) {
            LOG_ERROR("Error when dumping {}", outFile.string());
        }
    }

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
                WriteScr(json, asset->boneNames[i]);
            }
            json.EndArray();

            json.WriteFieldNameString("boneNameIndices");
            json.BeginArray();
            for (size_t i = 0; asset->boneNameIndices && i < asset->numBones; i++) {
                json.BeginObject();
                json.WriteFieldNameString("name");
                WriteScr(json, asset->boneNameIndices[i].name);
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
            // submodels, 0x28 each: an xmodel, a script string at 8
            json.WriteFieldNameString("submodels");
            json.BeginArray();
            for (int i = 0; asset->submodels && i < asset->numSubmodels; i++) {
                byte* sm{ (byte*)asset->submodels + 0x28ull * i };
                json.BeginObject();
                json.WriteFieldValueString("raw", Hex(sm, 0x28));
                json.WriteFieldValueXAsset("model", games::bo4::pool::XAssetType::ASSET_TYPE_XMODEL, *(void**)sm);
                json.WriteFieldNameString("str");
                WriteScr(json, *(ScrString_t*)(sm + 8));
                json.EndObject();
            }
            json.EndArray();
            json.WriteFieldValueBool("lossless", true);
            json.WriteFieldValueNumber("numSubmodels", (int)asset->numSubmodels);
            json.WriteFieldValueNumber("numLods", asset->numLods);
            json.WriteFieldValueNumber("numBones", (int)asset->numBones);
            json.EndObject();

            if (!json.WriteToFile(outFile)) {
                LOG_ERROR("Error when dumping {}", outFile.string());
            }
            // the skeleton to model on
            if (asset->numBones > 1) {
                std::filesystem::path gltfFile{ outFile };
                gltfFile.replace_extension(".gltf");
                WriteSkeletonGltf(gltfFile, asset);
            }
        }
    };

    class MeshWorker : public Worker {
        using Worker::Worker;

        void PreXFileLoading(fastfile::FastFileOption& opt, fastfile::FastFileContext& ctx) override {
            xpak::AddHint(ctx.ffname);
        }

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
                // streamed: the data from the game's xpak, by the mesh's key
                uint64_t key{ *(uint64_t*)asset->xpakEntry };
                if ((flags & 1) && key && dataSize) {
                    std::vector<byte> streamed{};
                    if (xpak::ReadEntry(opt, key, dataSize, streamed)) {
                        json.WriteFieldValueString("streamData", Hex(streamed.data(), streamed.size()));
                    } else {
                        LOG_WARNING("xmodelmesh {}: no xpak entry 0x{:x}", hashutils::ExtractTmp("hash", asset->name), key);
                    }
                }
                // the script strings at 0x1E0: count, a string, 4 bytes strings, 8 bytes entries (a string first)
                byte* sh{ (byte*)asset->shared };
                uint32_t count{ *(uint32_t*)(sh + 0x1E0) };
                json.WriteFieldNameString("sharedStr");
                WriteScr(json, *(ScrString_t*)(sh + 0x1E4));
                ScrString_t* list4{ *(ScrString_t**)(sh + 0x1E8) };
                if (list4) {
                    json.WriteFieldNameString("sharedStrings");
                    json.BeginArray();
                    for (uint32_t i = 0; i < count; i++) {
                        WriteScr(json, list4[i]);
                    }
                    json.EndArray();
                }
                byte* list8{ *(byte**)(sh + 0x1F0) };
                if (list8) {
                    json.WriteFieldValueString("sharedEntries", Hex(list8, count * 8ull));
                    json.WriteFieldNameString("sharedEntryStrings");
                    json.BeginArray();
                    for (uint32_t i = 0; i < count; i++) {
                        WriteScr(json, *(ScrString_t*)(list8 + 8ull * i));
                    }
                    json.EndArray();
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
