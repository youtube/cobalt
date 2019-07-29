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

#include "cobalt/layout/box_intersection_observer_module.h"

#include "base/trace_event/trace_event.h"
#include "cobalt/layout/box.h"

namespace cobalt {
namespace layout {

BoxIntersectionObserverModule::BoxIntersectionObserverModule(Box* box)
    : box_(box) {}

void BoxIntersectionObserverModule::AddIntersectionObserverRoots(
    IntersectionObserverRootVector&& roots) {
  intersection_observer_roots_ = std::move(roots);
}

void BoxIntersectionObserverModule::AddIntersectionObserverTargets(
    IntersectionObserverTargetVector&& targets) {
  intersection_observer_targets_ = std::move(targets);
}

bool BoxIntersectionObserverModule::BoxContainsIntersectionObserverRoot(
    const scoped_refptr<IntersectionObserverRoot>& intersection_observer_root)
    const {
  for (auto it = intersection_observer_roots_.begin();
       it != intersection_observer_roots_.end(); ++it) {
    if (*it == intersection_observer_root) {
      return true;
    }
  }
  return false;
}

void BoxIntersectionObserverModule::UpdateIntersectionObservations() {
  TRACE_EVENT0(
      "cobalt::layout",
      "BoxIntersectionObserverModule::UpdateIntersectionObservations()");
  ContainerBox* container_box = box_->AsContainerBox();
  if (!container_box) {
    return;
  }
  for (const auto& target : intersection_observer_targets_) {
    target->UpdateIntersectionObservationsForTarget(container_box);
  }
}

}  // namespace layout
}  // namespace cobalt
