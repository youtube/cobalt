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

#ifndef CSSOM_TIMING_FUNCTION_LIST_VALUE_H_
#define CSSOM_TIMING_FUNCTION_LIST_VALUE_H_

#include "cobalt/cssom/scoped_ref_list_value.h"
#include "cobalt/cssom/timing_function.h"

namespace cobalt {
namespace cssom {

class TimingFunctionListValue : public ScopedRefListValue<TimingFunction> {
 public:
  explicit TimingFunctionListValue(
      scoped_ptr<ScopedRefListValue<TimingFunction>::Builder> value)
      : ScopedRefListValue(value.Pass()) {}

  void Accept(PropertyValueVisitor* visitor) OVERRIDE {
    visitor->VisitTimingFunctionList(this);
  }

  DEFINE_POLYMORPHIC_EQUATABLE_TYPE(TimingFunctionListValue);

 private:
  virtual ~TimingFunctionListValue() OVERRIDE {}
};

}  // namespace cssom
}  // namespace cobalt

#endif  // CSSOM_TIMING_FUNCTION_LIST_VALUE_H_
