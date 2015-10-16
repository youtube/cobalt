/*
 * Copyright 2015 Google Inc. All Rights Reserved.
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

#include "cobalt/renderer/test/scenes/scaling_text_scene.h"

#include <cmath>

#include "base/bind.h"
#include "cobalt/math/size_f.h"
#include "cobalt/math/transform_2d.h"
#include "cobalt/render_tree/composition_node.h"
#include "cobalt/render_tree/rect_node.h"
#include "cobalt/render_tree/text_node.h"

using cobalt::math::Matrix3F;
using cobalt::math::RectF;
using cobalt::math::SizeF;
using cobalt::math::ScaleMatrix;
using cobalt::math::TranslateMatrix;
using cobalt::render_tree::ColorRGBA;
using cobalt::render_tree::CompositionNode;
using cobalt::render_tree::Font;
using cobalt::render_tree::ResourceProvider;
using cobalt::render_tree::TextNode;
using cobalt::render_tree::animations::Animation;
using cobalt::render_tree::animations::AnimationList;
using cobalt::render_tree::animations::NodeAnimationsMap;

namespace cobalt {
namespace renderer {
namespace test {
namespace scenes {

namespace {

void ScaleText(CompositionNode::Builder* composition_node,
               base::TimeDelta time) {
  DCHECK_EQ(1, composition_node->composed_children().size());
  composition_node->GetChild(0)->transform =
      ScaleMatrix(sin((time.InSecondsF() / 5) * 2 * M_PI) + 1.5);
}

}  // namespace

RenderTreeWithAnimations CreateScalingTextScene(
    ResourceProvider* resource_provider, const math::SizeF& output_dimensions,
    base::TimeDelta start_time) {
  const std::string kText(
      "ABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890abcdefghijklmnopqrstuvwxyz");

  // Load a font for use with text rendering.
  const float kBaseFontSize = 20.0f;
  scoped_refptr<Font> font = resource_provider->GetPreInstalledFont(
      "Droid Sans", render_tree::kNormal, kBaseFontSize);

  // Add the actual text node to our composition.
  CompositionNode::Builder text_collection_builder;
  for (int i = 0; i < 60; ++i) {
    text_collection_builder.AddChild(
        make_scoped_refptr(
            new TextNode(kText, font, ColorRGBA(0.0f, 0.0f, 0.0f))),
        TranslateMatrix(100, 50 + i * (10)) * ScaleMatrix(1 + i * 0.03f));
  }

  CompositionNode::Builder scaling_text_scene_builder;
  scaling_text_scene_builder.AddChild(
      make_scoped_refptr(new CompositionNode(text_collection_builder.Pass())),
      Matrix3F::Identity());
  scoped_refptr<CompositionNode> scaling_text_scene(
      new CompositionNode(scaling_text_scene_builder.Pass()));

  // Setup the scaling animation on the text
  AnimationList<CompositionNode>::Builder scale_animation;
  scale_animation.animations.push_back(base::Bind(&ScaleText));

  NodeAnimationsMap::Builder animations;
  animations.Add(scaling_text_scene,
                 make_scoped_refptr(new AnimationList<CompositionNode>(
                     scale_animation.Pass())));

  return RenderTreeWithAnimations(scaling_text_scene,
                                  new NodeAnimationsMap(animations.Pass()));
}

}  // namespace scenes
}  // namespace test
}  // namespace renderer
}  // namespace cobalt
