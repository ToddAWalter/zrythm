// SPDX-FileCopyrightText: © 2018-2022 Alexandros Theodotou <alex@zrythm.org>
// SPDX-License-Identifier: LicenseRef-ZrythmLicense

/**
 * \file
 *
 * Descriptors for chords.
 */

#ifndef __AUDIO_CHORD_DESCRIPTOR_H__
#define __AUDIO_CHORD_DESCRIPTOR_H__

#include <stdint.h>

#include "dsp/position.h"
#include "utils/yaml.h"

/**
 * @addtogroup dsp
 *
 * @{
 */

#define CHORD_DESCRIPTOR_SCHEMA_VERSION 2

#define CHORD_DESCRIPTOR_MAX_NOTES 48

enum class MusicalNote
{
  NOTE_C = 0,
  NOTE_CS,
  NOTE_D,
  NOTE_DS,
  NOTE_E,
  NOTE_F,
  NOTE_FS,
  NOTE_G,
  NOTE_GS,
  NOTE_A,
  NOTE_AS,
  NOTE_B
};

/**
 * Chord type.
 */
enum class ChordType
{
  CHORD_TYPE_NONE,
  CHORD_TYPE_MAJ,
  CHORD_TYPE_MIN,
  CHORD_TYPE_DIM,
  CHORD_TYPE_SUS4,
  CHORD_TYPE_SUS2,
  CHORD_TYPE_AUG,
  CHORD_TYPE_CUSTOM,
};

/**
 * Chord accents.
 */
enum class ChordAccent
{
  CHORD_ACC_NONE,
  /** b7 is 10 semitones from chord root, or 9
   * if the chord is diminished. */
  CHORD_ACC_7,
  /** Maj7 is 11 semitones from the root. */
  CHORD_ACC_j7,
  /* NOTE: all accents below assume 7 */
  /** 13 semitones. */
  CHORD_ACC_b9,
  /** 14 semitones. */
  CHORD_ACC_9,
  /** 15 semitones. */
  CHORD_ACC_S9,
  /** 17 semitones. */
  CHORD_ACC_11,
  /** 6 and 18 semitones. */
  CHORD_ACC_b5_S11,
  /** 8 and 16 semitones. */
  CHORD_ACC_S5_b13,
  /** 9 and 21 semitones. */
  CHORD_ACC_6_13,
};

/**
 * A ChordDescriptor describes a chord and is not
 * linked to any specific object by itself.
 *
 * Chord objects should include a ChordDescriptor.
 */
typedef struct ChordDescriptor
{
  /** Has bass note or not. */
  bool has_bass;

  /** Root note. */
  MusicalNote root_note;

  /** Bass note 1 octave below. */
  MusicalNote bass_note;

  /** Chord type. */
  ChordType type;

  /**
   * Chord accent.
   *
   * Does not apply to custom chords.
   */
  ChordAccent accent;

  /**
   * Only used if custom chord.
   *
   * 4 octaves, 1st octave is where bass note is,
   * but bass note should not be part of this.
   *
   * Starts at C always, from MIDI pitch 36.
   */
  int notes[CHORD_DESCRIPTOR_MAX_NOTES];

  /**
   * 0 no inversion,
   * less than 0 highest note(s) drop an octave,
   * greater than 0 lowest note(s) receive an octave.
   */
  int inversion;
} ChordDescriptor;

/**
 * Creates a ChordDescriptor.
 */
ChordDescriptor *
chord_descriptor_new (
  MusicalNote root,
  int         has_bass,
  MusicalNote bass,
  ChordType   type,
  ChordAccent accent,
  int         inversion);

static inline int
chord_descriptor_get_max_inversion (const ChordDescriptor * const self)
{
  int max_inv = 2;
  switch (self->accent)
    {
    case ChordAccent::CHORD_ACC_NONE:
      break;
    case ChordAccent::CHORD_ACC_7:
    case ChordAccent::CHORD_ACC_j7:
    case ChordAccent::CHORD_ACC_b9:
    case ChordAccent::CHORD_ACC_9:
    case ChordAccent::CHORD_ACC_S9:
    case ChordAccent::CHORD_ACC_11:
      max_inv = 3;
      break;
    case ChordAccent::CHORD_ACC_b5_S11:
    case ChordAccent::CHORD_ACC_S5_b13:
    case ChordAccent::CHORD_ACC_6_13:
      max_inv = 4;
      break;
    default:
      break;
    }

  return max_inv;
}

static inline int
chord_descriptor_get_min_inversion (const ChordDescriptor * const self)
{
  return -chord_descriptor_get_max_inversion (self);
}

static inline int
chord_descriptor_are_notes_equal (int * notes_a, int * notes_b)
{
  /* 36 notes in Chord */
  for (int i = 0; i < 36; i++)
    {
      if (notes_a[i] != notes_b[i])
        return 0;
    }
  return 1;
}

static inline int
chord_descriptor_is_equal (ChordDescriptor * a, ChordDescriptor * b)
{
  return a->has_bass == b->has_bass && a->root_note == b->root_note
         && a->bass_note == b->bass_note && a->type == b->type
         && chord_descriptor_are_notes_equal (a->notes, b->notes)
         && a->inversion == b->inversion;
}

/**
 * Returns if the given key is in the chord
 * represented by the given ChordDescriptor.
 *
 * @param key A note inside a single octave (0-11).
 */
bool
chord_descriptor_is_key_in_chord (ChordDescriptor * chord, MusicalNote key);

/**
 * Returns if @ref key is the bass or root note of
 * @ref chord.
 *
 * @param key A note inside a single octave (0-11).
 */
bool
chord_descriptor_is_key_bass (ChordDescriptor * chord, MusicalNote key);

/**
 * Clones the given ChordDescriptor.
 */
ChordDescriptor *
chord_descriptor_clone (const ChordDescriptor * src);

void
chord_descriptor_copy (ChordDescriptor * dest, const ChordDescriptor * src);

/**
 * Returns the chord type as a string (eg. "aug").
 */
const char *
chord_descriptor_chord_type_to_string (ChordType type);

/**
 * Returns the chord accent as a string (eg. "j7").
 */
const char *
chord_descriptor_chord_accent_to_string (ChordAccent accent);

/**
 * Returns the musical note as a string (eg. "C3").
 */
const char *
chord_descriptor_note_to_string (MusicalNote note);

/**
 * Returns the chord in human readable string.
 *
 * MUST be free'd by caller.
 */
char *
chord_descriptor_to_new_string (const ChordDescriptor * chord);

/**
 * Returns the chord in human readable string.
 */
NONNULL void
chord_descriptor_to_string (const ChordDescriptor * chord, char * str);

/**
 * Updates the notes array based on the current
 * settings.
 */
NONNULL void
chord_descriptor_update_notes (ChordDescriptor * self);

/**
 * Frees the ChordDescriptor.
 */
NONNULL void
chord_descriptor_free (ChordDescriptor * self);

/**
 * @}
 */

#endif
