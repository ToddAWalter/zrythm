// SPDX-FileCopyrightText: © 2024 Alexandros Theodotou <alex@zrythm.org>
// SPDX-FileCopyrightText: Copyright (C) 2017 The Qt Company Ltd.
// SPDX-License-Identifier: GPL-3.0-only

import QtQuick
import QtQuick.Templates as T
import ZrythmStyle 1.0

T.Popup {
  id: control

  implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset, contentHeight + topPadding + bottomPadding)
  implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset, contentWidth + leftPadding + rightPadding)
  padding: 12
  popupType: Popup.Native

  T.Overlay.modal: Rectangle {
    color: Color.transparent(control.palette.shadow, 0.5)
  }
  T.Overlay.modeless: Rectangle {
    color: Color.transparent(control.palette.shadow, 0.12)
  }
  background: PopupBackgroundRect {
  }
}
