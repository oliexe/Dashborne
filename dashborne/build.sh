#!/bin/sh
set -eu
cd "$(dirname "$0")"
mkdir -p build/card/Dashborne/lib
clang=${DASHBORNE_CC:-clang}
linker=${DASHBORNE_LD:-ld.lld}
for source in loader state patches/patch extensions/engine debug lan browser device config; do
    name=${source##*/}
    "$clang" --target=armv7-linux-gnueabihf -march=armv7-a -mfpu=vfpv3-d16 -mfloat-abi=hard -marm -fPIC -ffreestanding -fno-stack-protector -fno-builtin -fno-strict-aliasing -fvisibility=hidden -Os -Wall -Wextra -Werror "-DDASHBORNE_VERSION=\"$(cat ../VERSION)\"" -c "$source.c" -o "build/$name.o"
done
"$clang" --target=armv7-linux-gnueabihf -mfloat-abi=hard -fPIC -x c -c /dev/null -o build/stub.o
"$linker" -shared -soname /res/lib/libzkgui.so build/stub.o -o build/app-stub.so
"$linker" -shared -soname /lib/libz.so.1 build/stub.o -o build/zlib-stub.so
"$linker" -shared -z noexecstack -soname libdashborne.so --no-as-needed build/app-stub.so build/zlib-stub.so build/loader.o build/state.o build/patch.o build/engine.o build/debug.o build/lan.o build/browser.o build/device.o build/config.o -o build/card/Dashborne/lib/libdashborne.so
python3 layout.py
python3 extensions/build.py
python3 package.py
