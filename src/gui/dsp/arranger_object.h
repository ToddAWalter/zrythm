// SPDX-FileCopyrightText: © 2019-2022, 2024-2025 Alexandros Theodotou <alex@zrythm.org>
// SPDX-License-Identifier: LicenseRef-ZrythmLicense

#ifndef __GUI_BACKEND_ARRANGER_OBJECT_H__
#define __GUI_BACKEND_ARRANGER_OBJECT_H__

#include "gui/backend/position_proxy.h"
#include "gui/dsp/arranger_object_fwd.h"
#include "gui/dsp/track_fwd.h"

#include <QtGlobal>

using namespace zrythm;

#define DEFINE_ARRANGER_OBJECT_QML_PROPERTIES(ClassType) \
public: \
  /* ================================================================ */ \
  /* type */ \
  /* ================================================================ */ \
  Q_PROPERTY (int type READ getType CONSTANT) \
  int getType () const \
  { \
    return ENUM_VALUE_TO_INT (type_); \
  } \
  /* ================================================================ */ \
  /* hasLength */ \
  /* ================================================================ */ \
  Q_PROPERTY (bool hasLength READ getHasLength CONSTANT) \
  bool getHasLength () const \
  { \
    return std::derived_from<ClassType, BoundedObject>; \
  } \
  /* ================================================================ */ \
  /* selected (changes to be emitted by each ArrangerSelections) */ \
  /* ================================================================ */ \
  Q_PROPERTY (bool selected READ getSelected NOTIFY selectedChanged) \
  bool getSelected () const \
  { \
    return is_selected (); \
  } \
  void setSelected (bool selected) \
  { \
    if (selected == getSelected ()) \
      { \
        return; \
      } \
\
    selected_ = selected; \
\
    Q_EMIT selectedChanged (selected); \
  } \
  Q_SIGNAL void selectedChanged (bool selected); \
  /* ================================================================ */ \
  /* position */ \
  /* ================================================================ */ \
  Q_PROPERTY (PositionProxy * position READ getPosition CONSTANT) \
  PositionProxy * getPosition () const \
  { \
    return pos_; \
  }

/**
 * @brief Base class for all objects in the arranger.
 *
 * The ArrangerObject class is the base class for all objects that can be
 * placed in the arranger, such as regions, MIDI notes, chord objects, etc.
 * It provides common functionality and properties shared by all these
 * objects.
 *
 * Arranger objects should be managed as shared pointers because their ownership
 * is shared by both their parents and the project selections. Logically, only
 * the ArrangerObject parent actually owns the object, but if we use unique_ptr
 * we would have to use more complex strategies like observer pattern so we
 * avoid that by using shared_ptr.
 *
 * We also need shared_from_this() in various cases (TODO explain).
 */
