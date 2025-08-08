// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ALLOCATOR_PARTITION_ALLOCATOR_SRC_PARTITION_ALLOC_PARTITION_ADDRESS_SPACE_H_
#define BASE_ALLOCATOR_PARTITION_ALLOCATOR_SRC_PARTITION_ALLOC_PARTITION_ADDRESS_SPACE_H_

#include <cstddef>
#include <utility>

#include "base/allocator/partition_allocator/src/partition_alloc/address_pool_manager_types.h"
#include "base/allocator/partition_allocator/src/partition_alloc/page_allocator_constants.h"
#include "base/allocator/partition_allocator/src/partition_alloc/partition_alloc_base/bits.h"
#include "base/allocator/partition_allocator/src/partition_alloc/partition_alloc_base/compiler_specific.h"
#include "base/allocator/partition_allocator/src/partition_alloc/partition_alloc_base/component_export.h"
#include "base/allocator/partition_allocator/src/partition_alloc/partition_alloc_base/notreached.h"
#include "base/allocator/partition_allocator/src/partition_alloc/partition_alloc_buildflags.h"
#include "base/allocator/partition_allocator/src/partition_alloc/partition_alloc_check.h"
#include "base/allocator/partition_allocator/src/partition_alloc/partition_alloc_config.h"
#include "base/allocator/partition_allocator/src/partition_alloc/partition_alloc_constants.h"
#include "base/allocator/partition_allocator/src/partition_alloc/partition_alloc_forward.h"
#include "base/allocator/partition_allocator/src/partition_alloc/thread_isolation/alignment.h"
#include "build/build_config.h"

#if BUILDFLAG(ENABLE_THREAD_ISOLATION)
#include "base/allocator/partition_allocator/src/partition_alloc/thread_isolation/thread_isolation.h"
#endif

// The feature is not applicable to 32-bit address space.
#if BUILDFLAG(HAS_64_BIT_POINTERS)

namespace partition_alloc {

namespace internal {

// Manages PartitionAlloc address space, which is split into pools.
// See `glossary.md`.
class PA_COMPONENT_EXPORT(PARTITION_ALLOC) PartitionAddressSpace {
 public:
  // Represents pool-specific information about a given address.
  struct PoolInfo {
    pool_handle handle;
    uintptr_t base;
    uintptr_t offset;
  };

#if PA_CONFIG(DYNAMICALLY_SELECT_POOL_SIZE)
  PA_ALWAYS_INLINE static uintptr_t RegularPoolBaseMask() {
    return setup_.regular_pool_base_mask_;
  }
#else
  PA_ALWAYS_INLINE static constexpr uintptr_t RegularPoolBaseMask() {
    return kRegularPoolBaseMask;
  }
#endif

  PA_ALWAYS_INLINE static PoolInfo GetPoolInfo(uintptr_t address) {
    // When USE_BACKUP_REF_PTR is off, BRP pool isn't used.
#if !BUILDFLAG(ENABLE_BACKUP_REF_PTR_SUPPORT)
    PA_DCHECK(!IsInBRPPool(address));
#endif
    pool_handle pool = kNullPoolHandle;
    uintptr_t base = 0;
#if BUILDFLAG(ENABLE_BACKUP_REF_PTR_SUPPORT)
    if (IsInBRPPool(address)) {
      pool = kBRPPoolHandle;
      base = setup_.brp_pool_base_address_;
    } else
#endif  // BUILDFLAG(ENABLE_BACKUP_REF_PTR_SUPPORT)
      if (IsInRegularPool(address)) {
        pool = kRegularPoolHandle;
        base = setup_.regular_pool_base_address_;
      } else if (IsInConfigurablePool(address)) {
        PA_DCHECK(IsConfigurablePoolInitialized());
        pool = kConfigurablePoolHandle;
        base = setup_.configurable_pool_base_address_;
#if BUILDFLAG(ENABLE_THREAD_ISOLATION)
      } else if (IsInThreadIsolatedPool(address)) {
        pool = kThreadIsolatedPoolHandle;
        base = setup_.thread_isolated_pool_base_address_;
#endif
      } else {
        PA_NOTREACHED();
      }
    return PoolInfo{.handle = pool, .base = base, .offset = address - base};
  }
  PA_ALWAYS_INLINE static constexpr size_t ConfigurablePoolMaxSize() {
    return kConfigurablePoolMaxSize;
  }
  PA_ALWAYS_INLINE static constexpr size_t ConfigurablePoolMinSize() {
    return kConfigurablePoolMinSize;
  }

