#!/usr/bin/env bash
# Collects Farside Host and Farside Client with the DLLs they need, runs the self-test of each collected
# copy, and builds the two Setup programs (the files a GitHub release carries and the updater fetches).
# Run in the MSYS2 MINGW64 shell, from the source folder, after both programs are built:
#   cmake --build build --target chiaki farside-client
#   farside/package.sh <path to dumbpipe.exe> [<path to Inno Setup's ISCC.exe>]
# Result: build/farside/out/FarsideHost-Setup.exe and FarsideClient-Setup.exe.
set -euo pipefail

dumbpipe="$1"
# Folder 28 is this user's local application data, where winget puts Inno Setup.
iscc="${2:-$(cygpath -u -F 28)/Programs/Inno Setup 6/ISCC.exe}"
version="$(sed -n 's/^FARSIDE_VERSION:STRING=//p' build/CMakeCache.txt)"
out=build/farside
rm -rf "$out"
mkdir -p "$out/out"

for edition in host client; do
    exe=build/gui/Farside.exe
    [[ $edition == client ]] && exe=build/gui/FarsideClient.exe
    scripts/deploy-windows-msys2.sh "$out/$edition" "$exe" "$(pwd)/build/third-party/cpp-steam-tools" /mingw64 gui/src/qml > "$out/deploy-$edition.log"
    # The tunnel between the PCs (github.com/n0-computer/dumbpipe).
    cp "$dumbpipe" "$out/$edition/dumbpipe.exe"
    # Qt's JPEG picture plugin needs this one; the collecting above only follows the program's own imports.
    cp /mingw64/bin/libjpeg-8.dll "$out/$edition/"
    cp COPYING farside/THIRD-PARTY.txt "$out/$edition/"
    "$out/$edition/$(basename "$exe")" --farside-selftest
    echo "$edition: self-test passed"
done
# The collecting script always adds SDL3, for the stream. The Client has no stream.
rm -f "$out/client"/SDL3*.dll

for edition in host client; do
    MSYS2_ARG_CONV_EXCL='*' "$iscc" /Qp "/DVersion=$version" "/DSource=$(cygpath -w "$(pwd)/$out/$edition")" \
        "/O$(cygpath -w "$(pwd)/$out/out")" "$(cygpath -w "$(pwd)/farside/installer/$edition.iss")"
done
ls -l "$out/out"
