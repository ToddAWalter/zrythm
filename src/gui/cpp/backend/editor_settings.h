// SPDX-FileCopyrightText: © 2020-2022, 2024 Alexandros Theodotou <alex@zrythm.org>
// SPDX-License-Identifier: LicenseRef-ZrythmLicense

#ifndef __GUI_BACKEND_EDITOR_SETTINGS_H__
#define __GUI_BACKEND_EDITOR_SETTINGS_H__

#include "common/io/serialization/iserializable.h"

/**
 * @addtogroup gui_backend
 *
 * @{
 */

/**
 * Common editor settings.
 */
class EditorSettings : public ISerializable<EditorSettings>
{
public:
  void set_scroll_start_x (int x, bool validate);

  void set_scroll_start_y (int y, bool validate);

  /**
   * Appends the given deltas to the scroll x/y values.
   */
  void append_scroll (int dx, int dy, bool validate);

protected:
  void copy_members_from (const EditorSettings &other)
  {
    scroll_start_x_ = other.scroll_start_x_;
    scroll_start_y_ = other.scroll_start_y_;
    hzoom_level_ = other.hzoom_level_;
  }

  DECLARE_DEFINE_BASE_FIELDS_METHOD ();

public:
  /** Horizontal scroll start position. */
  int scroll_start_x_ = 0;

  /** Vertical scroll start position. */
  int scroll_start_y_ = 0;

  /** Horizontal zoom level. */
  double hzoom_level_ = 1.0;
};

class Timeline;
class PianoRoll;
class AutomationEditor;
class ChordEditor;
class AudioClipEditor;
using EditorSettingsVariant = std::
  variant<Timeline, PianoRoll, AutomationEditor, ChordEditor, AudioClipEditor>;
using EditorSettingsPtrVariant = to_pointer_variant<EditorSettingsVariant>;

/**
 * @}
 */

#endif
