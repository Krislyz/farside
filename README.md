# Farside

Play your PS5 from another PC with the real DualSense in your hands. The PS5 sees the actual controller,
not an emulated one, so its adaptive triggers work.

Farside is two programs.

| | Runs on | What it does |
|---|---|---|
| **Farside Host** | the Windows PC at home, on the same network as the PS5 | Connects to the PS5 and shows its picture (it is built on [chiaki-ng](https://github.com/streetpea/chiaki-ng) 1.10.1). Receives the controller of a paired PC and hands it to the PS5 stream. |
| **Farside Client** | the PC you play from (Windows; Arch Linux from source) | Pairs with a Host and shares this PC's DualSense with it over USB/IP. Nothing else: no stream, no PS5 settings. |

The picture of the Host PC reaches you with the remote desktop or game streaming program you already use
(Moonlight with Sunshine, Parsec and the like). Farside carries the controller.

Farside is not affiliated with Sony Interactive Entertainment.

## Status

Early. What has been checked and what has not is listed in every release note. Expect rough edges and
please open an issue with what you see.

## Install

Take the Setup programs from the [latest release](https://github.com/Krislyz/farside/releases/latest).
They are not code-signed, so Windows SmartScreen asks before the first start.

### Farside Client (the PC you play from)

Run `FarsideClient-Setup.exe`. It needs administrator rights, because it

1. installs Farside Client,
2. fetches [usbipd-win](https://github.com/dorssel/usbipd-win) 5.3.0 from that project's release page,
   checks its SHA-256 and installs it (skipped when usbipd-win is already there),
3. shares the DualSense that is plugged in by USB cable.

While the controller is shared, Windows on that PC cannot use it itself. Remote Play, Setup has a
**Share controller** button for later (another controller, another USB port).

Removing Farside Client (Windows Settings, Apps) gives the controller back, removes usbipd-win if this
Setup installed it, and deletes the pairing.

### Farside Host (the PC at home)

Run `FarsideHost-Setup.exe`. It installs for the current user and asks for no administrator rights.

The Host also needs [usbip-win2](https://github.com/vadimgrn/usbip-win2), which you install yourself,
and a PS5 registered for Remote Play, done on the Play page the same way as in chiaki-ng.

## Pair

1. Host: Remote Play, Pair. It shows a code as text and as a QR picture, valid for 10 minutes.
2. Client: Pair. Paste the code or a screenshot of the QR picture, then click Pair.
3. Host: click Accept.

After that the Client finds the Host by itself from any network, through an encrypted
[iroh](https://github.com/n0-computer/dumbpipe) tunnel. On one network or a VPN you can also turn on
"Direct connections" on the Host's Pair page; both PCs then show a number to compare before you accept.

## Updates

Both programs ask this repository for the latest release when they start and every six hours after
that. It is one plain request to api.github.com: no account, no identifier, nothing about your PC in it. A newer Setup program is
fetched in the background and checked against the SHA-256 GitHub lists for it. It is installed only
when you click **Restart and update** at the bottom of the window, never in the middle of a game.

To switch the check off, set the registry value `HKCU\Software\Chiaki\Chiaki\farside\updates` to `false`.

## Build from source

Windows (MSYS2 MINGW64):

1. Set up the build the way chiaki-ng does: the steps of `.github/workflows/build-msys2.yml`
   (MSYS2 packages, libplacebo, FFmpeg, sdl2-compat, hidapi).
2. Build [zxing-cpp](https://github.com/zxing-cpp/zxing-cpp) 3.1.1 as a static library into `/mingw64`
   (`-DBUILD_SHARED_LIBS=OFF -DZXING_READERS=ON -DZXING_WRITERS=OLD -DZXING_C_API=OFF -DZXING_EXAMPLES=OFF -DZXING_DEPENDENCIES=LOCAL`).
3. `cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCHIAKI_ENABLE_CLI=OFF`
4. `cmake --build build --target chiaki farside-client` (`chiaki` is Farside Host)
5. `farside/package.sh <path to dumbpipe.exe>` collects both programs with their DLLs, runs their
   self-tests and builds the two Setup programs with [Inno Setup](https://jrsoftware.org/isinfo.php) 6.

Arch Linux (Client only): `farside/linux/build-arch.sh`. It has not been run on Linux yet.

Each program checks itself with `--farside-selftest` (exit code 0 means every check passed).

Where things are: `gui/src/farside*.cpp` and `gui/include/farside*.h` (pairing, sessions, updates),
`gui/src/qml/Farside*.qml` and `F*.qml` (pages), `farside/` (Setup scripts, packaging, Linux).

## Licence

GNU Affero General Public License v3 with the OpenSSL exception of chiaki-ng: see `COPYING` and
`LICENSES/`. The programs and libraries Farside comes with are listed in
[`farside/THIRD-PARTY.txt`](farside/THIRD-PARTY.txt). chiaki-ng's own README is
[`README-chiaki-ng.md`](README-chiaki-ng.md).

© 2026 Moonthia. chiaki-ng and Chiaki © their authors.
