#include <includes.hpp>
#include <tools/fastfile/linkers/linker_bo4.hpp>

/*
 * gfx_map (GfxWorld, retail 0x1AB0, the loader sub_142E1E830, the arrays in bo4_unlinker_map_gfx.cpp): the renderer's
 * world, loaded by R_LoadWorld (sub_14353A520) with cpu_occlusion_data. The header is in TEMP (aligned 16), all it
 * points to in VIRTUAL. Null pointers are skipped by the loader and by the draw buffers' creation (sub_14353AF10), so a
 * world can be empty. For now: the name, the base name, the bounds, and the arrays without pointers as raw files (the
 * dumper's gfx_map_raw/<ptr>.bin), written in the loader's order; the count is the file's size by element.
 *
 * {
 *     "name": "maps/zm/enh_test.d3dbsp",   // maps/<mode>/<map>.d3dbsp
 *     "baseName": "enh_test",              // optional
 *     "mins": [ -4096, -4096, -4096 ],
 *     "maxs": [ 4096, 4096, 4096 ],
 *     "checksum": 0,
 *     "arrays": { "c0": "gfx/c0.bin" },    // optional, by the offset (hex) of the pointer in the header
 *     "settings": "gfx/header.bin",        // optional, a game map's header (the dumper's gfx_map_raw/header.bin)
 *     "settingsIndexTables": true,         // optional, its index tables too (with its lighting and arrays)
 *     "iesProfileImage": "hash_7a2b772e4c99fcf4", "cookieImage": "hash_3dfc5bb74b4e6a95", "cookieCount": 15,
 *     "decalRevealImage": "hash_b5bb050d4d322ca", "images550": [ "hash_46308ef839eb98d4", ... ], // the game's, by name
 *     "lighting": "maps/lighting.json",    // linker_bo4_map_lighting.cpp, loaded inline at 0x17F8
 *     "streamerworld": "maps/streamerworld.json" // linker_bo4_map_empty.cpp, inline at 0x1800
 * }
 * The streamer's world start (sub_143720AF0) reads the lighting without a null check, and sub_143713EF0 the
 * streamerworld. "settings" copies the header's plain values, the same in the frontend's and zm_towers': the render
 * settings 0x20-0xB8, 0x1B8 (-1), the 32 records of 0x608-0x1618 (1.0s). The index tables next to their arrays
 * (u32[4] + u32[32] at 0x16C8, 0x1898, 0x1968) are per map: none (-1) in all, 0 is an index.
 * The dpvs' 16 vis buffers (0x1F8 + 0x10 * i: size, pointer; 4 views of smodels, smodels, surfaces, volume decals) are
 * runtime memory the cull jobs write, the count rounded to 16 in the game's (2080, 2080, 160, 16 in the frontend):
 * 16 each, or the static models' count rounded for the 8 of smodels.
 *
 * "staticModels": [ { "model": "enh_floor", "origin": [ 0, 0, 0 ], "angles": [ 0, 0, 0 ], "scale": 1,
 *     "mins": [ -2048, -2048, -64 ], "maxs": [ 2048, 2048, 0 ] } ]   // mins/maxs: the model's own bounds
 * Static models (the dpvs, sub_142E1F510): the count at dpvs +0x04, the draw insts at +0x248 (0x38: the model, -1 at
 * 0x20, the id at 0x30 that scripts and smodel animations match (sub_143700740), -1 at 0x2E/0x34), their placements at
 * +0x250 (0x40: quat, origin, scale, world mins/maxs, the draw inst's index at 0x38, radius 0x3C; the streamer's world
 * start sorts them and points each draw inst's 0x18 at its own, sub_143720AF0), 8 bytes each at +0x258 (0 in the
 * game's), a byte each at +0x2A0 (3, the count at +0x298 rounded as the vis buffers) and the cull tree at +0x188
 * (data size/data, 2 u32 lists: 0x10 bytes then 0x38 nodes, bounds, [begin, directEnd) drawn when the node is seen): one
 * root over them all, all in its tested range with their bounds (sub_14357E980 marks them visible). The cull queues
 * are seeded with their root only when the surfaces' tree (+0x1B8) has indices (sub_143581720): an empty one with 1.
 * The sun's dynamic cascades draw the models in the list at +0x228 (count +0x220, culled against each cascade by
 * sub_143587FE0, drawn by sub_143509D00), the rest go to the cached tiles only (the list at +0x230, count +0x224), which
 * lose the shadows of models off the screen's edges: all of them in the first (zm_towers': 4108 and 34073).
 */
