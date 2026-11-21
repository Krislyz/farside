import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Window
import QtCore

import org.streetpea.chiaking

import "controls" as C

Item {
    id: view

    readonly property var hostWindow: view.Window.window
    property bool sessionError: false
    property bool sessionLoading: true
    property list<Item> restoreFocusItems
    readonly property bool useSeparateMenuWindow: Chiaki.window.runtimeRendererBackend === 1
    readonly property int streamMenuHeight: 200
    readonly property bool streamStatsVisible: Chiaki.settings.showStreamStats && Chiaki.session && !(menuController.open || menuController.closing) && !sessionLoading && !sessionError && !(Chiaki.settings.audioVideoDisabled & 0x02)
    property int separateMenuX: 0
    property int separateMenuY: 0
    property int separateMenuWidth: 0
    property int separateStatsX: 0
    property int separateStatsY: 0
    property int separateStatsWidth: 0
    property int separateStatsHeight: 0
    property int separateDialogX: 0
    property int separateDialogY: 0
    property bool sessionStopDialogActive: false
    property bool sessionPinDialogActive: false

    function grabInput(item) {
        Chiaki.window.grabInput();
        restoreFocusItems.push(hostWindow ? hostWindow.activeFocusItem : null);
        if (item)
            item.forceActiveFocus(Qt.TabFocusReason);
    }

    function releaseInput() {
        Chiaki.window.releaseInput();
        let item = restoreFocusItems.pop();
        if (item && item.visible)
            item.forceActiveFocus(Qt.TabFocusReason);
    }

    function updateSeparateMenuGeometry() {
        if (!hostWindow)
            return;
        const topLeft = view.mapToGlobal(0, 0);
        if (useSeparateMenuWindow) {
            separateMenuX = Math.round(topLeft.x);
            separateMenuY = Math.round(topLeft.y + view.height - streamMenuHeight);
            separateMenuWidth = Math.round(view.width);
            separateStatsX = Math.round(topLeft.x);
            separateStatsY = Math.round(topLeft.y);
            separateStatsWidth = Math.round(view.width);
            separateStatsHeight = Math.round(view.height);
        }
    }

    function updateSeparateDialogGeometry(width, height) {
        if (!useSeparateMenuWindow || !hostWindow)
            return;
        separateDialogX = Math.round(hostWindow.x + (hostWindow.width - width) / 2);
        separateDialogY = Math.round(hostWindow.y + (hostWindow.height - height) / 2);
    }

    function updateOverlayInteractionActive() {
        Chiaki.window.setOverlayInteractionActive(
            Chiaki.window.barHot ||
            menuController.open ||
            menuController.closing ||
            sessionStopDialogActive ||
            separateSessionStopWindow.visible ||
            sessionPinDialogActive ||
            separateSessionPinWindow.visible
        );
    }

    StackView.onActivating: {
        Chiaki.window.keepVideo = true;
        sessionError = false;
        errorTitleLabel.text = "";
        errorTextLabel.text = "";
        sessionLoading = !(Chiaki.window.loadingTransitionComplete || (Chiaki.settings.audioVideoDisabled & 0x02));
    }
    StackView.onDeactivated: Chiaki.window.keepVideo = false

    Component.onCompleted: {
        updateSeparateMenuGeometry();
        updateOverlayInteractionActive();
        Chiaki.window.setStatsOverlayActive(streamStatsVisible);
    }
    onStreamStatsVisibleChanged: {
        if (Chiaki.window)
            Chiaki.window.setStatsOverlayActive(streamStatsVisible);
    }
    onWidthChanged: updateSeparateMenuGeometry()
    onHeightChanged: updateSeparateMenuGeometry()
    onUseSeparateMenuWindowChanged: updateSeparateMenuGeometry()

    Connections {
        target: view.hostWindow
        function onXChanged() { view.updateSeparateMenuGeometry() }
        function onYChanged() { view.updateSeparateMenuGeometry() }
        function onWidthChanged() { view.updateSeparateMenuGeometry() }
        function onHeightChanged() { view.updateSeparateMenuGeometry() }
        function onVisibilityChanged() { view.updateSeparateMenuGeometry() }
    }

    QtObject {
        id: menuController
        property bool closing: false
        property bool open: false

        property real lastToggleTime: 0
        function toggle() {
            var now = Date.now();
            if (now - lastToggleTime < 200)
                return;
            lastToggleTime = now;
            if (open)
                close();
            else {
                if (useSeparateMenuWindow) {
                    view.updateSeparateMenuGeometry();
                    view.grabInput(null);
                }
                open = true;
            }
            view.updateOverlayInteractionActive();
        }

        function close() {
            if (!open || closing)
                return;
            closing = true;
            open = false;
            view.releaseInput();
            view.updateOverlayInteractionActive();
        }
    }

    Rectangle {
        id: loadingView
        anchors.fill: parent
        color: "black"
        opacity: sessionError || sessionLoading || (Chiaki.settings.audioVideoDisabled & 0x02) ? 1.0 : 0.0
        visible: opacity

        Behavior on opacity { NumberAnimation { duration: 250 } }

        Item {
            anchors {
                top: parent.verticalCenter
                left: parent.left
                right: parent.right
                bottom: parent.bottom
            }

            BusyIndicator {
                id: spinner
                anchors.centerIn: parent
                width: 70
                height: width
                visible: sessionLoading
                running: sessionLoading
            }

            Label {
                anchors {
                    top: spinner.bottom
                    horizontalCenter: spinner.horizontalCenter
                    topMargin: 30
                }
                horizontalAlignment: Text.AlignHCenter
                text: {
                    if(Chiaki.settings.dpadTouchEnabled)
                    {
                        if(Chiaki.settings.audioVideoDisabled == 0x01)
                            qsTr("Audio Disabled in settings\n") + qsTr("Press %1 to open stream menu").arg(Chiaki.controllers.length ? Chiaki.settings.stringForStreamMenuShortcut() : "Ctrl+O") + "\n" + qsTr("Press %1 to toggle between regular dpad and dpad touch").arg(Chiaki.settings.stringForDpadShortcut())
                        else
                            qsTr("Press %1 to open stream menu").arg(Chiaki.controllers.length ? Chiaki.settings.stringForStreamMenuShortcut() : "Ctrl+O") + "\n" + qsTr("Press %1 to toggle between regular dpad and dpad touch").arg(Chiaki.settings.stringForDpadShortcut())
                    }
                    else
                    {
                        if(Chiaki.settings.audioVideoDisabled == 0x01)
                            qsTr("Audio Disabled in settings\n") + qsTr("Press %1 to open stream menu").arg(Chiaki.controllers.length ? Chiaki.settings.stringForStreamMenuShortcut() : "Ctrl+O")
                        else
                            qsTr("Press %1 to open stream menu").arg(Chiaki.controllers.length ? Chiaki.settings.stringForStreamMenuShortcut() : "Ctrl+O")
                    }
                }
                visible: sessionLoading
            }

            Label {
                id: audioVideoDisabledTitleLabel
                anchors {
                    bottom: spinner.top
                    horizontalCenter: spinner.horizontalCenter
                }
                text: (Chiaki.settings.audioVideoDisabled & 0x01) ? qsTr("Audio and Video Disabled") : qsTr("Video Disabled")
                font.pixelSize: 24
                visible: !sessionLoading && !sessionError && (Chiaki.settings.audioVideoDisabled & 0x02)
            }

            Label {
                id: audioVideoDisabledTextLabel
                anchors {
                    top: audioVideoDisabledTitleLabel.bottom
                    horizontalCenter: audioVideoDisabledTitleLabel.horizontalCenter
                    topMargin: 10
                }
                horizontalAlignment: Text.AlignHCenter
                font.pixelSize: 20
                text: (Chiaki.settings.audioVideoDisabled & 0x01) ? qsTr("You have disabled audio and video in your settings.\nTo re-enable change Audio/Video to Audio and Video Enabled in the General tab of the settings.") : qsTr("You have disabled video in your settings.\nTo re-enable change Audio/Video to Audio and Video Enabled in the General tab of the settings.")
                visible: !sessionLoading && !sessionError && (Chiaki.settings.audioVideoDisabled & 0x02)
            }

            Label {
                id: errorTitleLabel
                anchors {
                    bottom: spinner.top
                    horizontalCenter: spinner.horizontalCenter
                }
                font.pixelSize: 24
                visible: text
                onVisibleChanged: if (visible) view.grabInput(errorTitleLabel)
                Keys.onReturnPressed: root.showMainView()
                Keys.onEscapePressed: root.showMainView()
            }

            Label {
                id: errorTextLabel
                anchors {
                    top: errorTitleLabel.bottom
                    horizontalCenter: errorTitleLabel.horizontalCenter
                    topMargin: 10
                }
                horizontalAlignment: Text.AlignHCenter
                font.pixelSize: 20
                visible: text
            }
        }
    }

    ColumnLayout {
        id: cantDisplayMessage
        anchors.centerIn: parent
        opacity: Chiaki.window.hasVideo && Chiaki.session && Chiaki.session.cantDisplay ? 1.0 : 0.0
        visible: opacity
        spacing: 30

        Behavior on opacity { NumberAnimation { duration: 250 } }

        onVisibleChanged: {
            if (visible) {
                menuController.close();
                view.grabInput(goToHomeButton);
            } else {
                view.releaseInput();
            }
        }

        Label {
            Layout.alignment: Qt.AlignCenter
            text: qsTr("The screen contains content that can't be displayed using Remote Play.")
        }

        Button {
            id: goToHomeButton
            Layout.alignment: Qt.AlignCenter
            Layout.preferredHeight: 60
            text: qsTr("Go to Home Screen")
            // Farside look.
            background: FBack { }
            onClicked: Chiaki.sessionGoHome()
            Keys.onReturnPressed: clicked()
            Keys.onEscapePressed: clicked()
        }
    }

    RoundButton {
        anchors {
            right: parent.right
            top: parent.top
            margins: 40
        }
        icon.source: "qrc:/icons/discover-off-24px.svg"
        icon.width: 50
        icon.height: 50
        padding: 20
        checked: true
        opacity: networkIndicatorTimer.running ? 0.7 : 0.0
        visible: opacity
        Material.background: Material.accent

        Behavior on opacity { NumberAnimation { duration: 400 } }

        Timer {
            id: networkIndicatorTimer
            running: Chiaki.session?.averagePacketLoss > (Chiaki.settings.wifiDroppedNotif * 0.01)
            interval: 400
        }
    }

    // ---- Farside: floating bar, modelled on capstream ----
    // Collapsed it is a small tab with an arrow, shown only while the mouse is at the top edge.
    // Opened it holds the stream settings and Fullscreen. F11 toggles fullscreen at any time.
    // Resolution, frame rate and bitrate are fixed when a session starts (PS5 Remote Play works that way),
    // so a change here is stored and takes effect at the next connect.
    Connections {
        target: Chiaki.window
        function onBarHotChanged() {
            view.updateOverlayInteractionActive();
        }
        function onBarOpenChanged() { Chiaki.window.requestOverlayUpdate() }
    }

    Item {
        id: farsideBar
        readonly property bool open: Chiaki.window.barOpen
        readonly property color accent: "#d4267e"
        readonly property var resolutionNames: ["640x360 (360p)", "960x540 (540p)", "1280x720 (720p)", "1920x1080 (1080p)"]
        // What chiaki-ng itself uses when the bitrate is left on automatic, in Mbps per resolution.
        readonly property var autoBitrate: [2, 6, 10, 15]
        // ponytail: three fixed presets as [resolution, fps, kbps]. Tune these numbers after real use;
        // the single controls beside them allow any other combination.
        readonly property var presets: [[3, 60, 8000], [4, 60, 15000], [4, 60, 30000]]
        readonly property var presetNames: [qsTr("Smooth"), qsTr("Balanced"), qsTr("Quality")]
        readonly property int currentPreset: {
            for (var i = 0; i < presets.length; i++)
                if (Chiaki.settings.resolutionLocalPS5 === presets[i][0] && Chiaki.settings.fpsLocalPS5 === presets[i][1] && Chiaki.settings.bitrateLocalPS5 === presets[i][2])
                    return i;
            return -1;
        }
        property bool changed: false

        // Farside options, kept between sessions (stored with chiaki-ng's own settings, under "farside").
        Settings {
            id: farsideOptions
            category: "farside"
            // HUD options: off unless ticked on the Settings tab of the Farside app, which writes this value.
            property bool hudResponse: false
        }

        // The local and the remote profile are kept the same: this window is always used on the PS5's own network.
        function store(resolution, fps, kbps) {
            Chiaki.settings.resolutionLocalPS5 = resolution; Chiaki.settings.resolutionRemotePS5 = resolution;
            Chiaki.settings.fpsLocalPS5 = fps; Chiaki.settings.fpsRemotePS5 = fps;
            Chiaki.settings.bitrateLocalPS5 = kbps; Chiaki.settings.bitrateRemotePS5 = kbps;
            changed = true;
            lowestLatency();
            Chiaki.window.requestOverlayUpdate();
        }

        // Whatever is picked, response time comes first: vertical sync stays off. How the picture is
        // enlarged (the render preset) is the user's choice under Settings, Picture.
        function lowestLatency() {
            Chiaki.settings.vSyncEnabled = false;
        }
        Component.onCompleted: lowestLatency()

        // What the chosen bitrate costs in response time. Bigger frames take longer to send and to decode.
        // ponytail: fixed steps by Mbps; replace with a measured delay if chiaki-ng ever exposes one.
        readonly property string latencyHint: {
            var mbps = Chiaki.settings.bitrateLocalPS5 > 0 ? Chiaki.settings.bitrateLocalPS5 / 1000 : autoBitrate[Chiaki.settings.resolutionLocalPS5 - 1];
            return mbps <= 10 ? qsTr("fastest") : mbps <= 20 ? qsTr("fast") : mbps <= 50 ? qsTr("slower") : qsTr("slowest");
        }

        // Shown while the mouse is at the top edge. It fades and slides in and out, and grows and
        // shrinks between tab and bar; every step is short (160 to 220 ms) so it never feels slow.
        readonly property bool shown: Chiaki.window.barHot && Chiaki.window.hasVideo && !menuController.open && !sessionLoading && !sessionError
        opacity: shown ? 1 : 0
        visible: opacity > 0
        anchors.top: parent.top
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.topMargin: (open ? 12 : 0) - (shown ? 0 : 14)
        width: open ? barRow.implicitWidth + 28 : 72
        height: open ? 56 : 24
        z: 10
        clip: true

        Behavior on opacity { NumberAnimation { duration: 160; easing.type: Easing.OutCubic } }
        Behavior on anchors.topMargin { NumberAnimation { duration: 200; easing.type: Easing.OutCubic } }
        Behavior on width { NumberAnimation { duration: 220; easing.type: Easing.OutCubic } }
        Behavior on height { NumberAnimation { duration: 220; easing.type: Easing.OutCubic } }

        // While something moves, the window redraws the bar on every picture; a still bar costs nothing.
        onOpacityChanged: Chiaki.window.requestOverlayUpdate()
        onWidthChanged: Chiaki.window.requestOverlayUpdate()
        onYChanged: Chiaki.window.requestOverlayUpdate()

        Rectangle {
            anchors.fill: parent
            radius: farsideBar.open ? 16 : 8
            color: "#e60b0b10"
            border.color: tabArea.containsMouse && !farsideBar.open ? farsideBar.accent : "#33334a"
            border.width: 1

            Behavior on radius { NumberAnimation { duration: 220; easing.type: Easing.OutCubic } }
            Behavior on border.color { ColorAnimation { duration: 120 } }
        }

        MouseArea {
            id: tabArea
            anchors.fill: parent
            enabled: !farsideBar.open
            hoverEnabled: true
            onClicked: Chiaki.window.barOpen = true
        }

        Label {
            opacity: farsideBar.open ? 0 : 1
            visible: opacity > 0
            Behavior on opacity { NumberAnimation { duration: 120 } }
            anchors.centerIn: parent
            text: "▾"
            color: "#d6d6e0"
            font.pixelSize: 16
        }

        RowLayout {
            id: barRow
            // The controls come in a moment after the bar has started to grow, and leave at once.
            opacity: farsideBar.open ? 1 : 0
            visible: opacity > 0
            Behavior on opacity {
                SequentialAnimation {
                    PauseAnimation { duration: farsideBar.open ? 90 : 0 }
                    NumberAnimation { duration: farsideBar.open ? 160 : 60; easing.type: Easing.OutCubic }
                }
            }
            anchors.centerIn: parent
            spacing: 8

            Label {
                text: "FARSIDE"
                color: farsideBar.accent
                font.pixelSize: 15
                font.bold: true
                font.letterSpacing: 2
                Layout.rightMargin: 6
            }

            // Pickers, as in Parsec's settings. While a list is open the window takes the mouse away from the
            // PS5 (grabInput); otherwise a click on the list, which hangs below the bar, would go to the console.
            C.ComboBox {
                id: priorityPicker
                property bool listOpen: popup.visible
                onListOpenChanged: { if (listOpen) view.grabInput(null); else view.releaseInput(); Chiaki.window.requestOverlayUpdate() }
                model: [farsideBar.presetNames[0], farsideBar.presetNames[1], farsideBar.presetNames[2], qsTr("Custom")]
                currentIndex: farsideBar.currentPreset >= 0 ? farsideBar.currentPreset : 3
                implicitWidth: 130
                implicitHeight: 40
                font.pixelSize: 13
                focusPolicy: Qt.NoFocus
                Material.accent: farsideBar.accent
                onActivated: (index) => { if (index < 3) farsideBar.store(farsideBar.presets[index][0], farsideBar.presets[index][1], farsideBar.presets[index][2]) }
            }

            C.ComboBox {
                property bool listOpen: popup.visible
                onListOpenChanged: { if (listOpen) view.grabInput(null); else view.releaseInput(); Chiaki.window.requestOverlayUpdate() }
                model: farsideBar.resolutionNames
                currentIndex: Chiaki.settings.resolutionLocalPS5 - 1
                implicitWidth: 200
                implicitHeight: 40
                font.pixelSize: 13
                focusPolicy: Qt.NoFocus
                Material.accent: farsideBar.accent
                // A bitrate the user chose stays when the resolution changes. On automatic (0) the
                // bitrate follows the new resolution by itself.
                onActivated: (index) => farsideBar.store(index + 1, Chiaki.settings.fpsLocalPS5, Chiaki.settings.bitrateLocalPS5)
            }

            C.ComboBox {
                property bool listOpen: popup.visible
                onListOpenChanged: { if (listOpen) view.grabInput(null); else view.releaseInput(); Chiaki.window.requestOverlayUpdate() }
                model: ["30 fps", "60 fps"]
                currentIndex: Chiaki.settings.fpsLocalPS5 / 30 - 1
                implicitWidth: 100
                implicitHeight: 40
                font.pixelSize: 13
                focusPolicy: Qt.NoFocus
                Material.accent: farsideBar.accent
                onActivated: (index) => farsideBar.store(Chiaki.settings.resolutionLocalPS5, (index + 1) * 30, Chiaki.settings.bitrateLocalPS5)
            }

            C.ComboBox {
                // 0 = automatic: chiaki-ng picks the bitrate that belongs to the resolution.
                // 100 Mbps is the most chiaki-ng lets you ask the PS5 for.
                readonly property var rates: [0, 5000, 8000, 10000, 15000, 20000, 30000, 50000, 75000, 100000]
                property bool listOpen: popup.visible
                onListOpenChanged: { if (listOpen) view.grabInput(null); else view.releaseInput(); Chiaki.window.requestOverlayUpdate() }
                model: rates.map((r) => r === 0 ? qsTr("Auto (%1 Mbps)").arg(farsideBar.autoBitrate[Chiaki.settings.resolutionLocalPS5 - 1]) : (r / 1000) + " Mbps")
                currentIndex: rates.indexOf(Chiaki.settings.bitrateLocalPS5)
                // A bitrate set in Settings that is not in the list still shows as what it is.
                displayText: currentIndex >= 0 ? currentText : (Chiaki.settings.bitrateLocalPS5 / 1000) + " Mbps"
                implicitWidth: 160
                implicitHeight: 40
                font.pixelSize: 13
                focusPolicy: Qt.NoFocus
                Material.accent: farsideBar.accent
                onActivated: (index) => farsideBar.store(Chiaki.settings.resolutionLocalPS5, Chiaki.settings.fpsLocalPS5, rates[index])
            }
            Label {
                visible: farsideOptions.hudResponse
                text: qsTr("response: %1").arg(farsideBar.latencyHint)
                color: farsideBar.latencyHint === qsTr("fastest") || farsideBar.latencyHint === qsTr("fast") ? "#3ddc97" : "#ff8a3d"
                font.pixelSize: 12
            }

            Label {
                visible: farsideBar.changed
                text: qsTr("applies on reconnect")
                color: "#ff8a3d"
                font.pixelSize: 12
            }

            FButton {
                primary: true
                text: qsTr("Fullscreen")
                font.pixelSize: 13
                implicitHeight: 36
                focusPolicy: Qt.NoFocus
                onClicked: Chiaki.window.toggleFullscreen()
            }

            FButton {
                text: "▴"
                implicitHeight: 36
                implicitWidth: 40
                focusPolicy: Qt.NoFocus
                onClicked: Chiaki.window.barOpen = false
            }
        }
    }

    Component {
        id: streamStatsContent
        Item {
            id: streamStatsContentRoot
            anchors.fill: parent
            ColumnLayout {
                anchors {
                    right: parent.right
                    verticalCenter: parent.verticalCenter
                    rightMargin: 5
                }

                Label {
                    Layout.alignment: Qt.AlignRight
                    text: "Mbps"
                    font.pixelSize: 18
                    visible: Chiaki.session ? true : false

                    Label {
                        anchors {
                            right: parent.left
                            baseline: parent.baseline
                            rightMargin: 5
                        }
                        text: parent.visible ? Chiaki.session.measuredBitrate.toFixed(1) : ""
                        color: Material.accent
                        font.bold: true
                        font.pixelSize: 28
                    }
                }

                Label {
                    Layout.alignment: Qt.AlignRight
                    text: qsTr("queue depth avg")
                    font.pixelSize: 15
                    opacity: Chiaki.session ? 1 : 0
                    visible: opacity > 0

                    Behavior on opacity { NumberAnimation { duration: 250 } }

                    Label {
                        anchors {
                            right: parent.left
                            baseline: parent.baseline
                            rightMargin: 5
                        }
                        text: parent.visible ? Chiaki.window.queueDepthAverage.toFixed(1) : ""
                        font.bold: true
                        color: "#f3f3f6"
                        font.pixelSize: 18
                    }
                }

                Label {
                    Layout.alignment: Qt.AlignRight
                    text: qsTr("pending frame age")
                    font.pixelSize: 15
                    opacity: Chiaki.session ? 1 : 0
                    visible: opacity > 0

                    Behavior on opacity { NumberAnimation { duration: 250 } }

                    Label {
                        anchors {
                            right: parent.left
                            baseline: parent.baseline
                            rightMargin: 5
                        }
                        text: parent.visible ? qsTr("%1 ms").arg((Chiaki.window.pendingFrameAge * 1000.0).toFixed(0)) : ""
                        font.bold: true
                        color: "#f3f3f6"
                        font.pixelSize: 18
                    }
                }

                Label {
                    Layout.alignment: Qt.AlignRight
                    id: statsPacketLossLabel
                    text: qsTr("packet loss")
                    font.pixelSize: 15
                    opacity: Chiaki.session ? 1 : 0
                    visible: opacity > 0

                    Behavior on opacity { NumberAnimation { duration: 250 } }

                    Label {
                        anchors {
                            right: parent.left
                            baseline: parent.baseline
                            rightMargin: 5
                        }
                        text: parent.visible ? "%1<font size=\"1\">%</font>".arg((((Chiaki.session && isFinite(Chiaki.session.averagePacketLoss)) ? Chiaki.session.averagePacketLoss : 0) * 100).toFixed(1)) : ""
                        font.bold: true
                        color: "#ec6078"
                        font.pixelSize: 18
                    }
                }

                Label {
                    Layout.alignment: Qt.AlignRight
                    text: qsTr("dropped frames")
                    font.pixelSize: 15
                    opacity: Chiaki.session ? 1 : 0
                    visible: opacity > 0

                    Behavior on opacity { NumberAnimation { duration: 250 } }

                    Label {
                        id: statsDroppedFramesLabel
                        anchors {
                            right: parent.left
                            baseline: parent.baseline
                            rightMargin: 5
                        }
                        text: parent.visible ? Chiaki.window.droppedFrames : ""
                        color: "#ec6078"
                        font.bold: true
                        font.pixelSize: 18
                    }
                }

                Label {
                    Layout.alignment: Qt.AlignRight
                    text: qsTr("lost frames")
                    font.pixelSize: 15
                    opacity: Chiaki.session ? 1 : 0
                    visible: opacity > 0

                    Behavior on opacity { NumberAnimation { duration: 250 } }

                    Label {
                        anchors {
                            right: parent.left
                            baseline: parent.baseline
                            rightMargin: 5
                        }
                        text: parent.visible ? ((Chiaki.session && isFinite(Chiaki.session.framesLost)) ? Chiaki.session.framesLost : 0) : ""
                        color: "#ec6078"
                        font.bold: true
                        font.pixelSize: 18
                    }
                }
            }
        }
    }

    Component {
        id: menuContentComponent

        Item {
            property Item initialFocusItem: closeButton
            anchors.fill: parent

            Canvas {
                anchors.fill: parent
                onWidthChanged: requestPaint()
                onHeightChanged: requestPaint()
                onPaint: {
                    let ctx = getContext("2d");
                    let gradient = ctx.createLinearGradient(0, 0, 0, height);
                    gradient.addColorStop(0.0, Qt.rgba(0.0, 0.0, 0.0, 0.0));
                    // Farside look: the dark of the pages (#0b0b10), not grey.
                    gradient.addColorStop(0.7, Qt.rgba(0.043, 0.043, 0.063, 0.85));
                    gradient.addColorStop(1.0, Qt.rgba(0.043, 0.043, 0.063, 0.95));
                    ctx.fillStyle = gradient;
                    ctx.fillRect(0, 0, width, height);
                }
            }

            RowLayout {
                anchors {
                    left: parent.left
                    bottom: parent.bottom
                    leftMargin: 30
                    bottomMargin: 40
                }
                spacing: 0

                ToolButton {
                    // Farside look.
                    background: FBack { flat: true }
                    id: closeButton
                    Layout.rightMargin: 20
                    text: "×"
                    padding: 10
                    font.pixelSize: 50
                    down: activeFocus
                    onClicked: {
                        if (Chiaki.session)
                            Chiaki.window.close();
                        else
                            root.showMainView();
                    }
                    KeyNavigation.right: volumeSlider
                    Keys.onReturnPressed: clicked()
                    Keys.onEscapePressed: menuController.close()
                }

            ToolSeparator {
                Layout.leftMargin: -10
                Layout.rightMargin: 10
            }

            FSlider {
                id: volumeSlider
                Layout.rightMargin: 20
                orientation: Qt.Vertical
                from: 0
                to: 128
                Layout.preferredHeight: 100
                padding: 10
                stepSize: 1
                value: Chiaki.settings.audioVolume
                onMoved: Chiaki.settings.audioVolume = value
                KeyNavigation.left: closeButton
                KeyNavigation.right: muteButton
                Keys.onEscapePressed: menuController.close()
                Label {
                    anchors {
                        top: parent.bottom
                        horizontalCenter: parent.horizontalCenter
                        leftMargin: 10
                    }
                    text: {
                        ((parent.value / 128.0) * 100).toFixed(0) + qsTr("% Volume")
                    }
                }
            }

            ToolSeparator {
                Layout.leftMargin: -10
                Layout.rightMargin: -10
            }

            ToolButton {
                // Farside look.
                background: FBack { flat: true }
                id: muteButton
                Layout.rightMargin: 20
                text: qsTr("Mic")
                padding: 10
                checkable: true
                enabled: Chiaki.session && Chiaki.session.connected
                checked: Chiaki.session && !Chiaki.session.muted
                onToggled: Chiaki.session.muted = !Chiaki.session.muted
                KeyNavigation.left: volumeSlider
                KeyNavigation.right: zoomButton
                Keys.onReturnPressed: toggled()
                Keys.onEscapePressed: menuController.close()
            }

            ToolButton {
                // Farside look.
                background: FBack { flat: true }
                id: zoomButton
                text: qsTr("Zoom")
                padding: 10
                checkable: true
                checked: Chiaki.window.videoMode == ChiakiWindow.VideoMode.Zoom
                onToggled: Chiaki.window.videoMode = Chiaki.window.videoMode == ChiakiWindow.VideoMode.Zoom ? ChiakiWindow.VideoMode.Normal : ChiakiWindow.VideoMode.Zoom
                KeyNavigation.left: muteButton
                KeyNavigation.right: {
                    if(Chiaki.window.videoMode == ChiakiWindow.VideoMode.Zoom)
                        zoomFactor
                    else
                        stretchButton
                }
                Keys.onReturnPressed: toggled()
                Keys.onEscapePressed: menuController.close()
            }

            FSlider {
                id: zoomFactor
                orientation: Qt.Vertical
                from: -1
                to: 4
                Layout.preferredHeight: 100
                stepSize: 0.01
                visible: Chiaki.window.videoMode == ChiakiWindow.VideoMode.Zoom
                value: Chiaki.window.ZoomFactor
                onMoved: {
                    Chiaki.window.ZoomFactor = value
                    Chiaki.settings.sZoomFactor = value
                }
                Label {
                    anchors {
                        top: parent.bottom
                        horizontalCenter: parent.horizontalCenter
                        leftMargin: 10
                    }
                    text: {
                        if(parent.value === -1)
                            qsTr("No Black Bars")
                        else if(parent.value >= 0)
                            qsTr((parent.value + 1).toFixed(2)) + qsTr(" x")
                        else
                            qsTr(parent.value.toFixed(2)) + qsTr(" x")
                    }

                }
            }

            ToolSeparator {
                Layout.leftMargin: -10
                Layout.rightMargin: -10
            }

            ToolButton {
                // Farside look.
                background: FBack { flat: true }
                id: stretchButton
                Layout.rightMargin: 50
                text: qsTr("Stretch")
                padding: 10
                checkable: true
                checked: Chiaki.window.videoMode == ChiakiWindow.VideoMode.Stretch
                onToggled: Chiaki.window.videoMode = Chiaki.window.videoMode == ChiakiWindow.VideoMode.Stretch ? ChiakiWindow.VideoMode.Normal : ChiakiWindow.VideoMode.Stretch
                KeyNavigation.left: {
                    if(Chiaki.window.videoMode == ChiakiWindow.VideoMode.Zoom)
                        zoomFactor
                    else
                        zoomButton
                }
                KeyNavigation.right: defaultButton
                Keys.onReturnPressed: toggled()
                Keys.onEscapePressed: menuController.close()
            }

            ToolButton {
                // Farside look.
                background: FBack { flat: true }
                id: defaultButton
                text: qsTr("Default")
                padding: 10
                checkable: true
                checked: Chiaki.window.videoPreset == ChiakiWindow.VideoPreset.Default
                onToggled: {
                    Chiaki.window.videoPreset = ChiakiWindow.VideoPreset.Default
                    Chiaki.settings.videoPreset = ChiakiWindow.VideoPreset.Default
                }
                KeyNavigation.left: stretchButton
                KeyNavigation.right: highQualityButton
                Keys.onReturnPressed: toggled()
                Keys.onEscapePressed: menuController.close()
            }

            ToolSeparator {
                Layout.leftMargin: -10
                Layout.rightMargin: -10
            }

            ToolButton {
                // Farside look.
                background: FBack { flat: true }
                id: highQualityButton
                text: qsTr("High Quality")
                padding: 10
                checkable: true
                checked: Chiaki.window.videoPreset == ChiakiWindow.VideoPreset.HighQuality
                onToggled: {
                    Chiaki.window.videoPreset = ChiakiWindow.VideoPreset.HighQuality
                    Chiaki.settings.videoPreset = ChiakiWindow.VideoPreset.HighQuality
                }
                KeyNavigation.left: defaultButton
                KeyNavigation.right: highQualitySpatialButton
                Keys.onReturnPressed: toggled()
                Keys.onEscapePressed: menuController.close()
            }

            ToolButton {
                // Farside look.
                background: FBack { flat: true }
                id: highQualitySpatialButton
                text: qsTr("HQ + Spatial")
                padding: 10
                checkable: true
                checked: Chiaki.window.videoPreset == ChiakiWindow.VideoPreset.HighQualitySpatial
                onToggled: {
                    Chiaki.window.videoPreset = ChiakiWindow.VideoPreset.HighQualitySpatial
                    Chiaki.settings.videoPreset = ChiakiWindow.VideoPreset.HighQualitySpatial
                }
                KeyNavigation.left: highQualityButton
                KeyNavigation.right: highQualityAdvancedSpatialButton
                Keys.onReturnPressed: toggled()
                Keys.onEscapePressed: menuController.close()
            }

            ToolButton {
                // Farside look.
                background: FBack { flat: true }
                id: highQualityAdvancedSpatialButton
                text: qsTr("HQ + Adv Spatial")
                padding: 10
                checkable: true
                checked: Chiaki.window.videoPreset == ChiakiWindow.VideoPreset.HighQualityAdvancedSpatial
                onToggled: {
                    Chiaki.window.videoPreset = ChiakiWindow.VideoPreset.HighQualityAdvancedSpatial
                    Chiaki.settings.videoPreset = ChiakiWindow.VideoPreset.HighQualityAdvancedSpatial
                }
                KeyNavigation.left: highQualitySpatialButton
                KeyNavigation.right: customButton
                Keys.onReturnPressed: toggled()
                Keys.onEscapePressed: menuController.close()
            }

            ToolSeparator {
                Layout.leftMargin: -10
                Layout.rightMargin: -10
            }

            ToolButton {
                // Farside look.
                background: FBack { flat: true }
                id: customButton
                text: qsTr("Custom")
                Layout.rightMargin: 40
                padding: 10
                checkable: true
                checked: Chiaki.window.videoPreset == ChiakiWindow.VideoPreset.Custom
                onToggled: {
                    Chiaki.window.videoPreset = ChiakiWindow.VideoPreset.Custom
                    Chiaki.settings.videoPreset = ChiakiWindow.VideoPreset.Custom
                }
                KeyNavigation.left: highQualityAdvancedSpatialButton
                KeyNavigation.right: displaySettingsButton
                Keys.onReturnPressed: toggled()
                Keys.onEscapePressed: menuController.close()
            }

            ToolButton {
                // Farside look.
                background: FBack { flat: true }
                id: displaySettingsButton
                text: qsTr("Display")
                padding: 10
                checkable: false
                icon.source: "qrc:/icons/settings-20px.svg";
                onClicked: root.openDisplaySettings()
                KeyNavigation.left: customButton
                KeyNavigation.right: {
                    if(Chiaki.window.videoPreset == ChiakiWindow.VideoPreset.Custom)
                        placeboSettingsButton;
                    else
                        displaySettingsButton;
                }
                Keys.onReturnPressed: {
                    menuController.close();
                    clicked();
                }
                Keys.onEscapePressed: menuController.close()
            }

            ToolButton {
                // Farside look.
                background: FBack { flat: true }
                id: placeboSettingsButton
                text: qsTr("Placebo")
                icon.source: "qrc:/icons/settings-20px.svg";
                padding: 10
                checkable: false
                onClicked: root.openPlaceboSettings()
                KeyNavigation.left: displaySettingsButton
                visible: Chiaki.window.videoPreset == ChiakiWindow.VideoPreset.Custom
                Keys.onReturnPressed: {
                    menuController.close();
                    clicked();
                }
                Keys.onEscapePressed: menuController.close()
            }
            }

            Label {
                anchors {
                    right: consoleNameLabel.right
                    bottom: consoleNameLabel.top
                    bottomMargin: 5

                }
                text: "Mbps"
                font.pixelSize: 18
                visible: Chiaki.session

                Label {
                    anchors {
                        right: parent.left
                        baseline: parent.baseline
                        rightMargin: 5
                    }
                        text: visible && Chiaki.session ? Chiaki.session.measuredBitrate.toFixed(1) : ""
                    color: Material.accent
                    font.bold: true
                    font.pixelSize: 28
                }
            }

            Label {
                id: consoleNameLabel
                anchors {
                    right: parent.right
                    verticalCenter: parent.verticalCenter
                    rightMargin: 30
                }
                text: {
                    if (!Chiaki.session)
                        return "";
                    if (Chiaki.session.connected)
                        return qsTr("Connected to <b>%1</b>").arg(Chiaki.settings.streamerMode ? "hidden" : Chiaki.session.host);
                    return qsTr("Connecting to <b>%1</b>").arg(Chiaki.settings.streamerMode ? "hidden" : Chiaki.session.host);
                }

                RowLayout {
                    anchors {
                        right: parent.right
                        top: parent.bottom
                        topMargin: 12
                    }

                    Label {
                        text: qsTr("packet loss")
                        font.pixelSize: 15
                        opacity: parent.visible && Chiaki.session ? 1.0 : 0.0
                        visible: opacity

                        Behavior on opacity { NumberAnimation { duration: 250 } }

                        Label {
                            anchors {
                                right: parent.left
                                baseline: parent.baseline
                                rightMargin: 5
                            }
                            text: visible ? "%1<font size=\"1\">%</font>".arg((((Chiaki.session && isFinite(Chiaki.session.averagePacketLoss)) ? Chiaki.session.averagePacketLoss : 0) * 100).toFixed(1)) : ""
                            color: "#ec6078" // Material.Red
                            font.bold: true
                            font.pixelSize: 18
                        }
                    }

                    Label {
                        Layout.leftMargin: droppedFramesLabel.width + 6
                        text: qsTr("dropped frames")
                        font.pixelSize: 15
                        opacity: parent.visible && Chiaki.window.droppedFrames ? 1.0 : 0.0
                        visible: opacity

                        Behavior on opacity { NumberAnimation { duration: 250 } }

                        Label {
                            id: droppedFramesLabel
                            anchors {
                                right: parent.left
                                baseline: parent.baseline
                                rightMargin: 5
                            }
                            text: visible ? Chiaki.window.droppedFrames : ""
                            color: "#ec6078" // Material.Red
                            font.bold: true
                            font.pixelSize: 18
                        }
                    }
                }
            }
        }
    }

    Item {
        id: menuView
        anchors {
            left: parent.left
            right: parent.right
            bottom: parent.bottom
        }
        height: streamMenuHeight
        y: menuController.open ? parent.height - height : parent.height
        visible: !useSeparateMenuWindow && (menuController.open || menuController.closing)
        enabled: !useSeparateMenuWindow && menuController.open
        onVisibleChanged: {
            if (visible)
                view.grabInput(inlineMenuContent.item ? inlineMenuContent.item.initialFocusItem : null);
        }

        Behavior on y {
            NumberAnimation {
                id: inlineMenuAnimation
                duration: 250
                onRunningChanged: {
                    if (!running && !menuController.open) {
                        menuController.closing = false;
                        view.updateOverlayInteractionActive();
                    }
                }
            }
        }

        Loader {
            id: inlineMenuContent
            anchors.fill: parent
            active: !useSeparateMenuWindow
            sourceComponent: menuContentComponent
        }
    }

    StreamMenuWindow {
        id: separateMenuWindow
        transientParent: view.hostWindow
        x: separateMenuX
        y: menuController.open ? separateMenuY : separateMenuY + streamMenuHeight
        width: separateMenuWidth > 0 ? separateMenuWidth : view.width
        height: streamMenuHeight
        open: useSeparateMenuWindow && menuController.open
        closing: useSeparateMenuWindow && menuController.closing
        onCloseRequested: menuController.close()
        onDisplaySettingsRequested: {
            menuController.close();
            root.openDisplaySettings();
        }
        onPlaceboSettingsRequested: {
            menuController.close();
            root.openPlaceboSettings();
        }
        onMainViewRequested: root.showMainView()
        onCloseAnimationFinished: {
            if (view.hostWindow)
                view.hostWindow.requestActivate();
            menuController.closing = false;
            view.updateOverlayInteractionActive();
        }
    }

    Popup {
        id: sessionStopDialog
        property int closeAction: 0
        parent: Overlay.overlay
        x: Math.round((root.width - width) / 2)
        y: Math.round((root.height - height) / 2)
        modal: true
        padding: 30
        // Farside look (see FDialog.qml): a dark card, and the picture behind dimmed dark, not with the style's light veil.
        background: Rectangle {
            radius: 14
            color: "#14141d"
            border.color: "#262633"
        }
        Overlay.modal: Rectangle {
            color: "#99000000"
        }
        onAboutToShow: {
            closeAction = 0;
            sessionStopDialogActive = true;
            view.updateOverlayInteractionActive();
        }
        onClosed: {
            view.releaseInput();
            sessionStopDialogActive = false;
            view.updateOverlayInteractionActive();
            if (closeAction)
                Chiaki.stopSession(closeAction == 1);
        }

        ColumnLayout {
            Label {
                Layout.alignment: Qt.AlignCenter
                text: qsTr("Disconnect Session")
                font.weight: Font.DemiBold
                font.pixelSize: 22
            }

            Label {
                Layout.topMargin: 10
                Layout.alignment: Qt.AlignCenter
                text: qsTr("Do you want the Console to go into sleep mode?")
                color: "#a3a3b5"
                font.pixelSize: 18
            }

            RowLayout {
                Layout.topMargin: 30
                Layout.alignment: Qt.AlignCenter
                spacing: 30

                Button {
                    id: sleepButton
                    Layout.preferredWidth: 160
                    Layout.minimumHeight: 56
                    Layout.maximumHeight: 56
                    text: qsTr("Sleep")
                    font.pixelSize: 18
                    // Farside look.
                    background: FBack { }
                    KeyNavigation.right: noButton
                    Keys.onReturnPressed: clicked()
                    Keys.onEscapePressed: sessionStopDialog.close()
                    onVisibleChanged: if (visible) view.grabInput(sleepButton)
                    onClicked: {
                        sessionStopDialog.closeAction = 1;
                        sessionStopDialog.close();
                    }
                }

                Button {
                    id: noButton
                    Layout.preferredWidth: 160
                    Layout.minimumHeight: 56
                    Layout.maximumHeight: 56
                    text: qsTr("No")
                    font.pixelSize: 18
                    // Farside look.
                    background: FBack { }
                    KeyNavigation.left: sleepButton
                    Keys.onReturnPressed: clicked()
                    Keys.onEscapePressed: sessionStopDialog.close()
                    onClicked: {
                        sessionStopDialog.closeAction = 2;
                        sessionStopDialog.close();
                    }
                }
            }
        }
    }

    Window {
        id: separateSessionStopWindow
        property int closeAction: 0
        readonly property int dialogWidth: 700
        readonly property int dialogHeight: 260
        visible: false
        flags: Qt.Dialog | Qt.FramelessWindowHint
        color: "transparent"
        transientParent: view.hostWindow
        modality: Qt.ApplicationModal
        x: separateDialogX
        y: separateDialogY
        width: dialogWidth
        height: dialogHeight

        onVisibleChanged: {
            if (visible) {
                closeAction = 0;
                view.updateSeparateDialogGeometry(width, height);
                requestActivate();
                view.grabInput(separateSleepButton);
            } else {
                view.releaseInput();
                if (closeAction)
                    Chiaki.stopSession(closeAction === 1);
            }
            view.updateOverlayInteractionActive();
        }

        Rectangle {
            anchors.fill: parent
            // Farside look.
            radius: 14
            color: "#14141d"
            border.color: "#262633"
            border.width: 1
        }

        ColumnLayout {
            anchors.centerIn: parent

            Label {
                Layout.alignment: Qt.AlignCenter
                text: qsTr("Disconnect Session")
                font.weight: Font.DemiBold
                font.pixelSize: 22
            }

            Label {
                Layout.topMargin: 10
                Layout.alignment: Qt.AlignCenter
                text: qsTr("Do you want the Console to go into sleep mode?")
                color: "#a3a3b5"
                font.pixelSize: 18
            }

            RowLayout {
                Layout.topMargin: 30
                Layout.alignment: Qt.AlignCenter
                spacing: 30

                Button {
                    id: separateSleepButton
                    Layout.preferredWidth: 160
                    Layout.minimumHeight: 56
                    Layout.maximumHeight: 56
                    text: qsTr("Sleep")
                    font.pixelSize: 18
                    // Farside look.
                    background: FBack { }
                    KeyNavigation.right: separateNoButton
                    Keys.onReturnPressed: clicked()
                    Keys.onEscapePressed: separateSessionStopWindow.visible = false
                    onClicked: {
                        separateSessionStopWindow.closeAction = 1;
                        separateSessionStopWindow.visible = false;
                    }
                }

                Button {
                    id: separateNoButton
                    Layout.preferredWidth: 160
                    Layout.minimumHeight: 56
                    Layout.maximumHeight: 56
                    text: qsTr("No")
                    font.pixelSize: 18
                    // Farside look.
                    background: FBack { }
                    KeyNavigation.left: separateSleepButton
                    Keys.onReturnPressed: clicked()
                    Keys.onEscapePressed: separateSessionStopWindow.visible = false
                    onClicked: {
                        separateSessionStopWindow.closeAction = 2;
                        separateSessionStopWindow.visible = false;
                    }
                }
            }
        }
    }

    FDialog {
        id: sessionPinDialog
        parent: Overlay.overlay
        x: Math.round((root.width - width) / 2)
        y: Math.round((root.height - height) / 2)
        title: qsTr("Console Login PIN")
        modal: true
        closePolicy: Popup.NoAutoClose
        standardButtons: Dialog.Ok | Dialog.Cancel
        onAboutToShow: {
            standardButton(Dialog.Ok).enabled = Qt.binding(function() {
                return pinField.acceptableInput;
            });
            view.grabInput(pinField);
            sessionPinDialogActive = true;
            view.updateOverlayInteractionActive();
        }
        onClosed: {
            view.releaseInput();
            sessionPinDialogActive = false;
            view.updateOverlayInteractionActive();
        }
        onAccepted: Chiaki.enterPin(pinField.text)
        onRejected: Chiaki.stopSession(false)

        FField {
            id: pinField
            echoMode: Chiaki.settings.streamerMode ? TextInput.Password : TextInput.Normal
            implicitWidth: 200
            validator: RegularExpressionValidator { regularExpression: /[0-9]{4}/ }
            Keys.onReturnPressed: {
                if(sessionPinDialog.standardButton(Dialog.Ok).enabled)
                    sessionPinDialog.standardButton(Dialog.Ok).clicked()
            }
        }
    }

    Window {
        id: separateSessionPinWindow
        readonly property int dialogWidth: 360
        readonly property int dialogHeight: 220
        visible: false
        flags: Qt.Dialog | Qt.FramelessWindowHint
        color: "transparent"
        transientParent: view.hostWindow
        modality: Qt.ApplicationModal
        x: separateDialogX
        y: separateDialogY
        width: dialogWidth
        height: dialogHeight

        onVisibleChanged: {
            if (visible) {
                separatePinField.text = "";
                view.updateSeparateDialogGeometry(width, height);
                requestActivate();
                view.grabInput(separatePinField);
            } else {
                view.releaseInput();
            }
            view.updateOverlayInteractionActive();
        }

        Rectangle {
            anchors.fill: parent
            // Farside look.
            radius: 14
            color: "#14141d"
            border.color: "#262633"
            border.width: 1
        }

        ColumnLayout {
            anchors {
                fill: parent
                margins: 24
            }

            Label {
                Layout.alignment: Qt.AlignCenter
                text: qsTr("Console Login PIN")
                font.weight: Font.DemiBold
                font.pixelSize: 22
            }

            FField {
                id: separatePinField
                Layout.topMargin: 20
                Layout.fillWidth: true
                echoMode: Chiaki.settings.streamerMode ? TextInput.Password : TextInput.Normal
                validator: RegularExpressionValidator { regularExpression: /[0-9]{4}/ }
                Keys.onReturnPressed: if (acceptableInput) separatePinOkButton.clicked()
                Keys.onEscapePressed: separateSessionPinWindow.visible = false
            }

            RowLayout {
                Layout.topMargin: 20
                Layout.alignment: Qt.AlignRight
                spacing: 16

                Button {
                    text: qsTr("Cancel")
                    // Farside look.
                    background: FBack { }
                    Keys.onReturnPressed: clicked()
                    onClicked: {
                        separateSessionPinWindow.visible = false;
                        Chiaki.stopSession(false);
                    }
                }

                Button {
                    id: separatePinOkButton
                    text: qsTr("OK")
                    enabled: separatePinField.acceptableInput
                    // Farside look.
                    background: FBack { }
                    Keys.onReturnPressed: clicked()
                    onClicked: {
                        const pin = separatePinField.text;
                        separateSessionPinWindow.visible = false;
                        Chiaki.enterPin(pin);
                    }
                }
            }
        }
    }

    Timer {
        id: closeTimer
        interval: 2000
        onTriggered: root.showMainView()
    }

    Connections {
        target: Chiaki

        function onSessionChanged() {
            if (!Chiaki.session) {
                if (errorTitleLabel.text)
                    closeTimer.start();
                else
                    root.showMainView();
            } else {
                sessionError = false;
                errorTitleLabel.text = "";
                errorTextLabel.text = "";
                sessionLoading = !(Chiaki.window.loadingTransitionComplete || (Chiaki.settings.audioVideoDisabled & 0x02));
            }
        }

        function onSessionError(title, text) {
            sessionError = true;
            sessionLoading = false;
            errorTitleLabel.text = title;
            errorTextLabel.text = text;
            closeTimer.start();
        }

        function onSessionPinDialogRequested() {
            if (sessionPinDialog.opened || separateSessionPinWindow.visible)
                return;
            menuController.close();
            if (useSeparateMenuWindow)
                separateSessionPinWindow.visible = true;
            else
                sessionPinDialog.open();
            Chiaki.window.requestOverlayUpdate();
        }

        function onSessionStopDialogRequested() {
            if (sessionStopDialog.opened || separateSessionStopWindow.visible)
                return;
            menuController.close();
            if (useSeparateMenuWindow)
                separateSessionStopWindow.visible = true;
            else
                sessionStopDialog.open();
            Chiaki.window.requestOverlayUpdate();
        }
    }

    Connections {
        target: Chiaki.window

        function onLoadingTransitionCompleteChanged() {
            if (Chiaki.window.loadingTransitionComplete) {
                sessionLoading = false;
                Chiaki.window.noteLoadingTransitionComplete();
                Chiaki.window.requestOverlayUpdate();
            } else if (Chiaki.session) {
                sessionLoading = !(Chiaki.settings.audioVideoDisabled & 0x02);
            }
        }

        function onMenuRequested() {
            if (sessionPinDialog.opened || sessionStopDialog.opened || separateSessionPinWindow.visible || separateSessionStopWindow.visible)
                return;
            menuController.toggle();
            Chiaki.window.requestOverlayUpdate();
        }
    }
    Connections {
        target: Chiaki

        function onSessionChanged() {
            if (!Chiaki.session)
                menuController.close();
        }
    }
    Connections {
        target: Chiaki.session

        function onConnectedChanged() {
            if (Chiaki.settings.audioVideoDisabled & 0x02)
                sessionLoading = false;
        }
    }
}
