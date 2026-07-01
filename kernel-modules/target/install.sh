#!/bin/sh
# =============================================================================
#  install.sh -- 【在龙芯板子上运行】安装 UVC/V4L2 模块并加载
#  优先 depmod + modprobe(自动解析依赖); 失败则回退手动顺序 insmod。
# =============================================================================
set -u
KREL=$(uname -r)
HERE=$(cd "$(dirname "$0")" && pwd)
DEST="/lib/modules/$KREL/extra"

echo ">>> 目标内核: $KREL"
echo ">>> 拷贝模块到 $DEST (先清理旧残留)"
mkdir -p "$DEST"
rm -f "$DEST"/uvcvideo.ko "$DEST"/videodev.ko "$DEST"/videobuf2-*.ko \
      "$DEST"/v4l2-*.ko "$DEST"/media.ko "$DEST"/vb2compat.ko 2>/dev/null || true
cp "$HERE"/modules/*.ko "$DEST"/ 2>/dev/null || { echo "拷贝失败"; exit 1; }

if command -v depmod >/dev/null 2>&1; then
    echo ">>> depmod -a"
    depmod -a "$KREL" 2>/dev/null || depmod -a
    echo ">>> modprobe uvcvideo"
    if modprobe uvcvideo 2>/tmp/uvc_modprobe.err; then
        echo "modprobe uvcvideo 成功"
    else
        echo "modprobe 失败: $(cat /tmp/uvc_modprobe.err 2>/dev/null)"
        echo ">>> 回退手动顺序加载"
        sh "$HERE/load_uvc.sh" "$DEST"
    fi
else
    sh "$HERE/load_uvc.sh" "$DEST"
fi

echo ""
echo ">>> 配置开机自动加载"
if [ -d /etc/modules-load.d ]; then
    echo uvcvideo > /etc/modules-load.d/uvc.conf
    echo "已写 /etc/modules-load.d/uvc.conf (systemd 开机自动 modprobe uvcvideo)"
elif [ -f /etc/modules ]; then
    grep -qx uvcvideo /etc/modules 2>/dev/null || echo uvcvideo >> /etc/modules
    echo "已写 /etc/modules"
fi

echo ""
echo ">>> 结果:"
lsmod | grep -E 'uvcvideo|videobuf2|vb2compat|videodev' || echo "(未见相关模块,检查 dmesg)"
ls -l /dev/video* 2>/dev/null || echo "(暂无 /dev/video*,插入摄像头后再看)"
echo ""
echo "提示: /dev/video0 已就绪即表示成功; 重启后应由 modules-load.d 自动加载。"
