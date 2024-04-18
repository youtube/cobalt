// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_REF_COUNT_H_
#define BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_REF_COUNT_H_

#include <atomic>
#include <cstdint>

#include "base/allocator/partition_allocator/dangling_raw_ptr_checks.h"
#include "base/allocator/partition_allocator/partition_alloc_base/compiler_specific.h"
#include "base/allocator/partition_allocator/partition_alloc_base/component_export.h"
#include "base/allocator/partition_allocator/partition_alloc_base/debug/debugging_buildflags.h"
#include "base/allocator/partition_allocator/partition_alloc_base/immediate_crash.h"
#include "base/allocator/partition_allocator/partition_alloc_buildflags.h"
#include "base/allocator/partition_allocator/partition_alloc_check.h"
#include "base/allocator/partition_allocator/partition_alloc_config.h"
#include "base/allocator/partition_allocator/partition_alloc_constants.h"
#include "base/allocator/partition_allocator/partition_alloc_forward.h"
#include "base/allocator/partition_allocator/tagging.h"
#include "build/build_config.h"

namespace partition_alloc::internal {

#if BUILDFLAG(ENABLE_BACKUP_REF_PTR_SUPPORT)

// Special-purpose atomic reference count class used by RawPtrBackupRefImpl.
// The least significant bit of the count is reserved for tracking the liveness
// state of an allocation: it's set when the allocation is created and cleared
// on free(). So the count can be:
//
// 1 for an allocation that is just returned from Alloc()
// 2 * k + 1 for a "live" allocation with k references
// 2 * k for an allocation with k dangling references after Free()
//
// This protects against double-free's, as we check whether the reference count
// is odd in |ReleaseFromAllocator()|, and if not we have a double-free.
class PA_COMPONENT_EXPORT(PARTITION_ALLOC) PartitionRefCount {
 public:
  // This class holds an atomic bit field: `count_`. It holds up to 5 values:
  //
  // bits   name                   description
  // -----  ---------------------  ----------------------------------------
  // 0      is_allocated           Whether or not the memory is held by the
  //                               allocator.
  //                               - 1 at construction time.
  //                               - Decreased in ReleaseFromAllocator();
  //
  // 1-31   ptr_count              Number of raw_ptr<T>.
  //                               - Increased in Acquire()
  //                               - Decreased in Release()
  //
  // 32     dangling_detected      A dangling raw_ptr<> has been detected.
  // 33     needs_mac11_malloc_    Whether malloc_size() return value needs to
  //          size_hack            be adjusted for this allocation.
  //
  // 34-63  unprotected_ptr_count  Number of
  //                               raw_ptr<T, DisableDanglingPtrDetection>
  //                               - Increased in AcquireFromUnprotectedPtr().
  //                               - Decreased in ReleaseFromUnprotectedPtr().
  //
  // The allocation is reclaimed if all of:
  // - |is_allocated|
  // - |ptr_count|
  // - |unprotected_ptr_count|
  // are zero.
  //
  // During ReleaseFromAllocator(), if |ptr_count| is not zero,
  // |dangling_detected| is set and the error is reported via
  // DanglingRawPtrDetected(id). The matching DanglingRawPtrReleased(id) will be
  // called when the last raw_ptr<> is released.
#if BUILDFLAG(ENABLE_DANGLING_RAW_PTR_CHECKS)
  using CountType = uint64_t;
  static constexpr CountType kMemoryHeldByAllocatorBit = 0x0000'0000'0000'0001;
  static constexpr CountType kPtrCountMask = 0x0000'0000'FFFF'FFFE;
  static constexpr CountType kUnprotectedPtrCountMask = 0xFFFF'FFFC'0000'0000;
  static constexpr CountType kDanglingRawPtrDetectedBit = 0x0000'0001'0000'0000;
  static constexpr CountType kNeedsMac11MallocSizeHackBit =
      0x0000'0002'0000'0000;

  static constexpr CountType kPtrInc = 0x0000'0000'0000'0002;
  static constexpr CountType kUnprotectedPtrInc = 0x0000'0004'0000'0000;
#else
  using CountType = uint32_t;
  static constexpr CountType kMemoryHeldByAllocatorBit = 0x0000'0001;

  static constexpr CountType kPtrCountMask = 0x7FFF'FFFE;
  static constexpr CountType kUnprotectedPtrCountMask = 0x0000'0000;
  static constexpr CountType kDanglingRawPtrDetectedBit = 0x0000'0000;
  static constexpr CountType kNeedsMac11MallocSizeHackBit = 0x8000'0000;

  static constexpr CountType kPtrInc = 0x0000'0002;
#endif

