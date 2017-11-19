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

#ifndef COBALT_DOM_REGISTERED_OBSERVER_LIST_H_
#define COBALT_DOM_REGISTERED_OBSERVER_LIST_H_

#include <vector>

#include "base/memory/ref_counted.h"
#include "cobalt/dom/mutation_observer.h"
#include "cobalt/dom/mutation_observer_init.h"
#include "cobalt/dom/registered_observer.h"
#include "cobalt/script/tracer.h"

namespace cobalt {
namespace dom {

class Node;

// A list of "registered observers" as described in the Mutation Observer spec.
// https://www.w3.org/TR/dom/#registered-observer
//
// Implements the functionality described in the MutationObserver.observe
// method:
// https://www.w3.org/TR/dom/#dom-mutationobserver-observe
class RegisteredObserverList : public script::Traceable {
 public:
  typedef std::vector<RegisteredObserver> RegisteredObserverVector;

  // A RegisteredObserverList must not outlive the |target| node that is being
  // observed.
  explicit RegisteredObserverList(const Node* target) : target_(target) {}

  // Implement the MutationObserver.observe method
  // https://www.w3.org/TR/dom/#dom-mutationobserver-observe
  bool AddMutationObserver(const scoped_refptr<MutationObserver>& observer,
                           const MutationObserverInit& options);
  void RemoveMutationObserver(const scoped_refptr<MutationObserver>& observer);

  const RegisteredObserverVector& registered_observers() {
    return registered_observers_;
  }

  void TraceMembers(script::Tracer* tracer) override;

 private:
  const Node* target_;
  RegisteredObserverVector registered_observers_;
};
}  // namespace dom
}  // namespace cobalt
#endif  // COBALT_DOM_REGISTERED_OBSERVER_LIST_H_
