#ifndef PATROL_UTILS_FILEUTIL_H
#define PATROL_UTILS_FILEUTIL_H

#include <string>
#include <fstream>
#include <sstream>

namespace patrol {
namespace fs {

inline bool readFile(const std::string& path, std::string& out) {
    std::ifstream f(path.c_str());
    if (!f.is_open()) return false;
    std::ostringstream ss;
    ss << f.rdbuf();
    out = ss.str();
    return !f.bad();
}

inline bool writeFile(const std::string& path, const std::string& content) {
    std::ofstream f(path.c_str());
    if (!f.is_open()) return false;
    f << content;
    return f.good();
}

} // namespace fs
} // namespace patrol

#endif // PATROL_UTILS_FILEUTIL_H