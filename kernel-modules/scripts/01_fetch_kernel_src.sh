#!/usr/bin/env bash
# =============================================================================
#  01_fetch_kernel_src.sh -- 获取与板子匹配的内核源码树
#  板子内核: 4.19.190-rt79-yocto-standard (Yocto/poky, PREEMPT_RT, LoongArch 旧世界)
#
#  编译 uvcvideo/videodev 这类【内核自带驱动】需要【完整源码树】(含 drivers/media/*.c)。
#  仅有 kernel-headers 不够。三种来源(按可靠性排序):
#
#   来源A (最稳) 板子上就有完整源码树:
#       先跑 00_probe_board.sh，若显示 build 树=【完整源码树】，直接打包取回:
#         ssh root@<板子IP> 'tar czf /tmp/ksrc.tar.gz -C /lib/modules/$(uname -r) build/'
#         scp root@<板子IP>:/tmp/ksrc.tar.gz .
#       然后 KERNEL_SRC 指向解压出的 build 树即可(自带 .config/Module.symvers)。
#
#   来源B 厂商 Yocto BSP / SDK 的内核源码(久久派/广东龙芯):
#       在 Yocto 工程里: bitbake -c devshell virtual/kernel  → ${S} 即源码树
#       或从 meta-loongson / 久久派 gitee 仓库取 linux-4.19 分支。
#       设置 KSRC_GIT_URL / KSRC_REF 后本脚本可 git clone。
#
#   来源C (不推荐) 主线 linux-4.19.190 + rt79 补丁 + 龙芯旧世界补丁:
#       难以复刻厂商改动;若板子开了 CONFIG_MODVERSIONS,符号 CRC 对不上会拒绝加载。
# =============================================================================
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
KROOT="$(dirname "$HERE")"
DEST="${KERNEL_SRC_DIR:-$KROOT/linux-src}"

# 默认源: 龙芯官方适配 LS2K0300 的 linux-4.19.190 (含久久派 defconfig 与 uvc 源码)
KSRC_GIT_URL="${KSRC_GIT_URL:-https://github.com/AirFortressIlikara/LS2K0300-linux-4.19.git}"
KSRC_REF="${KSRC_REF:-main}"
KSRC_TARBALL="${KSRC_TARBALL:-}"

mkdir -p "$KROOT"

if [ -n "$KSRC_TARBALL" ]; then
    echo ">>> 从本地 tarball 解压: $KSRC_TARBALL"
    mkdir -p "$DEST"
    tar -xf "$KSRC_TARBALL" -C "$DEST" --strip-components=1 2>/dev/null \
        || tar -xf "$KSRC_TARBALL" -C "$DEST"
    echo ">>> 源码已就绪: $DEST"
elif [ -n "$KSRC_GIT_URL" ]; then
    echo ">>> git clone $KSRC_GIT_URL (ref=${KSRC_REF:-默认分支}) -> $DEST"
    if [ -n "$KSRC_REF" ]; then
        git clone --depth 1 --branch "$KSRC_REF" "$KSRC_GIT_URL" "$DEST"
    else
        git clone --depth 1 "$KSRC_GIT_URL" "$DEST"
    fi
    echo ">>> 源码已就绪: $DEST"
else
    cat <<'EOF'
!! 未指定源码来源。请用下列之一:

  # A) 从板子取回完整 build 树(最稳):
  ssh root@<板子IP> 'tar czf /tmp/ksrc.tar.gz -C /lib/modules/$(uname -r) build/'
  scp root@<板子IP>:/tmp/ksrc.tar.gz .
  KSRC_TARBALL=./ksrc.tar.gz bash scripts/01_fetch_kernel_src.sh

  # B) 从本地 tarball:
  KSRC_TARBALL=/path/to/linux-4.19.190-rt79.tar.gz bash scripts/01_fetch_kernel_src.sh

  # C) 从 git:
  KSRC_GIT_URL=<仓库URL> KSRC_REF=<分支/tag> bash scripts/01_fetch_kernel_src.sh
EOF
    exit 1
fi

echo ">>> 校验:"
[ -f "$DEST/Makefile" ] && head -5 "$DEST/Makefile" | grep -E 'VERSION|PATCHLEVEL|SUBLEVEL' || echo "(注意: 未找到内核 Makefile 版本头)"
[ -d "$DEST/drivers/media/usb/uvc" ] && echo "OK: 含 uvc 源码(完整源码树)" || echo "!! 缺 drivers/media/usb/uvc —— 可能只是 headers,无法编 uvcvideo"
echo ">>> 之后: KERNEL_SRC=$DEST bash scripts/02_build_modules.sh"
