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

#include "cobalt/cssom/time_list_value.h"

#include "cobalt/cssom/property_value_visitor.h"

namespace cobalt {
namespace cssom {

TimeListValue::TimeListValue(scoped_ptr<TimeList> value) :
    value_(value.Pass()) {
  DCHECK(value_.get());
}

TimeListValue::~TimeListValue() {}

void TimeListValue::Accept(PropertyValueVisitor* visitor) {
  visitor->VisitTimeList(this);
}

}  // namespace cssom
}  // namespace cobalt
