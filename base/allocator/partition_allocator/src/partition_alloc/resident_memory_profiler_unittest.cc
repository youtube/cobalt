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

#include "base/allocator/partition_allocator/src/partition_alloc/resident_memory_profiler.h"


#include "build/build_config.h"
#include "partition_alloc/buildflags.h"


#if BUILDFLAG(IS_COBALT) && PA_BUILDFLAG(ENABLE_BACKUP_REF_PTR_SUPPORT)

#include "base/allocator/partition_allocator/src/partition_alloc/in_slot_metadata.h"
#include "base/allocator/partition_allocator/src/partition_alloc/partition_root.h"
#include "base/allocator/partition_allocator/src/partition_alloc/partition_alloc.h"
#include "testing/gtest/include/gtest/gtest.h" // nogncheck

namespace partition_alloc {

namespace {

class ResidentMemoryProfilerTest : public testing::Test {
 protected:
  void SetUp() override {
    allocator_.init(partition_alloc::PartitionOptions{});
  }

  void TearDown() override {
    // Empty
  }

  partition_alloc::PartitionAllocator allocator_;
};

TEST_F(ResidentMemoryProfilerTest, BitStolenCorrectly) {
  // Test that kSampledBitMask is defined and correctly bitwise positioned.
#if !PA_BUILDFLAG(ENABLE_DANGLING_RAW_PTR_CHECKS)
  EXPECT_EQ(InSlotMetadata::kSampledBitMask, 1U << 31);
#else
  EXPECT_EQ(InSlotMetadata::kSampledBitMask, 1ULL << 33);
#endif
}

TEST_F(ResidentMemoryProfilerTest, HistogramUpdatedOnSample) {
  g_is_memory_profiler_enabled.store(true, std::memory_order_relaxed);

  void* ptr = allocator_.root()->Alloc(1024, "test_alloc");
  EXPECT_TRUE(ptr != nullptr);

  // Manually force sampling on the pointer
  ThreadLocalData* tld = GetThreadLocalData();
  tld->bytes_until_next_sample = 0; // Force it for next time or trigger manually

  SampleAllocation(ptr, 1024, tld);

  // Check the bit is set
  auto* metadata = PartitionRoot::InSlotMetadataPointerFromSlotStartAndSize(
      internal::SlotStart::FromObject(ptr).untagged_slot_start_,
      ReadOnlySlotSpanMetadata::FromObject(ptr)->bucket->slot_size);
  
  EXPECT_TRUE(metadata->IsSampled());

  allocator_.root()->Free(ptr); // Will trigger OnFreeSampled due to bit being set

  // Bit should be unset after Free
  EXPECT_FALSE(metadata->IsSampled());
}

}  // namespace
}  // namespace partition_alloc

#endif  // BUILDFLAG(IS_COBALT)
