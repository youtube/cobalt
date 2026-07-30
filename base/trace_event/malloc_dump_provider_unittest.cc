// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/trace_event/malloc_dump_provider.h"

#include "base/allocator/buildflags.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_WIN)
#include <windows.h>
#endif

#if PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
#include "base/memory/advanced_memory_safety_checks.h"
#include "base/trace_event/process_memory_dump.h"
#include "partition_alloc/partition_root.h"
#endif  // PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)

namespace base::trace_event {

#if BUILDFLAG(IS_WIN)

namespace {

class ScopedTestHeap {
 public:
  ScopedTestHeap() : handle_(::HeapCreate(0, 0, 0)) { CHECK(handle_); }
  ~ScopedTestHeap() { CHECK(::HeapDestroy(handle_)); }

  ScopedTestHeap(const ScopedTestHeap&) = delete;
  ScopedTestHeap& operator=(const ScopedTestHeap&) = delete;

  HANDLE handle() { return handle_; }

 private:
  HANDLE handle_;
};

// Above the historical HeapAlloc->VirtualAlloc threshold (~512 KB), so the
// allocation is guaranteed to appear as an orphan busy entry.
constexpr size_t kLargeAllocBytes = 2 * 1024 * 1024;

}  // namespace

TEST(MallocDumpProviderTest, WinHeapInfo_EmptyHeap) {
  ScopedTestHeap heap;

  auto info = internal::WinHeapInfo::FromHandleForTesting(heap.handle());

  EXPECT_EQ(info.allocated_size, 0u);
  EXPECT_EQ(info.block_count, 0u);
  // A fresh heap has at least one reserved region.
  EXPECT_GT(info.committed_size + info.uncommitted_size, 0u);
}

TEST(MallocDumpProviderTest, WinHeapInfo_SmallAllocStaysInRegion) {
  ScopedTestHeap heap;
  void* p = ::HeapAlloc(heap.handle(), 0, 64);
  ASSERT_TRUE(p);

  auto info = internal::WinHeapInfo::FromHandleForTesting(heap.handle());

  EXPECT_GE(info.allocated_size, 64u);
  EXPECT_GE(info.block_count, 1u);
  // The block lives inside a region whose committed bytes already include it,
  // so committed_size must dominate allocated_size.
  EXPECT_GE(info.committed_size, info.allocated_size);

  ::HeapFree(heap.handle(), 0, p);
}

TEST(MallocDumpProviderTest, WinHeapInfo_LargeAllocBecomesOrphanBusy) {
  ScopedTestHeap heap;
  void* p = ::HeapAlloc(heap.handle(), 0, kLargeAllocBytes);
  ASSERT_TRUE(p);

  auto info = internal::WinHeapInfo::FromHandleForTesting(heap.handle());

  EXPECT_GE(info.allocated_size, kLargeAllocBytes);
  EXPECT_GE(info.block_count, 1u);
  // Regression assertion for the orphan-busy-entry fix: committed_size must
  // grow with the large allocation. Before the fix, large blocks lived
  // outside any PROCESS_HEAP_REGION and were not counted as committed, so
  // committed_size would have been (much) less than allocated_size.
  EXPECT_GE(info.committed_size, info.allocated_size);

  ::HeapFree(heap.handle(), 0, p);
}

#endif  // BUILDFLAG(IS_WIN)

#if PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)

namespace {

class NormalTestClass1 {
 public:
  NormalTestClass1() = default;

  uint8_t unused_padding[15];
};

class LeakedTestClass1 {
  LEAKED_SANITIZED_OBJECT();

 public:
  LeakedTestClass1() = default;

  uint8_t unused_padding[15];
};

class LeakedTestClass2 {
  LEAKED_SANITIZED_OBJECT();

 public:
  LeakedTestClass2() = default;

