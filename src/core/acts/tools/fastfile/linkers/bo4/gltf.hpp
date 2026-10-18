#pragma once
#include <includes.hpp>

/*
 * A small glTF 2.0 reader (.gltf with its .bin or data uris, .glb) for the bo4 xmodel and xanim linkers: the node
 * tree, the meshes (positions, normals, uvs, colors, joints, weights, indices, material names), the skins and the
 * animations (translation, rotation and scale channels). Matrices are column major as glTF has them.
 */
namespace fastfile::linker::bo4::gltf {
    struct Node {
        std::string name{};
        int parent{ -1 };
        std::vector<int> children{};
        double t[3]{ 0, 0, 0 };
        double r[4]{ 0, 0, 0, 1 }; // x y z w
        double s[3]{ 1, 1, 1 };
        int mesh{ -1 };
        int skin{ -1 };
    };

    struct Primitive {
        std::vector<float> positions{}; // 3 per vertex
        std::vector<float> normals{};   // 3 per vertex or none
        std::vector<float> uvs{};       // 2 per vertex or none
        std::vector<float> colors{};    // 4 per vertex or none
        std::vector<uint32_t> joints{}; // 4 per vertex or none
        std::vector<float> weights{};   // 4 per vertex or none
        std::vector<uint32_t> indices{};
        std::string material{};
    };

    struct Mesh {
        std::string name{};
        std::vector<Primitive> primitives{};
    };

    struct Skin {
        std::string name{};
        std::vector<int> joints{};
        std::vector<double> inverseBind{}; // 16 per joint, column major
    };

    enum ChannelPath { PATH_TRANSLATION, PATH_ROTATION, PATH_SCALE, PATH_OTHER };

    struct Channel {
        int node{ -1 };
        ChannelPath path{ PATH_OTHER };
        std::vector<float> times{};
        std::vector<float> values{}; // 3 or 4 per key (the middle value of cubic splines)
        bool step{};
    };

    struct Animation {
        std::string name{};
        std::vector<Channel> channels{};
    };

    // 4x4 column major
    struct Mat4 {
        double m[16]{ 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1 };

        Mat4 operator*(const Mat4& o) const {
            Mat4 r{};
            for (int c = 0; c < 4; c++) {
                for (int rr = 0; rr < 4; rr++) {
                    double v{};
                    for (int k = 0; k < 4; k++) {
                        v += m[k * 4 + rr] * o.m[c * 4 + k];
                    }
                    r.m[c * 4 + rr] = v;
                }
            }
            return r;
        }

        void Apply(const double in[3], double out[3], double w = 1) const {
            for (int rr = 0; rr < 3; rr++) {
                out[rr] = m[rr] * in[0] + m[4 + rr] * in[1] + m[8 + rr] * in[2] + m[12 + rr] * w;
            }
        }

        static Mat4 FromTRS(const double t[3], const double q[4], const double s[3]) {
            double x{ q[0] }, y{ q[1] }, z{ q[2] }, w{ q[3] };
            Mat4 r{};
            r.m[0] = (1 - 2 * (y * y + z * z)) * s[0];
            r.m[1] = (2 * (x * y + z * w)) * s[0];
            r.m[2] = (2 * (x * z - y * w)) * s[0];
            r.m[3] = 0;
            r.m[4] = (2 * (x * y - z * w)) * s[1];
            r.m[5] = (1 - 2 * (x * x + z * z)) * s[1];
            r.m[6] = (2 * (y * z + x * w)) * s[1];
            r.m[7] = 0;
            r.m[8] = (2 * (x * z + y * w)) * s[2];
            r.m[9] = (2 * (y * z - x * w)) * s[2];
            r.m[10] = (1 - 2 * (x * x + y * y)) * s[2];
            r.m[11] = 0;
            r.m[12] = t[0];
            r.m[13] = t[1];
            r.m[14] = t[2];
            r.m[15] = 1;
            return r;
        }

