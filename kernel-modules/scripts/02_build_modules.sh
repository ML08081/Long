#!/usr/bin/env bash
# =============================================================================
#  02_build_modules.sh -- 交叉编译 UVC + V4L2 模块 (龙芯 2K0300, 旧世界 GCC 8.3)
#  目标内核: 4.19.190+ (vermagic: 4.19.190+ SMP mod_unload modversions LOONGARCH 64BIT)
#
#  采用【out-of-tree 编译 O=】: 源码树保持只读, 全部产物写到 build/kbuild。
#  因 CONFIG_MODVERSIONS=y, 全量编内核以生成匹配的 Module.symvers, 保证符号 CRC 对上。
#
#  用法:
#     KERNEL_SRC=/opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main \
#       bash scripts/02_build_modules.sh
#  可选:
#     DEFCONFIG=loongson_2k0300_99pi_wifi_defconfig  (默认;久久派板)
#     ARCH=loongarch    KERNELRELEASE_EXPECT=4.19.190+
# =============================================================================
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
KROOT="$(dirname "$HERE")"
REPO="$(dirname "$KROOT")"

: "${KERNEL_SRC:?请设置 KERNEL_SRC (内核源码树路径)}"
[ -f "$KERNEL_SRC/Makefile" ] || { echo "错误: $KERNEL_SRC 不是内核源码树"; exit 1; }
[ -d "$KERNEL_SRC/drivers/media/usb/uvc" ] || { echo "错误: 缺 uvc 源码"; exit 1; }

ARCH="${ARCH:-loongarch}"
DEFCONFIG="${DEFCONFIG:-loongson_2k0300_99pi_wifi_defconfig}"
KERNELRELEASE_EXPECT="${KERNELRELEASE_EXPECT:-4.19.190+}"
KBUILD="$KROOT/build/kbuild"          # out-of-tree 产物目录(可写)
mkdir -p "$KBUILD"

# ---- 旧世界工具链 ----
TCDIR="$(bash "$REPO/scripts/detect_toolchain.sh")" || { echo "错误: 未找到旧世界 GCC8.3 工具链"; exit 1; }
export PATH="$TCDIR:$PATH"
CROSS="loongarch64-linux-gnu-"
echo ">>> 工具链: $TCDIR ($("${CROSS}gcc" -dumpversion))"

# ---- 主机依赖 ----
miss=""; for t in make bc flex bison; do command -v "$t" >/dev/null 2>&1 || miss="$miss $t"; done
[ -n "$miss" ] && { echo "缺主机工具:$miss -> sudo apt-get install -y build-essential bc flex bison libssl-dev libelf-dev"; exit 1; }

# LOCALVERSION=+ 复现板子的 4.19.190+ (源码无 .git, 用 make 变量而非写 .scmversion)
MK="make -C $KERNEL_SRC O=$KBUILD ARCH=$ARCH CROSS_COMPILE=$CROSS LOCALVERSION=+"

# ---- 配置: defconfig -> 确定版本号 -> 合并 UVC ----
echo ">>> $DEFCONFIG (O=$KBUILD)"
$MK "$DEFCONFIG"
# 确定性地产出 4.19.190+ : 关掉 AUTO、清空 CONFIG_LOCALVERSION, 由 LOCALVERSION=+ 提供后缀
bash "$KERNEL_SRC/scripts/config" --file "$KBUILD/.config" --disable LOCALVERSION_AUTO --set-str LOCALVERSION ""
# ZIP 源码丢失了 scripts/dtc/include-prefixes 符号链接, 导致内建DTB编译失败;
# 我们只需 Module.symvers, 不需要内建DTB(不影响符号CRC), 直接关闭它绕过。
bash "$KERNEL_SRC/scripts/config" --file "$KBUILD/.config" --disable BUILTIN_DTB
# 厂商 CAN 驱动(CAN_LSCAN)靠 cp 预编译 .elf blob, 该 blob 在 ZIP 里缺失导致编译失败;
# 与 UVC 无关, 禁用之(不影响核心符号 CRC)。如遇其它缺 blob 的厂商驱动同样在此禁用。
bash "$KERNEL_SRC/scripts/config" --file "$KBUILD/.config" --disable CAN_LSCAN --disable CAN_LSCAN_PLATFORM
echo ">>> 合并 uvc.config (UVC/V4L2 = m)"
cat "$KROOT/config/uvc.config" >> "$KBUILD/.config"
$MK olddefconfig

REL="$($MK -s kernelrelease 2>/dev/null || true)"
echo ">>> kernelrelease = $REL  (期望 $KERNELRELEASE_EXPECT)"
if [ "$REL" != "$KERNELRELEASE_EXPECT" ]; then
    echo "!! 警告: 版本号与板子不一致, 模块 vermagic 可能对不上。继续但请留意。"
fi

# ---- 全量编 (生成匹配 Module.symvers) + 编模块 ----
echo ">>> 编译 vmlinux (生成 Module.symvers, 耗时较长) ..."
$MK -j"$(nproc)" vmlinux
echo ">>> 编译 modules ..."
$MK -j"$(nproc)" modules

# ---- 收集媒体 .ko ----
OUT="$KROOT/build/modules"; rm -rf "$OUT"; mkdir -p "$OUT"
# 收集 drivers/media 下全部 .ko (uvcvideo 的完整依赖闭包: media/videodev/videobuf2-*/v4l2-* 等)
find "$KBUILD/drivers/media" -name '*.ko' -exec cp -v {} "$OUT/" \;
[ -f "$OUT/uvcvideo.ko" ] || { echo "错误: 未生成 uvcvideo.ko"; exit 1; }
cp "$KBUILD/Module.symvers" "$KROOT/build/Module.symvers" 2>/dev/null || true

echo ">>> strip"
for ko in "$OUT"/*.ko; do "${CROSS}strip" --strip-debug "$ko"; done

echo ""; echo ">>> 产物 ($OUT):"; ls -lh "$OUT"
echo ">>> vermagic 核对(应为: $KERNELRELEASE_EXPECT SMP mod_unload modversions LOONGARCH 64BIT):"
modinfo "$OUT/uvcvideo.ko" 2>/dev/null | grep -iE 'vermagic|depends'
echo ">>> 下一步: bash scripts/03_make_package.sh"