  // Initialize pools (except for the configurable one).
  //
  // This function must only be called from the main thread.
  static void Init();
  // Initialize the ConfigurablePool at the given address |pool_base|. It must
  // be aligned to the size of the pool. The size must be a power of two and
  // must be within [ConfigurablePoolMinSize(), ConfigurablePoolMaxSize()].
  //
  // This function must only be called from the main thread.
  static void InitConfigurablePool(uintptr_t pool_base, size_t size);
#if BUILDFLAG(ENABLE_THREAD_ISOLATION)
  static void InitThreadIsolatedPool(ThreadIsolationOption thread_isolation);
  static void UninitThreadIsolatedPoolForTesting();
#endif
  static void UninitForTesting();
  static void UninitConfigurablePoolForTesting();

  PA_ALWAYS_INLINE static bool IsInitialized() {
    // Either neither or both regular and BRP pool are initialized. The
    // configurable and thread isolated pool are initialized separately.
    if (setup_.regular_pool_base_address_ != kUninitializedPoolBaseAddress) {
      PA_DCHECK(setup_.brp_pool_base_address_ != kUninitializedPoolBaseAddress);
      return true;
    }

    PA_DCHECK(setup_.brp_pool_base_address_ == kUninitializedPoolBaseAddress);
    return false;
  }

  PA_ALWAYS_INLINE static bool IsConfigurablePoolInitialized() {
    return setup_.configurable_pool_base_address_ !=
           kUninitializedPoolBaseAddress;
  }

#if BUILDFLAG(ENABLE_THREAD_ISOLATION)
  PA_ALWAYS_INLINE static bool IsThreadIsolatedPoolInitialized() {
    return setup_.thread_isolated_pool_base_address_ !=
           kUninitializedPoolBaseAddress;
  }
#endif

  // Returns false for nullptr.
  PA_ALWAYS_INLINE static bool IsInRegularPool(uintptr_t address) {
#if PA_CONFIG(DYNAMICALLY_SELECT_POOL_SIZE)
    const uintptr_t regular_pool_base_mask = setup_.regular_pool_base_mask_;
#else
    constexpr uintptr_t regular_pool_base_mask = kRegularPoolBaseMask;
#endif
    return (address & regular_pool_base_mask) ==
           setup_.regular_pool_base_address_;
  }

  PA_ALWAYS_INLINE static uintptr_t RegularPoolBase() {
    return setup_.regular_pool_base_address_;
  }

  // Returns false for nullptr.
  PA_ALWAYS_INLINE static bool IsInBRPPool(uintptr_t address) {
#if PA_CONFIG(DYNAMICALLY_SELECT_POOL_SIZE)
    const uintptr_t brp_pool_base_mask = setup_.brp_pool_base_mask_;
#else
    constexpr uintptr_t brp_pool_base_mask = kBRPPoolBaseMask;
#endif
    return (address & brp_pool_base_mask) == setup_.brp_pool_base_address_;
  }

#if BUILDFLAG(GLUE_CORE_POOLS)
  // Checks whether the address belongs to either regular or BRP pool.
  // Returns false for nullptr.
  PA_ALWAYS_INLINE static bool IsInCorePools(uintptr_t address) {
#if PA_CONFIG(DYNAMICALLY_SELECT_POOL_SIZE)
    const uintptr_t core_pools_base_mask = setup_.core_pools_base_mask_;
#else
    // When PA_GLUE_CORE_POOLS is on, the BRP pool is placed at the end of the
    // regular pool, effectively forming one virtual pool of a twice bigger
    // size. Adjust the mask appropriately.
    constexpr uintptr_t core_pools_base_mask = kRegularPoolBaseMask << 1;
#endif  // PA_CONFIG(DYNAMICALLY_SELECT_POOL_SIZE)
    bool ret =
        (address & core_pools_base_mask) == setup_.regular_pool_base_address_;
    PA_DCHECK(ret == (IsInRegularPool(address) || IsInBRPPool(address)));
    return ret;
  }
#if PA_CONFIG(DYNAMICALLY_SELECT_POOL_SIZE)
  PA_ALWAYS_INLINE static size_t CorePoolsSize() {
    return RegularPoolSize() * 2;
  }
#else
  PA_ALWAYS_INLINE static constexpr size_t CorePoolsSize() {
    return RegularPoolSize() * 2;
  }
#endif  // PA_CONFIG(DYNAMICALLY_SELECT_POOL_SIZE)
#endif  // BUILDFLAG(GLUE_CORE_POOLS)

