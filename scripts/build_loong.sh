#!/bin/bash
# =============================================================================
#  build_loong.sh -- 交叉编译到龙芯 2K0300 (LoongArch64)
#  ★ 固定使用【旧世界 old-world】GCC 8.3 工具链 (loongson-gnu-toolchain-8.3)
# =============================================================================
set -e
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(dirname "$SCRIPT_DIR")"
BUILD="$ROOT/build-loong"

# ---- 定位旧世界工具链 -----------------------------------------------------
if ! TCDIR="$(bash "$SCRIPT_DIR/detect_toolchain.sh")"; then
    echo "错误: 未找到龙芯【旧世界 old-world】GCC 8.3 交叉工具链"
    echo ""
    echo "预期位置: \$HOME/toolchains/loongson-gnu-toolchain-8.3-*-loongarch64-linux-gnu-*/bin"
    echo "或设置环境变量: export LOONG_TOOLCHAIN_DIR=/path/to/toolchain/bin"
    echo ""
    echo "下载(x86_64 主机版):"
    echo "  http://ftp.loongnix.cn/toolchain/gcc/release/loongarch/gcc8/"
    echo "  loongson-gnu-toolchain-8.3-x86_64-loongarch64-linux-gnu-rc1.6.tar.xz"
    exit 1
fi

echo "==> 使用旧世界工具链: $TCDIR"
"$TCDIR/loongarch64-linux-gnu-gcc" --version | head -1

# ---- 配置 + 构建 ----------------------------------------------------------
mkdir -p "$BUILD" && cd "$BUILD"
cmake -DCMAKE_TOOLCHAIN_FILE="$ROOT/toolchain.cmake" \
      -DTOOLCHAIN_DIR="$TCDIR" \
      -DCMAKE_BUILD_TYPE=Release "$ROOT"
make -j"$(nproc)"

BIN="$BUILD/bin/patrol_system"
echo ""
echo "==> 龙芯交叉编译完成: $BIN"

# ---- 校验：LoongArch + 旧世界(ld.so.1) ------------------------------------
FINFO="$(file "$BIN")"
echo "    $FINFO"
echo "$FINFO" | grep -q "LoongArch" || { echo "错误: 产物不是 LoongArch 架构!"; exit 1; }
if echo "$FINFO" | grep -q "ld.so.1"; then
    echo "    OK: 旧世界(old-world) 产物 (interpreter=/lib64/ld.so.1)"
elif echo "$FINFO" | grep -qE "ld-linux-loongarch"; then
    echo "    !! 警告: 检测到【新世界】解释器，工具链可能不对!"
    exit 1
else
    echo "    注意: 未检出动态解释器(可能为静态链接)"
fi
echo ""
echo "部署: scp $BIN root@<龙芯IP>:/usr/bin/"
