// Bottom strip of a Farside window: the version and the legal line with "Check for updates", the answer
// to that check, or the update that waits.
// A fetched update is installed only when the user clicks: a restart in the middle of a game would
// drop the controller (Client) or the stream (Host).
import QtQuick
import QtQuick.Templates as T

import org.streetpea.chiaking

Rectangle {
    height: 28
    color: shell.side

    component Link: T.AbstractButton {
        id: link
        property color ink: shell.accent

        anchors.verticalCenter: parent.verticalCenter
        implicitWidth: word.implicitWidth
        implicitHeight: 20
        hoverEnabled: true
        Keys.onReturnPressed: clicked()

        contentItem: FText {
            id: word
            text: link.text
            color: link.ink
            font.pixelSize: 12
            font.weight: Font.DemiBold
            font.underline: link.hovered || link.visualFocus
            verticalAlignment: Text.AlignVCenter
        }
    }

    Row {
        anchors.centerIn: parent
        visible: Farside.updateVersion === ""
        spacing: 12

        FText {
            anchors.verticalCenter: parent.verticalCenter
            text: Farside.updateState === "checking" ? qsTr("Checking for updates.")
                : Farside.updateState === "current" ? qsTr("Version %1 is the newest.").arg(Farside.version)
                : Farside.updateState === "failed" ? qsTr("Could not ask GitHub for updates. Try again later.")
                : qsTr("Version %1 · © 2026 Moonthia. Not affiliated with Sony Interactive Entertainment.").arg(Farside.version)
            color: shell.muted
            font.pixelSize: 12
        }
        // The check also runs by itself (see farsidebackend.h); updates come as Setup programs for Windows.
        Link {
            visible: Farside.updateState === "" && Qt.platform.os === "windows"
            text: qsTr("Check for updates")
            ink: shell.ink
            onClicked: Farside.checkForUpdate(true)
        }
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
        Link {
            visible: Farside.updateState === "ready"
            text: qsTr("Restart and update")
            onClicked: Farside.installUpdate()
        }
    }
}
