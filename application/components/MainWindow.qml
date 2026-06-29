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

        MusicPlayer {
            Layout.preferredWidth: 250
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

    function askForSelfUpdate() {
        if(App.updateStatus === App.CanUpdate) {
            selfUpdateDialog.open();
        }
    }
}
