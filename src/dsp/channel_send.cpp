// SPDX-FileCopyrightText: © 2020-2022 Alexandros Theodotou <alex@zrythm.org>
// SPDX-License-Identifier: LicenseRef-ZrythmLicense

#include "dsp/channel_send.h"
#include "dsp/control_port.h"
#include "dsp/engine.h"
#include "dsp/graph.h"
#include "dsp/midi_event.h"
#include "dsp/port.h"
#include "dsp/router.h"
#include "dsp/track.h"
#include "dsp/tracklist.h"
#include "gui/widgets/center_dock.h"
#include "gui/widgets/channel_send.h"
#include "gui/widgets/channel_sends_expander.h"
#include "gui/widgets/inspector_track.h"
#include "gui/widgets/left_dock_edge.h"
#include "gui/widgets/main_window.h"
#include "project.h"
#include "utils/dsp.h"
#include "utils/flags.h"
#include "utils/math.h"
#include "utils/objects.h"
#include "zrythm_app.h"

#include <glib/gi18n.h>

#include <fmt/format.h>

typedef enum
{
  Z_AUDIO_CHANNEL_SEND_ERROR_FAILED,
} ZAudioChannelSendError;

#define Z_AUDIO_CHANNEL_SEND_ERROR z_audio_channel_send_error_quark ()
GQuark
z_audio_channel_send_error_quark (void);
G_DEFINE_QUARK (
  z - audio - channel - send - error - quark,
  z_audio_channel_send_error)

static PortType
get_signal_type (const ChannelSend * self)
{
  Track * track = channel_send_get_track (self);
  g_return_val_if_fail (track, PortType::Audio);
  return track->out_signal_type;
}

/**
 * Returns a string describing @p self
 * (track/plugin name/etc.).
 */
char *
channel_send_target_describe (const ChannelSendTarget * self)
{
  switch (self->type)
    {
    case ChannelSendTargetType::CHANNEL_SEND_TARGET_TYPE_NONE:
      return g_strdup (_ ("None"));
    case ChannelSendTargetType::CHANNEL_SEND_TARGET_TYPE_TRACK:
      {
        Track * tr = TRACKLIST->tracks[self->track_pos];
        return g_strdup (tr->name);
      }
    case ChannelSendTargetType::CHANNEL_SEND_TARGET_TYPE_PLUGIN_SIDECHAIN:
      {
        Plugin * pl = plugin_find (&self->pl_id);
        char     designation[1200];
        plugin_get_full_port_group_designation (
          pl, self->port_group, designation);
        return g_strdup (designation);
      }
    default:
      break;
    }
  g_return_val_if_reached (_ ("Invalid"));
}

char *
channel_send_target_get_icon (const ChannelSendTarget * self)
{
  switch (self->type)
    {
    case ChannelSendTargetType::CHANNEL_SEND_TARGET_TYPE_NONE:
      return g_strdup ("edit-none");
    case ChannelSendTargetType::CHANNEL_SEND_TARGET_TYPE_TRACK:
      {
        Track * tr = TRACKLIST->tracks[self->track_pos];
        return g_strdup (tr->icon_name);
      }
    case ChannelSendTargetType::CHANNEL_SEND_TARGET_TYPE_PLUGIN_SIDECHAIN:
      {
        return g_strdup ("media-album-track");
      }
    default:
      break;
    }
  g_return_val_if_reached (g_strdup (_ ("Invalid")));
}

void
channel_send_target_free (ChannelSendTarget * self)
{
  g_free_and_null (self->port_group);
  object_zero_and_free (self);
}

void
channel_send_init_loaded (ChannelSend * self, Track * track)
{
  self->track = track;

#define INIT_LOADED_PORT(x) self->x->init_loaded (self)

  INIT_LOADED_PORT (enabled);
  INIT_LOADED_PORT (amount);
  INIT_LOADED_PORT (midi_in);
  INIT_LOADED_PORT (midi_out);
  self->stereo_in->init_loaded (self);
  self->stereo_out->init_loaded (self);

#undef INIT_LOADED_PORT
}

