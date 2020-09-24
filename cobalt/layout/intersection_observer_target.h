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

#ifndef COBALT_LAYOUT_INTERSECTION_OBSERVER_TARGET_H_
#define COBALT_LAYOUT_INTERSECTION_OBSERVER_TARGET_H_

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "cobalt/cssom/property_list_value.h"
#include "cobalt/layout/layout_unit.h"
#include "cobalt/math/rect_f.h"

namespace cobalt {
namespace layout {

class Box;
class ContainerBox;
class IntersectionObserverRoot;

// A IntersectionObserverTarget roughly maps to |target| in a call to observe()
// (https://www.w3.org/TR/intersection-observer/#dom-intersectionobserver-observe).
// In lieu of having IntersectionObserverRegistration records, these objects can
// keep track of the |previousThresholdIndex| and |previousIsIntersecting|
// properties that determine whether IntersectionObserver objects need to be
// notified of changes
// (https://www.w3.org/TR/intersection-observer/#intersectionobserverregistration).
// An IntersectionObserverTarget references the IntersectionObserverRoot that it
// it associated with, not the other way around.
class IntersectionObserverTarget
    : public base::RefCountedThreadSafe<IntersectionObserverTarget> {
 public:
  // Callback that runs when an intersection change is observed, that goes back
  // into DOM logic to notify the intersection observers.
  typedef base::Callback<void(math::RectF root_bounds, math::RectF target_rect,
                              math::RectF intersection_rect,
                              bool is_intersecting, float intersection_ratio)>
      OnIntersectionCallback;

  IntersectionObserverTarget(
      const OnIntersectionCallback& on_intersection_callback,
      scoped_refptr<IntersectionObserverRoot> intersection_observer_root)
      : on_intersection_callback_(on_intersection_callback),
        intersection_observer_root_(intersection_observer_root),
        previous_threshold_index_(-1),
        previous_is_intersecting_(false) {}

  ~IntersectionObserverTarget() {}

  // The Intersection Observer spec describes a sequence of steps to update the
  // target element of an observer in step 2.2 of the "Run the Update
  // Intersection Observation Steps" algorithm.
  // This function follows the algorithm in the web spec, with a few notable
  // modifications: (1) All computations occur with respect to objects in the
  // box tree, not the dom tree. (2) This class already keeps track of the
  // previousThresholdIndex and previousIsIntersecting fields, so no
  // intersection observer registration objects are used to determine whether
  // IntersectionObserver objects need to be notified of a change.
  // https://www.w3.org/TR/intersection-observer/#update-intersection-observations-algo
  void UpdateIntersectionObservationsForTarget(ContainerBox* target_box);

  scoped_refptr<IntersectionObserverRoot> intersection_observer_root() {
    return intersection_observer_root_;
  }

 private:
  OnIntersectionCallback on_intersection_callback_;

  scoped_refptr<IntersectionObserverRoot> intersection_observer_root_;

  int32 previous_threshold_index_;

  bool previous_is_intersecting_;
};

}  // namespace layout
}  // namespace cobalt

#endif  // COBALT_LAYOUT_INTERSECTION_OBSERVER_TARGET_H_
