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

#include "cobalt/dom/intersection_observer_task_manager.h"

#include "base/message_loop/message_loop.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/trace_event.h"
#include "cobalt/dom/intersection_observer.h"

namespace cobalt {
namespace dom {

IntersectionObserverTaskManager::IntersectionObserverTaskManager()
    : intersection_observer_task_queued_(false) {}

IntersectionObserverTaskManager::~IntersectionObserverTaskManager() {}

void IntersectionObserverTaskManager::OnIntersectionObserverCreated(
    IntersectionObserver* observer) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(observer_list_.find(observer) == observer_list_.end());
  TRACE_EVENT0(
      "cobalt::dom",
      "IntersectionObserverTaskManager::OnIntersectionObserverCreated()");
  observer_list_.insert(observer);
}

void IntersectionObserverTaskManager::OnIntersectionObserverDestroyed(
    IntersectionObserver* observer) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(observer_list_.find(observer) != observer_list_.end());
  TRACE_EVENT0(
      "cobalt::dom",
      "IntersectionObserverTaskManager::OnIntersectionObserverDestroyed()");
  observer_list_.erase(observer);
}

void IntersectionObserverTaskManager::QueueIntersectionObserverTask() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  TRACE_EVENT0(
      "cobalt::dom",
      "IntersectionObserverTaskManager::QueueIntersectionObserverTask()");

  // https://www.w3.org/TR/intersection-observer/#queue-intersection-observer-task
  // To queue an intersection observer task for a Document document, run these
  // steps: 1. If document's IntersectionObserverTaskQueued flag is set to true,
  // return.
  if (intersection_observer_task_queued_) {
    return;
  }
  // 2. Set document's IntersectionObserverTaskQueued flag to true.
  intersection_observer_task_queued_ = true;
  // 3. Queue a task to the document's event loop to notify intersection
  // observers. (Cobalt does not support the document event loop concept, so we
  // will instead PostTask() to the WebModule's message loop)
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::Bind(&IntersectionObserverTaskManager::NotifyIntersectionObservers,
                 base::Unretained(this)));
}

void IntersectionObserverTaskManager::TraceMembers(script::Tracer* tracer) {
  tracer->TraceItems(observer_list_);
}

void IntersectionObserverTaskManager::NotifyIntersectionObservers() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  TRACE_EVENT0(
      "cobalt::dom",
      "IntersectionObserverTaskManager::NotifyIntersectionObservers()");
  DCHECK(intersection_observer_task_queued_);
  DCHECK(base::MessageLoop::current());

  // https://www.w3.org/TR/intersection-observer/#notify-intersection-observers-algo
  // To notify intersection observers for a Document document, run these steps:
  // 1. Set document's IntersectionObserverTaskQueued flag to false.
  intersection_observer_task_queued_ = false;

  // 2. Let notify list be a list of all IntersectionObservers whose root is in
  //    the DOM tree of document.
  // 3. For each IntersectionObserver object observer
  //    in notify list, run a set of subtasks (subtask steps are implemented in
  //    IntersectionObserver::Notify).
  for (auto observer : observer_list_) {
    if (!observer->Notify()) {
      DLOG(ERROR) << "Exception when notifying intersection observer.";
    }
  }
}

}  // namespace dom
}  // namespace cobalt
