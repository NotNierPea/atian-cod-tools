#pragma once
#include <includes.hpp>
#include <tools/fastfile/linkers/linker_bo4.hpp>
#include <tools/fastfile/linkers/bo4/gltf.hpp>

/*
 * A game skeleton for the xmodel and xanim linkers ("skeleton": a model json of the acts dumper): its bones as they are,
 * the cosmetic ones too (after the main bones, their parent list entries are absolute indices). Load_XModel reads the
 * name indices for the main bones, the names, parents, quats, trans (16 bytes a bone), classification and baseMat (the
 * bind pose in the model's space: quat, trans, 2) for all, boneInfo (0x2C, hit boxes in each bone's frame) for the main.
 */
namespace fastfile::linker::bo4::gltf {
    struct GameSkeleton {
        static constexpr size_t HEADER_SIZE = 0x150;
        static constexpr size_t BASE_MAT_SIZE = 0x20;
        static constexpr size_t BONE_INFO_SIZE = 0x2C;

        std::vector<std::string> names{};
        std::vector<int> parents{};  // -1 for a root
        size_t numMain{}, numCosmetic{}, numRoots{};
        std::vector<byte> parentList{}, quats{}, trans{}, partClassification{}, baseMat{}, boneInfo{};

        size_t Count() const { return numMain + numCosmetic; }

        int Index(const std::string& name) const {
            auto it{ std::find(names.begin(), names.end(), name) };
            return it == names.end() ? -1 : (int)(it - names.begin());
        }

        // the bind pose in the model's space
        void Bind(size_t b, double q[4], double t[3]) const {
            const float* f{ (const float*)(baseMat.data() + BASE_MAT_SIZE * b) };
            for (int i = 0; i < 4; i++) q[i] = f[i];
            for (int i = 0; i < 3; i++) t[i] = f[4 + i];
        }

        // the bind pose as a glTF matrix (y up, translation / scale), the inverse of gltf::Skeleton::ToGame
        Mat4 BindMatrix(size_t b, double scale) const {
            double q[4], t[3];
            Bind(b, q, t);
            double gq[4]{ q[0], q[2], -q[1], q[3] };
            double gt[3]{ t[0] / scale, t[2] / scale, -t[1] / scale };
            double s[3]{ 1, 1, 1 };
            return Mat4::FromTRS(gt, gq, s);
        }

        // v turned by the quaternion q (x, y, z, w)
        static void Rotate(const double q[4], const double v[3], double out[3]) {
            double c[3]{ q[1] * v[2] - q[2] * v[1], q[2] * v[0] - q[0] * v[2], q[0] * v[1] - q[1] * v[0] };
            double cc[3]{ q[1] * c[2] - q[2] * c[1], q[2] * c[0] - q[0] * c[2], q[0] * c[1] - q[1] * c[0] };
            for (int i = 0; i < 3; i++) out[i] = v[i] + 2 * (q[3] * c[i] + cc[i]);
        }