        // translation, rotation (x y z w) and scale of an affine matrix without shear
        void Decompose(double t[3], double q[4], double s[3]) const {
            t[0] = m[12];
            t[1] = m[13];
            t[2] = m[14];
            double c[3][3];
            for (int i = 0; i < 3; i++) {
                s[i] = std::sqrt(m[i * 4] * m[i * 4] + m[i * 4 + 1] * m[i * 4 + 1] + m[i * 4 + 2] * m[i * 4 + 2]);
                for (int j = 0; j < 3; j++) {
                    c[i][j] = s[i] > 0 ? m[i * 4 + j] / s[i] : 0;
                }
            }
            // c[col][row]
            double tr{ c[0][0] + c[1][1] + c[2][2] };
            if (tr > 0) {
                double k{ std::sqrt(tr + 1) * 2 };
                q[3] = k / 4;
                q[0] = (c[1][2] - c[2][1]) / k;
                q[1] = (c[2][0] - c[0][2]) / k;
                q[2] = (c[0][1] - c[1][0]) / k;
            } else if (c[0][0] > c[1][1] && c[0][0] > c[2][2]) {
                double k{ std::sqrt(1 + c[0][0] - c[1][1] - c[2][2]) * 2 };
                q[3] = (c[1][2] - c[2][1]) / k;
                q[0] = k / 4;
                q[1] = (c[1][0] + c[0][1]) / k;
                q[2] = (c[2][0] + c[0][2]) / k;
            } else if (c[1][1] > c[2][2]) {
                double k{ std::sqrt(1 + c[1][1] - c[0][0] - c[2][2]) * 2 };
                q[3] = (c[2][0] - c[0][2]) / k;
                q[0] = (c[1][0] + c[0][1]) / k;
                q[1] = k / 4;
                q[2] = (c[2][1] + c[1][2]) / k;
            } else {
                double k{ std::sqrt(1 + c[2][2] - c[0][0] - c[1][1]) * 2 };
                q[3] = (c[0][1] - c[1][0]) / k;
                q[0] = (c[2][0] + c[0][2]) / k;
                q[1] = (c[2][1] + c[1][2]) / k;
                q[2] = k / 4;
            }
            double n{ std::sqrt(q[0] * q[0] + q[1] * q[1] + q[2] * q[2] + q[3] * q[3]) };
            for (int i = 0; i < 4; i++) {
                q[i] /= n > 0 ? n : 1;
            }
        }

