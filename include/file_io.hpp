#ifndef VISTA_FILE_IO_HPP
#define VISTA_FILE_IO_HPP

#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

namespace vista {
inline bool read_binary_file(const std::filesystem::path& path, std::string& bytes) {
    bytes.clear();
    if (path.string().find('\0') != std::string::npos) return false;
    try {
        std::ifstream input(path, std::ios::binary);
        if (!input) return false;
        bytes.assign(std::istreambuf_iterator<char>(input), {});
        if (!input.bad()) return true;
    } catch (const std::ios_base::failure&) {
        // Stream-buffer iteration can throw even without an exception mask.
    }
    bytes.clear();
    return false;
}
}  // namespace vista
#endif
