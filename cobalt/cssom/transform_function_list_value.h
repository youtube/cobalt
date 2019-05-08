// Copyright 2014 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_CSSOM_TRANSFORM_FUNCTION_LIST_VALUE_H_
#define COBALT_CSSOM_TRANSFORM_FUNCTION_LIST_VALUE_H_

#include <memory>
#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "cobalt/base/polymorphic_equatable.h"
#include "cobalt/cssom/transform_function.h"
#include "cobalt/cssom/transform_property_value.h"
#include "cobalt/math/matrix3_f.h"

namespace cobalt {
namespace cssom {

// A list of transform functions that define how transformation is applied
// to the coordinate system an HTML element renders in.
//   https://www.w3.org/TR/css-transforms-1/#typedef-transform-list
class TransformFunctionListValue : public TransformPropertyValue {
 public:
  class Builder {
   private:
    typedef std::vector<std::unique_ptr<TransformFunction>> FunctionList;

   public:
    typedef FunctionList::const_iterator const_iterator;

    Builder();
    Builder(Builder&& other);

    void emplace_back(TransformFunction* function);
    void push_back(std::unique_ptr<TransformFunction>&& function);
    const_iterator begin() const { return functions_.begin(); }
    const_iterator end() const { return functions_.end(); }
    size_t size() const { return functions_.size(); }
    const std::unique_ptr<TransformFunction>& operator[](size_t index) const {
      return functions_[index];
    }

    // This checks whether any function in the list has the given trait.
    bool HasTrait(TransformFunction::Trait trait) const {
      return (traits_ & trait) != 0;
    }

   private:
    FunctionList functions_;
    uint32 traits_;
  };

  explicit TransformFunctionListValue(Builder&& value)
      : value_(std::move(value)) {}

  std::string ToString() const override;

  bool HasTrait(TransformFunction::Trait trait) const override {
    return value_.HasTrait(trait);
  }

  math::Matrix3F ToMatrix(const math::SizeF& used_size,
      const scoped_refptr<ui_navigation::NavItem>& used_ui_nav_focus)
      const override;

  bool operator==(const TransformFunctionListValue& other) const;

  const Builder& value() const { return value_; }

  DEFINE_POLYMORPHIC_EQUATABLE_TYPE(TransformFunctionListValue);

 private:
  ~TransformFunctionListValue() override {}

  const Builder value_;
};

}  // namespace cssom
}  // namespace cobalt

#endif  // COBALT_CSSOM_TRANSFORM_FUNCTION_LIST_VALUE_H_
