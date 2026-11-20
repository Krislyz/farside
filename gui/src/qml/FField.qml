// Text field in the Farside look: dark and rounded, with a pink edge while it has the focus.
import QtQuick
import QtQuick.Templates as T

T.TextField {
    id: field

    implicitWidth: implicitBackgroundWidth + leftInset + rightInset
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset, contentHeight + topPadding + bottomPadding)
    leftPadding: 14
    rightPadding: 14
    topPadding: 8
    bottomPadding: 8
    hoverEnabled: true
    opacity: enabled ? 1 : 0.45
    color: "#f3f3f6"
    selectionColor: "#d4267e"
    selectedTextColor: "#ffffff"
    placeholderTextColor: "#5d5d70"
    verticalAlignment: TextInput.AlignVCenter

    Text {
        x: field.leftPadding
        y: field.topPadding
        width: field.width - field.leftPadding - field.rightPadding
        height: field.height - field.topPadding - field.bottomPadding
        text: field.placeholderText
        font: field.font
        color: field.placeholderTextColor
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
        visible: !field.length && !field.preeditText
    }
    background: Rectangle {
        implicitWidth: 120
        implicitHeight: 48
        radius: 10
        color: "#08080c"
        border.color: field.activeFocus ? "#d4267e" : field.hovered ? "#4a4a60" : "#34344a"
    }
}
