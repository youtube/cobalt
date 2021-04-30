// Copyright 2021 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_DOM_PERFORMANCE_OBSERVER_ENTRY_LIST_H_
#define COBALT_DOM_PERFORMANCE_OBSERVER_ENTRY_LIST_H_

#include <string>

#include "base/strings/string_piece.h"
#include "cobalt/dom/performance_entry_list_impl.h"
#include "cobalt/script/sequence.h"
#include "cobalt/script/wrappable.h"

namespace cobalt {
namespace dom {

// Implements the PerformanceObserverEntryList IDL interface, as described here:
//   https://www.w3.org/TR/performance-timeline-2/#performanceobserverentrylist-interface
class PerformanceObserverEntryList : public script::Wrappable {
 public:
  explicit PerformanceObserverEntryList(
      const PerformanceEntryList& observer_buffer);

  PerformanceEntryList GetEntries();
  PerformanceEntryList GetEntriesByType(const std::string& entry_type);
  PerformanceEntryList GetEntriesByName(const std::string& name,
                                        const base::StringPiece& type);

  DEFINE_WRAPPABLE_TYPE(PerformanceObserverEntryList);

 protected:
  PerformanceEntryList observer_buffer_;
};

}  // namespace dom
}  // namespace cobalt

#endif  // COBALT_DOM_PERFORMANCE_OBSERVER_ENTRY_LIST_H_
