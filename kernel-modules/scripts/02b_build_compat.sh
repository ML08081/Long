#!/usr/bin/env bash
# =============================================================================
#  02b_build_compat.sh -- 编译 vb2compat.ko (frame_vector 符号补丁模块)
#  依赖 02_build_modules.sh 已完成(需 build/kbuild 已配置好、有 Module.symvers)。
#  产物: build/modules/vb2compat.ko
# =============================================================================
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
KROOT="$(dirname "$HERE")"
REPO="$(dirname "$KROOT")"

: "${KERNEL_SRC:?请设置 KERNEL_SRC}"
ARCH="${ARCH:-loongarch}"
KBUILD="$KROOT/build/kbuild"
COMPAT="$KROOT/compat"
[ -f "$KBUILD/.config" ] || { echo "错误: 请先运行 02_build_modules.sh (缺 build/kbuild)"; exit 1; }

TCDIR="$(bash "$REPO/scripts/detect_toolchain.sh")"
export PATH="$TCDIR:$PATH"
CROSS="loongarch64-linux-gnu-"

# 拷入内核源码 frame_vector.c (与 kbuild 同源, 保证符号 CRC 一致)
cp "$KERNEL_SRC/mm/frame_vector.c" "$COMPAT/frame_vector.c"

echo ">>> 编译 vb2compat.ko (M=$COMPAT)"
make -C "$KERNEL_SRC" O="$KBUILD" M="$COMPAT" ARCH="$ARCH" CROSS_COMPILE="$CROSS" LOCALVERSION=+ modules

OUT="$KROOT/build/modules"; mkdir -p "$OUT"
cp -v "$COMPAT/vb2compat.ko" "$OUT/"
"${CROSS}strip" --strip-debug "$OUT/vb2compat.ko"

echo ">>> vb2compat 导出符号:"
"${CROSS}nm" "$OUT/vb2compat.ko" | grep -iE ' T (get_vaddr_frames|put_vaddr_frames|frame_vector)' || \
  modinfo "$OUT/vb2compat.ko" | grep -i vermagic
echo ">>> 完成: $OUT/vb2compat.ko"
