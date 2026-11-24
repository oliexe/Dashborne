#!/bin/sh
set -eu
cd "$(dirname "$0")"
output=$(mktemp -d)
trap 'rm -rf "$output"' EXIT
clang -std=c11 -D_POSIX_C_SOURCE=200809L -Os -Wall -Wextra -Werror -fno-builtin \
    -I../../dashborne/extensions tests/benchmark_window.c model.c graph.c -o "$output/benchmark"
"$output/benchmark"
