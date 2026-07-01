#!/bin/sh
# =============================================================================
#  load_uvc.sh -- 【在龙芯板子上运行】按依赖顺序手动 insmod (depmod/modprobe 的兜底)
#  用法: sh load_uvc.sh [模块目录]   (默认为脚本同级 modules/)
#  多趟循环以自动解决加载顺序。
# =============================================================================
DIR="${1:-$(cd "$(dirname "$0")" && pwd)/modules}"
[ -d "$DIR" ] || { echo "找不到模块目录: $DIR"; exit 1; }

# vb2compat 必须最先(补 frame_vector 符号); 不含 v4l2-dv-timings(需 rational, uvcvideo 用不到)
ORDER="vb2compat media videodev v4l2-common videobuf2-common videobuf2-memops videobuf2-vmalloc videobuf2-v4l2 uvcvideo"

is_loaded() { lsmod | awk '{print $1}' | grep -qx "$(echo "$1" | tr '-' '_')"; }

for pass in 1 2 3; do
    for m in $ORDER; do
        f="$DIR/$m.ko"
        [ -f "$f" ] || continue
        is_loaded "$m" && continue
        insmod "$f" 2>/dev/null && echo "insmod $m OK"
    done
done

echo ""
echo ">>> 已加载:"
lsmod | grep -E 'uvcvideo|videobuf2|videodev|^mc ' || echo "(无)"
ls -l /dev/video* 2>/dev/null
