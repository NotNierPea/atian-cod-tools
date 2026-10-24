#include <includes.hpp>
#include <tools/fastfile/handlers/handler_game_bo4.hpp>

/*
 * XCams (the cameras of scenes and menus), retail 0xC0: the dev elf's XCam with XHash names (0x10 more from parent_scene
 * on). Loader sub_142E36290 (type 0x54, Load_XAssetHeader). The json is the linker's (linker_bo4_xcam.cpp).
 */
namespace {
    using namespace fastfile::handlers::bo4;

    template<typename T>
    T& At(void* base, size_t off) {
        return *(T*)((byte*)base + off);
    }

    class ImplWorker : public Worker {
        using Worker::Worker;

        void Unlink(fastfile::FastFileOption& opt, void* ptr) {
            byte* x{ (byte*)ptr };
            XHash& name{ At<XHash>(x, 0) };

            std::filesystem::path outFile{ opt.m_output / "bo4" / "graphics" / "xcam" /
                                           std::format("{}.json", hashutils::ExtractTmp("file", name)) };
            std::filesystem::create_directories(outFile.parent_path());
            BO4JsonWriter json{};
            LOG_OPT_INFO("Dump xcam {}", outFile.string());

            auto Scr = [&json](const char* key, ScrString_t s) {
                json.WriteFieldValueScrString(key, s);
            };
            auto Frames = [&json](const char* key, byte* frames, int32_t count) {
                // XCamFrame (0x30): frameNum, origin, angleQuat (x y z w), fov, focalLength, focalDistance, fStop
                json.WriteFieldNameString(key);
                json.BeginArray();
                for (int32_t i = 0; frames && i < count; i++) {
                    byte* f{ frames + 0x30 * i };
                    json.BeginObject();
                    json.WriteFieldValueNumber("frame", At<int32_t>(f, 0));
                    json.WriteFieldValueVector("origin", At<vec3_t>(f, 4));
                    json.WriteFieldValueVector("quat", At<vec4_t>(f, 0x10));
                    json.WriteFieldValueNumber("fov", At<float>(f, 0x20));
                    json.WriteFieldValueNumber("focalLength", At<float>(f, 0x24));
                    json.WriteFieldValueNumber("focalDistance", At<float>(f, 0x28));
                    json.WriteFieldValueNumber("fStop", At<float>(f, 0x2C));
                    json.EndObject();
                }
                json.EndArray();
            };
            auto Notifies = [&](const char* key, byte* list, size_t count) {
                // as the xanims': XHash (sound, rumble, fx) at 0, name 0x10, tag 0x14, param2 0x18, kind 0x1C, time 0x20
                json.WriteFieldNameString(key);
                json.BeginArray();
                for (size_t i = 0; list && i < count; i++) {
                    byte* n{ list + 0x28 * i };
                    json.BeginObject();
                    if (At<uint64_t>(n, 0)) {
                        json.WriteFieldValueXHash("asset", At<XHash>(n, 0));
                    }
                    Scr("name", At<ScrString_t>(n, 0x10));
                    Scr("tag", At<ScrString_t>(n, 0x14));
                    Scr("param2", At<ScrString_t>(n, 0x18));
                    Scr("kind", At<ScrString_t>(n, 0x1C));
                    json.WriteFieldValueNumber("time", At<float>(n, 0x20));
                    json.EndObject();
                }
                json.EndArray();
            };

            json.BeginObject();
            json.WriteFieldValueXHash("name", name);
            if (At<uint64_t>(x, 0x10)) {
                json.WriteFieldValueXHash("parentScene", At<XHash>(x, 0x10));
            }
            json.WriteFieldValueBool("hideHud", x[0x20]);
            json.WriteFieldValueBool("looping", x[0x21]);
            json.WriteFieldValueBool("hideLocalPlayerModel", x[0x22]);
            json.WriteFieldValueBool("useFirstPersonPlayerModel", x[0x23]);
            json.WriteFieldValueBool("disableNearDof", x[0x24]);
            json.WriteFieldValueBool("autoMotionBlur", x[0x25]);
            json.WriteFieldValueBool("easeAnimationsOut", x[0x26]);
            if (x[0x27]) {
                json.WriteFieldValueNumber("unk27", x[0x27]);
            }
            int32_t cameraCount{ At<int32_t>(x, 0x28) };
            int32_t frames{ At<int32_t>(x, 0x2C) };
            json.WriteFieldValueNumber("frames", frames);
            json.WriteFieldValueNumber("frameRate", At<int32_t>(x, 0x30));
            json.WriteFieldValueVector("rotationOffsetInv", At<vec4_t>(x, 0x38));
            json.WriteFieldValueVector("offset", At<vec3_t>(x, 0x48));
            json.WriteFieldValueVector("rightStickRotateOffset", At<vec3_t>(x, 0xA4));
            json.WriteFieldValueVector("rightStickRotateMaxDegrees", At<vec2_t>(x, 0xB0));
            json.WriteFieldValueNumber("freeCursorParallax", At<float>(x, 0xB8));

            // the target model (0x10: name, frames, its frames 0x1C: offset, axis quat)
            if (byte* tm{ At<byte*>(x, 0x58) }) {
                json.WriteFieldNameString("targetModel");
                json.BeginObject();
                Scr("name", At<ScrString_t>(tm, 0));
                json.WriteFieldNameString("frames");
                json.BeginArray();
                byte* tf{ At<byte*>(tm, 8) };
                for (int32_t i = 0; tf && i < At<int32_t>(tm, 4); i++) {
                    json.BeginObject();
                    json.WriteFieldValueVector("offset", At<vec3_t>(tf + 0x1C * i, 0));
                    json.WriteFieldValueVector("quat", At<vec4_t>(tf + 0x1C * i, 0xC));
                    json.EndObject();
                }
                json.EndArray();
                json.EndObject();
            }

            // the cameras (0x20: name, index, frames, apertureType, their frames, farZ)
            json.WriteFieldNameString("cameras");
            json.BeginArray();
            byte* cams{ At<byte*>(x, 0x60) };
            for (int32_t i = 0; cams && i < cameraCount; i++) {
                byte* c{ cams + 0x20 * i };
                json.BeginObject();
                Scr("name", At<ScrString_t>(c, 0));
                json.WriteFieldValueNumber("index", At<int32_t>(c, 4));
                json.WriteFieldValueNumber("apertureType", At<int32_t>(c, 0xC));
                json.WriteFieldValueNumber("farZ", At<float>(c, 0x18));
                if (At<uint32_t>(c, 0x1C)) {
                    json.WriteFieldValueNumber("unk1c", At<uint32_t>(c, 0x1C));
                }
                Frames("frames", At<byte*>(c, 0x10), At<int32_t>(c, 8));
                json.EndObject();
            }
            json.EndArray();

            // a switch a frame (0x10: frameNum, camera1, camera2, dissolve)
            json.WriteFieldNameString("cameraSwitches");
            json.BeginArray();
            byte* sw{ At<byte*>(x, 0x68) };
            for (int32_t i = 0; sw && i < frames; i++) {
                byte* s{ sw + 0x10 * i };
                json.BeginArray();
                json.WriteValueNumber(At<int32_t>(s, 0));
                json.WriteValueNumber(At<int32_t>(s, 4));
                json.WriteValueNumber(At<int32_t>(s, 8));
                json.WriteValueNumber(At<float>(s, 0xC));
                json.EndArray();
            }
            json.EndArray();

            // the named frames (ScrString, frameNum)
            json.WriteFieldNameString("frameNames");
            json.BeginObject();
            byte* fn{ At<byte*>(x, 0x70) };
            for (int32_t i = 0; fn && i < At<int32_t>(x, 0x34); i++) {
                json.WriteFieldNameString(GetScrString(At<ScrString_t>(fn + 8 * i, 0)));
                json.WriteValueNumber(At<int32_t>(fn + 8 * i, 4));
            }
            json.EndObject();

            Notifies("notifies", At<byte*>(x, 0x78), x[0x80]);
            Notifies("startupNotifies", At<byte*>(x, 0x88), x[0x90]);
            Notifies("shutdownNotifies", At<byte*>(x, 0x98), x[0xA0]);
            json.EndObject();

            if (!json.WriteToFile(outFile)) {
                LOG_ERROR("Error when dumping {}", outFile.string());
            }
        }
    };

    utils::MapAdder<ImplWorker, games::bo4::pool::XAssetType, Worker> impl{
        GetWorkers(), games::bo4::pool::XAssetType::ASSET_TYPE_XCAM
    };
} // namespace
