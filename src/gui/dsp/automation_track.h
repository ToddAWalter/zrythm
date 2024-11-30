// SPDX-FileCopyrightText: © 2018-2022, 2024 Alexandros Theodotou <alex@zrythm.org>
// SPDX-License-Identifier: LicenseRef-ZrythmLicense

#ifndef __AUDIO_AUTOMATION_TRACK_H__
#define __AUDIO_AUTOMATION_TRACK_H__

#include "zrythm-config.h"

#include "gui/dsp/automation_point.h"
#include "gui/dsp/port.h"
#include "gui/dsp/region_owner.h"

#include <QtQmlIntegration>

#include "automation_region.h"
#include "dsp/position.h"

class Port;
class AutomatableTrack;
class AutomationTracklist;

/**
 * @addtogroup dsp
 *
 * @{
 */

/** Release time in ms when in touch record mode. */
constexpr int AUTOMATION_RECORDING_TOUCH_REL_MS = 800;

constexpr int AUTOMATION_TRACK_DEFAULT_HEIGHT = 48;

enum class AutomationRecordMode
{
  Touch,
  Latch,
  NUM_AUTOMATION_RECORD_MODES,
};

DEFINE_ENUM_FORMATTER (
  AutomationRecordMode,
  AutomationRecordMode,
  QT_TR_NOOP_UTF8 ("Touch"),
  QT_TR_NOOP_UTF8 ("Latch"));

