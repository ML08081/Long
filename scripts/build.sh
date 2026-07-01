#!/usr/bin/env bash
# =============================================================================
#  build.sh — 一键构建脚本（在 WSL / Linux 下运行）
#
#  本地编译（x86_64，用于联调）：
#      ./scripts/build.sh
#  交叉编译到龙芯 2K0300 (LoongArch64, 旧世界 GCC 8.3)：
#      ./scripts/build.sh loong
#  指定工具链 bin 目录：
#      ./scripts/build.sh loong /path/to/toolchain/bin
#  清理：
#      ./scripts/build.sh clean
# =============================================================================
set -euo pipefail

# 切到工程根目录（脚本所在目录的上一级）
cd "$(dirname "$0")/.."
ROOT="$(pwd)"
SCRIPT_DIR="$ROOT/scripts"

TARGET="${1:-native}"

if [[ "$TARGET" == "clean" ]]; then
    rm -rf "$ROOT/build" "$ROOT/build-loong"
    echo "已清理 build/ 与 build-loong/"
    exit 0
fi

JOBS="$(nproc 2>/dev/null || echo 2)"

if [[ "$TARGET" == "loong" ]]; then
    BUILD_DIR="$ROOT/build-loong"
    # 定位旧世界 GCC 8.3 工具链：命令行第二参数优先，否则自动探测
    if [[ -n "${2:-}" ]]; then
        TCDIR="$2"
    elif ! TCDIR="$(bash "$SCRIPT_DIR/detect_toolchain.sh")"; then
        echo "错误: 未找到龙芯【旧世界】GCC 8.3 交叉工具链" >&2
        echo "  预期: \$HOME/toolchains/loongson-gnu-toolchain-8.3-*-loongarch64-linux-gnu-*/bin" >&2
        echo "  或:   ./scripts/build.sh loong /path/to/toolchain/bin" >&2
        exit 1
    fi
    echo ">>> 交叉编译 (LoongArch64 旧世界, 工具链=${TCDIR})"
    cmake -S "$ROOT" -B "$BUILD_DIR" \
        -DCMAKE_TOOLCHAIN_FILE="$ROOT/toolchain.cmake" \
        -DTOOLCHAIN_DIR="$TCDIR" \
        -DCMAKE_BUILD_TYPE=Release
    cmake --build "$BUILD_DIR" -j "$JOBS"
    echo ">>> 产物: $BUILD_DIR/bin/patrol_system"
    file "$BUILD_DIR/bin/patrol_system" || true
else
    BUILD_DIR="$ROOT/build"
    echo ">>> 本地编译 (native)"
    cmake -S "$ROOT" -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Release
    cmake --build "$BUILD_DIR" -j "$JOBS"
    echo ">>> 产物: $BUILD_DIR/bin/patrol_system"
fi
