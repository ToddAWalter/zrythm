// SPDX-FileCopyrightText: © 2025 Alexandros Theodotou <alex@zrythm.org>
// SPDX-License-Identifier: LicenseRef-ZrythmLicense

import QtQuick
import ZrythmStyle
import Zrythm

Rectangle {
  id: root

  required property ArrangerObject arrangerObject
  required property real xOnConstruction

  color: palette.accent
  height: 16
  radius: 4
  width: 100

  Text {
    text: ArrangerObjectHelpers.getObjectName(root.arrangerObject).name
  }
}
