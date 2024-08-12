// SPDX-FileCopyrightText: © 2018-2022, 2024 Alexandros Theodotou <alex@zrythm.org>
// SPDX-License-Identifier: LicenseRef-ZrythmLicense

/**
 * @file
 *
 * Musical scales.
 *
 * See https://pianoscales.org/
 */

#ifndef __AUDIO_SCALE_H__
#define __AUDIO_SCALE_H__

#include <cstdint>

#include "dsp/chord_descriptor.h"
#include "io/serialization/iserializable.h"

#include <glib/gi18n.h>

/**
 * @addtogroup dsp
 *
 * @{
 */

/**
 * Musical scale descriptor.
 */
class MusicalScale : public ISerializable<MusicalScale>
{
public:
  /**
   * Scale type (name) eg Aeolian.
   */
  enum class Type
  {
    /** All keys. */
    Chromatic,

    /* --- popular scales --- */

    Major,

    /** Natural minor. */
    Minor,

    /** Major (same as SCALE_MAJOR). */
    Ionian,

    Dorian,
    Phrygian,
    Lydian,
    Mixolydian,

    /** Natural minor (same as @ref Minor). */
    Aeolian,

    Locrian,
    MelodicMinor,
    HarmonicMinor,
    WholeTone,
    MajorPentatonic,
    MinorPentatonic,
    OctatonicHalfWhole,
    OctatonicWholeHalf,

    /* --- exotic scales --- */

    /** Lydian dominant. */
    Acoustic,

    HarmonicMajor,
    PhrygianDominant,
    MajorLocrian,
    Algerian,
    Augmented,
    DoubleHarmonic,
    Chinese,
    Diminished,
    DominantDiminished,
    Egyptian,
    EightToneSpanish,
    Enigmatic,
    Geez,
    Hindu,
    Hirajoshi,
    HungarianGypsy,
    Insen,
    NeapolitanMajor,
    NeapolitanMinor,
    Oriental,
    RomanianMinor,
    Altered,
    Maqam,
    Yo,
    BebopLocrian,
    BebopDominant,
    BebopMajor,
    SuperLocrian,
    EnigmaticMinor,
    Composite,
    Bhairav,
    HungarianMinor,
    Persian,
    Iwato,
    Kumoi,
    Pelog,
    Prometheus,
    PrometheusNeapolitan,
    PrometheusLiszt,
    Balinese,
    Ragatodi,
    Japanese1,
    Japanese2,

    /* --- TODO unimplemented --- */

    Blues,
    Flamenco,
    Gypsy,
    HalfDiminished,
    In,
    Istrian,
    LydianAugmented,
    Tritone,
    UkranianDorian,
  };

public:
  MusicalScale () = default;
  MusicalScale (Type type, MusicalNote root)
      : type_ (type), root_key_ (root) { }

  /**
   * Returns the notes in the given scale.
   *
   * @param ascending Whether to get the notes when ascending or descending
   * (some scales have different notes when rising/falling).
   */
  static const bool * get_notes_for_type (Type type, bool ascending);

  /**
   * Returns the triads in the given scale.
   *
   * There will be as many chords are enabled notes in the scale, and the rest
   * of the array will be filled with CHORD_TYPE_NONE.
   *
   * @param ascending Whether to get the triads when ascending or descending
   * (some scales have different triads when rising/falling).
   */
  RETURNS_NONNULL
  static const ChordType * get_triad_types_for_type (Type type, bool ascending);

  static const char * type_to_string (Type type);

  /**
   * Prints the MusicalScale to a string.
   */
  std::string to_string () const;

  /**
   * Returns if all of the chord's notes are in the scale.
   */
  bool contains_chord (const ChordDescriptor &chord) const;

  /**
   * Returns if the accent is in the scale.
   */
  bool is_accent_in_scale (
    MusicalNote chord_root,
    ChordType   type,
    ChordAccent chord_accent) const;

  /**
   * Returns if the given key is in the given MusicalScale.
   *
   * @param note A note inside a single octave (0-11).
   */
  bool contains_note (MusicalNote note) const;

  DECLARE_DEFINE_FIELDS_METHOD ();

public:
  /** Identification of the scale (e.g. AEOLIAN). */
  Type type_ = Type::Aeolian;

  /** Root key of the scale. */
  MusicalNote root_key_ = MusicalNote::A;
};

inline bool
operator== (const MusicalScale &lhs, const MusicalScale &rhs)
{
  return lhs.type_ == rhs.type_ && lhs.root_key_ == rhs.root_key_;
}

/**
 * @}
 */

#endif