class AutomationTrack final
    : public QObject,
      public ICloneable<AutomationTrack>,
      public RegionOwnerImpl<AutomationRegion>,
      public zrythm::utils::serialization::ISerializable<AutomationTrack>
{
  Q_OBJECT
  QML_ELEMENT
  Q_PROPERTY (double height READ getHeight WRITE setHeight NOTIFY heightChanged)
  Q_PROPERTY (QString label READ getLabel NOTIFY labelChanged)
  Q_PROPERTY (
    int automationMode READ getAutomationMode WRITE setAutomationMode NOTIFY
      automationModeChanged)
  Q_PROPERTY (
    int recordMode READ getRecordMode WRITE setRecordMode NOTIFY
      recordModeChanged)
  DEFINE_REGION_OWNER_QML_PROPERTIES (AutomationTrack)

public:
  using Position = dsp::Position;
  using PortIdentifier = dsp::PortIdentifier;

public:
  AutomationTrack () = default;

  /** Creates an automation track for the given Port. */
  AutomationTrack (ControlPort &port);

  using RegionOwnerImplType = RegionOwnerImpl<AutomationRegion>;

public:
  // ========================================================================
  // QML Interface
  // ========================================================================

  QString getLabel () const
  {
    return QString::fromStdString (port_id_->get_label ());
  }
  Q_SIGNAL void labelChanged (QString label);

  double        getHeight () const { return height_; }
  void          setHeight (double height);
  Q_SIGNAL void heightChanged (double height);

  int getAutomationMode () const
  {
    return ENUM_VALUE_TO_INT (automation_mode_);
  }
  void          setAutomationMode (int automation_mode);
  Q_SIGNAL void automationModeChanged (int automation_mode);

  int  getRecordMode () const { return ENUM_VALUE_TO_INT (record_mode_); }
  void setRecordMode (int record_mode);
  Q_SIGNAL void recordModeChanged (int record_mode);

  // ========================================================================

  void init_loaded (AutomationTracklist * atl);

  bool validate () const;

  bool is_in_active_project () const override;
  bool is_auditioner () const override;

  /**
   * @note This is expensive and should only be used
   *   if @ref PortIdentifier.at_idx is not set. Use
   *   port_get_automation_track() instead.
   *
   * @param basic_search If true, only basic port
   *   identifier members are checked.
   */
  static AutomationTrack *
  find_from_port_id (const PortIdentifier &id, bool basic_search);

  /**
   * @brief Clone the given port identifier and take ownership of the clone.
   * @param port_id
   */
  void set_port_id (const PortIdentifier &port_id);

  /**
   * Finds the AutomationTrack associated with `port`.
   *
   * @param track The track that owns the port, if known.
   *
   * FIXME use a hashtable
   */
  ATTR_HOT static AutomationTrack * find_from_port (
    const ControlPort       &port,
    const AutomatableTrack * track,
    bool                     basic_search);

  void set_automation_mode (AutomationMode mode, bool fire_events);

  inline void swap_record_mode ()
  {
    record_mode_ = static_cast<AutomationRecordMode> (
      (static_cast<int> (record_mode_) + 1)
      % static_cast<int> (AutomationRecordMode::NUM_AUTOMATION_RECORD_MODES));
  }

  AutomationTracklist * get_automation_tracklist () const;

  /**
   * Returns whether the automation in the automation
   * track should be read.
   *
   * @param cur_time Current time from
   *   g_get_monotonic_time() passed here to ensure
   *   the same timestamp is Pmused in sequential calls.
   */
  ATTR_HOT bool should_read_automation (RtTimePoint cur_time) const;

  /**
   * Returns if the automation track should currently be recording data.
   *
   * Returns false if in touch mode after the release time has passed.
   *
   * This function assumes that the transport will be rolling.
   *
   * @param cur_time Current time from g_get_monotonic_time() passed here to
   *   ensure the same timestamp is used in sequential calls.
   * @param record_aps If set to true, this function will return whether we
   *   should be recording automation point data. If set to false, this
   *   function will return whether we should be recording a region (eg, if an
   *   automation point was already created before and we are still recording
   *   inside a region regardless of whether we should create/edit automation
   *   points or not.
   */
  ATTR_HOT bool
  should_be_recording (RtTimePoint cur_time, bool record_aps) const;

  /**
   * Sets the index of the AutomationTrack in the AutomationTracklist.
   */
  void set_index (int index);

  /**
   * Clones the AutomationTrack.
   */
  void init_after_cloning (const AutomationTrack &other) override;

  AutomatableTrack * get_track () const;

  /**
   * Returns the automation point before the Position on the timeline.
   *
   * @param ends_after Whether to only check in regions that also end after
   * \ref pos (ie, the region surrounds @ref pos), otherwise check in the
   * region that ends last.
   */
  AutomationPoint *
  get_ap_before_pos (const Position &pos, bool ends_after, bool use_snapshots)
    const;

  /**
   * Returns the Region that starts before given Position, if any.
   *
   * @param ends_after Whether to only check for regions that also end after
   * @ref pos (ie, the region surrounds @ref pos), otherwise get the region
   * that ends last.
   */
  AutomationRegion *
  get_region_before_pos (const Position &pos, bool ends_after, bool use_snapshots)
    const;

  /**
   * Returns the actual parameter value at the given position.
   *
   * If there is no automation point/curve during the position, it returns the
   * current value of the parameter it is automating.
   *
   * @param normalized Whether to return the value normalized.
   * @param ends_after Whether to only check in regions that also end after
   * \ref pos (ie, the region surrounds @ref pos), otherwise check in the
   * region that ends last.
   * @param use_snapshots Whether to get the value from the snapshotted
   * (cached) regions. This should be set to true when called during dsp
   * playback. TODO unimplemented
   */
  float get_val_at_pos (
    const Position &pos,
    bool            normalized,
    bool            ends_after,
    bool            use_snapshots) const;

  static int get_y_px_from_height_and_normalized_val (
    const float height,
    const float normalized_val)
  {
    return static_cast<int> (height - normalized_val * height);
  }

  /**
   * Returns the y pixels from the value based on the allocation of the
   * automation track.
   */
  int get_y_px_from_normalized_val (float normalized_val) const
  {
    return get_y_px_from_height_and_normalized_val (
      static_cast<float> (height_), normalized_val);
  }

  void set_caches (CacheType types);

  bool contains_automation () const { return !region_list_->regions_.empty (); }

  bool verify () const;

  DECLARE_DEFINE_FIELDS_METHOD ();

public:
  /** Index in parent AutomationTracklist. */
  int index_ = 0;

  /** Identifier of the Port this AutomationTrack is for (owned pointer). */
  std::unique_ptr<dsp::PortIdentifier> port_id_;

  /** Whether it has been created by the user yet or not. */
  bool created_ = false;

  /**
   * Whether visible or not.
   *
   * Being created is a precondition for this.
   *
   * @important Must only be set with automation_tracklist_set_at_visible().
   */
  bool visible_ = false;

  /** Y local to track. */
  int y_ = 0;

  /** Position of multipane handle. */
  double height_ = AUTOMATION_TRACK_DEFAULT_HEIGHT;

  /** Last value recorded in this automation track. */
  float last_recorded_value_ = 0.f;

  /** Automation mode. */
  AutomationMode automation_mode_ = AutomationMode::Read;

  /** Automation record mode, when @ref automation_mode_ is set to record. */
  AutomationRecordMode record_mode_ = (AutomationRecordMode) 0;

  /** To be set to true when recording starts (when the first change is
   * received) and false when recording ends. */
  bool recording_started_ = false;

  /** Region currently recording to. */
  AutomationRegion * recording_region_ = nullptr;

  /**
   * This is a flag to let the recording manager know that a START signal was
   * already sent for recording.
   *
   * This is because @ref AutomationTrack.recording_region takes a cycle or 2
   * to become non-NULL.
   */
  bool recording_start_sent_ = false;

  /**
   * This must only be set by the RecordingManager when temporarily pausing
   * recording, eg when looping or leaving the punch range.
   *
   * See \ref
   * RECORDING_EVENT_TYPE_PAUSE_AUTOMATION_RECORDING.
   */
  bool recording_paused_ = false;

  /** Pointer to owner automation tracklist, if any. */
  AutomationTracklist * atl_ = nullptr;

  /** Cache used during DSP. */
  ControlPort * port_ = nullptr;
};

#endif // __AUDIO_AUTOMATION_TRACK_H__
