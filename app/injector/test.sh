#!/bin/sh
set -eu
cd "$(dirname "$0")"
mkdir -p build
${CC:-cc} -std=c11 -Wall -Wextra -Werror -shared -fPIC patch.c -lz -o build/patch-test.so
python3 -m unittest discover -s tests -v
sh ../official-ui/test.sh
