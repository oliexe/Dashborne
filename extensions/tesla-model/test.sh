#!/bin/sh
set -eu
export UBSAN_OPTIONS=halt_on_error=1
cd "$(dirname "$0")"
output=$(mktemp -d)
trap 'rm -rf "$output"' EXIT
for module in scene pack data; do
    source="$module.c"
    if [ "$module" = data ]; then source=""; fi
    clang -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined \
        -I../../dashborne/extensions "tests/test_$module.c" $source -lz -o "$output/$module"
    "$output/$module"
done
clang -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined \
    -I../../dashborne/extensions tests/test_extension.c scene.c -o "$output/extension"
"$output/extension"
python3 tests/test_import.py
