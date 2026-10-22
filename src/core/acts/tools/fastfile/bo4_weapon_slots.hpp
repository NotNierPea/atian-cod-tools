#pragma once
#include <games/bo4/pool.hpp>

/*
 * The WeaponDef's (retail 0x1550, the loader sub_142E333C0) pointers and script strings in the loader's order, made
 * from the decompiled loader (scratchpad gen_weapon_spec.py). The bo4 weapon dumper writes them by offset, the weapon
 * linker writes them back in this order: the assets by name (references), the tunables inline (sub_142E34C90), the
 * arrays sized by a count of the header (the variants, i32 at 0x1350). The XHash fields need nothing.
 */
namespace fastfile::bo4weapon {
    using XAssetType = games::bo4::pool::XAssetType;

    constexpr size_t WEAPON_DEF_SIZE = 0x1550;
    constexpr size_t WEAPON_TUNABLES_SIZE = 0x1218;
    constexpr uint32_t VARIANTS = 0xFFFFFFFF;

    enum SlotKind : uint8_t {
        SK_ASSET,       // a pointer to an asset
        SK_ASSET_FIXED, // count asset pointers in the header
        SK_ASSET_ARRAY, // a pointer to count asset pointers (8 aligned)
        SK_SCR,
        SK_SCR_ARRAY,   // a pointer to count script strings (4 aligned)
        SK_BLOB,        // a pointer to count * elemSize bytes
        SK_XSTRING,
        SK_STRUCT5,     // 5 entries of 0x20 in the header (sub_142E31E80): models, strings, 2 x 0xC per variant
        SK_TUNABLES,    // the weapon tunables, inline
        SK_UNSUPPORTED, // a struct the linker doesn't write (must be null)
    };

    struct Slot {
        SlotKind kind;
        uint32_t offset;
        uint32_t count{ 1 };
        uint32_t countOffset{};
        uint32_t elemSize{ 8 };
        uint32_t align{ 8 };
        uint32_t type{};
    };

    struct CountAt {
        uint32_t offset;
    };

    constexpr Slot Asset(uint32_t off, uint32_t type) { return { SK_ASSET, off, 1, 0, 8, 8, type }; }
    constexpr Slot AssetFixed(uint32_t off, uint32_t n, uint32_t type) { return { SK_ASSET_FIXED, off, n, 0, 8, 8, type }; }
    constexpr Slot AssetArray(uint32_t off, uint32_t n, uint32_t type) {
        return { SK_ASSET_ARRAY, off, n == VARIANTS ? 0 : n, n == VARIANTS ? 0x1350u : 0, 8, 8, type };
    }
    constexpr Slot Scr(uint32_t off) { return { SK_SCR, off, 1, 0, 4, 4 }; }
    constexpr Slot ScrArray(uint32_t off, uint32_t n) { return { SK_SCR_ARRAY, off, n, 0, 4, 4 }; }
    constexpr Slot Blob(uint32_t off, uint32_t n, uint32_t elem, uint32_t align) {
        return { SK_BLOB, off, n == VARIANTS ? 0 : n, n == VARIANTS ? 0x1350u : 0, elem, align };
    }
    constexpr Slot Blob(uint32_t off, CountAt n, uint32_t elem, uint32_t align) { return { SK_BLOB, off, 0, n.offset, elem, align }; }
    constexpr Slot XString(uint32_t off) { return { SK_XSTRING, off }; }
    constexpr Slot Struct5(uint32_t off) { return { SK_STRUCT5, off, 5 }; }
    constexpr Slot Tunables(uint32_t off) { return { SK_TUNABLES, off, 1, 0, 8, 8, 22 }; }
    constexpr Slot Unsupported(uint32_t off) { return { SK_UNSUPPORTED, off }; }

