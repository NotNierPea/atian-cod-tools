#include "editor.hpp"
#include <algorithm>

namespace editor {
    namespace {
        // the selected entry by its line: entries are rebuilt on a reload
        size_t selectedLine{};
        fs::path selectedZonePath{};

        const ZoneEntry* FindEntry(const ZoneFile& zone, size_t line) {
            for (const ZoneBlock& b : zone.blocks) {
                for (const ZoneEntry& e : b.entries) {
                    if (e.line == line) return &e;
                }
            }
            return nullptr;
        }

        const ImVec4 COLOR_MISSING{ 1.0f, 0.45f, 0.4f, 1 };
        const ImVec4 COLOR_NAME{ 0.6f, 0.62f, 0.66f, 1 };
        const ImVec4 COLOR_TYPE{ 0.55f, 0.75f, 1.0f, 1 };

        bool ContainsNoCase(const std::string& text, const std::string& needle) {
            if (needle.empty()) return true;
            return std::search(text.begin(), text.end(), needle.begin(), needle.end(),
                [](char a, char b) { return std::tolower((unsigned char)a) == std::tolower((unsigned char)b); }) != text.end();
        }

        // the two panes: the list on the left, the selected file on the right
        void SplitBegin(const char* id, float leftFraction) {
            ImGui::BeginTable(id, 2, ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_SizingStretchProp,
                ImGui::GetContentRegionAvail());
            ImGui::TableSetupColumn("left", ImGuiTableColumnFlags_WidthStretch, leftFraction);
            ImGui::TableSetupColumn("right", ImGuiTableColumnFlags_WidthStretch, 1 - leftFraction);
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
        }

        // the folder tree: listings kept until a reload
        struct DirListing {
            std::vector<fs::path> dirs{};
            std::vector<fs::path> files{};
        };
        std::map<fs::path, DirListing> listings{};

        const DirListing& List(const fs::path& dir) {
            auto it{ listings.find(dir) };
            if (it != listings.end()) return it->second;
            DirListing l{};
            std::error_code ec;
            for (const fs::directory_entry& e : fs::directory_iterator{ dir, ec }) {
                if (e.is_directory(ec)) l.dirs.push_back(e.path());
                else l.files.push_back(e.path());
            }
            std::sort(l.dirs.begin(), l.dirs.end());
            std::sort(l.files.begin(), l.files.end());
            return listings.emplace(dir, std::move(l)).first->second;
        }

