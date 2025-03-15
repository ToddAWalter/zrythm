// SPDX-FileCopyrightText: © 2018-2022, 2024 Alexandros Theodotou <alex@zrythm.org>
// SPDX-License-Identifier: LicenseRef-ZrythmLicense

#ifndef __AUDIO_TRACK_H__
#define __AUDIO_TRACK_H__

#include "gui/dsp/automation_tracklist.h"
#include "gui/dsp/fader.h"
#include "gui/dsp/plugin.h"
#include "gui/dsp/region.h"
#include "gui/dsp/track_lane.h"

#include <QColor>
#include <QHash>

#include "utils/format.h"

using namespace zrythm;

class FileDescriptor;
class FoldableTrack;
class ChannelTrack;
struct FileImportInfo;

/**
 * @addtogroup dsp
 *
 * @{
 */

constexpr int TRACK_MIN_HEIGHT = 26;
constexpr int TRACK_DEF_HEIGHT = 52;

constexpr int TRACK_MAGIC = 21890135;
#define IS_TRACK(x) (((Track *) x)->magic_ == TRACK_MAGIC)
#define IS_TRACK_AND_NONNULL(x) (x && IS_TRACK (x))

#define DECLARE_FINAL_TRACK_CONSTRUCTORS(ClassType) \
public: \
  ClassType (const DeserializationDependencyHolder &dh) \
      : ClassType ( \
          dh.get<std::reference_wrapper<TrackRegistry>> ().get (), \
          dh.get<std::reference_wrapper<PluginRegistry>> ().get (), \
          dh.get<std::reference_wrapper<PortRegistry>> ().get (), false) \
  { \
  } \
\
private: \
  ClassType ( \
    TrackRegistry &track_registry, PluginRegistry &plugin_registry, \
    PortRegistry &port_registry, bool new_identity);

#define DEFINE_TRACK_QML_PROPERTIES(ClassType) \
public: \
  /* ================================================================ */ \
  /* name */ \
  /* ================================================================ */ \
  Q_PROPERTY (QString name READ getName WRITE setName NOTIFY nameChanged) \
  QString getName () const \
  { \
    return QString::fromStdString (name_); \
  } \
  void setName (const QString &name) \
  { \
    if (name_ == name.toStdString ()) \
      return; \
\
    name_ = name.toStdString (); \
    Q_EMIT nameChanged (name); \
  } \
\
  Q_SIGNAL void nameChanged (const QString &name); \
\
  Q_INVOKABLE unsigned int getNameHash () const \
  { \
    return qHash (QString::fromStdString (name_)); \
  } \
\
  /* ================================================================ */ \
  /* color */ \
  /* ================================================================ */ \
  Q_PROPERTY (QColor color READ getColor WRITE setColor NOTIFY colorChanged) \
\
  QColor getColor () const \
  { \
    return color_.to_qcolor (); \
  } \
  void setColor (const QColor &color) \
  { \
    if (color_.to_qcolor () == color) \
      return; \
\
    color_ = color; \
    Q_EMIT colorChanged (color); \
  } \
\
  Q_SIGNAL void colorChanged (const QColor &color); \
\
  /* ================================================================ */ \
  /* color */ \
  /* ================================================================ */ \
  Q_PROPERTY ( \
    bool visible READ getVisible WRITE setVisible NOTIFY visibleChanged) \
  bool getVisible () const \
  { \
    return visible_; \
  } \
  void setVisible (bool visible) \
  { \
    if (visible_ == visible) \
      return; \
\
    visible_ = visible; \
    Q_EMIT visibleChanged (visible); \
  } \
\
  Q_SIGNAL void visibleChanged (bool visible); \
\
  /* ================================================================ */ \
  /* enabled */ \
  /* ================================================================ */ \
  Q_PROPERTY ( \
    bool enabled READ getEnabled WRITE setEnabled NOTIFY enabledChanged) \
  bool getEnabled () const \
  { \
    return enabled_; \
  } \
\
  void setEnabled (bool enabled) \
  { \
    set_enabled (enabled, true, true, true); \
  } \
  Q_SIGNAL void enabledChanged (bool enabled); \
\
  /* ================================================================ */ \
  /* selected */ \
  /* ================================================================ */ \
  Q_PROPERTY (bool selected READ getSelected NOTIFY selectedChanged) \
  bool getSelected () const \
  { \
    return is_selected (); \
  } \
  void setSelected (bool selected) \
  { \
    if (selected_ == selected) \
      return; \
\
    selected_ = selected; \
    Q_EMIT selectedChanged (selected); \
  } \
  Q_SIGNAL void selectedChanged (bool selected); \
\
  /* ================================================================ */ \
  /* type */ \
  /* ================================================================ */ \
  Q_PROPERTY (int type READ getType CONSTANT) \
  int getType () const \
  { \
    return ENUM_VALUE_TO_INT (type_); \
  } \
  /* ================================================================ */ \
  /* isRecordable */ \
  /* ================================================================ */ \
  Q_PROPERTY (bool isRecordable READ getIsRecordable CONSTANT) \
  bool getIsRecordable () const \
  { \
    if constexpr (std::derived_from<ClassType, RecordableTrack>) \
      { \
        return true; \
      } \
    return false; \
  } \
