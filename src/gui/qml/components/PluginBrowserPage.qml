// SPDX-FileCopyrightText: © 2025 Alexandros Theodotou <alex@zrythm.org>
// SPDX-License-Identifier: LicenseRef-ZrythmLicense

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Zrythm 1.0
import ZrythmStyle 1.0

ColumnLayout {
  id: root

  required property var pluginManager

  Item {
    id: filters

  }

  Item {
    id: searchBar

  }

  ListView {
    id: pluginListView

    readonly property var selectionModel: ItemSelectionModel {
      model: pluginListView.model
    }

    Layout.fillHeight: true
    Layout.fillWidth: true
    activeFocusOnTab: true
    clip: true
    focus: true
    model: root.pluginManager.pluginDescriptors

    ScrollBar.vertical: ScrollBar {
    }
    delegate: ItemDelegate {
      id: itemDelegate

      required property var descriptor
      required property int index

      highlighted: ListView.isCurrentItem
      text: descriptor.name
      width: pluginListView.width

      action: Action {
        text: "Activate"

        onTriggered: {
          console.log("activated", descriptor, descriptor.name);
          root.pluginManager.createPluginInstance(descriptor);
        }
      }

      // This avoids the delegate onClicked triggering the action
      MouseArea {
        anchors.fill: parent

        onClicked: pluginListView.currentIndex = itemDelegate.index
        onDoubleClicked: itemDelegate.animateClick()
      }
    }

    Keys.onDownPressed: incrementCurrentIndex()
    Keys.onUpPressed: decrementCurrentIndex()

    Binding {
      property: "descriptor"
      target: pluginInfoLabel
      value: pluginListView.currentItem ? pluginListView.currentItem.descriptor : null
    }
  }

  Label {
    id: pluginInfoLabel

    property var descriptor

    Layout.fillWidth: true
    text: descriptor ? "Plugin Info:\n" + descriptor.name + "\n" + descriptor.format : "No plugin selected"
  }
}
