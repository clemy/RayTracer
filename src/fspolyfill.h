#pragma once

#include <string>

// std::filesystem support missing in old C++17 compilers (gcc 7.5.0)
inline std::string replace_filename(const std::string &path, const std::string &replacement) {
    const std::string DRIVE_SEPARATORS { ":" };
    const std::string DIR_SEPARATORS { ":/\\" };
    // handle absolute path
    if (DIR_SEPARATORS.find_first_of(replacement.front()) != std::string::npos) {
        return replacement;
    }
    // handle absolute drive
    if (replacement.find_first_of(DRIVE_SEPARATORS) != std::string::npos) {
        return replacement;
    }
    const auto seppos = path.find_last_of(DIR_SEPARATORS);    
    std::string ret(path, 0, seppos == std::string::npos ? 0 : seppos + 1);
    return ret.append(replacement);
}
