#pragma once
#include <games/bo4/pool.hpp>

/*
 * The vehicle assets' pointers and script strings in their loaders' order (retail Load_XAssetHeader cases 0x34, 0x3C,
 * 0x3D): VehicleDef (0x1A30, sub_142E30C60), VehicleFxDef (0x3E0, inline in sub_142E318C0), VehicleSoundDef (0x1020,
 * sub_142E319D0). The vehicle dumper writes them by offset, the vehicle linkers write them back in this order: assets
 * by name, script strings, the arrays sized by a count of the header. The XHash fields need nothing.
 */
namespace fastfile::bo4vehicle {
    using XAssetType = games::bo4::pool::XAssetType;

    constexpr size_t VEHICLE_DEF_SIZE = 0x1A30;
    constexpr size_t VEHICLE_FX_DEF_SIZE = 0x3E0;
    constexpr size_t VEHICLE_SOUND_DEF_SIZE = 0x1020;

    enum SlotKind : uint8_t {
        SK_ASSET,          // an asset pointer
        SK_SCR,            // a script string
        SK_BLOB,           // a pointer to count (i32 at countOffset) * elemSize bytes
        SK_XSTRING,        // a string pointer
        SK_STRUCT_XSTRING, // a pointer to elemSize bytes, a string pointer at their start
        SK_BUNDLE,         // an inline script bundle object array (SB_ObjectsArray, 0x20)
    };

    struct Slot {
        SlotKind kind;
        uint32_t offset;
        uint32_t type{};
        uint32_t countOffset{};
        uint32_t elemSize{};
        uint32_t align{ 1 };
    };

    class SlotList {
      public:
        std::vector<Slot> slots{};

        void Scr(uint32_t off, uint32_t n = 1, uint32_t step = 4) {
            for (uint32_t i = 0; i < n; i++) {
                slots.push_back({ SK_SCR, off + step * i });
            }
        }
        void Asset(uint32_t off, XAssetType type, uint32_t n = 1, uint32_t step = 8) {
            for (uint32_t i = 0; i < n; i++) {
                slots.push_back({ SK_ASSET, off + step * i, (uint32_t)type });
            }
        }
        void XString(uint32_t off) { slots.push_back({ SK_XSTRING, off }); }
        void Blob(uint32_t off, uint32_t countOff, uint32_t elem, uint32_t align) {
            slots.push_back({ SK_BLOB, off, 0, countOff, elem, align });
        }
        void StructXString(uint32_t off, uint32_t size, uint32_t align) {
            slots.push_back({ SK_STRUCT_XSTRING, off, 0, 0, size, align });
        }
        void Bundle(uint32_t off) { slots.push_back({ SK_BUNDLE, off }); }
    };

