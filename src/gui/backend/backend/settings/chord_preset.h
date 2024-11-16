// SPDX-FileCopyrightText: © 2022, 2024 Alexandros Theodotou <alex@zrythm.org>
// SPDX-License-Identifier: LicenseRef-ZrythmLicense

#ifndef __SETTINGS_CHORD_PRESET_H__
#define __SETTINGS_CHORD_PRESET_H__

#include "zrythm-config.h"

#include "common/dsp/chord_descriptor.h"
#include "common/utils/types.h"
#include "gui/backend/backend/chord_editor.h"

class ChordPresetPack;
TYPEDEF_STRUCT_UNDERSCORED (GMenuModel);

/**
 * @addtogroup settings
 *
 * @{
 */

/**
 * A preset of chord descriptors.
 */
class ChordPreset final
    : public QObject,
      public ICloneable<ChordPreset>,
      public ISerializable<ChordPreset>
{
  Q_OBJECT
  QML_ELEMENT
  Q_PROPERTY (QString name READ getName WRITE setName NOTIFY nameChanged)
public:
  using NameT = QString;
  ChordPreset (QObject * parent = nullptr);
  ChordPreset (const NameT &name, QObject * parent = nullptr);

  // =================================================================
  // QML Interface
  // =================================================================

  NameT         getName () const;
  void          setName (const NameT &name);
  Q_SIGNAL void nameChanged (const NameT &name);

  // =================================================================

  /**
   * Gets informational text.
   */
  std::string get_info_text () const;

  // GMenuModel * generate_context_menu () const;

  void init_after_cloning (const ChordPreset &other) override;

  DECLARE_DEFINE_FIELDS_METHOD ();

public:
  /** Preset name. */
  NameT name_;

  /** Chord descriptors. */
  std::vector<ChordDescriptor> descr_;

  /** Pointer to owner pack. */
  ChordPresetPack * pack_ = nullptr;
};

inline bool
operator== (const ChordPreset &lhs, const ChordPreset &rhs)
{
  return lhs.name_ == rhs.name_ && lhs.descr_ == rhs.descr_;
}

/**
 * @}
 */

#endif
