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

#ifndef COBALT_DOM_MUTATION_OBSERVER_TASK_MANAGER_H_
#define COBALT_DOM_MUTATION_OBSERVER_TASK_MANAGER_H_

#include <utility>

#include "base/hash_tables.h"
#include "base/threading/thread_checker.h"
#include "cobalt/script/global_environment.h"
#include "cobalt/script/tracer.h"

namespace cobalt {
namespace dom {

class MutationObserver;

// The Mutation observer spec describes a sequence of steps to "queue a mutation
// observer compound microtask" and "notify mutation observers". These steps
// are implemented in this helper class, which must outlive all
// MutationObserver instances.
// The spec expects an EventLoop implementation, which Cobalt does not currently
// have.
// https://www.w3.org/TR/dom/#mutation-observers
class MutationObserverTaskManager : public script::Traceable {
 public:
  MutationObserverTaskManager() : task_posted_(false) {}

  void RegisterAsTracingRoot(script::GlobalEnvironment* global_environment) {
    // Note that we only add ourselves, and never remove ourselves, as we will
    // actually outlive the web module.
    global_environment->AddRoot(this);
  }

  // These should be called in the constructor/destructor of the
  // MutationObserver.
  void OnMutationObserverCreated(MutationObserver* observer);
  void OnMutationObserverDestroyed(MutationObserver* observer);

  // Post a task to notify mutation observers, if one is not already posted.
  void QueueMutationObserverMicrotask();

  void TraceMembers(script::Tracer* tracer) override;

 private:
  typedef base::hash_set<MutationObserver*> MutationObserverSet;

  // Notify all mutation observers.
  void NotifyMutationObservers();

  base::ThreadChecker thread_checker_;
  MutationObserverSet observers_;
  bool task_posted_;
};

}  // namespace dom
}  // namespace cobalt

#endif  // COBALT_DOM_MUTATION_OBSERVER_TASK_MANAGER_H_