        Mat4 Inverse() const {
            const double* a{ m };
            double inv[16];
            inv[0] = a[5] * a[10] * a[15] - a[5] * a[11] * a[14] - a[9] * a[6] * a[15] + a[9] * a[7] * a[14] + a[13] * a[6] * a[11] - a[13] * a[7] * a[10];
            inv[4] = -a[4] * a[10] * a[15] + a[4] * a[11] * a[14] + a[8] * a[6] * a[15] - a[8] * a[7] * a[14] - a[12] * a[6] * a[11] + a[12] * a[7] * a[10];
            inv[8] = a[4] * a[9] * a[15] - a[4] * a[11] * a[13] - a[8] * a[5] * a[15] + a[8] * a[7] * a[13] + a[12] * a[5] * a[11] - a[12] * a[7] * a[9];
            inv[12] = -a[4] * a[9] * a[14] + a[4] * a[10] * a[13] + a[8] * a[5] * a[14] - a[8] * a[6] * a[13] - a[12] * a[5] * a[10] + a[12] * a[6] * a[9];
            inv[1] = -a[1] * a[10] * a[15] + a[1] * a[11] * a[14] + a[9] * a[2] * a[15] - a[9] * a[3] * a[14] - a[13] * a[2] * a[11] + a[13] * a[3] * a[10];
            inv[5] = a[0] * a[10] * a[15] - a[0] * a[11] * a[14] - a[8] * a[2] * a[15] + a[8] * a[3] * a[14] + a[12] * a[2] * a[11] - a[12] * a[3] * a[10];
            inv[9] = -a[0] * a[9] * a[15] + a[0] * a[11] * a[13] + a[8] * a[1] * a[15] - a[8] * a[3] * a[13] - a[12] * a[1] * a[11] + a[12] * a[3] * a[9];
            inv[13] = a[0] * a[9] * a[14] - a[0] * a[10] * a[13] - a[8] * a[1] * a[14] + a[8] * a[2] * a[13] + a[12] * a[1] * a[10] - a[12] * a[2] * a[9];
            inv[2] = a[1] * a[6] * a[15] - a[1] * a[7] * a[14] - a[5] * a[2] * a[15] + a[5] * a[3] * a[14] + a[13] * a[2] * a[7] - a[13] * a[3] * a[6];
            inv[6] = -a[0] * a[6] * a[15] + a[0] * a[7] * a[14] + a[4] * a[2] * a[15] - a[4] * a[3] * a[14] - a[12] * a[2] * a[7] + a[12] * a[3] * a[6];
            inv[10] = a[0] * a[5] * a[15] - a[0] * a[7] * a[13] - a[4] * a[1] * a[15] + a[4] * a[3] * a[13] + a[12] * a[1] * a[7] - a[12] * a[3] * a[5];
            inv[14] = -a[0] * a[5] * a[14] + a[0] * a[6] * a[13] + a[4] * a[1] * a[14] - a[4] * a[2] * a[13] - a[12] * a[1] * a[6] + a[12] * a[2] * a[5];
            inv[3] = -a[1] * a[6] * a[11] + a[1] * a[7] * a[10] + a[5] * a[2] * a[11] - a[5] * a[3] * a[10] - a[9] * a[2] * a[7] + a[9] * a[3] * a[6];
            inv[7] = a[0] * a[6] * a[11] - a[0] * a[7] * a[10] - a[4] * a[2] * a[11] + a[4] * a[3] * a[10] + a[8] * a[2] * a[7] - a[8] * a[3] * a[6];
            inv[11] = -a[0] * a[5] * a[11] + a[0] * a[7] * a[9] + a[4] * a[1] * a[11] - a[4] * a[3] * a[9] - a[8] * a[1] * a[7] + a[8] * a[3] * a[5];
            inv[15] = a[0] * a[5] * a[10] - a[0] * a[6] * a[9] - a[4] * a[1] * a[10] + a[4] * a[2] * a[9] + a[8] * a[1] * a[6] - a[8] * a[2] * a[5];
            double det{ a[0] * inv[0] + a[1] * inv[4] + a[2] * inv[8] + a[3] * inv[12] };
            Mat4 r{};
            for (int i = 0; i < 16; i++) {
                r.m[i] = det != 0 ? inv[i] / det : 0;
            }
            return r;
        }
    };

    class Document {
      public:
        std::vector<Node> nodes{};
        std::vector<Mesh> meshes{};
        std::vector<Skin> skins{};
        std::vector<Animation> animations{};

        Mat4 LocalMatrix(int node) const {
            const Node& n{ nodes[node] };
            return Mat4::FromTRS(n.t, n.r, n.s);
        }

        Mat4 WorldMatrix(int node) const {
            Mat4 m{ LocalMatrix(node) };
            for (int p = nodes[node].parent; p >= 0; p = nodes[p].parent) {
                m = LocalMatrix(p) * m;
            }
            return m;
        }

