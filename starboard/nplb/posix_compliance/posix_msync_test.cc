// Copyright 2025 The Cobalt Authors. All Rights Reserved.
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

/*
  The following error conditions are not tested due to the difficulty of
  reliably triggering them in a portable unit testing environment:
  - EBUSY: An address in the range is locked and MS_INVALIDATE was specified.
*/

#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>
#include <string>
#include <vector>

#include "starboard/common/file.h"
#include "starboard/configuration_constants.h"
#include "starboard/nplb/file_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace nplb {
namespace {

class PosixMsyncTest : public ::testing::Test {
 protected:
  PosixMsyncTest()
      : mapping_size_(3 * kSbMemoryPageSize), scoped_file_(mapping_size_) {}

  void SetUp() override {
    fd_ = open(scoped_file_.filename().c_str(), O_RDWR);
    ASSERT_NE(fd_, -1) << "Failed to open file: " << strerror(errno);

    mapped_memory_ =
        mmap(NULL, mapping_size_, PROT_READ | PROT_WRITE, MAP_SHARED, fd_, 0);
    ASSERT_NE(mapped_memory_, MAP_FAILED);
  }

  void TearDown() override {
    if (mapped_memory_ != MAP_FAILED) {
      munmap(mapped_memory_, mapping_size_);
    }
    if (fd_ != -1) {
      close(fd_);
    }
  }

  int fd_ = -1;
  const size_t mapping_size_;
  void* mapped_memory_ = MAP_FAILED;
  ScopedRandomFile scoped_file_;
};

TEST_F(PosixMsyncTest, SyncWritesDataToFile) {
  char* data = static_cast<char*>(mapped_memory_);
  data[0] = 'B';
  data[kSbMemoryPageSize - 1] = 'C';

  // Synchronize memory with the file.
  int result = msync(mapped_memory_, kSbMemoryPageSize, MS_SYNC);
  ASSERT_EQ(result, 0) << "msync failed: " << strerror(errno);

  // Verify the file on disk contains the new data.
  std::vector<char> file_content(kSbMemoryPageSize);
  ASSERT_EQ(lseek(fd_, 0, SEEK_SET), 0);
  ssize_t readres = read(fd_, file_content.data(), file_content.size());
  ASSERT_EQ(static_cast<size_t>(readres), kSbMemoryPageSize);
  EXPECT_EQ(file_content[0], 'B');
  EXPECT_EQ(file_content[kSbMemoryPageSize - 1], 'C');
}

TEST_F(PosixMsyncTest, AsyncSyncReturnsSuccess) {
  char* data = static_cast<char*>(mapped_memory_);
  data[0] = 'D';

  // Asynchronously synchronize. The test only verifies that the call returns
  // success, as waiting for an async write will take an unpredictable
  // amount of time.
  int result = msync(mapped_memory_, kSbMemoryPageSize, MS_ASYNC);
  EXPECT_EQ(result, 0) << "msync failed: " << strerror(errno);
}

TEST_F(PosixMsyncTest, InvalidateUpdatesCache) {
  // Write new data directly to the file.
  const char new_data[] = "XYZ";
  ASSERT_EQ(lseek(fd_, 0, SEEK_SET), 0u);
  ssize_t write_res = write(fd_, new_data, strlen(new_data));
  ASSERT_EQ(static_cast<unsigned long>(write_res), strlen(new_data));

  // Sync and invalidate the cache.
  int result =
      msync(mapped_memory_, kSbMemoryPageSize, MS_SYNC | MS_INVALIDATE);
  ASSERT_EQ(result, 0) << "msync failed: " << strerror(errno);

  // Subsequent read from memory should see the new data.
  char* mapped_data = static_cast<char*>(mapped_memory_);
  EXPECT_EQ(mapped_data[0], 'X');
  EXPECT_EQ(mapped_data[1], 'Y');
  EXPECT_EQ(mapped_data[2], 'Z');
}

TEST_F(PosixMsyncTest, FailsWithInvalidFlags) {
  // MS_SYNC and MS_ASYNC are mutually exclusive.
  int result = msync(mapped_memory_, kSbMemoryPageSize, MS_SYNC | MS_ASYNC);
  EXPECT_EQ(result, -1);
  EXPECT_EQ(errno, EINVAL);
}

TEST_F(PosixMsyncTest, FailsWithUnmappedAddressInMiddle) {
  // Create a hole in the middle of our 3-page mapping.
  char* middle_page = static_cast<char*>(mapped_memory_) + kSbMemoryPageSize;
  ASSERT_EQ(munmap(middle_page, kSbMemoryPageSize), 0);

  // msync on the entire original range should now fail.
  int result = msync(mapped_memory_, mapping_size_, MS_SYNC);
  EXPECT_EQ(result, -1);
  EXPECT_EQ(errno, ENOMEM);
}

// POSIX specifies that msync MAY fail if addr is not page-aligned.
TEST_F(PosixMsyncTest, MayFailWithUnalignedAddress) {
  void* unaligned_addr = static_cast<char*>(mapped_memory_) + 1;
  int result = msync(unaligned_addr, kSbMemoryPageSize - 1, MS_SYNC);
  if (result == -1) {
    EXPECT_EQ(errno, EINVAL);
  }
}

}  // namespace
}  // namespace nplb
