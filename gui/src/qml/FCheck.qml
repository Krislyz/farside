// Tick box with its text, for the Settings page.
import QtQuick
import QtQuick.Templates as T

T.CheckBox {
    id: box
    implicitHeight: 28
    implicitWidth: 30 + label.implicitWidth
    hoverEnabled: true

    indicator: Rectangle {
        y: (box.height - height) / 2
        width: 18
        height: 18
        radius: 4
        color: box.checked ? "#d4267e" : "#08080c"
        border.width: box.checked ? 0 : 1
        border.color: box.hovered || box.visualFocus ? "#d4267e" : "#4a4a60"
        // The tick, drawn at twice the size so it stays sharp when the page is scaled up.
        Canvas {
            width: 36
            height: 36
            scale: 0.5
            transformOrigin: Item.TopLeft
            visible: box.checked
            onPaint: {
                const c = getContext("2d");
                c.lineWidth = 4.5;
                c.lineCap = "round";
                c.lineJoin = "round";
                c.strokeStyle = "#ffffff";
                c.beginPath();
                c.moveTo(9, 19);
                c.lineTo(15.5, 25);
                c.lineTo(27, 11.5);
                c.stroke();
            }
        }
    }
    contentItem: FText {
        id: label
        leftPadding: 30
        text: box.text
        verticalAlignment: Text.AlignVCenter
    }
}
