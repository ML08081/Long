#!/bin/bash
# =============================================================================
#  setup_network.sh -- 开机 WiFi 自动连接（Station 模式，连 B410，设固定 IP）
#  由 patrol-network.service 在开机时调用。针对 LoongOS + AIC8800D80 WiFi：
#    - 等待 wlan0 芯片就绪（固件加载慢，规避开机时序竞争 -> 这是之前失败的主因）
#    - 停掉系统自带 wifi-autoconnect / AP 模式，强制 station(managed) 模式
#    - 多驱动尝试 wpa_supplicant 并等待关联成功
#    - 关联后设置固定 IP，保证上位机固定连接
# =============================================================================
CONF="${1:-/etc/patrol/network.conf}"
[ -f "$CONF" ] && . "$CONF"

IFACE="${WIFI_IFACE:-wlan0}"
WPA_CONF="${WPA_CONF:-/etc/wpa_supplicant.conf}"
STATIC_IP="${STATIC_IP:-}"
NETMASK="${NETMASK:-24}"
GATEWAY="${GATEWAY:-}"
DNS1="${DNS1:-192.168.3.1}"
DNS2="${DNS2:-223.5.5.5}"
PING_HOST="${PING_HOST:-baidu.com}"

log(){ echo "[network] $*"; }

# 0) 停掉 LoongOS 自带 WiFi 服务（会把 wlan0 置为 AP 模式，与本流程冲突）
systemctl stop wifi-autoconnect.service 2>/dev/null || true

# 1) 等待无线网卡就绪（AIC8800 固件加载较慢，开机时 wlan0 可能尚未出现）
log "等待无线网卡 $IFACE 就绪..."
for i in $(seq 1 40); do
    ip link show "$IFACE" >/dev/null 2>&1 && { log "$IFACE 已就绪"; break; }
    sleep 1
done
if ! ip link show "$IFACE" >/dev/null 2>&1; then
    log "错误: 无线网卡 $IFACE 始终未就绪，退出（主服务仍会启动）"
    exit 0
fi

# 2) 生成 wpa_supplicant.conf（B410 + MLML 双网络，priority 自动择优/failover：
#    优先连 priority 高且可用的；断开自动切另一个）
log "生成 $WPA_CONF (主=$WIFI_SSID  次=${WIFI_SSID2:-无})"
{
    echo "ctrl_interface=/var/run/wpa_supplicant"
    echo "update_config=1"
} > "$WPA_CONF"
add_network(){   # $1=ssid $2=psk $3=priority
    [ -n "$1" ] || return 0
    {
        echo ""
        echo "network={"
        echo "    ssid=\"$1\""
        echo "    psk=\"$2\""
        echo "    key_mgmt=WPA-PSK"
        echo "    priority=$3"
        echo "    scan_ssid=1"
        echo "}"
    } >> "$WPA_CONF"
}
add_network "$WIFI_SSID"  "$WIFI_PASSWORD"  "${WIFI_PRIORITY:-5}"
add_network "$WIFI_SSID2" "$WIFI_PASSWORD2" "${WIFI_PRIORITY2:-1}"

