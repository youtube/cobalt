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

#include <memory>

#include "cobalt/renderer/test/scenes/growing_rect_scene.h"

#include "base/bind.h"
#include "cobalt/math/size_f.h"
#include "cobalt/math/transform_2d.h"
#include "cobalt/render_tree/color_rgba.h"
#include "cobalt/render_tree/matrix_transform_node.h"
#include "cobalt/render_tree/rect_node.h"

using cobalt::math::Matrix3F;
using cobalt::math::ScaleMatrix;
using cobalt::math::SizeF;
using cobalt::math::TranslateMatrix;
using cobalt::render_tree::animations::AnimateNode;
using cobalt::render_tree::animations::Animation;
using cobalt::render_tree::animations::AnimationList;
using cobalt::render_tree::Brush;
using cobalt::render_tree::ColorRGBA;
using cobalt::render_tree::MatrixTransformNode;
using cobalt::render_tree::Node;
using cobalt::render_tree::RectNode;
using cobalt::render_tree::SolidColorBrush;

namespace cobalt {
namespace renderer {
namespace test {
namespace scenes {

namespace {

void AnimateGrowingRectComposition(base::TimeDelta start_time,
                                   const math::SizeF& output_dimensions,
                                   MatrixTransformNode::Builder* transform_node,
                                   base::TimeDelta time) {
  const float kGrowingRectPeriod = 5.0f;
  float rect_size_scale =
      Sawtooth((time - start_time).InSecondsF() / kGrowingRectPeriod);
  SizeF rect_size(ScaleSize(output_dimensions, rect_size_scale));

  // Scale and translate the matrix based on how much time has passed.
  transform_node->transform =
      TranslateMatrix((output_dimensions.width() - rect_size.width()) / 2,
                      (output_dimensions.height() - rect_size.height()) / 2) *
      ScaleMatrix(rect_size_scale);
}

}  // namespace

scoped_refptr<render_tree::Node> CreateGrowingRectScene(
    const math::SizeF& output_dimensions, base::TimeDelta start_time) {
  // Create a centered, sawtoothed-growing black rectangle.  We need a
  // composition node for this so that the rectangle's position can be set
  // and animated.  We also use the composition node to animate the RectNode's
  // size.
  AnimateNode::Builder animations;

  scoped_refptr<RectNode> growing_rect_node(
      new RectNode(math::RectF(output_dimensions),
                   std::unique_ptr<Brush>(
                       new SolidColorBrush(ColorRGBA(0.2f, 0.2f, 0.2f)))));

  scoped_refptr<MatrixTransformNode> transformed_growing_rect(
      new MatrixTransformNode(growing_rect_node, Matrix3F::Identity()));

  animations.Add(transformed_growing_rect,
                 base::Bind(&AnimateGrowingRectComposition, start_time,
                            output_dimensions));

  return new AnimateNode(animations, transformed_growing_rect);
}

}  // namespace scenes
}  // namespace test
}  // namespace renderer
}  // namespace cobalt