namespace {
    using namespace fastfile::linker::bo4;

    constexpr size_t GFXWORLD_SIZE = 0x1AB0;

    struct RawArray {
        uint32_t ptr;
        uint32_t count;
        uint32_t countSize;
        uint32_t elemSize;
        uint32_t align;
    };

    // the loader's order; the arrays with pointers (0x578, 0x598) and the runtime ones are left out
    constexpr RawArray rawArrays[]{
        { 0xC0, 0xB8, 8, 0xC, 4 }, { 0xD0, 0xC8, 8, 0x10, 4 },
        { 0x100, 0xF8, 4, 0xC, 16 }, { 0x128, 0xF8, 4, 0x14, 16 }, { 0x158, 0x150, 4, 0x2, 2 },
        { 0x188, 0x180, 4, 0x50, 16 }, { 0x588, 0x580, 4, 0x54, 4 }, { 0x5F0, 0x5E8, 4, 0xC0, 4 }, { 0x600, 0x5F8, 4, 0xC0, 4 },
        { 0x1770, 0x1768, 4, 0xB4, 4 }, { 0x1780, 0x1778, 4, 0xB4, 4 }, { 0x1790, 0x1788, 4, 0x54, 4 },
        { 0x1620, 0x1618, 4, 0x40, 4 }, { 0x1630, 0x1628, 4, 0xB0, 4 }, { 0x1640, 0x1638, 4, 0xB0, 4 },
        { 0x1660, 0x1658, 4, 0x40, 4 }, { 0x1670, 0x1668, 4, 0xB0, 4 }, { 0x1680, 0x1678, 4, 0xB0, 4 },
        { 0x1690, 0x1688, 4, 0xC4, 4 }, { 0x16A0, 0x1698, 4, 0xC4, 4 }, { 0x16B0, 0x16A8, 4, 0xB4, 4 },
        { 0x16C0, 0x16B8, 4, 0xB4, 4 }, { 0x1760, 0x1758, 4, 0x200, 4 }, { 0x17A0, 0x1798, 4, 0xB4, 4 },
        { 0x17B0, 0x17A8, 4, 0xB4, 4 }, { 0x17C0, 0x17B8, 4, 0xB4, 4 }, { 0x17D0, 0x17C8, 4, 0xB4, 4 },
        { 0x17E0, 0x17D8, 4, 0x90, 4 }, { 0x17F0, 0x17E8, 4, 0xE0, 4 },
        { 0x1930, 0x1928, 4, 0x48, 4 }, { 0x1940, 0x1938, 4, 0xB8, 4 }, { 0x1950, 0x1948, 4, 0xB8, 4 },
        { 0x1960, 0x1958, 4, 0xFD0, 4 }, { 0x1A00, 0x19F8, 4, 0x48, 4 }, { 0x1A10, 0x1A08, 4, 0xB8, 4 },
        { 0x1A20, 0x1A18, 4, 0xB8, 4 }, { 0x1A30, 0x1A28, 4, 0x2B0, 4 },
        // their counts are those of other arrays (0x1788, 0x17D8, 0x1758, 0x1958, 0x1A28)
        { 0x1A38, 0x1788, 4, 0x40, 1 }, { 0x1A40, 0x17D8, 4, 0x40, 1 }, { 0x1A48, 0x1758, 4, 0x40, 1 },
        { 0x1A50, 0x1958, 4, 0x40, 1 }, { 0x1A58, 0x1A28, 4, 0x40, 1 },
    };

    class XAssetLinkerImpl : public XAssetLinker {
      public:
        using XAssetLinker::XAssetLinker;

