// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/controller/memory_usage_monitor.h"

#include "build/build_config.h"
#include "build/buildflag.h"

#if BUILDFLAG(IS_COBALT)
#include "base/feature_list.h"
#include "third_party/blink/public/common/features.h"
#endif

#include "base/observer_list.h"
#include "base/task/single_thread_task_runner.h"
#include "third_party/blink/renderer/platform/heap/process_heap.h"
#include "third_party/blink/renderer/platform/scheduler/public/main_thread.h"
#include "third_party/blink/renderer/platform/scheduler/public/main_thread_scheduler.h"
#include "third_party/blink/renderer/platform/wtf/allocator/partitions.h"
#include "v8/include/v8.h"

namespace blink {

namespace {
#if BUILDFLAG(IS_COBALT)
base::TimeDelta GetPingInterval() {
  if (base::FeatureList::IsEnabled(features::kMemoryUsageMonitorConfigurable)) {
    return base::Milliseconds(
        features::kMemoryUsageMonitorPollingIntervalMs.Get());
  }
  return base::Seconds(1);
}
#else
constexpr base::TimeDelta kPingInterval = base::Seconds(1);
#endif
}

MemoryUsageMonitor::MemoryUsageMonitor() {
  MainThreadScheduler* scheduler =
      Thread::MainThread()->Scheduler()->ToMainThreadScheduler();
  DCHECK(scheduler);
  timer_.SetTaskRunner(scheduler->NonWakingTaskRunner());
}

MemoryUsageMonitor::MemoryUsageMonitor(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner_for_testing,
    const base::TickClock* clock_for_testing)
    : timer_(clock_for_testing) {
  timer_.SetTaskRunner(task_runner_for_testing);
}

void MemoryUsageMonitor::AddObserver(Observer* observer) {
  StartMonitoringIfNeeded();
  observers_.AddObserver(observer);
}

void MemoryUsageMonitor::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

bool MemoryUsageMonitor::HasObserver(Observer* observer) {
  return observers_.HasObserver(observer);
}

void MemoryUsageMonitor::StartMonitoringIfNeeded() {
  if (timer_.IsRunning())
    return;
#if BUILDFLAG(IS_COBALT)
  timer_.Start(FROM_HERE, GetPingInterval(),
               WTF::BindRepeating(&MemoryUsageMonitor::TimerFired,
                                  WTF::Unretained(this)));
#else
  timer_.Start(FROM_HERE, kPingInterval,
               WTF::BindRepeating(&MemoryUsageMonitor::TimerFired,
                                  WTF::Unretained(this)));
#endif
}

void MemoryUsageMonitor::StopMonitoring() {
  timer_.Stop();
}

MemoryUsage MemoryUsageMonitor::GetCurrentMemoryUsage() {
  MemoryUsage usage;
  GetProcessMemoryUsage(usage);
  return usage;
}

void MemoryUsageMonitor::TimerFired() {
  MemoryUsage usage = GetCurrentMemoryUsage();
  for (auto& observer : observers_)
    observer.OnMemoryPing(usage);
  if (observers_.empty())
    StopMonitoring();
}

}  // namespace blink
