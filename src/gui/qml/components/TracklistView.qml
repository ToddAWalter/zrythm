// SPDX-FileCopyrightText: © 2024-2025 Alexandros Theodotou <alex@zrythm.org>
// SPDX-License-Identifier: LicenseRef-ZrythmLicense

pragma ComponentBehavior: Bound

import QtQuick
import Zrythm

Item {
  id: root

  readonly property real contentHeight: listView.contentHeight + (pinned ? 0 : dropSpace.height)
  required property bool pinned
  required property TrackSelectionManager trackSelectionManager
  required property Tracklist tracklist
  required property UndoStack undoStack

  ListView {
    id: listView

    boundsBehavior: Flickable.StopAtBounds
    implicitHeight: 200
    implicitWidth: 200

    delegate: TrackView {
      trackSelectionManager: root.trackSelectionManager
      tracklist: root.tracklist
      undoStack: root.undoStack
      width: ListView.view.width
    }
    model: TrackFilterProxyModel {
      sourceModel: root.tracklist.collection

      Component.onCompleted: {
        addVisibilityFilter(true);
        addPinnedFilter(root.pinned);
      }
    }

    anchors {
      bottom: root.pinned ? parent.bottom : dropSpace.top
      left: parent.left
      right: parent.right
      top: parent.top
    }
  }

  Item {
    id: dropSpace

    height: 40
    visible: !root.pinned

    ContextMenu.menu: Menu {
      MenuItem {
        text: qsTr("Test")

        onTriggered: console.log("Clicked")
      }
    }

    anchors {
      bottom: parent.bottom
      left: parent.left
      right: parent.right
    }
  }
}
