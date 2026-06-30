# =============================================================================
#  toolchain.cmake -- 龙芯 2K0300 (LoongArch64) 交叉编译工具链
# =============================================================================
#  使用方法（在工程根目录）：
#     bash scripts/build_loong.sh
#  或手动：
#     mkdir -p build-loong && cd build-loong
#     cmake -DCMAKE_TOOLCHAIN_FILE=../toolchain.cmake ..
#     make -j$(nproc)
#
#  当前系统已安装工具链：loongarch64-linux-gnu-gcc-14
#  可通过 -DTOOLCHAIN_SUFFIX=-14 覆盖版本后缀（默认 -14）
# =============================================================================

set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR loongarch64)

# 工具链前缀（可通过 -DTOOLCHAIN_PREFIX=... 或环境变量 CROSS_COMPILE 覆盖）
if(NOT DEFINED TOOLCHAIN_PREFIX)
    if(DEFINED ENV{CROSS_COMPILE})
        set(TOOLCHAIN_PREFIX $ENV{CROSS_COMPILE})
    else()
        set(TOOLCHAIN_PREFIX "loongarch64-linux-gnu-")
    endif()
endif()

# 版本后缀（Ubuntu apt 包名为 gcc-14-loongarch64-linux-gnu，二进制为 loongarch64-linux-gnu-gcc-14）
if(NOT DEFINED TOOLCHAIN_SUFFIX)
    set(TOOLCHAIN_SUFFIX "-14")
endif()

# 可选：若工具链不在 PATH 中，可指定 -DTOOLCHAIN_DIR=/opt/.../bin
if(DEFINED TOOLCHAIN_DIR)
    set(_tc "${TOOLCHAIN_DIR}/${TOOLCHAIN_PREFIX}")
else()
    set(_tc "${TOOLCHAIN_PREFIX}")
endif()

set(CMAKE_C_COMPILER   "${_tc}gcc${TOOLCHAIN_SUFFIX}")
set(CMAKE_CXX_COMPILER "${_tc}g++${TOOLCHAIN_SUFFIX}")
set(CMAKE_AR           "${_tc}ar"    CACHE FILEPATH "Archiver")
set(CMAKE_STRIP        "${_tc}strip" CACHE FILEPATH "Strip")

# sysroot（按需指定 -DTARGET_SYSROOT=/path/to/rootfs）
if(DEFINED TARGET_SYSROOT)
    set(CMAKE_SYSROOT "${TARGET_SYSROOT}")
    set(CMAKE_FIND_ROOT_PATH "${TARGET_SYSROOT}")
endif()

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)

add_compile_definitions(PATROL_CROSS_BUILD=1)