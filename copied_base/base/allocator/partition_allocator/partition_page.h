// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_PAGE_H_
#define BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_PAGE_H_

#include <cstdint>
#include <cstring>
#include <limits>
#include <utility>

#include "base/allocator/partition_allocator/address_pool_manager.h"
#include "base/allocator/partition_allocator/address_pool_manager_types.h"
#include "base/allocator/partition_allocator/freeslot_bitmap_constants.h"
#include "base/allocator/partition_allocator/partition_address_space.h"
#include "base/allocator/partition_allocator/partition_alloc_base/bits.h"
#include "base/allocator/partition_allocator/partition_alloc_base/compiler_specific.h"
#include "base/allocator/partition_allocator/partition_alloc_base/component_export.h"
#include "base/allocator/partition_allocator/partition_alloc_base/debug/debugging_buildflags.h"
#include "base/allocator/partition_allocator/partition_alloc_base/thread_annotations.h"
#include "base/allocator/partition_allocator/partition_alloc_buildflags.h"
#include "base/allocator/partition_allocator/partition_alloc_check.h"
#include "base/allocator/partition_allocator/partition_alloc_constants.h"
#include "base/allocator/partition_allocator/partition_alloc_forward.h"
#include "base/allocator/partition_allocator/partition_bucket.h"
#include "base/allocator/partition_allocator/partition_freelist_entry.h"
#include "base/allocator/partition_allocator/reservation_offset_table.h"
#include "base/allocator/partition_allocator/tagging.h"
#include "build/build_config.h"

#if BUILDFLAG(USE_STARSCAN)
#include "base/allocator/partition_allocator/starscan/state_bitmap.h"
#endif

#if BUILDFLAG(PUT_REF_COUNT_IN_PREVIOUS_SLOT)
#include "base/allocator/partition_allocator/partition_ref_count.h"
#endif

namespace partition_alloc::internal {

// An "extent" is a span of consecutive superpages. We link the partition's next
// extent (if there is one) to the very start of a superpage's metadata area.
template <bool thread_safe>
struct PartitionSuperPageExtentEntry {
  PartitionRoot<thread_safe>* root;
  PartitionSuperPageExtentEntry<thread_safe>* next;
  uint16_t number_of_consecutive_super_pages;
  uint16_t number_of_nonempty_slot_spans;

  PA_ALWAYS_INLINE void IncrementNumberOfNonemptySlotSpans();
  PA_ALWAYS_INLINE void DecrementNumberOfNonemptySlotSpans();
};
static_assert(
    sizeof(PartitionSuperPageExtentEntry<ThreadSafe>) <= kPageMetadataSize,
    "PartitionSuperPageExtentEntry must be able to fit in a metadata slot");
static_assert(
    kMaxSuperPagesInPool / kSuperPageSize <=
        std::numeric_limits<
            decltype(PartitionSuperPageExtentEntry<
                     ThreadSafe>::number_of_consecutive_super_pages)>::max(),
    "number_of_consecutive_super_pages must be big enough");

// Returns the base of the first super page in the range of consecutive super
// pages.
//
// CAUTION! |extent| must point to the extent of the first super page in the
// range of consecutive super pages.
template <bool thread_safe>
PA_ALWAYS_INLINE uintptr_t SuperPagesBeginFromExtent(
    const PartitionSuperPageExtentEntry<thread_safe>* extent) {
  PA_DCHECK(0 < extent->number_of_consecutive_super_pages);
  uintptr_t extent_as_uintptr = reinterpret_cast<uintptr_t>(extent);
  PA_DCHECK(IsManagedByNormalBuckets(extent_as_uintptr));
  return base::bits::AlignDown(extent_as_uintptr, kSuperPageAlignment);
}

// Returns the end of the last super page in the range of consecutive super
// pages.
//
// CAUTION! |extent| must point to the extent of the first super page in the
// range of consecutive super pages.
template <bool thread_safe>
PA_ALWAYS_INLINE uintptr_t SuperPagesEndFromExtent(
    const PartitionSuperPageExtentEntry<thread_safe>* extent) {
  return SuperPagesBeginFromExtent(extent) +
         (extent->number_of_consecutive_super_pages * kSuperPageSize);
}

#if BUILDFLAG(USE_STARSCAN)
using AllocationStateMap =
    StateBitmap<kSuperPageSize, kSuperPageAlignment, kAlignment>;
#endif

// Metadata of the slot span.
//
// Some notes on slot span states. It can be in one of four major states:
// 1) Active.
// 2) Full.
// 3) Empty.
// 4) Decommitted.
// An active slot span has available free slots, as well as allocated ones.
// A full slot span has no free slots. An empty slot span has no allocated
// slots, and a decommitted slot span is an empty one that had its backing
// memory released back to the system.
//
// There are three linked lists tracking slot spans. The "active" list is an
// approximation of a list of active slot spans. It is an approximation because
// full, empty and decommitted slot spans may briefly be present in the list
// until we next do a scan over it. The "empty" list holds mostly empty slot
// spans, but may briefly hold decommitted ones too. The "decommitted" list
// holds only decommitted slot spans.
//
// The significant slot span transitions are:
// - Free() will detect when a full slot span has a slot freed and immediately
//   return the slot span to the head of the active list.
// - Free() will detect when a slot span is fully emptied. It _may_ add it to
//   the empty list or it _may_ leave it on the active list until a future
//   list scan.
// - Alloc() _may_ scan the active page list in order to fulfil the request.
//   If it does this, full, empty and decommitted slot spans encountered will be
//   booted out of the active list. If there are no suitable active slot spans
//   found, an empty or decommitted slot spans (if one exists) will be pulled
//   from the empty/decommitted list on to the active list.
#pragma pack(push, 1)
template <bool thread_safe>
struct SlotSpanMetadata {
 private:
  PartitionFreelistEntry* freelist_head = nullptr;

