#!/bin/bash
# Shares this PC's DualSense through usbip, so the Farside Host can take it.
#   sudo ./share-controller.sh         share it (plug it in with a USB cable first)
#   sudo ./share-controller.sh stop    give it back to this PC
# While it is shared, this PC cannot use the controller itself.
# NOT TESTED: written on Windows, never run on Linux.
set -euo pipefail

find_pad() {
	for d in /sys/bus/usb/devices/*; do
		[ -f "$d/idVendor" ] || continue
		[ "$(cat "$d/idVendor")" = "054c" ] || continue
		case "$(cat "$d/idProduct")" in
			0ce6|0df2) basename "$d"; return 0 ;;   # DualSense, DualSense Edge
		esac
	done
	return 1
}

busid="$(find_pad)" || { echo "No DualSense found. Plug it in with a USB cable."; exit 1; }

if [ "${1:-}" = "stop" ]; then
	usbip unbind -b "$busid" || true
	systemctl stop usbipd || true
	echo "Controller $busid is back with this PC."
	exit 0
fi

modprobe usbip-host
systemctl start usbipd
usbip bind -b "$busid" || true
echo "Controller $busid is shared. Farside's Remote Play page should now show it."