\
  /* ================================================================ */ \
  /* hasLanes */ \
  /* ================================================================ */ \
  Q_PROPERTY (bool hasLanes READ getHasLanes CONSTANT) \
  bool getHasLanes () const \
  { \
    if constexpr (std::derived_from<ClassType, LanedTrack>) \
      { \
        return true; \
      } \
    return false; \
  } \
\
  /* ================================================================ */ \
  /* hasChannel */ \
  /* ================================================================ */ \
  Q_PROPERTY (bool hasChannel READ getHasChannel CONSTANT) \
  bool getHasChannel () const \
  { \
    if constexpr (std::derived_from<ClassType, ChannelTrack>) \
      { \
        return true; \
      } \
    return false; \
  } \
\
  /* ================================================================ */ \
  /* hasChannel */ \
  /* ================================================================ */ \
  Q_PROPERTY (bool isRegionOwner READ getIsRegionOwner CONSTANT) \
  bool getIsRegionOwner () const \
  { \
    if constexpr (std::derived_from<ClassType, RegionOwner>) \
      { \
        return true; \
      } \
    return false; \
  } \
\
  /* ================================================================ */ \
  /* isAutomatable */ \
  /* ================================================================ */ \
  Q_PROPERTY (bool isAutomatable READ getIsAutomatable CONSTANT) \
  bool getIsAutomatable () const \
  { \
    if constexpr (std::derived_from<ClassType, AutomatableTrack>) \
      { \
        return true; \
      } \
    return false; \
  } \
\
  /* ================================================================ */ \
  /* height */ \
  /* ================================================================ */ \
  Q_PROPERTY ( \
    double height READ getHeight WRITE setHeight NOTIFY heightChanged) \
  double getHeight () const \
  { \
    return main_height_; \
  } \
  void setHeight (double height) \
  { \
    if (utils::math::floats_equal (height, main_height_)) \
      { \
        return; \
      } \
    main_height_ = height; \
    Q_EMIT heightChanged (height); \
  } \
  Q_SIGNAL void heightChanged (double height); \
  Q_PROPERTY ( \
    double fullVisibleHeight READ getFullVisibleHeight NOTIFY \
      fullVisibleHeightChanged) \
  double getFullVisibleHeight () const \
  { \
    return get_full_visible_height (); \
  } \
  Q_SIGNAL void fullVisibleHeightChanged (); \
  /* ================================================================ */ \
  /* icon */ \
  /* ================================================================ */ \
  Q_PROPERTY (QString icon READ getIcon WRITE setIcon NOTIFY iconChanged) \
  QString getIcon () const \
  { \
    return QString::fromStdString (icon_name_); \
  } \
  void setIcon (const QString &icon) \
  { \
    if (icon.toStdString () == icon_name_) \
      { \
        return; \
      } \
    icon_name_ = icon.toStdString (); \
    Q_EMIT iconChanged (icon); \
  } \
  Q_SIGNAL void iconChanged (const QString &icon);

/**
 * Called when track(s) are actually imported into the project.
 */
using TracksReadyCallback = void (*) (const FileImportInfo *);

/**
 * @brief Represents a track in the project.
 *
 * The `Track` class is the base class for all types of tracks in the
 * project. It provides common functionality and properties shared by
 * all track types, such as the track's position in the tracklist, its label,
 * and whether it is muted.
 *
 * Subclasses of `Track` represent specific types of tracks, such as
 * MIDI tracks, instrument tracks, and audio tracks.
 */
