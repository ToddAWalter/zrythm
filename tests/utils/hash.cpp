// SPDX-FileCopyrightText: © 2021, 2024 Alexandros Theodotou <alex@zrythm.org>
// SPDX-License-Identifier: LicenseRef-ZrythmLicense

#include "zrythm-test-config.h"

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include "utils/hash.h"
#include "utils/logger.h"

#include "doctest_wrapper.h"
#include <glibmm.h>
#include <xxhash.h>

TEST_SUITE_BEGIN ("utils/hash");

TEST_CASE ("get from file")
{
  auto filepath =
    Glib::build_filename (TESTS_SRCDIR, "test_start_with_signal.mp3");

  z_info ("start");
  auto     hash = hash_get_from_file (filepath, HASH_ALGORITHM_XXH32);
  uint32_t hash_simple = hash_get_from_file_simple (filepath.c_str ());
  z_info ("end");
  REQUIRE_EQ (hash, "ca5b86cb");
  REQUIRE_EQ (hash_simple, 3394995915);

#if XXH_VERSION_NUMBER >= 800
  z_info ("start");
  hash = hash_get_from_file (filepath, HASH_ALGORITHM_XXH3_64);
  z_info ("end");
  REQUIRE_EQ (hash, "e9cd4b9c1e12785e");
#endif
}

TEST_SUITE_END;