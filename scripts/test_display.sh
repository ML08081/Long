#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p build-display-test/previews
g++ -std=c++11 -Wall -Wextra -g -fsanitize=address,undefined -Iinclude \
  test/display_test.cpp src/modules/display/St7789Display.cpp src/modules/logger/Logger.cpp \
  -pthread -Wl,--wrap=open,--wrap=close,--wrap=write,--wrap=access,--wrap=usleep,--wrap=ioctl \
  -o build-display-test/display_test
build-display-test/display_test build-display-test/previews
