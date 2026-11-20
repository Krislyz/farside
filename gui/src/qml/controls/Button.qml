import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material

Button {
    id: control
    property bool firstInFocusChain: false
    property bool lastInFocusChain: false
    property bool sendOutput: false

    Material.background: visualFocus ? Material.accent : undefined

    // Farside look (see FButton.qml): outlined, and filled with the accent colour while it has the focus.
    background: Rectangle {
        implicitWidth: 64
        implicitHeight: 40
        radius: 10
        color: control.visualFocus ? "#d4267e" : control.down ? "#20202c" : control.hovered ? "#1a1a25" : "transparent"
        border.width: control.visualFocus ? 0 : 1
        border.color: "#34344a"
    }

    Component.onDestruction: {
        if (visualFocus) {
            let item = nextItemInFocusChain();
            if (item)
                item.forceActiveFocus(Qt.TabFocusReason);
        }
    }

    Keys.onPressed: (event) => {
        switch (event.key) {
        case Qt.Key_Up:
            if (!firstInFocusChain) {
                let item = nextItemInFocusChain(false);
                if (item)
                    item.forceActiveFocus(Qt.TabFocusReason);
                if(!sendOutput)
                    event.accepted = true;
            }
            break;
        case Qt.Key_Down:
            if (!lastInFocusChain) {
                let item = nextItemInFocusChain();
                if (item)
                    item.forceActiveFocus(Qt.TabFocusReason);
                if(!sendOutput)
                    event.accepted = true;
            }
            break;
        case Qt.Key_Return:
            if (visualFocus) {
                clicked();
            }
            event.accepted = true;
            break;
        }
    }
}
