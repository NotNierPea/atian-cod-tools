#include "editor.hpp"
#include <misc/cpp/imgui_stdlib.h>
#include <algorithm>
#include <fstream>

/*
 * The lighting panel: the lighting linker's json (linker_bo4_map_lighting.cpp: "exposure", "probeColor", "lightGrid",
 * "sky") and the values known in a lighting state (0x14C0, its plain part 4 blocks of 0x394 from 0x50, sub_143660730):
 * the sun at +0x00..+0x1C (2 angles, a colour, the illuminance) and again at +0x178 (the illuminance, then the sun
 * disk's radiance), the exposure's EV clamp at +0x6C / +0x70, the height fog from +0x2A8. Block 0 is zm_towers' sun,
 * block 1 a second warmer one, 2 / 3 unused there. Presets keep the json's values and the state files whole.
 */
namespace editor {
    namespace {
        constexpr size_t STATE_SIZE{ 0x14C0 };
        constexpr size_t BLOCK_BASE{ 0x50 };
        constexpr size_t BLOCK_SIZE{ 0x394 };
        constexpr int BLOCKS{ 4 };
        // the fog's parts (the frontend's off, zm_towers' sand fog)
        constexpr std::pair<size_t, size_t> FOG_RANGES[]{ { 0x2A8, 0x2E4 }, { 0x33C, 0x378 } };

        struct Field {
            const char* label;
            size_t offset;
            int count;
            float speed;
            const char* help;
            bool color{};
        };
        const Field SUN_FIELDS[]{
            { "Sun angle A", 0x00, 1, 0.5f, "degrees, unverified which axis (zm_towers 137.5)" },
            { "Sun angle B", 0x04, 1, 0.5f, "degrees, unverified which axis (zm_towers 137.5)" },
            { "Sun colour", 0x08, 3, 0.01f, "the sun's tint (zm_towers' block 1: 1, 0.85, 0.6)", true },
        };
        const Field EXPOSURE_FIELDS[]{
            { "Exposure compensation", 0x68, 1, 0.05f, "unverified (0 in zm_towers)" },
            { "EV min", 0x6C, 1, 0.05f, "the auto exposure's darkest clamp (zm_towers 8)" },
            { "EV max", 0x70, 1, 0.05f, "the auto exposure's brightest clamp (zm_towers 14.5): the light read is clamped to 2^EV x 0.18, raise it for a brighter scene" },
        };
        const Field FOG_FIELDS[]{
            { "Fog height", 0x2A8, 1, 1.0f, "zm_towers -500, the frontend's (no fog) 1000" },
            { "Fog falloff", 0x2AC, 1, 0.0001f, "zm_towers 0.001, the frontend 0.0057" },
            { "Fog distance", 0x2B0, 1, 10.0f, "zm_towers 3000, the frontend 3937" },
            { "Fog +0x2B4", 0x2B4, 1, 0.1f, "unknown (zm_towers 32, the frontend 31)" },
            { "Fog +0x2D4", 0x2D4, 1, 0.1f, "unknown, likely the colour's scale (zm_towers 4, the frontend 1)" },
            { "Fog colour", 0x2D8, 3, 1.0f, "zm_towers' sand: 2048, 1766, 1285.5 (HDR), the frontend 0" },
        };

        float* StateFloat(LightingState& s, int block, size_t off) {
            size_t at{ BLOCK_BASE + BLOCK_SIZE * block + off };
            return s.data.size() >= at + 4 ? reinterpret_cast<float*>(s.data.data() + at) : nullptr;
        }

        // a field of the selected block, written to it or to every block
        void DrawField(LightingState& s, int block, bool allBlocks, const Field& f) {
            float* v{ StateFloat(s, block, f.offset) };
            if (!v || !StateFloat(s, block, f.offset + 4 * (f.count - 1))) return;
            float values[4]{};
            std::copy(v, v + f.count, values);
            bool changed{};
            if (f.color) {
                changed = ImGui::ColorEdit3(f.label, values, ImGuiColorEditFlags_Float | ImGuiColorEditFlags_HDR);
            }
            else if (f.count == 1) {
                changed = ImGui::DragFloat(f.label, values, f.speed, 0, 0, "%.4g");
            }
            else {
                changed = ImGui::DragScalarN(f.label, ImGuiDataType_Float, values, f.count, f.speed, nullptr, nullptr, "%.4g");
            }
            ImGui::SameLine();
            HelpMarker(f.help);
            if (!changed) return;
            for (int b = 0; b < BLOCKS; b++) {
                if (!allBlocks && b != block) continue;
                if (float* t{ StateFloat(s, b, f.offset) }) std::copy(values, values + f.count, t);
            }
            s.dirty = true;
        }

