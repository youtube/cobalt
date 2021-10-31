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

#include "performance_entry.h"

#include "base/atomic_sequence_num.h"
#include "base/strings/string_util.h"

namespace cobalt {
namespace dom {

namespace {
static base::AtomicSequenceNumber index_seq;
}

// static
constexpr const char* PerformanceEntry::kEntryTypeString[];

PerformanceEntry::PerformanceEntry(const std::string& name,
                                   DOMHighResTimeStamp start_time,
                                   DOMHighResTimeStamp finish_time)
    : duration_(finish_time - start_time),
    name_(name),
    start_time_(start_time),
    index_(index_seq.GetNext()) {}

DOMHighResTimeStamp PerformanceEntry::start_time() const {
  return start_time_;
}

DOMHighResTimeStamp PerformanceEntry::duration() const {
  return duration_;
}

PerformanceEntry::EntryType PerformanceEntry::ToEntryTypeEnum(
    const std::string& entry_type) {
  if (entry_type.empty()) return kInvalid;

  for (size_t i = 0; i < arraysize(PerformanceEntry::kEntryTypeString); ++i) {
    if (base::LowerCaseEqualsASCII(entry_type,
        PerformanceEntry::kEntryTypeString[i])) {
      return static_cast<PerformanceEntry::EntryType>(i);
    }
  }
  return kInvalid;
}

// static
bool PerformanceEntry::StartTimeCompareLessThan(
    const scoped_refptr<PerformanceEntry>& a,
    const scoped_refptr<PerformanceEntry>& b) {
  if (a->start_time() == b->start_time())
    return a->index_ < b->index_;
  return a->start_time() < b->start_time();
}

}  // namespace dom
}  // namespace cobalt
