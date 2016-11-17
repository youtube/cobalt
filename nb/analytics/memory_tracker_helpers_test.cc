/*
 * Copyright 2016 Google Inc. All Rights Reserved.
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

#include "nb/analytics/memory_tracker_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace nb {
namespace analytics {
namespace {

///////////////////////////////////////////////////////////////////////////////
TEST(AtomicStringAllocationGroupMap, Use) {
  AtomicStringAllocationGroupMap map;
  AllocationGroup* tag = map.Ensure("MemoryRegion");
  EXPECT_TRUE(tag != NULL);
  EXPECT_EQ(std::string("MemoryRegion"), tag->name());
  AllocationGroup* tag2 = map.Ensure("MemoryRegion");
  EXPECT_EQ(tag, tag2);
}

TEST(AtomicAllocationMap, AddHasRemove) {
  AtomicAllocationMap atomic_pointer_map;
  int32_t int_a = 0;
  int32_t int_b = 1;

  // Initially empty.
  EXPECT_TRUE(atomic_pointer_map.Empty());

  const AllocationRecord int32_alloc_record =
      AllocationRecord(sizeof(int32_t), NULL);
  AllocationRecord alloc_output;

  EXPECT_TRUE(atomic_pointer_map.Add(&int_a, int32_alloc_record));
  EXPECT_FALSE(atomic_pointer_map.Add(&int_a, int32_alloc_record));
  EXPECT_TRUE(atomic_pointer_map.Get(&int_a, &alloc_output));
  EXPECT_EQ(alloc_output.size, sizeof(int32_t));

  EXPECT_FALSE(atomic_pointer_map.Get(&int_b, &alloc_output));
  EXPECT_EQ(0, alloc_output.size);
  // Adding pointer to int_a increases set to 1 element.
  EXPECT_EQ(atomic_pointer_map.Size(), 1);
  EXPECT_FALSE(atomic_pointer_map.Empty());

  // Adds pointer to int_b.
  EXPECT_TRUE(atomic_pointer_map.Add(&int_b, int32_alloc_record));
  EXPECT_TRUE(atomic_pointer_map.Get(&int_b, &alloc_output));
  EXPECT_EQ(sizeof(int32_t), alloc_output.size);
  // Expect that the second pointer added will increase the number of elements.
  EXPECT_EQ(atomic_pointer_map.Size(), 2);
  EXPECT_FALSE(atomic_pointer_map.Empty());

  // Now remove the elements and ensure that they no longer found and that
  // the size of the table shrinks to empty.
  EXPECT_TRUE(atomic_pointer_map.Remove(&int_a, &alloc_output));
  EXPECT_EQ(sizeof(int32_t), alloc_output.size);
  EXPECT_EQ(atomic_pointer_map.Size(), 1);
  EXPECT_FALSE(atomic_pointer_map.Remove(&int_a, &alloc_output));
  EXPECT_EQ(0, alloc_output.size);
  EXPECT_TRUE(atomic_pointer_map.Remove(&int_b, &alloc_output));
  EXPECT_EQ(atomic_pointer_map.Size(), 0);

  EXPECT_TRUE(atomic_pointer_map.Empty());
}

TEST(ConcurrentAllocationMap, AddHasRemove) {
  ConcurrentAllocationMap alloc_map;
  int32_t int_a = 0;
  int32_t int_b = 1;

  // Initially empty.
  EXPECT_TRUE(alloc_map.Empty());

  const AllocationRecord int32_alloc_record =
      AllocationRecord(sizeof(int32_t), NULL);
  AllocationRecord alloc_output;

  EXPECT_TRUE(alloc_map.Add(&int_a, int32_alloc_record));
  EXPECT_FALSE(alloc_map.Add(&int_a, int32_alloc_record));
  EXPECT_TRUE(alloc_map.Get(&int_a, &alloc_output));
  EXPECT_EQ(alloc_output.size, sizeof(int32_t));

  EXPECT_FALSE(alloc_map.Get(&int_b, &alloc_output));
  EXPECT_EQ(0, alloc_output.size);
  // Adding pointer to int_a increases set to 1 element.
  EXPECT_EQ(alloc_map.Size(), 1);
  EXPECT_FALSE(alloc_map.Empty());

  // Adds pointer to int_b.
  EXPECT_TRUE(alloc_map.Add(&int_b, int32_alloc_record));
  EXPECT_TRUE(alloc_map.Get(&int_b, &alloc_output));
  EXPECT_EQ(sizeof(int32_t), alloc_output.size);
  // Expect that the second pointer added will increase the number of elements.
  EXPECT_EQ(alloc_map.Size(), 2);
  EXPECT_FALSE(alloc_map.Empty());

  // Now remove the elements and ensure that they no longer found and that
  // the size of the table shrinks to empty.
  EXPECT_TRUE(alloc_map.Remove(&int_a, &alloc_output));
  EXPECT_EQ(sizeof(int32_t), alloc_output.size);
  EXPECT_EQ(alloc_map.Size(), 1);
  EXPECT_FALSE(alloc_map.Remove(&int_a, &alloc_output));
  EXPECT_EQ(0, alloc_output.size);
  EXPECT_TRUE(alloc_map.Remove(&int_b, &alloc_output));
  EXPECT_EQ(alloc_map.Size(), 0);

  EXPECT_TRUE(alloc_map.Empty());
}

}  // namespace
}  // namespace analytics
}  // namespace nb