        // the illuminance: +0x14 and +0x178, the disk's radiance (+0x17C..+0x188) scaled with it
        void DrawIlluminance(LightingState& s, int block, bool allBlocks) {
            float* v{ StateFloat(s, block, 0x14) };
            if (!v) return;
            float value{ *v };
            bool changed{ ImGui::DragFloat("Sun illuminance", &value, 64.0f, 0, 1e7f, "%.1f") };
            ImGui::SameLine();
            HelpMarker("the sun's strength (zm_towers 32768 in block 0, 11585 in block 1). Also written at +0x178, the sun disk's "
                "radiance (+0x17C..+0x188) scaled by the same amount. The frontend has 0 there: a dark world.");
            if (!changed || value < 0) return;
            for (int b = 0; b < BLOCKS; b++) {
                if (!allBlocks && b != block) continue;
                float* illum{ StateFloat(s, b, 0x14) };
                float* disk{ StateFloat(s, b, 0x178) };
                if (!illum || !disk) continue;
                float old{ *illum };
                if (old > 0) {
                    for (size_t o = 0x17C; o <= 0x188; o += 4) *StateFloat(s, b, o) *= value / old;
                }
                *illum = value;
                if (*disk != 0 || old == 0) *disk = value;
            }
            s.dirty = true;
        }

        // the json's numbers, written with 7 digits (not a float's double: 2511.199951171875)
        float JsonFloat(const json& j, float def = 0) { return j.is_number() ? j.get<float>() : def; }

        double Num(float v) {
            char buff[32];
            snprintf(buff, sizeof(buff), "%.7g", v);
            return strtod(buff, nullptr);
        }

        bool DrawJsonFloat(json& obj, const char* key, const char* label, float speed, const char* help) {
            float v{ obj.contains(key) ? JsonFloat(obj[key]) : 0 };
            bool changed{ ImGui::DragFloat(label, &v, speed, 0, 0, "%.4g") };
            ImGui::SameLine();
            HelpMarker(help);
            if (changed) obj[key] = Num(v);
            return changed;
        }

        bool DrawJsonString(json& obj, const char* key, const char* label, const char* help) {
            std::string v{ obj.contains(key) && obj[key].is_string() ? obj[key].get<std::string>() : "" };
            bool changed{ ImGui::InputText(label, &v) };
            ImGui::SameLine();
            HelpMarker(help);
            if (changed) obj[key] = v;
            return changed;
        }

        bool DrawJsonFloats(json& obj, const char* key, size_t at, int count, const char* label, float speed, bool color, const char* help) {
            json& arr{ obj[key] };
            if (!arr.is_array()) arr = json::array();
            while (arr.size() < at + count) arr.push_back(color ? 1.0f : 0.0f);
            float values[4]{};
            for (int i = 0; i < count; i++) values[i] = JsonFloat(arr[at + i]);
            bool changed{ color
                ? ImGui::ColorEdit3(label, values, ImGuiColorEditFlags_Float | ImGuiColorEditFlags_HDR)
                : ImGui::DragScalarN(label, ImGuiDataType_Float, values, count, speed, nullptr, nullptr, "%.4g") };
            ImGui::SameLine();
            HelpMarker(help);
            if (changed) {
                for (int i = 0; i < count; i++) arr[at + i] = Num(values[i]);
            }
            return changed;
        }

        // the lighting state files near the json (the dumps of game maps'), to copy the fog from
        std::vector<fs::path> FindStateFiles(const fs::path& dir) {
            std::vector<fs::path> out{};
            std::error_code ec;
            for (const fs::directory_entry& e : fs::recursive_directory_iterator{ dir, ec }) {
                if (e.is_regular_file(ec) && e.path().extension() == ".bin" && e.path().filename().string().rfind("lighting_state", 0) == 0) {
                    out.push_back(e.path());
                }
            }
            std::sort(out.begin(), out.end());
            return out;
        }

        // presets: <lighting json's folder>/lighting_presets/<name>.json
        fs::path PresetDir(const Lighting& l) { return l.path.parent_path() / "lighting_presets"; }

        std::string Hex(const std::vector<uint8_t>& data) {
            static const char* digits{ "0123456789abcdef" };
            std::string s(data.size() * 2, '0');
            for (size_t i = 0; i < data.size(); i++) {
                s[i * 2] = digits[data[i] >> 4];
                s[i * 2 + 1] = digits[data[i] & 15];
            }
            return s;
        }

