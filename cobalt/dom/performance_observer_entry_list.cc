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

#include "cobalt/dom/performance_observer_entry_list.h"

#include <algorithm>

#include "cobalt/dom/performance_entry.h"

namespace cobalt {
namespace dom {

PerformanceObserverEntryList::PerformanceObserverEntryList(
    const PerformanceEntryList& entry_list)
    : observer_buffer_(entry_list) {}

PerformanceEntryList PerformanceObserverEntryList::GetEntries() {
  return PerformanceEntryListImpl::GetEntries(observer_buffer_);
}

PerformanceEntryList PerformanceObserverEntryList::GetEntriesByType(
    const std::string& entry_type) {
  return PerformanceEntryListImpl::GetEntriesByType(
      observer_buffer_, entry_type);
}

PerformanceEntryList PerformanceObserverEntryList::GetEntriesByName(
    const std::string& name, const base::StringPiece& type) {
  return PerformanceEntryListImpl::GetEntriesByName(
      observer_buffer_, name, type);
}

}  // namespace dom
}  // namespace cobalt
