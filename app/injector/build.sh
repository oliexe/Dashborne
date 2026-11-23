#!/bin/sh
set -eu
cd "$(dirname "$0")"
mkdir -p build
clang=${DASHBORNE_CC:-clang}
linker=${DASHBORNE_LD:-ld.lld}
for source in patch injector ../official-ui/launcher ../dashboard/wifi_debug ../adb-enabler/tcp_adb; do
    name=${source##*/}
    "$clang" --target=armv7-linux-gnueabihf -march=armv7-a -mfpu=vfpv3-d16 -mfloat-abi=hard -marm -fPIC -ffreestanding -fno-stack-protector -fno-builtin -fno-strict-aliasing -fvisibility=hidden -Os -Wall -Wextra -Werror -DDASHBORNE_PATCH_INJECTOR -DWIFI_CONFIG_PATH='"/mnt/extsd/adb-enabler/wifi.conf"' -c "$source.c" -o "build/$name.o"
done
"$clang" --target=armv7-linux-gnueabihf -mfloat-abi=hard -fPIC -x c -c /dev/null -o build/stub.o
"$linker" -shared -soname /res/lib/libzkgui.so build/stub.o -o build/app-stub.so
"$linker" -shared -soname /lib/libz.so.1 build/stub.o -o build/zlib-stub.so
"$linker" -shared -z noexecstack -soname libdashborne-injector.so --no-as-needed build/app-stub.so build/zlib-stub.so build/launcher.o build/patch.o build/injector.o build/wifi_debug.o build/tcp_adb.o -o build/libdashborne-injector.so
python3 package.py
