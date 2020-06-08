// Copyright 2015 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <iomanip>
#include <string>

#include "starboard/file.h"
#include "starboard/nplb/file_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {
namespace {

void BasicTest(bool existing,
               int open_flags,
               bool expected_created,
               bool expected_success,
               int original_line) {
  ScopedRandomFile random_file(existing ? ScopedRandomFile::kCreate
                                        : ScopedRandomFile::kDontCreate);
  const std::string& filename = random_file.filename();
#define SB_FILE_OPEN_TEST_CONTEXT                                      \
  "existing=" << existing << ", flags=0x" << std::hex << open_flags    \
              << std::dec << ", expected_created=" << expected_created \
              << ", expected_success=" << expected_success             \
              << ", filename=" << filename                             \
              << ", original_line=" << original_line

  if (!existing) {
    EXPECT_FALSE(SbFileExists(filename.c_str())) << SB_FILE_OPEN_TEST_CONTEXT;
    if (SbFileExists(filename.c_str())) {
      return;
    }
  }

  bool created = !expected_created;
  SbFileError error = kSbFileErrorMax;
  SbFile file = SbFileOpen(filename.c_str(), open_flags, &created, &error);
  if (!expected_success) {
    EXPECT_FALSE(SbFileIsValid(file)) << SB_FILE_OPEN_TEST_CONTEXT;
    EXPECT_EQ(expected_created, created) << SB_FILE_OPEN_TEST_CONTEXT;
    EXPECT_NE(kSbFileOk, error) << SB_FILE_OPEN_TEST_CONTEXT;

    // Try to clean up in case test fails.
    if (SbFileIsValid(file)) {
      SbFileClose(file);
    }
  } else {
    EXPECT_TRUE(SbFileIsValid(file));
    EXPECT_EQ(expected_created, created) << SB_FILE_OPEN_TEST_CONTEXT;
    EXPECT_EQ(kSbFileOk, error) << SB_FILE_OPEN_TEST_CONTEXT;
    if (SbFileIsValid(file)) {
      bool result = SbFileClose(file);
      EXPECT_TRUE(result) << SB_FILE_OPEN_TEST_CONTEXT;
    }
  }
#undef SB_FILE_OPEN_TEST_CONTEXT
}

TEST(SbFileOpenTest, OpenOnlyOpensExistingFile) {
  BasicTest(true, kSbFileOpenOnly | kSbFileRead, false, true, __LINE__);
}

TEST(SbFileOpenTest, OpenOnlyDoesNotOpenNonExistingFile) {
  BasicTest(false, kSbFileOpenOnly | kSbFileRead, false, false, __LINE__);
}

TEST(SbFileOpenTest, CreateOnlyDoesNotCreateExistingFile) {
  BasicTest(true, kSbFileCreateOnly | kSbFileWrite, false, false, __LINE__);
}

TEST(SbFileOpenTest, CreateOnlyCreatesNonExistingFile) {
  BasicTest(false, kSbFileCreateOnly | kSbFileWrite, true, true, __LINE__);
}

TEST(SbFileOpenTest, OpenAlwaysOpensExistingFile) {
  BasicTest(true, kSbFileOpenAlways | kSbFileWrite, false, true, __LINE__);
}

TEST(SbFileOpenTest, OpenAlwaysCreatesNonExistingFile) {
  BasicTest(false, kSbFileOpenAlways | kSbFileWrite, true, true, __LINE__);
}

TEST(SbFileOpenTest, CreateAlwaysTruncatesExistingFile) {
  BasicTest(true, kSbFileCreateAlways | kSbFileWrite, true, true, __LINE__);
}

TEST(SbFileOpenTest, CreateAlwaysCreatesNonExistingFile) {
  BasicTest(false, kSbFileCreateAlways | kSbFileWrite, true, true, __LINE__);
}

TEST(SbFileOpenTest, OpenTruncatedTruncatesExistingFile) {
  BasicTest(true, kSbFileOpenTruncated | kSbFileWrite, false, true, __LINE__);
}

TEST(SbFileOpenTest, OpenTruncatedDoesNotCreateNonExistingFile) {
  BasicTest(false, kSbFileOpenTruncated | kSbFileWrite, false, false, __LINE__);
}

TEST(SbFileOpenTest, WorksWithNullOutParams) {
  ScopedRandomFile random_file;
  const std::string& filename = random_file.filename();

  // What error?
  {
    SbFileError error = kSbFileErrorMax;
    SbFile file = SbFileOpen(filename.c_str(), kSbFileOpenOnly | kSbFileRead,
                             NULL, &error);
    ASSERT_TRUE(SbFileIsValid(file));
    EXPECT_EQ(kSbFileOk, error) << "Failed to set error code for " << filename;
    bool result = SbFileClose(file);
    EXPECT_TRUE(result) << "SbFileClose failed for " << filename;
  }

  // Created what?
  {
    bool created = true;
    SbFile file = SbFileOpen(filename.c_str(), kSbFileOpenOnly | kSbFileRead,
                             &created, NULL);
    ASSERT_TRUE(SbFileIsValid(file));
    EXPECT_FALSE(created) << "Failed to set created to false for " << filename;
    bool result = SbFileClose(file);
    EXPECT_TRUE(result) << "SbFileClose failed for " << filename;
  }

  // What what?
  {
    SbFile file =
        SbFileOpen(filename.c_str(), kSbFileOpenOnly | kSbFileRead, NULL, NULL);
    ASSERT_TRUE(SbFileIsValid(file));
    bool result = SbFileClose(file);
    EXPECT_TRUE(result) << "SbFileClose failed for " << filename;
  }
}

TEST(SbFileOpenTest, OpenOnlyDoesNotOpenNonExistingStaticContentFile) {
  std::string path = GetFileTestsDataDir();
  std::string missing_file = path + kSbFileSepChar + "missing_file";
  bool created = true;
  SbFileError error = kSbFileErrorMax;
  SbFile file = SbFileOpen(missing_file.c_str(), kSbFileOpenOnly | kSbFileRead,
                           &created, &error);
  EXPECT_FALSE(SbFileIsValid(file));
  EXPECT_FALSE(created);
  EXPECT_NE(kSbFileOk, error);
}

}  // namespace
}  // namespace nplb
}  // namespace starboard
