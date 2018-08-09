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

#include "cobalt/script/v8c/cobalt_platform.h"

#include "base/logging.h"

namespace cobalt {
namespace script {
namespace v8c {

CobaltPlatform::MessageLoopMapEntry* CobaltPlatform::FindOrAddMapEntry(
    v8::Isolate* isolate) {
  auto iter = message_loop_map_.find(isolate);
  // Because of the member unique_ptr, we need explicit creation.
  if (iter == message_loop_map_.end()) {
    auto new_entry = std::unique_ptr<CobaltPlatform::MessageLoopMapEntry>(
        new CobaltPlatform::MessageLoopMapEntry());
    message_loop_map_.emplace(std::make_pair(isolate, std::move(new_entry)));
  }
  DCHECK(message_loop_map_[isolate]);
  return message_loop_map_[isolate].get();
}

void CobaltPlatform::RegisterIsolateOnThread(v8::Isolate* isolate,
                                             MessageLoop* message_loop) {
  base::AutoLock auto_lock(lock_);
  auto* message_loop_entry = FindOrAddMapEntry(isolate);
  message_loop_entry->message_loop = message_loop;
  if (!message_loop) {
    DLOG(WARNING) << "Isolate is registered without a valid message loop!";
    return;
  }
  std::vector<std::unique_ptr<TaskBeforeRegistration>> task_vec;
  task_vec.swap(message_loop_entry->tasks_before_registration);
  DCHECK(message_loop_entry->tasks_before_registration.empty());
  for (unsigned int i = 0; i < task_vec.size(); ++i) {
    scoped_ptr<v8::Task> scoped_task(task_vec[i]->task.release());
    base::TimeDelta delay =
        std::max(task_vec[i]->target_time - base::TimeTicks::Now(),
                 base::TimeDelta::FromSeconds(0));
    message_loop->PostDelayedTask(
        FROM_HERE,
        base::Bind(&CobaltPlatform::RunV8Task, this, isolate,
                   base::Passed(&scoped_task)),
        delay);
  }
}

void CobaltPlatform::UnregisterIsolateOnThread(v8::Isolate* isolate) {
  base::AutoLock auto_lock(lock_);
  MessageLoopMap::iterator iter = message_loop_map_.find(isolate);
  if (iter != message_loop_map_.end()) {
    message_loop_map_.erase(iter);
  } else {
    DLOG(WARNING) << "Isolate is not in the map and can not be unregistered.";
  }
}

void CobaltPlatform::RunV8Task(v8::Isolate* isolate,
                               scoped_ptr<v8::Task> task) {
  {
    base::AutoLock auto_lock(lock_);
    MessageLoopMap::iterator iter = message_loop_map_.find(isolate);
    if (iter == message_loop_map_.end() || !iter->second->message_loop) {
      DLOG(WARNING) << "V8 foreground task executes after isolate "
                       "unregistered, aborting.";
      return;
    }
  }
  task->Run();
}

void CobaltPlatform::CallOnForegroundThread(v8::Isolate* isolate,
                                            v8::Task* task) {
  CallDelayedOnForegroundThread(isolate, task, 0);
}

void CobaltPlatform::CallDelayedOnForegroundThread(v8::Isolate* isolate,
                                                   v8::Task* task,
                                                   double delay_in_seconds) {
  base::AutoLock auto_lock(lock_);
  auto* message_loop_entry = FindOrAddMapEntry(isolate);
  if (message_loop_entry->message_loop != NULL) {
    scoped_ptr<v8::Task> scoped_task(task);
    message_loop_entry->message_loop->PostDelayedTask(
        FROM_HERE,
        base::Bind(&CobaltPlatform::RunV8Task, this, isolate,
                   base::Passed(&scoped_task)),
        base::TimeDelta::FromSecondsD(delay_in_seconds));
  } else {
    message_loop_map_[isolate]->tasks_before_registration.push_back(
        std::unique_ptr<TaskBeforeRegistration>(
            new TaskBeforeRegistration(delay_in_seconds, task)));
  }
}

}  // namespace v8c
}  // namespace script
}  // namespace cobalt
