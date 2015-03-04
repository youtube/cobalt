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

#ifndef CSSOM_TRANSFORM_FUNCTION_VISITOR_H_
#define CSSOM_TRANSFORM_FUNCTION_VISITOR_H_

namespace cobalt {
namespace cssom {

class ScaleFunction;
class TranslateXFunction;
class TranslateYFunction;
class TranslateZFunction;

// Type-safe branching on a class hierarchy of transform functions,
// implemented after a classical GoF pattern (see
// http://en.wikipedia.org/wiki/Visitor_pattern#Java_example).
class TransformFunctionVisitor {
 public:
  virtual void VisitScale(ScaleFunction* scale_function) = 0;
  virtual void VisitTranslateX(TranslateXFunction* translate_x_function) = 0;
  virtual void VisitTranslateY(TranslateYFunction* translate_y_function) = 0;
  virtual void VisitTranslateZ(TranslateZFunction* translate_z_function) = 0;

 protected:
  ~TransformFunctionVisitor() {}
};

}  // namespace cssom
}  // namespace cobalt

#endif  // CSSOM_TRANSFORM_FUNCTION_VISITOR_H_
