// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_ALLOC_CONSTANTS_H_
#define BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_ALLOC_CONSTANTS_H_

#include <algorithm>
#include <climits>
#include <cstddef>
#include <limits>

#include "base/allocator/partition_allocator/address_pool_manager_types.h"
#include "base/allocator/partition_allocator/page_allocator_constants.h"
#include "base/allocator/partition_allocator/partition_alloc_base/compiler_specific.h"
#include "base/allocator/partition_allocator/partition_alloc_buildflags.h"
#include "base/allocator/partition_allocator/partition_alloc_config.h"
#include "base/allocator/partition_allocator/partition_alloc_forward.h"
#include "base/allocator/partition_allocator/tagging.h"
#include "build/build_config.h"

#if BUILDFLAG(IS_APPLE) && defined(ARCH_CPU_64_BITS)
#include <mach/vm_page_size.h>
#endif

namespace partition_alloc {

// Bit flag constants used as `flag` argument of PartitionRoot::AllocWithFlags,
// AlignedAllocWithFlags, etc.
struct AllocFlags {
  static constexpr unsigned int kReturnNull = 1 << 0;
  static constexpr unsigned int kZeroFill = 1 << 1;
  // Don't allow allocation override hooks. Override hooks are expected to
  // check for the presence of this flag and return false if it is active.
  static constexpr unsigned int kNoOverrideHooks = 1 << 2;
  // Never let a memory tool like ASan (if active) perform the allocation.
  static constexpr unsigned int kNoMemoryToolOverride = 1 << 3;
  // Don't allow any hooks (override or observers).
  static constexpr unsigned int kNoHooks = 1 << 4;  // Internal.
  // If the allocation requires a "slow path" (such as allocating/committing a
  // new slot span), return nullptr instead. Note this makes all large
  // allocations return nullptr, such as direct-mapped ones, and even for
  // smaller ones, a nullptr value is common.
  static constexpr unsigned int kFastPathOrReturnNull = 1 << 5;  // Internal.

  static constexpr unsigned int kLastFlag = kFastPathOrReturnNull;
};

// Bit flag constants used as `flag` argument of PartitionRoot::FreeWithFlags.
struct FreeFlags {
  // See AllocFlags::kNoMemoryToolOverride.
  static constexpr unsigned int kNoMemoryToolOverride = 1 << 0;

