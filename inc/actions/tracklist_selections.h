// SPDX-FileCopyrightText: © 2019-2022 Alexandros Theodotou <alex@zrythm.org>
// SPDX-License-Identifier: LicenseRef-ZrythmLicense

#ifndef __ACTIONS_TRACKLIST_SELECTIONS_ACTION_H__
#define __ACTIONS_TRACKLIST_SELECTIONS_ACTION_H__

#include "actions/undoable_action.h"
#include "dsp/port_connections_manager.h"
#include "dsp/supported_file.h"
#include "dsp/track.h"
#include "gui/backend/tracklist_selections.h"
#include "settings/plugin_settings.h"

/**
 * @addtogroup actions
 *
 * @{
 */

enum class TracklistSelectionsActionType
{
  TRACKLIST_SELECTIONS_ACTION_COPY,
  TRACKLIST_SELECTIONS_ACTION_COPY_INSIDE,
  TRACKLIST_SELECTIONS_ACTION_CREATE,
  TRACKLIST_SELECTIONS_ACTION_DELETE,
  TRACKLIST_SELECTIONS_ACTION_EDIT,
  TRACKLIST_SELECTIONS_ACTION_MOVE,
  TRACKLIST_SELECTIONS_ACTION_MOVE_INSIDE,
  TRACKLIST_SELECTIONS_ACTION_PIN,
  TRACKLIST_SELECTIONS_ACTION_UNPIN,
};

/**
 * Action type.
 */
enum class EditTrackActionType
{
  EDIT_TRACK_ACTION_TYPE_SOLO,
  EDIT_TRACK_ACTION_TYPE_SOLO_LANE,
  EDIT_TRACK_ACTION_TYPE_MUTE,
  EDIT_TRACK_ACTION_TYPE_MUTE_LANE,
  EDIT_TRACK_ACTION_TYPE_LISTEN,
  EDIT_TRACK_ACTION_TYPE_ENABLE,
  EDIT_TRACK_ACTION_TYPE_FOLD,
  EDIT_TRACK_ACTION_TYPE_VOLUME,
  EDIT_TRACK_ACTION_TYPE_PAN,

  /** Direct out change. */
  EDIT_TRACK_ACTION_TYPE_DIRECT_OUT,

  /** Rename track. */
  EDIT_TRACK_ACTION_TYPE_RENAME,

  /** Rename lane. */
  EDIT_TRACK_ACTION_TYPE_RENAME_LANE,

  EDIT_TRACK_ACTION_TYPE_COLOR,
  EDIT_TRACK_ACTION_TYPE_COMMENT,
  EDIT_TRACK_ACTION_TYPE_ICON,

  EDIT_TRACK_ACTION_TYPE_MIDI_FADER_MODE,
};

/**
 * Tracklist selections (tracks) action.
 */
typedef struct TracklistSelectionsAction
{
  UndoableAction parent_instance;

  /** Type of action. */
  TracklistSelectionsActionType type;

  /** Position to make the tracks at.
   *
   * Used when undoing too. */
  int track_pos;

  /** Lane position, if editing lane. */
  int lane_pos;

  /** Position to add the audio region to, if
   * applicable. */
  Position pos;

  bool have_pos;

  /** Track type. */
  TrackType track_type;

  /** Flag to know if we are making an empty
   * track. */
  int is_empty;

  /** PluginSetting, if making an instrument or
   * bus track from a plugin.
   *
   * If this is empty and the track type is
   * instrument, it is assumed that it's an empty
   * track. */
  PluginSetting * pl_setting;

  /**
   * The basename of the file, if any.
   *
   * This will be used as the track name.
   */
  char * file_basename;

  /**
   * If this is an action to create a MIDI track
   * from a MIDI file, this is the base64
   * representation so that the file does not need
   * to be stored in the project.
   *
   * @note For audio files,
   *   TracklistSelectionsAction.pool_id is used.
   */
  char * base64_midi;

  /**
   * If this is an action to create an Audio track
   * from an audio file, this is the pool ID of the
   * audio file.
   *
   * If this is not -1, this means that an audio
   * file exists in the pool.
   */
  int pool_id;

  /** Source sends that need to be deleted/
   * recreated on do/undo. */
  ChannelSend ** src_sends;
  int            num_src_sends;
  size_t         src_sends_size;

  /**
   * Direct out tracks of the original tracks.
   *
   * These are track name hashes.
   */
  unsigned int * out_tracks;
  int            num_out_tracks;

  /**
   * Number of tracks under folder affected.
   *
   * Counter to be filled while doing to be used
   * when undoing.
   */
  int num_fold_change_tracks;

  EditTrackActionType edit_type;

  /**
   * Track positions.
   *
   * Used for actions where full selection clones
   * are not needed.
   */
  int tracks_before[600];
  int tracks_after[600];
  int num_tracks;

  /** Clone of the TracklistSelections, if
   * applicable. */
  TracklistSelections * tls_before;

  /** Clone of the TracklistSelections, if
   * applicable. */
  TracklistSelections * tls_after;

  /**
   * Foldable tracks before the change, used when
   * undoing to set the correct sizes.
   */
  TracklistSelections * foldable_tls_before;

  /** A clone of the port connections at the
   * start of the action. */
  PortConnectionsManager * connections_mgr_before;

  /** A clone of the port connections after
   * applying the action. */
  PortConnectionsManager * connections_mgr_after;

  /* --------------- DELTAS ---------------- */

  /**
   * Int value.
   *
   * Also used for bool.
   */
  int * ival_before;
  int   ival_after;

  /* -------------- end DELTAS ------------- */

  GdkRGBA * colors_before;
  GdkRGBA   new_color;

  char * new_txt;

  /** Skip do if true. */
  bool already_edited;

  /** Float values. */
  float val_before;
  float val_after;

} TracklistSelectionsAction;

