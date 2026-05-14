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

#include "cobalt/memory/cobalt_memory_attribution_manager.h"

#include <string>
#include <string_view>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/trace_event/memory_dump_manager.h"
#include "base/trace_event/process_memory_dump.h"
#include "cobalt/browser/features.h"
#include "partition_alloc/partition_alloc_allocation_data.h"
#include "partition_alloc/partition_alloc_hooks.h"

namespace cobalt {
namespace memory {

thread_local MemoryContext g_current_memory_context = MemoryContext::kUnknown;

namespace {

std::string_view ContextToString(MemoryContext context) {
  switch (context) {
    case MemoryContext::kUnknown:
      return "Unknown";
    case MemoryContext::kDOM:
      return "DOM";
    case MemoryContext::kLayout:
      return "Layout";
    case MemoryContext::kMedia:
      return "Media";
    case MemoryContext::kScript:
      return "Script";
    case MemoryContext::kNetwork:
      return "Network";
    case MemoryContext::kGraphics:
      return "Graphics";
    case MemoryContext::kStorage:
      return "Storage";
    case MemoryContext::kCount:
      return "Unknown";
  }
}

}  // namespace

// static
std::atomic<uint64_t>
    CobaltMemoryAttributionManager::counters_[static_cast<size_t>(
        MemoryContext::kCount)];

// static
CobaltMemoryAttributionManager* CobaltMemoryAttributionManager::Get() {
  return base::Singleton<
      CobaltMemoryAttributionManager,
      base::LeakySingletonTraits<CobaltMemoryAttributionManager>>::get();
}

CobaltMemoryAttributionManager::CobaltMemoryAttributionManager() {
  for (size_t i = 0; i < static_cast<size_t>(MemoryContext::kCount); ++i) {
    counters_[i].store(0, std::memory_order_relaxed);
    last_snapshots_[i] = 0;
  }
}

CobaltMemoryAttributionManager::~CobaltMemoryAttributionManager() = default;

void CobaltMemoryAttributionManager::Start() {
  if (!base::FeatureList::IsEnabled(
          cobalt::features::kCobaltMemoryAttributionManager)) {
    return;
  }

  if (is_observing_) {
    return;
  }

  partition_alloc::PartitionAllocHooks::SetObserverHooks(&AllocationHook,
                                                         nullptr);
  is_observing_ = true;

  base::trace_event::MemoryDumpManager::GetInstance()->RegisterDumpProvider(
      this, "CobaltMemoryAttributionManager",
      base::SingleThreadTaskRunner::GetCurrentDefault());

  timer_task_runner_ = base::ThreadPool::CreateSequencedTaskRunner(
      {base::MayBlock(), base::TaskPriority::BEST_EFFORT});
  timer_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&CobaltMemoryAttributionManager::StartTimerOnSequence,
                     base::Unretained(this)));
}

void CobaltMemoryAttributionManager::Stop() {
  if (!is_observing_) {
    return;
  }

  partition_alloc::PartitionAllocHooks::SetObserverHooks(nullptr, nullptr);
  is_observing_ = false;

  base::trace_event::MemoryDumpManager::GetInstance()->UnregisterDumpProvider(
      this);

  if (timer_task_runner_) {
    timer_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&CobaltMemoryAttributionManager::StopTimerOnSequence,
                       base::Unretained(this)));
  }
}

// static
void CobaltMemoryAttributionManager::AllocationHook(
    const partition_alloc::AllocationNotificationData& notification_data) {
  counters_[static_cast<size_t>(g_current_memory_context)].fetch_add(
      notification_data.size(), std::memory_order_relaxed);
}

void CobaltMemoryAttributionManager::StartTimerOnSequence() {
  last_report_time_ = base::TimeTicks::Now();
  timer_.Start(FROM_HERE, base::Minutes(5), this,
               &CobaltMemoryAttributionManager::ReportUma);
}

void CobaltMemoryAttributionManager::StopTimerOnSequence() {
  timer_.Stop();
}

void CobaltMemoryAttributionManager::RequestReportUmaForTesting(
    base::OnceClosure callback) {
  if (!timer_task_runner_) {
    std::move(callback).Run();
    return;
  }
  timer_task_runner_->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(&CobaltMemoryAttributionManager::ReportUma,
                     base::Unretained(this)),
      std::move(callback));
}

void CobaltMemoryAttributionManager::ReportUma() {
  base::TimeTicks now = base::TimeTicks::Now();
  if ((now - last_report_time_) > base::Minutes(7)) {
    // Skip reporting if timer was delayed by device suspension
    last_report_time_ = now;
    for (size_t i = 0; i < static_cast<size_t>(MemoryContext::kCount); ++i) {
      last_snapshots_[i] = counters_[i].load(std::memory_order_relaxed);
    }
    return;
  }
  last_report_time_ = now;

  for (size_t i = 0; i < static_cast<size_t>(MemoryContext::kCount); ++i) {
    uint64_t current = counters_[i].load(std::memory_order_relaxed);
    uint64_t delta = current - last_snapshots_[i];
    uint64_t emitted_mb = delta / (1024 * 1024);
    last_snapshots_[i] += emitted_mb * (1024 * 1024);

    base::UmaHistogramMemoryMB(
        base::StrCat({"Memory.Cobalt.AllocationVolume.",
                      ContextToString(static_cast<MemoryContext>(i))}),
        emitted_mb);
  }
}

bool CobaltMemoryAttributionManager::OnMemoryDump(
    const base::trace_event::MemoryDumpArgs& args,
    base::trace_event::ProcessMemoryDump* pmd) {
  for (size_t i = 0; i < static_cast<size_t>(MemoryContext::kCount); ++i) {
    uint64_t current = counters_[i].load(std::memory_order_relaxed);
    std::string dump_name =
        base::StrCat({"cobalt/memory_attribution/",
                      ContextToString(static_cast<MemoryContext>(i))});
    auto* dump = pmd->CreateAllocatorDump(dump_name);
    dump->AddScalar(base::trace_event::MemoryAllocatorDump::kNameSize,
                    base::trace_event::MemoryAllocatorDump::kUnitsBytes,
                    current);
  }
  return true;
}

}  // namespace memory
}  // namespace cobalt