class ArrangerObject
    : public zrythm::utils::serialization::ISerializable<ArrangerObject>,
      public utils::UuidIdentifiableObject<ArrangerObject>
{
  Q_DISABLE_COPY_MOVE (ArrangerObject)

public:
  using TrackUuid = TrackUuid;

  static constexpr double DEFAULT_NUDGE_TICKS = 0.1;

  /**
   * Flag used in some functions.
   */
  enum class ResizeType
  {
    Normal,
    Loop,
    Fade,
    Stretch,

    /**
     * Used when we want to resize to contents when BPM changes.
     *
     * @note Only applies to audio.
     */
    StretchTempoChange,
  };

  /**
   * The type of the object.
   */
  enum class Type
  {
    MidiRegion,
    AudioRegion,
    ChordRegion,
    AutomationRegion,
    MidiNote,
    ChordObject,
    ScaleObject,
    Marker,
    AutomationPoint,
  };

  /**
   * Flags.
   */
  enum class Flags
  {
    /** This object is not a project object, but an object used temporarily eg.
     * when undoing/ redoing. */
    NonProject = 1 << 0,

    /** The object is a snapshot (cache used during DSP). TODO */
    // FLAG_SNAPSHOT = 1 << 1,
  };

  enum class PositionType
  {
    Start,
    End,
    ClipStart,
    LoopStart,
    LoopEnd,
    FadeIn,
    FadeOut,
  };

  using Position = zrythm::dsp::Position;

public:
  ArrangerObject (Type type);

  ~ArrangerObject () override = default;

  using ArrangerObjectPtr = ArrangerObject *;

  /**
   * @brief Generate @ref transient_.
   */
  void generate_transient ();

  /**
   * @brief Returns whether the object is hovered in the corresponding arranger.
   *
   * @return true
   * @return false
   */
  bool is_hovered () const;

  /**
   * Generates a human readable name for the object.
   *
   * If the object has a name, this returns a copy of the name, otherwise
   * generates something appropriate.
   */
  virtual std::string gen_human_friendly_name () const
  {
    /* this will be called if unimplemented - it's not needed for things like
     * Velocity, which don't have reasonable names. */
    z_return_val_if_reached ("");
  };

  /**
   * Initializes the object after loading a Project.
   */
  virtual void init_loaded () = 0;

  /**
   * Returns whether the given object is hit by the given  range.
   *
   * @param start Start position.
   * @param end End position.
   * @param range_start_inclusive Whether @ref pos_ == @p start is allowed. Can't
   * imagine a case where this would be false, but kept an option just in case.
   * @param range_end_inclusive Whether @ref pos_ == @p end is allowed.
   */
  bool is_start_hit_by_range (
    const dsp::Position &start,
    const dsp::Position &end,
    bool                 range_start_inclusive = true,
    bool                 range_end_inclusive = false) const
  {
    return is_start_hit_by_range (
      start.frames_, end.frames_, range_start_inclusive, range_end_inclusive);
  }

  /**
   * @brief @see @ref is_start_hit_by_range().
   */
  bool is_start_hit_by_range (
    const signed_frame_t global_frames_start,
    const signed_frame_t global_frames_end,
    bool                 range_start_inclusive = true,
    bool                 range_end_inclusive = false) const
  {
    return (range_start_inclusive
              ? (pos_->frames_ >= global_frames_start)
              : (pos_->frames_ > global_frames_start))
           && (range_end_inclusive ? (pos_->frames_ <= global_frames_end) : (pos_->frames_ < global_frames_end));
  }

  void parent_base_qproperties (QObject &derived);

  /**
   * Returns if the object is in the selections.
   */
  bool is_selected () const { return selected_; }

  /**
   * @brief Prints the given object to a string.
   */
  virtual std::string print_to_str () const = 0;

  /**
   * @brief Prints debug information about the given object.
   *
   * Uses @ref print_to_str() internally.
   */
  void print () const;

  /**
   * Getter.
   */
  void get_pos (dsp::Position * pos) const
  {
    *pos = *static_cast<dsp::Position *> (pos_);
  };

  void get_position_from_type (dsp::Position * pos, PositionType type) const;

  template <typename T = ArrangerObject> T * get_transient () const
  {
    return dynamic_cast<T *> (transient_);
  };

  /**
   * Callback when beginning to edit the object.
   *
   * This saves a clone of its current state to its arranger.
   */
  void edit_begin () const;

  /**
   * Callback when finishing editing the object.
   *
   * This performs an undoable action.
   *
   * @param action_edit_type A @ref ArrangerSelectionsAction::EditType.
   */
  void edit_finish (int action_edit_type) const;

  void edit_position_finish () const;

  /**
   * The setter is for use in e.g. the digital meters whereas the set_pos func
   * is used during arranger actions.
   *
   * @note This validates the position.
   */
  void pos_setter (const dsp::Position &pos);

  /**
   * Returns if the given Position is valid.
   *
   * @param pos The position to set to.
   * @param pos_type The type of Position to set in the ArrangerObject.
   */
  [[nodiscard, gnu::hot]] bool
  is_position_valid (const dsp::Position &pos, PositionType pos_type) const;

  /**
   * Sets the given position on the object, optionally attempting to validate
   * before.
   *
   * @param pos The position to set to.
   * @param pos_type The type of Position to set in the ArrangerObject.
   * @param validate Validate the Position before setting it.
   *
   * @return Whether the position was set (false if invalid).
   */
  bool
  set_position (const dsp::Position &pos, PositionType pos_type, bool validate);

  /**
   * Moves the object by the given amount of ticks.
   */
  void move (double ticks);

  void set_track_id (TrackUuid track_id) { track_id_ = track_id; }

  /**
   * Updates the positions in each child recursively.
   *
   * @param from_ticks Whether to update the positions based on ticks (true) or
   * frames (false).
   * @param frames_per_tick This will be used when doing position conversions via
   * dependency injection instead of relying on the current project's transport.
   */
  void
  update_positions (bool from_ticks, bool bpm_change, double frames_per_tick);

  /**
   * Returns the Track this ArrangerObject is in.
   */
  [[gnu::hot]] virtual TrackPtrVariant get_track () const;

  TrackUuid get_track_id () const { return track_id_; }

  /**
   * @brief Performs some post-deserialization logic, like adjusting positions
   * to BPM changes.
   */
  void post_deserialize ();

  /**
   * Validates the arranger object.
   *
   * @param frames_per_tick Frames per tick used when validating audio
   * regions. Passing 0 will use the value from the current engine.
   * @return Whether valid.
   *
   * Must only be implemented by final objects.
   */
  virtual bool validate (bool is_project, double frames_per_tick) const = 0;

  /**
   * Returns the project ArrangerObject matching this.
   *
   * This should be called when we have a copy or a clone, to get the actual
   * region in the project.
   */
  virtual std::optional<ArrangerObjectPtrVariant> find_in_project () const = 0;

  /**
   * Appends the ArrangerObject to where it belongs in the project (eg, a
   * Track), without taking into account its previous index (eg, before deletion
   * if undoing).
   *
   * @throw ZrythmError on failure.
   * @return A reference to the newly added clone.
   */
  virtual ArrangerObjectPtrVariant
  add_clone_to_project (bool fire_events) const = 0;

  /**
   * Inserts the object where it belongs in the project (eg, a Track).
   *
   * This function assumes that the object already knows the index where it
   * should be inserted in its parent.
   *
   * This is mostly used when undoing.
   *
   * @throw ZrythmException on failure.
   * @return A reference to the newly inserted clone.
   */
  virtual ArrangerObjectPtrVariant insert_clone_to_project () const = 0;

  /**
   * Removes the object (which can be obtained from @ref find_in_project()) from
   * its parent in the project.
   *
   * @return A pointer to the removed object (whose lifetime is now the
   * responsibility of the caller).
   */
  std::optional<ArrangerObjectPtrVariant>
  remove_from_project (bool free_obj, bool fire_events = false);

  /**
   * Returns whether the arranger object is part of a frozen track.
   *
   * @note Currently unused.
   */
  bool is_frozen () const;

  /**
   * Returns whether the given object is deletable or not (eg, start marker).
   */
  virtual bool is_deletable () const { return true; };

protected:
  void
  copy_members_from (const ArrangerObject &other, ObjectCloneType clone_type);

  void init_loaded_base ();

  DECLARE_DEFINE_BASE_FIELDS_METHOD ();

  /**
   * @brief To be called by @ref validate() implementations.
   */
  bool are_members_valid (bool is_project) const;

private:
  dsp::Position * get_position_ptr (PositionType type);

public:
  /**
   * Position (or start Position if the object has length).
   *
   * For audio/MIDI, the material starts at this frame.
   *
   * Midway Position between previous and next AutomationPoint's, if
   * AutomationCurve.
   */
  PositionProxy * pos_ = nullptr;

  Type type_{};

  /** ID of the track this object belongs to. */
  TrackUuid track_id_;

  /** Track this object belongs to (cache to be set during graph calculation). */
  OptionalTrackPtrVariant track_;

  /**
   * A copy ArrangerObject corresponding to this, such as when ctrl+dragging.
   *
   * This is generated when an object is added to the project selections.
   *
   * This will be the clone object saved in the cloned arranger selections in
   * each arranger during actions, and would get drawn separately.
   */
  ArrangerObject * transient_ = nullptr;

  /**
   * The opposite of the above. This will be set on the transient objects.
   */
  ArrangerObject * main_ = nullptr;

  /**
   * Whether deleted with delete tool.
   *
   * This is used to simply hide these objects until the action finishes so
   * that they can be cloned for the actions.
   */
  bool deleted_temporarily_ = false;

  /** Flags. */
  Flags flags_{};

  bool selected_{};

  /**
   * Whether part of an auditioner track. */
  bool is_auditioner_ = false;
};