void
tracklist_selections_action_init_loaded (TracklistSelectionsAction * self);

/**
 * Creates a new TracklistSelectionsAction.
 *
 * @param tls_before Tracklist selections to act
 *   upon.
 * @param port_connections_mgr Port connections
 *   manager at the start of the action.
 * @param pos Position to make the tracks at.
 * @param pl_setting Plugin setting, if any.
 * @param track Track, if single-track action. Used
 *   if @ref tls_before and @ref tls_after are NULL.
 * @param error To be filled in if an error occurred.
 */
WARN_UNUSED_RESULT UndoableAction *
tracklist_selections_action_new (
  TracklistSelectionsActionType  type,
  TracklistSelections *          tls_before,
  TracklistSelections *          tls_after,
  const PortConnectionsManager * port_connections_mgr,
  Track *                        track,
  TrackType                      track_type,
  const PluginSetting *          pl_setting,
  const SupportedFile *          file_descr,
  int                            track_pos,
  int                            lane_pos,
  const Position *               pos,
  int                            num_tracks,
  EditTrackActionType            edit_type,
  int                            ival_after,
  const GdkRGBA *                color_new,
  float                          val_before,
  float                          val_after,
  const char *                   new_txt,
  bool                           already_edited,
  GError **                      error);

/**
 * @param disable_track_pos Track position to
 *   disable, or -1 to not disable any track.
 */
#define tracklist_selections_action_new_create( \
  track_type, pl_setting, file_descr, track_pos, pos, num_tracks, \
  disable_track_pos, err) \
  tracklist_selections_action_new ( \
    TracklistSelectionsActionType::TRACKLIST_SELECTIONS_ACTION_CREATE, NULL, \
    NULL, NULL, NULL, track_type, pl_setting, file_descr, track_pos, -1, pos, \
    num_tracks, ENUM_INT_TO_VALUE (EditTrackActionType, 0), disable_track_pos, \
    NULL, 0.f, 0.f, NULL, false, err)

/**
 * Creates a new TracklistSelectionsAction for an
 * audio FX track.
 */
#define tracklist_selections_action_new_create_audio_fx( \
  pl_setting, track_pos, num_tracks, err) \
  tracklist_selections_action_new_create ( \
    TRACK_TYPE_AUDIO_BUS, pl_setting, NULL, track_pos, NULL, num_tracks, -1, \
    err)

/**
 * Creates a new TracklistSelectionsAction for a
 * MIDI FX track.
 */
#define tracklist_selections_action_new_create_midi_fx( \
  pl_setting, track_pos, num_tracks, err) \
  tracklist_selections_action_new_create ( \
    TRACK_TYPE_MIDI_BUS, pl_setting, NULL, track_pos, NULL, num_tracks, -1, \
    err)

/**
 * Creates a new TracklistSelectionsAction for an
 * instrument track.
 */
#define tracklist_selections_action_new_create_instrument( \
  pl_setting, track_pos, num_tracks, err) \
  tracklist_selections_action_new_create ( \
    TRACK_TYPE_INSTRUMENT, pl_setting, NULL, track_pos, NULL, num_tracks, -1, \
    err)

/**
 * Creates a new TracklistSelectionsAction for an
 * audio group track.
 */
#define tracklist_selections_action_new_create_audio_group( \
  track_pos, num_tracks, err) \
  tracklist_selections_action_new_create ( \
    TRACK_TYPE_AUDIO_GROUP, NULL, NULL, track_pos, NULL, num_tracks, -1, err)

/**
 * Creates a new TracklistSelectionsAction for a
 * MIDI group track.
 */
#define tracklist_selections_action_new_create_midi_group( \
  track_pos, num_tracks, err) \
  tracklist_selections_action_new_create ( \
    TRACK_TYPE_MIDI_GROUP, NULL, NULL, track_pos, NULL, num_tracks, -1, err)

/**
 * Creates a new TracklistSelectionsAction for a MIDI
 * track.
 */
#define tracklist_selections_action_new_create_midi(track_pos, num_tracks, err) \
  tracklist_selections_action_new_create ( \
    TRACK_TYPE_MIDI, NULL, NULL, track_pos, NULL, num_tracks, -1, err)

/**
 * Creates a new TracklistSelectionsAction for a
 * folder track.
 */
#define tracklist_selections_action_new_create_folder( \
  track_pos, num_tracks, err) \
  tracklist_selections_action_new_create ( \
    TRACK_TYPE_FOLDER, NULL, NULL, track_pos, NULL, num_tracks, -1, err)

/**
 * Generic edit action.
 */
#define tracklist_selections_action_new_edit_generic( \
  type, tls_before, tls_after, already_edited, err) \
  tracklist_selections_action_new ( \
    TracklistSelectionsActionType::TRACKLIST_SELECTIONS_ACTION_EDIT, \
    tls_before, tls_after, NULL, NULL, 0, NULL, NULL, -1, -1, NULL, -1, type, \
    false, NULL, 0.f, 0.f, NULL, already_edited, err)

