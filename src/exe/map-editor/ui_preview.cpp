#include "editor.hpp"
#include <misc/cpp/imgui_stdlib.h>
#include <algorithm>
#include <fstream>
#include <set>
#include <sstream>

namespace editor {
    namespace {
        fs::path selected{};

        const std::set<std::string> IMAGE_EXT{ ".png", ".jpg", ".jpeg", ".tga", ".bmp", ".gif", ".psd", ".hdr" };
        const std::set<std::string> TEXT_EXT{ ".json", ".gsc", ".csc", ".gsh", ".lua", ".csv", ".zone", ".txt", ".md", ".py",
            ".cfg", ".ini", ".gdt", ".str", ".xml", ".toml", ".yaml", ".yml", ".obj", ".mtl", ".h", ".hpp", ".c", ".cpp", ".tsv" };
        // past this a text file is shown read only, its start
        constexpr uintmax_t EDIT_LIMIT{ 4 * 1024 * 1024 };
        constexpr size_t READ_ONLY_SHOWN{ 512 * 1024 };

        // the open text file
        struct TextDoc {
            fs::path path{};
            std::string text{};
            std::string original{};
            bool readOnly{};
            std::string error{};
        } doc{};

        std::string Ext(const fs::path& p) {
            std::string e{ ToUtf8(p.extension()) };
            std::transform(e.begin(), e.end(), e.begin(), [](unsigned char c) { return (char)std::tolower(c); });
            return e;
        }

        void LoadDoc(const fs::path& p) {
            doc = {};
            doc.path = p;
            std::error_code ec;
            uintmax_t size{ fs::file_size(p, ec) };
            std::ifstream is{ p, std::ios::binary };
            if (!is) {
                doc.error = "can't read the file";
                return;
            }
            if (size > EDIT_LIMIT) {
                doc.readOnly = true;
                doc.text.resize(READ_ONLY_SHOWN);
                is.read(doc.text.data(), (std::streamsize)doc.text.size());
                doc.text.resize((size_t)is.gcount());
                doc.text += "\n\n... (read only: the first 512 KB of a file over 4 MB)";
            }
            else {
                std::stringstream ss;
                ss << is.rdbuf();
                doc.text = ss.str();
            }
            doc.original = doc.text;
        }

        bool SaveDoc() {
            std::ofstream os{ doc.path, std::ios::binary };
            if (!os) {
                Logf(LogLevel::ERR, "can't write %s", ToUtf8(doc.path).c_str());
                return false;
            }
            os.write(doc.text.data(), (std::streamsize)doc.text.size());
            doc.original = doc.text;
            Logf(LogLevel::INFO, "saved %s", ToUtf8(doc.path).c_str());
            // the zone's own file: its entries again
            App& app{ GetApp() };
            if (!app.project.zones.empty() && doc.path == app.project.zones[app.project.zoneIndex]) {
                app.project.Reload();
            }
            return true;
        }

