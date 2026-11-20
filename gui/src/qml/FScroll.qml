// Scroll bar in the Farside look (the bar of FarsideSettings.qml), for the other screens.
import QtQuick
import QtQuick.Templates as T

T.ScrollBar {
    id: bar

    implicitWidth: 12
    implicitHeight: 12
    padding: 3
    minimumSize: 0.1

    contentItem: Rectangle {
        implicitWidth: 6
        implicitHeight: 6
        radius: 3
        color: bar.pressed ? "#d4267e" : bar.hovered ? "#5d5d70" : "#3a3a50"
    }
}
