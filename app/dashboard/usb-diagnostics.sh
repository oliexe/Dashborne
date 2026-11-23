#!/bin/sh
# Only shell builtins and the observed ls utility are required on the DASH.
# An optional fixture root supports off-device tests.
set -u
root=${1:-}

read_node() {
    echo
    echo "FILE $1"
    if [ -e "$root$1" ]; then
        count=0
        while IFS= read -r line || [ -n "$line" ]; do
            echo "$line"
            count=$((count + 1))
            [ "$count" -lt 120 ] || break
        done < "$root$1"
        echo
    else
        echo absent
    fi
}

list_dir() {
    echo
    echo "DIRECTORY $1"
    if [ -d "$root$1" ]; then ls -la "$root$1" 2>&1; else echo absent; fi
}

echo 'Dashborne USB diagnostics v2'
for path in /proc/version /proc/self/status /proc/filesystems /proc/mounts /proc/modules; do read_node "$path"; done
for path in /sys/class/udc /sys/class/usb_role /sys/class/typec \
    /sys/class/zkswe_usb /sys/class/android_usb /sys/kernel/config/usb_gadget \
    /sys/class/sunxi_usb /dev/usb-ffs/adb; do list_dir "$path"; done
for path in /sys/class/zkswe_usb/zkswe0/enable /sys/class/android_usb/android0/enable; do
    read_node "$path"
done

for node in "$root"/sys/class/udc/*; do
    [ -e "$node" ] || continue
    node=${node#"$root"}
    for field in state current_speed maximum_speed function is_otg; do read_node "$node/$field"; done
done
for node in "$root"/sys/class/usb_role/*/role \
    "$root"/sys/class/typec/*/data_role "$root"/sys/class/typec/*/power_role \
    "$root"/sys/kernel/config/usb_gadget/*/UDC; do
    [ -e "$node" ] || continue
    read_node "${node#"$root"}"
done
for node in "$root"/sys/bus/platform/devices/*usb* "$root"/sys/bus/platform/devices/*otg*; do
    [ -e "$node" ] || continue
    node=${node#"$root"}
    list_dir "$node"
    for field in of_node/status of_node/dr_mode otg_role usb_role; do read_node "$node/$field"; done
done
echo
echo 'Installed helper availability'
for path in /res/bin/setusbconfig /bin/setusbconfig /bin/adbd; do
    if [ -e "$root$path" ]; then ls -l "$root$path"; else echo "$path absent"; fi
done
echo
echo 'Diagnostic snapshot complete'
