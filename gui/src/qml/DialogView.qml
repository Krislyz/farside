import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Templates as T

// Base of the full-window screens that are not Farside pages, in the Farside look (colours of FarsideShell.qml).
// These screens keep chiaki-ng's sizes: they are not scaled to the window like the pages.
Item {
    id: dialog
    Material.theme: Material.Dark
    Material.accent: "#d4267e"
    // The colour of the bars (ToolBar) in these screens.
    Material.primary: "#08080c"
    Material.foreground: "#f3f3f6"
    property alias header: headerLabel.text
    property alias title: titleLabel.text
    property alias buttonText: okButton.text
    property alias buttonEnabled: okButton.enabled
    property alias buttonVisible: okButton.visible
    property Item restoreFocusItem
    default property Item mainItem: null

    signal accepted()
    signal rejected()

    function close() {
        root.closeDialog();
    }

    Keys.onEscapePressed: close()

    Keys.onMenuPressed: {
        if (okButton.enabled)
            okButton.clicked()
    }

    StackView.onDeactivating: {
        restoreFocusItem = Window.window.activeFocusItem;
    }

    StackView.onActivated: {
        if (!restoreFocusItem) {
            let item = mainItem.nextItemInFocusChain();
            if (item)
                item.forceActiveFocus(Qt.TabFocusReason);
        } else {
            restoreFocusItem.forceActiveFocus(Qt.TabFocusReason);
            restoreFocusItem = null;
        }
    }

    onMainItemChanged: {
        if (mainItem) {
            mainItem.parent = contentItem;
            mainItem.anchors.fill = contentItem;
        }
    }

    Rectangle {
        anchors.fill: parent
        color: "#0b0b10"
    }

    ToolBar {
        id: toolBar
        anchors {
            top: parent.top
            left: parent.left
            right: parent.right
        }
        height: 80

        RowLayout {
            anchors {
                fill: parent
                leftMargin: 10
                rightMargin: 10
            }

            FButton {
                id: backButton
                Layout.alignment: Qt.AlignVCenter
                Layout.preferredHeight: 48
                font.pixelSize: 16
                text: qsTr("Back")
                focusPolicy: Qt.NoFocus
                onClicked: {
                    dialog.rejected();
                    dialog.close();
                }
            }

            Item { Layout.fillWidth: true }

            Button {
                id: okButton
                Layout.alignment: Qt.AlignVCenter
                Layout.preferredHeight: 48
                topInset: 0
                bottomInset: 0
                font.family: "Inter"
                font.pixelSize: 16
                font.weight: Font.DemiBold
                focusPolicy: Qt.NoFocus
                Material.foreground: "#ffffff"
                onClicked: dialog.accepted()
                icon.source: "qrc:/icons/options.svg";
                icon.width: 26
                icon.height: 26
                // The main action, in the look of a primary FButton; the style's own content keeps the icon.
                background: Rectangle {
                    radius: 10
                    color: !okButton.enabled ? "transparent" : okButton.down ? Qt.darker("#d4267e", 1.25) : okButton.hovered ? Qt.lighter("#d4267e", 1.12) : "#d4267e"
                    border.width: okButton.enabled ? 0 : 1
                    border.color: "#34344a"
                }
            }
        }

        FText {
            id: titleLabel
            anchors.centerIn: parent
            // Kept clear of the buttons on both sides.
            width: Math.min(implicitWidth, Math.max(0, parent.width - 2 * (Math.max(backButton.width, okButton.visible ? okButton.width : 0) + 30)))
            horizontalAlignment: Qt.AlignHCenter
            verticalAlignment: Qt.AlignVCenter
            font.weight: Font.DemiBold
            font.pixelSize: 22
            elide: Text.ElideRight
        }

        FText {
            id: headerLabel
            anchors {
                top: parent.top
                left: titleLabel.right
                right: parent.right
                verticalCenter: parent.verticalCenter
                leftMargin: 16
                rightMargin: 16
            }
            horizontalAlignment: Qt.AlignHCenter
            verticalAlignment: Qt.AlignVCenter
            color: "#a3a3b5"
            wrapMode: Text.WordWrap
        }

        Rectangle {
            anchors.bottom: parent.bottom
            width: parent.width
            height: 1
            color: "#262633"
        }
    }

    // A control only so that the controls of the screen take its typeface.
    T.Control {
        id: contentItem
        anchors {
            top: toolBar.bottom
            left: parent.left
            right: parent.right
            bottom: parent.bottom
        }
        font.family: "Inter"
    }
}