inline bool
operator== (const ArrangerObject &lhs, const ArrangerObject &rhs)
{
  return lhs.type_ == rhs.type_ && *lhs.pos_ == *rhs.pos_
         && lhs.track_id_ == rhs.track_id_;
}

using ArrangerObjectRegistry =
  utils::OwningObjectRegistry<ArrangerObjectPtrVariant, ArrangerObject>;
using ArrangerObjectUuidReference = utils::UuidReference<ArrangerObjectRegistry>;
static_assert (UuidReferenceType<ArrangerObjectUuidReference>);

template <typename T>
concept ArrangerObjectSubclass = std::derived_from<T, ArrangerObject>;

template <typename T>
concept FinalArrangerObjectSubclass =
  std::derived_from<T, ArrangerObject> && FinalClass<T> && CompleteType<T>;

DEFINE_ENUM_FORMATTER (
  ArrangerObject::Type,
  ArrangerObject_Type,
  QT_TR_NOOP ("None"),
  QT_TR_NOOP ("All"),
  QT_TR_NOOP ("Region"),
  QT_TR_NOOP ("Midi Note"),
  QT_TR_NOOP ("Chord Object"),
  QT_TR_NOOP ("Scale Object"),
  QT_TR_NOOP ("Marker"),
  QT_TR_NOOP ("Automation Point"));

#endif
