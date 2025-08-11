// Copyright 2024 The Cobalt Authors. All Rights Reserved.
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
  The following mmap error conditions are not tested due to the difficulty of
  reliably triggering them in a portable unit testing environment:

  - EAGAIN: Could not lock the mapping in memory due to a lack of resources.
  - EMFILE: The number of mapped regions would exceed a process or system limit.
  - ENOMEM: Not enough memory, or the address space is full.
  - ENOTSUP: The implementation does not support a requested feature.
  - EOVERFLOW: The values of `off` plus `len` exceed the maximum file size.
*/

#include <sys/mman.h>

#include "starboard/common/memory.h"
#include "starboard/configuration_constants.h"
#include "starboard/nplb/file_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard::nplb {
namespace {

const size_t kSize = kSbMemoryPageSize * 8;
const void* kFailed = MAP_FAILED;

TEST(PosixMemoryMapTest, AllocatesNormally) {
  void* memory = mmap(nullptr, kSize, PROT_READ, MAP_PRIVATE | MAP_ANON, -1, 0);
  ASSERT_NE(kFailed, memory);
  EXPECT_EQ(munmap(memory, kSize), 0);
}

TEST(PosixMemoryMapTest, FailsWithZeroLength) {
  errno = 0;
  void* memory = mmap(nullptr, 0, PROT_READ, MAP_PRIVATE | MAP_ANON, -1, 0);
  EXPECT_EQ(kFailed, memory);
  EXPECT_EQ(EINVAL, errno);
}

TEST(PosixMemoryMapTest, AllocatesOne) {
  void* memory = mmap(nullptr, 1, PROT_READ, MAP_PRIVATE | MAP_ANON, -1, 0);
  ASSERT_NE(kFailed, memory);
  EXPECT_EQ(munmap(memory, 1), 0);
}

TEST(PosixMemoryMapTest, AllocatesOnePage) {
  void* memory = mmap(nullptr, kSbMemoryPageSize, PROT_READ,
                      MAP_PRIVATE | MAP_ANON, -1, 0);
  ASSERT_NE(kFailed, memory);
  EXPECT_EQ(munmap(memory, kSbMemoryPageSize), 0);
}

TEST(PosixMemoryMapTest, CanReadWriteToResult) {
  void* memory = mmap(nullptr, kSize, PROT_READ | PROT_WRITE,
                      MAP_PRIVATE | MAP_ANON, -1, 0);
  ASSERT_NE(kFailed, memory);
  char* data = static_cast<char*>(memory);
  for (size_t i = 0; i < kSize; ++i) {
    data[i] = static_cast<char>(i);
  }

  for (size_t i = 0; i < kSize; ++i) {
    EXPECT_EQ(data[i], static_cast<char>(i));
  }

  EXPECT_EQ(munmap(memory, kSize), 0);
}

typedef int (*SumFunction)(int, int);
static int Sum(int x, int y) {
  return x + y;
}
static int Sum2(int x, int y) {
  return x + y + x;
}

// Copy the contents of |Sum| into |memory|, returning an assertion result
// indicating whether the copy was successful, a function pointer to the
// possibly copied |Sum| function data and a function pointer to the selected
// original function..  Clients are expected to immediately call |ASSERT| on
// the assertion result.
std::tuple<::testing::AssertionResult, SumFunction, SumFunction>
CopySumFunctionIntoMemory(void* memory) {
  // There's no reliable way to determine the size of the 'Sum' function. If we
  // assume the function is at most a certain size, then we might try to read
  // beyond mapped memory when copying it to the destination address. We can
  // get a reasonable upper bound by assuming that the function's implementation
  // does not cross a page boundary, and copy the memory from the beginning of
  // the function to the end of the page that it is mapped to.
  //
  // However, since it's possible the function may cross the page boundary, we
  // define two functions and use the one closest to the start of a page. There
  // is no guarantee that the linker will place these definitions sequentially
  // (although it likely will), so we can't use the address of 'Sum2' as the
  // end of 'Sum'.
  //
  // To avoid the possibility that COMDAT folding will combine these two
  // definitions into one, make sure they are different.

  // A function pointer can't be cast to void*, but uint8* seems to be okay. So
  // cast to a uint* which will be implicitly casted to a void* below.
  SumFunction original_function = &Sum;
  if (reinterpret_cast<uintptr_t>(&Sum2) % kSbMemoryPageSize <
      reinterpret_cast<uintptr_t>(&Sum) % kSbMemoryPageSize) {
    original_function = &Sum2;
  }

  uint8_t* sum_function_start = reinterpret_cast<uint8_t*>(original_function);

  // MIPS16 instruction are kept odd addresses to differentiate between that and
  // MIPS32 instructions. Most other instructions are aligned to at least even
  // addresses, so this code should do nothing for those architectures.
  // https://www.linux-mips.org/wiki/MIPS16
  ptrdiff_t sum_function_offset =
      sum_function_start -
      reinterpret_cast<uint8_t*>(
          reinterpret_cast<uintptr_t>(sum_function_start) & ~0x1);
  sum_function_start -= sum_function_offset;

  // Get the last address of the page that |sum_function_start| is on.
  uint8_t* sum_function_page_end = reinterpret_cast<uint8_t*>(
      (reinterpret_cast<uintptr_t>(sum_function_start) / kSbMemoryPageSize) *
          kSbMemoryPageSize +
      kSbMemoryPageSize);
  if (!starboard::common::MemoryIsAligned(sum_function_page_end,
                                          kSbMemoryPageSize)) {
    return std::make_tuple(::testing::AssertionFailure()
                               << "Expected |Sum| page end ("
                               << static_cast<void*>(sum_function_page_end)
                               << ") to be aligned to " << kSbMemoryPageSize,
                           nullptr, nullptr);
  }
  if (sum_function_start >= sum_function_page_end) {
    return std::make_tuple(::testing::AssertionFailure()
                               << "Expected |Sum| function page start ("
                               << static_cast<void*>(sum_function_start)
                               << ") to be less than |Sum| function page end ("
                               << static_cast<void*>(sum_function_page_end)
                               << ")",
                           nullptr, nullptr);
  }

  size_t bytes_to_copy = sum_function_page_end - sum_function_start;
  if (bytes_to_copy > kSbMemoryPageSize) {
    return std::make_tuple(
        ::testing::AssertionFailure()
            << "Expected bytes required to copy |Sum| to be less than "
            << kSbMemoryPageSize << ", Actual: " << bytes_to_copy,
        nullptr, nullptr);
  }

  memcpy(memory, sum_function_start, bytes_to_copy);

  SumFunction mapped_function = reinterpret_cast<SumFunction>(
      reinterpret_cast<uint8_t*>(memory) + sum_function_offset);

  return std::make_tuple(::testing::AssertionSuccess(), mapped_function,
                         original_function);
}

TEST(PosixMemoryMapTest, CanChangeMemoryProtection) {
  int all_from_flags[] = {
      PROT_NONE,
      PROT_READ,
      PROT_WRITE,
      PROT_READ | PROT_WRITE,
  };
  int all_to_flags[] = {
      PROT_NONE,  PROT_READ,
      PROT_WRITE, PROT_READ | PROT_WRITE,
      PROT_EXEC,  PROT_READ | PROT_EXEC,
  };

  for (int from_flags : all_from_flags) {
    for (int to_flags : all_to_flags) {
      void* memory =
          mmap(nullptr, kSize, from_flags, MAP_PRIVATE | MAP_ANON, -1, 0);
      // If the platform does not support a particular protection flags
      // configuration in the first place, then just give them a pass, knowing
      // that that feature will be tested elsewhere.
      if (memory == MAP_FAILED) {
        continue;
      }

      SumFunction mapped_function = nullptr;
      SumFunction original_function = nullptr;
      if (kSbCanMapExecutableMemory) {
        // We can only test the ability to execute memory after changing
        // protections if we have write permissions either now or then, because
        // we have to actually put a valid function into the mapped memory.
        if (from_flags & PROT_WRITE) {
          auto copy_sum_function_result = CopySumFunctionIntoMemory(memory);
          ASSERT_TRUE(std::get<0>(copy_sum_function_result));
          mapped_function = std::get<1>(copy_sum_function_result);
          original_function = std::get<2>(copy_sum_function_result);
        }
      }

      if (mprotect(memory, kSize, to_flags) != 0) {
        EXPECT_EQ(munmap(memory, kSize), 0);
        continue;
      }

      if (kSbCanMapExecutableMemory) {
        if ((to_flags & PROT_EXEC) && mapped_function != nullptr) {
          EXPECT_EQ(original_function(0xc0ba178, 0xbadf00d),
                    mapped_function(0xc0ba178, 0xbadf00d));
        }
      }

      if (to_flags & PROT_READ) {
        for (size_t i = 0; i < kSize; i++) {
          volatile uint8_t force_read = static_cast<uint8_t*>(memory)[i];
          (void)force_read;
        }
      }
      if (to_flags & PROT_WRITE) {
        for (size_t i = 0; i < kSize; i++) {
          static_cast<uint8_t*>(memory)[i] = 0xff;
        }
      }

      EXPECT_EQ(munmap(memory, kSize), 0);
    }
  }
}

TEST(PosixMemoryMapTest, FailsWithNonMappableFileType) {
  int fds[2];
  ASSERT_EQ(pipe(fds), 0);
  errno = 0;
  void* mem = mmap(NULL, kSize, PROT_READ, MAP_SHARED, fds[0], 0);
  EXPECT_EQ(mem, MAP_FAILED);
  EXPECT_EQ(errno, ENODEV);
  close(fds[0]);
  close(fds[1]);
}

class PosixMemoryMapFileTest : public ::testing::Test {
 protected:
  PosixMemoryMapFileTest() : scoped_file_(kSize) {}

