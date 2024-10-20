// SPDX-FileCopyrightText: © 2024 Alexandros Theodotou <alex@zrythm.org>
// SPDX-FileCopyrightText: Copyright (C) 2017 The Qt Company Ltd.
// SPDX-License-Identifier: GPL-3.0-only

import QtQuick
import QtQuick.Controls.impl
import QtQuick.Templates as T
import ZrythmStyle 1.0

T.ItemDelegate {
    id: control

    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset, implicitContentWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset, implicitContentHeight + topPadding + bottomPadding, implicitIndicatorHeight + topPadding + bottomPadding)
    padding: 4
    spacing: 4
    icon.width: 24
    icon.height: 24
    icon.color: control.palette.text
    font: Style.semiBoldTextFont

    contentItem: IconLabel {
        spacing: control.spacing
        mirrored: control.mirrored
        display: control.display
        alignment: control.display === IconLabel.IconOnly || control.display === IconLabel.TextUnderIcon ? Qt.AlignCenter : Qt.AlignLeft
        icon: control.icon
        text: control.text
        font: control.font
        color: control.highlighted ? control.palette.highlightedText : control.palette.text
    }

    background: Rectangle {
        readonly property color baseColor: control.highlighted ? control.palette.highlight : control.palette.button
        readonly property color colorAdjustedForHoverOrFocusOrDown: Style.adjustColorForHoverOrVisualFocusOrDown(baseColor, /*control.hovered*/ false, control.visualFocus, control.down)

        implicitWidth: 100
        implicitHeight: Style.buttonHeight
        visible: control.down || control.highlighted || control.visualFocus
        color: colorAdjustedForHoverOrFocusOrDown

        Behavior on color {
            animation: Style.propertyAnimation
        }

    }

}