/**
 * Convenience wrapper over
 * tracklist_selections_action_new() for single-track
 * float edit changes.
 */
#define tracklist_selections_action_new_edit_single_float( \
  type, track, val_before, val_after, already_edited, err) \
  tracklist_selections_action_new ( \
    TracklistSelectionsActionType::TRACKLIST_SELECTIONS_ACTION_EDIT, NULL, \
    NULL, NULL, track, (TrackType) 0, NULL, NULL, -1, -1, NULL, -1, type, \
    false, NULL, val_before, val_after, NULL, already_edited, err)

/**
 * Convenience wrapper over
 * tracklist_selections_action_new() for single-track
 * int edit changes.
 */
#define tracklist_selections_action_new_edit_single_int( \
  type, track, val_after, already_edited, err) \
  tracklist_selections_action_new ( \
    TracklistSelectionsActionType::TRACKLIST_SELECTIONS_ACTION_EDIT, NULL, \
    NULL, NULL, track, 0, NULL, NULL, -1, -1, NULL, -1, type, val_after, NULL, \
    0.f, 0.f, NULL, already_edited, err)

#define tracklist_selections_action_new_edit_mute(tls_before, mute_new, err) \
  tracklist_selections_action_new ( \
    TracklistSelectionsActionType::TRACKLIST_SELECTIONS_ACTION_EDIT, \
    tls_before, NULL, NULL, NULL, 0, NULL, NULL, -1, -1, NULL, -1, \
    EditTrackActionType::EDIT_TRACK_ACTION_TYPE_MUTE, mute_new, NULL, 0.f, \
    0.f, NULL, false, err)

#define tracklist_selections_action_new_edit_mute_lane( \
  track_lane, mute_new, err) \
  tracklist_selections_action_new ( \
    TracklistSelectionsActionType::TRACKLIST_SELECTIONS_ACTION_EDIT, NULL, \
    NULL, NULL, track_lane_get_track (track_lane), 0, NULL, NULL, -1, \
    track_lane->pos, NULL, -1, \
    EditTrackActionType::EDIT_TRACK_ACTION_TYPE_MUTE_LANE, mute_new, NULL, \
    0.f, 0.f, NULL, false, err)

#define tracklist_selections_action_new_edit_solo(tls_before, solo_new, err) \
  tracklist_selections_action_new ( \
    TracklistSelectionsActionType::TRACKLIST_SELECTIONS_ACTION_EDIT, \
    tls_before, NULL, NULL, NULL, 0, NULL, NULL, -1, -1, NULL, -1, \
    EditTrackActionType::EDIT_TRACK_ACTION_TYPE_SOLO, solo_new, NULL, 0.f, \
    0.f, NULL, false, err)

#define tracklist_selections_action_new_edit_solo_lane( \
  track_lane, solo_new, err) \
  tracklist_selections_action_new ( \
    TracklistSelectionsActionType::TRACKLIST_SELECTIONS_ACTION_EDIT, NULL, \
    NULL, NULL, track_lane_get_track (track_lane), 0, NULL, NULL, -1, \
    track_lane->pos, NULL, -1, \
    EditTrackActionType::EDIT_TRACK_ACTION_TYPE_SOLO_LANE, solo_new, NULL, \
    0.f, 0.f, NULL, false, err)

#define tracklist_selections_action_new_edit_listen(tls_before, solo_new, err) \
  tracklist_selections_action_new ( \
    TracklistSelectionsActionType::TRACKLIST_SELECTIONS_ACTION_EDIT, \
    tls_before, NULL, NULL, NULL, 0, NULL, NULL, -1, -1, NULL, -1, \
    EditTrackActionType::EDIT_TRACK_ACTION_TYPE_LISTEN, solo_new, NULL, 0.f, \
    0.f, NULL, false, err)

#define tracklist_selections_action_new_edit_enable( \
  tls_before, enable_new, err) \
  tracklist_selections_action_new ( \
    TracklistSelectionsActionType::TRACKLIST_SELECTIONS_ACTION_EDIT, \
    tls_before, NULL, NULL, NULL, 0, NULL, NULL, -1, -1, NULL, -1, \
    EditTrackActionType::EDIT_TRACK_ACTION_TYPE_ENABLE, enable_new, NULL, 0.f, \
    0.f, NULL, false, err)

#define tracklist_selections_action_new_edit_fold(tls_before, fold_new, err) \
  tracklist_selections_action_new ( \
    TracklistSelectionsActionType::TRACKLIST_SELECTIONS_ACTION_EDIT, \
    tls_before, NULL, NULL, NULL, 0, NULL, NULL, -1, -1, NULL, -1, \
    EditTrackActionType::EDIT_TRACK_ACTION_TYPE_FOLD, fold_new, NULL, 0.f, \
    0.f, NULL, false, err)

#define tracklist_selections_action_new_edit_direct_out( \
  tls, port_connections_mgr, direct_out, err) \
  tracklist_selections_action_new ( \
    TracklistSelectionsActionType::TRACKLIST_SELECTIONS_ACTION_EDIT, tls, NULL, \
    port_connections_mgr, NULL, ENUM_INT_TO_VALUE (TrackType, 0), NULL, NULL, \
    -1, -1, NULL, -1, EditTrackActionType::EDIT_TRACK_ACTION_TYPE_DIRECT_OUT, \
    (direct_out)->pos, NULL, 0.f, 0.f, NULL, false, err)

