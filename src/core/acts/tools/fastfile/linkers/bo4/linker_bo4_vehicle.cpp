#include <includes.hpp>
#include <tools/fastfile/linkers/linker_bo4.hpp>
#include <tools/fastfile/linkers/bo4/linker_bo4_scriptbundle.hpp>
#include <tools/fastfile/bo4_vehicle_slots.hpp>

/*
 * Stock vehicles as the bo4 vehicle dumper writes them ("lossless": true, "header", "slots" by hex offset): the
 * VehicleDef, its effects (VehicleFxDef) and sounds (VehicleSoundDef) as they were, their pointers and script strings
 * written back in the loaders' order (bo4_vehicle_slots.hpp). An asset is linked as the json names it: ",hash_x" for one
 * of another zone (a reference), a path or "#hash_x" for one this zone builds (an effects or sounds def here is built
 * inline under the vehicle), null for none. A file is the id, tables/<folder>/<id>.json or, for a dumper's unnamed one,
 * tables/<folder>/file_<x>.json.
 */
namespace {
    using namespace fastfile::linker::bo4;
    using namespace fastfile::bo4vehicle;

    std::vector<byte> Unhex(const rapidjson::Value* v) {
        std::vector<byte> out{};
        if (!v || !v->IsString()) {
            return out;
        }
        const char* s{ v->GetString() };
        size_t n{ v->GetStringLength() / 2 };
        out.resize(n);
        auto Nib = [](char c) -> byte { return (byte)(c <= '9' ? c - '0' : (c | 0x20) - 'a' + 10); };
        for (size_t i = 0; i < n; i++) {
            out[i] = (byte)(Nib(s[2 * i]) << 4 | Nib(s[2 * i + 1]));
        }
        return out;
    }

    const rapidjson::Value* Member(const rapidjson::Value& v, const char* name) {
        if (!v.IsObject()) {
            return nullptr;
        }
        auto it{ v.FindMember(name) };
        return it == v.MemberEnd() || it->value.IsNull() ? nullptr : &it->value;
    }

    std::filesystem::path FindFile(BO4LinkContext& ctx, const char* folder, const char* id) {
        std::filesystem::path direct{ ctx.linkCtx.input / id };
        if (std::filesystem::is_regular_file(direct)) {
            return direct;
        }
        std::filesystem::path named{ ctx.linkCtx.input / "tables" / folder / std::format("{}.json", id) };
        std::string_view sv{ id };
        if (!std::filesystem::is_regular_file(named) && sv.starts_with("hash_")) {
            std::filesystem::path dumped{ ctx.linkCtx.input / "tables" / folder / std::format("file_{}.json", sv.substr(5)) };
            if (std::filesystem::is_regular_file(dumped)) {
                return dumped;
            }
        }
        return named;
    }

    template<XAssetType Type>
    class XAssetLinkerImpl : public XAssetLinker {
      public:
        const char* folder;
        size_t size;
        std::vector<Slot> slots;

        XAssetLinkerImpl(const char* folder, size_t size, std::vector<Slot>&& slots)
            : folder(folder), size(size), slots(std::move(slots)) {}

        uint32_t Scr(BO4FFContext& ff, const rapidjson::Value* v) {
            return v && v->IsString() ? ff.data.AddScrStringKeepEmpty(v->GetString()) : 0;
        }

        void Write(BO4FFContext& ff, const std::vector<byte>& data, size_t align, size_t len) {
            std::vector<byte> d{ data };
            d.resize(len);
            ff.data.Align(align);
            if (len) {
                ff.data.WriteStream(d.data(), d.size());
            }
        }

        // an asset as the json names it, null for none
        void Ref(BO4LinkContext& ctx, BO4FFContext& ff, uint32_t type, const rapidjson::Value* v, void*& ref) {
            ref = nullptr;
            if (v && v->IsString() && v->GetStringLength()) {
                ctx.LinkAsset((XAssetType)type, v->GetString(), ref, false, &ff);
            }
        }

