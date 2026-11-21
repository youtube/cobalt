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

#include "partition_alloc/superpage_region_32bit.h"

#include "partition_alloc/build_config.h"
#include "partition_alloc/buildflags.h"

#if !PA_BUILDFLAG(HAS_64_BIT_POINTERS) && PA_BUILDFLAG(IS_POSIX)

#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <sys/mman.h>

#include <algorithm>
#include <limits>
#include <string>
#include <utility>
#include <vector>

#include "partition_alloc/address_pool_manager_bitmap.h"
#include "partition_alloc/page_allocator.h"
#include "partition_alloc/partition_alloc.h"
#include "partition_alloc/partition_alloc_constants.h"
#include "partition_alloc/partition_lock.h"
#include "partition_alloc/partition_root.h"
#include "partition_alloc/reservation_offset_table.h"
#include "partition_alloc/use_death_tests.h"
#include "testing/gtest/include/gtest/gtest.h"

#if PA_BUILDFLAG(IS_LINUX) || PA_BUILDFLAG(IS_ANDROID)
#include "partition_alloc/partition_alloc_base/debug/proc_maps_linux.h"
#endif

namespace partition_alloc::internal {
namespace {

constexpr size_t kMiB = 1024 * 1024;

bool IsMapped(uintptr_t address, size_t length) {
  errno = 0;
  return msync(reinterpret_cast<void*>(address), length, MS_ASYNC) == 0;
}

// Bottom-up mode is implemented only where MAP_FIXED_NOREPLACE semantics are
// available (ConfigureBottomUpSuperPagesFor32Bit() returns false elsewhere).
#if PA_BUILDFLAG(IS_ANDROID) || PA_BUILDFLAG(IS_LINUX) || \
    PA_BUILDFLAG(IS_CHROMEOS)
#define PA_TEST_BOTTOM_UP 1
#else
#define PA_TEST_BOTTOM_UP 0
#endif

#if PA_TEST_BOTTOM_UP
#ifndef MAP_FIXED_NOREPLACE
#define MAP_FIXED_NOREPLACE 0x100000
#endif

// Finds a free, kSuperPageSize-aligned range of `bytes` by reserving it briefly
// and unmapping it. Tests run single-threaded so the window remains free.
uintptr_t ProbeFreeAlignedRange(size_t bytes) {
  const size_t probe_size = bytes + kSuperPageSize;
  void* raw = mmap(nullptr, probe_size, PROT_NONE,
                   MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE, -1, 0);
  if (raw == MAP_FAILED) {
    return 0;
  }
  const uintptr_t addr = reinterpret_cast<uintptr_t>(raw);
  const uintptr_t aligned =
      (addr + kSuperPageOffsetMask) & ~kSuperPageOffsetMask;
  munmap(raw, probe_size);
  return aligned;
}
#endif  // PA_TEST_BOTTOM_UP

// Snapshots the base address of every free super page across all configured
// regions and immediately releases them back. Because ReserveAlignedUnhinted
// may keep a misaligned exact mapping trimmed by one super page (e.g. 14 MiB +
// 2 MiB instead of one 16 MiB region), tests inspect the set of pages rather
// than assuming a single contiguous reservation.
std::vector<uintptr_t> CollectRegionSuperPages(size_t total_bytes) {
  std::vector<uintptr_t> pages;
  const size_t n = total_bytes / kSuperPageSize;
  pages.reserve(n);
  for (size_t i = 0; i < n; ++i) {
    uintptr_t p = SuperPageRegionReserve(kSuperPageSize);
    EXPECT_NE(p, 0u);
    if (p) {
      pages.push_back(p);
    }
  }
  for (uintptr_t p : pages) {
    EXPECT_TRUE(SuperPageRegionRelease(p, kSuperPageSize));
  }
  return pages;
}

bool ContainsInSuperPages(const std::vector<uintptr_t>& pages, uintptr_t addr) {
  const uintptr_t sp = addr & ~kSuperPageOffsetMask;
  return std::find(pages.begin(), pages.end(), sp) != pages.end();
}

#if PA_BUILDFLAG(IS_LINUX) || PA_BUILDFLAG(IS_ANDROID)
constexpr uint8_t kRWXMask =
    base::debug::MappedMemoryRegion::READ |
    base::debug::MappedMemoryRegion::WRITE |
    base::debug::MappedMemoryRegion::EXECUTE;
bool RangeIsMappedProtNone(uintptr_t start,
                           uintptr_t end,
                           bool* saw_region_name) {
  std::string proc_maps;
  if (!base::debug::ReadProcMaps(&proc_maps)) {
    return false;
  }
  std::vector<base::debug::MappedMemoryRegion> regions;
  if (!base::debug::ParseProcMaps(proc_maps, &regions)) {
    return false;
  }
  uintptr_t covered = start;
  for (const auto& r : regions) {
    if (r.end <= covered || r.start >= end) {
      continue;
    }
    if (r.start > covered) {
      return false;
    }
    if ((r.permissions & kRWXMask) != 0) {
      return false;
    }
    if (saw_region_name &&
        r.path.find("pa-superpage-region") != std::string::npos) {
      *saw_region_name = true;
    }
    covered = std::min(r.end, end);
    if (covered == end) {
      return true;
    }
  }
  return covered == end;
}
#endif  // PA_BUILDFLAG(IS_LINUX) || PA_BUILDFLAG(IS_ANDROID)

class SuperPageRegion32BitTest : public testing::Test {
 protected:
  void SetUp() override { ResetSuperPageRegionsForTesting(); }
  void TearDown() override { ResetSuperPageRegionsForTesting(); }
};

TEST_F(SuperPageRegion32BitTest, DisabledByDefault) {
  const SuperPageRegionStats stats = GetSuperPageRegionStats();
  EXPECT_EQ(stats.regions, 0u);
  EXPECT_EQ(stats.reserved_bytes, 0u);
  EXPECT_EQ(stats.used_bytes, 0u);
  EXPECT_EQ(stats.fallback_super_pages, 0u);

  EXPECT_EQ(SuperPageRegionReserve(kSuperPageSize), 0u);
  EXPECT_FALSE(SuperPageRegionRelease(kSuperPageSize, kSuperPageSize));

  // Fallback counter is ignored while regions are disabled.
  SuperPageRegionNoteFallback(3);
  EXPECT_EQ(GetSuperPageRegionStats().fallback_super_pages, 0u);
}

TEST_F(SuperPageRegion32BitTest, ConfigureInitialRegionAlignedAndProtNone) {
  constexpr size_t kInitial = 16 * kMiB;
  const size_t reserved =
      ConfigureSuperPageRegionsFor32Bit(kInitial, 16 * kMiB, 64 * kMiB);
  EXPECT_EQ(reserved, kInitial);

  const SuperPageRegionStats stats = GetSuperPageRegionStats();
  // Exact-size mmap may split a misaligned 16 MiB request into 14 MiB + 2 MiB.
  EXPECT_GE(stats.regions, 1u);
  EXPECT_LE(stats.regions, 2u);
  EXPECT_EQ(stats.reserved_bytes, kInitial);
  EXPECT_EQ(stats.used_bytes, 0u);
  EXPECT_EQ(stats.fallback_super_pages, 0u);

  // Check every super page in the initial reservation is aligned, mapped, and
  // PROT_NONE.
  std::vector<uintptr_t> pages = CollectRegionSuperPages(kInitial);
  ASSERT_EQ(pages.size(), kInitial / kSuperPageSize);
  for (uintptr_t page : pages) {
    EXPECT_EQ(page & kSuperPageOffsetMask, 0u);
    EXPECT_TRUE(IsMapped(page, kSuperPageSize));
#if PA_BUILDFLAG(IS_LINUX) || PA_BUILDFLAG(IS_ANDROID)
    EXPECT_TRUE(RangeIsMappedProtNone(page, page + kSuperPageSize, nullptr));
#endif
  }

  // First super page carries PROT_NONE; after releasing, the trailing pages in
  // a freshly reserved region keep the [anon:pa-superpage-region] name when
  // PR_SET_VMA_ANON_NAME is supported.
#if PA_BUILDFLAG(IS_LINUX) || PA_BUILDFLAG(IS_ANDROID)
  ResetSuperPageRegionsForTesting();
  ASSERT_EQ(ConfigureSuperPageRegionsFor32Bit(kInitial, 16 * kMiB, 64 * kMiB),
            kInitial);
  const uintptr_t base = SuperPageRegionReserve(kSuperPageSize);
  ASSERT_NE(base, 0u);
  EXPECT_TRUE(SuperPageRegionRelease(base, kSuperPageSize));
  const size_t min_first_region = kInitial - kSuperPageSize;
  bool tail_named = false;
  EXPECT_TRUE(RangeIsMappedProtNone(base + kSuperPageSize,
                                    base + min_first_region, &tail_named));
#endif
}

TEST_F(SuperPageRegion32BitTest, ReserveFirstFitGrowthAndMaxCap) {
  constexpr size_t kStep = 16 * kMiB;
  constexpr size_t kMax = 64 * kMiB;
  constexpr size_t kPagesPerStep = kStep / kSuperPageSize;
  constexpr size_t kMaxPages = kMax / kSuperPageSize;
  ASSERT_EQ(ConfigureSuperPageRegionsFor32Bit(kStep, kStep, kMax), kStep);
  const size_t initial_regions = GetSuperPageRegionStats().regions;
  EXPECT_GE(initial_regions, 1u);
  EXPECT_LE(initial_regions, 2u);

  std::vector<uintptr_t> pages;
  pages.reserve(kMaxPages);

  // Fill the initial 16 MiB (8 super pages): consecutive within region 0
  // (at least kPagesPerStep - 1 = 7 consecutive pages when trimmed to 14 MiB).
  for (size_t i = 0; i < kPagesPerStep; ++i) {
    uintptr_t addr = SuperPageRegionReserve(kSuperPageSize);
    ASSERT_NE(addr, 0u);
    EXPECT_EQ(addr & kSuperPageOffsetMask, 0u);
    if (i > 0 && i < kPagesPerStep - 1) {
      EXPECT_EQ(addr, pages[0] + i * kSuperPageSize);
    }
    pages.push_back(addr);
  }
  {
    SuperPageRegionStats stats = GetSuperPageRegionStats();
    EXPECT_EQ(stats.regions, initial_regions);
    EXPECT_EQ(stats.reserved_bytes, kStep);
    EXPECT_EQ(stats.used_bytes, kStep);
  }

  // Next reservation exhausts the initial reservation and triggers one step
  // growth (14 MiB if the exact 16 MiB mmap was misaligned, or 16 MiB).
  uintptr_t p8 = SuperPageRegionReserve(kSuperPageSize);
  ASSERT_NE(p8, 0u);
  EXPECT_EQ(p8 & kSuperPageOffsetMask, 0u);
  pages.push_back(p8);
  {
    SuperPageRegionStats stats = GetSuperPageRegionStats();
    EXPECT_EQ(stats.regions, initial_regions + 1u);
    EXPECT_GE(stats.reserved_bytes, 2 * kStep - kSuperPageSize);
    EXPECT_LE(stats.reserved_bytes, 2 * kStep);
    EXPECT_EQ(stats.used_bytes, kStep + kSuperPageSize);
  }

  // Fill through max (64 MiB total).
  while (pages.size() < kMaxPages) {
    uintptr_t addr = SuperPageRegionReserve(kSuperPageSize);
    ASSERT_NE(addr, 0u);
    EXPECT_EQ(addr & kSuperPageOffsetMask, 0u);
    pages.push_back(addr);
  }
  const size_t max_regions = GetSuperPageRegionStats().regions;
  {
    SuperPageRegionStats stats = GetSuperPageRegionStats();
    EXPECT_GE(stats.regions, 4u);
    EXPECT_EQ(stats.reserved_bytes, kMax);
    EXPECT_EQ(stats.used_bytes, kMax);
  }

  // At max cap, further reservations return 0 and do not grow.
  EXPECT_EQ(SuperPageRegionReserve(kSuperPageSize), 0u);
  {
    SuperPageRegionStats stats = GetSuperPageRegionStats();
    EXPECT_EQ(stats.regions, max_regions);
    EXPECT_EQ(stats.reserved_bytes, kMax);
    EXPECT_EQ(stats.used_bytes, kMax);
  }

  for (uintptr_t addr : pages) {
    EXPECT_TRUE(SuperPageRegionRelease(addr, kSuperPageSize));
  }
  EXPECT_EQ(GetSuperPageRegionStats().used_bytes, 0u);
  EXPECT_EQ(GetSuperPageRegionStats().reserved_bytes, kMax);
}

TEST_F(SuperPageRegion32BitTest, ZeroStepPreventsGrowth) {
  constexpr size_t kInitial = 16 * kMiB;
  constexpr size_t kPages = kInitial / kSuperPageSize;
  ASSERT_EQ(ConfigureSuperPageRegionsFor32Bit(kInitial, /*step_bytes=*/0,
                                              64 * kMiB),
            kInitial);
  const size_t initial_regions = GetSuperPageRegionStats().regions;
  EXPECT_GE(initial_regions, 1u);
  EXPECT_LE(initial_regions, 2u);

  std::vector<uintptr_t> pages;
  for (size_t i = 0; i < kPages; ++i) {
    uintptr_t addr = SuperPageRegionReserve(kSuperPageSize);
    ASSERT_NE(addr, 0u);
    pages.push_back(addr);
  }

  EXPECT_EQ(SuperPageRegionReserve(kSuperPageSize), 0u);
  const SuperPageRegionStats stats = GetSuperPageRegionStats();
  EXPECT_EQ(stats.regions, initial_regions);
  EXPECT_EQ(stats.reserved_bytes, kInitial);
  EXPECT_EQ(stats.used_bytes, kInitial);

  for (uintptr_t addr : pages) {
    EXPECT_TRUE(SuperPageRegionRelease(addr, kSuperPageSize));
  }
  EXPECT_EQ(GetSuperPageRegionStats().used_bytes, 0u);
}

TEST_F(SuperPageRegion32BitTest, ZeroInitialGrowsOnDemandAndRoundsDownSizes) {
  // Zero initial reserves nothing until the first request.
  EXPECT_EQ(ConfigureSuperPageRegionsFor32Bit(0, 16 * kMiB, 32 * kMiB), 0u);
  EXPECT_EQ(GetSuperPageRegionStats().regions, 0u);
  EXPECT_EQ(GetSuperPageRegionStats().reserved_bytes, 0u);

  uintptr_t p = SuperPageRegionReserve(kSuperPageSize);
  ASSERT_NE(p, 0u);
  EXPECT_EQ(GetSuperPageRegionStats().regions, 1u);
  // Misaligned exact 16 MiB placement keeps 14 MiB; aligned keeps 16 MiB.
  const size_t reserved_step = GetSuperPageRegionStats().reserved_bytes;
  EXPECT_TRUE(reserved_step == 14 * kMiB || reserved_step == 16 * kMiB)
      << "reserved_step=" << reserved_step;
  EXPECT_EQ(GetSuperPageRegionStats().used_bytes, kSuperPageSize);
  EXPECT_TRUE(SuperPageRegionRelease(p, kSuperPageSize));

  ResetSuperPageRegionsForTesting();

  // Unaligned sizes are rounded down to a multiple of kSuperPageSize (2 MiB).
  const size_t rounded = ConfigureSuperPageRegionsFor32Bit(
      4 * kMiB + 1234, /*step_bytes=*/0, 4 * kMiB + 5678);
  EXPECT_EQ(rounded, 4 * kMiB);
  EXPECT_EQ(GetSuperPageRegionStats().reserved_bytes, 4 * kMiB);

  // Invalid reservation sizes return 0 safely.
  EXPECT_EQ(SuperPageRegionReserve(0), 0u);
  EXPECT_EQ(SuperPageRegionReserve(257 * kSuperPageSize), 0u);
}

TEST_F(SuperPageRegion32BitTest, MultiPageReserveAcrossFragmentedRegion) {
  constexpr size_t kStep = 16 * kMiB;
  constexpr size_t kPages = kStep / kSuperPageSize;
  ASSERT_EQ(ConfigureSuperPageRegionsFor32Bit(kStep, kStep, 32 * kMiB), kStep);
  const size_t initial_regions = GetSuperPageRegionStats().regions;

  std::vector<uintptr_t> pages;
  for (size_t i = 0; i < kPages; ++i) {
    pages.push_back(SuperPageRegionReserve(kSuperPageSize));
    ASSERT_NE(pages.back(), 0u);
  }
  // Free alternating pages so 4 are free, none contiguous within the same
  // region.
  for (size_t i = 1; i < kPages; i += 2) {
    EXPECT_TRUE(SuperPageRegionRelease(pages[i], kSuperPageSize));
  }

  // A 2-super-page span cannot fit in existing regions, so it grows a new one
  // (14 MiB if misaligned, 16 MiB if aligned).
  uintptr_t span = SuperPageRegionReserve(2 * kSuperPageSize);
  ASSERT_NE(span, 0u);
  EXPECT_EQ(span & kSuperPageOffsetMask, 0u);
  EXPECT_EQ(GetSuperPageRegionStats().regions, initial_regions + 1u);
  const size_t reserved = GetSuperPageRegionStats().reserved_bytes;
  EXPECT_TRUE(reserved == 30 * kMiB || reserved == 32 * kMiB)
      << "reserved=" << reserved;

  EXPECT_TRUE(SuperPageRegionRelease(span, 2 * kSuperPageSize));
  for (size_t i = 0; i < kPages; i += 2) {
    EXPECT_TRUE(SuperPageRegionRelease(pages[i], kSuperPageSize));
  }
  EXPECT_EQ(GetSuperPageRegionStats().used_bytes, 0u);
}

TEST_F(SuperPageRegion32BitTest, ReleaseDecommitsZeroesAndReusesBit) {
  constexpr size_t kInitial = 16 * kMiB;
  ASSERT_EQ(ConfigureSuperPageRegionsFor32Bit(kInitial, 0, kInitial), kInitial);

  const uintptr_t p0 = SuperPageRegionReserve(kSuperPageSize);
  const uintptr_t p1 = SuperPageRegionReserve(kSuperPageSize);
  ASSERT_NE(p0, 0u);
  ASSERT_EQ(p1, p0 + kSuperPageSize);

  // Commit the first system page inside `p0` and fill it with non-zero bytes.
  const size_t sys_page = SystemPageSize();
  ASSERT_EQ(
      mprotect(reinterpret_cast<void*>(p0), sys_page, PROT_READ | PROT_WRITE),
      0);
  memset(reinterpret_cast<void*>(p0), 0xA5, sys_page);
  EXPECT_EQ(reinterpret_cast<const uint8_t*>(p0)[0], 0xA5);
  EXPECT_EQ(reinterpret_cast<const uint8_t*>(p0)[sys_page - 1], 0xA5);

  // Release decommits `p0` in place (remains mapped, PROT_NONE, zeroed).
  EXPECT_TRUE(SuperPageRegionRelease(p0, kSuperPageSize));
  EXPECT_EQ(GetSuperPageRegionStats().used_bytes, kSuperPageSize);
  EXPECT_EQ(GetSuperPageRegionStats().reserved_bytes, kInitial);
  EXPECT_TRUE(IsMapped(p0, kSuperPageSize));
#if PA_BUILDFLAG(IS_LINUX) || PA_BUILDFLAG(IS_ANDROID)
  EXPECT_TRUE(RangeIsMappedProtNone(p0, p0 + kSuperPageSize, nullptr));
#endif

  // Outside address returns false and does not touch the region.
  void* foreign = mmap(nullptr, kSuperPageSize, PROT_NONE,
                       MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  ASSERT_NE(foreign, MAP_FAILED);
  EXPECT_FALSE(SuperPageRegionRelease(reinterpret_cast<uintptr_t>(foreign),
                                      kSuperPageSize));
  munmap(foreign, kSuperPageSize);

  // Re-reserving picks up `p0` again (first fit), and its bytes are zeroed.
  const uintptr_t reused = SuperPageRegionReserve(kSuperPageSize);
  EXPECT_EQ(reused, p0);
  ASSERT_EQ(
      mprotect(reinterpret_cast<void*>(reused), sys_page, PROT_READ | PROT_WRITE),
      0);
  const uint8_t* bytes = reinterpret_cast<const uint8_t*>(reused);
  for (size_t i = 0; i < sys_page; ++i) {
    ASSERT_EQ(bytes[i], 0u) << "non-zero byte at offset " << i;
  }

  EXPECT_TRUE(SuperPageRegionRelease(reused, kSuperPageSize));
  EXPECT_TRUE(SuperPageRegionRelease(p1, kSuperPageSize));
  EXPECT_EQ(GetSuperPageRegionStats().used_bytes, 0u);
}

#if PA_USE_DEATH_TESTS()
using SuperPageRegion32BitDeathTest = SuperPageRegion32BitTest;

TEST_F(SuperPageRegion32BitDeathTest, DoubleReleaseCheckFails) {
  ASSERT_EQ(ConfigureSuperPageRegionsFor32Bit(16 * kMiB, 0, 16 * kMiB),
            16 * kMiB);
  const uintptr_t p = SuperPageRegionReserve(kSuperPageSize);
  ASSERT_NE(p, 0u);
  ASSERT_TRUE(SuperPageRegionRelease(p, kSuperPageSize));

  EXPECT_DEATH_IF_SUPPORTED(SuperPageRegionRelease(p, kSuperPageSize), "");
}

#if PA_TEST_BOTTOM_UP
TEST_F(SuperPageRegion32BitDeathTest, ConfigureRegionThenBottomUpChecks) {
  const uintptr_t window = ProbeFreeAlignedRange(2 * kSuperPageSize);
  ASSERT_NE(window, 0u);
  ASSERT_EQ(ConfigureSuperPageRegionsFor32Bit(4 * kMiB, 0, 4 * kMiB), 4 * kMiB);
  EXPECT_DEATH_IF_SUPPORTED(
      ConfigureBottomUpSuperPagesFor32Bit(window, window + 2 * kSuperPageSize),
      "");
}

TEST_F(SuperPageRegion32BitDeathTest, ConfigureBottomUpThenRegionChecks) {
  const uintptr_t window = ProbeFreeAlignedRange(2 * kSuperPageSize);
  ASSERT_NE(window, 0u);
  ASSERT_TRUE(
      ConfigureBottomUpSuperPagesFor32Bit(window, window + 2 * kSuperPageSize));
  EXPECT_DEATH_IF_SUPPORTED(
      ConfigureSuperPageRegionsFor32Bit(4 * kMiB, 0, 4 * kMiB), "");
}
#endif  // PA_TEST_BOTTOM_UP
#endif  // PA_USE_DEATH_TESTS()

TEST_F(SuperPageRegion32BitTest,
       PartitionRootNormalBucketsUseRegionAndDestructReleasesInPlace) {
  constexpr size_t kRegionSize = 16 * kMiB;
  ASSERT_EQ(ConfigureSuperPageRegionsFor32Bit(kRegionSize, 0, kRegionSize),
            kRegionSize);
  const size_t initial_regions = GetSuperPageRegionStats().regions;
  const std::vector<uintptr_t> region_pages =
      CollectRegionSuperPages(kRegionSize);
  ASSERT_EQ(region_pages.size(), kRegionSize / kSuperPageSize);

  // Default PartitionOptions disables BRP and thread cache, using the regular
  // pool (kRegularPoolHandle).
  PartitionOptions opts;
  ASSERT_EQ(opts.backup_ref_ptr, PartitionOptions::kDisabled);
  PartitionAllocator allocator(opts);
  PartitionRoot* root = allocator.root();
  ASSERT_EQ(root->ChoosePool(), kRegularPoolHandle);

  // Allocate ~6 MiB in 64 KiB normal-bucket objects to span several super
  // pages while staying below the 16 MiB region capacity.
  constexpr size_t kAllocSize = 64 * 1024;
  constexpr size_t kAllocCount = 96;  // 6 MiB payload -> >= 4 super pages
  std::vector<void*> ptrs;
  ptrs.reserve(kAllocCount);
  for (size_t i = 0; i < kAllocCount; ++i) {
    void* p = root->Alloc(kAllocSize);
    ASSERT_NE(p, nullptr);
    memset(p, static_cast<int>(i & 0xFF), 64);
    const uintptr_t addr = reinterpret_cast<uintptr_t>(p);
    EXPECT_TRUE(ContainsInSuperPages(region_pages, addr));
    EXPECT_TRUE(IsManagedByPartitionAlloc(addr));
    EXPECT_TRUE(IsManagedByPartitionAllocRegularPool(addr));
    ptrs.push_back(p);
  }

  const SuperPageRegionStats active_stats = GetSuperPageRegionStats();
  EXPECT_EQ(active_stats.regions, initial_regions);
  EXPECT_EQ(active_stats.reserved_bytes, kRegionSize);
  EXPECT_GE(active_stats.used_bytes, 3 * kSuperPageSize);
  EXPECT_LT(active_stats.used_bytes, kRegionSize);
  EXPECT_EQ(active_stats.fallback_super_pages, 0u);

  // Unused tail super pages inside the region must NOT be marked as managed by
  // PartitionAlloc.
  const size_t used_count = active_stats.used_bytes / kSuperPageSize;
  for (size_t i = used_count; i < region_pages.size(); ++i) {
    const uintptr_t unused = region_pages[i];
    EXPECT_FALSE(IsManagedByPartitionAlloc(unused));
    EXPECT_FALSE(IsManagedByPartitionAlloc(unused + SystemPageSize()));
    EXPECT_FALSE(IsManagedByPartitionAlloc(unused + kSuperPageSize / 2));
  }

  // Free all allocations, then explicitly destruct the root via
  // DestructForTesting() and confirm the region pages are released in place
  // (still mapped PROT_NONE, no munmap).
  for (void* p : ptrs) {
    root->Free(p);
  }
  {
    ScopedGuard guard{PartitionRootLock(root)};
    root->DestructForTesting();
  }

  const SuperPageRegionStats after_stats = GetSuperPageRegionStats();
  EXPECT_EQ(after_stats.regions, initial_regions);
  EXPECT_EQ(after_stats.reserved_bytes, kRegionSize);
  EXPECT_EQ(after_stats.used_bytes, 0u);
  for (uintptr_t page : region_pages) {
    EXPECT_TRUE(IsMapped(page, kSuperPageSize));
#if PA_BUILDFLAG(IS_LINUX) || PA_BUILDFLAG(IS_ANDROID)
    EXPECT_TRUE(RangeIsMappedProtNone(page, page + kSuperPageSize, nullptr));
#endif
    EXPECT_FALSE(IsManagedByPartitionAlloc(page));
  }
}

TEST_F(SuperPageRegion32BitTest, PartitionRootFallsBackWhenRegionExhausted) {
  constexpr size_t kRegionSize = 4 * kMiB;  // 2 super pages, no growth.
  ASSERT_EQ(ConfigureSuperPageRegionsFor32Bit(kRegionSize, 0, kRegionSize),
            kRegionSize);
  const std::vector<uintptr_t> region_pages =
      CollectRegionSuperPages(kRegionSize);
  ASSERT_EQ(region_pages.size(), kRegionSize / kSuperPageSize);

  PartitionAllocator allocator(PartitionOptions{});
  PartitionRoot* root = allocator.root();
  ASSERT_EQ(root->ChoosePool(), kRegularPoolHandle);

  // Allocate > 4 MiB so the root fills both region super pages and must fall
  // back to normal per-page reservation for at least one extra super page.
  constexpr size_t kAllocSize = 64 * 1024;
  constexpr size_t kAllocCount = 96;  // 6 MiB payload
  std::vector<void*> ptrs;
  ptrs.reserve(kAllocCount);
  size_t in_region = 0;
  size_t outside_region = 0;
  for (size_t i = 0; i < kAllocCount; ++i) {
    void* p = root->Alloc(kAllocSize);
    ASSERT_NE(p, nullptr);
    const uintptr_t addr = reinterpret_cast<uintptr_t>(p);
    EXPECT_TRUE(IsManagedByPartitionAlloc(addr));
    if (ContainsInSuperPages(region_pages, addr)) {
      ++in_region;
    } else {
      ++outside_region;
    }
    ptrs.push_back(p);
  }

  EXPECT_GT(in_region, 0u);
  EXPECT_GT(outside_region, 0u);
  const SuperPageRegionStats stats = GetSuperPageRegionStats();
  EXPECT_EQ(stats.used_bytes, kRegionSize);
  EXPECT_GE(stats.fallback_super_pages, 1u);

  for (void* p : ptrs) {
    root->Free(p);
  }
  {
    ScopedGuard guard{PartitionRootLock(root)};
    root->DestructForTesting();
  }
  EXPECT_EQ(GetSuperPageRegionStats().used_bytes, 0u);
}

TEST_F(SuperPageRegion32BitTest, ConfigureRecordsPreexistingSuperPages) {
  // A root that already holds a super page before configuration.
  PartitionAllocator allocator(PartitionOptions{});
  PartitionRoot* root = allocator.root();
  void* p = root->Alloc(64);
  ASSERT_NE(p, nullptr);

  ASSERT_EQ(ConfigureSuperPageRegionsFor32Bit(0, 16 * kMiB, 64 * kMiB), 0u);
  EXPECT_GE(GetSuperPageRegionStats().preexisting_super_pages, 1u);
  ResetSuperPageRegionsForTesting();
  EXPECT_EQ(GetSuperPageRegionStats().preexisting_super_pages, 0u);

#if PA_TEST_BOTTOM_UP
  const uintptr_t base = ProbeFreeAlignedRange(8 * kMiB);
  ASSERT_NE(base, 0u);
  ASSERT_TRUE(ConfigureBottomUpSuperPagesFor32Bit(base, base + 8 * kMiB));
  EXPECT_GE(GetSuperPageRegionStats().preexisting_super_pages, 1u);
  ResetSuperPageRegionsForTesting();
#endif  // PA_TEST_BOTTOM_UP

  root->Free(p);
  {
    ScopedGuard guard{PartitionRootLock(root)};
    root->DestructForTesting();
  }
}

TEST_F(SuperPageRegion32BitTest, RegionReservationsCountAsMapped) {
  const size_t mapped_before = GetTotalMappedSize();
  const size_t reserved =
      ConfigureSuperPageRegionsFor32Bit(8 * kMiB, 0, 8 * kMiB);
  ASSERT_GT(reserved, 0u);
  EXPECT_EQ(GetTotalMappedSize(), mapped_before + reserved);
  // Handing out and releasing region pages maps nothing new.
  const uintptr_t p = SuperPageRegionReserve(kSuperPageSize);
  ASSERT_NE(p, 0u);
  EXPECT_EQ(GetTotalMappedSize(), mapped_before + reserved);
  EXPECT_TRUE(SuperPageRegionRelease(p, kSuperPageSize));
  EXPECT_EQ(GetTotalMappedSize(), mapped_before + reserved);
  ResetSuperPageRegionsForTesting();
  EXPECT_EQ(GetTotalMappedSize(), mapped_before);
}

TEST_F(SuperPageRegion32BitTest, ResetKeepsRegionsWithLivePages) {
  ASSERT_EQ(ConfigureSuperPageRegionsFor32Bit(4 * kMiB, 0, 4 * kMiB),
            4 * kMiB);
  const uintptr_t p = SuperPageRegionReserve(kSuperPageSize);
  ASSERT_NE(p, 0u);
  ResetSuperPageRegionsForTesting();
  // The live page is still mapped, and is freed like any other reservation.
  EXPECT_TRUE(IsMapped(p, SystemPageSize()));
  EXPECT_FALSE(SuperPageRegionRelease(p, kSuperPageSize));
  EXPECT_EQ(SuperPageRegionReserve(kSuperPageSize), 0u);
  FreePages(p, kSuperPageSize);
}

TEST_F(SuperPageRegion32BitTest, DirectMapsBypassRegion) {
  constexpr size_t kRegionSize = 16 * kMiB;
  ASSERT_EQ(ConfigureSuperPageRegionsFor32Bit(kRegionSize, 0, kRegionSize),
            kRegionSize);
  const std::vector<uintptr_t> region_pages =
      CollectRegionSuperPages(kRegionSize);

  PartitionAllocator allocator(PartitionOptions{});
  PartitionRoot* root = allocator.root();
  ASSERT_EQ(root->ChoosePool(), kRegularPoolHandle);

  // PartitionAlloc's bucket limit on 32-bit is 983,040 B; allocate 1 MiB +
  // 64 KiB (> 983,040 B) so it takes the direct-map path.
  constexpr size_t kDirectMapSize = kMiB + 64 * 1024;
  void* direct = root->Alloc(kDirectMapSize);
  ASSERT_NE(direct, nullptr);
  const uintptr_t addr = reinterpret_cast<uintptr_t>(direct);
  EXPECT_TRUE(IsManagedByDirectMap(addr));
  EXPECT_FALSE(ContainsInSuperPages(region_pages, addr));
  EXPECT_TRUE(IsManagedByPartitionAlloc(addr));

  const SuperPageRegionStats stats = GetSuperPageRegionStats();
  EXPECT_EQ(stats.used_bytes, 0u);
  EXPECT_EQ(stats.fallback_super_pages, 0u);

  root->Free(direct);
  {
    ScopedGuard guard{PartitionRootLock(root)};
    root->DestructForTesting();
  }
}

TEST_F(SuperPageRegion32BitTest, ResetUnmapsRegionsAndDisables) {
  constexpr size_t kRegionSize = 16 * kMiB;
  ASSERT_EQ(ConfigureSuperPageRegionsFor32Bit(kRegionSize, 16 * kMiB,
                                              32 * kMiB),
            kRegionSize);

  const uintptr_t region_begin = SuperPageRegionReserve(kSuperPageSize);
  ASSERT_NE(region_begin, 0u);
  ASSERT_TRUE(SuperPageRegionRelease(region_begin, kSuperPageSize));
  ASSERT_TRUE(IsMapped(region_begin, SystemPageSize()));

  ResetSuperPageRegionsForTesting();

  const SuperPageRegionStats stats = GetSuperPageRegionStats();
  EXPECT_EQ(stats.regions, 0u);
  EXPECT_EQ(stats.reserved_bytes, 0u);
  EXPECT_EQ(stats.used_bytes, 0u);
  EXPECT_EQ(stats.fallback_super_pages, 0u);
  EXPECT_EQ(SuperPageRegionReserve(kSuperPageSize), 0u);

  errno = 0;
  EXPECT_EQ(
      msync(reinterpret_cast<void*>(region_begin), SystemPageSize(), MS_ASYNC),
      -1);
  EXPECT_EQ(errno, ENOMEM);
}

TEST_F(SuperPageRegion32BitTest,
       GrowthBackoffRetriesEvery32ndMissAndClearsOnRelease) {
  constexpr size_t kStep = 16 * kMiB;
  constexpr size_t kMax = 64 * kMiB;
  constexpr size_t kPagesPerStep = kStep / kSuperPageSize;
  ASSERT_EQ(ConfigureSuperPageRegionsFor32Bit(kStep, kStep, kMax), kStep);
  const size_t initial_regions = GetSuperPageRegionStats().regions;

  std::vector<uintptr_t> pages;
  for (size_t i = 0; i < kPagesPerStep; ++i) {
    uintptr_t p = SuperPageRegionReserve(kSuperPageSize);
    ASSERT_NE(p, 0u);
    pages.push_back(p);
  }
  const size_t initial_attempts = GetSuperPageRegionReserveAttemptsForTesting();

  // First failed growth sets g_growth_failed.
  SetSuperPageRegionReserveFailForTesting(true);
  EXPECT_EQ(SuperPageRegionReserve(kSuperPageSize), 0u);
  const size_t attempts_after_fail =
      GetSuperPageRegionReserveAttemptsForTesting();
  EXPECT_GT(attempts_after_fail, initial_attempts);
  EXPECT_EQ(GetSuperPageRegionStats().regions, initial_regions);

  // Re-enable reserve so any retry would succeed; misses 1..31 must skip it.
  SetSuperPageRegionReserveFailForTesting(false);
  for (size_t miss = 1; miss <= 31; ++miss) {
    EXPECT_EQ(SuperPageRegionReserve(kSuperPageSize), 0u)
        << "unexpected retry on miss " << miss;
    EXPECT_EQ(GetSuperPageRegionReserveAttemptsForTesting(),
              attempts_after_fail);
    EXPECT_EQ(GetSuperPageRegionStats().regions, initial_regions);
  }

  // 32nd miss retries AddRegionLocked and grows one region (14 MiB or 16 MiB).
  uintptr_t grown = SuperPageRegionReserve(kSuperPageSize);
  ASSERT_NE(grown, 0u);
  pages.push_back(grown);
  EXPECT_EQ(GetSuperPageRegionStats().regions, initial_regions + 1u);
  const size_t reserved_after_growth = GetSuperPageRegionStats().reserved_bytes;
  EXPECT_GE(reserved_after_growth, 2 * kStep - kSuperPageSize);
  EXPECT_LE(reserved_after_growth, 2 * kStep);
  EXPECT_EQ(GetSuperPageRegionReserveAttemptsForTesting(),
            attempts_after_fail + 1u);

  // Fill through the new region, force another failure, then release one single
  // page so a 2-super-page span still misses the freed hole and retries growth
  // right away (without waiting 32 misses).
  while (GetSuperPageRegionStats().used_bytes < reserved_after_growth) {
    uintptr_t p = SuperPageRegionReserve(kSuperPageSize);
    ASSERT_NE(p, 0u);
    pages.push_back(p);
  }
  SetSuperPageRegionReserveFailForTesting(true);
  EXPECT_EQ(SuperPageRegionReserve(kSuperPageSize), 0u);
  const size_t attempts_after_second_fail =
      GetSuperPageRegionReserveAttemptsForTesting();
  SetSuperPageRegionReserveFailForTesting(false);
  EXPECT_EQ(SuperPageRegionReserve(kSuperPageSize), 0u);
  EXPECT_EQ(GetSuperPageRegionReserveAttemptsForTesting(),
            attempts_after_second_fail);

  // Release one single page -> clears g_growth_failed. A 2-page reserve can't
  // fit in the 1-page hole, so it immediately grows a new region.
  uintptr_t released = pages.back();
  pages.pop_back();
  ASSERT_TRUE(SuperPageRegionRelease(released, kSuperPageSize));
  uintptr_t span = SuperPageRegionReserve(2 * kSuperPageSize);
  ASSERT_NE(span, 0u);
  EXPECT_EQ(GetSuperPageRegionStats().regions, initial_regions + 2u);
  EXPECT_EQ(GetSuperPageRegionReserveAttemptsForTesting(),
            attempts_after_second_fail + 1u);

  EXPECT_TRUE(SuperPageRegionRelease(span, 2 * kSuperPageSize));
  for (uintptr_t addr : pages) {
    EXPECT_TRUE(SuperPageRegionRelease(addr, kSuperPageSize));
  }
  EXPECT_EQ(GetSuperPageRegionStats().used_bytes, 0u);
}

TEST_F(SuperPageRegion32BitTest, ResetClearsGrowthBackoffState) {
  constexpr size_t kStep = 16 * kMiB;
  // Live super pages keep their region mapped across a reset, so release
  // them first.
  std::vector<uintptr_t> pages;
  auto release_all = [&pages] {
    for (uintptr_t p : pages) {
      EXPECT_TRUE(SuperPageRegionRelease(p, kSuperPageSize));
    }
    pages.clear();
  };
  ASSERT_EQ(ConfigureSuperPageRegionsFor32Bit(kStep, kStep, 32 * kMiB), kStep);
  for (size_t i = 0; i < kStep / kSuperPageSize; ++i) {
    pages.push_back(SuperPageRegionReserve(kSuperPageSize));
    ASSERT_NE(pages.back(), 0u);
  }
  SetSuperPageRegionReserveFailForTesting(true);
  EXPECT_EQ(SuperPageRegionReserve(kSuperPageSize), 0u);

  release_all();
  ResetSuperPageRegionsForTesting();

  ASSERT_EQ(ConfigureSuperPageRegionsFor32Bit(kStep, kStep, 32 * kMiB), kStep);
  const size_t initial_regions = GetSuperPageRegionStats().regions;
  for (size_t i = 0; i < kStep / kSuperPageSize; ++i) {
    pages.push_back(SuperPageRegionReserve(kSuperPageSize));
    ASSERT_NE(pages.back(), 0u);
  }
  pages.push_back(SuperPageRegionReserve(kSuperPageSize));
  EXPECT_NE(pages.back(), 0u);
  EXPECT_EQ(GetSuperPageRegionStats().regions, initial_regions + 1u);
  release_all();
}

TEST_F(SuperPageRegion32BitTest, ReserveAlignedUnhintedTrimsMisalignedExactHole) {
  // Carve an exact 16 MiB hole inside a 20 MiB top-down slab at a non-2 MiB-
  // aligned address (offset by one system page). An over-allocating mmap
  // (18 MiB - 4 KiB) could not fit inside it.
  constexpr size_t kHoleSize = 16 * kMiB;
  constexpr size_t kSlabSize = kHoleSize + 2 * kSuperPageSize;
  const size_t sys_page = SystemPageSize();

  // Top-down mmap takes the highest free hole that fits, and earlier tests in
  // the process can leave a 16 MiB hole above the carved one. Until the new
  // region lands at `want`, block the hole it took and try again.
  std::vector<std::pair<uintptr_t, size_t>> blockers;
  auto reserve_into = [&blockers](uintptr_t want) -> uintptr_t {
    for (int attempt = 0; attempt < 32; ++attempt) {
      if (ConfigureSuperPageRegionsFor32Bit(0, kHoleSize, 32 * kMiB) != 0u) {
        return 0;
      }
      const uintptr_t got = SuperPageRegionReserve(kSuperPageSize);
      if (!got || got == want) {
        return got;
      }
      const size_t length = GetSuperPageRegionStats().reserved_bytes;
      EXPECT_TRUE(SuperPageRegionRelease(got, kSuperPageSize));
      ResetSuperPageRegionsForTesting();
      void* blocker = mmap(reinterpret_cast<void*>(got), length, PROT_NONE,
                           MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE, -1, 0);
      if (blocker == MAP_FAILED) {
        return 0;
      }
      blockers.emplace_back(reinterpret_cast<uintptr_t>(blocker), length);
    }
    return 0;
  };

  void* slab = mmap(nullptr, kSlabSize, PROT_NONE,
                    MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE, -1, 0);
  ASSERT_NE(slab, MAP_FAILED);
  const uintptr_t slab_begin = reinterpret_cast<uintptr_t>(slab);
  const uintptr_t slab_end = slab_begin + kSlabSize;

  // Place `hole_start` at a misaligned address with non-empty PROT_NONE guard
  // fences remaining mapped on both sides inside `slab`.
  const uintptr_t aligned_anchor =
      (slab_begin + kSuperPageOffsetMask) & ~kSuperPageOffsetMask;
  const uintptr_t base_anchor = (aligned_anchor == slab_begin)
                                    ? slab_begin
                                    : aligned_anchor - kSuperPageSize;
  const uintptr_t hole_start =
      (base_anchor >= slab_begin)
          ? base_anchor + sys_page
          : aligned_anchor + sys_page;
  const uintptr_t hole_end = hole_start + kHoleSize;
  ASSERT_GT(hole_start, slab_begin);
  ASSERT_LT(hole_end, slab_end);
  ASSERT_EQ(hole_start & kSuperPageOffsetMask, sys_page);
  const uintptr_t expected_aligned =
      (hole_start + kSuperPageOffsetMask) & ~kSuperPageOffsetMask;
  constexpr size_t kExpectedKept = kHoleSize - kSuperPageSize;  // 14 MiB.
  ASSERT_LE(expected_aligned + kExpectedKept, hole_end);

  ASSERT_EQ(munmap(reinterpret_cast<void*>(hole_start), kHoleSize), 0);

  const uintptr_t p = reserve_into(expected_aligned);
  ASSERT_NE(p, 0u);
  EXPECT_EQ(p & kSuperPageOffsetMask, 0u);

  const SuperPageRegionStats stats = GetSuperPageRegionStats();
  EXPECT_EQ(stats.regions, 1u);
  // Placed directly inside the exact misaligned 16 MiB hole: kept the 14 MiB
  // aligned interior and unmapped both pre- and post-slack edges.
  EXPECT_EQ(p, expected_aligned);
  EXPECT_EQ(stats.reserved_bytes, kExpectedKept);
  EXPECT_TRUE(IsMapped(expected_aligned, kExpectedKept));
  EXPECT_FALSE(IsMapped(hole_start, sys_page));
  EXPECT_FALSE(IsMapped(expected_aligned + kExpectedKept, sys_page));
  EXPECT_TRUE(IsMapped(slab_begin, hole_start - slab_begin));
  EXPECT_TRUE(IsMapped(hole_end, slab_end - hole_end));

  EXPECT_TRUE(SuperPageRegionRelease(p, kSuperPageSize));
  ResetSuperPageRegionsForTesting();
  ASSERT_EQ(munmap(reinterpret_cast<void*>(slab_begin), hole_start - slab_begin),
            0);
  ASSERT_EQ(munmap(reinterpret_cast<void*>(hole_end), slab_end - hole_end), 0);

  // Now carve an exact 2 MiB-aligned 16 MiB hole inside a fresh slab: the exact
  // placement succeeds with pre_slack == 0 and keeps the full 16 MiB.
  void* slab2 = mmap(nullptr, kSlabSize, PROT_NONE,
                     MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE, -1, 0);
  ASSERT_NE(slab2, MAP_FAILED);
  const uintptr_t s2_begin = reinterpret_cast<uintptr_t>(slab2);
  const uintptr_t s2_end = s2_begin + kSlabSize;
  uintptr_t aligned_hole = (s2_begin + sys_page + kSuperPageOffsetMask) &
                           ~kSuperPageOffsetMask;
  const uintptr_t aligned_hole_end = aligned_hole + kHoleSize;
  ASSERT_GT(aligned_hole, s2_begin);
  ASSERT_LT(aligned_hole_end, s2_end);
  ASSERT_EQ(munmap(reinterpret_cast<void*>(aligned_hole), kHoleSize), 0);

  const uintptr_t p_aligned = reserve_into(aligned_hole);
  ASSERT_EQ(p_aligned, aligned_hole);
  EXPECT_EQ(GetSuperPageRegionStats().regions, 1u);
  EXPECT_EQ(GetSuperPageRegionStats().reserved_bytes, kHoleSize);
  EXPECT_TRUE(IsMapped(aligned_hole, kHoleSize));

  EXPECT_TRUE(SuperPageRegionRelease(p_aligned, kSuperPageSize));
  ResetSuperPageRegionsForTesting();
  ASSERT_EQ(munmap(reinterpret_cast<void*>(s2_begin), aligned_hole - s2_begin),
            0);
  ASSERT_EQ(
      munmap(reinterpret_cast<void*>(aligned_hole_end), s2_end - aligned_hole_end),
      0);
  for (const auto& [address, length] : blockers) {
    munmap(reinterpret_cast<void*>(address), length);
  }
}

#if PA_TEST_BOTTOM_UP
TEST_F(SuperPageRegion32BitTest, BottomUpConsecutiveAscendingAndRounding) {
  const uintptr_t window = ProbeFreeAlignedRange(8 * kSuperPageSize);
  ASSERT_NE(window, 0u);
  const size_t mapped_before = GetTotalMappedSize();

  // Unaligned base rounds UP to next super page; unaligned limit rounds DOWN.
  ASSERT_TRUE(ConfigureBottomUpSuperPagesFor32Bit(
      window + 1234, window + 5 * kSuperPageSize + 4096));
  EXPECT_EQ(GetSuperPageRegionStats().bottom_up_cursor,
            window + kSuperPageSize);
  EXPECT_EQ(GetSuperPageRegionStats().bottom_up_super_pages, 0u);

  std::vector<uintptr_t> pages;
  for (size_t i = 0; i < 4; ++i) {
    const uintptr_t expected = window + (i + 1) * kSuperPageSize;
    const uintptr_t p = SuperPageRegionReserve(kSuperPageSize);
    ASSERT_EQ(p, expected) << "index " << i;
    EXPECT_EQ(p & kSuperPageOffsetMask, 0u);
    EXPECT_TRUE(IsMapped(p, kSuperPageSize));
    EXPECT_EQ(GetSuperPageRegionStats().bottom_up_cursor,
              expected + kSuperPageSize);
    EXPECT_EQ(GetSuperPageRegionStats().bottom_up_super_pages, i + 1);
    EXPECT_EQ(GetTotalMappedSize(), mapped_before + (i + 1) * kSuperPageSize);
    pages.push_back(p);
  }

  // Leading slot before the rounded-up base stays unmapped.
  errno = 0;
  EXPECT_FALSE(IsMapped(window, SystemPageSize()));
  EXPECT_EQ(errno, ENOMEM);

  // Extra 4096 bytes past the rounded-down limit are not allocated.
  EXPECT_EQ(SuperPageRegionReserve(kSuperPageSize), 0u);
  EXPECT_EQ(GetSuperPageRegionStats().bottom_up_cursor,
            window + 5 * kSuperPageSize);

  for (uintptr_t p : pages) {
    FreePages(p, kSuperPageSize);
  }
  EXPECT_EQ(GetTotalMappedSize(), mapped_before);

  // Already-aligned base stays exact (does not round up by an extra 2 MiB).
  ResetSuperPageRegionsForTesting();
  ASSERT_TRUE(
      ConfigureBottomUpSuperPagesFor32Bit(window, window + 2 * kSuperPageSize));
  EXPECT_EQ(GetSuperPageRegionStats().bottom_up_cursor, window);
  const uintptr_t a0 = SuperPageRegionReserve(kSuperPageSize);
  const uintptr_t a1 = SuperPageRegionReserve(kSuperPageSize);
  EXPECT_EQ(a0, window);
  EXPECT_EQ(a1, window + kSuperPageSize);
  EXPECT_EQ(SuperPageRegionReserve(kSuperPageSize), 0u);
  FreePages(a0, kSuperPageSize);
  FreePages(a1, kSuperPageSize);
  EXPECT_EQ(GetTotalMappedSize(), mapped_before);
}

TEST_F(SuperPageRegion32BitTest, BottomUpStepsOverOccupiedMappingsSafely) {
  const uintptr_t window = ProbeFreeAlignedRange(8 * kSuperPageSize);
  ASSERT_NE(window, 0u);
  const size_t sys_page = SystemPageSize();
  const size_t mapped_before = GetTotalMappedSize();

  // Obstacle 1: one RW system page filled with 0xC7 inside slot 1 (+2 MiB).
  void* rw_guard1 =
      mmap(reinterpret_cast<void*>(window + 1 * kSuperPageSize), sys_page,
           PROT_READ | PROT_WRITE,
           MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
  ASSERT_EQ(reinterpret_cast<uintptr_t>(rw_guard1),
            window + 1 * kSuperPageSize);
  memset(rw_guard1, 0xC7, sys_page);

  // Obstacle 2: a full 2 MiB PROT_NONE mapping filling slot 3 (+6 MiB).
  void* none_guard =
      mmap(reinterpret_cast<void*>(window + 3 * kSuperPageSize), kSuperPageSize,
           PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
  ASSERT_EQ(reinterpret_cast<uintptr_t>(none_guard),
            window + 3 * kSuperPageSize);

  // Obstacle 3: one RW system page offset inside slot 4 (+8 MiB + sys_page).
  void* rw_guard2 = mmap(
      reinterpret_cast<void*>(window + 4 * kSuperPageSize + sys_page), sys_page,
      PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE,
      -1, 0);
  ASSERT_EQ(reinterpret_cast<uintptr_t>(rw_guard2),
            window + 4 * kSuperPageSize + sys_page);
  memset(rw_guard2, 0xD9, sys_page);

  ASSERT_TRUE(
      ConfigureBottomUpSuperPagesFor32Bit(window, window + 8 * kSuperPageSize));

  // Slot 0 free -> p0 at slot 0.
  const uintptr_t p0 = SuperPageRegionReserve(kSuperPageSize);
  ASSERT_EQ(p0, window);
  // Slot 1 occupied (rw_guard1) -> stepped over, p1 at slot 2.
  const uintptr_t p1 = SuperPageRegionReserve(kSuperPageSize);
  ASSERT_EQ(p1, window + 2 * kSuperPageSize);
  // Slots 3 and 4 occupied (none_guard, rw_guard2) -> both stepped over, p2 at
  // slot 5.
  const uintptr_t p2 = SuperPageRegionReserve(kSuperPageSize);
  ASSERT_EQ(p2, window + 5 * kSuperPageSize);

  // A 2-super-page (4 MiB) reserve hits remaining slots [6, 8) without moving
  // past earlier guards.
  const uintptr_t p_span = SuperPageRegionReserve(2 * kSuperPageSize);
  ASSERT_EQ(p_span, window + 6 * kSuperPageSize);

  // All obstacles survived unclobbered.
  const auto* g1_bytes = static_cast<const uint8_t*>(rw_guard1);
  const auto* g2_bytes = static_cast<const uint8_t*>(rw_guard2);
  for (size_t i = 0; i < sys_page; ++i) {
    ASSERT_EQ(g1_bytes[i], 0xC7u);
    ASSERT_EQ(g2_bytes[i], 0xD9u);
  }
  EXPECT_TRUE(IsMapped(window + 3 * kSuperPageSize, kSuperPageSize));
#if PA_BUILDFLAG(IS_LINUX) || PA_BUILDFLAG(IS_ANDROID)
  EXPECT_TRUE(RangeIsMappedProtNone(window + 3 * kSuperPageSize,
                                    window + 4 * kSuperPageSize, nullptr));
#endif

  FreePages(p0, kSuperPageSize);
  FreePages(p1, kSuperPageSize);
  FreePages(p2, kSuperPageSize);
  FreePages(p_span, 2 * kSuperPageSize);
  EXPECT_EQ(GetTotalMappedSize(), mapped_before);
  ASSERT_EQ(munmap(rw_guard2, sys_page), 0);
  ASSERT_EQ(munmap(none_guard, kSuperPageSize), 0);
  ASSERT_EQ(munmap(rw_guard1, sys_page), 0);
}

TEST_F(SuperPageRegion32BitTest,
       BottomUpMultiPageSkipPreservesHoleForSinglePage) {
  const uintptr_t window = ProbeFreeAlignedRange(4 * kSuperPageSize);
  ASSERT_NE(window, 0u);
  const size_t sys_page = SystemPageSize();
  const size_t mapped_before = GetTotalMappedSize();

  // Occupy slot 1: slot 0 is a 1-super-page hole, slots [2, 4) are free.
  void* mid_guard =
      mmap(reinterpret_cast<void*>(window + kSuperPageSize), sys_page,
           PROT_READ | PROT_WRITE,
           MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
  ASSERT_EQ(reinterpret_cast<uintptr_t>(mid_guard), window + kSuperPageSize);
  memset(mid_guard, 0x5E, sys_page);

  ASSERT_TRUE(
      ConfigureBottomUpSuperPagesFor32Bit(window, window + 4 * kSuperPageSize));

  // Multi-super-page reserve jumps over slot 0 (too small) and slot 1
  // (occupied) to claim [2, 4), leaving the cursor at slot 0.
  const uintptr_t pair = SuperPageRegionReserve(2 * kSuperPageSize);
  ASSERT_EQ(pair, window + 2 * kSuperPageSize);
  EXPECT_EQ(GetSuperPageRegionStats().bottom_up_cursor, window);
  EXPECT_EQ(GetSuperPageRegionStats().bottom_up_super_pages, 2u);
  EXPECT_EQ(GetTotalMappedSize(), mapped_before + 2 * kSuperPageSize);
  EXPECT_EQ(static_cast<const uint8_t*>(mid_guard)[0], 0x5Eu);
  EXPECT_EQ(static_cast<const uint8_t*>(mid_guard)[sys_page - 1], 0x5Eu);

  // A subsequent single-super-page reserve fills the preserved hole at slot 0.
  const uintptr_t hole = SuperPageRegionReserve(kSuperPageSize);
  ASSERT_EQ(hole, window);
  EXPECT_EQ(GetSuperPageRegionStats().bottom_up_cursor,
            window + kSuperPageSize);
  EXPECT_EQ(GetSuperPageRegionStats().bottom_up_super_pages, 3u);
  EXPECT_EQ(GetTotalMappedSize(), mapped_before + 3 * kSuperPageSize);

  // Next single-super-page reserve scans past occupied slot 1 and [2, 4) to the
  // limit and returns 0.
  EXPECT_EQ(SuperPageRegionReserve(kSuperPageSize), 0u);
  EXPECT_EQ(GetSuperPageRegionStats().bottom_up_cursor,
            window + 4 * kSuperPageSize);

  FreePages(hole, kSuperPageSize);
  FreePages(pair, 2 * kSuperPageSize);
  EXPECT_EQ(GetTotalMappedSize(), mapped_before);

  // Also test a failing multi-page reserve (3 slots, slot 1 occupied): cursor
  // stays at slot 0 so two later single-page reserves claim slot 0 and slot 2.
  ResetSuperPageRegionsForTesting();
  ASSERT_TRUE(
      ConfigureBottomUpSuperPagesFor32Bit(window, window + 3 * kSuperPageSize));
  EXPECT_EQ(SuperPageRegionReserve(2 * kSuperPageSize), 0u);
  EXPECT_EQ(GetSuperPageRegionStats().bottom_up_cursor, window);
  const uintptr_t s0 = SuperPageRegionReserve(kSuperPageSize);
  const uintptr_t s2 = SuperPageRegionReserve(kSuperPageSize);
  EXPECT_EQ(s0, window);
  EXPECT_EQ(s2, window + 2 * kSuperPageSize);
  EXPECT_EQ(SuperPageRegionReserve(kSuperPageSize), 0u);
  FreePages(s0, kSuperPageSize);
  FreePages(s2, kSuperPageSize);
  EXPECT_EQ(GetTotalMappedSize(), mapped_before);
  ASSERT_EQ(munmap(mid_guard, sys_page), 0);
}

TEST_F(SuperPageRegion32BitTest, BottomUpConfigureRejectsInvalidRanges) {
  const uintptr_t window = ProbeFreeAlignedRange(4 * kSuperPageSize);
  ASSERT_NE(window, 0u);
  constexpr uintptr_t kMaxAddr = std::numeric_limits<uintptr_t>::max();

  // Base == 0 is rejected and leaves bottom-up off.
  EXPECT_FALSE(
      ConfigureBottomUpSuperPagesFor32Bit(0, window + 2 * kSuperPageSize));
  EXPECT_EQ(GetSuperPageRegionStats().bottom_up_cursor, 0u);
  EXPECT_EQ(SuperPageRegionReserve(kSuperPageSize), 0u);
  SuperPageRegionNoteFallback(2);
  EXPECT_EQ(GetSuperPageRegionStats().fallback_super_pages, 0u);
  EXPECT_FALSE(SuperPageRegionShouldDropHint(window));

  // Wrapping bases (`base + kSuperPageOffsetMask` overflows uintptr_t).
  EXPECT_FALSE(ConfigureBottomUpSuperPagesFor32Bit(
      kMaxAddr - kSuperPageOffsetMask + 1, kMaxAddr));
  EXPECT_FALSE(ConfigureBottomUpSuperPagesFor32Bit(kMaxAddr, kMaxAddr));
  EXPECT_EQ(GetSuperPageRegionStats().bottom_up_cursor, 0u);

  // Empty, inverted, and rounding-crossed ranges return false and stay off.
  EXPECT_FALSE(ConfigureBottomUpSuperPagesFor32Bit(window, window));
  EXPECT_FALSE(
      ConfigureBottomUpSuperPagesFor32Bit(window + 2 * kSuperPageSize, window));
  EXPECT_FALSE(ConfigureBottomUpSuperPagesFor32Bit(
      window + 1, window + kSuperPageSize - 1));
  EXPECT_FALSE(ConfigureBottomUpSuperPagesFor32Bit(
      window + 1, window + kSuperPageSize));
  EXPECT_EQ(GetSuperPageRegionStats().bottom_up_cursor, 0u);
  EXPECT_EQ(SuperPageRegionReserve(kSuperPageSize), 0u);

  // Rejected ConfigureBottomUp calls leave g_bottom_up == false, so region mode
  // can still be configured without tripping the mutual-exclusion CHECK.
  ASSERT_EQ(ConfigureSuperPageRegionsFor32Bit(4 * kMiB, 0, 4 * kMiB), 4 * kMiB);
}

TEST_F(SuperPageRegion32BitTest, BottomUpLimitAndMultiSuperPageBounds) {
  const uintptr_t window = ProbeFreeAlignedRange(4 * kSuperPageSize);
  ASSERT_NE(window, 0u);
  const size_t mapped_before = GetTotalMappedSize();

  ASSERT_TRUE(
      ConfigureBottomUpSuperPagesFor32Bit(window, window + 3 * kSuperPageSize));

  const uintptr_t span = SuperPageRegionReserve(2 * kSuperPageSize);
  ASSERT_EQ(span, window);
  EXPECT_EQ(GetSuperPageRegionStats().bottom_up_cursor,
            window + 2 * kSuperPageSize);

  // Only 1 super page remains below limit, so a 2-super-page reserve fails
  // without advancing the cursor past the remaining slot.
  EXPECT_EQ(SuperPageRegionReserve(2 * kSuperPageSize), 0u);
  EXPECT_EQ(GetSuperPageRegionStats().bottom_up_cursor,
            window + 2 * kSuperPageSize);

  // A 1-super-page reserve takes that final slot, then hits the limit.
  const uintptr_t last = SuperPageRegionReserve(kSuperPageSize);
  ASSERT_EQ(last, window + 2 * kSuperPageSize);
  EXPECT_EQ(GetSuperPageRegionStats().bottom_up_cursor,
            window + 3 * kSuperPageSize);
  EXPECT_EQ(SuperPageRegionReserve(kSuperPageSize), 0u);

  FreePages(span, 2 * kSuperPageSize);
  FreePages(last, kSuperPageSize);
  EXPECT_EQ(GetTotalMappedSize(), mapped_before);

  // Invalid reservation lengths return 0 safely.
  ResetSuperPageRegionsForTesting();
  ASSERT_TRUE(
      ConfigureBottomUpSuperPagesFor32Bit(window, window + 4 * kSuperPageSize));
  EXPECT_EQ(SuperPageRegionReserve(0), 0u);
  EXPECT_EQ(SuperPageRegionReserve(257 * kSuperPageSize), 0u);
}

TEST_F(SuperPageRegion32BitTest, BottomUpShouldDropHintBoundaries) {
  const uintptr_t window = ProbeFreeAlignedRange(5 * kSuperPageSize);
  ASSERT_NE(window, 0u);

  // Disabled -> never drops hint.
  EXPECT_FALSE(SuperPageRegionShouldDropHint(0));
  EXPECT_FALSE(SuperPageRegionShouldDropHint(window));

  // Unaligned (window + 1234, window + 4*SP + 4096) aligns to
  // [window + 1*SP, window + 4*SP].
  ASSERT_TRUE(ConfigureBottomUpSuperPagesFor32Bit(
      window + 1234, window + 4 * kSuperPageSize + 4096));
  const uintptr_t aligned_base = window + kSuperPageSize;
  const uintptr_t aligned_limit = window + 4 * kSuperPageSize;

  EXPECT_FALSE(SuperPageRegionShouldDropHint(0));
  EXPECT_FALSE(SuperPageRegionShouldDropHint(window));
  EXPECT_FALSE(SuperPageRegionShouldDropHint(aligned_base - 1));
  EXPECT_TRUE(SuperPageRegionShouldDropHint(aligned_base));
  EXPECT_TRUE(SuperPageRegionShouldDropHint(aligned_base + kSuperPageSize));
  EXPECT_TRUE(SuperPageRegionShouldDropHint(aligned_limit));
  EXPECT_FALSE(SuperPageRegionShouldDropHint(aligned_limit + 1));
  EXPECT_FALSE(SuperPageRegionShouldDropHint(aligned_limit + 4096));
  EXPECT_FALSE(SuperPageRegionShouldDropHint(aligned_limit + kSuperPageSize));

  ResetSuperPageRegionsForTesting();
  EXPECT_FALSE(SuperPageRegionShouldDropHint(aligned_base));
  EXPECT_FALSE(SuperPageRegionShouldDropHint(aligned_limit));

  // Region mode does not drop chain hints.
  ASSERT_EQ(ConfigureSuperPageRegionsFor32Bit(4 * kMiB, 0, 4 * kMiB), 4 * kMiB);
  const uintptr_t p = SuperPageRegionReserve(kSuperPageSize);
  ASSERT_NE(p, 0u);
  EXPECT_FALSE(SuperPageRegionShouldDropHint(p));
  EXPECT_TRUE(SuperPageRegionRelease(p, kSuperPageSize));
}

TEST_F(SuperPageRegion32BitTest, BottomUpStatsFallbackAndReleaseReturnsFalse) {
  const uintptr_t window = ProbeFreeAlignedRange(4 * kSuperPageSize);
  ASSERT_NE(window, 0u);
  const size_t mapped_before = GetTotalMappedSize();

  // Disabled state: NoteFallback ignored, stats zeroed.
  SuperPageRegionNoteFallback(4);
  {
    const SuperPageRegionStats stats = GetSuperPageRegionStats();
    EXPECT_EQ(stats.bottom_up_super_pages, 0u);
    EXPECT_EQ(stats.bottom_up_cursor, 0u);
    EXPECT_EQ(stats.fallback_super_pages, 0u);
  }

  ASSERT_TRUE(
      ConfigureBottomUpSuperPagesFor32Bit(window, window + 4 * kSuperPageSize));
  {
    const SuperPageRegionStats stats = GetSuperPageRegionStats();
    EXPECT_EQ(stats.regions, 0u);
    EXPECT_EQ(stats.reserved_bytes, 0u);
    EXPECT_EQ(stats.used_bytes, 0u);
    EXPECT_EQ(stats.bottom_up_super_pages, 0u);
    EXPECT_EQ(stats.bottom_up_cursor, window);
    EXPECT_EQ(stats.fallback_super_pages, 0u);
  }

  const uintptr_t p0 = SuperPageRegionReserve(kSuperPageSize);
  ASSERT_EQ(p0, window);
  EXPECT_EQ(GetSuperPageRegionStats().bottom_up_super_pages, 1u);
  EXPECT_EQ(GetSuperPageRegionStats().bottom_up_cursor,
            window + kSuperPageSize);
  EXPECT_EQ(GetTotalMappedSize(), mapped_before + kSuperPageSize);

  const uintptr_t p_two = SuperPageRegionReserve(2 * kSuperPageSize);
  ASSERT_EQ(p_two, window + kSuperPageSize);
  EXPECT_EQ(GetSuperPageRegionStats().bottom_up_super_pages, 3u);
  EXPECT_EQ(GetSuperPageRegionStats().bottom_up_cursor,
            window + 3 * kSuperPageSize);
  EXPECT_EQ(GetTotalMappedSize(), mapped_before + 3 * kSuperPageSize);

  // SuperPageRegionRelease returns false for bottom-up pages and leaves them
  // mapped (the caller unmaps via FreePages).
  EXPECT_FALSE(SuperPageRegionRelease(p0, kSuperPageSize));
  EXPECT_TRUE(IsMapped(p0, kSuperPageSize));
  EXPECT_FALSE(SuperPageRegionRelease(p_two, 2 * kSuperPageSize));
  EXPECT_TRUE(IsMapped(p_two, 2 * kSuperPageSize));

  // Failed reserve at limit doesn't bump bottom_up_super_pages or mapped size.
  EXPECT_EQ(SuperPageRegionReserve(2 * kSuperPageSize), 0u);
  EXPECT_EQ(GetSuperPageRegionStats().bottom_up_super_pages, 3u);
  EXPECT_EQ(GetTotalMappedSize(), mapped_before + 3 * kSuperPageSize);

  // Fallback counter records super pages while bottom-up mode is active.
  SuperPageRegionNoteFallback(2);
  SuperPageRegionNoteFallback(3);
  EXPECT_EQ(GetSuperPageRegionStats().fallback_super_pages, 5u);

  FreePages(p0, kSuperPageSize);
  FreePages(p_two, 2 * kSuperPageSize);
  EXPECT_EQ(GetTotalMappedSize(), mapped_before);
}

TEST_F(SuperPageRegion32BitTest, BottomUpResetClearsState) {
  const uintptr_t window = ProbeFreeAlignedRange(4 * kSuperPageSize);
  ASSERT_NE(window, 0u);
  const size_t mapped_before = GetTotalMappedSize();

  ASSERT_TRUE(
      ConfigureBottomUpSuperPagesFor32Bit(window, window + 4 * kSuperPageSize));
  const uintptr_t p = SuperPageRegionReserve(kSuperPageSize);
  ASSERT_EQ(p, window);
  SuperPageRegionNoteFallback(3);
  EXPECT_TRUE(SuperPageRegionShouldDropHint(window));
  FreePages(p, kSuperPageSize);
  EXPECT_EQ(GetTotalMappedSize(), mapped_before);

  ResetSuperPageRegionsForTesting();

  const SuperPageRegionStats stats = GetSuperPageRegionStats();
  EXPECT_EQ(stats.bottom_up_super_pages, 0u);
  EXPECT_EQ(stats.bottom_up_cursor, 0u);
  EXPECT_EQ(stats.fallback_super_pages, 0u);
  EXPECT_EQ(SuperPageRegionReserve(kSuperPageSize), 0u);
  EXPECT_FALSE(SuperPageRegionShouldDropHint(window));

  // After reset, bottom-up is disabled so NoteFallback is ignored again.
  SuperPageRegionNoteFallback(7);
  EXPECT_EQ(GetSuperPageRegionStats().fallback_super_pages, 0u);

  // Region mode works cleanly after resetting out of bottom-up mode.
  ASSERT_EQ(ConfigureSuperPageRegionsFor32Bit(16 * kMiB, 0, 16 * kMiB),
            16 * kMiB);
  const uintptr_t reg_p = SuperPageRegionReserve(kSuperPageSize);
  ASSERT_NE(reg_p, 0u);
  EXPECT_TRUE(SuperPageRegionRelease(reg_p, kSuperPageSize));
}

TEST_F(SuperPageRegion32BitTest, BottomUpPartitionRootEndToEndAndFallback) {
  // Pre-size the vector before probing so malloc doesn't land in `window`.
  constexpr size_t kAllocSize = 64 * 1024;
  constexpr size_t kAllocCount = 96;  // ~6 MiB payload -> > 2 super pages.
  std::vector<void*> ptrs;
  ptrs.reserve(kAllocCount);

  const size_t mapped_before = GetTotalMappedSize();
  const uintptr_t window = ProbeFreeAlignedRange(2 * kSuperPageSize);
  ASSERT_NE(window, 0u);
  const uintptr_t limit = window + 2 * kSuperPageSize;
  ASSERT_TRUE(ConfigureBottomUpSuperPagesFor32Bit(window, limit));

  PartitionAllocator allocator(PartitionOptions{});
  PartitionRoot* root = allocator.root();
  ASSERT_EQ(root->ChoosePool(), kRegularPoolHandle);

  size_t in_window = 0;
  size_t outside_window = 0;
  for (size_t i = 0; i < kAllocCount; ++i) {
    void* p = root->Alloc(kAllocSize);
    ASSERT_NE(p, nullptr);
    const uintptr_t addr = reinterpret_cast<uintptr_t>(p);
    EXPECT_TRUE(IsManagedByPartitionAlloc(addr));
    if (addr >= window && addr < limit) {
      ++in_window;
    } else {
      ++outside_window;
    }
    ptrs.push_back(p);
  }

  EXPECT_GT(in_window, 0u);
  EXPECT_GT(outside_window, 0u);
  const SuperPageRegionStats stats = GetSuperPageRegionStats();
  EXPECT_EQ(stats.bottom_up_super_pages, 2u);
  EXPECT_EQ(stats.bottom_up_cursor, limit);
  EXPECT_GE(stats.fallback_super_pages, 1u);
  EXPECT_GE(GetTotalMappedSize(),
            mapped_before + (2u + stats.fallback_super_pages) * kSuperPageSize);

  for (void* p : ptrs) {
    root->Free(p);
  }
  {
    ScopedGuard guard{PartitionRootLock(root)};
    root->DestructForTesting();
  }

  // Because NoteMappedAddressSpace accounted both bottom-up super pages,
  // FreePages inside UnreserveAndDecommit returns GetTotalMappedSize() back to
  // its starting value without underflowing.
  EXPECT_EQ(GetTotalMappedSize(), mapped_before);
  EXPECT_FALSE(IsMapped(window, kSuperPageSize));
  EXPECT_FALSE(IsMapped(window + kSuperPageSize, kSuperPageSize));
}

#endif  // PA_TEST_BOTTOM_UP

}  // namespace
}  // namespace partition_alloc::internal

#endif  // !PA_BUILDFLAG(HAS_64_BIT_POINTERS) && PA_BUILDFLAG(IS_POSIX)
