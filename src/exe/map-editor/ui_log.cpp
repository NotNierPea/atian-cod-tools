#include "editor.hpp"
#include <deque>

namespace editor {
    namespace {
        struct LogLine {
            LogLevel level;
            std::string text;
        };
        constexpr size_t MAX_LINES{ 50000 };
        std::mutex logMutex{};
        std::deque<LogLine> lines{};
        bool scrollDown{};
        FILE* logFile{};

        // an acts line's level by its tag
        LogLevel LevelOf(const LogLine& line) {
            if (line.level != LogLevel::PROCESS) return line.level;
            if (line.text.find("ERROR") != std::string::npos || line.text.find("[ERR") != std::string::npos) return LogLevel::ERR;
            if (line.text.find("WARN") != std::string::npos) return LogLevel::WARN;
            return LogLevel::PROCESS;
        }
    }

    void SetLogFile(const fs::path& path) {
        if (logFile) fclose(logFile);
        logFile = _wfopen(path.c_str(), L"w");
    }

    void Log(LogLevel level, const std::string& text) {
        std::lock_guard lg{ logMutex };
        if (logFile) {
            fprintf(logFile, "%s\n", text.c_str());
            fflush(logFile);
        }
        lines.push_back({ level, text });
        while (lines.size() > MAX_LINES) {
            lines.pop_front();
        }
        scrollDown = true;
    }

    void DrawLogPanel() {
        static bool problemsOnly{};
        static bool hideAssets{ true };
        static ImGuiTextFilter filter{};

        if (ImGui::SmallButton("Clear")) {
            std::lock_guard lg{ logMutex };
            lines.clear();
        }
        ImGui::SameLine();
        ImGui::Checkbox("Problems only", &problemsOnly);
        ImGui::SameLine();
        ImGui::Checkbox("Hide \"Added asset\" lines", &hideAssets);
        ImGui::SameLine();
        filter.Draw("Filter", 220);

        ImGui::BeginChild("##log", ImVec2(0, 0), ImGuiChildFlags_Border, ImGuiWindowFlags_HorizontalScrollbar);
        std::lock_guard lg{ logMutex };
        std::vector<const LogLine*> shown{};
        shown.reserve(lines.size());
        for (const LogLine& line : lines) {
            LogLevel level{ LevelOf(line) };
            if (problemsOnly && level != LogLevel::ERR && level != LogLevel::WARN) continue;
            if (hideAssets && line.level == LogLevel::PROCESS && line.text.find("Added asset") != std::string::npos) continue;
            if (!filter.PassFilter(line.text.c_str())) continue;
            shown.push_back(&line);
        }
        ImGuiListClipper clipper{};
        clipper.Begin((int)shown.size());
        while (clipper.Step()) {
            for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++) {
                const LogLine& line{ *shown[i] };
                ImVec4 color{ 0.85f, 0.85f, 0.85f, 1 };
                switch (LevelOf(line)) {
                case LogLevel::ERR: color = ImVec4(1.0f, 0.42f, 0.38f, 1); break;
                case LogLevel::WARN: color = ImVec4(1.0f, 0.8f, 0.35f, 1); break;
                case LogLevel::PROCESS: color = ImVec4(0.62f, 0.66f, 0.72f, 1); break;
                default: break;
                }
                ImGui::TextColored(color, "%s", line.text.c_str());
            }
        }
        if (scrollDown && ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - ImGui::GetTextLineHeight() * 4) {
            ImGui::SetScrollHereY(1.0f);
        }
        scrollDown = false;
        ImGui::EndChild();
    }
}
