import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

import org.streetpea.chiaking

// Farside look. It opens in the window's own layer, which is not scaled with the pages, so it is scaled here.
Dialog {
    id: dialog
    property alias text: label.text
    property var callback
    property var rejectCallback
    property bool newDialogOpen: false
    property Item restoreFocusItem
    parent: Overlay.overlay
    x: Math.round((root.width - width) / 2)
    y: Math.round((root.height - height) / 2)
    width: 520
    padding: 24
    topPadding: 8
    modal: true
    scale: Math.max(0.75, Math.min(1.2, root.width / 1120, root.height / 700))
    // The page behind is dimmed by this oversized sheet: the style's own dimmer (Overlay.modal) did not show here.
    dim: false
    // A click beside the question is not an answer to it.
    closePolicy: Popup.CloseOnEscape
    background: Item {
        Rectangle {
            x: -4000
            y: -4000
            width: 8000 + parent.width
            height: 8000 + parent.height
            color: "#cc000000"
        }
        Rectangle {
            anchors.fill: parent
            radius: 14
            color: "#14141d"
            border.color: "#262633"
        }
    }
    header: FText {
        text: dialog.title
        padding: 24
        bottomPadding: 0
        font.pixelSize: 18
        font.weight: Font.DemiBold
        wrapMode: Text.WordWrap
    }
    onOpened: label.forceActiveFocus(Qt.TabFocusReason)
    onAccepted: {
        newDialogOpen = true;
        restoreFocus();
        callback();
    }
    onClosed: if(!newDialogOpen) { restoreFocus() }

    onRejected: {
        if(rejectCallback)
        {
            newDialogOpen = true;
            restoreFocus();
            rejectCallback();
        }
    }

    function restoreFocus() {
        if (restoreFocusItem)
            restoreFocusItem.forceActiveFocus(Qt.TabFocusReason);
        label.focus = false;
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 20

        FText {
            id: label
            Layout.fillWidth: true
            color: "#a3a3b5"
            wrapMode: Text.WordWrap
            Keys.onEscapePressed: dialog.reject()
            Keys.onReturnPressed: dialog.accept()
        }

        RowLayout {
            Layout.alignment: Qt.AlignRight
            spacing: 12

            FButton {
                text: qsTr("No")
                focusPolicy: Qt.NoFocus
                onClicked: dialog.reject()
            }
            FButton {
                primary: true
                text: qsTr("Yes")
                focusPolicy: Qt.NoFocus
                onClicked: dialog.accept()
            }
        }
    }
}
