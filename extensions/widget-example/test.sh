#!/bin/sh
set -eu
export UBSAN_OPTIONS=halt_on_error=1
cd "$(dirname "$0")"
output=$(mktemp -d)
trap 'rm -rf "$output"' EXIT
clang -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined -I../../dashborne/extensions tests/test_example.c -o "$output/example"
"$output/example"
