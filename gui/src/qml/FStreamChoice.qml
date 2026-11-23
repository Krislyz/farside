// The stream's quality choice: what is stored, what the lists offer and how it reads as one line.
// The Play page changes it; the bar in the stream only shows it. Resolution, frame rate and bitrate
// are fixed when a session starts (PS5 Remote Play works that way), so they are chosen before it.
import QtQuick

import org.streetpea.chiaking

QtObject {
    readonly property var resolutionNames: ["640x360 (360p)", "960x540 (540p)", "1280x720 (720p)", "1920x1080 (1080p)"]
    readonly property var resolutionShort: ["360p", "540p", "720p", "1080p"]
    // What chiaki-ng itself uses when the bitrate is left on automatic, in Mbps per resolution.
    readonly property var autoBitrate: [2, 6, 10, 15]
    // ponytail: three fixed presets as [resolution, fps, kbps]. Tune these numbers after real use;
    // the single lists beside them allow any other combination.
    readonly property var presets: [[3, 60, 8000], [4, 60, 15000], [4, 60, 30000]]
    readonly property var presetNames: [qsTr("Smooth"), qsTr("Balanced"), qsTr("Quality"), qsTr("Custom")]
    // 0 = automatic: chiaki-ng picks the bitrate that belongs to the resolution.
    // 100 Mbps is the most chiaki-ng lets you ask the PS5 for.
    readonly property var rates: [0, 5000, 8000, 10000, 15000, 20000, 30000, 50000, 75000, 100000]

    readonly property int resolution: Chiaki.settings.resolutionLocalPS5
    readonly property int fps: Chiaki.settings.fpsLocalPS5
    readonly property int kbps: Chiaki.settings.bitrateLocalPS5
    // Place in presetNames; the last one, "Custom", when no preset matches.
    readonly property int preset: {
        for (let i = 0; i < presets.length; i++)
            if (resolution === presets[i][0] && fps === presets[i][1] && kbps === presets[i][2])
                return i;
        return presets.length;
    }
    readonly property real mbps: kbps > 0 ? kbps / 1000 : autoBitrate[resolution - 1]
    readonly property string bitrateText: kbps > 0 ? qsTr("%1 Mbps").arg(mbps) : qsTr("Auto (%1 Mbps)").arg(mbps)
    // What these settings cost in time, in ms: on average half the time between two pictures until the
    // PS5 takes the next one (500 / fps), plus sending one picture of the full bitrate over a wired
    // gigabit network (1 Mbit takes 1 ms there, and a picture is mbps / fps Mbit).
    // ponytail: worked out from the settings alone, nothing measured; the PS5's capture and encode time,
    // decoding and the game are not in it. Replace with measured numbers once Farside can measure them.
    readonly property real estimatedMs: 500 / fps + mbps / fps
    // How good the picture can look: from its size and the bits each pixel gets at the full bitrate.
    // ponytail: steps set by eye for h265; h264 needs more bits for the same look. Tune after real use.
    readonly property var resolutionPixels: [640 * 360, 960 * 540, 1280 * 720, 1920 * 1080]
    readonly property real bitsPerPixel: mbps * 1000000 / (resolutionPixels[resolution - 1] * fps)
    readonly property var qualityNames: [qsTr("Potato"), qsTr("Baked potato"), qsTr("Crunchy"), qsTr("Decent"), qsTr("Good"), qsTr("Great"), qsTr("GOD TIER")]
    // The small pictures are potatoes whatever the bitrate; GOD TIER takes 1080p.
    readonly property int qualityRank: resolution <= 2 ? resolution - 1
                                     : bitsPerPixel < 0.06 ? 2 : bitsPerPixel < 0.11 ? 3 : bitsPerPixel < 0.2 ? 4
                                     : bitsPerPixel < 0.4 || resolution < 4 ? 5 : 6
    readonly property string qualityName: qualityNames[qualityRank]
    // The bitrate is the most the PS5 may send, not what it sends: it is named "max" wherever it shows.
    readonly property string summary: presetNames[preset] + " · " + resolutionShort[resolution - 1] + " · " + fps + " fps · "
                                      + (kbps > 0 ? qsTr("max %1 Mbps").arg(mbps) : qsTr("max %1 Mbps (auto)").arg(mbps))

    // The local and the remote profile are kept the same: a Host is always on the PS5's own network.
    function store(resolution, fps, kbps) {
        Chiaki.settings.resolutionLocalPS5 = resolution; Chiaki.settings.resolutionRemotePS5 = resolution;
        Chiaki.settings.fpsLocalPS5 = fps; Chiaki.settings.fpsRemotePS5 = fps;
        Chiaki.settings.bitrateLocalPS5 = kbps; Chiaki.settings.bitrateRemotePS5 = kbps;
    }
}
