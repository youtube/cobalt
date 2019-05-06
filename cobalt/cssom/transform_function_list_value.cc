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

TransformMatrix TransformFunctionListValue::ToMatrix() const {
  // Iterate through all transforms in the transform list appending them
  // to our transform_matrix.
  TransformMatrix matrix;
  for (Builder::const_iterator iter = value().begin(); iter != value().end();
       ++iter) {
    matrix.PostMultiplyByTransform(iter->get());
  }

  return matrix;
}

std::string TransformFunctionListValue::ToString() const {
  std::string result;
  for (size_t i = 0; i < value().size(); ++i) {
    if (!result.empty()) result.append(" ");
    result.append(value()[i]->ToString());
  }
  return result;
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
