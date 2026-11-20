// Picker of the Farside pages: a button that shows the choice and opens the list below it.
// The pages are scaled to the window (FarsideShell.u) but a list opens in the window's own layer,
// which is not, so the list is scaled here to match.
import QtQuick
import QtQuick.Templates as T

T.ComboBox {
    id: combo

    implicitWidth: 320
    implicitHeight: 44
    hoverEnabled: true
    font.family: "Inter"
    font.pixelSize: 14
    Keys.onReturnPressed: popup.visible ? popup.close() : popup.open()

    contentItem: FText {
        leftPadding: 14
        rightPadding: 36
        text: combo.displayText
        font: combo.font
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }
    indicator: Canvas {
        x: combo.width - 28
        y: (combo.height - 8) / 2
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
    background: Rectangle {
        radius: 10
        color: combo.down || combo.popup.visible ? "#20202c" : combo.hovered ? "#1a1a25" : "#08080c"
        border.color: combo.visualFocus || combo.popup.visible ? "#d4267e" : "#34344a"
    }

    delegate: T.ItemDelegate {
        id: option
        required property var modelData
        required property int index

        width: ListView.view ? ListView.view.width : 0
        height: 38
        hoverEnabled: true
        highlighted: combo.highlightedIndex === index

        contentItem: FText {
            leftPadding: 14
            rightPadding: 14
            text: option.modelData
            color: combo.currentIndex === option.index ? "#ffffff" : "#f3f3f6"
            font.weight: combo.currentIndex === option.index ? Font.DemiBold : Font.Normal
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }
        background: Rectangle {
            radius: 8
            color: option.highlighted ? "#d4267e" : "transparent"
        }
    }

    popup: T.Popup {
        y: combo.height + 4
        width: combo.width
        height: Math.min(list.contentHeight, 38 * 7) + 12
        padding: 6
        scale: shell.u
        transformOrigin: T.Popup.TopLeft

        contentItem: ListView {
            id: list
            clip: true
            model: combo.delegateModel
            currentIndex: combo.highlightedIndex
            boundsBehavior: Flickable.StopAtBounds
        }
        background: Rectangle {
            radius: 12
            color: "#14141d"
            border.color: "#34344a"
        }
    }
}
