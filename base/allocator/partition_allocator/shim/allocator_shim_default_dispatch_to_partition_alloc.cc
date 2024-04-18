// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/allocator/partition_allocator/shim/allocator_shim_default_dispatch_to_partition_alloc.h"

#include <atomic>
#include <cstddef>
#include <map>
#include <string>
#include <tuple>

#include "base/allocator/partition_alloc_features.h"
#include "base/allocator/partition_allocator/allocation_guard.h"
#include "base/allocator/partition_allocator/memory_reclaimer.h"
#include "base/allocator/partition_allocator/partition_alloc.h"
#include "base/allocator/partition_allocator/partition_alloc_base/bits.h"
#include "base/allocator/partition_allocator/partition_alloc_base/compiler_specific.h"
#include "base/allocator/partition_allocator/partition_alloc_base/no_destructor.h"
#include "base/allocator/partition_allocator/partition_alloc_base/numerics/checked_math.h"
#include "base/allocator/partition_allocator/partition_alloc_base/numerics/safe_conversions.h"
#include "base/allocator/partition_allocator/partition_alloc_base/threading/platform_thread.h"
#include "base/allocator/partition_allocator/partition_alloc_buildflags.h"
#include "base/allocator/partition_allocator/partition_alloc_check.h"
#include "base/allocator/partition_allocator/partition_alloc_constants.h"
#include "base/allocator/partition_allocator/partition_root.h"
#include "base/allocator/partition_allocator/partition_stats.h"
#include "base/allocator/partition_allocator/shim/allocator_shim_internals.h"
#include "base/memory/nonscannable_memory.h"
#include "base/threading/platform_thread.h"
#include "build/build_config.h"
#include "build/chromecast_buildflags.h"

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
#include <malloc.h>
#endif

using allocator_shim::AllocatorDispatch;

namespace {

class SimpleScopedSpinLocker {
 public:
  explicit SimpleScopedSpinLocker(std::atomic<bool>& lock) : lock_(lock) {
    // Lock. Semantically equivalent to base::Lock::Acquire().
    bool expected = false;
    // Weak CAS since we are in a retry loop, relaxed ordering for failure since
    // in this case we don't imply any ordering.
    //
    // This matches partition_allocator/spinning_mutex.h fast path on Linux.
    while (!lock_.compare_exchange_weak(
        expected, true, std::memory_order_acquire, std::memory_order_relaxed)) {
      expected = false;
    }
  }

  ~SimpleScopedSpinLocker() { lock_.store(false, std::memory_order_release); }

 private:
  std::atomic<bool>& lock_;
};

// We can't use a "static local" or a base::LazyInstance, as:
// - static local variables call into the runtime on Windows, which is not
//   prepared to handle it, as the first allocation happens during CRT init.
// - We don't want to depend on base::LazyInstance, which may be converted to
//   static locals one day.
//
// Nevertheless, this provides essentially the same thing.
template <typename T, typename Constructor>
class LeakySingleton {
 public:
  constexpr LeakySingleton() = default;

  PA_ALWAYS_INLINE T* Get() {
    auto* instance = instance_.load(std::memory_order_acquire);
    if (PA_LIKELY(instance))
      return instance;

    return GetSlowPath();
  }

  // Replaces the instance pointer with a new one.
  void Replace(T* new_instance) {
    SimpleScopedSpinLocker scoped_lock{initialization_lock_};

    // Modify under the lock to avoid race between |if (instance)| and
    // |instance_.store()| in GetSlowPath().
    instance_.store(new_instance, std::memory_order_release);
  }

 private:
  T* GetSlowPath();

