#include <includes.hpp>
#include <tools/fastfile/handlers/bo4/bo4_xpak_reader.hpp>
#include <utils/compress_utils.hpp>
#include <CascLib.h>

/*
 * The game's xpak entries by key. The index of an xpak (header 0x200: data section offset 0x20, index section offset
 * 0x38 / size 0x40) is 0x18 bytes entries {key, offset in the data section, size (bit 63 compressed)}. An entry is
 * groups at 0x80 aligned offsets: u32 chunk count (<= 30), u32 destination offset, u32[30] (type << 24) | size, the
 * chunks after; type 0 raw, 1/4 zlib, 2/3 lz4 block, 8 oodle (u32 size first). The groups are written in order, a
 * filler group writes where the next one writes after it (sub_1422B7E50). A big xpak is split in parts (x.xpak, x.xpak_1,
 * x.xpak_2...), its offsets running over them as one file.
 */
namespace fastfile::handlers::bo4::xpak {
    namespace {
        struct Location {
            std::string file;
            uint64_t offset;
            uint64_t size;
        };

        std::unordered_map<uint64_t, Location> entries{};
        std::vector<std::string> paks{};
        // the size of every pak file, the parts too
        std::unordered_map<std::string, uint64_t> sizes{};
        std::unordered_set<std::string> loadedPaks{};
        std::vector<std::string> hints{};
        bool listed{};

        bool ReadFileAt(fastfile::FastFileOption& opt, const std::string& file, uint64_t offset, void* out, size_t len) {
            if (!opt.cascStorage) {
                std::ifstream is{ file, std::ios::binary };
                if (!is) {
                    return false;
                }
                is.seekg((std::streamoff)offset);
                is.read((char*)out, (std::streamsize)len);
                return (size_t)is.gcount() == len;
            }
            HANDLE fh{};
            if (!CascOpenFile(opt.cascStorage, file.data(), CASC_LOCALE_ALL, 0, &fh)) {
                return false;
            }
            utils::CloseEnd ce{ [fh] { CascCloseFile(fh); } };
            ULONGLONG pos{};
            if (!CascSetFilePointer64(fh, (LONGLONG)offset, &pos, FILE_BEGIN)) {
                return false;
            }
            DWORD read{};
            return CascReadFile(fh, out, (DWORD)len, &read) && read == len;
        }

        // a read over the parts of a split pak: x.xpak then x.xpak_1, x.xpak_2... by their sizes
        bool ReadAt(fastfile::FastFileOption& opt, const std::string& file, uint64_t offset, void* out, size_t len) {
            byte* dst{ (byte*)out };
            for (size_t part = 0; len; part++) {
                std::string name{ part ? std::format("{}_{}", file, part) : file };
                auto it{ sizes.find(name) };
                if (it == sizes.end()) {
                    // not a listed part: a single file
                    return !part && ReadFileAt(opt, file, offset, dst, len);
                }
                if (offset >= it->second) {
                    offset -= it->second;
                    continue;
                }
                size_t n{ (size_t)std::min<uint64_t>(len, it->second - offset) };
                if (!ReadFileAt(opt, name, offset, dst, n)) {
                    return false;
                }
                dst += n;
                len -= n;
                offset = 0;
            }
            return true;
        }

        void ListPaks(fastfile::FastFileOption& opt) {
            if (listed) {
                return;
            }
            listed = true;
            if (opt.cascStorage) {
                CASC_FIND_DATA data;
                HANDLE find{ CascFindFirstFile(opt.cascStorage, "*.xpak*", &data, NULL) };
                if (!find) {
                    return;
                }
                do {
                    if (data.bFileAvailable) {
                        sizes[data.szFileName] = data.FileSize;
                        // the parts of a split pak are read through its first file
                        if (!std::strstr(data.szFileName, ".xpak_")) {
                            paks.emplace_back(data.szFileName);
                        }
                    }
                } while (CascFindNextFile(find, &data));
                CascFindClose(find);
            } else {
                // the xpaks next to the fastfiles read
                std::set<std::filesystem::path> dirs{};
                for (const char* f : opt.files) {
                    std::filesystem::path p{ f };
                    dirs.insert(std::filesystem::is_directory(p) ? p : p.parent_path());
                }
                for (const std::filesystem::path& dir : dirs) {
                    std::error_code ec{};
                    for (const auto& e : std::filesystem::directory_iterator{ dir, ec }) {
                        std::string name{ e.path().filename().string() };
                        if (name.find(".xpak") != std::string::npos) {
                            sizes[e.path().string()] = (uint64_t)e.file_size(ec);
                            if (name.find(".xpak_") == std::string::npos) {
                                paks.emplace_back(e.path().string());
                            }
                        }
                    }
                }
            }
        }

