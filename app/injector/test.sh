#!/bin/sh
set -eu
cd "$(dirname "$0")"
mkdir -p build
${CC:-cc} -std=c11 -Wall -Wextra -Werror -shared -fPIC patch.c -lz -o build/patch-test.so
${CC:-cc} -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined -g patch.c tests/bounds_test.c -lz -o build/bounds-test
build/bounds-test
python3 -m unittest discover -s tests -v
sh ../official-ui/test.sh
