// SPDX-FileCopyrightText: © 2024 Alexandros Theodotou <alex@zrythm.org>
// SPDX-License-Identifier: LicenseRef-ZrythmLicense

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Dialogs
import QtQuick.Layouts

RowLayout {
    id: control

    property string buttonLabel: qsTr("Browse")
    property string initialPath // optional path to use as initial value (auto-sets initialUrl)
    property url initialUrl // optional URL to use as initial value
    property url selectedUrl: pickFolder ? folderDialog.currentFolder : fileDialog.currentFile // to communicate the result to the parent
    property bool pickFolder: true // whether to pick a folder or a file

    Binding {
        target: control
        property: "initialUrl"
        value: Qt.resolvedUrl("file://" + initialPath)
        when: control.initialPath !== undefined
    }

    Binding {
        target: fileDialog
        property: "currentFile"
        value: control.initialUrl
        when: !pickFolder && control.initialUrl !== undefined
    }

    Binding {
        target: folderDialog
        property: "currentFolder"
        value: control.initialUrl
        when: pickFolder && control.initialUrl !== undefined
    }

    TextField {
        id: textField

        readonly property url currentUrl: pickFolder ? folderDialog.currentFolder : fileDialog.currentFile

        Layout.fillWidth: true
        text: currentUrl ? currentUrl.toString().replace("file://", "") : "(no path selected)"
        readOnly: true
    }

    ZrythmButton {
        id: btn

        text: control.buttonLabel
        onClicked: pickFolder ? folderDialog.open() : fileDialog.open()
    }

    FolderDialog {
        id: folderDialog
    }

    FileDialog {
        id: fileDialog

        fileMode: FileDialog.OpenFile
    }

}