#define tracklist_selections_action_new_edit_remove_direct_out( \
  tls, port_connections_mgr, err) \
  tracklist_selections_action_new ( \
    TracklistSelectionsActionType::TRACKLIST_SELECTIONS_ACTION_EDIT, tls, NULL, \
    port_connections_mgr, NULL, ENUM_INT_TO_VALUE (TrackType, 0), NULL, NULL, \
    -1, -1, NULL, -1, EditTrackActionType::EDIT_TRACK_ACTION_TYPE_DIRECT_OUT, \
    -1, NULL, 0.f, 0.f, NULL, false, err)

#define tracklist_selections_action_new_edit_color(tls, color, err) \
  tracklist_selections_action_new ( \
    TracklistSelectionsActionType::TRACKLIST_SELECTIONS_ACTION_EDIT, tls, \
    NULL, NULL, NULL, ENUM_INT_TO_VALUE (TrackType, 0), NULL, NULL, -1, -1, \
    NULL, -1, EditTrackActionType::EDIT_TRACK_ACTION_TYPE_COLOR, false, color, \
    0.f, 0.f, NULL, false, err)

#define tracklist_selections_action_new_edit_icon(tls, icon, err) \
  tracklist_selections_action_new ( \
    TracklistSelectionsActionType::TRACKLIST_SELECTIONS_ACTION_EDIT, tls, \
    NULL, NULL, NULL, ENUM_INT_TO_VALUE (TrackType, 0), NULL, NULL, -1, -1, \
    NULL, -1, EditTrackActionType::EDIT_TRACK_ACTION_TYPE_ICON, false, NULL, \
    0.f, 0.f, icon, false, err)

#define tracklist_selections_action_new_edit_comment(tls, comment, err) \
  tracklist_selections_action_new ( \
    TracklistSelectionsActionType::TRACKLIST_SELECTIONS_ACTION_EDIT, tls, \
    NULL, NULL, NULL, ENUM_INT_TO_VALUE (TrackType, 0), NULL, NULL, -1, -1, \
    NULL, -1, EditTrackActionType::EDIT_TRACK_ACTION_TYPE_COMMENT, false, \
    NULL, 0.f, 0.f, comment, false, err)

#define tracklist_selections_action_new_edit_rename( \
  track, port_connections_mgr, name, err) \
  tracklist_selections_action_new ( \
    TracklistSelectionsActionType::TRACKLIST_SELECTIONS_ACTION_EDIT, NULL, \
    NULL, port_connections_mgr, track, ENUM_INT_TO_VALUE (TrackType, 0), NULL, \
    NULL, -1, -1, NULL, -1, EditTrackActionType::EDIT_TRACK_ACTION_TYPE_RENAME, \
    false, NULL, 0.f, 0.f, name, false, err)

#define tracklist_selections_action_new_edit_rename_lane(track_lane, name, err) \
  tracklist_selections_action_new ( \
    TracklistSelectionsActionType::TRACKLIST_SELECTIONS_ACTION_EDIT, NULL, \
    NULL, NULL, track_lane_get_track (track_lane), \
    ENUM_INT_TO_VALUE (TrackType, 0), NULL, NULL, -1, track_lane->pos, NULL, \
    -1, EditTrackActionType::EDIT_TRACK_ACTION_TYPE_RENAME_LANE, false, NULL, \
    0.f, 0.f, name, false, err)

/**
 * Move @ref tls to @ref track_pos.
 *
 * Tracks starting at @ref track_pos will be pushed
 * down.
 *
 * @param track_pos Track position indicating the
 *   track to push down and insert the selections
 *   above. This is the track position before the
 *   move will be executed.
 */
#define tracklist_selections_action_new_move( \
  tls, port_connections_mgr, track_pos, err) \
  tracklist_selections_action_new ( \
    TracklistSelectionsActionType::TRACKLIST_SELECTIONS_ACTION_MOVE, tls, \
    NULL, NULL, NULL, ENUM_INT_TO_VALUE (TrackType, 0), NULL, NULL, track_pos, \
    -1, NULL, -1, 0, false, NULL, 0.f, 0.f, NULL, false, err)

#define tracklist_selections_action_new_copy( \
  tls, port_connections_mgr, track_pos, err) \
  tracklist_selections_action_new ( \
    TracklistSelectionsActionType::TRACKLIST_SELECTIONS_ACTION_COPY, tls, \
    NULL, port_connections_mgr, NULL, ENUM_INT_TO_VALUE (TrackType, 0), NULL, \
    NULL, track_pos, -1, NULL, -1, 0, false, NULL, 0.f, 0.f, NULL, false, err)

/**
 * Move inside a foldable track.
 *
 * @param track_pos Foldable track index.
 *
 * When foldable tracks are included in @ref tls,
 * all their children must be marked as
 * selected as well before calling this.
 *
 * @note This should be called in combination with
 *   a move action to move the tracks to the required
 *   index after putting them inside a group.
 */
#define tracklist_selections_action_new_move_inside( \
  tls, port_connections_mgr, track_pos, err) \
  tracklist_selections_action_new ( \
    TracklistSelectionsActionType::TRACKLIST_SELECTIONS_ACTION_MOVE_INSIDE, \
    tls, NULL, NULL, NULL, ENUM_INT_TO_VALUE (TrackType, 0), NULL, NULL, \
    track_pos, -1, NULL, -1, 0, false, NULL, 0.f, 0.f, NULL, false, err)

