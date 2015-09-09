/*
 * Copyright 2014 Google Inc. All Rights Reserved.
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

#ifndef CSSOM_TRANSLATE_FUNCTION_H_
#define CSSOM_TRANSLATE_FUNCTION_H_

#include "base/memory/ref_counted.h"
#include "cobalt/cssom/length_value.h"
#include "cobalt/cssom/percentage_value.h"
#include "cobalt/cssom/transform_function.h"

namespace cobalt {
namespace cssom {

// Specifies a translation by the given amount in the X, Y or Z direction.
//   http://www.w3.org/TR/css-transforms-1/#funcdef-translatex
//   http://www.w3.org/TR/css-transforms-1/#funcdef-translatey
//   http://www.w3.org/TR/css-transforms-1/#funcdef-translatez
class TranslateFunction : public TransformFunction {
 public:
  enum Axis {
    kXAxis,
    kYAxis,
    kZAxis,
  };
  enum OffsetType {
    kLength,
    kPercentage,
  };

  explicit TranslateFunction(Axis axis,
                             const scoped_refptr<PropertyValue>& offset)
      : axis_(axis), offset_(offset) {
    DCHECK(offset);
  }

  void Accept(TransformFunctionVisitor* visitor) const OVERRIDE;

  OffsetType offset_type() const;

  scoped_refptr<LengthValue> offset_as_length() const;
  scoped_refptr<PercentageValue> offset_as_percentage() const;

  Axis axis() const { return axis_; }

  std::string ToString() const OVERRIDE {
    char axis = 'X';
    switch (axis_) {
      case kXAxis: {
        axis = 'X';
        break;
      }
      case kYAxis: {
        axis = 'Y';
        break;
      }
      case kZAxis: {
        axis = 'Z';
        break;
      }
    }
    std::string result = "translate";
    result.push_back(axis);
    result.push_back('(');
    if (offset_) {
      result.append(offset_->ToString());
    }
    result.push_back(')');
    return result;
  }

  bool operator==(const TranslateFunction& other) const {
    return offset_->Equals(*other.offset_) && axis_ == other.axis_;
  }

  DEFINE_POLYMORPHIC_EQUATABLE_TYPE(TranslateFunction);

 private:
  const Axis axis_;
  const scoped_refptr<PropertyValue> offset_;
};

}  // namespace cssom
}  // namespace cobalt

#endif  // CSSOM_TRANSLATE_FUNCTION_H_
