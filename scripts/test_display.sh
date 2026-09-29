#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p build-display-test/previews
python3 -c 'import json; d=json.load(open("config/config.json"))["display"]; assert (d["gpio_dc"], d["gpio_rst"], d["gpio_bl"]) == (73,72,-1), "99PAI IO7=CAN2_TX/GPIO73; IO9=CAN2_RX/GPIO72"; assert d["spi_mode"] == 0; assert (d["controller"],d["width"],d["height"]) == ("ST7789",240,320); print("PASS: verified ST7789 two-page board profile")'
g++ -std=c++11 -Wall -Wextra -g -fsanitize=address,undefined -Iinclude \
  test/display_test.cpp src/modules/display/St7789Display.cpp src/modules/logger/Logger.cpp \
  -pthread -Wl,--wrap=open,--wrap=close,--wrap=write,--wrap=access,--wrap=usleep,--wrap=ioctl \
  -o build-display-test/display_test
build-display-test/display_test build-display-test/previews
