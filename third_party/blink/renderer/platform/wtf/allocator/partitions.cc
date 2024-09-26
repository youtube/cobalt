/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/platform/wtf/allocator/partitions.h"

#include "base/allocator/partition_alloc_features.h"
#include "base/allocator/partition_alloc_support.h"
#include "base/allocator/partition_allocator/memory_reclaimer.h"
#include "base/allocator/partition_allocator/oom.h"
#include "base/allocator/partition_allocator/page_allocator.h"
#include "base/allocator/partition_allocator/partition_alloc.h"
#include "base/allocator/partition_allocator/partition_alloc_buildflags.h"
#include "base/allocator/partition_allocator/partition_root.h"
#include "base/debug/alias.h"
#include "base/feature_list.h"
#include "base/no_destructor.h"
#include "base/strings/safe_sprintf.h"
#include "base/task/sequenced_task_runner.h"
#include "base/thread_annotations.h"
#include "build/build_config.h"
#include "components/crash/core/common/crash_key.h"
#include "third_party/blink/renderer/platform/wtf/allocator/partition_allocator.h"
#include "third_party/blink/renderer/platform/wtf/wtf.h"

namespace WTF {

const char* const Partitions::kAllocatedObjectPoolName =
    "partition_alloc/allocated_objects";

#if BUILDFLAG(USE_STARSCAN)
// Runs PCScan on WTF partitions.
BASE_FEATURE(kPCScanBlinkPartitions,
             "PartitionAllocPCScanBlinkPartitions",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif

bool Partitions::initialized_ = false;
bool Partitions::scan_is_enabled_ = false;

// These statics are inlined, so cannot be LazyInstances. We create the values,
// and then set the pointers correctly in Initialize().
partition_alloc::ThreadSafePartitionRoot* Partitions::fast_malloc_root_ =
    nullptr;
partition_alloc::ThreadSafePartitionRoot* Partitions::array_buffer_root_ =
    nullptr;
partition_alloc::ThreadSafePartitionRoot* Partitions::buffer_root_ = nullptr;

// static
void Partitions::Initialize() {
  static bool initialized = InitializeOnce();
  DCHECK(initialized);
}

// static
bool Partitions::InitializeOnce() {
  base::features::BackupRefPtrMode brp_mode =
      base::features::kBackupRefPtrModeParam.Get();
#if BUILDFLAG(ENABLE_BACKUP_REF_PTR_SUPPORT)
  const bool process_affected_by_brp_flag =
      base::features::kBackupRefPtrEnabledProcessesParam.Get() ==
          base::features::BackupRefPtrEnabledProcesses::kAllProcesses ||
      base::features::kBackupRefPtrEnabledProcessesParam.Get() ==
          base::features::BackupRefPtrEnabledProcesses::kBrowserAndRenderer;
  const bool enable_brp =
      base::FeatureList::IsEnabled(
          base::features::kPartitionAllocBackupRefPtr) &&
      (brp_mode == base::features::BackupRefPtrMode::kEnabled ||
       brp_mode == base::features::BackupRefPtrMode::kEnabledWithoutZapping) &&
      process_affected_by_brp_flag;
#else  // BUILDFLAG(ENABLE_BACKUP_REF_PTR_SUPPORT)
  const bool process_affected_by_brp_flag = false;
  const bool enable_brp = false;
#endif
  const auto brp_setting =
      enable_brp ? partition_alloc::PartitionOptions::BackupRefPtr::kEnabled
                 : partition_alloc::PartitionOptions::BackupRefPtr::kDisabled;
  const auto brp_zapping_setting =
      enable_brp && brp_mode == base::features::BackupRefPtrMode::kEnabled
          ? partition_alloc::PartitionOptions::BackupRefPtrZapping::kEnabled
          : partition_alloc::PartitionOptions::BackupRefPtrZapping::kDisabled;
  const auto add_dummy_ref_count_setting =
      process_affected_by_brp_flag &&
              brp_mode ==
                  base::features::BackupRefPtrMode::kDisabledButAddDummyRefCount
          ? partition_alloc::PartitionOptions::AddDummyRefCount::kEnabled
          : partition_alloc::PartitionOptions::AddDummyRefCount::kDisabled;
  scan_is_enabled_ =
      !enable_brp &&
#if BUILDFLAG(USE_STARSCAN)
      (base::FeatureList::IsEnabled(base::features::kPartitionAllocPCScan) ||
       base::FeatureList::IsEnabled(kPCScanBlinkPartitions));
#else
      false;
#endif  // BUILDFLAG(USE_STARSCAN)

  // FastMalloc doesn't provide isolation, only a (hopefully fast) malloc().
  // When PartitionAlloc is already the malloc() implementation, there is
  // nothing to do.
  //
  // Note that we could keep the two heaps separate, but each PartitionAlloc's
  // root has a cost, both in used memory and in virtual address space. Don't
  // pay it when we don't have to.
  //
  // In addition, enable the FastMalloc partition if
  // --enable-features=PartitionAllocPCScanBlinkPartitions is specified.
  if (scan_is_enabled_ || !BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)) {
    constexpr partition_alloc::PartitionOptions::ThreadCache thread_cache =
#if BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
        partition_alloc::PartitionOptions::ThreadCache::kDisabled;
#else
        partition_alloc::PartitionOptions::ThreadCache::kEnabled;
#endif
    static base::NoDestructor<partition_alloc::PartitionAllocator>
        fast_malloc_allocator{};
    fast_malloc_allocator->init({
        partition_alloc::PartitionOptions::AlignedAlloc::kDisallowed,
        thread_cache,
        partition_alloc::PartitionOptions::Quarantine::kAllowed,
        partition_alloc::PartitionOptions::Cookie::kAllowed,
        brp_setting,
        brp_zapping_setting,
        partition_alloc::PartitionOptions::UseConfigurablePool::kNo,
        add_dummy_ref_count_setting,
    });
    fast_malloc_root_ = fast_malloc_allocator->root();
  }

  partition_alloc::PartitionAllocGlobalInit(&Partitions::HandleOutOfMemory);

  static base::NoDestructor<partition_alloc::PartitionAllocator>
      buffer_allocator{};
  buffer_allocator->init({
      partition_alloc::PartitionOptions::AlignedAlloc::kDisallowed,
      partition_alloc::PartitionOptions::ThreadCache::kDisabled,
      partition_alloc::PartitionOptions::Quarantine::kAllowed,
      partition_alloc::PartitionOptions::Cookie::kAllowed,
      brp_setting,
      brp_zapping_setting,
      partition_alloc::PartitionOptions::UseConfigurablePool::kNo,
      add_dummy_ref_count_setting,
  });
  buffer_root_ = buffer_allocator->root();

#if BUILDFLAG(USE_STARSCAN)
  if (scan_is_enabled_) {
    if (!partition_alloc::internal::PCScan::IsInitialized()) {
      partition_alloc::internal::PCScan::Initialize(
          {partition_alloc::internal::PCScan::InitConfig::
               WantedWriteProtectionMode::kDisabled,
           partition_alloc::internal::PCScan::InitConfig::SafepointMode::
               kDisabled});
    }
    partition_alloc::internal::PCScan::RegisterScannableRoot(fast_malloc_root_);
    // Ignore other partitions for now.
  }
#endif  // BUILDFLAG(USE_STARSCAN)

  if (!base::FeatureList::IsEnabled(
          base::features::kPartitionAllocUseAlternateDistribution)) {
#if !BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
    fast_malloc_root_->SwitchToDenserBucketDistribution();
#endif
    buffer_root_->SwitchToDenserBucketDistribution();
  }

  initialized_ = true;
  return initialized_;
}

// static
void Partitions::InitializeArrayBufferPartition() {
  CHECK(initialized_);
  CHECK(!ArrayBufferPartitionInitialized());

  static base::NoDestructor<partition_alloc::PartitionAllocator>
      array_buffer_allocator{};

  // BackupRefPtr disallowed because it will prevent allocations from being 16B
  // aligned as required by ArrayBufferContents.
  array_buffer_allocator->init({
      partition_alloc::PartitionOptions::AlignedAlloc::kDisallowed,
      partition_alloc::PartitionOptions::ThreadCache::kDisabled,
      partition_alloc::PartitionOptions::Quarantine::kAllowed,
      partition_alloc::PartitionOptions::Cookie::kAllowed,
      partition_alloc::PartitionOptions::BackupRefPtr::kDisabled,
      partition_alloc::PartitionOptions::BackupRefPtrZapping::kDisabled,
      // When the V8 virtual memory cage is enabled, the ArrayBuffer partition
      // must be placed inside of it. For that, PA's ConfigurablePool is
      // created inside the V8 Cage during initialization. As such, here all we
      // need to do is indicate that we'd like to use that Pool if it has been
      // created by now (if it hasn't been created, the cage isn't enabled, and
      // so we'll use the default Pool).
      partition_alloc::PartitionOptions::UseConfigurablePool::kIfAvailable,
  });

  array_buffer_root_ = array_buffer_allocator->root();

#if BUILDFLAG(USE_STARSCAN)
  // PCScan relies on the fact that quarantinable allocations go to PA's
  // regular pool. This is not the case if configurable pool is available.
  if (scan_is_enabled_ && !array_buffer_root_->uses_configurable_pool()) {
    partition_alloc::internal::PCScan::RegisterNonScannableRoot(
        array_buffer_root_);
  }
#endif  // BUILDFLAG(USE_STARSCAN)
  if (!base::FeatureList::IsEnabled(
          base::features::kPartitionAllocUseAlternateDistribution)) {
    array_buffer_root_->SwitchToDenserBucketDistribution();
  }
}

// static
void Partitions::StartPeriodicReclaim(
    scoped_refptr<base::SequencedTaskRunner> task_runner) {
  CHECK(IsMainThread());
  DCHECK(initialized_);

  base::allocator::StartMemoryReclaimer(task_runner);
}

// static
void Partitions::DumpMemoryStats(
    bool is_light_dump,
    partition_alloc::PartitionStatsDumper* partition_stats_dumper) {
  // Object model and rendering partitions are not thread safe and can be
  // accessed only on the main thread.
  DCHECK(IsMainThread());

  if (auto* fast_malloc_partition = FastMallocPartition()) {
    fast_malloc_partition->DumpStats("fast_malloc", is_light_dump,
                                     partition_stats_dumper);
  }
  if (ArrayBufferPartitionInitialized()) {
    ArrayBufferPartition()->DumpStats("array_buffer", is_light_dump,
                                      partition_stats_dumper);
  }
  BufferPartition()->DumpStats("buffer", is_light_dump, partition_stats_dumper);
}

namespace {

class LightPartitionStatsDumperImpl
    : public partition_alloc::PartitionStatsDumper {
 public:
  LightPartitionStatsDumperImpl() : total_active_bytes_(0) {}

  void PartitionDumpTotals(
      const char* partition_name,
      const partition_alloc::PartitionMemoryStats* memory_stats) override {
    total_active_bytes_ += memory_stats->total_active_bytes;
  }

  void PartitionsDumpBucketStats(
      const char* partition_name,
      const partition_alloc::PartitionBucketMemoryStats*) override {}

  size_t TotalActiveBytes() const { return total_active_bytes_; }

 private:
  size_t total_active_bytes_;
};

}  // namespace

// static
size_t Partitions::TotalSizeOfCommittedPages() {
  DCHECK(initialized_);
  size_t total_size = 0;
  // Racy reads below: this is fine to collect statistics.
  if (auto* fast_malloc_partition = FastMallocPartition()) {
    total_size +=
        TS_UNCHECKED_READ(fast_malloc_partition->total_size_of_committed_pages);
  }
  if (ArrayBufferPartitionInitialized()) {
    total_size += TS_UNCHECKED_READ(
        ArrayBufferPartition()->total_size_of_committed_pages);
  }
  total_size +=
      TS_UNCHECKED_READ(BufferPartition()->total_size_of_committed_pages);
  return total_size;
}

// static
size_t Partitions::TotalActiveBytes() {
  LightPartitionStatsDumperImpl dumper;
  WTF::Partitions::DumpMemoryStats(true, &dumper);
  return dumper.TotalActiveBytes();
}

NOINLINE static void PartitionsOutOfMemoryUsing2G(size_t size) {
  NO_CODE_FOLDING();
  size_t signature = 2UL * 1024 * 1024 * 1024;
  base::debug::Alias(&signature);
  OOM_CRASH(size);
}

NOINLINE static void PartitionsOutOfMemoryUsing1G(size_t size) {
  NO_CODE_FOLDING();
  size_t signature = 1UL * 1024 * 1024 * 1024;
  base::debug::Alias(&signature);
  OOM_CRASH(size);
}

NOINLINE static void PartitionsOutOfMemoryUsing512M(size_t size) {
  NO_CODE_FOLDING();
  size_t signature = 512 * 1024 * 1024;
  base::debug::Alias(&signature);
  OOM_CRASH(size);
}

NOINLINE static void PartitionsOutOfMemoryUsing256M(size_t size) {
  NO_CODE_FOLDING();
  size_t signature = 256 * 1024 * 1024;
  base::debug::Alias(&signature);
  OOM_CRASH(size);
}

NOINLINE static void PartitionsOutOfMemoryUsing128M(size_t size) {
  NO_CODE_FOLDING();
  size_t signature = 128 * 1024 * 1024;
  base::debug::Alias(&signature);
  OOM_CRASH(size);
}

NOINLINE static void PartitionsOutOfMemoryUsing64M(size_t size) {
  NO_CODE_FOLDING();
  size_t signature = 64 * 1024 * 1024;
  base::debug::Alias(&signature);
  OOM_CRASH(size);
}

NOINLINE static void PartitionsOutOfMemoryUsing32M(size_t size) {
  NO_CODE_FOLDING();
  size_t signature = 32 * 1024 * 1024;
  base::debug::Alias(&signature);
  OOM_CRASH(size);
}

NOINLINE static void PartitionsOutOfMemoryUsing16M(size_t size) {
  NO_CODE_FOLDING();
  size_t signature = 16 * 1024 * 1024;
  base::debug::Alias(&signature);
  OOM_CRASH(size);
}

NOINLINE static void PartitionsOutOfMemoryUsingLessThan16M(size_t size) {
  NO_CODE_FOLDING();
  size_t signature = 16 * 1024 * 1024 - 1;
  base::debug::Alias(&signature);
  OOM_CRASH(size);
}

// static
void* Partitions::BufferMalloc(size_t n, const char* type_name) {
  return BufferPartition()->Alloc(n, type_name);
}

// static
void* Partitions::BufferTryRealloc(void* p, size_t n, const char* type_name) {
  return BufferPartition()->TryRealloc(p, n, type_name);
}

// static
void Partitions::BufferFree(void* p) {
  BufferPartition()->Free(p);
}

// static
size_t Partitions::BufferPotentialCapacity(size_t n) {
  return BufferPartition()->AllocationCapacityFromRequestedSize(n);
}

// Ideally this would be removed when PartitionAlloc is malloc(), but there are
// quite a few callers. Just forward to the C functions instead.  Most of the
// usual callers will never reach here though, as USING_FAST_MALLOC() becomes a
// no-op.
// static
void* Partitions::FastMalloc(size_t n, const char* type_name) {
  auto* fast_malloc_partition = FastMallocPartition();
  if (UNLIKELY(fast_malloc_partition))
    return fast_malloc_partition->Alloc(n, type_name);
  else
    return malloc(n);
}

// static
void* Partitions::FastZeroedMalloc(size_t n, const char* type_name) {
  auto* fast_malloc_partition = FastMallocPartition();
  if (UNLIKELY(fast_malloc_partition)) {
    return fast_malloc_partition->AllocWithFlags(
        partition_alloc::AllocFlags::kZeroFill, n, type_name);
  } else {
    return calloc(n, 1);
  }
}

// static
void Partitions::FastFree(void* p) {
  auto* fast_malloc_partition = FastMallocPartition();
  if (UNLIKELY(fast_malloc_partition))
    fast_malloc_partition->Free(p);
  else
    free(p);
}

// static
void Partitions::HandleOutOfMemory(size_t size) {
  volatile size_t total_usage = TotalSizeOfCommittedPages();
  uint32_t alloc_page_error_code = partition_alloc::GetAllocPageErrorCode();
  base::debug::Alias(&alloc_page_error_code);

  // Report the total mapped size from PageAllocator. This is intended to
  // distinguish better between address space exhaustion and out of memory on 32
  // bit platforms. PartitionAlloc can use a lot of address space, as free pages
  // are not shared between buckets (see crbug.com/421387). There is already
  // reporting for this, however it only looks at the address space usage of a
  // single partition. This allows to look across all the partitions, and other
  // users such as V8.
  char value[24];
  // %d works for 64 bit types as well with SafeSPrintf(), see its unit tests
  // for an example.
  base::strings::SafeSPrintf(value, "%d",
                             partition_alloc::GetTotalMappedSize());
  static crash_reporter::CrashKeyString<24> g_page_allocator_mapped_size(
      "page-allocator-mapped-size");
  g_page_allocator_mapped_size.Set(value);

  if (total_usage >= 2UL * 1024 * 1024 * 1024)
    PartitionsOutOfMemoryUsing2G(size);
  if (total_usage >= 1UL * 1024 * 1024 * 1024)
    PartitionsOutOfMemoryUsing1G(size);
  if (total_usage >= 512 * 1024 * 1024)
    PartitionsOutOfMemoryUsing512M(size);
  if (total_usage >= 256 * 1024 * 1024)
    PartitionsOutOfMemoryUsing256M(size);
  if (total_usage >= 128 * 1024 * 1024)
    PartitionsOutOfMemoryUsing128M(size);
  if (total_usage >= 64 * 1024 * 1024)
    PartitionsOutOfMemoryUsing64M(size);
  if (total_usage >= 32 * 1024 * 1024)
    PartitionsOutOfMemoryUsing32M(size);
  if (total_usage >= 16 * 1024 * 1024)
    PartitionsOutOfMemoryUsing16M(size);
  PartitionsOutOfMemoryUsingLessThan16M(size);
}

}  // namespace WTF
