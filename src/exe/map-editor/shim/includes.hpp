#pragma once
// what acts' includes.hpp gives the headers the editor shares with it (linkers/bo4/gltf.hpp): rapidjson, byte and
// utils::ReadFile, without the rest of acts
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <format>
#include <fstream>
#include <string>
#include <vector>
#include <rapidjson/document.h>

typedef unsigned char byte;

namespace utils {
    inline bool ReadFile(const std::filesystem::path& path, std::vector<byte>& buffer) {
        std::ifstream is{ path, std::ios::binary };
        if (!is) {
            return false;
        }
        buffer.assign(std::istreambuf_iterator<char>{ is }, {});
        return true;
    }
}