/**
 * Creates a channel send instance.
 */
ChannelSend *
channel_send_new (unsigned int track_name_hash, int slot, Track * track)
{
  ChannelSend * self = object_new (ChannelSend);
  self->schema_version = CHANNEL_SEND_SCHEMA_VERSION;
  self->track = track;
  self->track_name_hash = track_name_hash;
  self->slot = slot;

#define SET_PORT_OWNER(x) \
  self->x->set_owner (PortIdentifier::OwnerType::CHANNEL_SEND, self)

  self->enabled = new Port (
    PortType::Control, PortFlow::Input,
    fmt::format (_ ("Channel Send {} enabled"), slot + 1));
  self->enabled->id_.sym_ = fmt::format ("channel_send_{}_enabled", slot + 1);
  self->enabled->id_.flags_ |= PortIdentifier::Flags::TOGGLE;
  self->enabled->id_.flags2_ |= PortIdentifier::Flags2::CHANNEL_SEND_ENABLED;
  SET_PORT_OWNER (enabled);
  self->enabled->set_control_value (0.f, F_NOT_NORMALIZED, F_NO_PUBLISH_EVENTS);

  self->amount = new Port (
    PortType::Control, PortFlow::Input,
    fmt::format (_ ("Channel Send {} amount"), slot + 1));
  self->amount->id_.sym_ = fmt::format ("channel_send_{}_amount", slot + 1);
  self->amount->id_.flags_ |= PortIdentifier::Flags::AMPLITUDE;
  self->amount->id_.flags_ |= PortIdentifier::Flags::AUTOMATABLE;
  self->amount->id_.flags2_ |= PortIdentifier::Flags2::CHANNEL_SEND_AMOUNT;
  SET_PORT_OWNER (amount);
  self->amount->set_control_value (1.f, F_NOT_NORMALIZED, F_NO_PUBLISH_EVENTS);

  self->stereo_in = new StereoPorts (
    F_INPUT, fmt::format (_ ("Channel Send {} audio in"), slot + 1),
    fmt::format ("channel_send_{}_audio_in", slot + 1),
    PortIdentifier::OwnerType::CHANNEL_SEND, self);
  self->stereo_in->set_owner (PortIdentifier::OwnerType::CHANNEL_SEND, self);

  self->midi_in = new Port (
    PortType::Event, PortFlow::Input,
    fmt::format (_ ("Channel Send {} MIDI in"), slot + 1));
  self->midi_in->id_.sym_ = fmt::format ("channel_send_{}_midi_in", slot + 1);
  SET_PORT_OWNER (midi_in);

  self->stereo_out = new StereoPorts (
    F_NOT_INPUT, fmt::format (_ ("Channel Send {} audio out"), slot + 1),
    fmt::format ("channel_send_{}_audio_out", slot + 1),
    PortIdentifier::OwnerType::CHANNEL_SEND, self);
  self->stereo_out->set_owner (PortIdentifier::OwnerType::CHANNEL_SEND, self);

  self->midi_out = new Port (
    PortType::Event, PortFlow::Output,
    fmt::format (_ ("Channel Send {} MIDI out"), slot + 1));
  self->midi_out->id_.sym_ = fmt::format ("channel_send_{}_midi_out", slot + 1);
  SET_PORT_OWNER (midi_out);

#undef SET_PORT_OWNER

  return self;
}

Track *
channel_send_get_track (const ChannelSend * self)
{
  Track * track = self->track;
  g_return_val_if_fail (track, NULL);
  return track;
}

/**
 * Returns whether the channel send target is a
 * sidechain port (rather than a target track).
 */
bool
channel_send_is_target_sidechain (ChannelSend * self)
{
  return channel_send_is_enabled (self) && self->is_sidechain;
}

void
channel_send_prepare_process (ChannelSend * self)
{
  AudioEngine &engine = *AUDIO_ENGINE;
  if (self->midi_in)
    {
      self->midi_in->clear_buffer (engine);
      self->midi_out->clear_buffer (engine);
    }
  if (self->stereo_in)
    {
      self->stereo_in->clear_buffer (engine);
      self->stereo_out->clear_buffer (engine);
    }
}

