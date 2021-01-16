// Copyright 2018 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef COBALT_SCRIPT_V8C_COBALT_PLATFORM_H_
#define COBALT_SCRIPT_V8C_COBALT_PLATFORM_H_

#include <map>
#include <memory>

#include "base/memory/ref_counted.h"
#include "base/message_loop/message_loop.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread_task_runner_handle.h"
#include "v8/include/libplatform/libplatform.h"
#include "v8/include/v8-platform.h"
#include "v8/include/v8.h"

namespace cobalt {
namespace script {
namespace v8c {

// Implements v8's platform class to handle some requests from v8 to cobalt.
// It contains a v8::DefaultPlatform and uses most of its implementations.
class CobaltPlatform : public v8::Platform {
 public:
  explicit CobaltPlatform(std::unique_ptr<v8::Platform> platform)
      : default_platform_(std::move(platform)) {
    DCHECK(default_platform_.get());
  }

  // Because foreground tasks have to be run on the isolate's main thread,
  // each JavaScriptEngine needs to register its isolate in IsolateFellowship
  // so that when v8 post tasks to an isolate, we know which thread to call
  // Pumpbase::MessageLoop and run the posted task on.
  void RegisterIsolateOnThread(v8::Isolate* isolate,
                               base::MessageLoop* message_loop);
  void UnregisterIsolateOnThread(v8::Isolate* isolate);

  // v8::Platform APIs.
  v8::PageAllocator* GetPageAllocator() override {
    return default_platform_->GetPageAllocator();
  }

  void OnCriticalMemoryPressure() override {
    default_platform_->OnCriticalMemoryPressure();
  }

  bool OnCriticalMemoryPressure(size_t length) override {
    return default_platform_->OnCriticalMemoryPressure(length);
  }

  int NumberOfWorkerThreads() {
    return default_platform_->NumberOfWorkerThreads();
  }

  std::shared_ptr<v8::TaskRunner> GetForegroundTaskRunner(
      v8::Isolate* isolate) override;

  std::unique_ptr<v8::JobHandle> PostJob(
       v8::TaskPriority priority, std::unique_ptr<v8::JobTask> job_task) override {
     return v8::platform::NewDefaultJobHandle(
         this, priority, std::move(job_task), NumberOfWorkerThreads());
  }
  void CallOnWorkerThread(std::unique_ptr<v8::Task> task) override {
    default_platform_->CallOnWorkerThread(std::move(task));
  }

  virtual void CallDelayedOnWorkerThread(std::unique_ptr<v8::Task> task,
                                         double delay_in_seconds) {
    default_platform_->CallDelayedOnWorkerThread(std::move(task),
                                                 delay_in_seconds);
  }

  bool IdleTasksEnabled(v8::Isolate* isolate) override { return false; }

  double MonotonicallyIncreasingTime() override {
    return default_platform_->MonotonicallyIncreasingTime();
  }

  double CurrentClockTimeMillis() override {
    return default_platform_->CurrentClockTimeMillis();
  }

  v8::TracingController* GetTracingController() override {
    return default_platform_->GetTracingController();
  }

  v8::Platform* platform() const { return default_platform_.get(); }

 private:
  std::unique_ptr<v8::Platform> default_platform_;

  struct TaskBeforeRegistration {
    TaskBeforeRegistration(double delay_in_seconds,
                           std::unique_ptr<v8::Task> task)
        : target_time(base::TimeTicks::Now() +
                      base::TimeDelta::FromSecondsD(delay_in_seconds)),
          task(std::move(task)) {}
    base::TimeTicks target_time;
    std::unique_ptr<v8::Task> task;
  };
  class CobaltV8TaskRunner : public v8::TaskRunner {
   public:
    CobaltV8TaskRunner();
    // v8::TaskRunner API
    void PostTask(std::unique_ptr<v8::Task> task) override;
    void PostDelayedTask(std::unique_ptr<v8::Task> task,
                         double delay_in_seconds) override;
    void PostIdleTask(std::unique_ptr<v8::IdleTask> task) override {}
    // TODO: Investigate if we want to enable Idle task and/or non-netable
    // tasks.
    bool IdleTasksEnabled() override { return false; }
    void PostNonNestableTask(std::unique_ptr<v8::Task> task) override;
    void PostNonNestableDelayedTask(std::unique_ptr<v8::Task> task,
                                    double delay_in_seconds) override;
    bool NonNestableTasksEnabled() const override { return true; }
    bool NonNestableDelayedTasksEnabled() const override { return true; }

    // custom helper methods
    void SetTaskRunner(base::SingleThreadTaskRunner* task_runner);
    void RunV8Task(std::unique_ptr<v8::Task> task);

   private:
    // We keep raw pointer instead of scoped_refptr because this class can be
    // posted with refptr and keeping a reference to task_runner_ might create
    // reference cycle. Also this class should be guaranteed to live shorter
    // than the thread.
    base::SingleThreadTaskRunner* task_runner_;
    base::WeakPtrFactory<CobaltV8TaskRunner> weak_ptr_factory_;
    // If tasks are posted before isolate is registered, we record their delay
    // and post them when isolate is registered to a thread.
    std::vector<std::unique_ptr<TaskBeforeRegistration>>
        tasks_before_registration_;
    base::Lock lock_;
  };
  typedef std::map<v8::Isolate*, std::shared_ptr<CobaltV8TaskRunner>>
      TaskRunnerMap;

  // A lookup table of isolate to its main thread's task runner.
  TaskRunnerMap v8_task_runner_map_;
  base::Lock lock_;

  DISALLOW_COPY_AND_ASSIGN(CobaltPlatform);
};

}  // namespace v8c
}  // namespace script
}  // namespace cobalt

#endif  // COBALT_SCRIPT_V8C_COBALT_PLATFORM_H_
