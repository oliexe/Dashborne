#!/bin/sh
set -eu
cd "$(dirname "$0")"
mkdir -p build
${HOST_CC:-cc} -std=c11 -Wall -Wextra -Werror -fno-strict-aliasing -fsanitize=address,undefined -g tests/runtime_test.c -o build/runtime-test
for scenario in recovery-retry recovery-bounded core-failure rejected-extension force-cleanup-fail force-normal force-watchdog force-watchdog-fail autolaunch autolaunch-tap autolaunch-invalid autolaunch-return autolaunch-restart-failure config-invalid device browser browser-prepare-failure browser-restart-failure external-app external-empty external-return external-invalid external-load-failure external-symbol-failure external-self extension extension-failure extension-cleanup-failure config-on config-off normal list missing-list-api developer-auto developer-tap tapped toggles patch-failure save-failure shutdown-prepared wrong-runtime read-error missing-layout missing-symbol wrong-vtable create-failure allocation-failure; do
    build/runtime-test "$scenario"
done
${HOST_CC:-cc} -std=c11 -Wall -Wextra -Werror -fno-strict-aliasing -fsanitize=address,undefined -g tests/debug_test.c -o build/debug-test
${HOST_CC:-cc} -std=c11 -Wall -Wextra -Werror -Iextensions -fsanitize=address,undefined tests/force_test.c -o build/force-test
build/force-test
build/debug-test
${HOST_CC:-cc} -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined -g tests/lan_test.c -o build/lan-test
build/lan-test
python3 -m unittest discover -s tests -p 'test_*.py' -v