        void Compute(BO4LinkContext& ctx, const char* id, BO4FFContext& ff) override {
            std::filesystem::path path{ ctx.linkCtx.input / id };
            BO4Json cfg{ path };
            if (!cfg.SyncConfig(false)) {
                LOG_ERROR("Can't read gfx_map {}", path.string());
                ctx.error = true;
                return;
            }
            std::string assetName{ cfg.GetString("name", "") };
            if (assetName.empty()) {
                LOG_ERROR("gfx_map {}: no name (maps/<mode>/<map>.d3dbsp)", path.string());
                ctx.error = true;
                return;
            }
            std::string baseName{ cfg.GetString("baseName", "") };

            // the raw arrays, read first: their counts go in the header
            std::string arrays[ACTS_ARRAYSIZE(rawArrays)]{};
            auto arraysIt{ cfg.main.FindMember("arrays") };
            if (arraysIt != cfg.main.MemberEnd() && arraysIt->value.IsObject()) {
                for (auto it = arraysIt->value.MemberBegin(); it != arraysIt->value.MemberEnd(); ++it) {
                    uint32_t off{ (uint32_t)std::strtoul(it->name.GetString(), nullptr, 16) };
                    size_t idx{ 0 };
                    while (idx < ACTS_ARRAYSIZE(rawArrays) && rawArrays[idx].ptr != off) {
                        idx++;
                    }
                    if (idx == ACTS_ARRAYSIZE(rawArrays) || !it->value.IsString()) {
                        LOG_ERROR("gfx_map {}: no raw array at 0x{}", path.string(), it->name.GetString());
                        ctx.error = true;
                        return;
                    }
                    if (!utils::ReadFile(path.parent_path() / it->value.GetString(), arrays[idx]) || arrays[idx].size() % rawArrays[idx].elemSize) {
                        LOG_ERROR("gfx_map {}: can't read {} or its size isn't a multiple of 0x{:x}", path.string(), it->value.GetString(), rawArrays[idx].elemSize);
                        ctx.error = true;
                        return;
                    }
                }
            }

            /*
             * the brush models (0x188, 0x50 each: bounds 0x30/0x3C, surface count 0x48, first surface 0x4C): model 0 is the
             * world's own (all its surfaces in the game's), the renderer reads it each frame (sub_14358E230). Without a
             * file: model 0 over the bounds, no surfaces
             */
            for (size_t i = 0; i < ACTS_ARRAYSIZE(rawArrays); i++) {
                if (rawArrays[i].ptr != 0x188 || !arrays[i].empty()) {
                    continue;
                }
                arrays[i].assign(0x50, 0);
                auto VecTo = [&](const char* key, size_t off, float def) {
                    auto vIt{ cfg.main.FindMember(key) };
                    for (rapidjson::SizeType j = 0; j < 3; j++) {
                        float v{ vIt != cfg.main.MemberEnd() && vIt->value.IsArray() && j < vIt->value.Size() && vIt->value[j].IsNumber()
                            ? vIt->value[j].GetFloat() : def };
                        std::memcpy(arrays[i].data() + off + j * 4, &v, 4);
                    }
                };
                VecTo("mins", 0x30, -4096);
                VecTo("maxs", 0x3C, 4096);
            }

            // the static models (see above)
            struct StaticModel {
                std::string model;
                float quat[4]{ 0, 0, 0, 1 };
                float origin[3]{};
                float scale{ 1 };
                float mins[3]{};
                float maxs[3]{};
                float radius{};
            };
            std::vector<StaticModel> staticModels{};
            auto smodelsIt{ cfg.main.FindMember("staticModels") };
            if (smodelsIt != cfg.main.MemberEnd() && smodelsIt->value.IsArray()) {
                for (auto& v : smodelsIt->value.GetArray()) {
                    if (!v.IsObject() || !v.HasMember("model") || !v["model"].IsString()) {
                        LOG_ERROR("gfx_map {}: a static model has no model", path.string());
                        ctx.error = true;
                        return;
                    }
                    StaticModel& sm{ staticModels.emplace_back() };
                    sm.model = v["model"].GetString();
                    auto Vec = [&](const char* key, float* out, float def) {
                        auto it{ v.FindMember(key) };
                        for (rapidjson::SizeType j = 0; j < 3; j++) {
                            out[j] = it != v.MemberEnd() && it->value.IsArray() && j < it->value.Size() && it->value[j].IsNumber() ? it->value[j].GetFloat() : def;
                        }
                    };
                    float angles[3]{};
                    float localMins[3]{};
                    float localMaxs[3]{};
                    Vec("origin", sm.origin, 0);
                    Vec("angles", angles, 0);
                    Vec("mins", localMins, -1);
                    Vec("maxs", localMaxs, 1);
                    sm.scale = v.HasMember("scale") && v["scale"].IsNumber() ? v["scale"].GetFloat() : 1;

                    // pitch, yaw, roll (degrees) to a quat, then the world bounds from the 8 corners
                    constexpr float DEG2RAD{ 3.14159265f / 180.0f };
                    const float cp{ std::cos(angles[0] * DEG2RAD / 2) }, sp{ std::sin(angles[0] * DEG2RAD / 2) };
                    const float cy{ std::cos(angles[1] * DEG2RAD / 2) }, sy{ std::sin(angles[1] * DEG2RAD / 2) };
                    const float cr{ std::cos(angles[2] * DEG2RAD / 2) }, sr{ std::sin(angles[2] * DEG2RAD / 2) };
                    sm.quat[0] = sr * cp * cy - cr * sp * sy;
                    sm.quat[1] = cr * sp * cy + sr * cp * sy;
                    sm.quat[2] = cr * cp * sy - sr * sp * cy;
                    sm.quat[3] = cr * cp * cy + sr * sp * sy;
                    const float* q{ sm.quat };
                    for (size_t j = 0; j < 3; j++) {
                        sm.mins[j] = 1e30f;
                        sm.maxs[j] = -1e30f;
                    }
                    for (size_t c = 0; c < 8; c++) {
                        float p[3]{ (c & 1 ? localMaxs[0] : localMins[0]) * sm.scale, (c & 2 ? localMaxs[1] : localMins[1]) * sm.scale,
                            (c & 4 ? localMaxs[2] : localMins[2]) * sm.scale };
                        // v' = v + 2w(q x v) + 2(q x (q x v))
                        float t[3]{ 2 * (q[1] * p[2] - q[2] * p[1]), 2 * (q[2] * p[0] - q[0] * p[2]), 2 * (q[0] * p[1] - q[1] * p[0]) };
                        float r[3]{ p[0] + q[3] * t[0] + (q[1] * t[2] - q[2] * t[1]), p[1] + q[3] * t[1] + (q[2] * t[0] - q[0] * t[2]),
                            p[2] + q[3] * t[2] + (q[0] * t[1] - q[1] * t[0]) };
                        for (size_t j = 0; j < 3; j++) {
                            sm.mins[j] = std::min(sm.mins[j], r[j] + sm.origin[j]);
                            sm.maxs[j] = std::max(sm.maxs[j], r[j] + sm.origin[j]);
                        }
                    }
                    const float ext[3]{ (sm.maxs[0] - sm.mins[0]) / 2, (sm.maxs[1] - sm.mins[1]) / 2, (sm.maxs[2] - sm.mins[2]) / 2 };
                    sm.radius = std::sqrt(ext[0] * ext[0] + ext[1] * ext[1] + ext[2] * ext[2]);
                }
            }
            const uint32_t smodelCount{ (uint32_t)staticModels.size() };
            // the 8 smodel vis buffers and the bytes at +0x2A0: the count rounded to 16
            const uint32_t smodelVisSize{ std::max<uint32_t>(16, (smodelCount + 15) & ~15u) };

            ff.data.PushStream(XFILE_BLOCK_TEMP);
            ff.data.Align(16);
            byte* header{ ff.data.AllocStreamPtr<byte>(GFXWORLD_SIZE) };
            std::memset(header, 0, GFXWORLD_SIZE);
            std::string settingsFile{ cfg.GetString("settings", "") };
            // the index tables from the settings' map too (its lighting by name, "lighting": ",maps/zm/<map>.d3dbsp", its
            // arrays): its states index its arrays with them
            const bool settingsIndexTables{ cfg.GetBool("settingsIndexTables", false) };
            if (!settingsFile.empty()) {
                std::string settings{};
                if (!utils::ReadFile(path.parent_path() / settingsFile, settings) || settings.size() != GFXWORLD_SIZE) {
                    LOG_ERROR("gfx_map {}: can't read {} or it isn't a 0x{:x} header", path.string(), settingsFile, GFXWORLD_SIZE);
                    ctx.error = true;
                    ff.data.PopStream();
                    return;
                }
                constexpr std::pair<size_t, size_t> ranges[]{ { 0x20, 0xB8 }, { 0x1B8, 0x1BC }, { 0x608, 0x1618 } };
                for (auto [lo, hi] : ranges) {
                    std::memcpy(header + lo, settings.data() + lo, hi - lo);
                }
                if (settingsIndexTables) {
                    for (size_t table : { 0x16C8, 0x1898, 0x1968 }) {
                        std::memcpy(header + table, settings.data() + table, 0x90);
                    }
                }
            }
            for (size_t table : { 0x16C8, 0x1898, 0x1968 }) {
                if (!settingsIndexTables) {
                    std::memset(header + table + 0x10, 0xFF, 0x80);
                }
            }
            constexpr size_t VIS_BUFFERS = 0x1F8;
            constexpr size_t VIS_BUFFER_SIZE = 0x10;
            for (size_t i = 0; i < 16; i++) {
                *(uint32_t*)(header + VIS_BUFFERS + i * 0x10) = i < 8 ? smodelVisSize : (uint32_t)VIS_BUFFER_SIZE;
                *(uint64_t*)(header + VIS_BUFFERS + i * 0x10 + 8) = fastfile::linker::memory::POINTER_NEXT;
            }
            /*
             * the dyn ents' dpvs (0x498, sub_142E1F430): count of list 0 (the col_map's client dyn ent slots, the gibs), 4
             * view buffers and one vis byte each (sub_143699270), 16 rounded, in the runtime block. As the frontend's.
             */
            constexpr size_t DPVS_DYN = 0x498;
            constexpr uint32_t DYNENT_SLOTS = 500;
            constexpr uint32_t DYNENT_BUFFER_SIZE = (DYNENT_SLOTS + 15) & ~15u;
            *(uint32_t*)(header + DPVS_DYN) = DYNENT_SLOTS;
            for (size_t j = 0; j < 4; j++) {
                *(uint32_t*)(header + DPVS_DYN + 0x08 + 0x20 * j) = DYNENT_BUFFER_SIZE;
                *(uint64_t*)(header + DPVS_DYN + 0x10 + 0x20 * j) = fastfile::linker::memory::POINTER_NEXT;
            }
            *(uint32_t*)(header + DPVS_DYN + 0x88) = DYNENT_BUFFER_SIZE;
            *(uint64_t*)(header + DPVS_DYN + 0x90) = fastfile::linker::memory::POINTER_NEXT;
            // 8 bytes per dyn ent at 0x1B0 (runtime, before the dpvs), cleared by sub_143557C30 with that count
            constexpr size_t DYNENT_DATA = 0x1B0;
            *(uint64_t*)(header + DYNENT_DATA) = fastfile::linker::memory::POINTER_NEXT;

            // the static models in the dpvs (0x1B8): count, cull tree (0x340), draw insts (0x400), placements, 0x410, bytes
            constexpr size_t DPVS = 0x1B8;
            constexpr size_t SMODEL_TREE = DPVS + 0x188;
            constexpr size_t SMODEL_TREE_DATA = 0x10 + 0x38;
            if (smodelCount) {
                *(uint32_t*)(header + DPVS + 0x4) = smodelCount;
                *(uint64_t*)(header + SMODEL_TREE) = SMODEL_TREE_DATA + 0x18 * (size_t)smodelCount;
                *(uint64_t*)(header + SMODEL_TREE + 0x8) = fastfile::linker::memory::POINTER_NEXT;
                *(uint64_t*)(header + SMODEL_TREE + 0x10) = smodelCount;
                *(uint64_t*)(header + SMODEL_TREE + 0x18) = fastfile::linker::memory::POINTER_NEXT;
                *(uint64_t*)(header + SMODEL_TREE + 0x20) = smodelCount;
                *(uint64_t*)(header + SMODEL_TREE + 0x28) = fastfile::linker::memory::POINTER_NEXT;
                // the sun's dynamic cascades' models (see above)
                *(uint32_t*)(header + DPVS + 0x220) = smodelCount;
                *(uint64_t*)(header + DPVS + 0x228) = fastfile::linker::memory::POINTER_NEXT;
                *(uint64_t*)(header + DPVS + 0x248) = fastfile::linker::memory::POINTER_NEXT;
                *(uint64_t*)(header + DPVS + 0x250) = fastfile::linker::memory::POINTER_NEXT;
                *(uint64_t*)(header + DPVS + 0x258) = fastfile::linker::memory::POINTER_NEXT;
                *(uint64_t*)(header + DPVS + 0x298) = smodelVisSize;
                *(uint64_t*)(header + DPVS + 0x2A0) = fastfile::linker::memory::POINTER_NEXT;
                // the surfaces' tree too: each cull queue gets its root only when that tree has indices (sub_143581720)
                constexpr size_t SURFACE_TREE = DPVS + 0x1B8;
                *(uint64_t*)(header + SURFACE_TREE) = SMODEL_TREE_DATA;
                *(uint64_t*)(header + SURFACE_TREE + 0x8) = fastfile::linker::memory::POINTER_NEXT;
                *(uint64_t*)(header + SURFACE_TREE + 0x10) = 1;
                *(uint64_t*)(header + SURFACE_TREE + 0x18) = fastfile::linker::memory::POINTER_NEXT;
                *(uint64_t*)(header + SURFACE_TREE + 0x20) = 1;
                *(uint64_t*)(header + SURFACE_TREE + 0x28) = fastfile::linker::memory::POINTER_NEXT;
            }
            const uint64_t nameHash{ ctx.HashXHash(assetName, true) };
            ((XHash*)header)->name = nameHash;

            auto ReadVec = [&](const char* key, size_t off, float def) {
                float* v{ (float*)(header + off) };
                auto vIt{ cfg.main.FindMember(key) };
                for (rapidjson::SizeType i = 0; i < 3; i++) {
                    v[i] = vIt != cfg.main.MemberEnd() && vIt->value.IsArray() && i < vIt->value.Size() && vIt->value[i].IsNumber()
                        ? vIt->value[i].GetFloat() : def;
                }
            };
            ReadVec("mins", 0x190, -4096);
            ReadVec("maxs", 0x19C, 4096);
            *(uint32_t*)(header + 0x1A8) = (uint32_t)cfg.GetInteger("checksum", 0);
            /*
             * The sun shadow cascades' caster lists (0x2F8 + 0x18 * i: count, capacity 0x300, list 0x308): the cascade jobs
             * (sub_143582B50, sub_143509D00) append to them and walk min(count, capacity), so a capacity 0 draws nothing in
             * the cascades. They're the dpvs' (+0x140): its loader (sub_142E1F510) reads each list's capacity u32s from the
             * file after the vis buffers, so a capacity without its list crashes the first cull (memcpy to null).
             * zm_towers': 4000, 16000, 0
             */
            uint32_t casterCapacities[3]{};
            if (auto capIt{ cfg.main.FindMember("shadowCasterLists") }; capIt != cfg.main.MemberEnd() && capIt->value.IsArray()) {
                for (rapidjson::SizeType i = 0; i < capIt->value.Size() && i < 3; i++) {
                    casterCapacities[i] = capIt->value[i].GetUint();
                    *(uint32_t*)(header + 0x300 + 0x18 * i) = casterCapacities[i];
                    if (casterCapacities[i]) {
                        *(uint64_t*)(header + 0x308 + 0x18 * i) = fastfile::linker::memory::POINTER_NEXT;
                    }
                }
            }

            if (!baseName.empty()) {
                *(uint64_t*)(header + 0x10) = fastfile::linker::memory::POINTER_NEXT;
            }
            for (size_t i = 0; i < ACTS_ARRAYSIZE(rawArrays); i++) {
                const RawArray& arr{ rawArrays[i] };
                if (arrays[i].empty()) {
                    continue;
                }
                uint64_t count{ arrays[i].size() / arr.elemSize };
                uint64_t old{ arr.countSize == 8 ? *(uint64_t*)(header + arr.count) : *(uint32_t*)(header + arr.count) };
                if (old && old != count) {
                    LOG_ERROR("gfx_map {}: the arrays sharing the count at 0x{:x} have {} and {} elements", path.string(), arr.count, old, count);
                    ctx.error = true;
                    ff.data.PopStream();
                    return;
                }
                if (arr.countSize == 8) {
                    *(uint64_t*)(header + arr.count) = count;
                }
                else {
                    *(uint32_t*)(header + arr.count) = (uint32_t)count;
                }
                *(uint64_t*)(header + arr.ptr) = fastfile::linker::memory::POINTER_NEXT;
            }

            ff.data.PushStream(XFILE_BLOCK_VIRTUAL);
            if (!baseName.empty()) {
                ff.data.WriteStream(baseName.c_str(), baseName.size() + 1);
            }
            std::string lighting{ cfg.GetString("lighting", "") };
            std::string streamerWorld{ cfg.GetString("streamerworld", "") };
            // the images by name (the game's), each where the loader takes it
            auto LinkImage = [&](const char* key, size_t off) {
                std::string image{ cfg.GetString(key, "") };
                if (!image.empty()) {
                    ctx.LinkAsset(XAssetType::ASSET_TYPE_IMAGE, std::format(",{}", image).data(), *(void**)(header + off), false, &ff);
                }
            };
            *(uint32_t*)(header + 0xE8) = (uint32_t)cfg.GetInteger("cookieCount", 0);
            size_t written{};
            for (size_t i = 0; i < ACTS_ARRAYSIZE(rawArrays); i++) {
                // the draw's (sub_142E1FC20) before its vertices
                if (rawArrays[i].ptr == 0x100) {
                    LinkImage("iesProfileImage", 0xE0);
                    LinkImage("cookieImage", 0xF0);
                }
                // the volume decals' reveal texture after 0x588 (and the decals)
                if (rawArrays[i].ptr == 0x5F0) {
                    LinkImage("decalRevealImage", 0x5A8);
                }
                // the lighting and the streamerworld come after 0x17F0 in the loader
                if (rawArrays[i].ptr == 0x1930) {
                    if (!lighting.empty()) {
                        ctx.LinkAsset(XAssetType::ASSET_TYPE_LIGHTING, lighting.data(), *(void**)(header + 0x17F8), false, &ff);
                    }
                    if (!streamerWorld.empty()) {
                        ctx.LinkAsset(XAssetType::ASSET_TYPE_STREAMERWORLD, streamerWorld.data(), *(void**)(header + 0x1800), false, &ff);
                    }
                }
                // the dpvs (sub_142E1F510) after the models: its vis buffers in the runtime block, not in the file
                if (rawArrays[i].ptr == 0x588) {
                    ff.data.PushStream(XFILE_BLOCK_RUNTIME_VIRTUAL);
                    ff.data.Align(4);
                    ff.data.AllocRuntime(8 * DYNENT_SLOTS);
                    for (size_t j = 0; j < 16; j++) {
                        ff.data.Align(16);
                        ff.data.AllocRuntime(j < 8 ? smodelVisSize : VIS_BUFFER_SIZE);
                    }
                    ff.data.PopStream();
                    // the caster lists (see above), zeros in the file
                    for (uint32_t capacity : casterCapacities) {
                        if (capacity) {
                            ff.data.Align(4);
                            std::vector<byte> list(4 * (size_t)capacity, 0);
                            ff.data.WriteStream(list.data(), list.size());
                        }
                    }
                    if (smodelCount) {
                        /*
                         * the cull tree: 0x10 bytes, the root (bounds, [begin, directEnd, testedEnd, end) = 0, 0, N, N), each
                         * model's bounds after it (the tested ones are marked visible against them, sub_14357E980), its 2 lists
                         */
                        std::vector<byte> treeData(SMODEL_TREE_DATA + 0x18 * (size_t)smodelCount, 0);
                        byte* tree{ treeData.data() };
                        float* bounds{ (float*)(tree + 0x10) };
                        for (size_t j = 0; j < 3; j++) {
                            bounds[j] = 1e30f;
                            bounds[3 + j] = -1e30f;
                        }
                        for (const StaticModel& sm : staticModels) {
                            for (size_t j = 0; j < 3; j++) {
                                bounds[j] = std::min(bounds[j], sm.mins[j]);
                                bounds[3 + j] = std::max(bounds[3 + j], sm.maxs[j]);
                            }
                        }
                        *(uint32_t*)(tree + 0x10 + 0x20) = smodelCount;
                        *(uint32_t*)(tree + 0x10 + 0x24) = smodelCount;
                        *(uint32_t*)(tree + 0x10 + 0x30) = (uint32_t)SMODEL_TREE_DATA;
                        for (uint32_t j = 0; j < smodelCount; j++) {
                            float* objectBounds{ (float*)(tree + SMODEL_TREE_DATA + 0x18 * j) };
                            std::memcpy(objectBounds, staticModels[j].mins, 0xC);
                            std::memcpy(objectBounds + 3, staticModels[j].maxs, 0xC);
                        }
                        ff.data.WriteStream(treeData.data(), treeData.size());
                        std::vector<uint32_t> order(smodelCount);
                        for (uint32_t j = 0; j < smodelCount; j++) {
                            order[j] = j;
                        }
                        ff.data.Align(4);
                        ff.data.WriteStream(order.data(), order.size() * sizeof(uint32_t));
                        ff.data.Align(4);
                        ff.data.WriteStream(order.data(), order.size() * sizeof(uint32_t));

                        // the surfaces' tree (+0x1B8): a root over the same bounds with no surfaces in it, 1 index each list
                        byte surfaceTree[SMODEL_TREE_DATA]{};
                        std::memcpy(surfaceTree + 0x10, tree + 0x10, 0x18);
                        ff.data.WriteStream(surfaceTree, sizeof(surfaceTree));
                        ff.data.Align(4);
                        ff.data.WriteStream<uint32_t>(0);
                        ff.data.Align(4);
                        ff.data.WriteStream<uint32_t>(0);

                        // the sun's dynamic cascades' models: all
                        ff.data.Align(4);
                        ff.data.WriteStream(order.data(), order.size() * sizeof(uint32_t));

                        // the draw insts, then each one's model (by name, the game's or another zone's)
                        ff.data.Align(8);
                        std::vector<byte> insts(0x38 * (size_t)smodelCount);
                        for (uint32_t j = 0; j < smodelCount; j++) {
                            byte* inst{ insts.data() + 0x38 * j };
                            *(uint64_t*)inst = fastfile::linker::memory::POINTER_NEXT;
                            *(int32_t*)(inst + 0x20) = -1;
                            *(uint16_t*)(inst + 0x2E) = 0xFFFF;
                            *(uint32_t*)(inst + 0x30) = j;
                            *(int32_t*)(inst + 0x34) = -1;
                        }
                        ff.data.WriteStream(insts.data(), insts.size());
                        for (const StaticModel& sm : staticModels) {
                            void* ref{};
                            ctx.LinkAsset(XAssetType::ASSET_TYPE_XMODEL, std::format(",{}", sm.model).data(), ref, false, &ff);
                        }
                        // the placements
                        ff.data.Align(4);
                        for (uint32_t j = 0; j < smodelCount; j++) {
                            const StaticModel& sm{ staticModels[j] };
                            byte placement[0x40]{};
                            float* f{ (float*)placement };
                            std::memcpy(f, sm.quat, 0x10);
                            std::memcpy(f + 4, sm.origin, 0xC);
                            f[7] = sm.scale;
                            std::memcpy(f + 8, sm.mins, 0xC);
                            std::memcpy(f + 11, sm.maxs, 0xC);
                            *(uint32_t*)(placement + 0x38) = j;
                            f[15] = sm.radius;
                            ff.data.WriteStream(placement, sizeof(placement));
                        }
                        ff.data.Align(8);
                        std::vector<byte> zeros(8 * (size_t)smodelCount, 0);
                        ff.data.WriteStream(zeros.data(), zeros.size());
                        std::vector<byte> flags(smodelVisSize, 3);
                        ff.data.WriteStream(flags.data(), flags.size());
                    }
                    // the dyn ents' dpvs buffers (0x498) after the dpvs
                    ff.data.PushStream(XFILE_BLOCK_RUNTIME_VIRTUAL);
                    for (size_t j = 0; j < 5; j++) {
                        ff.data.Align(16);
                        ff.data.AllocRuntime(DYNENT_BUFFER_SIZE);
                    }
                    ff.data.PopStream();
                    // then (after the 0x498 struct and the lut materials) the 4 images at 0x550
                    auto images550{ cfg.main.FindMember("images550") };
                    if (images550 != cfg.main.MemberEnd() && images550->value.IsArray()) {
                        for (rapidjson::SizeType j = 0; j < images550->value.Size() && j < 4; j++) {
                            if (images550->value[j].IsString()) {
                                ctx.LinkAsset(XAssetType::ASSET_TYPE_IMAGE, std::format(",{}", images550->value[j].GetString()).data(),
                                    *(void**)(header + 0x550 + 8 * j), false, &ff);
                            }
                        }
                    }
                }
                if (arrays[i].empty()) {
                    continue;
                }
                ff.data.Align(rawArrays[i].align);
                ff.data.WriteStream(arrays[i].data(), arrays[i].size());
                written++;
            }
            ff.data.PopStream();
            ff.data.PopStream();

            LOG_INFO("Added asset gfx_map {} (hash_{:x}), {} raw arrays", assetName, nameHash, written);
        }
    };

    utils::MapAdder<XAssetLinkerImpl, XAssetType, XAssetLinker> impl{ GetWorkers(), XAssetType::ASSET_TYPE_GFXWORLD };
} // namespace
