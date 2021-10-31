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

#ifndef COBALT_DOM_PERFORMANCE_ENTRY_LIST_IMPL_H_
#define COBALT_DOM_PERFORMANCE_ENTRY_LIST_IMPL_H_

#include <string>

#include "base/strings/string_piece.h"
#include "cobalt/script/sequence.h"
#include "cobalt/script/wrappable.h"

namespace cobalt {
namespace dom {

class PerformanceEntry;
typedef script::Sequence<scoped_refptr<PerformanceEntry>> PerformanceEntryList;

class PerformanceEntryListImpl {
 public:
  static PerformanceEntryList GetEntries(
      PerformanceEntryList performance_entry_buffer);
  static PerformanceEntryList GetEntriesByType(
      PerformanceEntryList performance_entry_buffer,
      const std::string& entry_type);
  static PerformanceEntryList GetEntriesByName(
      PerformanceEntryList performance_entry_buffer,
      const std::string& name,
      const base::StringPiece& type);
};

}  // namespace dom
}  // namespace cobalt

#endif  // COBALT_DOM_PERFORMANCE_ENTRY_LIST_IMPL_H_
