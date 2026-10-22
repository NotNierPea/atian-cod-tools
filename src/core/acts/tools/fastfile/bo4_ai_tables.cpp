#include <includes.hpp>
#include <tools/fastfile/bo4_ai_tables.hpp>

namespace fastfile::bo4ai {
    namespace {
        using games::bo4::pool::XAssetType;

        Field Hash(const char* name, uint32_t offset) { return { name, offset, FK_XHASH }; }
        Field Scr(const char* name, uint32_t offset) { return { name, offset, FK_SCR }; }
        Field Array(const char* name, uint32_t offset, const Struct* sub, uint32_t countOffset, uint32_t align = 8) {
            return { name, offset, FK_ARRAY, sub, countOffset, align };
        }
        Field Asset(const char* name, uint32_t offset, const Struct* sub, XAssetType type, uint32_t count = 1) {
            return { name, offset, FK_ASSET, sub, 0, 8, type, count };
        }

        const Struct XHashElem{ "xhash", 0x10, { Hash("value", 0) }, true };
        const Struct Int32Elem{ "int", 4, { { "value", 0, FK_I32 } }, true };

        // animmappingtable (Load_AnimMappingTable): a name to its anims
        const Struct AnimMappingTableEntry{ "AnimMappingTableEntry", 0x20, {
            Hash("name", 0),
            Array("anims", 0x10, &XHashElem, 0x18),
        } };
        const Struct AnimMappingTable{ "AnimMappingTable", 0x20, {
            Hash("name", 0),
            Array("mappings", 0x10, &AnimMappingTableEntry, 0x18),
        } };

        // animselectortable: tables of columns and rows of cells
        const Struct AnimSelectorTableColumn{ "AnimSelectorTableColumn", 0x28, {
            Hash("name", 0),
            Hash("hash10", 0x10),
        } };
        const Struct AnimSelectorTableCell{ "AnimSelectorTableCell", 0x18, {
            Hash("value", 0),
        } };
        const Struct AnimSelectorTableRow{ "AnimSelectorTableRow", 0x10, {
            Array("cells", 0, &AnimSelectorTableCell, 8),
        } };
        const Struct AnimSelectorTable{ "AnimSelectorTable", 0x60, {
            Hash("name", 0),
            Array("columns", 0x10, &AnimSelectorTableColumn, 0x18),
            Array("rows", 0x20, &AnimSelectorTableRow, 0x28),
        } };
        const Struct AnimSelectorTableSet{ "AnimSelectorTableSet", 0x20, {
            Hash("name", 0),
            Array("tables", 0x10, &AnimSelectorTable, 0x18),
        } };

        // animstatemachine
        const Struct AnimStateMachine28{ "AnimStateMachine28", 0x28, {
            Hash("name", 0),
            Array("indices", 0x18, &Int32Elem, 0x20, 4),
        } };
        const Struct AnimStateMachineD0{ "AnimStateMachineD0", 0xD0, {
            Hash("name", 0),
            Hash("hash10", 0x10),
            Hash("hash30", 0x30),
            Hash("hash40", 0x40),
            Hash("hash50", 0x50),
            Hash("hash60", 0x60),
            Hash("hash70", 0x70),
            Hash("hash80", 0x80),
            Hash("hash90", 0x90),
            Hash("hashA0", 0xA0),
            Hash("hashB0", 0xB0),
            Array("indices", 0xC0, &Int32Elem, 0xC8, 4),
        } };
        const Struct AnimStateMachineB0{ "AnimStateMachineB0", 0xB0, {
            Hash("name", 0),
            Hash("hash10", 0x10),
            Hash("hash30", 0x30),
            Hash("hash40", 0x40),
            Hash("hash50", 0x50),
            Hash("hash60", 0x60),
            Hash("hash70", 0x70),
            Hash("hash80", 0x80),
            Hash("hash90", 0x90),
            Hash("hashA0", 0xA0),
        } };
        const Struct AnimStateMachine{ "AnimStateMachine", 0x40, {
            Hash("name", 0),
            Array("list10", 0x10, &AnimStateMachine28, 0x18),
            Array("list20", 0x20, &AnimStateMachineD0, 0x28),
            Array("list30", 0x30, &AnimStateMachineB0, 0x38),
        } };

        // behaviorstatemachine
        const Struct BehaviorStateMachine28{ "BehaviorStateMachine28", 0x28, {
            Hash("name", 8),
        } };
        const Struct BehaviorStateMachineState{ "BehaviorStateMachineState", 0x78, {
            Hash("name", 0),
            Hash("hash10", 0x10),
            Hash("hash20", 0x20),
            Hash("hash30", 0x30),
            Hash("hash40", 0x40),
            Hash("hash50", 0x50),
            Scr("str60", 0x60),
            Array("list68", 0x68, &BehaviorStateMachine28, 0x70),
        } };
        const Struct BehaviorStateMachine{ "BehaviorStateMachine", 0x20, {
            Hash("name", 0),
            Array("states", 0x10, &BehaviorStateMachineState, 0x18),
        } };

