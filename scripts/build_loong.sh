#!/bin/bash
# 交叉编译到龙芯 2K0300 (LoongArch64)
# 前提: sudo apt-get install gcc-loongarch64-linux-gnu g++-loongarch64-linux-gnu
set -e
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(dirname "$SCRIPT_DIR")"
BUILD="$ROOT/build-loong"

# 检查龙芯交叉编译器：兼容无后缀(gcc)与带版本号(gcc-14/gcc-13...)两种命名
if ! ls /usr/bin/loongarch64-linux-gnu-gcc* >/dev/null 2>&1 \
   && ! command -v loongarch64-linux-gnu-gcc >/dev/null 2>&1; then
    echo "错误: 未找到 loongarch64-linux-gnu-gcc 交叉编译器"
    echo "安装: sudo apt-get install gcc-loongarch64-linux-gnu g++-loongarch64-linux-gnu"
    exit 1
fi

mkdir -p "$BUILD" && cd "$BUILD"
cmake -DCMAKE_TOOLCHAIN_FILE="$ROOT/toolchain.cmake" -DCMAKE_BUILD_TYPE=Release "$ROOT"
make -j"$(nproc)"
echo "龙芯交叉编译完成: $BUILD/bin/patrol_system"
echo "部署: scp $BUILD/bin/patrol_system root@<龙芯IP>:/usr/bin/"
