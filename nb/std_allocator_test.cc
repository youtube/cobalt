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

class CountingDynamicAllocator : public nb::Allocator {
 public:
  CountingDynamicAllocator() : bytes_allocated_(0), num_allocations_(0) {}
  void* Allocate(size_t size) override {
    bytes_allocated_ += size;
    ++num_allocations_;
    return SbMemoryAllocate(size);
  }

  void* Allocate(std::size_t size, std::size_t alignment) override {
    EXPECT_TRUE(false) << "Unexpected that aligned version is called.";
    return Allocate(size);
  }

  void Free(void* memory) override {
    FreeWithSize(memory, 0);
    ASSERT_TRUE(false) << "Unexpected that free without size "
                          "version is called.";
  }

  void FreeWithSize(void* ptr, size_t optional_size) override {
    SbMemoryDeallocate(ptr);

    bytes_allocated_ -= optional_size;
    --num_allocations_;
  }

  std::size_t GetCapacity() const override {
    EXPECT_TRUE(false) << "Unexpected that GetCapacity().";
    return 0;
  }

  std::size_t GetAllocated() const override {
    return static_cast<size_t>(bytes_allocated_);
  }

  void PrintAllocations() const override {}

  int64_t bytes_allocated_;
  int64_t num_allocations_;
};

// Test the expectation that vector will go through the supplied allocator.
TEST(StdDynamicAllocator, vector) {
  typedef StdDynamicAllocator<int> IntAllocator;
  typedef std::vector<int, IntAllocator> IntVector;

  CountingDynamicAllocator counting_allocator;

  EXPECT_EQ(0, counting_allocator.bytes_allocated_);
  EXPECT_EQ(0, counting_allocator.num_allocations_);

  IntVector* int_vector = new IntVector(IntAllocator(&counting_allocator));
  int_vector->push_back(0);

  for (int i = 0; i < 10; ++i) {
    int_vector->push_back(i);
  }

  // We aren't sure how much allocation is here, but we know it's more than 0.
  EXPECT_LT(0, counting_allocator.bytes_allocated_);
  EXPECT_LT(0, counting_allocator.num_allocations_);

  delete int_vector;

  // Expect that all allocations are destroyed now.
  EXPECT_EQ(0, counting_allocator.bytes_allocated_);
  EXPECT_EQ(0, counting_allocator.num_allocations_);
}

// Test the expectation that vector will go through the supplied allocator.
TEST(StdDynamicAllocator, map) {
  typedef StdDynamicAllocator<int> IntAllocator;
  typedef std::map<int, int, std::less<int>, IntAllocator> IntMap;

  CountingDynamicAllocator counting_allocator;

  EXPECT_EQ(0, counting_allocator.bytes_allocated_);
  EXPECT_EQ(0, counting_allocator.num_allocations_);

  IntMap* int_map = new IntMap(std::less<int>(),  // required.
                               IntAllocator(&counting_allocator));
  for (int i = 0; i < 10; ++i) {
    (*int_map)[i] = i;
  }

  // We aren't sure how much allocation is here, but we know it's more than 0.
  EXPECT_LT(0, counting_allocator.bytes_allocated_);
  EXPECT_LT(0, counting_allocator.num_allocations_);

  delete int_map;

  // Expect that all allocations are destroyed now.
  EXPECT_EQ(0, counting_allocator.bytes_allocated_);
  EXPECT_EQ(0, counting_allocator.num_allocations_);
}

// Test the expectation that a copied vector will go through the allocator from
// the first vector.
TEST(StdDynamicAllocator, vector_copy) {
  typedef StdDynamicAllocator<int> IntAllocator;
  typedef std::vector<int, IntAllocator> IntVector;

  CountingDynamicAllocator counting_allocator;

  EXPECT_EQ(0, counting_allocator.bytes_allocated_);
  EXPECT_EQ(0, counting_allocator.num_allocations_);

  IntVector* int_vector = new IntVector(IntAllocator(&counting_allocator));
  int_vector->push_back(0);

  for (int i = 0; i < 10; ++i) {
    int_vector->push_back(i);
  }

  // We aren't sure how much allocation is here, but we know it's more than 0.
  EXPECT_LT(0, counting_allocator.bytes_allocated_);
  EXPECT_LT(0, counting_allocator.num_allocations_);

  int64_t saved_bytes_allocated = counting_allocator.bytes_allocated_;
  int64_t saved_num_allocations = counting_allocator.num_allocations_;

  IntVector* int_vector_copy = new IntVector(*int_vector);
  for (int i = 0; i < 10; ++i) {
    int_vector_copy->push_back(i);
  }

  EXPECT_LT(saved_bytes_allocated, counting_allocator.bytes_allocated_);
  EXPECT_LT(saved_num_allocations, counting_allocator.num_allocations_);

  delete int_vector_copy;
  delete int_vector;

  // Expect that all allocations are destroyed now.
  EXPECT_EQ(0, counting_allocator.bytes_allocated_);
  EXPECT_EQ(0, counting_allocator.num_allocations_);
}

// Test the expectation that vector will go through the supplied allocator.
TEST(StdDynamicAllocator, map_copy) {
  typedef StdDynamicAllocator<int> IntAllocator;
  typedef std::map<int, int, std::less<int>, IntAllocator> IntMap;

  CountingDynamicAllocator counting_allocator;

  EXPECT_EQ(0, counting_allocator.bytes_allocated_);
  EXPECT_EQ(0, counting_allocator.num_allocations_);

  IntMap* int_map = new IntMap(std::less<int>(),  // required.
                               IntAllocator(&counting_allocator));
  for (int i = 0; i < 10; ++i) {
    (*int_map)[i] = i;
  }

  int64_t saved_bytes_allocated = counting_allocator.bytes_allocated_;
  int64_t saved_num_allocations = counting_allocator.num_allocations_;

  IntMap* int_map_copy = new IntMap(*int_map);

  for (int i = 0; i < 10; ++i) {
    (*int_map_copy)[i] = i;
  }

  EXPECT_LT(saved_bytes_allocated, counting_allocator.bytes_allocated_);
  EXPECT_LT(saved_num_allocations, counting_allocator.num_allocations_);

  delete int_map_copy;
  delete int_map;

  // Expect that all allocations are destroyed now.
  EXPECT_EQ(0, counting_allocator.bytes_allocated_);
  EXPECT_EQ(0, counting_allocator.num_allocations_);
}

}  // namespace
}  // namespace nb
