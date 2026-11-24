#!/bin/sh
set -eu
cd "$(dirname "$0")"
output=$(mktemp -d)
trap 'rm -rf "$output"' EXIT
clang -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined \
    tests/test_scene.c scene.c ../tesla-ui/motion.c -o "$output/scene"
"$output/scene"
clang -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined \
    -I../../dashborne/extensions tests/test_adapter.c scene.c ../tesla-ui/motion.c \
    ../tesla-ui/sample.c -o "$output/adapter"
"$output/adapter"
clang -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined \
    -I../../dashborne/extensions tests/test_panels.c panels.c -o "$output/panels"
"$output/panels"
python3 tests/test_assets.py
