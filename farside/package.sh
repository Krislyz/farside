#!/usr/bin/env bash
# Collects Farside and Farside Remote with the DLLs they need, runs the self-test of each collected
# copy, and builds the two Setup programs (the files a GitHub release carries and the updater fetches).
# Run in the MSYS2 MINGW64 shell, from the source folder, after both programs are built:
#   cmake --build build --target chiaki farside-client
#   farside/package.sh <path to dumbpipe.exe> [<path to Inno Setup's ISCC.exe>]
# Result: build/farside/out/Farside-Setup.exe and FarsideRemote-Setup.exe, and a copy of each under the
# name it had until 0.1.5.2 (see the end of this file).
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
    [[ $edition == client ]] && exe=build/gui/FarsideRemote.exe
    scripts/deploy-windows-msys2.sh "$out/$edition" "$exe" "$(pwd)/build/third-party/cpp-steam-tools" /mingw64 gui/src/qml > "$out/deploy-$edition.log"
    # The tunnel between the PCs (github.com/n0-computer/dumbpipe).
    cp "$dumbpipe" "$out/$edition/dumbpipe.exe"
    # Qt's JPEG picture plugin needs this one; the collecting above only follows the program's own imports.
    cp /mingw64/bin/libjpeg-8.dll "$out/$edition/"
    cp COPYING farside/THIRD-PARTY.txt "$out/$edition/"
    "$out/$edition/$(basename "$exe")" --farside-selftest
    echo "$edition: self-test passed"
done
# The collecting script always adds SDL3, for the stream. Farside Remote has no stream.
rm -f "$out/client"/SDL3*.dll

for edition in host client; do
    MSYS2_ARG_CONV_EXCL='*' "$iscc" /Qp "/DVersion=$version" "/DSource=$(cygpath -w "$(pwd)/$out/$edition")" \
        "/O$(cygpath -w "$(pwd)/$out/out")" "$(cygpath -w "$(pwd)/farside/installer/$edition.iss")"
done
# Versions up to 0.1.5.2 (Farside Host, Farside Client) ask a release for these two names. A release
# that carries them is one those versions can update to; from there on the new names are asked for.
cp "$out/out/Farside-Setup.exe" "$out/out/FarsideHost-Setup.exe"
cp "$out/out/FarsideRemote-Setup.exe" "$out/out/FarsideClient-Setup.exe"
ls -l "$out/out"
