import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material

ComboBox {
    id: control
    property bool firstInFocusChain: false
    property bool lastInFocusChain: false
    implicitContentWidthPolicy: ComboBox.WidestText
    opacity: enabled ? 1 : 0.45
    hoverEnabled: true
    // Farside look (see FCombo.qml): a rounded dark box with an arrow, and a list whose current row is pink.
    background: Rectangle {
        implicitWidth: 120
        implicitHeight: 48
        radius: 10
        color: control.down || control.popup.visible ? "#20202c" : control.hovered ? "#1a1a25" : "#08080c"
        border.color: control.visualFocus || control.popup.visible ? "#d4267e" : "#34344a"
    }
    indicator: Canvas {
        x: control.width - 30
        y: (control.height - 8) / 2
        width: 24
        height: 16
        scale: 0.5
        transformOrigin: Item.TopLeft
        onPaint: {
            const c = getContext("2d");
            c.lineWidth = 3.5;
            c.lineCap = "round";
            c.lineJoin = "round";
            c.strokeStyle = "#a3a3b5";
            c.beginPath();
            c.moveTo(3, 3);
            c.lineTo(12, 12);
            c.lineTo(21, 3);
            c.stroke();
        }
    }
    delegate: ItemDelegate {
        id: option
        required property var model
        required property int index

        width: ListView.view ? ListView.view.width : 0
        text: model[control.textRole]
        font: control.font
        highlighted: control.highlightedIndex === index
        hoverEnabled: control.hoverEnabled

        contentItem: Text {
            leftPadding: 4
            text: option.text
            font.family: option.font.family
            font.pixelSize: option.font.pixelSize
            font.weight: control.currentIndex === option.index ? Font.DemiBold : Font.Normal
            color: option.highlighted || control.currentIndex === option.index ? "#ffffff" : "#f3f3f6"
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }
        background: Rectangle {
            radius: 8
            color: option.highlighted ? "#d4267e" : "transparent"
        }
    }
    popup.y: control.height + 4
    popup.padding: 6
    popup.background: Rectangle {
        radius: 12
        color: "#14141d"
        border.color: "#34344a"
    }

    Keys.onPressed: (event) => {
        switch (event.key) {
        case Qt.Key_Up:
            if (!popup.visible) {
                let item = nextItemInFocusChain(false);
                if (!firstInFocusChain && item)
                    item.forceActiveFocus(Qt.TabFocusReason);
                event.accepted = true;
            }
            break;
        case Qt.Key_Down:
            if (!popup.visible) {
                let item = nextItemInFocusChain();
                if (!lastInFocusChain && item)
                    item.forceActiveFocus(Qt.TabFocusReason);
                event.accepted = true;
            }
            break;
        case Qt.Key_Return:
            if (popup.visible) {
                activated(highlightedIndex);
                popup.close();
            } else {
                popup.open();
            }
            event.accepted = true;
            break;
        }
    }

    Keys.onReleased: (event) => {
        if (event.key == Qt.Key_Return)
            event.accepted = true;
    }
}