void
channel_send_process (
  ChannelSend *   self,
  const nframes_t local_offset,
  const nframes_t nframes)
{
  if (channel_send_is_empty (self))
    return;

  Track * track = channel_send_get_track (self);
  g_return_if_fail (track);
  if (track->out_signal_type == PortType::Audio)
    {
      if (math_floats_equal_epsilon (self->amount->control_, 1.f, 0.00001f))
        {
          dsp_copy (
            &self->stereo_out->get_l ().buf_[local_offset],
            &self->stereo_in->get_l ().buf_[local_offset], nframes);
          dsp_copy (
            &self->stereo_out->get_r ().buf_[local_offset],
            &self->stereo_in->get_r ().buf_[local_offset], nframes);
        }
      else
        {
          dsp_mix2 (
            &self->stereo_out->get_l ().buf_[local_offset],
            &self->stereo_in->get_l ().buf_[local_offset], 1.f,
            self->amount->control_, nframes);
          dsp_mix2 (
            &self->stereo_out->get_r ().buf_[local_offset],
            &self->stereo_in->get_r ().buf_[local_offset], 1.f,
            self->amount->control_, nframes);
        }
    }
  else if (track->out_signal_type == PortType::Event)
    {
      midi_events_append (
        self->midi_out->midi_events_, self->midi_in->midi_events_, local_offset,
        nframes, F_NOT_QUEUED);
    }
}

void
channel_send_copy_values (ChannelSend * dest, const ChannelSend * src)
{
  dest->slot = src->slot;
  dest->enabled->set_control_value (
    src->enabled->control_, F_NOT_NORMALIZED, F_NO_PUBLISH_EVENTS);
  dest->amount->set_control_value (
    src->amount->control_, F_NOT_NORMALIZED, F_NO_PUBLISH_EVENTS);
  dest->is_sidechain = src->is_sidechain;
}

/**
 * Gets the target track.
 *
 * @param owner The owner track of the send
 *   (optional).
 */
Track *
channel_send_get_target_track (ChannelSend * self, Track * owner)
{
  if (channel_send_is_empty (self))
    return NULL;

  PortType         signal_type = get_signal_type (self);
  PortConnection * conn = NULL;
  switch (signal_type)
    {
    case PortType::Audio:
      conn = port_connections_manager_get_source_or_dest (
        PORT_CONNECTIONS_MGR, &self->stereo_out->get_l ().id_, false);
      break;
    case PortType::Event:
      conn = port_connections_manager_get_source_or_dest (
        PORT_CONNECTIONS_MGR, &self->midi_out->id_, false);
      break;
    default:
      g_return_val_if_reached (NULL);
      break;
    }

  g_return_val_if_fail (conn, NULL);
  Port * port = Port::find_from_identifier (conn->dest_id);
  g_return_val_if_fail (IS_PORT_AND_NONNULL (port), NULL);

  return port->get_track (true);
}

/**
 * Gets the target sidechain port.
 *
 * Returned StereoPorts instance must be free'd.
 */
StereoPorts *
channel_send_get_target_sidechain (ChannelSend * self)
{
  g_return_val_if_fail (
    !channel_send_is_empty (self) && self->is_sidechain, NULL);

  PortType signal_type = get_signal_type (self);
  g_return_val_if_fail (signal_type == PortType::Audio, NULL);

  PortConnection * conn = port_connections_manager_get_source_or_dest (
    PORT_CONNECTIONS_MGR, &self->stereo_out->get_l ().id_, false);
  g_return_val_if_fail (conn, NULL);
  Port * l = Port::find_from_identifier (conn->dest_id);
  g_return_val_if_fail (l, NULL);

  conn = port_connections_manager_get_source_or_dest (
    PORT_CONNECTIONS_MGR, &self->stereo_out->get_r ().id_, false);
  g_return_val_if_fail (conn, NULL);
  Port * r = Port::find_from_identifier (conn->dest_id);
  g_return_val_if_fail (r, NULL);

  StereoPorts * sp = new StereoPorts (l->clone (), r->clone ());

  return sp;
}

