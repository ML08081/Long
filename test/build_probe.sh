#!/bin/bash
set -e
GCC=/home/molim/toolchains/loongson-gnu-toolchain-8.3-x86_64-loongarch64-linux-gnu-rc1.6/bin/loongarch64-linux-gnu-gcc
"$GCC" --version | head -1
"$GCC" -O2 -static -o /opPJ/PatrolSystem/test/st7789_probe /opPJ/PatrolSystem/test/st7789_probe.c
echo BUILD_OK
file /opPJ/PatrolSystem/test/st7789_probe
