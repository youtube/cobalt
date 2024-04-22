// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/allocator/partition_allocator/partition_bucket.h"

#include <algorithm>
#include <cstdint>
#include <tuple>

#include "base/allocator/partition_allocator/address_pool_manager.h"
#include "base/allocator/partition_allocator/freeslot_bitmap.h"
#include "base/allocator/partition_allocator/freeslot_bitmap_constants.h"
#include "base/allocator/partition_allocator/oom.h"
#include "base/allocator/partition_allocator/page_allocator.h"
#include "base/allocator/partition_allocator/page_allocator_constants.h"
#include "base/allocator/partition_allocator/partition_address_space.h"
#include "base/allocator/partition_allocator/partition_alloc.h"
#include "base/allocator/partition_allocator/partition_alloc_base/bits.h"
#include "base/allocator/partition_allocator/partition_alloc_base/compiler_specific.h"
#include "base/allocator/partition_allocator/partition_alloc_base/component_export.h"
#include "base/allocator/partition_allocator/partition_alloc_base/debug/alias.h"
#include "base/allocator/partition_allocator/partition_alloc_base/debug/debugging_buildflags.h"
#include "base/allocator/partition_allocator/partition_alloc_base/immediate_crash.h"
#include "base/allocator/partition_allocator/partition_alloc_base/thread_annotations.h"
#include "base/allocator/partition_allocator/partition_alloc_buildflags.h"
#include "base/allocator/partition_allocator/partition_alloc_check.h"
#include "base/allocator/partition_allocator/partition_alloc_config.h"
#include "base/allocator/partition_allocator/partition_alloc_constants.h"
#include "base/allocator/partition_allocator/partition_alloc_forward.h"
#include "base/allocator/partition_allocator/partition_direct_map_extent.h"
#include "base/allocator/partition_allocator/partition_oom.h"
#include "base/allocator/partition_allocator/partition_page.h"
#include "base/allocator/partition_allocator/reservation_offset_table.h"
#include "base/allocator/partition_allocator/tagging.h"
#include "build/build_config.h"

#if BUILDFLAG(USE_STARSCAN)
#include "base/allocator/partition_allocator/starscan/pcscan.h"
#endif