/**
 * Connects the ports to the owner track if not
 * connected.
 *
 * Only to be called on project sends.
 */
void
channel_send_connect_to_owner (ChannelSend * self)
{
  PortType signal_type = get_signal_type (self);
  switch (signal_type)
    {
    case PortType::Audio:
      for (int i = 0; i < 2; i++)
        {
          Port &self_port =
            i == 0 ? self->stereo_in->get_l () : self->stereo_in->get_r ();
          Port * src_port = NULL;
          if (channel_send_is_prefader (self))
            {
              src_port =
                i == 0
                  ? &self->track->channel->prefader->stereo_out->get_l ()
                  : &self->track->channel->prefader->stereo_out->get_r ();
            }
          else
            {
              src_port =
                i == 0
                  ? &self->track->channel->fader->stereo_out->get_l ()
                  : &self->track->channel->fader->stereo_out->get_r ();
            }

          /* make the connection if not exists */
          port_connections_manager_ensure_connect (
            PORT_CONNECTIONS_MGR, &src_port->id_, &self_port.id_, 1.f, F_LOCKED,
            F_ENABLE);
        }
      break;
    case PortType::Event:
      {
        Port * self_port = self->midi_in;
        Port * src_port = NULL;
        if (channel_send_is_prefader (self))
          {
            src_port = self->track->channel->prefader->midi_out;
          }
        else
          {
            src_port = self->track->channel->fader->midi_out;
          }

        /* make the connection if not exists */
        port_connections_manager_ensure_connect (
          PORT_CONNECTIONS_MGR, &src_port->id_, &self_port->id_, 1.f, F_LOCKED,
          F_ENABLE);
      }
      break;
    default:
      break;
    }
}

/**
 * Gets the amount to be used in widgets (0.0-1.0).
 */
float
channel_send_get_amount_for_widgets (ChannelSend * self)
{
  g_return_val_if_fail (channel_send_is_enabled (self), 0.f);

  return math_get_fader_val_from_amp (self->amount->control_);
}

/**
 * Sets the amount from a widget amount (0.0-1.0).
 */
void
channel_send_set_amount_from_widget (ChannelSend * self, float val)
{
  g_return_if_fail (channel_send_is_enabled (self));

  channel_send_set_amount (self, math_get_amp_val_from_fader (val));
}

/**
 * Connects a send to stereo ports.
 *
 * This function takes either \ref stereo or both
 * \ref l and \ref r.
 */
bool
channel_send_connect_stereo (
  ChannelSend * self,
  StereoPorts * stereo,
  Port *        l,
  Port *        r,
  bool          sidechain,
  bool          recalc_graph,
  bool          validate,
  GError **     error)
{
  /* verify can be connected */
  if (validate && l->is_in_active_project ())
    {
      Port * src = Port::find_from_identifier (&self->stereo_out->get_l ().id_);
      if (!src->can_be_connected_to (l))
        {
          g_set_error_literal (
            error, Z_AUDIO_CHANNEL_SEND_ERROR,
            Z_AUDIO_CHANNEL_SEND_ERROR_FAILED, _ ("Ports cannot be connected"));
          return false;
        }
    }

  channel_send_disconnect (self, false);

  /* connect */
  if (stereo)
    {
      l = &stereo->get_l ();
      r = &stereo->get_r ();
    }

  port_connections_manager_ensure_connect (
    PORT_CONNECTIONS_MGR, &self->stereo_out->get_l ().id_, &l->id_, 1.f,
    F_LOCKED, F_ENABLE);
  port_connections_manager_ensure_connect (
    PORT_CONNECTIONS_MGR, &self->stereo_out->get_r ().id_, &r->id_, 1.f,
    F_LOCKED, F_ENABLE);

  self->enabled->set_control_value (1.f, F_NOT_NORMALIZED, F_PUBLISH_EVENTS);
  self->is_sidechain = sidechain;

#if 0
  /* set multipliers */
  channel_send_update_connections (self);
#endif

  if (recalc_graph)
    router_recalc_graph (ROUTER, F_NOT_SOFT);

  return true;
}

