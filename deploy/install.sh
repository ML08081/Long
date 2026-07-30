#!/bin/bash
# =============================================================================
#  install.sh -- PatrolSystem 龙芯板一键部署脚本
#  在龙芯板上以 root 运行：  sudo ./install.sh
# =============================================================================
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"

[ "$(id -u)" = "0" ] || { echo "请用 root 运行:  sudo ./install.sh"; exit 1; }

echo "==> [1/7] 校验可执行文件架构"
if command -v file >/dev/null 2>&1; then
    if ! file "$HERE/patrol_system" | grep -q "LoongArch"; then
        echo "错误: patrol_system 不是 LoongArch 架构，拒绝安装！"
        file "$HERE/patrol_system"
        exit 1
    fi
    echo "    OK: 确认为 LoongArch 可执行文件"
fi
[ -f "$HERE/VERSION" ] && echo "    部署版本: $(cat "$HERE/VERSION")"

echo "==> [2/7] 停用系统自带 wifi-autoconnect（避免其占用 wlan0 进入 AP 模式）"
echo "    注意: 此操作会修改板端 WiFi 服务状态，并将 wifi-autoconnect.service 设为 masked。"
echo "    如需恢复: systemctl unmask wifi-autoconnect.service && systemctl enable --now wifi-autoconnect.service"
systemctl stop wifi-autoconnect.service 2>/dev/null || true
systemctl disable wifi-autoconnect.service 2>/dev/null || true
systemctl mask wifi-autoconnect.service 2>/dev/null || true
echo "    (如需恢复系统自带 WiFi: systemctl unmask wifi-autoconnect.service)"

echo "==> [3/7] 安装可执行文件与脚本"
install -m 755 "$HERE/patrol_system"            /usr/bin/patrol_system
install -m 755 "$HERE/scripts/start.sh"         /usr/bin/patrol_start
install -m 755 "$HERE/scripts/setup_network.sh" /usr/bin/patrol_setup_network
install -m 755 "$HERE/scripts/health_check.sh"  /usr/bin/patrol_healthcheck

echo "==> [4/7] 安装配置文件（已存在则保留，不覆盖）"
mkdir -p /etc/patrol
[ -f /etc/patrol/config.json ]  && echo "    /etc/patrol/config.json 已存在，跳过" \
    || install -m 644 "$HERE/config/config.json" /etc/patrol/config.json
[ -f /etc/patrol/network.conf ] && echo "    /etc/patrol/network.conf 已存在，跳过" \
    || install -m 644 "$HERE/network.conf" /etc/patrol/network.conf

echo "==> [5/7] 创建日志目录"
mkdir -p /var/log/patrol

echo "==> [6/7] 安装 systemd 服务"
install -m 644 "$HERE/systemd/patrol.service"          /etc/systemd/system/patrol.service
install -m 644 "$HERE/systemd/patrol-network.service"  /etc/systemd/system/patrol-network.service
systemctl daemon-reload

echo "==> [7/7] 启用开机自启"
systemctl enable patrol-network.service
systemctl enable patrol.service

echo ""
echo "=================== 部署完成 ==================="
echo " 立即连接WiFi: systemctl start patrol-network"
echo " 立即启动服务: systemctl start patrol"
echo " WiFi连接日志: journalctl -u patrol-network -f"
echo " 服务日志:     journalctl -u patrol -f"
echo " 程序日志:     /var/log/patrol/patrol-v*.log"
echo " 健康检测日志: /var/log/patrol/health.log"
echo " WiFi/IP配置:  /etc/patrol/network.conf  (SSID=B410, 固定IP=192.168.3.100)"
echo ""
echo " 重启后自动: 连接WiFi(B410) -> 固定IP 192.168.3.100 -> 启动巡检服务。"
echo " 上位机 LongLook 连接: 192.168.3.100:8080"
echo "================================================"
