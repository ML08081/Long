#!/bin/sh
# Release only the verified single PCAN USB adapter during host shutdown.
set -eu
PATH=/usr/sbin:/usr/bin:/sbin:/bin
export PATH
IFACE=${1:-can2}
case "$IFACE" in can*) suffix=${IFACE#can};; *) exit 2;; esac
case "$suffix" in ''|*[!0-9]*) exit 2;; esac
# A service restart must not detach the adapter from its running host.
state=$(systemctl is-system-running 2>/dev/null || true)
[ "$state" = stopping ] || exit 0
exec 9>"/run/loong-usb-can-$IFACE.lock"
flock -x 9 || exit 1
[ -r /proc/pcan ] || exit 0
devices=$(awk '$1 ~ /^[0-9]+$/ && $2 == "usb" {print $3}' /proc/pcan)
[ "$devices" = "$IFACE" ] || exit 0
[ "$(cat "/sys/class/net/$IFACE/type" 2>/dev/null || true)" = 280 ] || exit 0
binding=
for entry in /sys/bus/usb/drivers/pcan/*:*; do
    [ -L "$entry" ] || continue
    [ -z "$binding" ] || exit 0
    binding=${entry##*/}
done
[ -n "$binding" ] || exit 0
device=${binding%%:*}
base=/sys/bus/usb/devices/$device
[ "$(cat "$base/idVendor")" = 0c72 ] || exit 0
[ "$(cat "$base/idProduct")" = 000c ] || exit 0
ip link set dev "$IFACE" down
printf '%s\n' "$binding" > /sys/bus/usb/drivers/pcan/unbind
echo "USB-CAN: released verified PCAN USB $binding before host shutdown"
