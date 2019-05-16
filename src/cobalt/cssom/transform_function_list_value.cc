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

#include "cobalt/cssom/transform_function_list_value.h"

namespace cobalt {
namespace cssom {

TransformFunctionListValue::Builder::Builder()
    : traits_(0) {}

TransformFunctionListValue::Builder::Builder(Builder&& other)
    : functions_(std::move(other.functions_)),
      traits_(other.traits_) {}

void TransformFunctionListValue::Builder::emplace_back(
    TransformFunction* function) {
  traits_ |= function->Traits();
  functions_.emplace_back(function);
}

void TransformFunctionListValue::Builder::push_back(
    std::unique_ptr<TransformFunction>&& function) {
  traits_ |= function->Traits();
  functions_.push_back(std::move(function));
}

std::string TransformFunctionListValue::ToString() const {
  std::string result;
  for (size_t i = 0; i < value().size(); ++i) {
    if (!result.empty()) result.append(" ");
    result.append(value()[i]->ToString());
  }
  return result;
}

math::Matrix3F TransformFunctionListValue::ToMatrix(
    const math::SizeF& used_size,
    const scoped_refptr<ui_navigation::NavItem>& used_ui_nav_focus) const {
  math::Matrix3F matrix(math::Matrix3F::Identity());
  for (const auto& function : value()) {
    matrix = matrix * function->ToMatrix(used_size, used_ui_nav_focus);
  }
  return matrix;
}

bool TransformFunctionListValue::operator==(
    const TransformFunctionListValue& other) const {
  if (value().size() != other.value().size()) {
    return false;
  }

  for (size_t i = 0; i < value().size(); ++i) {
    if (!value()[i]->Equals(*(other.value()[i]))) {
      return false;
    }
  }

  return true;
}

}  // namespace cssom
}  // namespace cobalt