  static constexpr unsigned int kLastFlag = kNoMemoryToolOverride;
};

namespace internal {

// Size of a cache line. Not all CPUs in the world have a 64 bytes cache line
// size, but as of 2021, most do. This is in particular the case for almost all
// x86_64 and almost all ARM CPUs supported by Chromium. As this is used for
// static alignment, we cannot query the CPU at runtime to determine the actual
// alignment, so use 64 bytes everywhere. Since this is only used to avoid false
// sharing, getting this wrong only results in lower performance, not incorrect
// code.
constexpr size_t kPartitionCachelineSize = 64;

// Underlying partition storage pages (`PartitionPage`s) are a power-of-2 size.
// It is typical for a `PartitionPage` to be based on multiple system pages.
// Most references to "page" refer to `PartitionPage`s.
//
// *Super pages* are the underlying system allocations we make. Super pages
// contain multiple partition pages and include space for a small amount of
// metadata per partition page.
//
// Inside super pages, we store *slot spans*. A slot span is a continguous range
// of one or more `PartitionPage`s that stores allocations of the same size.
// Slot span sizes are adjusted depending on the allocation size, to make sure
// the packing does not lead to unused (wasted) space at the end of the last
// system page of the span. For our current maximum slot span size of 64 KiB and
// other constant values, we pack _all_ `PartitionRoot::Alloc` sizes perfectly
// up against the end of a system page.

#if defined(_MIPS_ARCH_LOONGSON) || defined(ARCH_CPU_LOONG64)
PA_ALWAYS_INLINE PAGE_ALLOCATOR_CONSTANTS_DECLARE_CONSTEXPR size_t
PartitionPageShift() {
  return 16;  // 64 KiB
}
#elif defined(ARCH_CPU_PPC64)
PA_ALWAYS_INLINE PAGE_ALLOCATOR_CONSTANTS_DECLARE_CONSTEXPR size_t
PartitionPageShift() {
  return 18;  // 256 KiB
}
#elif (BUILDFLAG(IS_APPLE) && defined(ARCH_CPU_64_BITS)) || \
    (BUILDFLAG(IS_LINUX) && defined(ARCH_CPU_ARM64))
PA_ALWAYS_INLINE PAGE_ALLOCATOR_CONSTANTS_DECLARE_CONSTEXPR size_t
PartitionPageShift() {
  return PageAllocationGranularityShift() + 2;
}
#else
PA_ALWAYS_INLINE PAGE_ALLOCATOR_CONSTANTS_DECLARE_CONSTEXPR size_t
PartitionPageShift() {
  return 14;  // 16 KiB
}
#endif
PA_ALWAYS_INLINE PAGE_ALLOCATOR_CONSTANTS_DECLARE_CONSTEXPR size_t
PartitionPageSize() {
  return 1 << PartitionPageShift();
}
PA_ALWAYS_INLINE PAGE_ALLOCATOR_CONSTANTS_DECLARE_CONSTEXPR size_t
PartitionPageOffsetMask() {
  return PartitionPageSize() - 1;
}
PA_ALWAYS_INLINE PAGE_ALLOCATOR_CONSTANTS_DECLARE_CONSTEXPR size_t
PartitionPageBaseMask() {
  return ~PartitionPageOffsetMask();
}

// Number of system pages per regular slot span. Above this limit, we call it
// a single-slot span, as the span literally hosts only one slot, and has
// somewhat different implementation. At run-time, single-slot spans can be
// differentiated with a call to CanStoreRawSize().
// TODO: Should this be 1 on platforms with page size larger than 4kB, e.g.
// ARM macOS or defined(_MIPS_ARCH_LOONGSON)?
constexpr size_t kMaxPartitionPagesPerRegularSlotSpan = 4;

// To avoid fragmentation via never-used freelist entries, we hand out partition
// freelist sections gradually, in units of the dominant system page size. What
// we're actually doing is avoiding filling the full `PartitionPage` (16 KiB)
// with freelist pointers right away. Writing freelist pointers will fault and
// dirty a private page, which is very wasteful if we never actually store
// objects there.

PA_ALWAYS_INLINE PAGE_ALLOCATOR_CONSTANTS_DECLARE_CONSTEXPR size_t
NumSystemPagesPerPartitionPage() {
  return PartitionPageSize() >> SystemPageShift();
}

PA_ALWAYS_INLINE PAGE_ALLOCATOR_CONSTANTS_DECLARE_CONSTEXPR size_t
MaxSystemPagesPerRegularSlotSpan() {
  return NumSystemPagesPerPartitionPage() *
         kMaxPartitionPagesPerRegularSlotSpan;
}

PA_ALWAYS_INLINE PAGE_ALLOCATOR_CONSTANTS_DECLARE_CONSTEXPR size_t
MaxRegularSlotSpanSize() {
  return kMaxPartitionPagesPerRegularSlotSpan << PartitionPageShift();
}

// The maximum size that is used in an alternate bucket distribution. After this
// threshold, we only have 1 slot per slot-span, so external fragmentation
// doesn't matter. So, using the alternate bucket distribution after this
// threshold has no benefit, and only increases internal fragmentation.
//
// We would like this to be |MaxRegularSlotSpanSize()| on all platforms, but
// this is not constexpr on all platforms, so on other platforms we hardcode it,
// even though this may be too low, e.g. on systems with a page size >4KiB.
constexpr size_t kHighThresholdForAlternateDistribution =
#if PAGE_ALLOCATOR_CONSTANTS_ARE_CONSTEXPR
    MaxRegularSlotSpanSize();
#else
    1 << 16;
#endif

// We reserve virtual address space in 2 MiB chunks (aligned to 2 MiB as well).
// These chunks are called *super pages*. We do this so that we can store
// metadata in the first few pages of each 2 MiB-aligned section. This makes
// freeing memory very fast. 2 MiB size & alignment were chosen, because this
// virtual address block represents a full but single page table allocation on
// ARM, ia32 and x64, which may be slightly more performance&memory efficient.
// (Note, these super pages are backed by 4 KiB system pages and have nothing to
// do with OS concept of "huge pages"/"large pages", even though the size
// coincides.)
//
// The layout of the super page is as follows. The sizes below are the same for
// 32- and 64-bit platforms.
//
//     +-----------------------+
//     | Guard page (4 KiB)    |
//     | Metadata page (4 KiB) |
//     | Guard pages (8 KiB)   |
//     | Free Slot Bitmap      |
//     | *Scan State Bitmap    |
//     | Slot span             |
//     | Slot span             |
//     | ...                   |
//     | Slot span             |
//     | Guard pages (16 KiB)  |
//     +-----------------------+
//
// Free Slot Bitmap is only present when USE_FREESLOT_BITMAP is true. State
// Bitmap is inserted for partitions that may have quarantine enabled.
//
// If refcount_at_end_allocation is enabled, RefcountBitmap(4KiB) is inserted
// after the Metadata page for BackupRefPtr. The guard pages after the bitmap
// will be 4KiB.
//
//...
//     | Metadata page (4 KiB) |
//     | RefcountBitmap (4 KiB)|
//     | Guard pages (4 KiB)   |
//...
//
// Each slot span is a contiguous range of one or more `PartitionPage`s. Note
// that slot spans of different sizes may co-exist with one super page. Even
// slot spans of the same size may support different slot sizes. However, all
// slots within a span have to be of the same size.
//
// The metadata page has the following format. Note that the `PartitionPage`
// that is not at the head of a slot span is "unused" (by most part, it only
// stores the offset from the head page). In other words, the metadata for the
// slot span is stored only in the first `PartitionPage` of the slot span.
// Metadata accesses to other `PartitionPage`s are redirected to the first
// `PartitionPage`.
//
//     +---------------------------------------------+
//     | SuperPageExtentEntry (32 B)                 |
//     | PartitionPage of slot span 1 (32 B, used)   |
//     | PartitionPage of slot span 1 (32 B, unused) |
//     | PartitionPage of slot span 1 (32 B, unused) |
//     | PartitionPage of slot span 2 (32 B, used)   |
//     | PartitionPage of slot span 3 (32 B, used)   |
//     | ...                                         |
//     | PartitionPage of slot span N (32 B, used)   |
//     | PartitionPage of slot span N (32 B, unused) |
//     | PartitionPage of slot span N (32 B, unused) |
//     +---------------------------------------------+
//
// A direct-mapped page has an identical layout at the beginning to fake it
// looking like a super page:
//
//     +---------------------------------+
//     | Guard page (4 KiB)              |
//     | Metadata page (4 KiB)           |
//     | Guard pages (8 KiB)             |
//     | Direct mapped object            |
//     | Guard page (4 KiB, 32-bit only) |
//     +---------------------------------+
//
// A direct-mapped page's metadata page has the following layout (on 64 bit
// architectures. On 32 bit ones, the layout is identical, some sizes are
// different due to smaller pointers.):
//
//     +----------------------------------+
//     | SuperPageExtentEntry (32 B)      |
//     | PartitionPage (32 B)             |
//     | PartitionBucket (40 B)           |
//     | PartitionDirectMapExtent (32 B)  |
//     +----------------------------------+
//
// See |PartitionDirectMapMetadata| for details.

constexpr size_t kGiB = 1024 * 1024 * 1024ull;
constexpr size_t kSuperPageShift = 21;  // 2 MiB
constexpr size_t kSuperPageSize = 1 << kSuperPageShift;
constexpr size_t kSuperPageAlignment = kSuperPageSize;
constexpr size_t kSuperPageOffsetMask = kSuperPageAlignment - 1;
constexpr size_t kSuperPageBaseMask = ~kSuperPageOffsetMask;

// PartitionAlloc's address space is split into pools. See `glossary.md`.

enum pool_handle : unsigned {
  kNullPoolHandle = 0u,

