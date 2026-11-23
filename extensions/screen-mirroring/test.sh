#!/bin/sh
set -eu
cd "$(dirname "$0")"
output=$(mktemp -d)
trap 'rm -rf "$output"' EXIT
clang -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined tests/test_memory.c memory.c -o "$output/memory"
"$output/memory"
clang -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined tests/test_session.c session.c -o "$output/session"
"$output/session"
for test in window; do
  clang -std=c11 -Wall -Wextra -Werror -fno-builtin -fsanitize=address,undefined -I../../app/extensions "tests/test_$test.c" -o "$output/$test"
  "$output/$test"
done
python3 tests/test_layout.py
