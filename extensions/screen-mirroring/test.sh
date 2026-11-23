#!/bin/sh
set -eu
cd "$(dirname "$0")"
output=$(mktemp -d)
trap 'rm -rf "$output"' EXIT
clang -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined tests/test_memory.c memory.c -o "$output/memory"
"$output/memory"
clang -std=c11 -Wall -Wextra -Werror -fno-builtin -fsanitize=address,undefined -I../../app/extensions tests/test_awake.c -o "$output/awake"
"$output/awake"
