#!/bin/sh
set -eu
cd "$(dirname "$0")"
mkdir -p build
clang=${DASHBORNE_CC:-clang}
linker=${DASHBORNE_LD:-ld.lld}
for source in launcher ../dashboard/wifi_debug ../adb-enabler/tcp_adb; do
    name=${source##*/}
    "$clang" --target=armv7-linux-gnueabihf -march=armv7-a -mfpu=vfpv3-d16 -mfloat-abi=hard -marm -fPIC -ffreestanding -fno-stack-protector -fno-builtin -fno-strict-aliasing -fvisibility=hidden -Os -Wall -Wextra -Werror -DWIFI_CONFIG_PATH='"/mnt/extsd/adb-enabler/wifi.conf"' -c "$source.c" -o "build/$name.o"
done
"$clang" --target=armv7-linux-gnueabihf -mfloat-abi=hard -fPIC -x c -c /dev/null -o build/dependency-stub.o
"$linker" -shared -soname /res/lib/libzkgui.so build/dependency-stub.o -o build/installed-app-link-stub.so
"$linker" -shared -z noexecstack -soname libofficial-preview.so --no-as-needed build/installed-app-link-stub.so build/launcher.o build/wifi_debug.o build/tcp_adb.o -o build/libofficial-preview.so
