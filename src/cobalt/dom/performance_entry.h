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

#ifndef COBALT_DOM_PERFORMANCE_ENTRY_H_
#define COBALT_DOM_PERFORMANCE_ENTRY_H_

#include <string>

#include "cobalt/dom/performance_high_resolution_time.h"
#include "cobalt/script/wrappable.h"

namespace cobalt {
namespace dom {

typedef unsigned PerformanceEntryType;

// Implements the PerformanceEntry IDL interface, as described here:
//   https://w3c.github.io/performance-timeline/#the-performanceentry-interface
class PerformanceEntry : public script::Wrappable {
 public:
  PerformanceEntry(const std::string& name, DOMHighResTimeStamp start_time,
                   DOMHighResTimeStamp finish_time);

  enum EntryType : PerformanceEntryType {
    kResource = 0,
    kNavigation = 1,
    kLifecycle = 2,
    kMark = 3,
    kMeasure = 4,
    kInvalid = 5,
  };

  static constexpr const char* kEntryTypeString[] = {
      "resource", "navigation", "lifecycle", "mark", "measure", "invalid"};

  std::string name() const { return name_; }
  DOMHighResTimeStamp start_time() const;
  virtual std::string entry_type() const = 0;
  virtual PerformanceEntryType EntryTypeEnum() const = 0;
  // PerformanceNavigationTiming will override this function,
  // other classes must NOT override this.
  virtual DOMHighResTimeStamp duration() const;

  static PerformanceEntry::EntryType ToEntryTypeEnum(
      const std::string& entry_type);

  static bool StartTimeCompareLessThan(
      const scoped_refptr<PerformanceEntry>& a,
      const scoped_refptr<PerformanceEntry>& b);

  DEFINE_WRAPPABLE_TYPE(PerformanceEntry);

  ~PerformanceEntry() {}

 protected:
  // PerformanceEventTiming needs to modify it.
  DOMHighResTimeStamp duration_;

 private:
  const std::string name_;
  const DOMHighResTimeStamp start_time_;
  const int index_;

  DISALLOW_COPY_AND_ASSIGN(PerformanceEntry);
};

}  // namespace dom
}  // namespace cobalt

#endif  // COBALT_DOM_PERFORMANCE_ENTRY_H_
