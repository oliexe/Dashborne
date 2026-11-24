#!/bin/sh
set -eu
cd "$(dirname "$0")"
python3 tests/test_panel.py
output=$(mktemp -d)
trap 'rm -rf "$output"' EXIT
clang -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined tests/test_display.c -o "$output/display"
"$output/display"
clang -std=c11 -Wall -Wextra -Werror -fno-builtin -fsanitize=address,undefined tests/test_settings.c ../../dashborne/patches/patch.c -I ../../dashborne/extensions -lz -o "$output/settings"
"$output/settings"