        void LoadPak(fastfile::FastFileOption& opt, const std::string& file) {
            if (!loadedPaks.insert(file).second) {
                return;
            }
            byte header[0x200];
            if (!ReadAt(opt, file, 0, header, sizeof(header)) || *(uint32_t*)header != 0x4950414B) {
                LOG_WARNING("can't read xpak header of {}", file);
                return;
            }
            uint64_t dataOffset{ *(uint64_t*)(header + 0x20) };
            uint64_t indexOffset{ *(uint64_t*)(header + 0x38) };
            uint64_t indexSize{ *(uint64_t*)(header + 0x40) };
            if (!indexSize || indexSize > 0x40000000) {
                return;
            }
            std::vector<byte> index(indexSize);
            if (!ReadAt(opt, file, indexOffset, index.data(), index.size())) {
                return;
            }
            for (size_t i = 0; i + 0x18 <= index.size(); i += 0x18) {
                uint64_t key{ *(uint64_t*)&index[i] };
                uint64_t off{ *(uint64_t*)&index[i + 8] };
                uint64_t size{ *(uint64_t*)&index[i + 0x10] & ~(1ull << 63) };
                entries.try_emplace(key, Location{ file, dataOffset + off, size });
            }
        }

        // a group header: 1 to 30 chunks of known types, in the entry
        bool ValidGroup(const std::vector<byte>& raw, size_t pos) {
            const uint32_t* h{ (const uint32_t*)&raw[pos] };
            if (!h[0] || h[0] > 30) {
                return false;
            }
            size_t len{};
            for (uint32_t c = 0; c < h[0]; c++) {
                uint32_t type{ h[2 + c] >> 24 };
                if (type > 4 && type != 8) {
                    return false;
                }
                len += h[2 + c] & 0xFFFFFF;
            }
            return pos + 0x80 + len <= raw.size();
        }

