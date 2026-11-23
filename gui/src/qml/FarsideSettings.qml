// Settings page. A change waits until "Save changes" in the bar at the bottom; "Reset" or leaving the
// page drops it.
pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts
import QtQuick.Templates as T

import org.streetpea.chiaking

Item {
    id: page

    // The changes that wait: name of the setting -> new value.
    property var pending: ({})
    readonly property bool dirty: Object.keys(pending).length > 0
    // Every setting of this page: how to read what is stored and how to store.
    readonly property var fields: ({
        updateChannel: { get: () => Farside.updateChannel, set: (v) => Farside.updateChannel = v },
        textSize: { get: () => shell.options.textSize, set: (v) => shell.options.textSize = v },
        logEnabled: { get: () => Farside.logEnabled, set: (v) => Farside.logEnabled = v },
        debugMs: { get: () => shell.options.debugMs, set: (v) => shell.options.debugMs = v },
        // The same order as ChiakiWindow.VideoPreset; the second line applies it to a running stream too.
        videoPreset: { get: () => Chiaki.settings.videoPreset, set: (v) => { Chiaki.settings.videoPreset = v; Chiaki.window.videoPreset = v; } },
        placeboUpscaler: { get: () => Chiaki.settings.placeboUpscaler, set: (v) => Chiaki.settings.placeboUpscaler = v },
        placeboFrameMixer: { get: () => Chiaki.settings.placeboFrameMixer, set: (v) => Chiaki.settings.placeboFrameMixer = v },
        placeboDebandEnabled: { get: () => Chiaki.settings.placeboDebandEnabled, set: (v) => Chiaki.settings.placeboDebandEnabled = v },
        hudResponse: { get: () => shell.options.hudResponse, set: (v) => shell.options.hudResponse = v },
        hudStyle: { get: () => shell.options.hudStyle, set: (v) => shell.options.hudStyle = v },
        hudPlace: { get: () => shell.options.hudPlace, set: (v) => shell.options.hudPlace = v },
        hudOpacity: { get: () => shell.options.hudOpacity, set: (v) => shell.options.hudOpacity = v },
        showStreamStats: { get: () => Chiaki.settings.showStreamStats, set: (v) => Chiaki.settings.showStreamStats = v },
        logVerbose: { get: () => Chiaki.settings.logVerbose, set: (v) => Chiaki.settings.logVerbose = v }
    })

    // What the page shows for a setting: the waiting change, or else what is stored.
    function value(key) {
        return key in pending ? pending[key] : fields[key].get();
    }

    function change(key, v) {
        let next = Object.assign({}, pending);
        if (v === fields[key].get())
            delete next[key];
        else
            next[key] = v;
        pending = next;
    }

    function save() {
        const done = pending;
        pending = ({});
        for (const key in done)
            fields[key].set(done[key]);
    }

    function reset() {
        pending = ({});
    }

    onVisibleChanged: if (!visible) reset()

    component Card: Rectangle {
        default property alias body: column.data
        property alias title: heading.text

        Layout.fillWidth: true
        Layout.fillHeight: true
        implicitHeight: column.implicitHeight + 40
        radius: 14
        color: shell.card
        border.color: shell.line

        ColumnLayout {
            id: column
            x: 22
            y: 20
            width: parent.width - 44
            spacing: 14

            FText { id: heading; font.pixelSize: 15; font.weight: Font.DemiBold }
        }
    }

    // A tick box and a picker that show page.value(key) and hand a change to page.change(key, ...).
    // A click breaks the binding of the control, so it is set again each time.
    component Tick: FCheck {
        required property string key

        checked: page.value(key)
        onToggled: {
            page.change(key, checked);
            checked = Qt.binding(() => page.value(key));
        }
    }

    component Pick: FCombo {
        id: pick
        required property string key
        // What each entry stands for; without it, the entry's place in the list.
        property var values: null
        function place() {
            return values ? Math.max(0, values.indexOf(page.value(key))) : page.value(key);
        }

        implicitWidth: 380
        currentIndex: pick.place()
        onActivated: (index) => {
            page.change(key, values ? values[index] : index);
            currentIndex = Qt.binding(() => pick.place());
        }
    }

    Flickable {
        id: scroller
        anchors.fill: parent
        anchors.bottomMargin: page.dirty ? saveBar.height + 24 : 0
        contentHeight: content.implicitHeight + 56
        clip: true
        boundsBehavior: Flickable.StopAtBounds

        T.ScrollBar.vertical: T.ScrollBar {
            id: bar
            implicitWidth: 12
            padding: 3
            policy: scroller.contentHeight > scroller.height ? T.ScrollBar.AlwaysOn : T.ScrollBar.AlwaysOff
            // The bare template shows its bar whatever the policy says.
            visible: scroller.contentHeight > scroller.height

            contentItem: Rectangle {
                implicitWidth: 6
                radius: 3
                color: bar.pressed ? "#d4267e" : bar.hovered ? "#5d5d70" : "#3a3a50"
            }
        }

        // Two cards side by side on a wide window, so the page needs no scrolling there.
        GridLayout {
            id: content
            readonly property bool wide: scroller.width >= 1100

            x: 28
            y: 28
            width: Math.min(scroller.width - 56, wide ? 1560 : 980)
            columns: wide ? 2 : 1
            uniformCellWidths: true
            columnSpacing: 18
            rowSpacing: 18

            // The stream's own settings live in a dialog of their own (chiaki-ng's): this is the way in.
            Card {
                title: qsTr("Stream")

                FText {
                    Layout.fillWidth: true
                    text: qsTr("Video and sound of the stream, controllers, keys, consoles and your PSN sign-in.")
                    color: shell.muted
                    wrapMode: Text.WordWrap
                }
                FButton {
                    primary: true
                    text: qsTr("Open stream settings")
                    onClicked: root.showSettingsDialog()
                }
            }

            Card {
                title: qsTr("Updates")

                RowLayout {
                    spacing: 16

                    FText { text: qsTr("Channel") }
                    Pick {
                        key: "updateChannel"
                        values: ["stable", "alpha"]
                        model: [qsTr("Stable"), qsTr("Alpha: early versions, may break")]
                    }
                    FButton { text: qsTr("Check for updates"); onClicked: Farside.checkForUpdate(true) }
                }
                FText {
                    Layout.fillWidth: true
                    text: qsTr("Farside looks for a newer version when it starts and every six hours. A new version is fetched by itself and installed when you click Restart and update at the bottom of the window. This is version %1.").arg(Farside.version)
                    color: shell.muted
                    wrapMode: Text.WordWrap
                }
            }

            Card {
                title: qsTr("Appearance")

                RowLayout {
                    spacing: 16

                    FText { text: qsTr("Text size") }
                    Pick {
                        key: "textSize"
                        values: [0.8, 0.9, 1.05, 1.2]
                        model: [qsTr("Small"), qsTr("Normal"), qsTr("Large"), qsTr("Largest")]
                    }
                }
                FText {
                    Layout.fillWidth: true
                    text: qsTr("Sizes everything in this window. The display scale set in Windows is followed on top of it; a small window shrinks the page further.")
                    color: shell.muted
                    wrapMode: Text.WordWrap
                }
            }

            Card {
                title: qsTr("Diagnostics")

                Tick {
                    key: "logEnabled"
                    text: qsTr("Log: write controller link events to a file")
                }
                Tick {
                    key: "debugMs"
                    text: qsTr("Debug ms: show the delay to each connected PC in milliseconds")
                }
                FButton { text: qsTr("Open log folder"); onClicked: Farside.openFolder(Farside.logFolder()) }
            }

            Card {
                title: qsTr("Picture")
                visible: !shell.isClient

                GridLayout {
                    columns: 2
                    columnSpacing: 16
                    rowSpacing: 12

                    FText { text: qsTr("Upscaling") }
                    Pick {
                        key: "videoPreset"
                        model: [qsTr("Fast (least GPU work)"), qsTr("Default"), qsTr("High quality"), qsTr("High quality + spatial upscaling"), qsTr("High quality + advanced spatial upscaling"), qsTr("Custom")]
                    }

                    FText { text: qsTr("Upscaler"); visible: custom.visible }
                    Pick {
                        id: custom
                        key: "placeboUpscaler"
                        visible: page.value("videoPreset") === 5
                        model: [qsTr("From the renderer options"), qsTr("Nearest"), qsTr("Bilinear"), qsTr("Oversample"), qsTr("Bicubic"), qsTr("Gaussian"), qsTr("Catmull Rom"), qsTr("Lanczos"), qsTr("EwaLanczos"), qsTr("EwaLanczosSharp"), qsTr("EwaLanczosSharpest"), qsTr("FSR"), qsTr("FSRCNNX x2 8-0-4-1"), qsTr("FSRCNNX x2 16-0-4-1")]
                    }

                    FText { text: qsTr("Frame mixing"); visible: custom.visible }
                    Pick {
                        key: "placeboFrameMixer"
                        visible: custom.visible
                        model: [qsTr("Off"), qsTr("Oversample"), qsTr("Hermite"), qsTr("Linear"), qsTr("Cubic")]
                    }
                }

                Tick {
                    key: "placeboDebandEnabled"
                    text: qsTr("Debanding: smooth out colour steps in skies and shadows")
                    visible: custom.visible
                }
                FButton {
                    text: qsTr("All renderer options")
                    visible: custom.visible
                    onClicked: root.showPlaceboSettingsDialog()
                }
                FText {
                    Layout.fillWidth: true
                    text: qsTr("Upscaling is done by this PC's graphics card after the picture arrives. It adds no network delay; the higher presets cost a little GPU time per frame.")
                    color: shell.muted
                    wrapMode: Text.WordWrap
                }
            }

            Card {
                title: qsTr("Stream HUD")
                visible: !shell.isClient

                Tick {
                    key: "hudResponse"
                    text: qsTr("Estimated ms and quality name in the bar at the top of the stream")
                }
                Tick {
                    key: "showStreamStats"
                    text: qsTr("Stream statistics: pictures per second, delays in ms, bitrate, this PC's load")
                }
                GridLayout {
                    columns: 2
                    columnSpacing: 16
                    rowSpacing: 12

                    FText { text: qsTr("Shown as") }
                    Pick {
                        key: "hudStyle"
                        values: ["bar", "graph"]
                        model: [qsTr("Slim bar"), qsTr("Panel with a graph")]
                    }

                    FText { text: qsTr("Bar sits"); visible: barPlace.visible }
                    Pick {
                        id: barPlace
                        key: "hudPlace"
                        visible: page.value("hudStyle") === "bar"
                        model: [qsTr("Top left"), qsTr("Top centre"), qsTr("Top right"), qsTr("Bottom left"), qsTr("Bottom centre"), qsTr("Bottom right")]
                    }

                    FText { text: qsTr("Bar background"); visible: barPlace.visible }
                    Pick {
                        key: "hudOpacity"
                        visible: barPlace.visible
                        values: [25, 40, 55, 70, 85]
                        model: [qsTr("Very light"), qsTr("Light"), qsTr("Medium"), qsTr("Dark"), qsTr("Very dark")]
                    }
                }
                Tick {
                    key: "logVerbose"
                    text: qsTr("Verbose stream log")
                }
                FButton { text: qsTr("Open stream logs"); onClicked: Farside.openFolder(Chiaki.settings.logDirectory) }
                FText { text: qsTr("These take effect the next time a stream starts."); color: shell.muted }
            }
        }
    }

    Rectangle {
        id: saveBar
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: 16
        height: 60
        radius: 12
        color: "#1a1a25"
        border.color: shell.edge
        visible: page.dirty

        FText {
            x: 20
            anchors.verticalCenter: parent.verticalCenter
            text: qsTr("You have unsaved changes.")
        }
        Row {
            anchors.right: parent.right
            anchors.rightMargin: 8
            anchors.verticalCenter: parent.verticalCenter
            spacing: 10

            FButton { text: qsTr("Reset"); onClicked: page.reset() }
            FButton { primary: true; text: qsTr("Save changes"); onClicked: page.save() }
        }
    }
}