class Track
    : public dsp::IProcessable,
      public utils::serialization::ISerializable<Track>,
      public IPortOwner,
      public utils::UuidIdentifiableObject<Track>
{
  Q_GADGET
  QML_ELEMENT

public:
  using PortType = dsp::PortType;
  using PluginRegistry = gui::old_dsp::plugins::PluginRegistry;
  using PluginPtrVariant = PluginRegistry::VariantType;
  using PluginSlot = dsp::PluginSlot;

  /**
   * The Track's type.
   */
  enum class Type
  {
    /**
     * Instrument tracks must have an Instrument plugin at the first slot and
     * they produce audio output.
     */
    Instrument,

    /**
     * Audio tracks can record and contain audio clips. Other than that their
     * channel strips are similar to buses.
     */
    Audio,

    /**
     * The master track is a special type of group track.
     */
    Master,

    /**
     * The chord track contains chords that can be used to modify midi in real
     * time or to color the piano roll.
     */
    Chord,

    /**
     * Marker Track's contain named markers at specific Position's in the song.
     */
    Marker,

    /**
     * Special track for BPM (tempo) and time signature events.
     */
    Tempo,

    /**
     * Special track to contain global Modulator's.
     */
    Modulator,

    /**
     * Buses are channels that receive audio input and have effects on their
     * channel strip. They are similar to Group Tracks, except that they
     * cannot be routed to directly. Buses are used for send effects.
     */
    AudioBus,

    /**
     * Group Tracks are used for grouping audio signals, for example routing
     * multiple drum tracks to a "Drums" group track. Like buses, they only
     * contain effects but unlike buses they can be routed to.
     */
    AudioGroup,

    /**
     * Midi tracks can only have MIDI effects in the strip and produce MIDI
     * output that can be routed to instrument channels or hardware.
     */
    Midi,

    /** Same with audio bus but for MIDI signals. */
    MidiBus,

    /** Same with audio group but for MIDI signals. */
    MidiGroup,

    /** Foldable track used for visual grouping. */
    Folder,
  };
  Q_ENUM (Type)

  using Color = zrythm::utils::Color;
  using Position = zrythm::dsp::Position;

  /**
   * Returns the prefader type.
   */
  static Fader::Type type_get_prefader_type (const Type type)
  {
    switch (type)
      {
      case Type::Midi:
      case Type::MidiBus:
      case Type::Chord:
      case Type::MidiGroup:
        return Fader::Type::MidiChannel;
      case Type::Instrument:
      case Type::Audio:
      case Type::AudioBus:
      case Type::Master:
      case Type::AudioGroup:
        return Fader::Type::AudioChannel;
      case Type::Marker:
      case Type::Folder:
        return Fader::Type::None;
      default:
        z_return_val_if_reached (Fader::Type::None);
      }
  }

  static constexpr bool type_has_processor (const Type type)
  {
    return type != Type::Tempo && type != Type::Marker;
  }

  static bool type_has_lanes (const Type type)
  {
    return type == Type::Audio || type == Type::Instrument || type == Type::Midi;
  }

  /**
   * Returns if the given Type is a type of Track that has a Channel.
   */
  static bool type_has_channel (Type type)
  {
    switch (type)
      {
      case Type::Marker:
      case Type::Tempo:
      case Type::Modulator:
      case Type::Folder:
        return false;
      default:
        break;
      }

    return true;
  }

  static constexpr bool type_can_have_direct_out (Type type)
  {
    return type != Type::Master;
  }

  static constexpr bool
  type_can_have_region_type (Type type, RegionType region_type)
  {
    switch (region_type)
      {
      case RegionType::Audio:
        return type == Type::Audio;
      case RegionType::Midi:
        return type == Type::Midi || type == Type::Instrument;
      case RegionType::Chord:
        return type == Type::Chord;
      case RegionType::Automation:
        return true;
      }

    z_return_val_if_reached (false);
  }

  static constexpr bool type_is_foldable (Type type)
  {
    return type == Type::Folder || type == Type::MidiGroup
           || type == Type::AudioGroup;
  }

  static constexpr bool type_is_copyable (Type type)
  {
    return type != Type::Master && type != Type::Tempo && type != Type::Chord
           && type != Type::Modulator && type != Type::Marker;
  }

  /**
   * Returns whether a track of the given type should be deletable by the user.
   */
  static constexpr bool type_is_deletable (Type type)
  {
    return type_is_copyable (type);
  }

  static Type type_get_from_plugin_descriptor (
    const zrythm::gui::old_dsp::plugins::PluginDescriptor &descr);

  /**
   * Returns if the given Type can host the given RegionType.
   */
  static bool type_can_host_region_type (const Type tt, const RegionType rt);

  static bool type_has_mono_compat_switch (const Type tt)
  {
    return tt == Type::AudioGroup || tt == Type::Master;
  }

#define type_is_audio_group type_has_mono_compat_switch

  static bool type_is_fx (const Type type)
  {
    return type == Type::AudioBus || type == Type::MidiBus;
  }

  /**
   * Returns if the Track can record.
   */
  static int type_can_record (const Type type)
  {
    return type == Type::Audio || type == Type::Midi || type == Type::Chord
           || type == Type::Instrument;
  }

  static bool type_has_automation (const Type type)
  {
    return type != Type::Marker && type != Type::Folder;
  }

  /**
   * Returns if regions in tracks from @p type1 can be moved to @p type2.
   */
  static constexpr bool
  type_is_compatible_for_moving (const Type type1, const Type type2)
  {
    return type1 == type2 || (type1 == Type::Midi && type2 == Type::Instrument)
           || (type1 == Type::Instrument && type2 == Type::Midi);
  }

  /**
   * Returns if the Track should have a piano roll.
   */
  ATTR_CONST
  static constexpr bool type_has_piano_roll (const Type type)
  {
    return type == Type::Midi || type == Type::Instrument;
  }

  /**
   * Returns if the Track should have an inputs selector.
   */
  static constexpr bool type_has_inputs (const Type type)
  {
    return type == Type::Midi || type == Type::Instrument || type == Type::Audio;
  }

  /**
   * @brief Returns if the Track can be a direct route target.
   *
   * @param type
   * @return true
   * @return false
   */
  static bool type_can_be_group_target (const Type type)
  {
    return type == Type::AudioGroup || type == Type::MidiGroup
           || type == Type::Instrument || type == Type::Master;
  }

  template <typename T> static consteval Type get_type_for_class ()
  {
    if constexpr (std::is_same_v<T, MidiTrack>)
      return Type::Midi;
    else if constexpr (std::is_same_v<T, AudioTrack>)
      return Type::Audio;
    else if constexpr (std::is_same_v<T, ChordTrack>)
      return Type::Chord;
    else if constexpr (std::is_same_v<T, InstrumentTrack>)
      return Type::Instrument;
    else if constexpr (std::is_same_v<T, AudioBusTrack>)
      return Type::AudioBus;
    else if constexpr (std::is_same_v<T, MidiBusTrack>)
      return Type::MidiBus;
    else if constexpr (std::is_same_v<T, MasterTrack>)
      return Type::Master;
    else if constexpr (std::is_same_v<T, TempoTrack>)
      return Type::Tempo;
    else if constexpr (std::is_same_v<T, ModulatorTrack>)
      return Type::Modulator;
    else if constexpr (std::is_same_v<T, MarkerTrack>)
      return Type::Marker;
    else if constexpr (std::is_same_v<T, FolderTrack>)
      return Type::Folder;
    else if constexpr (std::is_same_v<T, AudioGroupTrack>)
      return Type::AudioGroup;
    else if constexpr (std::is_same_v<T, MidiGroupTrack>)
      return Type::MidiGroup;
    else
      {
        static_assert (dependent_false_v<T>, "Unknown track type");
      }
  }

public:
  ~Track () override = default;
  Q_DISABLE_COPY_MOVE (Track)

  [[nodiscard]] static TrackUniquePtrVariant
  create_unique_from_type (Type type);

protected:
  /**
   * Constructor to be used by subclasses.
   *
   * @param pos Position in the Tracklist.
   */
  Track (Type type, PortType in_signal_type, PortType out_signal_type);

public:
  /**
   * @brief Adds additional metadata to track members after deserialization.
   *
   * @note Each implementor must chain up to its direct superclass.
   */
  virtual void
  init_loaded (PluginRegistry &plugin_registry, PortRegistry &port_registry) = 0;

  Tracklist * get_tracklist () const;

  PortConnectionsManager * get_port_connections_manager () const;

  bool has_channel () const { return type_has_channel (type_); }

  bool has_piano_roll () const { return type_has_piano_roll (type_); }

  bool can_record () const { return type_can_record (type_); }

  /**
   * Returns whether the track should be visible.
   *
   * Takes into account Track.visible and whether any of the track's foldable
   * parents are folded.
   */
  bool should_be_visible () const;

  bool is_foldable () const { return type_is_foldable (type_); }

  bool is_automatable () const { return type_has_automation (type_); }

  bool is_tempo () const { return type_ == Type::Tempo; }
  bool is_folder () const { return type_ == Type::Folder; }
  bool is_audio_group () const { return type_ == Type::AudioGroup; }
  bool is_midi_group () const { return type_ == Type::MidiGroup; }
  bool is_audio_bus () const { return type_ == Type::AudioBus; }
  bool is_midi_bus () const { return type_ == Type::MidiBus; }
  bool is_modulator () const { return type_ == Type::Modulator; }
  bool is_chord () const { return type_ == Type::Chord; }
  bool is_marker () const { return type_ == Type::Marker; }
  bool is_audio () const { return type_ == Type::Audio; }
  bool is_instrument () const { return type_ == Type::Instrument; }
  bool is_midi () const { return type_ == Type::Midi; }
  bool is_master () const { return type_ == Type::Master; }

  static Track * from_variant (const TrackPtrVariant &variant);

  bool has_lanes () const { return type_has_lanes (type_); }

  bool is_deletable () const { return type_is_deletable (type_); }
  bool is_copyable () const { return type_is_copyable (type_); }
  bool has_automation () const { return type_has_automation (type_); }

  /**
   * Returns the full visible height (main height + height of all visible
   * automation tracks + height of all visible lanes).
   */
  template <typename DerivedT>
  double get_full_visible_height (this DerivedT &&self)
    requires std::derived_from<base_type<DerivedT>, Track>
             && FinalClass<base_type<DerivedT>>
  {
    double height = self.main_height_;

    if constexpr (std::derived_from<DerivedT, LanedTrack>)
      {
        height += self.get_visible_lane_heights ();
      }
    if constexpr (std::derived_from<DerivedT, AutomatableTrack>)
      {
        if (self.automation_visible_)
          {
            const AutomationTracklist &atl = self.get_automation_tracklist ();
            for (const auto &at : atl.visible_ats_)
              {
                z_warn_if_fail (at->height_ > 0);
                if (at->visible_)
                  height += at->height_;
              }
          }
      }
    return height;
  }

  template <typename DerivedT>
  bool multiply_heights (
    this DerivedT &&self,
    double          multiplier,
    bool            visible_only,
    bool            check_only)
    requires std::derived_from<base_type<DerivedT>, Track>
             && FinalClass<base_type<DerivedT>>
  {
    if (self.main_height_ * multiplier < TRACK_MIN_HEIGHT)
      return false;

    if (!check_only)
      {
        self.main_height_ *= multiplier;
      }

    if constexpr (std::derived_from<DerivedT, LanedTrack>)
      {
        if (!visible_only || self.lanes_visible_)
          {
            for (auto &lane_var : self.lanes_)
              {
                using TrackLaneT = DerivedT::TrackLaneType;
                auto lane = std::get<TrackLaneT *> (lane_var);
                if (lane->height_ * multiplier < TRACK_MIN_HEIGHT)
                  {
                    return false;
                  }

                if (!check_only)
                  {
                    lane->height_ *= multiplier;
                  }
              }
          }
      }
    if constexpr (std::derived_from<DerivedT, AutomatableTrack>)
      {
        if (!visible_only || self.automation_visible_)
          {
            auto &atl = self.get_automation_tracklist ();
            for (auto &at : atl.ats_)
              {
                if (visible_only && !at->visible_)
                  continue;

                if (at->height_ * multiplier < TRACK_MIN_HEIGHT)
                  {
                    return false;
                  }

                if (!check_only)
                  {
                    at->height_ *= multiplier;
                  }
              }
          }
      }

    return true;
  }

  /** Whether this track is part of the SampleProcessor auditioner tracklist. */
  bool is_auditioner () const;

  /**
   * Returns if the track is currently selected in the tracklist.
   */
  bool is_selected () const { return selected_; }

  bool can_be_group_target () const { return type_can_be_group_target (type_); }

  /**
   * Inserts a Region to the given lane or AutomationTrack of the track, at
   * the given index.
   *
   * The Region must be the main region (see ArrangerObjectInfo).
   *
   * @param at The AutomationTrack of this Region, if automation region.
   * @param lane_pos The position of the lane to add to, if applicable.
   * @param idx The index to insert the region at inside its parent, or -1 to
   * append.
   * @param gen_name Generate a unique region name or not. This will be 0 if
   * the caller already generated a unique name.
   *
   * @throw ZrythmException if the insertion fails.
   */
  template <FinalRegionSubclass T>
  T * insert_region (
    T *               region,
    AutomationTrack * at,
    int               lane_pos,
    int               idx,
    bool              gen_name,
    bool              fire_events);

  /**
   * Appends a Region to the given lane or AutomationTrack of the track.
   *
   * @see insert_region().
   *
   * @throw ZrythmException if the insertion fails.
   */
  template <FinalRegionSubclass T>
  T * add_region (
    T *               region,
    AutomationTrack * at,
    int               lane_pos,
    bool              gen_name,
    bool              fire_events)
  {
    return insert_region<T> (region, at, lane_pos, -1, gen_name, fire_events);
  }

  /**
   * @brief Appends all the objects in the track to @p objects.
   *
   * This only appends top-level objects. For example, region children will
   * not be added.
   *
   * @param objects
   */
  void append_objects (std::vector<ArrangerObjectPtrVariant> &objects) const;

  /**
   * Unselects all arranger objects in the track.
   */
  void unselect_all ();

  template <typename DerivedT>
  bool contains_uninstantiated_plugin (this DerivedT &&self)
    requires std::derived_from<base_type<DerivedT>, Track>
             && FinalClass<base_type<DerivedT>>
  {
    std::vector<zrythm::gui::old_dsp::plugins::Plugin *> plugins;
    self.get_plugins (plugins);
    return std::ranges::any_of (plugins, [] (auto pl) {
      return pl->instantiation_failed_;
    });
  }

  std::string get_node_name () const override { return get_name (); }

  /**
   * Removes all objects recursively from the track.
   */
  virtual void clear_objects () {};

  /**
   * Verifies the identifiers on a live Track (in the project, not a clone).
   *
   * @return True if pass.
   */
  virtual bool validate () const = 0;

  /**
   * Adds the track's folder parents to the given vector.
   *
   * @param prepend Whether to prepend instead of append.
   */
  void
  add_folder_parents (std::vector<FoldableTrack *> &parents, bool prepend) const;

  /**
   * Returns the closest foldable parent or NULL.
   */
  FoldableTrack * get_direct_folder_parent () const
  {
    std::vector<FoldableTrack *> parents;
    add_folder_parents (parents, true);
    if (!parents.empty ())
      {
        return parents.front ();
      }
    return nullptr;
  }

  /**
   * Remove the track from all folders.
   *
   * Used when deleting tracks.
   */
  void remove_from_folder_parents ();

  /**
   * Getter for the track name.
   */
  std::string get_name () const { return name_; };

  /**
   * Internally called by set_name_with_action().
   */
  bool set_name_with_action_full (const std::string &name);

  /**
   * Setter to be used by the UI to create an undoable action.
   */
  void set_name_with_action (const std::string &name);

  /**
   * Setter for the track name.
   *
   * If a track with that name already exists, it adds a number at the end.
   *
   * Must only be called from the GTK thread.
   */
  void
  set_name (const Tracklist &tracklist, const std::string &name, bool pub_events);

  /**
   * Returns a unique name for a new track based on the given name.
   */
  std::string
  get_unique_name (const Tracklist &tracklist, const std::string &name);

  /**
   * Updates the frames/ticks of each position in each child of the track
   * recursively.
   *
   * @param from_ticks Whether to update the positions based on ticks (true)
   * or frames (false).
   * @param frames_per_tick This will be used when doing position conversions
   via
   * dependency injection instead of relying on the current project's transport.
   */
  void
  update_positions (bool from_ticks, bool bpm_change, double frames_per_tick);

  /**
   * Returns all the regions inside the given range, or all the regions if both
   * @ref p1 and @ref p2 are NULL.
   *
   * @return The number of regions returned.
   */
  virtual void get_regions_in_range (
    std::vector<Region *> &regions,
    const dsp::Position *  p1,
    const dsp::Position *  p2)
  {
  }

  /**
   * Fills in the given array with all plugins in the track.
   */
  template <typename DerivedT>
  void get_plugins (
    this DerivedT                                       &&self,
    std::vector<zrythm::gui::old_dsp::plugins::Plugin *> &arr)
    requires std::derived_from<base_type<DerivedT>, Track>
             && FinalClass<base_type<DerivedT>>
  {
    if constexpr (std::derived_from<DerivedT, ChannelTrack>)
      {
        self.channel_->get_plugins (arr);
      }

    if constexpr (std::is_same_v<DerivedT, ModulatorTrack>)
      {
        for (const auto &modulator : self.modulators_)
          {
            if (modulator)
              {
                arr.push_back (modulator.get ());
              }
          }
      }
  }

  /**
   * Activate or deactivate all plugins.
   *
   * This is useful for exporting: deactivating and reactivating a plugin will
   * reset its state.
   */
  template <typename DerivedT>
  void activate_all_plugins (this DerivedT &&self, bool activate)
    requires std::derived_from<base_type<DerivedT>, Track>
             && FinalClass<base_type<DerivedT>>
  {
    std::vector<zrythm::gui::old_dsp::plugins::Plugin *> pls;
    self.get_plugins (pls);

    for (auto &pl : pls)
      {
        if (!pl->instantiated_ && !pl->instantiation_failed_)
          {
            try
              {
                pl->instantiate ();
              }
            catch (const ZrythmException &e)
              {
                e.handle ("Failed to instantiate plugin");
              }
          }

        if (pl->instantiated_)
          {
            pl->activate (activate);
          }
      }
  }

  std::string get_comment () const { return comment_; }

  /**
   * @param undoable Create an undable action.
   */
  void set_comment (const std::string &comment, bool undoable);

  void set_comment_with_action (const std::string &comment)
  {
    set_comment (comment, true);
  }

  /**
   * Sets the track color.
   */
  void set_color (const Color &color, bool undoable, bool fire_events);

  /**
   * Sets the track icon.
   */
  void set_icon (const std::string &icon_name, bool undoable, bool fire_events);

  /**
   * Marks the track for bouncing.
   *
   * @param mark_children Whether to mark all children tracks as well. Used
   * when exporting stems on the specific track stem only. IMPORTANT:
   * Track.bounce_to_master must be set beforehand if this is true.
   * @param mark_parents Whether to mark all parent tracks as well.
   */
  void mark_for_bounce (
    bool bounce,
    bool mark_regions,
    bool mark_children,
    bool mark_parents);

  /**
   * Appends all channel ports and optionally plugin ports to the array.
   */
  virtual void
  append_ports (std::vector<Port *> &ports, bool include_plugins) const = 0;

  /**
   * Freezes or unfreezes the track.
   *
   * When a track is frozen, it is bounced with effects to a temporary file in
   * the pool, which is played back directly from disk.
   *
   * When the track is unfrozen, this file will be removed from the pool and
   * the track will be played normally again.
   *
   * @remark Unimplemented/not used.
   *
   * @throw ZrythmException on error.
   */
  void track_freeze (bool freeze);

  /**
   * Wrapper over Channel.add_plugin() and ModulatorTrack.insert_modulator().
   *
   * @param instantiate_plugin Whether to attempt to instantiate the plugin.
   */
  template <typename DerivedT>
  PluginPtrVariant insert_plugin (
    this DerivedT &&self,
    PluginUuid      plugin_id,
    dsp::PluginSlot slot,
    bool            instantiate_plugin,
    bool            replacing_plugin,
    bool            moving_plugin,
    bool            confirm,
    bool            gen_automatables,
    bool            recalc_graph,
    bool            fire_events)
    requires std::derived_from<base_type<DerivedT>, Track>
             && FinalClass<base_type<DerivedT>>
  {
    if (!slot.validate_slot_type_slot_combo ())
      {
        throw std::runtime_error ("Invalid slot type and slot combo");
      }

    PluginPtrVariant plugin_var{};

    if (slot.is_modulator ())
      {
        if constexpr (std::is_same_v<DerivedT, ModulatorTrack>)
          {
            plugin_var = self.insert_modulator (
              slot.get_slot_with_index ().second, plugin_id, replacing_plugin,
              confirm, gen_automatables, recalc_graph, fire_events);
          }
      }
    else
      {
        if constexpr (std::derived_from<DerivedT, ChannelTrack>)
          {
            plugin_var = self.get_channel ()->add_plugin (
              plugin_id, slot, confirm, moving_plugin, gen_automatables,
              recalc_graph, fire_events);
          }
      }

    std::visit (
      [&] (auto &&plugin) {
        if (plugin && !plugin->instantiated_ && !plugin->instantiation_failed_)
          {
            try
              {
                plugin->instantiate ();
              }
            catch (const ZrythmException &e)
              {
                e.handle ("Failed to instantiate plugin");
              }
          }
      },
      plugin_var);

    return plugin_var;
  }

  /**
   * Wrapper over channel_remove_plugin() and
   * modulator_track_remove_modulator().
   */
  void remove_plugin (
    dsp::PluginSlot slot,
    bool            replacing_plugin,
    bool            moving_plugin,
    bool            deleting_plugin,
    bool            deleting_track,
    bool            recalc_graph);

  /**
   * Disconnects the track from the processing chain.
   *
   * This should be called immediately when the track is getting deleted,
   * and track_free should be designed to be called later after an arbitrary
   * delay.
   *
   * @param remove_pl Remove the zrythm::gui::old_dsp::plugins::Plugin from
   * the Channel. Useful when deleting the channel.
   * @param recalc_graph Recalculate mixer graph.
   */
  void disconnect (bool remove_pl, bool recalc_graph);

  bool is_enabled () const { return enabled_; }
  bool get_enabled () const { return enabled_; }
  bool get_disabled () const { return !enabled_; }

  void set_enabled (
    bool enabled,
    bool trigger_undo,
    bool auto_select,
    bool fire_events);

  /** TODO: document why it's a pointer. */
  int get_total_bars (const Transport &transport, int total_bars) const;

  /**
   * Set various caches (snapshots, track name hash, plugin input/output
   * ports, etc).
   */
  void set_caches (CacheType types);

  /**
   * @brief Creates a new track with the given parameters.
   *
   * @param disable_track_idx Track index to disable, or -1.
   * @param ready_cb Callback to be called when the tracks are ready (added
   * to the project).
   *
   * @throw ZrythmException on error.
   */
  static void create_with_action (
    Type                   type,
    const PluginSetting *  pl_setting,
    const FileDescriptor * file_descr,
    const dsp::Position *  pos,
    int                    index,
    int                    num_tracks,
    int                    disable_track_idx,
    TracksReadyCallback    ready_cb);

  /**
   * @brief Creates a new empty track at the given index.
   *
   * @param type
   * @param index
   * @param error
   * @return Track*
   * @throw ZrythmException on error.
   */
  static Track * create_empty_at_idx_with_action (Type type, int index);

  /**
   * @brief Creates a new track for the given plugin at the given index.
   *
   * @param type
   * @param pl_setting
   * @param index
   * @param error
   * @return Track*
   * @throw ZrythmException on error.
   */
  static Track * create_for_plugin_at_idx_w_action (
    Type                  type,
    const PluginSetting * pl_setting,
    int                   index);

  template <typename T = Track>
  static T *
  create_for_plugin_at_idx_w_action (const PluginSetting * pl_setting, int index)
  {
    return dynamic_cast<T *> (create_for_plugin_at_idx_w_action (
      get_type_for_class<T> (), pl_setting, index));
  }

  /**
   * @brief Creates a new empty track at the end of the tracklist.
   *
   * @param type
   * @return Track*
   * @throw ZrythmException on error.
   */
  static Track * create_empty_with_action (Type type);

  template <typename T = Track> static T * create_empty_with_action ()
  {
    return dynamic_cast<T *> (
      create_empty_with_action (get_type_for_class<T> ()));
  }

  /**
   * @brief Create a track of the given type with the given name and position.
   *
   * @note Only works for non-singleton tracks.
   * @param name
   * @param pos
   */
  [[nodiscard]] static TrackUniquePtrVariant
  create_track (Type type, const std::string &name, int pos);

  // GMenu * generate_edit_context_menu (int num_selected);

  bool is_in_active_project () const override;

  void set_port_metadata_from_owner (dsp::PortIdentifier &id, PortRange &range)
    const override;

  std::string
  get_full_designation_for_port (const dsp::PortIdentifier &id) const override;

  virtual bool get_muted () const { return false; }

  virtual bool get_listened () const { return false; }

  /**
   * Returns whether the track is not soloed on its own but its direct out (or
   * its direct out's direct out, etc.) is soloed.
   */
  virtual bool get_implied_soloed () const { return false; }

  virtual bool get_soloed () const { return false; }

protected:
  void copy_members_from (const Track &other, ObjectCloneType clone_type);

  bool validate_base () const;

  DECLARE_DEFINE_BASE_FIELDS_METHOD ();

  /**
   * @brief Set the playback caches for a track.
   *
   * This is called by @ref set_caches().
   */
  virtual void set_playback_caches () { }

  void add_region_if_in_range (
    const dsp::Position *  p1,
    const dsp::Position *  p2,
    std::vector<Region *> &regions,
    Region *               region);

private:
  /**
   * @brief Create a new track.
   *
   * @param type
   * @param pl_setting
   * @param index
   * @return void*
   * @throw ZrythmException on error.
   */
  static Track * create_without_file_with_action (
    Type                  type,
    const PluginSetting * pl_setting,
    int                   index);

public:
  /**
   * Position in the Tracklist.
   *
   * This is also used in the Mixer for the Channels.
   * If a track doesn't have a Channel, the Mixer can just skip.
   */
  int pos_ = 0;

  /** The type of track this is. */
  Type type_ = {};

  /** Track name, used in channel too. */
  std::string name_;

  /** Icon name of the track. */
  std::string icon_name_;

  /**
   * Track Widget created dynamically.
   *
   * 1 track has 1 widget.
   */
  // TrackWidget * widget_ = nullptr;

  /** Whole Track is visible or not. */
  bool visible_ = true;

  /** Track will be hidden if true (temporary and not serializable). */
  bool filtered_ = false;

  /** Height of the main part (without lanes). */
  double main_height_ = TRACK_DEF_HEIGHT;

  /**
   * Active (enabled) or not.
   *
   * Disabled tracks should be ignored in routing.
   * Similar to Plugin.enabled (bypass).
   */
  bool enabled_ = true;

  /**
   * Track color.
   *
   * This is used in the channels as well.
   */
  Color color_;

  /**
   * Flag to tell the UI that this channel had MIDI activity.
   *
   * When processing this and setting it to 0, the UI should create a separate
   * event using EVENTS_PUSH.
   */
  bool trigger_midi_activity_ = false;

  /**
   * The input signal type (eg audio bus tracks have audio input signals).
   */
  PortType in_signal_type_ = {};

  /**
   * The output signal type (eg midi tracks have MIDI output signals).
   */
  PortType out_signal_type_ = {};

  /** User comments. */
  std::string comment_;

  /**
   * Set to ON during bouncing if this track should be included.
   *
   * Only relevant for tracks that output audio.
   */
  bool bounce_{};

  /**
   * @brief Whether the track is selected.
   *
   * Selection is tracked directly on Track instances to make life easier when
   * working with QML and to avoid having extra classes to track selection.
   */
  bool selected_{};

  /**
   * Whether to temporarily route the output to master (e.g., when bouncing
   * the track on its own without its parents).
   */
  bool bounce_to_master_ = false;

  /** Whether the track is currently frozen. */
  bool frozen_ = false;

  /** Pool ID of the clip if track is frozen. */
  int pool_id_ = 0;

  int magic_ = TRACK_MAGIC;

  /** Whether currently disconnecting. */
  bool disconnecting_ = false;

  /** Pointer to owner tracklist, if any. */
  Tracklist * tracklist_ = nullptr;
};

