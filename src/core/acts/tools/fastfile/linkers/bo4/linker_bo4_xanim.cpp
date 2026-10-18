#include <includes.hpp>
#include <tools/fastfile/linkers/linker_bo4.hpp>
#include <tools/fastfile/linkers/bo4/gltf.hpp>
#include <tools/fastfile/linkers/bo4/game_skeleton.hpp>
#include <numbers>

/*
 * XAnims from a glTF animation, on the bones of the xmodel linker (gltf::Skeleton: same file, same bones, same scale).
 * Every bone is sampled at the framerate: a rotation or translation that never changes is static, the others have a key
 * per frame (full quats, 16 bits translations).
 *
 * {
 *     "name": "my_anim",
 *     "gltf": "models/my_model.gltf",
 *     "animation": "my_action",   // its name or index, the first by default
 *     "framerate": 30,
 *     "scale": 1.0,               // the model's
 *     "loop": true,
 *     "rootMotion": "tag_origin",  // a root bone whose moves (translation, yaw) move the entity instead (delta part)
 *     "skeleton": "skeletons/body.json",  // for a game skeleton (the xmodel linker's): its bones' frames
 *     "notifies": [                // time 0..1, "end" is added at 1
 *         { "name": "hit", "time": 0.5 },                           // a script notify (the notetrack)
 *         { "sound": "my_alias", "tag": "j_head", "time": 0.2 },    // a sound alias, on a tag or the entity
 *         { "rumble": "damage_light", "time": 0.3 },
 *         { "fx": "my_fx", "tag": "tag_fx", "time": 0.4 },         // play fx on a tag
 *         { "kind": "footstep", "name": "footstep_left_forward", "time": 0.6 }  // any kind of the game's
 *     ],
 *     "startupNotifies": [ ... ], "shutdownNotifies": [ ... ]   // when the anim starts / ends, no time
 * }
 *
 * The game's notify kinds (param3): "self notify", "sound", "rumble", "footstep", "play fx", "play unmanaged fx",
 * "stop fx", "kill fx", "stop sound", "start looping sound", "stop looping sound", "clip attach/detach left"...
 *
 * XAnimParts (retail 0x130, the loader sub_142E35510): bone names in rotation category order, boneCount[10] at 0xFC
 * (0 no rot, 1 2d, 2 full animated, 3 2d static, 4 full static, 5 byte trans, 6 short trans, 7 static trans, 8 no
 * trans, 9 all). dataShort: the key counts - 1 of the animated rotations, the static quats, the key counts - 1 of the
 * animated translations. dataByte: the frame indices of the animated rotations, then per animated translation its
 * bone index and frame indices, then the bones of the static translations and of those without. randomDataShort:
 * the rotation keys then the translation keys (3 u16). dataInt: per animated translation its min and size (range /
 * 65535), then the static translations. Quats are packed in 64 bits (PackQuat). From 256 frames the frame indices are
 * u16 in dataShort after each key count instead of u8 in dataByte. At most 255 bones (a byte in the models).
 *
 * Root motion, the delta part at 0x18 (the loader sub_142E35180, the flag at 0x118): trans {u16 key count - 1, byte
 * smallTrans; mins, size, frames}, quat2 {u16 key count - 1; frames} (the yaw: z as a float, bit 30 when w < 0), the
 * frame indices inline after each (u8, u16 from 256 frames), the keys at frames (trans 3 u16 or 3 u8, quat2 4 bytes).
 * Relative to the first frame, in its frame.
 *
 * Game skeleton ("skeleton", a model json of the acts dumper, the glTF made on its glTF): the keys are in its bones'
 * own frames, so the anim plays on the game's models of that skeleton too. Each joint moves from its rest as in the
 * glTF (its world matrix times the inverse of its rest, the skinning of the glTF), the game bone's frame is that move
 * of its bind frame, local to its game parent (a game bone without a joint follows its parent). The joints that aren't
 * bones of the skeleton are left out, the bone orientations of the glTF don't matter (blender changes them).
 */
namespace {
    using namespace fastfile::linker::bo4;