/**
 * Connects a send to a midi port.
 */
bool
channel_send_connect_midi (
  ChannelSend * self,
  Port *        port,
  bool          recalc_graph,
  bool          validate,
  GError **     error)
{
  /* verify can be connected */
  if (validate && port->is_in_active_project ())
    {
      Port * src = Port::find_from_identifier (&self->midi_out->id_);
      if (!src->can_be_connected_to (port))
        {
          g_set_error_literal (
            error, Z_AUDIO_CHANNEL_SEND_ERROR,
            Z_AUDIO_CHANNEL_SEND_ERROR_FAILED, _ ("Ports cannot be connected"));
          return false;
        }
    }

  channel_send_disconnect (self, false);

  port_connections_manager_ensure_connect (
    PORT_CONNECTIONS_MGR, &self->midi_out->id_, &port->id_, 1.f, F_LOCKED,
    F_ENABLE);

  self->enabled->set_control_value (1.f, F_NOT_NORMALIZED, F_PUBLISH_EVENTS);

  if (recalc_graph)
    router_recalc_graph (ROUTER, F_NOT_SOFT);

  return true;
}

static void
disconnect_midi (ChannelSend * self)
{
  PortConnection * conn = port_connections_manager_get_source_or_dest (
    PORT_CONNECTIONS_MGR, &self->midi_out->id_, false);
  if (!conn)
    return;

  Port * dest_port = Port::find_from_identifier (conn->dest_id);
  g_return_if_fail (dest_port);

  port_connections_manager_ensure_disconnect (
    PORT_CONNECTIONS_MGR, &self->midi_out->id_, &dest_port->id_);
}

static void
disconnect_audio (ChannelSend * self)
{
  for (int i = 0; i < 2; i++)
    {
      Port * src_port =
        i == 0 ? &self->stereo_out->get_l () : &self->stereo_out->get_r ();
      PortConnection * conn = port_connections_manager_get_source_or_dest (
        PORT_CONNECTIONS_MGR, &src_port->id_, false);
      if (!conn)
        continue;

      Port * dest_port = Port::find_from_identifier (conn->dest_id);
      g_return_if_fail (dest_port);
      port_connections_manager_ensure_disconnect (
        PORT_CONNECTIONS_MGR, &src_port->id_, &dest_port->id_);
    }
}

/**
 * Removes the connection at the given send.
 */
void
channel_send_disconnect (ChannelSend * self, bool recalc_graph)
{
  if (channel_send_is_empty (self))
    return;

  PortType signal_type = get_signal_type (self);

  switch (signal_type)
    {
    case PortType::Audio:
      disconnect_audio (self);
      break;
    case PortType::Event:
      disconnect_midi (self);
      break;
    default:
      break;
    }

  self->enabled->set_control_value (0.f, F_NOT_NORMALIZED, F_PUBLISH_EVENTS);
  self->is_sidechain = false;

  if (recalc_graph)
    router_recalc_graph (ROUTER, F_NOT_SOFT);
}

void
channel_send_set_amount (ChannelSend * self, float amount)
{
  self->amount->set_control_value (amount, F_NOT_NORMALIZED, F_PUBLISH_EVENTS);
}

/**
 * Get the name of the destination, or a placeholder
 * text if empty.
 */
