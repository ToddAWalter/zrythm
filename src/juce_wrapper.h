// SPDX-FileCopyrightText: © 2024-2025 Alexandros Theodotou <alex@zrythm.org>
// SPDX-License-Identifier: LicenseRef-ZrythmLicense

#pragma once

#include "zrythm-config.h"

#include <span>

#ifdef __GNUC__
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wundef"
#  pragma GCC diagnostic ignored "-Wcast-qual"
#  ifndef __clang__
#    pragma GCC diagnostic ignored "-Wduplicated-branches"
#    pragma GCC diagnostic ignored "-Wanalyzer-use-of-uninitialized-value"
#  endif // __clang__
#endif   // __GNUC__

// clang-format off
#include "juce_audio_basics/juce_audio_basics.h"
#include "juce_core/juce_core.h"
// #include "ext/juce/modules/juce_dsp/juce_dsp.h"
#include "juce_audio_devices/juce_audio_devices.h"
#include "juce_audio_formats/juce_audio_formats.h"
#include "juce_audio_processors/juce_audio_processors.h"
#include "juce_audio_utils/juce_audio_utils.h"
#include "juce_dsp/juce_dsp.h"
// clang-format on

#ifdef __GNUC__
#  pragma GCC diagnostic pop
#endif // __GNUC__