#if 0
template <typename TrackT> class TrackImpl : virtual public Track
{
};
#endif

inline bool
operator< (const Track &lhs, const Track &rhs)
{
  return lhs.pos_ < rhs.pos_;
}

DEFINE_ENUM_FORMATTER (
  Track::Type,
  Track_Type,
  QT_TR_NOOP_UTF8 ("Instrument"),
  QT_TR_NOOP_UTF8 ("Audio"),
  QT_TR_NOOP_UTF8 ("Master"),
  QT_TR_NOOP_UTF8 ("Chord"),
  QT_TR_NOOP_UTF8 ("Marker"),
  QT_TR_NOOP_UTF8 ("Tempo"),
  QT_TR_NOOP_UTF8 ("Modulator"),
  QT_TR_NOOP_UTF8 ("Audio FX"),
  QT_TR_NOOP_UTF8 ("Audio Group"),
  QT_TR_NOOP_UTF8 ("MIDI"),
  QT_TR_NOOP_UTF8 ("MIDI FX"),
  QT_TR_NOOP_UTF8 ("MIDI Group"),
  QT_TR_NOOP_UTF8 ("Folder"));

template <typename T>
concept TrackSubclass = std::derived_from<T, Track>;

template <typename TrackT>
concept FinalTrackSubclass = TrackSubclass<TrackT> && FinalClass<TrackT>;

using TrackRegistry = utils::OwningObjectRegistry<TrackPtrVariant, Track>;
using TrackRegistryRef = std::reference_wrapper<TrackRegistry>;

class RecordableTrack;

extern template MidiRegion *
Track::add_region (
  MidiRegion *      region,
  AutomationTrack * at,
  int               lane_pos,
  bool              gen_name,
  bool              fire_events);

extern template AudioRegion *
Track::add_region (
  AudioRegion *     region,
  AutomationTrack * at,
  int               lane_pos,
  bool              gen_name,
  bool              fire_events);

extern template ChordRegion *
Track::add_region (
  ChordRegion *     region,
  AutomationTrack * at,
  int               lane_pos,
  bool              gen_name,
  bool              fire_events);

extern template AutomationRegion *
Track::add_region (
  AutomationRegion * region,
  AutomationTrack *  at,
  int                lane_pos,
  bool               gen_name,
  bool               fire_events);

/**
 * @}
 */

#endif // __AUDIO_TRACK_H__
