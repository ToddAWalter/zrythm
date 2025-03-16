// SPDX-FileCopyrightText: © 2019-2021, 2024 Alexandros Theodotou <alex@zrythm.org>
// SPDX-License-Identifier: LicenseRef-ZrythmLicense

#ifndef __GUI_BACKEND_CLIP_EDITOR_H__
#define __GUI_BACKEND_CLIP_EDITOR_H__

#include "gui/backend/backend/audio_clip_editor.h"
#include "gui/backend/backend/automation_editor.h"
#include "gui/backend/backend/chord_editor.h"
#include "gui/backend/backend/piano_roll.h"
#include "gui/dsp/arranger_object_span.h"
#include "gui/dsp/region.h"
#include "gui/dsp/region_identifier.h"

class ArrangerSelections;

/**
 * @addtogroup gui_backend
 *
 * @{
 */

#define CLIP_EDITOR (PROJECT->clip_editor_)

/**
 * Clip editor serializable backend.
 *
 * The actual widgets should reflect the information here.
 */
class ClipEditor final
    : public QObject,
      public zrythm::utils::serialization::ISerializable<ClipEditor>,
      public ICloneable<ClipEditor>
{
  Q_OBJECT
  QML_ELEMENT
  Q_PROPERTY (QVariant region READ getRegion NOTIFY regionChanged)
public:
  ClipEditor (const DeserializationDependencyHolder &dh)
      : ClipEditor (
          dh.get<std::reference_wrapper<ArrangerObjectRegistry>> ().get ())
  {
  }

  ClipEditor (ArrangerObjectRegistry &reg, QObject * parent = nullptr)
      : QObject (parent), object_registry_ (reg)
  {
  }

  // ============================================================================
  // QML Interface
  // ============================================================================

  QVariant getRegion () const
  {
    if (has_region ())
      {
        return QVariant::fromStdVariant (get_region ().value ());
      }

    return QVariant{};
  }
  Q_SIGNAL void regionChanged (QVariant region);

  // ============================================================================

  /**
   * Inits the ClipEditor after a Project is loaded.
   */
  void init_loaded ();

  /**
   * Inits the clip editor.
   */
  void init ();

  /**
   * Sets the track and refreshes the piano roll widgets.
   */
  void set_region (Region::Uuid region_id) { region_id_ = region_id; };
  void unset_region () { region_id_.reset (); }

  bool has_region () const { return region_id_.has_value (); }

  std::optional<RegionPtrVariant> get_region () const;
  std::optional<Region::Uuid> get_region_id () const {return region_id_;}

  // ArrangerObjectRegistrySpan get_arranger_selections ();

  std::optional<TrackPtrVariant> get_track () const;

  std::optional<Region::TrackUuid> get_track_id () const;
  /**
   * @brief Unsets the region if it belongs to the given track.
   */
  void unset_region_if_belongs_to_track (const Region::TrackUuid &track_id);

  /**
   * To be called when recalculating the graph.
   */
  void set_caches ();

  DECLARE_DEFINE_FIELDS_METHOD ();

  void init_after_cloning (const ClipEditor &other, ObjectCloneType clone_type)
    override
  {
    region_id_ = other.region_id_;
    audio_clip_editor_ = other.audio_clip_editor_;
    automation_editor_ = other.automation_editor_;
    chord_editor_ = other.chord_editor_;
    track_ = other.track_;
  }

public:
  /** Region currently attached to the clip editor. */
  std::optional<ArrangerObject::Uuid> region_id_;

  PianoRoll        piano_roll_;
  AudioClipEditor  audio_clip_editor_;
  AutomationEditor automation_editor_;
  ChordEditor      chord_editor_;

  /* --- caches --- */
  // std::optional<RegionPtrVariant> region_;

  std::optional<TrackPtrVariant> track_;

  ArrangerObjectRegistry &object_registry_;
};

/**
 * @}
 */

#endif