        void DrawHex(const fs::path& p) {
            std::ifstream is{ p, std::ios::binary };
            std::vector<uint8_t> data(4096);
            is.read((char*)data.data(), (std::streamsize)data.size());
            data.resize((size_t)is.gcount());
            ImGui::TextDisabled("binary file, its first %zu bytes", data.size());
            ImGui::BeginChild("##hex", ImVec2(0, 0), ImGuiChildFlags_Border);
            ImGuiListClipper clipper{};
            clipper.Begin((int)((data.size() + 15) / 16));
            while (clipper.Step()) {
                for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; row++) {
                    char line[128];
                    int n{ snprintf(line, sizeof(line), "%06X  ", row * 16) };
                    std::string ascii{};
                    for (int i = 0; i < 16; i++) {
                        size_t at{ (size_t)row * 16 + i };
                        if (at < data.size()) {
                            n += snprintf(line + n, sizeof(line) - n, "%02X ", data[at]);
                            ascii += data[at] >= 32 && data[at] < 127 ? (char)data[at] : '.';
                        }
                        else {
                            n += snprintf(line + n, sizeof(line) - n, "   ");
                        }
                    }
                    ImGui::Text("%s %s", line, ascii.c_str());
                }
            }
            ImGui::EndChild();
        }
    }

    void SelectFile(const fs::path& file) { selected = file; }
    const fs::path& SelectedFile() { return selected; }

    void DrawPreview(const fs::path& file) {
        if (file.empty()) {
            ImGui::TextDisabled("Select a file to see it here.");
            return;
        }
        std::error_code ec;
        if (!fs::is_regular_file(file, ec)) {
            ImGui::TextColored(ImVec4(1, 0.45f, 0.4f, 1), "Missing: %s", ToUtf8(file).c_str());
            return;
        }

        std::string ext{ Ext(file) };
        bool isText{ TEXT_EXT.contains(ext) };
        if (isText && doc.path != file) {
            LoadDoc(file);
        }
        bool dirty{ isText && doc.path == file && doc.text != doc.original };

        ImGui::TextUnformatted(ToUtf8(file.filename()).c_str());
        ImGui::SameLine();
        uintmax_t size{ fs::file_size(file, ec) };
        if (size >= 1024 * 1024) ImGui::TextDisabled("%.1f MB", size / 1048576.0);
        else ImGui::TextDisabled("%.1f KB", size / 1024.0);
        if (dirty) {
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(1, 0.8f, 0.35f, 1), "modified");
        }

        if (ImGui::SmallButton("Open")) OpenInShell(file);
        ImGui::SameLine();
        if (ImGui::SmallButton("Show in Explorer")) ShowInExplorer(file);
        if (isText && !doc.readOnly) {
            ImGui::SameLine();
            ImGui::BeginDisabled(!dirty);
            if (ImGui::SmallButton("Save") || (dirty && ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_S))) SaveDoc();
            ImGui::SameLine();
            if (ImGui::SmallButton("Revert")) doc.text = doc.original;
            ImGui::EndDisabled();
        }
        ImGui::TextDisabled("%s", ToUtf8(file.parent_path()).c_str());
        ImGui::Separator();

        if (IMAGE_EXT.contains(ext)) {
            const Texture& tex{ GetTexture(file) };
            if (!tex.srv) {
                ImGui::TextColored(ImVec4(1, 0.45f, 0.4f, 1), "can't show it: %s", tex.error.c_str());
                return;
            }
            ImGui::TextDisabled("%d x %d", tex.width, tex.height);
            ImVec2 avail{ ImGui::GetContentRegionAvail() };
            float scale{ std::min(1.0f, std::min(avail.x / tex.width, avail.y / tex.height)) };
            // small images bigger, sharp
            if (tex.width * 4 < avail.x && tex.height * 4 < avail.y) scale = 4;
            ImVec2 size2{ tex.width * scale, tex.height * scale };
            ImVec2 pos{ ImGui::GetCursorScreenPos() };
            // a checkerboard under the alpha
            ImDrawList* dl{ ImGui::GetWindowDrawList() };
            const float cell{ 12 };
            for (float y = 0; y < size2.y; y += cell) {
                for (float x = 0; x < size2.x; x += cell) {
                    bool dark{ ((int)(x / cell) + (int)(y / cell)) % 2 == 0 };
                    dl->AddRectFilled(ImVec2(pos.x + x, pos.y + y), ImVec2(pos.x + std::min(x + cell, size2.x), pos.y + std::min(y + cell, size2.y)),
                        dark ? IM_COL32(60, 60, 60, 255) : IM_COL32(90, 90, 90, 255));
                }
            }
            ImGui::Image((ImTextureID)tex.srv, size2);
            return;
        }

        if (isText) {
            if (!doc.error.empty()) {
                ImGui::TextColored(ImVec4(1, 0.45f, 0.4f, 1), "%s", doc.error.c_str());
                return;
            }
            ImGuiInputTextFlags flags{ ImGuiInputTextFlags_AllowTabInput };
            if (doc.readOnly) flags |= ImGuiInputTextFlags_ReadOnly;
            ImGui::InputTextMultiline("##text", &doc.text, ImGui::GetContentRegionAvail(), flags);
            return;
        }

        DrawHex(file);
    }
}
