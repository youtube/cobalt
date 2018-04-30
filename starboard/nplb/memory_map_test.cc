// Copyright 2016 Google Inc. All Rights Reserved.
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

#include <algorithm>

#include "starboard/memory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {
namespace {

#if SB_HAS(MMAP)
const size_t kSize = SB_MEMORY_PAGE_SIZE * 8;
const void* kFailed = SB_MEMORY_MAP_FAILED;

TEST(SbMemoryMapTest, AllocatesNormally) {
  void* memory = SbMemoryMap(kSize, kSbMemoryMapProtectRead, "test");
  ASSERT_NE(kFailed, memory);
  EXPECT_TRUE(SbMemoryUnmap(memory, kSize));
}

TEST(SbMemoryMapTest, AllocatesZero) {
  void* memory = SbMemoryMap(0, kSbMemoryMapProtectRead, "test");
  ASSERT_EQ(kFailed, memory);
  EXPECT_FALSE(SbMemoryUnmap(memory, 0));
}

TEST(SbMemoryMapTest, AllocatesOne) {
  void* memory = SbMemoryMap(1, kSbMemoryMapProtectRead, "test");
  ASSERT_NE(kFailed, memory);
  EXPECT_TRUE(SbMemoryUnmap(memory, 1));
}

TEST(SbMemoryMapTest, AllocatesOnePage) {
  void* memory =
      SbMemoryMap(SB_MEMORY_PAGE_SIZE, kSbMemoryMapProtectRead, "test");
  ASSERT_NE(kFailed, memory);
  EXPECT_TRUE(SbMemoryUnmap(memory, SB_MEMORY_PAGE_SIZE));
}

// Disabled because it is too slow -- currently ~5 seconds on a Linux desktop
// with lots of memory.
TEST(SbMemoryMapTest, DISABLED_DoesNotLeak) {
  const int64_t kIterations = 16;
  const double kFactor = 1.25;
  const size_t kSparseCommittedPages = 256;

  const int64_t kBytesMappedPerIteration =
      static_cast<int64_t>(SbSystemGetTotalCPUMemory() * kFactor) / kIterations;
  const int64_t kMaxBytesMapped = kBytesMappedPerIteration * kIterations;

  for (int64_t total_bytes_mapped = 0; total_bytes_mapped < kMaxBytesMapped;
       total_bytes_mapped += kBytesMappedPerIteration) {
    void* memory =
        SbMemoryMap(kBytesMappedPerIteration, kSbMemoryMapProtectWrite, "test");
    ASSERT_NE(kFailed, memory);

    // If this is the last iteration of the loop, then force a page commit for
    // every single page.  For any other iteration, force a page commit for
    // |kSparseCommittedPages|.
    bool last_iteration =
        !(total_bytes_mapped + kBytesMappedPerIteration < kMaxBytesMapped);
    uint8_t* first_page = static_cast<uint8_t*>(memory);
    const size_t page_increment_factor =
        (last_iteration)
            ? size_t(1u)
            : std::max(static_cast<size_t>(
                           kBytesMappedPerIteration /
                           (kSparseCommittedPages * SB_MEMORY_PAGE_SIZE)),
                       size_t(1u));

    for (uint8_t* page = first_page;
         page < first_page + kBytesMappedPerIteration;
         page += SB_MEMORY_PAGE_SIZE * page_increment_factor) {
      *page = 0x55;
    }

    EXPECT_TRUE(SbMemoryUnmap(memory, kBytesMappedPerIteration));
  }
}

TEST(SbMemoryMapTest, CanReadWriteToResult) {
  void* memory = SbMemoryMap(kSize, kSbMemoryMapProtectReadWrite, "test");
  ASSERT_NE(kFailed, memory);
  char* data = static_cast<char*>(memory);
  for (int i = 0; i < kSize; ++i) {
    data[i] = static_cast<char>(i);
  }

  for (int i = 0; i < kSize; ++i) {
    EXPECT_EQ(data[i], static_cast<char>(i));
  }

  EXPECT_TRUE(SbMemoryUnmap(memory, kSize));
}

#if SB_CAN(MAP_EXECUTABLE_MEMORY)

typedef int (*SumFunction)(int, int);
static int Sum(int x, int y) {
  return x + y;
}
static int Sum2(int x, int y) {
  return x + y + x;
}

// Copy the contents of |Sum| into |memory|, returning an assertion result
// indicating whether the copy was successful, and a function pointer to the
// possibly copied |Sum| function data.  Clients are expected to immediately
// call |ASSERT| on the assertion result.
std::pair<::testing::AssertionResult, SumFunction> CopySumFunctionIntoMemory(
    void* memory) {
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
  if (reinterpret_cast<uintptr_t>(&Sum2) % SB_MEMORY_PAGE_SIZE <
      reinterpret_cast<uintptr_t>(&Sum) % SB_MEMORY_PAGE_SIZE) {
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
      (reinterpret_cast<uintptr_t>(sum_function_start) / SB_MEMORY_PAGE_SIZE) *
          SB_MEMORY_PAGE_SIZE +
      SB_MEMORY_PAGE_SIZE);
  if (!SbMemoryIsAligned(sum_function_page_end, SB_MEMORY_PAGE_SIZE)) {
    return {::testing::AssertionFailure()
                << "Expected |Sum| page end ("
                << static_cast<void*>(sum_function_page_end)
                << ") to be aligned to " << SB_MEMORY_PAGE_SIZE,
            nullptr};
  }
  if (sum_function_start >= sum_function_page_end) {
    return {::testing::AssertionFailure()
                << "Expected |Sum| function page start ("
                << static_cast<void*>(sum_function_start)
                << ") to be less than |Sum| function page end ("
                << static_cast<void*>(sum_function_page_end) << ")",
            nullptr};
  }

  size_t bytes_to_copy = sum_function_page_end - sum_function_start;
  if (bytes_to_copy > SB_MEMORY_PAGE_SIZE) {
    return {::testing::AssertionFailure()
                << "Expected bytes required to copy |Sum| to be less than "
                << SB_MEMORY_PAGE_SIZE << ", Actual: " << bytes_to_copy,
            nullptr};
  }

  SbMemoryCopy(memory, sum_function_start, bytes_to_copy);
  SbMemoryFlush(memory, bytes_to_copy);

  SumFunction mapped_function = reinterpret_cast<SumFunction>(
      reinterpret_cast<uint8_t*>(memory) + sum_function_offset);

  return {::testing::AssertionSuccess(), mapped_function};
}

// This test is known to run on x64, ARM, and MIPS32 with MIPS32 and MIPS16
// instructions.
TEST(SbMemoryMapTest, CanExecuteMappedMemoryWithExecFlag) {
  void* memory = SbMemoryMap(
      kSize, kSbMemoryMapProtectReadWrite | kSbMemoryMapProtectExec, "test");
  ASSERT_NE(kFailed, memory);

  auto copy_sum_function_result = CopySumFunctionIntoMemory(memory);
  ASSERT_TRUE(copy_sum_function_result.first);
  SumFunction mapped_function = copy_sum_function_result.second;

  EXPECT_EQ(Sum(1, 3), (*mapped_function)(1, 3));
  EXPECT_EQ(Sum(10, -5), (*mapped_function)(10, -5));

  EXPECT_TRUE(SbMemoryUnmap(memory, kSize));
}
#endif  // SB_CAN(MAP_EXECUTABLE_MEMORY)

#if SB_API_VERSION >= SB_MEMORY_PROTECT_API_VERSION
TEST(SbMemoryMapTest, CanChangeMemoryProtection) {
  SbMemoryMapFlags all_flags[] = {
    SbMemoryMapFlags(0),
    SbMemoryMapFlags(kSbMemoryMapProtectRead),
    SbMemoryMapFlags(kSbMemoryMapProtectWrite),
    SbMemoryMapFlags(kSbMemoryMapProtectRead | kSbMemoryMapProtectWrite),
#if SB_CAN(MAP_EXECUTABLE_MEMORY)
    SbMemoryMapFlags(kSbMemoryMapProtectExec),
    SbMemoryMapFlags(kSbMemoryMapProtectRead | kSbMemoryMapProtectExec),
    SbMemoryMapFlags(kSbMemoryMapProtectWrite | kSbMemoryMapProtectExec),
    SbMemoryMapFlags(kSbMemoryMapProtectRead | kSbMemoryMapProtectWrite |
                     kSbMemoryMapProtectExec),
#endif
  };

  for (SbMemoryMapFlags from_flags : all_flags) {
    for (SbMemoryMapFlags to_flags : all_flags) {
      void* memory = SbMemoryMap(kSize, from_flags, "test");
      // If the platform does not support a particular protection flags
      // configuration in the first place, then just give them a pass, knowing
      // that that feature will be tested elsewhere.
      if (memory == SB_MEMORY_MAP_FAILED) {
        continue;
      }

#if SB_CAN(MAP_EXECUTABLE_MEMORY)
      // We can only test the ability to execute memory after changing
      // protections if we have write permissions either now or then, because
      // we have to actually put a valid function into the mapped memory.
      SumFunction mapped_function = nullptr;
      if (from_flags & kSbMemoryMapProtectWrite) {
        auto copy_sum_function_result = CopySumFunctionIntoMemory(memory);
        ASSERT_TRUE(copy_sum_function_result.first);
        mapped_function = copy_sum_function_result.second;
      }
#endif

      if (!SbMemoryProtect(memory, kSize, to_flags)) {
        EXPECT_TRUE(SbMemoryUnmap(memory, kSize));
        continue;
      }

#if SB_CAN(MAP_EXECUTABLE_MEMORY)
      if ((to_flags & kSbMemoryMapProtectWrite & kSbMemoryMapProtectExec) &&
          mapped_function == nullptr) {
        auto copy_sum_function_result = CopySumFunctionIntoMemory(memory);
        ASSERT_TRUE(copy_sum_function_result.first);
        mapped_function = copy_sum_function_result.second;
      }
      if ((to_flags & kSbMemoryMapProtectExec) && mapped_function != nullptr) {
        EXPECT_EQ(Sum(0xc0ba178, 0xbadf00d),
                  mapped_function(0xc0ba178, 0xbadf00d));
      }
#endif

      if (to_flags & kSbMemoryMapProtectRead) {
        for (int i = 0; i < kSize; i++) {
          volatile uint8_t force_read = static_cast<uint8_t*>(memory)[i];
        }
      }
      if (to_flags & kSbMemoryMapProtectWrite) {
        for (int i = 0; i < kSize; i++) {
          static_cast<uint8_t*>(memory)[i] = 0xff;
        }
      }

      EXPECT_TRUE(SbMemoryUnmap(memory, kSize));
    }
  }
}
#endif  // SB_API_VERSION >= SB_MEMORY_PROTECT_API_VERSION

#endif  // SB_HAS(MMAP)

}  // namespace
}  // namespace nplb
}  // namespace starboard
