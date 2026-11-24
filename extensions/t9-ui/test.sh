#!/bin/sh
set -eu
cd "$(dirname "$0")"
python3 -m unittest discover -s tests
test_bin=$(mktemp -t dashborne-t9-test)
trap 'rm -f "$test_bin"' EXIT
cc -Wall -Wextra -Werror -I ../../dashborne/extensions tests/lifecycle.c -o "$test_bin"
"$test_bin"
