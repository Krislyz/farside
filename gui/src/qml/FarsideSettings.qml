// Settings page: the switches that are for looking into problems, not for playing.
import QtQuick
import QtQuick.Layouts
import QtQuick.Templates as T

import org.streetpea.chiaking

Item {
    // Taller than a small window, so the page scrolls (mouse wheel, or drag the bar at the right).
    Flickable {
        id: scroller
        anchors.fill: parent
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

        ColumnLayout {
            id: content
            x: 28
            y: 28
            width: Math.min(scroller.width - 56, 980)
            spacing: 18

            Rectangle {
                Layout.fillWidth: true
                implicitHeight: diagnostics.implicitHeight + 40
                radius: 14
                color: shell.card
                border.color: shell.line

                ColumnLayout {
                    id: diagnostics
                    x: 22
                    y: 20
                    width: parent.width - 44
                    spacing: 14

                    FText { text: qsTr("Diagnostics"); font.pixelSize: 15; font.weight: Font.DemiBold }
                    FCheck {
                        text: qsTr("Log: write controller link events to a file")
                        checked: Farside.logEnabled
                        onToggled: Farside.logEnabled = checked
                    }
                    FCheck {
                        text: qsTr("Debug ms: show the delay to each connected PC in milliseconds")
                        checked: shell.options.debugMs
                        onToggled: shell.options.debugMs = checked
                    }
                    FButton { text: qsTr("Open log folder"); onClicked: Farside.openFolder(Farside.logFolder()) }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                implicitHeight: picture.implicitHeight + 40
                radius: 14
                color: shell.card
                border.color: shell.line
                visible: !shell.isClient

                ColumnLayout {
                    id: picture
                    x: 22
                    y: 20
                    width: parent.width - 44
                    spacing: 14

                    FText { text: qsTr("Picture"); font.pixelSize: 15; font.weight: Font.DemiBold }

                    GridLayout {
                        columns: 2
                        columnSpacing: 16
                        rowSpacing: 12

                        FText { text: qsTr("Upscaling") }
                        FCombo {
                            implicitWidth: 380
                            model: [qsTr("Fast (least GPU work)"), qsTr("Default"), qsTr("High quality"), qsTr("High quality + spatial upscaling"), qsTr("High quality + advanced spatial upscaling"), qsTr("Custom")]
                            currentIndex: Chiaki.settings.videoPreset
                            onActivated: (index) => {
                                Chiaki.settings.videoPreset = index;
                                // The same order as ChiakiWindow.VideoPreset; this applies it to a running stream too.
                                Chiaki.window.videoPreset = index;
                            }
                        }

                        FText { text: qsTr("Upscaler"); visible: custom.visible }
                        FCombo {
                            id: custom
                            implicitWidth: 380
                            visible: Chiaki.settings.videoPreset === 5
                            model: [qsTr("From the renderer options"), qsTr("Nearest"), qsTr("Bilinear"), qsTr("Oversample"), qsTr("Bicubic"), qsTr("Gaussian"), qsTr("Catmull Rom"), qsTr("Lanczos"), qsTr("EwaLanczos"), qsTr("EwaLanczosSharp"), qsTr("EwaLanczosSharpest"), qsTr("FSR"), qsTr("FSRCNNX x2 8-0-4-1"), qsTr("FSRCNNX x2 16-0-4-1")]
                            currentIndex: Chiaki.settings.placeboUpscaler
                            onActivated: (index) => Chiaki.settings.placeboUpscaler = index
                        }

                        FText { text: qsTr("Frame mixing"); visible: custom.visible }
                        FCombo {
                            implicitWidth: 380
                            visible: custom.visible
                            model: [qsTr("Off"), qsTr("Oversample"), qsTr("Hermite"), qsTr("Linear"), qsTr("Cubic")]
                            currentIndex: Chiaki.settings.placeboFrameMixer
                            onActivated: (index) => Chiaki.settings.placeboFrameMixer = index
                        }
                    }

                    FCheck {
                        text: qsTr("Debanding: smooth out colour steps in skies and shadows")
                        visible: custom.visible
                        checked: Chiaki.settings.placeboDebandEnabled
                        onToggled: Chiaki.settings.placeboDebandEnabled = checked
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
            }

            Rectangle {
                Layout.fillWidth: true
                implicitHeight: hud.implicitHeight + 40
                radius: 14
                color: shell.card
                border.color: shell.line
                visible: !shell.isClient

                ColumnLayout {
                    id: hud
                    x: 22
                    y: 20
                    width: parent.width - 44
                    spacing: 14

                    FText { text: qsTr("Stream HUD"); font.pixelSize: 15; font.weight: Font.DemiBold }
                    FCheck {
                        text: qsTr("Response label next to the bitrate")
                        checked: shell.options.hudResponse
                        onToggled: shell.options.hudResponse = checked
                    }
                    FCheck {
                        text: qsTr("Stream statistics (bitrate, packet loss, frame times in ms)")
                        checked: Chiaki.settings.showStreamStats
                        onToggled: Chiaki.settings.showStreamStats = checked
                    }
                    FCheck {
                        text: qsTr("Verbose stream log")
                        checked: Chiaki.settings.logVerbose
                        onToggled: Chiaki.settings.logVerbose = checked
                    }
                    FButton { text: qsTr("Open stream logs"); onClicked: Farside.openFolder(Chiaki.settings.logDirectory) }
                    FText { text: qsTr("These take effect the next time a stream starts."); color: shell.muted }
                }
            }
        }
    }
}
