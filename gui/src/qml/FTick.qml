// The box of the style's own CheckBox on the screens that are not Farside pages, in the Farside look
// (see FCheck.qml):
//     indicator: FTick { }
import QtQuick

Rectangle {
    id: tick
    // The check box: an indicator is a child of its control.
    readonly property Item box: parent
    readonly property bool checked: box ? box.checked : false

    x: !box ? 0 : box.text ? box.leftPadding : box.leftPadding + (box.availableWidth - width) / 2
    y: !box ? 0 : box.topPadding + (box.availableHeight - height) / 2
    implicitWidth: 22
    implicitHeight: 22
    radius: 5
    opacity: !box || box.enabled ? 1 : 0.45
    color: checked ? "#d4267e" : "#08080c"
    border.width: checked ? 0 : 1
    border.color: box && box.hovered ? "#d4267e" : "#4a4a60"

    // Shown while the box has the keyboard or controller focus.
    Rectangle {
        anchors.fill: parent
        anchors.margins: -4
        radius: 8
        color: "transparent"
        border.width: 2
        border.color: "#d4267e"
        visible: tick.box ? tick.box.visualFocus : false
    }
    // The tick, drawn at twice the size so it stays sharp.
    Canvas {
        width: 44
        height: 44
        scale: 0.5
        transformOrigin: Item.TopLeft
        visible: tick.checked
        onPaint: {
            const c = getContext("2d");
            c.lineWidth = 5;
            c.lineCap = "round";
            c.lineJoin = "round";
            c.strokeStyle = "#ffffff";
            c.beginPath();
            c.moveTo(11, 23);
            c.lineTo(19, 30.5);
            c.lineTo(33, 14);
            c.stroke();
        }
    }
}
