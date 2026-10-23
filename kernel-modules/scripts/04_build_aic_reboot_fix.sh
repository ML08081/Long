#!/bin/bash
# Build only the AIC8800 BSP module against the matching configured kernel.
set -euo pipefail
[ "$#" -eq 2 ] || { echo "Usage: CROSS_COMPILE=/path/loongarch64-linux-gnu- $0 KERNEL_SOURCE KERNEL_BUILD"; exit 2; }
HERE=$(cd "$(dirname "$0")" && pwd)
ROOT=$(dirname "$HERE")
SOURCE=$(realpath "$1")
KBUILD=$(realpath "$2")
CROSS_COMPILE=${CROSS_COMPILE:-loongarch64-linux-gnu-}
test -f "$KBUILD/Module.symvers"
grep -q '4.19.190+' "$KBUILD/include/generated/utsrelease.h"
mkdir -p "$ROOT/build"
WORK=$(mktemp -d "$ROOT/build/aic8800-reboot.XXXXXX")
REL=drivers/net/wireless/aic8800/aic8800_bsp
mkdir -p "$WORK/$(dirname "$REL")"
cp -a "$SOURCE/$REL" "$WORK/$REL"
patch -d "$WORK" -p1 < "$ROOT/patches/aic8800-bsp-detached-card.patch"
make -C "$KBUILD" M="$WORK/$REL" ARCH=loongarch CROSS_COMPILE="$CROSS_COMPILE" modules -j1
modinfo "$WORK/$REL/aic8800_bsp.ko"
sha256sum "$WORK/$REL/aic8800_bsp.ko"
printf 'Build retained at %s\n' "$WORK"
