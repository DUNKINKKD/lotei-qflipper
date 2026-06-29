pragma Singleton

import QtQuick 2.15
import QFlipper 1.0

QtObject {
    // Every color flows from the runtime-editable Palette singleton, so the
    // color editor recolors the whole UI live. (transparent stays literal.)
    readonly property var color: QtObject {
        readonly property color transparent: Qt.rgba(0, 0, 0, 0)

        readonly property color lightorange1: Palette.colors.lightorange1
        readonly property color lightorange2: Palette.colors.lightorange2
        readonly property color lightorange3: Palette.colors.lightorange3
        readonly property color darkorange1: Palette.colors.darkorange1
        readonly property color darkorange2: Palette.colors.darkorange2
        readonly property color mediumorange1: Palette.colors.mediumorange1
        readonly property color mediumorange2: Palette.colors.mediumorange2
        readonly property color mediumorange3: Palette.colors.mediumorange3
        readonly property color mediumorange4: Palette.colors.mediumorange4
        readonly property color mediumorange5: Palette.colors.mediumorange5

        readonly property color lightgreen: Palette.colors.lightgreen
        readonly property color mediumgreen1: Palette.colors.mediumgreen1
        readonly property color mediumgreen2: Palette.colors.mediumgreen2
        readonly property color darkgreen: Palette.colors.darkgreen

        readonly property color lightblue: Palette.colors.lightblue
        readonly property color mediumblue: Palette.colors.mediumblue
        readonly property color darkblue1: Palette.colors.darkblue1
        readonly property color darkblue2: Palette.colors.darkblue2

        readonly property color lightred1: Palette.colors.lightred1
        readonly property color lightred2: Palette.colors.lightred2
        readonly property color lightred3: Palette.colors.lightred3
        readonly property color lightred4: Palette.colors.lightred4
        readonly property color mediumred1: Palette.colors.mediumred1
        readonly property color mediumred2: Palette.colors.mediumred2
        readonly property color darkred1: Palette.colors.darkred1
        readonly property color darkred2: Palette.colors.darkred2
    }

    readonly property var timing: QtObject {
        readonly property int toolTipDelay: 500
    }
}
