// Button of the Farside pages: outlined, or filled with the accent colour when primary.
import QtQuick
import QtQuick.Templates as T

T.Button {
    id: button
    property bool primary: false
    readonly property color accent: "#d4267e"

    implicitHeight: 44
    implicitWidth: label.implicitWidth + (primary ? 44 : 36)
    hoverEnabled: true
    opacity: enabled ? 1 : 0.45
    font.family: "Inter"
    font.pixelSize: 14
    font.weight: primary ? Font.DemiBold : Font.Normal
    Keys.onReturnPressed: clicked()

    contentItem: Text {
        id: label
        text: button.text
        font: button.font
        color: button.primary ? "#ffffff" : "#f3f3f6"
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
    }
    background: Rectangle {
        radius: 10
        color: button.primary ? (button.down ? Qt.darker(button.accent, 1.25) : button.hovered ? Qt.lighter(button.accent, 1.12) : button.accent)
                              : (button.down ? "#20202c" : button.hovered ? "#1a1a25" : "transparent")
        border.width: button.primary ? (button.visualFocus ? 2 : 0) : 1
        border.color: button.visualFocus ? "#ffffff" : "#34344a"
    }
}
