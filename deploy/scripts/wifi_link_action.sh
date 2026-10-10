#!/bin/sh
# Called by wpa_cli when the station loses/re-establishes association.
set -eu
IFACE="${1:-}"
case "$IFACE" in *[!a-zA-Z0-9_.:-]*|'') exit 1;; esac
PIDFILE="/run/patrol-dhcp-$IFACE.pid"
[ -f "$PIDFILE" ] || exit 0
IFS= read -r DHCP_PID < "$PIDFILE" || exit 0
case "$DHCP_PID" in *[!0-9]*|'') exit 1;; esac
case "${2:-}" in
    CONNECTED) kill -USR1 "$DHCP_PID" 2>/dev/null || true;;
    DISCONNECTED) kill -USR2 "$DHCP_PID" 2>/dev/null || true;;
esac