  std::atomic<T*> instance_;
  // Before C++20, having an initializer here causes a "variable does not have a
  // constant initializer" error.  In C++20, omitting it causes a similar error.
  // Presumably this is due to the C++20 changes to make atomic initialization
  // (of the other members of this class) sane, so guarding under that
  // feature-test.
#if !defined(__cpp_lib_atomic_value_initialization) || \
    __cpp_lib_atomic_value_initialization < 201911L
  alignas(T) uint8_t instance_buffer_[sizeof(T)];
#else
  alignas(T) uint8_t instance_buffer_[sizeof(T)] = {0};
#endif
  std::atomic<bool> initialization_lock_;
};

template <typename T, typename Constructor>
T* LeakySingleton<T, Constructor>::GetSlowPath() {
  // The instance has not been set, the proper way to proceed (correct
  // double-checked locking) is:
  //
  // auto* instance = instance_.load(std::memory_order_acquire);
  // if (!instance) {
  //   ScopedLock initialization_lock;
  //   root = instance_.load(std::memory_order_relaxed);
  //   if (root)
  //     return root;
  //   instance = Create new root;
  //   instance_.store(instance, std::memory_order_release);
  //   return instance;
  // }
  //
  // However, we don't want to use a base::Lock here, so instead we use
  // compare-and-exchange on a lock variable, which provides the same
  // guarantees.
  SimpleScopedSpinLocker scoped_lock{initialization_lock_};

  T* instance = instance_.load(std::memory_order_relaxed);
  // Someone beat us.
  if (instance)
    return instance;

  instance = Constructor::New(reinterpret_cast<void*>(instance_buffer_));
  instance_.store(instance, std::memory_order_release);

  return instance;
}

class MainPartitionConstructor {
 public:
  static partition_alloc::ThreadSafePartitionRoot* New(void* buffer) {
    constexpr partition_alloc::PartitionOptions::ThreadCache thread_cache =
#if BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
        // Additional partitions may be created in ConfigurePartitions(). Since
        // only one partition can have thread cache enabled, postpone the
        // decision to turn the thread cache on until after that call.
        // TODO(bartekn): Enable it here by default, once the "split-only" mode
        // is no longer needed.
        partition_alloc::PartitionOptions::ThreadCache::kDisabled;
#else   // BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
        // Other tests, such as the ThreadCache tests create a thread cache,
        // and only one is supported at a time.
        partition_alloc::PartitionOptions::ThreadCache::kDisabled;
#endif  // BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
    auto* new_root = new (buffer) partition_alloc::ThreadSafePartitionRoot({
        partition_alloc::PartitionOptions::AlignedAlloc::kAllowed,
        thread_cache,
        partition_alloc::PartitionOptions::Quarantine::kAllowed,
        partition_alloc::PartitionOptions::Cookie::kAllowed,
        partition_alloc::PartitionOptions::BackupRefPtr::kDisabled,
        partition_alloc::PartitionOptions::BackupRefPtrZapping::kDisabled,
        partition_alloc::PartitionOptions::UseConfigurablePool::kNo,
    });

    return new_root;
  }
};

LeakySingleton<partition_alloc::ThreadSafePartitionRoot,
               MainPartitionConstructor>
    g_root PA_CONSTINIT = {};
partition_alloc::ThreadSafePartitionRoot* Allocator() {
  return g_root.Get();
}

// Original g_root_ if it was replaced by ConfigurePartitions().
std::atomic<partition_alloc::ThreadSafePartitionRoot*> g_original_root(nullptr);

class AlignedPartitionConstructor {
 public:
  static partition_alloc::ThreadSafePartitionRoot* New(void* buffer) {
    return g_root.Get();
  }
};

LeakySingleton<partition_alloc::ThreadSafePartitionRoot,
               AlignedPartitionConstructor>
    g_aligned_root PA_CONSTINIT = {};

partition_alloc::ThreadSafePartitionRoot* OriginalAllocator() {
  return g_original_root.load(std::memory_order_relaxed);
}

partition_alloc::ThreadSafePartitionRoot* AlignedAllocator() {
  return g_aligned_root.Get();
}

void* AllocateAlignedMemory(size_t alignment, size_t size) {
  // Memory returned by the regular allocator *always* respects |kAlignment|,
  // which is a power of two, and any valid alignment is also a power of two. So
  // we can directly fulfill these requests with the main allocator.
  //
  // This has several advantages:
  // - The thread cache is supported on the main partition
  // - Reduced fragmentation
  // - Better coverage for MiraclePtr variants requiring extras
  //
  // There are several call sites in Chromium where base::AlignedAlloc is called
  // with a small alignment. Some may be due to overly-careful code, some are
  // because the client code doesn't know the required alignment at compile
  // time.
  //
  // Note that all "AlignedFree()" variants (_aligned_free() on Windows for
  // instance) directly call PartitionFree(), so there is no risk of
  // mismatch. (see below the default_dispatch definition).
  if (alignment <= partition_alloc::internal::kAlignment) {
    // This is mandated by |posix_memalign()| and friends, so should never fire.
    PA_CHECK(partition_alloc::internal::base::bits::IsPowerOfTwo(alignment));
    // TODO(bartekn): See if the compiler optimizes branches down the stack on
    // Mac, where PartitionPageSize() isn't constexpr.
    return Allocator()->AllocWithFlagsNoHooks(
        0, size, partition_alloc::PartitionPageSize());
  }

  return AlignedAllocator()->AlignedAllocWithFlags(
      partition_alloc::AllocFlags::kNoHooks, alignment, size);
}

}  // namespace

