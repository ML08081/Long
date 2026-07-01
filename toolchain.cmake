# =============================================================================
#  toolchain.cmake -- 龙芯 2K0300 (LoongArch64) 交叉编译工具链
#  ★ 本工程固定使用【旧世界 old-world】GNU 工具链: loongson-gnu-toolchain-8.3
#    - 目标三元组 : loongarch64-linux-gnu
#    - 编译器版本 : GCC 8.3.0 (rc1.x)，二进制【无版本后缀】
#    - 动态解释器 : /lib64/ld.so.1  (旧世界标志；新世界为 ld-linux-loongarch-*)
# =============================================================================
#  使用方法（工程根目录）:
#     bash scripts/build_loong.sh          # 推荐，自动定位工具链
#  或手动:
#     cmake -B build-loong -DCMAKE_TOOLCHAIN_FILE=toolchain.cmake \
#           -DTOOLCHAIN_DIR=/path/to/.../bin
#
#  工具链定位优先级:
#     1) -DTOOLCHAIN_DIR=<bin>            显式指定
#     2) 环境变量 LOONG_TOOLCHAIN_DIR     (bin 目录)
#     3) 环境变量 LOONGARCH_TOOLCHAIN     (工具链根，自动补 /bin；env.sh 已导出)
#     4) 自动扫描 $HOME/toolchains/loongson-gnu-toolchain-8.3-*-loongarch64-linux-gnu-*/bin
#     5) 回退到 PATH 中的 loongarch64-linux-gnu-gcc
# =============================================================================

set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR loongarch64)

set(TOOLCHAIN_PREFIX "loongarch64-linux-gnu-")

# ---- 定位工具链 bin 目录 ---------------------------------------------------
if(NOT DEFINED TOOLCHAIN_DIR)
    if(DEFINED ENV{LOONG_TOOLCHAIN_DIR})
        set(TOOLCHAIN_DIR "$ENV{LOONG_TOOLCHAIN_DIR}")
    elseif(DEFINED ENV{LOONGARCH_TOOLCHAIN})
        set(TOOLCHAIN_DIR "$ENV{LOONGARCH_TOOLCHAIN}/bin")
    else()
        file(GLOB _loong_cands
            "$ENV{HOME}/toolchains/loongson-gnu-toolchain-8.3-*-loongarch64-linux-gnu-*/bin")
        if(_loong_cands)
            list(SORT _loong_cands)
            list(GET _loong_cands -1 TOOLCHAIN_DIR)   # 取版本最高的一个
        endif()
    endif()
endif()

if(TOOLCHAIN_DIR)
    set(_tc "${TOOLCHAIN_DIR}/${TOOLCHAIN_PREFIX}")
else()
    set(_tc "${TOOLCHAIN_PREFIX}")   # 依赖 PATH
endif()

# 旧世界工具链二进制无版本后缀
set(CMAKE_C_COMPILER   "${_tc}gcc")
set(CMAKE_CXX_COMPILER "${_tc}g++")
set(CMAKE_AR           "${_tc}ar"      CACHE FILEPATH "Archiver")
set(CMAKE_RANLIB       "${_tc}ranlib"  CACHE FILEPATH "Ranlib")
set(CMAKE_STRIP        "${_tc}strip"   CACHE FILEPATH "Strip")
set(CMAKE_OBJCOPY      "${_tc}objcopy" CACHE FILEPATH "Objcopy")

# ---- 强制校验：必须是旧世界 GCC 8.x，防止误用新世界(apt gcc-14 等) ----------
execute_process(
    COMMAND "${CMAKE_CXX_COMPILER}" -dumpversion
    OUTPUT_VARIABLE _loong_gccver
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
    RESULT_VARIABLE _loong_gccrc)
if(NOT _loong_gccrc EQUAL 0)
    message(FATAL_ERROR
        "找不到龙芯旧世界交叉编译器: ${CMAKE_CXX_COMPILER}\n"
        "请先安装 loongson-gnu-toolchain-8.3 (old-world)，或用 -DTOOLCHAIN_DIR 指定其 bin 目录。")
endif()
if(NOT _loong_gccver MATCHES "^8\\.")
    message(FATAL_ERROR
        "本工程要求龙芯【旧世界 old-world】GCC 8.3 工具链，但检测到 GCC ${_loong_gccver}。\n"
        "GCC 9/10/12+ 属于【新世界 new-world】，与 2K0300 旧世界系统不兼容。\n"
        "编译器: ${CMAKE_CXX_COMPILER}")
endif()
message(STATUS "龙芯旧世界工具链: ${CMAKE_CXX_COMPILER} (GCC ${_loong_gccver})")

# ---- sysroot（按需 -DTARGET_SYSROOT=/path/to/rootfs）----------------------
if(DEFINED TARGET_SYSROOT)
    set(CMAKE_SYSROOT "${TARGET_SYSROOT}")
    set(CMAKE_FIND_ROOT_PATH "${TARGET_SYSROOT}")
endif()

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)

add_compile_definitions(PATROL_CROSS_BUILD=1)
