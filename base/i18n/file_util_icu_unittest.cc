// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/i18n/file_util_icu.h"

#include "base/file_util.h"
#include "base/path_service.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

// file_util winds up using autoreleased objects on the Mac, so this needs
// to be a PlatformTest
class FileUtilICUTest : public PlatformTest {
 protected:
  virtual void SetUp() {
    PlatformTest::SetUp();
    // Name a subdirectory of the temp directory.
    ASSERT_TRUE(PathService::Get(base::DIR_TEMP, &test_dir_));
    test_dir_ = test_dir_.Append(FILE_PATH_LITERAL("FileUtilTest"));

    // Create a fresh, empty copy of this directory.
    file_util::Delete(test_dir_, true);
    file_util::CreateDirectory(test_dir_);
  }
  virtual void TearDown() {
    PlatformTest::TearDown();
    // Clean up test directory
    ASSERT_TRUE(file_util::Delete(test_dir_, true));
    ASSERT_FALSE(file_util::PathExists(test_dir_));
  }

  // the path to temporary directory used to contain the test operations
  FilePath test_dir_;
};

static const struct goodbad_pair {
  std::wstring bad_name;
  std::wstring good_name;
} kIllegalCharacterCases[] = {
  {L"bad*file:name?.jpg", L"bad-file-name-.jpg"},
  {L"**********::::.txt", L"--------------.txt"},
  // We can't use UCNs (universal character names) for C0/C1 characters and
  // U+007F, but \x escape is interpreted by MSVC and gcc as we intend.
  {L"bad\x0003\x0091 file\u200E\u200Fname.png", L"bad-- file--name.png"},
#if defined(OS_WIN)
  {L"bad*file\\name.jpg", L"bad-file-name.jpg"},
  {L"\t  bad*file\\name/.jpg ", L"bad-file-name-.jpg"},
#elif defined(OS_POSIX)
  {L"bad*file?name.jpg", L"bad-file-name.jpg"},
  {L"\t  bad*file?name/.jpg ", L"bad-file-name-.jpg"},
#endif
  {L"this_file_name is okay!.mp3", L"this_file_name is okay!.mp3"},
  {L"\u4E00\uAC00.mp3", L"\u4E00\uAC00.mp3"},
  {L"\u0635\u200C\u0644.mp3", L"\u0635\u200C\u0644.mp3"},
  {L"\U00010330\U00010331.mp3", L"\U00010330\U00010331.mp3"},
  // Unassigned codepoints are ok.
  {L"\u0378\U00040001.mp3", L"\u0378\U00040001.mp3"},
  // Non-characters are not allowed.
  {L"bad\uFFFFfile\U0010FFFEname.jpg ", L"bad-file-name.jpg"},
  {L"bad\uFDD0file\uFDEFname.jpg ", L"bad-file-name.jpg"},
};

TEST_F(FileUtilICUTest, ReplaceIllegalCharactersTest) {
  for (unsigned int i = 0; i < arraysize(kIllegalCharacterCases); ++i) {
    std::wstring bad_name(kIllegalCharacterCases[i].bad_name);
    file_util::ReplaceIllegalCharacters(&bad_name, L'-');
    EXPECT_EQ(kIllegalCharacterCases[i].good_name, bad_name);
  }
}

