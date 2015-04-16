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

#include <vector>

#include "base/memory/scoped_ptr.h"
#include "base/time.h"
#include "cobalt/cssom/property_value.h"

namespace cobalt {
namespace cssom {

enum TimeUnit {
  kSecondsUnit,
  kMillisecondsUnit,
};

struct Time {
  float value;
  TimeUnit unit;

  base::TimeDelta ToTimeDelta() const {
    switch (unit) {
      case kSecondsUnit:
        return base::TimeDelta::FromMilliseconds(
            static_cast<int64>(value * base::Time::kMillisecondsPerSecond));
      case kMillisecondsUnit:
        return base::TimeDelta::FromMilliseconds(static_cast<int64>(value));
      default:
        NOTREACHED();
        return base::TimeDelta();
    }
  }

  bool operator==(const Time& other) const {
    return value == other.value && unit == other.unit;
  }
};

// A list of times that may be used to define things like animation durations.
class TimeListValue : public PropertyValue {
 public:
  typedef std::vector<Time> TimeList;

  explicit TimeListValue(scoped_ptr<TimeList> value);

  virtual void Accept(PropertyValueVisitor* visitor) OVERRIDE;

  const TimeList& value() const { return *value_; }

  // Returns the time at the given list index, modulos the list size.  This
  // is used when matching transition property durations with transition
  // property names (the CSS property "transition-property"), in which case the
  // specification says that the time values list should repeat (hence the
  // modulos) until it is the same size as the property names list.
  const Time& time_at_index(int index) {
    return value()[index % value().size()];
  }

  bool operator==(const TimeListValue& other) const {
    return *value_ == *other.value_;
  }

  DEFINE_PROPERTY_VALUE_TYPE(TimeListValue);

 private:
  ~TimeListValue() OVERRIDE;

  const scoped_ptr<TimeList> value_;

  DISALLOW_COPY_AND_ASSIGN(TimeListValue);
};

}  // namespace cssom
}  // namespace cobalt

#endif  // CSSOM_TIME_LIST_VALUE_H_