  kRegularPoolHandle,
  kBRPPoolHandle,
#if BUILDFLAG(HAS_64_BIT_POINTERS)
  kConfigurablePoolHandle,
#endif

// New pool_handles will be added here.

#if BUILDFLAG(ENABLE_PKEYS)
  // The pkey pool must come last since we pkey_mprotect its entry in the
  // metadata tables, e.g. AddressPoolManager::aligned_pools_
  kPkeyPoolHandle,
#endif
  kMaxPoolHandle
};

// kNullPoolHandle doesn't have metadata, hence - 1
constexpr size_t kNumPools = kMaxPoolHandle - 1;

// Maximum pool size. With exception of Configurable Pool, it is also
// the actual size, unless PA_DYNAMICALLY_SELECT_POOL_SIZE is set, which
// allows to choose a different size at initialization time for certain
// configurations.
//
// Special-case Android and iOS, which incur test failures with larger
// pools. Regardless, allocating >8GiB with malloc() on these platforms is
// unrealistic as of 2022.
//
// When pointer compression is enabled, we cannot use large pools (at most
// 8GB for each of the glued pools).
#if BUILDFLAG(HAS_64_BIT_POINTERS)
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS) || PA_CONFIG(POINTER_COMPRESSION)
constexpr size_t kPoolMaxSize = 8 * kGiB;
#else
constexpr size_t kPoolMaxSize = 16 * kGiB;
#endif
#else  // BUILDFLAG(HAS_64_BIT_POINTERS)
constexpr size_t kPoolMaxSize = 4 * kGiB;
#endif
constexpr size_t kMaxSuperPagesInPool = kPoolMaxSize / kSuperPageSize;

#if BUILDFLAG(ENABLE_PKEYS)
static_assert(
    kPkeyPoolHandle == kNumPools,
    "The pkey pool must come last since we pkey_mprotect its metadata.");
#endif

// Slots larger than this size will not receive MTE protection. Pages intended
// for allocations larger than this constant should not be backed with PROT_MTE
// (which saves shadow tag memory). We also save CPU cycles by skipping tagging
// of large areas which are less likely to benefit from MTE protection.
// TODO(Richard.Townsend@arm.com): adjust RecommitSystemPagesForData to skip
// PROT_MTE.
constexpr size_t kMaxMemoryTaggingSize = 1024;

#if PA_CONFIG(HAS_MEMORY_TAGGING)
// Returns whether the tag of |object| overflowed, meaning the containing slot
// needs to be moved to quarantine.
PA_ALWAYS_INLINE bool HasOverflowTag(void* object) {
  // The tag with which the slot is put to quarantine.
  constexpr uintptr_t kOverflowTag = 0x0f00000000000000uLL;
  static_assert((kOverflowTag & kPtrTagMask) != 0,
                "Overflow tag must be in tag bits");
  return (reinterpret_cast<uintptr_t>(object) & kPtrTagMask) == kOverflowTag;
}
#endif  // PA_CONFIG(HAS_MEMORY_TAGGING)

PA_ALWAYS_INLINE PAGE_ALLOCATOR_CONSTANTS_DECLARE_CONSTEXPR size_t
NumPartitionPagesPerSuperPage() {
  return kSuperPageSize >> PartitionPageShift();
}

PA_ALWAYS_INLINE constexpr size_t MaxSuperPagesInPool() {
  return kMaxSuperPagesInPool;
}

#if BUILDFLAG(HAS_64_BIT_POINTERS)
// In 64-bit mode, the direct map allocation granularity is super page size,
// because this is the reservation granularity of the pools.
PA_ALWAYS_INLINE constexpr size_t DirectMapAllocationGranularity() {
  return kSuperPageSize;
}

PA_ALWAYS_INLINE constexpr size_t DirectMapAllocationGranularityShift() {
  return kSuperPageShift;
}
#else   // BUILDFLAG(HAS_64_BIT_POINTERS)
// In 32-bit mode, address space is space is a scarce resource. Use the system
// allocation granularity, which is the lowest possible address space allocation
// unit. However, don't go below partition page size, so that pool bitmaps
// don't get too large. See kBytesPer1BitOfBRPPoolBitmap.
PA_ALWAYS_INLINE PAGE_ALLOCATOR_CONSTANTS_DECLARE_CONSTEXPR size_t
DirectMapAllocationGranularity() {
  return std::max(PageAllocationGranularity(), PartitionPageSize());
}

PA_ALWAYS_INLINE PAGE_ALLOCATOR_CONSTANTS_DECLARE_CONSTEXPR size_t
DirectMapAllocationGranularityShift() {
  return std::max(PageAllocationGranularityShift(), PartitionPageShift());
}
#endif  // BUILDFLAG(HAS_64_BIT_POINTERS)

PA_ALWAYS_INLINE PAGE_ALLOCATOR_CONSTANTS_DECLARE_CONSTEXPR size_t
DirectMapAllocationGranularityOffsetMask() {
  return DirectMapAllocationGranularity() - 1;
}

// The "order" of an allocation is closely related to the power-of-1 size of the
// allocation. More precisely, the order is the bit index of the
// most-significant-bit in the allocation size, where the bit numbers starts at
// index 1 for the least-significant-bit.
//
// In terms of allocation sizes, order 0 covers 0, order 1 covers 1, order 2
// covers 2->3, order 3 covers 4->7, order 4 covers 8->15.

// PartitionAlloc should return memory properly aligned for any type, to behave
// properly as a generic allocator. This is not strictly required as long as
// types are explicitly allocated with PartitionAlloc, but is to use it as a
// malloc() implementation, and generally to match malloc()'s behavior.
//
// In practice, this means 8 bytes alignment on 32 bit architectures, and 16
// bytes on 64 bit ones.
//
// Keep in sync with //tools/memory/partition_allocator/objects_per_size_py.
constexpr size_t kMinBucketedOrder =
    kAlignment == 16 ? 5 : 4;  // 2^(order - 1), that is 16 or 8.
// The largest bucketed order is 1 << (20 - 1), storing [512 KiB, 1 MiB):
constexpr size_t kMaxBucketedOrder = 20;
constexpr size_t kNumBucketedOrders =
    (kMaxBucketedOrder - kMinBucketedOrder) + 1;
// 8 buckets per order (for the higher orders).
// Note: this is not what is used by default, but the maximum amount of buckets
// per order. By default, only 4 are used.
constexpr size_t kNumBucketsPerOrderBits = 3;
constexpr size_t kNumBucketsPerOrder = 1 << kNumBucketsPerOrderBits;
constexpr size_t kNumBuckets = kNumBucketedOrders * kNumBucketsPerOrder;
constexpr size_t kSmallestBucket = 1 << (kMinBucketedOrder - 1);
constexpr size_t kMaxBucketSpacing =
    1 << ((kMaxBucketedOrder - 1) - kNumBucketsPerOrderBits);
constexpr size_t kMaxBucketed = (1 << (kMaxBucketedOrder - 1)) +
                                ((kNumBucketsPerOrder - 1) * kMaxBucketSpacing);
// Limit when downsizing a direct mapping using `realloc`:
constexpr size_t kMinDirectMappedDownsize = kMaxBucketed + 1;
// Intentionally set to less than 2GiB to make sure that a 2GiB allocation
// fails. This is a security choice in Chrome, to help making size_t vs int bugs
// harder to exploit.

// The definition of MaxDirectMapped does only depend on constants that are
// unconditionally constexpr. Therefore it is not necessary to use
// PAGE_ALLOCATOR_CONSTANTS_DECLARE_CONSTEXPR here.
PA_ALWAYS_INLINE constexpr size_t MaxDirectMapped() {
  // Subtract kSuperPageSize to accommodate for granularity inside
  // PartitionRoot::GetDirectMapReservationSize.
  return (1UL << 31) - kSuperPageSize;
}

// Max alignment supported by AlignedAllocWithFlags().
// kSuperPageSize alignment can't be easily supported, because each super page
// starts with guard pages & metadata.
constexpr size_t kMaxSupportedAlignment = kSuperPageSize / 2;

constexpr size_t kBitsPerSizeT = sizeof(void*) * CHAR_BIT;

// When a SlotSpan becomes empty, the allocator tries to avoid re-using it
// immediately, to help with fragmentation. At this point, it becomes dirty
// committed memory, which we want to minimize. This could be decommitted
// immediately, but that would imply doing a lot of system calls. In particular,
// for single-slot SlotSpans, a malloc() / free() loop would cause a *lot* of
// system calls.
//
// As an intermediate step, empty SlotSpans are placed into a per-partition
// global ring buffer, giving the newly-empty SlotSpan a chance to be re-used
// before getting decommitted. A new entry (i.e. a newly empty SlotSpan) taking
// the place used by a previous one will lead the previous SlotSpan to be
// decommitted immediately, provided that it is still empty.
//
// Setting this value higher means giving more time for reuse to happen, at the
// cost of possibly increasing peak committed memory usage (and increasing the
// size of PartitionRoot a bit, since the ring buffer is there). Note that the
// ring buffer doesn't necessarily contain an empty SlotSpan, as SlotSpans are
// *not* removed from it when re-used. So the ring buffer really is a buffer of
// *possibly* empty SlotSpans.
//
// In all cases, PartitionRoot::PurgeMemory() with the
// PurgeFlags::kDecommitEmptySlotSpans flag will eagerly decommit all entries
// in the ring buffer, so with periodic purge enabled, this typically happens
// every few seconds.
constexpr size_t kEmptyCacheIndexBits = 7;
// kMaxFreeableSpans is the buffer size, but is never used as an index value,
// hence <= is appropriate.
constexpr size_t kMaxFreeableSpans = 1 << kEmptyCacheIndexBits;
constexpr size_t kDefaultEmptySlotSpanRingSize = 16;

// If the total size in bytes of allocated but not committed pages exceeds this
// value (probably it is a "out of virtual address space" crash), a special
// crash stack trace is generated at
// `PartitionOutOfMemoryWithLotsOfUncommitedPages`. This is to distinguish "out
// of virtual address space" from "out of physical memory" in crash reports.
constexpr size_t kReasonableSizeOfUnusedPages = 1024 * 1024 * 1024;  // 1 GiB

// These byte values match tcmalloc.
constexpr unsigned char kUninitializedByte = 0xAB;
constexpr unsigned char kFreedByte = 0xCD;

constexpr unsigned char kQuarantinedByte = 0xEF;

// 1 is smaller than anything we can use, as it is not properly aligned. Not
// using a large size, since PartitionBucket::slot_size is a uint32_t, and
// static_cast<uint32_t>(-1) is too close to a "real" size.
constexpr size_t kInvalidBucketSize = 1;

#if PA_CONFIG(ENABLE_MAC11_MALLOC_SIZE_HACK)
// Requested size that require the hack.
constexpr size_t kMac11MallocSizeHackRequestedSize = 32;
// Usable size for allocations that require the hack.
constexpr size_t kMac11MallocSizeHackUsableSize =
#if BUILDFLAG(ENABLE_DANGLING_RAW_PTR_CHECKS) || \
    PA_CONFIG(REF_COUNT_STORE_REQUESTED_SIZE) || \
    PA_CONFIG(REF_COUNT_CHECK_COOKIE)
    40;
#else
    44;
#endif  // BUILDFLAG(ENABLE_DANGLING_RAW_PTR_CHECKS) ||
        // PA_CONFIG(REF_COUNT_STORE_REQUESTED_SIZE) ||
        // PA_CONFIG(REF_COUNT_CHECK_COOKIE)
#endif  // PA_CONFIG(ENABLE_MAC11_MALLOC_SIZE_HACK)
}  // namespace internal

// These constants are used outside PartitionAlloc itself, so we provide
// non-internal aliases here.
using ::partition_alloc::internal::kInvalidBucketSize;
using ::partition_alloc::internal::kMaxSuperPagesInPool;
using ::partition_alloc::internal::kMaxSupportedAlignment;
using ::partition_alloc::internal::kNumBuckets;
using ::partition_alloc::internal::kSuperPageSize;
using ::partition_alloc::internal::MaxDirectMapped;
using ::partition_alloc::internal::PartitionPageSize;

}  // namespace partition_alloc

#endif  // BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_ALLOC_CONSTANTS_H_
