// SPDX-FileCopyrightText: © 2020-2022, 2024 Alexandros Theodotou <alex@zrythm.org>
// SPDX-License-Identifier: LicenseRef-ZrythmLicense

#include "zrythm-test-config.h"

#include "common/utils/flags.h"
#include "common/utils/io.h"

#include "tests/helpers/zrythm_helper.h"

#if 0
TEST (GetParentDirectory)
{
  auto parent;

#  ifdef _WIN32
  parent = io_path_get_parent_dir ("C:\\ab\\cd\\ef\\gh");
  ASSERT_EQ (parent, "C:\\ab\\cd\\ef");
  parent = io_path_get_parent_dir ("C:\\ab\\cd\\ef\\gh\\");
  ASSERT_EQ (parent, "C:\\ab\\cd\\ef");
  parent = io_path_get_parent_dir ("C:\\ab");
  ASSERT_EQ (parent, "C:\\");
  parent = io_path_get_parent_dir ("C:\\");
  ASSERT_EQ (parent, "C:\\");
#  else
  parent = io_path_get_parent_dir ("/ab/cd/ef/gh");
  ASSERT_EQ (parent, "/ab/cd/ef");
  parent = io_path_get_parent_dir ("/ab/cd/ef/gh/");
  ASSERT_EQ (parent, "/ab/cd/ef");
  parent = io_path_get_parent_dir ("/ab");
  ASSERT_EQ (parent, "/");
  parent = io_path_get_parent_dir ("/");
  ASSERT_EQ (parent, "/");
#  endif
}
#endif

TEST (IO, GetExtension)
{
  auto test_get_ext =
    [] (const std::string &file, const std::string &expected_ext) {
      std::string res = io_file_get_ext (file.c_str ());
      ASSERT_EQ (res, expected_ext);
    };

  test_get_ext ("abc.wav", "wav");
  test_get_ext ("abc.test.wav", "wav");
  test_get_ext ("abctestwav", "");
  test_get_ext ("abctestwav.", "");
  test_get_ext ("...", "");
}

TEST (IO, StripExtension)
{
  auto test_strip_ext =
    [] (const std::string &file, const std::string &expected) {
      std::string res = io_file_strip_ext (file);
      ASSERT_EQ (res, expected);
    };

  test_strip_ext ("abc.wav", "abc");
  test_strip_ext ("abc.test.wav", "abc.test");
  test_strip_ext ("abctestwav", "abctestwav");
  test_strip_ext ("abctestwav.", "abctestwav");
  test_strip_ext ("...", "..");
}

TEST_F (ZrythmFixture, GetFilesInDirectory)
{
#ifdef __linux__

  {
    auto files =
      io_get_files_in_dir_ending_in (TESTS_SRCDIR, F_NO_RECURSIVE, ".wav");
    ASSERT_SIZE_EQ (files, 1);
  }

  ASSERT_ANY_THROW (
    io_get_files_in_dir_ending_in ("/non-existent", F_RECURSIVE, ".wav"));
#endif
}