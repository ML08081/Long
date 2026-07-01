#!/usr/bin/env bash
# =============================================================================
#  03_make_package.sh -- 把编好的 .ko + 加载脚本打成可迁移的部署包
#  产物: build/uvc_modules_loong.tar.gz
# =============================================================================
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
KROOT="$(dirname "$HERE")"
OUT="$KROOT/build/modules"

ls "$OUT"/*.ko >/dev/null 2>&1 || { echo "错误: 未找到 .ko,请先运行 02_build_modules.sh"; exit 1; }

PKG="$KROOT/build/uvc_pkg"; rm -rf "$PKG"; mkdir -p "$PKG/modules"
cp "$OUT"/*.ko              "$PKG/modules/"
cp "$KROOT/target/install.sh"   "$PKG/"
cp "$KROOT/target/load_uvc.sh"  "$PKG/"
chmod +x "$PKG"/*.sh
modinfo "$OUT/uvcvideo.ko" 2>/dev/null | grep -i vermagic > "$PKG/VERMAGIC.txt" || true
printf '%s\n' "$(ls "$PKG/modules")" > "$PKG/MODULES.txt"

TAR="$KROOT/build/uvc_modules_loong.tar.gz"
tar -czf "$TAR" -C "$KROOT/build" uvc_pkg

echo "部署包: $TAR"
echo ""
echo "迁移到板子并安装:"
echo "  scp $TAR root@<板子IP>:/tmp/"
echo "  ssh root@<板子IP> 'cd /tmp && tar xzf uvc_modules_loong.tar.gz && cd uvc_pkg && sh install.sh'"
