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

#ifndef COBALT_DOM_ELEMENT_INTERSECTION_OBSERVER_MODULE_H_
#define COBALT_DOM_ELEMENT_INTERSECTION_OBSERVER_MODULE_H_

#include <vector>

#include "cobalt/dom/intersection_observer.h"
#include "cobalt/math/rect_f.h"
#include "cobalt/script/tracer.h"

namespace cobalt {
namespace dom {

class Element;

// This helper class groups the methods and  data related to the root and target
// elements outlined in the intersection observer spec.
// https://www.w3.org/TR/intersection-observer
class ElementIntersectionObserverModule : public script::Traceable {
 public:
  typedef std::vector<scoped_refptr<IntersectionObserver>>
      IntersectionObserverVector;
  typedef std::vector<scoped_refptr<layout::IntersectionObserverRoot>>
      LayoutIntersectionObserverRootVector;
  typedef std::vector<scoped_refptr<layout::IntersectionObserverTarget>>
      LayoutIntersectionObserverTargetVector;

  explicit ElementIntersectionObserverModule(Element* element);
  ~ElementIntersectionObserverModule();

  void RegisterIntersectionObserverForRoot(IntersectionObserver* observer);
  void UnregisterIntersectionObserverForRoot(IntersectionObserver* observer);
  void RegisterIntersectionObserverForTarget(IntersectionObserver* observer);
  void UnregisterIntersectionObserverForTarget(IntersectionObserver* observer);

  LayoutIntersectionObserverRootVector
  GetLayoutIntersectionObserverRootsForElement();
  LayoutIntersectionObserverTargetVector
  GetLayoutIntersectionObserverTargetsForElement();

  void CreateIntersectionObserverEntryForObserver(
      const base::WeakPtr<IntersectionObserver>& observer,
      math::RectF root_bounds, math::RectF target_rect,
      math::RectF intersection_rect, bool is_intersecting,
      float intersection_ratio);

  void TraceMembers(script::Tracer* tracer) override;

 private:
  void AddLayoutTargetForObserver(IntersectionObserver* observer);
  void RemoveLayoutTargetForObserver(IntersectionObserver* observer);
  // We invalidate the layout boxes when an intersection observer root/target
  // is added/removed, so that the boxes associated with the roots and targets
  // get updated accordingly during the box generation phase.
  void InvalidateLayoutBoxesForElement();

  Element* element_;
  IntersectionObserverVector root_registered_intersection_observers_;
  IntersectionObserverVector target_registered_intersection_observers_;
  LayoutIntersectionObserverTargetVector layout_targets_;
};

}  // namespace dom
}  // namespace cobalt

#endif  // COBALT_DOM_ELEMENT_INTERSECTION_OBSERVER_MODULE_H_
