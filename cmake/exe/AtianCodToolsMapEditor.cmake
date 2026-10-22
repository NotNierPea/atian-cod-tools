# AtianCodToolsMapEditor: the map editor (ImGui on Win32 + DirectX 11)
file(GLOB_RECURSE ACTS_MAP_EDITOR_SOURCES
    "${CMAKE_SOURCE_DIR}/src/exe/map-editor/*.hpp"
    "${CMAKE_SOURCE_DIR}/src/exe/map-editor/*.h"
    "${CMAKE_SOURCE_DIR}/src/exe/map-editor/*.cpp"
)
add_executable(AtianCodToolsMapEditor WIN32 ${ACTS_MAP_EDITOR_SOURCES})
source_group(
    TREE "${CMAKE_SOURCE_DIR}"
    FILES ${ACTS_MAP_EDITOR_SOURCES}
)
set_target_properties(AtianCodToolsMapEditor PROPERTIES
    OUTPUT_NAME "acts-map-editor"
    FOLDER "Executables"
)

# shim/ first: its includes.hpp stands in for acts' in the shared headers (linkers/bo4/gltf.hpp)
target_include_directories(AtianCodToolsMapEditor PRIVATE
    "${CMAKE_SOURCE_DIR}/src/exe/map-editor/shim"
    "${CMAKE_SOURCE_DIR}/src/exe/map-editor"
    "${CMAKE_SOURCE_DIR}/src/core/acts/tools/fastfile/linkers/bo4"
    "${CMAKE_SOURCE_DIR}/deps/imgui"
    "${CMAKE_SOURCE_DIR}/deps/stb"
    "${CMAKE_SOURCE_DIR}/deps/json/include"
    "${CMAKE_SOURCE_DIR}/deps/rapidjson/include"
)

target_link_libraries(AtianCodToolsMapEditor PRIVATE imgui d3d11 dxgi d3dcompiler)
add_dependencies(AtianCodToolsMapEditor imgui)
append_common_defs(AtianCodToolsMapEditor)
