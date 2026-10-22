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

    struct Part {
        uint32_t start{};
        uint32_t count{};
        fs::path texture{}; // its material's color map, or none
    };

    struct Mesh {
        std::vector<Vertex> vertices{};
        std::vector<uint32_t> indices{};
        std::vector<Part> parts{};
        float mins[3]{};
        float maxs[3]{};
        std::string error{};
    };

    // the model's xmodel json, by its name: the zone's xmodel entries, <map>/xmodels, the other fastfile folders'
    fs::path FindXModel(const Project& project, const std::string& name);
    // its mesh (the first lod's obj or gltf, as acts reads it) and color maps
    bool LoadXModel(const fs::path& json, const fs::path& mapDir, Mesh& out);

    enum class Kind { STATIC_MODEL, SCRIPT_MODEL, ENTITY };

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
        fs::path source{};   // the file it's from (gfx_map.json, entitylist.json) or its model's json
        json data{};         // its json object
    };

    struct Scene {
        std::vector<Mesh> meshes{};
        std::vector<Instance> instances{};
        fs::path gfxMap{};
        fs::path entityList{};
        float mins[3]{};
        float maxs[3]{};

        void Build(const Project& project);
    };
}
