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
#include "base/message_loop.h"
#include "base/synchronization/lock.h"
#include "v8/include/libplatform/libplatform.h"
#include "v8/include/v8-platform.h"
#include "v8/include/v8.h"

namespace cobalt {
namespace script {
namespace v8c {

// Implements v8's platform class to handle some requests from v8 to cobalt.
// It contains a v8::DefaultPlatform and uses most of its implementations.
class CobaltPlatform : public base::RefCounted<CobaltPlatform>,
                       public v8::Platform {
 public:
  explicit CobaltPlatform(std::unique_ptr<v8::Platform> platform)
      : default_platform_(std::move(platform)) {
    DCHECK(default_platform_.get());
  }

  // Because foreground tasks have to be run on the isolate's main thread,
  // each JavaScriptEngine needs to register its isolate in IsolateFellowship
  // so that when v8 post tasks to an isolate, we know which thread to call
  // PumpMessageLoop and run the posted task on.
  void RegisterIsolateOnThread(v8::Isolate* isolate, MessageLoop* message_loop);
  void UnregisterIsolateOnThread(v8::Isolate* isolate);
  void RunV8Task(v8::Isolate* isolate, scoped_ptr<v8::Task> task);

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

  std::shared_ptr<v8::TaskRunner> GetForegroundTaskRunner(
      v8::Isolate* isolate) override {
    return default_platform_->GetForegroundTaskRunner(isolate);
  }

  std::shared_ptr<v8::TaskRunner> GetBackgroundTaskRunner(
      v8::Isolate* isolate) override {
    return default_platform_->GetBackgroundTaskRunner(isolate);
  }

  void CallOnBackgroundThread(v8::Task* task,
                              ExpectedRuntime expected_runtime) override {
    // DefaultPlatform initializes threads running in the background to do
    // requested background work.
    default_platform_->CallOnBackgroundThread(task, expected_runtime);
  }

  // Post task on the message loop of the isolate's corresponding
  // JavaScriptEngine's main thread.
  void CallOnForegroundThread(v8::Isolate* isolate, v8::Task* task) override;

  void CallDelayedOnForegroundThread(v8::Isolate* isolate, v8::Task* task,
                                     double delay_in_seconds) override;

  void CallIdleOnForegroundThread(v8::Isolate* isolate,
                                  v8::IdleTask* task) override {
    SB_NOTIMPLEMENTED();
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
    TaskBeforeRegistration(double delay_in_seconds, v8::Task* task)
        : target_time(base::TimeTicks::Now() +
                      base::TimeDelta::FromSecondsD(delay_in_seconds)),
          task(task) {}
    base::TimeTicks target_time;
    std::unique_ptr<v8::Task> task;
  };
  struct MessageLoopMapEntry {
    // If tasks are posted before isolate is registered, we record their delay
    // and post them when isolate is registered.
    std::vector<std::unique_ptr<TaskBeforeRegistration>>
        tasks_before_registration;
    MessageLoop* message_loop = NULL;
  };
  typedef std::map<v8::Isolate*, std::unique_ptr<MessageLoopMapEntry>>
      MessageLoopMap;

  MessageLoopMapEntry* FindOrAddMapEntry(v8::Isolate* isolate);

  // A lookup table of isolate to its main thread's task runner.
  MessageLoopMap message_loop_map_;
  base::Lock lock_;

  DISALLOW_COPY_AND_ASSIGN(CobaltPlatform);
};

}  // namespace v8c
}  // namespace script
}  // namespace cobalt

#endif  // COBALT_SCRIPT_V8C_COBALT_PLATFORM_H_
