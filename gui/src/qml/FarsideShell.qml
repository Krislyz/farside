// The Farside window outside a stream: header, sidebar (Play, Remote Play, Settings) and the page.
// Sizes are those of the design boards (1120 x 700); the whole frame is scaled to the window, so the
// window looks like the boards at any size.
import QtCore
import QtQuick
import QtQuick.Controls
import QtQuick.Templates as T

import org.streetpea.chiaking

Item {
    id: shell

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

    // A Client only shares its controller; it has no Play page.
    readonly property bool isClient: Farside.role === "client"
    property string page: isClient ? "remote" : "play"
    // The frame is drawn at the size chosen under Settings, "Text size" (0.9 unless changed); a window too
    // small for the boards (1120 x 700) shrinks it further.
    readonly property real u: Math.max(Math.min(0.75, options.textSize), Math.min(options.textSize, width / 1120, height / 700))
    property alias options: options

    function showPage(name) {
        page = name;
        if (name === "play" && !isClient)
            playPage.activated();
        else
            frame.forceActiveFocus();
    }

    StackView.onActivated: showPage(page)

    // Shared with the bar of the stream (StreamView.qml), which reads hudResponse when a stream starts.
    Settings {
        id: options
        category: "farside"
        property bool debugMs: false
        property bool hudResponse: false
        property real textSize: 0.9
    }

    Rectangle {
        anchors.fill: parent
        color: shell.night
    }

    Item {
        id: frame
        width: shell.width / shell.u
        height: shell.height / shell.u
        scale: shell.u
        transformOrigin: Item.TopLeft
        Keys.onPressed: (event) => remotePage.keyPressed(event)

        FHeader {
            id: header
            width: parent.width
            title: qsTr("Farside")
            subtitle: qsTr("Plays your PS5 here and takes the controller of a paired PC")
        }

        Rectangle {
            id: nav
            y: header.height
            width: 190
            height: parent.height - header.height - footer.height
            color: shell.side

            Rectangle {
                anchors.right: parent.right
                width: 1
                height: parent.height
                color: shell.line
            }

            Column {
                y: 16
                width: parent.width - 1
                spacing: 4

                Repeater {
                    model: [
                        { page: "play", text: qsTr("Play"), shown: !shell.isClient },
                        { page: "remote", text: qsTr("Remote Play"), shown: true },
                        { page: "settings", text: qsTr("Settings"), shown: true }
                    ]

                    T.AbstractButton {
                        id: entry
                        required property var modelData
                        readonly property bool current: shell.page === modelData.page

                        width: parent.width
                        height: 44
                        visible: modelData.shown
                        hoverEnabled: true
                        onClicked: shell.showPage(modelData.page)
                        Keys.onReturnPressed: clicked()

                        background: Rectangle {
                            color: entry.current ? shell.card : entry.hovered ? "#101018" : "transparent"

                            Rectangle {
                                width: 3
                                height: parent.height
                                color: entry.current || entry.visualFocus ? shell.accent : "transparent"
                            }
                        }
                        contentItem: FText {
                            leftPadding: 20
                            text: entry.modelData.text
                            color: entry.current ? shell.ink : shell.muted
                            font.weight: entry.current ? Font.DemiBold : Font.Normal
                            verticalAlignment: Text.AlignVCenter
                        }
                    }
                }
            }
        }

        Item {
            x: nav.width
            y: header.height
            width: parent.width - nav.width
            height: nav.height

            FarsidePlay {
                id: playPage
                anchors.fill: parent
                visible: shell.page === "play" && !shell.isClient
            }

            FarsideRemotePlay {
                id: remotePage
                anchors.fill: parent
                visible: shell.page === "remote"
            }

            FarsideSettings {
                anchors.fill: parent
                visible: shell.page === "settings"
            }
        }

        FFooter {
            id: footer
            y: parent.height - height
            width: parent.width
        }

        // A PC entered this PC's pairing code and waits for the answer.
        Rectangle {
            anchors.fill: parent
            color: "#cc000000"
            visible: Farside.pairRequest !== ""
            onVisibleChanged: if (visible) accept.forceActiveFocus()

            MouseArea {
                anchors.fill: parent
                hoverEnabled: true
                acceptedButtons: Qt.AllButtons
                onWheel: (wheel) => wheel.accepted = true
            }

            Rectangle {
                anchors.centerIn: parent
                width: 460
                height: question.implicitHeight + 48
                radius: 14
                color: shell.card
                border.color: shell.line

                Column {
                    id: question
                    x: 24
                    y: 24
                    width: parent.width - 48
                    spacing: 12

                    FText {
                        width: parent.width
                        text: qsTr("%1 wants to pair").arg(Farside.pairRequest.replace(/_/g, " "))
                        font.pixelSize: 18
                        font.weight: Font.DemiBold
                        elide: Text.ElideRight
                    }
                    FText {
                        width: parent.width
                        visible: Farside.pairCheck !== ""
                        text: qsTr("Number: %1").arg(Farside.pairCheck)
                        font.family: "Space Grotesk"
                        font.pixelSize: 28
                        font.weight: Font.DemiBold
                        color: shell.accent
                    }
                    FText {
                        width: parent.width
                        text: Farside.pairCheck !== ""
                            ? qsTr("Accept only if the other PC shows this same number right now. A paired PC can attach its controller to this PC until you unpair it.")
                            : qsTr("Accept only if you entered this PC's code there just now. A paired PC can attach its controller to this PC until you unpair it.")
                        color: shell.muted
                        wrapMode: Text.WordWrap
                    }
                    Row {
                        anchors.right: parent.right
                        spacing: 12

                        FButton {
                            text: qsTr("Reject")
                            onClicked: Farside.answerPair(false)
                        }
                        FButton {
                            id: accept
                            primary: true
                            text: qsTr("Accept")
                            onClicked: Farside.answerPair(true)
                        }
                    }
                }
            }
        }

        FChannelQuestion { }
    }
}