        std::vector<uint8_t> Unhex(const std::string& s) {
            std::vector<uint8_t> out(s.size() / 2);
            for (size_t i = 0; i < out.size(); i++) {
                out[i] = (uint8_t)std::stoi(s.substr(i * 2, 2), nullptr, 16);
            }
            return out;
        }

        const char* PRESET_KEYS[]{ "exposure", "probeColor", "lightGrid", "sky" };

        bool SavePreset(Lighting& l, const std::string& name) {
            json preset{};
            preset["name"] = name;
            json values{};
            for (const char* k : PRESET_KEYS) {
                if (l.root.contains(k)) values[k] = l.root[k];
            }
            preset["lighting"] = values;
            json states{};
            for (const LightingState& s : l.states) {
                states[ToUtf8(fs::relative(s.path, l.path.parent_path()))] = Hex(s.data);
            }
            preset["states"] = states;
            std::error_code ec;
            fs::create_directories(PresetDir(l), ec);
            std::ofstream os{ PresetDir(l) / FromUtf8(name + ".json") };
            if (!os) {
                Logf(LogLevel::ERR, "can't write the preset %s", name.c_str());
                return false;
            }
            os << preset.dump(4);
            Logf(LogLevel::INFO, "saved the lighting preset %s", name.c_str());
            return true;
        }

        void ApplyPreset(Lighting& l, const fs::path& file) {
            std::ifstream is{ file };
            json preset{ json::parse(is, nullptr, false) };
            if (!preset.is_object()) {
                Logf(LogLevel::ERR, "not a valid preset: %s", ToUtf8(file).c_str());
                return;
            }
            if (preset.contains("lighting") && preset["lighting"].is_object()) {
                for (auto& [k, v] : preset["lighting"].items()) l.root[k] = v;
                l.dirty = true;
            }
            if (preset.contains("states") && preset["states"].is_object()) {
                for (LightingState& s : l.states) {
                    std::string rel{ ToUtf8(fs::relative(s.path, l.path.parent_path())) };
                    if (!preset["states"].contains(rel)) continue;
                    std::vector<uint8_t> data{ Unhex(preset["states"][rel].get<std::string>()) };
                    if (data.size() != s.data.size()) {
                        Logf(LogLevel::WARN, "preset state %s has another size, skipped", rel.c_str());
                        continue;
                    }
                    s.data = std::move(data);
                    s.dirty = true;
                }
            }
            Logf(LogLevel::INFO, "applied the preset %s (not saved yet)", ToUtf8(file.stem()).c_str());
        }

        std::vector<fs::path> ListPresets(const Lighting& l) {
            std::vector<fs::path> out{};
            std::error_code ec;
            for (const fs::directory_entry& e : fs::directory_iterator{ PresetDir(l), ec }) {
                if (e.is_regular_file(ec) && e.path().extension() == ".json") out.push_back(e.path());
            }
            std::sort(out.begin(), out.end());
            return out;
        }
    }