namespace allocator_shim::internal {

namespace {
#if BUILDFLAG(IS_APPLE)
unsigned int g_alloc_flags = 0;
#else
constexpr unsigned int g_alloc_flags = 0;
#endif
}  // namespace

void PartitionAllocSetCallNewHandlerOnMallocFailure(bool value) {
#if BUILDFLAG(IS_APPLE)
  // We generally prefer to always crash rather than returning nullptr for
  // OOM. However, on some macOS releases, we have to locally allow it due to
  // weirdness in OS code. See https://crbug.com/654695 for details.
  //
  // Apple only since it's not needed elsewhere, and there is a performance
  // penalty.

  if (value)
    g_alloc_flags = 0;
  else
    g_alloc_flags = partition_alloc::AllocFlags::kReturnNull;
#endif
}

void* PartitionMalloc(const AllocatorDispatch*, size_t size, void* context) {
  partition_alloc::ScopedDisallowAllocations guard{};
  return Allocator()->AllocWithFlagsNoHooks(
      g_alloc_flags, size, partition_alloc::PartitionPageSize());
}

void* PartitionMallocUnchecked(const AllocatorDispatch*,
                               size_t size,
                               void* context) {
  partition_alloc::ScopedDisallowAllocations guard{};
  return Allocator()->AllocWithFlagsNoHooks(
      partition_alloc::AllocFlags::kReturnNull | g_alloc_flags, size,
      partition_alloc::PartitionPageSize());
}

void* PartitionCalloc(const AllocatorDispatch*,
                      size_t n,
                      size_t size,
                      void* context) {
  partition_alloc::ScopedDisallowAllocations guard{};
  const size_t total =
      partition_alloc::internal::base::CheckMul(n, size).ValueOrDie();
  return Allocator()->AllocWithFlagsNoHooks(
      partition_alloc::AllocFlags::kZeroFill | g_alloc_flags, total,
      partition_alloc::PartitionPageSize());
}

void* PartitionMemalign(const AllocatorDispatch*,
                        size_t alignment,
                        size_t size,
                        void* context) {
  partition_alloc::ScopedDisallowAllocations guard{};
  return AllocateAlignedMemory(alignment, size);
}

void* PartitionAlignedAlloc(const AllocatorDispatch* dispatch,
                            size_t size,
                            size_t alignment,
                            void* context) {
  partition_alloc::ScopedDisallowAllocations guard{};
  return AllocateAlignedMemory(alignment, size);
}

// aligned_realloc documentation is
// https://docs.microsoft.com/ja-jp/cpp/c-runtime-library/reference/aligned-realloc
// TODO(tasak): Expand the given memory block to the given size if possible.
// This realloc always free the original memory block and allocates a new memory
// block.
// TODO(tasak): Implement PartitionRoot<thread_safe>::AlignedReallocWithFlags
// and use it.
void* PartitionAlignedRealloc(const AllocatorDispatch* dispatch,
                              void* address,
                              size_t size,
                              size_t alignment,
                              void* context) {
  partition_alloc::ScopedDisallowAllocations guard{};
  void* new_ptr = nullptr;
  if (size > 0) {
    new_ptr = AllocateAlignedMemory(alignment, size);
  } else {
    // size == 0 and address != null means just "free(address)".
    if (address)
      partition_alloc::ThreadSafePartitionRoot::FreeNoHooks(address);
  }
  // The original memory block (specified by address) is unchanged if ENOMEM.
  if (!new_ptr)
    return nullptr;
  // TODO(tasak): Need to compare the new alignment with the address' alignment.
  // If the two alignments are not the same, need to return nullptr with EINVAL.
  if (address) {
    size_t usage =
        partition_alloc::ThreadSafePartitionRoot::GetUsableSize(address);
    size_t copy_size = usage > size ? size : usage;
    memcpy(new_ptr, address, copy_size);

    partition_alloc::ThreadSafePartitionRoot::FreeNoHooks(address);
  }
  return new_ptr;
}

void* PartitionRealloc(const AllocatorDispatch*,
                       void* address,
                       size_t size,
                       void* context) {
  partition_alloc::ScopedDisallowAllocations guard{};
#if BUILDFLAG(IS_APPLE)
  if (PA_UNLIKELY(!partition_alloc::IsManagedByPartitionAlloc(
                      reinterpret_cast<uintptr_t>(address)) &&
                  address)) {
    // A memory region allocated by the system allocator is passed in this
    // function.  Forward the request to `realloc` which supports zone-
    // dispatching so that it appropriately selects the right zone.
    return realloc(address, size);
  }
#endif  // BUILDFLAG(IS_APPLE)

  return Allocator()->ReallocWithFlags(
      partition_alloc::AllocFlags::kNoHooks | g_alloc_flags, address, size, "");
}

#if BUILDFLAG(IS_CAST_ANDROID)
extern "C" {
void __real_free(void*);
}  // extern "C"
#endif  // BUILDFLAG(IS_CAST_ANDROID)

void PartitionFree(const AllocatorDispatch*, void* object, void* context) {
  partition_alloc::ScopedDisallowAllocations guard{};
#if BUILDFLAG(IS_APPLE)
  // TODO(bartekn): Add MTE unmasking here (and below).
  if (PA_UNLIKELY(!partition_alloc::IsManagedByPartitionAlloc(
                      reinterpret_cast<uintptr_t>(object)) &&
                  object)) {
    // A memory region allocated by the system allocator is passed in this
    // function.  Forward the request to `free` which supports zone-
    // dispatching so that it appropriately selects the right zone.
    return free(object);
  }
#endif  // BUILDFLAG(IS_APPLE)

  // On Android Chromecast devices, there is at least one case where a system
  // malloc() pointer can be passed to PartitionAlloc's free(). If we don't own
  // the pointer, pass it along. This should not have a runtime cost vs regular
  // Android, since on Android we have a PA_CHECK() rather than the branch here.
#if BUILDFLAG(IS_CAST_ANDROID)
  if (PA_UNLIKELY(!partition_alloc::IsManagedByPartitionAlloc(
                      reinterpret_cast<uintptr_t>(object)) &&
                  object)) {
    // A memory region allocated by the system allocator is passed in this
    // function.  Forward the request to `free()`, which is `__real_free()`
    // here.
    return __real_free(object);
  }
#endif  // BUILDFLAG(IS_CAST_ANDROID)

  partition_alloc::ThreadSafePartitionRoot::FreeNoHooks(object);
}

#if BUILDFLAG(IS_APPLE)
// Normal free() path on Apple OSes:
// 1. size = GetSizeEstimate(ptr);
// 2. if (size) FreeDefiniteSize(ptr, size)
//
// So we don't need to re-check that the pointer is owned in Free(), and we
// can use the size.
void PartitionFreeDefiniteSize(const AllocatorDispatch*,
                               void* address,
                               size_t size,
                               void* context) {
  partition_alloc::ScopedDisallowAllocations guard{};
  // TODO(lizeb): Optimize PartitionAlloc to use the size information. This is
  // still useful though, as we avoid double-checking that the address is owned.
  partition_alloc::ThreadSafePartitionRoot::FreeNoHooks(address);
}
#endif  // BUILDFLAG(IS_APPLE)

size_t PartitionGetSizeEstimate(const AllocatorDispatch*,
                                void* address,
                                void* context) {
  // This is used to implement malloc_usable_size(3). Per its man page, "if ptr
  // is NULL, 0 is returned".
  if (!address)
    return 0;

#if BUILDFLAG(IS_APPLE)
  if (!partition_alloc::IsManagedByPartitionAlloc(
          reinterpret_cast<uintptr_t>(address))) {
    // The object pointed to by `address` is not allocated by the
    // PartitionAlloc.  The return value `0` means that the pointer does not
    // belong to this malloc zone.
    return 0;
  }
#endif  // BUILDFLAG(IS_APPLE)

  // TODO(lizeb): Returns incorrect values for aligned allocations.
  const size_t size = partition_alloc::ThreadSafePartitionRoot::
      GetUsableSizeWithMac11MallocSizeHack(address);
#if BUILDFLAG(IS_APPLE)
  // The object pointed to by `address` is allocated by the PartitionAlloc.
  // So, this function must not return zero so that the malloc zone dispatcher
  // finds the appropriate malloc zone.
  PA_DCHECK(size);
#endif  // BUILDFLAG(IS_APPLE)
  return size;
}

#if BUILDFLAG(IS_APPLE)
bool PartitionClaimedAddress(const AllocatorDispatch*,
                             void* address,
                             void* context) {
  return partition_alloc::IsManagedByPartitionAlloc(
      reinterpret_cast<uintptr_t>(address));
}
#endif  // BUILDFLAG(IS_APPLE)

unsigned PartitionBatchMalloc(const AllocatorDispatch*,
                              size_t size,
                              void** results,
                              unsigned num_requested,
                              void* context) {
  // No real batching: we could only acquire the lock once for instance, keep it
  // simple for now.
  for (unsigned i = 0; i < num_requested; i++) {
    // No need to check the results, we crash if it fails.
    results[i] = PartitionMalloc(nullptr, size, nullptr);
  }

  // Either all succeeded, or we crashed.
  return num_requested;
}

void PartitionBatchFree(const AllocatorDispatch*,
                        void** to_be_freed,
                        unsigned num_to_be_freed,
                        void* context) {
  // No real batching: we could only acquire the lock once for instance, keep it
  // simple for now.
  for (unsigned i = 0; i < num_to_be_freed; i++) {
    PartitionFree(nullptr, to_be_freed[i], nullptr);
  }
}

#if BUILDFLAG(IS_APPLE)
void PartitionTryFreeDefault(const AllocatorDispatch*,
                             void* address,
                             void* context) {
  partition_alloc::ScopedDisallowAllocations guard{};

  if (UNLIKELY(!partition_alloc::IsManagedByPartitionAlloc(
          reinterpret_cast<uintptr_t>(address)))) {
    // The object pointed to by `address` is not allocated by the
    // PartitionAlloc. Call find_zone_and_free.
    return allocator_shim::TryFreeDefaultFallbackToFindZoneAndFree(address);
  }

  partition_alloc::ThreadSafePartitionRoot::FreeNoHooks(address);
}
#endif  // BUILDFLAG(IS_APPLE)

// static
partition_alloc::ThreadSafePartitionRoot* PartitionAllocMalloc::Allocator() {
  return ::Allocator();
}

// static
partition_alloc::ThreadSafePartitionRoot*
PartitionAllocMalloc::OriginalAllocator() {
  return ::OriginalAllocator();
}

// static
partition_alloc::ThreadSafePartitionRoot*
PartitionAllocMalloc::AlignedAllocator() {
  return ::AlignedAllocator();
}

}  // namespace allocator_shim::internal