        bool Load(const std::filesystem::path& path, std::string& err) {
            std::vector<byte> file{};
            if (!utils::ReadFile(path, file)) {
                err = "can't read the file";
                return false;
            }
            std::string json{};
            std::vector<byte> glbBin{};
            if (file.size() >= 12 && !std::memcmp(file.data(), "glTF", 4)) {
                // glb: header, then chunks (length, type, data)
                size_t pos{ 12 };
                while (pos + 8 <= file.size()) {
                    uint32_t len{ *(uint32_t*)&file[pos] };
                    uint32_t type{ *(uint32_t*)&file[pos + 4] };
                    if (pos + 8 + len > file.size()) {
                        break;
                    }
                    if (type == 0x4E4F534A) {
                        json.assign((const char*)&file[pos + 8], len);
                    } else if (type == 0x004E4942) {
                        glbBin.assign(&file[pos + 8], &file[pos + 8 + len]);
                    }
                    pos += 8 + ((len + 3) & ~3u);
                }
            } else {
                json.assign((const char*)file.data(), file.size());
            }
            rapidjson::Document doc{};
            doc.Parse(json.data(), json.size());
            if (doc.HasParseError() || !doc.IsObject()) {
                err = "bad json";
                return false;
            }

            // buffers
            std::vector<std::vector<byte>> buffers{};
            for (const rapidjson::Value& b : Array(doc, "buffers")) {
                std::vector<byte>& buf{ buffers.emplace_back() };
                const char* uri{ String(b, "uri") };
                if (!uri) {
                    buf = glbBin;
                } else if (!std::strncmp(uri, "data:", 5)) {
                    const char* comma{ std::strchr(uri, ',') };
                    if (!comma || !Base64(comma + 1, buf)) {
                        err = "bad data uri";
                        return false;
                    }
                } else if (!utils::ReadFile(path.parent_path() / uri, buf)) {
                    err = std::format("can't read buffer {}", uri);
                    return false;
                }
            }
            // accessors: the raw values as doubles, normalized ints mapped to [0, 1] or [-1, 1]
            auto Read = [&](int index, std::vector<double>& out, int& comps) -> bool {
                const rapidjson::Value* acc{ Element(doc, "accessors", index) };
                if (!acc) {
                    return false;
                }
                static const std::pair<const char*, int> types[]{ { "SCALAR", 1 }, { "VEC2", 2 }, { "VEC3", 3 }, { "VEC4", 4 }, { "MAT4", 16 } };
                comps = 0;
                for (const auto& [name, n] : types) {
                    if (!std::strcmp(String(*acc, "type", ""), name)) {
                        comps = n;
                    }
                }
                int ctype{ Int(*acc, "componentType", 0) };
                size_t count{ (size_t)Int(*acc, "count", 0) };
                bool normalized{ Bool(*acc, "normalized") };
                size_t csize{ ctype == 5120 || ctype == 5121 ? 1u : ctype == 5122 || ctype == 5123 ? 2u : 4u };
                out.assign(count * comps, 0.0);
                if (!comps) {
                    return false;
                }
                if (!acc->HasMember("bufferView")) {
                    return true; // zeros
                }
                const rapidjson::Value* view{ Element(doc, "bufferViews", Int(*acc, "bufferView", -1)) };
                if (!view) {
                    return false;
                }
                int bi{ Int(*view, "buffer", -1) };
                if (bi < 0 || bi >= (int)buffers.size()) {
                    return false;
                }
                const std::vector<byte>& buf{ buffers[bi] };
                size_t base{ (size_t)Int(*view, "byteOffset", 0) + (size_t)Int(*acc, "byteOffset", 0) };
                size_t stride{ (size_t)Int(*view, "byteStride", 0) };
                if (!stride) {
                    stride = csize * comps;
                }
                for (size_t i = 0; i < count; i++) {
                    for (int c = 0; c < comps; c++) {
                        size_t off{ base + i * stride + c * csize };
                        if (off + csize > buf.size()) {
                            return false;
                        }
                        const byte* p{ &buf[off] };
                        double v{};
                        switch (ctype) {
                        case 5120: v = *(int8_t*)p; if (normalized) v = std::max(v / 127.0, -1.0); break;
                        case 5121: v = *(uint8_t*)p; if (normalized) v /= 255.0; break;
                        case 5122: v = *(int16_t*)p; if (normalized) v = std::max(v / 32767.0, -1.0); break;
                        case 5123: v = *(uint16_t*)p; if (normalized) v /= 65535.0; break;
                        case 5125: v = *(uint32_t*)p; break;
                        case 5126: v = *(float*)p; break;
                        default: return false;
                        }
                        out[i * comps + c] = v;
                    }
                }
                return true;
            };

            std::vector<std::string> materials{};
            for (const rapidjson::Value& m : Array(doc, "materials")) {
                materials.emplace_back(String(m, "name", ""));
            }

            for (const rapidjson::Value& n : Array(doc, "nodes")) {
                Node& node{ nodes.emplace_back() };
                node.name = String(n, "name", "");
                node.mesh = Int(n, "mesh", -1);
                node.skin = Int(n, "skin", -1);
                for (const rapidjson::Value& c : Array(n, "children")) {
                    node.children.push_back(c.GetInt());
                }
                if (const rapidjson::Value* mat{ Member(n, "matrix") }; mat && mat->IsArray() && mat->Size() == 16) {
                    Mat4 m{};
                    for (rapidjson::SizeType i = 0; i < 16; i++) {
                        m.m[i] = (*mat)[i].GetDouble();
                    }
                    m.Decompose(node.t, node.r, node.s);
                } else {
                    Numbers(n, "translation", node.t, 3);
                    Numbers(n, "rotation", node.r, 4);
                    Numbers(n, "scale", node.s, 3);
                }
            }
            for (size_t i = 0; i < nodes.size(); i++) {
                for (int c : nodes[i].children) {
                    if (c < 0 || c >= (int)nodes.size()) {
                        err = "bad node child";
                        return false;
                    }
                    nodes[c].parent = (int)i;
                }
            }

            std::vector<double> tmp{};
            int comps{};
            for (const rapidjson::Value& m : Array(doc, "meshes")) {
                Mesh& mesh{ meshes.emplace_back() };
                mesh.name = String(m, "name", "");
                for (const rapidjson::Value& p : Array(m, "primitives")) {
                    if (Int(p, "mode", 4) != 4) {
                        continue; // triangles only
                    }
                    Primitive& prim{ mesh.primitives.emplace_back() };
                    int mat{ Int(p, "material", -1) };
                    prim.material = mat >= 0 && mat < (int)materials.size() ? materials[mat] : "";
                    const rapidjson::Value* attr{ Member(p, "attributes") };
                    if (!attr) {
                        err = "primitive without attributes";
                        return false;
                    }
                    auto Attr = [&](const char* name, size_t want, auto& out) -> bool {
                        const rapidjson::Value* a{ Member(*attr, name) };
                        if (!a) {
                            return true;
                        }
                        if (!Read(a->GetInt(), tmp, comps) || (want && (size_t)comps < want)) {
                            err = std::format("bad attribute {}", name);
                            return false;
                        }
                        size_t n{ want ? want : (size_t)comps };
                        size_t count{ tmp.size() / comps };
                        out.resize(count * n);
                        for (size_t i = 0; i < count; i++) {
                            for (size_t c = 0; c < n; c++) {
                                out[i * n + c] = (std::remove_reference_t<decltype(out[0])>)(c < (size_t)comps ? tmp[i * comps + c] : 1.0);
                            }
                        }
                        return true;
                    };
                    if (!Attr("POSITION", 3, prim.positions) || !Attr("NORMAL", 3, prim.normals) ||
                        !Attr("TEXCOORD_0", 2, prim.uvs) || !Attr("JOINTS_0", 4, prim.joints) ||
                        !Attr("WEIGHTS_0", 4, prim.weights)) {
                        return false;
                    }
                    if (const rapidjson::Value* a{ Member(*attr, "COLOR_0") }) {
                        if (!Read(a->GetInt(), tmp, comps) || comps < 3) {
                            err = "bad attribute COLOR_0";
                            return false;
                        }
                        size_t count{ tmp.size() / comps };
                        prim.colors.resize(count * 4);
                        for (size_t i = 0; i < count; i++) {
                            for (int c = 0; c < 4; c++) {
                                prim.colors[i * 4 + c] = (float)(c < comps ? tmp[i * comps + c] : 1.0);
                            }
                        }
                    }
                    size_t vcount{ prim.positions.size() / 3 };
                    if (p.HasMember("indices")) {
                        if (!Read(Int(p, "indices", -1), tmp, comps)) {
                            err = "bad indices";
                            return false;
                        }
                        prim.indices.assign(tmp.begin(), tmp.end());
                    } else {
                        for (size_t i = 0; i < vcount; i++) {
                            prim.indices.push_back((uint32_t)i);
                        }
                    }
                    for (uint32_t i : prim.indices) {
                        if (i >= vcount) {
                            err = "index out of the vertices";
                            return false;
                        }
                    }
                }
            }

            for (const rapidjson::Value& s : Array(doc, "skins")) {
                Skin& skin{ skins.emplace_back() };
                skin.name = String(s, "name", "");
                for (const rapidjson::Value& j : Array(s, "joints")) {
                    skin.joints.push_back(j.GetInt());
                }
                if (s.HasMember("inverseBindMatrices")) {
                    if (!Read(Int(s, "inverseBindMatrices", -1), skin.inverseBind, comps) || comps != 16) {
                        err = "bad inverseBindMatrices";
                        return false;
                    }
                } else {
                    for (size_t i = 0; i < skin.joints.size(); i++) {
                        Mat4 id{};
                        skin.inverseBind.insert(skin.inverseBind.end(), id.m, id.m + 16);
                    }
                }
            }

            for (const rapidjson::Value& a : Array(doc, "animations")) {
                Animation& anim{ animations.emplace_back() };
                anim.name = String(a, "name", "");
                for (const rapidjson::Value& c : Array(a, "channels")) {
                    const rapidjson::Value* target{ Member(c, "target") };
                    const rapidjson::Value* sampler{ Element(a, "samplers", Int(c, "sampler", -1)) };
                    if (!target || !sampler) {
                        continue;
                    }
                    Channel& ch{ anim.channels.emplace_back() };
                    ch.node = Int(*target, "node", -1);
                    std::string pathName{ String(*target, "path", "") };
                    ch.path = pathName == "translation" ? PATH_TRANSLATION : pathName == "rotation" ? PATH_ROTATION : pathName == "scale" ? PATH_SCALE : PATH_OTHER;
                    std::string interp{ String(*sampler, "interpolation", "LINEAR") };
                    ch.step = interp == "STEP";
                    if (!Read(Int(*sampler, "input", -1), tmp, comps)) {
                        err = "bad animation input";
                        return false;
                    }
                    ch.times.assign(tmp.begin(), tmp.end());
                    if (!Read(Int(*sampler, "output", -1), tmp, comps)) {
                        err = "bad animation output";
                        return false;
                    }
                    if (interp == "CUBICSPLINE") {
                        // in tangent, value, out tangent per key: keep the values
                        for (size_t k = 0; k < ch.times.size(); k++) {
                            for (int i = 0; i < comps; i++) {
                                ch.values.push_back((float)tmp[(k * 3 + 1) * comps + i]);
                            }
                        }
                    } else {
                        ch.values.assign(tmp.begin(), tmp.end());
                    }
                }
            }
            return true;
        }

