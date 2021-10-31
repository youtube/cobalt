// Copyright 2015 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/cssom/matrix_function.h"

#include "base/strings/stringprintf.h"
#include "cobalt/cssom/transform_function_visitor.h"

namespace cobalt {
namespace cssom {

MatrixFunction::MatrixFunction(const math::Matrix3F& matrix) : value_(matrix) {}

MatrixFunction::MatrixFunction(float m00, float m10, float m01, float m11,
                               float m02, float m12)
    : value_(math::Matrix3F::FromValues(m00, m01, m02, m10, m11, m12, 0.0f,
                                        0.0f, 1.0f)) {}

void MatrixFunction::Accept(TransformFunctionVisitor* visitor) const {
  visitor->VisitMatrix(this);
}

std::string MatrixFunction::ToString() const {
  return base::StringPrintf("matrix(%.7g, %.7g, %.7g, %.7g, %.7g, %.7g)",
                            value_(0, 0), value_(1, 0), value_(0, 1),
                            value_(1, 1), value_(0, 2), value_(1, 2));
}

}  // namespace cssom
}  // namespace cobalt
