// SPDX-FileCopyrightText: © 2019-2022, 2024-2025 Alexandros Theodotou <alex@zrythm.org>
// SPDX-License-Identifier: LicenseRef-ZrythmLicense

#include "structure/arrangement/arranger_object_all.h"
#include "structure/arrangement/automation_region.h"

namespace zrythm::structure::arrangement
{
AutomationRegion::AutomationRegion (
  const dsp::TempoMap          &tempo_map,
  ArrangerObjectRegistry       &object_registry,
  dsp::FileAudioSourceRegistry &file_audio_source_registry,
  QObject *                     parent)
    : QObject (parent), ArrangerObject (Type::AutomationRegion, tempo_map, *this),
      ArrangerObjectOwner (object_registry, file_audio_source_registry, *this),
      region_mixin_ (utils::make_qobject_unique<RegionMixin> (*position ()))
{
}

double
AutomationRegion::get_normalized_value_in_curve (
  const AutomationPoint &ap,
  double                 x) const
{
  z_return_val_if_fail (x >= 0.0 && x <= 1.0, 0.0);

  AutomationPoint * next_ap = get_next_ap (ap, true);
  if (next_ap == nullptr)
    {
      return ap.value ();
    }

  double dy;

  bool start_higher = next_ap->value () < ap.value ();
  dy = ap.curveOpts ()->normalizedY (x, start_higher);
  return dy;
}

bool
AutomationRegion::curves_up (const AutomationPoint &ap) const
{
  AutomationPoint * next_ap = get_next_ap (ap, true);

  if (next_ap == nullptr)
    return false;

  // comment from previous implementation which split normalized value and real
  // value (currently all values are normalized):
  /* fvalue can be equal in non-float automation even though there is a curve.
   * use the normalized value instead */
  return next_ap->value () > ap.value ();
}

void
AutomationRegion::force_sort ()
{
  std::ranges::sort (
    get_children_vector (), [&] (const auto &a_id, const auto &b_id) {
      const auto a = std::get<AutomationPoint *> (a_id.get_object ());
      const auto b = std::get<AutomationPoint *> (b_id.get_object ());
      return a->position ()->ticks () < b->position ()->ticks ();
    });
}

AutomationPoint *
AutomationRegion::get_prev_ap (const AutomationPoint &ap) const
{
  auto it = std::ranges::find (
    get_children_vector (), ap.get_uuid (), &ArrangerObjectUuidReference::id);

  // if found and not the first element
  if (
    it != get_children_vector ().end () && it != get_children_vector ().begin ())
    {
      return std::get<AutomationPoint *> (
        (*std::ranges::prev (it)).get_object ());
    }

  return nullptr;
}

AutomationPoint *
AutomationRegion::get_next_ap (const AutomationPoint &ap, bool check_positions)
  const
{
  if (check_positions)
    {
      AutomationPoint * next_ap = nullptr;
      for (auto * cur_ap_outer : get_children_view ())
        {
          AutomationPoint * cur_ap = cur_ap_outer;

          if (cur_ap->get_uuid () == ap.get_uuid ())
            continue;

          if (
            cur_ap->position ()->ticks() >= ap.position ()->ticks()
            && ((next_ap == nullptr) || cur_ap->position ()->ticks() < next_ap->position ()->ticks()))
            {
              next_ap = cur_ap;
            }
        }
      return next_ap;
    }

  auto it = std::ranges::find (
    get_children_vector (), ap.get_uuid (), &ArrangerObjectUuidReference::id);

  // if found and not the last element
  if (
    it != get_children_vector ().end ()
    && std::ranges::next (it) != get_children_vector ().end ())
    {

      return std::get<AutomationPoint *> (
        (*std::ranges::next (it)).get_object ());
    }

  return nullptr;
}

void
init_from (
  AutomationRegion       &obj,
  const AutomationRegion &other,
  utils::ObjectCloneType  clone_type)
{
  init_from (
    static_cast<ArrangerObject &> (obj),
    static_cast<const ArrangerObject &> (other), clone_type);
  init_from (*obj.region_mixin_, *other.region_mixin_, clone_type);
  init_from (
    static_cast<ArrangerObjectOwner<AutomationPoint> &> (obj),
    static_cast<const ArrangerObjectOwner<AutomationPoint> &> (other),
    clone_type);
  obj.force_sort ();
}
}
