// SPDX-FileCopyrightText: © 2025 Alexandros Theodotou <alex@zrythm.org>
// SPDX-License-Identifier: LicenseRef-ZrythmLicense

#pragma once

#include <stdexcept>

#include "structure/tracks/track_all.h"
#include "undo/undo_stack.h"

#include <QtQmlIntegration>

namespace zrythm::actions
{
class TrackOperator : public QObject
{
  Q_OBJECT
  Q_PROPERTY (
    zrythm::structure::tracks::Track * track READ track WRITE setTrack NOTIFY
      trackChanged)
  Q_PROPERTY (
    zrythm::undo::UndoStack * undoStack READ undoStack WRITE setUndoStack NOTIFY
      undoStackChanged)
  QML_ELEMENT

public:
  explicit TrackOperator (QObject * parent = nullptr) : QObject (parent) { }

  Q_SIGNAL void trackChanged ();
  Q_SIGNAL void undoStackChanged ();

  structure::tracks::Track * track () const { return track_; }
  void                       setTrack (structure::tracks::Track * track)
  {
    if (track == nullptr)
      {
        throw std::invalid_argument ("Track cannot be null");
      }
    if (track_ != track)
      {
        track_ = track;
        Q_EMIT trackChanged ();
      }
  }

  undo::UndoStack * undoStack () const { return undo_stack_; }
  void              setUndoStack (undo::UndoStack * undoStack)
  {
    if (undoStack == nullptr)
      {
        throw std::invalid_argument ("UndoStack cannot be null");
      }
    if (undo_stack_ != undoStack)
      {
        undo_stack_ = undoStack;
        Q_EMIT undoStackChanged ();
      }
  }

  Q_INVOKABLE void rename (const QString &newName);
  Q_INVOKABLE void setColor (const QColor &color);

private:
  structure::tracks::Track * track_{};
  undo::UndoStack *          undo_stack_{};
};
}
