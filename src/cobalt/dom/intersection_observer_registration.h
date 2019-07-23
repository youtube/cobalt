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

#ifndef COBALT_DOM_INTERSECTION_OBSERVER_REGISTRATION_H_
#define COBALT_DOM_INTERSECTION_OBSERVER_REGISTRATION_H_

#include "base/memory/ref_counted.h"
#include "cobalt/dom/intersection_observer.h"
#include "cobalt/script/tracer.h"

namespace cobalt {
namespace dom {

class Element;

// Represents the concept of an "intersection observer registration" as
// described in the Intersection Observer spec.
// https://www.w3.org/TR/intersection-observer/#intersectionobserverregistration
// This class is expected to be used internally in the Element class as a part
// of intersection reporting.
class IntersectionObserverRegistration : public script::Traceable {
 public:
  // An IntersectionObserverRegistration must not outlive the |target| element
  // that is being observed.
  IntersectionObserverRegistration(
      const scoped_refptr<IntersectionObserver>& observer)
      : observer_(observer),
        previous_threshold_index_(-1),
        previous_is_intersecting_(false) {}

  const IntersectionObserver* observer() const { return observer_; }
  int32 previous_threshold_index() const { return previous_threshold_index_; }
  void set_previous_threshold_index(int32 previous_threshold_index) {
    previous_threshold_index_ = previous_threshold_index;
  }
  bool previous_is_intersecting() const { return previous_is_intersecting_; }
  void set_previous_is_intersecting(bool previous_is_intersecting) {
    previous_is_intersecting_ = previous_is_intersecting;
  }

  void TraceMembers(script::Tracer* tracer) override {
    tracer->Trace(observer_);
  }

 private:
  IntersectionObserver* observer_;
  int32 previous_threshold_index_;
  bool previous_is_intersecting_;
};

}  // namespace dom
}  // namespace cobalt

#endif  // COBALT_DOM_INTERSECTION_OBSERVER_REGISTRATION_H_
