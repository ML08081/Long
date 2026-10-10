#!/bin/bash
# Stage MLML DHCP, confirm over its new link, or recover locally after SSH loss.
# No target Python modules are required. Password comes only from stdin/terminal.
set -eu
umask 077
BASE=/var/lib/patrol/network-transition
PENDING=$BASE/pending
CONFIG=/etc/patrol/network.conf
PASSWORD_FILE=/etc/patrol/wifi-password
WPA=/etc/wpa_supplicant.conf
UNIT_DIR=/etc/systemd/system
SELF=/usr/bin/patrol_network_migrate
SETUP=/usr/bin/patrol_setup_network
DHCP_HOOK=/usr/bin/patrol_dhcp_hook
LINK_ACTION=/usr/bin/patrol_wifi_link_action
LOCK=/run/patrol-network-migration.lock
ON_ERROR=0
LOCK_HELD=0
TEMP_FILE=

fail(){ printf '[network] %s\n' "$1" >&2; exit 1; }
stop_timer(){ systemctl disable --now patrol-network-rollback.timer >/dev/null 2>&1 || true; }
acquire_lock(){
    local tries=0 owner
    while ! mkdir "$LOCK" 2>/dev/null; do
        tries=$((tries+1))
        owner="$(cat "$LOCK/pid" 2>/dev/null || true)"
        if [ "$tries" -ge 5 ]; then
            case "$owner" in *[!0-9]*|'') rm -f "$LOCK/pid"; rmdir "$LOCK" 2>/dev/null || true;;
                *) if ! kill -0 "$owner" 2>/dev/null; then rm -f "$LOCK/pid"; rmdir "$LOCK" 2>/dev/null || true; fi;;
            esac
        fi
        [ "$tries" -lt 360 ] || fail 'Another network operation is still running'
        sleep 1
    done
    printf '%s\n' "$$" > "$LOCK/pid"
    LOCK_HELD=1
}
backup_file(){
    local name="$1" original="$2"
    if [ -e "$original" ]; then
        cp "$original" "$BACKUP/$name"
        chmod 600 "$BACKUP/$name"
        : > "$BACKUP/exists-$name"
    fi
}
restore_file(){
    local name="$1" original="$2"
    if [ -f "$BACKUP/exists-$name" ]; then
        cp "$BACKUP/$name" "$original" || return 1
        chmod 600 "$original" || return 1
    else
        rm -f "$original" || return 1
    fi
}
get_backup(){
    [ -L "$PENDING" ] || return 1
    BACKUP="$(readlink "$PENDING")" || return 1
    case "$BACKUP" in "$BASE"/backup-*) [ -d "$BACKUP" ] || return 1;; *) return 1;; esac
}
restore_profile(){
    get_backup || return 1
    local switch_unit
    switch_unit="$(cat "$BACKUP/switch-unit")" || return 1
    systemctl stop "$switch_unit.timer" >/dev/null 2>&1 || true
    restore_file config "$CONFIG" || return 1
    restore_file password "$PASSWORD_FILE" || return 1
    restore_file wpa "$WPA" || return 1
    stop_timer
    printf 'rolled_back\n' > "$BACKUP/result"
    rm -f "$PENDING"
}
cleanup(){
    local code=$?
    trap - EXIT
    if [ "$code" != 0 ] && [ "$ON_ERROR" = 1 ] && [ -L "$PENDING" ]; then
        restore_profile || printf '[network] Recovery remains pending; inspect device timer\n' >&2
    fi
    [ -z "$TEMP_FILE" ] || rm -f "$TEMP_FILE"
    if [ "$LOCK_HELD" = 1 ]; then rm -f "$LOCK/pid"; rmdir "$LOCK" 2>/dev/null || true; fi
    exit "$code"
}
trap cleanup EXIT
trap 'exit 1' HUP INT TERM
atomic_stdin(){
    local target="$1" mode="${2:-600}"
    TEMP_FILE="$(mktemp "${target}.XXXXXX")"
    cat > "$TEMP_FILE"
    chmod "$mode" "$TEMP_FILE"
    mv -f "$TEMP_FILE" "$target"
    TEMP_FILE=
}
stage(){
    local timeout="$1" switch_unit
    [ ! -e "$PENDING" ] && [ ! -L "$PENDING" ] || fail 'Confirm or roll back the pending migration first'
    for helper in "$SELF" "$SETUP" "$DHCP_HOOK" "$LINK_ACTION"; do
        [ -x "$helper" ] || fail 'A required network helper is not installed'
    done
    mkdir -p "$BASE" "$(dirname "$CONFIG")" "$UNIT_DIR"
    chmod 700 "$BASE"
    BACKUP="$(mktemp -d "$BASE/backup-XXXXXXXX")"
    chmod 700 "$BACKUP"
    backup_file config "$CONFIG"
    backup_file password "$PASSWORD_FILE"
    backup_file wpa "$WPA"
    switch_unit="patrol-network-switch-${BACKUP##*/}"
    printf '%s\n' "$switch_unit" > "$BACKUP/switch-unit"
    printf 'MLML\n' > "$BACKUP/ssid"
    ln -s "$BACKUP" "$PENDING"
    ON_ERROR=1
    atomic_stdin "$UNIT_DIR/patrol-network-rollback.service" 644 <<EOF
[Unit]
Description=Restore previous Patrol Wi-Fi configuration
[Service]
Type=oneshot
ExecStart=$SELF rollback
Restart=on-failure
RestartSec=10
EOF
    atomic_stdin "$UNIT_DIR/patrol-network-rollback.timer" 644 <<EOF
[Unit]
Description=Roll back unconfirmed Patrol Wi-Fi migration
[Timer]
OnActiveSec=${timeout}s
AccuracySec=1s
Unit=patrol-network-rollback.service
[Install]
WantedBy=timers.target
EOF
    systemctl daemon-reload
    # Arm device-local recovery before replacing either live configuration file.
    systemctl enable --now patrol-network-rollback.timer
    atomic_stdin "$PASSWORD_FILE" <<< "$PASSWORD"
    atomic_stdin "$CONFIG" <<'EOF'
# Device-provisioned MLML DHCP configuration. Password is stored separately.
WIFI_IFACE=wlan0
WPA_CONF=/etc/wpa_supplicant.conf
WIFI_SSID=MLML
WIFI_PASSWORD_FILE=/etc/patrol/wifi-password
WIFI_PRIORITY=10
WIFI_SSID2=
STATIC_SSID=
STATIC_IP=
NETMASK=24
GATEWAY=
DNS1=
DNS2=
DISCOVERY_PORT=8083
EOF
    unset PASSWORD
    systemd-run --unit="$switch_unit" --on-active=3s --timer-property=AccuracySec=1s "$SELF" apply
    ON_ERROR=0
    printf 'MLML migration staged; switching in 3 seconds.\n'
    printf 'Reconnect at the DHCP address and run: patrol_network_migrate confirm\n'
    printf 'Without confirmation, previous configuration returns after %s seconds.\n' "$timeout"
}
confirm(){
    get_backup || fail 'No pending network migration'
    local status ssid state address
    status="$(wpa_cli -i wlan0 status 2>/dev/null)" || fail 'Unable to read association state'
    ssid="$(printf '%s\n' "$status" | sed -n 's/^ssid=//p' | head -1)"
    state="$(printf '%s\n' "$status" | sed -n 's/^wpa_state=//p' | head -1)"
    [ "$ssid" = MLML ] && [ "$state" = COMPLETED ] || fail 'MLML association is not complete; recovery stays armed'
    address="$(ip -4 addr show dev wlan0 scope global | awk '/inet / {split($2,a,"/"); print a[1]; exit}')"
    printf '%s\n' "$address" | awk -F. 'NF != 4 {exit 1} {for(i=1;i<=4;i++) if($i !~ /^[0-9]+$/ || $i>255) exit 1; if($1==0 || $1==127 || $1>=224 || ($1==169 && $2==254)) exit 1}' || fail 'No valid DHCP address; recovery stays armed'
    systemctl disable --now patrol-network-rollback.timer
    printf 'confirmed\n' > "$BACKUP/result"
    rm -f "$PENDING"
    printf 'Confirmed SSID=MLML DHCP address=%s; rollback cancelled.\n' "$address"
}

