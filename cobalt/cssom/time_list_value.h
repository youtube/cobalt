/*
 * Copyright 2015 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef CSSOM_TIME_LIST_VALUE_H_
#define CSSOM_TIME_LIST_VALUE_H_

#include <inttypes.h>

#include "base/stringprintf.h"
#include "base/time.h"
#include "cobalt/cssom/list_value.h"

namespace cobalt {
namespace cssom {

// A CSS property value that is a list of base::TimeDelta values.  Example
// properties that can hold this value type are 'transition-duration' and
// 'transition-delay'.
class TimeListValue : public ListValue<base::TimeDelta> {
 public:
  explicit TimeListValue(scoped_ptr<ListValue<base::TimeDelta>::Builder> value)
      : ListValue(value.Pass()) {}

  void Accept(PropertyValueVisitor* visitor) OVERRIDE {
    visitor->VisitTimeList(this);
  }

  std::string ToString() const OVERRIDE {
    std::string result;
    for (size_t i = 0; i < value().size(); ++i) {
      if (!result.empty()) result.append(", ");
      int64 in_ms = value()[i].InMilliseconds();
      int64 truncated_to_seconds = in_ms / base::Time::kMillisecondsPerSecond;
      if (in_ms == base::Time::kMillisecondsPerSecond * truncated_to_seconds) {
        result.append(base::StringPrintf("%" PRIu64 "s", truncated_to_seconds));
      } else {
        result.append(base::StringPrintf("%" PRIu64 "ms", in_ms));
      }
    }
    return result;
  }

  DEFINE_POLYMORPHIC_EQUATABLE_TYPE(TimeListValue);

 private:
  virtual ~TimeListValue() OVERRIDE {}
};

}  // namespace cssom
}  // namespace cobalt

#endif  // CSSOM_TIME_LIST_VALUE_H_