void
channel_send_get_dest_name (ChannelSend * self, char * buf)
{
  if (channel_send_is_empty (self))
    {
      if (channel_send_is_prefader (self))
        strcpy (buf, _ ("Pre-fader send"));
      else
        strcpy (buf, _ ("Post-fader send"));
    }
  else
    {
      PortType type = get_signal_type (self);
      Port    &search_port =
        (type == PortType::Audio) ? self->stereo_out->get_l () : *self->midi_out;
      PortConnection * conn = port_connections_manager_get_source_or_dest (
        PORT_CONNECTIONS_MGR, &search_port.id_, false);
      g_return_if_fail (conn);
      Port * dest = Port::find_from_identifier (conn->dest_id);
      g_return_if_fail (IS_PORT_AND_NONNULL (dest));
      if (self->is_sidechain)
        {
          Plugin * pl = dest->get_plugin (true);
          g_return_if_fail (IS_PLUGIN (pl));
          plugin_get_full_port_group_designation (
            pl, dest->id_.port_group_.c_str (), buf);
        }
      else /* else if not sidechain */
        {
          switch (dest->id_.owner_type_)
            {
            case PortIdentifier::OwnerType::TRACK_PROCESSOR:
              {
                Track * track = dest->get_track (true);
                g_return_if_fail (IS_TRACK_AND_NONNULL (track));
                sprintf (buf, _ ("%s input"), track->name);
              }
              break;
            default:
              break;
            }
        }
    }
}

ChannelSend *
channel_send_clone (const ChannelSend * src)
{
  ChannelSend * self = channel_send_new (src->track_name_hash, src->slot, NULL);

  self->amount->control_ = src->amount->control_;
  self->enabled->control_ = src->enabled->control_;
  self->is_sidechain = src->is_sidechain;
  self->track_name_hash = src->track_name_hash;

  g_return_val_if_fail (
    self->track_name_hash != 0 && self->track_name_hash == src->track_name_hash,
    NULL);

  return self;
}

bool
channel_send_is_enabled (const ChannelSend * self)
{
  if (ZRYTHM_TESTING)
    {
      g_return_val_if_fail (IS_PORT_AND_NONNULL (self->enabled), false);
    }

  bool enabled = control_port_is_toggled (self->enabled);

  if (!enabled)
    return false;

  PortType signal_type = get_signal_type (self);
  Port    &search_port =
    (signal_type == PortType::Audio) ? self->stereo_out->get_l () : *self->midi_out;

  if (G_LIKELY (router_is_processing_thread (ROUTER)))
    {
      if (search_port.dests_.size () == 1)
        {
          Port * dest = search_port.dests_[0];
          g_return_val_if_fail (IS_PORT_AND_NONNULL (dest), false);

          if (dest->id_.owner_type_ == PortIdentifier::OwnerType::PLUGIN)
            {
              Plugin * pl = plugin_find (&dest->id_.plugin_id_);
              g_return_val_if_fail (IS_PLUGIN_AND_NONNULL (pl), false);
              if (pl->instantiation_failed)
                return false;
            }

          return true;
        }
      else
        return false;
    }

  /* get dest port */
  PortConnection * conn = port_connections_manager_get_source_or_dest (
    PORT_CONNECTIONS_MGR, &search_port.id_, false);
  g_return_val_if_fail (conn, false);
  Port * dest = Port::find_from_identifier (conn->dest_id);
  g_return_val_if_fail (IS_PORT_AND_NONNULL (dest), false);

  /* if dest port is a plugin port and plugin instantiation failed, assume that
   * the send is disabled */
  if (dest->id_.owner_type_ == PortIdentifier::OwnerType::PLUGIN)
    {
      Plugin * pl = plugin_find (&dest->id_.plugin_id_);
      if (pl->instantiation_failed)
        enabled = false;
    }

  return enabled;
}

ChannelSendWidget *
channel_send_find_widget (ChannelSend * self)
{
  if (ZRYTHM_HAVE_UI && MAIN_WINDOW)
    {
      return MW_TRACK_INSPECTOR->sends->slots[self->slot];
    }
  return NULL;
}

/**
 * Finds the project send from a given send
 * instance.
 */
ChannelSend *
channel_send_find (ChannelSend * self)
{
  Track * track =
    tracklist_find_track_by_name_hash (TRACKLIST, self->track_name_hash);
  g_return_val_if_fail (IS_TRACK_AND_NONNULL (track), NULL);

  return track->channel->sends[self->slot];
}

