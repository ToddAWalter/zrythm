// SPDX-FileCopyrightText: © 2020-2021, 2024 Alexandros Theodotou <alex@zrythm.org>
// SPDX-License-Identifier: LicenseRef-ZrythmLicense

#ifndef __ACTIONS_TRANSPORT_ACTION_H__
#define __ACTIONS_TRANSPORT_ACTION_H__

#include "actions/undoable_action.h"
#include "utils/icloneable.h"
#include "utils/types.h"

/**
 * @addtogroup actions
 *
 * @{
 */

/**
 * Transport action.
 */
class TransportAction final
    : public UndoableAction,
      public ICloneable<TransportAction>,
      public ISerializable<TransportAction>
{
public:
  enum class Type
  {
    TempoChange,
    BeatsPerBarChange,
    BeatUnitChange,
  };

  TransportAction () : UndoableAction (UndoableAction::Type::Transport) { }

  /**
   * @brief Construct a new Transport Action object for a BPM change.
   *
   * @param bpm_before
   * @param bpm_after
   * @param already_done
   */
  TransportAction (bpm_t bpm_before, bpm_t bpm_after, bool already_done);

  /**
   * @brief Construct a new Transport object for a beat unit/beats per bar
   * change.
   *
   * @param type
   * @param before
   * @param after
   * @param already_done
   */
  TransportAction (Type type, int before, int after, bool already_done);

  void init_after_cloning (const TransportAction &other) override
  {
    UndoableAction::copy_members_from (other);
    type_ = other.type_;
    bpm_before_ = other.bpm_before_;
    bpm_after_ = other.bpm_after_;
    int_before_ = other.int_before_;
    int_after_ = other.int_after_;
    already_done_ = other.already_done_;
    musical_mode_ = other.musical_mode_;
  }

  std::string to_string () const override;

  DECLARE_DEFINE_FIELDS_METHOD ();

private:
  void init_loaded_impl () override { }
  void undo_impl () override;
  void perform_impl () override;

  bool need_update_positions_from_ticks ()
  {
    return type_ == Type::TempoChange || type_ == Type::BeatsPerBarChange;
  }

   void do_or_undo (bool do_it);

 public:
   Type type_ = Type::TempoChange;

   bpm_t bpm_before_ = 0.0;
   bpm_t bpm_after_ = 0.0;

   int int_before_ = 0;
   int int_after_ = 0;

   /** Flag whether the action was already performed the first time. */
   bool already_done_ = false;

   /** Whether musical mode was enabled when this action was made. */
   bool musical_mode_ = false;
};

/**
 * @}
 */

#endif
