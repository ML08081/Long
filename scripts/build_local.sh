#!/bin/bash
set -e
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(dirname "$SCRIPT_DIR")"
BUILD="$ROOT/build"
mkdir -p "$BUILD" && cd "$BUILD"
cmake -DCMAKE_BUILD_TYPE=Debug "$ROOT"
make -j"$(nproc)"
echo "本地构建完成: $BUILD/bin/patrol_system"
