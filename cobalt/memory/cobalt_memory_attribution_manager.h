// Copyright 2026 The Cobalt Authors. All Rights Reserved.

#ifndef COBALT_MEMORY_COBALT_MEMORY_ATTRIBUTION_MANAGER_H_
#define COBALT_MEMORY_COBALT_MEMORY_ATTRIBUTION_MANAGER_H_

#include <array>
#include <atomic>
#include <cstdint>

#include "base/allocator/partition_allocator/src/partition_alloc/partition_address_space.h"
#include "base/allocator/partition_allocator/src/partition_alloc/partition_alloc_constants.h"
#include "base/allocator/partition_allocator/src/partition_alloc/partition_alloc_hooks.h"
#include "base/allocator/partition_allocator/src/partition_alloc/partition_page.h"
#include "base/allocator/partition_allocator/src/partition_alloc/partition_root.h"
#include "base/memory/cobalt_memory_context.h"
#include "base/memory/singleton.h"
#include "base/power_monitor/power_observer.h"
#include "base/timer/timer.h"
#include "base/trace_event/memory_dump_provider.h"

namespace cobalt {
namespace memory {

class CobaltMemoryAttributionManager
    : public base::trace_event::MemoryDumpProvider,
      public base::PowerSuspendObserver {
 public:
  static CobaltMemoryAttributionManager* Get();

  CobaltMemoryAttributionManager(const CobaltMemoryAttributionManager&) =
      delete;
  CobaltMemoryAttributionManager& operator=(
      const CobaltMemoryAttributionManager&) = delete;

  void Start();
  void Stop();

  // base::trace_event::MemoryDumpProvider implementation.
  bool OnMemoryDump(const base::trace_event::MemoryDumpArgs& args,
                    base::trace_event::ProcessMemoryDump* pmd) override;

  // base::PowerSuspendObserver implementation.
  void OnSuspend() override;
  void OnResume() override;

  void RequestReportUmaForTesting(base::OnceClosure callback);
  void FlushThreadLocalCountersForTesting();

  // For reconciliation
  uint64_t GetResidentBytes(base::memory::MemoryContext context);
  uint64_t GetPartitionAllocOverhead() const;
  uint64_t GetPartitionAllocTotalCommitted() const;

 private:
  friend struct base::DefaultSingletonTraits<CobaltMemoryAttributionManager>;
  friend struct base::LeakySingletonTraits<CobaltMemoryAttributionManager>;

  CobaltMemoryAttributionManager();
  ~CobaltMemoryAttributionManager() override;

  void ReportUma();
  void SyncAllThreadLocalCounters();

  static void AllocHook(
      const partition_alloc::AllocationNotificationData& data);
  static void FreeHook(const partition_alloc::FreeNotificationData& data);

  // Shadow Map mapping 16 bytes to 1 byte.
  static constexpr size_t kBytesPerShadowByte = 16;
  static constexpr size_t kShadowChunkSize =
      partition_alloc::internal::kSuperPageSize / kBytesPerShadowByte;

#if PA_BUILDFLAG(HAS_64_BIT_POINTERS)
  static std::atomic<uint8_t*>
      shadow_map_tables_[partition_alloc::internal::kNumPools]
                        [partition_alloc::internal::kMaxSuperPagesInPool];
#else
  static std::atomic<uint8_t*>
      shadow_map_tables_[4ULL * 1024 * 1024 * 1024 /
                         partition_alloc::internal::kSuperPageSize];
#endif

  static uint8_t* GetOrCreateShadowChunk(uintptr_t address);
  static uint8_t* GetShadowChunk(uintptr_t address);

  // Thread-local variables
  struct ThreadLocalCounters {
    std::array<std::atomic<int64_t>,
               static_cast<size_t>(base::memory::MemoryContext::kCount)>
        bytes_allocated;
    bool is_hooking = false;
    bool is_registered = false;
    ThreadLocalCounters* next = nullptr;

    ThreadLocalCounters();
    ~ThreadLocalCounters();
  };
  static thread_local ThreadLocalCounters tls_counters_;

  // Global list of thread-local counters for syncing
  base::Lock threads_mutex_;
  ThreadLocalCounters* threads_head_ GUARDED_BY(threads_mutex_) = nullptr;
  void RegisterThread(ThreadLocalCounters* tls);
  void UnregisterThread(ThreadLocalCounters* tls);

  std::array<std::atomic<uint64_t>,
             static_cast<size_t>(base::memory::MemoryContext::kCount)>
      global_resident_bytes_;

  base::TimeTicks last_report_time_;
  base::RepeatingTimer timer_;
  bool is_observing_ = false;
};

}  // namespace memory
}  // namespace cobalt

#endif  // COBALT_MEMORY_COBALT_MEMORY_ATTRIBUTION_MANAGER_H_
