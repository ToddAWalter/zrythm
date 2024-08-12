// SPDX-FileCopyrightText: © 2018-2020, 2024 Alexandros Theodotou <alex@zrythm.org>
// SPDX-License-Identifier: LicenseRef-ZrythmLicense

#ifndef __AUDIO_INSTRUMENT_TRACK_H__
#define __AUDIO_INSTRUMENT_TRACK_H__

#include "dsp/group_target_track.h"
#include "dsp/piano_roll_track.h"

/**
 * @addtogroup dsp
 *
 * @{
 */

class InstrumentTrack final
    : public GroupTargetTrack,
      public PianoRollTrack,
      public ICloneable<InstrumentTrack>,
      public ISerializable<InstrumentTrack>
{
public:
  // Rule of 0
  InstrumentTrack () = default;

  /**
   * @brief Main constructor.
   *
   * @param name Track name.
   * @param pos Track position.
   */
  InstrumentTrack (const std::string &name, int pos);

  void init_loaded () override;

  void init_after_cloning (const InstrumentTrack &other) override;

  Plugin * get_instrument ();

  const Plugin * get_instrument () const;

  /**
   * Returns if the first plugin's UI in the instrument track is visible.
   */
  bool is_plugin_visible () const;

  /**
   * Toggles whether the first plugin's UI in the instrument Track is visible.
   */
  void toggle_plugin_visible ();

  bool validate () const override
  {
    return GroupTargetTrack::validate () && PianoRollTrack::validate ();
  }

  DECLARE_DEFINE_FIELDS_METHOD ();

public:
};

/**
 * @}
 */

#endif // __AUDIO_INSTRUMENT_TRACK_H__