namespace partition_alloc::internal {

namespace {

#if PA_CONFIG(ENABLE_SHADOW_METADATA)
PA_ALWAYS_INLINE uintptr_t ShadowMetadataStart(uintptr_t super_page,
                                               pool_handle pool) {
  uintptr_t shadow_metadata_start =
      super_page + SystemPageSize() + ShadowPoolOffset(pool);
  PA_DCHECK(!PartitionAddressSpace::IsInRegularPool(shadow_metadata_start));
  PA_DCHECK(!PartitionAddressSpace::IsInBRPPool(shadow_metadata_start));
  return shadow_metadata_start;
}
#endif

template <bool thread_safe>
[[noreturn]] PA_NOINLINE void PartitionOutOfMemoryMappingFailure(
    PartitionRoot<thread_safe>* root,
    size_t size) PA_LOCKS_EXCLUDED(root->lock_) {
  PA_NO_CODE_FOLDING();
  root->OutOfMemory(size);
  PA_IMMEDIATE_CRASH();  // Not required, kept as documentation.
}

template <bool thread_safe>
[[noreturn]] PA_NOINLINE void PartitionOutOfMemoryCommitFailure(
    PartitionRoot<thread_safe>* root,
    size_t size) PA_LOCKS_EXCLUDED(root->lock_) {
  PA_NO_CODE_FOLDING();
  root->OutOfMemory(size);
  PA_IMMEDIATE_CRASH();  // Not required, kept as documentation.
}

#if !BUILDFLAG(HAS_64_BIT_POINTERS) && BUILDFLAG(ENABLE_BACKUP_REF_PTR_SUPPORT)
// |start| has to be aligned to kSuperPageSize, but |end| doesn't. This means
// that a partial super page is allowed at the end. Since the block list uses
// kSuperPageSize granularity, a partial super page is considered blocked if
// there is a raw_ptr<T> pointing anywhere in that super page, even if doesn't
// point to that partially allocated region.
bool AreAllowedSuperPagesForBRPPool(uintptr_t start, uintptr_t end) {
  PA_DCHECK(!(start % kSuperPageSize));
  for (uintptr_t super_page = start; super_page < end;
       super_page += kSuperPageSize) {
    // If any blocked super page is found inside the given memory region,
    // the memory region is blocked.
    if (!AddressPoolManagerBitmap::IsAllowedSuperPageForBRPPool(super_page)) {
      AddressPoolManagerBitmap::IncrementBlocklistHitCount();
      return false;
    }
  }
  return true;
}
#endif  // !BUILDFLAG(HAS_64_BIT_POINTERS) &&
        // BUILDFLAG(ENABLE_BACKUP_REF_PTR_SUPPORT)

// Reserves |requested_size| worth of super pages from the specified pool.
// If BRP pool is requested this function will honor BRP block list.
//
// The returned address will be aligned to kSuperPageSize, and so
// |requested_address| should be. |requested_size| doesn't have to be, however.
//
// |requested_address| is merely a hint, which will be attempted, but easily
// given up on if doesn't work the first time.
//
// The function doesn't need to hold root->lock_ or any other locks, because:
// - It (1) reserves memory, (2) then consults AreAllowedSuperPagesForBRPPool
//   for that memory, and (3) returns the memory if
//   allowed, or unreserves and decommits if not allowed. So no other
//   overlapping region can be allocated while executing
//   AreAllowedSuperPagesForBRPPool.
// - IsAllowedSuperPageForBRPPool (used by AreAllowedSuperPagesForBRPPool) is
//   designed to not need locking.
uintptr_t ReserveMemoryFromPool(pool_handle pool,
                                uintptr_t requested_address,
                                size_t requested_size) {
  PA_DCHECK(!(requested_address % kSuperPageSize));

  uintptr_t reserved_address = AddressPoolManager::GetInstance().Reserve(
      pool, requested_address, requested_size);

  // In 32-bit mode, when allocating from BRP pool, verify that the requested
  // allocation honors the block list. Find a better address otherwise.
#if !BUILDFLAG(HAS_64_BIT_POINTERS) && BUILDFLAG(ENABLE_BACKUP_REF_PTR_SUPPORT)
  if (pool == kBRPPoolHandle) {
    constexpr int kMaxRandomAddressTries = 10;
    for (int i = 0; i < kMaxRandomAddressTries; ++i) {
      if (!reserved_address ||
          AreAllowedSuperPagesForBRPPool(reserved_address,
                                         reserved_address + requested_size)) {
        break;
      }
      AddressPoolManager::GetInstance().UnreserveAndDecommit(
          pool, reserved_address, requested_size);
      // No longer try to honor |requested_address|, because it didn't work for
      // us last time.
      reserved_address =
          AddressPoolManager::GetInstance().Reserve(pool, 0, requested_size);
    }

    // If the allocation attempt succeeds, we will break out of the following
    // loop immediately.
    //
    // Last resort: sequentially scan the whole 32-bit address space. The number
    // of blocked super-pages should be very small, so we expect to practically
    // never need to run the following code. Note that it may fail to find an
    // available super page, e.g., when it becomes available after the scan
    // passes through it, but we accept the risk.
    for (uintptr_t address_to_try = kSuperPageSize; address_to_try != 0;
         address_to_try += kSuperPageSize) {
      if (!reserved_address ||
          AreAllowedSuperPagesForBRPPool(reserved_address,
                                         reserved_address + requested_size)) {
        break;
      }
      AddressPoolManager::GetInstance().UnreserveAndDecommit(
          pool, reserved_address, requested_size);
      // Reserve() can return a different pointer than attempted.
      reserved_address = AddressPoolManager::GetInstance().Reserve(
          pool, address_to_try, requested_size);
    }

    // If the loop ends naturally, the last allocated region hasn't been
    // verified. Do it now.
    if (reserved_address &&
        !AreAllowedSuperPagesForBRPPool(reserved_address,
                                        reserved_address + requested_size)) {
      AddressPoolManager::GetInstance().UnreserveAndDecommit(
          pool, reserved_address, requested_size);
      reserved_address = 0;
    }
  }
#endif  // !BUILDFLAG(HAS_64_BIT_POINTERS) &&
        // BUILDFLAG(ENABLE_BACKUP_REF_PTR_SUPPORT)

#if !BUILDFLAG(HAS_64_BIT_POINTERS)
  // Only mark the region as belonging to the pool after it has passed the
  // blocklist check in order to avoid a potential race with destructing a
  // raw_ptr<T> object that points to non-PA memory in another thread.
  // If `MarkUsed` was called earlier, the other thread could incorrectly
  // determine that the allocation had come form PartitionAlloc.
  if (reserved_address) {
    AddressPoolManager::GetInstance().MarkUsed(pool, reserved_address,
                                               requested_size);
  }
#endif

  PA_DCHECK(!(reserved_address % kSuperPageSize));
  return reserved_address;
}

template <bool thread_safe>
SlotSpanMetadata<thread_safe>* PartitionDirectMap(
    PartitionRoot<thread_safe>* root,
    unsigned int flags,
    size_t raw_size,
    size_t slot_span_alignment) {
  PA_DCHECK((slot_span_alignment >= PartitionPageSize()) &&
            base::bits::IsPowerOfTwo(slot_span_alignment));

  // No static EXCLUSIVE_LOCKS_REQUIRED(), as the checker doesn't understand
  // scoped unlocking.
  root->lock_.AssertAcquired();

  const bool return_null = flags & AllocFlags::kReturnNull;
  if (PA_UNLIKELY(raw_size > MaxDirectMapped())) {
    if (return_null) {
      return nullptr;
    }

    // The lock is here to protect PA from:
    // 1. Concurrent calls
    // 2. Reentrant calls
    //
    // This is fine here however, as:
    // 1. Concurrency: |PartitionRoot::OutOfMemory()| never returns, so the lock
    //    will not be re-acquired, which would lead to acting on inconsistent
    //    data that could have been modified in-between releasing and acquiring
    //    it.
    // 2. Reentrancy: This is why we release the lock. On some platforms,
    //    terminating the process may free() memory, or even possibly try to
    //    allocate some. Calling free() is fine, but will deadlock since
    //    |PartitionRoot::lock_| is not recursive.
    //
    // Supporting reentrant calls properly is hard, and not a requirement for
    // PA. However up to that point, we've only *read* data, not *written* to
    // any state. Reentrant calls are then fine, especially as we don't continue
    // on this path. The only downside is possibly endless recursion if the OOM
    // handler allocates and fails to use UncheckedMalloc() or equivalent, but
    // that's violating the contract of base::TerminateBecauseOutOfMemory().
    ScopedUnlockGuard unlock{root->lock_};
    PartitionExcessiveAllocationSize(raw_size);
  }

  PartitionDirectMapExtent<thread_safe>* map_extent = nullptr;
  PartitionPage<thread_safe>* page = nullptr;

  {
    // Getting memory for direct-mapped allocations doesn't interact with the
    // rest of the allocator, but takes a long time, as it involves several
    // system calls. Although no mmap() (or equivalent) calls are made on
    // 64 bit systems, page permissions are changed with mprotect(), which is
    // a syscall.
    //
    // These calls are almost always slow (at least a couple us per syscall on a
    // desktop Linux machine), and they also have a very long latency tail,
    // possibly from getting descheduled. As a consequence, we should not hold
    // the lock when performing a syscall. This is not the only problematic
    // location, but since this one doesn't interact with the rest of the
    // allocator, we can safely drop and then re-acquire the lock.
    //
    // Note that this only affects allocations that are not served out of the
    // thread cache, but as a simple example the buffer partition in blink is
    // frequently used for large allocations (e.g. ArrayBuffer), and frequent,
    // small ones (e.g. WTF::String), and does not have a thread cache.
    ScopedUnlockGuard scoped_unlock{root->lock_};

    const size_t slot_size =
        PartitionRoot<thread_safe>::GetDirectMapSlotSize(raw_size);
    // The super page starts with a partition page worth of metadata and guard
    // pages, hence alignment requests ==PartitionPageSize() will be
    // automatically satisfied. Padding is needed for higher-order alignment
    // requests. Note, |slot_span_alignment| is at least 1 partition page.
    const size_t padding_for_alignment =
        slot_span_alignment - PartitionPageSize();
    const size_t reservation_size =
        PartitionRoot<thread_safe>::GetDirectMapReservationSize(
            raw_size + padding_for_alignment);
#if BUILDFLAG(PA_DCHECK_IS_ON)
    const size_t available_reservation_size =
        reservation_size - padding_for_alignment -
        PartitionRoot<thread_safe>::GetDirectMapMetadataAndGuardPagesSize();
    PA_DCHECK(slot_size <= available_reservation_size);
#endif

    pool_handle pool = root->ChoosePool();
    uintptr_t reservation_start;
    {
      // Reserving memory from the pool is actually not a syscall on 64 bit
      // platforms.
#if !BUILDFLAG(HAS_64_BIT_POINTERS)
      ScopedSyscallTimer timer{root};
#endif
      reservation_start = ReserveMemoryFromPool(pool, 0, reservation_size);
    }
    if (PA_UNLIKELY(!reservation_start)) {
      if (return_null) {
        return nullptr;
      }

      PartitionOutOfMemoryMappingFailure(root, reservation_size);
    }

    root->total_size_of_direct_mapped_pages.fetch_add(
        reservation_size, std::memory_order_relaxed);

    // Shift by 1 partition page (metadata + guard pages) and alignment padding.
    const uintptr_t slot_start =
        reservation_start + PartitionPageSize() + padding_for_alignment;

    {
      ScopedSyscallTimer timer{root};
      RecommitSystemPages(reservation_start + SystemPageSize(),
                          SystemPageSize(),
#if PA_CONFIG(ENABLE_SHADOW_METADATA)
                          root->PageAccessibilityWithPkeyIfEnabled(
                              PageAccessibilityConfiguration::kRead),
#else
                          root->PageAccessibilityWithPkeyIfEnabled(
                              PageAccessibilityConfiguration::kReadWrite),
#endif
                          PageAccessibilityDisposition::kRequireUpdate);
    }

#if BUILDFLAG(PUT_REF_COUNT_IN_PREVIOUS_SLOT)
    // If PUT_REF_COUNT_IN_PREVIOUS_SLOT is on, and if the BRP pool is
    // used, allocate a SystemPage for RefCount "bitmap" (only one of its
    // elements will be used).
    if (pool == kBRPPoolHandle) {
      ScopedSyscallTimer timer{root};
      RecommitSystemPages(reservation_start + SystemPageSize() * 2,
                          SystemPageSize(),
                          root->PageAccessibilityWithPkeyIfEnabled(
                              PageAccessibilityConfiguration::kReadWrite),
                          PageAccessibilityDisposition::kRequireUpdate);
    }
#endif

#if PA_CONFIG(ENABLE_SHADOW_METADATA)
    {
      ScopedSyscallTimer timer{root};
      RecommitSystemPages(ShadowMetadataStart(reservation_start, pool),
                          SystemPageSize(),
                          root->PageAccessibilityWithPkeyIfEnabled(
                              PageAccessibilityConfiguration::kReadWrite),
                          PageAccessibilityDisposition::kRequireUpdate);
    }
#endif

    // No need to hold root->lock_. Now that memory is reserved, no other
    // overlapping region can be allocated (because of how pools work),
    // so no other thread can update the same offset table entries at the
    // same time. Furthermore, nobody will be ready these offsets until this
    // function returns.
    uintptr_t address_start = reservation_start;
    uintptr_t address_end = address_start + reservation_size;
    auto* offset_ptr = ReservationOffsetPointer(address_start);
    uint16_t offset = 0;
    while (address_start < address_end) {
      PA_DCHECK(offset_ptr < GetReservationOffsetTableEnd(address_start));
      PA_DCHECK(offset < kOffsetTagNormalBuckets);
      *offset_ptr++ = offset++;
      address_start += kSuperPageSize;
    }

    auto* super_page_extent =
        PartitionSuperPageToExtent<thread_safe>(reservation_start);
    super_page_extent->root = root;
    // The new structures are all located inside a fresh system page so they
    // will all be zeroed out. These DCHECKs are for documentation and to assert
    // our expectations of the kernel.
    PA_DCHECK(!super_page_extent->number_of_consecutive_super_pages);
    PA_DCHECK(!super_page_extent->next);

    PartitionPage<thread_safe>* first_page =
        reinterpret_cast<PartitionPage<thread_safe>*>(super_page_extent) + 1;
    page = PartitionPage<thread_safe>::FromAddr(slot_start);
    // |first_page| and |page| may be equal, if there is no alignment padding.
    if (page != first_page) {
      PA_DCHECK(page > first_page);
      PA_DCHECK(page - first_page <=
                PartitionPage<thread_safe>::kMaxSlotSpanMetadataOffset);
      PA_CHECK(!first_page->is_valid);
      first_page->has_valid_span_after_this = true;
      first_page->slot_span_metadata_offset = page - first_page;
    }
    auto* metadata =
        reinterpret_cast<PartitionDirectMapMetadata<thread_safe>*>(page);
    // Since direct map metadata is larger than PartitionPage, make sure the
    // first and the last bytes are on the same system page, i.e. within the
    // super page metadata region.
    PA_DCHECK(base::bits::AlignDown(reinterpret_cast<uintptr_t>(metadata),
                                    SystemPageSize()) ==
              base::bits::AlignDown(
                  reinterpret_cast<uintptr_t>(metadata) +
                      sizeof(PartitionDirectMapMetadata<thread_safe>) - 1,
                  SystemPageSize()));
    PA_DCHECK(page == &metadata->page);
    page->is_valid = true;
    PA_DCHECK(!page->has_valid_span_after_this);
    PA_DCHECK(!page->slot_span_metadata_offset);
    PA_DCHECK(!page->slot_span_metadata.next_slot_span);
    PA_DCHECK(!page->slot_span_metadata.marked_full);
    PA_DCHECK(!page->slot_span_metadata.num_allocated_slots);
    PA_DCHECK(!page->slot_span_metadata.num_unprovisioned_slots);
    PA_DCHECK(!page->slot_span_metadata.in_empty_cache());

    PA_DCHECK(!metadata->subsequent_page.subsequent_page_metadata.raw_size);
    // Raw size is set later, by the caller.
    metadata->subsequent_page.slot_span_metadata_offset = 1;

    PA_DCHECK(!metadata->bucket.active_slot_spans_head);
    PA_DCHECK(!metadata->bucket.empty_slot_spans_head);
    PA_DCHECK(!metadata->bucket.decommitted_slot_spans_head);
    PA_DCHECK(!metadata->bucket.num_system_pages_per_slot_span);
    PA_DCHECK(!metadata->bucket.num_full_slot_spans);
    metadata->bucket.slot_size = slot_size;

    new (&page->slot_span_metadata)
        SlotSpanMetadata<thread_safe>(&metadata->bucket);

    // It is typically possible to map a large range of inaccessible pages, and
    // this is leveraged in multiple places, including the pools. However,
    // this doesn't mean that we can commit all this memory.  For the vast
    // majority of allocations, this just means that we crash in a slightly
    // different place, but for callers ready to handle failures, we have to
    // return nullptr. See crbug.com/1187404.
    //
    // Note that we didn't check above, because if we cannot even commit a
    // single page, then this is likely hopeless anyway, and we will crash very
    // soon.
    const bool ok = root->TryRecommitSystemPagesForData(
        slot_start, slot_size, PageAccessibilityDisposition::kRequireUpdate);
    if (!ok) {
      if (!return_null) {
        PartitionOutOfMemoryCommitFailure(root, slot_size);
      }

      {
        ScopedSyscallTimer timer{root};
#if !BUILDFLAG(HAS_64_BIT_POINTERS)
        AddressPoolManager::GetInstance().MarkUnused(pool, reservation_start,
                                                     reservation_size);
#endif
        AddressPoolManager::GetInstance().UnreserveAndDecommit(
            pool, reservation_start, reservation_size);
      }

      root->total_size_of_direct_mapped_pages.fetch_sub(
          reservation_size, std::memory_order_relaxed);

      return nullptr;
    }

    auto* next_entry = PartitionFreelistEntry::EmplaceAndInitNull(slot_start);
    page->slot_span_metadata.SetFreelistHead(next_entry);

    map_extent = &metadata->direct_map_extent;
    map_extent->reservation_size = reservation_size;
    map_extent->padding_for_alignment = padding_for_alignment;
    map_extent->bucket = &metadata->bucket;
  }

  root->lock_.AssertAcquired();

  // Maintain the doubly-linked list of all direct mappings.
  map_extent->next_extent = root->direct_map_list;
  if (map_extent->next_extent) {
    map_extent->next_extent->prev_extent = map_extent;
  }
  map_extent->prev_extent = nullptr;
  root->direct_map_list = map_extent;

  return &page->slot_span_metadata;
}

uint8_t ComputeSystemPagesPerSlotSpanPreferSmall(size_t slot_size) {
  if (slot_size > MaxRegularSlotSpanSize()) {
    // This is technically not needed, as for now all the larger slot sizes are
    // multiples of the system page size.
    return base::bits::AlignUp(slot_size, SystemPageSize()) / SystemPageSize();
  }

  // Smaller slot spans waste less address space, as well as potentially lower
  // fragmentation:
  // - Address space: This comes from fuller SuperPages (since the tail end of a
  //   SuperPage is more likely to be used when the slot span is smaller. Also,
  //   if a slot span is partially used, a smaller slot span will use less
  //   address space.
  // - In-slot fragmentation: Slot span management code will prioritize
  //   almost-full slot spans, as well as trying to keep empty slot spans
  //   empty. The more granular this logic can work, the better.
  //
  // Since metadata space overhead is constant per-PartitionPage, keeping
  // smaller slot spans makes sense.
  //
  // Underlying memory allocation is done per-PartitionPage, but memory commit
  // is done per system page. This means that we prefer to fill the entirety of
  // a PartitionPage with a slot span, but we can tolerate some system pages
  // being empty at the end, as these will not cost committed or dirty memory.
  //
  // The choice below is, for multi-slot slot spans:
  // - If a full PartitionPage slot span is possible with less than 2% of a
  //   *single* system page wasted, use it. The smallest possible size wins.
  // - Otherwise, select the size with the smallest virtual address space
  //   loss. Allow a SlotSpan to leave some slack in its PartitionPage, up to
  //   1/4 of the total.
  for (size_t partition_page_count = 1;
       partition_page_count <= kMaxPartitionPagesPerRegularSlotSpan;
       partition_page_count++) {
    size_t candidate_size = partition_page_count * PartitionPageSize();
    size_t waste = candidate_size % slot_size;
    if (waste <= .02 * SystemPageSize()) {
      return partition_page_count * NumSystemPagesPerPartitionPage();
    }
  }

  size_t best_count = 0;
  size_t best_waste = std::numeric_limits<size_t>::max();
  for (size_t partition_page_count = 1;
       partition_page_count <= kMaxPartitionPagesPerRegularSlotSpan;
       partition_page_count++) {
    // Prefer no slack.
    for (size_t slack = 0; slack < partition_page_count; slack++) {
      size_t system_page_count =
          partition_page_count * NumSystemPagesPerPartitionPage() - slack;
      size_t candidate_size = system_page_count * SystemPageSize();
      size_t waste = candidate_size % slot_size;
      if (waste < best_waste) {
        best_waste = waste;
        best_count = system_page_count;
      }
    }
  }
  return best_count;
}

uint8_t ComputeSystemPagesPerSlotSpanInternal(size_t slot_size) {
  // This works out reasonably for the current bucket sizes of the generic
  // allocator, and the current values of partition page size and constants.
  // Specifically, we have enough room to always pack the slots perfectly into
  // some number of system pages. The only waste is the waste associated with
  // unfaulted pages (i.e. wasted address space).
  // TODO: we end up using a lot of system pages for very small sizes. For
  // example, we'll use 12 system pages for slot size 24. The slot size is so
  // small that the waste would be tiny with just 4, or 1, system pages.  Later,
  // we can investigate whether there are anti-fragmentation benefits to using
  // fewer system pages.
  double best_waste_ratio = 1.0f;
  uint16_t best_pages = 0;
  if (slot_size > MaxRegularSlotSpanSize()) {
    // TODO(ajwong): Why is there a DCHECK here for this?
    // http://crbug.com/776537
    PA_DCHECK(!(slot_size % SystemPageSize()));
    best_pages = static_cast<uint16_t>(slot_size >> SystemPageShift());
    PA_CHECK(best_pages <= std::numeric_limits<uint8_t>::max());
    return static_cast<uint8_t>(best_pages);
  }
  PA_DCHECK(slot_size <= MaxRegularSlotSpanSize());
  for (uint16_t i = NumSystemPagesPerPartitionPage() - 1;
       i <= MaxSystemPagesPerRegularSlotSpan(); ++i) {
    size_t page_size = i << SystemPageShift();
    size_t num_slots = page_size / slot_size;
    size_t waste = page_size - (num_slots * slot_size);
    // Leaving a page unfaulted is not free; the page will occupy an empty page
    // table entry.  Make a simple attempt to account for that.
    //
    // TODO(ajwong): This looks wrong. PTEs are allocated for all pages
    // regardless of whether or not they are wasted. Should it just
    // be waste += i * sizeof(void*)?
    // http://crbug.com/776537
    size_t num_remainder_pages = i & (NumSystemPagesPerPartitionPage() - 1);
    size_t num_unfaulted_pages =
        num_remainder_pages
            ? (NumSystemPagesPerPartitionPage() - num_remainder_pages)
            : 0;
    waste += sizeof(void*) * num_unfaulted_pages;
    double waste_ratio =
        static_cast<double>(waste) / static_cast<double>(page_size);
    if (waste_ratio < best_waste_ratio) {
      best_waste_ratio = waste_ratio;
      best_pages = i;
    }
  }
  PA_DCHECK(best_pages > 0);
  PA_CHECK(best_pages <= MaxSystemPagesPerRegularSlotSpan());
  return static_cast<uint8_t>(best_pages);
}

}  // namespace

uint8_t ComputeSystemPagesPerSlotSpan(size_t slot_size,
                                      bool prefer_smaller_slot_spans) {
  if (prefer_smaller_slot_spans) {
    size_t system_page_count =
        ComputeSystemPagesPerSlotSpanPreferSmall(slot_size);
    size_t waste = (system_page_count * SystemPageSize()) % slot_size;
    // In case the waste is too large (more than 5% of a page), don't try to use
    // the "small" slot span formula. This happens when we have a lot of
    // buckets, in some cases the formula doesn't find a nice, small size.
    if (waste <= .05 * SystemPageSize()) {
      return system_page_count;
    }
  }

  return ComputeSystemPagesPerSlotSpanInternal(slot_size);
}

template <bool thread_safe>
void PartitionBucket<thread_safe>::Init(uint32_t new_slot_size) {
  slot_size = new_slot_size;
  slot_size_reciprocal = kReciprocalMask / new_slot_size + 1;
  active_slot_spans_head =
      SlotSpanMetadata<thread_safe>::get_sentinel_slot_span_non_const();
  empty_slot_spans_head = nullptr;
  decommitted_slot_spans_head = nullptr;
  num_full_slot_spans = 0;
  bool prefer_smaller_slot_spans =
#if PA_CONFIG(PREFER_SMALLER_SLOT_SPANS)
      true
#else
      false
#endif
      ;
  num_system_pages_per_slot_span =
      ComputeSystemPagesPerSlotSpan(slot_size, prefer_smaller_slot_spans);
}

template <bool thread_safe>
PA_ALWAYS_INLINE SlotSpanMetadata<thread_safe>*
PartitionBucket<thread_safe>::AllocNewSlotSpan(PartitionRoot<thread_safe>* root,
                                               unsigned int flags,
                                               size_t slot_span_alignment) {
  PA_DCHECK(!(root->next_partition_page % PartitionPageSize()));
  PA_DCHECK(!(root->next_partition_page_end % PartitionPageSize()));

  size_t num_partition_pages = get_pages_per_slot_span();
  size_t slot_span_reservation_size = num_partition_pages
                                      << PartitionPageShift();
  size_t slot_span_committed_size = get_bytes_per_span();
  PA_DCHECK(num_partition_pages <= NumPartitionPagesPerSuperPage());
  PA_DCHECK(slot_span_committed_size % SystemPageSize() == 0);
  PA_DCHECK(slot_span_committed_size <= slot_span_reservation_size);

  uintptr_t adjusted_next_partition_page =
      base::bits::AlignUp(root->next_partition_page, slot_span_alignment);
  if (PA_UNLIKELY(adjusted_next_partition_page + slot_span_reservation_size >
                  root->next_partition_page_end)) {
    // AllocNewSuperPage() may crash (e.g. address space exhaustion), put data
    // on stack.
    PA_DEBUG_DATA_ON_STACK("slotsize", slot_size);
    PA_DEBUG_DATA_ON_STACK("spansize", slot_span_reservation_size);

    // In this case, we can no longer hand out pages from the current super page
    // allocation. Get a new super page.
    if (!AllocNewSuperPage(root, flags)) {
      return nullptr;
    }
    // AllocNewSuperPage() updates root->next_partition_page, re-query.
    adjusted_next_partition_page =
        base::bits::AlignUp(root->next_partition_page, slot_span_alignment);
    PA_CHECK(adjusted_next_partition_page + slot_span_reservation_size <=
             root->next_partition_page_end);
  }

  auto* gap_start_page =
      PartitionPage<thread_safe>::FromAddr(root->next_partition_page);
  auto* gap_end_page =
      PartitionPage<thread_safe>::FromAddr(adjusted_next_partition_page);
  for (auto* page = gap_start_page; page < gap_end_page; ++page) {
    PA_DCHECK(!page->is_valid);
    page->has_valid_span_after_this = 1;
  }
  root->next_partition_page =
      adjusted_next_partition_page + slot_span_reservation_size;

  uintptr_t slot_span_start = adjusted_next_partition_page;
  auto* slot_span = &gap_end_page->slot_span_metadata;
  InitializeSlotSpan(slot_span);
  // Now that slot span is initialized, it's safe to call FromSlotStart.
  PA_DCHECK(slot_span ==
            SlotSpanMetadata<thread_safe>::FromSlotStart(slot_span_start));

  // System pages in the super page come in a decommited state. Commit them
  // before vending them back.
  // If lazy commit is enabled, pages will be committed when provisioning slots,
  // in ProvisionMoreSlotsAndAllocOne(), not here.
  if (!kUseLazyCommit) {
    PA_DEBUG_DATA_ON_STACK("slotsize", slot_size);
    PA_DEBUG_DATA_ON_STACK("spansize", slot_span_reservation_size);
    PA_DEBUG_DATA_ON_STACK("spancmt", slot_span_committed_size);

    root->RecommitSystemPagesForData(
        slot_span_start, slot_span_committed_size,
        PageAccessibilityDisposition::kRequireUpdate);
  }

  PA_CHECK(get_slots_per_span() <=
           SlotSpanMetadata<ThreadSafe>::kMaxSlotsPerSlotSpan);

  // Double check that we had enough space in the super page for the new slot
  // span.
  PA_DCHECK(root->next_partition_page <= root->next_partition_page_end);

  return slot_span;
}

template <bool thread_safe>
uintptr_t PartitionBucket<thread_safe>::AllocNewSuperPageSpan(
    PartitionRoot<thread_safe>* root,
    size_t super_page_count,
    unsigned int flags) {
  PA_CHECK(super_page_count > 0);
  PA_CHECK(super_page_count <=
           std::numeric_limits<size_t>::max() / kSuperPageSize);
  // Need a new super page. We want to allocate super pages in a contiguous
  // address region as much as possible. This is important for not causing
  // page table bloat and not fragmenting address spaces in 32 bit
  // architectures.
  uintptr_t requested_address = root->next_super_page;
  pool_handle pool = root->ChoosePool();
  uintptr_t super_page_span_start = ReserveMemoryFromPool(
      pool, requested_address, super_page_count * kSuperPageSize);
  if (PA_UNLIKELY(!super_page_span_start)) {
    if (flags & AllocFlags::kReturnNull) {
      return 0;
    }

    // Didn't manage to get a new uncommitted super page -> address space issue.
    ::partition_alloc::internal::ScopedUnlockGuard unlock{root->lock_};
    PartitionOutOfMemoryMappingFailure(root, kSuperPageSize);
  }

  uintptr_t super_page_span_end =
      super_page_span_start + super_page_count * kSuperPageSize;
  for (uintptr_t super_page = super_page_span_start;
       super_page < super_page_span_end; super_page += kSuperPageSize) {
    InitializeSuperPage(root, super_page, 0);
  }
  return super_page_span_start;
}

template <bool thread_safe>
PA_ALWAYS_INLINE uintptr_t PartitionBucket<thread_safe>::AllocNewSuperPage(
    PartitionRoot<thread_safe>* root,
    unsigned int flags) {
  auto super_page = AllocNewSuperPageSpan(root, 1, flags);
  if (PA_UNLIKELY(!super_page)) {
    // If the `kReturnNull` flag isn't set and the allocation attempt fails,
    // `AllocNewSuperPageSpan` should've failed with an OOM crash.
    PA_DCHECK(flags & AllocFlags::kReturnNull);
    return 0;
  }
  return SuperPagePayloadBegin(super_page, root->IsQuarantineAllowed());
}

template <bool thread_safe>
PA_ALWAYS_INLINE uintptr_t PartitionBucket<thread_safe>::InitializeSuperPage(
    PartitionRoot<thread_safe>* root,
    uintptr_t super_page,
    uintptr_t requested_address) {
  *ReservationOffsetPointer(super_page) = kOffsetTagNormalBuckets;

  root->total_size_of_super_pages.fetch_add(kSuperPageSize,
                                            std::memory_order_relaxed);

  root->next_super_page = super_page + kSuperPageSize;
  uintptr_t state_bitmap =
      super_page + PartitionPageSize() +
      (is_direct_mapped() ? 0 : ReservedFreeSlotBitmapSize());
#if BUILDFLAG(USE_STARSCAN)
  PA_DCHECK(SuperPageStateBitmapAddr(super_page) == state_bitmap);
  const size_t state_bitmap_reservation_size =
      root->IsQuarantineAllowed() ? ReservedStateBitmapSize() : 0;
  const size_t state_bitmap_size_to_commit =
      root->IsQuarantineAllowed() ? CommittedStateBitmapSize() : 0;
  PA_DCHECK(state_bitmap_reservation_size % PartitionPageSize() == 0);
  PA_DCHECK(state_bitmap_size_to_commit % SystemPageSize() == 0);
  PA_DCHECK(state_bitmap_size_to_commit <= state_bitmap_reservation_size);
  uintptr_t payload = state_bitmap + state_bitmap_reservation_size;
#else
  uintptr_t payload = state_bitmap;
#endif  // BUILDFLAG(USE_STARSCAN)

  root->next_partition_page = payload;
  root->next_partition_page_end = root->next_super_page - PartitionPageSize();
  PA_DCHECK(payload ==
            SuperPagePayloadBegin(super_page, root->IsQuarantineAllowed()));
  PA_DCHECK(root->next_partition_page_end == SuperPagePayloadEnd(super_page));

  // Keep the first partition page in the super page inaccessible to serve as a
  // guard page, except an "island" in the middle where we put page metadata and
  // also a tiny amount of extent metadata.
  {
    ScopedSyscallTimer timer{root};
    RecommitSystemPages(super_page + SystemPageSize(), SystemPageSize(),
#if PA_CONFIG(ENABLE_SHADOW_METADATA)
                        root->PageAccessibilityWithPkeyIfEnabled(
                            PageAccessibilityConfiguration::kRead),
#else
                        root->PageAccessibilityWithPkeyIfEnabled(
                            PageAccessibilityConfiguration::kReadWrite),
#endif
                        PageAccessibilityDisposition::kRequireUpdate);
  }

#if BUILDFLAG(PUT_REF_COUNT_IN_PREVIOUS_SLOT)
  // If PUT_REF_COUNT_IN_PREVIOUS_SLOT is on, and if the BRP pool is
  // used, allocate a SystemPage for RefCount bitmap.
  if (root->ChoosePool() == kBRPPoolHandle) {
    ScopedSyscallTimer timer{root};
    RecommitSystemPages(super_page + SystemPageSize() * 2, SystemPageSize(),
                        root->PageAccessibilityWithPkeyIfEnabled(
                            PageAccessibilityConfiguration::kReadWrite),
                        PageAccessibilityDisposition::kRequireUpdate);
  }
#endif

#if PA_CONFIG(ENABLE_SHADOW_METADATA)
  {
    ScopedSyscallTimer timer{root};
    RecommitSystemPages(ShadowMetadataStart(super_page, root->ChoosePool()),
                        SystemPageSize(),
                        root->PageAccessibilityWithPkeyIfEnabled(
                            PageAccessibilityConfiguration::kReadWrite),
                        PageAccessibilityDisposition::kRequireUpdate);
  }
#endif

  // If we were after a specific address, but didn't get it, assume that
  // the system chose a lousy address. Here most OS'es have a default
  // algorithm that isn't randomized. For example, most Linux
  // distributions will allocate the mapping directly before the last
  // successful mapping, which is far from random. So we just get fresh
  // randomness for the next mapping attempt.
  if (requested_address && requested_address != super_page) {
    root->next_super_page = 0;
  }

  // We allocated a new super page so update super page metadata.
  // First check if this is a new extent or not.
  auto* latest_extent = PartitionSuperPageToExtent<thread_safe>(super_page);
  // By storing the root in every extent metadata object, we have a fast way
  // to go from a pointer within the partition to the root object.
  latest_extent->root = root;
  // Most new extents will be part of a larger extent, and these two fields
  // are unused, but we initialize them to 0 so that we get a clear signal
  // in case they are accidentally used.
  latest_extent->number_of_consecutive_super_pages = 0;
  latest_extent->next = nullptr;
  latest_extent->number_of_nonempty_slot_spans = 0;

  PartitionSuperPageExtentEntry<thread_safe>* current_extent =
      root->current_extent;
  const bool is_new_extent = super_page != requested_address;
  if (PA_UNLIKELY(is_new_extent)) {
    if (PA_UNLIKELY(!current_extent)) {
      PA_DCHECK(!root->first_extent);
      root->first_extent = latest_extent;
    } else {
      PA_DCHECK(current_extent->number_of_consecutive_super_pages);
      current_extent->next = latest_extent;
    }
    root->current_extent = latest_extent;
    latest_extent->number_of_consecutive_super_pages = 1;
  } else {
    // We allocated next to an existing extent so just nudge the size up a
    // little.
    PA_DCHECK(current_extent->number_of_consecutive_super_pages);
    ++current_extent->number_of_consecutive_super_pages;
    PA_DCHECK(payload > SuperPagesBeginFromExtent(current_extent) &&
              payload < SuperPagesEndFromExtent(current_extent));
  }

  // If PCScan is used, commit the state bitmap. Otherwise, leave it uncommitted
  // and let PartitionRoot::RegisterScannableRoot() commit it when needed. Make
  // sure to register the super-page after it has been fully initialized.
  // Otherwise, the concurrent scanner may try to access |extent->root| which
  // could be not initialized yet.
#if BUILDFLAG(USE_STARSCAN)
  if (root->IsQuarantineEnabled()) {
    {
      ScopedSyscallTimer timer{root};
      RecommitSystemPages(state_bitmap, state_bitmap_size_to_commit,
                          root->PageAccessibilityWithPkeyIfEnabled(
                              PageAccessibilityConfiguration::kReadWrite),
                          PageAccessibilityDisposition::kRequireUpdate);
    }
    PCScan::RegisterNewSuperPage(root, super_page);
  }
#endif  // BUILDFLAG(USE_STARSCAN)

#if BUILDFLAG(USE_FREESLOT_BITMAP)
  // Commit the pages for freeslot bitmap.
  if (!is_direct_mapped()) {
    uintptr_t freeslot_bitmap_addr = super_page + PartitionPageSize();
    PA_DCHECK(SuperPageFreeSlotBitmapAddr(super_page) == freeslot_bitmap_addr);
    ScopedSyscallTimer timer{root};
    RecommitSystemPages(freeslot_bitmap_addr, CommittedFreeSlotBitmapSize(),
                        root->PageAccessibilityWithPkeyIfEnabled(
                            PageAccessibilityConfiguration::kReadWrite),
                        PageAccessibilityDisposition::kRequireUpdate);
  }
#endif

  return payload;
}

template <bool thread_safe>
PA_ALWAYS_INLINE void PartitionBucket<thread_safe>::InitializeSlotSpan(
    SlotSpanMetadata<thread_safe>* slot_span) {
  new (slot_span) SlotSpanMetadata<thread_safe>(this);

  slot_span->Reset();

  uint16_t num_partition_pages = get_pages_per_slot_span();
  auto* page = reinterpret_cast<PartitionPage<thread_safe>*>(slot_span);
  for (uint16_t i = 0; i < num_partition_pages; ++i, ++page) {
    PA_DCHECK(i <= PartitionPage<thread_safe>::kMaxSlotSpanMetadataOffset);
    page->slot_span_metadata_offset = i;
    page->is_valid = true;
  }
}

template <bool thread_safe>
PA_ALWAYS_INLINE uintptr_t
PartitionBucket<thread_safe>::ProvisionMoreSlotsAndAllocOne(
    PartitionRoot<thread_safe>* root,
    SlotSpanMetadata<thread_safe>* slot_span) {
  PA_DCHECK(slot_span !=
            SlotSpanMetadata<thread_safe>::get_sentinel_slot_span());
  size_t num_slots = slot_span->num_unprovisioned_slots;
  PA_DCHECK(num_slots);
  PA_DCHECK(num_slots <= get_slots_per_span());
  // We should only get here when _every_ slot is either used or unprovisioned.
  // (The third possible state is "on the freelist". If we have a non-empty
  // freelist, we should not get here.)
  PA_DCHECK(num_slots + slot_span->num_allocated_slots == get_slots_per_span());
  // Similarly, make explicitly sure that the freelist is empty.
  PA_DCHECK(!slot_span->get_freelist_head());
  PA_DCHECK(!slot_span->is_full());

  uintptr_t slot_span_start =
      SlotSpanMetadata<thread_safe>::ToSlotSpanStart(slot_span);
  // If we got here, the first unallocated slot is either partially or fully on
  // an uncommitted page. If the latter, it must be at the start of that page.
  uintptr_t return_slot =
      slot_span_start + (slot_size * slot_span->num_allocated_slots);
  uintptr_t next_slot = return_slot + slot_size;
  uintptr_t commit_start = base::bits::AlignUp(return_slot, SystemPageSize());
  PA_DCHECK(next_slot > commit_start);
  uintptr_t commit_end = base::bits::AlignUp(next_slot, SystemPageSize());
  // If the slot was partially committed, |return_slot| and |next_slot| fall
  // in different pages. If the slot was fully uncommitted, |return_slot| points
  // to the page start and |next_slot| doesn't, thus only the latter gets
  // rounded up.
  PA_DCHECK(commit_end > commit_start);

  // The slot being returned is considered allocated.
  slot_span->num_allocated_slots++;
  // Round down, because a slot that doesn't fully fit in the new page(s) isn't
  // provisioned.
  size_t slots_to_provision = (commit_end - return_slot) / slot_size;
  slot_span->num_unprovisioned_slots -= slots_to_provision;
  PA_DCHECK(slot_span->num_allocated_slots +
                slot_span->num_unprovisioned_slots <=
            get_slots_per_span());

  // If lazy commit is enabled, meaning system pages in the slot span come
  // in an initially decommitted state, commit them here.
  // Note, we can't use PageAccessibilityDisposition::kAllowKeepForPerf, because
  // we have no knowledge which pages have been committed before (it doesn't
  // matter on Windows anyway).
  if (kUseLazyCommit) {
    // TODO(lizeb): Handle commit failure.
    root->RecommitSystemPagesForData(
        commit_start, commit_end - commit_start,
        PageAccessibilityDisposition::kRequireUpdate);
  }

  if (PA_LIKELY(slot_size <= kMaxMemoryTaggingSize &&
                root->IsMemoryTaggingEnabled())) {
    // Ensure the MTE-tag of the memory pointed by |return_slot| is unguessable.
    TagMemoryRangeRandomly(return_slot, slot_size);
  }

  // Add all slots that fit within so far committed pages to the free list.
  PartitionFreelistEntry* prev_entry = nullptr;
  uintptr_t next_slot_end = next_slot + slot_size;
  size_t free_list_entries_added = 0;
  while (next_slot_end <= commit_end) {
    void* next_slot_ptr;
    if (PA_LIKELY(slot_size <= kMaxMemoryTaggingSize)) {
      // Ensure the MTE-tag of the memory pointed by other provisioned slot is
      // unguessable. They will be returned to the app as is, and the MTE-tag
      // will only change upon calling Free().
      next_slot_ptr = TagMemoryRangeRandomly(next_slot, slot_size);
    } else {
      // No MTE-tagging for larger slots, just cast.
      next_slot_ptr = reinterpret_cast<void*>(next_slot);
    }
    auto* entry = PartitionFreelistEntry::EmplaceAndInitNull(next_slot_ptr);
    if (!slot_span->get_freelist_head()) {
      PA_DCHECK(!prev_entry);
      PA_DCHECK(!free_list_entries_added);
      slot_span->SetFreelistHead(entry);
    } else {
      PA_DCHECK(free_list_entries_added);
      prev_entry->SetNext(entry);
    }
#if BUILDFLAG(USE_FREESLOT_BITMAP)
    FreeSlotBitmapMarkSlotAsFree(next_slot);
#endif
    next_slot = next_slot_end;
    next_slot_end = next_slot + slot_size;
    prev_entry = entry;
#if BUILDFLAG(PA_DCHECK_IS_ON)
    free_list_entries_added++;
#endif
  }

#if BUILDFLAG(USE_FREESLOT_BITMAP)
  FreeSlotBitmapMarkSlotAsFree(return_slot);
#endif

#if BUILDFLAG(PA_DCHECK_IS_ON)
  // The only provisioned slot not added to the free list is the one being
  // returned.
  PA_DCHECK(slots_to_provision == free_list_entries_added + 1);
  // We didn't necessarily provision more than one slot (e.g. if |slot_size|
  // is large), meaning that |slot_span->freelist_head| can be nullptr.
  if (slot_span->get_freelist_head()) {
    PA_DCHECK(free_list_entries_added);
    slot_span->get_freelist_head()->CheckFreeList(slot_size);
  }
#endif

  // We had no free slots, and created some (potentially 0) in sorted order.
  slot_span->set_freelist_sorted();

  return return_slot;
}

template <bool thread_safe>
bool PartitionBucket<thread_safe>::SetNewActiveSlotSpan() {
  SlotSpanMetadata<thread_safe>* slot_span = active_slot_spans_head;
  if (slot_span == SlotSpanMetadata<thread_safe>::get_sentinel_slot_span()) {
    return false;
  }

  SlotSpanMetadata<thread_safe>* next_slot_span;

  // The goal here is to find a suitable slot span in the active list. Suitable
  // slot spans are |is_active()|, i.e. they either have (a) freelist entries,
  // or (b) unprovisioned free space. The first case is preferable, since it
  // doesn't cost a system call, and doesn't cause new memory to become dirty.
  //
  // While looking for a new slot span, active list maintenance is performed,
  // that is:
  // - Empty and decommitted slot spans are moved to their respective lists.
  // - Full slot spans are removed from the active list but are not moved
  //   anywhere. They could be tracked in a separate list, but this would
  //   increase cost non trivially. Indeed, a full slot span is likely to become
  //   non-full at some point (due to a free() hitting it). Since we only have
  //   space in the metadata for a single linked list pointer, removing the
  //   newly-non-full slot span from the "full" list would require walking it
  //   (to know what's before it in the full list).
  //
  // Since we prefer slot spans with provisioned freelist entries, maintenance
  // happens in two stages:
  // 1. Walk the list to find candidates. Each of the skipped slot span is moved
  //    to either:
  //   - one of the long-lived lists: empty, decommitted
  //   - the temporary "active slots spans with no freelist entry" list
  //   - Nowhere for full slot spans.
  // 2. Once we have a candidate:
  //   - Set it as the new active list head
  //   - Reattach the temporary list
  //
  // Note that in most cases, the whole list will not be walked and maintained
  // at this stage.

  SlotSpanMetadata<thread_safe>* to_provision_head = nullptr;
  SlotSpanMetadata<thread_safe>* to_provision_tail = nullptr;

  for (; slot_span; slot_span = next_slot_span) {
    next_slot_span = slot_span->next_slot_span;
    PA_DCHECK(slot_span->bucket == this);
    PA_DCHECK(slot_span != empty_slot_spans_head);
    PA_DCHECK(slot_span != decommitted_slot_spans_head);

    if (slot_span->is_active()) {
      // Has provisioned slots.
      if (slot_span->get_freelist_head()) {
        // Will use this slot span, no need to go further.
        break;
      } else {
        // Keeping head and tail because we don't want to reverse the list.
        if (!to_provision_head) {
          to_provision_head = slot_span;
        }
        if (to_provision_tail) {
          to_provision_tail->next_slot_span = slot_span;
        }
        to_provision_tail = slot_span;
        slot_span->next_slot_span = nullptr;
      }
    } else if (slot_span->is_empty()) {
      slot_span->next_slot_span = empty_slot_spans_head;
      empty_slot_spans_head = slot_span;
    } else if (PA_LIKELY(slot_span->is_decommitted())) {
      slot_span->next_slot_span = decommitted_slot_spans_head;
      decommitted_slot_spans_head = slot_span;
    } else {
      PA_DCHECK(slot_span->is_full());
      // Move this slot span... nowhere, and also mark it as full. We need it
      // marked so that free'ing can tell, and move it back into the active
      // list.
      slot_span->marked_full = 1;
      ++num_full_slot_spans;
      // Overflow. Most likely a correctness issue in the code.  It is in theory
      // possible that the number of full slot spans really reaches (1 << 24),
      // but this is very unlikely (and not possible with most pool settings).
      PA_CHECK(num_full_slot_spans);
      // Not necessary but might help stop accidents.
      slot_span->next_slot_span = nullptr;
    }
  }

  bool usable_active_list_head = false;
  // Found an active slot span with provisioned entries on the freelist.
  if (slot_span) {
    usable_active_list_head = true;
    // We have active slot spans with unprovisioned entries. Re-attach them into
    // the active list, past the span with freelist entries.
    if (to_provision_head) {
      auto* next = slot_span->next_slot_span;
      slot_span->next_slot_span = to_provision_head;
      to_provision_tail->next_slot_span = next;
    }
    active_slot_spans_head = slot_span;
  } else if (to_provision_head) {
    usable_active_list_head = true;
    // Need to provision new slots.
    active_slot_spans_head = to_provision_head;
  } else {
    // Active list is now empty.
    active_slot_spans_head =
        SlotSpanMetadata<thread_safe>::get_sentinel_slot_span_non_const();
  }

  return usable_active_list_head;
}

template <bool thread_safe>
void PartitionBucket<thread_safe>::MaintainActiveList() {
  SlotSpanMetadata<thread_safe>* slot_span = active_slot_spans_head;
  if (slot_span == SlotSpanMetadata<thread_safe>::get_sentinel_slot_span()) {
    return;
  }

  SlotSpanMetadata<thread_safe>* new_active_slot_spans_head = nullptr;
  SlotSpanMetadata<thread_safe>* new_active_slot_spans_tail = nullptr;

  SlotSpanMetadata<thread_safe>* next_slot_span;
  for (; slot_span; slot_span = next_slot_span) {
    next_slot_span = slot_span->next_slot_span;

    if (slot_span->is_active()) {
      // Ordering in the active slot span list matters, don't reverse it.
      if (!new_active_slot_spans_head) {
        new_active_slot_spans_head = slot_span;
      }
      if (new_active_slot_spans_tail) {
        new_active_slot_spans_tail->next_slot_span = slot_span;
      }
      new_active_slot_spans_tail = slot_span;
      slot_span->next_slot_span = nullptr;
    } else if (slot_span->is_empty()) {
      // For the empty and decommitted lists, LIFO ordering makes sense (since
      // it would lead to reusing memory which has been touched relatively
      // recently, which only matters for committed spans though).
      slot_span->next_slot_span = empty_slot_spans_head;
      empty_slot_spans_head = slot_span;
    } else if (slot_span->is_decommitted()) {
      slot_span->next_slot_span = decommitted_slot_spans_head;
      decommitted_slot_spans_head = slot_span;
    } else {
      // Full slot spans are not tracked, just accounted for.
      PA_DCHECK(slot_span->is_full());
      slot_span->marked_full = 1;
      ++num_full_slot_spans;
      PA_CHECK(num_full_slot_spans);  // Overflow.
      slot_span->next_slot_span = nullptr;
    }
  }

  if (!new_active_slot_spans_head) {
    new_active_slot_spans_head =
        SlotSpanMetadata<thread_safe>::get_sentinel_slot_span_non_const();
  }
  active_slot_spans_head = new_active_slot_spans_head;
}

template <bool thread_safe>
void PartitionBucket<thread_safe>::SortSlotSpanFreelists() {
  for (auto* slot_span = active_slot_spans_head; slot_span;
       slot_span = slot_span->next_slot_span) {
    // No need to sort the freelist if it's already sorted. Note that if the
    // freelist is sorted, this means that it didn't change at all since the
    // last call. This may be a good signal to shrink it if possible (if an
    // entire OS page is free, we can decommit it).
    //
    // Besides saving CPU, this also avoids touching memory of fully idle slot
    // spans, which may required paging.
    if (slot_span->num_allocated_slots > 0 &&
        !slot_span->freelist_is_sorted()) {
      slot_span->SortFreelist();
    }
  }
}

PA_COMPONENT_EXPORT(PARTITION_ALLOC)
bool CompareSlotSpans(SlotSpanMetadata<ThreadSafe>* a,
                      SlotSpanMetadata<ThreadSafe>* b) {
  auto criteria_tuple = [](SlotSpanMetadata<ThreadSafe> const* a) {
    size_t freelist_length = a->GetFreelistLength();
    // The criteria are, in order (hence the lexicographic comparison below):
    // 1. Prefer slot spans with freelist entries. The ones without freelist
    //    entries would be skipped in SetNewActiveSlotSpan() anyway.
    // 2. Then the ones with the fewest freelist entries. They are either close
    //    to being full (for the provisioned memory), or close to being pushed
    //    at the end of the list (since they would not have freelist entries
    //    anymore, and would either fall into the first case, or be skipped by
    //    SetNewActiveSlotSpan()).
    // 3. The ones with the fewer unprovisioned slots, meaning that they are
    //    close to being completely full.
    //
    // Note that this sorting order is not necessarily the best one when slot
    // spans are partially provisioned. From local testing, in steady-state,
    // most slot spans are entirely provisioned (or decommitted), which may be a
    // consequence of the lack of partial slot span decommit, or of fairly
    // effective fragmentation avoidance heuristics. Make sure to evaluate
    // whether an alternative sorting order (sorting according to freelist size
    // + unprovisioned slots) makes more sense.
    return std::tuple<bool, size_t, size_t>{
        freelist_length == 0, freelist_length, a->num_unprovisioned_slots};
  };

  return criteria_tuple(a) < criteria_tuple(b);
}

template <bool thread_safe>
void PartitionBucket<thread_safe>::SortActiveSlotSpans() {
  // Sorting up to |kMaxSlotSpansToSort| slot spans. This is capped for two
  // reasons:
  // - Limiting execution time
  // - Current code cannot allocate.
  //
  // In practice though, it's rare to have that many active slot spans.
  SlotSpanMetadata<thread_safe>* active_spans_array[kMaxSlotSpansToSort];
  size_t index = 0;
  SlotSpanMetadata<thread_safe>* overflow_spans_start = nullptr;

  for (auto* slot_span = active_slot_spans_head; slot_span;
       slot_span = slot_span->next_slot_span) {
    if (index < kMaxSlotSpansToSort) {
      active_spans_array[index++] = slot_span;
    } else {
      // Starting from this one, not sorting the slot spans.
      overflow_spans_start = slot_span;
      break;
    }
  }

  // We sort the active slot spans so that allocations are preferably serviced
  // from the fullest ones. This way we hope to reduce fragmentation by keeping
  // as few slot spans as full as possible.
  //
  // With perfect information on allocation lifespan, we would be able to pack
  // allocations and get almost no fragmentation. This is obviously not the
  // case, so we have partially full SlotSpans. Nevertheless, as a heuristic we
  // want to:
  // - Keep almost-empty slot spans as empty as possible
  // - Keep mostly-full slot spans as full as possible
  //
  // The first part is done in the hope that future free()s will make these
  // slot spans completely empty, allowing us to reclaim them. To that end, sort
  // SlotSpans periodically so that the fullest ones are preferred.
  //
  // std::sort() is not completely guaranteed to never allocate memory. However,
  // it may not throw std::bad_alloc, which constrains the implementation. In
  // addition, this is protected by the reentrancy guard, so we would detect
  // such an allocation.
  std::sort(active_spans_array, active_spans_array + index, CompareSlotSpans);

  active_slot_spans_head = overflow_spans_start;

  // Reverse order, since we insert at the head of the list.
  for (int i = index - 1; i >= 0; i--) {
    if (active_spans_array[i] ==
        SlotSpanMetadata<thread_safe>::get_sentinel_slot_span()) {
      // The sentinel is const, don't try to write to it.
      PA_DCHECK(active_slot_spans_head == nullptr);
    } else {
      active_spans_array[i]->next_slot_span = active_slot_spans_head;
    }
    active_slot_spans_head = active_spans_array[i];
  }
}

template <bool thread_safe>
uintptr_t PartitionBucket<thread_safe>::SlowPathAlloc(
    PartitionRoot<thread_safe>* root,
    unsigned int flags,
    size_t raw_size,
    size_t slot_span_alignment,
    bool* is_already_zeroed) {
  PA_DCHECK((slot_span_alignment >= PartitionPageSize()) &&
            base::bits::IsPowerOfTwo(slot_span_alignment));

  // The slow path is called when the freelist is empty. The only exception is
  // when a higher-order alignment is requested, in which case the freelist
  // logic is bypassed and we go directly for slot span allocation.
  bool allocate_aligned_slot_span = slot_span_alignment > PartitionPageSize();
  PA_DCHECK(!active_slot_spans_head->get_freelist_head() ||
            allocate_aligned_slot_span);

  SlotSpanMetadata<thread_safe>* new_slot_span = nullptr;
  // |new_slot_span->bucket| will always be |this|, except when |this| is the
  // sentinel bucket, which is used to signal a direct mapped allocation.  In
  // this case |new_bucket| will be set properly later. This avoids a read for
  // most allocations.
  PartitionBucket* new_bucket = this;
  *is_already_zeroed = false;

  // For the PartitionRoot::Alloc() API, we have a bunch of buckets
  // marked as special cases. We bounce them through to the slow path so that
  // we can still have a blazing fast hot path due to lack of corner-case
  // branches.
  //
  // Note: The ordering of the conditionals matter! In particular,
  // SetNewActiveSlotSpan() has a side-effect even when returning
  // false where it sweeps the active list and may move things into the empty or
  // decommitted lists which affects the subsequent conditional.
  if (PA_UNLIKELY(is_direct_mapped())) {
    PA_DCHECK(raw_size > kMaxBucketed);
    PA_DCHECK(this == &root->sentinel_bucket);
    PA_DCHECK(active_slot_spans_head ==
              SlotSpanMetadata<thread_safe>::get_sentinel_slot_span());

    // No fast path for direct-mapped allocations.
    if (flags & AllocFlags::kFastPathOrReturnNull) {
      return 0;
    }

    new_slot_span =
        PartitionDirectMap(root, flags, raw_size, slot_span_alignment);
    if (new_slot_span) {
      new_bucket = new_slot_span->bucket;
    }
    // Memory from PageAllocator is always zeroed.
    *is_already_zeroed = true;
  } else if (PA_LIKELY(!allocate_aligned_slot_span && SetNewActiveSlotSpan())) {
    // First, did we find an active slot span in the active list?
    new_slot_span = active_slot_spans_head;
    PA_DCHECK(new_slot_span->is_active());
  } else if (PA_LIKELY(!allocate_aligned_slot_span &&
                       (empty_slot_spans_head != nullptr ||
                        decommitted_slot_spans_head != nullptr))) {
    // Second, look in our lists of empty and decommitted slot spans.
    // Check empty slot spans first, which are preferred, but beware that an
    // empty slot span might have been decommitted.
    while (PA_LIKELY((new_slot_span = empty_slot_spans_head) != nullptr)) {
      PA_DCHECK(new_slot_span->bucket == this);
      PA_DCHECK(new_slot_span->is_empty() || new_slot_span->is_decommitted());
      empty_slot_spans_head = new_slot_span->next_slot_span;
      // Accept the empty slot span unless it got decommitted.
      if (new_slot_span->get_freelist_head()) {
        new_slot_span->next_slot_span = nullptr;
        new_slot_span->ToSuperPageExtent()
            ->IncrementNumberOfNonemptySlotSpans();

        // Re-activating an empty slot span, update accounting.
        size_t dirty_size = base::bits::AlignUp(
            new_slot_span->GetProvisionedSize(), SystemPageSize());
        PA_DCHECK(root->empty_slot_spans_dirty_bytes >= dirty_size);
        root->empty_slot_spans_dirty_bytes -= dirty_size;

        break;
      }
      PA_DCHECK(new_slot_span->is_decommitted());
      new_slot_span->next_slot_span = decommitted_slot_spans_head;
      decommitted_slot_spans_head = new_slot_span;
    }
    if (PA_UNLIKELY(!new_slot_span) &&
        PA_LIKELY(decommitted_slot_spans_head != nullptr)) {
      // Commit can be expensive, don't do it.
      if (flags & AllocFlags::kFastPathOrReturnNull) {
        return 0;
      }

      new_slot_span = decommitted_slot_spans_head;
      PA_DCHECK(new_slot_span->bucket == this);
      PA_DCHECK(new_slot_span->is_decommitted());
      decommitted_slot_spans_head = new_slot_span->next_slot_span;

      // If lazy commit is enabled, pages will be recommitted when provisioning
      // slots, in ProvisionMoreSlotsAndAllocOne(), not here.
      if (!kUseLazyCommit) {
        uintptr_t slot_span_start =
            SlotSpanMetadata<thread_safe>::ToSlotSpanStart(new_slot_span);
        // Since lazy commit isn't used, we have a guarantee that all slot span
        // pages have been previously committed, and then decommitted using
        // PageAccessibilityDisposition::kAllowKeepForPerf, so use the
        // same option as an optimization.
        // TODO(lizeb): Handle commit failure.
        root->RecommitSystemPagesForData(
            slot_span_start, new_slot_span->bucket->get_bytes_per_span(),
            PageAccessibilityDisposition::kAllowKeepForPerf);
      }

      new_slot_span->Reset();
      *is_already_zeroed = DecommittedMemoryIsAlwaysZeroed();
    }
    PA_DCHECK(new_slot_span);
  } else {
    // Getting a new slot span is expensive, don't do it.
    if (flags & AllocFlags::kFastPathOrReturnNull) {
      return 0;
    }

    // Third. If we get here, we need a brand new slot span.
    // TODO(bartekn): For single-slot slot spans, we can use rounded raw_size
    // as slot_span_committed_size.
    new_slot_span = AllocNewSlotSpan(root, flags, slot_span_alignment);
    // New memory from PageAllocator is always zeroed.
    *is_already_zeroed = true;
  }

  // Bail if we had a memory allocation failure.
  if (PA_UNLIKELY(!new_slot_span)) {
    PA_DCHECK(active_slot_spans_head ==
              SlotSpanMetadata<thread_safe>::get_sentinel_slot_span());
    if (flags & AllocFlags::kReturnNull) {
      return 0;
    }
    // See comment in PartitionDirectMap() for unlocking.
    ScopedUnlockGuard unlock{root->lock_};
    root->OutOfMemory(raw_size);
    PA_IMMEDIATE_CRASH();  // Not required, kept as documentation.
  }

  PA_DCHECK(new_bucket != &root->sentinel_bucket);
  new_bucket->active_slot_spans_head = new_slot_span;
  if (new_slot_span->CanStoreRawSize()) {
    new_slot_span->SetRawSize(raw_size);
  }

  // If we found an active slot span with free slots, or an empty slot span, we
  // have a usable freelist head.
  if (PA_LIKELY(new_slot_span->get_freelist_head() != nullptr)) {
    PartitionFreelistEntry* entry =
        new_slot_span->PopForAlloc(new_bucket->slot_size);

    // We may have set *is_already_zeroed to true above, make sure that the
    // freelist entry doesn't contain data. Either way, it wouldn't be a good
    // idea to let users see our internal data.
    uintptr_t slot_start = entry->ClearForAllocation();
    return slot_start;
  }

  // Otherwise, we need to provision more slots by committing more pages. Build
  // the free list for the newly provisioned slots.
  PA_DCHECK(new_slot_span->num_unprovisioned_slots);
  return ProvisionMoreSlotsAndAllocOne(root, new_slot_span);
}

template <bool thread_safe>
uintptr_t PartitionBucket<thread_safe>::AllocNewSuperPageSpanForGwpAsan(
    PartitionRoot<thread_safe>* root,
    size_t super_page_count,
    unsigned int flags) {
  return AllocNewSuperPageSpan(root, super_page_count, flags);
}

template <bool thread_safe>
void PartitionBucket<thread_safe>::InitializeSlotSpanForGwpAsan(
    SlotSpanMetadata<thread_safe>* slot_span) {
  InitializeSlotSpan(slot_span);
}

template struct PartitionBucket<ThreadSafe>;

}  // namespace partition_alloc::internal