  PA_ALWAYS_INLINE explicit PartitionRefCount(
      bool needs_mac11_malloc_size_hack);

  // Incrementing the counter doesn't imply any visibility about modified
  // memory, hence relaxed atomics. For decrement, visibility is required before
  // the memory gets freed, necessitating an acquire/release barrier before
  // freeing the memory.
  //
  // For details, see base::AtomicRefCount, which has the same constraints and
  // characteristics.
  //
  // FYI: The assembly produced by the compiler on every platform, in particular
  // the uint64_t fetch_add on 32bit CPU.
  // https://docs.google.com/document/d/1cSTVDVEE-8l2dXLPcfyN75r6ihMbeiSp1ncL9ae3RZE
  PA_ALWAYS_INLINE void Acquire() {
    CheckCookieIfSupported();

#if BUILDFLAG(ENABLE_DANGLING_RAW_PTR_PERF_EXPERIMENT)
    constexpr CountType kInc = kUnprotectedPtrInc;
    constexpr CountType kMask = kUnprotectedPtrCountMask;
#else
    constexpr CountType kInc = kPtrInc;
    constexpr CountType kMask = kPtrCountMask;
#endif
    CountType old_count = count_.fetch_add(kInc, std::memory_order_relaxed);
    // Check overflow.
    PA_CHECK((old_count & kMask) != kMask);
  }

  // Similar to |Acquire()|, but for raw_ptr<T, DisableDanglingPtrDetection>
  // instead of raw_ptr<T>.
  PA_ALWAYS_INLINE void AcquireFromUnprotectedPtr() {
#if BUILDFLAG(ENABLE_DANGLING_RAW_PTR_CHECKS)
    CheckCookieIfSupported();
    CountType old_count =
        count_.fetch_add(kUnprotectedPtrInc, std::memory_order_relaxed);
    // Check overflow.
    PA_CHECK((old_count & kUnprotectedPtrCountMask) !=
             kUnprotectedPtrCountMask);
#else
    Acquire();
#endif
  }

  // Returns true if the allocation should be reclaimed.
  PA_ALWAYS_INLINE bool Release() {
#if BUILDFLAG(ENABLE_DANGLING_RAW_PTR_PERF_EXPERIMENT)
    constexpr CountType kInc = kUnprotectedPtrInc;
    constexpr CountType kMask = kUnprotectedPtrCountMask;
#else
    constexpr CountType kInc = kPtrInc;
    constexpr CountType kMask = kPtrCountMask;
#endif
    CheckCookieIfSupported();

    CountType old_count = count_.fetch_sub(kInc, std::memory_order_release);
    // Check underflow.
    PA_DCHECK(old_count & kMask);

#if BUILDFLAG(ENABLE_DANGLING_RAW_PTR_CHECKS)
    // If a dangling raw_ptr<> was detected, report it.
    if (PA_UNLIKELY((old_count & kDanglingRawPtrDetectedBit) ==
                    kDanglingRawPtrDetectedBit)) {
      partition_alloc::internal::DanglingRawPtrReleased(
          reinterpret_cast<uintptr_t>(this));
    }
#endif

    return ReleaseCommon(old_count - kInc);
  }

  // Similar to |Release()|, but for raw_ptr<T, DisableDanglingPtrDetection>
  // instead of raw_ptr<T>.
  PA_ALWAYS_INLINE bool ReleaseFromUnprotectedPtr() {
#if BUILDFLAG(ENABLE_DANGLING_RAW_PTR_CHECKS)
    CheckCookieIfSupported();

    CountType old_count =
        count_.fetch_sub(kUnprotectedPtrInc, std::memory_order_release);
    // Check underflow.
    PA_DCHECK(old_count & kUnprotectedPtrCountMask);

    return ReleaseCommon(old_count - kUnprotectedPtrInc);
#else
    return Release();
#endif
  }

  // Returns true if the allocation should be reclaimed.
  // This function should be called by the allocator during Free().
  PA_ALWAYS_INLINE bool ReleaseFromAllocator() {
    CheckCookieIfSupported();

    // TODO(bartekn): Make the double-free check more effective. Once freed, the
    // ref-count is overwritten by an encoded freelist-next pointer.
    CountType old_count =
        count_.fetch_and(~kMemoryHeldByAllocatorBit, std::memory_order_release);

    if (PA_UNLIKELY(!(old_count & kMemoryHeldByAllocatorBit))) {
      DoubleFreeOrCorruptionDetected(old_count);
    }

    if (PA_LIKELY((old_count & ~kNeedsMac11MallocSizeHackBit) ==
                  kMemoryHeldByAllocatorBit)) {
      std::atomic_thread_fence(std::memory_order_acquire);
      // The allocation is about to get freed, so clear the cookie.
      ClearCookieIfSupported();
      return true;
    }

#if BUILDFLAG(ENABLE_DANGLING_RAW_PTR_CHECKS)
    // Check if any raw_ptr<> still exists. It is now dangling.
    if (PA_UNLIKELY(old_count & kPtrCountMask)) {
      count_.fetch_or(kDanglingRawPtrDetectedBit, std::memory_order_relaxed);
      partition_alloc::internal::DanglingRawPtrDetected(
          reinterpret_cast<uintptr_t>(this));
    }
#endif  // BUILDFLAG(ENABLE_DANGLING_RAW_PTR_CHECKS)
    return false;
  }