    constexpr size_t XANIM_PARTS_SIZE = 0x130;
    constexpr size_t NOTIFY_SIZE = 0x28;
    constexpr size_t MAX_FRAMES = 0xFFFF;
    constexpr size_t LONG_ANIM_FRAMES = 0x100;
    constexpr uint32_t QUAT2_W_NEGATIVE = 0x40000000;
    constexpr double STATIC_EPSILON = 1e-5;
    constexpr double TRANS_STATIC_EPSILON = 1e-4;  // game units, the float noise of a game skeleton's bind

    enum BoneCategory : size_t {
        CAT_ROT_ANIMATED = 2,
        CAT_ROT_STATIC = 4,
        CAT_TRANS_SHORT = 6,
        CAT_TRANS_STATIC = 7,
        CAT_TOTAL = 9,
    };

    uint64_t PackQuat(const double qin[4]) {
        double q[4]{ qin[0], qin[1], qin[2], qin[3] };
        double n{ std::sqrt(q[0] * q[0] + q[1] * q[1] + q[2] * q[2] + q[3] * q[3]) };
        int a{};
        for (int i = 0; i < 4; i++) {
            q[i] = n > 0 ? q[i] / n : (i == 3 ? 1 : 0);
            if (std::abs(q[i]) > std::abs(q[a])) {
                a = i;
            }
        }
        static constexpr uint64_t AXIS_INDEX[4]{ 0, 3, 2, 1 };
        uint64_t v{ AXIS_INDEX[a] };
        for (int k = 0; k < 3; k++) {
            double c{ q[(a + 1 + k) % 4] * std::numbers::sqrt2 * (1 << 19) };
            int64_t ic{ std::clamp<int64_t>(std::llround(c), -(1 << 19), (1 << 19) - 1) };
            v |= ((uint64_t)ic & 0xFFFFF) << (2 + 20 * k);
        }
        if (q[a] < 0) {
            v |= 1ull << 63;
        }
        return v;
    }

    // the value of a channel at a time, linear (slerp for rotations) or step
    void Sample(const gltf::Channel& ch, double t, size_t comps, double* out) {
        const std::vector<float>& times{ ch.times };
        if (times.empty() || ch.values.size() < times.size() * comps) {
            return;
        }
        size_t k{};
        while (k + 1 < times.size() && times[k + 1] <= t) {
            k++;
        }
        const float* a{ &ch.values[k * comps] };
        if (k + 1 >= times.size() || t <= times[0] || ch.step) {
            const float* v{ t <= times[0] ? &ch.values[0] : a };
            for (size_t i = 0; i < comps; i++) {
                out[i] = v[i];
            }
            return;
        }
        const float* b{ &ch.values[(k + 1) * comps] };
        double f{ (t - times[k]) / std::max(1e-9, (double)times[k + 1] - times[k]) };
        if (comps == 4) {
            double dot{ (double)a[0] * b[0] + (double)a[1] * b[1] + (double)a[2] * b[2] + (double)a[3] * b[3] };
            double sb{ dot < 0 ? -1.0 : 1.0 };
            dot = std::abs(dot);
            double wa{ 1 - f }, wb{ f };
            if (dot < 0.9995) {
                double th{ std::acos(dot) };
                wa = std::sin((1 - f) * th) / std::sin(th);
                wb = std::sin(f * th) / std::sin(th);
            }
            double n{};
            for (size_t i = 0; i < 4; i++) {
                out[i] = wa * a[i] + wb * sb * b[i];
                n += out[i] * out[i];
            }
            n = std::sqrt(n);
            for (size_t i = 0; i < 4; i++) {
                out[i] /= n > 0 ? n : 1;
            }
            return;
        }
        for (size_t i = 0; i < comps; i++) {
            out[i] = a[i] + (b[i] - a[i]) * f;
        }
    }

    template<typename T>
    void Put(std::vector<byte>& out, T v) {
        out.insert(out.end(), (const byte*)&v, (const byte*)&v + sizeof(T));
    }

    class XAssetLinkerImpl : public XAssetLinker {
      public:
        using XAssetLinker::XAssetLinker;