# 3) 整体连接尝试（最多 2 轮）
connected=0
for round in 1 2; do
    log "===== WiFi 连接尝试 第 $round 轮 ====="

    # 3.1 清理残留 + 切换到 station(managed) 模式（脱离 AP 模式）
    killall wpa_supplicant 2>/dev/null || true
    killall hostapd 2>/dev/null || true
    rm -f "/var/run/wpa_supplicant/$IFACE" 2>/dev/null || true
    sleep 1
    ip link set "$IFACE" down 2>/dev/null || true
    iw dev "$IFACE" set type managed 2>/dev/null || true
    ip link set "$IFACE" up 2>/dev/null || true
    sleep 1

    # 3.2 多驱动尝试 wpa_supplicant（AIC8800 优先 nl80211，回退 wext / 默认）
    for drv in nl80211 wext ""; do
        killall wpa_supplicant 2>/dev/null || true
        rm -f "/var/run/wpa_supplicant/$IFACE" 2>/dev/null || true
        sleep 1
        if [ -n "$drv" ]; then
            log "wpa_supplicant -B -D $drv -i $IFACE"
            wpa_supplicant -B -D "$drv" -i "$IFACE" -c "$WPA_CONF" 2>/dev/null || continue
        else
            log "wpa_supplicant -B -i $IFACE （默认驱动）"
            wpa_supplicant -B -i "$IFACE" -c "$WPA_CONF" 2>/dev/null || continue
        fi
        # 等待关联成功（最多 12 秒）
        for i in $(seq 1 12); do
            if wpa_cli -i "$IFACE" status 2>/dev/null | grep -q "wpa_state=COMPLETED"; then
                connected=1; break
            fi
            if iw dev "$IFACE" link 2>/dev/null | grep -qi "Connected to"; then
                connected=1; break
            fi
            sleep 1
        done
        [ "$connected" = "1" ] && { log "WiFi 关联成功（驱动 ${drv:-默认}）"; break; }
        log "驱动 ${drv:-默认} 关联超时，尝试下一个"
    done

    [ "$connected" = "1" ] && break
    log "第 $round 轮未关联成功，2 秒后重试"
    sleep 2
done

[ "$connected" = "1" ] || log "警告: WiFi 关联失败（检查 SSID=$WIFI_SSID / 密码 / 信号强度）"

# 4) 配置 IP：当前连的是 STATIC_SSID(B410) 才用固定 IP；否则(手机热点等异网段) 用 DHCP。
#    跨网段固定 IP 不通，故按“当前关联 SSID”自适应选择。
CUR_SSID=$(wpa_cli -i "$IFACE" status 2>/dev/null | sed -n 's/^ssid=//p' | head -1)
log "当前关联 SSID: ${CUR_SSID:-未知}"
if [ -n "$STATIC_IP" ] && [ -n "$STATIC_SSID" ] && [ "$CUR_SSID" = "$STATIC_SSID" ]; then
    log "连的是 $STATIC_SSID → 固定 IP: $STATIC_IP/$NETMASK  网关=${GATEWAY:-无}"
    ip addr flush dev "$IFACE" 2>/dev/null || true
    ip addr add "$STATIC_IP/$NETMASK" dev "$IFACE"
    ip link set "$IFACE" up 2>/dev/null || true
    [ -n "$GATEWAY" ] && ip route replace default via "$GATEWAY" dev "$IFACE" 2>/dev/null || true
else
    log "连的是 ${CUR_SSID:-其它网络}(非 $STATIC_SSID) → DHCP 自动获取 IP（上位机靠 UDP 自动发现）"
    ip addr flush dev "$IFACE" 2>/dev/null || true
    udhcpc -b -i "$IFACE" -t 8 -T 2 2>/dev/null || true
    sleep 2
fi

# 5) DNS
{ echo "nameserver $DNS1"; echo "nameserver $DNS2"; } > /etc/resolv.conf

# 6) 连通性验证并打印本机 IP（上位机连接此地址）
CURIP=$(ip -4 addr show "$IFACE" 2>/dev/null | awk '/inet /{print $2; exit}')
log "本机 IP: ${CURIP:-未获取}  （上位机 LongLook 连接此地址:8080）"
if [ -n "$GATEWAY" ] && ping -c 2 -W 2 "$GATEWAY" >/dev/null 2>&1; then
    log "[OK] 网关 $GATEWAY 可达，B410 局域网连通"
elif ping -c 1 -W 2 "$PING_HOST" >/dev/null 2>&1; then
    log "[OK] 外网 $PING_HOST 连通"
else
    log "[WARN] 网关/外网均不通，请检查 WiFi 关联与固定 IP 网段是否与 B410 一致"
fi

# 始终成功返回，避免阻塞主服务（主程序绑定 0.0.0.0，不依赖外网）
exit 0
