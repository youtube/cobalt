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
  - EAGAIN: Insufficient memory resources to lock a private page.
  - ENOTSUP: Hard to reliably create combination of flags that are unsupported.
*/

#include <sys/mman.h>
#include <cerrno>

#include "starboard/configuration_constants.h"
#include "starboard/nplb/file_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard::nplb {
namespace {

class PosixMprotectTest : public ::testing::Test {
 protected:
  PosixMprotectTest()
      : mapping_size_(3 * kSbMemoryPageSize), scoped_file_(mapping_size_) {}

  void SetUp() override {
    fd_ = open(scoped_file_.filename().c_str(), O_RDWR);
    ASSERT_NE(fd_, -1);
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

TEST_F(PosixMprotectTest, SuccessSetsProtNone) {
  int result = mprotect(mapped_memory_, kSbMemoryPageSize, PROT_NONE);
  EXPECT_EQ(result, 0) << "mprotect failed: " << strerror(errno);
}

TEST_F(PosixMprotectTest, SuccessSetsProtRead) {
  int result = mprotect(mapped_memory_, kSbMemoryPageSize, PROT_READ);
  EXPECT_EQ(result, 0) << "mprotect failed: " << strerror(errno);
  volatile char c = static_cast<char*>(mapped_memory_)[0];
  (void)c;
}

TEST_F(PosixMprotectTest, SuccessSetsProtWrite) {
  int result = mprotect(mapped_memory_, kSbMemoryPageSize, PROT_WRITE);
  EXPECT_EQ(result, 0) << "mprotect failed: " << strerror(errno);
  *static_cast<volatile char*>(mapped_memory_) = 'a';
}

TEST_F(PosixMprotectTest, SuccessSetsProtReadWrite) {
  int result =
      mprotect(mapped_memory_, kSbMemoryPageSize, PROT_READ | PROT_WRITE);
  EXPECT_EQ(result, 0) << "mprotect failed: " << strerror(errno);
  *static_cast<volatile char*>(mapped_memory_) = 'b';
  volatile char c = static_cast<char*>(mapped_memory_)[0];
  EXPECT_EQ(c, 'b');
}

TEST_F(PosixMprotectTest, FailsWithUnmappedAddress) {
  // Create a hole in the middle of our 3-page mapping.
  char* middle_page = static_cast<char*>(mapped_memory_) + kSbMemoryPageSize;
  ASSERT_EQ(munmap(middle_page, kSbMemoryPageSize), 0)
      << "munmap failed: " << strerror(errno);

  // mprotect on the entire original range should now fail.
  int result = mprotect(mapped_memory_, mapping_size_, PROT_READ);
  EXPECT_EQ(result, -1);
  EXPECT_EQ(errno, ENOMEM);
}

TEST_F(PosixMprotectTest, FailsWithReadOnlyFile) {
  // Close the read-write file descriptor from SetUp.
  close(fd_);
  munmap(mapped_memory_, mapping_size_);

  // Re-open the file as read-only.
  fd_ = open(scoped_file_.filename().c_str(), O_RDONLY);
  ASSERT_NE(fd_, -1);

  // Map it with read-only protection.
  mapped_memory_ = mmap(NULL, mapping_size_, PROT_READ, MAP_SHARED, fd_, 0);
  ASSERT_NE(mapped_memory_, MAP_FAILED);

  // Attempting to upgrade to write access should fail.
  int result = mprotect(mapped_memory_, kSbMemoryPageSize, PROT_WRITE);
  EXPECT_EQ(result, -1);
  EXPECT_EQ(errno, EACCES);
}

TEST_F(PosixMprotectTest, MayFailWithUnalignedAddress) {
  void* unaligned_addr = static_cast<char*>(mapped_memory_) + 1;
  int result = mprotect(unaligned_addr, kSbMemoryPageSize, PROT_READ);
  // This is a "may fail" condition. We just check that if it fails,
  // it's with EINVAL. We don't assert failure.
  if (result == -1) {
    EXPECT_EQ(errno, EINVAL);
  }
}

}  // namespace
}  // namespace starboard::nplb
