#!/bin/sh
set -eu
cd "$(dirname "$0")"
mkdir -p build
${CC:-cc} -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined tests/launcher_test.c -o build/launcher-test
build/launcher-test