#if BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)

namespace allocator_shim {

void EnablePartitionAllocMemoryReclaimer() {
  // Unlike other partitions, Allocator() and AlignedAllocator() do not register
  // their PartitionRoots to the memory reclaimer, because doing so may allocate
  // memory. Thus, the registration to the memory reclaimer has to be done
  // some time later, when the main root is fully configured.
  // TODO(bartekn): Aligned allocator can use the regular initialization path.
  ::partition_alloc::MemoryReclaimer::Instance()->RegisterPartition(
      Allocator());
  auto* original_root = OriginalAllocator();
  if (original_root)
    ::partition_alloc::MemoryReclaimer::Instance()->RegisterPartition(
        original_root);
  if (AlignedAllocator() != Allocator()) {
    ::partition_alloc::MemoryReclaimer::Instance()->RegisterPartition(
        AlignedAllocator());
  }
}

void ConfigurePartitions(
    EnableBrp enable_brp,
    EnableBrpZapping enable_brp_zapping,
    EnableBrpPartitionMemoryReclaimer enable_brp_memory_reclaimer,
    SplitMainPartition split_main_partition,
    UseDedicatedAlignedPartition use_dedicated_aligned_partition,
    AddDummyRefCount add_dummy_ref_count,
    AlternateBucketDistribution use_alternate_bucket_distribution) {
  // BRP cannot be enabled without splitting the main partition. Furthermore, in
  // the "before allocation" mode, it can't be enabled without further splitting
  // out the aligned partition.
  PA_CHECK(!enable_brp || split_main_partition);
#if !BUILDFLAG(PUT_REF_COUNT_IN_PREVIOUS_SLOT)
  PA_CHECK(!enable_brp || use_dedicated_aligned_partition);
#endif
  // Can't split out the aligned partition, without splitting the main one.
  PA_CHECK(!use_dedicated_aligned_partition || split_main_partition);

  static bool configured = false;
  PA_CHECK(!configured);
  configured = true;

  // Calling Get() is actually important, even if the return values weren't
  // used, because it has a side effect of initializing the variables, if they
  // weren't already.
  auto* current_root = g_root.Get();
  auto* current_aligned_root = g_aligned_root.Get();

  if (!split_main_partition) {
    switch (use_alternate_bucket_distribution) {
      case AlternateBucketDistribution::kDefault:
        // We start in the 'default' case.
        break;
      case AlternateBucketDistribution::kDenser:
        current_root->SwitchToDenserBucketDistribution();
        current_aligned_root->SwitchToDenserBucketDistribution();
        break;
    }
    PA_DCHECK(!enable_brp);
    PA_DCHECK(!use_dedicated_aligned_partition);
    PA_DCHECK(!current_root->flags.with_thread_cache);
    return;
  }

  // We've been bitten before by using a static local when initializing a
  // partition. For synchronization, static local variables call into the
  // runtime on Windows, which may not be ready to handle it, if the path is
  // invoked on an allocation during the runtime initialization.
  // ConfigurePartitions() is invoked explicitly from Chromium code, so this
  // shouldn't bite us here. Mentioning just in case we move this code earlier.
  static partition_alloc::internal::base::NoDestructor<
      partition_alloc::ThreadSafePartitionRoot>
      new_main_partition(partition_alloc::PartitionOptions(
          !use_dedicated_aligned_partition
              ? partition_alloc::PartitionOptions::AlignedAlloc::kAllowed
              : partition_alloc::PartitionOptions::AlignedAlloc::kDisallowed,
          partition_alloc::PartitionOptions::ThreadCache::kDisabled,
          partition_alloc::PartitionOptions::Quarantine::kAllowed,
          partition_alloc::PartitionOptions::Cookie::kAllowed,
          enable_brp
              ? partition_alloc::PartitionOptions::BackupRefPtr::kEnabled
              : partition_alloc::PartitionOptions::BackupRefPtr::kDisabled,
          enable_brp_zapping
              ? partition_alloc::PartitionOptions::BackupRefPtrZapping::kEnabled
              : partition_alloc::PartitionOptions::BackupRefPtrZapping::
                    kDisabled,
          partition_alloc::PartitionOptions::UseConfigurablePool::kNo,
          add_dummy_ref_count
              ? partition_alloc::PartitionOptions::AddDummyRefCount::kEnabled
              : partition_alloc::PartitionOptions::AddDummyRefCount::
                    kDisabled));
  partition_alloc::ThreadSafePartitionRoot* new_root = new_main_partition.get();

  partition_alloc::ThreadSafePartitionRoot* new_aligned_root;
  if (use_dedicated_aligned_partition) {
    // TODO(bartekn): Use the original root instead of creating a new one. It'd
    // result in one less partition, but come at a cost of commingling types.
    static partition_alloc::internal::base::NoDestructor<
        partition_alloc::ThreadSafePartitionRoot>
        new_aligned_partition(partition_alloc::PartitionOptions{
            partition_alloc::PartitionOptions::AlignedAlloc::kAllowed,
            partition_alloc::PartitionOptions::ThreadCache::kDisabled,
            partition_alloc::PartitionOptions::Quarantine::kAllowed,
            partition_alloc::PartitionOptions::Cookie::kAllowed,
            partition_alloc::PartitionOptions::BackupRefPtr::kDisabled,
            partition_alloc::PartitionOptions::BackupRefPtrZapping::kDisabled,
            partition_alloc::PartitionOptions::UseConfigurablePool::kNo,
        });
    new_aligned_root = new_aligned_partition.get();
  } else {
    // The new main root can also support AlignedAlloc.
    new_aligned_root = new_root;
  }

  // Now switch traffic to the new partitions.
  g_aligned_root.Replace(new_aligned_root);
  g_root.Replace(new_root);

  // g_original_root has to be set after g_root, because other code doesn't
  // handle well both pointing to the same root.
  // TODO(bartekn): Reorder, once handled well. It isn't ideal for one
  // partition to be invisible temporarily.
  g_original_root = current_root;

  // No need for g_original_aligned_root, because in cases where g_aligned_root
  // is replaced, it must've been g_original_root.
  PA_CHECK(current_aligned_root == g_original_root);

  if (enable_brp_memory_reclaimer) {
    partition_alloc::MemoryReclaimer::Instance()->RegisterPartition(new_root);
    if (new_aligned_root != new_root) {
      partition_alloc::MemoryReclaimer::Instance()->RegisterPartition(
          new_aligned_root);
    }
  }

  // Purge memory, now that the traffic to the original partition is cut off.
  current_root->PurgeMemory(
      partition_alloc::PurgeFlags::kDecommitEmptySlotSpans |
      partition_alloc::PurgeFlags::kDiscardUnusedSystemPages);

  switch (use_alternate_bucket_distribution) {
    case AlternateBucketDistribution::kDefault:
      // We start in the 'default' case.
      break;
    case AlternateBucketDistribution::kDenser:
      g_root.Get()->SwitchToDenserBucketDistribution();
      g_aligned_root.Get()->SwitchToDenserBucketDistribution();
      break;
  }
}

#if BUILDFLAG(USE_STARSCAN)
void EnablePCScan(partition_alloc::internal::PCScan::InitConfig config) {
  partition_alloc::internal::base::PlatformThread::SetThreadNameHook(
      &::base::PlatformThread::SetName);
  partition_alloc::internal::PCScan::Initialize(config);

  partition_alloc::internal::PCScan::RegisterScannableRoot(Allocator());
  if (OriginalAllocator() != nullptr)
    partition_alloc::internal::PCScan::RegisterScannableRoot(
        OriginalAllocator());
  if (Allocator() != AlignedAllocator())
    partition_alloc::internal::PCScan::RegisterScannableRoot(
        AlignedAllocator());

  base::internal::NonScannableAllocator::Instance().NotifyPCScanEnabled();
  base::internal::NonQuarantinableAllocator::Instance().NotifyPCScanEnabled();
}
#endif  // BUILDFLAG(USE_STARSCAN)
}  // namespace allocator_shim