#define tracklist_selections_action_new_copy_inside( \
  tls, port_connections_mgr, track_pos, err) \
  tracklist_selections_action_new ( \
    TracklistSelectionsActionType::TRACKLIST_SELECTIONS_ACTION_COPY_INSIDE, \
    tls, NULL, port_connections_mgr, NULL, ENUM_INT_TO_VALUE (TrackType, 0), \
    NULL, NULL, track_pos, -1, NULL, -1, 0, false, NULL, 0.f, 0.f, NULL, \
    false, err)

#define tracklist_selections_action_new_delete(tls, port_connections_mgr, err) \
  tracklist_selections_action_new ( \
    TracklistSelectionsActionType::TRACKLIST_SELECTIONS_ACTION_DELETE, tls, \
    NULL, port_connections_mgr, NULL, ENUM_INT_TO_VALUE (TrackType, 0), NULL, \
    NULL, -1, -1, NULL, -1, 0, false, NULL, 0.f, 0.f, NULL, false, err)

/**
 * Toggle the current pin status of the track.
 */
#define tracklist_selections_action_new_pin(tls, port_connections_mgr, err) \
  tracklist_selections_action_new ( \
    TracklistSelectionsActionType::TRACKLIST_SELECTIONS_ACTION_PIN, tls, NULL, \
    port_connections_mgr, NULL, ENUM_INT_TO_VALUE (TrackType, 0), NULL, NULL, \
    -1, -1, NULL, -1, 0, false, NULL, 0.f, 0.f, NULL, false, err)

/**
 * Toggle the current pin status of the track.
 */
#define tracklist_selections_action_new_unpin(tls, port_connections_mgr, err) \
  tracklist_selections_action_new ( \
    TracklistSelectionsActionType::TRACKLIST_SELECTIONS_ACTION_UNPIN, tls, \
    NULL, port_connections_mgr, NULL, ENUM_INT_TO_VALUE (TrackType, 0), NULL, \
    NULL, -1, -1, NULL, -1, 0, false, NULL, 0.f, 0.f, NULL, false, err)

NONNULL TracklistSelectionsAction *
tracklist_selections_action_clone (const TracklistSelectionsAction * src);

bool
tracklist_selections_action_perform (
  TracklistSelectionsActionType  type,
  TracklistSelections *          tls_before,
  TracklistSelections *          tls_after,
  const PortConnectionsManager * port_connections_mgr,
  Track *                        track,
  TrackType                      track_type,
  const PluginSetting *          pl_setting,
  const SupportedFile *          file_descr,
  int                            track_pos,
  int                            lane_pos,
  const Position *               pos,
  int                            num_tracks,
  EditTrackActionType            edit_type,
  int                            ival_after,
  const GdkRGBA *                color_new,
  float                          val_before,
  float                          val_after,
  const char *                   new_txt,
  bool                           already_edited,
  GError **                      error);

/**
 * @param disable_track_pos Track position to
 *   disable, or -1 to not disable any track.
 */
#define tracklist_selections_action_perform_create( \
  track_type, pl_setting, file_descr, track_pos, pos, num_tracks, \
  disable_track_pos, err) \
  tracklist_selections_action_perform ( \
    TracklistSelectionsActionType::TRACKLIST_SELECTIONS_ACTION_CREATE, NULL, \
    NULL, NULL, NULL, track_type, pl_setting, file_descr, track_pos, -1, pos, \
    num_tracks, EditTrackActionType::EDIT_TRACK_ACTION_TYPE_MUTE, \
    disable_track_pos, NULL, 0.f, 0.f, NULL, false, err)

/**
 * Creates a perform TracklistSelectionsAction for an
 * audio FX track.
 */
#define tracklist_selections_action_perform_create_audio_fx( \
  pl_setting, track_pos, num_tracks, err) \
  tracklist_selections_action_perform_create ( \
    TRACK_TYPE_AUDIO_BUS, pl_setting, NULL, track_pos, NULL, num_tracks, -1, \
    err)

/**
 * Creates a perform TracklistSelectionsAction for a
 * MIDI FX track.
 */
#define tracklist_selections_action_perform_create_midi_fx( \
  pl_setting, track_pos, num_tracks, err) \
  tracklist_selections_action_perform_create ( \
    TRACK_TYPE_MIDI_BUS, pl_setting, NULL, track_pos, NULL, num_tracks, -1, \
    err)

/**
 * Creates a perform TracklistSelectionsAction for an
 * instrument track.
 */
#define tracklist_selections_action_perform_create_instrument( \
  pl_setting, track_pos, num_tracks, err) \
  tracklist_selections_action_perform_create ( \
    TRACK_TYPE_INSTRUMENT, pl_setting, NULL, track_pos, NULL, num_tracks, -1, \
    err)

/**
 * Creates a perform TracklistSelectionsAction for an
 * audio group track.
 */
#define tracklist_selections_action_perform_create_audio_group( \
  track_pos, num_tracks, err) \
  tracklist_selections_action_perform_create ( \
    TRACK_TYPE_AUDIO_GROUP, NULL, NULL, track_pos, NULL, num_tracks, -1, err)

/**
 * Creates a perform TracklistSelectionsAction for a
 * MIDI group track.
 */
