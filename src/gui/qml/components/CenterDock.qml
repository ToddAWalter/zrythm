// SPDX-FileCopyrightText: © 2024 Alexandros Theodotou <alex@zrythm.org>
// SPDX-License-Identifier: LicenseRef-ZrythmLicense

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Zrythm
import ZrythmStyle

ColumnLayout {
  id: root

  required property Project project
  readonly property int tempoMapLaneHeight: 24
  readonly property int tempoMapLaneSpacing: 1

  spacing: 0

  StackLayout {
    Layout.fillHeight: true
    Layout.fillWidth: true
    currentIndex: centerTabBar.currentIndex

    SplitView {
      Layout.fillHeight: true
      Layout.fillWidth: true
      orientation: Qt.Horizontal

      ColumnLayout {
        id: leftPane

        SplitView.minimumWidth: 120
        SplitView.preferredWidth: 200
        spacing: 1

        TracklistHeader {
          id: tracklistHeader

          Layout.fillWidth: true
          Layout.maximumHeight: ruler.height
          Layout.minimumHeight: ruler.height
          trackFactory: root.project.trackFactory
          tracklist: root.project.tracklist
        }

        Loader {
          id: tempoMapLegendLoader

          Layout.fillWidth: true
          Layout.preferredHeight: active ? item.implicitHeight : 0
          active: tracklistHeader.tempoMapVisible
          clip: true

          Behavior on Layout.preferredHeight {
            animation: Style.propertyAnimation
          }
          sourceComponent: TempoMapLegend {
            id: tempoMapLegend
            labelHeights: root.tempoMapLaneHeight
            spacing: root.tempoMapLaneSpacing

            tempoMap: root.project.tempoMap
          }
        }

        TracklistView {
          id: pinnedTracklist

          Layout.fillWidth: true
          Layout.preferredHeight: contentHeight
          pinned: true
          tracklist: root.project.tracklist
        }

        TracklistView {
          id: unpinnedTracklist

          Layout.fillWidth: true
          Layout.maximumHeight: unpinnedTimelineArranger.height
          Layout.minimumHeight: unpinnedTimelineArranger.height
          pinned: false
          tracklist: root.project.tracklist
        }
      }

      ColumnLayout {
        id: rightPane

        SplitView.fillHeight: true
        SplitView.fillWidth: true
        SplitView.minimumWidth: 200
        spacing: 1

        ScrollView {
          id: rulerScrollView

          Layout.fillWidth: true
          Layout.preferredHeight: ruler.height
          ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
          ScrollBar.vertical.policy: ScrollBar.AlwaysOff

          Ruler {
            id: ruler

            editorSettings: root.project.timeline.editorSettings
            tempoMap: root.project.tempoMap
            transport: root.project.transport
          }

          Binding {
            property: "contentX"
            target: rulerScrollView.contentItem
            value: root.project.timeline.editorSettings.x
          }

          Connections {
            function onContentXChanged() {
              root.project.timeline.editorSettings.x = rulerScrollView.contentItem.contentX;
            }

            target: rulerScrollView.contentItem
          }
        }

        Loader {
          id: tempoMapArrangerLoader

          Layout.fillWidth: true
          Layout.preferredHeight: active ? tempoMapLegendLoader.height : 0
          active: tracklistHeader.tempoMapVisible
          visible: active

          Behavior on Layout.preferredHeight {
            animation: Style.propertyAnimation
          }
          sourceComponent: TempoMapArranger {
            id: tempoMapArranger

            anchors.fill: parent
            clipEditor: root.project.clipEditor
            editorSettings: root.project.timeline.editorSettings
            objectFactory: root.project.arrangerObjectFactory
            ruler: ruler
            tempoMap: root.project.tempoMap
            tool: root.project.tool
            transport: root.project.transport
            laneHeight: root.tempoMapLaneHeight
            laneSpacing: root.tempoMapLaneSpacing
          }
        }

        Timeline {
          id: pinnedTimelineArranger

          Layout.fillWidth: true
          Layout.maximumHeight: pinnedTracklist.height
          Layout.minimumHeight: pinnedTracklist.height
          clipEditor: root.project.clipEditor
          objectFactory: root.project.arrangerObjectFactory
          pinned: true
          ruler: ruler
          tempoMap: root.project.tempoMap
          timeline: root.project.timeline
          tool: root.project.tool
          tracklist: root.project.tracklist
          transport: root.project.transport
        }

        Timeline {
          id: unpinnedTimelineArranger

          Layout.fillHeight: true
          Layout.fillWidth: true
          clipEditor: root.project.clipEditor
          objectFactory: root.project.arrangerObjectFactory
          pinned: false
          ruler: ruler
          tempoMap: root.project.tempoMap
          timeline: root.project.timeline
          tool: root.project.tool
          tracklist: root.project.tracklist
          transport: root.project.transport
        }
      }
    }

    Item {
      id: portConnectionsTab

      Layout.fillHeight: true
      Layout.fillWidth: true
    }

    Item {
      id: midiCcBindingsTab

      Layout.fillHeight: true
      Layout.fillWidth: true
    }
  }

  TabBar {
    id: centerTabBar

    Layout.fillWidth: true

    TabButton {
      icon.source: ResourceManager.getIconUrl("zrythm-dark", "roadmap.svg")
      text: qsTr("Timeline")
    }

    TabButton {
      icon.source: ResourceManager.getIconUrl("zrythm-dark", "connector.svg")
      text: qsTr("Port Connections")
    }

    TabButton {
      icon.source: ResourceManager.getIconUrl("zrythm-dark", "signal-midi.svg")
      text: qsTr("Midi CC Bindings")
    }
  }
}
