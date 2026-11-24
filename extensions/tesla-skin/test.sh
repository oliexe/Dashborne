#!/bin/sh
set -eu
cd "$(dirname "$0")"
python3 -m unittest discover -s tests
output=$(mktemp -d)
trap 'rm -rf "$output"' EXIT
clang -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined \
    -I../../dashborne/extensions tests/test_runtime.c layout_patch.c -lz -o "$output/runtime"
UBSAN_OPTIONS=halt_on_error=1 "$output/runtime"
