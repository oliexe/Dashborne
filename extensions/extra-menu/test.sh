#!/bin/sh
set -eu
cd "$(dirname "$0")"
output=$(mktemp -d)
trap 'rm -rf "$output"' EXIT
clang -std=c11 -Wall -Wextra -Werror -fno-builtin -fsanitize=address,undefined -I../../app/extensions tests/test_window.c -o "$output/window"
"$output/window"
python3 tests/test_layout.py