  PA_ALWAYS_INLINE static uintptr_t OffsetInBRPPool(uintptr_t address) {
    PA_DCHECK(IsInBRPPool(address));
    return address - setup_.brp_pool_base_address_;
  }

  // Returns false for nullptr.
  PA_ALWAYS_INLINE static bool IsInConfigurablePool(uintptr_t address) {
    return (address & setup_.configurable_pool_base_mask_) ==
           setup_.configurable_pool_base_address_;
  }

  PA_ALWAYS_INLINE static uintptr_t ConfigurablePoolBase() {
    return setup_.configurable_pool_base_address_;
  }

#if BUILDFLAG(ENABLE_THREAD_ISOLATION)
  // Returns false for nullptr.
  PA_ALWAYS_INLINE static bool IsInThreadIsolatedPool(uintptr_t address) {
    return (address & kThreadIsolatedPoolBaseMask) ==
           setup_.thread_isolated_pool_base_address_;
  }
#endif

#if PA_CONFIG(ENABLE_SHADOW_METADATA)
  PA_ALWAYS_INLINE static std::ptrdiff_t ShadowPoolOffset(pool_handle pool) {
    if (pool == kRegularPoolHandle) {
      return regular_pool_shadow_offset_;
    } else if (pool == kBRPPoolHandle) {
      return brp_pool_shadow_offset_;
    } else {
      // TODO(crbug.com/1362969): Add shadow for configurable pool as well.
      // Shadow is not created for ConfigurablePool for now, so this part should
      // be unreachable.
      PA_NOTREACHED();
    }
  }
#endif

  // PartitionAddressSpace is static_only class.
  PartitionAddressSpace() = delete;
  PartitionAddressSpace(const PartitionAddressSpace&) = delete;
  void* operator new(size_t) = delete;
  void* operator new(size_t, void*) = delete;

 private:
#if PA_CONFIG(DYNAMICALLY_SELECT_POOL_SIZE)
  PA_ALWAYS_INLINE static size_t RegularPoolSize();
  PA_ALWAYS_INLINE static size_t BRPPoolSize();
#else
  // The pool sizes should be as large as maximum whenever possible.
  PA_ALWAYS_INLINE static constexpr size_t RegularPoolSize() {
    return kRegularPoolSize;
  }
  PA_ALWAYS_INLINE static constexpr size_t BRPPoolSize() {
    return kBRPPoolSize;
  }
#endif  // PA_CONFIG(DYNAMICALLY_SELECT_POOL_SIZE)

#if BUILDFLAG(ENABLE_THREAD_ISOLATION)
  PA_ALWAYS_INLINE static constexpr size_t ThreadIsolatedPoolSize() {
    return kThreadIsolatedPoolSize;
  }
#endif

  // On 64-bit systems, PA allocates from several contiguous, mutually disjoint
  // pools. The BRP pool is where all allocations have a BRP ref-count, thus
  // pointers pointing there can use a BRP protection against UaF. Allocations
  // in the other pools don't have that.
  //
  // Pool sizes have to be the power of two. Each pool will be aligned at its
  // own size boundary.
  //
  // NOTE! The BRP pool must be preceded by an inaccessible region. This is to
  // prevent a pointer to the end of a non-BRP-pool allocation from falling into
  // the BRP pool, thus triggering BRP mechanism and likely crashing. This
  // "forbidden zone" can be as small as 1B, but it's simpler to just reserve an
  // allocation granularity unit.
  //
  // The ConfigurablePool is an optional Pool that can be created inside an
  // existing mapping provided by the embedder. This Pool can be used when
  // certain PA allocations must be located inside a given virtual address
  // region. One use case for this Pool is V8 Sandbox, which requires that
  // ArrayBuffers be located inside of it.
  static constexpr size_t kRegularPoolSize = kPoolMaxSize;
  static constexpr size_t kBRPPoolSize = kPoolMaxSize;
  static_assert(base::bits::IsPowerOfTwo(kRegularPoolSize));
  static_assert(base::bits::IsPowerOfTwo(kBRPPoolSize));
#if BUILDFLAG(ENABLE_THREAD_ISOLATION)
  static constexpr size_t kThreadIsolatedPoolSize = kGiB / 4;
  static_assert(base::bits::IsPowerOfTwo(kThreadIsolatedPoolSize));
#endif
  static constexpr size_t kConfigurablePoolMaxSize = kPoolMaxSize;
  static constexpr size_t kConfigurablePoolMinSize = 1 * kGiB;
  static_assert(kConfigurablePoolMinSize <= kConfigurablePoolMaxSize);
  static_assert(base::bits::IsPowerOfTwo(kConfigurablePoolMaxSize));
  static_assert(base::bits::IsPowerOfTwo(kConfigurablePoolMinSize));

#if BUILDFLAG(IS_IOS)

#if !PA_CONFIG(DYNAMICALLY_SELECT_POOL_SIZE)
#error iOS is only supported with a dynamically sized GigaCase.
#endif

