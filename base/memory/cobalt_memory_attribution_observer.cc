// Copyright 2026 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "base/memory/cobalt_memory_attribution_observer.h"

#if defined(OS_LINUX) || defined(OS_ANDROID)
#include <sys/prctl.h>
#endif


#if BUILDFLAG(IS_ANDROID)
#include <android/log.h>
#endif
#include "base/logging.h"

#include "base/no_destructor.h"
#include "base/threading/platform_thread.h"


namespace base {
namespace memory {


// static
CobaltMemoryAttributionObserver* CobaltMemoryAttributionObserver::Get() {
  static base::NoDestructor<CobaltMemoryAttributionObserver> observer;
  return observer.get();
}

CobaltMemoryAttributionObserver::CobaltMemoryAttributionObserver() {
  for (size_t i = 0; i < static_cast<size_t>(MemoryContext::kCount); ++i) {
    counters_[i].value.store(0, std::memory_order_relaxed);
  }
  // Ensure ThreadLocalStorage is fully initialized and warmed up before the
  // observer hooks into Dispatcher, preventing reentrancy crashes during the
  // first allocation.
  GetCurrentMemoryContext();
}

void CobaltMemoryAttributionObserver::OnAllocation(
    const base::allocator::dispatcher::AllocationNotificationData& notification_data) {

  MemoryContext current_context = GetCurrentMemoryContext();
  if (current_context >= MemoryContext::kCount) {
    current_context = MemoryContext::kUnknown;
  }
  counters_[static_cast<size_t>(current_context)].value.fetch_add(
      notification_data.size(), std::memory_order_relaxed);

  if (current_context == MemoryContext::kUnknown) {
    char thread_name[16] = {0};
    prctl(PR_GET_NAME, thread_name, 0, 0, 0);
    
    if (notification_data.size() >= 1024 * 64) {
      char buffer[128];
      snprintf(buffer, sizeof(buffer), "COBALT_MEM_TRACE: kUnknown Allocation of %zu bytes on thread %s", notification_data.size(), thread_name);
      RAW_LOG(INFO, buffer);
    }
    
    // Aggregation
    static struct {
      std::atomic<size_t> bytes;
      char name[16];
    } g_unknown_stats[32];
    static std::atomic<int> g_num_unknown_stats{0};
    
    bool found = false;
    int num = g_num_unknown_stats.load(std::memory_order_relaxed);
    if (num > 32) num = 32;
    for (int i = 0; i < num; ++i) {
      if (strncmp(g_unknown_stats[i].name, thread_name, 16) == 0) {
        size_t old_val = g_unknown_stats[i].bytes.fetch_add(notification_data.size(), std::memory_order_relaxed);
        if ((old_val / (1024 * 1024)) < ((old_val + notification_data.size()) / (1024 * 1024))) {
          char buffer[128];
          snprintf(buffer, sizeof(buffer), "COBALT_MEM_TRACE_AGGREGATE: Thread %s accumulated %zu bytes of kUnknown", thread_name, old_val + notification_data.size());
          RAW_LOG(INFO, buffer);
        }
        found = true;
        break;
      }
    }
    if (!found) {
      int idx = g_num_unknown_stats.load(std::memory_order_relaxed);
      while (idx < 32) {
        if (g_num_unknown_stats.compare_exchange_weak(idx, idx + 1, std::memory_order_relaxed)) {
          strncpy(g_unknown_stats[idx].name, thread_name, 16);
          g_unknown_stats[idx].bytes.store(notification_data.size(), std::memory_order_relaxed);
          break;
        }
      }
    }
    
    static std::atomic<size_t> g_total_unknown{0};
    size_t old_total = g_total_unknown.fetch_add(notification_data.size(), std::memory_order_relaxed);
    if ((old_total / (1024 * 1024 * 2)) < ((old_total + notification_data.size()) / (1024 * 1024 * 2))) {
#if BUILDFLAG(IS_ANDROID)
      __android_log_print(ANDROID_LOG_INFO, "COBALT_MEM", "COBALT_MEM_TRACE: --- UNKNOWN STATS DUMP ---");
#endif
      int max_i = g_num_unknown_stats.load(std::memory_order_relaxed);
      if (max_i > 32) max_i = 32;
      for (int i = 0; i < max_i; ++i) {
        size_t b = g_unknown_stats[i].bytes.load(std::memory_order_relaxed);
        if (b > 1024 * 1024) {
#if BUILDFLAG(IS_ANDROID)
          __android_log_print(ANDROID_LOG_INFO, "COBALT_MEM", "COBALT_MEM_TRACE: Thread %s -> %zu MB", g_unknown_stats[i].name, b / (1024 * 1024));
#endif
        }
      }
    }
  }

}

}  // namespace memory
}  // namespace base
