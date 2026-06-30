#!/bin/bash
set -e
SCRIPT_DIR="$(cd "\$(dirname \"\")" && pwd)"
ROOT="$(dirname "$SCRIPT_DIR")"
BUILD="$ROOT/build"
mkdir -p "$BUILD" && cd "$BUILD"
cmake -DCMAKE_BUILD_TYPE=Debug "$ROOT"
make -j"$(nproc)"
echo done