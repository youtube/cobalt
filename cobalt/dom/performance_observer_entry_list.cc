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

#include <algorithm>

#include "cobalt/dom/performance_entry.h"
#include "cobalt/dom/performance_observer_entry_list.h"

namespace cobalt {
namespace dom {

PerformanceObserverEntryList::PerformanceObserverEntryList(
    const PerformanceEntryList& entry_list)
    : performance_entries_(entry_list) {}

PerformanceEntryList PerformanceObserverEntryList::getEntries() {
  std::string empty_name_type;
  return FilterBufferByNameAndType(empty_name_type, empty_name_type);
}

PerformanceEntryList PerformanceObserverEntryList::getEntriesByType(
    const std::string& entry_type) {
  std::string empty_name;
  return FilterBufferByNameAndType(empty_name, entry_type);
}

PerformanceEntryList PerformanceObserverEntryList::getEntriesByName(
    const std::string& name, const base::StringPiece& type) {
  if (!type.data()) {
    std::string empty_type;
    return FilterBufferByNameAndType(name, empty_type);
  }
  return FilterBufferByNameAndType(name, type);
}

PerformanceEntryList PerformanceObserverEntryList::FilterBufferByNameAndType(
    const std::string& name, const base::StringPiece& entry_type) {
  // Algorithm for filtering buffers.
  //   https://www.w3.org/TR/2019/WD-performance-timeline-2-20191024/#filter-buffer-by-name-and-type
  // 1.  Let the list of entry objects be the empty PerformanceEntryList.
  PerformanceEntryList entries;

  // 2.  For each PerformanceEntry object (entryObject) in the buffer,
  // in chronological order with respect to startTime:
  for (const auto& entry : performance_entries_) {
    // 2.1  If name is not null and entryObject's name attribute does not
    // match name in a case-sensitive manner, go to next entryObject.
    bool is_name_not_valid = !entry->name().empty() && name != entry->name();
    if (is_name_not_valid) {
      continue;
    }

    // 2.2  If type is not null and entryObject's type attribute does not
    // match type in a case-sensitive manner, go to next entryObject.
    const PerformanceEntry::EntryType type =
        PerformanceEntry::ToEntryTypeEnum(entry_type.as_string());
    bool is_entry_type_not_valid = entry_type.data() != nullptr &&
        type != entry->EntryTypeEnum();
    if (is_entry_type_not_valid) {
      continue;
    }

    // 2.3  Add entryObject to the list of entry objects.
    entries.push_back(entry);
  }

  // Sort entry with respect to startTime.
  std::sort(entries.begin(), entries.end(),
            PerformanceEntry::StartTimeCompareLessThan);

  // 3.  Return the list of entry objects.
  return entries;
}

}  // namespace dom
}  // namespace cobalt
