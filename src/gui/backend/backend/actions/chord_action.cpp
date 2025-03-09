// SPDX-FileCopyrightText: © 2022, 2024 Alexandros Theodotou <alex@zrythm.org>
// SPDX-License-Identifier: LicenseRef-ZrythmLicense

#include "gui/backend/backend/actions/chord_action.h"
#include "gui/backend/backend/clip_editor.h"
#include "gui/backend/backend/project.h"
#include "gui/backend/backend/zrythm.h"

using namespace zrythm::gui::actions;

ChordAction::ChordAction (QObject * parent)
    : QObject (parent), UndoableAction (UndoableAction::Type::Chord)
{
}

ChordAction::ChordAction (
  const std::vector<ChordDescriptor> &chords_before,
  const std::vector<ChordDescriptor> &chords_after,
  QObject *                           parent)
    : ChordAction (parent)
{
  type_ = Type::All;
  chords_before_ = chords_before;
  chords_after_ = chords_after;
}

ChordAction::ChordAction (
  const ChordDescriptor &chord,
  const int              chord_idx,
  QObject *              parent)
    : ChordAction (parent)
{
  type_ = Type::Single;
  chord_before_ = CHORD_EDITOR->chords_[chord_idx];

  chord_after_ = chord;
  chord_idx_ = chord_idx;
}

void
ChordAction::init_after_cloning (
  const ChordAction &other,
  ObjectCloneType    clone_type)
{
  UndoableAction::copy_members_from (other, clone_type);
  type_ = other.type_;
  chord_before_ = other.chord_before_;
  chord_after_ = other.chord_after_;
  chord_idx_ = other.chord_idx_;
}

void
ChordAction::do_or_undo (bool do_it)
{
  switch (type_)
    {
    case Type::All:
      CHORD_EDITOR->apply_chords (do_it ? chords_after_ : chords_before_, false);
      break;
    case Type::Single:
      CHORD_EDITOR->apply_single_chord (
        do_it ? chord_after_ : chord_before_, chord_idx_, false);
      break;
    }
}

void
ChordAction::perform_impl ()
{
  do_or_undo (true);
}

void
ChordAction::undo_impl ()
{
  do_or_undo (false);
}

QString
ChordAction::to_string () const
{
  return QObject::tr ("Change chords");
}
