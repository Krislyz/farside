// The whole Farside Client window: header, the Remote Play page (Status, Pair, Setup) and the footer.
// Farside Client has no Play page, no Settings page and no sidebar: it only links this PC's controller
// to a Farside Host. Sizes are those of the design boards (1120 x 700), scaled to the window.
import QtQuick

import org.streetpea.chiaking

Item {
    id: shell

    // The same colours as FarsideShell.qml (the pages read them from "shell").
    readonly property color night: "#0b0b10"
    readonly property color side: "#08080c"
    readonly property color card: "#14141d"
    readonly property color line: "#262633"
    readonly property color edge: "#34344a"
    readonly property color ink: "#f3f3f6"
    readonly property color muted: "#a3a3b5"
    readonly property color accent: "#d4267e"
    readonly property color good: "#3ddc97"
    readonly property color warn: "#ff8a3d"
    readonly property color bad: "#ec6078"
    readonly property color off: "#5d5d70"

    readonly property bool isClient: true
    readonly property real u: Math.max(0.75, Math.min(1.2, width / 1120, height / 700))
    // The Host's Settings page has these switches; the Client has none of them.
    readonly property var options: ({ debugMs: false })
    property alias tab: remotePage.tab

    Rectangle {
        anchors.fill: parent
        color: shell.night
    }

    FocusScope {
        id: frame
        width: shell.width / shell.u
        height: shell.height / shell.u
        scale: shell.u
        transformOrigin: Item.TopLeft
        focus: true
        Keys.onPressed: (event) => remotePage.keyPressed(event)

        FHeader {
            id: header
            width: parent.width
            title: qsTr("Farside Client")
            subtitle: qsTr("Sends this PC's controller to your Farside Host")
        }

        FarsideRemotePlay {
            id: remotePage
            y: header.height
            width: parent.width
            height: parent.height - header.height - footer.height
        }

        FFooter {
            id: footer
            y: parent.height - height
            width: parent.width
        }
    }
}
