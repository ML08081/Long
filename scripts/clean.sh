#!/bin/bash
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(dirname "$SCRIPT_DIR")"
echo "清理 build/ ..."
rm -rf "$ROOT/build" "$ROOT/build-loong"
echo "完成。"
