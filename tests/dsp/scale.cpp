// SPDX-FileCopyrightText: © 2022, 2024 Alexandros Theodotou <alex@zrythm.org>
// SPDX-License-Identifier: LicenseRef-ZrythmLicense

#include "zrythm-test-config.h"

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include "dsp/scale.h"

#include "tests/helpers/zrythm_helper.h"

TEST_SUITE_BEGIN ("dsp/scale");

TEST_CASE ("scale contains note")
{
  MusicalScale scale (MusicalScale::Type::Minor, MusicalNote::D);

  ASSERT_TRUE (scale.contains_note (MusicalNote::C));
  ASSERT_FALSE (scale.contains_note (MusicalNote::CSharp));
  ASSERT_TRUE (scale.contains_note (MusicalNote::D));
  ASSERT_FALSE (scale.contains_note (MusicalNote::DSharp));
  ASSERT_TRUE (scale.contains_note (MusicalNote::E));
  ASSERT_TRUE (scale.contains_note (MusicalNote::F));
  ASSERT_FALSE (scale.contains_note (MusicalNote::FSharp));
  ASSERT_TRUE (scale.contains_note (MusicalNote::G));
  ASSERT_FALSE (scale.contains_note (MusicalNote::GSharp));
  ASSERT_TRUE (scale.contains_note (MusicalNote::A));
  ASSERT_TRUE (scale.contains_note (MusicalNote::ASharp));
  ASSERT_FALSE (scale.contains_note (MusicalNote::B));
}

TEST_SUITE_END;