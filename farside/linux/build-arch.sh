#!/bin/bash
# Builds Farside Client on Arch Linux, from source. Farside Client shares this PC's controller with a
# Farside Host (a Windows PC); that is all it does.
#
#   ./build-arch.sh
#
# What it does, in order:
#   1. installs the packages the build needs with pacman (asks for your password)
#   2. fetches Farside's source from GitHub
#   3. builds the QR code library zxing-cpp into a folder of its own (nothing goes into /usr)
#   4. builds Farside Client
#   5. fetches the tunnel program dumbpipe from its GitHub release and checks its SHA-256
# Everything lands in ~/farside-build (or $FARSIDE_BUILD_DIR). To remove it all: delete that folder.
# The packages from step 1 stay; pacman lists them above the question before it installs.
#
# NOT TESTED: this script and the Linux build were written on Windows and have never run on Linux.
# If a step fails, please open an issue with the last 30 lines of its output.
set -euo pipefail

work="${FARSIDE_BUILD_DIR:-$HOME/farside-build}"
src="$work/farside"
mkdir -p "$work"

echo "== 1/5 packages"
# The build files of the whole source tree are made, so the list is the one of chiaki-ng; only
# Farside Client is compiled.
sudo pacman -S --needed base-devel git cmake ninja python python-protobuf protobuf \
	qt6-base qt6-declarative qt6-svg qt6-wayland ffmpeg libplacebo sdl2-compat opus openssl hidapi json-c \
	miniupnpc curl speexdsp fftw libevent libevdev lcms2 vulkan-headers vulkan-icd-loader usbip

echo "== 2/5 Farside's source"
if [ ! -d "$src/.git" ]; then
	git clone --depth 1 --recurse-submodules --shallow-submodules https://github.com/Krislyz/farside.git "$src"
else
	git -C "$src" pull --ff-only
fi

echo "== 3/5 zxing-cpp"
if [ ! -f "$work/prefix/lib/cmake/ZXing/ZXingConfig.cmake" ] && [ ! -f "$work/prefix/lib64/cmake/ZXing/ZXingConfig.cmake" ]; then
	curl -fL -o "$work/zxing-cpp-3.1.1.tar.gz" https://github.com/zxing-cpp/zxing-cpp/releases/download/v3.1.1/zxing-cpp-3.1.1.tar.gz
	echo "c3c02c29c0b519de7bd4e25b376e606e87f0761befd1282815642a2246613d14  $work/zxing-cpp-3.1.1.tar.gz" | sha256sum -c -
	rm -rf "$work/zxing"
	mkdir -p "$work/zxing"
	tar -xzf "$work/zxing-cpp-3.1.1.tar.gz" -C "$work/zxing"
	cmake -S "$work/zxing/zxing-cpp-3.1.1" -B "$work/zxing/build" -G Ninja -DCMAKE_BUILD_TYPE=Release \
		-DCMAKE_INSTALL_PREFIX="$work/prefix" -DCMAKE_POSITION_INDEPENDENT_CODE=ON -DBUILD_SHARED_LIBS=OFF \
		-DZXING_READERS=ON -DZXING_WRITERS=OLD -DZXING_C_API=OFF -DZXING_EXAMPLES=OFF -DZXING_DEPENDENCIES=LOCAL
	cmake --build "$work/zxing/build"
	cmake --install "$work/zxing/build"
fi

echo "== 4/5 Farside Client"
# nanopb's generator leaves a file made for one protobuf version behind; upstream's build removes it too
rm -f "$src/third-party/nanopb/generator/proto/nanopb_pb2.py"
cmake -S "$src" -B "$src/build" -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="$work/prefix" \
	-DCHIAKI_ENABLE_CLI=OFF -DCHIAKI_ENABLE_TESTS=OFF
cmake --build "$src/build" --target farside-client
bin="$src/build/gui"

echo "== 5/5 dumbpipe 0.40.2"
if [ ! -x "$bin/dumbpipe" ]; then
	curl -fL -o "$work/dumbpipe.tar.gz" \
		https://github.com/n0-computer/dumbpipe/releases/download/v0.40.2/dumbpipe-v0.40.2-linux-x86_64.tar.gz
	echo "25823c4fb7d10430740a1b57111108c6950c2eb901874a7ebd894bda5894089d  $work/dumbpipe.tar.gz" | sha256sum -c -
	mkdir -p "$work/dumbpipe"
	tar -xzf "$work/dumbpipe.tar.gz" -C "$work/dumbpipe"
	install -m 755 "$(find "$work/dumbpipe" -type f -name dumbpipe | head -n 1)" "$bin/dumbpipe"
fi

echo "== self-test"
if "$bin/FarsideClient" --farside-selftest; then
	echo "self-test passed"
else
	echo "self-test FAILED at check $? (put this number in the issue)"
fi
echo
echo "Start Farside Client with:  $bin/FarsideClient"
echo "Share the controller first:  sudo $src/farside/linux/share-controller.sh"
