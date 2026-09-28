#!/bin/sh
# Runs on the minimal LoongOS rootfs (no Python standard library required).
# Args: staged executable, merged board config, original config SHA256.
set -eu
test "$#" -eq 3
staged_bin=$1
staged_config=$2
expected_config_sha=$3
"$staged_bin" --version | grep -F '1.24.0'
actual_config_sha=$(sha256sum /etc/patrol/config.json | cut -d ' ' -f1)
test "$actual_config_sha" = "$expected_config_sha" || {
    echo 'Board configuration changed since download; merge again.' >&2
    exit 1
}
mkdir -p /root/patrol-display-backups
display_backup=$(mktemp -d /root/patrol-display-backups/v1.24.0-XXXXXXXX)
cp -p /usr/bin/patrol_system "$display_backup/patrol_system"
cp -p /etc/patrol/config.json "$display_backup/config.json"
rollback() {
    echo "Deployment failed; restoring $display_backup" >&2
    systemctl stop patrol || true
    cp -p "$display_backup/patrol_system" /usr/bin/patrol_system
    cp -p "$display_backup/config.json" /etc/patrol/config.json
    systemctl start patrol || true
}
trap rollback EXIT
systemctl stop patrol
cp "$staged_bin" /usr/bin/patrol_system.display-new
chmod 755 /usr/bin/patrol_system.display-new
cp -p /etc/patrol/config.json /etc/patrol/config.json.display-new
cp "$staged_config" /etc/patrol/config.json.display-new
mv /usr/bin/patrol_system.display-new /usr/bin/patrol_system
mv /etc/patrol/config.json.display-new /etc/patrol/config.json
systemctl start patrol
sleep 5
systemctl is-active --quiet patrol
trap - EXIT
echo "Backup: $display_backup"
/usr/bin/patrol_system --version
