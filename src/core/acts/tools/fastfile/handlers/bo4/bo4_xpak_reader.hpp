#pragma once
#include <tools/fastfile/fastfile_handlers.hpp>

namespace fastfile::handlers::bo4::xpak {
    // a zone whose xpaks are searched first (its name, the dumpers' PreXFileLoading)
    void AddHint(const char* zoneName);
    // the decoded data of the entry with this key in the game's xpaks (casc or the zone folder), size the data's
    bool ReadEntry(fastfile::FastFileOption& opt, uint64_t key, size_t size, std::vector<byte>& out);
} // namespace fastfile::handlers::bo4::xpak