    // sub_142E30C60's order
    inline std::vector<Slot> VehicleDefSlots() {
        SlotList s{};
        s.Scr(0x14);
        s.Scr(0x28);
        s.Scr(0x2C);
        s.Scr(0x30);
        s.Scr(0x11C);
        s.Blob(0x140, 0x148, 8, 4);
        s.Scr(0x398 + 0x10, 16, 0x40);
        s.Scr(0x7A0 + 0x30, 16, 0x68);
        s.Asset(0xF38, XAssetType::ASSET_TYPE_SCRIPTBUNDLE, 5, 0x48);
        s.Asset(0x1188, XAssetType::ASSET_TYPE_VEHICLESOUNDDEF);
        s.Asset(0x1190, XAssetType::ASSET_TYPE_VEHICLESOUNDDEF);
        s.XString(0x11D8);
        s.Scr(0x11F4, 4);
        s.Scr(0x1204, 4);
        s.Scr(0x1254);
        s.Scr(0x1258, 4);
        s.Scr(0x1268);
        s.Asset(0x1270, XAssetType::ASSET_TYPE_XMODEL, 4);
        s.Scr(0x1290, 4);
        s.Asset(0x12A0, XAssetType::ASSET_TYPE_XMODEL, 4);
        s.Scr(0x12C0, 4);
        s.Scr(0x12D4, 4);
        s.Scr(0x12FC);
        s.Asset(0x1308, XAssetType::ASSET_TYPE_XMODEL, 4);
        s.Asset(0x1330, XAssetType::ASSET_TYPE_FX);
        s.Scr(0x1338);
        s.Scr(0x133C);
        s.Asset(0x1340, XAssetType::ASSET_TYPE_FX);
        s.Scr(0x1348);
        s.Asset(0x1350, XAssetType::ASSET_TYPE_FX);
        s.Scr(0x1358);
        s.Scr(0x135C);
        s.Asset(0x1360, XAssetType::ASSET_TYPE_VEHICLEFXDEF, 7);
        s.Asset(0x13A0, XAssetType::ASSET_TYPE_FX);
        s.Scr(0x13A8);
        s.Asset(0x13C0, XAssetType::ASSET_TYPE_FX);
        s.Scr(0x13C8);
        s.Asset(0x13E8, XAssetType::ASSET_TYPE_FX);
        s.Scr(0x13F0);
        for (uint32_t off : { 0x1408, 0x1438, 0x1468, 0x1498 }) {
            s.Asset(off, XAssetType::ASSET_TYPE_FX, 4);
            s.Scr(off + 0x20, 4);
        }
        s.XString(0x14D8);
        s.Asset(0x14F0, XAssetType::ASSET_TYPE_RUMBLE);
        s.XString(0x14F8);
        s.Asset(0x1540, XAssetType::ASSET_TYPE_MATERIAL);
        s.Scr(0x1548);
        // the input names (steerAxisName .. steerGraphName)
        for (uint32_t off = 0x1558; off <= 0x1668; off += 0x10) {
            s.XString(off);
        }
        s.Blob(0x1678, 0x1680, 8, 4);
        s.Asset(0x16B0, XAssetType::ASSET_TYPE_RUMBLE);
        s.Asset(0x16C0, XAssetType::ASSET_TYPE_RUMBLE, 2);
        s.Asset(0x1980, XAssetType::ASSET_TYPE_SCRIPTBUNDLE, 7);
        s.Asset(0x19B8, XAssetType::ASSET_TYPE_ANIMSTATEMACHINE);
        s.Asset(0x19C0, XAssetType::ASSET_TYPE_ANIMSELECTORTABLESET);
        s.Asset(0x19C8, XAssetType::ASSET_TYPE_ANIMMAPPINGTABLE);
        s.Asset(0x19D8, XAssetType::ASSET_TYPE_SURFACESOUNDDEF);
        s.Asset(0x19E0, XAssetType::ASSET_TYPE_SURFACEFX_TABLE);
        s.Asset(0x19E8, XAssetType::ASSET_TYPE_DESTRUCTIBLEDEF);
        s.Asset(0x19F0, XAssetType::ASSET_TYPE_IMAGE);
        s.Bundle(0x1A10);
        return s.slots;
    }

    // inline in sub_142E318C0: three arrays of 0x28 effects
    inline std::vector<Slot> VehicleFxDefSlots() {
        SlotList s{};
        s.Asset(0x20, XAssetType::ASSET_TYPE_FX, 0x28);
        s.Asset(0x160, XAssetType::ASSET_TYPE_FX, 0x28);
        s.Asset(0x2A0, XAssetType::ASSET_TYPE_FX, 0x28);
        return s.slots;
    }

    // sub_142E319D0's order
    inline std::vector<Slot> VehicleSoundDefSlots() {
        SlotList s{};
        s.XString(0x388);
        for (uint32_t i = 0; i < 0x14; i++) {
            s.XString(0x760 + 0x28 * i);
        }
        for (uint32_t i = 0; i < 0x14; i++) {
            uint32_t e{ 0xA80 + 0x28 * i };
            s.XString(e);
            s.StructXString(e + 8, 0x28, 8);
            s.Scr(e + 0x20);
            s.Scr(e + 0x24);
        }
        return s.slots;
    }

    inline uint32_t SlotCount(const Slot& s, const byte* header) {
        return s.kind == SK_BLOB ? (uint32_t)std::max(0, *(const int32_t*)(header + s.countOffset)) : 1;
    }
} // namespace fastfile::bo4vehicle
