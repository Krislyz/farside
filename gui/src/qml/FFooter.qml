// Bottom strip of a Farside window: the version and the legal line, or the update that waits.
// A fetched update is installed only when the user clicks: a restart in the middle of a game would
// drop the controller (Client) or the stream (Host).
import QtQuick
import QtQuick.Templates as T

import org.streetpea.chiaking

Rectangle {
    height: 28
    color: shell.side

    FText {
        anchors.centerIn: parent
        visible: Farside.updateVersion === ""
        text: qsTr("Version %1 · © 2026 Moonthia. Not affiliated with Sony Interactive Entertainment.").arg(Farside.version)
        color: shell.muted
        font.pixelSize: 12
    }

    Row {
        anchors.centerIn: parent
        visible: Farside.updateVersion !== ""
        spacing: 12

        FText {
            anchors.verticalCenter: parent.verticalCenter
            text: Farside.updateState === "ready" ? qsTr("Version %1 is ready to install.").arg(Farside.updateVersion)
                                                  : qsTr("Fetching version %1.").arg(Farside.updateVersion)
            color: shell.ink
            font.pixelSize: 12
        }
        T.AbstractButton {
            id: install
            anchors.verticalCenter: parent.verticalCenter
            visible: Farside.updateState === "ready"
            implicitWidth: word.implicitWidth
            implicitHeight: 20
            hoverEnabled: true
            onClicked: Farside.installUpdate()
            Keys.onReturnPressed: clicked()

            contentItem: FText {
                id: word
                text: qsTr("Restart and update")
                color: shell.accent
                font.pixelSize: 12
                font.weight: Font.DemiBold
                font.underline: install.hovered || install.visualFocus
                verticalAlignment: Text.AlignVCenter
            }
        }
    }
}
