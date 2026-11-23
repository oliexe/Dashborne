#!/bin/sh
set -eu
cd "$(dirname "$0")"
mkdir -p build card/lib
clang=${DASHBORNE_CC:-clang}
linker=${DASHBORNE_LD:-ld.lld}
"$clang" --target=armv7-linux-gnueabihf -march=armv7-a -mfpu=neon -mfloat-abi=hard -marm -fPIC -ffreestanding -fno-stack-protector -fno-builtin -fvisibility=hidden -Os -Wall -Wextra -Werror -c sdprobe.c -o build/sdprobe.o
"$clang" --target=armv7-linux-gnueabihf -march=armv7-a -mfpu=neon -mfloat-abi=hard -marm -fPIC -x c -c /dev/null -o build/dependency-stub.o
# Build-time placeholder only. NEVER copy this stub onto the card/device.
# At runtime the absolute DT_NEEDED path loads the installed real application.
"$linker" -shared -soname /res/lib/libzkgui.so build/dependency-stub.o -o build/installed-app-link-stub.so
"$linker" -shared -z noexecstack -soname libsdprobe.so --no-as-needed build/installed-app-link-stub.so build/sdprobe.o -o card/lib/libsdprobe.so
