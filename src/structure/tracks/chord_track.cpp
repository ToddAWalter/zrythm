// SPDX-FileCopyrightText: © 2018-2022, 2024-2025 Alexandros Theodotou <alex@zrythm.org>
// SPDX-License-Identifier: LicenseRef-ZrythmLicense

#include <algorithm>

#include "gui/backend/backend/project.h"
#include "gui/backend/backend/settings_manager.h"
#include "gui/backend/backend/zrythm.h"
#include "structure/arrangement/chord_region.h"
#include "structure/tracks/chord_track.h"
#include "structure/tracks/track.h"
#include "utils/rt_thread_id.h"

namespace zrythm::structure::tracks
{
ChordTrack::ChordTrack (FinalTrackDependencies dependencies)
    : Track (
        Track::Type::Chord,
        PortType::Midi,
        PortType::Midi,
        dependencies.to_base_dependencies ()),
      ProcessableTrack (
        dependencies.transport_,
        PortType::Midi,
        Dependencies{
          dependencies.tempo_map_, dependencies.file_audio_source_registry_,
          dependencies.port_registry_, dependencies.param_registry_,
          dependencies.obj_registry_ }),
      RecordableTrack (
        dependencies.transport_,
        Dependencies{
          dependencies.tempo_map_, dependencies.file_audio_source_registry_,
          dependencies.port_registry_, dependencies.param_registry_,
          dependencies.obj_registry_ }),
      ChannelTrack (dependencies),
      arrangement::ArrangerObjectOwner<ChordRegion> (
        dependencies.obj_registry_,
        dependencies.file_audio_source_registry_,
        *this),
      arrangement::ArrangerObjectOwner<ScaleObject> (
        dependencies.obj_registry_,
        dependencies.file_audio_source_registry_,
        *this)
{
  color_ = Color (QColor ("#1C8FFB"));
  icon_name_ = u8"gnome-icon-library-library-music-symbolic";
  automationTracklist ()->setParent (this);
  processor_->set_fill_events_callback (
    [this] (
      const dsp::ITransport &transport, const EngineProcessTimeInfo &time_nfo,
      dsp::MidiEventVector *                        midi_events,
      std::optional<TrackProcessor::StereoPortPair> stereo_ports) {
      for (
        const auto * r : structure::arrangement::template ArrangerObjectOwner<
          structure::arrangement::ChordRegion>::get_children_view ())
        {
          TrackProcessor::fill_events_from_region_rt (
            transport, time_nfo, midi_events, stereo_ports, *r);
        }
    });
  processor_->set_append_midi_inputs_to_outputs_func (
    [this] (
      dsp::MidiEventVector &out_events, const dsp::MidiEventVector &in_events,
      const EngineProcessTimeInfo &time_nfo) {
      out_events.transform_chord_and_append (
        in_events,
        [this] (midi_byte_t note_number) {
          return note_pitch_to_chord_descriptor (note_number);
        },
        arrangement::MidiNote::DEFAULT_VELOCITY, time_nfo.local_offset_,
        time_nfo.nframes_);
    });
}

void
init_from (
  ChordTrack            &obj,
  const ChordTrack      &other,
  utils::ObjectCloneType clone_type)
{
  init_from (
    static_cast<Track &> (obj), static_cast<const Track &> (other), clone_type);
  init_from (
    static_cast<ProcessableTrack &> (obj),
    static_cast<const ProcessableTrack &> (other), clone_type);
  init_from (
    static_cast<RecordableTrack &> (obj),
    static_cast<const RecordableTrack &> (other), clone_type);
  init_from (
    static_cast<ChannelTrack &> (obj),
    static_cast<const ChannelTrack &> (other), clone_type);
  init_from (
    static_cast<arrangement::ArrangerObjectOwner<arrangement::ChordRegion> &> (
      obj),
    static_cast<const arrangement::ArrangerObjectOwner<
      arrangement::ChordRegion> &> (other),
    clone_type);
  init_from (
    static_cast<arrangement::ArrangerObjectOwner<arrangement::ScaleObject> &> (
      obj),
    static_cast<const arrangement::ArrangerObjectOwner<
      arrangement::ScaleObject> &> (other),
    clone_type);
}

bool
ChordTrack::initialize ()
{
  init_channel ();
  generate_automation_tracks (*this);
  init_recordable_track ([] () {
    return ZRYTHM_HAVE_UI
           && zrythm::gui::SettingsManager::get_instance ()->get_trackAutoArm ();
  });

  return true;
}

void
ChordTrack::clear_objects ()
{
  ArrangerObjectOwner<ChordRegion>::clear_objects ();
  ArrangerObjectOwner<ScaleObject>::clear_objects ();
}

void
ChordTrack::set_playback_caches ()
{
// FIXME: TODO
#if 0
  region_snapshots_.clear ();
  region_snapshots_.reserve (region_list_->regions_.size ());
  foreach_region ([&] (auto &region) {
    z_return_if_fail (region.track_id_ == get_uuid ());
    region_snapshots_.push_back (region.clone_unique ());
  });

  scale_snapshots_.clear ();
  scale_snapshots_.reserve (scales_.size ());
  for (const auto &scale : get_scales_view ())
    {
      scale_snapshots_.push_back (scale->clone_unique ());
    }
#endif
}

void
ChordTrack::init_loaded (
  PluginRegistry                  &plugin_registry,
  dsp::PortRegistry               &port_registry,
  dsp::ProcessorParameterRegistry &param_registry)
{
  // ChannelTrack must be initialized before AutomatableTrack
  ChannelTrack::init_loaded (plugin_registry, port_registry, param_registry);
  ProcessableTrack::init_loaded (plugin_registry, port_registry, param_registry);
  RecordableTrack::init_loaded (plugin_registry, port_registry, param_registry);
}

auto
ChordTrack::get_scale_at (size_t index) const -> ScaleObject *
{
  return ArrangerObjectOwner<ScaleObject>::get_children_view ()[index];
}

auto
ChordTrack::get_scale_at_ticks (double timeline_ticks) const -> ScaleObject *
{
  auto view = std::ranges::reverse_view (
    ArrangerObjectOwner<ScaleObject>::get_children_view ());
  auto it = std::ranges::find_if (view, [timeline_ticks] (const auto &scale) {
    return scale->position ()->ticks () <= timeline_ticks;
  });

  return it != view.end () ? (*it) : nullptr;
}

auto
ChordTrack::get_chord_at_ticks (double timeline_ticks) const -> ChordObject *
{
  const auto timeline_frames = static_cast<signed_frame_t> (std::round (
    base_dependencies_.tempo_map_.tick_to_samples (timeline_ticks)));
  auto       region_var =
    arrangement::ArrangerObjectSpan{
      arrangement::ArrangerObjectOwner<
        arrangement::ChordRegion>::get_children_vector ()
    }
      .get_bounded_object_at_position (timeline_frames, false);
  if (!region_var)
    {
      return nullptr;
    }
  const auto * region = std::get<ChordRegion *> (*region_var);

  const auto local_frames =
    timeline_frames_to_local (*region, timeline_frames, true);

  auto chord_objects_view = region->get_children_view () | std::views::reverse;
  auto it =
    std::ranges::find_if (chord_objects_view, [local_frames] (const auto &co) {
      return co->position ()->samples () <= local_frames;
    });

  return it != chord_objects_view.end () ? (*it) : nullptr;
}

}