  // "IsAlive" means is allocated and not freed. "KnownRefs" refers to
  // raw_ptr<T> references. There may be other references from raw pointers or
  // unique_ptr, but we have no way of tracking them, so we hope for the best.
  // To summarize, the function returns whether we believe the allocation can be
  // safely freed.
  PA_ALWAYS_INLINE bool IsAliveWithNoKnownRefs() {
    CheckCookieIfSupported();
    return (count_.load(std::memory_order_acquire) &
            ~kNeedsMac11MallocSizeHackBit) == kMemoryHeldByAllocatorBit;
  }

  PA_ALWAYS_INLINE bool IsAlive() {
    bool alive =
        count_.load(std::memory_order_relaxed) & kMemoryHeldByAllocatorBit;
    if (alive) {
      CheckCookieIfSupported();
    }
    return alive;
  }

  // Called when a raw_ptr is not banning dangling ptrs, but the user still
  // wants to ensure the pointer is not currently dangling. This is currently
  // used in UnretainedWrapper to make sure callbacks are not invoked with
  // dangling pointers. If such a raw_ptr exists but the allocation is no longer
  // alive, then we have a dangling pointer to a dead object.
  PA_ALWAYS_INLINE void ReportIfDangling() {
    if (!IsAlive()) {
      partition_alloc::internal::UnretainedDanglingRawPtrDetected(
          reinterpret_cast<uintptr_t>(this));
    }
  }

  // GWP-ASan slots are assigned an extra reference (note `kPtrInc` below) to
  // make sure the `raw_ptr<T>` release operation will never attempt to call the
  // PA `free` on such a slot. GWP-ASan takes the extra reference into account
  // when determining whether the slot can be reused.
  PA_ALWAYS_INLINE void InitalizeForGwpAsan() {
#if PA_CONFIG(REF_COUNT_CHECK_COOKIE)
    brp_cookie_ = CalculateCookie();
#endif
    count_.store(kPtrInc | kMemoryHeldByAllocatorBit,
                 std::memory_order_release);
  }

  PA_ALWAYS_INLINE bool CanBeReusedByGwpAsan() {
    return (count_.load(std::memory_order_acquire) &
            ~kNeedsMac11MallocSizeHackBit) ==
           (kPtrInc | kMemoryHeldByAllocatorBit);
  }

  bool NeedsMac11MallocSizeHack() {
    return count_.load(std::memory_order_relaxed) &
           kNeedsMac11MallocSizeHackBit;
  }

#if PA_CONFIG(REF_COUNT_STORE_REQUESTED_SIZE)
  PA_ALWAYS_INLINE void SetRequestedSize(size_t size) {
    requested_size_ = static_cast<uint32_t>(size);
  }
  PA_ALWAYS_INLINE uint32_t requested_size() const { return requested_size_; }
#endif  // PA_CONFIG(REF_COUNT_STORE_REQUESTED_SIZE)

 private:
  // The common parts shared by Release() and ReleaseFromUnprotectedPtr().
  // Called after updating the ref counts, |count| is the new value of |count_|
  // set by fetch_sub. Returns true if memory can be reclaimed.
  PA_ALWAYS_INLINE bool ReleaseCommon(CountType count) {
    // Do not release memory, if it is still held by any of:
    // - The allocator
    // - A raw_ptr<T>
    // - A raw_ptr<T, DisableDanglingPtrDetection>
    //
    // Assuming this raw_ptr is not dangling, the memory must still be held at
    // least by the allocator, so this is PA_LIKELY true.
    if (PA_LIKELY((count & (kMemoryHeldByAllocatorBit | kPtrCountMask |
                            kUnprotectedPtrCountMask)))) {
      return false;  // Do not release the memory.
    }

    // In most thread-safe reference count implementations, an acquire
    // barrier is required so that all changes made to an object from other
    // threads are visible to its destructor. In our case, the destructor
    // finishes before the final `Release` call, so it shouldn't be a problem.
    // However, we will keep it as a precautionary measure.
    std::atomic_thread_fence(std::memory_order_acquire);

    // The allocation is about to get freed, so clear the cookie.
    ClearCookieIfSupported();
    return true;
  }

