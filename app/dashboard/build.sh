#!/bin/sh
set -eu
cd "$(dirname "$0")"
mkdir -p build card/lib card/ui card/Dashborne
clang=${DASHBORNE_CC:-clang}
linker=${DASHBORNE_LD:-ld.lld}
for source in dashboard usb_debug usb_configfs; do
    "$clang" --target=armv7-linux-gnueabihf -march=armv7-a -mfpu=vfpv3-d16 -mfloat-abi=hard -marm -fPIC -ffreestanding -fno-stack-protector -fno-builtin -fno-strict-aliasing -fvisibility=hidden -Os -Wall -Wextra -Werror -c "$source.c" -o "build/$source.o"
done
"$clang" --target=armv7-linux-gnueabihf -mfloat-abi=hard -fPIC -x c -c /dev/null -o build/dependency-stub.o
"$linker" -shared -soname /res/lib/libzkgui.so build/dependency-stub.o -o build/installed-app-link-stub.so
"$linker" -shared -z noexecstack -soname libdashborne.so --no-as-needed build/installed-app-link-stub.so build/dashboard.o build/usb_debug.o build/usb_configfs.o -o card/lib/libdashborne.so
python3 layout.py
cp usb-diagnostics.sh card/Dashborne/usb-diagnostics.sh
