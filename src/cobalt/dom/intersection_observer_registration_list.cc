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

#include "cobalt/dom/intersection_observer_registration_list.h"

namespace cobalt {
namespace dom {

void IntersectionObserverRegistrationList::AddIntersectionObserver(
    const scoped_refptr<IntersectionObserver>& observer) {
  // https://www.w3.org/TR/intersection-observer/#ref-for-dom-intersectionobserver-observe
  // 1. If target is in this's internal [[ObservationTargets]] slot, return.
  for (auto it = registered_intersection_observers_.begin();
       it != registered_intersection_observers_.end(); ++it) {
    if (it->observer() == observer) {
      registered_intersection_observers_.erase(it);
      return;
    }
  }
  // 2. Let intersectionObserverRegistration be an
  //    IntersectionObserverRegistration record with an observer property set to
  //    this, a previousThresholdIndex property set to -1, and a
  //    previousIsIntersecting property set to false.
  // 3. Append intersectionObserverRegistration to target's internal
  //    [[RegisteredIntersectionObservers]] slot.
  registered_intersection_observers_.push_back(
      IntersectionObserverRegistration(observer));
  return;
}

void IntersectionObserverRegistrationList::RemoveIntersectionObserver(
    const scoped_refptr<IntersectionObserver>& observer) {
  for (auto it = registered_intersection_observers_.begin();
       it != registered_intersection_observers_.end(); ++it) {
    if (it->observer() == observer) {
      registered_intersection_observers_.erase(it);
      return;
    }
  }
  NOTREACHED() << "Did not find an intersection observer to unregister.";
}

IntersectionObserverRegistration*
IntersectionObserverRegistrationList::FindRegistrationForObserver(
    const scoped_refptr<IntersectionObserver>& observer) {
  for (auto& record : registered_intersection_observers_) {
    if (record.observer() == observer) {
      return &record;
    }
  }
  return NULL;
}

void IntersectionObserverRegistrationList::TraceMembers(
    script::Tracer* tracer) {
  tracer->TraceItems(registered_intersection_observers_);
}

}  // namespace dom
}  // namespace cobalt
