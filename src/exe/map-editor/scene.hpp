#pragma once
#include "editor.hpp"

// scene.cpp: the map's world for the 3D view, the gfx_map's static models and the entity list's entities
namespace editor::scene {
    // game space: x forward, y left, z up, inches
    struct Vertex {
        float pos[3];
        float normal[3];
        float uv[2]; // v down, as D3D samples
    };

    // how a part's texture alpha is drawn: ignored, cut out under half (opaque materials, the chalk outlines), blended
    // (decals and transparent materials, drawn after the rest)
    enum class Alpha { SOLID, CUTOUT, BLEND };

    struct Part {
        uint32_t start{};
        uint32_t count{};
        fs::path texture{}; // its material's color map (a png, or a dumped image's json), or none
        Alpha alpha{};
        bool mask{};        // the color map is a mask (black, its alpha the shape): drawn as white chalk (wall-buys)
    };

    struct Mesh {
        std::vector<Vertex> vertices{};
        std::vector<uint32_t> indices{};
        std::vector<Part> parts{};
        float mins[3]{};
        float maxs[3]{};
        std::string error{};
    };

    // the dumped (lossless) assets under the map's folder: <set>/xmodel/hash_x.json, xmodelmesh/file_x.json, material/,
    // image/, by type and hash
    enum class AssetType { XMODEL, XMODELMESH, MATERIAL, IMAGE, COUNT };
    struct AssetIndex {
        std::map<std::string, fs::path> files[(int)AssetType::COUNT]{};

        void Build(const fs::path& mapDir);
        fs::path Find(AssetType type, const std::string& name) const; // name: "#hash_x", "hash_x" or a plain name
    };
    std::string HashKey(const std::string& name); // the hash's hex digits ("x" of hash_x), a plain name's fnv1a

    // the model's xmodel json, by its name: the zone's xmodel entries, <map>/xmodels, the other fastfile folders',
    // the dumped stock models
    fs::path FindXModel(const Project& project, const AssetIndex& assets, const std::string& name);
    // its mesh (the first lod's obj or gltf, as acts reads it; a stock model's most detailed lod) and color maps
    bool LoadXModel(const fs::path& json, const fs::path& mapDir, const AssetIndex& assets, Mesh& out);

    // SPAWNED: a model the map script spawns (editor_bindings.json's "scriptModels"), shown where its layout puts it
    enum class Kind { STATIC_MODEL, SCRIPT_MODEL, ENTITY, SPAWNED };

    struct Instance {
        Kind kind{};
        std::string name{};  // the model, or the classname
        std::string label{}; // what the lists show
        int mesh{ -1 };      // in Scene::meshes, -1 a marker box
        float origin[3]{};
        float angles[3]{};   // pitch, yaw, roll
        float scale{ 1 };
        float mins[3]{};     // model space: the mesh's bounds or the marker's box
        float maxs[3]{};
        float color[3]{ 0.6f, 0.6f, 0.62f }; // untextured parts' and markers'
        fs::path source{};   // the file it's from (gfx_map.json, entitylist.json)
        int sourceIndex{ -1 }; // its index there (staticModels / entities)
        json data{};         // its json object
        fs::path model{};    // its model's json
        int binding{ -1 };   // in Scene::bindings, -1 none (saved into its source)
        std::string locked{}; // why it can't move without a binding (a SPAWNED one)
        float savedOrigin[3]{}; // as the files have it
        float savedAngles[3]{};
        float followed[3]{};  // the part of its move that follows its group's numbers (a sill with its window): not its own
        float scaleDelta[3]{}; // the scale mode's handle drags by axis (a binding's scale numbers)
        float savedScale{ 1 };
        float rot[4]{ 0, 0, 0, 1 }; // with useRot its full turn (a quaternion, x y z w) instead of the angles
        bool useRot{};
        int board{};         // a zbarrier's board: its number (from 1), its source the barrier's
        float boardOffset[3]{}; // a board's place and turn in its barrier's space (its repair anim's end)
        float boardRot[4]{ 0, 0, 0, 1 };

        bool Moved() const;
    };

    // edit.cpp: an object's config, the json objects it's made from: its binding's targets, its own entry
    struct ConfigEntry {
        fs::path file{};
        json path{};                    // the keys / indices to the object in the file
        std::string what{};             // what the entry is to the object
        std::vector<std::string> run{}; // the scripts to run after saving it (its binding's)
        std::vector<std::string> keys{}; // only these keys of it (none: all)
    };
    json ReadConfig(const ConfigEntry& entry, std::string& error);
    bool SaveConfig(const ConfigEntry& entry, const json& value, std::string& error); // a .bak kept

