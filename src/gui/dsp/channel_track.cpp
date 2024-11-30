// SPDX-License-Identifier: LicenseRef-ZrythmLicense
// SPDX-FileCopyrightText: © 2018-2019, 2024 Alexandros Theodotou <alex@zrythm.org>

#include "gui/backend/backend/actions/tracklist_selections_action.h"
#include "gui/backend/backend/project.h"
#include "gui/backend/backend/tracklist_selections.h"
#include "gui/backend/backend/zrythm.h"
#include "gui/dsp/channel_track.h"
#include "gui/dsp/tracklist.h"

using namespace zrythm;

ChannelTrack::ChannelTrack () : channel_ (new Channel (*this)) { }

ChannelTrack::~ChannelTrack ()
{
  // remove_ats_from_automation_tracklist (false);
}

void
ChannelTrack::init_loaded ()
{
  channel_->init_loaded (*this);
}

void
ChannelTrack::copy_members_from (const ChannelTrack &other)
{
  channel_ = other.channel_->clone_raw_ptr ();
  if (auto * qobject = dynamic_cast<QObject *> (this))
    channel_->setParent (qobject);
  channel_->set_track_ptr (*this);
}

void
ChannelTrack::init_channel ()
{
  channel_->init ();
  if (auto * qobject = dynamic_cast<QObject *> (this))
    channel_->setParent (qobject);
}

void
ChannelTrack::
  set_muted (bool mute, bool trigger_undo, bool auto_select, bool fire_events)
{
  if (auto_select)
    {
      select (true, true, fire_events);
    }

  if (trigger_undo)
    {
      /* this is only supported if only selected track */
      z_return_if_fail (
        TRACKLIST_SELECTIONS->get_num_tracks () == 1 && is_selected ());

      UNDO_MANAGER->perform (new gui::actions::MuteTrackAction (*this, mute));
    }
  else
    {
      channel_->fader_->set_muted (mute, fire_events);
    }
}

void
ChannelTrack::
  set_soloed (bool solo, bool trigger_undo, bool auto_select, bool fire_events)
{
  if (auto_select)
    {
      select (true, true, fire_events);
    }

  if (trigger_undo)
    {
      /* this is only supported if only selected track */
      z_return_if_fail (
        TRACKLIST_SELECTIONS->get_num_tracks () == 1 && is_selected ());

      UNDO_MANAGER->perform (new gui::actions::SoloTrackAction (*this, solo));
    }
  else
    {
      channel_->fader_->set_soloed (solo, fire_events);
    }
}

void
ChannelTrack::set_listened (
  bool listen,
  bool trigger_undo,
  bool auto_select,
  bool fire_events)
{
  if (auto_select)
    {
      select (true, true, fire_events);
    }

  if (trigger_undo)
    {
      /* this is only supported if only selected track */
      z_return_if_fail (
        TRACKLIST_SELECTIONS->get_num_tracks () == 1 && is_selected ());

      UNDO_MANAGER->perform (
        new gui::actions::ListenTrackAction (*this, listen));
    }
  else
    {
      channel_->fader_->set_listened (listen, fire_events);
    }
}

void
ChannelTrack::remove_ats_from_automation_tracklist (bool fire_events)
{
  auto &atl = get_automation_tracklist ();

  for (auto &at : atl.ats_ | std::views::reverse)
    {
      if (
        ENUM_BITSET_TEST (
          dsp::PortIdentifier::Flags, at->port_id_->flags_,
          dsp::PortIdentifier::Flags::ChannelFader)
        || ENUM_BITSET_TEST (
          dsp::PortIdentifier::Flags, at->port_id_->flags_,
          dsp::PortIdentifier::Flags::FaderMute)
        || ENUM_BITSET_TEST (
          dsp::PortIdentifier::Flags, at->port_id_->flags_,
          dsp::PortIdentifier::Flags::StereoBalance))
        {
          atl.remove_at (*at, false, fire_events);
        }
    }
}

bool
ChannelTrack::validate_base () const
{
  z_return_val_if_fail (channel_ != nullptr, false);
  for (auto &send : channel_->sends_)
    {
      z_return_val_if_fail (
        send->track_name_hash_ == send->amount_->id_->track_name_hash_, false);
    }

  /* verify output and sends */
  auto out_track = channel_->get_output_track ();
  z_return_val_if_fail (
    static_cast<Track *> (out_track) != static_cast<const Track *> (this),
    false);

  /* verify plugins */
  std::vector<zrythm::gui::old_dsp::plugins::Plugin *> plugins;
  channel_->get_plugins (plugins);
  for (auto pl : plugins)
    {
      z_return_val_if_fail (pl->validate (), false);
    }

  /* verify sends */
  for (auto &send : channel_->sends_)
    {
      z_return_val_if_fail (send->validate (), false);
    }

  return true;
}

Fader *
ChannelTrack::get_fader (bool post_fader)
{
  auto ch = get_channel ();
  if (ch)
    {
      if (post_fader)
        {
          return ch->fader_;
        }
      else
        {
          return ch->prefader_;
        }
    }

  return nullptr;
}

#if 0
GMenu *
ChannelTrack::generate_channel_context_menu ()
{
  GMenu *     channel_submenu = g_menu_new ();
  GMenuItem * menuitem = g_menu_item_new (nullptr, nullptr);
  g_menu_item_set_attribute (menuitem, "custom", "s", "fader-buttons");
  g_menu_append_item (channel_submenu, menuitem);

  GMenu * direct_out_submenu = g_menu_new ();

  if (type_can_have_direct_out (type_))
    {
      /* add "route to new group" */
      menuitem = z_gtk_create_menu_item (
        QObject::tr ("New direct out"), nullptr, "app.selected-tracks-direct-out-new");
      g_menu_append_item (direct_out_submenu, menuitem);
    }

  /* direct out targets */
  GMenu * target_submenu = g_menu_new ();
  bool    have_groups = false;
  for (auto &cur_tr : TRACKLIST->tracks_)
    {
      if (
        !cur_tr->can_be_group_target ()
        || out_signal_type_ != cur_tr->in_signal_type_ || this == cur_tr.get ())
        continue;

      char tmp[600];
      sprintf (tmp, "app.selected-tracks-direct-out-to");
      menuitem = z_gtk_create_menu_item (cur_tr->name_.c_str (), nullptr, tmp);
      g_menu_item_set_action_and_target_value (
        menuitem, tmp, g_variant_new_int32 (cur_tr->pos_));
      g_menu_append_item (target_submenu, menuitem);
      have_groups = true;
    }
  if (have_groups)
    {
      g_menu_append_section (
        direct_out_submenu, QObject::tr ("Route Target"), G_MENU_MODEL (target_submenu));
    }

  if (type_can_have_direct_out (type_))
    {
      g_menu_append_submenu (
        channel_submenu, QObject::tr ("Direct Output"), G_MENU_MODEL (direct_out_submenu));
    }

  return channel_submenu;
}
#endif

void
ChannelTrack::append_member_ports (
  std::vector<Port *> &ports,
  bool                 include_plugins) const
{
  get_channel ()->append_ports (ports, include_plugins);
}
