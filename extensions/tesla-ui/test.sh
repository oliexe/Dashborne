#!/bin/sh
set -eu
cd "$(dirname "$0")"
output=$(mktemp -d)
trap 'rm -rf "$output"' EXIT
clang -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined tests/test_model.c model.c -o "$output/model"
"$output/model"
clang -std=c11 -Wall -Wextra -Werror -fno-builtin -fsanitize=address,undefined -I../../dashborne/extensions tests/test_window.c model.c graph.c road.c sample.c -o "$output/window"
"$output/window"
clang -std=c11 -Wall -Wextra -Werror -fno-builtin -fsanitize=address,undefined -I../../dashborne/extensions tests/test_source.c -lz -o "$output/source"
"$output/source"
clang -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined tests/test_graph.c graph.c -o "$output/graph"
"$output/graph"
clang -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined tests/test_road.c model.c road.c -o "$output/road"
"$output/road"
clang -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined -I../../dashborne/extensions tests/test_road_canvas.c road.c -o "$output/road-canvas"
"$output/road-canvas"
python3 tests/test_layout.py
clang -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined tests/test_motion.c motion.c -o "$output/motion"
"$output/motion"
clang -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined -I../../dashborne/extensions tests/test_visual.c motion.c -o "$output/visual"
"$output/visual"
python3 tests/test_motion_assets.py
