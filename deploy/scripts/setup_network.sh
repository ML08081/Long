#!/bin/bash
# LoongOS/AIC8800 station connection. MLML uses DHCP; secrets stay on the device.
set -u
umask 077
CONF="${1:-/etc/patrol/network.conf}"
[ -f "$CONF" ] && . "$CONF"
IFACE="${WIFI_IFACE:-wlan0}"
WPA_CONF="${WPA_CONF:-/etc/wpa_supplicant.conf}"
PASSWORD_FILE="${WIFI_PASSWORD_FILE:-/etc/patrol/wifi-password}"
STATIC_IP="${STATIC_IP:-}"
STATIC_SSID="${STATIC_SSID:-}"
NETMASK="${NETMASK:-24}"
GATEWAY="${GATEWAY:-}"
DNS1="${DNS1:-}"
DNS2="${DNS2:-}"
WPA_PID="/run/patrol-wpa-$IFACE.pid"
DHCP_PID="/run/patrol-dhcp-$IFACE.pid"
ACTION_PID="/run/patrol-wpa-action-$IFACE.pid"
log(){ echo "[network] $*"; }

case "$IFACE" in *[!a-zA-Z0-9_.:-]*|'') log "Invalid interface"; exit 1;; esac
if [ -z "${WIFI_SSID:-}" ]; then log "No Wi-Fi SSID configured"; exit 1; fi
if [ -f "$PASSWORD_FILE" ]; then
    # Read without evaluation: quotes/backticks in a password are literal data.
    IFS= read -r WIFI_PASSWORD < "$PASSWORD_FILE" || true
    chmod 600 "$PASSWORD_FILE" || exit 1
fi
if [ -z "${WIFI_PASSWORD:-}" ]; then
    log "Missing device password file; keep current connection intact"
    exit 1
fi
if [ "${#WIFI_PASSWORD}" -lt 8 ] || [ "${#WIFI_PASSWORD}" -gt 63 ]; then
    log "Wi-Fi password length must be 8..63 bytes"; exit 1
fi

# Generate atomically before disturbing the current connection.
quote_wpa(){ local value="$1"; value="${value//\\/\\\\}"; value="${value//\"/\\\"}"; printf '%s' "$value"; }
TEMP_CONF="$(mktemp "${WPA_CONF}.XXXXXX")" || exit 1
trap 'rm -f "$TEMP_CONF"' EXIT
{
    printf 'ctrl_interface=/var/run/wpa_supplicant\nupdate_config=0\n'
    printf 'network={\n    ssid="%s"\n    psk="%s"\n    key_mgmt=WPA-PSK\n    priority=%s\n    scan_ssid=1\n}\n' \
        "$(quote_wpa "$WIFI_SSID")" "$(quote_wpa "$WIFI_PASSWORD")" "${WIFI_PRIORITY:-10}"
    # Legacy backups may contain a secondary network; allow rollback to them.
    if [ -n "${WIFI_SSID2:-}" ] && [ -n "${WIFI_PASSWORD2:-}" ]; then
        printf 'network={\n    ssid="%s"\n    psk="%s"\n    key_mgmt=WPA-PSK\n    priority=%s\n    scan_ssid=1\n}\n' \
            "$(quote_wpa "$WIFI_SSID2")" "$(quote_wpa "$WIFI_PASSWORD2")" "${WIFI_PRIORITY2:-1}"
    fi
} > "$TEMP_CONF"
chmod 600 "$TEMP_CONF" || exit 1
mv -f "$TEMP_CONF" "$WPA_CONF" || exit 1

systemctl stop wifi-autoconnect.service 2>/dev/null || true
for i in $(seq 1 40); do
    ip link show "$IFACE" >/dev/null 2>&1 && break
    sleep 1
done
if ! ip link show "$IFACE" >/dev/null 2>&1; then log "Interface not ready: $IFACE"; exit 1; fi

stop_pid(){
    [ -f "$1" ] || return 0
    local pid
    IFS= read -r pid < "$1" || true
    case "${pid:-}" in *[!0-9]*|'') ;; *) kill "$pid" 2>/dev/null || true;; esac
    rm -f "$1"
}
stop_pid "$ACTION_PID"
stop_pid "$DHCP_PID"
stop_pid "$WPA_PID"
wpa_cli -i "$IFACE" terminate >/dev/null 2>&1 || true
# The bundled LoongOS AP service can leave a hostapd process after stopping.
killall hostapd 2>/dev/null || true
ip link set "$IFACE" down 2>/dev/null || true
iw dev "$IFACE" set type managed 2>/dev/null || true
ip link set "$IFACE" up || exit 1

connected=0
for round in 1 2; do
    for driver in nl80211 wext ""; do
        stop_pid "$WPA_PID"
        wpa_cli -i "$IFACE" terminate >/dev/null 2>&1 || true
        rm -f "/var/run/wpa_supplicant/$IFACE"
        if [ -n "$driver" ]; then
            wpa_supplicant -B -D "$driver" -i "$IFACE" -c "$WPA_CONF" -P "$WPA_PID" >/dev/null 2>&1 || continue
        else
            wpa_supplicant -B -i "$IFACE" -c "$WPA_CONF" -P "$WPA_PID" >/dev/null 2>&1 || continue
        fi
        for i in $(seq 1 12); do
            wpa_cli -i "$IFACE" status 2>/dev/null | grep -q '^wpa_state=COMPLETED$' && { connected=1; break; }
            sleep 1
        done
        [ "$connected" = 1 ] && break
        log "Association timed out with driver ${driver:-default}"
    done
    [ "$connected" = 1 ] && break
    sleep 2
done
if [ "$connected" != 1 ]; then log "Association failed; retry service after checking hotspot"; exit 1; fi
CUR_SSID="$(wpa_cli -i "$IFACE" status 2>/dev/null | sed -n 's/^ssid=//p' | head -1)"

ip -4 addr flush dev "$IFACE" scope global || exit 1
ip -4 route flush default dev "$IFACE" 2>/dev/null || true
if [ -n "$STATIC_IP" ] && [ -n "$STATIC_SSID" ] && [ "$CUR_SSID" = "$STATIC_SSID" ]; then
    ip addr add "$STATIC_IP/$NETMASK" dev "$IFACE" || exit 1
    [ -z "$GATEWAY" ] || ip route replace default via "$GATEWAY" dev "$IFACE" || exit 1
    if [ -n "$DNS1$DNS2" ]; then
        { [ -z "$DNS1" ] || printf 'nameserver %s\n' "$DNS1"; [ -z "$DNS2" ] || printf 'nameserver %s\n' "$DNS2"; } > /etc/resolv.conf
    fi
else
    # The hook applies address, route and DHCP DNS; no B410 resolver overwrite.
    udhcpc -b -i "$IFACE" -p "$DHCP_PID" -s /usr/bin/patrol_dhcp_hook -t 8 -T 2 >/dev/null 2>&1 || exit 1
    # Reassociation must renew the lease, including a changed phone subnet.
    wpa_cli -B -i "$IFACE" -a /usr/bin/patrol_wifi_link_action -P "$ACTION_PID" >/dev/null 2>&1 || exit 1
fi
CURIP="$(ip -4 addr show dev "$IFACE" scope global 2>/dev/null | awk '/inet /{print $2; exit}')"
log "Connected SSID=$CUR_SSID address=${CURIP:-DHCP pending}; controller discovers UDP :8083"
exit 0
