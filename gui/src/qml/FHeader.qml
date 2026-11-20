// Top strip of a Farside window: the mark, the program's name and one line under it.
// Used inside a shell (FarsideShell.qml, FarsideClientShell.qml), whose colours it reads.
import QtQuick

Rectangle {
    property alias title: name.text
    property alias subtitle: under.text

    height: 84
    color: shell.side

    Rectangle {
        anchors.bottom: parent.bottom
        width: parent.width
        height: 1
        color: shell.line
    }

    Row {
        x: 24
        anchors.verticalCenter: parent.verticalCenter
        spacing: 16

        Rectangle {
            width: 48
            height: 48
            radius: 14
            color: "#10121b"
            border.color: "#3a3a50"

            // The Farside mark (brand/farside-mark.svg), drawn at twice the size so it stays sharp.
            Canvas {
                x: 7
                y: 7
                width: 68
                height: 68
                scale: 0.5
                transformOrigin: Item.TopLeft
                onPaint: {
                    const c = getContext("2d");
                    c.scale(width / 860, height / 860);
                    c.translate(-238, -197);
                    function shape(points, fill) {
                        c.beginPath();
                        c.moveTo(points[0], points[1]);
                        for (let i = 2; i < points.length; i += 2)
                            c.lineTo(points[i], points[i + 1]);
                        c.closePath();
                        c.fillStyle = fill;
                        c.fill();
                    }
                    const fold = c.createLinearGradient(505, 500, 893, 538);
                    fold.addColorStop(0, "#ff3e91");
                    fold.addColorStop(1, "#9e1f5b");
                    shape([272, 760, 517, 405, 497, 597], "#ff53a9");
                    shape([517, 405, 1063, 290, 893, 538], "#fd4fa4");
                    shape([517, 405, 893, 538, 497, 597], fold);
                    shape([320, 963, 552, 623, 872, 604, 802, 714, 623, 719], "#e9eef6");
                    shape([623, 719, 802, 714, 758, 782, 624, 820], "#3f546f");
                    shape([623, 719, 624, 820, 320, 963], "#4c6581");
                }
            }
        }

        Column {
            anchors.verticalCenter: parent.verticalCenter

            FText {
                id: name
                font.family: "Space Grotesk"
                font.pixelSize: 22
                font.weight: Font.DemiBold
            }
            FText {
                id: under
                color: shell.muted
                font.pixelSize: 13
            }
        }
    }
}