const AllocatorDispatch AllocatorDispatch::default_dispatch = {
    &allocator_shim::internal::PartitionMalloc,  // alloc_function
    &allocator_shim::internal::
        PartitionMallocUnchecked,  // alloc_unchecked_function
    &allocator_shim::internal::
        PartitionCalloc,  // alloc_zero_initialized_function
    &allocator_shim::internal::PartitionMemalign,  // alloc_aligned_function
    &allocator_shim::internal::PartitionRealloc,   // realloc_function
    &allocator_shim::internal::PartitionFree,      // free_function
    &allocator_shim::internal::
        PartitionGetSizeEstimate,  // get_size_estimate_function
#if BUILDFLAG(IS_APPLE)
    &allocator_shim::internal::PartitionClaimedAddress,  // claimed_address
#else
    nullptr,  // claimed_address
#endif
    &allocator_shim::internal::PartitionBatchMalloc,  // batch_malloc_function
    &allocator_shim::internal::PartitionBatchFree,    // batch_free_function
#if BUILDFLAG(IS_APPLE)
    // On Apple OSes, free_definite_size() is always called from free(), since
    // get_size_estimate() is used to determine whether an allocation belongs to
    // the current zone. It makes sense to optimize for it.
    &allocator_shim::internal::PartitionFreeDefiniteSize,
    // On Apple OSes, try_free_default() is sometimes called as an optimization
    // of free().
    &allocator_shim::internal::PartitionTryFreeDefault,
#else
    nullptr,  // free_definite_size_function
    nullptr,  // try_free_default_function
#endif
    &allocator_shim::internal::
        PartitionAlignedAlloc,  // aligned_malloc_function
    &allocator_shim::internal::
        PartitionAlignedRealloc,               // aligned_realloc_function
    &allocator_shim::internal::PartitionFree,  // aligned_free_function
    nullptr,                                   // next
};

