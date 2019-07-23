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

#ifndef COBALT_DOM_INTERSECTION_OBSERVER_REGISTRATION_LIST_H_
#define COBALT_DOM_INTERSECTION_OBSERVER_REGISTRATION_LIST_H_

#include <vector>

#include "base/memory/ref_counted.h"
#include "cobalt/dom/intersection_observer.h"
#include "cobalt/dom/intersection_observer_registration.h"

namespace cobalt {
namespace dom {

// A list of "intersection observer registrations" as described in the
// Intersection Observer spec.
// https://www.w3.org/TR/intersection-observer/#intersectionobserverregistration
//
// Implements the functionality described in the IntersectionObserver.observe
// method:
// https://www.w3.org/TR/intersection-observer/#ref-for-dom-intersectionobserver-observe
class IntersectionObserverRegistrationList : public script::Traceable {
 public:
  typedef std::vector<IntersectionObserverRegistration>
      IntersectionObserverRegistrationVector;

  IntersectionObserverRegistrationList() {}

  // Implement the IntersectionObserver.observe method
  // https://www.w3.org/TR/intersection-observer/#ref-for-dom-intersectionobserver-observe
  void AddIntersectionObserver(
      const scoped_refptr<IntersectionObserver>& observer);
  // Implement the IntersectionObserver.unobserve method
  // https://www.w3.org/TR/intersection-observer/#dom-intersectionobserver-unobserve
  void RemoveIntersectionObserver(
      const scoped_refptr<IntersectionObserver>& observer);

  IntersectionObserverRegistration* FindRegistrationForObserver(
      const scoped_refptr<IntersectionObserver>& observer);

  const IntersectionObserverRegistrationVector&
  registered_intersection_observers() {
    return registered_intersection_observers_;
  }

  void TraceMembers(script::Tracer* tracer) override;

 private:
  IntersectionObserverRegistrationVector registered_intersection_observers_;
};

}  // namespace dom
}  // namespace cobalt

#endif  // COBALT_DOM_INTERSECTION_OBSERVER_REGISTRATION_LIST_H_