#define tracklist_selections_action_perform_create_midi_group( \
  track_pos, num_tracks, err) \
  tracklist_selections_action_perform_create ( \
    TRACK_TYPE_MIDI_GROUP, NULL, NULL, track_pos, NULL, num_tracks, -1, err)

/**
 * Creates a perform TracklistSelectionsAction for a MIDI
 * track.
 */
#define tracklist_selections_action_perform_create_midi( \
  track_pos, num_tracks, err) \
  tracklist_selections_action_perform_create ( \
    TrackType::TRACK_TYPE_MIDI, NULL, NULL, track_pos, NULL, num_tracks, -1, \
    err)

/**
 * Creates a perform TracklistSelectionsAction for a
 * folder track.
 */
#define tracklist_selections_action_perform_create_folder( \
  track_pos, num_tracks, err) \
  tracklist_selections_action_perform_create ( \
    TRACK_TYPE_FOLDER, NULL, NULL, track_pos, NULL, num_tracks, -1, err)

/**
 * Generic edit action.
 */
#define tracklist_selections_action_perform_edit_generic( \
  type, tls_before, tls_after, already_edited, err) \
  tracklist_selections_action_perform ( \
    TracklistSelectionsActionType::TRACKLIST_SELECTIONS_ACTION_EDIT, \
    tls_before, tls_after, NULL, NULL, 0, NULL, NULL, -1, -1, NULL, -1, type, \
    false, NULL, 0.f, 0.f, NULL, already_edited, err)

/**
 * Convenience wrapper over
 * tracklist_selections_action_perform() for single-track
 * float edit changes.
 */
#define tracklist_selections_action_perform_edit_single_float( \
  type, track, val_before, val_after, already_edited, err) \
  tracklist_selections_action_perform ( \
    TracklistSelectionsActionType::TRACKLIST_SELECTIONS_ACTION_EDIT, NULL, \
    NULL, NULL, track, TrackType::TRACK_TYPE_MASTER, NULL, NULL, -1, -1, NULL, \
    -1, type, false, NULL, val_before, val_after, NULL, already_edited, err)

/**
 * Convenience wrapper over
 * tracklist_selections_action_perform() for single-track
 * int edit changes.
 */
#define tracklist_selections_action_perform_edit_single_int( \
  type, track, val_after, already_edited, err) \
  tracklist_selections_action_perform ( \
    TracklistSelectionsActionType::TRACKLIST_SELECTIONS_ACTION_EDIT, NULL, \
    NULL, NULL, track, ENUM_INT_TO_VALUE (TrackType, 0), NULL, NULL, -1, -1, \
    NULL, -1, type, val_after, NULL, 0.f, 0.f, NULL, already_edited, err)

#define tracklist_selections_action_perform_edit_mute( \
  tls_before, mute_new, err) \
  tracklist_selections_action_perform ( \
    TracklistSelectionsActionType::TRACKLIST_SELECTIONS_ACTION_EDIT, \
    tls_before, NULL, NULL, NULL, ENUM_INT_TO_VALUE (TrackType, 0), NULL, \
    NULL, -1, -1, NULL, -1, EditTrackActionType::EDIT_TRACK_ACTION_TYPE_MUTE, \
    mute_new, NULL, 0.f, 0.f, NULL, false, err)

#define tracklist_selections_action_perform_edit_mute_lane( \
  track_lane, mute_new, err) \
  tracklist_selections_action_perform ( \
    TracklistSelectionsActionType::TRACKLIST_SELECTIONS_ACTION_EDIT, NULL, \
    NULL, NULL, track_lane_get_track (track_lane), \
    ENUM_INT_TO_VALUE (TrackType, 0), NULL, NULL, -1, track_lane->pos, NULL, \
    -1, EditTrackActionType::EDIT_TRACK_ACTION_TYPE_MUTE_LANE, mute_new, NULL, \
    0.f, 0.f, NULL, false, err)

#define tracklist_selections_action_perform_edit_solo( \
  tls_before, solo_new, err) \
  tracklist_selections_action_perform ( \
    TracklistSelectionsActionType::TRACKLIST_SELECTIONS_ACTION_EDIT, \
    tls_before, NULL, NULL, NULL, ENUM_INT_TO_VALUE (TrackType, 0), NULL, \
    NULL, -1, -1, NULL, -1, EditTrackActionType::EDIT_TRACK_ACTION_TYPE_SOLO, \
    solo_new, NULL, 0.f, 0.f, NULL, false, err)

#define tracklist_selections_action_perform_edit_solo_lane( \
  track_lane, solo_new, err) \
  tracklist_selections_action_perform ( \
    TracklistSelectionsActionType::TRACKLIST_SELECTIONS_ACTION_EDIT, NULL, \
    NULL, NULL, track_lane_get_track (track_lane), \
    ENUM_INT_TO_VALUE (TrackType, 0), NULL, NULL, -1, track_lane->pos, NULL, \
    -1, EditTrackActionType::EDIT_TRACK_ACTION_TYPE_SOLO_LANE, solo_new, NULL, \
    0.f, 0.f, NULL, false, err)

#define tracklist_selections_action_perform_edit_listen( \
  tls_before, solo_new, err) \
  tracklist_selections_action_perform ( \
    TracklistSelectionsActionType::TRACKLIST_SELECTIONS_ACTION_EDIT, \
    tls_before, NULL, NULL, NULL, ENUM_INT_TO_VALUE (TrackType, 0), NULL, \
    NULL, -1, -1, NULL, -1, EditTrackActionType::EDIT_TRACK_ACTION_TYPE_LISTEN, \
    solo_new, NULL, 0.f, 0.f, NULL, false, err)

