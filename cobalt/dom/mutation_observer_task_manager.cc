// Copyright 2017 Google Inc. All Rights Reserved.
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

#include "cobalt/dom/mutation_observer_task_manager.h"

#include "base/callback.h"
#include "base/message_loop.h"
#include "cobalt/dom/mutation_observer.h"

namespace cobalt {
namespace dom {

void MutationObserverTaskManager::OnMutationObserverCreated(
    MutationObserver* observer) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(observers_.find(observer) == observers_.end());
  observers_.insert(observer);
}

void MutationObserverTaskManager::OnMutationObserverDestroyed(
    MutationObserver* observer) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(observers_.find(observer) != observers_.end());
  observers_.erase(observer);
}

void MutationObserverTaskManager::QueueMutationObserverMicrotask() {
  DCHECK(thread_checker_.CalledOnValidThread());
  // https://www.w3.org/TR/dom/#queue-a-mutation-observer-compound-microtask
  // To queue a mutation observer compound microtask, run these steps:
  // 1. If mutation observer compound microtask queued flag is set, terminate
  //    these steps.
  if (task_posted_) {
    return;
  }
  // 2. Set mutation observer compound microtask queued flag.
  task_posted_ = true;
  // 3. Queue a compound microtask to notify mutation observers.
  MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&MutationObserverTaskManager::NotifyMutationObservers,
                 base::Unretained(this)));
}

void MutationObserverTaskManager::TraceMembers(script::Tracer* tracer) {
  tracer->TraceItems(observers_);
}

void MutationObserverTaskManager::NotifyMutationObservers() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(task_posted_);
  DCHECK(MessageLoop::current());
  // https://www.w3.org/TR/dom/#notify-mutation-observers
  // To notify mutation observers, run these steps:
  // 1. Unset mutation observer compound microtask queued flag.
  task_posted_ = false;

  // 2. Let notify list be a copy of unit of related similar-origin browsing
  //    contexts's list of MutationObserver objects.
  // 3. For each MutationObserver object mo in notify list, execute a compound
  //    microtask subtask to run these steps: [HTML]
  //
  // Subtask steps are implemented in MutationObserver::Notify.

  // Calling Notify could eventually add/remove observers from the set, so make
  // a copy of it.
  MutationObserverSet observers_copy = observers_;
  while (!observers_copy.empty()) {
    MutationObserver* observer = *observers_copy.begin();
    // Check that this observer was not removed as a side effect of Notify().
    if (observers_.find(observer) != observers_.end()) {
      if (!observer->Notify()) {
        DLOG(ERROR) << "Exception when notifying mutation observer.";
      }
    }
    observers_copy.erase(observers_copy.begin());
  }
}

}  // namespace dom
}  // namespace cobalt
