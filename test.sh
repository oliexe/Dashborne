#!/bin/sh
set -eu
cd "$(dirname "$0")"
python3 -m unittest discover -s tools/tests
sh dashborne/patches/test.sh
sh dashborne/extensions/test.sh
sh dashborne/test.sh
sh dashborne/core/extra-menu/test.sh
sh dashborne/core/widget-runtime/test.sh
for extension in extensions/*; do
    if [ -f "$extension/test.sh" ]; then sh "$extension/test.sh"; fi
done
