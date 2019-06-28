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

#ifndef COBALT_LAYOUT_INTERSECTION_OBSERVER_ROOT_H_
#define COBALT_LAYOUT_INTERSECTION_OBSERVER_ROOT_H_

#include <vector>

#include "base/memory/ref_counted.h"
#include "cobalt/cssom/property_list_value.h"

namespace cobalt {
namespace layout {

class Box;

// An IntersectionObserverRoot holds information associated with the W3
// IntersectionObserver object, such as rootMargin and thresholds.
// IntersectionObserverTarget objects reference these objects, instead of the
// other way around.
class IntersectionObserverRoot
    : public base::RefCountedThreadSafe<IntersectionObserverRoot> {
 public:
  IntersectionObserverRoot(
      const scoped_refptr<cssom::PropertyListValue>& root_margin_property_value,
      std::vector<double> thresholds_vector)
      : root_margin_property_value_(root_margin_property_value),
        thresholds_vector_(thresholds_vector) {}

  ~IntersectionObserverRoot() {}

  const scoped_refptr<cssom::PropertyListValue>& root_margin_property_value()
      const {
    return root_margin_property_value_;
  }

  std::vector<double> thresholds_vector() const { return thresholds_vector_; }

 private:
  // Properties associated with the IntersectionObserver object.
  scoped_refptr<cssom::PropertyListValue> root_margin_property_value_;
  std::vector<double> thresholds_vector_;
};

}  // namespace layout
}  // namespace cobalt

#endif  // COBALT_LAYOUT_INTERSECTION_OBSERVER_ROOT_H_
