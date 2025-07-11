// SPDX-FileCopyrightText: © 2018-2024 Alexandros Theodotou <alex@zrythm.org>
// SPDX-License-Identifier: LicenseRef-ZrythmLicense

#include <algorithm>
#include <cstdlib>

#include "gui/backend/backend/project.h"
#include "gui/backend/backend/zrythm.h"
#include "gui/dsp/plugin.h"
#include "structure/arrangement/automation_region.h"
#include "structure/tracks/automatable_track.h"
#include "structure/tracks/automation_track.h"
#include "structure/tracks/automation_tracklist.h"
#include "structure/tracks/channel.h"
#include "structure/tracks/track.h"
#include "structure/tracks/tracklist.h"
#include "utils/rt_thread_id.h"
#include "utils/views.h"

namespace zrythm::structure::tracks
{
AutomationTracklist::AutomationTracklist (
  dsp::FileAudioSourceRegistry    &file_audio_source_registry,
  dsp::PortRegistry               &port_registry,
  dsp::ProcessorParameterRegistry &param_registry,
  ArrangerObjectRegistry          &obj_registry,
  AutomatableTrack                &track,
  QObject *                        parent)
    : QAbstractListModel (parent),
      file_audio_source_registry_ (file_audio_source_registry),
      object_registry_ (obj_registry), port_registry_ (port_registry),
      param_registry_ (param_registry), track_ (track)
{
}

void
AutomationTracklist::init_loaded ()
{
  for (auto &at : ats_)
    {
      at->init_loaded ();
      if (at->visible_)
        {
          visible_ats_.emplace_back (at.get ());
        }
    }
}

void
init_from (
  AutomationTracklist       &obj,
  const AutomationTracklist &other,
  utils::ObjectCloneType     clone_type)
{
  obj.ats_.clear ();
  obj.ats_.reserve (other.ats_.size ());
// TODO
#if 0
  for (const auto &at : other.ats_)
    {
      auto * cloned_at = at->clone_raw_ptr ();
      cloned_at->setParent (this);
      ats_.push_back (cloned_at);
    }
#endif
}

// ========================================================================
// QML Interface
// ========================================================================
QHash<int, QByteArray>
AutomationTracklist::roleNames () const
{
  static QHash<int, QByteArray> roles = {
    { AutomationTrackPtrRole, "automationTrack" },
  };
  return roles;
}

int
AutomationTracklist::rowCount (const QModelIndex &parent) const
{
  if (parent.isValid ())
    return 0;
  return static_cast<int> (ats_.size ());
}

QVariant
AutomationTracklist::data (const QModelIndex &index, int role) const
{
  if (!index.isValid () || index.row () >= static_cast<int> (ats_.size ()))
    return {};

  switch (role)
    {
    case AutomationTrackPtrRole:
      return QVariant::fromValue (ats_.at (index.row ()).get ());
    default:
      return {};
    }
}

void
AutomationTracklist::showNextAvailableAutomationTrack (
  AutomationTrack * current_automation_track)
{
  auto * new_at = get_first_invisible_at ();

  /* if any invisible at exists, show it */
  if (new_at != nullptr)
    {
      if (!new_at->created_)
        new_at->created_ = true;
      set_at_visible (*new_at, true);

      /* move it after the clicked automation track */
      set_automation_track_index (
        *new_at, current_automation_track->index_ + 1, true);
      Q_EMIT dataChanged (
        index (new_at->index_), index ((int) ats_.size () - 1));
    }
}

void
AutomationTracklist::hideAutomationTrack (
  AutomationTrack * current_automation_track)
{
  /* don't allow deleting if no other visible automation tracks */
  if (visible_ats_.size () > 1)
    {
      set_at_visible (*current_automation_track, false);
      Q_EMIT dataChanged (
        index (current_automation_track->index_),
        index ((int) ats_.size () - 1));
    }
}

// ========================================================================

TrackPtrVariant
AutomationTracklist::get_track () const
{
  return convert_to_variant<TrackPtrVariant> (&track_);
}

dsp::ProcessorParameter &
AutomationTracklist::get_port (dsp::ProcessorParameter::Uuid id) const
{
  return *std::get<dsp::ProcessorParameter *> (
    param_registry_.find_by_id_or_throw (id));
}

AutomationTrack *
AutomationTracklist::add_automation_track (AutomationTrack &at)
{
  ats_.push_back (&at);
  auto &at_ref = ats_.back ();
  at_ref->setParent (this);

  at_ref->index_ = static_cast<int> (ats_.size ()) - 1;

  /* move automation track regions */
#if 0
  for (auto * region : at_ref->get_children_view ())
    {
      region->set_automation_track (*at_ref);
    }
#endif

  return at_ref.get ();
}

void
AutomationTracklist::
  set_automation_track_index (AutomationTrack &at, int index, bool push_down)
{
  /* special case */
  if (index == static_cast<int> (ats_.size ()) && push_down)
    {
      /* move AT to before last */
      set_automation_track_index (at, index - 1, push_down);
      /* move last AT to before last */
      set_automation_track_index (*ats_.back (), index - 1, push_down);
      return;
    }

  z_return_if_fail (index < static_cast<int> (ats_.size ()) && ats_[index]);

  auto it = std::ranges::find_if (ats_, [&] (const auto &at_ref) {
    return at_ref.get () == &at;
  });
  z_return_if_fail (
    it != ats_.end () && std::distance (ats_.begin (), it) == at.index_);

  if (at.index_ == index)
    return;

  auto clip_editor_region_opt = CLIP_EDITOR->get_region_and_track ();
  // int  clip_editor_region_idx = -2;
  if (clip_editor_region_opt)
    {
#if 0
      std::visit (
        [&] (auto &&clip_editor_region) {
          if (clip_editor_region->get_track_id () == track_.get_uuid ())
            {
              clip_editor_region_idx = clip_editor_region_id.at_idx_;
            }
        },
        clip_editor_region_opt.value ());
#endif
    }

  if (push_down)
    {
      auto from = at.index_;
      auto to = index;

      auto from_it = ats_.begin () + from;
      auto to_it = ats_.begin () + to;
      if (from < to)
        {
          std::rotate (from_it, from_it + 1, to_it + 1);
        }
      else
        {
          std::rotate (to_it, from_it, from_it + 1);
        }

      for (auto i = std::min (from, to); i <= std::max (from, to); ++i)
        {
          ats_[i]->set_index (i);
        }
    }
  else
    {
      int prev_index = at.index_;
      std::swap (ats_[index], ats_[prev_index]);
      ats_[index]->set_index (index);
      ats_[prev_index]->set_index (prev_index);

      z_trace (
        "new pos {} ({})", *ats_[prev_index]->parameter (),
        ats_[prev_index]->index_);
    }
}

void
AutomationTracklist::unselect_all ()
{
  for (auto &at : ats_)
    {
      for (auto * child : at->get_children_view ())
        {
          auto sel_mgr =
            arrangement::ArrangerObjectFactory::get_instance ()
              ->get_selection_manager_for_object (*child);
          sel_mgr.remove_from_selection (child->get_uuid ());
        }
    }
}

void
AutomationTracklist::clear_objects ()
{
  for (auto &at : ats_)
    {
      at->clear_objects ();
    }
}

AutomationTrack *
AutomationTracklist::get_prev_visible_at (const AutomationTrack &at) const
{
  auto it = std::find_if (
    ats_.rbegin () + (ats_.size () - at.index_ + 1), ats_.rend (),
    [] (const auto &at_ref) { return at_ref->created_ && at_ref->visible_; });
  return it == ats_.rend () ? nullptr : (*std::prev (it.base ())).get ();
}

AutomationTrack *
AutomationTracklist::get_next_visible_at (const AutomationTrack &at) const
{
  auto it = std::find_if (
    ats_.begin () + at.index_ + 1, ats_.end (),
    [] (const auto &at_ref) { return at_ref->created_ && at_ref->visible_; });
  return it == ats_.end () ? nullptr : (*it).get ();
}

AutomationTrack *
AutomationTracklist::get_visible_at_after_delta (
  const AutomationTrack &at,
  int                    delta) const
{
  if (delta > 0)
    {
      auto vis_at = &at;
      while (delta > 0)
        {
          vis_at = get_next_visible_at (*vis_at);

          if (vis_at == nullptr)
            return nullptr;

          delta--;
        }
      return const_cast<AutomationTrack *> (vis_at);
    }
  else if (delta < 0)
    {
      auto vis_at = &at;
      while (delta < 0)
        {
          vis_at = get_prev_visible_at (*vis_at);

          if (vis_at == nullptr)
            return nullptr;

          delta++;
        }
      return const_cast<AutomationTrack *> (vis_at);
    }
  else
    return const_cast<AutomationTrack *> (&at);
}

int
AutomationTracklist::get_visible_at_diff (
  const AutomationTrack &src,
  const AutomationTrack &dest) const
{
  int count = 0;
  if (src.index_ < dest.index_)
    {
      for (int i = src.index_; i < dest.index_; i++)
        {
          const auto &at = ats_[i];
          if (at->created_ && at->visible_)
            {
              count++;
            }
        }
    }
  else if (src.index_ > dest.index_)
    {
      for (int i = dest.index_; i < src.index_; i++)
        {
          const auto &at = ats_[i];
          if (at->created_ && at->visible_)
            {
              count--;
            }
        }
    }

  return count;
}

AutomationTrack *
AutomationTracklist::get_first_invisible_at () const
{
  /* prioritize automation tracks with existing lanes */
  auto it = std::ranges::find_if (ats_, [] (const auto &at) {
    return at->created_ && !at->visible_;
  });

  if (it != ats_.end ())
    return (*it).get ();

  it = std::ranges::find_if (ats_, [] (const auto &at) {
    return !at->created_;
  });

  return it != ats_.end () ? (*it).get () : nullptr;
}

void
AutomationTracklist::set_at_visible (AutomationTrack &at, bool visible)
{
  z_return_if_fail (at.created_);
  at.visible_ = visible;
  auto it =
    std::ranges::find (visible_ats_, &at, &QPointer<AutomationTrack>::get);
  if (visible)
    {
      if (it == visible_ats_.end ())
        {
          visible_ats_.emplace_back (&at);
        }
    }
  else if (it != visible_ats_.end ())
    {
      visible_ats_.erase (it);
    }
}

utils::QObjectUniquePtr<AutomationTrack>
AutomationTracklist::
  remove_at (AutomationTrack &at, bool free_at, bool fire_events)
{
  beginRemoveRows ({}, at.index_, at.index_);

  int deleted_idx = 0;

  {
    auto it = std::find (visible_ats_.begin (), visible_ats_.end (), &at);
    if (it != visible_ats_.end ())
      {
        visible_ats_.erase (it);
      }
  }

  z_trace (
    "[track {} atl] removing automation track at: {} '{}'", track_.get_index (),
    deleted_idx, *at.parameter ());

  if (free_at)
    {
      /* this needs to be called before removing the automation track in case
       * the region is referenced elsewhere (e.g., clip editor) */
      at.clear_objects ();
    }

  auto it = std::ranges::find (
    ats_, &at, &utils::QObjectUniquePtr<AutomationTrack>::get);
  if (it == ats_.end ())
    {
      z_warning (
        "[track {} atl] automation track not found", track_.get_index ());
      endRemoveRows ();
      return nullptr;
    }
  auto &deleted_at = *it;
  it = ats_.erase (it);

  /* move automation track regions for automation tracks after the deleted one*/
  for (auto cur_it = it; cur_it != ats_.end (); ++cur_it)
    {
      auto &cur_at = *cur_it;
      cur_at->index_ = std::distance (ats_.begin (), cur_it);
#if 0
      for (auto * region : cur_at->get_children_view ())
        {
          region->set_automation_track (*cur_at);
        }
#endif
    }

  /* if the deleted at was the last visible/created at, make the next one
   * visible */
  if (visible_ats_.empty ())
    {
      auto first_invisible_at = get_first_invisible_at ();
      if (first_invisible_at)
        {
          if (!first_invisible_at->created_)
            first_invisible_at->created_ = true;

          set_at_visible (*first_invisible_at, true);

          if (fire_events)
            {
              /* EVENTS_PUSH (
                EventType::ET_AUTOMATION_TRACK_ADDED, first_invisible_at); */
            }
        }
    }

  endRemoveRows ();

  if (fire_events)
    {
      // EVENTS_PUSH (EventType::ET_AUTOMATION_TRACKLIST_AT_REMOVED, this);
    }

  if (free_at)
    {
      return nullptr;
    }

  return std::move (deleted_at);
}

void
AutomationTracklist::append_objects (
  std::vector<arrangement::ArrangerObject *> objects) const
{
  for (auto &at : ats_)
    {
      std::ranges::copy (at->get_children_view (), std::back_inserter (objects));
    }
}

void
AutomationTracklist::print_ats () const
{
  utils::Utf8String str =
    utils::Utf8String (u8"Automation tracklist (track '") + track_.get_name ()
    + u8"')\n";

  for (size_t i = 0; i < ats_.size (); i++)
    {
      const auto &at = ats_[i];
      const auto &port = at->parameter ();
      str += utils::Utf8String::from_utf8_encoded_string (
        fmt::format (
          "[{}] '{}' (uniqueId '{}')\n", i, at->parameter ()->label (),
          port->get_unique_id ()));
    }

  z_info (str);
}

int
AutomationTracklist::get_num_visible () const
{
  return std::ranges::count_if (ats_, [] (const auto &at) {
    return at->created_ && at->visible_;
  });
}

int
AutomationTracklist::get_num_regions () const
{
  return std::ranges::fold_left (
    ats_ | std::views::transform ([] (const auto &at) {
      return at->get_children_vector ().size ();
    }),
    0, std::plus{});
}

void
AutomationTracklist::set_caches (CacheType types)
{
  auto * track = &track_;
  z_return_if_fail (track);

  if (track->is_auditioner ())
    return;

  if (ENUM_BITSET_TEST (types, CacheType::AutomationLaneRecordModes))
    {
      ats_in_record_mode_.clear ();
    }

  for (auto &at : ats_)
    {
      at->set_caches (types);

      if (
        ENUM_BITSET_TEST (types, CacheType::AutomationLaneRecordModes)
        && at->automation_mode_ == AutomationTrack::AutomationMode::Record)
        {
          ats_in_record_mode_.emplace_back (at.get ());
        }
    }
}

void
AutomationTracklist::print_regions () const
{
  auto * track = &track_;
  z_return_if_fail (track);

  std::string str = fmt::format (
    "Automation regions for track {} (total automation tracks {}):",
    track->get_name (), ats_.size ());

  for (const auto &[index, at] : utils::views::enumerate (ats_))
    {
      if (at->get_children_vector ().empty ())
        continue;

      str += fmt::format (
        "\n  [{}] port '{}': {} regions", index, at->parameter ()->label (),
        at->get_children_vector ().size ());
    }

  z_info (str);
}

void
from_json (const nlohmann::json &j, AutomationTracklist &ats)
{
  for (const auto &at_json : j.at (AutomationTracklist::kAutomationTracksKey))
    {
      auto port_id = at_json.at ("portId").get<dsp::ProcessorParameter::Uuid> ();
      ats.ats_.emplace_back (
        utils::make_qobject_unique<AutomationTrack> (
          ats.file_audio_source_registry_, ats.object_registry_,
          [&ats] () { return convert_to_variant<TrackPtrVariant> (&ats.track_); },
          dsp::ProcessorParameterUuidReference{ port_id, ats.param_registry_ }));
    }
}
}