#define tracklist_selections_action_perform_edit_enable( \
  tls_before, enable_new, err) \
  tracklist_selections_action_perform ( \
    TracklistSelectionsActionType::TRACKLIST_SELECTIONS_ACTION_EDIT, \
    tls_before, NULL, NULL, NULL, ENUM_INT_TO_VALUE (TrackType, 0), NULL, \
    NULL, -1, -1, NULL, -1, EditTrackActionType::EDIT_TRACK_ACTION_TYPE_ENABLE, \
    enable_new, NULL, 0.f, 0.f, NULL, false, err)

#define tracklist_selections_action_perform_edit_fold( \
  tls_before, fold_new, err) \
  tracklist_selections_action_perform ( \
    TracklistSelectionsActionType::TRACKLIST_SELECTIONS_ACTION_EDIT, \
    tls_before, NULL, NULL, NULL, ENUM_INT_TO_VALUE (TrackType, 0), NULL, \
    NULL, -1, -1, NULL, -1, EditTrackActionType::EDIT_TRACK_ACTION_TYPE_FOLD, \
    fold_new, NULL, 0.f, 0.f, NULL, false, err)

#define tracklist_selections_action_perform_edit_direct_out( \
  tls, port_connections_mgr, direct_out, err) \
  tracklist_selections_action_perform ( \
    TracklistSelectionsActionType::TRACKLIST_SELECTIONS_ACTION_EDIT, tls, \
    NULL, port_connections_mgr, NULL, 0, NULL, NULL, -1, -1, NULL, -1, \
    EditTrackActionType::EDIT_TRACK_ACTION_TYPE_DIRECT_OUT, (direct_out)->pos, \
    NULL, 0.f, 0.f, NULL, false, err)

#define tracklist_selections_action_perform_edit_remove_direct_out( \
  tls, port_connections_mgr, err) \
  tracklist_selections_action_perform ( \
    TracklistSelectionsActionType::TRACKLIST_SELECTIONS_ACTION_EDIT, tls, \
    NULL, port_connections_mgr, NULL, 0, NULL, NULL, -1, -1, NULL, -1, \
    EditTrackActionType::EDIT_TRACK_ACTION_TYPE_DIRECT_OUT, -1, NULL, 0.f, \
    0.f, NULL, false, err)

#define tracklist_selections_action_perform_edit_color(tls, color, err) \
  tracklist_selections_action_perform ( \
    TracklistSelectionsActionType::TRACKLIST_SELECTIONS_ACTION_EDIT, tls, \
    NULL, NULL, NULL, ENUM_INT_TO_VALUE (TrackType, 0), NULL, NULL, -1, -1, \
    NULL, -1, EditTrackActionType::EDIT_TRACK_ACTION_TYPE_COLOR, false, color, \
    0.f, 0.f, NULL, false, err)

#define tracklist_selections_action_perform_edit_icon(tls, icon, err) \
  tracklist_selections_action_perform ( \
    TracklistSelectionsActionType::TRACKLIST_SELECTIONS_ACTION_EDIT, tls, \
    NULL, NULL, NULL, ENUM_INT_TO_VALUE (TrackType, 0), NULL, NULL, -1, -1, \
    NULL, -1, EditTrackActionType::EDIT_TRACK_ACTION_TYPE_ICON, false, NULL, \
    0.f, 0.f, icon, false, err)

#define tracklist_selections_action_perform_edit_comment(tls, comment, err) \
  tracklist_selections_action_perform ( \
    TracklistSelectionsActionType::TRACKLIST_SELECTIONS_ACTION_EDIT, tls, \
    NULL, NULL, NULL, ENUM_INT_TO_VALUE (TrackType, 0), NULL, NULL, -1, -1, \
    NULL, -1, EditTrackActionType::EDIT_TRACK_ACTION_TYPE_COMMENT, false, \
    NULL, 0.f, 0.f, comment, false, err)

#define tracklist_selections_action_perform_edit_rename( \
  track, port_connections_mgr, name, err) \
  tracklist_selections_action_perform ( \
    TracklistSelectionsActionType::TRACKLIST_SELECTIONS_ACTION_EDIT, NULL, \
    NULL, port_connections_mgr, track, ENUM_INT_TO_VALUE (TrackType, 0), NULL, \
    NULL, -1, -1, NULL, -1, EditTrackActionType::EDIT_TRACK_ACTION_TYPE_RENAME, \
    false, NULL, 0.f, 0.f, name, false, err)

#define tracklist_selections_action_perform_edit_rename_lane( \
  track_lane, name, err) \
  tracklist_selections_action_perform ( \
    TracklistSelectionsActionType::TRACKLIST_SELECTIONS_ACTION_EDIT, NULL, \
    NULL, NULL, track_lane_get_track (track_lane), \
    ENUM_INT_TO_VALUE (TrackType, 0), NULL, NULL, -1, track_lane->pos, NULL, \
    -1, EditTrackActionType::EDIT_TRACK_ACTION_TYPE_RENAME_LANE, false, NULL, \
    0.f, 0.f, name, false, err)

