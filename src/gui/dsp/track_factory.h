#pragma once

#include <utility>

#include "gui/backend/backend/settings_manager.h"
#include "gui/dsp/plugin_all.h"
#include "gui/dsp/track_all.h"

/**
 * @brief Factory for tracks.
 */
class TrackFactory : public QObject
{
  Q_OBJECT
  QML_ELEMENT
public:
  TrackFactory () = delete;
  TrackFactory (
    TrackRegistry          &track_registry,
    PluginRegistry         &plugin_registry,
    PortRegistry           &port_registry,
    ArrangerObjectRegistry &arranger_object_registry,
    gui::SettingsManager   &settings_mgr,
    QObject *               parent = nullptr)
      : QObject (parent), track_registry_ (track_registry),
        plugin_registry_ (plugin_registry), port_registry_ (port_registry),
        arranger_object_registry_ (arranger_object_registry),
        settings_manager_ (settings_mgr)
  {
  }

  static TrackFactory * get_instance ();

  template <typename TrackT> class Builder
  {
    friend class TrackFactory;

  private:
    explicit Builder (
      TrackRegistry          &track_registry,
      PluginRegistry         &plugin_registry,
      PortRegistry           &port_registry,
      ArrangerObjectRegistry &arranger_object_registry)
        : track_registry_ (track_registry), plugin_registry_ (plugin_registry),
          port_registry_ (port_registry),
          arranger_object_registry_ (arranger_object_registry)
    {
    }

    Builder &with_settings_manager (gui::SettingsManager &settings_manager)
    {
      settings_manager_ = settings_manager;
      return *this;
    }

  public:
    auto build ()
    {
      auto obj_ref = [&] () {
        return track_registry_.create_object<TrackT> (
          track_registry_, plugin_registry_, port_registry_,
          arranger_object_registry_, true);
      }();

      // auto * obj = std::get<PluginT *> (obj_ref.get_object ());

      return obj_ref;
    }

  private:
    TrackRegistry                    &track_registry_;
    PluginRegistry                   &plugin_registry_;
    PortRegistry                     &port_registry_;
    ArrangerObjectRegistry           &arranger_object_registry_;
    OptionalRef<gui::SettingsManager> settings_manager_;
  };

  template <typename TrackT> auto get_builder () const
  {
    auto builder =
      Builder<TrackT> (
        track_registry_, plugin_registry_, port_registry_,
        arranger_object_registry_)
        .with_settings_manager (settings_manager_);
    return builder;
  }

private:
  template <FinalTrackSubclass TrackT>
  TrackUuidReference create_empty_track () const
  {
    auto obj_ref = get_builder<TrackT> ().build ();
    return obj_ref;
  }

  auto create_empty_track (Track::Type type) const
  {
    switch (type)
      {
      case Track::Type::Audio:
        return create_empty_track<AudioTrack> ();
      case Track::Type::Midi:
        return create_empty_track<MidiTrack> ();
      case Track::Type::MidiGroup:
        return create_empty_track<MidiGroupTrack> ();
      case Track::Type::Folder:
        return create_empty_track<FolderTrack> ();
      case Track::Type::Instrument:
        return create_empty_track<InstrumentTrack> ();
      case Track::Type::Master:
        return create_empty_track<MasterTrack> ();
      case Track::Type::Chord:
        return create_empty_track<ChordTrack> ();
      case Track::Type::Marker:
        return create_empty_track<MarkerTrack> ();
      case Track::Type::Tempo:
        return create_empty_track<TempoTrack> ();
      case Track::Type::Modulator:
        return create_empty_track<ModulatorTrack> ();
      case Track::Type::AudioBus:
        return create_empty_track<AudioBusTrack> ();
      case Track::Type::MidiBus:
        return create_empty_track<MidiBusTrack> ();
      case Track::Type::AudioGroup:
        return create_empty_track<AudioGroupTrack> ();
      }
  }

public:
  template <FinalTrackSubclass TrackT>
  auto add_empty_track (Tracklist &tracklist)
  {
    auto track_ref = create_empty_track<TrackT> ();
// TODO
#if 0
    tracklist.append_track (
      track_ref,
      *audio_engine_, false, false);
#endif
    return track_ref;
  }

  Q_INVOKABLE QVariant addEmptyTrackFromType (int type)
  {
    Track::Type tt = ENUM_INT_TO_VALUE (Track::Type, type);
    //   if (zrythm_app->check_and_show_trial_limit_error ())
    //     return;

    try
      {
        auto * track_base = Track::create_empty_with_action (tt);
        std::visit (
          [&] (auto &&track) {
            using TrackT = base_type<decltype (track)>;
            z_debug (
              "created {} track: {}", typename_to_string<TrackT> (),
              track->get_name ());
          },
          convert_to_variant<TrackPtrVariant> (track_base));
        return QVariant::fromStdVariant (
          convert_to_variant<TrackPtrVariant> (track_base));
      }
    catch (const ZrythmException &e)
      {
        e.handle (QObject::tr ("Failed to create track"));
      }
    // TODO
    return {};
  }

  template <typename TrackT>
  auto clone_new_object_identity (const TrackT &other) const
  {
    return plugin_registry_.clone_object (other, plugin_registry_);
  }

  template <typename TrackT>
  auto clone_object_snapshot (const TrackT &other, QObject &owner) const
  {
    TrackT * new_obj{};

    new_obj =
      other.clone_qobject (&owner, ObjectCloneType::Snapshot, plugin_registry_);
    return new_obj;
  }

private:
  TrackRegistry          &track_registry_;
  PluginRegistry         &plugin_registry_;
  PortRegistry           &port_registry_;
  ArrangerObjectRegistry &arranger_object_registry_;
  gui::SettingsManager   &settings_manager_;
};
