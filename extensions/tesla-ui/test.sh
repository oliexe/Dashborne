#!/bin/sh
set -eu
cd "$(dirname "$0")"
output=$(mktemp -d)
trap 'rm -rf "$output"' EXIT
clang -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined tests/test_model.c model.c -o "$output/model"
"$output/model"
clang -std=c11 -Wall -Wextra -Werror -fno-builtin -fsanitize=address,undefined -I../../dashborne/extensions tests/test_window.c model.c graph.c -o "$output/window"
"$output/window"
clang -std=c11 -Wall -Wextra -Werror -fno-builtin -fsanitize=address,undefined -I../../dashborne/extensions tests/test_source.c -lz -o "$output/source"
"$output/source"
clang -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined tests/test_graph.c graph.c -o "$output/graph"
"$output/graph"
python3 tests/test_layout.py
