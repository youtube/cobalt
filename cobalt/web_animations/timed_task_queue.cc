// Copyright 2015 The Cobalt Authors. All Rights Reserved.
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

#include "base/memory/ptr_util.h"
#include "cobalt/web_animations/timed_task_queue.h"

namespace cobalt {
namespace web_animations {

TimedTaskQueue::TimedTaskQueue() {}

std::unique_ptr<TimedTaskQueue::Task> TimedTaskQueue::QueueTask(
    base::TimeDelta fire_time, const base::Closure& task) {
  return base::WrapUnique(new Task(this, fire_time, task));
}

void TimedTaskQueue::UpdateTime(base::TimeDelta time) {
  // Search through the list of tasks and fire each one whose time is now
  // in the past.  We use a while loop instead of a for loop so that we can
  // erase elements from the set as we go.
  while (!task_priority_queue_.empty() &&
         (*task_priority_queue_.begin())->fire_time() <= time) {
    Task* task = *task_priority_queue_.begin();
    DCHECK(!task->fired());
    RemoveTaskFromQueue(task);

    task->Fire();
  }
}

void TimedTaskQueue::Register(Task* task) {
  FireTimeSortedTaskList::iterator inserted = task_priority_queue_.insert(task);
  task_map_.insert(std::make_pair(task, inserted));
}

void TimedTaskQueue::Deregister(Task* task) {
  if (!task->fired()) {
    // If the task has not yet fired, we need to remove it from
    // task_priority_queue so that it is no longer queued to fire.
    RemoveTaskFromQueue(task);
  } else {
    // If the task has already been fired, then check that it has already been
    // removed from the queues and do nothing else.
    DCHECK(task_map_.find(task) == task_map_.end());
  }
}

void TimedTaskQueue::RemoveTaskFromQueue(Task* task) {
  TaskMap::iterator found = task_map_.find(task);
  DCHECK(found != task_map_.end());
  task_priority_queue_.erase(found->second);
  task_map_.erase(found);
}

}  // namespace web_animations
}  // namespace cobalt
