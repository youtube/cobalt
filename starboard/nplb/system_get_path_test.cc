// Copyright 2015 Google Inc. All Rights Reserved.
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

#include "starboard/file.h"
#include "starboard/memory.h"
#include "starboard/nplb/file_helpers.h"
#include "starboard/string.h"
#include "starboard/system.h"
#include "starboard/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {
namespace {

// Size of appropriate path buffer.
const size_t kPathSize = SB_FILE_MAX_PATH + 1;

void BasicTest(SbSystemPathId id,
               bool expect_result,
               bool expected_result,
               int line) {
#define LOCAL_CONTEXT "Context : id=" << id << ", line=" << line;
  char path[kPathSize];
  SbMemorySet(path, 0xCD, kPathSize);
  bool result = SbSystemGetPath(id, path, kPathSize);
  if (expect_result) {
    EXPECT_EQ(expected_result, result) << LOCAL_CONTEXT;
  }
  if (result) {
    EXPECT_NE('\xCD', path[0]) << LOCAL_CONTEXT;
    int len = static_cast<int>(SbStringGetLength(path));
    EXPECT_GT(len, 0) << LOCAL_CONTEXT;
  } else {
    EXPECT_EQ('\xCD', path[0]) << LOCAL_CONTEXT;
  }
#undef LOCAL_CONTEXT
}

TEST(SbSystemGetPathTest, ReturnsRequiredPaths) {
  BasicTest(kSbSystemPathContentDirectory, true, true, __LINE__);
  BasicTest(kSbSystemPathCacheDirectory, true, true, __LINE__);
}

TEST(SbSystemGetPathTest, FailsGracefullyZeroBufferLength) {
  char path[kPathSize] = {0};
  bool result = SbSystemGetPath(kSbSystemPathContentDirectory, path, 0);
  EXPECT_FALSE(result);
}

TEST(SbSystemGetPathTest, FailsGracefullyNullBuffer) {
  char path[kPathSize] = {0};
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
  BasicTest(kSbSystemPathSourceDirectory, false, false, __LINE__);
  BasicTest(kSbSystemPathTempDirectory, false, false, __LINE__);
  BasicTest(kSbSystemPathTestOutputDirectory, false, false, __LINE__);
  BasicTest(kSbSystemPathCacheDirectory, false, false, __LINE__);
  BasicTest(kSbSystemPathFontDirectory, false, false, __LINE__);
  BasicTest(kSbSystemPathFontConfigurationDirectory, false, false, __LINE__);
}

TEST(SbSystemGetPathTest, CanCreateAndRemoveDirectoryInCache) {
  char path[kPathSize];
  SbMemorySet(path, 0xCD, kPathSize);
  bool result = SbSystemGetPath(kSbSystemPathCacheDirectory, path, kPathSize);
  EXPECT_TRUE(result);
  if (result) {
    EXPECT_NE('\xCD', path[0]);
    // Delete a directory and confirm that it does not exist.
    std::string sub_path =
        SB_FILE_SEP_STRING + ScopedRandomFile::MakeRandomFilename();
    EXPECT_GT(SbStringConcat(path, sub_path.c_str(), kPathSize), 0);
    EXPECT_TRUE(SbFileDelete(path));
    EXPECT_FALSE(SbFileExists(path));

    // Create the directory and confirm it exists and can be opened.
    EXPECT_TRUE(SbDirectoryCreate(path));
    EXPECT_TRUE(SbFileExists(path));
    EXPECT_TRUE(SbDirectoryCanOpen(path));
    SbDirectory directory = SbDirectoryOpen(path, NULL);
    EXPECT_TRUE(SbDirectoryIsValid(directory));

    // Lastly, close and delete the directory.
    EXPECT_TRUE(SbDirectoryClose(directory));
    EXPECT_TRUE(SbFileDelete(path));
    EXPECT_FALSE(SbFileExists(path));
  }
}

TEST(SbSystemGetPathTest, CanWriteAndReadCache) {
  char path[kPathSize];
  SbMemorySet(path, 0xCD, kPathSize);
  bool result = SbSystemGetPath(kSbSystemPathCacheDirectory, path, kPathSize);
  EXPECT_TRUE(result);
  if (result) {
    EXPECT_NE('\xCD', path[0]);
    int len = static_cast<int>(SbStringGetLength(path));
    EXPECT_GT(len, 0);
    // Delete a file and confirm that it does not exist.
    std::string sub_path =
        SB_FILE_SEP_STRING + ScopedRandomFile::MakeRandomFilename();
    EXPECT_GT(SbStringConcat(path, sub_path.c_str(), kPathSize), 0);
    EXPECT_TRUE(SbFileDelete(path));
    EXPECT_FALSE(SbFileExists(path));

    // Write to the file and check that we can read from it.
    std::string content_to_write = "test content";
    {
      starboard::ScopedFile test_file_writer(
          path, kSbFileCreateAlways | kSbFileWrite, NULL, NULL);
      EXPECT_GT(
          test_file_writer.WriteAll(content_to_write.c_str(),
                                    static_cast<int>(content_to_write.size())),
          0);
    }
    EXPECT_TRUE(SbFileExists(path));
    SbFileInfo info;
    EXPECT_TRUE(SbFileGetPathInfo(path, &info));
    const int kFileSize = static_cast<int>(info.size);
    EXPECT_GT(kFileSize, 0);
    const int kBufferLength = 16 * 1024;
    char content_read[kBufferLength] = {0};
    {
      starboard::ScopedFile test_file_reader(
          path, kSbFileOpenOnly | kSbFileRead, NULL, NULL);
      EXPECT_GT(test_file_reader.ReadAll(content_read, kFileSize), 0);
    }
    EXPECT_EQ(content_read, content_to_write);

    // Lastly, delete the file.
    EXPECT_TRUE(SbFileDelete(path));
    EXPECT_FALSE(SbFileExists(path));
  }
}

TEST(SbSystemGetPath, ExecutableFileCreationTimeIsSound) {
  // Verify that the creation time of the current executable file is not
  // greater than the current time.
  char path[kPathSize];
  SbMemorySet(path, 0xCD, kPathSize);
  bool result = SbSystemGetPath(kSbSystemPathExecutableFile, path, kPathSize);
  ASSERT_TRUE(result);

  EXPECT_NE('\xCD', path[0]);
  int len = static_cast<int>(SbStringGetLength(path));
  EXPECT_GT(len, 0);

  SbFileInfo executable_file_info;
  result = SbFileGetPathInfo(path, &executable_file_info);
  ASSERT_TRUE(result);

  SbTime now = SbTimeGetNow();
  EXPECT_GT(now, executable_file_info.creation_time);
}

}  // namespace
}  // namespace nplb
}  // namespace starboard
