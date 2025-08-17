// SPDX-FileCopyrightText: © 2024-2025 Alexandros Theodotou <alex@zrythm.org>
// SPDX-License-Identifier: LicenseRef-ZrythmLicense

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import ZrythmArrangement
import Zrythm

Arranger {
  id: root

  required property bool pinned
  required property var timeline
  required property Tracklist tracklist

  function beginObjectCreation(x: real, y: real): var {
    const track = getTrackAtY(y);
    if (!track) {
      return null;
    }

    const automationTrack = getAutomationTrackAtY(y);
    if (!automationTrack) {}
    const trackLane = getTrackLaneAtY(y) as TrackLane;
    if (!trackLane) {}
    console.log("Timeline: beginObjectCreation", x, y, track, trackLane, automationTrack);

    switch (track.type) {
    case Track.Chord:
      console.log("creating chord");
      break;
    case Track.Marker:
      console.log("creating marker", Track.Marker, ArrangerObject.Marker);
      let marker = objectFactory.addMarker(Marker.Custom, track, qsTr("Custom Marker"), x / root.ruler.pxPerTick);
      root.currentAction = Arranger.CreatingMoving;
      root.setObjectSnapshotsAtStart();
      CursorManager.setClosedHandCursor();
      root.actionObject = marker;
      return marker;
    case Track.Midi:
    case Track.Instrument:
      console.log("creating midi region", track.lanes.getFirstLane());
      let region = objectFactory.addEmptyMidiRegion(track, trackLane ? trackLane : track.lanes.getFirstLane(), x / root.ruler.pxPerTick);
      root.currentAction = Arranger.CreatingResizingR;
      root.setObjectSnapshotsAtStart();
      CursorManager.setResizeEndCursor();
      root.actionObject = region;
      return region;
    default:
      return null;
    }
    return null;
  }

  function getAutomationTrackAtY(y: real): var {
    y += tracksListView.contentY;
    const trackItem = tracksListView.itemAt(0, y);
    if (!trackItem?.track?.isAutomatable || !trackItem?.track?.automationVisible) {
      return null;
    }

    y -= trackItem.y;

    // Get the automation loader instance
    const columnLayout = trackItem.children[0]; // trackSectionRows
    const loader = columnLayout.children[2]; // automationLoader
    if (!loader?.item) {
      return null;
    }

    // Get relative Y position within the automation tracks list
    const automationListY = y - loader.y;
    const automationItem = loader.item.itemAt(0, automationListY);
    console.log("Timeline: getAutomationTrackAtY", y, trackItem, loader, automationListY, automationItem);
    return automationItem?.automationTrack ?? null;
  }

  function getTrackAtY(y: real): var {
    const item = tracksListView.itemAt(0, y + tracksListView.contentY);
    return item?.track ?? null;
  }

  function getTrackLaneAtY(y: real): var {
    // Get the track delegate
    const trackItem = tracksListView.itemAt(0, y + tracksListView.contentY);
    if (!trackItem?.track?.hasLanes || !trackItem?.track?.lanesVisible) {
      return null;
    }

    // Make y relative to trackItem
    const relativeY = y + tracksListView.contentY - trackItem.y;

    // Get ColumnLayout (trackSectionRows) and laneRegionsLoader
    const columnLayout = trackItem.children[0];
    const laneLoader = columnLayout.children[1]; // laneRegionsLoader is second child
    if (!laneLoader?.item) {
      return null;
    }

    // Get relative Y position within the lanes list
    const laneListY = relativeY - laneLoader.y;
    const laneItem = laneLoader.item.itemAt(0, laneListY);
    console.log("Timeline: getTrackLaneAtY", relativeY, trackItem, laneLoader, laneListY, laneItem);
    return laneItem?.trackLane ?? null;
  }

  function updateCursor() {
    let cursor = "default";

    switch (root.currentAction) {
    case Arranger.None:
      switch (root.tool.toolValue) {
      case Tool.Select:
        CursorManager.setPointerCursor();
        return;
      case Tool.Edit:
        CursorManager.setPencilCursor();
        return;
      case Tool.Cut:
        CursorManager.setCutCursor();
        return;
      case Tool.Eraser:
        CursorManager.setEraserCursor();
        return;
      case Tool.Ramp:
        CursorManager.setRampCursor();
        return;
      case Tool.Audition:
        CursorManager.setAuditionCursor();
        return;
      }
      break;
    case Arranger.StartingDeleteSelection:
    case Arranger.DeleteSelecting:
    case Arranger.StartingErasing:
    case Arranger.Erasing:
      CursorManager.setEraserCursor();
      return;
    case Arranger.StartingMovingCopy:
    case Arranger.MovingCopy:
      CursorManager.setCopyCursor();
      return;
    case Arranger.StartingMovingLink:
    case Arranger.MovingLink:
      CursorManager.setLinkCursor();
      return;
    case Arranger.StartingMoving:
    case Arranger.CreatingMoving:
    case Arranger.Moving:
      CursorManager.setClosedHandCursor();
      return;
    case Arranger.StartingPanning:
    case Arranger.Panning:
      CursorManager.setClosedHandCursor();
      return;
    case Arranger.StretchingL:
      CursorManager.setStretchStartCursor();
      return;
    case Arranger.ResizingL:
      CursorManager.setResizeStartCursor();
      return;
    case Arranger.ResizingLLoop:
      CursorManager.setResizeLoopStartCursor();
      return;
    case Arranger.ResizingLFade:
      CursorManager.setFadeInCursor();
      return;
    case Arranger.StretchingR:
      CursorManager.setStretchEndCursor();
      return;
    case Arranger.CreatingResizingR:
    case Arranger.ResizingR:
      CursorManager.setResizeEndCursor();
      return;
    case Arranger.ResizingRLoop:
      CursorManager.setResizeLoopEndCursor();
      return;
    case Arranger.ResizingRFade:
    case Arranger.ResizingUpFadeOut:
      CursorManager.setFadeOutCursor();
      return;
    case Arranger.ResizingUpFadeIn:
      CursorManager.setFadeInCursor();
      return;
    case Arranger.Autofilling:
      CursorManager.setBrushCursor();
      return;
    case Arranger.StartingSelection:
    case Arranger.Selecting:
      CursorManager.setPointerCursor();
      return;
    case Arranger.Renaming:
      cursor = "text";
      break;
    case Arranger.Cutting:
      CursorManager.setCutCursor();
      return;
    case Arranger.StartingAuditioning:
    case Arranger.Auditioning:
      CursorManager.setAuditionCursor();
      return;
    }

    CursorManager.setPointerCursor();
    return;
  }

  editorSettings: timeline.editorSettings
  enableYScroll: !pinned
  scrollView.ScrollBar.horizontal.policy: pinned ? ScrollBar.AlwaysOff : ScrollBar.AsNeeded

  content: ListView {
    id: tracksListView

    anchors.fill: parent
    interactive: false

    delegate: Item {
      id: trackDelegate

      required property Track track

      height: track.fullVisibleHeight
      width: tracksListView.width

      TextMetrics {
        id: arrangerObjectTextMetrics

        font: Style.arrangerObjectTextFont
        text: "Some text"
      }

      ColumnLayout {
        id: trackSectionRows

        anchors.fill: parent
        spacing: 0

        Item {
          id: mainTrackObjectsContainer

          Layout.fillWidth: true
          Layout.maximumHeight: trackDelegate.track.height
          Layout.minimumHeight: trackDelegate.track.height

          Loader {
            id: scalesLoader

            active: trackDelegate.track.type === Track.Chord
            height: arrangerObjectTextMetrics.height + 2 * Style.buttonPadding
            visible: active
            width: parent.width
            y: trackDelegate.track.height - height

            sourceComponent: Repeater {
              id: scalesRepeater

              readonly property ChordTrack track: trackDelegate.track as ChordTrack

              model: track.scaleObjects

              delegate: Loader {
                id: scaleLoader

                required property var arrangerObject
                property var scaleObject: arrangerObject
                readonly property real scaleX: scaleObject.position.ticks * root.ruler.pxPerTick

                active: scaleX + 100 + Style.scrollLoaderBufferPx >= root.scrollX && scaleX <= (root.scrollX + root.scrollViewWidth + Style.scrollLoaderBufferPx)
                asynchronous: true
                visible: status === Loader.Ready

                sourceComponent: Component {
                  ScaleObjectView {
                    id: scaleItem

                    arrangerObject: scaleLoader.scaleObject
                    pxPerTick: root.ruler.pxPerTick
                    track: trackDelegate.track
                    x: scaleLoader.scaleX
                  }
                }
              }
            }
          }

          Loader {
            id: markersLoader

            active: trackDelegate.track.type === Track.Marker
            height: arrangerObjectTextMetrics.height + 2 * Style.buttonPadding
            visible: active
            width: parent.width
            y: trackDelegate.track.height - height

            sourceComponent: Repeater {
              id: markersRepeater

              readonly property MarkerTrack track: trackDelegate.track as MarkerTrack

              model: track.markers

              delegate: Loader {
                id: markerLoader

                required property var arrangerObject
                property var marker: arrangerObject
                readonly property real markerX: marker.position.ticks * root.ruler.pxPerTick

                active: markerX + 100 + Style.scrollLoaderBufferPx >= root.scrollX && markerX <= (root.scrollX + root.scrollViewWidth + Style.scrollLoaderBufferPx)
                asynchronous: true
                visible: status === Loader.Ready

                sourceComponent: Component {
                  MarkerView {
                    id: markerItem

                    arrangerObject: markerLoader.marker
                    pxPerTick: root.ruler.pxPerTick
                    track: trackDelegate.track
                    x: markerLoader.markerX
                  }
                }
              }
            }
          }

          Loader {
            id: chordRegionsLoader

            active: trackDelegate.track.type === Track.Chord
            height: trackDelegate.track.height - scalesLoader.height
            visible: active
            width: parent.width
            y: 0

            sourceComponent: Repeater {
              id: chordRegionsRepeater

              readonly property ChordTrack track: trackDelegate.track as ChordTrack

              model: track.chordRegions

              delegate: Loader {
                id: chordRegionLoader

                readonly property real chordRegionEndX: region.endPosition.ticks * root.ruler.pxPerTick
                readonly property real chordRegionWidth: chordRegionEndX - chordRegionX
                readonly property real chordRegionX: region.position.ticks * root.ruler.pxPerTick
                required property var region

                active: chordRegionEndX + Style.scrollLoaderBufferPx >= root.scrollX && chordRegionX <= (root.scrollX + root.scrollViewWidth + Style.scrollLoaderBufferPx)
                asynchronous: true
                visible: status === Loader.Ready

                sourceComponent: Component {
                  ChordRegionView {
                    arrangerObject: chordRegionLoader.region
                    clipEditor: root.clipEditor
                    height: chordRegionsRepeater.height
                    pxPerTick: root.ruler.pxPerTick
                    track: trackDelegate.track
                    width: chordRegionLoader.chordRegionWidth
                    x: chordRegionLoader.chordRegionX
                  }
                }
              }
            }
          }

          Loader {
            id: mainTrackLanedRegionsLoader

            active: trackDelegate.track.lanes !== null
            anchors.fill: parent
            visible: active

            sourceComponent: Repeater {
              id: mainTrackLanesRepeater

              model: trackDelegate.track.lanes

              delegate: Repeater {
                id: mainTrackLaneRegionsRepeater

                required property TrackLane trackLane

                model: trackDelegate.track.type === Track.Audio ? trackLane.audioRegions : trackLane.midiRegions

                delegate: Loader {
                  id: mainTrackRegionLoader

                  required property var arrangerObject
                  property var region: arrangerObject
                  readonly property real regionEndX: regionX + regionWidth
                  readonly property real regionHeight: mainTrackLanedRegionsLoader.height
                  readonly property real regionWidth: region.regionMixin.bounds.length.ticks * root.ruler.pxPerTick
                  readonly property real regionX: region.position.ticks * root.ruler.pxPerTick
                  readonly property var trackLane: mainTrackLaneRegionsRepeater.trackLane

                  active: regionEndX + Style.scrollLoaderBufferPx >= root.scrollX && regionX <= (root.scrollX + root.scrollViewWidth + Style.scrollLoaderBufferPx)
                  asynchronous: true
                  height: regionHeight
                  sourceComponent: region.type === ArrangerObject.MidiRegion ? midiRegionComponent : audioRegionComponent
                  visible: status === Loader.Ready

                  Component {
                    id: midiRegionComponent

                    MidiRegionView {
                      arrangerObject: mainTrackRegionLoader.region
                      clipEditor: root.clipEditor
                      height: mainTrackRegionLoader.regionHeight
                      lane: mainTrackLaneRegionsRepeater.trackLane
                      pxPerTick: root.ruler.pxPerTick
                      track: trackDelegate.track
                      width: mainTrackRegionLoader.regionWidth
                      x: mainTrackRegionLoader.regionX
                    }
                  }

                  Component {
                    id: audioRegionComponent

                    AudioRegionView {
                      arrangerObject: mainTrackRegionLoader.region
                      clipEditor: root.clipEditor
                      height: mainTrackRegionLoader.regionHeight
                      lane: mainTrackLaneRegionsRepeater.trackLane
                      pxPerTick: root.ruler.pxPerTick
                      track: trackDelegate.track
                      width: mainTrackRegionLoader.regionWidth
                      x: mainTrackRegionLoader.regionX
                    }
                  }
                }
              }
            }
          }
        }

        Loader {
          id: laneRegionsLoader

          Layout.fillWidth: true
          Layout.maximumHeight: Layout.preferredHeight
          Layout.minimumHeight: Layout.preferredHeight
          Layout.preferredHeight: item ? item.contentHeight : 0
          active: trackDelegate.track.lanes && trackDelegate.track.lanes.lanesVisible
          visible: active

          sourceComponent: ListView {
            id: lanesListView

            anchors.fill: parent
            clip: true
            interactive: false
            model: trackDelegate.track.lanes
            orientation: Qt.Vertical
            spacing: 0

            delegate: Item {
              id: laneItem

              required property TrackLane trackLane

              height: trackLane.height
              width: ListView.view.width

              Component {
                id: laneRegionLoaderComponent

                Loader {
                  id: laneRegionLoader

                  required property var arrangerObject
                  property var region: arrangerObject
                  readonly property real regionEndX: regionX + regionWidth
                  readonly property real regionHeight: trackLane.height
                  readonly property real regionWidth: region.regionMixin.bounds.length.ticks * root.ruler.pxPerTick
                  readonly property real regionX: region.position.ticks * root.ruler.pxPerTick
                  readonly property var trackLane: laneItem.trackLane

                  active: regionEndX + Style.scrollLoaderBufferPx >= root.scrollX && regionX <= (root.scrollX + root.scrollViewWidth + Style.scrollLoaderBufferPx)
                  asynchronous: true
                  height: regionHeight
                  sourceComponent: region.type === ArrangerObject.MidiRegion ? laneMidiRegionComponent : laneAudioRegionComponent
                  visible: status === Loader.Ready

                  Component {
                    id: laneMidiRegionComponent

                    MidiRegionView {
                      arrangerObject: laneRegionLoader.region
                      clipEditor: root.clipEditor
                      height: laneItem.trackLane.height
                      lane: laneItem.trackLane
                      pxPerTick: root.ruler.pxPerTick
                      track: trackDelegate.track
                      width: laneRegionLoader.regionWidth
                      x: laneRegionLoader.regionX
                    }
                  }

                  Component {
                    id: laneAudioRegionComponent

                    AudioRegionView {
                      arrangerObject: laneRegionLoader.region
                      clipEditor: root.clipEditor
                      height: laneItem.trackLane.height
                      lane: laneItem.trackLane
                      pxPerTick: root.ruler.pxPerTick
                      track: trackDelegate.track
                      width: laneRegionLoader.regionWidth
                      x: laneRegionLoader.regionX
                    }
                  }
                }
              }

              Repeater {
                id: laneMidiRegionsRepeater

                anchors.fill: parent
                delegate: laneRegionLoaderComponent
                model: laneItem.trackLane.midiRegions
              }

              Repeater {
                id: laneAudioRegionsRepeater

                anchors.fill: parent
                delegate: laneRegionLoaderComponent
                model: laneItem.trackLane.audioRegions
              }
            }
          }
        }

        Loader {
          id: automationLoader

          readonly property AutomationTracklist automationTracklist: trackDelegate.track.automationTracklist

          Layout.fillWidth: true
          Layout.maximumHeight: Layout.preferredHeight
          Layout.minimumHeight: Layout.preferredHeight
          Layout.preferredHeight: item ? item.contentHeight : 0
          active: trackDelegate.track.automationTracklist !== null && automationTracklist.automationVisible
          visible: active

          sourceComponent: ListView {
            id: automationTracksListView

            anchors.fill: parent
            clip: true
            interactive: false
            orientation: Qt.Vertical
            spacing: 0

            delegate: Item {
              id: automationTrackItem

              readonly property var automationTrack: automationTrackHolder.automationTrack
              required property var automationTrackHolder

              height: automationTrackHolder.height
              width: ListView.view.width

              Repeater {
                id: automationRegionsRepeater

                anchors.fill: parent
                model: automationTrackItem.automationTrack.regions

                delegate: Loader {
                  id: automationRegionLoader

                  readonly property var automationTrack: automationTrackItem.automationTrack
                  required property var region
                  readonly property real regionEndX: region.endPosition.ticks * root.ruler.pxPerTick
                  readonly property real regionHeight: parent.height
                  readonly property real regionWidth: regionEndX - regionX
                  readonly property real regionX: region.position.ticks * root.ruler.pxPerTick

                  active: regionEndX + Style.scrollLoaderBufferPx >= root.scrollX && regionX <= (root.scrollX + root.scrollViewWidth + Style.scrollLoaderBufferPx)
                  asynchronous: true
                  height: regionHeight
                  visible: status === Loader.Ready

                  sourceComponent: AutomationRegionView {
                    arrangerObject: automationRegionLoader.region
                    automationTrack: automationRegionLoader.automationTrack
                    clipEditor: root.clipEditor
                    height: automationTrackItem.automationTrackHolder.height
                    pxPerTick: root.ruler.pxPerTick
                    track: trackDelegate.track
                    width: automationRegionLoader.regionWidth
                    x: automationRegionLoader.regionX
                  }
                }
              }
            }
            model: AutomationTracklistProxyModel {
              showOnlyCreated: true
              showOnlyVisible: true
              sourceModel: automationLoader.automationTracklist
            }
          }
        }
      }
    }
    model: TrackFilterProxyModel {
      sourceModel: root.tracklist

      Component.onCompleted: {
        addVisibilityFilter(true);
        addPinnedFilter(root.pinned);
      }
    }
  }
}
