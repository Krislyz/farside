// Remote Play page: who is paired and linked (Devices on a Host, Status on a Client), pairing, and
// what this PC needs installed. Everything it shows comes from the Farside object (farsidebackend.h).
import QtQuick
import QtQuick.Layouts
import QtQuick.Templates as T

import org.streetpea.chiaking

Item {
    id: remote

    property int tab: Farside.role === "none" ? 2 : 0
    property string pairMessage
    property bool pairFailed: false
    // By address nothing may be there at all, so "click Accept there" would be wrong until the number shows.
    property bool pairByAddress: false

    // Names travel with "_" for every character outside A-Z, a-z, 0-9, ".", "-".
    function shown(name) {
        return name.replace(/_/g, " ");
    }

    function dotColor() {
        switch (Farside.state) {
        case "Linked": return shell.good;
        case "Client online":
        case "Host online":
        case "Connecting": return shell.warn;
        case "Stopped":
        case "Host busy": return shell.bad;
        default: return shell.off;
        }
    }

    function sentence() {
        const peer = shown(Farside.peerName);
        const pad = Farside.controller ? shown(Farside.controller) : qsTr("The controller");
        if (shell.isClient) {
            switch (Farside.state) {
            case "Linked": return qsTr("%1 is attached to %2.").arg(pad).arg(peer);
            case "Host online": return Farside.bus ? qsTr("Connected to %1. %2 on bus %3 is ready.").arg(peer).arg(pad).arg(Farside.bus)
                                                   : qsTr("Connected to %1. No shared controller was found on this PC.").arg(peer);
            case "Connecting": return qsTr("Looking for %1.").arg(peer);
            case "Host busy": return qsTr("%1 is linked to another PC.").arg(peer);
            case "Stopped": return Farside.role === "none" ? qsTr("usbipd-win is not installed on this PC. See Setup.") : "";
            default: return qsTr("Enter the pairing code from Farside Host on the Pair tab.");
            }
        }
        switch (Farside.state) {
        case "Linked": return qsTr("The controller from %1 is attached to this PC.").arg(peer);
        case "Client online": return !Farside.bus ? qsTr("%1 is connected. No controller is shared there.").arg(peer)
                                   : Farside.keepLinked ? qsTr("Attaching the controller from %1.").arg(peer)
                                   : qsTr("%1 is connected. Its controller is not attached.").arg(peer);
        case "Waiting for client": return qsTr("No paired PC is connected right now.");
        case "Stopped": return Farside.role === "none" ? qsTr("usbip-win2 is not installed on this PC. See Setup.") : "";
        default: return qsTr("Pair a PC to use its controller here.");
        }
    }

    function deviceText(device) {
        let text = device.linked ? qsTr("Linked · %1 · bus %2").arg(Farside.controller ? shown(Farside.controller) : qsTr("controller")).arg(Farside.bus)
                 : device.online ? (Farside.bus ? qsTr("Connected · controller ready") : qsTr("Connected · no controller shared there"))
                 : qsTr("Offline");
        if (device.online && shell.options.debugMs && Farside.rttMs >= 0)
            text += " · " + Farside.rttMs + " ms";
        return text;
    }

    // A code that was found (in the clipboard, on the screen) is entered and sent at once; the Host's
    // user still has to accept it.
    function useCode(code, problem) {
        if (Farside.pairing)
            return;
        if (code === "") {
            pairFailed = true;
            pairMessage = problem;
            return;
        }
        codeField.text = code;
        pairMessage = "";
        pairByAddress = false;
        Farside.pair(code);
    }

    function pasteCode() {
        useCode(Farside.codeFromClipboard(), qsTr("No pairing code in the clipboard. On the Host, copy the code or take a screenshot of the QR code."));
    }

    // Ctrl+V anywhere on the Client's Pair tab (FarsideShell passes the keys on).
    function keyPressed(event) {
        if (clientPair.visible && event.matches(StandardKey.Paste)) {
            pasteCode();
            event.accepted = true;
        }
    }

    function clock(seconds) {
        return Math.floor(seconds / 60) + ":" + String(seconds % 60).padStart(2, "0");
    }

    onTabChanged: if (tab === 2) Farside.refreshTools()

    Connections {
        target: Farside

        function onPairFinished(problem) {
            remote.pairFailed = problem !== "";
            remote.pairMessage = problem !== "" ? problem : qsTr("Paired.");
            if (problem === "") {
                codeField.text = "";
                remote.tab = 0;
            }
        }
    }

    component Dot: Rectangle {
        width: 10
        height: 10
        radius: width / 2
    }

    component Card: Rectangle {
        radius: 14
        color: shell.card
        border.color: shell.line
    }

    component Headline: Card {
        Layout.fillWidth: true
        implicitHeight: Math.max(76, words.implicitHeight + 32)

        Dot {
            x: 20
            anchors.verticalCenter: parent.verticalCenter
            width: 12
            height: 12
            color: remote.dotColor()
        }
        Column {
            id: words
            x: 46
            width: parent.width - 66
            anchors.verticalCenter: parent.verticalCenter
            spacing: 2

            FText {
                text: Farside.state === "Stopped" ? qsTr("Not running") : Farside.state
                font.pixelSize: 17
                font.weight: Font.DemiBold
            }
            FText {
                width: parent.width
                text: remote.sentence()
                color: shell.muted
                wrapMode: Text.WordWrap
                visible: text !== ""
            }
            FText {
                width: parent.width
                text: Farside.detail
                color: shell.warn
                wrapMode: Text.WordWrap
                visible: text !== ""
            }
        }
    }

    Item {
        id: tabs
        width: parent.width
        height: 48

        Rectangle {
            anchors.bottom: parent.bottom
            width: parent.width
            height: 1
            color: shell.line
        }

        Row {
            x: 28
            height: parent.height
            spacing: 6

            Repeater {
                model: [shell.isClient ? qsTr("Status") : qsTr("Devices"), qsTr("Pair"), qsTr("Setup")]

                T.AbstractButton {
                    id: entry
                    required property string modelData
                    required property int index
                    readonly property bool current: remote.tab === index

                    height: parent.height
                    width: title.implicitWidth + 36
                    hoverEnabled: true
                    // A PC with no USB/IP driver has nobody to pair with yet.
                    enabled: index === 2 || Farside.role !== "none"
                    opacity: enabled ? 1 : 0.45
                    onClicked: remote.tab = index
                    Keys.onReturnPressed: clicked()

                    background: Rectangle {
                        color: entry.current ? shell.card : entry.hovered ? "#101018" : "transparent"

                        Rectangle {
                            anchors.bottom: parent.bottom
                            width: parent.width
                            height: 3
                            color: entry.current || entry.visualFocus ? shell.accent : "transparent"
                        }
                    }
                    contentItem: FText {
                        id: title
                        text: entry.modelData
                        color: entry.current ? shell.ink : shell.muted
                        font.weight: entry.current ? Font.DemiBold : Font.Normal
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                }
            }
        }
    }

    Item {
        id: body
        x: 28
        y: tabs.height + 24
        width: Math.min(parent.width - 56, 980)
        height: parent.height - y - 24

        // ---- Devices (Host) ----
        ColumnLayout {
            anchors.fill: parent
            spacing: 16
            visible: remote.tab === 0 && !shell.isClient

            Headline { }

            Card {
                Layout.fillWidth: true
                implicitHeight: list.implicitHeight
                visible: Farside.devices.length > 0

                Column {
                    id: list
                    width: parent.width

                    Repeater {
                        model: Farside.devices

                        Item {
                            id: row
                            required property var modelData
                            required property int index
                            property bool asking: false

                            width: list.width
                            height: 72

                            Rectangle {
                                anchors.bottom: parent.bottom
                                x: 1
                                width: parent.width - 2
                                height: 1
                                color: shell.line
                                visible: row.index < Farside.devices.length - 1
                            }
                            Dot {
                                x: 20
                                anchors.verticalCenter: parent.verticalCenter
                                color: row.modelData.linked ? shell.good : row.modelData.online ? shell.warn : shell.off
                            }
                            Column {
                                x: 44
                                width: parent.width - 44 - actions.width - 32
                                anchors.verticalCenter: parent.verticalCenter
                                spacing: 2

                                FText {
                                    width: parent.width
                                    text: remote.shown(row.modelData.name)
                                    font.pixelSize: 15
                                    font.weight: Font.DemiBold
                                    elide: Text.ElideRight
                                }
                                FText {
                                    width: parent.width
                                    text: row.asking ? qsTr("Unpair this PC? It must be paired again with a new code.") : remote.deviceText(row.modelData)
                                    color: row.asking ? shell.ink : shell.muted
                                    elide: Text.ElideRight
                                }
                            }
                            Row {
                                id: actions
                                anchors.right: parent.right
                                anchors.rightMargin: 20
                                anchors.verticalCenter: parent.verticalCenter
                                spacing: 10

                                FButton {
                                    text: qsTr("Cancel")
                                    visible: row.asking
                                    onClicked: row.asking = false
                                }
                                FButton {
                                    text: qsTr("Unpair")
                                    onClicked: {
                                        if (row.asking)
                                            Farside.unpair(row.modelData.id);
                                        row.asking = !row.asking;
                                    }
                                }
                            }
                        }
                    }
                }
            }

            FText {
                text: qsTr("No PC is paired with this one yet.")
                color: shell.muted
                visible: Farside.devices.length === 0 && Farside.role === "host"
            }

            Item { Layout.fillHeight: true }

            Rectangle {
                Layout.fillWidth: true
                implicitHeight: 1
                color: shell.line
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 12
                enabled: Farside.role === "host"
                opacity: enabled ? 1 : 0.45

                T.Switch {
                    id: keep
                    implicitWidth: 40
                    implicitHeight: 24
                    hoverEnabled: true
                    checked: Farside.keepLinked
                    onToggled: Farside.keepLinked = checked
                    Keys.onReturnPressed: toggle()

                    indicator: Rectangle {
                        width: 40
                        height: 24
                        radius: 12
                        color: keep.checked ? shell.accent : "#34344a"
                        border.width: keep.visualFocus ? 2 : 0
                        border.color: "#ffffff"

                        Rectangle {
                            x: keep.checked ? 19 : 3
                            y: 3
                            width: 18
                            height: 18
                            radius: 9
                            color: "#ffffff"

                            Behavior on x { NumberAnimation { duration: 120 } }
                        }
                    }
                }
                FText {
                    Layout.fillWidth: true
                    text: qsTr("Keep linked")
                }
                FButton {
                    text: qsTr("Unlink")
                    visible: Farside.state === "Linked"
                    onClicked: Farside.unlink()
                }
                FButton {
                    primary: true
                    text: qsTr("Pair a PC")
                    onClicked: remote.tab = 1
                }
            }
        }

        // ---- Status (Client) ----
        ColumnLayout {
            anchors.fill: parent
            spacing: 16
            visible: remote.tab === 0 && shell.isClient

            Headline { }

            Item { Layout.fillHeight: true }
        }

        // ---- Pair (Host): the code to enter on the other PC ----
        ColumnLayout {
            id: hostPair
            anchors.fill: parent
            spacing: 16
            visible: remote.tab === 1 && !shell.isClient

            // A code is on show for as long as this tab is; a used or run-out one is replaced.
            Timer {
                interval: 1500
                repeat: true
                triggeredOnStart: true
                running: hostPair.visible && Farside.code === "" && Farside.state !== "Stopped"
                onTriggered: Farside.newCode()
            }

            FText {
                Layout.fillWidth: true
                text: qsTr("On the other PC, open Farside Client and go to Pair. Paste a screenshot of this QR code there, or enter the code. It works once.")
                color: shell.muted
                wrapMode: Text.WordWrap
            }

            FCheck {
                text: qsTr("Direct connections: PCs on this network or VPN pair and link without the tunnel")
                checked: Farside.directEnabled
                onToggled: Farside.directEnabled = checked
            }
            FText {
                Layout.fillWidth: true
                visible: Farside.directEnabled
                text: qsTr("While this page is open, the other PC finds this one by itself (same network) or by this PC's address (VPN). Both then show the same number; accept only if they match. The first time, Windows asks whether Farside may accept connections: allow it.")
                color: shell.muted
                wrapMode: Text.WordWrap
            }

            Card {
                Layout.fillWidth: true
                implicitHeight: 280

                Rectangle {
                    id: qrPlate
                    x: 22
                    y: 22
                    width: 236
                    height: 236
                    radius: 12
                    color: Farside.code !== "" ? "#ffffff" : shell.side

                    Image {
                        anchors.centerIn: parent
                        width: 224
                        height: 224
                        visible: Farside.code !== ""
                        // Drawn at twice the size so it stays sharp when the page is scaled up.
                        sourceSize: Qt.size(448, 448)
                        source: Farside.code !== "" ? "image://farsideqr/" + encodeURIComponent(Farside.code) : ""
                    }
                    FText {
                        anchors.centerIn: parent
                        width: parent.width - 32
                        horizontalAlignment: Text.AlignHCenter
                        wrapMode: Text.WordWrap
                        text: Farside.state === "Stopped" ? qsTr("The controller link is not running.") : qsTr("Starting the tunnel\u2026")
                        color: shell.muted
                        visible: Farside.code === ""
                    }
                }

                Column {
                    x: qrPlate.x + qrPlate.width + 24
                    y: 22
                    width: parent.width - x - 22
                    spacing: 12

                    FText {
                        text: qsTr("Pairing code")
                        font.pixelSize: 15
                        font.weight: Font.DemiBold
                    }
                    TextEdit {
                        width: parent.width
                        readOnly: true
                        selectByMouse: true
                        wrapMode: TextEdit.WrapAnywhere
                        text: Farside.code
                        visible: text !== ""
                        color: shell.muted
                        selectionColor: shell.accent
                        selectedTextColor: "#ffffff"
                        font.family: "Consolas"
                        font.pixelSize: 13
                    }
                    FText {
                        text: qsTr("Valid for %1").arg(remote.clock(Farside.codeSecondsLeft))
                        visible: Farside.code !== ""
                    }
                }

                Row {
                    x: qrPlate.x + qrPlate.width + 24
                    y: parent.height - height - 22
                    spacing: 12

                    FButton {
                        primary: true
                        text: copiedImage.running ? qsTr("Copied") : qsTr("Copy image")
                        enabled: Farside.code !== ""
                        onClicked: {
                            Farside.copyQrImage();
                            copiedImage.restart();
                        }

                        Timer { id: copiedImage; interval: 1500 }
                    }
                    FButton {
                        text: copied.running ? qsTr("Copied") : qsTr("Copy code")
                        enabled: Farside.code !== ""
                        onClicked: {
                            Farside.copyText(Farside.code);
                            copied.restart();
                        }

                        Timer { id: copied; interval: 1500 }
                    }
                    FButton {
                        text: qsTr("New code")
                        enabled: Farside.state !== "Stopped"
                        onClicked: Farside.newCode()
                    }
                }
            }

            Item { Layout.fillHeight: true }
        }

        // ---- Pair (Client): give it the Host's code ----
        ColumnLayout {
            id: clientPair
            anchors.fill: parent
            spacing: 16
            visible: remote.tab === 1 && shell.isClient

            // Without the tunnel: Hosts on this network, or any Host by address (VPN).
            FText {
                text: qsTr("On this network")
                font.weight: Font.DemiBold
            }
            Repeater {
                model: Farside.foundHosts

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 12

                    FText {
                        Layout.fillWidth: true
                        text: modelData.name.replace(/_/g, " ") + "  \u00b7  " + modelData.address
                        elide: Text.ElideRight
                    }
                    FText {
                        visible: !modelData.pairing
                        text: qsTr("open Pair on that PC first")
                        color: shell.muted
                    }
                    FButton {
                        text: qsTr("Pair")
                        enabled: !Farside.pairing && modelData.pairing
                        onClicked: {
                            remote.pairMessage = "";
                            remote.pairByAddress = true;
                            Farside.pairDirect(modelData.address + ":" + modelData.port);
                        }
                    }
                }
            }
            RowLayout {
                Layout.fillWidth: true
                spacing: 12

                FField {
                    id: addressField
                    Layout.fillWidth: true
                    implicitHeight: 44
                    font.family: "Inter"
                    font.pixelSize: 14
                    enabled: !Farside.pairing
                    placeholderText: Farside.foundHosts.length === 0 ? qsTr("No Host found here. Or type its address, for example 192.168.1.20") : qsTr("Or type a Host's address, for example 192.168.1.20")
                    onAccepted: addressButton.clicked()
                }
                FButton {
                    id: addressButton
                    text: qsTr("Pair by address")
                    enabled: !Farside.pairing && addressField.text.trim() !== ""
                    onClicked: {
                        remote.pairMessage = "";
                        remote.pairByAddress = true;
                        Farside.pairDirect(addressField.text);
                    }
                }
            }
            Timer {
                interval: 2000
                repeat: true
                triggeredOnStart: true
                running: clientPair.visible
                onTriggered: Farside.findHosts()
            }

            FText {
                text: qsTr("From anywhere: enter the pairing code from Farside Host.")
                color: shell.muted
            }

            T.AbstractButton {
                id: drop
                Layout.fillWidth: true
                implicitHeight: 96
                hoverEnabled: true
                enabled: !Farside.pairing
                onClicked: remote.pasteCode()
                Keys.onReturnPressed: clicked()

                background: Canvas {
                    property color stroke: drop.hovered || drop.visualFocus ? shell.accent : "#3a3a50"
                    onStrokeChanged: requestPaint()
                    onWidthChanged: requestPaint()
                    onPaint: {
                        const c = getContext("2d");
                        c.reset();
                        c.strokeStyle = stroke;
                        c.lineWidth = 1;
                        c.setLineDash([5, 4]);
                        const r = 14, w = width - 1, h = height - 1;
                        c.beginPath();
                        c.moveTo(0.5 + r, 0.5);
                        c.arcTo(0.5 + w, 0.5, 0.5 + w, 0.5 + h, r);
                        c.arcTo(0.5 + w, 0.5 + h, 0.5, 0.5 + h, r);
                        c.arcTo(0.5, 0.5 + h, 0.5, 0.5, r);
                        c.arcTo(0.5, 0.5, 0.5 + w, 0.5, r);
                        c.closePath();
                        c.stroke();
                    }
                }
                contentItem: Item {
                    Column {
                        anchors.centerIn: parent
                        spacing: 6

                        FText {
                            anchors.horizontalCenter: parent.horizontalCenter
                            text: qsTr("Paste a screenshot of the QR code here")
                            font.weight: Font.DemiBold
                        }
                        FText {
                            anchors.horizontalCenter: parent.horizontalCenter
                            text: qsTr("Ctrl+V, or click. It is scanned at once, also when it is blurry.")
                            color: shell.muted
                        }
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 12

                T.TextField {
                    id: codeField
                    Layout.fillWidth: true
                    implicitHeight: 44
                    leftPadding: 14
                    rightPadding: 14
                    verticalAlignment: TextInput.AlignVCenter
                    color: shell.ink
                    selectionColor: shell.accent
                    selectedTextColor: "#ffffff"
                    selectByMouse: true
                    font.family: "Inter"
                    font.pixelSize: 14
                    enabled: !Farside.pairing
                    onAccepted: pairButton.clicked()
                    // A screenshot in the clipboard is not text, so pasting goes through the scanner.
                    Keys.onPressed: (event) => {
                        if (event.matches(StandardKey.Paste)) {
                            remote.pasteCode();
                            event.accepted = true;
                        }
                    }

                    background: Rectangle {
                        radius: 10
                        color: shell.side
                        border.color: codeField.activeFocus ? shell.accent : shell.edge
                    }

                    FText {
                        x: codeField.leftPadding
                        anchors.verticalCenter: parent.verticalCenter
                        text: "farside:1:\u2026"
                        color: shell.off
                        visible: codeField.text === "" && !codeField.activeFocus
                    }
                }
                FButton {
                    text: qsTr("Paste")
                    enabled: !Farside.pairing
                    onClicked: remote.pasteCode()
                }
                FButton {
                    text: qsTr("Scan screen")
                    enabled: !Farside.pairing
                    onClicked: remote.useCode(Farside.codeFromScreen(), qsTr("No Farside QR code was found on this PC's screens. Show the Host's Pair page here, for example in a screen share."))
                }
                FButton {
                    id: pairButton
                    primary: true
                    text: qsTr("Pair")
                    enabled: !Farside.pairing && codeField.text.trim() !== ""
                    onClicked: {
                        remote.pairMessage = "";
                        remote.pairByAddress = false;
                        Farside.pair(codeField.text);
                    }
                }
            }

            FText {
                Layout.fillWidth: true
                text: Farside.pairing ? (Farside.pairCheck !== "" ? qsTr("Number: %1. If the Host shows the same number, click Accept there.").arg(Farside.pairCheck) : remote.pairByAddress ? qsTr("Connecting to the Host.") : qsTr("Waiting for the Host. Click Accept there.")) : remote.pairMessage
                color: Farside.pairing ? shell.muted : remote.pairFailed ? shell.warn : shell.good
                wrapMode: Text.WordWrap
                visible: text !== ""
            }

            Item { Layout.fillHeight: true }

            Rectangle {
                Layout.fillWidth: true
                implicitHeight: 1
                color: shell.line
            }

            RowLayout {
                id: pairedRow
                property bool asking: false

                Layout.fillWidth: true
                spacing: 12

                Dot { color: Farside.peerName === "" ? shell.off : remote.dotColor() }
                FText {
                    Layout.fillWidth: true
                    text: pairedRow.asking ? qsTr("Unpair from %1? You need a new code to pair again.").arg(remote.shown(Farside.peerName))
                        : Farside.peerName === "" ? qsTr("Not paired")
                        : qsTr("Paired with %1 \u00B7 %2").arg(remote.shown(Farside.peerName)).arg(Farside.state)
                    elide: Text.ElideRight
                }
                FButton {
                    text: qsTr("Cancel")
                    visible: pairedRow.asking
                    onClicked: pairedRow.asking = false
                }
                FButton {
                    text: qsTr("Unpair")
                    visible: Farside.peerName !== ""
                    onClicked: {
                        if (pairedRow.asking)
                            Farside.unpair();
                        pairedRow.asking = !pairedRow.asking;
                    }
                }
            }
        }

        // ---- Setup: what this PC needs ----
        ColumnLayout {
            anchors.fill: parent
            spacing: 16
            visible: remote.tab === 2

            FText {
                Layout.fillWidth: true
                text: shell.isClient ? qsTr("This PC shares its controller. It needs these two programs.")
                                     : qsTr("This PC receives a controller. It needs these two programs.")
                color: shell.muted
                wrapMode: Text.WordWrap
            }

            Card {
                Layout.fillWidth: true
                implicitHeight: needs.implicitHeight

                Column {
                    id: needs
                    width: parent.width

                    Repeater {
                        model: [
                            { name: "usbip-win2", found: Farside.tools.usbip, shown: !shell.isClient,
                              text: qsTr("Attaches the other PC's controller to this PC.") },
                            { name: "usbipd-win", found: Farside.tools.usbipd, shown: shell.isClient,
                              text: qsTr("Shares this PC's controller.") },
                            { name: "dumbpipe", found: Farside.tools.tunnel, shown: true,
                              text: qsTr("The tunnel between the PCs. It comes with Farside.") }
                        ]

                        Item {
                            id: need
                            required property var modelData

                            width: needs.width
                            height: visible ? 64 : 0
                            visible: modelData.shown

                            Dot {
                                x: 20
                                anchors.verticalCenter: parent.verticalCenter
                                color: need.modelData.found ? shell.good : shell.bad
                            }
                            Column {
                                x: 44
                                width: parent.width - 64
                                anchors.verticalCenter: parent.verticalCenter
                                spacing: 2

                                FText {
                                    text: need.modelData.name + " · " + (need.modelData.found ? qsTr("found") : qsTr("not found"))
                                    font.pixelSize: 15
                                    font.weight: Font.DemiBold
                                }
                                FText {
                                    width: parent.width
                                    text: need.modelData.text
                                    color: shell.muted
                                    elide: Text.ElideRight
                                }
                            }
                        }
                    }
                }
            }

            FText {
                Layout.fillWidth: true
                visible: !shell.isClient
                text: qsTr("usbip-win2 is installed on its own: github.com/vadimgrn/usbip-win2. Restart Farside Host afterwards.")
                color: shell.muted
                wrapMode: Text.WordWrap
            }

            // Client: the two things Setup did, to do again by hand (another controller, another USB port).
            FText {
                Layout.fillWidth: true
                visible: shell.isClient
                text: Farside.tools.usbipd
                    ? qsTr("To share the controller: plug the DualSense in with a USB cable, then click Share controller. Windows asks for permission. While it is shared, Windows on this PC cannot use the controller itself.")
                    : qsTr("usbipd-win is missing. Run the Farside Client Setup program again; it installs it.")
                color: shell.muted
                wrapMode: Text.WordWrap
            }
            RowLayout {
                visible: shell.isClient
                spacing: 12

                FButton {
                    primary: true
                    enabled: Farside.tools.usbipd
                    text: qsTr("Share controller")
                    onClicked: Farside.shareController()
                }
                FButton { text: qsTr("Open log folder"); onClicked: Farside.openFolder(Farside.logFolder()) }
                FCheck {
                    text: qsTr("Log: write controller link events to a file")
                    checked: Farside.logEnabled
                    onToggled: Farside.logEnabled = checked
                }
            }
            RowLayout {
                visible: shell.isClient
                spacing: 12

                FText { text: qsTr("Updates") }
                FCombo {
                    implicitWidth: 320
                    model: [qsTr("Stable"), qsTr("Alpha: early versions, may break")]
                    currentIndex: Farside.updateChannel === "alpha" ? 1 : 0
                    onActivated: (index) => Farside.updateChannel = index === 1 ? "alpha" : "stable"
                }
            }

            Item { Layout.fillHeight: true }
        }
    }
}
