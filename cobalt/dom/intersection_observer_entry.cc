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

#include "cobalt/dom/intersection_observer_entry.h"

#include "cobalt/dom/dom_rect_read_only.h"
#include "cobalt/dom/element.h"
#include "cobalt/dom/intersection_observer_entry_init.h"

namespace cobalt {
namespace dom {

IntersectionObserverEntry::IntersectionObserverEntry(
    const IntersectionObserverEntryInit& init_dict)
    : time_(init_dict.time()),
      root_bounds_(init_dict.root_bounds()),
      bounding_client_rect_(init_dict.bounding_client_rect()),
      intersection_rect_(init_dict.intersection_rect()),
      is_intersecting_(init_dict.is_intersecting()),
      intersection_ratio_(init_dict.intersection_ratio()),
      target_(init_dict.target()) {}

IntersectionObserverEntry::~IntersectionObserverEntry() {}

void IntersectionObserverEntry::TraceMembers(script::Tracer* tracer) {
  tracer->Trace(root_bounds_);
  tracer->Trace(bounding_client_rect_);
  tracer->Trace(intersection_rect_);
  tracer->Trace(target_);
}

}  // namespace dom
}  // namespace cobalt
