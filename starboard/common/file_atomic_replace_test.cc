// Copyright 2019 The Cobalt Authors. All Rights Reserved.
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

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <string>
#include <vector>

#include "starboard/common/file.h"
#include "starboard/configuration_constants.h"
#include "starboard/system.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace {

std::string GetTempDir() {
  std::vector<char> path(kSbFileMaxPath + 1, 0);
  if (!SbSystemGetPath(kSbSystemPathTempDirectory, path.data(), path.size())) {
    return "";
  }
  return path.data();
}

std::string MakeRandomFilename() {
  static int counter = 0;
  char filename[128];
  snprintf(filename, sizeof(filename), "file_atomic_replace_test_%d_%d",
           getpid(), counter++);
  return filename;
}

std::string MakeRandomFilePath() {
  std::string temp_dir = GetTempDir();
  if (temp_dir.empty()) {
    return "";
  }
  return temp_dir + kSbFileSepChar + MakeRandomFilename();
}

std::string MakeRandomFile(int length) {
  std::string filename = MakeRandomFilePath();
  if (filename.empty()) {
    return "";
  }

  int file =
      open(filename.c_str(), O_CREAT | O_EXCL | O_WRONLY, S_IRUSR | S_IWUSR);
  EXPECT_GE(file, 0);
  if (file < 0) {
    return "";
  }

  std::vector<char> data(length);
  for (int i = 0; i < length; ++i) {
    data[i] = static_cast<char>(i & 0xFF);
  }

  ssize_t bytes = starboard::WriteAll(file, data.data(), length);
  EXPECT_EQ(bytes, length);

  close(file);
  return filename;
}

class ScopedRandomFile {
 public:
  enum Flags {
    kDontCreate = 0,
    kCreate = 1,
  };
  static const int kDefaultLength = 10;

  explicit ScopedRandomFile(Flags flags = kCreate)
      : ScopedRandomFile(kDefaultLength, flags) {}

  ScopedRandomFile(int length, Flags flags) {
    if (flags == kCreate) {
      filename_ = MakeRandomFile(length);
    } else {
      filename_ = MakeRandomFilePath();
    }
  }

  ~ScopedRandomFile() {
    if (!filename_.empty()) {
      unlink(filename_.c_str());
    }
  }

  const std::string& filename() const { return filename_; }

 private:
  std::string filename_;
};

bool FileExists(const char* path) {
  struct stat info;
  return stat(path, &info) == 0;
}

static const char kTestContents[] =
    "The quick brown fox jumps over the lazy dog.";
static const int kTestContentsLength = sizeof(kTestContents);

bool CompareFileContentsToString(const char* filename,
                                 const char* str,
                                 int size) {
  char result[kTestContentsLength] = {'\0'};

  int file = open(filename, O_RDONLY);

  EXPECT_TRUE(starboard::IsValid(file));

  // We always try to read kTestContentsLength since the data will at most be
  // this long. There are test cases where the number of bytes read will be
  // less.
  EXPECT_EQ(size, starboard::ReadAll(file, result, kTestContentsLength));
  EXPECT_TRUE(!close(file));

  return strncmp(str, result, kTestContentsLength) == 0;
}

TEST(FileAtomicReplaceTest, ReplacesValidFile) {
  ScopedRandomFile random_file(ScopedRandomFile::kDefaultLength,
                               ScopedRandomFile::kCreate);
  const std::string& filename = random_file.filename();

  EXPECT_TRUE(FileExists(filename.c_str()));
  EXPECT_TRUE(
      FileAtomicReplace(filename.c_str(), kTestContents, kTestContentsLength));
  EXPECT_TRUE(CompareFileContentsToString(filename.c_str(), kTestContents,
                                          kTestContentsLength));
}

TEST(FileAtomicReplaceTest, ReplacesNonExistentFile) {
  ScopedRandomFile random_file(ScopedRandomFile::kDontCreate);
  const std::string& filename = random_file.filename();

  EXPECT_FALSE(FileExists(filename.c_str()));
  EXPECT_TRUE(
      FileAtomicReplace(filename.c_str(), kTestContents, kTestContentsLength));
  EXPECT_TRUE(CompareFileContentsToString(filename.c_str(), kTestContents,
                                          kTestContentsLength));
}

TEST(FileAtomicReplaceTest, ReplacesWithNoData) {
  ScopedRandomFile random_file(ScopedRandomFile::kCreate);
  const std::string& filename = random_file.filename();

  EXPECT_TRUE(FileExists(filename.c_str()));
  EXPECT_TRUE(FileAtomicReplace(filename.c_str(), nullptr, 0));
  EXPECT_TRUE(CompareFileContentsToString(filename.c_str(), "\0", 0));
}

TEST(FileAtomicReplaceTest, FailsWithNoDataButLength) {
  ScopedRandomFile random_file(ScopedRandomFile::kCreate);
  const std::string& filename = random_file.filename();

  EXPECT_TRUE(FileExists(filename.c_str()));
  EXPECT_FALSE(FileAtomicReplace(filename.c_str(), nullptr, 1));
}

TEST(FileAtomicReplaceTest, FailsWithInvalidLength) {
  ScopedRandomFile random_file(ScopedRandomFile::kCreate);
  const std::string& filename = random_file.filename();

  EXPECT_TRUE(FileExists(filename.c_str()));
  EXPECT_FALSE(FileAtomicReplace(filename.c_str(), kTestContents, -1));
}

}  // namespace
}  // namespace starboard
