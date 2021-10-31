// Copyright 2019 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_DOM_INTERSECTION_OBSERVER_TASK_MANAGER_H_
#define COBALT_DOM_INTERSECTION_OBSERVER_TASK_MANAGER_H_

#include <set>

#include "base/memory/ref_counted.h"
#include "base/threading/thread_checker.h"
#include "cobalt/script/tracer.h"

namespace cobalt {
namespace dom {

class IntersectionObserver;

// The Intersection Observer spec describes a sequence of steps to "queue an
// intersection observer task" and "notify intersection observers". These steps
// are implemented in this helper class, which must outlive all
// IntersectionObserver instances. The spec expects an EventLoop implementation,
// which Cobalt does not currently have.
// https://www.w3.org/TR/intersection-observer/#algorithms
class IntersectionObserverTaskManager
    : public base::RefCounted<IntersectionObserverTaskManager>,
      public script::Traceable {
 public:
  IntersectionObserverTaskManager();
  ~IntersectionObserverTaskManager();

  // These should be called in the constructor/destructor of the
  // IntersectionObserver.
  void OnIntersectionObserverCreated(IntersectionObserver* observer);
  void OnIntersectionObserverDestroyed(IntersectionObserver* observer);

  // Queue a task to notify intersection observers, if one is not already
  // posted.
  void QueueIntersectionObserverTask();

  void TraceMembers(script::Tracer* tracer) override;

 private:
  typedef std::set<IntersectionObserver*> IntersectionObserverSet;

  // Notify all intersection observers.
  void NotifyIntersectionObservers();

  THREAD_CHECKER(thread_checker_);
  IntersectionObserverSet observer_list_;
  bool intersection_observer_task_queued_;
};

}  // namespace dom
}  // namespace cobalt

#endif  // COBALT_DOM_INTERSECTION_OBSERVER_TASK_MANAGER_H_
