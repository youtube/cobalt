// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/allocator/partition_allocator/src/partition_alloc/pointers/raw_ptr_backup_ref_impl.h"

#include <cstdint>

#include "base/allocator/partition_allocator/src/partition_alloc/dangling_raw_ptr_checks.h"
#include "base/allocator/partition_allocator/src/partition_alloc/partition_alloc.h"
#include "base/allocator/partition_allocator/src/partition_alloc/partition_alloc_base/check.h"
#include "base/allocator/partition_allocator/src/partition_alloc/partition_alloc_buildflags.h"
#include "base/allocator/partition_allocator/src/partition_alloc/partition_ref_count.h"
#include "base/allocator/partition_allocator/src/partition_alloc/partition_root.h"
#include "base/allocator/partition_allocator/src/partition_alloc/reservation_offset_table.h"

namespace base::internal {

template <bool AllowDangling, bool ExperimentalAsh>
void RawPtrBackupRefImpl<AllowDangling, ExperimentalAsh>::AcquireInternal(
    uintptr_t address) {
#if BUILDFLAG(PA_DCHECK_IS_ON) || BUILDFLAG(ENABLE_BACKUP_REF_PTR_SLOW_CHECKS)
  PA_BASE_CHECK(UseBrp(address));
#endif
  uintptr_t slot_start =
      partition_alloc::PartitionAllocGetSlotStartInBRPPool(address);
  if constexpr (AllowDangling) {
    partition_alloc::internal::PartitionRefCountPointer(slot_start)
        ->AcquireFromUnprotectedPtr();
  } else {
    partition_alloc::internal::PartitionRefCountPointer(slot_start)->Acquire();
  }
}

template <bool AllowDangling, bool ExperimentalAsh>
void RawPtrBackupRefImpl<AllowDangling, ExperimentalAsh>::ReleaseInternal(
    uintptr_t address) {
#if BUILDFLAG(PA_DCHECK_IS_ON) || BUILDFLAG(ENABLE_BACKUP_REF_PTR_SLOW_CHECKS)
  PA_BASE_CHECK(UseBrp(address));
#endif
  uintptr_t slot_start =
      partition_alloc::PartitionAllocGetSlotStartInBRPPool(address);
  if constexpr (AllowDangling) {
    if (partition_alloc::internal::PartitionRefCountPointer(slot_start)
            ->ReleaseFromUnprotectedPtr()) {
      partition_alloc::internal::PartitionAllocFreeForRefCounting(slot_start);
    }
  } else {
    if (partition_alloc::internal::PartitionRefCountPointer(slot_start)
            ->Release()) {
      partition_alloc::internal::PartitionAllocFreeForRefCounting(slot_start);
    }
  }
}

template <bool AllowDangling, bool ExperimentalAsh>
void RawPtrBackupRefImpl<AllowDangling, ExperimentalAsh>::
    ReportIfDanglingInternal(uintptr_t address) {
  if (partition_alloc::internal::IsUnretainedDanglingRawPtrCheckEnabled()) {
    if (IsSupportedAndNotNull(address)) {
      uintptr_t slot_start =
          partition_alloc::PartitionAllocGetSlotStartInBRPPool(address);
      partition_alloc::internal::PartitionRefCountPointer(slot_start)
          ->ReportIfDangling();
    }
  }
}

// static
template <bool AllowDangling, bool ExperimentalAsh>
bool RawPtrBackupRefImpl<AllowDangling, ExperimentalAsh>::
    CheckPointerWithinSameAlloc(uintptr_t before_addr,
                                uintptr_t after_addr,
                                size_t type_size) {
  partition_alloc::internal::PtrPosWithinAlloc ptr_pos_within_alloc =
      partition_alloc::internal::IsPtrWithinSameAlloc(before_addr, after_addr,
                                                      type_size);
  // No need to check that |new_ptr| is in the same pool, as
  // IsPtrWithinSameAlloc() checks that it's within the same allocation, so
  // must be the same pool.
  PA_BASE_CHECK(ptr_pos_within_alloc !=
                partition_alloc::internal::PtrPosWithinAlloc::kFarOOB);

#if BUILDFLAG(BACKUP_REF_PTR_POISON_OOB_PTR)
  return ptr_pos_within_alloc ==
         partition_alloc::internal::PtrPosWithinAlloc::kAllocEnd;
#else
  return false;
#endif
}

template <bool AllowDangling, bool ExperimentalAsh>
bool RawPtrBackupRefImpl<AllowDangling, ExperimentalAsh>::IsPointeeAlive(
    uintptr_t address) {
#if BUILDFLAG(PA_DCHECK_IS_ON) || BUILDFLAG(ENABLE_BACKUP_REF_PTR_SLOW_CHECKS)
  PA_BASE_CHECK(UseBrp(address));
#endif
  uintptr_t slot_start =
      partition_alloc::PartitionAllocGetSlotStartInBRPPool(address);
  return partition_alloc::internal::PartitionRefCountPointer(slot_start)
      ->IsAlive();
}

// Explicitly instantiates the two BackupRefPtr variants in the .cc. This
// ensures the definitions not visible from the .h are available in the binary.
template struct RawPtrBackupRefImpl</*AllowDangling=*/false,
                                    /*ExperimentalAsh=*/false>;
template struct RawPtrBackupRefImpl</*AllowDangling=*/false,
                                    /*ExperimentalAsh=*/true>;
template struct RawPtrBackupRefImpl</*AllowDangling=*/true,
                                    /*ExperimentalAsh=*/false>;
template struct RawPtrBackupRefImpl</*AllowDangling=*/true,
                                    /*ExperimentalAsh=*/true>;

#if BUILDFLAG(PA_DCHECK_IS_ON) || BUILDFLAG(ENABLE_BACKUP_REF_PTR_SLOW_CHECKS)
void CheckThatAddressIsntWithinFirstPartitionPage(uintptr_t address) {
  if (partition_alloc::internal::IsManagedByDirectMap(address)) {
    uintptr_t reservation_start =
        partition_alloc::internal::GetDirectMapReservationStart(address);
    PA_BASE_CHECK(address - reservation_start >=
                  partition_alloc::PartitionPageSize());
  } else {
    PA_BASE_CHECK(partition_alloc::internal::IsManagedByNormalBuckets(address));
    PA_BASE_CHECK(address % partition_alloc::kSuperPageSize >=
                  partition_alloc::PartitionPageSize());
  }
}
#endif  // BUILDFLAG(PA_DCHECK_IS_ON) ||
        // BUILDFLAG(ENABLE_BACKUP_REF_PTR_SLOW_CHECKS)

}  // namespace base::internal
