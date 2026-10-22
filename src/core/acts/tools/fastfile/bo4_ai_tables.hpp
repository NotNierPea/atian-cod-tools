#pragma once
#include <games/bo4/pool.hpp>

/*
 * The AI tables' layouts (animmappingtable, animselectortable, animstatemachine, behaviortree, behaviorstatemachine,
 * footsteptable, surfacesounddef), read from their Load_ functions. The bo4 unlinker dumps them to json with it and the
 * linker writes them back the same way, in the loader's order. The bytes no field covers are dumped as "unk_<offset>".
 */
namespace fastfile::bo4ai {
    using XAssetType = games::bo4::pool::XAssetType;

    enum FieldKind : uint8_t {
        FK_XHASH,
        FK_SCR,
        FK_I32,
        FK_U32,
        FK_F32,
        FK_U8,
        FK_BOOL,
        FK_ARRAY, // a pointer to count elements of sub, the count an int32 at countOffset
        FK_ASSET, // fixedCount pointers to sub assets (the zone has them inline)
    };

    struct Struct;
    struct Field {
        const char* name;
        uint32_t offset;
        FieldKind kind;
        const Struct* sub{};
        uint32_t countOffset{};
        uint32_t align{ 8 };
        XAssetType assetType{};
        uint32_t fixedCount{ 1 };
    };

    struct Struct {
        const char* name;
        uint32_t size;
        std::vector<Field> fields;
        // its json is the value of its only field
        bool flat{};
    };

    struct AssetSchema {
        XAssetType type;
        const char* dir;
        const Struct* header;
    };

    const AssetSchema* FindSchema(XAssetType type);
    const std::vector<AssetSchema>& GetSchemas();
    // the field's size in its struct
    uint32_t FieldSize(const Field& field);
} // namespace fastfile::bo4ai
