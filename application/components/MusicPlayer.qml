import QtQuick 2.15
import QtQuick.Layouts 1.15
import QtMultimedia
import Qt.labs.folderlistmodel 2.15

import Theme 1.0
import QFlipper 1.0

// Tiny shuffle music player. Drop .mp3 files into the Music folder next to
// qFlipper.exe and they auto-play on shuffle when the app starts.
Rectangle {
    id: root

    radius: 6
    color: "#0b0410"
    border.width: 1
    border.color: Theme.color.mediumorange2

    property var tracks: []
    property int idx: -1
    property bool muted: false

    function shuffle(a) {
        var r = a.slice();
        for (var i = r.length - 1; i > 0; i--) {
            var j = Math.floor(Math.random() * (i + 1));
            var t = r[i]; r[i] = r[j]; r[j] = t;
        }
        return r;
    }
    function playNext() {
        if (tracks.length === 0) return;
        idx = idx + 1;
        if (idx >= tracks.length) { tracks = shuffle(tracks); idx = 0; }
        player.source = tracks[idx];
        player.play();
    }
    function trackName() {
        if (idx < 0 || idx >= tracks.length) return "no music — add .mp3 to /Music";
        var s = "" + tracks[idx];
        s = decodeURIComponent(s.substring(s.lastIndexOf("/") + 1));
        return s.replace(/\.mp3$/i, "");
    }

    FolderListModel {
        id: folderModel
        folder: Lotei.musicFolderUrl()
        nameFilters: ["*.mp3"]
        showDirs: false
        onStatusChanged: {
            if (status === FolderListModel.Ready) {
                var list = [];
                for (var i = 0; i < count; i++) list.push(get(i, "fileUrl"));
                root.tracks = root.shuffle(list);
                root.idx = -1;
                root.playNext();
            }
        }
    }

    MediaPlayer {
        id: player
        audioOutput: AudioOutput { volume: root.muted ? 0.0 : Lotei.musicVolume }
        onMediaStatusChanged: { if (mediaStatus === MediaPlayer.EndOfMedia) root.playNext(); }
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 9
        anchors.rightMargin: 9
        spacing: 9

        Text {
            text: root.muted ? "♪x" : "♪"
            color: noteMouse.containsMouse ? Theme.color.lightorange2 : Theme.color.mediumorange1
            font.pixelSize: 14
            MouseArea { id: noteMouse; anchors.fill: parent; anchors.margins: -4; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: root.muted = !root.muted }
        }
        Item {
            id: musicVol
            visible: !root.muted
            Layout.alignment: Qt.AlignVCenter
            implicitWidth: 44
            implicitHeight: 14
            Rectangle {   // track
                anchors.verticalCenter: parent.verticalCenter
                width: parent.width; height: 4; radius: 2
                color: Theme.color.mediumorange1
                Rectangle {   // fill
                    width: parent.width * Lotei.musicVolume
                    height: parent.height; radius: 2
                    color: Theme.color.lightorange2
                }
            }
            Rectangle {   // handle
                width: 9; height: 9; radius: 5
                color: Theme.color.lightorange2
                anchors.verticalCenter: parent.verticalCenter
                x: parent.width * Lotei.musicVolume - width / 2
            }
            MouseArea {
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onPressed: Lotei.musicVolume = Math.max(0, Math.min(1, mouseX / musicVol.width))
                onPositionChanged: if (pressed) Lotei.musicVolume = Math.max(0, Math.min(1, mouseX / musicVol.width))
            }
        }
        Text {
            Layout.fillWidth: true
            text: root.trackName()
            color: Theme.color.lightorange2
            font.family: "HaxrCorp 4089"
            font.pixelSize: 30
            font.capitalization: Font.AllUppercase
            elide: Text.ElideRight
        }
        Text {
            text: player.playbackState === MediaPlayer.PlayingState ? "||" : ">"
            color: ppMouse.containsMouse ? Theme.color.lightorange2 : Theme.color.mediumorange1
            font.family: "Share Tech Mono"
            font.pixelSize: 13
            MouseArea { id: ppMouse; anchors.fill: parent; anchors.margins: -5; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                onClicked: player.playbackState === MediaPlayer.PlayingState ? player.pause() : player.play() }
        }
        Text {
            text: ">>"
            color: nxMouse.containsMouse ? Theme.color.lightorange2 : Theme.color.mediumorange1
            font.family: "Share Tech Mono"
            font.pixelSize: 13
            MouseArea { id: nxMouse; anchors.fill: parent; anchors.margins: -5; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: root.playNext() }
        }
    }
}