        void Compute(BO4LinkContext& ctx, const char* id, BO4FFContext& ff) override {
            std::filesystem::path path{ ctx.linkCtx.input / id };
            std::filesystem::path rfpath{ path.filename() };
            rfpath.replace_extension();
            auto Fail = [&ctx, &path](const std::string& what) {
                LOG_ERROR("Invalid xanim {}: {}", path.string(), what);
                ctx.error = true;
            };

            BO4Json cfg{ path };
            if (!cfg.SyncConfig(false)) {
                Fail("can't read");
                return;
            }
            const rapidjson::Value& root{ cfg.main };
            std::string assetName{ cfg.GetString("name", rfpath.string().c_str()) };
            std::string gltfPath{ cfg.GetString("gltf", "") };
            double framerate{ cfg.GetDouble("framerate", 30.0) };
            double scale{ cfg.GetDouble("scale", 1.0) };
            auto loopIt{ root.FindMember("loop") };
            bool loop{ loopIt != root.MemberEnd() && loopIt->value.IsBool() && loopIt->value.GetBool() };
            if (framerate <= 0) {
                Fail("bad framerate");
                return;
            }

            gltf::Document doc{};
            std::string err{};
            if (gltfPath.empty() || !doc.Load(path.parent_path() / gltfPath, err)) {
                Fail(std::format("can't read gltf {}: {}", gltfPath, err));
                return;
            }
            if (doc.skins.empty() || doc.animations.empty()) {
                Fail("the gltf has no skin or no animation");
                return;
            }
            gltf::Skeleton skel{};
            if (!skel.Build(doc, 0, scale, err)) {
                Fail(err);
                return;
            }
            // the animation by name or index
            int animIndex{};
            auto animIt{ root.FindMember("animation") };
            if (animIt != root.MemberEnd() && animIt->value.IsInt()) {
                animIndex = animIt->value.GetInt();
            } else if (animIt != root.MemberEnd() && animIt->value.IsString()) {
                animIndex = -1;
                for (size_t i = 0; i < doc.animations.size(); i++) {
                    if (doc.animations[i].name == animIt->value.GetString()) {
                        animIndex = (int)i;
                    }
                }
            }
            if (animIndex < 0 || animIndex >= (int)doc.animations.size()) {
                Fail("no such animation");
                return;
            }
            const gltf::Animation& anim{ doc.animations[animIndex] };
            double duration{};
            for (const gltf::Channel& ch : anim.channels) {
                if (!ch.times.empty()) {
                    duration = std::max(duration, (double)ch.times.back());
                }
            }
            size_t numframes{ std::max<size_t>(1, (size_t)std::llround(duration * framerate)) };
            if (numframes > MAX_FRAMES) {
                Fail(std::format("{} frames, at most {}", numframes + 1, MAX_FRAMES + 1));
                return;
            }
            bool longAnim{ numframes >= LONG_ANIM_FRAMES };
            // the anim's bones: the skin's joints, or those of them in the game skeleton
            std::string skeletonPath{ cfg.GetString("skeleton", "") };
            gltf::GameSkeleton game{};
            std::vector<std::string> boneNames{ skel.names };
            std::vector<bool> boneIsRoot(skel.nodes.size());
            std::vector<size_t> boneJoint(skel.nodes.size()), boneGame{};
            for (size_t b = 0; b < skel.nodes.size(); b++) {
                boneIsRoot[b] = skel.parents[b] < 0;
                boneJoint[b] = b;
            }
            std::vector<gltf::Mat4> gameBind{}, gameLocalBind{}, restInv{};
            std::vector<int> gameJoint{};
            if (!skeletonPath.empty()) {
                if (!game.Load(path.parent_path() / skeletonPath, err)) {
                    Fail(std::format("skeleton {}: {}", skeletonPath, err));
                    return;
                }
                gameJoint.assign(game.Count(), -1);
                std::string unmatched{};
                for (size_t j = 0; j < skel.nodes.size(); j++) {
                    int g{ game.Index(skel.names[j]) };
                    if (g < 0) {
                        unmatched += (unmatched.empty() ? "" : ", ") + skel.names[j];
                    } else {
                        gameJoint[g] = (int)j;
                    }
                }
                if (!unmatched.empty()) {
                    LOG_WARNING("{}: joints not in the skeleton, left out: {}", assetName, unmatched);
                }
                boneNames.clear();
                boneIsRoot.clear();
                boneJoint.clear();
                for (size_t g = 0; g < game.Count(); g++) {
                    if (gameJoint[g] >= 0) {
                        boneNames.push_back(game.names[g]);
                        boneIsRoot.push_back(game.parents[g] < 0);
                        boneJoint.push_back((size_t)gameJoint[g]);
                        boneGame.push_back(g);
                    }
                }
                for (size_t g = 0; g < game.Count(); g++) {
                    gameBind.push_back(game.BindMatrix(g, scale));
                    int p{ game.parents[g] };
                    gameLocalBind.push_back(p < 0 ? gameBind[g] : gameBind[p].Inverse() * gameBind[g]);
                }
                for (size_t j = 0; j < skel.nodes.size(); j++) {
                    restInv.push_back(skel.restWorld[j].Inverse());
                }
            }
            size_t numBones{ boneNames.size() };
            if (!numBones || numBones > 0xFF) {
                Fail("1 to 255 bones");
                return;
            }

            // the bone frames at each frame
            std::vector<std::vector<std::array<double, 4>>> quats(numframes + 1);
            std::vector<std::vector<std::array<double, 3>>> trans(numframes + 1);
            for (size_t f = 0; f <= numframes; f++) {
                double t{ std::min(duration, f / framerate) };
                std::vector<gltf::Node> nodes{ doc.nodes };
                for (const gltf::Channel& ch : anim.channels) {
                    if (ch.node < 0 || ch.node >= (int)nodes.size()) {
                        continue;
                    }
                    gltf::Node& n{ nodes[ch.node] };
                    switch (ch.path) {
                    case gltf::PATH_TRANSLATION: Sample(ch, t, 3, n.t); break;
                    case gltf::PATH_ROTATION: Sample(ch, t, 4, n.r); break;
                    case gltf::PATH_SCALE: Sample(ch, t, 3, n.s); break;
                    default: break;
                    }
                }
                std::vector<gltf::Mat4> locals(nodes.size());
                for (size_t i = 0; i < nodes.size(); i++) {
                    locals[i] = gltf::Mat4::FromTRS(nodes[i].t, nodes[i].r, nodes[i].s);
                }
                std::vector<gltf::Mat4> world{ doc.WorldMatrices(locals) };
                if (game.Count()) {
                    // the game bones' frames: a joint's move from its rest on the bind frame, the others on the parent
                    std::vector<gltf::Mat4> frames(game.Count());
                    for (size_t g = 0; g < game.Count(); g++) {
                        int j{ gameJoint[g] }, p{ game.parents[g] };
                        frames[g] = j >= 0 ? world[skel.nodes[j]] * restInv[j] * gameBind[g] :
                                    p < 0 ? gameBind[g] : frames[p] * gameLocalBind[g];
                    }
                    quats[f].resize(numBones);
                    trans[f].resize(numBones);
                    for (size_t b = 0; b < numBones; b++) {
                        size_t g{ boneGame[b] };
                        int p{ game.parents[g] };
                        gltf::Mat4 local{ p < 0 ? frames[g] : frames[p].Inverse() * frames[g] };
                        skel.ToGame(local, quats[f][b].data(), trans[f][b].data());
                    }
                } else {
                    skel.LocalFrames(world, quats[f], trans[f]);
                }
            }

            // root motion: the root bone's moves from its first frame (translation in that frame, yaw), the bone keeps it
            std::string rootMotion{ cfg.GetString("rootMotion", "") };
            std::vector<std::array<double, 3>> deltaTrans{};
            std::vector<double> deltaYaw{};
            if (!rootMotion.empty()) {
                auto it{ std::find(boneNames.begin(), boneNames.end(), rootMotion) };
                if (it == boneNames.end() || !boneIsRoot[it - boneNames.begin()]) {
                    Fail(std::format("rootMotion {} isn't a root bone", rootMotion));
                    return;
                }
                size_t rb{ (size_t)(it - boneNames.begin()) };
                auto Yaw = [](const std::array<double, 4>& q) {
                    // the direction of x after the rotation
                    double fx{ 1 - 2 * (q[1] * q[1] + q[2] * q[2]) };
                    double fy{ 2 * (q[0] * q[1] + q[2] * q[3]) };
                    return std::atan2(fy, fx);
                };
                double yaw0{ Yaw(quats[0][rb]) };
                double prev{};
                for (size_t f = 0; f <= numframes; f++) {
                    double y{ Yaw(quats[f][rb]) - yaw0 };
                    // continuous
                    while (f && y - prev > std::numbers::pi) y -= 2 * std::numbers::pi;
                    while (f && y - prev < -std::numbers::pi) y += 2 * std::numbers::pi;
                    prev = y;
                    deltaYaw.push_back(y);
                    double d[3]{ trans[f][rb][0] - trans[0][rb][0], trans[f][rb][1] - trans[0][rb][1], trans[f][rb][2] - trans[0][rb][2] };
                    double c{ std::cos(-yaw0) }, sn{ std::sin(-yaw0) };
                    deltaTrans.push_back({ d[0] * c - d[1] * sn, d[0] * sn + d[1] * c, d[2] });
                }
                for (size_t f = 1; f <= numframes; f++) {
                    quats[f][rb] = quats[0][rb];
                    trans[f][rb] = trans[0][rb];
                }
            }

            // the keys of each bone in one hemisphere, static or animated
            std::vector<bool> rotAnimated(numBones), transAnimated(numBones);
            for (size_t b = 0; b < numBones; b++) {
                for (size_t f = 1; f <= numframes; f++) {
                    auto& q{ quats[f][b] };
                    const auto& p{ quats[f - 1][b] };
                    if (q[0] * p[0] + q[1] * p[1] + q[2] * p[2] + q[3] * p[3] < 0) {
                        for (double& c : q) {
                            c = -c;
                        }
                    }
                    for (int i = 0; i < 4; i++) {
                        if (std::abs(q[i] - quats[0][b][i]) > STATIC_EPSILON) {
                            rotAnimated[b] = true;
                        }
                    }
                    for (int i = 0; i < 3; i++) {
                        if (std::abs(trans[f][b][i] - trans[0][b][i]) > TRANS_STATIC_EPSILON) {
                            transAnimated[b] = true;
                        }
                    }
                }
            }
            // the names in rotation category order: the animated ones then the static ones
            std::vector<size_t> order{};
            for (size_t b = 0; b < numBones; b++) {
                if (rotAnimated[b]) {
                    order.push_back(b);
                }
            }
            size_t numRotAnimated{ order.size() };
            for (size_t b = 0; b < numBones; b++) {
                if (!rotAnimated[b]) {
                    order.push_back(b);
                }
            }
            std::vector<size_t> nameIndex(numBones);
            for (size_t i = 0; i < order.size(); i++) {
                nameIndex[order[i]] = i;
            }

            uint16_t boneCount[10]{};
            std::vector<byte> dataByte{}, dataShort{}, dataInt{}, randomDataShort{};
            // a key per frame: its count - 1, then its frame indices
            auto PutKeys = [&]() {
                Put<uint16_t>(dataShort, (uint16_t)numframes);
                for (size_t f = 0; f <= numframes; f++) {
                    if (longAnim) {
                        Put<uint16_t>(dataShort, (uint16_t)f);
                    } else {
                        dataByte.push_back((byte)f);
                    }
                }
            };
            for (size_t i = 0; i < numRotAnimated; i++) {
                size_t b{ order[i] };
                PutKeys();
                for (size_t f = 0; f <= numframes; f++) {
                    Put<uint64_t>(randomDataShort, PackQuat(quats[f][b].data()));
                }
            }
            boneCount[CAT_ROT_ANIMATED] = (uint16_t)numRotAnimated;
            for (size_t i = numRotAnimated; i < numBones; i++) {
                Put<uint64_t>(dataShort, PackQuat(quats[0][order[i]].data()));
            }
            boneCount[CAT_ROT_STATIC] = (uint16_t)(numBones - numRotAnimated);
            std::vector<byte> transKeys{};
            for (size_t i = 0; i < numBones; i++) {
                size_t b{ order[i] };
                if (!transAnimated[b]) {
                    continue;
                }
                double mn[3]{ 1e300, 1e300, 1e300 }, mx[3]{ -1e300, -1e300, -1e300 };
                for (size_t f = 0; f <= numframes; f++) {
                    for (int c = 0; c < 3; c++) {
                        mn[c] = std::min(mn[c], trans[f][b][c]);
                        mx[c] = std::max(mx[c], trans[f][b][c]);
                    }
                }
                float size[3];
                for (int c = 0; c < 3; c++) {
                    size[c] = (float)((mx[c] - mn[c]) / 65535.0);
                    Put<float>(dataInt, (float)mn[c]);
                }
                for (int c = 0; c < 3; c++) {
                    Put<float>(dataInt, size[c]);
                }
                dataByte.push_back((byte)nameIndex[b]);
                PutKeys();
                for (size_t f = 0; f <= numframes; f++) {
                    for (int c = 0; c < 3; c++) {
                        double v{ size[c] > 0 ? (trans[f][b][c] - (float)mn[c]) / size[c] : 0 };
                        Put<uint16_t>(transKeys, (uint16_t)std::clamp<long long>(std::llround(v), 0, 0xFFFF));
                    }
                }
                boneCount[CAT_TRANS_SHORT]++;
            }
            randomDataShort.insert(randomDataShort.end(), transKeys.begin(), transKeys.end());
            for (size_t i = 0; i < numBones; i++) {
                size_t b{ order[i] };
                if (!transAnimated[b]) {
                    dataByte.push_back((byte)nameIndex[b]);
                    for (int c = 0; c < 3; c++) {
                        Put<float>(dataInt, (float)trans[0][b][c]);
                    }
                    boneCount[CAT_TRANS_STATIC]++;
                }
            }
            boneCount[CAT_TOTAL] = (uint16_t)numBones;

            /*
             * The notifies (0x28): XHash at 0 (the sound, rumble or fx), ScrStrings: the name at 0x10, the tag at 0x14,
             * param2 at 0x18, the kind at 0x1C, the time 0..1 at 0x20. "end" last of the notifies.
             */
            struct Notify {
                std::string name{};
                std::string tag{};
                std::string param2{};
                std::string kind{ "self notify" };
                std::string asset{};
                float time{};
            };
            auto ReadNotifies = [&](const char* key, std::vector<Notify>& out) -> bool {
                auto it{ root.FindMember(key) };
                if (it == root.MemberEnd() || !it->value.IsArray()) {
                    return true;
                }
                for (const rapidjson::Value& n : it->value.GetArray()) {
                    if (!n.IsObject()) {
                        continue;
                    }
                    auto Str = [&n](const char* k) {
                        auto m{ n.FindMember(k) };
                        return m != n.MemberEnd() && m->value.IsString() ? std::string{ m->value.GetString() } : std::string{};
                    };
                    Notify& no{ out.emplace_back() };
                    no.name = Str("name");
                    no.tag = Str("tag");
                    no.param2 = Str("param2");
                    if (std::string v{ Str("sound") }; !v.empty()) {
                        no.kind = "sound";
                        no.asset = v;
                    } else if (std::string v{ Str("rumble") }; !v.empty()) {
                        no.kind = "rumble";
                        no.asset = v;
                    } else if (std::string v{ Str("fx") }; !v.empty()) {
                        no.kind = "play fx";
                        no.asset = v;
                    }
                    if (std::string v{ Str("kind") }; !v.empty()) {
                        no.kind = v;
                    }
                    if (std::string v{ Str("asset") }; !v.empty()) {
                        no.asset = v;
                    }
                    auto t{ n.FindMember("time") };
                    no.time = t != n.MemberEnd() && t->value.IsNumber() ? (float)std::clamp(t->value.GetDouble(), 0.0, 1.0) : 0.0f;
                    if (no.kind == "self notify" && no.name.empty()) {
                        Fail(std::format("{}: a notify without a name", key));
                        return false;
                    }
                }
                std::stable_sort(out.begin(), out.end(), [](const Notify& a, const Notify& b) { return a.time < b.time; });
                return out.size() <= 0xFF || !std::strcmp(key, "notifies");
            };
            std::vector<Notify> notifies{}, startupNotifies{}, shutdownNotifies{};
            if (!ReadNotifies("notifies", notifies) || !ReadNotifies("startupNotifies", startupNotifies) ||
                !ReadNotifies("shutdownNotifies", shutdownNotifies)) {
                Fail("bad notifies (at most 255 startup / shutdown)");
                return;
            }
            Notify end{};
            end.name = "end";
            end.time = 1.0f;
            notifies.push_back(end);

            // XAnimParts, then in the loader's order: names, notifies, the data
            ff.data.PushStream(XFILE_BLOCK_TEMP);
            ff.data.Align(8);
            byte* parts{ ff.data.AllocStreamPtr<byte>(XANIM_PARTS_SIZE) };
            std::memset(parts, 0, XANIM_PARTS_SIZE);
            auto At = [parts](size_t off) -> byte* { return parts + off; };
            ((XHash*)At(0x70))->name = ctx.HashXHash(assetName, true);
            ctx.AddScrString("body", *(ScrString_t*)At(0xA8), ff);
            *(float*)At(0xC0) = (float)framerate;
            *(float*)At(0xC4) = (float)(framerate / numframes);
            *(uint32_t*)At(0xDC) = (uint32_t)(randomDataShort.size() / 2);
            *(uint32_t*)At(0xE4) = (uint32_t)dataByte.size();
            *(uint32_t*)At(0xE8) = (uint32_t)(dataInt.size() / 4);
            *(uint32_t*)At(0xEC) = (uint32_t)(dataShort.size() / 2);
            std::memcpy(At(0xFC), boneCount, sizeof(boneCount));
            *(uint16_t*)At(0x110) = (uint16_t)notifies.size();
            *At(0x125) = (byte)startupNotifies.size();
            *At(0x126) = (byte)shutdownNotifies.size();
            *At(0x118) = deltaTrans.empty() ? 0 : 1;
            *(uint16_t*)At(0x112) = (uint16_t)numBones;
            *(uint16_t*)At(0x114) = (uint16_t)numframes;
            *At(0x11E) = loop ? 1 : 0;
            *At(0x120) = 1;

            ff.data.PushStream(XFILE_BLOCK_VIRTUAL);
            *(void**)At(0x08) = (void*)fastfile::linker::memory::POINTER_NEXT;
            ff.data.Align(4);
            ScrString_t* names{ ff.data.AllocStreamPtr<ScrString_t>(numBones) };
            for (size_t i = 0; i < numBones; i++) {
                ctx.AddScrString(boneNames[order[i]], names[i], ff);
            }
            // notifies, then the shutdown and startup ones (Load order: 0x38, 0x48, 0x40)
            auto WriteNotifies = [&](size_t ptrOff, const std::vector<Notify>& list) {
                if (list.empty()) {
                    return;
                }
                *(void**)At(ptrOff) = (void*)fastfile::linker::memory::POINTER_NEXT;
                ff.data.Align(8);
                byte* notes{ ff.data.AllocStreamPtr<byte>(NOTIFY_SIZE * list.size()) };
                std::memset(notes, 0, NOTIFY_SIZE * list.size());
                for (size_t i = 0; i < list.size(); i++) {
                    byte* n{ notes + NOTIFY_SIZE * i };
                    if (!list[i].asset.empty()) {
                        ((XHash*)n)->name = ctx.HashXHash(list[i].asset[0] == '#' ? list[i].asset.substr(1) : list[i].asset);
                    }
                    ctx.AddScrString(list[i].name, *(ScrString_t*)(n + 0x10), ff);
                    ctx.AddScrString(list[i].tag, *(ScrString_t*)(n + 0x14), ff);
                    ctx.AddScrString(list[i].param2, *(ScrString_t*)(n + 0x18), ff);
                    ctx.AddScrString(list[i].kind, *(ScrString_t*)(n + 0x1C), ff);
                    *(float*)(n + 0x20) = list[i].time;
                }
            };
            WriteNotifies(0x38, notifies);
            WriteNotifies(0x48, shutdownNotifies);
            WriteNotifies(0x40, startupNotifies);

            // the delta part: trans then quat2, a key per frame
            if (!deltaTrans.empty()) {
                *(void**)At(0x18) = (void*)fastfile::linker::memory::POINTER_NEXT;
                ff.data.Align(8);
                void** dp{ ff.data.AllocStreamPtr<void*>(3) };
                dp[0] = dp[1] = dp[2] = nullptr;
                auto PutIndices = [&](std::vector<byte>& out) {
                    for (size_t f = 0; f <= numframes; f++) {
                        if (longAnim) {
                            Put<uint16_t>(out, (uint16_t)f);
                        } else {
                            out.push_back((byte)f);
                        }
                    }
                };
                // trans: header, mins, size, frames, the indices, then the keys (3 u16)
                dp[0] = (void*)fastfile::linker::memory::POINTER_NEXT;
                double mn[3]{ 1e300, 1e300, 1e300 }, mx[3]{ -1e300, -1e300, -1e300 };
                for (const auto& d : deltaTrans) {
                    for (int c = 0; c < 3; c++) {
                        mn[c] = std::min(mn[c], d[c]);
                        mx[c] = std::max(mx[c], d[c]);
                    }
                }
                std::vector<byte> tr{};
                Put<uint16_t>(tr, (uint16_t)numframes);
                tr.resize(8);
                float size[3];
                for (int c = 0; c < 3; c++) {
                    Put<float>(tr, (float)mn[c]);
                }
                for (int c = 0; c < 3; c++) {
                    size[c] = (float)((mx[c] - mn[c]) / 65535.0);
                    Put<float>(tr, size[c]);
                }
                size_t framesPtr{ tr.size() };
                Put<uint64_t>(tr, 0);
                PutIndices(tr);
                ff.data.Align(8);
                fastfile::linker::memory::LinkerDataChunk* trChunk{};
                byte* trData{ ff.data.AllocStreamPtr<byte>(tr.size(), &trChunk) };
                std::memcpy(trData, tr.data(), tr.size());
                *(void**)(trData + framesPtr) = (void*)fastfile::linker::memory::POINTER_NEXT;
                std::vector<byte> trKeys{};
                for (const auto& d : deltaTrans) {
                    for (int c = 0; c < 3; c++) {
                        double v{ size[c] > 0 ? (d[c] - (float)mn[c]) / size[c] : 0 };
                        Put<uint16_t>(trKeys, (uint16_t)std::clamp<long long>(std::llround(v), 0, 0xFFFF));
                    }
                }
                ff.data.Align(2);
                ff.data.WriteStream(trKeys.data(), trKeys.size());

                // quat2: header, frames, the indices, then the keys
                dp[1] = (void*)fastfile::linker::memory::POINTER_NEXT;
                std::vector<byte> q2{};
                Put<uint16_t>(q2, (uint16_t)numframes);
                q2.resize(8);
                Put<uint64_t>(q2, 0);
                PutIndices(q2);
                ff.data.Align(8);
                byte* q2Data{ ff.data.AllocStreamPtr<byte>(q2.size()) };
                std::memcpy(q2Data, q2.data(), q2.size());
                *(void**)(q2Data + 8) = (void*)fastfile::linker::memory::POINTER_NEXT;
                std::vector<byte> q2Keys{};
                for (double y : deltaYaw) {
                    float z{ (float)std::sin(y / 2) };
                    double w{ std::cos(y / 2) };
                    uint32_t v;
                    std::memcpy(&v, &z, sizeof(v));
                    if (w < 0) {
                        v |= QUAT2_W_NEGATIVE;
                    }
                    Put<uint32_t>(q2Keys, v);
                }
                ff.data.Align(4);
                ff.data.WriteStream(q2Keys.data(), q2Keys.size());
            }
            auto Stream = [&](size_t ptrOff, const std::vector<byte>& data, size_t align) {
                if (data.empty()) {
                    return;
                }
                *(void**)At(ptrOff) = (void*)fastfile::linker::memory::POINTER_NEXT;
                ff.data.Align(align);
                ff.data.WriteStream(data.data(), data.size());
            };
            // DB_PushStreamPos(XFILE_BLOCK_MEMMAPPED) keeps the current block on pc
            Stream(0x58, dataByte, 1);
            Stream(0x90, dataShort, 2);
            Stream(0x80, dataInt, 4);
            Stream(0x98, randomDataShort, 2);
            ff.data.PopStream();

            ff.data.PopStream();
            LOG_INFO(
                "Added asset xanim {} (hash_{:x}), {} bone(s), {} frame(s) at {} fps, {} animated rotation(s), {} animated translation(s), {} notify(s){}{}",
                assetName, ((XHash*)At(0x70))->name, numBones, numframes + 1, framerate, numRotAnimated,
                boneCount[CAT_TRANS_SHORT], notifies.size() + startupNotifies.size() + shutdownNotifies.size(),
                deltaTrans.empty() ? "" : ", root motion", loop ? ", looping" : ""
            );
        }
    };

    utils::MapAdder<XAssetLinkerImpl, XAssetType, XAssetLinker> impl{ GetWorkers(), XAssetType::ASSET_TYPE_XANIM };
} // namespace
