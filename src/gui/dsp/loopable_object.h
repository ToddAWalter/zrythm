// SPDX-FileCopyrightText: © 2024 Alexandros Theodotou <alex@zrythm.org>
// SPDX-License-Identifier: LicenseRef-ZrythmLicense

#ifndef __DSP_LOOPABLE_OBJECT_H__
#define __DSP_LOOPABLE_OBJECT_H__

#include "gui/dsp/bounded_object.h"
#include "utils/types.h"

#define DEFINE_LOOPABLE_OBJECT_QML_PROPERTIES(ClassType) \
  DEFINE_BOUNDED_OBJECT_QML_PROPERTIES (ClassType)

class LoopableObject
    : virtual public BoundedObject,
      public zrythm::utils::serialization::ISerializable<LoopableObject>
{
public:
  ~LoopableObject () override = default;

  /**
   * Returns the number of loops in the ArrangerObject, optionally including
   * incomplete ones.
   */
  int get_num_loops (bool count_incomplete) const;

  /**
   * Getter.
   */
  void get_clip_start_pos (dsp::Position * pos) const
  {
    *pos = clip_start_pos_;
  }

  /**
   * Getter.
   */
  void get_loop_start_pos (dsp::Position * pos) const
  {
    *pos = loop_start_pos_;
  }

  /**
   * Getter.
   */
  void get_loop_end_pos (dsp::Position * pos) const { *pos = loop_end_pos_; }

  /**
   * The setter is for use in e.g. the digital meters whereas the set_pos func
   * is used during arranger actions.
   */
  void clip_start_pos_setter (const dsp::Position * pos)
  {
    set_position (pos, ArrangerObject::PositionType::ClipStart, true);
  }

  /**
   * The setter is for use in e.g. the digital meters whereas the set_pos func
   * is used during arranger actions.
   */
  void loop_start_pos_setter (const dsp::Position * pos)
  {
    set_position (pos, ArrangerObject::PositionType::LoopStart, true);
  }

  /**
   * The setter is for use in e.g. the digital meters whereas the set_pos func
   * is used during arranger actions.
   */
  void loop_end_pos_setter (const dsp::Position * pos)
  {
    set_position (pos, ArrangerObject::PositionType::LoopEnd, true);
  }

  /**
   * Returns the length of the loop in ticks.
   */
  double get_loop_length_in_ticks () const
  {
    return loop_end_pos_.ticks_ - loop_start_pos_.ticks_;
  }

  /**
   * Returns the length of the loop in frames.
   */
  [[gnu::hot]] inline signed_frame_t get_loop_length_in_frames () const
  {
    return loop_end_pos_.frames_ - loop_start_pos_.frames_;
  }

  bool is_looped () const
  {
    return
    loop_start_pos_.ticks_ > 0
    || clip_start_pos_.ticks_ > 0
    ||
    (end_pos_->ticks_ - pos_->ticks_) >
       (loop_end_pos_.ticks_ +
         /* add some buffer because these are not accurate */
          0.1)
    ||
    (end_pos_->ticks_ - pos_->ticks_) <
       (loop_end_pos_.ticks_ -
         /* add some buffer because these are not accurate */
          0.1);
  }

protected:
  void
  copy_members_from (const LoopableObject &other, ObjectCloneType clone_type);

  void init_loaded_base ();

  bool are_members_valid (bool is_project) const;

  DECLARE_DEFINE_BASE_FIELDS_METHOD ();

public:
  /**
   * Start position of the clip loop, relative to the object's start.
   *
   * The first time the object plays it will start playing from the
   * this position and then loop to @ref loop_start_pos_.
   */
  dsp::Position clip_start_pos_;

  /** Loop start Position relative to the object's start. */
  dsp::Position loop_start_pos_;

  /**
   * End position of the clip loop, relative to the object's start.
   *
   * Once this is reached, the clip will go back to @ref loop_start_pos_.
   */
  dsp::Position loop_end_pos_;
};

inline bool
operator== (const LoopableObject &lhs, const LoopableObject &rhs)
{
  return lhs.clip_start_pos_ == rhs.clip_start_pos_
         && lhs.loop_start_pos_ == rhs.loop_start_pos_
         && lhs.loop_end_pos_ == rhs.loop_end_pos_;
}

#endif // __DSP_LOOPABLE_OBJECT_H__