 public:
  // TODO(lizeb): Make as many fields as possible private or const, to
  // encapsulate things more clearly.
  SlotSpanMetadata<thread_safe>* next_slot_span = nullptr;
  PartitionBucket<thread_safe>* const bucket = nullptr;

  // CHECK()ed in AllocNewSlotSpan().
#if BUILDFLAG(HAS_64_BIT_POINTERS) && BUILDFLAG(IS_APPLE)
  // System page size is not a constant on Apple OSes, but is either 4 or 16kiB
  // (1 << 12 or 1 << 14), as checked in PartitionRoot::Init(). And
  // PartitionPageSize() is 4 times the OS page size.
  static constexpr size_t kMaxSlotsPerSlotSpan =
      4 * (1 << 14) / kSmallestBucket;
#elif BUILDFLAG(IS_LINUX) && defined(ARCH_CPU_ARM64)
  // System page size can be 4, 16, or 64 kiB on Linux on arm64. 64 kiB is
  // currently (kMaxSlotsPerSlotSpanBits == 13) not supported by the code,
  // so we use the 16 kiB maximum (64 kiB will crash).
  static constexpr size_t kMaxSlotsPerSlotSpan =
      4 * (1 << 14) / kSmallestBucket;
#else
  // A slot span can "span" multiple PartitionPages, but then its slot size is
  // larger, so it doesn't have as many slots.
  static constexpr size_t kMaxSlotsPerSlotSpan =
      PartitionPageSize() / kSmallestBucket;
#endif  // BUILDFLAG(HAS_64_BIT_POINTERS) && BUILDFLAG(IS_APPLE)
  // The maximum number of bits needed to cover all currently supported OSes.
  static constexpr size_t kMaxSlotsPerSlotSpanBits = 13;
  static_assert(kMaxSlotsPerSlotSpan < (1 << kMaxSlotsPerSlotSpanBits), "");

  // |marked_full| isn't equivalent to being full. Slot span is marked as full
  // iff it isn't on the active slot span list (or any other list).
  uint32_t marked_full : 1;
  // |num_allocated_slots| is 0 for empty or decommitted slot spans, which can
  // be further differentiated by checking existence of the freelist.
  uint32_t num_allocated_slots : kMaxSlotsPerSlotSpanBits;
  uint32_t num_unprovisioned_slots : kMaxSlotsPerSlotSpanBits;

 private:
  const uint32_t can_store_raw_size_ : 1;
  uint32_t freelist_is_sorted_ : 1;
  uint32_t unused1_ : (32 - 1 - 2 * kMaxSlotsPerSlotSpanBits - 1 - 1);
  // If |in_empty_cache_|==1, |empty_cache_index| is undefined and mustn't be
  // used.
  uint16_t in_empty_cache_ : 1;
  uint16_t empty_cache_index_ : kEmptyCacheIndexBits;  // < kMaxFreeableSpans.
  uint16_t unused2_ : (16 - 1 - kEmptyCacheIndexBits);
  // Can use only 48 bits (6B) in this bitfield, as this structure is embedded
  // in PartitionPage which has 2B worth of fields and must fit in 32B.

 public:
  PA_COMPONENT_EXPORT(PARTITION_ALLOC)
  explicit SlotSpanMetadata(PartitionBucket<thread_safe>* bucket);

  // Public API
  // Note the matching Alloc() functions are in PartitionPage.
  PA_NOINLINE PA_COMPONENT_EXPORT(PARTITION_ALLOC) void FreeSlowPath(
      size_t number_of_freed);
  PA_ALWAYS_INLINE PartitionFreelistEntry* PopForAlloc(size_t size);
  PA_ALWAYS_INLINE void Free(uintptr_t ptr);
  // Appends the passed freelist to the slot-span's freelist. Please note that
  // the function doesn't increment the tags of the passed freelist entries,
  // since FreeNoHooks() did it already.
  PA_ALWAYS_INLINE void AppendFreeList(PartitionFreelistEntry* head,
                                       PartitionFreelistEntry* tail,
                                       size_t number_of_freed);

  void Decommit(PartitionRoot<thread_safe>* root);
  void DecommitIfPossible(PartitionRoot<thread_safe>* root);

  // Sorts the freelist in ascending addresses order.
  void SortFreelist();
  // Inserts the slot span into the empty ring, making space for the new slot
  // span, and potentially shrinking the ring.
  void RegisterEmpty();

  // Pointer/address manipulation functions. These must be static as the input
  // |slot_span| pointer may be the result of an offset calculation and
  // therefore cannot be trusted. The objective of these functions is to
  // sanitize this input.
  PA_ALWAYS_INLINE static uintptr_t ToSlotSpanStart(
      const SlotSpanMetadata* slot_span);
  PA_ALWAYS_INLINE static SlotSpanMetadata* FromAddr(uintptr_t address);
  PA_ALWAYS_INLINE static SlotSpanMetadata* FromSlotStart(uintptr_t slot_start);
  PA_ALWAYS_INLINE static SlotSpanMetadata* FromObject(void* object);
  PA_ALWAYS_INLINE static SlotSpanMetadata* FromObjectInnerAddr(
      uintptr_t address);
  PA_ALWAYS_INLINE static SlotSpanMetadata* FromObjectInnerPtr(void* ptr);

