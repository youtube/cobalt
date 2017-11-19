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

#ifndef COBALT_DOM_REGISTERED_OBSERVER_H_
#define COBALT_DOM_REGISTERED_OBSERVER_H_

#include "base/memory/ref_counted.h"
#include "cobalt/dom/mutation_observer.h"
#include "cobalt/dom/mutation_observer_init.h"
#include "cobalt/script/tracer.h"

namespace cobalt {
namespace dom {

class Node;

// Represents the concept of a "registered observer" as described in the
// Mutation Observer spec.
// https://www.w3.org/TR/dom/#registered-observer
// This class is expected to be used internally in the Node class as a part of
// mutation reporting.
class RegisteredObserver : public script::Traceable {
 public:
  // A RegisteredObserver must not outlive the |target| node that is being
  // observed.
  RegisteredObserver(const Node* target,
                     const scoped_refptr<MutationObserver>& observer,
                     const MutationObserverInit& options)
      : target_(target), observer_(observer), options_(options) {}

  const scoped_refptr<MutationObserver>& observer() const { return observer_; }
  const MutationObserverInit& options() const { return options_; }
  void set_options(const MutationObserverInit& options) { options_ = options; }
  const Node* target() const { return target_; }

  void TraceMembers(script::Tracer* tracer) override {
    tracer->Trace(observer_);
  }

 private:
  const Node* target_;
  scoped_refptr<MutationObserver> observer_;
  MutationObserverInit options_;
};
}  // namespace dom
}  // namespace cobalt
#endif  // COBALT_DOM_REGISTERED_OBSERVER_H_
