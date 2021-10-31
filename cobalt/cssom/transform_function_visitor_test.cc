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

#include "cobalt/cssom/transform_function_visitor.h"

#include "cobalt/cssom/cobalt_ui_nav_focus_transform_function.h"
#include "cobalt/cssom/cobalt_ui_nav_spotlight_transform_function.h"
#include "cobalt/cssom/length_value.h"
#include "cobalt/cssom/matrix_function.h"
#include "cobalt/cssom/number_value.h"
#include "cobalt/cssom/rotate_function.h"
#include "cobalt/cssom/scale_function.h"
#include "cobalt/cssom/translate_function.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace cssom {

class MockTransformFunctionVisitor : public TransformFunctionVisitor {
 public:
  MOCK_METHOD1(VisitMatrix, void(const MatrixFunction* matrix_function));
  MOCK_METHOD1(VisitRotate, void(const RotateFunction* rotate_function));
  MOCK_METHOD1(VisitScale, void(const ScaleFunction* scale_function));
  MOCK_METHOD1(VisitTranslate,
               void(const TranslateFunction* translate_function));
  MOCK_METHOD1(VisitCobaltUiNavFocusTransform,
               void(const CobaltUiNavFocusTransformFunction* function));
  MOCK_METHOD1(VisitCobaltUiNavSpotlightTransform,
               void(const CobaltUiNavSpotlightTransformFunction* function));
};

TEST(TransformFunctionVisitorTest, VisitsMatrixFunction) {
  MatrixFunction matrix_function(1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f);
  MockTransformFunctionVisitor mock_visitor;
  EXPECT_CALL(mock_visitor, VisitMatrix(&matrix_function));
  matrix_function.Accept(&mock_visitor);
}

TEST(TransformFunctionVisitorTest, VisitsRotateFunction) {
  RotateFunction rotate_function(1.0f);
  MockTransformFunctionVisitor mock_visitor;
  EXPECT_CALL(mock_visitor, VisitRotate(&rotate_function));
  rotate_function.Accept(&mock_visitor);
}

TEST(TransformFunctionVisitorTest, VisitsScaleFunction) {
  ScaleFunction scale_function(2, 2);
  MockTransformFunctionVisitor mock_visitor;
  EXPECT_CALL(mock_visitor, VisitScale(&scale_function));
  scale_function.Accept(&mock_visitor);
}

TEST(TransformFunctionVisitorTest, VisitsTranslateXFunction) {
  TranslateFunction translate_function(TranslateFunction::kXAxis,
                                       new LengthValue(0, kPixelsUnit));
  MockTransformFunctionVisitor mock_visitor;
  EXPECT_CALL(mock_visitor, VisitTranslate(&translate_function));
  translate_function.Accept(&mock_visitor);
}

TEST(TransformFunctionVisitorTest, VisitsCobaltUiNavFocusTransform) {
  CobaltUiNavFocusTransformFunction focus_function(1.0f, 1.0f);
  MockTransformFunctionVisitor mock_visitor;
  EXPECT_CALL(mock_visitor, VisitCobaltUiNavFocusTransform(&focus_function));
  focus_function.Accept(&mock_visitor);
}

TEST(TransformFunctionVisitorTest, VisitsCobaltUiNavSpotlightTransform) {
  CobaltUiNavSpotlightTransformFunction spotlight_function;
  MockTransformFunctionVisitor mock_visitor;
  EXPECT_CALL(mock_visitor,
              VisitCobaltUiNavSpotlightTransform(&spotlight_function));
  spotlight_function.Accept(&mock_visitor);
}

}  // namespace cssom
}  // namespace cobalt
