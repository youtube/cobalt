// Copyright 2026 The Cobalt Authors. All Rights Reserved.

#include "cobalt/memory/cobalt_memory_attribution_manager.h"

#include <stdlib.h>

#include <string>
#include <string_view>

#include "base/allocator/partition_allocator/src/partition_alloc/shim/allocator_shim_default_dispatch_to_partition_alloc.h"
#include "base/command_line.h"
#include "base/containers/span.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/power_monitor/power_monitor.h"
#include "base/strings/strcat.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/trace_event/memory_dump_manager.h"
#include "base/trace_event/process_memory_dump.h"
#include "cobalt/browser/features.h"

namespace cobalt {
namespace memory {

#if PA_BUILDFLAG(HAS_64_BIT_POINTERS)
std::atomic<uint8_t*> CobaltMemoryAttributionManager::shadow_map_tables_
    [partition_alloc::internal::kNumPools]
    [partition_alloc::internal::kMaxSuperPagesInPool];
#else
std::atomic<uint8_t*> CobaltMemoryAttributionManager::shadow_map_tables_
    [4ULL * 1024 * 1024 * 1024 / partition_alloc::internal::kSuperPageSize];
#endif

CobaltMemoryAttributionManager::ThreadLocalCounters::ThreadLocalCounters() {
  for (auto& atomic : bytes_allocated) {
    atomic.store(0, std::memory_order_relaxed);
  }
}

CobaltMemoryAttributionManager::ThreadLocalCounters::~ThreadLocalCounters() {
  if (is_registered) {
    CobaltMemoryAttributionManager::Get()->UnregisterThread(this);
  }
}

thread_local CobaltMemoryAttributionManager::ThreadLocalCounters
    CobaltMemoryAttributionManager::tls_counters_;

// static
CobaltMemoryAttributionManager* CobaltMemoryAttributionManager::Get() {
  return base::Singleton<
      CobaltMemoryAttributionManager,
      base::LeakySingletonTraits<CobaltMemoryAttributionManager>>::get();
}

CobaltMemoryAttributionManager::CobaltMemoryAttributionManager() {}

CobaltMemoryAttributionManager::~CobaltMemoryAttributionManager() = default;

void CobaltMemoryAttributionManager::Start() {
  if (!base::FeatureList::IsEnabled(
          cobalt::features::kCobaltMemoryAttributionManager)) {
    return;
  }

  if (is_observing_) {
    return;
  }
  is_observing_ = true;

  base::trace_event::MemoryDumpManager::GetInstance()->RegisterDumpProvider(
      this, "CobaltMemoryAttributionManager",
      base::SingleThreadTaskRunner::GetCurrentDefault());

  base::PowerMonitor::GetInstance()->AddPowerSuspendObserver(this);

  partition_alloc::PartitionAllocHooks::SetObserverHooks(&AllocHook, &FreeHook);

  last_report_time_ = base::TimeTicks::Now();
  timer_.Start(
      FROM_HERE,
      base::Seconds(
          cobalt::features::kCobaltMemoryAttributionReportIntervalParam.Get()),
      this, &CobaltMemoryAttributionManager::ReportUma);
}

void CobaltMemoryAttributionManager::Stop() {
  if (!is_observing_) {
    return;
  }
  is_observing_ = false;

  partition_alloc::PartitionAllocHooks::SetObserverHooks(nullptr, nullptr);

  base::trace_event::MemoryDumpManager::GetInstance()->UnregisterDumpProvider(
      this);
  base::PowerMonitor::GetInstance()->RemovePowerSuspendObserver(this);
  timer_.Stop();
}

void CobaltMemoryAttributionManager::RequestReportUmaForTesting(
    base::OnceClosure callback) {
  last_report_time_ = base::TimeTicks::Now();
  ReportUma();
  std::move(callback).Run();
}

void CobaltMemoryAttributionManager::RegisterThread(ThreadLocalCounters* tls) {
  base::AutoLock lock(threads_mutex_);
  tls->next = threads_head_;
  threads_head_ = tls;
  tls->is_registered = true;
}

void CobaltMemoryAttributionManager::UnregisterThread(
    ThreadLocalCounters* tls) {
  base::AutoLock lock(threads_mutex_);
  ThreadLocalCounters** current = &threads_head_;
  while (*current != nullptr) {
    if (*current == tls) {
      *current = tls->next;
      break;
    }
    current = &(*current)->next;
  }

  for (size_t i = 0;
       i < static_cast<size_t>(base::memory::MemoryContext::kCount); ++i) {
    int64_t transferred =
        tls->bytes_allocated[i].exchange(0, std::memory_order_relaxed);
    if (transferred != 0) {
      global_resident_bytes_[i].fetch_add(transferred,
                                          std::memory_order_relaxed);
    }
  }
}

void CobaltMemoryAttributionManager::SyncAllThreadLocalCounters() {
  base::AutoLock lock(threads_mutex_);
  ThreadLocalCounters* current = threads_head_;
  while (current != nullptr) {
    for (size_t i = 0;
         i < static_cast<size_t>(base::memory::MemoryContext::kCount); ++i) {
      int64_t transferred =
          current->bytes_allocated[i].exchange(0, std::memory_order_relaxed);
      if (transferred != 0) {
        global_resident_bytes_[i].fetch_add(transferred,
                                            std::memory_order_relaxed);
      }
    }
    current = current->next;
  }
}

void CobaltMemoryAttributionManager::ReportUma() {
  SyncAllThreadLocalCounters();

  base::TimeTicks now = base::TimeTicks::Now();
  if ((now - last_report_time_) >
      base::Seconds(
          cobalt::features::kCobaltMemoryAttributionReportIntervalParam.Get()) *
          2) {
    last_report_time_ = now;
    return;
  }
  last_report_time_ = now;

  for (size_t i = 0;
       i < static_cast<size_t>(base::memory::MemoryContext::kCount); ++i) {
    uint64_t resident =
        global_resident_bytes_[i].load(std::memory_order_relaxed);
    uint64_t resident_kb = resident / 1024;
    if (resident_kb > 0) {
      base::UmaHistogramCustomCounts(
          base::StrCat({"Memory.Cobalt.ResidentSize.",
                        base::memory::ContextToString(
                            static_cast<base::memory::MemoryContext>(i))}),
          resident_kb, 1, 67108864, 100);
    }
  }
}

void CobaltMemoryAttributionManager::OnSuspend() {}

void CobaltMemoryAttributionManager::OnResume() {
  last_report_time_ = base::TimeTicks::Now();
  timer_.Reset();
}

bool CobaltMemoryAttributionManager::OnMemoryDump(
    const base::trace_event::MemoryDumpArgs& args,
    base::trace_event::ProcessMemoryDump* pmd) {
  SyncAllThreadLocalCounters();

  for (size_t i = 0;
       i < static_cast<size_t>(base::memory::MemoryContext::kCount); ++i) {
    uint64_t resident =
        global_resident_bytes_[i].load(std::memory_order_relaxed);
    if (resident > 0) {
      std::string dump_name =
          base::StrCat({"cobalt/memory_attribution/",
                        base::memory::ContextToString(
                            static_cast<base::memory::MemoryContext>(i))});
      auto* dump = pmd->CreateAllocatorDump(dump_name);
      dump->AddScalar(base::trace_event::MemoryAllocatorDump::kNameSize,
                      base::trace_event::MemoryAllocatorDump::kUnitsBytes,
                      resident);
    }
  }
  return true;
}

uint64_t CobaltMemoryAttributionManager::GetResidentBytes(
    base::memory::MemoryContext context) {
  SyncAllThreadLocalCounters();
  return global_resident_bytes_[static_cast<size_t>(context)].load(
      std::memory_order_relaxed);
}

void CobaltMemoryAttributionManager::FlushThreadLocalCountersForTesting() {
  SyncAllThreadLocalCounters();
}

uint64_t CobaltMemoryAttributionManager::GetPartitionAllocOverhead() const {
  auto* root = allocator_shim::internal::PartitionAllocMalloc::Allocator();
  if (!root) {
    return 0;
  }
  return root->get_total_size_of_committed_pages() -
         root->get_total_size_of_allocated_bytes();
}

uint64_t CobaltMemoryAttributionManager::GetPartitionAllocTotalCommitted()
    const {
  auto* root = allocator_shim::internal::PartitionAllocMalloc::Allocator();
  if (!root) {
    return 0;
  }
  return root->get_total_size_of_committed_pages();
}

uint8_t* CobaltMemoryAttributionManager::GetShadowChunk(uintptr_t address) {
#if PA_BUILDFLAG(HAS_64_BIT_POINTERS)
  if (!partition_alloc::IsManagedByPartitionAlloc(address)) {
    return nullptr;
  }
  auto pool_info =
      partition_alloc::internal::PartitionAddressSpace::GetPoolInfo(address);
  if (pool_info.handle == partition_alloc::internal::kNullPoolHandle) {
    return nullptr;
  }
  size_t pool_index = pool_info.handle - 1;
  size_t super_page_index =
      pool_info.offset / partition_alloc::internal::kSuperPageSize;
  return shadow_map_tables_[pool_index][super_page_index].load(
      std::memory_order_relaxed);
#else
  size_t super_page_index = address / partition_alloc::internal::kSuperPageSize;
  return shadow_map_tables_[super_page_index].load(std::memory_order_relaxed);
#endif
}

uint8_t* CobaltMemoryAttributionManager::GetOrCreateShadowChunk(
    uintptr_t address) {
#if PA_BUILDFLAG(HAS_64_BIT_POINTERS)
  if (!partition_alloc::IsManagedByPartitionAlloc(address)) {
    return nullptr;
  }
  auto pool_info =
      partition_alloc::internal::PartitionAddressSpace::GetPoolInfo(address);
  if (pool_info.handle == partition_alloc::internal::kNullPoolHandle) {
    return nullptr;
  }
  size_t pool_index = pool_info.handle - 1;
  size_t super_page_index =
      pool_info.offset / partition_alloc::internal::kSuperPageSize;
  std::atomic<uint8_t*>& entry =
      shadow_map_tables_[pool_index][super_page_index];
#else
  size_t super_page_index = address / partition_alloc::internal::kSuperPageSize;
  std::atomic<uint8_t*>& entry = shadow_map_tables_[super_page_index];
#endif

  uint8_t* chunk = entry.load(std::memory_order_acquire);
  if (!chunk) {
    tls_counters_.is_hooking = true;
    uint8_t* new_chunk = static_cast<uint8_t*>(calloc(1, kShadowChunkSize));
    tls_counters_.is_hooking = false;

    uint8_t* expected = nullptr;
    if (entry.compare_exchange_strong(expected, new_chunk,
                                      std::memory_order_acq_rel)) {
      chunk = new_chunk;
    } else {
      tls_counters_.is_hooking = true;
      free(new_chunk);
      tls_counters_.is_hooking = false;
      chunk = expected;  // another thread won
    }
  }
  return chunk;
}

// static
void CobaltMemoryAttributionManager::AllocHook(
    const partition_alloc::AllocationNotificationData& data) {
  if (tls_counters_.is_hooking) {
    return;
  }
  tls_counters_.is_hooking = true;

  if (!tls_counters_.is_registered) {
    Get()->RegisterThread(&tls_counters_);
  }

  uintptr_t address = reinterpret_cast<uintptr_t>(data.address());
  if (address) {
    uint8_t* chunk = GetOrCreateShadowChunk(address);
    if (chunk) {
      base::memory::MemoryContext context =
          base::memory::GetCurrentMemoryContext();
      if (context >= base::memory::MemoryContext::kCount) {
        context = base::memory::MemoryContext::kUnknown;
      }

      size_t chunk_offset =
          (address & partition_alloc::internal::kSuperPageOffsetMask) /
          kBytesPerShadowByte;
      chunk[chunk_offset] = static_cast<uint8_t>(context);

      size_t usable_size =
          partition_alloc::PartitionRoot::GetUsableSize(data.address());
      tls_counters_.bytes_allocated[static_cast<size_t>(context)].fetch_add(
          usable_size, std::memory_order_relaxed);
    }
  }

  tls_counters_.is_hooking = false;
}

// static
void CobaltMemoryAttributionManager::FreeHook(
    const partition_alloc::FreeNotificationData& data) {
  if (tls_counters_.is_hooking) {
    return;
  }
  tls_counters_.is_hooking = true;

  if (!tls_counters_.is_registered) {
    Get()->RegisterThread(&tls_counters_);
  }

  uintptr_t address = reinterpret_cast<uintptr_t>(data.address());
  if (address) {
    uint8_t* chunk = GetShadowChunk(address);
    if (chunk) {
      size_t chunk_offset =
          (address & partition_alloc::internal::kSuperPageOffsetMask) /
          kBytesPerShadowByte;
      uint8_t context_val = chunk[chunk_offset];

      if (context_val <
          static_cast<uint8_t>(base::memory::MemoryContext::kCount)) {
        size_t usable_size =
            partition_alloc::PartitionRoot::GetUsableSize(data.address());
        tls_counters_.bytes_allocated[context_val].fetch_sub(
            usable_size, std::memory_order_relaxed);
      }
    }
  }

  tls_counters_.is_hooking = false;
}

}  // namespace memory
}  // namespace cobalt
