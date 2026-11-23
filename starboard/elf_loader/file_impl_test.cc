// Copyright 2026 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/elf_loader/file_impl.h"

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cstring>
#include <string>
#include <vector>

#include "starboard/common/file.h"
#include "starboard/configuration_constants.h"
#include "starboard/system.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace elf_loader {
namespace {

constexpr char kContainer[] = "HEADER_PAYLOAD_TRAILER";
constexpr int kContainerSize = sizeof(kContainer) - 1;

// Opens a file and then narrows it to |size| bytes at |offset|, the way
// OpenFileInPlace() does for an asset inside an APK.
class SliceFileImpl : public FileImpl {
 public:
  bool OpenSlice(const char* path, int64_t offset, int64_t size) {
    if (!FileImpl::Open(path)) {
      return false;
    }
    offset_ = offset;
    size_ = size;
    return true;
  }
};

class FileImplTest : public ::testing::Test {
 protected:
  void SetUp() override {
    std::vector<char> temp_dir(kSbFileMaxPath + 1);
    ASSERT_TRUE(SbSystemGetPath(kSbSystemPathTempDirectory, temp_dir.data(),
                                kSbFileMaxPath + 1));
    path_ =
        std::string(temp_dir.data()) + kSbFileSepChar + "file_impl_test.bin";
    starboard::ScopedFile out(path_.c_str(), O_CREAT | O_TRUNC | O_WRONLY,
                              S_IRUSR | S_IWUSR);
    ASSERT_TRUE(out.IsValid());
    ASSERT_EQ(out.WriteAll(kContainer, kContainerSize), kContainerSize);
  }

  void TearDown() override { unlink(path_.c_str()); }

  std::string path_;
};

TEST_F(FileImplTest, ReadBeforeOpeningFails) {
  FileImpl file;
  char buffer[4];
  EXPECT_FALSE(file.ReadFromOffset(0, buffer, sizeof(buffer)));
}

TEST_F(FileImplTest, ReadStopsAtEndOfFile) {
  FileImpl file;
  ASSERT_TRUE(file.Open(path_.c_str()));

  char buffer[32];
  memset(buffer, 'X', sizeof(buffer));
  EXPECT_TRUE(file.ReadFromOffset(7, buffer, sizeof(buffer)));
  EXPECT_EQ(std::string(buffer, kContainerSize - 7), "PAYLOAD_TRAILER");
  EXPECT_EQ(buffer[kContainerSize - 7], 'X');
}

TEST_F(FileImplTest, SliceReadStartsAtOffsetAndStopsAtEndOfSlice) {
  SliceFileImpl file;
  ASSERT_TRUE(file.OpenSlice(path_.c_str(), /*offset=*/7, /*size=*/7));

  char buffer[16];
  memset(buffer, 'X', sizeof(buffer));
  // Reading past the end of "PAYLOAD" must not read into "_TRAILER".
  EXPECT_TRUE(file.ReadFromOffset(0, buffer, sizeof(buffer)));
  EXPECT_EQ(std::string(buffer, 7), "PAYLOAD");
  EXPECT_EQ(buffer[7], 'X');

  memset(buffer, 'X', sizeof(buffer));
  EXPECT_TRUE(file.ReadFromOffset(3, buffer, 2));
  EXPECT_EQ(std::string(buffer, 2), "LO");
  EXPECT_EQ(buffer[2], 'X');

  // Reading at the end of the slice reads nothing.
  EXPECT_TRUE(file.ReadFromOffset(7, buffer, 1));
}

TEST_F(FileImplTest, SliceReadOutOfRangeFails) {
  SliceFileImpl file;
  ASSERT_TRUE(file.OpenSlice(path_.c_str(), /*offset=*/7, /*size=*/7));

  char buffer[4];
  EXPECT_FALSE(file.ReadFromOffset(-1, buffer, 1));
  EXPECT_FALSE(file.ReadFromOffset(0, buffer, -1));
  EXPECT_FALSE(file.ReadFromOffset(8, buffer, 1));
}

TEST_F(FileImplTest, ReadAfterCloseFails) {
  FileImpl file;
  ASSERT_TRUE(file.Open(path_.c_str()));
  file.Close();

  char buffer[4];
  EXPECT_FALSE(file.ReadFromOffset(0, buffer, sizeof(buffer)));
}

}  // namespace
}  // namespace elf_loader
