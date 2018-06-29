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

#include "cobalt/render_tree/brush_visitor.h"

#include "cobalt/math/point.h"
#include "cobalt/render_tree/brush.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace render_tree {

class MockBrushVisitor : public BrushVisitor {
 public:
  MOCK_METHOD1(Visit, void(const SolidColorBrush* solid_color_brush));
  MOCK_METHOD1(Visit, void(const LinearGradientBrush* linear_gradient_brush));
  MOCK_METHOD1(Visit, void(const RadialGradientBrush* radial_gradient_brush));
};

TEST(BrushVisitorTest, VisitsSolidColorBrush) {
  SolidColorBrush solid_color_brush(ColorRGBA(0.0f, 0.0f, 0.0f, 1.0f));
  MockBrushVisitor mock_visitor;
  EXPECT_CALL(mock_visitor, Visit(&solid_color_brush));
  solid_color_brush.Accept(&mock_visitor);
}

TEST(BrushVisitorTest, VisitsLinearGradientBrush) {
  LinearGradientBrush linear_gradient_brush(
      math::PointF(0.0f, 0.0f), math::PointF(1.0f, 1.0f),
      ColorRGBA(0.0f, 0.0f, 0.0f, 0.0f), ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f));
  MockBrushVisitor mock_visitor;
  EXPECT_CALL(mock_visitor, Visit(&linear_gradient_brush));
  linear_gradient_brush.Accept(&mock_visitor);
}

TEST(BrushVisitorTest, VisitsRadialGradientBrush) {
  RadialGradientBrush radial_gradient_brush(math::PointF(0.0f, 0.0f), 1.0f,
                                            ColorRGBA(0.0f, 0.0f, 0.0f, 0.0f),
                                            ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f));
  MockBrushVisitor mock_visitor;
  EXPECT_CALL(mock_visitor, Visit(&radial_gradient_brush));
  radial_gradient_brush.Accept(&mock_visitor);
}

}  // namespace render_tree
}  // namespace cobalt
