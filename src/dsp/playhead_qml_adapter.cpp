// SPDX-FileCopyrightText: © 2025 Alexandros Theodotou <alex@zrythm.org>
// SPDX-License-Identifier: LicenseRef-ZrythmLicense

#include "dsp/playhead_qml_adapter.h"

namespace zrythm::dsp
{

PlayheadQmlWrapper::PlayheadQmlWrapper (Playhead &playhead, QObject * parent)
    : QObject (parent), playhead_ (playhead),
      last_ticks_ (playhead_.position_ticks ())
{
  // Set up timer to update at ~30Hz (33ms)
  timer_.setInterval (33);
  connect (&timer_, &QTimer::timeout, this, &PlayheadQmlWrapper::updateTicks);
  timer_.start ();
}

double
PlayheadQmlWrapper::ticks () const
{
  return last_ticks_;
}

void
PlayheadQmlWrapper::setTicks (double ticks)
{
  playhead_.set_position_ticks (ticks);
  updateTicks ();
}

void
PlayheadQmlWrapper::updateTicks ()
{
  playhead_.update_ticks_from_samples ();
  const double current_ticks = playhead_.position_ticks ();

  // Only update if changed significantly
  if (std::abs (current_ticks - last_ticks_) > 1e-6)
    {
      last_ticks_ = current_ticks;
      Q_EMIT ticksChanged ();
    }
}
} // namespace zrythm::dsp
