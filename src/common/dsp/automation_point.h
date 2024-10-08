// SPDX-FileCopyrightText: © 2018-2022 Alexandros Theodotou <alex@zrythm.org>
// SPDX-License-Identifier: LicenseRef-ZrythmLicense

/**
 * @file
 *
 * Automation point API.
 */

#ifndef __AUDIO_AUTOMATION_POINT_H__
#define __AUDIO_AUTOMATION_POINT_H__

#include "common/dsp/arranger_object.h"
#include "common/dsp/control_port.h"
#include "common/dsp/curve.h"
#include "common/dsp/position.h"
#include "common/dsp/region_owned_object.h"
#include "common/utils/icloneable.h"
#include "common/utils/math.h"
#include "common/utils/types.h"

class Port;
class AutomationRegion;
class AutomationTrack;

/**
 * @addtogroup dsp
 *
 * @{
 */

/**
 * An automation point inside an AutomationTrack.
 */
class AutomationPoint final
    : public RegionOwnedObjectImpl<AutomationRegion>,
      public ICloneable<AutomationPoint>,
      public ISerializable<AutomationPoint>
{
public:
  // Rule of 0
  AutomationPoint () = default;

  AutomationPoint (const Position &pos);

  ~AutomationPoint ();

  /**
   * Creates an AutomationPoint at the given Position.
   */
  AutomationPoint (
    const float     value,
    const float     normalized_val,
    const Position &pos);

  /**
   * Sets the value from given real or normalized
   * value and notifies interested parties.
   *
   * @param is_normalized Whether the given value is
   *   normalized.
   */
  void set_fvalue (float real_val, bool is_normalized, bool pub_events);

  /** String getter for the value. */
  std::string get_fvalue_as_string () const;

  /** String setter. */
  void set_fvalue_with_action (const std::string &fval_str);

  /**
   * The function to return a point on the curve.
   *
   * See
   * https://stackoverflow.com/questions/17623152/how-map-tween-a-number-based-on-a-dynamic-curve
   *
   * @param region region The automation region (if known), otherwise the
   * non-cached region will be used.
   * @param x Normalized x.
   */
  ATTR_HOT double
  get_normalized_value_in_curve (AutomationRegion * region, double x) const;

  std::string print_to_str () const override;

  /**
   * Sets the curviness of the AutomationPoint.
   */
  void set_curviness (const curviness_t curviness);

  /**
   * Convenience function to return the control port that this AutomationPoint
   * is for.
   */
  ControlPort * get_port () const;

  /**
   * Convenience function to return the AutomationTrack that this
   * AutomationPoint is in.
   */
  AutomationTrack * get_automation_track () const;

  /**
   * Returns if the curve of the AutomationPoint curves upwards as you move
   * right on the x axis.
   */
  bool curves_up () const;

  ArrangerObjectPtr find_in_project () const override;

  void init_after_cloning (const AutomationPoint &other) override;

  void init_loaded () override;

  ArrangerObjectPtr add_clone_to_project (bool fire_events) const override;

  ArrangerObjectPtr insert_clone_to_project () const override;

  bool validate (bool is_project, double frames_per_tick) const override;

  DECLARE_DEFINE_FIELDS_METHOD ();

public:
  /** Float value (real). */
  float fvalue_ = 0.f;

  /** Normalized value (0 to 1) used as a cache. */
  float normalized_val_ = 0.f;

  CurveOptions curve_opts_ = {};
};

inline bool
operator< (const AutomationPoint &a, const AutomationPoint &b)
{
  if (a.pos_ == b.pos_) [[unlikely]]
    {
      return a.index_ < b.index_;
    }

  return a.pos_ < b.pos_;
}

inline bool
operator== (const AutomationPoint &a, const AutomationPoint &b)
{
  /* note: we don't care about the index, only the position and the value */
  /* note2: previously, this code was comparing position ticks, now it only
   * compares frames. TODO: if no problems are caused delete this note */
  return a.pos_ == b.pos_
         && math_floats_equal_epsilon (a.fvalue_, b.fvalue_, 0.001f);
}

/**
 * @brief FIXME: move to a more appropriate place.
 *
 */
enum class AutomationMode
{
  Read,
  Record,
  Off,
  NUM_AUTOMATION_MODES,
};

constexpr size_t NUM_AUTOMATION_MODES =
  static_cast<size_t> (AutomationMode::NUM_AUTOMATION_MODES);

DEFINE_ENUM_FORMATTER (
  AutomationMode,
  AutomationMode,
  N_ ("On"),
  N_ ("Rec"),
  N_ ("Off"));

DEFINE_OBJECT_FORMATTER (AutomationPoint, [] (const AutomationPoint &ap) {
  return fmt::format (
    "AutomationPoint [{}]: val {}, normalized val {}", ap.pos_, ap.fvalue_,
    ap.normalized_val_);
});

/**
 * @}
 */

#endif // __AUDIO_AUTOMATION_POINT_H__
