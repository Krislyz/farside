Farside Client for Arch Linux
=============================
Farside Client is for the PC you play from. It shares this PC's DualSense with a Farside Host
(a Windows PC) and does nothing else.

NOT TESTED. These scripts were written on Windows and have never run on Linux. The same source
compiles on Windows; the first build on your laptop is the first real test. If a step fails, the
last 30 lines it printed are what is needed to fix it.

Build
-----
Copy this folder to the laptop, then in a terminal in this folder:

    chmod +x build-arch.sh share-controller.sh
    ./build-arch.sh

It installs packages with pacman (it shows the list and asks), fetches Farside's source from
https://github.com/Krislyz/farside, zxing-cpp 3.1.1 and dumbpipe 0.40.2 from GitHub, and builds into
~/farside-build. To remove everything it built: delete ~/farside-build.

Use
---
1. Plug the DualSense in with a USB cable, then:    sudo ./share-controller.sh
2. Start:    ~/farside-build/farside/build/gui/FarsideClient
3. Pair tab: paste the code from the Host (or a screenshot of its QR code), click Pair, then click
   Accept on the Host.
   Or without the tunnel: turn "Direct connections" on at the Host's Pair page, then pick the Host
   under "On this network" or type its address, and compare the number both PCs show.
4. To take the controller back:    sudo ./share-controller.sh stop
