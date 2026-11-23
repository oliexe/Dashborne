#!/bin/sh
set -eu
cd "$(dirname "$0")"
mkdir -p build
${HOST_CC:-cc} -std=c11 -Wall -Wextra -Werror -fno-strict-aliasing -fsanitize=address,undefined -g tests/runtime_test.c -o build/runtime-test
for scenario in normal shutdown-prepared wrong-runtime read-error missing-layout missing-symbol wrong-vtable create-failure allocation-failure; do
    ./build/runtime-test "$scenario"
done
${HOST_CC:-cc} -std=c11 -Wall -Wextra -Werror -fno-strict-aliasing -fsanitize=address,undefined -g tests/usb_debug_test.c -o build/usb-debug-test
for scenario in normal shutdown missing-api missing-usb external-adb unknown-service unset-service busy-usb start-failure stop-failure write-denied diagnostics diagnostic-failure configfs tcp existing-tcp; do
    ./build/usb-debug-test "$scenario"
done
${HOST_CC:-cc} -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined -g tests/usb_configfs_test.c -o build/usb-configfs-test
for scenario in normal config-mount-fail ffs-mount-fail busy-controller old-gadget attribute-fail mkdir-fail endpoint-timeout bind-timeout stop-fail; do
    ./build/usb-configfs-test "$scenario"
done
${HOST_CC:-cc} -std=c11 -Wall -Wextra -Werror -fno-strict-aliasing -fsanitize=address,undefined -g tests/wifi_debug_test.c -o build/wifi-debug-test
for scenario in normal missing-api missing-config invalid-config read-failure timeout; do
    ./build/wifi-debug-test "$scenario"
done
${HOST_CC:-cc} -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined -g tests/demo_test.c -o build/demo-test
./build/demo-test
python3 -m unittest discover -s tests -p 'test_*.py'
