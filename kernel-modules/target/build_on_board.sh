#!/bin/sh
# =============================================================================
#  build_on_board.sh -- 【在龙芯板子上运行】原生编译 UVC/V4L2 模块
#  厂商推荐方式(见"驱动程序module模式安装"文档): 直接对 /lib/modules/$(uname-r)/build 编。
#  仅当板子满足: ① 有【完整内核源码树】 ② 有本机 gcc/make 时可用。
#  优点: vermagic 天然一致,无需交叉工具链、无需处理 Module.symvers。
# =============================================================================
set -u
KREL=$(uname -r)
KSRC="/lib/modules/$KREL/build"

[ -d "$KSRC/drivers/media/usb/uvc" ] || {
    echo "错误: $KSRC 不是完整源码树(无 drivers/media/usb/uvc)。"
    echo "      本机无法原生编 uvcvideo;请在主机交叉编译(见 kernel-modules/README.md)。"
    exit 1; }
command -v make >/dev/null 2>&1 && command -v gcc >/dev/null 2>&1 || {
    echo "错误: 本机缺 gcc/make。"; exit 1; }

HERE=$(cd "$(dirname "$0")" && pwd)

echo ">>> 合并 UVC 配置到内核 .config"
if [ -x "$KSRC/scripts/kconfig/merge_config.sh" ] && [ -f "$HERE/uvc.config" ]; then
    ( cd "$KSRC" && ./scripts/kconfig/merge_config.sh -m .config "$HERE/uvc.config" )
else
    echo "(未找到 merge_config.sh 或 uvc.config,假定 .config 已启用 UVC=m)"
fi

echo ">>> modules_prepare + 编译 drivers/media"
make -C "$KSRC" modules_prepare -j"$(nproc)"
make -C "$KSRC" M=drivers/media modules -j"$(nproc)"

OUT="$HERE/modules"; mkdir -p "$OUT"
find "$KSRC/drivers/media" -name '*.ko' \
    \( -name 'uvcvideo.ko' -o -name 'videodev.ko' -o -name 'videobuf2*.ko' \
       -o -name 'mc.ko' -o -name 'v4l2*.ko' \) -exec cp -v {} "$OUT/" \;

echo ">>> 安装并加载"
sh "$HERE/install.sh"
