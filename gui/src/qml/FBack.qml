// Background for the style's own Button on the screens that are not Farside pages, in the Farside look
// (see FButton.qml): outlined, and filled with pink while the button has the focus.
//     background: FBack { }
// For a tool button (flat: true) there is no outline; the one that has the focus, which these buttons
// show as "down", gets a dark fill and a pink edge.
import QtQuick

Rectangle {
    // The button: a background is a child of its control.
    readonly property Item button: parent
    property bool flat: false

    implicitWidth: flat ? 48 : 64
    implicitHeight: flat ? 48 : 40
    radius: 10
    opacity: !button || button.enabled ? 1 : 0.45
    color: !button ? "transparent"
         : flat ? (button.down || button.visualFocus ? "#20202c" : button.hovered ? "#1a1a25" : "transparent")
         : button.visualFocus || button.highlighted ? "#d4267e" : button.down ? "#20202c" : button.hovered ? "#1a1a25" : "transparent"
    border.width: !button ? 0 : flat ? (button.down || button.visualFocus ? 1 : 0) : (button.visualFocus || button.highlighted ? 0 : 1)
    border.color: flat ? "#d4267e" : "#34344a"
}
