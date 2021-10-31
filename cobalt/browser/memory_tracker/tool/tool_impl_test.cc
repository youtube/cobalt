// Copyright 2016 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/browser/memory_tracker/tool/tool_impl.h"
#include "build/build_config.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace browser {
namespace memory_tracker {

using ::nb::analytics::AllocationGroup;
using ::nb::analytics::AllocationRecord;
using ::nb::analytics::AllocationVisitor;

// Tests the expectation that AllocationSizeBinner will correctly bin
// allocations.
TEST(MemoryTrackerToolTest, AllocationSizeBinner) {
  const size_t index =
      AllocationSizeBinner::GetBucketIndexForAllocationSize(24);
  EXPECT_EQ(5, static_cast<int>(index));

  const size_t kAllocSize = 24;

  AllocationSizeBinner binner(NULL);
  void* dummy_memory = SbMemoryAllocate(kAllocSize);

  AllocationRecord allocation_record(kAllocSize, NULL);
  binner.Visit(dummy_memory, allocation_record);

  size_t min_val = 0;
  size_t max_val = 0;
  AllocationSizeBinner::GetSizeRange(kAllocSize, &min_val, &max_val);
  EXPECT_EQ(16, static_cast<int>(min_val));
  EXPECT_EQ(31, static_cast<int>(max_val));

  // 0 -> [0,0]
  // 1 -> [1,1]
  // 2 -> [2,3]
  // 3 -> [4,7]
  // 4 -> [8,15]
  // 5 -> [16,31] ...
  EXPECT_EQ(1, binner.allocation_histogram()[5]);

  allocation_record.size = 15;
  binner.Visit(NULL, allocation_record);
  binner.Visit(NULL, allocation_record);

  size_t min_value = 0;
  size_t max_value = 0;
  binner.GetLargestSizeRange(&min_value, &max_value);
  EXPECT_EQ(min_value, static_cast<int>(8));
  EXPECT_EQ(max_value, static_cast<int>(15));

  std::string csv_string = binner.ToCSVString();

  // Expect header.
  bool found = (csv_string.find("\"8...15\",\"16...31\"") != std::string::npos);
  EXPECT_TRUE(found);

  // Expect data.
  found = (csv_string.find("2,1") != std::string::npos);
  SbMemoryDeallocate(dummy_memory);
}

// Tests the expectation that AllocationSizeBinner will correctly bin
// allocations.
TEST(MemoryTrackerToolTest, FindTopSizes) {
  const size_t min_size = 21;
  const size_t max_size = 23;
  AllocationGroup* group = NULL;  // signals "accept any group".

  FindTopSizes top_sizes_visitor(min_size, max_size, group);

  AllocationRecord record_a(21, group);
  AllocationRecord record_b(22, group);
  AllocationRecord record_c(22, group);
  AllocationRecord record_d(24, group);

  const void* dummy_alloc = NULL;
  top_sizes_visitor.Visit(dummy_alloc, record_a);
  top_sizes_visitor.Visit(dummy_alloc, record_b);
  top_sizes_visitor.Visit(dummy_alloc, record_c);
  top_sizes_visitor.Visit(dummy_alloc, record_d);

  std::vector<FindTopSizes::GroupAllocation> top_allocations =
      top_sizes_visitor.GetTopAllocations();

  // Expect element 24 to be filtered out, leaving only 22 and 24.
  EXPECT_EQ(top_allocations.size(), 2);
  // Expect that allocation size 22 is the top allocation.
  EXPECT_EQ(top_allocations[0].allocation_size, 22);
  EXPECT_EQ(top_allocations[0].allocation_count, 2);
  // And then the 21 byte allocation.
  EXPECT_EQ(top_allocations[1].allocation_size, 21);
  EXPECT_EQ(top_allocations[1].allocation_count, 1);
}

}  // namespace memory_tracker
}  // namespace browser
}  // namespace cobalt
