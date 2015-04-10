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

#include "cobalt/layout/box.h"

#include "cobalt/layout/used_style.h"
#include "cobalt/math/transform_2d.h"
#include "cobalt/render_tree/brush.h"
#include "cobalt/render_tree/color_rgba.h"
#include "cobalt/render_tree/rect_node.h"

namespace cobalt {
namespace layout {

Box::Box(ContainingBlock* containing_block,
         const scoped_refptr<cssom::CSSStyleDeclarationData>& computed_style,
         UsedStyleProvider* used_style_provider)
    : containing_block_(containing_block),
      computed_style_(computed_style),
      used_style_provider_(used_style_provider),
      height_below_baseline_(0.0f) {}

void Box::AddToRenderTree(
    render_tree::CompositionNode::Builder* composition_node_builder) {
  render_tree::ColorRGBA used_background_color =
      GetUsedColor(computed_style_->background_color());
  // Assuming 8-bit resolution of alpha channel, only create background
  // when it's opaque enough to notice.
  if (used_background_color.a() >= 1.0 / 255.0) {
    scoped_ptr<render_tree::Brush> used_background_brush(
        new render_tree::SolidColorBrush(used_background_color));
    composition_node_builder->AddChild(
        new render_tree::RectNode(used_size(), used_background_brush.Pass()),
        GetTransform());
  }
}

math::Matrix3F Box::GetTransform() {
  return math::TranslateMatrix(offset_.x(), offset_.y());
}

}  // namespace layout
}  // namespace cobalt
