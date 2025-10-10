// SPDX-FileCopyrightText: © 2025 Alexandros Theodotou <alex@zrythm.org>
// SPDX-License-Identifier: LicenseRef-ZrythmLicense

#include "dsp/atomic_position.h"
#include "dsp/tempo_map.h"

#include "unit/dsp/atomic_position_helpers.h"
#include <gtest/gtest.h>

namespace zrythm::dsp
{
class AtomicPositionTest : public ::testing::Test
{
protected:
  void SetUp () override
  {
    // Use custom conversion providers that support negative positions
    // 120 BPM = 960 ticks per beat, 0.5 seconds per beat
    conversion_providers = basic_conversion_providers ();
    pos = std::make_unique<AtomicPosition> (*conversion_providers);
  }

  std::unique_ptr<AtomicPosition::TimeConversionFunctions> conversion_providers;
  std::unique_ptr<AtomicPosition>                          pos;
};

// Test initial state
TEST_F (AtomicPositionTest, InitialState)
{
  EXPECT_EQ (pos->get_current_mode (), TimeFormat::Musical);
  EXPECT_DOUBLE_EQ (pos->get_ticks ().in (units::ticks), 0.0);
  EXPECT_DOUBLE_EQ (pos->get_seconds ().in (units::seconds), 0.0);
}

// Test setting/getting in Musical mode
TEST_F (AtomicPositionTest, MusicalModeOperations)
{
  // Set ticks in Musical mode
  pos->set_ticks (units::ticks (960.0));
  EXPECT_EQ (pos->get_current_mode (), TimeFormat::Musical);
  EXPECT_DOUBLE_EQ (pos->get_ticks ().in (units::ticks), 960.0);
  EXPECT_DOUBLE_EQ (
    pos->get_seconds ().in (units::seconds), 0.5); // 960 ticks @ 120 BPM = 0.5s

  // Set ticks again
  pos->set_ticks (units::ticks (1920.0));
  EXPECT_EQ (pos->get_current_mode (), TimeFormat::Musical);
  EXPECT_DOUBLE_EQ (pos->get_ticks ().in (units::ticks), 1920.0);
  EXPECT_DOUBLE_EQ (pos->get_seconds ().in (units::seconds), 1.0);
}

// Test setting/getting in Absolute mode
TEST_F (AtomicPositionTest, AbsoluteModeOperations)
{
  // Switch to Absolute mode
  pos->set_mode (TimeFormat::Absolute);

  // Set seconds in Absolute mode
  pos->set_seconds (units::seconds (0.5));
  EXPECT_EQ (pos->get_current_mode (), TimeFormat::Absolute);
  EXPECT_DOUBLE_EQ (pos->get_seconds ().in (units::seconds), 0.5);
  EXPECT_DOUBLE_EQ (
    pos->get_ticks ().in (units::ticks), 960.0); // 0.5s @ 120 BPM = 960 ticks

  // Set seconds again
  pos->set_seconds (units::seconds (1.0));
  EXPECT_EQ (pos->get_current_mode (), TimeFormat::Absolute);
  EXPECT_DOUBLE_EQ (pos->get_seconds ().in (units::seconds), 1.0);
  EXPECT_DOUBLE_EQ (pos->get_ticks ().in (units::ticks), 1920.0);
}

// Test mode conversion
TEST_F (AtomicPositionTest, ModeConversion)
{
  // Initial state: Musical mode, 0 ticks
  pos->set_ticks (units::ticks (960.0));

  // Convert to Absolute mode
  pos->set_mode (TimeFormat::Absolute);
  EXPECT_EQ (pos->get_current_mode (), TimeFormat::Absolute);
  EXPECT_DOUBLE_EQ (pos->get_seconds ().in (units::seconds), 0.5);

  // Convert back to Musical mode
  pos->set_mode (TimeFormat::Musical);
  EXPECT_EQ (pos->get_current_mode (), TimeFormat::Musical);
  EXPECT_DOUBLE_EQ (pos->get_ticks ().in (units::ticks), 960.0);
}

// Test setting ticks in Absolute mode
TEST_F (AtomicPositionTest, SetTicksInAbsoluteMode)
{
  pos->set_mode (TimeFormat::Absolute);
  pos->set_ticks (units::ticks (960.0)); // Should convert to seconds

  EXPECT_EQ (pos->get_current_mode (), TimeFormat::Absolute);
  EXPECT_DOUBLE_EQ (pos->get_seconds ().in (units::seconds), 0.5);
  EXPECT_DOUBLE_EQ (
    pos->get_ticks ().in (units::ticks), 960.0); // Should convert back
}

// Test setting seconds in Musical mode
TEST_F (AtomicPositionTest, SetSecondsInMusicalMode)
{
  pos->set_seconds (units::seconds (0.5)); // Should convert to ticks

  EXPECT_EQ (pos->get_current_mode (), TimeFormat::Musical);
  EXPECT_DOUBLE_EQ (pos->get_ticks ().in (units::ticks), 960.0);
  EXPECT_DOUBLE_EQ (
    pos->get_seconds ().in (units::seconds), 0.5); // Should convert back
}

// Test fractional positions
TEST_F (AtomicPositionTest, FractionalPositions)
{
  // Test fractional ticks in Musical mode
  pos->set_ticks (units::ticks (480.5));
  EXPECT_DOUBLE_EQ (pos->get_ticks ().in (units::ticks), 480.5);
  EXPECT_DOUBLE_EQ (
    pos->get_seconds ().in (units::seconds), 480.5 / 960.0 * 0.5);

  // Test fractional seconds in Absolute mode
  pos->set_mode (TimeFormat::Absolute);
  pos->set_seconds (units::seconds (0.25));
  EXPECT_DOUBLE_EQ (pos->get_seconds ().in (units::seconds), 0.25);
  EXPECT_DOUBLE_EQ (pos->get_ticks ().in (units::ticks), 0.25 / 0.5 * 960.0);
}

// Test get_samples() in Musical mode
TEST_F (AtomicPositionTest, GetSetSamplesInMusicalMode)
{
  // Set musical position
  pos->set_ticks (units::ticks (960.0));

  // 960 ticks @ 120 BPM = 0.5 seconds
  // 0.5s * 44100 Hz = 22050 samples
  EXPECT_EQ (pos->get_samples ().in (units::samples), 22050);

  // Roundtrip
  pos->set_ticks (units::ticks (0));
  pos->set_samples (units::samples (22050));
  EXPECT_DOUBLE_EQ (pos->get_ticks ().in (units::ticks), 960.0);
}

// Test get_samples() in Absolute mode
TEST_F (AtomicPositionTest, GetSetSamplesInAbsoluteMode)
{
  // Switch to Absolute mode and set position
  pos->set_mode (TimeFormat::Absolute);
  pos->set_seconds (units::seconds (0.5));

  // Same as above: 0.5s * 44100 Hz = 22050 samples
  EXPECT_EQ (pos->get_samples ().in (units::samples), 22050);

  // Roundtrip
  pos->set_seconds (units::seconds (0));
  pos->set_samples (units::samples (22050));
  EXPECT_DOUBLE_EQ (pos->get_seconds ().in (units::seconds), 0.5);
}

// Test get_samples() with fractional positions
TEST_F (AtomicPositionTest, GetSetSamplesFractional)
{
  // Fractional ticks
  pos->set_ticks (units::ticks (480.5));
  const double expectedSeconds = 480.5 / 960.0 * 0.5;
  EXPECT_EQ (
    pos->get_samples ().in (units::samples),
    static_cast<int64_t> (expectedSeconds * 44100.0));

  // Fractional seconds
  pos->set_mode (TimeFormat::Absolute);
  pos->set_seconds (units::seconds (0.25));
  EXPECT_EQ (
    pos->get_samples ().in (units::samples),
    static_cast<int64_t> (0.25 * 44100.0));

  // Roundtrip
  pos->set_seconds (units::seconds (0));
  pos->set_samples (units::samples (0.25 * 44100.0));
  EXPECT_DOUBLE_EQ (pos->get_seconds ().in (units::seconds), 0.25);
}

// Test thread safety (simulated with concurrent access)
TEST_F (AtomicPositionTest, ThreadSafety)
{
  // Writer thread sets values
  pos->set_ticks (units::ticks (960.0));

  // Reader thread gets values
  const double ticks = pos->get_ticks ().in (units::ticks);
  const double seconds = pos->get_seconds ().in (units::seconds);

  // Should be consistent
  EXPECT_DOUBLE_EQ (ticks, 960.0);
  EXPECT_DOUBLE_EQ (seconds, 0.5);

  // Change mode and values
  pos->set_mode (TimeFormat::Absolute);
  pos->set_seconds (units::seconds (1.0));

  // Reader gets again
  const double newTicks = pos->get_ticks ().in (units::ticks);
  const double newSeconds = pos->get_seconds ().in (units::seconds);

  // Should be consistent
  EXPECT_DOUBLE_EQ (newSeconds, 1.0);
  EXPECT_DOUBLE_EQ (newTicks, 1920.0);
}

// Test edge cases
TEST_F (AtomicPositionTest, EdgeCases)
{
  // Zero position
  pos->set_ticks (units::ticks (0.0));
  EXPECT_DOUBLE_EQ (pos->get_ticks ().in (units::ticks), 0.0);
  EXPECT_DOUBLE_EQ (pos->get_seconds ().in (units::seconds), 0.0);

  // Negative position
  pos->set_ticks (units::ticks (-100.0));
  EXPECT_DOUBLE_EQ (pos->get_ticks ().in (units::ticks), -100.0);
  EXPECT_DOUBLE_EQ (
    pos->get_seconds ().in (units::seconds), -100.0 / 960.0 * 0.5);

  // Test negative seconds as well
  pos->set_seconds (units::seconds (-0.5));
  EXPECT_DOUBLE_EQ (pos->get_seconds ().in (units::seconds), -0.5);
  EXPECT_DOUBLE_EQ (pos->get_ticks ().in (units::ticks), -0.5 / 0.5 * 960.0);

  // Large position
  pos->set_ticks (units::ticks (1e9));
  EXPECT_GT (pos->get_seconds ().in (units::seconds), 0.0);

  // NaN/infinity protection (should assert)
  // Uncomment to test:
  // pos->set_seconds (std::numeric_limits<double>::quiet_NaN ());
}

// Test serialization/deserialization in Musical mode
TEST_F (AtomicPositionTest, SerializationMusicalMode)
{
  // Set musical position
  pos->set_ticks (units::ticks (960.0));

  // Serialize to JSON
  nlohmann::json j;
  j = *pos;

  // Create new position with same tempo map
  AtomicPosition new_pos (*conversion_providers);
  j.get_to (new_pos);

  // Verify state
  EXPECT_EQ (new_pos.get_current_mode (), TimeFormat::Musical);
  EXPECT_DOUBLE_EQ (new_pos.get_ticks ().in (units::ticks), 960.0);
  EXPECT_DOUBLE_EQ (new_pos.get_seconds ().in (units::seconds), 0.5);
}

// Test serialization/deserialization in Absolute mode
TEST_F (AtomicPositionTest, SerializationAbsoluteMode)
{
  // Set absolute position
  pos->set_mode (TimeFormat::Absolute);
  pos->set_seconds (units::seconds (1.5));

  // Serialize to JSON
  nlohmann::json j;
  j = *pos;

  // Create new position with same tempo map
  AtomicPosition new_pos (*conversion_providers);
  j.get_to (new_pos);

  // Verify state
  EXPECT_EQ (new_pos.get_current_mode (), TimeFormat::Absolute);
  EXPECT_DOUBLE_EQ (new_pos.get_seconds ().in (units::seconds), 1.5);
  EXPECT_DOUBLE_EQ (new_pos.get_ticks ().in (units::ticks), 1.5 / 0.5 * 960.0);
}

// Test serialization after mode conversion
TEST_F (AtomicPositionTest, SerializationAfterModeConversion)
{
  // Set musical position and convert to absolute
  pos->set_ticks (units::ticks (1920.0));
  pos->set_mode (TimeFormat::Absolute);

  // Serialize to JSON
  nlohmann::json j;
  j = *pos;

  // Create new position with same tempo map
  AtomicPosition new_pos (*conversion_providers);
  j.get_to (new_pos);

  // Verify state
  EXPECT_EQ (new_pos.get_current_mode (), TimeFormat::Absolute);
  EXPECT_DOUBLE_EQ (new_pos.get_seconds ().in (units::seconds), 1.0);
  EXPECT_DOUBLE_EQ (new_pos.get_ticks ().in (units::ticks), 1920.0);
}

TEST_F (AtomicPositionTest, Formatter)
{
  pos->set_ticks (units::ticks (960.0));
  pos->set_mode (TimeFormat::Musical);

  const std::string formatted = fmt::format ("{}", *pos);
  EXPECT_TRUE (formatted.find ("Ticks: 960.00") != std::string::npos);
  EXPECT_TRUE (formatted.find ("Seconds: 0.500") != std::string::npos);
  EXPECT_TRUE (formatted.find ("Samples: 22050") != std::string::npos);
  EXPECT_TRUE (formatted.find ("Mode: Musical") != std::string::npos);

  // Test in Absolute mode
  pos->set_mode (TimeFormat::Absolute);
  pos->set_seconds (units::seconds (1.5));

  const std::string formatted2 = fmt::format ("{}", *pos);
  EXPECT_TRUE (formatted2.find ("Ticks: 2880.00") != std::string::npos);
  EXPECT_TRUE (formatted2.find ("Seconds: 1.500") != std::string::npos);
  EXPECT_TRUE (formatted2.find ("Mode: Absolute") != std::string::npos);
}
}
