import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

import Theme 1.0
import QFlipper 1.0

Rectangle {
    id: root

    // ---- view state / geometry ------------------------------------------
    // "normal" = docked in the corner, "max" = big read view, "min" = collapsed
    property string viewState: "normal"

    readonly property int dockX: 28
    readonly property int dockY: 250
    readonly property int dockW: 424
    readonly property int dockH: 130
    readonly property int minH: 44

    property bool healthChecked: false
    property int streamIdx: -1   // index of the message currently being streamed
    property bool showScreen: false   // mirror the live Flipper screen in-panel
    // The assistant takes the connected Flipper's name as its own (fallback LOTEI).
    readonly property string aiName: (Backend.deviceState && Backend.deviceState.info && Backend.deviceState.info.name && Backend.deviceState.info.name.length > 0)
                                     ? Backend.deviceState.info.name
                                     : (Lotei.manualName.length > 0 ? Lotei.manualName : "LOTEI")

    x: dockX
    y: viewState === "max" ? 76 : dockY
    width:  viewState === "max" ? ((parent ? parent.width  : 804) - 56) : dockW
    height: viewState === "max" ? ((parent ? parent.height : 394) - 88)
            : (viewState === "min" ? minH : dockH)

    Behavior on width  { NumberAnimation { duration: 140; easing.type: Easing.OutCubic } }
    Behavior on height { NumberAnimation { duration: 140; easing.type: Easing.OutCubic } }
    Behavior on y      { NumberAnimation { duration: 140; easing.type: Easing.OutCubic } }

    radius: 9
    color: "#0b0410"
    border.width: 2
    border.color: Theme.color.lightorange2

    // ---- helpers --------------------------------------------------------
    function deviceContext() {
        var ds = Backend.deviceState;
        if(!ds || !ds.info) {
            return "No Flipper is currently connected.";
        }
        var i = ds.info;
        var lines = [];
        lines.push("Name: " + i.name);
        if(ds.isRecoveryMode) {
            lines.push("Mode: Update & Recovery (DFU)");
        } else {
            var fw = i.firmware.version;
            if(i.firmware.commit && i.firmware.commit.length) { fw += " (commit " + i.firmware.commit + ")"; }
            lines.push("Firmware: " + fw);
            lines.push("SD card: " + (i.storage.isExternalPresent ? (i.storage.externalFree + "% free") : "not present"));
            lines.push("Databases: " + (i.storage.isAssetsInstalled ? "installed" : "missing"));
            lines.push("Radio firmware: " + (i.radioVersion.length ? i.radioVersion : "corrupted"));
        }
        lines.push("Hardware: " + i.hardware.version + "." + i.hardware.target + i.hardware.body + i.hardware.connect);
        return lines.join("\n");
    }

    function appendMessage(role, text) {
        chatModel.append({ "role": role, "text": text });
        listView.positionViewAtEnd();
    }

    function sendCurrent() {
        var t = input.text.trim();
        if(t.length === 0 || Lotei.thinking) {
            return;
        }
        appendMessage("you", t);
        Lotei.send(t, deviceContext());
        input.text = "";
    }

    // Auto health-check once per connection.
    function maybeHealthCheck() {
        var ds = Backend.deviceState;
        if(!ds || !ds.isOnline) { healthChecked = false; return; }
        if(healthChecked || Lotei.thinking) { return; }
        healthChecked = true;
        Lotei.send("A Flipper Zero just connected. Give me a brief, in-character health check: firmware, SD card space, radio firmware, and anything worth flagging. Keep it short.", deviceContext());
    }

    ListModel { id: chatModel }

    Connections {
        target: Lotei
        // live typing: grow one bubble as tokens arrive
        function onPartialReceived(text) {
            if(root.streamIdx < 0) {
                chatModel.append({ "role": "lotei", "text": text });
                root.streamIdx = chatModel.count - 1;
            } else {
                chatModel.setProperty(root.streamIdx, "text", text);
            }
            listView.positionViewAtEnd();
        }
        function onReplyReceived(text) {
            if(root.streamIdx >= 0) {
                chatModel.setProperty(root.streamIdx, "text", text);  // finalize
                root.streamIdx = -1;
            } else {
                root.appendMessage("lotei", text);
            }
            listView.positionViewAtEnd();
        }
        function onErrorOccurred(text) {
            if(root.streamIdx >= 0) {
                chatModel.setProperty(root.streamIdx, "text", text);
                root.streamIdx = -1;
            } else {
                root.appendMessage("lotei", text);
            }
        }
    }

    // Robust connect detection: poll device state; fires the health check once
    // whenever a Flipper becomes available (startup, connect, or reconnect).
    Timer { interval: 2000; repeat: true; running: true; onTriggered: root.maybeHealthCheck() }

    Component.onCompleted: {
        appendMessage("lotei", root.aiName + " online and famished for RAM. Ask about your Flipper, or tell me what to script.");
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 9
        spacing: 5

        // ---- header: name + status + window buttons ----
        RowLayout {
            Layout.fillWidth: true
            spacing: 5

            Text {
                text: root.aiName
                color: Theme.color.lightorange2
                font.family: "Share Tech Mono"
                font.pixelSize: 18
                font.bold: true
            }
            Text {
                text: Lotei.thinking ? "thinking…" : "ready"
                color: Theme.color.mediumorange4
                font.family: "Share Tech Mono"
                font.pixelSize: 12
                Layout.alignment: Qt.AlignVCenter
            }
            // click-to-cycle local AI model (Ollama)
            Text {
                text: Lotei.modelName
                color: modelMouse.containsMouse ? Theme.color.lightorange2 : Theme.color.mediumorange4
                font.family: "Share Tech Mono"
                font.pixelSize: 11
                Layout.alignment: Qt.AlignVCenter
                MouseArea {
                    id: modelMouse
                    anchors.fill: parent
                    anchors.margins: -4
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: Lotei.cycleModel()
                }
            }
            Text {
                text: "voice"
                color: Lotei.muted ? Theme.color.mediumorange1 : Theme.color.lightorange2
                font.family: "Share Tech Mono"
                font.pixelSize: 11
                font.strikeout: Lotei.muted
                Layout.alignment: Qt.AlignVCenter
                MouseArea {
                    anchors.fill: parent
                    anchors.margins: -4
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: Lotei.muted = !Lotei.muted
                }
            }
            Text {
                visible: !Lotei.muted
                text: Lotei.voiceName
                color: voiceNameMouse.containsMouse ? Theme.color.lightorange2 : Theme.color.mediumorange4
                font.family: "Share Tech Mono"
                font.pixelSize: 11
                elide: Text.ElideRight
                Layout.maximumWidth: 84
                Layout.alignment: Qt.AlignVCenter
                MouseArea {
                    id: voiceNameMouse
                    anchors.fill: parent
                    anchors.margins: -4
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: Lotei.cycleVoice()
                }
            }

            // voice volume slider
            Item {
                id: voiceVol
                visible: !Lotei.muted
                Layout.alignment: Qt.AlignVCenter
                implicitWidth: 40
                implicitHeight: 16
                Rectangle {   // track
                    anchors.verticalCenter: parent.verticalCenter
                    width: parent.width; height: 4; radius: 2
                    color: Theme.color.mediumorange1
                    Rectangle {   // fill
                        width: parent.width * Lotei.voiceVolume
                        height: parent.height; radius: 2
                        color: Theme.color.lightorange2
                    }
                }
                Rectangle {   // handle
                    width: 9; height: 9; radius: 5
                    color: Theme.color.lightorange2
                    anchors.verticalCenter: parent.verticalCenter
                    x: parent.width * Lotei.voiceVolume - width / 2
                }
                MouseArea {
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onPressed: Lotei.voiceVolume = Math.max(0, Math.min(1, mouseX / voiceVol.width))
                    onPositionChanged: if (pressed) Lotei.voiceVolume = Math.max(0, Math.min(1, mouseX / voiceVol.width))
                }
            }

            // toggle the live Flipper-screen mirror
            Text {
                visible: Backend.screenStreamer && Backend.screenStreamer.isEnabled
                text: "screen"
                color: root.showScreen ? Theme.color.lightorange2 : Theme.color.mediumorange1
                font.family: "Share Tech Mono"
                font.pixelSize: 11
                Layout.alignment: Qt.AlignVCenter
                MouseArea {
                    anchors.fill: parent
                    anchors.margins: -4
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: root.showScreen = !root.showScreen
                }
            }
            Item { Layout.fillWidth: true }

            // Minimize / restore (collapse to title bar)
            Rectangle {
                Layout.preferredWidth: 22
                Layout.preferredHeight: 18
                radius: 3
                color: minMouse.containsMouse ? Qt.rgba(1, 1, 1, 0.14) : "transparent"

                Rectangle {
                    width: 11; height: 2; radius: 1
                    color: Theme.color.lightorange2
                    anchors.centerIn: parent
                }
                MouseArea {
                    id: minMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: root.viewState = (root.viewState === "min") ? "normal" : "min"
                }
            }

            // Maximize / restore (big read view)
            Rectangle {
                Layout.preferredWidth: 22
                Layout.preferredHeight: 18
                radius: 3
                color: maxMouse.containsMouse ? Qt.rgba(1, 1, 1, 0.14) : "transparent"

                Rectangle {
                    width: root.viewState === "max" ? 8 : 11
                    height: width
                    color: "transparent"
                    border.width: 2
                    border.color: Theme.color.lightorange2
                    anchors.centerIn: parent
                    Behavior on width { NumberAnimation { duration: 120 } }
                }
                MouseArea {
                    id: maxMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: root.viewState = (root.viewState === "max") ? "normal" : "max"
                }
            }
        }

        // ---- live Flipper screen mirror (Phase A: tap qFlipper's stream) ----
        Rectangle {
            visible: root.showScreen && Backend.screenStreamer && Backend.screenStreamer.isEnabled
            Layout.fillWidth: true
            Layout.preferredHeight: visible ? (root.viewState === "max" ? 180 : 72) : 0
            color: "#0b0410"
            radius: 5
            border.width: 1
            border.color: Theme.color.mediumorange2
            ScreenCanvas {
                anchors.fill: parent
                anchors.margins: 4
                frame: Backend.screenStreamer.screenFrame
                foregroundColor: Theme.color.lightorange2
                backgroundColor: "#0b0410"
            }
        }

        // ---- message log (hidden when minimized) ----
        ListView {
            id: listView
            visible: root.viewState !== "min"
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: chatModel
            spacing: 6
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: ScrollBar { }

            delegate: Column {
                width: ListView.view.width
                spacing: 1

                TextEdit { id: clip; visible: false }  // hidden clipboard helper

                Row {
                    spacing: 8
                    Text {
                        text: model.role === "lotei" ? root.aiName : "you"
                        color: model.role === "lotei" ? Theme.color.lightorange2 : Theme.color.mediumorange1
                        font.family: "Share Tech Mono"
                        font.pixelSize: 11
                    }
                    Text {
                        visible: model.role === "lotei"
                        text: "copy"
                        color: copyMouse.containsMouse ? Theme.color.lightorange2 : Theme.color.mediumorange1
                        font.family: "Share Tech Mono"
                        font.pixelSize: 11
                        MouseArea {
                            id: copyMouse
                            anchors.fill: parent
                            anchors.margins: -4
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: { clip.text = model.text; clip.selectAll(); clip.copy(); }
                        }
                    }
                    Text {
                        visible: model.role === "lotei" && model.text.indexOf("```") >= 0
                        text: "save→Flipper"
                        color: saveMouse.containsMouse ? Theme.color.lightorange2 : Theme.color.mediumorange1
                        font.family: "Share Tech Mono"
                        font.pixelSize: 11
                        MouseArea {
                            id: saveMouse
                            anchors.fill: parent
                            anchors.margins: -4
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                if(Lotei.thinking) return;
                                root.appendMessage("you", "save that to my Flipper");
                                Lotei.send("Save the script or code from this message onto my Flipper SD card using the save_file tool. Choose the correct folder (e.g. /ext/badusb for BadUSB, /ext/subghz for Sub-GHz, otherwise /ext) and a clear filename, then tell me where it landed. The message: " + model.text, root.deviceContext());
                            }
                        }
                    }
                }
                Text {
                    width: parent.width
                    text: model.text
                    wrapMode: Text.Wrap
                    textFormat: model.role === "lotei" ? Text.MarkdownText : Text.PlainText
                    color: "white"
                    font.family: "Share Tech Mono"
                    font.pixelSize: 13
                    onLinkActivated: function(link) { Qt.openUrlExternally(link) }
                }
            }
        }

        // ---- input row (hidden when minimized) ----
        RowLayout {
            visible: root.viewState !== "min"
            Layout.fillWidth: true
            spacing: 6

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 30
                radius: 6
                color: "black"
                border.width: 1
                border.color: Theme.color.mediumorange2

                TextInput {
                    id: input
                    anchors.fill: parent
                    anchors.leftMargin: 8
                    anchors.rightMargin: 8
                    verticalAlignment: TextInput.AlignVCenter
                    clip: true
                    color: "white"
                    selectionColor: Theme.color.lightorange2
                    font.family: "Share Tech Mono"
                    font.pixelSize: 13
                    enabled: !Lotei.thinking
                    onAccepted: root.sendCurrent()

                    Text {
                        anchors.fill: parent
                        verticalAlignment: Text.AlignVCenter
                        visible: input.text.length === 0 && !input.activeFocus
                        text: "Talk to LOTEI…"
                        color: Theme.color.mediumorange1
                        font: input.font
                    }
                }
            }

            Button {
                text: "Send"
                enabled: !Lotei.thinking && input.text.length > 0
                onClicked: root.sendCurrent()
            }
        }
    }
}