    void DrawLightingPanel(App& app) {
        Lighting& l{ app.lighting };
        if (!l.loaded) {
            l.Load(app.project);
        }

        bool statesDirty{ std::any_of(l.states.begin(), l.states.end(), [](const LightingState& s) { return s.dirty; }) };
        bool dirty{ l.dirty || statesDirty };

        ImGui::BeginDisabled(!dirty);
        if (ImGui::Button("Save")) l.Save();
        ImGui::SameLine();
        if (ImGui::Button("Save, link & deploy")) {
            if (l.Save()) app.build.Link(app.project, true);
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
        if (ImGui::Button(dirty ? "Revert" : "Reload")) l.Load(app.project);
        ImGui::SameLine();
        if (dirty) ImGui::TextColored(ImVec4(1, 0.8f, 0.35f, 1), "unsaved changes");
        else ImGui::TextDisabled("%s", ToUtf8(l.path).c_str());

        if (!l.error.empty()) {
            ImGui::TextColored(ImVec4(1, 0.45f, 0.4f, 1), "%s", l.error.c_str());
            return;
        }

        if (!ImGui::BeginTable("##lighting", 2, ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV, ImGui::GetContentRegionAvail())) {
            return;
        }
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::BeginChild("##json");
        ImGui::PushItemWidth(-200);

        if (ImGui::CollapsingHeader("Exposure and ambient", ImGuiTreeNodeFlags_DefaultOpen)) {
            l.dirty |= DrawJsonFloat(l.root, "exposure", "Light level", 10.0f,
                "the sun volume's light level where there are no probes (the auto exposure reads it): 0 is the most "
                "exposure there is, a white sky. zm_towers 2511.2, the frontend 0.205");
            l.dirty |= DrawJsonFloats(l.root, "probeColor", 0, 3, "Probe colour", 0.01f, true,
                "the reflection probes' colour (one 1x1 cube each): the models' ambient is the light grid times it");
            if (ImGui::TreeNodeEx("Light grid", ImGuiTreeNodeFlags_DefaultOpen)) {
                static float uniform{ 300 };
                ImGui::DragFloat("##uniform", &uniform, 1.0f, 0, 100000, "%.1f");
                ImGui::SameLine();
                if (ImGui::Button("Set all")) {
                    json& arr{ l.root["lightGrid"] };
                    if (!arr.is_array() || arr.size() < 12) arr = json::array({ 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1 });
                    for (int t = 0; t < 3; t++) for (int c = 0; c < 3; c++) arr[t * 4 + c] = Num(uniform);
                    l.dirty = true;
                }
                ImGui::SameLine();
                HelpMarker("the 3 texels of the light grid's volume images (RGBA each): the ambient the models take. enh_test has 300");
                for (int t = 0; t < 3; t++) {
                    char label[32];
                    snprintf(label, sizeof(label), "Texel %d", t + 1);
                    l.dirty |= DrawJsonFloats(l.root, "lightGrid", t * 4, 4, label, 1.0f, false, "R, G, B, A");
                }
                ImGui::TreePop();
            }
        }

        if (ImGui::CollapsingHeader("Sky", ImGuiTreeNodeFlags_DefaultOpen)) {
            json& sky{ l.root["sky"] };
            if (sky.is_object()) sky = json::array({ sky });
            if (!sky.is_array()) sky = json::array();
            ImGui::TextDisabled("One entry a quality (the last repeats). Models and images are game assets by name.");
            for (size_t i = 0; i < sky.size(); i++) {
                json& e{ sky[i] };
                if (!e.is_object()) continue;
                ImGui::PushID((int)i);
                char title[32];
                snprintf(title, sizeof(title), "Sky %zu", i);
                if (ImGui::TreeNodeEx(title, ImGuiTreeNodeFlags_DefaultOpen)) {
                    l.dirty |= DrawJsonString(e, "model", "Model", "the sky's xmodel (zm_towers' hash_765436262f9edc7d), in a zone the map loads");
                    l.dirty |= DrawJsonString(e, "image", "Image", "the sky's image: zm_towers' are lat-long BC6H skies (0x2000 x 0x800)");
                    l.dirty |= DrawJsonFloat(e, "rotation", "Rotation", 1.0f, "zm_towers 258");
                    l.dirty |= DrawJsonFloat(e, "distance", "Distance", 100.0f, "zm_towers 99999, the frontend 8000");
                    l.dirty |= DrawJsonString(e, "lensFlare", "Lens flare", "the sun's lens flare (a KLF json, from the json's folder)");
                    if (ImGui::SmallButton("Copy to the other skies")) {
                        for (size_t j = 0; j < sky.size(); j++) if (j != i) sky[j] = e;
                        l.dirty = true;
                    }
                    ImGui::SameLine();
                    if (sky.size() > 1 && ImGui::SmallButton("Remove")) {
                        sky.erase(sky.begin() + i);
                        l.dirty = true;
                        ImGui::TreePop();
                        ImGui::PopID();
                        break;
                    }
                    ImGui::TreePop();
                }
                ImGui::PopID();
            }
            if (ImGui::SmallButton("Add a sky") && !sky.empty()) {
                sky.push_back(sky.back());
                l.dirty = true;
            }
        }

        if (ImGui::CollapsingHeader("Presets", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::TextDisabled("A preset keeps these values and the state files whole (day, night...).");
            static std::string presetName{ "day" };
            ImGui::InputText("##presetname", &presetName);
            ImGui::SameLine();
            ImGui::BeginDisabled(presetName.empty());
            if (ImGui::Button("Save as preset")) SavePreset(l, presetName);
            ImGui::EndDisabled();
            static fs::path toDelete{};
            for (const fs::path& p : ListPresets(l)) {
                ImGui::PushID(ToUtf8(p).c_str());
                ImGui::BulletText("%s", ToUtf8(p.stem()).c_str());
                ImGui::SameLine();
                if (ImGui::SmallButton("Apply")) ApplyPreset(l, p);
                ImGui::SameLine();
                if (ImGui::SmallButton("Delete")) {
                    toDelete = p;
                    ImGui::OpenPopup("Delete preset?");
                }
                if (ImGui::BeginPopupModal("Delete preset?", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
                    ImGui::Text("Delete the preset %s?", ToUtf8(toDelete.stem()).c_str());
                    if (ImGui::Button("Delete")) {
                        std::error_code ec;
                        fs::remove(toDelete, ec);
                        ImGui::CloseCurrentPopup();
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("Cancel")) ImGui::CloseCurrentPopup();
                    ImGui::EndPopup();
                }
                ImGui::PopID();
            }
        }
        ImGui::PopItemWidth();
        ImGui::EndChild();

        ImGui::TableSetColumnIndex(1);
        ImGui::BeginChild("##state");
        ImGui::PushItemWidth(-200);
        static size_t stateIndex{};
        static int block{};
        static bool allBlocks{};
        if (l.states.empty()) {
            ImGui::TextDisabled("No lighting state in the json's \"states\".");
        }
        else {
            stateIndex = std::min(stateIndex, l.states.size() - 1);
            LightingState& s{ l.states[stateIndex] };
            std::string label{ ToUtf8(fs::relative(s.path, l.path.parent_path())) + (s.dirty ? " *" : "") };
            if (ImGui::BeginCombo("State file", label.c_str())) {
                for (size_t i = 0; i < l.states.size(); i++) {
                    std::string name{ ToUtf8(fs::relative(l.states[i].path, l.path.parent_path())) };
                    if (ImGui::Selectable(name.c_str(), i == stateIndex)) stateIndex = i;
                }
                ImGui::EndCombo();
            }
            if (s.data.size() < STATE_SIZE) {
                ImGui::TextColored(ImVec4(1, 0.45f, 0.4f, 1), "not a lighting state (0x%zx bytes, 0x%zx expected)", s.data.size(), STATE_SIZE);
            }
            else {
                const char* blocks[]{ "Block 0 (zm_towers' sun)", "Block 1 (a warmer second one)", "Block 2 (unused in zm_towers)", "Block 3 (unused in zm_towers)" };
                ImGui::Combo("Block", &block, blocks, BLOCKS);
                ImGui::SameLine();
                ImGui::Checkbox("Write all blocks", &allBlocks);

                ImGui::SeparatorText("Sun");
                DrawIlluminance(s, block, allBlocks);
                for (const Field& f : SUN_FIELDS) DrawField(s, block, allBlocks, f);
                ImGui::SeparatorText("Exposure");
                for (const Field& f : EXPOSURE_FIELDS) DrawField(s, block, allBlocks, f);
                ImGui::SeparatorText("Fog");
                for (const Field& f : FOG_FIELDS) DrawField(s, block, allBlocks, f);

                // the fog of another state file, all blocks
                static std::vector<fs::path> others{};
                static fs::path othersDir{};
                static size_t otherIndex{};
                if (othersDir != l.path.parent_path()) {
                    othersDir = l.path.parent_path();
                    others = FindStateFiles(othersDir);
                    otherIndex = 0;
                }
                if (!others.empty()) {
                    otherIndex = std::min(otherIndex, others.size() - 1);
                    std::string other{ ToUtf8(fs::relative(others[otherIndex], othersDir)) };
                    if (ImGui::BeginCombo("##fogfrom", other.c_str())) {
                        for (size_t i = 0; i < others.size(); i++) {
                            std::string name{ ToUtf8(fs::relative(others[i], othersDir)) };
                            if (ImGui::Selectable(name.c_str(), i == otherIndex)) otherIndex = i;
                        }
                        ImGui::EndCombo();
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("Copy its fog")) {
                        std::vector<uint8_t> from{};
                        std::ifstream is{ others[otherIndex], std::ios::binary };
                        from.assign(std::istreambuf_iterator<char>{ is }, {});
                        if (from.size() == s.data.size()) {
                            for (int b = 0; b < BLOCKS; b++) {
                                for (auto [start, end] : FOG_RANGES) {
                                    size_t a{ BLOCK_BASE + BLOCK_SIZE * b + start };
                                    std::copy(from.begin() + a, from.begin() + a + (end - start), s.data.begin() + a);
                                }
                            }
                            s.dirty = true;
                            Logf(LogLevel::INFO, "copied the fog of %s (not saved yet)", other.c_str());
                        }
                        else {
                            Log(LogLevel::ERR, "that state file has another size");
                        }
                    }
                    ImGui::SameLine();
                    HelpMarker("its fog parts in all 4 blocks: maps/gfx/lighting_state0.bin is the frontend's (no fog), "
                        "gfx/towers/lighting_state0.bin zm_towers' sand fog");
                }
            }
        }
        ImGui::PopItemWidth();
        ImGui::EndChild();
        ImGui::EndTable();
    }
}
