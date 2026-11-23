#!/bin/sh
# Snapshot USB support. An optional fixture root supports off-device tests.
set -u
root=${1:-}

read_node() {
    printf '\nFILE %s\n' "$1"
    if [ -e "$root$1" ]; then
        head -c 4096 "$root$1" 2>&1 | tr '\000' '\n'
        printf '\n'
    else
        printf 'absent\n'
    fi
}

list_dir() {
    printf '\nDIRECTORY %s\n' "$1"
    if [ -d "$root$1" ]; then ls -la "$root$1" 2>&1; else printf 'absent\n'; fi
}

printf 'Dashborne USB diagnostics v1\n'
if [ -z "$root" ]; then id; uname -a; fi
for path in /proc/filesystems /proc/mounts /proc/modules; do read_node "$path"; done
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
printf '\nInstalled helper availability\n'
for path in /res/bin/setusbconfig /bin/setusbconfig /bin/adbd; do
    if [ -e "$root$path" ]; then ls -l "$root$path"; else printf '%s absent\n' "$path"; fi
done
if [ -z "$root" ]; then
    printf '\nUSB-related kernel configuration\n'
    if [ -r /proc/config.gz ]; then
        gzip -dc /proc/config.gz | grep -E '^CONFIG_(USB|CONFIGFS|PHY_SUN|SUNXI_USB)' | head -n 160
    else printf '/proc/config.gz unavailable\n'; fi
    printf '\nRecent USB-related kernel messages\n'
    dmesg 2>&1 | grep -i -E 'usb|udc|gadget|configfs|musb|dwc|permission|denied' | tail -n 120
fi
printf '\nDiagnostic snapshot complete\n'