  PA_ALWAYS_INLINE PartitionSuperPageExtentEntry<thread_safe>*
  ToSuperPageExtent() const;

  // Checks if it is feasible to store raw_size.
  PA_ALWAYS_INLINE bool CanStoreRawSize() const { return can_store_raw_size_; }
  // The caller is responsible for ensuring that raw_size can be stored before
  // calling Set/GetRawSize.
  PA_ALWAYS_INLINE void SetRawSize(size_t raw_size);
  PA_ALWAYS_INLINE size_t GetRawSize() const;

  PA_ALWAYS_INLINE PartitionFreelistEntry* get_freelist_head() const {
    return freelist_head;
  }
  PA_ALWAYS_INLINE void SetFreelistHead(PartitionFreelistEntry* new_head);

  // Returns size of the region used within a slot. The used region comprises
  // of actual allocated data, extras and possibly empty space in the middle.
  PA_ALWAYS_INLINE size_t GetUtilizedSlotSize() const {
    // The returned size can be:
    // - The slot size for small buckets.
    // - Exact size needed to satisfy allocation (incl. extras), for large
    //   buckets and direct-mapped allocations (see also the comment in
    //   CanStoreRawSize() for more info).
    if (PA_LIKELY(!CanStoreRawSize())) {
      return bucket->slot_size;
    }
    return GetRawSize();
  }

  // This includes padding due to rounding done at allocation; we don't know the
  // requested size at deallocation, so we use this in both places.
  PA_ALWAYS_INLINE size_t GetSlotSizeForBookkeeping() const {
    // This could be more precise for allocations where CanStoreRawSize()
    // returns true (large allocations). However this is called for *every*
    // allocation, so we don't want an extra branch there.
    return bucket->slot_size;
  }

  // Returns the size available to the app. It can be equal or higher than the
  // requested size. If higher, the overage won't exceed what's actually usable
  // by the app without a risk of running out of an allocated region or into
  // PartitionAlloc's internal data (like extras).
  PA_ALWAYS_INLINE size_t
  GetUsableSize(PartitionRoot<thread_safe>* root) const {
    // The returned size can be:
    // - The slot size minus extras, for small buckets. This could be more than
    //   requested size.
    // - Raw size minus extras, for large buckets and direct-mapped allocations
    //   (see also the comment in CanStoreRawSize() for more info). This is
    //   equal to requested size.
    return root->AdjustSizeForExtrasSubtract(GetUtilizedSlotSize());
  }

  // Returns the total size of the slots that are currently provisioned.
  PA_ALWAYS_INLINE size_t GetProvisionedSize() const {
    size_t num_provisioned_slots =
        bucket->get_slots_per_span() - num_unprovisioned_slots;
    size_t provisioned_size = num_provisioned_slots * bucket->slot_size;
    PA_DCHECK(provisioned_size <= bucket->get_bytes_per_span());
    return provisioned_size;
  }

  // Return the number of entries in the freelist.
  size_t GetFreelistLength() const {
    size_t num_provisioned_slots =
        bucket->get_slots_per_span() - num_unprovisioned_slots;
    return num_provisioned_slots - num_allocated_slots;
  }

  PA_ALWAYS_INLINE void Reset();

  // TODO(ajwong): Can this be made private?  https://crbug.com/787153
  PA_COMPONENT_EXPORT(PARTITION_ALLOC)
  static const SlotSpanMetadata* get_sentinel_slot_span();
  // The sentinel is not supposed to be modified and hence we mark it as const
  // under the hood. However, we often store it together with mutable metadata
  // objects and need a non-const pointer.
  // You can use this function for this case, but you need to ensure that the
  // returned object will not be written to.
  static SlotSpanMetadata* get_sentinel_slot_span_non_const();

  // Slot span state getters.
  PA_ALWAYS_INLINE bool is_active() const;
  PA_ALWAYS_INLINE bool is_full() const;
  PA_ALWAYS_INLINE bool is_empty() const;
  PA_ALWAYS_INLINE bool is_decommitted() const;
  PA_ALWAYS_INLINE bool in_empty_cache() const { return in_empty_cache_; }
  PA_ALWAYS_INLINE bool freelist_is_sorted() const {
    return freelist_is_sorted_;
  }
  PA_ALWAYS_INLINE void set_freelist_sorted() { freelist_is_sorted_ = true; }

 private:
  // sentinel_slot_span_ is used as a sentinel to indicate that there is no slot
  // span in the active list. We could use nullptr, but in that case we need to
  // add a null-check branch to the hot allocation path. We want to avoid that.
  //
  // Note, this declaration is kept in the header as opposed to an anonymous
  // namespace so the getter can be fully inlined.
  static const SlotSpanMetadata sentinel_slot_span_;
  // For the sentinel.
  constexpr SlotSpanMetadata() noexcept
      : marked_full(0),
        num_allocated_slots(0),
        num_unprovisioned_slots(0),
        can_store_raw_size_(false),
        freelist_is_sorted_(true),
        unused1_(0),
        in_empty_cache_(0),
        empty_cache_index_(0),
        unused2_(0) {}
};
#pragma pack(pop)
static_assert(sizeof(SlotSpanMetadata<ThreadSafe>) <= kPageMetadataSize,
              "SlotSpanMetadata must fit into a Page Metadata slot.");

// Metadata of a non-first partition page in a slot span.
struct SubsequentPageMetadata {
  // Raw size is the size needed to satisfy the allocation (requested size +
  // extras). If available, it can be used to report better statistics or to
  // bring protective cookie closer to the allocated memory.
  //
  // It can be used only if:
  // - there is no more than one slot in the slot span (otherwise we wouldn't
  //   know which slot the raw size applies to)
  // - there is more than one partition page in the slot span (the metadata of
  //   the first one is used to store slot information, but the second one is
  //   available for extra information)
  size_t raw_size;
};

// Each partition page has metadata associated with it. The metadata of the
// first page of a slot span, describes that slot span. If a slot span spans
// more than 1 page, the page metadata may contain rudimentary additional
// information.
// "Pack" the union so that common page metadata still fits within
// kPageMetadataSize. (SlotSpanMetadata is also "packed".)
#pragma pack(push, 1)
template <bool thread_safe>
struct PartitionPage {
  union {
    SlotSpanMetadata<thread_safe> slot_span_metadata;

