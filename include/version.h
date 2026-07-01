#ifndef PATROL_VERSION_H
#define PATROL_VERSION_H

// =============================================================================
//  version.h -- 版本信息
//  版本号由 CMake 从根目录 VERSION 文件注入（单一来源），并附带构建时间与 git 短哈希。
//  直接编译（不经 CMake）时使用下方回退值。
// =============================================================================

#ifndef PATROL_VERSION
#define PATROL_VERSION "0.0.0-dev"
#endif
#ifndef PATROL_BUILD_DATE
#define PATROL_BUILD_DATE "unknown"
#endif
#ifndef PATROL_GIT_HASH
#define PATROL_GIT_HASH "nogit"
#endif

namespace patrol {
inline const char* versionString() { return PATROL_VERSION; }
inline const char* buildDate()     { return PATROL_BUILD_DATE; }
inline const char* gitHash()       { return PATROL_GIT_HASH; }
} // namespace patrol

#endif // PATROL_VERSION_H