        // the world matrices of all the nodes from their local ones
        std::vector<Mat4> WorldMatrices(const std::vector<Mat4>& locals) const {
            std::vector<Mat4> world(nodes.size());
            std::vector<bool> done(nodes.size());
            std::function<const Mat4&(int)> Get = [&](int i) -> const Mat4& {
                if (!done[i]) {
                    world[i] = nodes[i].parent >= 0 ? Get(nodes[i].parent) * locals[i] : locals[i];
                    done[i] = true;
                }
                return world[i];
            };
            for (size_t i = 0; i < nodes.size(); i++) {
                Get((int)i);
            }
            return world;
        }

      private:
        static const rapidjson::Value* Member(const rapidjson::Value& v, const char* name) {
            if (!v.IsObject()) {
                return nullptr;
            }
            auto it{ v.FindMember(name) };
            return it == v.MemberEnd() ? nullptr : &it->value;
        }
        static rapidjson::Value::ConstArray Array(const rapidjson::Value& v, const char* name) {
            static const rapidjson::Value empty{ rapidjson::kArrayType };
            const rapidjson::Value* m{ Member(v, name) };
            return m && m->IsArray() ? m->GetArray() : empty.GetArray();
        }
        static const rapidjson::Value* Element(const rapidjson::Value& v, const char* name, int index) {
            const rapidjson::Value* m{ Member(v, name) };
            return m && m->IsArray() && index >= 0 && index < (int)m->Size() ? &(*m)[(rapidjson::SizeType)index] : nullptr;
        }
        static const char* String(const rapidjson::Value& v, const char* name, const char* def = nullptr) {
            const rapidjson::Value* m{ Member(v, name) };
            return m && m->IsString() ? m->GetString() : def;
        }
        static int Int(const rapidjson::Value& v, const char* name, int def) {
            const rapidjson::Value* m{ Member(v, name) };
            return m && m->IsInt() ? m->GetInt() : def;
        }
        static bool Bool(const rapidjson::Value& v, const char* name) {
            const rapidjson::Value* m{ Member(v, name) };
            return m && m->IsBool() && m->GetBool();
        }
        static void Numbers(const rapidjson::Value& v, const char* name, double* out, size_t n) {
            const rapidjson::Value* m{ Member(v, name) };
            for (size_t i = 0; m && m->IsArray() && i < n && i < m->Size(); i++) {
                out[i] = (*m)[(rapidjson::SizeType)i].GetDouble();
            }
        }
        static bool Base64(const char* in, std::vector<byte>& out) {
            uint32_t acc{};
            int bits{};
            for (; *in && *in != '='; in++) {
                char c{ *in };
                int v{ c >= 'A' && c <= 'Z' ? c - 'A' : c >= 'a' && c <= 'z' ? c - 'a' + 26 : c >= '0' && c <= '9' ? c - '0' + 52 : c == '+' ? 62 : c == '/' ? 63 : -1 };
                if (v < 0) {
                    return false;
                }
                acc = (acc << 6) | v;
                bits += 6;
                if (bits >= 8) {
                    bits -= 8;
                    out.push_back((byte)(acc >> bits));
                }
            }
            return true;
        }
    };
    /*
     * The bones of a model from a skin: its joints, the roots first and every parent before its children (the game's
     * order). The bones have no rotation at rest (their frame is the joint's times the inverse of its rest rotation),
     * so the model's bind quats are identities and an animated frame is the joint's world matrix times the inverse of
     * its rest rotation: skinning gives the glTF result even when the bind pose isn't the rest pose. Game space: glTF
     * x y z is x -z y (z up), translations times scale.
     */
    struct Skeleton {
        std::vector<int> nodes{};        // the node of each bone
        std::vector<int> parents{};      // the parent bone, -1 for a root
        std::vector<std::string> names{};
        std::vector<Mat4> restWorld{};   // glTF world matrix of each joint at rest
        std::vector<Mat4> restRotInv{};  // the inverse of its rest rotation
        size_t numRoots{};
        double scale{ 1 };

