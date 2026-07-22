#!/bin/bash
# =============================================================================
#  make_package.sh -- 在开发主机(WSL)上构建龙芯部署包
#  流程：龙芯交叉编译 -> 强制校验产物为 LoongArch -> 组装部署包 -> 按版本打 tar.gz
# =============================================================================
set -e
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(dirname "$SCRIPT_DIR")"
BUILD="$ROOT/build-loong"
PKG="$ROOT/patrol_deploy"
VER="$(tr -d ' \n\r' < "$ROOT/VERSION")"

echo "==> [1/4] 龙芯交叉编译 (v$VER)"
bash "$ROOT/scripts/build_loong.sh"

BIN="$BUILD/bin/patrol_system"
echo ""
echo "==> [2/4] 校验交叉编译工具链产物架构"
[ -f "$BIN" ] || { echo "错误: 未找到产物 $BIN"; exit 1; }
ARCH_INFO="$(file "$BIN")"
echo "    $ARCH_INFO"
if ! echo "$ARCH_INFO" | grep -q "LoongArch"; then
    echo ""
    echo "##########################################################"
    echo "# 错误: 产物不是 LoongArch 架构！                        #"
    echo "# 可能误用了本机 x86 编译器而非龙芯交叉工具链。          #"
    echo "##########################################################"
    exit 1
fi
echo "    OK: 确认为龙芯 LoongArch64 交叉编译产物"
# 旧世界校验：动态解释器必须是 /lib64/ld.so.1
if echo "$ARCH_INFO" | grep -qE "ld-linux-loongarch"; then
    echo ""
    echo "##########################################################"
    echo "# 错误: 产物为【新世界 new-world】(ld-linux-loongarch)!  #"
    echo "# 本工程要求【旧世界 old-world】GCC 8.3 工具链。          #"
    echo "##########################################################"
    exit 1
fi
if echo "$ARCH_INFO" | grep -q "ld.so.1"; then
    echo "    OK: 确认为旧世界(old-world) 产物 (interpreter=/lib64/ld.so.1)"
fi

echo ""
echo "==> [3/4] 组装部署包 (v$VER)"
rm -rf "$PKG"
mkdir -p "$PKG/config" "$PKG/scripts" "$PKG/systemd"
cp "$BIN"                                          "$PKG/patrol_system"
cp "$ROOT/VERSION"                                 "$PKG/VERSION"
cp "$ROOT/config/config.json"                      "$PKG/config/config.json"
cp "$ROOT/deploy/scripts/start.sh"                 "$PKG/scripts/start.sh"
cp "$ROOT/deploy/scripts/setup_network.sh"         "$PKG/scripts/setup_network.sh"
cp "$ROOT/deploy/scripts/health_check.sh"          "$PKG/scripts/health_check.sh"
cp "$ROOT/deploy/systemd/patrol.service"           "$PKG/systemd/patrol.service"
cp "$ROOT/deploy/systemd/patrol-network.service"   "$PKG/systemd/patrol-network.service"
cp "$ROOT/deploy/systemd/can0.service"             "$PKG/systemd/can0.service"
cp "$ROOT/deploy/network.conf"                     "$PKG/network.conf"
cp "$ROOT/deploy/wpa_supplicant.conf"              "$PKG/wpa_supplicant.conf"
cp "$ROOT/deploy/install.sh"                        "$PKG/install.sh"
chmod +x "$PKG/scripts/"*.sh "$PKG/install.sh"

echo ""
echo "==> [4/4] 打包 tar.gz"
TARBALL="$ROOT/patrol_deploy_v${VER}.tar.gz"
rm -f "$TARBALL"
tar -czf "$TARBALL" -C "$ROOT" patrol_deploy

echo ""
echo "=================== 部署包已生成 (v$VER) ==================="
echo " 目录:   $PKG"
echo " 压缩包: $TARBALL"
echo ""
echo " 部署到龙芯板:"
echo "   scp $TARBALL root@<龙芯IP>:/tmp/"
echo "   ssh root@<龙芯IP> 'cd /tmp && tar xzf patrol_deploy_v${VER}.tar.gz && cd patrol_deploy && ./install.sh'"
echo "============================================================"
