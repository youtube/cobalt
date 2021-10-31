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

#ifndef COBALT_WEB_ANIMATIONS_TIMED_TASK_QUEUE_H_
#define COBALT_WEB_ANIMATIONS_TIMED_TASK_QUEUE_H_

#include <map>
#include <memory>
#include <set>

#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"

namespace cobalt {
namespace web_animations {

// Logic for registering a time-variable set of transitions and tracking them
// so that we can fire transitionend events when the transitions end.
class TimedTaskQueue {
 public:
  class Task {
   public:
    ~Task() { task_queue_->Deregister(this); }

    bool fired() const { return fired_; }
    base::TimeDelta fire_time() const { return fire_time_; }

   private:
    Task(TimedTaskQueue* task_queue, const base::TimeDelta& fire_time,
         const base::Closure& callback)
        : task_queue_(task_queue),
          fired_(false),
          fire_time_(fire_time),
          callback_(callback) {
      task_queue_->Register(this);
    }

    void Fire() {
      DCHECK(!fired_);
      fired_ = true;
      callback_.Run();
    }

    TimedTaskQueue* task_queue_;
    bool fired_;
    base::TimeDelta fire_time_;
    base::Closure callback_;

    friend class TimedTaskQueue;
  };

  TimedTaskQueue();

  std::unique_ptr<Task> QueueTask(base::TimeDelta fire_time,
                                  const base::Closure& task);

  void UpdateTime(base::TimeDelta time);

  bool empty() const { return task_priority_queue_.empty(); }
  base::TimeDelta next_fire_time() const {
    DCHECK(!empty());
    return (*task_priority_queue_.begin())->fire_time();
  }

 private:
  // The FireTimeSortedTaskList is used to keep a list of tasks
  // sorted by their end time, so that we can easily see which element is
  // next to be fired.  We use a sorted list versus a heap so that we can
  // remove/modify elements in the middle of the list as necessary.
  class FireTimeComparator {
   public:
    bool operator()(const Task* lhs, const Task* rhs) const {
      return lhs->fire_time() < rhs->fire_time();
    }
  };
  typedef std::multiset<Task*, FireTimeComparator> FireTimeSortedTaskList;

  // The TaskMap is used to lookup tasks given their associated
  // element and property name, in order to update or remove them from the
  // set of tasks.
  typedef std::map<Task*, FireTimeSortedTaskList::iterator> TaskMap;

  // Called by the Task nested class.
  void Register(Task* task);
  void Deregister(Task* task);

  // Finds the specified task in all queues and removes it from them.
  void RemoveTaskFromQueue(Task* task);

  FireTimeSortedTaskList task_priority_queue_;
  TaskMap task_map_;
};

}  // namespace web_animations
}  // namespace cobalt

#endif  // COBALT_WEB_ANIMATIONS_TIMED_TASK_QUEUE_H_
