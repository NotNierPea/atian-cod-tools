#include <includes.hpp>
#include <tools/fastfile/linkers/linker_bo4.hpp>

/*
 * XCams, the cameras of scenes (zone line: xcam,xcams/my_cam.json). The json is the bo4 xcam unlinker's, so a dumped game
 * xcam links again as it is:
 *
 * {
 *     "name": "c_my_cam",                    // the asset name, the file name by default
 *     "parentScene": "#hash_..",             // none by default
 *     "hideHud": true, "looping": false, "hideLocalPlayerModel": false, "useFirstPersonPlayerModel": false,
 *     "disableNearDof": false, "autoMotionBlur": true, "easeAnimationsOut": false,
 *     "frames": 111,                         // the length, by default the most frames of a camera
 *     "frameRate": 30,
 *     "rotationOffsetInv": "0 0 0 1",        // a quat (x y z w) the frames are turned by, the stock ones identity
 *     "offset": "0 0 72",                    // not used by the camera
 *     "rightStickRotateOffset": "0 0 0", "rightStickRotateMaxDegrees": "0 0", "freeCursorParallax": 0,
 *     "targetModel": { "name": "tag", "frames": [ { "offset": "x y z", "quat": "x y z w" }, ... ] }, // none by default
 *     "cameras": [
 *         {
 *             "name": "cam1", "index": 0, "apertureType": 1, "farZ": 3937007.8, "unk1c": 1048576000,
 *             // a frame for each frame (game axes: forward x, left y, up z, from the origin and angles the scene plays at)
 *             "frames": [ { "frame": 0, "origin": "x y z", "quat": "x y z w", "fov": 50, "focalLength": 40,
 *                           "focalDistance": 100, "fStop": 18 }, ... ]
 *         }
 *     ],
 *     "cameraSwitches": [ [ frame, camera1, camera2, dissolve ], ... ], // a switch a frame or none
 *     "frameNames": { "name": frame, ... },
 *     "notifies": [ { "asset": "#hash_..", "name": "", "tag": "", "param2": "", "kind": "play exploder", "time": 0 } ],
 *     "startupNotifies": [ ... ], "shutdownNotifies": [ ... ]
 * }
 *
 * A vector is "x y z" as the unlinker writes it or [x, y, z].
 */
namespace {
    using namespace fastfile::linker::bo4;

    constexpr size_t XCAM_SIZE = 0xC0;
    constexpr size_t TARGET_SIZE = 0x10;
    constexpr size_t TARGET_FRAME_SIZE = 0x1C;
    constexpr size_t CAMERA_SIZE = 0x20;
    constexpr size_t FRAME_SIZE = 0x30;
    constexpr size_t SWITCH_SIZE = 0x10;
    constexpr size_t FRAME_NAME_SIZE = 0x8;
    constexpr size_t NOTIFY_SIZE = 0x28;

    const rapidjson::Value* Member(const rapidjson::Value& obj, const char* name) {
        if (!obj.IsObject()) {
            return nullptr;
        }
        auto it{ obj.FindMember(name) };
        return it == obj.MemberEnd() ? nullptr : &it->value;
    }

    double Number(const rapidjson::Value& obj, const char* name, double def) {
        const rapidjson::Value* v{ Member(obj, name) };
        if (!v) {
            return def;
        }
        if (v->IsBool()) {
            return v->GetBool() ? 1 : 0;
        }
        return v->IsNumber() ? v->GetDouble() : def;
    }

    uint32_t Uint(const rapidjson::Value& obj, const char* name, uint32_t def) {
        const rapidjson::Value* v{ Member(obj, name) };
        if (!v) {
            return def;
        }
        if (v->IsUint64() || v->IsInt64()) {
            return (uint32_t)v->GetInt64();
        }
        if (v->IsNumber()) {
            return (uint32_t)(int64_t)v->GetDouble();
        }
        if (v->IsString()) {
            return (uint32_t)std::strtoull(v->GetString(), nullptr, 0);
        }
        return def;
    }

    // n floats of "x y z" or [x, y, z], the rest def
    void Vec(const rapidjson::Value& obj, const char* name, float* out, size_t n, const float* def = nullptr) {
        for (size_t i = 0; i < n; i++) {
            out[i] = def ? def[i] : 0;
        }
        const rapidjson::Value* v{ Member(obj, name) };
        if (!v) {
            return;
        }
        if (v->IsArray()) {
            for (rapidjson::SizeType i = 0; i < n && i < v->Size(); i++) {
                out[i] = (*v)[i].IsNumber() ? (float)(*v)[i].GetDouble() : 0;
            }
        } else if (v->IsString()) {
            const char* s{ v->GetString() };
            for (size_t i = 0; i < n; i++) {
                while (*s == ' ' || *s == '(' || *s == ',') {
                    s++;
                }
                char* end{};
                float f{ std::strtof(s, &end) };
                if (end == s) {
                    break;
                }
                out[i] = f;
                s = end;
            }
        }
    }

