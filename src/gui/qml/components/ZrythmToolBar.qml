import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

ToolBar {
    id: root

    property alias leftItems: leftSection.children
    property alias centerItems: centerSection.children
    property alias rightItems: rightSection.children
    property double verticalMargin: 2 // bottom and top margin

    background: Rectangle {
        color: palette.window
    }

    contentItem: RowLayout {
        spacing: 4

        RowLayout {
            /*
            ZrythmToolButton {
                text: "test"
            }

            ZrythmToolButton {
                text: "test"
                iconSource: Qt.resolvedUrl("../icons/zrythm.svg")
            }

            ZrythmToolButton {
                iconSource: Qt.resolvedUrl("../icons/zrythm.svg")
            }

            ZrythmSplitButton {
                text: "test"
            }
            */

            id: leftSection

            Layout.fillWidth: true
            Layout.alignment: Qt.AlignLeft
            Layout.bottomMargin: root.verticalMargin
            Layout.topMargin: root.verticalMargin
        }

        RowLayout {
            id: centerSection

            Layout.alignment: Qt.AlignCenter
            Layout.bottomMargin: root.verticalMargin
            Layout.topMargin: root.verticalMargin
        }

        RowLayout {
            id: rightSection

            Layout.alignment: Qt.AlignRight
            Layout.bottomMargin: root.verticalMargin
            Layout.topMargin: root.verticalMargin
        }

    }

}
