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

#include "cobalt/dom/element_intersection_observer_module.h"

#include <algorithm>

#include "cobalt/dom/document.h"
#include "cobalt/dom/dom_rect_read_only.h"
#include "cobalt/dom/element.h"
#include "cobalt/dom/intersection_observer_entry_init.h"
#include "cobalt/dom/performance.h"

namespace cobalt {
namespace dom {

ElementIntersectionObserverModule::ElementIntersectionObserverModule(
    Element* element)
    : element_(element) {}

ElementIntersectionObserverModule::~ElementIntersectionObserverModule() {
  // The intersection observer vectors may hold the last reference to the
  // corresponding intersection observer object. Since the intersection observer
  // dtor calls UnregisterIntersectionObserverForRoot, make sure the call
  // doesn't try to release the scoped_refptr<IntersectionObserver> again during
  // destruction.
  IntersectionObserverVector temp_root_registered_observers;
  IntersectionObserverVector temp_target_registered_observers;

  // Swap out the intersection observer vectors so that the call to
  // UnregisterIntersectionObserverForRoot won't do anything, as the objects are
  // already being destroyed here.
  temp_root_registered_observers.swap(root_registered_intersection_observers_);
  temp_target_registered_observers.swap(
      target_registered_intersection_observers_);

  // Force destruction of the intersection observer objects here so that the
  // call to UnregisterIntersectionObserverForRoot occurs before this object is
  // fully destroyed.
  temp_root_registered_observers.clear();
  temp_target_registered_observers.clear();
}

void ElementIntersectionObserverModule::RegisterIntersectionObserverForRoot(
    IntersectionObserver* observer) {
  auto it = std::find(root_registered_intersection_observers_.begin(),
                      root_registered_intersection_observers_.end(), observer);
  if (it != root_registered_intersection_observers_.end()) {
    return;
  }
  root_registered_intersection_observers_.push_back(
      base::WrapRefCounted(observer));

  // Also create the observer's layout root at this time.
  scoped_refptr<layout::IntersectionObserverRoot> new_root =
      new layout::IntersectionObserverRoot(
          observer->root_margin_property_value(),
          observer->thresholds_vector());
  observer->set_layout_root(new_root);

  InvalidateLayoutBoxesForElement();
}

void ElementIntersectionObserverModule::UnregisterIntersectionObserverForRoot(
    IntersectionObserver* observer) {
  auto it = std::find(root_registered_intersection_observers_.begin(),
                      root_registered_intersection_observers_.end(), observer);
  if (it != root_registered_intersection_observers_.end()) {
    root_registered_intersection_observers_.erase(it);
    InvalidateLayoutBoxesForElement();
    return;
  }
  DLOG(WARNING)
      << "Did not find an intersection observer to unregister for the root.";
}

void ElementIntersectionObserverModule::RegisterIntersectionObserverForTarget(
    IntersectionObserver* observer) {
  auto it =
      std::find(target_registered_intersection_observers_.begin(),
                target_registered_intersection_observers_.end(), observer);
  if (it != target_registered_intersection_observers_.end()) {
    return;
  }
  target_registered_intersection_observers_.push_back(
      base::WrapRefCounted(observer));
  AddLayoutTargetForObserver(observer);

  InvalidateLayoutBoxesForElement();
  // We record a mutation when an intersection observer target is added, in
  // order to mark the layout as dirty and make sure that the intersection
  // observer creates an entry for any new intersection.
  Document* node_document = element_->node_document();
  if (node_document != nullptr) {
    node_document->RecordMutation();
  }
}

void ElementIntersectionObserverModule::UnregisterIntersectionObserverForTarget(
    IntersectionObserver* observer) {
  auto it =
      std::find(target_registered_intersection_observers_.begin(),
                target_registered_intersection_observers_.end(), observer);
  if (it != target_registered_intersection_observers_.end()) {
    target_registered_intersection_observers_.erase(it);
    RemoveLayoutTargetForObserver(observer);
    InvalidateLayoutBoxesForElement();
    return;
  }
  DLOG(WARNING)
      << "Did not find an intersection observer to unregister for the target.";
}

ElementIntersectionObserverModule::LayoutIntersectionObserverRootVector
ElementIntersectionObserverModule::
    GetLayoutIntersectionObserverRootsForElement() {
  ElementIntersectionObserverModule::LayoutIntersectionObserverRootVector
      layout_roots;
  for (const auto& intersection_observer :
       root_registered_intersection_observers_) {
    layout_roots.push_back(intersection_observer->layout_root());
  }
  return layout_roots;
}

ElementIntersectionObserverModule::LayoutIntersectionObserverTargetVector
ElementIntersectionObserverModule::
    GetLayoutIntersectionObserverTargetsForElement() {
  return layout_targets_;
}

void ElementIntersectionObserverModule::
    CreateIntersectionObserverEntryForObserver(
        const base::WeakPtr<IntersectionObserver>& observer,
        math::RectF root_bounds, math::RectF target_rect,
        math::RectF intersection_rect, bool is_intersecting,
        float intersection_ratio) {
  if (!observer) {
    return;
  }

  IntersectionObserverEntryInit init_dict;
  init_dict.set_time(element_->owner_document()
                         ->window()
                         ->performance()
                         ->timing()
                         ->GetNavigationStartClock()
                         ->Now()
                         .InMillisecondsF());
  init_dict.set_root_bounds(
      base::WrapRefCounted(new DOMRectReadOnly(root_bounds)));
  init_dict.set_bounding_client_rect(
      base::WrapRefCounted(new DOMRectReadOnly(target_rect)));
  init_dict.set_intersection_rect(
      base::WrapRefCounted(new DOMRectReadOnly(intersection_rect)));
  init_dict.set_is_intersecting(is_intersecting);
  init_dict.set_intersection_ratio(intersection_ratio);
  init_dict.set_target(base::WrapRefCounted(element_));
  observer->QueueIntersectionObserverEntry(
      base::WrapRefCounted(new IntersectionObserverEntry(init_dict)));
}

void ElementIntersectionObserverModule::TraceMembers(script::Tracer* tracer) {
  tracer->TraceItems(root_registered_intersection_observers_);
  tracer->TraceItems(target_registered_intersection_observers_);
}

void ElementIntersectionObserverModule::AddLayoutTargetForObserver(
    IntersectionObserver* observer) {
  layout::IntersectionObserverTarget::OnIntersectionCallback
      on_intersection_callback =
          base::Bind(&ElementIntersectionObserverModule::
                         CreateIntersectionObserverEntryForObserver,
                     base::Unretained(this), base::AsWeakPtr(observer));
  scoped_refptr<layout::IntersectionObserverTarget> layout_target =
      new layout::IntersectionObserverTarget(on_intersection_callback,
                                             observer->layout_root());
  layout_targets_.push_back(layout_target);
}

void ElementIntersectionObserverModule::RemoveLayoutTargetForObserver(
    IntersectionObserver* observer) {
  for (auto it = layout_targets_.begin(); it != layout_targets_.end(); ++it) {
    if ((*it)->intersection_observer_root() == observer->layout_root()) {
      layout_targets_.erase(it);
      return;
    }
  }
  NOTREACHED() << "Did not find a layout target to remove";
}

void ElementIntersectionObserverModule::InvalidateLayoutBoxesForElement() {
  HTMLElement* html_element = element_->AsHTMLElement();
  if (!html_element) {
    NOTREACHED();
    return;
  }
  html_element->InvalidateLayoutBoxesOfNodeAndAncestors();
}

}  // namespace dom
}  // namespace cobalt
