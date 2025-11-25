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

#include <dirent.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>

#include "starboard/common/file.h"
#include "starboard/common/string.h"
#include "starboard/common/time.h"
#include "starboard/configuration_constants.h"
#include "starboard/nplb/file_helpers.h"
#include "starboard/system.h"
#include "testing/gtest/include/gtest/gtest.h"

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
  BasicTest(kSbSystemPathCacheDirectory, false, false, __LINE__);
  BasicTest(kSbSystemPathFontDirectory, false, false, __LINE__);
  BasicTest(kSbSystemPathFontConfigurationDirectory, false, false, __LINE__);
}

TEST(SbSystemGetPathTest, DoesNotTouchOutputBufferOnFailureForDefinedIds) {
  UnmodifiedOnFailureTest(kSbSystemPathDebugOutputDirectory, __LINE__);
  UnmodifiedOnFailureTest(kSbSystemPathTempDirectory, __LINE__);
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
    // rmdir return -1 when directory does not exist.
    EXPECT_TRUE(rmdir(path.data()) == 0 || !DirectoryExists(path.data()));
    EXPECT_FALSE(FileExists(path.data()));

    // Create the directory and confirm it exists and can be opened.
    EXPECT_TRUE(mkdir(path.data(), 0700) == 0 ||
                (DirectoryExists(path.data())));
    EXPECT_TRUE(FileExists(path.data()));
    EXPECT_TRUE(DirectoryExists(path.data()));
    DIR* directory = opendir(path.data());
    EXPECT_TRUE(directory);

    // Lastly, close and delete the directory.
    EXPECT_TRUE(closedir(directory) == 0);
    EXPECT_TRUE(rmdir(path.data()) == 0);
    EXPECT_FALSE(FileExists(path.data()));
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
    // unlink return -1 when directory does not exist.
    EXPECT_TRUE(unlink(path.data()) == 0 || !FileExists(path.data()));
    EXPECT_FALSE(FileExists(path.data()));

    // Write to the file and check that we can read from it.
    std::string content_to_write = "test content";
    {
      starboard::ScopedFile test_file_writer(path.data(),
                                             O_CREAT | O_TRUNC | O_WRONLY);
      EXPECT_GT(
          test_file_writer.WriteAll(content_to_write.c_str(),
                                    static_cast<int>(content_to_write.size())),
          0);
    }
    EXPECT_TRUE(FileExists(path.data()));
    struct stat info;
    EXPECT_TRUE(stat(path.data(), &info) == 0);
    const int kFileSize = static_cast<int>(info.st_size);
    EXPECT_GT(kFileSize, 0);
    const int kBufferLength = 16 * 1024;
    char content_read[kBufferLength] = {0};
    {
      starboard::ScopedFile test_file_reader(path.data(), 0);
      EXPECT_GT(test_file_reader.ReadAll(content_read, kFileSize), 0);
    }
    EXPECT_EQ(content_read, content_to_write);

    // Lastly, delete the file.
    EXPECT_TRUE(unlink(path.data()) == 0);
    EXPECT_FALSE(FileExists(path.data()));
  }
}

// Copied from SbSystemGetPathTest.CanWriteAndReadCache.
TEST(SbSystemGetPathTest, CanWriteAndReadFilesStorage) {
  std::vector<char> path(kPathSize);
  memset(path.data(), 0xCD, kPathSize);
  bool result =
      SbSystemGetPath(kSbSystemPathFilesDirectory, path.data(), kPathSize);
  EXPECT_TRUE(result);
  if (result) {
    EXPECT_NE('\xCD', path[0]);
    int len = static_cast<int>(strlen(path.data()));
    EXPECT_GT(len, 0);
    // Delete a file and confirm that it does not exist.
    std::string sub_path =
        kSbFileSepString + ScopedRandomFile::MakeRandomFilename();
    EXPECT_GT(starboard::strlcat(path.data(), sub_path.c_str(), kPathSize), 0);
    // unlink return -1 when directory does not exist.
    EXPECT_TRUE(unlink(path.data()) == 0 || !FileExists(path.data()));
    EXPECT_FALSE(FileExists(path.data()));

    // Write to the file and check that we can read from it.
    std::string content_to_write = "test content";
    {
      starboard::ScopedFile test_file_writer(path.data(),
                                             O_CREAT | O_TRUNC | O_WRONLY);
      EXPECT_GT(
          test_file_writer.WriteAll(content_to_write.c_str(),
                                    static_cast<int>(content_to_write.size())),
          0);
    }
    EXPECT_TRUE(FileExists(path.data()));
    struct stat info;
    EXPECT_TRUE(stat(path.data(), &info) == 0);
    const int kFileSize = static_cast<int>(info.st_size);
    EXPECT_GT(kFileSize, 0);
    const int kBufferLength = 16 * 1024;
    char content_read[kBufferLength] = {0};
    {
      starboard::ScopedFile test_file_reader(path.data(), 0);
      EXPECT_GT(test_file_reader.ReadAll(content_read, kFileSize), 0);
    }
    EXPECT_EQ(content_read, content_to_write);

    // Lastly, delete the file.
    EXPECT_TRUE(unlink(path.data()) == 0);
    EXPECT_FALSE(FileExists(path.data()));
  }
}

constexpr int64_t kMicrosecond = 1'000'000;
auto ToMicroseconds(const struct timespec& ts) {
  return ts.tv_sec * kMicrosecond + ts.tv_nsec / 1000;
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

  struct stat executable_file_info;
  result = stat(path.data(), &executable_file_info);
  ASSERT_EQ(0, result);

  int64_t now_usec = starboard::CurrentPosixTime();
  EXPECT_GT(now_usec, ToMicroseconds(executable_file_info.st_ctim));
}

}  // namespace
}  // namespace nplb
