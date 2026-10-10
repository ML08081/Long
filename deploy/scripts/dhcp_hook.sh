#!/bin/sh
# BusyBox udhcpc hook. Only existing shell/awk/ip tools are required.
set -eu
set -f
IFACE="${interface:-}"
case "$IFACE" in *[!a-zA-Z0-9_.:-]*|'') exit 1;; esac
valid_ip(){
    printf '%s\n' "$1" | awk -F. 'NF != 4 {exit 1} {for(i=1;i<=4;i++) if($i !~ /^[0-9]+$/ || $i>255) exit 1; if($1==0 || $1==127 || $1>=224 || ($1==169 && $2==254)) exit 1}'
}
case "${1:-}" in
    bound|renew)
        valid_ip "${ip:-}" || exit 1
        PREFIX="$(printf '%s\n' "${subnet:-255.255.255.0}" | awk -F. '
            BEGIN {bits[255]=8;bits[254]=7;bits[252]=6;bits[248]=5;bits[240]=4;bits[224]=3;bits[192]=2;bits[128]=1;bits[0]=0}
            NF!=4 {exit 1}
            {for(i=1;i<=4;i++){if($i !~ /^[0-9]+$/ || !($i in bits) || (partial && $i!=0)) exit 1; total+=bits[$i]; if($i!=255) partial=1} print total}')" || exit 1
        GATEWAY=""
        for value in ${router:-}; do valid_ip "$value" || exit 1; [ -n "$GATEWAY" ] || GATEWAY="$value"; done
        for value in ${dns:-}; do valid_ip "$value" || exit 1; done
        LEASE_ADDRESS="$ip/$PREFIX"
        CURRENT_ADDRESSES="$(/sbin/ip -4 addr show dev "$IFACE" scope global | awk '/inet / {print $2}')"
        HAS_LEASE=0
        for address in $CURRENT_ADDRESSES; do
            [ "$address" != "$LEASE_ADDRESS" ] || HAS_LEASE=1
        done
        if [ "$HAS_LEASE" = 0 ]; then
            # Install the new lease before removing old hotspot addresses.
            /sbin/ip addr replace "$LEASE_ADDRESS" dev "$IFACE"
            CURRENT_ADDRESSES="$(/sbin/ip -4 addr show dev "$IFACE" scope global | awk '/inet / {print $2}')"
            for address in $CURRENT_ADDRESSES; do
                [ "$address" != "$LEASE_ADDRESS" ] || HAS_LEASE=1
            done
            [ "$HAS_LEASE" = 1 ] || exit 1
        fi
        # An unchanged renewal must never remove its IP or flush the interface.
        for address in $CURRENT_ADDRESSES; do
            [ "$address" = "$LEASE_ADDRESS" ] || /sbin/ip addr del "$address" dev "$IFACE"
        done
        if [ -n "$GATEWAY" ]; then
            /sbin/ip route replace default via "$GATEWAY" dev "$IFACE"
        else
            /sbin/ip -4 route flush default dev "$IFACE"
        fi
        if [ -n "${dns:-}" ]; then
            # Keep the existing resolver symlink and use the phone's DNS values.
            { for value in $dns; do printf 'nameserver %s\n' "$value"; done; } > /etc/resolv.conf
        fi
        ;;
    deconfig)
        /sbin/ip -4 addr flush dev "$IFACE" scope global
        /sbin/ip -4 route flush default dev "$IFACE"
        ;;
esac
