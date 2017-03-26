/*
 * Copyright 2017 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "nb/std_allocator.h"

#include <map>
#include <vector>

#include "testing/gtest/include/gtest/gtest.h"

namespace nb {
namespace {

struct CountingAllocator {
  static int num_allocs;
  static int64_t num_bytes;
  static void Reset() {
    num_allocs = 0;
    num_bytes = 0;
  }

  static void* Allocate(size_t n) {
    ++num_allocs;
    num_bytes += n;
    return SbMemoryAllocate(n);
  }

  static void Deallocate(void* ptr, size_t n) {
    num_bytes -= n;
    --num_allocs;
    SbMemoryDeallocate(ptr);
  }
};
int CountingAllocator::num_allocs = 0;
int64_t CountingAllocator::num_bytes = 0;

// Test the expectation that vector will go through the supplied allocator.
TEST(StdAllocator, vector) {
  CountingAllocator::Reset();

  typedef std::vector<int, StdAllocator<int, CountingAllocator> > IntVector;

  EXPECT_EQ(0, CountingAllocator::num_allocs);
  EXPECT_EQ(0, CountingAllocator::num_bytes);

  IntVector* int_vector = new IntVector;
  int_vector->push_back(0);

  for (int i = 0; i < 10; ++i) {
    int_vector->push_back(i);
  }

  // We aren't sure how much allocation is here, but we know it's more than 0.
  EXPECT_LT(0, CountingAllocator::num_allocs);
  EXPECT_LT(0, CountingAllocator::num_bytes);

  delete int_vector;

  EXPECT_EQ(0, CountingAllocator::num_allocs);
  EXPECT_EQ(0, CountingAllocator::num_bytes);

  CountingAllocator::Reset();
}

// Test the expectation that map will go through the supplied allocator.
TEST(StdAllocator, map) {
  CountingAllocator::Reset();

  typedef std::map<int, int, std::less<int>,
                   StdAllocator<int, CountingAllocator> > IntMap;

  EXPECT_EQ(0, CountingAllocator::num_allocs);
  EXPECT_EQ(0, CountingAllocator::num_bytes);

  IntMap* int_map = new IntMap;
  for (int i = 0; i < 10; ++i) {
    (*int_map)[i] = i;
  }

  // We aren't sure how much allocation is here, but we know it's more than 0.
  EXPECT_LT(0, CountingAllocator::num_allocs);
  EXPECT_LT(0, CountingAllocator::num_bytes);

  delete int_map;

  EXPECT_EQ(0, CountingAllocator::num_allocs);
  EXPECT_EQ(0, CountingAllocator::num_bytes);

  CountingAllocator::Reset();
}

}  // namespace
}  // namespace nb
