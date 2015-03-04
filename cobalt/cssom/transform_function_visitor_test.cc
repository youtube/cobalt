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

#include "cobalt/cssom/transform_function_visitor.h"

#include "cobalt/cssom/number_value.h"
#include "cobalt/cssom/scale_function.h"
#include "cobalt/cssom/translate_function.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace cssom {

class MockTransformFunctionVisitor : public TransformFunctionVisitor {
 public:
  MOCK_METHOD1(VisitScale, void(ScaleFunction* scale_function));
  MOCK_METHOD1(VisitTranslateX, void(TranslateXFunction* translate_x_function));
  MOCK_METHOD1(VisitTranslateY, void(TranslateYFunction* translate_y_function));
  MOCK_METHOD1(VisitTranslateZ, void(TranslateZFunction* translate_z_function));
};

TEST(TransformFunctionVisitorTest, VisitsScaleFunction) {
  ScaleFunction scale_function(new NumberValue(2), new NumberValue(2));
  MockTransformFunctionVisitor mock_visitor;
  EXPECT_CALL(mock_visitor, VisitScale(&scale_function));
  scale_function.Accept(&mock_visitor);
}

TEST(TransformFunctionVisitorTest, VisitsTranslateXFunction) {
  TranslateXFunction translate_x_function(new NumberValue(0));
  MockTransformFunctionVisitor mock_visitor;
  EXPECT_CALL(mock_visitor, VisitTranslateX(&translate_x_function));
  translate_x_function.Accept(&mock_visitor);
}

TEST(TransformFunctionVisitorTest, VisitsTranslateYFunction) {
  TranslateYFunction translate_y_function(new NumberValue(0));
  MockTransformFunctionVisitor mock_visitor;
  EXPECT_CALL(mock_visitor, VisitTranslateY(&translate_y_function));
  translate_y_function.Accept(&mock_visitor);
}

TEST(TransformFunctionVisitorTest, VisitsTranslateZFunction) {
  TranslateZFunction translate_z_function(new NumberValue(0));
  MockTransformFunctionVisitor mock_visitor;
  EXPECT_CALL(mock_visitor, VisitTranslateZ(&translate_z_function));
  translate_z_function.Accept(&mock_visitor);
}

}  // namespace cssom
}  // namespace cobalt