        void DrawDir(const fs::path& dir) {
            const DirListing& l{ List(dir) };
            for (const fs::path& sub : l.dirs) {
                std::string label{ ToUtf8(sub.filename()) + "##" + ToUtf8(sub) };
                if (ImGui::TreeNodeEx(label.c_str(), ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth)) {
                    DrawDir(sub);
                    ImGui::TreePop();
                }
            }
            ImGuiListClipper clipper{};
            clipper.Begin((int)l.files.size());
            while (clipper.Step()) {
                for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++) {
                    const fs::path& f{ l.files[i] };
                    std::string label{ ToUtf8(f.filename()) + "##" + ToUtf8(f) };
                    ImGui::TreeNodeEx(label.c_str(), ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen |
                        ImGuiTreeNodeFlags_SpanAvailWidth | (SelectedFile() == f ? ImGuiTreeNodeFlags_Selected : 0));
                    if (ImGui::IsItemClicked()) {
                        SelectFile(f);
                    }
                    if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                        OpenInShell(f);
                    }
                }
            }
        }
    }

    void DrawZonePanel(App& app) {
        Project& project{ app.project };
        ZoneFile& zone{ project.zone };
        if (selectedZonePath != zone.path) {
            selectedLine = 0;
        }
        const ZoneEntry* selectedEntry{ selectedLine ? FindEntry(zone, selectedLine) : nullptr };

        static char filterBuff[256]{};
        static std::string typeFilter{};
        static bool missingOnly{};

        SplitBegin("##zonesplit", 0.42f);
        {
            ImGui::TextDisabled("game %s, compression %s", zone.Option("game").c_str(), zone.Option("compression").c_str());
            ImGui::Text("%zu entries, %zu blocks", zone.entries, zone.blocks.size());
            if (zone.missing) {
                ImGui::SameLine();
                ImGui::TextColored(COLOR_MISSING, "%zu missing", zone.missing);
            }
            ImGui::SameLine();
            if (ImGui::SmallButton("Edit .zone")) {
                SelectFile(zone.path);
                selectedLine = 0;
            }

            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.5f);
            ImGui::InputTextWithHint("##filter", "search type or path", filterBuff, sizeof(filterBuff));
            ImGui::SameLine();
            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.55f);
            std::string typeLabel{ typeFilter.empty() ? "all types" : typeFilter };
            if (ImGui::BeginCombo("##type", typeLabel.c_str())) {
                if (ImGui::Selectable("all types", typeFilter.empty())) typeFilter.clear();
                for (const auto& [type, count] : zone.typeCounts) {
                    std::string label{ type + " (" + std::to_string(count) + ")" };
                    if (ImGui::Selectable(label.c_str(), typeFilter == type)) typeFilter = type;
                }
                ImGui::EndCombo();
            }
            ImGui::SameLine();
            ImGui::Checkbox("Missing", &missingOnly);

            std::string needle{ filterBuff };
            bool filtering{ !needle.empty() || !typeFilter.empty() || missingOnly };

            ImGui::BeginChild("##blocks", ImVec2(0, 0), ImGuiChildFlags_Border);
            for (size_t b = 0; b < zone.blocks.size(); b++) {
                const ZoneBlock& block{ zone.blocks[b] };
                std::vector<const ZoneEntry*> shown{};
                for (const ZoneEntry& e : block.entries) {
                    if (missingOnly && e.state != EntryState::MISSING) continue;
                    if (!typeFilter.empty() && e.type != typeFilter) continue;
                    if (!ContainsNoCase(e.value, needle) && !ContainsNoCase(e.type, needle)) continue;
                    shown.push_back(&e);
                }
                if (shown.empty()) continue;

                std::string label{ block.name + "  (" + std::to_string(shown.size()) + ")##" + std::to_string(b) };
                if (filtering) ImGui::SetNextItemOpen(true, ImGuiCond_Always);
                if (block.marked) ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.75f, 0.88f, 1.0f, 1));
                bool open{ ImGui::TreeNodeEx(label.c_str(), ImGuiTreeNodeFlags_SpanAvailWidth) };
                if (block.marked) {
                    ImGui::PopStyleColor();
                    if (ImGui::IsItemHovered()) ImGui::SetTooltip("written by a tool (its begin / end markers), line %zu", block.line);
                }
                if (!open) continue;

                ImGuiListClipper clipper{};
                clipper.Begin((int)shown.size());
                while (clipper.Step()) {
                    for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++) {
                        const ZoneEntry& e{ *shown[i] };
                        ImGui::PushID(&e);
                        if (ImGui::Selectable("##e", selectedLine == e.line, ImGuiSelectableFlags_SpanAllColumns)) {
                            selectedLine = e.line;
                            selectedZonePath = zone.path;
                            SelectFile(e.state == EntryState::FILE ? e.file : fs::path{});
                        }
                        ImGui::SameLine();
                        ImGui::TextColored(COLOR_TYPE, "%s", e.type.c_str());
                        ImGui::SameLine();
                        ImVec4 color{ e.state == EntryState::MISSING ? COLOR_MISSING : e.state == EntryState::NAME ? COLOR_NAME : ImVec4(0.9f, 0.9f, 0.9f, 1) };
                        ImGui::TextColored(color, "%s", e.value.c_str());
                        if (!e.options.empty()) {
                            ImGui::SameLine();
                            ImGui::TextDisabled("%s", e.options.c_str());
                        }
                        ImGui::PopID();
                    }
                }
                ImGui::TreePop();
            }
            ImGui::EndChild();
        }

        ImGui::TableSetColumnIndex(1);
        {
            if (selectedEntry) {
                const ZoneEntry& e{ *selectedEntry };
                ImGui::TextColored(COLOR_TYPE, "%s", e.type.c_str());
                ImGui::SameLine();
                ImGui::TextUnformatted(e.value.c_str());
                ImGui::TextDisabled("line %zu%s%s", e.line, e.options.empty() ? "" : ", options ", e.options.c_str());
                if (e.state == EntryState::NAME) {
                    ImGui::TextColored(COLOR_NAME, "A name, not a file of the map: a stock asset or one another entry makes.");
                }
                ImGui::Separator();
            }
            DrawPreview(SelectedFile());
        }
        ImGui::EndTable();
    }

    void DrawFolderPanel(App& app) {
        static fs::path lastDir{};
        if (lastDir != app.project.dir) {
            listings.clear();
            lastDir = app.project.dir;
        }
        SplitBegin("##foldersplit", 0.35f);
        {
            if (ImGui::SmallButton("Refresh")) listings.clear();
            ImGui::SameLine();
            if (ImGui::SmallButton("Open folder")) OpenInShell(app.project.dir);
            ImGui::BeginChild("##tree", ImVec2(0, 0), ImGuiChildFlags_Border);
            DrawDir(app.project.dir);
            ImGui::EndChild();
        }
        ImGui::TableSetColumnIndex(1);
        DrawPreview(SelectedFile());
        ImGui::EndTable();
    }
}