        void Compute(BO4LinkContext& ctx, const char* id, BO4FFContext& ff) override {
            std::filesystem::path path{ FindFile(ctx, folder, id) };
            BO4Json cfg{ path };
            if (!cfg.SyncConfig(false)) {
                LOG_ERROR("Can't read {} {}", folder, path.string());
                ctx.error = true;
                return;
            }
            const rapidjson::Value& root{ cfg.main };
            const rapidjson::Value* jslots{ Member(root, "slots") };
            std::vector<byte> header{ Unhex(Member(root, "header")) };
            if (!Member(root, "lossless") || !jslots || header.size() != size) {
                LOG_ERROR("{} {}: only the dumper's lossless ones can be linked", folder, path.string());
                ctx.error = true;
                return;
            }
            auto Had = [&header](size_t off) { return *(uint64_t*)&header[off] != 0; };
            auto SlotValue = [jslots](uint32_t off) { return Member(*jslots, utils::va("%x", off)); };

            ff.data.PushStream(XFILE_BLOCK_TEMP);
            byte* a{ ff.data.AllocStreamPtr<byte>(size) };
            std::memcpy(a, header.data(), size);
            auto Next = [&](size_t off) { *(uint64_t*)(a + off) = fastfile::linker::memory::POINTER_NEXT; };

            // the dump process' pointers out, written again as they're linked
            for (const Slot& s : slots) {
                switch (s.kind) {
                case SK_ASSET:
                case SK_BLOB:
                case SK_XSTRING:
                case SK_STRUCT_XSTRING:
                    *(uint64_t*)(a + s.offset) = 0;
                    break;
                case SK_BUNDLE:
                    std::memset(a + s.offset, 0, sizeof(scriptbundle::SB_ObjectsArray));
                    break;
                default:
                    break;
                }
            }

            ff.data.PushStream(XFILE_BLOCK_VIRTUAL);
            for (const Slot& s : slots) {
                const rapidjson::Value* v{ SlotValue(s.offset) };
                byte* d{ a + s.offset };
                switch (s.kind) {
                case SK_ASSET:
                    if (Had(s.offset)) {
                        Ref(ctx, ff, s.type, v, *(void**)d);
                    }
                    break;
                case SK_SCR:
                    *(uint32_t*)d = Scr(ff, v);
                    break;
                case SK_BLOB:
                    if (Had(s.offset)) {
                        Next(s.offset);
                        Write(ff, Unhex(v), s.align, (size_t)SlotCount(s, a) * s.elemSize);
                    }
                    break;
                case SK_XSTRING:
                    if (Had(s.offset)) {
                        ff.data.AddXString(*(const char**)d, v && v->IsString() ? v->GetString() : "");
                    }
                    break;
                case SK_STRUCT_XSTRING:
                    if (Had(s.offset)) {
                        Next(s.offset);
                        std::vector<byte> data{ Unhex(v ? Member(*v, "data") : nullptr) };
                        data.resize(s.elemSize);
                        ff.data.Align(s.align);
                        byte* e{ ff.data.AllocStreamPtr<byte>(s.elemSize) };
                        std::memcpy(e, data.data(), s.elemSize);
                        bool had{ *(uint64_t*)e != 0 };
                        *(uint64_t*)e = 0;
                        const rapidjson::Value* str{ v ? Member(*v, "string") : nullptr };
                        if (had) {
                            ff.data.AddXString(*(const char**)e, str && str->IsString() ? str->GetString() : "");
                        }
                    }
                    break;
                case SK_BUNDLE:
                    if (v && v->IsObject() &&
                        !scriptbundle::WriteArray(ctx, const_cast<rapidjson::Value&>(*v), *(scriptbundle::SB_ObjectsArray*)d, ff)) {
                        LOG_ERROR("{} {}: can't write its bundle", folder, path.string());
                        ctx.error = true;
                        return;
                    }
                    break;
                }
            }
            ff.data.PopStream();
            ff.data.PopStream();
            LOG_INFO("Added asset {} {} (hash_{:x}), lossless", folder, path.filename().string(), *(uint64_t*)a);
        }
    };

    class VehicleDefLinker : public XAssetLinkerImpl<XAssetType::ASSET_TYPE_VEHICLEDEF> {
      public:
        VehicleDefLinker() : XAssetLinkerImpl("vehicle", VEHICLE_DEF_SIZE, VehicleDefSlots()) {}
    };

    class VehicleFxDefLinker : public XAssetLinkerImpl<XAssetType::ASSET_TYPE_VEHICLEFXDEF> {
      public:
        VehicleFxDefLinker() : XAssetLinkerImpl("vehiclefxdef", VEHICLE_FX_DEF_SIZE, VehicleFxDefSlots()) {}
    };

    class VehicleSoundDefLinker : public XAssetLinkerImpl<XAssetType::ASSET_TYPE_VEHICLESOUNDDEF> {
      public:
        VehicleSoundDefLinker() : XAssetLinkerImpl("vehiclesounddef", VEHICLE_SOUND_DEF_SIZE, VehicleSoundDefSlots()) {}
    };

    utils::MapAdder<VehicleDefLinker, XAssetType, XAssetLinker> implVehicle{ GetWorkers(), XAssetType::ASSET_TYPE_VEHICLEDEF };
    utils::MapAdder<VehicleFxDefLinker, XAssetType, XAssetLinker> implFx{ GetWorkers(), XAssetType::ASSET_TYPE_VEHICLEFXDEF };
    utils::MapAdder<VehicleSoundDefLinker, XAssetType, XAssetLinker> implSound{ GetWorkers(), XAssetType::ASSET_TYPE_VEHICLESOUNDDEF };
} // namespace
