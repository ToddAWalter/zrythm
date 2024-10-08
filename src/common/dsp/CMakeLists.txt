# SPDX-FileCopyrightText: © 2019-2021, 2024 Alexandros Theodotou <alex@zrythm.org>
# SPDX-License-Identifier: LicenseRef-ZrythmLicense

target_sources(zrythm_lib
  PRIVATE
    arranger_object.cpp
    audio_bus_track.cpp
    audio_function.cpp
    audio_group_track.cpp
    audio_port.cpp
    audio_region.cpp
    audio_track.cpp
    automatable_track.cpp
    automation_function.cpp
    automation_point.cpp
    automation_region.cpp
    automation_track.cpp
    automation_tracklist.cpp
    balance_control.cpp
    channel.cpp
    channel_send.cpp
    channel_track.cpp
    chord_descriptor.cpp
    chord_object.cpp
    chord_region.cpp
    chord_track.cpp
    clip.cpp
    colored_object.cpp
    control_port.cpp
    control_room.cpp
    cv_port.cpp
    ditherer.cpp
    engine.cpp
    engine_dummy.cpp
    engine_jack.cpp
    engine_pa.cpp
    engine_pulse.cpp
    engine_rtaudio.cpp
    engine_rtmidi.cpp
    exporter.cpp
    ext_port.cpp
    fadeable_object.cpp
    fader.cpp
    foldable_track.cpp
    folder_track.cpp
    graph.cpp
    graph_export.cpp
    graph_node.cpp
    graph_thread.cpp
    group_target_track.cpp
    hardware_processor.cpp
    instrument_track.cpp
    lane_owned_object.cpp
    laned_track.cpp
    lengthable_object.cpp
    loopable_object.cpp
    marker.cpp
    marker_track.cpp
    master_track.cpp
    meter.cpp
    metronome.cpp
    midi_bus_track.cpp
    midi_event.cpp
    midi_function.cpp
    midi_group_track.cpp
    midi_mapping.cpp
    midi_note.cpp
    midi_port.cpp
    midi_region.cpp
    midi_track.cpp
    modulator_macro_processor.cpp
    modulator_track.cpp
    muteable_object.cpp
    nameable_object.cpp
    pan.cpp
    peak_fall_smooth.cpp
    piano_roll_track.cpp
    pool.cpp
    port.cpp
    port_connection.cpp
    port_connections_manager.cpp
    port_identifier.cpp
    position.cpp
    processable_track.cpp
    quantize_options.cpp
    recordable_track.cpp
    recording_event.cpp
    recording_manager.cpp
    region.cpp
    region_identifier.cpp
    region_link_group.cpp
    region_link_group_manager.cpp
    region_owned_object.cpp
    region_owner.cpp
    router.cpp
    rtaudio_device.cpp
    rtmidi_device.cpp
    sample_processor.cpp
    scale.cpp
    scale_object.cpp
    snap_grid.cpp
    stretcher.cpp
    tempo_track.cpp
    timeline_object.cpp
    track.cpp
    track_lane.cpp
    track_processor.cpp
    tracklist.cpp
    transport.cpp
    true_peak_dsp.cpp
    velocity.cpp
)

add_subdirectory(vamp-ports)

# optimized audio
add_library(zrythm_optimized_audio_lib STATIC
  curve.cpp
  kmeter_dsp.cpp
  peak_dsp.cpp
)
target_link_libraries(zrythm_optimized_audio_lib PRIVATE ${zrythm_link_libs})
target_include_directories(zrythm_optimized_audio_lib PRIVATE ${zrythm_include_dirs})
target_compile_definitions(zrythm_optimized_audio_lib PRIVATE ${zrythm_definitions})
target_compile_options(zrythm_optimized_audio_lib PRIVATE
  ${zrythm_compile_args}
  $<$<OR:$<CXX_COMPILER_ID:GNU>,$<CXX_COMPILER_ID:Clang>,$<CXX_COMPILER_ID:AppleClang>>:-O3>
)
