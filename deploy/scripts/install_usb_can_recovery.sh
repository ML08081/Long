#!/bin/sh
# Run from this standalone recovery package on the board as root.
set -eu
PATH=/usr/sbin:/usr/bin:/sbin:/bin
export PATH
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
if [ -f "$ROOT/tools/loong-usb-can-up.sh" ]; then
    SCRIPTS="$ROOT/tools"
    DEPLOY="$ROOT/deploy"
else
    SCRIPTS="$ROOT/scripts"
    DEPLOY="$ROOT"
fi
case "${1:-}" in ''|--install-only) ;; *) echo 'Expected --install-only or no argument' >&2; exit 2;; esac
[ "$(id -u)" = 0 ] || { echo 'Run as root' >&2; exit 1; }
sh -n "$SCRIPTS/loong-usb-can-up.sh"
sh -n "$SCRIPTS/loong-usb-can-down.sh"
BACKUP="/root/can-recovery-backup-$(date +%Y%m%d-%H%M%S)"
mkdir -p "$BACKUP"
for path in \
    usr/local/sbin/loong-usb-can-up \
    usr/local/sbin/loong-usb-can-down \
    etc/systemd/system/usb-can2.service \
    etc/systemd/system/usb-can2-recover.service \
    etc/systemd/system/usb-can2-recover.timer \
    etc/systemd/system/patrol.service.d/10-usb-can2.conf \
    etc/modprobe.d/pcan-can2.conf \
    etc/modules-load.d/pcan.conf \
    etc/patrol/config.json; do
    if [ -e "/$path" ]; then
        printf '%s\n' "$path" >> "$BACKUP/existing.list"
    else
        printf '%s\n' "$path" >> "$BACKUP/absent.list"
    fi
done
tar -C / -czf "$BACKUP/files.tar.gz" -T "$BACKUP/existing.list"
systemctl is-enabled usb-can2.service > "$BACKUP/usb-can2.enabled" 2>&1 || true
systemctl is-enabled usb-can2-recover.timer > "$BACKUP/timer.enabled" 2>&1 || true
echo "Backup: $BACKUP"

timer_was_active=0
if systemctl is-active --quiet usb-can2-recover.timer; then
    timer_was_active=1
fi
systemctl stop usb-can2-recover.timer usb-can2-recover.service 2>/dev/null || true
install -d /usr/local/sbin /etc/modprobe.d /etc/modules-load.d /etc/systemd/system/patrol.service.d
install -m 755 "$SCRIPTS/loong-usb-can-up.sh" /usr/local/sbin/loong-usb-can-up
install -m 755 "$SCRIPTS/loong-usb-can-down.sh" /usr/local/sbin/loong-usb-can-down
for name in usb-can2.service usb-can2-recover.service usb-can2-recover.timer; do
    install -m 644 "$DEPLOY/systemd/$name" "/etc/systemd/system/$name"
done
install -m 644 "$DEPLOY/systemd/patrol-usb-can2.conf" /etc/systemd/system/patrol.service.d/10-usb-can2.conf
install -m 644 "$DEPLOY/modprobe/pcan-can2.conf" /etc/modprobe.d/pcan-can2.conf
printf 'pcan\n' > /etc/modules-load.d/pcan.conf
if [ -f "$ROOT/config.board.live.json" ]; then
    install -m 644 "$ROOT/config.board.live.json" /etc/patrol/config.json
fi
if command -v systemd-analyze >/dev/null 2>&1; then
    systemd-analyze verify /etc/systemd/system/usb-can2.service /etc/systemd/system/usb-can2-recover.service /etc/systemd/system/usb-can2-recover.timer
fi
systemctl daemon-reload
for name in usb-can2.service usb-can2-recover.service usb-can2-recover.timer; do
    [ "$(systemctl show "$name" -p LoadState --value)" = loaded ] || exit 1
done
systemctl enable usb-can2.service usb-can2-recover.timer
if [ "${1:-}" != --install-only ]; then
    systemctl start usb-can2-recover.timer
    systemctl restart usb-can2.service
    systemctl try-restart patrol.service
elif [ "$timer_was_active" -eq 1 ]; then
    systemctl start usb-can2-recover.timer
fi
echo 'Installed; inspect can2 statistics and patrol RX/heartbeat logs.'
