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

#include "base/bind.h"
#include "cobalt/layout/transition_render_tree_animations.h"
#include "cobalt/layout/used_style.h"
#include "cobalt/math/transform_2d.h"
#include "cobalt/render_tree/brush.h"
#include "cobalt/render_tree/color_rgba.h"
#include "cobalt/render_tree/rect_node.h"

using cobalt::render_tree::RectNode;
using cobalt::render_tree::animations::Animation;

namespace cobalt {
namespace layout {

Box::Box(
    ContainingBlock* containing_block,
    const scoped_refptr<const cssom::CSSStyleDeclarationData>& computed_style,
    const cssom::TransitionSet& transitions,
    UsedStyleProvider* used_style_provider)
    : containing_block_(containing_block),
      computed_style_(computed_style),
      used_style_provider_(used_style_provider),
      height_below_baseline_(0.0f),
      transitions_(transitions) {}

namespace {

void SetupRectNodeFromStyle(
    const scoped_refptr<const cssom::CSSStyleDeclarationData>& style,
    RectNode::Builder* rect_node_builder) {
  rect_node_builder->background_brush =
      scoped_ptr<render_tree::Brush>(new render_tree::SolidColorBrush(
          GetUsedColor(style->background_color())));
}

}  // namespace

void Box::AddToRenderTree(
    render_tree::CompositionNode::Builder* composition_node_builder,
    render_tree::animations::NodeAnimationsMap::Builder*
        node_animation_map_builder) {
  RectNode::Builder rect_node_builder(used_size(),
                                      scoped_ptr<render_tree::Brush>());
  SetupRectNodeFromStyle(computed_style_, &rect_node_builder);

  scoped_refptr<RectNode> rect_node(new RectNode(rect_node_builder.Pass()));
  composition_node_builder->AddChild(rect_node, GetTransform());

  AddTransitionAnimations<RectNode>(base::Bind(&SetupRectNodeFromStyle),
                                    *computed_style_, rect_node, transitions_,
                                    node_animation_map_builder);
}

math::Matrix3F Box::GetTransform() {
  return math::TranslateMatrix(offset_.x(), offset_.y());
}

}  // namespace layout
}  // namespace cobalt
