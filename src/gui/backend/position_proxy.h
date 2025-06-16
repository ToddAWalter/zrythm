// SPDX-FileCopyrightText: © 2024-2025 Alexandros Theodotou <alex@zrythm.org>
// SPDX-License-Identifier: LicenseRef-ZrythmLicense

#pragma once

#include "dsp/position.h"
#include "dsp/tempo_map_qml_adapter.h"
#include "utils/icloneable.h"
#include "utils/math.h"

#include <QObject>
#include <QtQmlIntegration>

#include "realtime_property.h"

namespace zrythm::engine::session
{
class Transport;
}

using namespace zrythm;

Q_DECLARE_OPAQUE_POINTER (zrythm::engine::session::Transport *)
Q_DECLARE_OPAQUE_POINTER (const zrythm::engine::session::Transport *)

/**
 * @brief QML-friendly position representation with real-time safety.
 *
 * Exposes Position functionality as Q_PROPERTYs with change notifications.
 * Supports atomic updates from real-time threads when realtime_updateable=true.
 */
class PositionProxy
    : public QObject,
      public IRealtimeProperty,
      public zrythm::dsp::Position
{
  Q_OBJECT
  QML_ELEMENT

  Q_PROPERTY (double ticks READ getTicks WRITE setTicks NOTIFY ticksChanged)
  Q_PROPERTY (
    signed_frame_t frames READ getFrames WRITE setFrames NOTIFY framesChanged)
  Q_PROPERTY (
    double ticksPerSixteenthNote READ getTicksPerSixteenthNote CONSTANT)

public:
  PositionProxy (
    QObject *        parent = nullptr,
    const Position * pos = nullptr,
    bool             realtime_updateable = false);
  ~PositionProxy () override;
  Q_DISABLE_COPY_MOVE (PositionProxy)

  signed_frame_t getFrames () const { return frames_; }
  void           setFrames (signed_frame_t frames);
  Q_SIGNAL void  framesChanged ();

  double        getTicks () const { return ticks_; }
  void          setTicks (double ticks);
  Q_SIGNAL void ticksChanged ();

  static double getTicksPerSixteenthNote ()
  {
    return TICKS_PER_SIXTEENTH_NOTE_DBL;
  }
  Position get_position () const
  {
    return *dynamic_cast<const Position *> (this);
  }

  Q_INVOKABLE void addTicks (double ticks) { setTicks (getTicks () + ticks); }

  Q_INVOKABLE QString getStringDisplay (
    const zrythm::engine::session::Transport * transport,
    const zrythm::dsp::TempoMapWrapper *       tempo_map) const;

public:
  // RT-safe wrappers

  void
  set_frames_rtsafe (signed_frame_t frames, dsp::TicksPerFrame ticks_per_frame)
  {
    from_frames (frames, ticks_per_frame);
    has_update_.store (true, std::memory_order_release);
  }
  void set_ticks_rtsafe (double ticks, dsp::FramesPerTick frames_per_tick)
  {
    from_ticks (ticks, frames_per_tick);
    has_update_.store (true, std::memory_order_release);
  }

  void update_from_ticks_rtsafe (dsp::FramesPerTick frames_per_tick)
  {
    update_frames_from_ticks (frames_per_tick);
    has_update_.store (true, std::memory_order_release);
  }

  void update_from_frames_rtsafe (dsp::TicksPerFrame ticks_per_frame)
  {
    update_ticks_from_frames (ticks_per_frame);
    has_update_.store (true, std::memory_order_release);
  }

  void set_position_rtsafe (const Position &pos)
  {
    if (pos.frames_ == frames_ && utils::math::floats_equal (pos.ticks_, ticks_))
      return;

    frames_ = pos.frames_;
    ticks_ = pos.ticks_;
    has_update_.store (true, std::memory_order_release);
  }

  void
  add_frames_rtsafe (signed_frame_t frames, dsp::TicksPerFrame ticks_per_frame)
  {
    if (frames == 0)
      return;

    add_frames (frames, ticks_per_frame);
    has_update_.store (true, std::memory_order_release);
  }

  bool processUpdates () override;

  friend void init_from (
    PositionProxy         &obj,
    const PositionProxy   &other,
    utils::ObjectCloneType clone_type);

  friend auto operator<=> (const PositionProxy &lhs, const PositionProxy &rhs)
  {
    return static_cast<const zrythm::dsp::Position &> (lhs)
           <=> static_cast<const zrythm::dsp::Position &> (rhs);
  }

  friend bool operator== (const PositionProxy &lhs, const PositionProxy &rhs)
  {
    return (lhs <=> rhs) == 0;
  }

private:
  std::atomic<bool> has_update_{ false };
  bool              realtime_updateable_;
};

DEFINE_OBJECT_FORMATTER (PositionProxy, PositionProxy, [] (const auto &obj) {
  return Position_to_string (obj);
});
