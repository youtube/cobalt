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

#include <memory>

#include "cobalt/base/polymorphic_downcast.h"
#include "cobalt/script/v8c/cobalt_platform.h"

#include "base/logging.h"

namespace cobalt {
namespace script {
namespace v8c {

CobaltPlatform::CobaltV8TaskRunner::CobaltV8TaskRunner()
    : ALLOW_THIS_IN_INITIALIZER_LIST(weak_ptr_factory_(this)) {}

void CobaltPlatform::CobaltV8TaskRunner::PostTask(
    std::unique_ptr<v8::Task> task) {
  PostDelayedTask(std::move(task), 0);
}

void CobaltPlatform::CobaltV8TaskRunner::PostDelayedTask(
    std::unique_ptr<v8::Task> task, double delay_in_seconds) {
  base::AutoLock auto_lock(lock_);
  if (task_runner_) {
    task_runner_->PostDelayedTask(
        FROM_HERE,
        base::Bind(&CobaltPlatform::CobaltV8TaskRunner::RunV8Task,
                   weak_ptr_factory_.GetWeakPtr(), base::Passed(&task)),
        base::TimeDelta::FromSecondsD(delay_in_seconds));
  } else {
    tasks_before_registration_.push_back(
        std::unique_ptr<TaskBeforeRegistration>(
            new TaskBeforeRegistration(delay_in_seconds, std::move(task))));
  }
}

// Since we put these v8 tasks direclty on the message loop, all the tasks are
// "NonNestable".
void CobaltPlatform::CobaltV8TaskRunner::PostNonNestableTask(std::unique_ptr<v8::Task> task) {
  return PostTask(std::move(task));
}
void CobaltPlatform::CobaltV8TaskRunner::PostNonNestableDelayedTask(std::unique_ptr<v8::Task> task,
                                    double delay_in_seconds) {
  return PostDelayedTask(std::move(task), 0);
}

void CobaltPlatform::CobaltV8TaskRunner::SetTaskRunner(
    base::SingleThreadTaskRunner* task_runner) {
  base::AutoLock auto_lock(lock_);
  task_runner_ = task_runner;
  DCHECK(task_runner);
  for (unsigned int i = 0; i < tasks_before_registration_.size(); ++i) {
    std::unique_ptr<v8::Task> scoped_task =
        std::move(tasks_before_registration_[i]->task);
    base::TimeDelta delay = std::max(
        tasks_before_registration_[i]->target_time - base::TimeTicks::Now(),
        base::TimeDelta::FromSeconds(0));
    task_runner_->PostDelayedTask(
        FROM_HERE,
        base::Bind(&CobaltPlatform::CobaltV8TaskRunner::RunV8Task,
                   weak_ptr_factory_.GetWeakPtr(), base::Passed(&scoped_task)),
        delay);
  }
  tasks_before_registration_.clear();
}

void CobaltPlatform::CobaltV8TaskRunner::RunV8Task(
    std::unique_ptr<v8::Task> task) {
  task->Run();
}

std::shared_ptr<v8::TaskRunner> CobaltPlatform::GetForegroundTaskRunner(
    v8::Isolate* isolate) {
  base::AutoLock auto_lock(lock_);
  std::shared_ptr<CobaltPlatform::CobaltV8TaskRunner> task_runner;
  if (v8_task_runner_map_.find(isolate) == v8_task_runner_map_.end()) {
    task_runner = std::make_shared<CobaltPlatform::CobaltV8TaskRunner>();
    v8_task_runner_map_.emplace(isolate, task_runner);
  } else {
    task_runner = v8_task_runner_map_[isolate];
  }
  DCHECK(task_runner);
  return task_runner;
}

void CobaltPlatform::RegisterIsolateOnThread(v8::Isolate* isolate,
                                             base::MessageLoop* message_loop) {
  DCHECK(message_loop);
  auto task_runner = GetForegroundTaskRunner(isolate);
  CobaltPlatform::CobaltV8TaskRunner* cobalt_v8_task_runner =
      base::polymorphic_downcast<CobaltPlatform::CobaltV8TaskRunner*>(
          task_runner.get());
  cobalt_v8_task_runner->SetTaskRunner(message_loop->task_runner().get());
}

void CobaltPlatform::UnregisterIsolateOnThread(v8::Isolate* isolate) {
  base::AutoLock auto_lock(lock_);
  auto iter = v8_task_runner_map_.find(isolate);
  if (iter != v8_task_runner_map_.end()) {
    v8_task_runner_map_.erase(iter);
  } else {
    DLOG(WARNING) << "Isolate is not in the map and can not be unregistered.";
  }
}

}  // namespace v8c
}  // namespace script
}  // namespace cobalt