  uint8_t unused_padding[2047];
};

std::pair<bool, size_t> GetIntendedLeakSize() {
  constexpr std::string_view kIntendedLeakSize = "intended_leak_size";
  constexpr std::string_view kAllocatorDumpName = "malloc/partitions/leaked";

  std::unique_ptr<MallocDumpProvider> mdp =
      MallocDumpProvider::CreateForTesting();
  const MemoryDumpArgs dump_args = {MemoryDumpLevelOfDetail::kBackground};
  ProcessMemoryDump pmd(dump_args);
  mdp->OnMemoryDump(dump_args, &pmd);

  auto iterator = pmd.allocator_dumps().find(std::string(kAllocatorDumpName));
  if (pmd.allocator_dumps().cend() == iterator) {
    return std::make_pair(false, 0u);
  }
  for (const auto& entry : iterator->second->entries()) {
    if (entry.name == kIntendedLeakSize) {
      CHECK_EQ(MemoryAllocatorDump::Entry::EntryType::kUint64,
               entry.entry_type);
      CHECK_EQ(MemoryAllocatorDump::kUnitsBytes, entry.units);
      return std::make_pair(true, entry.value_uint64);
    }
  }
  return std::make_pair(false, 0u);
}

}  // namespace

TEST(MallocDumpProviderTest, DumpIntendedLeakedSize) {
  // To avoid flakiness, firstly we will measure current `intended_leak_size`.
  // The flakiness will be caused by `safety_checks_unittests` because the tests
  // leak some objects at free().
  size_t expected_intended_leak_size;
  expected_intended_leak_size = GetIntendedLeakSize().second;

  const auto* leaked_security_object_root =
      base::internal::LeakedSecurityObjectAllocator();
  ASSERT_NE(leaked_security_object_root, nullptr);

  // Allocate and deallocate normal object. This doesn't cause any memory leaks.
  {
    std::unique_ptr<NormalTestClass1> normal_obj1 =
        std::make_unique<NormalTestClass1>();
    ASSERT_NE(normal_obj1, nullptr);
    EXPECT_NE(
        leaked_security_object_root,
        partition_alloc::PartitionRoot::GetRootFromAddress(normal_obj1.get()));
  }
  {
    auto intended_leak_size = GetIntendedLeakSize();
    EXPECT_TRUE(intended_leak_size.first);
    EXPECT_EQ(expected_intended_leak_size, intended_leak_size.second);
  }

  // Allocate and deallocate leaked security object. This will cause memory
  // leak.
  {
    std::unique_ptr<LeakedTestClass1> leaked_obj1 =
        std::make_unique<LeakedTestClass1>();
    ASSERT_NE(leaked_obj1, nullptr);
    EXPECT_EQ(
        leaked_security_object_root,
        partition_alloc::PartitionRoot::GetRootFromAddress(leaked_obj1.get()));
    // `intended_leaked_size` is calculated based on `slot_size`.
    expected_intended_leak_size +=
        leaked_security_object_root->GetSlotSizeForTesting(leaked_obj1.get());
  }

  {
    auto intended_leak_size = GetIntendedLeakSize();
    EXPECT_TRUE(intended_leak_size.first);
    EXPECT_EQ(expected_intended_leak_size, intended_leak_size.second);
  }

  {
    std::unique_ptr<LeakedTestClass2> leaked_obj2 =
        std::make_unique<LeakedTestClass2>();
    ASSERT_NE(leaked_obj2, nullptr);
    EXPECT_EQ(
        leaked_security_object_root,
        partition_alloc::PartitionRoot::GetRootFromAddress(leaked_obj2.get()));
    expected_intended_leak_size +=
        leaked_security_object_root->GetSlotSizeForTesting(leaked_obj2.get());
  }

  {
    auto intended_leak_size = GetIntendedLeakSize();
    EXPECT_TRUE(intended_leak_size.first);
    EXPECT_EQ(expected_intended_leak_size, intended_leak_size.second);
  }
}

#endif  // PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)

}  // namespace base::trace_event