        /*
         * The bones at other places, their orientations kept (a model of other proportions on the game's animations,
         * which turn the bones and move only the root and helper bones): the bind (baseMat) and the local trans from
         * the given model space positions, a bone without one at its parent's new place plus its own offset
         */
        void Reposition(const std::vector<std::optional<std::array<double, 3>>>& pos) {
            size_t n{ Count() };
            std::vector<std::array<double, 3>> world(n);
            std::vector<std::array<double, 4>> rots(n);
            for (size_t b = 0; b < n; b++) {
                double q[4], t[3];
                Bind(b, q, t);
                rots[b] = { q[0], q[1], q[2], q[3] };
                if (b < pos.size() && pos[b]) {
                    world[b] = *pos[b];
                } else if (parents[b] >= 0) {
                    const float* lt{ (const float*)(trans.data() + 12 * (b - numRoots)) };
                    double local[3]{ lt[0], lt[1], lt[2] }, off[3];
                    Rotate(rots[parents[b]].data(), local, off);
                    for (int i = 0; i < 3; i++) world[b][i] = world[parents[b]][i] + off[i];
                } else {
                    world[b] = { t[0], t[1], t[2] };
                }
            }
            for (size_t b = 0; b < n; b++) {
                float* f{ (float*)(baseMat.data() + BASE_MAT_SIZE * b) };
                for (int i = 0; i < 3; i++) f[4 + i] = (float)world[b][i];
                int p{ parents[b] };
                if (p < 0) {
                    continue;
                }
                // the offset in the parent's bind frame (the inverse of its rotation)
                double inv[4]{ -rots[p][0], -rots[p][1], -rots[p][2], rots[p][3] };
                double d[3]{ world[b][0] - world[p][0], world[b][1] - world[p][1], world[b][2] - world[p][2] }, local[3];
                Rotate(inv, d, local);
                float* lt{ (float*)(trans.data() + 12 * (b - numRoots)) };
                for (int i = 0; i < 3; i++) lt[i] = (float)local[i];
            }
        }

        bool Load(const std::filesystem::path& path, std::string& err) {
            BO4Json js{ path };
            if (!js.SyncConfig(false)) {
                err = "can't read it";
                return false;
            }
            auto Hex = [&js](const char* key, std::vector<byte>& out) {
                out.clear();
                auto it{ js.main.FindMember(key) };
                if (it == js.main.MemberEnd() || !it->value.IsString() || (it->value.GetStringLength() & 1)) {
                    return false;
                }
                auto Digit = [](char c) { return c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1; };
                const char* str{ it->value.GetString() };
                for (size_t i = 0; i < it->value.GetStringLength(); i += 2) {
                    int hi{ Digit(str[i]) }, lo{ Digit(str[i + 1]) };
                    if (hi < 0 || lo < 0) {
                        return false;
                    }
                    out.push_back((byte)((hi << 4) | lo));
                }
                return true;
            };
            std::vector<byte> header{};
            if (!Hex("header", header) || header.size() != HEADER_SIZE) {
                err = "no model header, not a model json of the acts dumper";
                return false;
            }
            numCosmetic = *(uint16_t*)(header.data() + 0x144);
            numMain = header[0x14B];
            numRoots = header[0x14C];
            auto namesIt{ js.main.FindMember("boneNames") };
            if (namesIt == js.main.MemberEnd() || !namesIt->value.IsArray() || namesIt->value.Size() != Count() ||
                !numRoots || numRoots > numMain) {
                err = "its bones don't match its header";
                return false;
            }
            for (const rapidjson::Value& n : namesIt->value.GetArray()) {
                names.push_back(n.IsString() ? n.GetString() : "");
            }
            size_t children{ Count() - numRoots };
            if (!Hex("partClassification", partClassification) || partClassification.size() != Count() ||
                !Hex("baseMat", baseMat) || baseMat.size() != BASE_MAT_SIZE * Count() ||
                !Hex("boneInfo", boneInfo) || boneInfo.size() != BONE_INFO_SIZE * numMain) {
                err = "missing bone data (partClassification, baseMat, boneInfo)";
                return false;
            }
            if (children && (!Hex("parentList", parentList) || parentList.size() != children ||
                             !Hex("quats", quats) || quats.size() != 8 * children ||
                             !Hex("trans", trans) || trans.size() != 0x10 * children)) {
                err = "missing bone data (parentList, quats, trans)";
                return false;
            }
            parents.assign(Count(), -1);
            for (size_t b = numRoots; b < Count(); b++) {
                byte p{ parentList[b - numRoots] };
                int parent{ b < numMain ? (int)b - p : (int)p };
                if (parent < 0 || parent >= (int)b) {
                    err = std::format("bone {} has a bad parent", names[b]);
                    return false;
                }
                parents[b] = parent;
            }
            return true;
        }
    };
} // namespace fastfile::linker::bo4::gltf
