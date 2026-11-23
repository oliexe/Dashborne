#!/bin/sh
set -eu
cd "$(dirname "$0")"
python3 tests/test_theme.py
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
cc -Wall -Wextra -Werror -fsanitize=address,undefined -g layout_patch.c tests/layout_bounds.c -lz -o "$tmp/bounds"
"$tmp/bounds"