    const char* String(const rapidjson::Value& obj, const char* name) {
        const rapidjson::Value* v{ Member(obj, name) };
        return v && v->IsString() ? v->GetString() : nullptr;
    }

    template<typename T>
    T& At(byte* base, size_t off) {
        return *(T*)(base + off);
    }

    class XAssetLinkerImpl : public XAssetLinker {
      public:
        using XAssetLinker::XAssetLinker;

        void Compute(BO4LinkContext& ctx, const char* id, BO4FFContext& ff) override {
            std::filesystem::path path{ ctx.linkCtx.input / id };
            std::filesystem::path rfpath{ path.filename() };
            rfpath.replace_extension();

            std::string buffer{};
            if (!utils::ReadFile(path, buffer)) {
                LOG_ERROR("Can't read xcam {}", path.string());
                ctx.error = true;
                return;
            }
            rapidjson::Document root{};
            root.Parse(buffer.data());
            if (root.HasParseError() || !root.IsObject()) {
                LOG_ERROR("Can't parse xcam {}", path.string());
                ctx.error = true;
                return;
            }
            auto Fail = [&ctx, &path](const std::string& what) {
                LOG_ERROR("Invalid xcam {}: {}", path.string(), what);
                ctx.error = true;
            };
            // a name of the game's ("#hash_..", a dumped one) or one to hash
            auto Hash = [&ctx](const char* val) -> uint64_t {
                if (*val == '#' || *val == ',') {
                    val++;
                }
                if (!std::strncmp(val, "hash_", 5)) {
                    return std::strtoull(val + 5, nullptr, 16) & 0x7FFFFFFFFFFFFFFFull;
                }
                return ctx.HashXHash(val, true);
            };
            // a missing key is none, "" a real empty string (as the game's)
            auto Scr = [&](const rapidjson::Value& obj, const char* name, ScrString_t& out) {
                const char* s{ String(obj, name) };
                out = s ? (ScrString_t)ff.data.AddScrStringKeepEmpty(s) : 0;
            };

            const rapidjson::Value* cameras{ Member(root, "cameras") };
            if (!cameras || !cameras->IsArray() || !cameras->Size()) {
                Fail("no cameras");
                return;
            }
            int32_t maxFrames{};
            for (const rapidjson::Value& cam : cameras->GetArray()) {
                const rapidjson::Value* frames{ Member(cam, "frames") };
                if (!frames || !frames->IsArray()) {
                    Fail("a camera without frames");
                    return;
                }
                maxFrames = std::max<int32_t>(maxFrames, (int32_t)frames->Size());
            }
            const rapidjson::Value* targetModel{ Member(root, "targetModel") };
            const rapidjson::Value* switches{ Member(root, "cameraSwitches") };
            const rapidjson::Value* frameNames{ Member(root, "frameNames") };
            int32_t frames{ (int32_t)Number(root, "frames", maxFrames) };
            if (switches && switches->IsArray() && switches->Size() && (int32_t)switches->Size() != frames) {
                Fail(std::format("{} camera switches for {} frames, a switch a frame", switches->Size(), frames));
                return;
            }
            const char* notifyKeys[]{ "notifies", "startupNotifies", "shutdownNotifies" };
            for (const char* key : notifyKeys) {
                const rapidjson::Value* list{ Member(root, key) };
                if (list && (!list->IsArray() || list->Size() > 0xFF)) {
                    Fail(std::format("{}: at most 255", key));
                    return;
                }
            }

            // Load_XCam (sub_142E36290): the header, then the target model, the cameras and their frames, the switches,
            // the frame names and the notifies
            ff.data.PushStream(XFILE_BLOCK_TEMP);
            ff.data.Align(8);
            byte* x{ ff.data.AllocStreamPtr<byte>(XCAM_SIZE) };
            std::memset(x, 0, XCAM_SIZE);
            std::string assetName{ String(root, "name") ? String(root, "name") : rfpath.string() };
            At<uint64_t>(x, 0) = Hash(assetName.data());
            if (const char* parent{ String(root, "parentScene") }) {
                At<uint64_t>(x, 0x10) = Hash(parent);
            }
            const char* flags[]{ "hideHud", "looping", "hideLocalPlayerModel", "useFirstPersonPlayerModel",
                                 "disableNearDof", "autoMotionBlur", "easeAnimationsOut" };
            const bool flagDefaults[]{ true, false, false, false, false, true, false };
            for (size_t i = 0; i < std::size(flags); i++) {
                x[0x20 + i] = Number(root, flags[i], flagDefaults[i] ? 1 : 0) != 0;
            }
            x[0x27] = (byte)Uint(root, "unk27", 0);
            At<int32_t>(x, 0x28) = (int32_t)cameras->Size();
            At<int32_t>(x, 0x2C) = frames;
            At<int32_t>(x, 0x30) = (int32_t)Number(root, "frameRate", 30);
            At<int32_t>(x, 0x34) = frameNames && frameNames->IsObject() ? (int32_t)frameNames->MemberCount() : 0;
            const float identity[4]{ 0, 0, 0, 1 };
            Vec(root, "rotationOffsetInv", &At<float>(x, 0x38), 4, identity);
            Vec(root, "offset", &At<float>(x, 0x48), 3);
            Vec(root, "rightStickRotateOffset", &At<float>(x, 0xA4), 3);
            Vec(root, "rightStickRotateMaxDegrees", &At<float>(x, 0xB0), 2);
            At<float>(x, 0xB8) = (float)Number(root, "freeCursorParallax", 0);

            ff.data.PushStream(XFILE_BLOCK_VIRTUAL);

            // the target model: name, frames, its frames (offset, quat)
            if (targetModel && targetModel->IsObject()) {
                const rapidjson::Value* tframes{ Member(*targetModel, "frames") };
                size_t n{ tframes && tframes->IsArray() ? tframes->Size() : 0 };
                At<uintptr_t>(x, 0x58) = fastfile::linker::memory::POINTER_NEXT;
                ff.data.Align(8);
                byte* tm{ ff.data.AllocStreamPtr<byte>(TARGET_SIZE) };
                std::memset(tm, 0, TARGET_SIZE);
                Scr(*targetModel, "name", At<ScrString_t>(tm, 0));
                At<int32_t>(tm, 4) = (int32_t)n;
                if (n) {
                    At<uintptr_t>(tm, 8) = fastfile::linker::memory::POINTER_NEXT;
                    ff.data.Align(4);
                    byte* tf{ ff.data.AllocStreamPtr<byte>(TARGET_FRAME_SIZE * n) };
                    for (rapidjson::SizeType i = 0; i < n; i++) {
                        Vec((*tframes)[i], "offset", &At<float>(tf + TARGET_FRAME_SIZE * i, 0), 3);
                        Vec((*tframes)[i], "quat", &At<float>(tf + TARGET_FRAME_SIZE * i, 0xC), 4, identity);
                    }
                }
            }

            // the cameras, then each one's frames
            At<uintptr_t>(x, 0x60) = fastfile::linker::memory::POINTER_NEXT;
            ff.data.Align(8);
            byte* cams{ ff.data.AllocStreamPtr<byte>(CAMERA_SIZE * cameras->Size()) };
            std::memset(cams, 0, CAMERA_SIZE * cameras->Size());
            for (rapidjson::SizeType i = 0; i < cameras->Size(); i++) {
                const rapidjson::Value& cam{ (*cameras)[i] };
                byte* c{ cams + CAMERA_SIZE * i };
                Scr(cam, "name", At<ScrString_t>(c, 0));
                At<int32_t>(c, 4) = (int32_t)Number(cam, "index", i);
                At<int32_t>(c, 8) = (int32_t)Member(cam, "frames")->Size();
                At<int32_t>(c, 0xC) = (int32_t)Number(cam, "apertureType", 1);
                At<float>(c, 0x18) = (float)Number(cam, "farZ", 3937007.8);
                At<uint32_t>(c, 0x1C) = Uint(cam, "unk1c", 0x3E800000); // 0.25f on the stock ones
            }
            for (rapidjson::SizeType i = 0; i < cameras->Size(); i++) {
                const rapidjson::Value& camFrames{ *Member((*cameras)[i], "frames") };
                if (!camFrames.Size()) {
                    continue;
                }
                At<uintptr_t>(cams + CAMERA_SIZE * i, 0x10) = fastfile::linker::memory::POINTER_NEXT;
                ff.data.Align(4);
                byte* fr{ ff.data.AllocStreamPtr<byte>(FRAME_SIZE * camFrames.Size()) };
                for (rapidjson::SizeType k = 0; k < camFrames.Size(); k++) {
                    const rapidjson::Value& f{ camFrames[k] };
                    byte* o{ fr + FRAME_SIZE * k };
                    At<int32_t>(o, 0) = (int32_t)Number(f, "frame", k);
                    Vec(f, "origin", &At<float>(o, 4), 3);
                    Vec(f, "quat", &At<float>(o, 0x10), 4, identity);
                    At<float>(o, 0x20) = (float)Number(f, "fov", 50);
                    At<float>(o, 0x24) = (float)Number(f, "focalLength", 40);
                    At<float>(o, 0x28) = (float)Number(f, "focalDistance", 100);
                    At<float>(o, 0x2C) = (float)Number(f, "fStop", 18);
                }
            }

            // the switches: frameNum, camera1, camera2, dissolve
            if (switches && switches->IsArray() && switches->Size()) {
                At<uintptr_t>(x, 0x68) = fastfile::linker::memory::POINTER_NEXT;
                ff.data.Align(4);
                byte* sw{ ff.data.AllocStreamPtr<byte>(SWITCH_SIZE * switches->Size()) };
                for (rapidjson::SizeType i = 0; i < switches->Size(); i++) {
                    const rapidjson::Value& s{ (*switches)[i] };
                    byte* o{ sw + SWITCH_SIZE * i };
                    auto Item = [&s](rapidjson::SizeType k) {
                        return s.IsArray() && k < s.Size() && s[k].IsNumber() ? s[k].GetDouble() : 0;
                    };
                    At<int32_t>(o, 0) = s.IsArray() && s.Size() ? (int32_t)Item(0) : (int32_t)i;
                    At<int32_t>(o, 4) = (int32_t)Item(1);
                    At<int32_t>(o, 8) = (int32_t)Item(2);
                    At<float>(o, 0xC) = (float)Item(3);
                }
            }

            // the frame names: name, frameNum
            if (At<int32_t>(x, 0x34)) {
                At<uintptr_t>(x, 0x70) = fastfile::linker::memory::POINTER_NEXT;
                ff.data.Align(4);
                byte* fn{ ff.data.AllocStreamPtr<byte>(FRAME_NAME_SIZE * frameNames->MemberCount()) };
                size_t i{};
                for (auto it{ frameNames->MemberBegin() }; it != frameNames->MemberEnd(); ++it, i++) {
                    At<ScrString_t>(fn + FRAME_NAME_SIZE * i, 0) = 0;
                    ctx.AddScrString(it->name.GetString(), At<ScrString_t>(fn + FRAME_NAME_SIZE * i, 0), ff);
                    At<int32_t>(fn + FRAME_NAME_SIZE * i, 4) = it->value.IsNumber() ? it->value.GetInt() : 0;
                }
            }

            // the notifies, the startup ones, the shutdown ones (pointer, u8 count)
            const size_t notifyOffsets[]{ 0x78, 0x88, 0x98 };
            for (size_t l = 0; l < std::size(notifyKeys); l++) {
                const rapidjson::Value* list{ Member(root, notifyKeys[l]) };
                if (!list || !list->Size()) {
                    continue;
                }
                x[notifyOffsets[l] + 8] = (byte)list->Size();
                At<uintptr_t>(x, notifyOffsets[l]) = fastfile::linker::memory::POINTER_NEXT;
                ff.data.Align(8);
                byte* notes{ ff.data.AllocStreamPtr<byte>(NOTIFY_SIZE * list->Size()) };
                std::memset(notes, 0, NOTIFY_SIZE * list->Size());
                for (rapidjson::SizeType i = 0; i < list->Size(); i++) {
                    const rapidjson::Value& n{ (*list)[i] };
                    byte* o{ notes + NOTIFY_SIZE * i };
                    if (const char* asset{ String(n, "asset") }; asset && *asset) {
                        At<uint64_t>(o, 0) = Hash(asset);
                    }
                    Scr(n, "name", At<ScrString_t>(o, 0x10));
                    Scr(n, "tag", At<ScrString_t>(o, 0x14));
                    Scr(n, "param2", At<ScrString_t>(o, 0x18));
                    Scr(n, "kind", At<ScrString_t>(o, 0x1C));
                    At<float>(o, 0x20) = (float)Number(n, "time", 0);
                }
            }
            ff.data.PopStream();

            ff.data.PopStream();
            LOG_INFO("Added asset xcam {} (hash_{:x}), {} cameras, {} frames", assetName, At<uint64_t>(x, 0), cameras->Size(), frames);
        }
    };

    utils::MapAdder<XAssetLinkerImpl, XAssetType, XAssetLinker> impl{ GetWorkers(), XAssetType::ASSET_TYPE_XCAM };
} // namespace
