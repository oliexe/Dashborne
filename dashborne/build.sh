#!/bin/sh
set -eu
cd "$(dirname "$0")"
mkdir -p build/card/Dashborne/lib
clang=${DASHBORNE_CC:-clang}
linker=${DASHBORNE_LD:-ld.lld}
for source in loader state patches/patch extensions/engine debug debug_force watchdog lan browser device config wifi_editor update; do
    name=${source##*/}
    "$clang" --target=armv7-linux-gnueabihf -march=armv7-a -mfpu=vfpv3-d16 -mfloat-abi=hard -marm -fPIC -ffreestanding -fno-stack-protector -fno-builtin -fno-strict-aliasing -fvisibility=hidden -Os -Wall -Wextra -Werror "-DDASHBORNE_VERSION=\"$(cat ../VERSION)\"" -Iextensions -c "$source.c" -o "build/$name.o"
done
core_objects=""
for source in core/extra-menu/menu core/extra-menu/window core/widget-runtime/catalog core/widget-runtime/runtime core/widget-runtime/registry core/widget-runtime/views core/widget-runtime/memory core/widget-runtime/storage; do
    name=$(printf '%s' "$source" | tr / _)
    "$clang" --target=armv7-linux-gnueabihf -march=armv7-a -mfpu=vfpv3-d16 -mfloat-abi=hard -marm -fPIC -ffreestanding -fno-stack-protector -fno-builtin -fno-strict-aliasing -fvisibility=hidden -Os -Wall -Wextra -Werror -Iextensions -c "$source.c" -o "build/$name.o"
    core_objects="$core_objects build/$name.o"
done
"$clang" --target=armv7-linux-gnueabihf -mfloat-abi=hard -fPIC -x c -c /dev/null -o build/stub.o
"$linker" -shared -soname /res/lib/libzkgui.so build/stub.o -o build/app-stub.so
"$linker" -shared -soname /lib/libz.so.1 build/stub.o -o build/zlib-stub.so
"$linker" -shared -z noexecstack -soname libdashborne.so --no-as-needed build/app-stub.so build/zlib-stub.so build/loader.o build/state.o build/patch.o build/engine.o build/debug.o build/debug_force.o build/watchdog.o build/lan.o build/browser.o build/device.o build/config.o build/wifi_editor.o build/update.o $core_objects -o build/card/Dashborne/lib/libdashborne.so
mkdir -p build/card/Dashborne/bin
for source in watchdog_main watchdog_linux; do
    "$clang" --target=armv7-linux-gnueabihf -march=armv7-a -mfloat-abi=hard -marm -ffreestanding -fno-stack-protector -fno-builtin -Os -Wall -Wextra -Werror -c "$source.c" -o "build/$source.o"
done
"$linker" -static -e _start -z noexecstack build/watchdog_main.o build/watchdog_linux.o -o build/card/Dashborne/bin/watchdog
python3 layout.py
python3 extensions/build.py
python3 package.py

"$clang" --target=armv7-linux-gnueabihf -march=armv7-a -mfloat-abi=hard -fPIC -ffreestanding -fno-stack-protector -Os -Wall -Wextra -Werror -c update_recovery.c -o build/update_recovery.o
"$linker" -shared -z noexecstack build/update_recovery.o -o build/card/Dashborne/lib/update-recovery.so
(cd updater && CGO_ENABLED=0 GOOS=linux GOARCH=arm GOARM=7 go build -trimpath -ldflags "-s -w -X main.buildDate=$(date -u +%Y-%m-%d)" -o ../build/card/Dashborne/bin/updater .)
mkdir -p build/card/Dashborne/licenses/updater
cp updater/GO*LICENSE updater/MPL-2.0 updater/NOTICE build/card/Dashborne/licenses/updater/
python3 update_manifest.py