    // edit.cpp: where a moved object is saved, the map's editor_bindings.json (maps/ or the map's folder)
    struct Target {
        fs::path file{};            // a json of the map's
        json path{};                // the keys / indices to the object in it
        std::string origin{};       // "xy" or "xyz": how its origin is written, "" not at all
        std::string originKey{ "origin" }; // the object's origin array, "" the object is the array
        std::string yaw{};          // the object's yaw number, or none
        std::string angles{};       // the object's [pitch, yaw, roll] array, or none
        // a move written as a change of the object's numbers (room.json's windows, wall-buys, room size): the objects
        // bound to the same object move as one group, the change written once
        std::string along{};        // a number the move along the object's wall changes ("center", "along")
        std::string wallKey{ "wall" }; // the key naming that wall: south / north run along x, west / east along y
        std::string zKey{};         // a number the z move changes ("barrierZ", "height")
        float zDefault{};           // its value when the object has none
        std::vector<std::string> zAlso{}; // numbers the z move changes too (a sill's top lifted with its base)
        std::string across{};       // a number the move across the wall changes (the other horizontal axis)
        std::string yawKey{};       // a number the turn changes ("barrierYaw", "sillYaw"), alone: its group doesn't turn
        std::vector<std::string> follow{}; // its group's numbers it follows live without writing them (a sill: "center")
        // the scale mode's numbers by world axis (from "scale": along / across / x / y / z): a handle's drag changes it,
        // twice the drag on x / y with scaleDouble (a length growing on both sides); scaleXY: the x handle scales x and
        // y together (the square room)
        std::string scaleKey[3]{};
        bool scaleDouble{};
        bool scaleXY{};
        json scaleRoles{};          // "scale" as written, its along / across resolved when the wall is known
        std::map<std::string, float> defaults{}; // the numbers' values when the object has none
        int axis{ -1 };             // along's axis (0 x, 1 y), from the wall when the bindings load (across the other)
        bool Scales() const { return !scaleKey[0].empty() || !scaleKey[1].empty() || !scaleKey[2].empty(); }
        bool Relative() const { return !along.empty() || !across.empty() || !zKey.empty() || !yawKey.empty() || Scales(); }
        int AcrossAxis() const { return axis < 0 ? -1 : 1 - axis; }
        float Default(const std::string& key) const {
            auto it{ defaults.find(key) };
            return it != defaults.end() ? it->second : key == zKey ? zDefault : 0.0f;
        }
    };
    struct GscPatch {
        fs::path file{};
        std::string function{};     // the layout function
        std::string field{};        // its "s.<field> = ( x, y, z );" line, the origin
    };
    struct Binding {
        std::string kind{};         // "static", "entity" or any
        json match{};               // the json object's values to match
        int index{ -1 };            // the n-th object matching, -1 all
        std::string locked{};       // why it can't move here
        std::vector<Target> targets{};
        bool direct{};              // its source (gfx_map.json / entitylist.json) written too
        std::vector<GscPatch> gsc{};
        std::vector<std::string> run{}; // the scripts to run after (python, from the map's folder)
        std::vector<ConfigEntry> config{}; // "config": more for the config panel (a locked object's real source)
        std::string note{};
        fs::path copyFile{};        // "duplicate": the list Ctrl+D adds a stone of the object's box to
        json copyPath{};
        std::string copySelect{};   // the copy's label after the rebuild ("{n}" its index)
        bool removable{};           // Delete takes its first target out of its list (a stone)

        bool Rotates() const;       // a yaw or angles written
        bool FullAngles() const;    // pitch and roll too
        bool MovesZ() const;
        const Target* Group() const; // its first relative target: the group it moves with, none
    };

    // what to select once the map reloads: by label, or its source's entry (sourceIndex >= 0)
    struct Selection {
        std::string label{};
        fs::path source{};
        int sourceIndex{ -1 };
    };

    struct Scene {
        std::vector<Mesh> meshes{};
        std::vector<Instance> instances{};
        std::vector<Binding> bindings{};
        std::map<fs::path, std::string> generated{}; // files a tool rewrites, why
        std::map<std::string, std::vector<std::string>> runGroups{};
        fs::path bindingsFile{};
        AssetIndex assets{};
        fs::path gfxMap{};
        fs::path entityList{};
        float mins[3]{};
        float maxs[3]{};

        void Build(const Project& project);
        void LoadBindings(const Project& project);
        // the bindings file's "scriptModels": models the map script spawns, from its layout functions or at entities
        void AddScriptModels(const Project& project, const std::function<int(const std::string&, fs::path&)>& meshFor);
        // the zbarrier entities' boards, where their repair anims leave them (scene.cpp)
        void AddBarrierBoards(const Project& project, const std::function<int(const std::string&, fs::path&)>& meshFor);
        void PlaceBoards(); // the boards where their barriers are now (moved, turned)
        const Binding* BindingOf(const Instance& inst) const;
        std::vector<ConfigEntry> ConfigOf(const Instance& inst) const;
        size_t MovedCount() const;
        // the moved objects written to their files (a .bak kept), the scripts to run after returned; write false: only
        // logged (the tests)
        bool Save(const Project& project, std::vector<std::string>& scripts, std::string& error, bool write = true);
        // Ctrl+D: a copy written into the object's file (its binding's "duplicate" list gets a stone of its box, else its
        // gfx_map / entitylist entry is copied), the scripts to run and what to select after; Delete: a removable one out
        bool Duplicate(const Instance& inst, std::vector<std::string>& scripts, Selection& select, std::string& error);
        bool Remove(const Instance& inst, std::vector<std::string>& scripts, std::string& error);
    };
}
