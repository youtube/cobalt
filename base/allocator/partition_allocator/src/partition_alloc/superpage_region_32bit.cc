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

#include "partition_alloc/address_space_randomization.h"
#include "partition_alloc/build_config.h"
#include "partition_alloc/buildflags.h"

#if !PA_BUILDFLAG(HAS_64_BIT_POINTERS) && PA_BUILDFLAG(IS_POSIX)

#include <errno.h>
#include <sys/mman.h>

#ifndef MAP_NORESERVE
#define MAP_NORESERVE 0
#endif


#include <atomic>
#include <cstdint>
#include <limits>

#include "partition_alloc/address_pool_manager.h"
#include "partition_alloc/address_space_stats.h"
#include "partition_alloc/page_allocator.h"
#include "partition_alloc/page_allocator_internal.h"
#include "partition_alloc/page_allocator_constants.h"
#include "partition_alloc/partition_alloc_base/cxx_wrapper/algorithm.h"
#include "partition_alloc/partition_alloc_base/thread_annotations.h"
#include "partition_alloc/partition_alloc_check.h"
#include "partition_alloc/partition_alloc_constants.h"
#include "partition_alloc/partition_lock.h"

namespace partition_alloc {

namespace {

using internal::kSuperPageOffsetMask;
using internal::kSuperPageSize;

constexpr size_t kMiB = 1024 * 1024;
constexpr size_t kMinStepBytes = 16 * kMiB;
// Up to 64 regions. With the default 32 MiB step and 512 MiB cap, regions
// grow to at most 16 plus whatever halving produced; the cap matters only for
// small steps (64 x 16 MiB = 1 GiB).
constexpr size_t kMaxRegions = 64;
// Up to 512 MiB per region: 256 super pages.
constexpr size_t kMaxPagesPerRegion = 256;
constexpr size_t kWordsPerRegion = kMaxPagesPerRegion / 64;

struct Region {
  uintptr_t begin;
  size_t pages;
  size_t used_pages;
  uint64_t used[kWordsPerRegion];
};

internal::Lock g_lock;
Region g_regions[kMaxRegions] PA_GUARDED_BY(g_lock);
size_t g_num_regions PA_GUARDED_BY(g_lock) = 0;
bool g_enabled PA_GUARDED_BY(g_lock) = false;
size_t g_step_bytes PA_GUARDED_BY(g_lock) = 0;
size_t g_max_bytes PA_GUARDED_BY(g_lock) = 0;
size_t g_reserved_bytes PA_GUARDED_BY(g_lock) = 0;
size_t g_fallback_super_pages PA_GUARDED_BY(g_lock) = 0;
size_t g_preexisting_super_pages PA_GUARDED_BY(g_lock) = 0;
// Bottom-up mode state.
bool g_bottom_up PA_GUARDED_BY(g_lock) = false;
uintptr_t g_bottom_up_base PA_GUARDED_BY(g_lock) = 0;
uintptr_t g_bottom_up_cursor PA_GUARDED_BY(g_lock) = 0;
uintptr_t g_bottom_up_limit PA_GUARDED_BY(g_lock) = 0;
size_t g_bottom_up_super_pages PA_GUARDED_BY(g_lock) = 0;
// After a growth attempt fails, retry only on every 32nd miss (each attempt is
// up to several failing mmaps under the lock), or after a release.
bool g_growth_failed PA_GUARDED_BY(g_lock) = false;
size_t g_misses_since_growth_failed PA_GUARDED_BY(g_lock) = 0;
bool g_fail_reserve_for_testing PA_GUARDED_BY(g_lock) = false;
size_t g_reserve_attempts_for_testing PA_GUARDED_BY(g_lock) = 0;
// True once either mode is configured, so the default (disabled) path never
// takes g_lock. Not reset across fork(); see the header.
std::atomic<bool> g_active{false};

void NameRange(uintptr_t address, size_t length, const char* name) {
#if defined(LINUX_NAME_REGION)
  // Best effort; older kernels reject it.
  prctl(PR_SET_VMA, PR_SET_VMA_ANON_NAME, address, length,
        reinterpret_cast<unsigned long>(name));
#endif
}

// Reserves about `*length` bytes of PROT_NONE address space aligned to the
// super page size, placed by the kernel (no hint, so no random placement
// splitting the largest free hole), or returns 0. Tries the exact size first so
// an exact-size hole can be used: if the kernel's placement is misaligned, the
// aligned part (one super page less) is kept when it is at least `min_length`,
// and `*length` is updated. Only then does it over-allocate by a super page
// and trim, which needs a larger hole.
uintptr_t ReserveAlignedUnhinted(size_t* length, size_t min_length) {
  const size_t page = internal::PageAllocationGranularity();
  void* p = mmap(nullptr, *length, PROT_NONE,
                 MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE, -1, 0);
  if (p != MAP_FAILED) {
    const uintptr_t raw = reinterpret_cast<uintptr_t>(p);
    const size_t pre_slack = (kSuperPageSize - (raw & kSuperPageOffsetMask)) &
                             kSuperPageOffsetMask;
    if (!pre_slack) {
      NameRange(raw, *length, "pa-superpage-region");
      return raw;
    }
    const size_t kept = *length - kSuperPageSize;
    if (*length > kSuperPageSize && kept >= min_length) {
      const uintptr_t aligned = raw + pre_slack;
      munmap(p, pre_slack);
      munmap(reinterpret_cast<void*>(aligned + kept),
             kSuperPageSize - pre_slack);
      *length = kept;
      NameRange(aligned, kept, "pa-superpage-region");
      return aligned;
    }
    munmap(p, *length);
  } else {
    // No hole of the exact size; a larger one won't be found either.
    return 0;
  }
  const size_t try_length = *length + kSuperPageSize - page;
  if (try_length < *length) {
    return 0;
  }
  p = mmap(nullptr, try_length, PROT_NONE,
           MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE, -1, 0);
  if (p == MAP_FAILED) {
    return 0;
  }
  const uintptr_t raw = reinterpret_cast<uintptr_t>(p);
  const size_t pre_slack = (kSuperPageSize - (raw & kSuperPageOffsetMask)) &
                           kSuperPageOffsetMask;
  const size_t post_slack = try_length - pre_slack - *length;
  const uintptr_t aligned = raw + pre_slack;
  if (pre_slack) {
    munmap(p, pre_slack);
  }
  if (post_slack) {
    munmap(reinterpret_cast<void*>(aligned + *length), post_slack);
  }
  NameRange(aligned, *length, "pa-superpage-region");
  return aligned;
}

// Adds a region of up to `bytes` (halving down to kMinStepBytes when it
// doesn't fit). Returns the bytes added.
size_t AddRegionLocked(size_t bytes, size_t min_bytes)
    PA_EXCLUSIVE_LOCKS_REQUIRED(g_lock) {
  bytes &= ~kSuperPageOffsetMask;
  bytes = std::min(bytes, kMaxPagesPerRegion * kSuperPageSize);
  if (g_reserved_bytes >= g_max_bytes) {
    return 0;
  }
  bytes = std::min(bytes, g_max_bytes - g_reserved_bytes);
  min_bytes = std::max(min_bytes, kSuperPageSize);
  while (bytes >= min_bytes && g_num_regions < kMaxRegions) {
    ++g_reserve_attempts_for_testing;
    uintptr_t begin = g_fail_reserve_for_testing
                          ? 0
                          : ReserveAlignedUnhinted(&bytes, min_bytes);
    if (begin) {
      Region& r = g_regions[g_num_regions++];
      r.begin = begin;
      r.pages = bytes / kSuperPageSize;
      r.used_pages = 0;
      for (uint64_t& w : r.used) {
        w = 0;
      }
      g_reserved_bytes += bytes;
      // Counted once here; super pages handed out from the region aren't
      // mapped again, and released ones stay mapped (decommitted).
      internal::NoteMappedAddressSpace(bytes);
      return bytes;
    }
    if (bytes <= std::max(kMinStepBytes, min_bytes)) {
      break;
    }
    bytes = std::max({kMinStepBytes, min_bytes,
                      (bytes / 2) & ~kSuperPageOffsetMask});
  }
  return 0;
}

bool TestBit(const Region& r, size_t i) {
  return (r.used[i / 64] >> (i % 64)) & 1;
}

void SetBits(Region& r, size_t first, size_t count, bool value) {
  PA_CHECK(first + count <= r.pages);
  for (size_t i = first; i < first + count; ++i) {
    PA_CHECK(TestBit(r, i) != value);
    const uint64_t bit = uint64_t{1} << (i % 64);
    if (value) {
      r.used[i / 64] |= bit;
    } else {
      r.used[i / 64] &= ~bit;
    }
  }
  if (value) {
    r.used_pages += count;
  } else {
    r.used_pages -= count;
  }
}

#if !defined(MAP_FIXED_NOREPLACE) &&                      \
    (PA_BUILDFLAG(IS_ANDROID) || PA_BUILDFLAG(IS_LINUX) || \
     PA_BUILDFLAG(IS_CHROMEOS))
#define MAP_FIXED_NOREPLACE 0x100000
#endif

// Bottom-up mode: the lowest free `length` bytes at or above the cursor,
// stepping one super page at a time over mapped ranges. The cursor only moves
// past slots that are taken for a single super page, so a multi-page request
// that skips a smaller hole leaves the hole for later single pages.
uintptr_t BottomUpReserveLocked(size_t length)
    PA_EXCLUSIVE_LOCKS_REQUIRED(g_lock) {
#if defined(MAP_FIXED_NOREPLACE)
  uintptr_t probe = g_bottom_up_cursor;
  bool cursor_follows = true;
  while (probe && probe < g_bottom_up_limit &&
         g_bottom_up_limit - probe >= length) {
    void* want = reinterpret_cast<void*>(probe);
    void* p = mmap(want, length, PROT_NONE,
                   MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
    if (p == want) {
      if (cursor_follows) {
        g_bottom_up_cursor = probe + length;
      }
      g_bottom_up_super_pages += length / kSuperPageSize;
      internal::NoteMappedAddressSpace(length);
      return probe;
    }
    if (p != MAP_FAILED) {
      // Old kernel (< 4.17): the flag was ignored and the hint not taken.
      munmap(p, length);
    } else if (errno != EEXIST) {
      // Out of memory or mappings, not an occupied address: stepping on would
      // only repeat the failure under the lock. Fall back to today's path.
      return 0;
    }
    if (cursor_follows && length == kSuperPageSize) {
      g_bottom_up_cursor = probe + kSuperPageSize;
    } else {
      cursor_follows = false;
    }
    probe += kSuperPageSize;
  }
#endif
  return 0;
}

// First fit of `count` consecutive free pages across regions, oldest first,
// which keeps long-lived super pages packed together.
uintptr_t FindLocked(size_t count) PA_EXCLUSIVE_LOCKS_REQUIRED(g_lock) {
  for (size_t ri = 0; ri < g_num_regions; ++ri) {
    Region& r = g_regions[ri];
    if (r.pages - r.used_pages < count) {
      continue;
    }
    size_t run = 0;
    for (size_t i = 0; i < r.pages; ++i) {
      run = TestBit(r, i) ? 0 : run + 1;
      if (run == count) {
        const size_t first = i + 1 - count;
        SetBits(r, first, count, true);
        return r.begin + first * kSuperPageSize;
      }
    }
  }
  return 0;
}

// Regular-pool 2 MiB slots in use. Called without g_lock held: it takes the
// address pool bitmap's lock.
size_t CountRegularPoolSuperPages() {
  struct Dumper : AddressSpaceStatsDumper {
    void DumpStats(const AddressSpaceStats* stats) override {
      super_pages = stats->regular_pool_stats.usage;
    }
    size_t super_pages = 0;
  } dumper;
  internal::AddressPoolManager::GetInstance().DumpStats(&dumper);
  return dumper.super_pages;
}

}  // namespace

size_t ConfigureSuperPageRegionsFor32Bit(size_t initial_bytes,
                                         size_t step_bytes,
                                         size_t max_bytes) {
  const size_t preexisting = CountRegularPoolSuperPages();
  internal::ScopedGuard guard(g_lock);
  PA_CHECK(!g_bottom_up);  // Mutually exclusive with the bottom-up cursor.
  g_enabled = true;
  g_active.store(true, std::memory_order_relaxed);
  g_preexisting_super_pages = preexisting;
  // 0 means no growth beyond the initial reservation.
  g_step_bytes = step_bytes & ~kSuperPageOffsetMask;
  g_max_bytes = max_bytes & ~kSuperPageOffsetMask;
  g_growth_failed = false;
  g_misses_since_growth_failed = 0;
  size_t added = 0;
  while (added < initial_bytes) {
    size_t got = AddRegionLocked(
        std::min(initial_bytes - added, kMaxPagesPerRegion * kSuperPageSize),
        kSuperPageSize);
    if (!got) {
      break;
    }
    added += got;
  }
  return added;
}

bool ConfigureBottomUpSuperPagesFor32Bit(uintptr_t base, uintptr_t limit) {
#if !defined(MAP_FIXED_NOREPLACE)
  // Without MAP_FIXED_NOREPLACE the cursor can't probe safely.
  return false;
#else
  const size_t preexisting = CountRegularPoolSuperPages();
  internal::ScopedGuard guard(g_lock);
  PA_CHECK(!g_enabled);  // Mutually exclusive with regions.
  if (!base || base > std::numeric_limits<uintptr_t>::max() - kSuperPageOffsetMask) {
    return false;
  }
  const uintptr_t cursor = (base + kSuperPageOffsetMask) & ~kSuperPageOffsetMask;
  const uintptr_t aligned_limit = limit & ~kSuperPageOffsetMask;
  if (cursor >= aligned_limit) {
    return false;
  }
  g_bottom_up = true;
  g_active.store(true, std::memory_order_relaxed);
  g_preexisting_super_pages = preexisting;
  g_bottom_up_base = cursor;
  g_bottom_up_cursor = cursor;
  g_bottom_up_limit = aligned_limit;
  return true;
#endif  // !defined(MAP_FIXED_NOREPLACE)
}

bool AreSuperPageRegionsAvailableFor32Bit() {
  return true;
}

namespace internal {
bool SuperPageRegionShouldDropHint(uintptr_t hint) {
  if (!g_active.load(std::memory_order_relaxed)) {
    return false;
  }
  ScopedGuard guard(g_lock);
  return g_bottom_up && hint >= g_bottom_up_base && hint <= g_bottom_up_limit;
}
}  // namespace internal

SuperPageRegionStats GetSuperPageRegionStats() {
  internal::ScopedGuard guard(g_lock);
  SuperPageRegionStats stats;
  stats.bottom_up_super_pages = g_bottom_up_super_pages;
  stats.bottom_up_cursor = g_bottom_up_cursor;
  stats.regions = g_num_regions;
  stats.reserved_bytes = g_reserved_bytes;
  for (size_t ri = 0; ri < g_num_regions; ++ri) {
    stats.used_bytes += g_regions[ri].used_pages * kSuperPageSize;
  }
  stats.fallback_super_pages = g_fallback_super_pages;
  stats.preexisting_super_pages = g_preexisting_super_pages;
  return stats;
}

namespace internal {

uintptr_t SuperPageRegionReserve(size_t length) {
  PA_DCHECK(!(length & kSuperPageOffsetMask));
  const size_t count = length / kSuperPageSize;
  if (!count || count > kMaxPagesPerRegion) {
    return 0;
  }
  if (!g_active.load(std::memory_order_relaxed)) {
    return 0;
  }
  uintptr_t address = 0;
  bool bottom_up = false;
  {
    ScopedGuard guard(g_lock);
    if (g_bottom_up) {
      bottom_up = true;
      address = BottomUpReserveLocked(length);
    } else if (g_enabled) {
      address = FindLocked(count);
      if (!address && g_step_bytes &&
          (!g_growth_failed || ++g_misses_since_growth_failed % 32 == 0)) {
        if (AddRegionLocked(std::max(g_step_bytes, length), length)) {
          address = FindLocked(count);
        } else {
          g_growth_failed = true;
        }
        if (address) {
          g_growth_failed = false;
          g_misses_since_growth_failed = 0;
        }
      }
    }
  }
  if (address && bottom_up) {
    NameRange(address, length, "partition_alloc");
    return address;
  }
  if (address) {
    // Fresh region pages are already zeroed PROT_NONE; this re-maps them in
    // place so the range carries PartitionAlloc's name like any super page.
    DecommitAndZeroSystemPages(address, length, PageTag::kPartitionAlloc);
  }
  return address;
}

bool SuperPageRegionRelease(uintptr_t address, size_t length) {
  if (!g_active.load(std::memory_order_relaxed)) {
    return false;
  }
  ScopedGuard guard(g_lock);
  for (size_t ri = 0; ri < g_num_regions; ++ri) {
    Region& r = g_regions[ri];
    const size_t region_size = r.pages * kSuperPageSize;
    if (address >= r.begin && address - r.begin < region_size) {
      PA_CHECK(!(address & kSuperPageOffsetMask));
      PA_CHECK(length && !(length & kSuperPageOffsetMask));
      PA_CHECK(length <= region_size - (address - r.begin));
      const size_t first = (address - r.begin) / kSuperPageSize;
      const size_t count = length / kSuperPageSize;
      for (size_t i = first; i < first + count; ++i) {
        PA_CHECK(TestBit(r, i));  // Releasing a page that isn't in use.
      }
      // Keep the range reserved: unmapping would let unrelated mappings land
      // inside the region. If this fails the pages can't be reused safely.
      PA_CHECK(
          DecommitAndZeroSystemPages(address, length, PageTag::kPartitionAlloc));
      SetBits(r, first, count, false);
      g_growth_failed = false;
      g_misses_since_growth_failed = 0;
      return true;
    }
  }
  return false;
}

void SuperPageRegionNoteFallback(size_t super_pages) {
  if (!g_active.load(std::memory_order_relaxed)) {
    return;
  }
  ScopedGuard guard(g_lock);
  if (g_enabled || g_bottom_up) {
    g_fallback_super_pages += super_pages;
  }
}

void ResetSuperPageRegionsForTesting() {
  ScopedGuard guard(g_lock);
  for (size_t ri = 0; ri < g_num_regions; ++ri) {
    // A region that still holds super pages (e.g. the test process's own
    // malloc took one) stays mapped: its pages are released later through
    // FreePages() like any other reservation.
    if (g_regions[ri].used_pages) {
      continue;
    }
    const size_t size = g_regions[ri].pages * kSuperPageSize;
    munmap(reinterpret_cast<void*>(g_regions[ri].begin), size);
    internal::NoteUnmappedAddressSpace(size);
  }
  g_active.store(false, std::memory_order_relaxed);
  g_num_regions = 0;
  g_enabled = false;
  g_step_bytes = 0;
  g_max_bytes = 0;
  g_reserved_bytes = 0;
  g_fallback_super_pages = 0;
  g_preexisting_super_pages = 0;
  g_bottom_up = false;
  g_bottom_up_base = 0;
  g_bottom_up_cursor = 0;
  g_bottom_up_limit = 0;
  g_bottom_up_super_pages = 0;
  g_growth_failed = false;
  g_misses_since_growth_failed = 0;
  g_fail_reserve_for_testing = false;
  g_reserve_attempts_for_testing = 0;
}

void SetSuperPageRegionReserveFailForTesting(bool fail) {
  ScopedGuard guard(g_lock);
  g_fail_reserve_for_testing = fail;
}

size_t GetSuperPageRegionReserveAttemptsForTesting() {
  ScopedGuard guard(g_lock);
  return g_reserve_attempts_for_testing;
}

}  // namespace internal

}  // namespace partition_alloc

#elif !PA_BUILDFLAG(HAS_64_BIT_POINTERS)

// Non-POSIX 32-bit builds: regions are unavailable.
namespace partition_alloc {
size_t ConfigureSuperPageRegionsFor32Bit(size_t, size_t, size_t) {
  return 0;
}
bool ConfigureBottomUpSuperPagesFor32Bit(uintptr_t, uintptr_t) {
  return false;
}
bool AreSuperPageRegionsAvailableFor32Bit() {
  return false;
}
SuperPageRegionStats GetSuperPageRegionStats() {
  return {};
}
namespace internal {
uintptr_t SuperPageRegionReserve(size_t) {
  return 0;
}
bool SuperPageRegionRelease(uintptr_t, size_t) {
  return false;
}
void SuperPageRegionNoteFallback(size_t) {}
bool SuperPageRegionShouldDropHint(uintptr_t) {
  return false;
}
void ResetSuperPageRegionsForTesting() {}
void SetSuperPageRegionReserveFailForTesting(bool) {}
size_t GetSuperPageRegionReserveAttemptsForTesting() {
  return 0;
}
}  // namespace internal
}  // namespace partition_alloc

#endif  // !PA_BUILDFLAG(HAS_64_BIT_POINTERS)

#if !PA_BUILDFLAG(HAS_64_BIT_POINTERS)
namespace partition_alloc {
void SetRandomHintsDisabledFor32Bit(bool disabled) {
  SetRandomPageBaseDisabledFor32Bit(disabled);
}
bool AreRandomHintsDisabledFor32Bit() {
  return IsRandomPageBaseDisabledFor32Bit();
}
}  // namespace partition_alloc
#endif  // !PA_BUILDFLAG(HAS_64_BIT_POINTERS)
