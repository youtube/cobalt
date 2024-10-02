// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/workers/worker_backing_thread.h"

#include <memory>

#include "base/location.h"
#include "base/synchronization/lock.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/web/blink.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_context_snapshot.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_gc_controller.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_idle_task_runner.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_initializer.h"
#include "third_party/blink/renderer/core/inspector/worker_thread_debugger.h"
#include "third_party/blink/renderer/core/workers/worker_backing_thread_startup_data.h"
#include "third_party/blink/renderer/platform/bindings/v8_per_isolate_data.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"

namespace blink {

namespace {

base::Lock& IsolatesLock() {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(base::Lock, lock, ());
  return lock;
}

HashSet<v8::Isolate*>& Isolates() EXCLUSIVE_LOCKS_REQUIRED(IsolatesLock()) {
  static HashSet<v8::Isolate*>& isolates = *new HashSet<v8::Isolate*>();
  return isolates;
}

void AddWorkerIsolate(v8::Isolate* isolate) {
  base::AutoLock locker(IsolatesLock());
  Isolates().insert(isolate);
}

void RemoveWorkerIsolate(v8::Isolate* isolate) {
  base::AutoLock locker(IsolatesLock());
  Isolates().erase(isolate);
}

}  // namespace

// Wrapper functions defined in third_party/blink/public/web/blink.h
void MemoryPressureNotificationToWorkerThreadIsolates(
    v8::MemoryPressureLevel level) {
  WorkerBackingThread::MemoryPressureNotificationToWorkerThreadIsolates(level);
}

WorkerBackingThread::WorkerBackingThread(const ThreadCreationParams& params)
    : backing_thread_(blink::NonMainThread::CreateThread(
          ThreadCreationParams(params).SetSupportsGC(true))) {}

WorkerBackingThread::~WorkerBackingThread() = default;

void WorkerBackingThread::InitializeOnBackingThread(
    const WorkerBackingThreadStartupData& startup_data) {
  DCHECK(backing_thread_->IsCurrentThread());

  DCHECK(!isolate_);
  ThreadScheduler* scheduler = BackingThread().Scheduler();
  isolate_ = V8PerIsolateData::Initialize(
      scheduler->V8TaskRunner(),
      V8PerIsolateData::V8ContextSnapshotMode::kDontUseSnapshot, nullptr,
      nullptr);
  scheduler->SetV8Isolate(isolate_);
  AddWorkerIsolate(isolate_);
  V8Initializer::InitializeWorker(isolate_);

  if (RuntimeEnabledFeatures::V8IdleTasksEnabled()) {
    V8PerIsolateData::EnableIdleTasks(
        isolate_, std::make_unique<V8IdleTaskRunner>(scheduler));
  }
  Platform::Current()->DidStartWorkerThread();

  V8PerIsolateData::From(isolate_)->SetThreadDebugger(
      std::make_unique<WorkerThreadDebugger>(isolate_));

  if (!base::FeatureList::IsEnabled(
          features::kV8OptimizeWorkersForPerformance)) {
    // Optimize for memory usage instead of latency for the worker isolate.
    // Service Workers that have the fetch event handler run with the Isolate
    // in foreground notification regardless of this configuration.
    // See ServiceWorkerGlobalScope::SetFetchHandlerExistence().
    isolate_->IsolateInBackgroundNotification();
  }

  if (startup_data.heap_limit_mode ==
      WorkerBackingThreadStartupData::HeapLimitMode::kIncreasedForDebugging) {
    isolate_->IncreaseHeapLimitForDebugging();
  }
  isolate_->SetAllowAtomicsWait(
      startup_data.atomics_wait_mode ==
      WorkerBackingThreadStartupData::AtomicsWaitMode::kAllow);
}

void WorkerBackingThread::ShutdownOnBackingThread() {
  DCHECK(backing_thread_->IsCurrentThread());
  BackingThread().Scheduler()->SetV8Isolate(nullptr);
  Platform::Current()->WillStopWorkerThread();

  V8PerIsolateData::WillBeDestroyed(isolate_);
  backing_thread_->ShutdownOnThread();

  RemoveWorkerIsolate(isolate_);
  V8PerIsolateData::Destroy(isolate_);
  isolate_ = nullptr;
}

// static
void WorkerBackingThread::MemoryPressureNotificationToWorkerThreadIsolates(
    v8::MemoryPressureLevel level) {
  base::AutoLock locker(IsolatesLock());
  for (v8::Isolate* isolate : Isolates())
    isolate->MemoryPressureNotification(level);
}

}  // namespace blink