        // behaviortree
        const Struct BehaviorTreeNode{ "BehaviorTreeNode", 0xB0, {
            Hash("name", 0),
            Array("children", 0x20, &Int32Elem, 0x28, 4),
            Asset("stateMachine", 0x30, &BehaviorStateMachine, XAssetType::ASSET_TYPE_BEHAVIORSTATEMACHINE),
            Hash("hash38", 0x38),
            Hash("hash70", 0x70),
            Hash("hash48", 0x48),
            Scr("str58", 0x58),
            Hash("hash80", 0x80),
            Hash("hash90", 0x90),
            Hash("hash60", 0x60),
        } };
        const Struct BehaviorTree{ "BehaviorTree", 0x30, {
            Hash("name", 0),
            Array("nodes", 0x10, &BehaviorTreeNode, 0x18),
            Array("list20", 0x20, &Int32Elem, 0x28, 4),
        } };

        // footsteptable: 8 surfacesounddefs, a sound for each surface
        std::vector<Field> SurfaceSoundDefFields() {
            std::vector<Field> fields{ Hash("name", 0) };
            static char names[0x28][16];
            for (uint32_t i = 0; i < 0x28; i++) {
                std::snprintf(names[i], sizeof(names[i]), "surface%u", i);
                fields.push_back(Hash(names[i], 0x10 + i * 0x10));
            }
            return fields;
        }
        const Struct SurfaceSoundDef{ "SurfaceSoundDef", 0x290, SurfaceSoundDefFields() };
        const Struct FootstepTable{ "FootstepTable", 0x50, {
            Hash("name", 0),
            Asset("sounds", 0x10, &SurfaceSoundDef, XAssetType::ASSET_TYPE_SURFACESOUNDDEF, 8),
        } };

        // sharedweaponsounds (sub_142E2A330): 2 surfacesounddefs, 9 entitysoundimpacts inline (0x490: hashes only)
        const Struct EntitySoundImpacts{ "EntitySoundImpacts", 0x490, { Hash("name", 0) } };
        const Struct SharedWeaponSounds{ "SharedWeaponSounds", 0x228, {
            Hash("name", 0),
            Asset("surfaceSounds1", 0x10, &SurfaceSoundDef, XAssetType::ASSET_TYPE_SURFACESOUNDDEF),
            Asset("surfaceSounds2", 0x18, &SurfaceSoundDef, XAssetType::ASSET_TYPE_SURFACESOUNDDEF),
            Asset("impacts", 0x20, &EntitySoundImpacts, XAssetType::ASSET_TYPE_ENTITYSOUNDIMPACTS, 9),
        } };

        const std::vector<AssetSchema> schemas{
            { XAssetType::ASSET_TYPE_SHAREDWEAPONSOUNDS, "sharedweaponsounds", &SharedWeaponSounds },
            { XAssetType::ASSET_TYPE_ENTITYSOUNDIMPACTS, "entitysoundimpacts", &EntitySoundImpacts },
            { XAssetType::ASSET_TYPE_ANIMMAPPINGTABLE, "animmappingtable", &AnimMappingTable },
            { XAssetType::ASSET_TYPE_ANIMSELECTORTABLESET, "animselectortable", &AnimSelectorTableSet },
            { XAssetType::ASSET_TYPE_ANIMSTATEMACHINE, "animstatemachine", &AnimStateMachine },
            { XAssetType::ASSET_TYPE_BEHAVIORTREE, "behaviortree", &BehaviorTree },
            { XAssetType::ASSET_TYPE_BEHAVIORSTATEMACHINE, "behaviorstatemachine", &BehaviorStateMachine },
            { XAssetType::ASSET_TYPE_FOOTSTEP_TABLE, "footsteptable", &FootstepTable },
            { XAssetType::ASSET_TYPE_SURFACESOUNDDEF, "surfacesounddef", &SurfaceSoundDef },
        };
    } // namespace

    const AssetSchema* FindSchema(XAssetType type) {
        for (const AssetSchema& s : schemas) {
            if (s.type == type) {
                return &s;
            }
        }
        return nullptr;
    }

    const std::vector<AssetSchema>& GetSchemas() { return schemas; }

    uint32_t FieldSize(const Field& field) {
        switch (field.kind) {
        case FK_XHASH: return 0x10;
        case FK_SCR:
        case FK_I32:
        case FK_U32:
        case FK_F32: return 4;
        case FK_U8:
        case FK_BOOL: return 1;
        case FK_ARRAY: return 8;
        case FK_ASSET: return 8 * field.fixedCount;
        default: return 0;
        }
    }
} // namespace fastfile::bo4ai
