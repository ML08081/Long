#!/bin/sh
# =============================================================================
#  00_probe_board.sh -- 【在龙芯 2K0300 板子上运行】
#  采集内核身份信息，判断能否/如何交叉编译 UVC 模块，结果打包回主机。
#  期望内核: 4.19.190-rt79-yocto-standard
#
#  用法:
#    scp scripts/00_probe_board.sh root@<板子IP>:/tmp/
#    ssh root@<板子IP> 'sh /tmp/00_probe_board.sh'
#    scp root@<板子IP>:/tmp/board-kinfo.tar.gz .    # 发回给我
# =============================================================================
OUT=/tmp/board-kinfo
rm -rf "$OUT"; mkdir -p "$OUT"
KREL=$(uname -r)
echo "内核: $KREL"

uname -a          > "$OUT/uname-a.txt"       2>&1
echo "$KREL"      > "$OUT/uname-r.txt"
cat /proc/version > "$OUT/proc-version.txt"   2>&1
cat /proc/cmdline > "$OUT/proc-cmdline.txt"   2>&1

# ---- 内核配置 ----
if [ -f /proc/config.gz ]; then
    zcat /proc/config.gz > "$OUT/config" 2>/dev/null || cp /proc/config.gz "$OUT/config.gz"
    echo "config: 来自 /proc/config.gz"
elif [ -f "/boot/config-$KREL" ]; then
    cp "/boot/config-$KREL" "$OUT/config"
    echo "config: 来自 /boot/config-$KREL"
else
    echo "MISSING: 无 /proc/config.gz 也无 /boot/config-$KREL" > "$OUT/config.MISSING"
fi
# MODVERSIONS / PREEMPT / SMP 关键项
if [ -f "$OUT/config" ]; then
    grep -E 'CONFIG_MODVERSIONS|CONFIG_MODULE_FORCE_LOAD|CONFIG_MODULE_SIG|CONFIG_PREEMPT|CONFIG_SMP|CONFIG_LOCALVERSION' "$OUT/config" > "$OUT/config-key.txt"
fi

# ---- 内核 build 树 (决定能否直接交叉编译内建驱动) ----
for BASE in /lib/modules /usr/lib/modules; do
    B="$BASE/$KREL/build"
    if [ -e "$B" ]; then
        RP=$(readlink -f "$B" 2>/dev/null || echo "$B")
        {
            echo "BUILD=$B -> $RP"
            echo "存在? $( [ -d "$RP" ] && echo yes || echo 'symlink悬空')"
            echo "有 Module.symvers? $( [ -f "$RP/Module.symvers" ] && echo YES || echo no )"
            echo "有 .config?        $( [ -f "$RP/.config" ] && echo YES || echo no )"
            echo "是【完整源码树】?  $( [ -d "$RP/drivers/media/usb/uvc" ] && echo 'YES(含uvc源码)' || echo 'NO(疑似仅headers)' )"
            echo "有 drivers/media?  $( [ -d "$RP/drivers/media" ] && echo yes || echo no )"
        } >> "$OUT/build-tree.txt"
        [ -f "$RP/Module.symvers" ] && cp "$RP/Module.symvers" "$OUT/Module.symvers"
        [ -f "$RP/.config" ]        && cp "$RP/.config"        "$OUT/config.from-build"
    else
        echo "无 build 树: $B" >> "$OUT/build-tree.txt"
    fi
done

# ---- 本机是否可原生编译(厂商推荐方式) ----
{
    echo "gcc:  $(command -v gcc || echo none)  $(gcc --version 2>/dev/null | head -1)"
    echo "make: $(command -v make || echo none)"
    echo "depmod: $(command -v depmod || echo none)"
} > "$OUT/native-build-tools.txt"

# ---- 已有 media/v4l2 模块 ----
find /lib/modules/"$KREL" /usr/lib/modules/"$KREL" -type f -name '*.ko*' 2>/dev/null \
    | grep -iE 'video|uvc|v4l|media' > "$OUT/existing-media-modules.txt"
grep -iE 'video|uvc|v4l|media' /lib/modules/"$KREL"/modules.builtin 2>/dev/null > "$OUT/builtin-media.txt"

# ---- 任一已装模块的 vermagic (参照物) ----
KO=$(find /lib/modules/"$KREL" /usr/lib/modules/"$KREL" -name '*.ko' 2>/dev/null | head -1)
[ -n "$KO" ] && modinfo "$KO" 2>/dev/null | grep -i vermagic > "$OUT/vermagic.txt"

# ---- USB 摄像头是否插着 ----
( lsusb 2>/dev/null || cat /sys/kernel/debug/usb/devices 2>/dev/null ) > "$OUT/usb.txt" 2>&1
ls -l /dev/video* > "$OUT/dev-video.txt" 2>&1
lsmod > "$OUT/lsmod.txt" 2>&1

tar -czf /tmp/board-kinfo.tar.gz -C /tmp board-kinfo 2>/dev/null

echo ""
echo "======================= 采集结果预览 ======================="
echo " 内核        : $KREL"
echo " config      : $( [ -f "$OUT/config" ] && echo 有 || echo 缺 )"
echo " build 树    : $(grep -h '完整源码树' "$OUT/build-tree.txt" 2>/dev/null | head -1)"
echo " Module.symvers: $( [ -f "$OUT/Module.symvers" ] && echo 有 || echo 缺 )"
echo " 原生 gcc    : $(gcc --version 2>/dev/null | head -1 || echo 无)"
echo "------------------------------------------------------------"
echo " 打包: /tmp/board-kinfo.tar.gz"
echo " 请拷回主机后发给我: scp root@<板子IP>:/tmp/board-kinfo.tar.gz ."
echo "============================================================"