    SubsequentPageMetadata subsequent_page_metadata;

    // sizeof(PartitionPageMetadata) must always be:
    // - a power of 2 (for fast modulo operations)
    // - below kPageMetadataSize
    //
    // This makes sure that this is respected no matter the architecture.
    char optional_padding[kPageMetadataSize - sizeof(uint8_t) - sizeof(bool)];
  };

  // The first PartitionPage of the slot span holds its metadata. This offset
  // tells how many pages in from that first page we are.
  // For direct maps, the first page metadata (that isn't super page extent
  // entry) uses this field to tell how many pages to the right the direct map
  // metadata starts.
  //
  // 6 bits is enough to represent all possible offsets, given that the smallest
  // partition page is 16kiB and the offset won't exceed 1MiB.
  static constexpr uint16_t kMaxSlotSpanMetadataBits = 6;
  static constexpr uint16_t kMaxSlotSpanMetadataOffset =
      (1 << kMaxSlotSpanMetadataBits) - 1;
  uint8_t slot_span_metadata_offset : kMaxSlotSpanMetadataBits;

  // |is_valid| tells whether the page is part of a slot span. If |false|,
  // |has_valid_span_after_this| tells whether it's an unused region in between
  // slot spans within the super page.
  // Note, |is_valid| has been added for clarity, but if we ever need to save
  // this bit, it can be inferred from:
  //   |!slot_span_metadata_offset && slot_span_metadata->bucket|.
  bool is_valid : 1;
  bool has_valid_span_after_this : 1;
  uint8_t unused;