  // We can't afford pool sizes as large as kPoolMaxSize in iOS EarlGrey tests,
  // since the test process cannot use an extended virtual address space (see
  // crbug.com/1250788).
  static constexpr size_t kRegularPoolSizeForIOSTestProcess = kGiB / 4;
  static constexpr size_t kBRPPoolSizeForIOSTestProcess = kGiB / 4;
  static_assert(kRegularPoolSizeForIOSTestProcess < kRegularPoolSize);
  static_assert(kBRPPoolSizeForIOSTestProcess < kBRPPoolSize);
  static_assert(base::bits::IsPowerOfTwo(kRegularPoolSizeForIOSTestProcess));
  static_assert(base::bits::IsPowerOfTwo(kBRPPoolSizeForIOSTestProcess));
#endif  // BUILDFLAG(IOS_IOS)

#if !PA_CONFIG(DYNAMICALLY_SELECT_POOL_SIZE)
  // Masks used to easy determine belonging to a pool.
  static constexpr uintptr_t kRegularPoolOffsetMask =
      static_cast<uintptr_t>(kRegularPoolSize) - 1;
  static constexpr uintptr_t kRegularPoolBaseMask = ~kRegularPoolOffsetMask;
  static constexpr uintptr_t kBRPPoolOffsetMask =
      static_cast<uintptr_t>(kBRPPoolSize) - 1;
  static constexpr uintptr_t kBRPPoolBaseMask = ~kBRPPoolOffsetMask;
#endif  // !PA_CONFIG(DYNAMICALLY_SELECT_POOL_SIZE)

#if BUILDFLAG(ENABLE_THREAD_ISOLATION)
  static constexpr uintptr_t kThreadIsolatedPoolOffsetMask =
      static_cast<uintptr_t>(kThreadIsolatedPoolSize) - 1;
  static constexpr uintptr_t kThreadIsolatedPoolBaseMask =
      ~kThreadIsolatedPoolOffsetMask;
#endif

  // This must be set to such a value that IsIn*Pool() always returns false when
  // the pool isn't initialized.
  static constexpr uintptr_t kUninitializedPoolBaseAddress =
      static_cast<uintptr_t>(-1);

  struct alignas(kPartitionCachelineSize) PA_THREAD_ISOLATED_ALIGN PoolSetup {
    // Before PartitionAddressSpace::Init(), no allocation are allocated from a
    // reserved address space. Therefore, set *_pool_base_address_ initially to
    // -1, so that PartitionAddressSpace::IsIn*Pool() always returns false.
    constexpr PoolSetup() = default;

    // Using a struct to enforce alignment and padding
    uintptr_t regular_pool_base_address_ = kUninitializedPoolBaseAddress;
    uintptr_t brp_pool_base_address_ = kUninitializedPoolBaseAddress;
    uintptr_t configurable_pool_base_address_ = kUninitializedPoolBaseAddress;
#if BUILDFLAG(ENABLE_THREAD_ISOLATION)
    uintptr_t thread_isolated_pool_base_address_ =
        kUninitializedPoolBaseAddress;
#endif
#if PA_CONFIG(DYNAMICALLY_SELECT_POOL_SIZE)
    uintptr_t regular_pool_base_mask_ = 0;
    uintptr_t brp_pool_base_mask_ = 0;
#if BUILDFLAG(GLUE_CORE_POOLS)
    uintptr_t core_pools_base_mask_ = 0;
#endif
#endif  // PA_CONFIG(DYNAMICALLY_SELECT_POOL_SIZE)
    uintptr_t configurable_pool_base_mask_ = 0;
#if BUILDFLAG(ENABLE_THREAD_ISOLATION)
    ThreadIsolationOption thread_isolation_;
#endif
  };
#if BUILDFLAG(ENABLE_THREAD_ISOLATION)
  static_assert(sizeof(PoolSetup) % SystemPageSize() == 0,
                "PoolSetup has to fill a page(s)");
#else
  static_assert(sizeof(PoolSetup) % kPartitionCachelineSize == 0,
                "PoolSetup has to fill a cacheline(s)");
#endif

