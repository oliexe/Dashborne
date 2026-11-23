#!/bin/sh
set -eu
cd "$(dirname "$0")/../.."
: "${DASHBORNE_LINK_LIBS:?Set to a private directory containing the device libc.so.6 and libdl.so.2}"
clang=${DASHBORNE_CC:-clang}
linker=${DASHBORNE_LD:-ld.lld}
mkdir -p build/hardware
for source in engine tests/hardware/main tests/hardware/overlay tests/hardware/fake_core; do
    name=${source##*/}
    "$clang" --target=armv7-linux-gnueabihf -march=armv7-a -mfpu=vfpv3-d16 -mfloat-abi=hard -marm -fPIC -ffreestanding -fno-stack-protector -fno-builtin -Wall -Wextra -Werror -I . -DDBX_ROOT='"/mnt/extsd/Dashborne/engine-smoke"' -c "$source.c" -o "build/hardware/$name.o"
done
"$clang" --target=armv7-linux-gnueabihf -mfloat-abi=hard -c tests/hardware/start.S -o build/hardware/start.o
"$linker" -shared -z noexecstack build/hardware/overlay.o -o build/hardware/overlay.so
"$linker" -shared -z noexecstack build/hardware/fake_core.o -o build/hardware/fake-core.so
"$linker" -e _start --dynamic-linker /lib/ld-linux-armhf.so.3 -z noexecstack build/hardware/start.o build/hardware/main.o build/hardware/engine.o "$DASHBORNE_LINK_LIBS/libc.so.6" "$DASHBORNE_LINK_LIBS/libdl.so.2" -o build/hardware/engine-smoke
