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

#include <string.h>

#include <algorithm>

#include "starboard/common/file.h"
#include "starboard/common/string.h"
#include "starboard/configuration_constants.h"
#include "starboard/memory.h"
#include "starboard/nplb/file_helpers.h"
#include "starboard/system.h"
#include "starboard/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {
namespace {

// Size of appropriate path buffer.
const size_t kPathSize = kSbFileMaxPath + 1;

void BasicTest(SbSystemPathId id,
               bool expect_result,
               bool expected_result,
               int line) {
#define LOCAL_CONTEXT "Context : id=" << id << ", line=" << line;
  std::vector<char> path(kPathSize);
  memset(path.data(), 0xCD, kPathSize);
  bool result = SbSystemGetPath(id, path.data(), kPathSize);
  if (expect_result) {
    EXPECT_EQ(expected_result, result) << LOCAL_CONTEXT;
  }
  if (result) {
    EXPECT_NE('\xCD', path[0]) << LOCAL_CONTEXT;
    int len = static_cast<int>(strlen(path.data()));
    EXPECT_GT(len, 0) << LOCAL_CONTEXT;
  } else {
    EXPECT_EQ('\xCD', path[0]) << LOCAL_CONTEXT;
  }
#undef LOCAL_CONTEXT
}

void UnmodifiedOnFailureTest(SbSystemPathId id, int line) {
  std::vector<char> path(kPathSize);
  memset(path.data(), 0xCD, kPathSize);
  for (size_t i = 0; i <= kPathSize; ++i) {
    if (SbSystemGetPath(id, path.data(), i)) {
      return;
    }
    for (auto ch : path) {
      ASSERT_EQ('\xCD', ch) << "Context : id=" << id << ", line=" << line;
    }
  }
}

TEST(SbSystemGetPathTest, ReturnsRequiredPaths) {
  BasicTest(kSbSystemPathContentDirectory, true, true, __LINE__);
  BasicTest(kSbSystemPathCacheDirectory, true, true, __LINE__);
}

TEST(SbSystemGetPathTest, FailsGracefullyZeroBufferLength) {
  std::vector<char> path(kPathSize, 0);
  bool result = SbSystemGetPath(kSbSystemPathContentDirectory, path.data(), 0);
  EXPECT_FALSE(result);
}

TEST(SbSystemGetPathTest, FailsGracefullyNullBuffer) {
  bool result = SbSystemGetPath(kSbSystemPathContentDirectory, NULL, kPathSize);
  EXPECT_FALSE(result);
}

TEST(SbSystemGetPathTest, FailsGracefullyNullBufferAndZeroLength) {
  bool result = SbSystemGetPath(kSbSystemPathContentDirectory, NULL, 0);
  EXPECT_FALSE(result);
}

TEST(SbSystemGetPathTest, FailsGracefullyBogusId) {
  BasicTest(static_cast<SbSystemPathId>(999), true, false, __LINE__);
}

TEST(SbSystemGetPathTest, DoesNotBlowUpForDefinedIds) {
  BasicTest(kSbSystemPathDebugOutputDirectory, false, false, __LINE__);
  BasicTest(kSbSystemPathTempDirectory, false, false, __LINE__);
  BasicTest(kSbSystemPathTestOutputDirectory, false, false, __LINE__);
  BasicTest(kSbSystemPathCacheDirectory, false, false, __LINE__);
  BasicTest(kSbSystemPathFontDirectory, false, false, __LINE__);
  BasicTest(kSbSystemPathFontConfigurationDirectory, false, false, __LINE__);
}

TEST(SbSystemGetPathTest, DoesNotTouchOutputBufferOnFailureForDefinedIds) {
  UnmodifiedOnFailureTest(kSbSystemPathDebugOutputDirectory, __LINE__);
  UnmodifiedOnFailureTest(kSbSystemPathTempDirectory, __LINE__);
  UnmodifiedOnFailureTest(kSbSystemPathTestOutputDirectory, __LINE__);
  UnmodifiedOnFailureTest(kSbSystemPathCacheDirectory, __LINE__);
  UnmodifiedOnFailureTest(kSbSystemPathFontDirectory, __LINE__);
  UnmodifiedOnFailureTest(kSbSystemPathFontConfigurationDirectory, __LINE__);
}

TEST(SbSystemGetPathTest, CanCreateAndRemoveDirectoryInCache) {
  std::vector<char> path(kPathSize);
  memset(path.data(), 0xCD, kPathSize);
  bool result =
      SbSystemGetPath(kSbSystemPathCacheDirectory, path.data(), kPathSize);
  EXPECT_TRUE(result);
  if (result) {
    EXPECT_NE('\xCD', path[0]);
    // Delete a directory and confirm that it does not exist.
    std::string sub_path =
        kSbFileSepString + ScopedRandomFile::MakeRandomFilename();
    EXPECT_GT(starboard::strlcat(path.data(), sub_path.c_str(), kPathSize), 0);
    EXPECT_TRUE(SbFileDelete(path.data()));
    EXPECT_FALSE(SbFileExists(path.data()));

    // Create the directory and confirm it exists and can be opened.
    EXPECT_TRUE(SbDirectoryCreate(path.data()));
    EXPECT_TRUE(SbFileExists(path.data()));
    EXPECT_TRUE(SbDirectoryCanOpen(path.data()));
    SbDirectory directory = SbDirectoryOpen(path.data(), NULL);
    EXPECT_TRUE(SbDirectoryIsValid(directory));

    // Lastly, close and delete the directory.
    EXPECT_TRUE(SbDirectoryClose(directory));
    EXPECT_TRUE(SbFileDelete(path.data()));
    EXPECT_FALSE(SbFileExists(path.data()));
  }
}

TEST(SbSystemGetPathTest, CanWriteAndReadCache) {
  std::vector<char> path(kPathSize);
  memset(path.data(), 0xCD, kPathSize);
  bool result =
      SbSystemGetPath(kSbSystemPathCacheDirectory, path.data(), kPathSize);
  EXPECT_TRUE(result);
  if (result) {
    EXPECT_NE('\xCD', path[0]);
    int len = static_cast<int>(strlen(path.data()));
    EXPECT_GT(len, 0);
    // Delete a file and confirm that it does not exist.
    std::string sub_path =
        kSbFileSepString + ScopedRandomFile::MakeRandomFilename();
    EXPECT_GT(starboard::strlcat(path.data(), sub_path.c_str(), kPathSize), 0);
    EXPECT_TRUE(SbFileDelete(path.data()));
    EXPECT_FALSE(SbFileExists(path.data()));

    // Write to the file and check that we can read from it.
    std::string content_to_write = "test content";
    {
      starboard::ScopedFile test_file_writer(
          path.data(), kSbFileCreateAlways | kSbFileWrite, NULL, NULL);
      EXPECT_GT(
          test_file_writer.WriteAll(content_to_write.c_str(),
                                    static_cast<int>(content_to_write.size())),
          0);
    }
    EXPECT_TRUE(SbFileExists(path.data()));
    SbFileInfo info;
    EXPECT_TRUE(SbFileGetPathInfo(path.data(), &info));
    const int kFileSize = static_cast<int>(info.size);
    EXPECT_GT(kFileSize, 0);
    const int kBufferLength = 16 * 1024;
    char content_read[kBufferLength] = {0};
    {
      starboard::ScopedFile test_file_reader(
          path.data(), kSbFileOpenOnly | kSbFileRead, NULL, NULL);
      EXPECT_GT(test_file_reader.ReadAll(content_read, kFileSize), 0);
    }
    EXPECT_EQ(content_read, content_to_write);

    // Lastly, delete the file.
    EXPECT_TRUE(SbFileDelete(path.data()));
    EXPECT_FALSE(SbFileExists(path.data()));
  }
}

TEST(SbSystemGetPath, ExecutableFileCreationTimeIsSound) {
  // Verify that the creation time of the current executable file is not
  // greater than the current time.
  std::vector<char> path(kPathSize);
  memset(path.data(), 0xCD, kPathSize);
  bool result =
      SbSystemGetPath(kSbSystemPathExecutableFile, path.data(), kPathSize);
  ASSERT_TRUE(result);

  EXPECT_NE('\xCD', path[0]);
  int len = static_cast<int>(strlen(path.data()));
  EXPECT_GT(len, 0);

  SbFileInfo executable_file_info;
  result = SbFileGetPathInfo(path.data(), &executable_file_info);
  ASSERT_TRUE(result);

  SbTime now = SbTimeGetNow();
  EXPECT_GT(now, executable_file_info.creation_time);
}

}  // namespace
}  // namespace nplb
}  // namespace starboard
