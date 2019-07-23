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

#ifndef COBALT_DOM_INTERSECTION_OBSERVER_TARGET_H_
#define COBALT_DOM_INTERSECTION_OBSERVER_TARGET_H_

#include "cobalt/dom/intersection_observer_registration_list.h"
#include "cobalt/math/rect_f.h"

namespace cobalt {
namespace dom {

class Element;
class HTMLElement;

// The Intersection Observer spec describes a sequence of steps to update the
// the target elements of an observer in step 2.2 of the "Run the Update
// Intersection Observations Steps" algorithm. This helper class represents
// one of those target elements.
// https://www.w3.org/TR/intersection-observer/#update-intersection-observations-algo
class IntersectionObserverTarget {
 public:
  explicit IntersectionObserverTarget(Element* target_element);

  void RegisterIntersectionObserver(
      const scoped_refptr<IntersectionObserver>& observer);

  void UnregisterIntersectionObserver(
      const scoped_refptr<IntersectionObserver>& observer);

  void UpdateIntersectionObservationsForTarget(
      const scoped_refptr<IntersectionObserver>& observer);

 private:
  scoped_refptr<DOMRectReadOnly> GetRootBounds(
      const scoped_refptr<HTMLElement>& html_intersection_root,
      scoped_refptr<cssom::PropertyListValue> root_margin_property_value);

  int32 GetUsedLengthOfRootMarginPropertyValue(
      const scoped_refptr<cssom::PropertyValue>& length_property_value,
      float percentage_base);

  scoped_refptr<DOMRectReadOnly> ComputeIntersectionBetweenTargetAndRoot(
      const scoped_refptr<HTMLElement>& intersection_root,
      const scoped_refptr<DOMRectReadOnly>& root_bounds,
      const scoped_refptr<DOMRectReadOnly>& target_rect,
      const scoped_refptr<HTMLElement>& html_target);

  // Similar to the IntersectRects function in math::RectF, but handles edge
  // adjacent intersections as valid intersections (instead of returning a
  // rectangle with zero dimensions)
  math::RectF IntersectIntersectionObserverRects(const math::RectF& a,
                                                 const math::RectF& b);

  Element* target_element_;

  IntersectionObserverRegistrationList intersection_observer_registration_list_;
};

}  // namespace dom
}  // namespace cobalt

#endif  // COBALT_DOM_INTERSECTION_OBSERVER_TARGET_H_
