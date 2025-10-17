// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/controller/memory_saver_controller.h"

#include "base/system/sys_info.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/renderer/core/workers/worker_backing_thread.h"
#include "third_party/blink/renderer/platform/scheduler/public/main_thread.h"
#include "third_party/blink/renderer/platform/scheduler/public/main_thread_scheduler.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread_scheduler.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

namespace {

constexpr uint64_t kMB = 1024 * 1024;

}  // namespace

void MemorySaverController::Initialize() {
  DEFINE_STATIC_LOCAL(MemorySaverController, controller, ());
  (void)controller;
}

MemorySaverController::MemorySaverController() {
  MainThreadScheduler* scheduler =
      Thread::MainThread()->Scheduler()->ToMainThreadScheduler();
  DCHECK(scheduler);
  sample_timer_.SetTaskRunner(scheduler->NonWakingTaskRunner());
  if (base::SysInfo::AmountOfPhysicalMemory() >= 4000 * kMB) {
    return;
  }
  if (base::FeatureList::IsEnabled(features::kMemorySaverModeRenderTuning)) {
    sample_timer_.Start(FROM_HERE, base::Seconds(5), this,
                        &MemorySaverController::Sample);
  }
}

void MemorySaverController::Sample() {
  uint64_t available_ram = base::SysInfo::AmountOfAvailablePhysicalMemory();
  if (available_ram < features::kAvailableMemoryThresholdParamMb.Get() * kMB) {
    if (!memory_saver_enabled_) {
      SetMemorySaverModeForAllIsolates(true);
      memory_saver_enabled_ = true;
    }
  } else if (memory_saver_enabled_) {
    SetMemorySaverModeForAllIsolates(false);
    memory_saver_enabled_ = false;
  }
}

void MemorySaverController::SetMemorySaverModeForAllIsolates(
    bool memory_saver_mode_enabled) {
  Thread::MainThread()
      ->Scheduler()
      ->ToMainThreadScheduler()
      ->ForEachMainThreadIsolate(WTF::BindRepeating(
          [](bool memory_saver_mode_enabled, v8::Isolate* isolate) {
            isolate->SetMemorySaverMode(memory_saver_mode_enabled);
          },
          memory_saver_mode_enabled));
  WorkerBackingThread::SetMemorySaverModeForWorkerThreadIsolates(
      memory_saver_mode_enabled);
}

}  // namespace blink
