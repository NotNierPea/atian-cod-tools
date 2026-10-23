#include "editor.hpp"

namespace editor {
    void Scripts::Start(const fs::path& mapDir, const std::vector<std::string>& scripts) {
        cwd = mapDir;
        queue = scripts;
        failed = false;
        running = true;
    }

    void Scripts::Update(Project& project) {
        if (!running || process.Running()) {
            return;
        }
        // the one before done: stop the queue on a failure
        if (started) {
            started = false;
            if (process.ExitCode()) {
                Logf(LogLevel::ERR, "the script failed (exit code %d): the rest not run, the map reloaded as it is", process.ExitCode());
                failed = true;
                queue.clear();
            }
        }
        if (queue.empty()) {
            running = false;
            project.Reload();
            return;
        }
        std::string script{ queue.front() };
        queue.erase(queue.begin());
        std::wstring cmd{ L"\"" + FromUtf8(GetSettings().python).wstring() + L"\" \"" + FromUtf8(script).wstring() + L"\"" };
        Logf(LogLevel::INFO, "running %s", script.c_str());
        if (!process.Start(cmd, cwd)) {
            Logf(LogLevel::ERR, "can't run %s with %s (File > Settings: Python)", script.c_str(), GetSettings().python.c_str());
            failed = true;
            queue.clear();
            return;
        }
        started = true;
    }

    Build::~Build() {
        if (worker.joinable()) {
            worker.join();
        }
    }

    float Build::Seconds() const {
        return std::chrono::duration<float>(std::chrono::steady_clock::now() - started).count();
    }

    void Build::Link(Project& project, bool deploy) {
        if (Busy() || project.zones.empty()) {
            return;
        }
        Settings& settings{ GetSettings() };
        std::error_code ec;
        if (!fs::is_regular_file(settings.acts, ec)) {
            Logf(LogLevel::ERR, "acts.exe isn't at %s (File > Settings)", ToUtf8(settings.acts).c_str());
            return;
        }
        zoneName = project.ZoneName();
        stage = settings.StageFor(zoneName);
        // a clean stage: only this link's files get deployed
        fs::remove_all(stage, ec);
        fs::create_directories(stage, ec);

        const fs::path& zone{ project.zones[project.zoneIndex] };
        std::wstring cmd{ L"\"" + settings.acts.wstring() + L"\" fastfilelinker \"" + zone.wstring() + L"\" -o \"" + stage.wstring() + L"\"" };
        Logf(LogLevel::INFO, "linking %s into %s", ToUtf8(zone.filename()).c_str(), ToUtf8(stage).c_str());
        if (!process.Start(cmd, zone.parent_path())) {
            return;
        }
        step = Step::LINKING;
        deployAfter = deploy;
        started = std::chrono::steady_clock::now();
    }

    void Build::Deploy(Project& project) {
        if (step == Step::DEPLOYING || process.Running()) {
            return;
        }
        Settings& settings{ GetSettings() };
        if (zoneName.empty()) {
            zoneName = project.ZoneName();
            stage = settings.StageFor(zoneName);
        }
        std::error_code ec;
        fs::path ff{ stage / "zone" / (zoneName + ".ff") };
        fs::path out{ settings.gameDir / "project-bo4" };
        if (!fs::is_regular_file(ff, ec)) {
            Logf(LogLevel::ERR, "nothing to deploy: %s isn't linked (Link first)", ToUtf8(ff).c_str());
            step = Step::IDLE;
            return;
        }
        if (!fs::is_directory(out, ec)) {
            Logf(LogLevel::ERR, "no project-bo4 folder in %s (File > Settings)", ToUtf8(settings.gameDir).c_str());
            step = Step::IDLE;
            return;
        }
        if (IsProcessRunning(L"BlackOps4.exe")) {
            Log(LogLevel::ERR, "Black Ops 4 is running: close it before deploying (its zone files are in use)");
            step = Step::IDLE;
            return;
        }

        step = Step::DEPLOYING;
        started = std::chrono::steady_clock::now();
        workerDone = false;
        if (worker.joinable()) {
            worker.join();
        }
        worker = std::thread([this, from = stage, out]() {
            std::error_code ec;
            size_t files{};
            uintmax_t bytes{};
            for (const fs::directory_entry& e : fs::recursive_directory_iterator{ from, ec }) {
                if (!e.is_regular_file()) {
                    continue;
                }
                fs::path rel{ fs::relative(e.path(), from, ec) };
                fs::path to{ out / rel };
                fs::create_directories(to.parent_path(), ec);
                if (!fs::copy_file(e.path(), to, fs::copy_options::overwrite_existing, ec)) {
                    Logf(LogLevel::ERR, "can't copy %s: %s", ToUtf8(rel).c_str(), ec.message().c_str());
                    continue;
                }
                files++;
                bytes += e.file_size();
                Logf(LogLevel::INFO, "deployed %s", ToUtf8(rel).c_str());
            }
            Logf(LogLevel::INFO, "deploy done: %zu files, %.1f MB in %s", files, bytes / 1048576.0, ToUtf8(out).c_str());
            workerDone = true;
        });
    }

    void Build::Update(Project& project) {
        if (step == Step::LINKING && !process.Running()) {
            step = Step::IDLE;
            std::error_code ec;
            fs::path ff{ stage / "zone" / (zoneName + ".ff") };
            if (process.ExitCode() || !fs::is_regular_file(ff, ec)) {
                Logf(LogLevel::ERR, "link failed (exit code %d) after %.1f s", process.ExitCode(), Seconds());
                return;
            }
            Logf(LogLevel::INFO, "linked %s.ff (%.1f MB) in %.1f s", zoneName.c_str(), fs::file_size(ff, ec) / 1048576.0, Seconds());
            if (deployAfter) {
                Deploy(project);
            }
        }
        else if (step == Step::DEPLOYING && workerDone) {
            worker.join();
            step = Step::IDLE;
        }
    }
}
