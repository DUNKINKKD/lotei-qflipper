import QtQuick 2.15
import QtQuick.Window 2.15
import QtQuick.Layouts 1.15
import QtQuick.Controls 2.15

import Qt.labs.platform 1.1 as Pf

import Theme 1.0
import QFlipper 1.0

Item {
    id: mainWindow

    signal expandStarted
    signal expandFinished
    signal collapseStarted
    signal collapseFinished
    signal resizeStarted
    signal resizeFinished

    property alias controls: windowControls

    readonly property int baseWidth: 830
    readonly property int baseHeight: 500

    readonly property int logHeight: 200
    readonly property int minimumLogHeight: 200

    readonly property int shadowSize: 16
    readonly property int shadowOffset: 4

    readonly property var deviceState: Backend.deviceState
    readonly property var deviceInfo: deviceState ? deviceState.info : undefined

    Component.onCompleted: {
        if(App.updateStatus === App.CanUpdate) {
            askForSelfUpdate();
        } else {
            App.updateStatusChanged.connect(askForSelfUpdate);
        }
    }

    width: baseWidth
    height: baseHeight

    x: shadowSize
    y: shadowSize - shadowOffset

    Pf.Menu {
        Pf.MenuItem {
            text: qsTr("Check for updates")
            role: Pf.MenuItem.ApplicationSpecificRole
            shortcut: "Ctrl+U"
            onTriggered: App.checkForUpdates()
        }

        Pf.MenuItem {
            text: qsTr("Refresh firmware")
            role: Pf.MenuItem.ApplicationSpecificRole
            shortcut: "Ctrl+R"
            onTriggered: Backend.checkFirmwareUpdates()
        }
    }

    PropertyAnimation {
        id: logExpand

        duration: 500
        target: mainWindow
        property: "height"

        to: target.baseHeight + logHeight
        easing.type: Easing.InOutQuad

        onStarted: mainWindow.expandStarted()
        onFinished: mainWindow.expandFinished()
    }

    PropertyAnimation {
        id: logCollapse
        target: logExpand.target
        easing: logExpand.easing
        duration: logExpand.duration
        property: logExpand.property

        to: target.baseHeight

        onStarted: mainWindow.collapseStarted()
        onFinished: mainWindow.collapseFinished()
    }

    ConfirmationDialog {
        id: confirmationDialog
        radius: bg.radius
        parent: bg
    }

    SelfUpdateDialog {
        id: selfUpdateDialog
        radius: bg.radius
        parent: bg
    }

    WindowShadow {
        id: shadow
        anchors.fill: mainWindow
        anchors.margins: -mainWindow.shadowSize
        anchors.topMargin: -(mainWindow.shadowSize - mainWindow.shadowOffset)
        anchors.bottomMargin: -(mainWindow.shadowSize + mainWindow.shadowOffset)
        opacity: 0.75
    }

    Rectangle {
        id: blackBorder
        anchors.fill: parent
        anchors.margins: -1
        radius: bg.radius + 1
        opacity: 0.5
        color: "black"
    }

    Rectangle {
        id: bg
        radius: 10
        anchors.fill: parent

        color: "black"
        border.color: Theme.color.mediumorange3
        border.width: 2
    }

    WindowControls {
        id: windowControls
        closeEnabled: Backend.backendState <= ApplicationBackend.ScreenStreaming ||
                      Backend.backendState >= ApplicationBackend.Finished

        controlPath: "qrc:/assets/gfx/controls"

        anchors.top: mainWindow.top
        anchors.left: mainContent.left
        anchors.right: mainContent.right
        anchors.bottom: mainContent.top

        anchors.topMargin: bg.border.width
    }

    GridBackground {
        id: mainContent

        anchors.horizontalCenter: parent.horizontalCenter
        y: 38

        width: 800 + border.width * 2
        height: 390 + border.width * 2

        TextLabel {
            id: versionLabel

            anchors.top: parent.top
            anchors.right: parent.right
            anchors.margins: 10
            anchors.rightMargin: 16

            color: Theme.color.lightorange2
            opacity: 0.5

            font.family: "ProggySquareTT"
            font.pixelSize: 16

            text: App.version

            // TODO: Implement copy version to clipboard
        }

        TextLabel {
            id: colorsButton
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.margins: 10
            anchors.leftMargin: 16

            color: Theme.color.lightorange2
            opacity: colorsMouse.containsMouse ? 1.0 : 0.5

            font.family: "ProggySquareTT"
            font.pixelSize: 16
            text: "COLORS"

            MouseArea {
                id: colorsMouse
                anchors.fill: parent
                anchors.margins: -6
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: colorEditor.open = true
            }
        }

        TextLabel {
            id: portToggle
            anchors.top: parent.top
            anchors.left: colorsButton.right
            anchors.leftMargin: 20
            anchors.topMargin: 10

            visible: Backend.portReleased || (Backend.deviceState && Backend.backendState === ApplicationBackend.Ready)
            color: Backend.portReleased ? Theme.color.lightgreen : Theme.color.lightorange2
            opacity: portMouse.containsMouse ? 1.0 : (Backend.portReleased ? 1.0 : 0.5)

            font.family: "ProggySquareTT"
            font.pixelSize: 16
            text: Backend.portReleased ? "RECONNECT" : "RELEASE PORT"

            MouseArea {
                id: portMouse
                anchors.fill: parent
                anchors.margins: -6
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: Backend.portReleased ? Backend.reacquirePort() : Backend.releasePort()
            }
        }

        // BLE-connection spike (Phase 1) trigger
        TextLabel {
            id: bleButton
            anchors.top: parent.top
            anchors.left: portToggle.right
            anchors.leftMargin: 20
            anchors.topMargin: 10

            color: Ble.connected ? Theme.color.lightgreen : Theme.color.lightorange2
            opacity: bleMouse.containsMouse ? 1.0 : 0.5

            font.family: "ProggySquareTT"
            font.pixelSize: 16
            text: "BLE"

            MouseArea {
                id: bleMouse
                anchors.fill: parent
                anchors.margins: -6
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: bleOverlay.open = true
            }
        }

        DeviceWidget {
            id: deviceWidget
            opacity: Backend.backendState !== ApplicationBackend.ScreenStreaming &&
                     Backend.backendState !== ApplicationBackend.ErrorOccured ? 1 : 0

            x: Backend.backendState === ApplicationBackend.Ready ? Math.round(mainContent.width / 2) : 216
            y: 82

            onScreenStreamRequested: Backend.startFullScreenStreaming()
        }

        NoDeviceOverlay {
            id: noDeviceOverlay
            anchors.fill: parent
            opacity: Backend.backendState === ApplicationBackend.WaitingForDevices ? 1 : 0
        }

        HomeOverlay {
            id: homeOverlay
            backgroundRect: bg
            anchors.fill: parent
            opacity: Backend.backendState === ApplicationBackend.Ready ? 1 : 0
        }

        UpdateOverlay {
            id: updateOverlay
            backgroundRect: bg
            anchors.fill: parent
            opacity: Backend.backendState > ApplicationBackend.ScreenStreaming &&
                     Backend.backendState < ApplicationBackend.Finished ? 1 : 0
        }

        FinishOverlay {
            id: finishOverlay
            backgroundRect: bg
            anchors.fill: parent
            opacity: Backend.backendState === ApplicationBackend.Finished ||
                     Backend.backendState === ApplicationBackend.ErrorOccured ? 1 : 0
        }

        StreamOverlay {
            id: streamOverlay
            anchors.fill: parent
            opacity: Backend.backendState === ApplicationBackend.ScreenStreaming ? 1 : 0
        }
    }

    RowLayout {
        id: footerLayour
        width: mainContent.width

        height: 42
        spacing: 15

        anchors.horizontalCenter: mainContent.horizontalCenter
        anchors.top: mainContent.bottom
        anchors.topMargin: 13

        Button {
            id: logButton
            text: qsTr("LOGS")

            Layout.preferredWidth: 110
            Layout.fillHeight: true

            icon.source: checked ? "qrc:/assets/gfx/symbolic/arrow-up.svg" :
                                   "qrc:/assets/gfx/symbolic/arrow-down.svg"
            icon.width: 24
            icon.height: 24

            checkable: true

            Image {
                x: parent.width - width / 2 - 1
                y: -height / 2 + 1

                source: "qrc:/assets/gfx/images/alert-badge.svg"
                sourceSize: Qt.size(18, 18)

                visible: Logger.errorCount > 0 && !logButton.checked
            }

            onCheckedChanged: {
                Logger.errorCount = 0;

                if(checked) {
                    if(!logCollapse.running) {
                        logExpand.start();
                    } else {
                        checked = false;
                    }

                } else {
                    if(!logExpand.running) {
                        logCollapse.start();
                    } else {
                        checked = true;
                    }
                }
            }
        }

        // Music needs QtMultimedia, which is Windows-only in this build. On Linux
        // Lotei.hasAudio is false, so the player (and its QtMultimedia import) is
        // never loaded and the slot collapses.
        Loader {
            active: Lotei.hasAudio
            source: Lotei.hasAudio ? "qrc:/components/MusicPlayer.qml" : ""
            visible: active
            Layout.preferredWidth: active ? 250 : 0
            Layout.fillHeight: true
        }

        StatusBar {
            id: statusBar
            Layout.fillWidth: true
            Layout.fillHeight: true
        }
    }

    TextView {
        id: logView
        visible: height > 0

        anchors.top: footerLayour.bottom
        anchors.left: mainContent.left
        anchors.right: mainContent.right
        anchors.bottom: parent.bottom

        anchors.topMargin: 14
        anchors.bottomMargin: 28

        content.textFormat: TextArea.RichText
        content.text: Logger.logText

        menu: Menu {
            id: logMenu
            width: 170

            MenuItem {
                text: "Select all"
                onTriggered: logView.content.selectAll()
            }

            MenuItem {
                text: "Copy to clipboard"
                onTriggered: logView.content.copy()
            }

            MenuItem {
                text: "Browse all logs..."
                onTriggered: Qt.openUrlExternally(Logger.logsPath)
            }
        }
    }

    MouseArea {
        id: resizer

        property int prevMouseY

        width: parent.width
        height: 28

        visible: logView.visible && !logCollapse.running && !logExpand.running
        cursorShape: Qt.SizeVerCursor

        anchors.bottom: parent.bottom

        preventStealing: true

        onPressed: {
            prevMouseY = mouseY;
            mainWindow.resizeStarted();
        }

        onReleased: mainWindow.resizeFinished()

        onMouseYChanged: {
            const dy = mouseY - prevMouseY;
            mainWindow.height = Math.max(mainWindow.height + dy, mainWindow.baseHeight + mainWindow.minimumLogHeight);
        }
    }

    Text {
        id: fullLogButton
        visible: opacity
        opacity: resizer.visible

        text: "<a href=\"#\">%1</a>".arg(qsTr("Open Full Log"))

        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 6

        font.pixelSize: 14
        font.family: "Share Tech"
        font.capitalization: Font.AllUppercase

        color: Theme.color.lightorange2
        linkColor: Theme.color.lightorange2

        onLinkActivated: Qt.openUrlExternally(Logger.logsFile)

        MouseArea {
            anchors.fill: parent
            acceptedButtons: Qt.NoButton
            cursorShape: Qt.PointingHandCursor
        }

        Behavior on opacity {
            PropertyAnimation {
                duration: 200
                easing.type: Easing.OutCubic
            }
        }
    }

    // ---- LOTEI color editor: live per-color palette tweaking ----
    Item {
        id: colorEditor
        property bool open: false
        anchors.fill: parent
        visible: opacity > 0
        opacity: open ? 1 : 0
        z: 9999
        Behavior on opacity { NumberAnimation { duration: 140 } }

        Rectangle {
            anchors.fill: parent
            color: "#000000"
            opacity: 0.55
            MouseArea { anchors.fill: parent; onClicked: colorEditor.open = false }
        }

        Rectangle {
            anchors.centerIn: parent
            width: 478
            height: Math.min(parent.height - 70, 580)
            color: "#0b0410"
            radius: 10
            border.width: 2
            border.color: Theme.color.mediumorange2
            MouseArea { anchors.fill: parent }   // swallow clicks so they don't close

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 14
                spacing: 10

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 10
                    Text {
                        text: "COLOR EDITOR"
                        color: Theme.color.lightorange2
                        font.family: "Share Tech Mono"; font.pixelSize: 16; font.bold: true
                        Layout.fillWidth: true
                    }
                    Text {
                        text: "reset to pink"
                        color: resetMouse.containsMouse ? Theme.color.lightorange2 : Theme.color.mediumorange1
                        font.family: "Share Tech Mono"; font.pixelSize: 12
                        MouseArea { id: resetMouse; anchors.fill: parent; anchors.margins: -5; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: Palette.reset() }
                    }
                    Text {
                        text: "✕"
                        color: closeMouse.containsMouse ? Theme.color.lightorange2 : Theme.color.mediumorange1
                        font.family: "Share Tech Mono"; font.pixelSize: 15
                        MouseArea { id: closeMouse; anchors.fill: parent; anchors.margins: -6; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: colorEditor.open = false }
                    }
                }

                ListView {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    model: Palette.names()
                    spacing: 7
                    boundsBehavior: Flickable.StopAtBounds
                    ScrollBar.vertical: ScrollBar {}

                    delegate: RowLayout {
                        width: ListView.view.width - 10
                        spacing: 7
                        property string cname: modelData
                        property color cval: Palette.colors[cname]

                        Rectangle {
                            Layout.preferredWidth: 26; Layout.preferredHeight: 26
                            radius: 4; color: parent.cval
                            border.width: 1; border.color: "#666"
                        }
                        Text {
                            text: parent.cname
                            color: Theme.color.lightorange2
                            font.family: "Share Tech Mono"; font.pixelSize: 11
                            Layout.preferredWidth: 92
                            elide: Text.ElideRight
                        }

                        // R / G / B sliders
                        Item {
                            Layout.preferredWidth: 52; Layout.preferredHeight: 16
                            Rectangle { anchors.verticalCenter: parent.verticalCenter; width: parent.width; height: 4; radius: 2; color: "#3a2a3a"
                                Rectangle { width: parent.width * cval.r; height: parent.height; radius: 2; color: "#e88888" } }
                            Rectangle { width: 8; height: 8; radius: 4; color: "#ffffff"; anchors.verticalCenter: parent.verticalCenter; x: parent.width * cval.r - 4 }
                            MouseArea { anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                                onPressed: Palette.setColor(cname, Qt.rgba(Math.max(0, Math.min(1, mouseX / width)), cval.g, cval.b, 1))
                                onPositionChanged: if (pressed) Palette.setColor(cname, Qt.rgba(Math.max(0, Math.min(1, mouseX / width)), cval.g, cval.b, 1)) }
                        }
                        Item {
                            Layout.preferredWidth: 52; Layout.preferredHeight: 16
                            Rectangle { anchors.verticalCenter: parent.verticalCenter; width: parent.width; height: 4; radius: 2; color: "#2a3a2a"
                                Rectangle { width: parent.width * cval.g; height: parent.height; radius: 2; color: "#88e888" } }
                            Rectangle { width: 8; height: 8; radius: 4; color: "#ffffff"; anchors.verticalCenter: parent.verticalCenter; x: parent.width * cval.g - 4 }
                            MouseArea { anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                                onPressed: Palette.setColor(cname, Qt.rgba(cval.r, Math.max(0, Math.min(1, mouseX / width)), cval.b, 1))
                                onPositionChanged: if (pressed) Palette.setColor(cname, Qt.rgba(cval.r, Math.max(0, Math.min(1, mouseX / width)), cval.b, 1)) }
                        }
                        Item {
                            Layout.preferredWidth: 52; Layout.preferredHeight: 16
                            Rectangle { anchors.verticalCenter: parent.verticalCenter; width: parent.width; height: 4; radius: 2; color: "#2a2a3a"
                                Rectangle { width: parent.width * cval.b; height: parent.height; radius: 2; color: "#8888e8" } }
                            Rectangle { width: 8; height: 8; radius: 4; color: "#ffffff"; anchors.verticalCenter: parent.verticalCenter; x: parent.width * cval.b - 4 }
                            MouseArea { anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                                onPressed: Palette.setColor(cname, Qt.rgba(cval.r, cval.g, Math.max(0, Math.min(1, mouseX / width)), 1))
                                onPositionChanged: if (pressed) Palette.setColor(cname, Qt.rgba(cval.r, cval.g, Math.max(0, Math.min(1, mouseX / width)), 1)) }
                        }

                        Text {
                            text: (Palette.colors, Palette.hex(cname))
                            color: Theme.color.mediumorange4
                            font.family: "Share Tech Mono"; font.pixelSize: 11
                            Layout.preferredWidth: 60
                        }
                    }
                }
            }
        }
    }

    // ---- BLE connection spike (Phase 1): scan / connect / prove a byte pipe ----
    Item {
        id: bleOverlay
        property bool open: false
        anchors.fill: parent
        visible: open
        z: 9995

        Rectangle {
            anchors.fill: parent; color: "#000000"; opacity: 0.72
            MouseArea { anchors.fill: parent; onClicked: bleOverlay.open = false }
        }

        Rectangle {
            anchors.centerIn: parent
            width: 580; height: 490
            color: "#0b0410"; radius: 12; border.width: 2; border.color: Theme.color.mediumorange2
            MouseArea { anchors.fill: parent }

            ColumnLayout {
                anchors.fill: parent; anchors.margins: 20; spacing: 12

                RowLayout {
                    Layout.fillWidth: true
                    Text { text: "📶  BLE CONNECT (spike)"; color: Theme.color.lightorange2; font.family: "Share Tech Mono"; font.pixelSize: 20; font.bold: true }
                    Item { Layout.fillWidth: true }
                    Text {
                        text: "✕"; color: closeBleMouse.containsMouse ? Theme.color.lightorange2 : Theme.color.mediumorange4
                        font.family: "Share Tech Mono"; font.pixelSize: 18
                        MouseArea { id: closeBleMouse; anchors.fill: parent; anchors.margins: -6; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: bleOverlay.open = false }
                    }
                }

                RowLayout {
                    Layout.fillWidth: true; spacing: 10
                    Rectangle {
                        Layout.preferredWidth: 110; Layout.preferredHeight: 30; radius: 6
                        border.width: 1; border.color: Theme.color.mediumorange2
                        color: scanMouse.containsMouse && !Ble.scanning ? Theme.color.mediumorange2 : "transparent"
                        Text { anchors.centerIn: parent; text: Ble.scanning ? "SCANNING…" : "SCAN"; color: Theme.color.lightorange2; font.family: "Share Tech Mono"; font.pixelSize: 12; font.bold: true }
                        MouseArea { id: scanMouse; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: if (!Ble.scanning) Ble.scan() }
                    }
                    Rectangle {
                        visible: Ble.connected
                        Layout.preferredWidth: 70; Layout.preferredHeight: 30; radius: 6
                        border.width: 1; border.color: Theme.color.mediumorange2
                        color: pingMouse.containsMouse ? Theme.color.mediumorange2 : "transparent"
                        Text { anchors.centerIn: parent; text: "PING"; color: Theme.color.lightorange2; font.family: "Share Tech Mono"; font.pixelSize: 12; font.bold: true }
                        MouseArea { id: pingMouse; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: Ble.ping() }
                    }
                    Item { Layout.fillWidth: true }
                    Text { text: Ble.connected ? "● connected" : "○ not connected"; color: Ble.connected ? Theme.color.lightgreen : Theme.color.mediumorange1; font.family: "Share Tech Mono"; font.pixelSize: 12 }
                }

                Text { text: "devices (click one to connect):"; color: Theme.color.mediumorange4; font.family: "Share Tech Mono"; font.pixelSize: 11 }
                Flow {
                    Layout.fillWidth: true; spacing: 6
                    Repeater {
                        model: Ble.devices
                        delegate: Rectangle {
                            radius: 5; height: 26; width: dtxt.width + 18
                            border.width: 1; border.color: Theme.color.mediumorange2
                            color: dMouse.containsMouse ? Theme.color.mediumorange2 : "transparent"
                            Text { id: dtxt; anchors.centerIn: parent; text: modelData.name; color: Theme.color.lightorange2; font.family: "Share Tech Mono"; font.pixelSize: 11 }
                            MouseArea { id: dMouse; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: Ble.connectToDevice(index) }
                        }
                    }
                }

                Rectangle {
                    Layout.fillWidth: true; Layout.fillHeight: true
                    color: "#160a1c"; radius: 6; border.width: 1; border.color: Theme.color.mediumorange1
                    Flickable {
                        id: logFlick
                        anchors.fill: parent; anchors.margins: 8; clip: true
                        contentHeight: logText.height; contentWidth: width
                        Text {
                            id: logText
                            width: logFlick.width
                            text: Ble.status
                            color: Theme.color.lightorange2; font.family: "Share Tech Mono"; font.pixelSize: 11
                            wrapMode: Text.WrapAnywhere
                            onTextChanged: logFlick.contentY = Math.max(0, logText.height - logFlick.height)
                        }
                    }
                }

                Text {
                    Layout.fillWidth: true; wrapMode: Text.WordWrap
                    text: "⚠ if connect fails: pair the Flipper in Windows Bluetooth settings first (enter the PIN it shows), then SCAN again."
                    color: Theme.color.mediumorange1; font.family: "Share Tech Mono"; font.pixelSize: 10
                }
            }
        }
    }

    // ---- custom-firmware store (Official / Momentum / Unleashed / RogueMaster) ----
    Item {
        id: firmwareOverlay
        anchors.fill: parent
        visible: Firmware.open
        z: 9990

        property real   dlProgress: 0
        property string dlNote: ""
        property string errText: ""

        Connections {
            target: Firmware
            function onProgress(index, frac, note) {
                firmwareOverlay.dlProgress = frac; firmwareOverlay.dlNote = note; firmwareOverlay.errText = "";
            }
            function onFailed(index, message) { firmwareOverlay.errText = message; firmwareOverlay.dlNote = ""; }
            function onReadyToInstall(fileUrl) {
                firmwareOverlay.dlNote = ""; firmwareOverlay.errText = "";
                Firmware.open = false;              // reveal qFlipper's normal update UI
                Backend.installFirmware(fileUrl);
            }
        }

        Rectangle {
            anchors.fill: parent; color: "#000000"; opacity: 0.72
            MouseArea { anchors.fill: parent; onClicked: Firmware.open = false }
        }

        Rectangle {
            anchors.centerIn: parent
            width: 560
            height: 470
            color: "#0b0410"; radius: 12
            border.width: 2; border.color: Theme.color.mediumorange2
            MouseArea { anchors.fill: parent }   // swallow backdrop clicks over the panel

            ColumnLayout {
                anchors.fill: parent; anchors.margins: 22; spacing: 12

                RowLayout {
                    Layout.fillWidth: true
                    Text { text: "⚡  CUSTOM FIRMWARE"; color: Theme.color.lightorange2; font.family: "Share Tech Mono"; font.pixelSize: 20; font.bold: true }
                    Item { Layout.fillWidth: true }
                    Text {
                        text: "✕"; color: closeFwMouse.containsMouse ? Theme.color.lightorange2 : Theme.color.mediumorange4
                        font.family: "Share Tech Mono"; font.pixelSize: 18
                        MouseArea { id: closeFwMouse; anchors.fill: parent; anchors.margins: -6; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: Firmware.open = false }
                    }
                }

                Text {
                    Layout.fillWidth: true; wrapMode: Text.WordWrap
                    color: Theme.color.mediumorange4; font.family: "Share Tech Mono"; font.pixelSize: 12
                    text: (Backend.deviceState && Backend.deviceState.info && Backend.deviceState.info.firmware)
                          ? ("On your Flipper now:  " + Backend.deviceState.info.firmware.version)
                          : "Connect your Flipper to flash. Latest builds are shown below."
                }

                ColumnLayout {
                    Layout.fillWidth: true; Layout.fillHeight: true; spacing: 8
                    Repeater {
                        model: Firmware.sources
                        delegate: Rectangle {
                            Layout.fillWidth: true
                            implicitHeight: 60
                            radius: 8; color: "#160a1c"
                            border.width: 1; border.color: Theme.color.mediumorange2

                            RowLayout {
                                anchors.fill: parent; anchors.margins: 10; spacing: 10

                                ColumnLayout {
                                    Layout.fillWidth: true; spacing: 2
                                    Text { text: modelData.name; color: Theme.color.lightorange2; font.family: "Share Tech Mono"; font.pixelSize: 15; font.bold: true }
                                    Text { text: modelData.blurb; color: Theme.color.mediumorange4; font.family: "Share Tech Mono"; font.pixelSize: 11 }
                                }
                                Rectangle {
                                    visible: (modelData.channelCount || 0) > 1
                                    Layout.alignment: Qt.AlignVCenter
                                    implicitHeight: 24
                                    implicitWidth: Math.min(chLbl.implicitWidth + 28, 116)
                                    radius: 5
                                    color: chMouse.containsMouse ? Theme.color.mediumorange2 : "transparent"
                                    border.width: 1; border.color: Theme.color.mediumorange2
                                    Text {
                                        id: chLbl
                                        anchors.left: parent.left; anchors.leftMargin: 8
                                        anchors.verticalCenter: parent.verticalCenter
                                        width: parent.width - 22
                                        elide: Text.ElideRight
                                        text: modelData.channel
                                        color: Theme.color.lightorange2; font.family: "Share Tech Mono"; font.pixelSize: 11
                                    }
                                    Text {
                                        anchors.right: parent.right; anchors.rightMargin: 6
                                        anchors.verticalCenter: parent.verticalCenter
                                        text: "▾"; color: Theme.color.lightorange2; font.family: "Share Tech Mono"; font.pixelSize: 9
                                    }
                                    MouseArea { id: chMouse; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: Firmware.cycleChannel(index) }
                                }
                                Text {
                                    text: modelData.status === "checking" ? "checking…"
                                        : modelData.status === "error"    ? "unavailable"
                                        : modelData.latest
                                    color: modelData.status === "error" ? Theme.color.mediumorange1 : Theme.color.lightgreen
                                    font.family: "Share Tech Mono"; font.pixelSize: 12
                                    elide: Text.ElideRight
                                    Layout.maximumWidth: 150
                                    Layout.alignment: Qt.AlignVCenter
                                }
                                Rectangle {
                                    id: instBtn
                                    Layout.preferredWidth: 84; Layout.preferredHeight: 30
                                    Layout.alignment: Qt.AlignVCenter
                                    radius: 6
                                    property bool canGo: modelData.ready && !Firmware.busy && Backend.deviceState
                                    enabled: canGo
                                    opacity: canGo ? 1.0 : 0.4
                                    color: instMouse.containsMouse && canGo ? Theme.color.mediumorange2 : "transparent"
                                    border.width: 1; border.color: Theme.color.mediumorange2
                                    Text { anchors.centerIn: parent; text: "INSTALL"; color: Theme.color.lightorange2; font.family: "Share Tech Mono"; font.pixelSize: 12; font.bold: true }
                                    MouseArea {
                                        id: instMouse; anchors.fill: parent; hoverEnabled: true
                                        cursorShape: instBtn.canGo ? Qt.PointingHandCursor : Qt.ArrowCursor
                                        onClicked: if (instBtn.canGo) Firmware.install(index)
                                    }
                                }
                            }
                        }
                    }
                    Item { Layout.fillHeight: true }
                }

                Text {
                    visible: firmwareOverlay.dlNote.length > 0 || firmwareOverlay.errText.length > 0
                    Layout.fillWidth: true; wrapMode: Text.WordWrap
                    text: firmwareOverlay.errText.length > 0
                          ? firmwareOverlay.errText
                          : (firmwareOverlay.dlNote + "  " + Math.round(firmwareOverlay.dlProgress * 100) + "%")
                    color: firmwareOverlay.errText.length > 0 ? Theme.color.lightred1 : Theme.color.lightgreen
                    font.family: "Share Tech Mono"; font.pixelSize: 12
                }

                RowLayout {
                    Layout.fillWidth: true
                    Text {
                        text: Firmware.busy ? "downloading…" : "↻ check for updates"
                        color: recheckFwMouse.containsMouse ? Theme.color.lightorange2 : Theme.color.mediumorange4
                        font.family: "Share Tech Mono"; font.pixelSize: 12
                        MouseArea { id: recheckFwMouse; anchors.fill: parent; anchors.margins: -4; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: if (!Firmware.busy) Firmware.refresh() }
                    }
                    Item { Layout.fillWidth: true }
                    Text {
                        text: "⚠ flashing replaces your current firmware"
                        color: Theme.color.mediumorange1; font.family: "Share Tech Mono"; font.pixelSize: 10
                    }
                }
            }
        }
    }

    // ---- first-run setup wizard (shown only until Lotei.setupComplete) ----
    Item {
        id: setupWizard
        anchors.fill: parent
        visible: !Lotei.setupComplete
        z: 10000
        property int step: 0

        Rectangle { anchors.fill: parent; color: "#000000"; opacity: 0.72 }
        MouseArea { anchors.fill: parent }   // block anything behind

        Rectangle {
            anchors.centerIn: parent
            width: 560
            height: 440
            color: "#0b0410"
            radius: 12
            border.width: 2
            border.color: Theme.color.mediumorange2

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 22
                spacing: 14

                Text {
                    text: "🐬  SET UP LOTEI"
                    color: Theme.color.lightorange2
                    font.family: "Share Tech Mono"; font.pixelSize: 20; font.bold: true
                }

                StackLayout {
                    currentIndex: setupWizard.step
                    Layout.fillWidth: true
                    Layout.fillHeight: true

                    // 0 · welcome
                    ColumnLayout {
                        spacing: 12
                        Text { text: "Meet LOTEI"; color: Theme.color.lightorange2; font.family: "Share Tech Mono"; font.pixelSize: 16; font.bold: true }
                        Text { Layout.fillWidth: true; wrapMode: Text.WordWrap; color: Theme.color.lightorange2; font.family: "Share Tech Mono"; font.pixelSize: 13
                            text: "A snarky, 100% local AI dolphin lives inside your Flipper app. Let's get him set up — about 30 seconds." }
                        Item { Layout.fillHeight: true }
                    }

                    // 1 · name
                    ColumnLayout {
                        spacing: 12
                        Text { text: "1 · Name your Flipper"; color: Theme.color.lightorange2; font.family: "Share Tech Mono"; font.pixelSize: 15; font.bold: true }
                        Text { Layout.fillWidth: true; wrapMode: Text.WordWrap; color: Theme.color.lightorange2; font.family: "Share Tech Mono"; font.pixelSize: 13
                            text: "LOTEI takes your Flipper's name as his own. Type it, or plug in your Flipper and read it." }
                        RowLayout {
                            Layout.fillWidth: true; spacing: 10
                            TextField {
                                id: nameField
                                Layout.fillWidth: true
                                placeholderText: "e.g. Lotei"
                                text: (Backend.deviceState && Backend.deviceState.info && Backend.deviceState.info.name) ? Backend.deviceState.info.name : ""
                            }
                            Button {
                                text: "Read from Flipper"
                                enabled: Backend.deviceState && Backend.deviceState.info && Backend.deviceState.info.name && Backend.deviceState.info.name.length > 0
                                onClicked: nameField.text = Backend.deviceState.info.name
                            }
                        }
                        Item { Layout.fillHeight: true }
                    }

                    // 2 · AI brain
                    ColumnLayout {
                        spacing: 10
                        Text { text: "2 · LOTEI's brain (local AI)"; color: Theme.color.lightorange2; font.family: "Share Tech Mono"; font.pixelSize: 15; font.bold: true }
                        Text {
                            Layout.fillWidth: true; wrapMode: Text.WordWrap
                            font.family: "Share Tech Mono"; font.pixelSize: 13; color: Theme.color.lightorange2
                            text: !Lotei.ollamaOnline
                                  ? "Ollama isn't running. Install it from ollama.com, then run:  ollama pull qwen2.5:7b  — then hit re-check. (You can skip and set this up later.)"
                                  : (Lotei.availableModels().length === 0
                                     ? "Ollama's running, but no models yet. Run:  ollama pull qwen2.5:7b  — then re-check."
                                     : "Found Ollama. Pick LOTEI's model:")
                        }
                        Flow {
                            Layout.fillWidth: true; spacing: 8
                            Repeater {
                                model: Lotei.ollamaOnline ? Lotei.availableModels() : []
                                delegate: Rectangle {
                                    radius: 5; height: 26; width: mtxt.width + 18
                                    color: modelData === Lotei.modelName ? Theme.color.mediumorange2 : "transparent"
                                    border.width: 1; border.color: Theme.color.mediumorange2
                                    Text { id: mtxt; anchors.centerIn: parent; text: modelData; color: Theme.color.lightorange2; font.family: "Share Tech Mono"; font.pixelSize: 12 }
                                    MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: Lotei.setModel(modelData) }
                                }
                            }
                        }
                        Text {
                            text: "↻ re-check"
                            color: recheckMouse.containsMouse ? Theme.color.lightorange2 : Theme.color.mediumorange1
                            font.family: "Share Tech Mono"; font.pixelSize: 12
                            MouseArea { id: recheckMouse; anchors.fill: parent; anchors.margins: -4; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: Lotei.recheckOllama() }
                        }
                        Item { Layout.fillHeight: true }
                    }

                    // 3 · personality
                    ColumnLayout {
                        spacing: 10
                        Text { text: "3 · Give LOTEI a personality"; color: Theme.color.lightorange2; font.family: "Share Tech Mono"; font.pixelSize: 15; font.bold: true }
                        Button {
                            text: "🎭 Build one from his name" + (nameField.text.length > 0 ? " (" + nameField.text + ")" : "")
                            onClicked: { Lotei.applyNamePersonality(); personaLabel.text = "→ personality built from the name" }
                        }
                        Text { text: "…or pick a preset:"; color: Theme.color.lightorange2; font.family: "Share Tech Mono"; font.pixelSize: 13 }
                        Flow {
                            Layout.fillWidth: true; spacing: 8
                            Repeater {
                                model: Lotei.personalityPresets()
                                delegate: Rectangle {
                                    radius: 5; height: 26; width: ptxt.width + 18
                                    color: "transparent"; border.width: 1; border.color: Theme.color.mediumorange2
                                    Text { id: ptxt; anchors.centerIn: parent; text: modelData; color: Theme.color.lightorange2; font.family: "Share Tech Mono"; font.pixelSize: 12 }
                                    MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: { Lotei.applyPreset(modelData); personaLabel.text = "→ " + modelData } }
                                }
                            }
                        }
                        Text { id: personaLabel; text: ""; color: Theme.color.lightgreen; font.family: "Share Tech Mono"; font.pixelSize: 12 }
                        Item { Layout.fillHeight: true }
                    }

                    // 4 · done
                    ColumnLayout {
                        spacing: 12
                        Text { text: "🎉  All set!"; color: Theme.color.lightorange2; font.family: "Share Tech Mono"; font.pixelSize: 16; font.bold: true }
                        Text { Layout.fillWidth: true; wrapMode: Text.WordWrap; color: Theme.color.lightorange2; font.family: "Share Tech Mono"; font.pixelSize: 13
                            text: "LOTEI's ready. You can change his model (click it in the header), voice, colors, and more any time." }
                        Item { Layout.fillHeight: true }
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 12
                    Text {
                        text: "skip"
                        color: skipMouse.containsMouse ? Theme.color.lightorange2 : Theme.color.mediumorange1
                        font.family: "Share Tech Mono"; font.pixelSize: 12
                        MouseArea { id: skipMouse; anchors.fill: parent; anchors.margins: -6; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: Lotei.completeSetup() }
                    }
                    Item { Layout.fillWidth: true }
                    Button {
                        text: "Back"
                        visible: setupWizard.step > 0
                        onClicked: setupWizard.step = setupWizard.step - 1
                    }
                    Button {
                        text: setupWizard.step >= 4 ? "Finish" : "Next"
                        onClicked: {
                            if (setupWizard.step === 1) Lotei.manualName = nameField.text.trim();
                            if (setupWizard.step >= 4) { Lotei.completeSetup(); return; }
                            setupWizard.step = setupWizard.step + 1;
                            if (setupWizard.step === 2) Lotei.recheckOllama();
                        }
                    }
                }
            }
        }
    }

    function askForSelfUpdate() {
        if(App.updateStatus === App.CanUpdate) {
            selfUpdateDialog.open();
        }
    }
}
