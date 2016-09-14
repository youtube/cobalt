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

TEST(SbMemoryMapTest, DoesNotLeak) {
  // Map 4x the amount of system memory (sequentially, not at once).
  int64_t bytes_mapped = SbSystemGetTotalCPUMemory() / 4;
  for (int64_t total_bytes_mapped = 0;
       total_bytes_mapped < SbSystemGetTotalCPUMemory() * 4;
       total_bytes_mapped += bytes_mapped) {
    void* memory = SbMemoryMap(bytes_mapped, kSbMemoryMapProtectWrite, "test");
    ASSERT_NE(kFailed, memory);

    // If this is the last iteration of the loop, then force a page commit for
    // every single page.  For any other iteration, force a page commit for
    // roughly 1000 of the pages.
    bool last_iteration =
        !(total_bytes_mapped + bytes_mapped < SbSystemGetTotalCPUMemory() * 4);
    uint8_t* first_page = static_cast<uint8_t*>(memory);
    const size_t page_increment_factor =
        (last_iteration)
            ? size_t(1u)
            : std::max(static_cast<size_t>(bytes_mapped /
                                           (1000 * SB_MEMORY_PAGE_SIZE)),
                       size_t(1u));

    for (uint8_t* page = first_page; page < first_page + bytes_mapped;
         page += SB_MEMORY_PAGE_SIZE * page_increment_factor) {
      *page = 0x55;
    }

    EXPECT_TRUE(SbMemoryUnmap(memory, bytes_mapped));
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
static int sum(int x, int y) {
  return x + y;
}

// It's not clear how portable this test is.
TEST(SbMemoryMapTest, CanExecuteMappedMemoryWithExecFlag) {
  void* memory = SbMemoryMap(
      kSize, kSbMemoryMapProtectReadWrite | kSbMemoryMapProtectExec, "test");
  ASSERT_NE(kFailed, memory);

  // There's no reliable way to determine the size of the 'sum' function. We can
  // get a reasonable upper bound by assuming that the function's implementation
  // does not cross a page boundary, and copy the memory from the beginning of
  // the function to the end of the page that it is mapped to.
  // Note that this will fail if the function implementation crosses a page
  // boundary, which seems pretty unlikely, especially given the size of the
  // function.

  // A function pointer can't be cast to void*, but uint8* seems to be okay. So
  // cast to a uint* which will be implicitly casted to a void* below.
  uint8_t* sum_function_start = reinterpret_cast<uint8_t*>(&sum);

  // Get the last address of the page that |sum_function_start| is on.
  uint8_t* sum_function_page_end = reinterpret_cast<uint8_t*>(
      (reinterpret_cast<intptr_t>(&sum) / SB_MEMORY_PAGE_SIZE) *
          SB_MEMORY_PAGE_SIZE +
      SB_MEMORY_PAGE_SIZE);
  ASSERT_TRUE(SbMemoryIsAligned(sum_function_page_end, SB_MEMORY_PAGE_SIZE));
  ASSERT_LT(sum_function_start, sum_function_page_end);

  size_t bytes_to_copy = sum_function_page_end - sum_function_start;
  ASSERT_LE(bytes_to_copy, SB_MEMORY_PAGE_SIZE);

  SbMemoryCopy(memory, sum_function_start, bytes_to_copy);
  SumFunction mapped_function =
      reinterpret_cast<SumFunction>(reinterpret_cast<char*>(memory));

  EXPECT_EQ(4, (*mapped_function)(1, 3));
  EXPECT_EQ(5, (*mapped_function)(10, -5));

  EXPECT_TRUE(SbMemoryUnmap(memory, kSize));
}
#endif  // SB_CAN(MAP_EXECUTABLE_MEMORY)
#endif  // SB_HAS(MMAP)

}  // namespace
}  // namespace nplb
}  // namespace starboard