        /*
         * The game reads an entry by windows of 1 MB from its file offset rounded down to 0x8000 (sub_143713AF0), each
         * window starting with a group: what's left of a window after its last group is padding, skipped.
         */
        bool Decode(const std::vector<byte>& raw, uint64_t fileOffset, size_t size, std::vector<byte>& out) {
            constexpr size_t WINDOW{ 0x100000 };
            out.assign(size, 0);
            size_t pos{};
            // the windows' starts, relative to the entry
            int64_t windowBase{ (int64_t)(fileOffset & ~0x7FFFull) - (int64_t)fileOffset };
            while (pos + 0x80 <= raw.size()) {
                if (!ValidGroup(raw, pos)) {
                    size_t next{ (size_t)(windowBase + ((int64_t)pos - windowBase) / WINDOW * WINDOW + WINDOW) };
                    if (next <= pos || next + 0x80 > raw.size() || !ValidGroup(raw, next)) {
                        break;
                    }
                    pos = next;
                }
                const uint32_t* h{ (const uint32_t*)&raw[pos] };
                uint32_t count{ h[0] };
                size_t dest{ h[1] };
                size_t src{ pos + 0x80 };
                for (uint32_t c = 0; c < count; c++) {
                    uint32_t type{ h[2 + c] >> 24 };
                    size_t len{ h[2 + c] & 0xFFFFFF };
                    if (src + len > raw.size()) {
                        LOG_ERROR("xpak entry: chunk out of the entry");
                        return false;
                    }
                    const byte* data{ &raw[src] };
                    int r{};
                    auto Room = [&](size_t need) {
                        if (out.size() < dest + need) {
                            out.resize(dest + need);
                        }
                    };
                    switch (type) {
                    case 0:
                        Room(len);
                        std::memcpy(&out[dest], data, len);
                        r = (int)len;
                        break;
                    case 1:
                    case 4:
                        Room(0x40000);
                        r = utils::compress::Decompress2(utils::compress::COMP_ZLIB, &out[dest], out.size() - dest, data, len);
                        if (r < 0) {
                            r = utils::compress::Decompress2(utils::compress::COMP_ZLIB_DEFLATE, &out[dest], out.size() - dest, data, len);
                        }
                        break;
                    case 2:
                    case 3:
                        Room(0x40000);
                        r = utils::compress::Decompress2(utils::compress::COMP_LZ4, &out[dest], out.size() - dest, data, len);
                        break;
                    case 8: {
                        uint32_t dsize{ *(const uint32_t*)data };
                        Room(dsize);
                        r = utils::compress::Decompress2(utils::compress::COMP_OODLE, &out[dest], dsize, data + 4, len - 4);
                        break;
                    }
                    default:
                        LOG_ERROR("xpak entry: unknown chunk type {}", type);
                        return false;
                    }
                    if (r < 0) {
                        LOG_ERROR("xpak entry: can't decompress a chunk of type {}", type);
                        return false;
                    }
                    dest += (size_t)r;
                    src += len;
                }
                pos = utils::Aligned<size_t>(src, 0x80);
            }
            if (out.size() > size) {
                out.resize(size);
            }
            return true;
        }
    } // namespace

    void AddHint(const char* zoneName) {
        if (zoneName && std::find(hints.begin(), hints.end(), zoneName) == hints.end()) {
            hints.emplace_back(zoneName);
        }
    }

    bool ReadEntry(fastfile::FastFileOption& opt, uint64_t key, size_t size, std::vector<byte>& out) {
        auto it{ entries.find(key) };
        if (it == entries.end()) {
            ListPaks(opt);
            // the paks of the hinted zones first: zone\<name>.xpak, _d, .xpak_N
            auto Matches = [](const std::string& pak, const std::string& zone) {
                std::string stem{ std::filesystem::path{ pak }.filename().string() };
                return stem.rfind(zone + ".xpak", 0) == 0 || stem.rfind(zone + "_d.xpak", 0) == 0;
            };
            for (const std::string& hint : hints) {
                for (const std::string& pak : paks) {
                    if (Matches(pak, hint)) {
                        LoadPak(opt, pak);
                    }
                }
            }
            it = entries.find(key);
            for (size_t i = 0; it == entries.end() && i < paks.size(); i++) {
                LoadPak(opt, paks[i]);
                it = entries.find(key);
            }
            if (it == entries.end()) {
                LOG_WARNING("xpak key 0x{:x} not found in {} paks ({} loaded, {} entries)", key, paks.size(), loadedPaks.size(), entries.size());
                return false;
            }
        }
        std::vector<byte> raw(it->second.size);
        if (!ReadAt(opt, it->second.file, it->second.offset, raw.data(), raw.size())) {
            LOG_ERROR("Can't read xpak entry 0x{:x} in {}", key, it->second.file);
            return false;
        }
        if (!Decode(raw, it->second.offset, size, out)) {
            LOG_WARNING("xpak entry 0x{:x}: {} at 0x{:x}, 0x{:x} bytes, starts {:08x} {:08x} {:08x}", key, it->second.file, it->second.offset,
                it->second.size, raw.size() >= 12 ? *(uint32_t*)&raw[0] : 0, raw.size() >= 12 ? *(uint32_t*)&raw[4] : 0, raw.size() >= 12 ? *(uint32_t*)&raw[8] : 0);
            return false;
        }
        return true;
    }
} // namespace fastfile::handlers::bo4::xpak
