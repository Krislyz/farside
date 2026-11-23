// Asked once when an alpha version was installed by hand (see Farside.channelQuestion): which versions
// the updater brings from now on. Lies over the whole window of a shell, whose colours it reads.
import QtQuick

import org.streetpea.chiaking

Rectangle {
    anchors.fill: parent
    color: "#cc000000"
    visible: Farside.channelQuestion
    onVisibleChanged: if (visible) stable.forceActiveFocus()

    MouseArea {
        anchors.fill: parent
        hoverEnabled: true
        acceptedButtons: Qt.AllButtons
        onWheel: (wheel) => wheel.accepted = true
    }

    Rectangle {
        anchors.centerIn: parent
        width: 480
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
                text: qsTr("This is an alpha version")
                font.pixelSize: 18
                font.weight: Font.DemiBold
            }
            FText {
                width: parent.width
                text: qsTr("Version %1 is an early version and may break. Which versions do you want from now on?").arg(Farside.version)
                wrapMode: Text.WordWrap
            }
            FText {
                width: parent.width
                text: qsTr("Stable only: the next update is the next full release. Stay on alpha: early versions keep coming.")
                      + (shell.isClient ? "" : " " + qsTr("You can change this later under Settings, Updates."))
                color: shell.muted
                wrapMode: Text.WordWrap
            }
            Row {
                anchors.right: parent.right
                spacing: 12

                FButton {
                    text: qsTr("Stay on alpha")
                    onClicked: Farside.answerChannel(true)
                }
                FButton {
                    id: stable
                    primary: true
                    text: qsTr("Stable only")
                    onClicked: Farside.answerChannel(false)
                }
            }
        }
    }
}