  PA_ALWAYS_INLINE static PartitionPage* FromAddr(uintptr_t address);
};
#pragma pack(pop)
static_assert(sizeof(PartitionPage<ThreadSafe>) == kPageMetadataSize,
              "PartitionPage must be able to fit in a metadata slot");

// Certain functions rely on PartitionPage being either SlotSpanMetadata or
// SubsequentPageMetadata, and therefore freely casting between each other.
static_assert(offsetof(PartitionPage<ThreadSafe>, slot_span_metadata) == 0, "");
static_assert(offsetof(PartitionPage<ThreadSafe>, subsequent_page_metadata) ==
                  0,
              "");

template <bool thread_safe>
PA_ALWAYS_INLINE PartitionPage<thread_safe>* PartitionSuperPageToMetadataArea(
    uintptr_t super_page) {
  // This can't be just any super page, but it has to be the first super page of
  // the reservation, as we assume here that the metadata is near its beginning.
  PA_DCHECK(IsReservationStart(super_page));
  PA_DCHECK(!(super_page & kSuperPageOffsetMask));
  // The metadata area is exactly one system page (the guard page) into the
  // super page.
  return reinterpret_cast<PartitionPage<thread_safe>*>(super_page +
                                                       SystemPageSize());
}

PA_ALWAYS_INLINE const SubsequentPageMetadata* GetSubsequentPageMetadata(
    const PartitionPage<ThreadSafe>* page) {
  return &(page + 1)->subsequent_page_metadata;
}

PA_ALWAYS_INLINE SubsequentPageMetadata* GetSubsequentPageMetadata(
    PartitionPage<ThreadSafe>* page) {
  return &(page + 1)->subsequent_page_metadata;
}

template <bool thread_safe>
PA_ALWAYS_INLINE PartitionSuperPageExtentEntry<thread_safe>*
PartitionSuperPageToExtent(uintptr_t super_page) {
  // The very first entry of the metadata is the super page extent entry.
  return reinterpret_cast<PartitionSuperPageExtentEntry<thread_safe>*>(
      PartitionSuperPageToMetadataArea<thread_safe>(super_page));
}

#if BUILDFLAG(USE_STARSCAN)

// Size that should be reserved for state bitmap (if present) inside a super
// page. Elements of a super page are partition-page-aligned, hence the returned
// size is a multiple of partition page size.
PA_ALWAYS_INLINE PAGE_ALLOCATOR_CONSTANTS_DECLARE_CONSTEXPR size_t
ReservedStateBitmapSize() {
  return base::bits::AlignUp(sizeof(AllocationStateMap), PartitionPageSize());
}

// Size that should be committed for state bitmap (if present) inside a super
// page. It is a multiple of system page size.
PA_ALWAYS_INLINE PAGE_ALLOCATOR_CONSTANTS_DECLARE_CONSTEXPR size_t
CommittedStateBitmapSize() {
  return base::bits::AlignUp(sizeof(AllocationStateMap), SystemPageSize());
}

// Returns the address/pointer to the state bitmap in the super page. It's the
// caller's responsibility to ensure that the bitmaps even exist.
PA_ALWAYS_INLINE uintptr_t SuperPageStateBitmapAddr(uintptr_t super_page) {
  PA_DCHECK(!(super_page % kSuperPageAlignment));
  return super_page + PartitionPageSize() +
         (IsManagedByNormalBuckets(super_page) ? ReservedFreeSlotBitmapSize()
                                               : 0);
}

PA_ALWAYS_INLINE AllocationStateMap* SuperPageStateBitmap(
    uintptr_t super_page) {
  return reinterpret_cast<AllocationStateMap*>(
      SuperPageStateBitmapAddr(super_page));
}

#else  // BUILDFLAG(USE_STARSCAN)

PA_ALWAYS_INLINE PAGE_ALLOCATOR_CONSTANTS_DECLARE_CONSTEXPR size_t
ReservedStateBitmapSize() {
  return 0ull;
}

#endif  // BUILDFLAG(USE_STARSCAN)

PA_ALWAYS_INLINE uintptr_t
SuperPagePayloadStartOffset(bool is_managed_by_normal_buckets,
                            bool with_quarantine) {
  return PartitionPageSize() +
         (is_managed_by_normal_buckets ? ReservedFreeSlotBitmapSize() : 0) +
         (with_quarantine ? ReservedStateBitmapSize() : 0);
}

PA_ALWAYS_INLINE uintptr_t SuperPagePayloadBegin(uintptr_t super_page,
                                                 bool with_quarantine) {
  PA_DCHECK(!(super_page % kSuperPageAlignment));
  return super_page +
         SuperPagePayloadStartOffset(IsManagedByNormalBuckets(super_page),
                                     with_quarantine);
}

PA_ALWAYS_INLINE uintptr_t SuperPagePayloadEndOffset() {
  return kSuperPageSize - PartitionPageSize();
}

PA_ALWAYS_INLINE uintptr_t SuperPagePayloadEnd(uintptr_t super_page) {
  PA_DCHECK(!(super_page % kSuperPageAlignment));
  return super_page + SuperPagePayloadEndOffset();
}

PA_ALWAYS_INLINE size_t SuperPagePayloadSize(uintptr_t super_page,
                                             bool with_quarantine) {
  return SuperPagePayloadEnd(super_page) -
         SuperPagePayloadBegin(super_page, with_quarantine);
}

template <bool thread_safe>
PA_ALWAYS_INLINE void PartitionSuperPageExtentEntry<
    thread_safe>::IncrementNumberOfNonemptySlotSpans() {
#if BUILDFLAG(PA_DCHECK_IS_ON)
  uintptr_t super_page = base::bits::AlignDown(
      reinterpret_cast<uintptr_t>(this), kSuperPageAlignment);
  PA_DCHECK((SuperPagePayloadSize(super_page, root->IsQuarantineAllowed()) /
             PartitionPageSize()) > number_of_nonempty_slot_spans);
#endif
  ++number_of_nonempty_slot_spans;
}

template <bool thread_safe>
PA_ALWAYS_INLINE void PartitionSuperPageExtentEntry<
    thread_safe>::DecrementNumberOfNonemptySlotSpans() {
  PA_DCHECK(number_of_nonempty_slot_spans);
  --number_of_nonempty_slot_spans;
}

template <bool thread_safe>
PA_ALWAYS_INLINE PartitionSuperPageExtentEntry<thread_safe>*
SlotSpanMetadata<thread_safe>::ToSuperPageExtent() const {
  uintptr_t super_page = reinterpret_cast<uintptr_t>(this) & kSuperPageBaseMask;
  return PartitionSuperPageToExtent<thread_safe>(super_page);
}

// Returns whether the pointer lies within the super page's payload area (i.e.
// area devoted to slot spans). It doesn't check whether it's within a valid
// slot span. It merely ensures it doesn't fall in a meta-data region that would
// surely never contain user data.
PA_ALWAYS_INLINE bool IsWithinSuperPagePayload(uintptr_t address,
                                               bool with_quarantine) {
  // Quarantine can only be enabled for normal buckets in the current code.
  PA_DCHECK(!with_quarantine || IsManagedByNormalBuckets(address));
  uintptr_t super_page = address & kSuperPageBaseMask;
  uintptr_t payload_start = SuperPagePayloadBegin(super_page, with_quarantine);
  uintptr_t payload_end = SuperPagePayloadEnd(super_page);
  return address >= payload_start && address < payload_end;
}

// Converts from an address inside a super page into a pointer to the
// PartitionPage object (within super pages's metadata) that describes the
// partition page where |address| is located. |address| doesn't have to be
// located within a valid (i.e. allocated) slot span, but must be within the
// super page's payload area (i.e. area devoted to slot spans).
//
// While it is generally valid for |ptr| to be in the middle of an allocation,
// care has to be taken with direct maps that span multiple super pages. This
// function's behavior is undefined if |ptr| lies in a subsequent super page.
template <bool thread_safe>
PA_ALWAYS_INLINE PartitionPage<thread_safe>*
PartitionPage<thread_safe>::FromAddr(uintptr_t address) {
  uintptr_t super_page = address & kSuperPageBaseMask;

#if BUILDFLAG(PA_DCHECK_IS_ON)
  PA_DCHECK(IsReservationStart(super_page));
  auto* extent = PartitionSuperPageToExtent<thread_safe>(super_page);
  PA_DCHECK(IsWithinSuperPagePayload(address,
                                     IsManagedByNormalBuckets(address) &&
                                         extent->root->IsQuarantineAllowed()));
#endif

  uintptr_t partition_page_index =
      (address & kSuperPageOffsetMask) >> PartitionPageShift();
  // Index 0 is invalid because it is the super page extent metadata and the
  // last index is invalid because the whole PartitionPage is set as guard
  // pages. This repeats part of the payload PA_DCHECK above, which also checks
  // for other exclusions.
  PA_DCHECK(partition_page_index);
  PA_DCHECK(partition_page_index < NumPartitionPagesPerSuperPage() - 1);
  return PartitionSuperPageToMetadataArea<thread_safe>(super_page) +
         partition_page_index;
}

// Converts from a pointer to the SlotSpanMetadata object (within a super
// pages's metadata) into a pointer to the beginning of the slot span. This
// works on direct maps too.
template <bool thread_safe>
PA_ALWAYS_INLINE uintptr_t SlotSpanMetadata<thread_safe>::ToSlotSpanStart(
    const SlotSpanMetadata* slot_span) {
  uintptr_t pointer_as_uint = reinterpret_cast<uintptr_t>(slot_span);
  uintptr_t super_page_offset = (pointer_as_uint & kSuperPageOffsetMask);

  // A valid |page| must be past the first guard System page and within
  // the following metadata region.
  PA_DCHECK(super_page_offset > SystemPageSize());
  // Must be less than total metadata region.
  PA_DCHECK(super_page_offset <
            SystemPageSize() +
                (NumPartitionPagesPerSuperPage() * kPageMetadataSize));
  uintptr_t partition_page_index =
      (super_page_offset - SystemPageSize()) >> kPageMetadataShift;
  // Index 0 is invalid because it is the super page extent metadata and the
  // last index is invalid because the whole PartitionPage is set as guard
  // pages.
  PA_DCHECK(partition_page_index);
  PA_DCHECK(partition_page_index < NumPartitionPagesPerSuperPage() - 1);
  uintptr_t super_page_base = (pointer_as_uint & kSuperPageBaseMask);
  return super_page_base + (partition_page_index << PartitionPageShift());
}

// Converts an address inside a slot span into a pointer to the SlotSpanMetadata
// object (within super pages's metadata) that describes the slot span
// containing that slot.
//
// CAUTION! For direct-mapped allocation, |address| has to be within the first
// partition page.
template <bool thread_safe>
PA_ALWAYS_INLINE SlotSpanMetadata<thread_safe>*
SlotSpanMetadata<thread_safe>::FromAddr(uintptr_t address) {
  auto* page = PartitionPage<thread_safe>::FromAddr(address);
  PA_DCHECK(page->is_valid);
  // Partition pages in the same slot span share the same SlotSpanMetadata
  // object (located in the first PartitionPage object of that span). Adjust
  // for that.
  page -= page->slot_span_metadata_offset;
  PA_DCHECK(page->is_valid);
  PA_DCHECK(!page->slot_span_metadata_offset);
  auto* slot_span = &page->slot_span_metadata;
  // TODO(crbug.com/1257655): See if we can afford to make this a CHECK.
  PA_DCHECK(PartitionRoot<thread_safe>::IsValidSlotSpan(slot_span));
  // For direct map, if |address| doesn't point within the first partition page,
  // |slot_span_metadata_offset| will be 0, |page| won't get shifted, leaving
  // |slot_size| at 0.
  PA_DCHECK(slot_span->bucket->slot_size);
  return slot_span;
}

// Like |FromAddr|, but asserts that |slot_start| indeed points to the
// beginning of a slot. It doesn't check if the slot is actually allocated.
//
// This works on direct maps too.
template <bool thread_safe>
PA_ALWAYS_INLINE SlotSpanMetadata<thread_safe>*
SlotSpanMetadata<thread_safe>::FromSlotStart(uintptr_t slot_start) {
  auto* slot_span = FromAddr(slot_start);
#if BUILDFLAG(PA_DCHECK_IS_ON)
  // Checks that the pointer is a multiple of slot size.
  uintptr_t slot_span_start = ToSlotSpanStart(slot_span);
  PA_DCHECK(!((slot_start - slot_span_start) % slot_span->bucket->slot_size));
#endif  // BUILDFLAG(PA_DCHECK_IS_ON)
  return slot_span;
}

// Like |FromAddr|, but asserts that |object| indeed points to the beginning of
// an object. It doesn't check if the object is actually allocated.
//
// This works on direct maps too.
template <bool thread_safe>
PA_ALWAYS_INLINE SlotSpanMetadata<thread_safe>*
SlotSpanMetadata<thread_safe>::FromObject(void* object) {
  uintptr_t object_addr = ObjectPtr2Addr(object);
  auto* slot_span = FromAddr(object_addr);
#if BUILDFLAG(PA_DCHECK_IS_ON)
  // Checks that the object is exactly |extras_offset| away from a multiple of
  // slot size (i.e. from a slot start).
  uintptr_t slot_span_start = ToSlotSpanStart(slot_span);
  auto* root = PartitionRoot<thread_safe>::FromSlotSpan(slot_span);
  PA_DCHECK((object_addr - slot_span_start) % slot_span->bucket->slot_size ==
            root->flags.extras_offset);
#endif  // BUILDFLAG(PA_DCHECK_IS_ON)
  return slot_span;
}

// Like |FromAddr|, but asserts that |address| indeed points within an object.
// It doesn't check if the object is actually allocated.
//
// CAUTION! For direct-mapped allocation, |address| has to be within the first
// partition page.
template <bool thread_safe>
PA_ALWAYS_INLINE SlotSpanMetadata<thread_safe>*
SlotSpanMetadata<thread_safe>::FromObjectInnerAddr(uintptr_t address) {
  auto* slot_span = FromAddr(address);
#if BUILDFLAG(PA_DCHECK_IS_ON)
  // Checks that the address is within the expected object boundaries.
  uintptr_t slot_span_start = ToSlotSpanStart(slot_span);
  auto* root = PartitionRoot<thread_safe>::FromSlotSpan(slot_span);
  uintptr_t shift_from_slot_start =
      (address - slot_span_start) % slot_span->bucket->slot_size;
  PA_DCHECK(shift_from_slot_start >= root->flags.extras_offset);
  // Use <= to allow an address immediately past the object.
  PA_DCHECK(shift_from_slot_start <=
            root->flags.extras_offset + slot_span->GetUsableSize(root));
#endif  // BUILDFLAG(PA_DCHECK_IS_ON)
  return slot_span;
}
template <bool thread_safe>
PA_ALWAYS_INLINE SlotSpanMetadata<thread_safe>*
SlotSpanMetadata<thread_safe>::FromObjectInnerPtr(void* ptr) {
  return FromObjectInnerAddr(ObjectInnerPtr2Addr(ptr));
}

template <bool thread_safe>
PA_ALWAYS_INLINE void SlotSpanMetadata<thread_safe>::SetRawSize(
    size_t raw_size) {
  PA_DCHECK(CanStoreRawSize());
  auto* subsequent_page_metadata = GetSubsequentPageMetadata(
      reinterpret_cast<PartitionPage<thread_safe>*>(this));
  subsequent_page_metadata->raw_size = raw_size;
}

template <bool thread_safe>
PA_ALWAYS_INLINE size_t SlotSpanMetadata<thread_safe>::GetRawSize() const {
  PA_DCHECK(CanStoreRawSize());
  const auto* subsequent_page_metadata = GetSubsequentPageMetadata(
      reinterpret_cast<const PartitionPage<thread_safe>*>(this));
  return subsequent_page_metadata->raw_size;
}

template <bool thread_safe>
PA_ALWAYS_INLINE void SlotSpanMetadata<thread_safe>::SetFreelistHead(
    PartitionFreelistEntry* new_head) {
#if BUILDFLAG(PA_DCHECK_IS_ON)
  // |this| is in the metadata region, hence isn't MTE-tagged. Untag |new_head|
  // as well.
  uintptr_t new_head_untagged = UntagPtr(new_head);
  PA_DCHECK(!new_head ||
            (reinterpret_cast<uintptr_t>(this) & kSuperPageBaseMask) ==
                (new_head_untagged & kSuperPageBaseMask));
#endif
  freelist_head = new_head;
  // Inserted something new in the freelist, assume that it is not sorted
  // anymore.
  freelist_is_sorted_ = false;
}

template <bool thread_safe>
PA_ALWAYS_INLINE PartitionFreelistEntry*
SlotSpanMetadata<thread_safe>::PopForAlloc(size_t size) {
  // Not using bucket->slot_size directly as the compiler doesn't know that
  // |bucket->slot_size| is the same as |size|.
  PA_DCHECK(size == bucket->slot_size);
  PartitionFreelistEntry* result = freelist_head;
  // Not setting freelist_is_sorted_ to false since this doesn't destroy
  // ordering.
  freelist_head = freelist_head->GetNext(size);
  num_allocated_slots++;
  return result;
}

template <bool thread_safe>
PA_ALWAYS_INLINE void SlotSpanMetadata<thread_safe>::Free(uintptr_t slot_start)
    PA_EXCLUSIVE_LOCKS_REQUIRED(
        PartitionRoot<thread_safe>::FromSlotSpan(this)->lock_) {
#if BUILDFLAG(PA_DCHECK_IS_ON)
  auto* root = PartitionRoot<thread_safe>::FromSlotSpan(this);
  root->lock_.AssertAcquired();
#endif

  auto* entry = static_cast<internal::PartitionFreelistEntry*>(
      SlotStartAddr2Ptr(slot_start));
  // Catches an immediate double free.
  PA_CHECK(entry != freelist_head);
  // Look for double free one level deeper in debug.
  PA_DCHECK(!freelist_head ||
            entry != freelist_head->GetNext(bucket->slot_size));
  entry->SetNext(freelist_head);
  SetFreelistHead(entry);
  // A best effort double-free check. Works only on empty slot spans.
  PA_CHECK(num_allocated_slots);
  --num_allocated_slots;
  // If the span is marked full, or became empty, take the slow path to update
  // internal state.
  if (PA_UNLIKELY(marked_full || num_allocated_slots == 0)) {
    FreeSlowPath(1);
  } else {
    // All single-slot allocations must go through the slow path to
    // correctly update the raw size.
    PA_DCHECK(!CanStoreRawSize());
  }
}

template <bool thread_safe>
PA_ALWAYS_INLINE void SlotSpanMetadata<thread_safe>::AppendFreeList(
    PartitionFreelistEntry* head,
    PartitionFreelistEntry* tail,
    size_t number_of_freed)
    PA_EXCLUSIVE_LOCKS_REQUIRED(
        PartitionRoot<thread_safe>::FromSlotSpan(this)->lock_) {
#if BUILDFLAG(PA_DCHECK_IS_ON)
  auto* root = PartitionRoot<thread_safe>::FromSlotSpan(this);
  root->lock_.AssertAcquired();
  PA_DCHECK(!tail->GetNext(bucket->slot_size));
  PA_DCHECK(number_of_freed);
  PA_DCHECK(num_allocated_slots);
  if (CanStoreRawSize()) {
    PA_DCHECK(number_of_freed == 1);
  }
  {
    size_t number_of_entries = 0;
    for (auto* entry = head; entry;
         entry = entry->GetNext(bucket->slot_size), ++number_of_entries) {
      uintptr_t untagged_entry = UntagPtr(entry);
      // Check that all entries belong to this slot span.
      PA_DCHECK(ToSlotSpanStart(this) <= untagged_entry);
      PA_DCHECK(untagged_entry <
                ToSlotSpanStart(this) + bucket->get_bytes_per_span());
    }
    PA_DCHECK(number_of_entries == number_of_freed);
  }
#endif

  tail->SetNext(freelist_head);
  SetFreelistHead(head);
  PA_DCHECK(num_allocated_slots >= number_of_freed);
  num_allocated_slots -= number_of_freed;
  // If the span is marked full, or became empty, take the slow path to update
  // internal state.
  if (PA_UNLIKELY(marked_full || num_allocated_slots == 0)) {
    FreeSlowPath(number_of_freed);
  } else {
    // All single-slot allocations must go through the slow path to
    // correctly update the raw size.
    PA_DCHECK(!CanStoreRawSize());
  }
}

template <bool thread_safe>
PA_ALWAYS_INLINE bool SlotSpanMetadata<thread_safe>::is_active() const {
  PA_DCHECK(this != get_sentinel_slot_span());
  bool ret =
      (num_allocated_slots > 0 && (freelist_head || num_unprovisioned_slots));
  if (ret) {
    PA_DCHECK(!marked_full);
    PA_DCHECK(num_allocated_slots < bucket->get_slots_per_span());
  }
  return ret;
}

template <bool thread_safe>
PA_ALWAYS_INLINE bool SlotSpanMetadata<thread_safe>::is_full() const {
  PA_DCHECK(this != get_sentinel_slot_span());
  bool ret = (num_allocated_slots == bucket->get_slots_per_span());
  if (ret) {
    PA_DCHECK(!freelist_head);
    PA_DCHECK(!num_unprovisioned_slots);
    // May or may not be marked full, so don't check for that.
  }
  return ret;
}

template <bool thread_safe>
PA_ALWAYS_INLINE bool SlotSpanMetadata<thread_safe>::is_empty() const {
  PA_DCHECK(this != get_sentinel_slot_span());
  bool ret = (!num_allocated_slots && freelist_head);
  if (ret) {
    PA_DCHECK(!marked_full);
  }
  return ret;
}

template <bool thread_safe>
PA_ALWAYS_INLINE bool SlotSpanMetadata<thread_safe>::is_decommitted() const {
  PA_DCHECK(this != get_sentinel_slot_span());
  bool ret = (!num_allocated_slots && !freelist_head);
  if (ret) {
    PA_DCHECK(!marked_full);
    PA_DCHECK(!num_unprovisioned_slots);
    PA_DCHECK(!in_empty_cache_);
  }
  return ret;
}

template <bool thread_safe>
PA_ALWAYS_INLINE void SlotSpanMetadata<thread_safe>::Reset() {
  PA_DCHECK(is_decommitted());

  num_unprovisioned_slots = bucket->get_slots_per_span();
  PA_DCHECK(num_unprovisioned_slots);

  ToSuperPageExtent()->IncrementNumberOfNonemptySlotSpans();

  next_slot_span = nullptr;
}

#if BUILDFLAG(USE_STARSCAN)
// Returns the state bitmap from an address within a normal-bucket super page.
// It's the caller's responsibility to ensure that the bitmap exists.
PA_ALWAYS_INLINE AllocationStateMap* StateBitmapFromAddr(uintptr_t address) {
  PA_DCHECK(IsManagedByNormalBuckets(address));
  uintptr_t super_page = address & kSuperPageBaseMask;
  return SuperPageStateBitmap(super_page);
}
#endif  // BUILDFLAG(USE_STARSCAN)

// Iterates over all slot spans in a super-page. |Callback| must return true if
// early return is needed.
template <bool thread_safe, typename Callback>
void IterateSlotSpans(uintptr_t super_page,
                      bool with_quarantine,
                      Callback callback) {
#if BUILDFLAG(PA_DCHECK_IS_ON)
  PA_DCHECK(!(super_page % kSuperPageAlignment));
  auto* extent_entry = PartitionSuperPageToExtent<thread_safe>(super_page);
  extent_entry->root->lock_.AssertAcquired();
#endif

  using Page = PartitionPage<thread_safe>;
  using SlotSpan = SlotSpanMetadata<thread_safe>;
  auto* const first_page =
      Page::FromAddr(SuperPagePayloadBegin(super_page, with_quarantine));
  auto* const last_page =
      Page::FromAddr(SuperPagePayloadEnd(super_page) - PartitionPageSize());
  Page* page;
  SlotSpan* slot_span;
  for (page = first_page; page <= last_page;) {
    PA_DCHECK(!page->slot_span_metadata_offset);  // Ensure slot span beginning.
    if (!page->is_valid) {
      if (page->has_valid_span_after_this) {
        // The page doesn't represent a valid slot span, but there is another
        // one somewhere after this. Keep iterating to find it.
        ++page;
        continue;
      }
      // There are currently no valid spans from here on. No need to iterate
      // the rest of the super page.
      break;
    }
    slot_span = &page->slot_span_metadata;
    if (callback(slot_span)) {
      return;
    }
    page += slot_span->bucket->get_pages_per_slot_span();
  }
  // Each super page must have at least one valid slot span.
  PA_DCHECK(page > first_page);
  // Just a quick check that the search ended at a valid slot span and there
  // was no unnecessary iteration over gaps afterwards.
  PA_DCHECK(page == reinterpret_cast<Page*>(slot_span) +
                        slot_span->bucket->get_pages_per_slot_span());
}

}  // namespace partition_alloc::internal

#endif  // BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_PAGE_H_
