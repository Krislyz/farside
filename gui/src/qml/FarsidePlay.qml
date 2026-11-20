// Play page of a Host: the consoles to play on, in the look of the Farside boards.
// It replaces chiaki-ng's MainView.qml and keeps what that did: the same console list (Chiaki.hosts),
// the same actions, the same keys and controller buttons, the same questions on first start.
import QtQuick
import QtQuick.Layouts

import org.streetpea.chiaking

Item {
    id: play

    // Row of Chiaki.hosts that the keys and controller buttons act on.
    property int current: 0

    // Called by FarsideShell when this page comes to the front.
    function activated() {
        forceActiveFocus(Qt.TabFocusReason);
        if (!Chiaki.autoConnect && !root.initialAsk && !Chiaki.window.directStream) {
            root.initialAsk = true;
            // chiaki-ng also asks here about a Steam shortcut; Farside leaves that to F1.
            if (Chiaki.settings.remotePlayAsk) {
                if (!Chiaki.settings.psnRefreshToken || !Chiaki.settings.psnAuthToken || !Chiaki.settings.psnAuthTokenExpiry || !Chiaki.settings.psnAccountId)
                    root.showRemindDialog(qsTr("Remote Play via PSN"), qsTr("Would you like to connect to PSN?\nThis enables:\n- Automatic registration\n- Playing outside of your home network without port forwarding?") + "\n\n" + qsTr("(Note: If you select no now and want to do this later, go to the Config section of the settings.)"), true, () => root.showPSNTokenDialog(false));
                else
                    Chiaki.settings.remotePlayAsk = false;
            }
        }
    }

    function shownCount() {
        let n = 0;
        for (const host of Chiaki.hosts)
            if (host.display)
                n++;
        return n;
    }

    // Moves to the next console that is shown, in the given direction, wrapping round.
    function step(by) {
        const hosts = Chiaki.hosts;
        for (let i = 1; i <= hosts.length; i++) {
            const at = (current + by * i + hosts.length * i) % hosts.length;
            if (hosts[at].display) {
                current = at;
                consoles.positionViewAtIndex(at, ListView.Contain);
                return;
            }
        }
    }

    function connectTo(index) {
        const host = Chiaki.hosts[index];
        if (!host)
            return;
        if (host.discovered)
            Chiaki.connectToHost(index, host.name);
        else
            Chiaki.connectToHost(index);
    }

    function wake(index) {
        const host = Chiaki.hosts[index];
        if (host && !host.discovered && !host.duid)
            Chiaki.wakeUpHost(index);
    }

    function remove(index) {
        const host = Chiaki.hosts[index];
        if (!host)
            return;
        if (host.manual)
            root.showConfirmDialog(qsTr("Delete Console"), qsTr("Are you sure you want to delete this console?"), () => Chiaki.deleteHost(index));
        else if (host.discovered && !host.registered)
            root.showConfirmDialog(qsTr("Hide Console"), qsTr("Are you sure you want to hide this console?") + "\n\n" + qsTr("Note: You can unhide from the Consoles section of the Settings under Hidden Consoles"), () => Chiaki.hideHost(host.mac, host.name));
    }

    function stateText(host) {
        if (host.duid && !host.discovered)
            return qsTr("Remote connection via PSN");
        const state = host.state === "ready" ? qsTr("Ready") : host.state === "standby" ? qsTr("Rest mode") : qsTr("Not seen on this network");
        return state + " · " + (host.registered ? qsTr("registered") : qsTr("not registered"));
    }

    function controllerText() {
        if (Farside.state === "Linked")
            return qsTr("%1, linked from %2").arg(Farside.controller ? Farside.controller.replace(/_/g, " ") : qsTr("Controller")).arg(Farside.peerName.replace(/_/g, " "));
        const n = Chiaki.controllers.length;
        return n === 0 ? qsTr("None. Link one under Remote Play, or plug one into this PC.")
             : n === 1 ? qsTr("1 controller on this PC") : qsTr("%1 controllers on this PC").arg(n);
    }

    Connections {
        target: Chiaki

        function onHostsChanged() {
            const hosts = Chiaki.hosts;
            if (!hosts[play.current] || !hosts[play.current].display)
                play.step(1);
        }
    }

    Keys.onUpPressed: step(-1)
    Keys.onDownPressed: step(1)
    Keys.onMenuPressed: root.showSettingsDialog()
    Keys.onReturnPressed: connectTo(current)
    Keys.onYesPressed: wake(current)
    Keys.onNoPressed: remove(current)
    Keys.onEscapePressed: root.showConfirmDialog(qsTr("Quit"), qsTr("Are you sure you want to quit?"), () => Qt.quit())
    Keys.onPressed: (event) => {
        if (event.modifiers)
            return;
        switch (event.key) {
        case Qt.Key_PageUp:
            if (Chiaki.hosts[current] && Chiaki.hosts[current].registered)
                root.showConsolePinDialog(current);
            event.accepted = true;
            break;
        case Qt.Key_PageDown:
            if (Chiaki.settings.psnAuthToken)
                Chiaki.refreshPsnToken();
            event.accepted = true;
            break;
        case Qt.Key_F1:
            if (typeof Chiaki.createSteamShortcut === "function")
                root.showSteamShortcutDialog(false);
            event.accepted = true;
            break;
        case Qt.Key_F2:
            root.showManualHostDialog();
            event.accepted = true;
            break;
        }
    }

    ColumnLayout {
        x: 28
        y: 28
        width: Math.min(parent.width - 56, 980)
        height: parent.height - 52
        spacing: 16

        FText {
            text: qsTr("Play")
            font.family: "Space Grotesk"
            font.pixelSize: 28
            font.weight: Font.DemiBold
        }
        FText {
            text: qsTr("The PS5 picture takes over this window while you play.")
            color: shell.muted
        }

        ListView {
            id: consoles
            Layout.fillWidth: true
            Layout.preferredHeight: Math.min(contentHeight, 3 * 144)
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            model: Chiaki.hosts
            visible: play.shownCount() > 0

            delegate: Item {
                id: row
                required property var modelData
                required property int index
                readonly property bool chosen: play.current === index && play.activeFocus

                width: consoles.width
                height: modelData.display ? 144 : 0
                visible: modelData.display

                Rectangle {
                    width: parent.width
                    height: 132
                    radius: 14
                    color: shell.card
                    border.color: row.chosen ? shell.accent : shell.line

                    MouseArea {
                        anchors.fill: parent
                        onClicked: {
                            play.current = row.index;
                            play.forceActiveFocus();
                        }
                        onDoubleClicked: play.connectTo(row.index)
                    }

                    Rectangle {
                        x: 22
                        anchors.verticalCenter: parent.verticalCenter
                        width: 88
                        height: 88
                        radius: 12
                        color: shell.side

                        Image {
                            anchors.centerIn: parent
                            width: 64
                            height: 64
                            // Asked for at twice the size so it stays sharp when the page is scaled up.
                            sourceSize: Qt.size(128, 128)
                            fillMode: Image.PreserveAspectFit
                            source: "image://svg/console-ps" + (row.modelData.ps5 ? "5" : "4") + (row.modelData.state === "standby" ? "#light_standby" : "#light_on")
                        }
                    }

                    Column {
                        x: 130
                        width: parent.width - 130 - actions.width - 44
                        anchors.verticalCenter: parent.verticalCenter
                        spacing: 4

                        FText {
                            width: parent.width
                            text: row.modelData.name ? row.modelData.name : qsTr("Console")
                            font.pixelSize: 20
                            font.weight: Font.DemiBold
                            elide: Text.ElideRight
                        }
                        Row {
                            spacing: 8

                            Rectangle {
                                anchors.verticalCenter: parent.verticalCenter
                                width: 10
                                height: 10
                                radius: 5
                                color: row.modelData.state === "ready" ? shell.good : row.modelData.state === "standby" ? shell.warn : shell.off
                            }
                            FText {
                                text: play.stateText(row.modelData)
                                color: shell.muted
                            }
                        }
                        FText {
                            width: parent.width
                            text: row.modelData.app ? qsTr("Running: %1").arg(row.modelData.app)
                                : row.modelData.address && !Chiaki.settings.streamerMode ? row.modelData.address : ""
                            color: shell.off
                            font.pixelSize: 13
                            elide: Text.ElideRight
                            visible: text !== ""
                        }
                    }

                    Row {
                        id: actions
                        anchors.right: parent.right
                        anchors.rightMargin: 22
                        anchors.verticalCenter: parent.verticalCenter
                        spacing: 10

                        FButton {
                            text: row.modelData.manual ? qsTr("Delete") : qsTr("Hide")
                            visible: row.modelData.manual || (row.modelData.discovered && !row.modelData.registered)
                            focusPolicy: Qt.NoFocus
                            onClicked: play.remove(row.index)
                        }
                        FButton {
                            text: qsTr("Wake up")
                            visible: row.modelData.registered && !row.modelData.duid && !row.modelData.discovered
                            focusPolicy: Qt.NoFocus
                            onClicked: play.wake(row.index)
                        }
                        FButton {
                            text: qsTr("Console PIN")
                            visible: row.modelData.registered
                            focusPolicy: Qt.NoFocus
                            onClicked: root.showConsolePinDialog(row.index)
                        }
                        FButton {
                            primary: true
                            implicitHeight: 48
                            font.pixelSize: 15
                            text: row.modelData.registered || row.modelData.duid ? qsTr("Play") : qsTr("Register")
                            focusPolicy: Qt.NoFocus
                            onClicked: play.connectTo(row.index)
                        }
                    }
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            implicitHeight: 76
            radius: 14
            color: shell.card
            border.color: shell.line
            visible: play.shownCount() === 0

            FText {
                x: 22
                width: parent.width - 44
                anchors.verticalCenter: parent.verticalCenter
                text: Chiaki.discoveryEnabled ? qsTr("Looking for a console on this network. Turn the PS5 on, or add it by address.")
                                              : qsTr("The search is off. Turn it on below, or add a console by address.")
                color: shell.muted
                wrapMode: Text.WordWrap
            }
        }

        Rectangle {
            Layout.fillWidth: true
            implicitHeight: 72
            radius: 14
            color: shell.card
            border.color: shell.line

            Rectangle {
                x: 22
                anchors.verticalCenter: parent.verticalCenter
                width: 10
                height: 10
                radius: 5
                color: Farside.state === "Linked" || Chiaki.controllers.length > 0 ? shell.good : shell.off
            }
            Column {
                x: 46
                width: parent.width - 68
                anchors.verticalCenter: parent.verticalCenter
                spacing: 2

                FText {
                    text: qsTr("Controller")
                    font.weight: Font.DemiBold
                }
                FText {
                    width: parent.width
                    text: play.controllerText()
                    color: shell.muted
                    elide: Text.ElideRight
                }
            }
        }

        FText {
            Layout.fillWidth: true
            text: qsTr("During a stream: F11 switches fullscreen, and the bar at the top edge holds quality, resolution, frame rate and bitrate.")
            color: shell.muted
            wrapMode: Text.WordWrap
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

            FCheck {
                text: qsTr("Search this network")
                checked: Chiaki.discoveryEnabled
                focusPolicy: Qt.NoFocus
                onToggled: Chiaki.discoveryEnabled = checked
            }
            FText {
                Layout.fillWidth: true
                text: qsTr("Built on chiaki-ng %1").arg(Qt.application.version)
                color: shell.off
                font.pixelSize: 12
                horizontalAlignment: Text.AlignRight
                elide: Text.ElideLeft
            }
            FButton {
                text: qsTr("Refresh PSN")
                visible: !!Chiaki.settings.psnAuthToken
                focusPolicy: Qt.NoFocus
                onClicked: Chiaki.refreshPsnToken()
            }
            FButton {
                text: qsTr("Add console")
                focusPolicy: Qt.NoFocus
                onClicked: root.showManualHostDialog()
            }
            FButton {
                text: qsTr("Stream settings")
                focusPolicy: Qt.NoFocus
                onClicked: root.showSettingsDialog()
            }
        }
    }
}
