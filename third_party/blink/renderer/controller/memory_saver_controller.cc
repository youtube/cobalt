// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/controller/memory_saver_controller.h"

#include <algorithm>

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
#if BUILDFLAG(IS_COBALT)
  // |memory_saver_enabled_| is otherwise uninitialized when returning early
  // below, and UpdateMemorySaverMode() reads it.
  memory_saver_enabled_ = false;
  if (base::FeatureList::IsEnabled(
          features::kCobaltV8MemorySaverOnMemoryPressure)) {
    memory_pressure_listener_ = std::make_unique<base::MemoryPressureListener>(
        FROM_HERE, WTF::BindRepeating(&MemorySaverController::OnMemoryPressure,
                                      WTF::Unretained(this)));
    // Like |sample_timer_|, don't wake the device just to end the hold time.
    memory_pressure_hold_timer_.SetTaskRunner(
        scheduler->NonWakingTaskRunner());
  }
#endif  // BUILDFLAG(IS_COBALT)
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
#if BUILDFLAG(IS_COBALT)
  // Combine with memory pressure, so that neither input turns memory saver
  // mode off while the other still asks for it.
  available_memory_low_ =
      available_ram < features::kAvailableMemoryThresholdParamMb.Get() * kMB;
  UpdateMemorySaverMode();
#else
  if (available_ram < features::kAvailableMemoryThresholdParamMb.Get() * kMB) {
    if (!memory_saver_enabled_) {
      SetMemorySaverModeForAllIsolates(true);
      memory_saver_enabled_ = true;
    }
  } else if (memory_saver_enabled_) {
    SetMemorySaverModeForAllIsolates(false);
    memory_saver_enabled_ = false;
  }
#endif  // BUILDFLAG(IS_COBALT)
}

#if BUILDFLAG(IS_COBALT)
void MemorySaverController::OnMemoryPressure(
    base::MemoryPressureListener::MemoryPressureLevel level) {
  // NONE is not reliably delivered in Cobalt, so memory saver mode is turned
  // off by the hold timer instead.
  if (level == base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE) {
    return;
  }
  if (features::kCobaltV8MemorySaverOnCriticalOnly.Get() &&
      level != base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL) {
    return;
  }
  memory_pressure_active_ = true;
  UpdateMemorySaverMode();
  memory_pressure_hold_timer_.Start(
      FROM_HERE,
      base::Seconds(
          std::max(0, features::kCobaltV8MemorySaverHoldSeconds.Get())),
      this, &MemorySaverController::OnMemoryPressureHoldExpired);
}

void MemorySaverController::OnMemoryPressureHoldExpired() {
  memory_pressure_active_ = false;
  UpdateMemorySaverMode();
}

void MemorySaverController::UpdateMemorySaverMode() {
  const bool enabled = available_memory_low_ || memory_pressure_active_;
  if (enabled == memory_saver_enabled_) {
    return;
  }
  SetMemorySaverModeForAllIsolates(enabled);
  memory_saver_enabled_ = enabled;
}
#endif  // BUILDFLAG(IS_COBALT)

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
