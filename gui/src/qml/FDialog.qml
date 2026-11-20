// Popup of the screens that are not Farside pages, in the Farside look (see ConfirmDialog.qml): a dark
// card, FButtons for the dialog's standard buttons, and scaled like the pages because it opens in the
// window's own layer.
import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material

Dialog {
    id: dialog
    padding: 24
    topPadding: 12
    bottomPadding: 12
    font.family: "Inter"
    font.pixelSize: 14
    Material.theme: Material.Dark
    Material.accent: "#d4267e"
    Material.foreground: "#f3f3f6"
    scale: Math.max(0.75, Math.min(1.2, root.width / 1120, root.height / 700))
    // Only the opacity is animated: the style's own transitions animate the scale, which is bound above.
    enter: Transition {
        NumberAnimation { property: "opacity"; from: 0.0; to: 1.0; duration: 150 }
    }
    exit: Transition {
        NumberAnimation { property: "opacity"; from: 1.0; to: 0.0; duration: 150 }
    }
    // The page behind is dimmed by this oversized sheet: the style's own dimmer (Overlay.modal) did not show here.
    dim: false
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
    footer: DialogButtonBox {
        visible: count > 0
        padding: 24
        topPadding: 8
        bottomPadding: 24
        spacing: 12
        background: Item { }
        delegate: FButton {
            primary: DialogButtonBox.buttonRole === DialogButtonBox.AcceptRole || DialogButtonBox.buttonRole === DialogButtonBox.YesRole
        }
    }
}