bool
channel_send_validate (ChannelSend * self)
{
  if (channel_send_is_enabled (self))
    {
      PortType signal_type = get_signal_type (self);
      if (signal_type == PortType::Audio)
        {
          int num_dests = port_connections_manager_get_sources_or_dests (
            PORT_CONNECTIONS_MGR, NULL, &self->stereo_out->get_l ().id_, false);
          g_return_val_if_fail (num_dests == 1, false);
          num_dests = port_connections_manager_get_sources_or_dests (
            PORT_CONNECTIONS_MGR, NULL, &self->stereo_out->get_r ().id_, false);
          g_return_val_if_fail (num_dests == 1, false);
        }
      else if (signal_type == PortType::Event)
        {
          int num_dests = port_connections_manager_get_sources_or_dests (
            PORT_CONNECTIONS_MGR, NULL, &self->midi_out->id_, false);
          g_return_val_if_fail (num_dests == 1, false);
        }
    } /* endif channel send is enabled */

  return true;
}

void
channel_send_append_ports (ChannelSend * self, GPtrArray * ports)
{
  auto _add = [ports] (Port &port) { g_ptr_array_add (ports, &port); };

  _add (*self->enabled);
  _add (*self->amount);
  if (self->midi_in)
    {
      _add (*self->midi_in);
    }
  if (self->midi_out)
    {
      _add (*self->midi_out);
    }
  if (self->stereo_in)
    {
      _add (self->stereo_in->get_l ());
      _add (self->stereo_in->get_r ());
    }
  if (self->stereo_out)
    {
      _add (self->stereo_out->get_l ());
      _add (self->stereo_out->get_r ());
    }
}

/**
 * Appends the connection(s), if non-empty, to the
 * given array (if not NULL) and returns the number
 * of connections added.
 */
int
channel_send_append_connection (
  const ChannelSend *            self,
  const PortConnectionsManager * mgr,
  GPtrArray *                    arr)
{
  if (channel_send_is_empty (self))
    return 0;

  PortType signal_type = get_signal_type (self);
  if (signal_type == PortType::Audio)
    {
      int num_dests = port_connections_manager_get_sources_or_dests (
        mgr, arr, &self->stereo_out->get_l ().id_, false);
      g_return_val_if_fail (num_dests == 1, false);
      num_dests = port_connections_manager_get_sources_or_dests (
        mgr, arr, &self->stereo_out->get_r ().id_, false);
      g_return_val_if_fail (num_dests == 1, false);
      return 2;
    }
  else if (signal_type == PortType::Event)
    {
      int num_dests = port_connections_manager_get_sources_or_dests (
        mgr, arr, &self->midi_out->id_, false);
      g_return_val_if_fail (num_dests == 1, false);
      return 1;
    }

  g_return_val_if_reached (0);
}

/**
 * Returns whether the send is connected to the
 * given ports.
 */
bool
channel_send_is_connected_to (
  const ChannelSend * self,
  const StereoPorts * stereo,
  const Port *        midi)
{
  GPtrArray * conns = g_ptr_array_new ();
  int         num_conns =
    channel_send_append_connection (self, PORT_CONNECTIONS_MGR, conns);
  bool ret = false;
  for (int i = 0; i < num_conns; i++)
    {
      PortConnection * conn =
        (PortConnection *) g_ptr_array_index (conns, (size_t) i);
      if ((stereo && (conn->dest_id->is_equal(stereo->get_l ().id_) || conn->dest_id->is_equal(stereo->get_r ().id_))) || (midi && conn->dest_id->is_equal(midi->id_)))
        {
          ret = true;
          break;
        }
    }
  g_ptr_array_unref (conns);

  return ret;
}

void
channel_send_free (ChannelSend * self)
{
  object_delete_and_null (self->amount);
  object_delete_and_null (self->enabled);
  object_delete_and_null (self->stereo_in);
  object_delete_and_null (self->midi_in);
  object_delete_and_null (self->stereo_out);
  object_delete_and_null (self->midi_out);

  object_zero_and_free (self);
}