[ "$(id -u)" = 0 ] || fail 'Run on the device as root'
COMMAND="${1:-}"
[ "$#" = 0 ] || shift
TIMEOUT=300
SSID=MLML
case "$COMMAND" in
    stage)
        while [ "$#" -gt 0 ]; do
            case "$1" in
                --ssid) [ "$#" -ge 2 ] || fail 'Missing SSID'; SSID="$2"; shift 2;;
                --timeout) [ "$#" -ge 2 ] || fail 'Missing timeout'; TIMEOUT="$2"; shift 2;;
                *) fail 'Unknown stage option';;
            esac
        done
        [ "$SSID" = MLML ] || fail 'This migration profile targets MLML'
        case "$TIMEOUT" in *[!0-9]*|'') fail 'Invalid timeout';; esac
        [ "$TIMEOUT" -ge 120 ] && [ "$TIMEOUT" -le 900 ] || fail 'Timeout must be 120..900 seconds'
        PASSWORD=
        if [ -t 0 ]; then IFS= read -r -s -p 'Hotspot password: ' PASSWORD; printf '\n';
        else IFS= read -r PASSWORD || [ -n "$PASSWORD" ] || fail 'Password input is empty'; fi
        LC_ALL=C
        [ "${#PASSWORD}" -ge 8 ] && [ "${#PASSWORD}" -le 63 ] || fail 'Password must be 8..63 bytes'
        ;;
    apply|confirm|rollback|status) [ "$#" = 0 ] || fail 'Unexpected arguments';;
    *) fail 'Choose stage, apply, confirm, rollback or status';;
esac
acquire_lock
case "$COMMAND" in
    stage) stage "$TIMEOUT";;
    apply) get_backup || fail 'No pending migration'; systemctl restart patrol-network.service;;
    confirm) confirm;;
    rollback)
        if get_backup; then restore_profile; printf 'Previous Wi-Fi profile restored.\n'; systemctl restart patrol-network.service;
        else stop_timer; printf 'No pending migration; nothing to restore.\n'; fi;;
    status)
        if get_backup; then printf 'Pending SSID=MLML backup=%s\n' "$BACKUP";
        else printf 'No pending network migration\n'; fi;;
esac