  // See the comment describing the address layout above.
  //
  // These are write-once fields, frequently accessed thereafter. Make sure they
  // don't share a cacheline with other, potentially writeable data, through
  // alignment and padding.
  static PoolSetup setup_ PA_CONSTINIT;

#if PA_CONFIG(ENABLE_SHADOW_METADATA)
  static std::ptrdiff_t regular_pool_shadow_offset_;
  static std::ptrdiff_t brp_pool_shadow_offset_;
#endif

#if BUILDFLAG(ENABLE_THREAD_ISOLATION)
  // If we use thread isolation, we need to write-protect its metadata.
  // Allow the function to get access to the PoolSetup.
  friend void WriteProtectThreadIsolatedGlobals(ThreadIsolationOption);
#endif
};

PA_ALWAYS_INLINE PartitionAddressSpace::PoolInfo GetPoolInfo(
    uintptr_t address) {
  return PartitionAddressSpace::GetPoolInfo(address);
}

PA_ALWAYS_INLINE pool_handle GetPool(uintptr_t address) {
  return GetPoolInfo(address).handle;
}

PA_ALWAYS_INLINE uintptr_t OffsetInBRPPool(uintptr_t address) {
  return PartitionAddressSpace::OffsetInBRPPool(address);
}

#if PA_CONFIG(ENABLE_SHADOW_METADATA)
PA_ALWAYS_INLINE std::ptrdiff_t ShadowPoolOffset(pool_handle pool) {
  return PartitionAddressSpace::ShadowPoolOffset(pool);
}
#endif

}  // namespace internal

// Returns false for nullptr.
PA_ALWAYS_INLINE bool IsManagedByPartitionAlloc(uintptr_t address) {
  // When ENABLE_BACKUP_REF_PTR_SUPPORT is off, BRP pool isn't used.
#if !BUILDFLAG(ENABLE_BACKUP_REF_PTR_SUPPORT)
  PA_DCHECK(!internal::PartitionAddressSpace::IsInBRPPool(address));
#endif
  return internal::PartitionAddressSpace::IsInRegularPool(address)
#if BUILDFLAG(ENABLE_BACKUP_REF_PTR_SUPPORT)
         || internal::PartitionAddressSpace::IsInBRPPool(address)
#endif
#if BUILDFLAG(ENABLE_THREAD_ISOLATION)
         || internal::PartitionAddressSpace::IsInThreadIsolatedPool(address)
#endif
         || internal::PartitionAddressSpace::IsInConfigurablePool(address);
}

// Returns false for nullptr.
PA_ALWAYS_INLINE bool IsManagedByPartitionAllocRegularPool(uintptr_t address) {
  return internal::PartitionAddressSpace::IsInRegularPool(address);
}

// Returns false for nullptr.
PA_ALWAYS_INLINE bool IsManagedByPartitionAllocBRPPool(uintptr_t address) {
  return internal::PartitionAddressSpace::IsInBRPPool(address);
}

#if BUILDFLAG(GLUE_CORE_POOLS)
// Checks whether the address belongs to either regular or BRP pool.
// Returns false for nullptr.
PA_ALWAYS_INLINE bool IsManagedByPartitionAllocCorePools(uintptr_t address) {
  return internal::PartitionAddressSpace::IsInCorePools(address);
}
#endif  // BUILDFLAG(GLUE_CORE_POOLS)

// Returns false for nullptr.
PA_ALWAYS_INLINE bool IsManagedByPartitionAllocConfigurablePool(
    uintptr_t address) {
  return internal::PartitionAddressSpace::IsInConfigurablePool(address);
}

#if BUILDFLAG(ENABLE_THREAD_ISOLATION)
// Returns false for nullptr.
PA_ALWAYS_INLINE bool IsManagedByPartitionAllocThreadIsolatedPool(
    uintptr_t address) {
  return internal::PartitionAddressSpace::IsInThreadIsolatedPool(address);
}
#endif

PA_ALWAYS_INLINE bool IsConfigurablePoolAvailable() {
  return internal::PartitionAddressSpace::IsConfigurablePoolInitialized();
}

}  // namespace partition_alloc

#endif  // BUILDFLAG(HAS_64_BIT_POINTERS)

#endif  // BASE_ALLOCATOR_PARTITION_ALLOCATOR_SRC_PARTITION_ALLOC_PARTITION_ADDRESS_SPACE_H_
