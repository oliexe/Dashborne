#!/bin/sh
set -eu
cd "$(dirname "$0")"
output=$(mktemp -d)
trap 'rm -rf "$output"' EXIT
cc=${HOST_CC:-clang}
flags="-std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined -I../../dashborne/extensions"
$cc $flags tests/test_model.c model.c -o "$output/model"
"$output/model"
$cc $flags tests/test_hardware.c -o "$output/hardware"
"$output/hardware"
$cc $flags "-DCONFIG_CARD=\"$output\"" tests/test_settings.c settings.c model.c -o "$output/settings"
"$output/settings"
$cc $flags tests/test_controller.c model.c -o "$output/controller"
"$output/controller"
$cc $flags tests/test_window.c model.c -o "$output/window"
"$output/window"
python3 tests/test_panel.py
