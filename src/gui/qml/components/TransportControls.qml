// SPDX-FileCopyrightText: © 2024 Alexandros Theodotou <alex@zrythm.org>
// SPDX-License-Identifier: LicenseRef-ZrythmLicense

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Zrythm 1.0
import ZrythmStyle 1.0

RowLayout {
    id: root

    required property var transport
    required property var tempoMap

    LinkedButtons {
        Button {
            icon.source: ResourceManager.getIconUrl("gnome-icon-library", "seek-backward-large-symbolic.svg")
            onClicked: transport.moveBackward()
        }

        Button {
            icon.source: ResourceManager.getIconUrl("gnome-icon-library", "seek-forward-large-symbolic.svg")
            onClicked: transport.moveForward()
        }

        Button {
            icon.source: ResourceManager.getIconUrl("gnome-icon-library", "skip-backward-large-symbolic.svg")
        }

        Button {
            icon.source: ResourceManager.getIconUrl("gnome-icon-library", (transport.playState == 1 ? "pause" : "play") + "-large-symbolic.svg")
            onClicked: {
                transport.isRolling() ? transport.requestPause(true) : transport.requestRoll(true);
            }
        }

        RecordButton {
            id: recordButton

            checked: transport.recordEnabled
            onCheckedChanged: {
                transport.recordEnabled = checked;
            }
        }

        Button {
            checkable: true
            checked: transport.loopEnabled
            onCheckedChanged: {
                transport.loopEnabled = checked;
            }
            icon.source: ResourceManager.getIconUrl("gnome-icon-library", "loop-arrow-symbolic.svg")
        }

    }

    RowLayout {
        spacing: 2

        EditableValueDisplay {
            value: root.tempoMap.tempoAtTick(transport.playhead.ticks).toFixed(2)
            label: "bpm"
        }

        EditableValueDisplay {
            id: timeDisplay

            value: root.tempoMap.getMusicalPositionString(transport.playhead.ticks)
            label: "time"
            minValueWidth: timeTextMetrics.width
            minValueHeight: timeTextMetrics.height

            TextMetrics {
                id: timeTextMetrics

                text: "99.9.9.999"
                font: Style.semiBoldTextFont
            }

        }

        EditableValueDisplay {
            value: {
              let timeSig = root.tempoMap.timeSignatureAtTick(transport.playhead.ticks);
              return `${timeSig.numerator}/${timeSig.denominator}`;
            }
            label: "sig"
        }

    }

}