// Intercept diagnostics symbols as well, even though they are not part of the
// unified shim layer.
//
// TODO(lizeb): Implement the ones that doable.

extern "C" {

#if !BUILDFLAG(IS_APPLE) && !BUILDFLAG(IS_ANDROID)

SHIM_ALWAYS_EXPORT void malloc_stats(void) __THROW {}

SHIM_ALWAYS_EXPORT int mallopt(int cmd, int value) __THROW {
  return 0;
}

#endif  // !BUILDFLAG(IS_APPLE) && !BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
SHIM_ALWAYS_EXPORT struct mallinfo mallinfo(void) __THROW {
  partition_alloc::SimplePartitionStatsDumper allocator_dumper;
  Allocator()->DumpStats("malloc", true, &allocator_dumper);
  // TODO(bartekn): Dump OriginalAllocator() into "malloc" as well.

  partition_alloc::SimplePartitionStatsDumper aligned_allocator_dumper;
  if (AlignedAllocator() != Allocator()) {
    AlignedAllocator()->DumpStats("posix_memalign", true,
                                  &aligned_allocator_dumper);
  }

  // Dump stats for nonscannable and nonquarantinable allocators.
  auto& nonscannable_allocator =
      base::internal::NonScannableAllocator::Instance();
  partition_alloc::SimplePartitionStatsDumper nonscannable_allocator_dumper;
  if (auto* nonscannable_root = nonscannable_allocator.root())
    nonscannable_root->DumpStats("malloc", true,
                                 &nonscannable_allocator_dumper);
  auto& nonquarantinable_allocator =
      base::internal::NonQuarantinableAllocator::Instance();
  partition_alloc::SimplePartitionStatsDumper nonquarantinable_allocator_dumper;
  if (auto* nonquarantinable_root = nonquarantinable_allocator.root())
    nonquarantinable_root->DumpStats("malloc", true,
                                     &nonquarantinable_allocator_dumper);

  struct mallinfo info = {0};
  info.arena = 0;  // Memory *not* allocated with mmap().

  // Memory allocated with mmap(), aka virtual size.
  info.hblks =
      partition_alloc::internal::base::checked_cast<decltype(info.hblks)>(
          allocator_dumper.stats().total_mmapped_bytes +
          aligned_allocator_dumper.stats().total_mmapped_bytes +
          nonscannable_allocator_dumper.stats().total_mmapped_bytes +
          nonquarantinable_allocator_dumper.stats().total_mmapped_bytes);
  // Resident bytes.
  info.hblkhd =
      partition_alloc::internal::base::checked_cast<decltype(info.hblkhd)>(
          allocator_dumper.stats().total_resident_bytes +
          aligned_allocator_dumper.stats().total_resident_bytes +
          nonscannable_allocator_dumper.stats().total_resident_bytes +
          nonquarantinable_allocator_dumper.stats().total_resident_bytes);
  // Allocated bytes.
  info.uordblks =
      partition_alloc::internal::base::checked_cast<decltype(info.uordblks)>(
          allocator_dumper.stats().total_active_bytes +
          aligned_allocator_dumper.stats().total_active_bytes +
          nonscannable_allocator_dumper.stats().total_active_bytes +
          nonquarantinable_allocator_dumper.stats().total_active_bytes);

  return info;
}
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)

}  // extern "C"

#if BUILDFLAG(IS_APPLE)

namespace allocator_shim {

void InitializeDefaultAllocatorPartitionRoot() {
  // On OS_APPLE, the initialization of PartitionRoot uses memory allocations
  // internally, e.g. __builtin_available, and it's not easy to avoid it.
  // Thus, we initialize the PartitionRoot with using the system default
  // allocator before we intercept the system default allocator.
  std::ignore = Allocator();
}

}  // namespace allocator_shim

#endif  // BUILDFLAG(IS_APPLE)

#endif  // BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