  void SetUp() override {
    fd_ = open(scoped_file_.filename().c_str(), O_RDWR);
    ASSERT_NE(fd_, -1);
  }

  void TearDown() override {
    if (fd_ != -1) {
      close(fd_);
    }
  }

  int fd_ = -1;
  ScopedRandomFile scoped_file_;
};

TEST_F(PosixMemoryMapFileTest, SharedMappingWritesThrough) {
  void* mem = mmap(NULL, kSize, PROT_READ | PROT_WRITE, MAP_SHARED, fd_, 0);
  ASSERT_NE(mem, MAP_FAILED);
  static_cast<char*>(mem)[0] = 'X';
  ASSERT_EQ(munmap(mem, kSize), 0)
      << "munmap failed with error " << strerror(errno);
  std::vector<char> buffer(kSize);
  int new_fd = open(scoped_file_.filename().c_str(), O_RDONLY);
  ASSERT_NE(new_fd, -1);
  ASSERT_EQ(read(new_fd, buffer.data(), 1), 1);
  close(new_fd);
  EXPECT_EQ(buffer[0], 'X');
}

TEST_F(PosixMemoryMapFileTest, PrivateMappingIsPrivate) {
  void* mem = mmap(NULL, kSize, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd_, 0);
  ASSERT_NE(mem, MAP_FAILED);
  static_cast<char*>(mem)[0] = 'Y';
  ASSERT_EQ(munmap(mem, kSize), 0)
      << "munmap failed with error " << strerror(errno);
  std::vector<char> buffer(kSize);
  int new_fd = open(scoped_file_.filename().c_str(), O_RDONLY);
  ASSERT_NE(new_fd, -1);
  ASSERT_EQ(read(new_fd, buffer.data(), 1), 1);
  close(new_fd);
  EXPECT_NE(buffer[0], 'Y');
}

TEST_F(PosixMemoryMapFileTest, FailsWithBadFileDescriptor) {
  errno = 0;
  void* mem = mmap(NULL, kSize, PROT_READ, MAP_SHARED, -2, 0);
  EXPECT_EQ(mem, MAP_FAILED);
  EXPECT_EQ(errno, EBADF);
}

TEST_F(PosixMemoryMapFileTest, FailsWithIncorrectPermissions) {
  int ro_fd = open(scoped_file_.filename().c_str(), O_RDONLY);
  ASSERT_NE(ro_fd, -1);
  errno = 0;
  void* mem = mmap(NULL, kSize, PROT_READ | PROT_WRITE, MAP_SHARED, ro_fd, 0);
  EXPECT_EQ(mem, MAP_FAILED);
  EXPECT_EQ(errno, EACCES);
  close(ro_fd);
}

TEST_F(PosixMemoryMapFileTest, MayFailWithUnalignedOffset) {
  if (kSbMemoryPageSize > 1) {
    errno = 0;
    void* mem = mmap(NULL, kSize - 1, PROT_READ, MAP_SHARED, fd_, 1);
    if (mem == MAP_FAILED) {
      EXPECT_EQ(errno, EINVAL);
    } else {
      munmap(mem, kSize - 1);
    }
  }
}

TEST_F(PosixMemoryMapFileTest, FailsWithInvalidFlags) {
  errno = 0;
  void* mem = mmap(NULL, kSize, PROT_READ, 0, fd_, 0);
  EXPECT_EQ(mem, MAP_FAILED);
  EXPECT_EQ(errno, EINVAL);
}

TEST_F(PosixMemoryMapFileTest, SuccessWithMapFixed) {
  void* placeholder =
      mmap(NULL, kSize, PROT_READ, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  ASSERT_NE(placeholder, MAP_FAILED);
  ASSERT_EQ(munmap(placeholder, kSize), 0)
      << "munmap failed with error " << strerror(errno);
  void* mem =
      mmap(placeholder, kSize, PROT_READ, MAP_SHARED | MAP_FIXED, fd_, 0);
  ASSERT_NE(mem, MAP_FAILED);
  EXPECT_EQ(mem, placeholder);
  EXPECT_EQ(munmap(mem, kSize), 0)
      << "munmap failed with error " << strerror(errno);
}

TEST_F(PosixMemoryMapFileTest, AnonymousMappingIsZeroed) {
  void* mem = mmap(NULL, kSize, PROT_READ | PROT_WRITE,
                   MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  ASSERT_NE(mem, MAP_FAILED);
  char* data = static_cast<char*>(mem);
  for (size_t i = 0; i < kSize; ++i) {
    ASSERT_EQ(data[i], 0);
  }
  ASSERT_EQ(munmap(mem, kSize), 0)
      << "munmap failed with error " << strerror(errno);
}

}  // namespace
}  // namespace starboard::nplb