/**
 * Move @ref tls to @ref track_pos.
 *
 * Tracks starting at @ref track_pos will be pushed
 * down.
 *
 * @param track_pos Track position indicating the
 *   track to push down and insert the selections
 *   above. This is the track position before the
 *   move will be executed.
 */
#define tracklist_selections_action_perform_move( \
  tls, port_connections_mgr, track_pos, err) \
  tracklist_selections_action_perform ( \
    TracklistSelectionsActionType::TRACKLIST_SELECTIONS_ACTION_MOVE, tls, \
    NULL, NULL, NULL, ENUM_INT_TO_VALUE (TrackType, 0), NULL, NULL, track_pos, \
    -1, NULL, -1, ENUM_INT_TO_VALUE (EditTrackActionType, 0), false, NULL, \
    0.f, 0.f, NULL, false, err)

#define tracklist_selections_action_perform_copy( \
  tls, port_connections_mgr, track_pos, err) \
  tracklist_selections_action_perform ( \
    TracklistSelectionsActionType::TRACKLIST_SELECTIONS_ACTION_COPY, tls, \
    NULL, port_connections_mgr, NULL, ENUM_INT_TO_VALUE (TrackType, 0), NULL, \
    NULL, track_pos, -1, NULL, -1, ENUM_INT_TO_VALUE (EditTrackActionType, 0), \
    false, NULL, 0.f, 0.f, NULL, false, err)

/**
 * Move inside a foldable track.
 *
 * @param track_pos Foldable track index.
 *
 * When foldable tracks are included in @ref tls,
 * all their children must be marked as
 * selected as well before calling this.
 *
 * @note This should be called in combination with
 *   a move action to move the tracks to the required
 *   index after putting them inside a group.
 */
#define tracklist_selections_action_perform_move_inside( \
  tls, port_connections_mgr, track_pos, err) \
  tracklist_selections_action_perform ( \
    TracklistSelectionsActionType::TRACKLIST_SELECTIONS_ACTION_MOVE_INSIDE, \
    tls, NULL, NULL, NULL, ENUM_INT_TO_VALUE (TrackType, 0), NULL, NULL, \
    track_pos, -1, NULL, -1, ENUM_INT_TO_VALUE (EditTrackActionType, 0), \
    false, NULL, 0.f, 0.f, NULL, false, err)

#define tracklist_selections_action_perform_copy_inside( \
  tls, port_connections_mgr, track_pos, err) \
  tracklist_selections_action_perform ( \
    TracklistSelectionsActionType::TRACKLIST_SELECTIONS_ACTION_COPY_INSIDE, \
    tls, NULL, port_connections_mgr, NULL, ENUM_INT_TO_VALUE (TrackType, 0), \
    NULL, NULL, track_pos, -1, NULL, -1, \
    ENUM_INT_TO_VALUE (EditTrackActionType, 0), false, NULL, 0.f, 0.f, NULL, \
    false, err)

#define tracklist_selections_action_perform_delete( \
  tls, port_connections_mgr, err) \
  tracklist_selections_action_perform ( \
    TracklistSelectionsActionType::TRACKLIST_SELECTIONS_ACTION_DELETE, tls, \
    NULL, port_connections_mgr, NULL, ENUM_INT_TO_VALUE (TrackType, 0), NULL, \
    NULL, -1, -1, NULL, -1, ENUM_INT_TO_VALUE (EditTrackActionType, 0), false, \
    NULL, 0.f, 0.f, NULL, false, err)

/**
 * Toggle the current pin status of the track.
 */
#define tracklist_selections_action_perform_pin(tls, port_connections_mgr, err) \
  tracklist_selections_action_perform ( \
    TracklistSelectionsActionType::TRACKLIST_SELECTIONS_ACTION_PIN, tls, NULL, \
    port_connections_mgr, NULL, ENUM_INT_TO_VALUE (TrackType, 0), NULL, NULL, \
    -1, -1, NULL, -1, ENUM_INT_TO_VALUE (EditTrackActionType, 0), false, NULL, \
    0.f, 0.f, NULL, false, err)

/**
 * Toggle the current pin status of the track.
 */
#define tracklist_selections_action_perform_unpin( \
  tls, port_connections_mgr, err) \
  tracklist_selections_action_perform ( \
    TracklistSelectionsActionType::TRACKLIST_SELECTIONS_ACTION_UNPIN, tls, \
    NULL, port_connections_mgr, NULL, ENUM_INT_TO_VALUE (TrackType, 0), NULL, \
    NULL, -1, -1, NULL, -1, ENUM_INT_TO_VALUE (EditTrackActionType, 0), false, \
    NULL, 0.f, 0.f, NULL, false, err)

/**
 * Edit or remove direct out.
 *
 * @param direct_out A track to route the
 *   selections to, or NULL to route nowhere.
 *
 * @return Whether successful.
 */
bool
tracklist_selections_action_perform_set_direct_out (
  TracklistSelections *    self,
  PortConnectionsManager * port_connections_mgr,
  Track *                  direct_out,
  GError **                error);

int
tracklist_selections_action_do (
  TracklistSelectionsAction * self,
  GError **                   error);

int
tracklist_selections_action_undo (
  TracklistSelectionsAction * self,
  GError **                   error);

char *
tracklist_selections_action_stringize (TracklistSelectionsAction * self);

void
tracklist_selections_action_free (TracklistSelectionsAction * self);

/**
 * @}
 */

#endif
