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

#ifndef COBALT_MEMORY_COBALT_MEMORY_ATTRIBUTION_MANAGER_H_
#define COBALT_MEMORY_COBALT_MEMORY_ATTRIBUTION_MANAGER_H_

#include <atomic>
#include <cstdint>
#include <memory>

#include "base/feature_list.h"
#include "base/functional/callback_forward.h"
#include "base/memory/singleton.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "base/trace_event/memory_dump_provider.h"

namespace partition_alloc {
class AllocationNotificationData;
}

namespace cobalt {
class MemoryAttributionBrowserTest;
namespace memory {

enum class MemoryContext {
  kUnknown = 0,
  kDOM = 1,
  kLayout = 2,
  kMedia = 3,
  kScript = 4,
  kNetwork = 5,
  kGraphics = 6,
  kStorage = 7,
  kCount
};

extern thread_local MemoryContext g_current_memory_context;

class ScopedMemoryContext {
 public:
  explicit ScopedMemoryContext(MemoryContext context)
      : prev_context_(g_current_memory_context) {
    g_current_memory_context = context;
  }
  ~ScopedMemoryContext() { g_current_memory_context = prev_context_; }

 private:
  MemoryContext prev_context_;
};

class CobaltMemoryAttributionManager
    : public base::trace_event::MemoryDumpProvider {
 public:
  static CobaltMemoryAttributionManager* Get();

  CobaltMemoryAttributionManager(const CobaltMemoryAttributionManager&) =
      delete;
  CobaltMemoryAttributionManager& operator=(
      const CobaltMemoryAttributionManager&) = delete;

  void Start();
  void Stop();  // For testing
  void RequestReportUmaForTesting(base::OnceClosure callback);

  MemoryContext GetCurrentContext() const { return g_current_memory_context; }

  // base::trace_event::MemoryDumpProvider implementation.
  bool OnMemoryDump(const base::trace_event::MemoryDumpArgs& args,
                    base::trace_event::ProcessMemoryDump* pmd) override;

 private:
  friend struct base::DefaultSingletonTraits<CobaltMemoryAttributionManager>;
  friend class CobaltMemoryAttributionManagerTest;
  friend class cobalt::MemoryAttributionBrowserTest;

  CobaltMemoryAttributionManager();
  ~CobaltMemoryAttributionManager() override;

  static void AllocationHook(
      const partition_alloc::AllocationNotificationData& notification_data);

  void StartTimerOnSequence();
  void StopTimerOnSequence();
  void ReportUma();

  static std::atomic<uint64_t>
      counters_[static_cast<size_t>(MemoryContext::kCount)];
  uint64_t last_snapshots_[static_cast<size_t>(MemoryContext::kCount)];
  base::TimeTicks last_report_time_;
  base::RepeatingTimer timer_;
  scoped_refptr<base::SequencedTaskRunner> timer_task_runner_;
  bool is_observing_ = false;
};

}  // namespace memory
}  // namespace cobalt

#endif  // COBALT_MEMORY_COBALT_MEMORY_ATTRIBUTION_MANAGER_H_
