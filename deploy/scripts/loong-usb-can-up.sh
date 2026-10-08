#!/bin/sh
# PCAN 8.14 exposes a virtual netdev: identify it via /proc/pcan, not canN.
set -eu
PATH=/usr/sbin:/usr/bin:/sbin:/bin
export PATH

IFACE="${1:-can2}"
BITRATE="${2:-500000}"
MODE="${3:-up}"
case "$IFACE" in can*) suffix=${IFACE#can};; *) exit 2;; esac
case "$suffix" in ''|*[!0-9]*) echo 'Invalid CAN interface' >&2; exit 2;; esac
case "$BITRATE" in ''|*[!0-9]*) echo 'Invalid bitrate' >&2; exit 2;; esac
[ "$BITRATE" -ge 10000 ] && [ "$BITRATE" -le 1000000 ] || exit 2
case "$MODE" in up|recover) ;; *) echo 'Expected up or recover' >&2; exit 2;; esac

exec 9>"/run/loong-usb-can-$IFACE.lock"
# BusyBox flock has no -w; systemd bounds the complete operation to 30 seconds.
flock -x 9 || exit 1
STATE="/run/loong-usb-can-$IFACE.state"
STATUS="/run/loong-usb-can-$IFACE.status"

notice() {
    old=$(cat "$STATUS" 2>/dev/null || true)
    if [ "$old" != "$1" ]; then
        echo "USB-CAN: $2"
        printf '%s\n' "$1" > "$STATUS"
    fi
}

usb_interfaces() {
    [ -r /proc/pcan ] || return 0
    awk '$1 ~ /^[0-9]+$/ && $2 == "usb" {print $3}' /proc/pcan
}

if ! modprobe pcan; then
    notice driver-error 'pcan driver could not be loaded; check kernel/module compatibility'
    exit 1
fi

count=0
while :; do
    devices=$(usb_interfaces)
    [ -n "$devices" ] && break
    [ "$MODE" = up ] && [ "$count" -lt 20 ] || break
    sleep 0.5
    count=$((count + 1))
done

if [ -z "$devices" ]; then
    dfu=0
    for path in /sys/bus/usb/devices/*; do
        [ -r "$path/idVendor" ] && [ -r "$path/idProduct" ] || continue
        if [ "$(cat "$path/idVendor")" = 314b ] && [ "$(cat "$path/idProduct")" = 0106 ]; then
            dfu=1
        fi
    done
    if [ "$dfu" -eq 1 ]; then
        notice missing-dfu '314b:0106 APM32 DFU ISP Mode; reconnect adapter with BOOT/download mode disabled'
    else
        notice missing 'no PCAN USB interface; check adapter USB connection/power'
    fi
    rm -f "$STATE"
    if [ "$MODE" = recover ]; then
        if systemctl is-active --quiet usb-can2.service; then
            systemctl --no-block stop usb-can2.service
        fi
        exit 0
    fi
    exit 1
fi

# Never guess between adapters or bring up an unrelated onboard interface.
set -- $devices
if [ "$#" -ne 1 ]; then
    notice ambiguous 'multiple PCAN USB interfaces; explicit device mapping is required'
    exit 1
fi
SOURCE="$1"
case "$SOURCE" in can*) suffix=${SOURCE#can};; *) exit 1;; esac
case "$suffix" in ''|*[!0-9]*) notice invalid 'invalid PCAN interface name'; exit 1;; esac
if [ "$SOURCE" != "$IFACE" ]; then
    if [ -e "/sys/class/net/$IFACE" ]; then
        notice collision "$IFACE is occupied by another device; refusing to change it"
        exit 1
    fi
    ip link set dev "$SOURCE" down
    ip link set dev "$SOURCE" name "$IFACE"
    echo "USB-CAN: renamed verified PCAN USB interface $SOURCE to $IFACE"
fi

# Check identity again after renaming; hot-unplug races must fail safely.
[ "$(usb_interfaces)" = "$IFACE" ] || exit 1
[ "$(cat "/sys/class/net/$IFACE/type")" = 280 ] || exit 1
details=$(ip -details link show dev "$IFACE")
index=$(cat "/sys/class/net/$IFACE/ifindex")
ready="ready:$IFACE:$index:$BITRATE:100"
previous=$(cat "$STATE" 2>/dev/null || true)
changed=0
[ "$previous" = "$ready" ] || changed=1
if ! printf '%s\n' "$details" | grep -Eq '<[^>]*\bUP\b' ||
   ! printf '%s\n' "$details" | grep -Eq "bitrate $BITRATE( |$)" ||
   ! printf '%s\n' "$details" | grep -Eq 'restart-ms 100( |$)' ||
   printf '%s\n' "$details" | grep -Eq 'LOOPBACK|LISTEN-ONLY|ONE-SHOT'; then
    ip link set dev "$IFACE" down
    ip link set dev "$IFACE" type can bitrate "$BITRATE" restart-ms 100 loopback off listen-only off one-shot off
    ip link set dev "$IFACE" up
    changed=1
fi

printf '%s\n' "$ready" > "$STATE"
notice "$ready" "$IFACE ready: $BITRATE bit/s, restart-ms=100, PCAN USB, ifindex=$index"

if [ "$MODE" = recover ] && [ "$changed" -eq 1 ]; then
    # Queue without waiting: patrol is ordered after usb-can2.service.
    systemctl --no-block start usb-can2.service
    if systemctl is-active --quiet patrol.service; then
        echo 'USB-CAN: interface recovered; restarting running patrol.service to reopen its CAN socket'
        systemctl --no-block try-restart patrol.service
    fi
fi