        // a game space rotation (x y z w) and translation of a rigid glTF matrix
        void ToGame(const Mat4& m, double q[4], double t[3]) const {
            double gt[3], gq[4], gs[3];
            m.Decompose(gt, gq, gs);
            q[0] = gq[0];
            q[1] = -gq[2];
            q[2] = gq[1];
            q[3] = gq[3];
            t[0] = gt[0] * scale;
            t[1] = -gt[2] * scale;
            t[2] = gt[1] * scale;
        }

        void ToGamePoint(const double p[3], double out[3]) const {
            out[0] = p[0] * scale;
            out[1] = -p[2] * scale;
            out[2] = p[1] * scale;
        }

        void ToGameVector(const double p[3], double out[3]) const {
            out[0] = p[0];
            out[1] = -p[2];
            out[2] = p[1];
        }

        // the bone frames (game space, local to the parent bone) of the node world matrices
        void LocalFrames(const std::vector<Mat4>& world, std::vector<std::array<double, 4>>& quats, std::vector<std::array<double, 3>>& trans) const {
            std::vector<Mat4> frames(nodes.size());
            for (size_t b = 0; b < nodes.size(); b++) {
                frames[b] = world[nodes[b]] * restRotInv[b];
            }
            quats.resize(nodes.size());
            trans.resize(nodes.size());
            for (size_t b = 0; b < nodes.size(); b++) {
                Mat4 local{ parents[b] >= 0 ? frames[parents[b]].Inverse() * frames[b] : frames[b] };
                ToGame(local, quats[b].data(), trans[b].data());
            }
        }