    inline constexpr Slot SLOTS[]{
        Tunables(0xA58),
        Tunables(0xA60),
        AssetArray(0x8B8, 0x154, 3),
        AssetArray(0x8C0, 0x12C, 3),
        Asset(0xA68, 3),
        AssetArray(0xA70, VARIANTS, 4),
        AssetArray(0xA78, VARIANTS, 4),
        Scr(0x1184),
        Scr(0x1188),
        Scr(0x118C),
        Scr(0x1190),
        Scr(0x1194),
        Scr(0x1198),
        Scr(0x119C),
        ScrArray(0x8C8, 64),
        ScrArray(0x8D0, 64),
        ScrArray(0x8D8, 2),
        Struct5(0xA88),
        Struct5(0xB28),
        Scr(0x11A0),
        Scr(0x11A4),
        Asset(0xA80, 4),
        Asset(0x8E8, 84),
        Asset(0x8F0, 84),
        Asset(0x8F8, 84),
        Asset(0xBC8, 33),
        Asset(0xBD0, 33),
        Asset(0xBD8, 33),
        Asset(0xBE0, 33),
        Asset(0xBF0, 34),
        Asset(0xBE8, 33),
        Asset(0xBF8, 34),
        Asset(0xC00, 34),
        Asset(0xC08, 33),
        Asset(0xC10, 26),
        Asset(0xC18, 26),
        AssetFixed(0xC20, 4, 34),
        AssetFixed(0xC40, 4, 34),
        Asset(0xC60, 9),
        Asset(0xC68, 9),
        Asset(0xC70, 9),
        Asset(0xC78, 9),
        Asset(0xC80, 33),
        Asset(0xC88, 33),
        Asset(0xC90, 33),
        Asset(0xC98, 33),
        Asset(0xCA0, 33),
        Asset(0xCA8, 33),
        Asset(0xCB0, 9),
        Asset(0xCB8, 9),
        Blob(0xCC0, VARIANTS, 4, 4),
        AssetArray(0xCC8, VARIANTS, 4),
        AssetArray(0xCD0, VARIANTS, 4),
        AssetArray(0xCD8, VARIANTS, 4),
        AssetArray(0xCE0, VARIANTS, 4),
        Blob(0xCE8, VARIANTS, 0x10, 8),
        Blob(0xCF0, VARIANTS, 0x10, 8),
        AssetArray(0xCF8, VARIANTS, 9),
        AssetArray(0xD00, VARIANTS, 9),
        AssetArray(0xD08, VARIANTS, 9),
        Blob(0xD10, VARIANTS, 0x10, 8),
        Blob(0xD18, VARIANTS, 1, 1),
        Blob(0xD20, VARIANTS, 1, 1),
        Asset(0xD28, 4),
        Asset(0xD30, 4),
        Asset(0xD38, 9),
        Asset(0xD40, 9),
        Asset(0xD48, 9),
        Unsupported(0x1420),
        XString(0x8B0),
        Asset(0xD50, 9),
        Asset(0xD58, 9),
        Asset(0xD60, 6),
        Asset(0xD68, 6),
        Asset(0xD70, 6),
        Asset(0xD78, 6),
        Asset(0xD80, 6),
        Asset(0xD88, 9),
        Asset(0xD90, 9),
        Asset(0xD98, 9),
        Asset(0xDA0, 9),
        Asset(0xDA8, 9),
        Asset(0xDB0, 9),
        Asset(0xDB8, 9),
        Scr(0x11A8),
        Scr(0x11AC),
        Scr(0x11B0),
        Scr(0x11B4),
        Asset(0x930, 34),
        Asset(0xEA8, 158),
        Asset(0xE90, 9),
        Asset(0xE98, 9),
        Asset(0xEA0, 9),
        Asset(0xEB0, 9),
        Asset(0xEB8, 94),
        Asset(0xEC0, 94),
        Asset(0xEC8, 4),
        Asset(0xED0, 4),
        Asset(0xED8, 33),
        Asset(0xEE0, 33),
        Asset(0xEE8, 33),
        Asset(0xEF0, 33),
        Asset(0xEF8, 4),
        Asset(0xF00, 33),
        Asset(0xF08, 33),
        Blob(0x938, CountAt(0x1384), 8, 4),
        Blob(0x950, CountAt(0x1384), 8, 4),
        Blob(0x940, CountAt(0x1388), 8, 4),
        Blob(0x958, CountAt(0x1388), 8, 4),
        Blob(0x948, CountAt(0x138C), 8, 4),
        Blob(0x960, CountAt(0x138C), 8, 4),
        AssetArray(0x968, 8, 18),
        AssetArray(0x978, 8, 19),
        Asset(0xDC0, 65),
        Asset(0xDC8, 65),
        Asset(0xDD0, 65),
        Asset(0xDD8, 65),
        Asset(0xDE0, 65),
        Asset(0xDE8, 65),
        Asset(0xDF0, 65),
        Asset(0xDF8, 65),
        Asset(0xE00, 65),
        Asset(0xE08, 65),
        Asset(0xE10, 65),
        Asset(0xE18, 65),
        Asset(0xE20, 65),
        Asset(0xE28, 65),
        AssetArray(0xA20, 5, 53),
        Asset(0xE30, 98),
        Asset(0xE38, 98),
        Asset(0xE40, 98),
        Asset(0xE48, 87),
        Asset(0xE50, 87),
        Asset(0xE58, 99),
        Asset(0xE60, 99),
        Asset(0xE68, 99),
        Asset(0xE70, 99),
        Asset(0xE78, 99),
        Asset(0xE80, 99),
        Asset(0xE88, 99),
        Asset(0xF10, 33),
        Asset(0xF18, 33),
        Unsupported(0xF20),
        Asset(0xF38, 33),
        Asset(0xF40, 33),
        Asset(0xF48, 33),
        Asset(0xF50, 33),
        Asset(0xF58, 33),
        Asset(0xF60, 33),
        Asset(0xF68, 33),
        Asset(0xF70, 33),
        Asset(0xF78, 34),
        Asset(0xF80, 34),
        Asset(0xA28, 29),
        Unsupported(0xA30),
        Asset(0xF88, 33),
        Asset(0x980, 80),
        Asset(0x9A0, 80),
        Asset(0x9C0, 80),
        Asset(0x9E0, 80),
        Asset(0xA00, 80),
        Asset(0xF90, 33),
        Asset(0xF98, 33),
        Asset(0xFA0, 33),
        Asset(0xFB8, 36),
        Asset(0xFC0, 36),
        Asset(0xFC8, 36),
        Asset(0xFD0, 36),
        Asset(0xFD8, 36),
        Asset(0xFE0, 37),
        Asset(0xFE8, 37),
        Asset(0xFF0, 37),
        Asset(0xFF8, 37),
        Asset(0x1000, 37),
        Asset(0x1008, 63),
        Asset(0x1010, 63),
        Asset(0x1018, 63),
        Asset(0x1020, 63),
        Asset(0x1028, 63),
        Asset(0x1030, 63),
        AssetArray(0xA40, 0x40, 27),
        AssetArray(0xA48, 0x80, 28),
        Asset(0x1038, 9),
        Asset(0x1040, 9),
        Asset(0x1048, 9),
        Blob(0xA50, 1, 0x5DC, 4),
        Asset(0x1050, 63),
    };

    // the element count of a slot in a header
    inline uint32_t SlotCount(const Slot& s, const byte* header) {
        return s.countOffset ? (uint32_t)std::max(0, *(const int32_t*)(header + s.countOffset)) : s.count;
    }
} // namespace fastfile::bo4weapon