  // The cookie helps us ensure that:
  // 1) The reference count pointer calculation is correct.
  // 2) The returned allocation slot is not freed.
  PA_ALWAYS_INLINE void CheckCookieIfSupported() {
#if PA_CONFIG(REF_COUNT_CHECK_COOKIE)
    PA_CHECK(brp_cookie_ == CalculateCookie());
#endif
  }

  PA_ALWAYS_INLINE void ClearCookieIfSupported() {
#if PA_CONFIG(REF_COUNT_CHECK_COOKIE)
    brp_cookie_ = 0;
#endif
  }

#if PA_CONFIG(REF_COUNT_CHECK_COOKIE)
  PA_ALWAYS_INLINE uint32_t CalculateCookie() {
    return static_cast<uint32_t>(reinterpret_cast<uintptr_t>(this)) ^
           kCookieSalt;
  }
#endif  // PA_CONFIG(REF_COUNT_CHECK_COOKIE)

  [[noreturn]] PA_NOINLINE PA_NOT_TAIL_CALLED void
  DoubleFreeOrCorruptionDetected(CountType count) {
    PA_DEBUG_DATA_ON_STACK("refcount", count);
    PA_NO_CODE_FOLDING();
    PA_IMMEDIATE_CRASH();
  }

  // Note that in free slots, this is overwritten by encoded freelist
  // pointer(s). The way the pointers are encoded on 64-bit little-endian
  // architectures, count_ happens stay even, which works well with the
  // double-free-detection in ReleaseFromAllocator(). Don't change the layout of
  // this class, to preserve this functionality.
  std::atomic<CountType> count_;

#if PA_CONFIG(REF_COUNT_CHECK_COOKIE)
  static constexpr uint32_t kCookieSalt = 0xc01dbeef;
  volatile uint32_t brp_cookie_;
#endif

#if PA_CONFIG(REF_COUNT_STORE_REQUESTED_SIZE)
  uint32_t requested_size_;
#endif
};

PA_ALWAYS_INLINE PartitionRefCount::PartitionRefCount(
    bool needs_mac11_malloc_size_hack)
    : count_(kMemoryHeldByAllocatorBit |
             (needs_mac11_malloc_size_hack ? kNeedsMac11MallocSizeHackBit : 0))
#if PA_CONFIG(REF_COUNT_CHECK_COOKIE)
      ,
      brp_cookie_(CalculateCookie())
#endif
{
}

#if BUILDFLAG(PUT_REF_COUNT_IN_PREVIOUS_SLOT)

static_assert(kAlignment % alignof(PartitionRefCount) == 0,
              "kAlignment must be multiples of alignof(PartitionRefCount).");

// Allocate extra space for the reference count to satisfy the alignment
// requirement.
static constexpr size_t kInSlotRefCountBufferSize = sizeof(PartitionRefCount);
constexpr size_t kPartitionRefCountOffsetAdjustment = 0;
constexpr size_t kPartitionPastAllocationAdjustment = 0;

#if BUILDFLAG(ENABLE_DANGLING_RAW_PTR_CHECKS)

#if PA_CONFIG(REF_COUNT_CHECK_COOKIE) || \
    PA_CONFIG(REF_COUNT_STORE_REQUESTED_SIZE)
static constexpr size_t kPartitionRefCountSizeShift = 4;
#else
static constexpr size_t kPartitionRefCountSizeShift = 3;
#endif

#else  // BUILDFLAG(ENABLE_DANGLING_RAW_PTR_CHECKS)

#if PA_CONFIG(REF_COUNT_CHECK_COOKIE) && \
    PA_CONFIG(REF_COUNT_STORE_REQUESTED_SIZE)
static constexpr size_t kPartitionRefCountSizeShift = 4;
#elif PA_CONFIG(REF_COUNT_CHECK_COOKIE) || \
    PA_CONFIG(REF_COUNT_STORE_REQUESTED_SIZE)
static constexpr size_t kPartitionRefCountSizeShift = 3;
#else
static constexpr size_t kPartitionRefCountSizeShift = 2;
#endif

#endif  // PA_CONFIG(REF_COUNT_CHECK_COOKIE)
static_assert((1 << kPartitionRefCountSizeShift) == sizeof(PartitionRefCount));

// We need one PartitionRefCount for each system page in a super page. They take
// `x = sizeof(PartitionRefCount) * (kSuperPageSize / SystemPageSize())` space.
// They need to fit into a system page of metadata as sparsely as possible to
// minimize cache line sharing, hence we calculate a multiplier as
// `SystemPageSize() / x`.
//
// The multiplier is expressed as a bitshift to optimize the code generation.
// SystemPageSize() isn't always a constrexpr, in which case the compiler
// wouldn't know it's a power of two. The equivalence of these calculations is
// checked in PartitionAllocGlobalInit().
PA_ALWAYS_INLINE static PAGE_ALLOCATOR_CONSTANTS_DECLARE_CONSTEXPR size_t
GetPartitionRefCountIndexMultiplierShift() {
  return SystemPageShift() * 2 - kSuperPageShift - kPartitionRefCountSizeShift;
}

PA_ALWAYS_INLINE PartitionRefCount* PartitionRefCountPointer(
    uintptr_t slot_start) {
#if BUILDFLAG(PA_DCHECK_IS_ON) || BUILDFLAG(ENABLE_BACKUP_REF_PTR_SLOW_CHECKS)
  CheckThatSlotOffsetIsZero(slot_start);
#endif
  if (PA_LIKELY(slot_start & SystemPageOffsetMask())) {
    uintptr_t refcount_address = slot_start - sizeof(PartitionRefCount);
#if BUILDFLAG(PA_DCHECK_IS_ON) || BUILDFLAG(ENABLE_BACKUP_REF_PTR_SLOW_CHECKS)
    PA_CHECK(refcount_address % alignof(PartitionRefCount) == 0);
#endif
    // Have to MTE-tag, because the address is untagged, but lies within a slot
    // area, which is protected by MTE.
    //
    // There could be a race condition though if the previous slot is
    // freed/retagged concurrently, so ideally the ref count should occupy its
    // own MTE granule.
    // TODO(richard.townsend@arm.com): improve this.
    return static_cast<PartitionRefCount*>(TagAddr(refcount_address));
  } else {
    // No need to tag, as the metadata region isn't protected by MTE.
    PartitionRefCount* bitmap_base = reinterpret_cast<PartitionRefCount*>(
        (slot_start & kSuperPageBaseMask) + SystemPageSize() * 2);
    size_t index = ((slot_start & kSuperPageOffsetMask) >> SystemPageShift())
                   << GetPartitionRefCountIndexMultiplierShift();
#if BUILDFLAG(PA_DCHECK_IS_ON) || BUILDFLAG(ENABLE_BACKUP_REF_PTR_SLOW_CHECKS)
    PA_CHECK(sizeof(PartitionRefCount) * index <= SystemPageSize());
#endif
    return bitmap_base + index;
  }
}

#else  // BUILDFLAG(PUT_REF_COUNT_IN_PREVIOUS_SLOT)

// Allocate extra space for the reference count to satisfy the alignment
// requirement.
static constexpr size_t kInSlotRefCountBufferSize = kAlignment;
constexpr size_t kPartitionRefCountOffsetAdjustment = kInSlotRefCountBufferSize;

// This is for adjustment of pointers right past the allocation, which may point
// to the next slot. First subtract 1 to bring them to the intended slot, and
// only then we'll be able to find ref-count in that slot.
constexpr size_t kPartitionPastAllocationAdjustment = 1;

PA_ALWAYS_INLINE PartitionRefCount* PartitionRefCountPointer(
    uintptr_t slot_start) {
#if BUILDFLAG(PA_DCHECK_IS_ON) || BUILDFLAG(ENABLE_BACKUP_REF_PTR_SLOW_CHECKS)
  CheckThatSlotOffsetIsZero(slot_start);
#endif
  // Have to MTE-tag, because the address is untagged, but lies within a slot
  // area, which is protected by MTE.
  return static_cast<PartitionRefCount*>(TagAddr(slot_start));
}

#endif  // BUILDFLAG(PUT_REF_COUNT_IN_PREVIOUS_SLOT)

static_assert(sizeof(PartitionRefCount) <= kInSlotRefCountBufferSize,
              "PartitionRefCount should fit into the in-slot buffer.");

#else  // BUILDFLAG(ENABLE_BACKUP_REF_PTR_SUPPORT)

static constexpr size_t kInSlotRefCountBufferSize = 0;
constexpr size_t kPartitionRefCountOffsetAdjustment = 0;

#endif  // BUILDFLAG(ENABLE_BACKUP_REF_PTR_SUPPORT)

constexpr size_t kPartitionRefCountSizeAdjustment = kInSlotRefCountBufferSize;

}  // namespace partition_alloc::internal

#endif  // BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_REF_COUNT_H_
