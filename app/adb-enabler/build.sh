#!/bin/sh
set -eu
cd "$(dirname "$0")"
mkdir -p build card/lib card/ui
clang=${DASHBORNE_CC:-clang}
linker=${DASHBORNE_LD:-ld.lld}
for source in enabler.c tcp_adb.c ../dashboard/wifi_debug.c; do
    name=$(basename "$source" .c)
    "$clang" --target=armv7-linux-gnueabihf -march=armv7-a -mfpu=vfpv3-d16 -mfloat-abi=hard -marm -fPIC -ffreestanding -fno-stack-protector -fno-builtin -fno-strict-aliasing -fvisibility=hidden -Os -Wall -Wextra -Werror \
        '-DWIFI_CONFIG_PATH="/mnt/extsd/adb-enabler/wifi.conf"' \
        '-DWIFI_TIMEOUT_STATUS="Wi-Fi unavailable; retrying"' \
        -c "$source" -o "build/$name.o"
done
"$clang" --target=armv7-linux-gnueabihf -mfloat-abi=hard -fPIC -x c -c /dev/null -o build/dependency-stub.o
"$linker" -shared -soname /res/lib/libzkgui.so build/dependency-stub.o -o build/installed-app-link-stub.so
"$linker" -shared -z noexecstack -soname libadb-enabler.so --no-as-needed build/installed-app-link-stub.so build/enabler.o build/tcp_adb.o build/wifi_debug.o -o card/lib/libadb-enabler.so
python3 layout.py
