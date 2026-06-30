#!/usr/bin/env bash
# =============================================================================
#  build.sh — 一键构建脚本（在 WSL / Linux 下运行）
#
#  本地编译（x86_64，用于联调）：
#      ./scripts/build.sh
#  交叉编译到龙芯 2K0300 (LoongArch64)：
#      ./scripts/build.sh loong
#  指定交叉工具链前缀：
#      ./scripts/build.sh loong loongarch64-linux-gnu-
#  清理：
#      ./scripts/build.sh clean
# =============================================================================
set -euo pipefail

# 切到工程根目录（脚本所在目录的上一级）
cd "$(dirname "$0")/.."
ROOT="$(pwd)"

TARGET="${1:-native}"

if [[ "$TARGET" == "clean" ]]; then
    rm -rf "$ROOT/build" "$ROOT/build-loong"
    echo "已清理 build/ 与 build-loong/"
    exit 0
fi

JOBS="$(nproc 2>/dev/null || echo 2)"

if [[ "$TARGET" == "loong" ]]; then
    PREFIX="${2:-loongarch64-linux-gnu-}"
    BUILD_DIR="$ROOT/build-loong"
    echo ">>> 交叉编译 (LoongArch64, 前缀=${PREFIX})"
    cmake -S "$ROOT" -B "$BUILD_DIR" \
        -DCMAKE_TOOLCHAIN_FILE="$ROOT/toolchain.cmake" \
        -DTOOLCHAIN_PREFIX="$PREFIX" \
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
