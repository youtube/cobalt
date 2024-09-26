// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_HEAP_TEST_PLATFORM_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_HEAP_TEST_PLATFORM_H_

#include <memory>

#include "v8/include/v8-platform.h"

namespace blink {

// Tests do not require that tasks can actually run, just that they can posted.
class HeapTestingMockTaskRunner final : public v8::TaskRunner {
 public:
  void PostTask(std::unique_ptr<v8::Task> task) override {}
  void PostNonNestableTask(std::unique_ptr<v8::Task> task) override {}
  void PostDelayedTask(std::unique_ptr<v8::Task> task,
                       double delay_in_seconds) override {}
  void PostNonNestableDelayedTask(std::unique_ptr<v8::Task> task,
                                  double delay_in_seconds) override {}
  void PostIdleTask(std::unique_ptr<v8::IdleTask> task) override {}

  bool IdleTasksEnabled() override { return false; }
  bool NonNestableTasksEnabled() const override { return false; }
  bool NonNestableDelayedTasksEnabled() const override { return false; }
};

class HeapTestingPlatformAdapter final : public v8::Platform {
 public:
  explicit HeapTestingPlatformAdapter(v8::Platform* platform)
      : platform_(platform),
        task_runner_(std::make_shared<HeapTestingMockTaskRunner>()) {}

  HeapTestingPlatformAdapter(const HeapTestingPlatformAdapter&) = delete;
  HeapTestingPlatformAdapter& operator=(const HeapTestingPlatformAdapter&) =
      delete;

  v8::PageAllocator* GetPageAllocator() final {
    return platform_->GetPageAllocator();
  }
  void OnCriticalMemoryPressure() final {
    platform_->OnCriticalMemoryPressure();
  }
  int NumberOfWorkerThreads() final {
    return platform_->NumberOfWorkerThreads();
  }
  std::shared_ptr<v8::TaskRunner> GetForegroundTaskRunner(
      v8::Isolate* isolate) final {
    // Provides task runner that allows for incremental tasks even in detached
    // mode.
    return task_runner_;
  }
  void CallOnWorkerThread(std::unique_ptr<v8::Task> task) final {
    platform_->CallOnWorkerThread(std::move(task));
  }
  void CallBlockingTaskOnWorkerThread(std::unique_ptr<v8::Task> task) final {
    platform_->CallBlockingTaskOnWorkerThread(std::move(task));
  }
  void CallLowPriorityTaskOnWorkerThread(std::unique_ptr<v8::Task> task) final {
    platform_->CallLowPriorityTaskOnWorkerThread(std::move(task));
  }
  void CallDelayedOnWorkerThread(std::unique_ptr<v8::Task> task,
                                 double delay_in_seconds) final {
    platform_->CallDelayedOnWorkerThread(std::move(task), delay_in_seconds);
  }
  bool IdleTasksEnabled(v8::Isolate* isolate) final {
    return platform_->IdleTasksEnabled(isolate);
  }
  std::unique_ptr<v8::JobHandle> CreateJob(
      v8::TaskPriority priority,
      std::unique_ptr<v8::JobTask> job_task) final {
    return platform_->CreateJob(priority, std::move(job_task));
  }
  double MonotonicallyIncreasingTime() final {
    return platform_->MonotonicallyIncreasingTime();
  }
  double CurrentClockTimeMillis() final {
    return platform_->CurrentClockTimeMillis();
  }
  StackTracePrinter GetStackTracePrinter() final {
    return platform_->GetStackTracePrinter();
  }
  v8::TracingController* GetTracingController() final {
    return platform_->GetTracingController();
  }
  void DumpWithoutCrashing() final { platform_->DumpWithoutCrashing(); }

 private:
  v8::Platform* platform_;
  std::shared_ptr<v8::TaskRunner> task_runner_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_HEAP_TEST_PLATFORM_H_
