import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material

RadioButton {
    id: control
    // Farside look: a ring with a pink dot, and a pink ring around it while it has the focus.
    indicator: Rectangle {
        x: control.text ? control.leftPadding : control.leftPadding + (control.availableWidth - width) / 2
        y: control.topPadding + (control.availableHeight - height) / 2
        implicitWidth: 22
        implicitHeight: 22
        radius: 11
        opacity: control.enabled ? 1 : 0.45
        color: "#08080c"
        border.color: control.checked || control.hovered ? "#d4267e" : "#4a4a60"

        Rectangle {
            anchors.centerIn: parent
            width: 10
            height: 10
            radius: 5
            color: "#d4267e"
            visible: control.checked
        }
        Rectangle {
            anchors.fill: parent
            anchors.margins: -4
            radius: 15
            color: "transparent"
            border.width: 2
            border.color: "#d4267e"
            visible: control.visualFocus
        }
    }
    property bool firstInFocusChain: false
    property bool lastInFocusChain: false

    Keys.onPressed: (event) => {
        switch (event.key) {
        case Qt.Key_Up:
            if (!firstInFocusChain) {
                let item = nextItemInFocusChain(false);
                if (item)
                    item.forceActiveFocus(Qt.TabFocusReason);
                event.accepted = true;
            }
            break;
        case Qt.Key_Down:
            if (!lastInFocusChain) {
                let item = nextItemInFocusChain();
                if (item)
                    item.forceActiveFocus(Qt.TabFocusReason);
                event.accepted = true;
            }
            break;
        case Qt.Key_Return:
            if (visualFocus) {
                toggle();
                toggled();
            }
            event.accepted = true;
            break;
        }
    }
}
