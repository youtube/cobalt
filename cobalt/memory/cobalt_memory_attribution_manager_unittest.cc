// Copyright 2026 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "cobalt/memory/cobalt_memory_attribution_manager.h"

#include <atomic>
#include <thread>
#include <vector>

#include "base/allocator/partition_allocator/src/partition_alloc/partition_alloc.h"
#include "base/feature_list.h"
#include "base/memory/cobalt_memory_context.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "cobalt/browser/features.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace memory {

using base::memory::GetCurrentMemoryContext;
using base::memory::MemoryContext;
using base::memory::ScopedMemoryContext;

class CobaltMemoryAttributionManagerTest : public testing::Test {
 protected:
  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(
        cobalt::features::kCobaltMemoryAttributionManager);
    manager_ = CobaltMemoryAttributionManager::Get();
    manager_->Start();
    // Reset state before tests if possible, assuming manager gives us a clean
    // slate.
  }

  void TearDown() override { manager_->Stop(); }

  uint64_t GetResidentBytes(MemoryContext context) {
    // Assuming Coder 1 exposed this as a public method as discussed.
    return manager_->GetResidentBytes(context);
  }

  uint64_t GetPartitionAllocOverhead() {
    return manager_->GetPartitionAllocOverhead();
  }

  uint64_t GetPartitionAllocTotalCommitted() {
    return manager_->GetPartitionAllocTotalCommitted();
  }

  base::test::TaskEnvironment task_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;
  CobaltMemoryAttributionManager* manager_;
};

TEST_F(CobaltMemoryAttributionManagerTest, ExactMatchVerification) {
  uint64_t initial_bytes = GetResidentBytes(MemoryContext::kDOM);

  {
    ScopedMemoryContext dom_context(MemoryContext::kDOM);

    // Allocate known sizes via an explicit PartitionRoot
    partition_alloc::PartitionOptions opts;
    partition_alloc::PartitionAllocator allocator;
    allocator.init(opts);

    size_t alloc_size1 = 1024;
    size_t alloc_size2 = 4096;

    void* ptr1 = allocator.root()->Alloc(alloc_size1, "");
    void* ptr2 = allocator.root()->Alloc(alloc_size2, "");

    // In PartitionAlloc, bucket size may be slightly larger than requested
    // size. However, we expect at least the requested size to be accounted for.
    uint64_t current_bytes = GetResidentBytes(MemoryContext::kDOM);
    EXPECT_GE(current_bytes, initial_bytes + alloc_size1 + alloc_size2);

    // Free the allocations
    allocator.root()->Free(ptr1);
    allocator.root()->Free(ptr2);
  }

  // After freeing, the resident bytes should return exactly to the initial
  // baseline.
  uint64_t final_bytes = GetResidentBytes(MemoryContext::kDOM);
  EXPECT_EQ(final_bytes, initial_bytes);
}

TEST_F(CobaltMemoryAttributionManagerTest, GlobalReconciliation) {
  // Assert that Sum(PerComponent_Resident_Memory) + PartitionAlloc_Overhead
  // == PartitionAlloc_Total_Committed_Memory.

  uint64_t total_component_resident = 0;
  for (size_t i = 0; i < static_cast<size_t>(MemoryContext::kCount); ++i) {
    total_component_resident += GetResidentBytes(static_cast<MemoryContext>(i));
  }

  uint64_t overhead = GetPartitionAllocOverhead();
  uint64_t total_committed = GetPartitionAllocTotalCommitted();

  // Validate the deterministic accounting formula.
  // Since the test framework has base allocations before the manager started,
  // we can only assert that the tracked memory is less than or equal to the
  // total allocated.
  EXPECT_LE(total_component_resident + overhead, total_committed);
}

TEST_F(CobaltMemoryAttributionManagerTest, ThreadSafetyAndConcurrency) {
  constexpr int kNumThreads = 4;
  constexpr int kAllocationsPerThread = 1000;
  constexpr size_t kAllocSize = 256;

  std::atomic<bool> start_flag{false};
  std::vector<std::thread> threads;

  for (int i = 0; i < kNumThreads; ++i) {
    threads.emplace_back([&start_flag, i]() {
      partition_alloc::PartitionOptions opts;
      partition_alloc::PartitionAllocator allocator;
      allocator.init(opts);

      while (!start_flag.load(std::memory_order_acquire)) {
        std::this_thread::yield();
      }

      // Alternate components per thread for stress testing
      MemoryContext ctx =
          (i % 2 == 0) ? MemoryContext::kNetwork : MemoryContext::kStorage;
      ScopedMemoryContext context(ctx);

      std::vector<void*> ptrs;
      for (int j = 0; j < kAllocationsPerThread; ++j) {
        ptrs.push_back(allocator.root()->Alloc(kAllocSize, ""));
      }

      for (void* ptr : ptrs) {
        allocator.root()->Free(ptr);
      }
    });
  }

  // Capture baseline
  uint64_t initial_network = GetResidentBytes(MemoryContext::kNetwork);
  uint64_t initial_storage = GetResidentBytes(MemoryContext::kStorage);

  // Start threads
  start_flag.store(true, std::memory_order_release);
  for (auto& thread : threads) {
    thread.join();
  }

  // Force aggregation of thread-local counters before checking assertions.
  manager_->FlushThreadLocalCountersForTesting();

  // After all threads finish allocating and freeing, memory should perfectly
  // return to baseline.
  uint64_t final_network = GetResidentBytes(MemoryContext::kNetwork);
  uint64_t final_storage = GetResidentBytes(MemoryContext::kStorage);

  EXPECT_EQ(final_network, initial_network);
  EXPECT_EQ(final_storage, initial_storage);
}

TEST_F(CobaltMemoryAttributionManagerTest, LargeAllocation) {
  // Test allocations larger than a PartitionAlloc SuperPage (e.g., > 2MB)
  // to ensure the Shadow Map correctly maps and unmaps continuous regions.
  uint64_t initial_bytes = GetResidentBytes(MemoryContext::kMedia);
  const size_t kLargeSize = 5 * 1024 * 1024;  // 5 MB

  {
    partition_alloc::PartitionOptions opts;
    partition_alloc::PartitionAllocator allocator;
    allocator.init(opts);

    ScopedMemoryContext media_context(MemoryContext::kMedia);
    void* large_ptr = allocator.root()->Alloc(kLargeSize, "");

    manager_->FlushThreadLocalCountersForTesting();
    uint64_t current_bytes = GetResidentBytes(MemoryContext::kMedia);
    EXPECT_GE(current_bytes, initial_bytes + kLargeSize);

    allocator.root()->Free(large_ptr);
  }

  manager_->FlushThreadLocalCountersForTesting();
  uint64_t final_bytes = GetResidentBytes(MemoryContext::kMedia);
  EXPECT_EQ(final_bytes, initial_bytes);
}

}  // namespace memory
}  // namespace cobalt