        bool Build(const Document& doc, int skin, double scaleIn, std::string& err) {
            scale = scaleIn;
            std::vector<Mat4> locals(doc.nodes.size());
            for (size_t i = 0; i < doc.nodes.size(); i++) {
                locals[i] = doc.LocalMatrix((int)i);
            }
            std::vector<Mat4> world{ doc.WorldMatrices(locals) };
            std::vector<int> joints{};
            if (skin >= 0) {
                joints = doc.skins[skin].joints;
            }
            if (joints.size() > 0xFF) {
                err = "more than 255 bones";
                return false;
            }
            auto IsJoint = [&joints](int n) { return std::find(joints.begin(), joints.end(), n) != joints.end(); };
            // the parent joint: the closest joint above
            auto ParentJoint = [&](int n) {
                for (int p = doc.nodes[n].parent; p >= 0; p = doc.nodes[p].parent) {
                    if (IsJoint(p)) {
                        return p;
                    }
                }
                return -1;
            };
            // roots, then breadth first
            std::vector<int> order{};
            for (int j : joints) {
                if (ParentJoint(j) < 0) {
                    order.push_back(j);
                }
            }
            numRoots = order.size();
            for (size_t i = 0; i < order.size(); i++) {
                for (int j : joints) {
                    if (ParentJoint(j) == order[i]) {
                        order.push_back(j);
                    }
                }
            }
            if (order.size() != joints.size()) {
                err = "bad joint hierarchy";
                return false;
            }
            for (int n : order) {
                nodes.push_back(n);
                names.push_back(doc.nodes[n].name.empty() ? std::format("bone_{}", n) : doc.nodes[n].name);
                int pj{ ParentJoint(n) };
                int pb{ pj < 0 ? -1 : (int)(std::find(order.begin(), order.end(), pj) - order.begin()) };
                if (pb >= 0 && nodes.size() - 1 - pb > 0xFF) {
                    err = "a bone too far from its parent";
                    return false;
                }
                parents.push_back(pb);
                restWorld.push_back(world[n]);
                double t[3], q[4], s[3];
                world[n].Decompose(t, q, s);
                double zero[3]{ 0, 0, 0 }, one[3]{ 1, 1, 1 };
                restRotInv.push_back(Mat4::FromTRS(zero, q, one).Inverse());
            }
            return true;
        }
    };
} // namespace fastfile::linker::bo4::gltf
