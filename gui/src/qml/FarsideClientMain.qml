// Farside Client: one window.
import QtQuick

Window {
    // Set by a test run (FARSIDE_SHOT_TAB) to show another tab than the first.
    property alias tab: shell.tab

    width: 1120
    height: 700
    minimumWidth: 840
    minimumHeight: 525
    visible: true
    title: qsTr("Farside Client")
    color: "#0b0b10"

    FarsideClientShell {
        id: shell
        anchors.fill: parent
    }
}
