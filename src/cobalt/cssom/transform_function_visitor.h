// Copyright 2014 Google Inc. All Rights Reserved.
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

#ifndef COBALT_CSSOM_TRANSFORM_FUNCTION_VISITOR_H_
#define COBALT_CSSOM_TRANSFORM_FUNCTION_VISITOR_H_

namespace cobalt {
namespace cssom {

class MatrixFunction;
class RotateFunction;
class ScaleFunction;
class TranslateFunction;

// Type-safe branching on a class hierarchy of transform functions,
// implemented after a classical GoF pattern (see
// http://en.wikipedia.org/wiki/Visitor_pattern#Java_example).
class TransformFunctionVisitor {
 public:
  virtual void VisitMatrix(const MatrixFunction* matrix_function) = 0;
  virtual void VisitRotate(const RotateFunction* rotate_function) = 0;
  virtual void VisitScale(const ScaleFunction* scale_function) = 0;
  virtual void VisitTranslate(const TranslateFunction* translate_function) = 0;

 protected:
  ~TransformFunctionVisitor() {}
};

}  // namespace cssom
}  // namespace cobalt

#endif  // COBALT_CSSOM_TRANSFORM_FUNCTION_VISITOR_H_
