// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/allocator/partition_allocator/src/partition_alloc/partition_dcheck_helper.h"

#include <cstdint>

#include "base/allocator/partition_allocator/src/partition_alloc/partition_bucket.h"
#include "base/allocator/partition_allocator/src/partition_alloc/partition_page.h"
#include "base/allocator/partition_allocator/src/partition_alloc/partition_root.h"

namespace partition_alloc::internal {

#if BUILDFLAG(PA_DCHECK_IS_ON)

void DCheckIsValidSlotSpan(internal::SlotSpanMetadata* slot_span) {
  PartitionRoot* root = PartitionRoot::FromSlotSpan(slot_span);
  PA_DCHECK(root->inverted_self == ~reinterpret_cast<uintptr_t>(root));
}

void DCheckIsValidShiftFromSlotStart(internal::SlotSpanMetadata* slot_span,
                                     uintptr_t shift_from_slot_start) {
  PartitionRoot* root = PartitionRoot::FromSlotSpan(slot_span);
  PA_DCHECK(shift_from_slot_start >= root->settings.extras_offset);
  // Use <= to allow an address immediately past the object.
  PA_DCHECK(shift_from_slot_start <=
            root->settings.extras_offset + root->GetSlotUsableSize(slot_span));
}

void DCheckIsWithInSuperPagePayload(uintptr_t address) {
  uintptr_t super_page = address & kSuperPageBaseMask;
  auto* extent = PartitionSuperPageToExtent(super_page);
  PA_DCHECK(IsWithinSuperPagePayload(address,
                                     IsManagedByNormalBuckets(address) &&
                                         extent->root->IsQuarantineAllowed()));
}

void DCheckIsValidObjectAddress(internal::SlotSpanMetadata* slot_span,
                                uintptr_t object_addr) {
  uintptr_t slot_span_start = SlotSpanMetadata::ToSlotSpanStart(slot_span);
  auto* root = PartitionRoot::FromSlotSpan(slot_span);
  PA_DCHECK((object_addr - slot_span_start) % slot_span->bucket->slot_size ==
            root->settings.extras_offset);
}

void DCheckNumberOfPartitionPagesInSuperPagePayload(
    const PartitionSuperPageExtentEntry* entry,
    const PartitionRoot* root,
    size_t number_of_nonempty_slot_spans) {
  uintptr_t super_page = base::bits::AlignDown(
      reinterpret_cast<uintptr_t>(entry), kSuperPageAlignment);
  size_t number_of_partition_pages_in_superpage_payload =
      SuperPagePayloadSize(super_page, root->IsQuarantineAllowed()) /
      PartitionPageSize();
  PA_DCHECK(number_of_partition_pages_in_superpage_payload >
            number_of_nonempty_slot_spans);
}

void DCheckRootLockIsAcquired(PartitionRoot* root) {
  PartitionRootLock(root).AssertAcquired();
}

void DCheckRootLockOfSlotSpanIsAcquired(internal::SlotSpanMetadata* slot_span) {
  DCheckRootLockIsAcquired(PartitionRoot::FromSlotSpan(slot_span));
}

#endif  // BUILDFLAG(PA_DCHECK_IS_ON)

}  // namespace partition_alloc::internal
