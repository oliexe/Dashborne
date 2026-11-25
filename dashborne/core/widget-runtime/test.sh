#!/bin/sh
set -eu
export UBSAN_OPTIONS=halt_on_error=1
cd "$(dirname "$0")"
output=$(mktemp -d)
trap 'rm -rf "$output"' EXIT
for test in catalog registry views runtime memory; do
    clang -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined -I../../extensions "tests/test_$test.c" -lz -o "$output/$test"
    "$output/$test"
done
python3 tests/test_storage.py
