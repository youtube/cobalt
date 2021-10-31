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

#ifndef COBALT_LAYOUT_BOX_INTERSECTION_OBSERVER_MODULE_H_
#define COBALT_LAYOUT_BOX_INTERSECTION_OBSERVER_MODULE_H_

#include <vector>

#include "cobalt/layout/intersection_observer_root.h"
#include "cobalt/layout/intersection_observer_target.h"

namespace cobalt {
namespace layout {

class Box;

// This helper class groups the methods and data related to the intersection
// root and target elements produced by a particular box.
class BoxIntersectionObserverModule {
 public:
  typedef std::vector<scoped_refptr<IntersectionObserverRoot>>
      IntersectionObserverRootVector;
  typedef std::vector<scoped_refptr<IntersectionObserverTarget>>
      IntersectionObserverTargetVector;

  explicit BoxIntersectionObserverModule(Box* box);
  ~BoxIntersectionObserverModule() {}

  void AddIntersectionObserverRoots(IntersectionObserverRootVector&& roots);
  void AddIntersectionObserverTargets(
      IntersectionObserverTargetVector&& targets);
  bool BoxContainsIntersectionObserverRoot(
      const scoped_refptr<IntersectionObserverRoot>& intersection_observer_root)
      const;
  void UpdateIntersectionObservations();

 private:
  Box* box_;

  IntersectionObserverRootVector intersection_observer_roots_;
  IntersectionObserverTargetVector intersection_observer_targets_;
};

}  // namespace layout
}  // namespace cobalt

#endif  // COBALT_LAYOUT_BOX_INTERSECTION_OBSERVER_MODULE_H_
