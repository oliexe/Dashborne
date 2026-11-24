#!/bin/sh
set -eu
cd "$(dirname "$0")"
output=$(mktemp -d)
trap 'rm -rf "$output"' EXIT
clang -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined \
    -I../../dashborne/extensions tests/test_extension.c -o "$output/test"
"$output/test"
