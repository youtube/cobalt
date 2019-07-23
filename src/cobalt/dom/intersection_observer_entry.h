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

#ifndef COBALT_DOM_INTERSECTION_OBSERVER_ENTRY_H_
#define COBALT_DOM_INTERSECTION_OBSERVER_ENTRY_H_

#include "base/memory/ref_counted.h"
#include "cobalt/script/wrappable.h"

namespace cobalt {
namespace dom {

class DOMRectReadOnly;
class Element;
class IntersectionObserverEntryInit;

// The IntersectionObserverEntry describes the intersection between the root and
// a target Element at a specific moment of transition.
//   https://www.w3.org/TR/intersection-observer/#intersection-observer-entry
class IntersectionObserverEntry : public script::Wrappable {
 public:
  explicit IntersectionObserverEntry(
      const IntersectionObserverEntryInit& init_dict);
  ~IntersectionObserverEntry();

  // Web API: IntersectionObserverEntry
  //
  double time() const { return time_; }
  const scoped_refptr<DOMRectReadOnly>& root_bounds() { return root_bounds_; }
  const scoped_refptr<DOMRectReadOnly>& bounding_client_rect() {
    return bounding_client_rect_;
  }
  const scoped_refptr<DOMRectReadOnly>& intersection_rect() {
    return intersection_rect_;
  }
  bool is_intersecting() const { return is_intersecting_; }
  double intersection_ratio() const { return intersection_ratio_; }
  const scoped_refptr<Element>& target() { return target_; }

  DEFINE_WRAPPABLE_TYPE(IntersectionObserverEntry);
  void TraceMembers(script::Tracer* tracer) override;

 private:
  double time_;
  scoped_refptr<DOMRectReadOnly> root_bounds_;
  scoped_refptr<DOMRectReadOnly> bounding_client_rect_;
  scoped_refptr<DOMRectReadOnly> intersection_rect_;
  bool is_intersecting_;
  double intersection_ratio_;
  scoped_refptr<Element> target_;
};

}  // namespace dom
}  // namespace cobalt

#endif  // COBALT_DOM_INTERSECTION_OBSERVER_ENTRY_H_
