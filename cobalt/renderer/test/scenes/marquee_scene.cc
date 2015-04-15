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

#include "cobalt/renderer/test/scenes/marquee_scene.h"

#include "base/bind.h"
#include "cobalt/math/size_f.h"
#include "cobalt/math/transform_2d.h"
#include "cobalt/render_tree/composition_node.h"
#include "cobalt/render_tree/rect_node.h"
#include "cobalt/render_tree/text_node.h"

using cobalt::math::Matrix3F;
using cobalt::math::RectF;
using cobalt::math::SizeF;
using cobalt::math::TranslateMatrix;
using cobalt::render_tree::animations::Animation;
using cobalt::render_tree::animations::AnimationList;
using cobalt::render_tree::animations::NodeAnimationsMap;
using cobalt::render_tree::Brush;
using cobalt::render_tree::ColorRGBA;
using cobalt::render_tree::CompositionNode;
using cobalt::render_tree::Font;
using cobalt::render_tree::Node;
using cobalt::render_tree::RectNode;
using cobalt::render_tree::ResourceProvider;
using cobalt::render_tree::SolidColorBrush;
using cobalt::render_tree::TextNode;

namespace cobalt {
namespace renderer {
namespace test {
namespace scenes {

namespace {

void AnimateMarqueeElement(base::Time start_time, int child_index,
                           const RectF& text_bounds,
                           const SizeF& output_dimensions,
                           CompositionNode::Builder* composition_node,
                           base::Time time) {
  // Calculate the animated x position of the text.
  const float kTextMarqueePeriod = 10.0f;
  float text_start_position = -text_bounds.right();
  float text_end_position = output_dimensions.width() - text_bounds.x();
  float periodic_position =
      Sawtooth((time - start_time).InSecondsF() / kTextMarqueePeriod);
  float x_position =
      text_start_position +
      (text_end_position - text_start_position) * periodic_position;

  // Translate the specified child element by the appropriate amount.
  CompositionNode::ComposedChild* child =
      composition_node->GetChild(child_index);
  child->transform = child->transform * TranslateMatrix(x_position, 0);
}

}  // namespace

RenderTreeWithAnimations CreateMarqueeScene(
    ResourceProvider* resource_provider,
    const math::SizeF& output_dimensions,
    base::Time start_time) {
  CompositionNode::Builder marquee_scene_builder;
  NodeAnimationsMap::Builder animations;

  const std::string kMarqueeText("YouTube");

  // All animations will affect the top-level composition node, and so they
  // will all go in this same list.
  AnimationList<CompositionNode>::Builder marquee_animations;

  // Load a font for use with text rendering.
  const float kFontSize = 40.0f;
  scoped_refptr<Font> test_font = resource_provider->GetPreInstalledFont(
      "Droid Sans", ResourceProvider::kNormal, kFontSize);

  // Use information about the string we are rendering to properly position
  // it in the vertical center of the screen and far enough offscreen that
  // a switch from the right side to the left is not noticed.
  RectF text_bounds = test_font->GetBounds(kMarqueeText);

  // Center the text's bounding box vertically on the screen.
  float y_position =
      (output_dimensions.height() / 2.0f) - text_bounds.y() -
          (text_bounds.height() / 2.0f);

  // Add a background rectangle to the text in order to demonstrate the
  // relationship between the text's origin and its bounding box.
  marquee_scene_builder.AddChild(
      make_scoped_refptr(new RectNode(
          SizeF(text_bounds.width(), text_bounds.height()),
          scoped_ptr<Brush>(new SolidColorBrush(ColorRGBA(0.7f, 0.2f, 1.0f))))),
      TranslateMatrix(text_bounds.x(), y_position + text_bounds.y()));
  marquee_animations.animations.push_back(base::Bind(
      &AnimateMarqueeElement, start_time, 0, text_bounds, output_dimensions));

  // Add the actual text node to our composition.
  marquee_scene_builder.AddChild(
      make_scoped_refptr(
          new TextNode(kMarqueeText, test_font, ColorRGBA(0.0f, 0.0f, 0.0f))),
      TranslateMatrix(0, y_position));
  marquee_animations.animations.push_back(base::Bind(
      &AnimateMarqueeElement, start_time, 1, text_bounds, output_dimensions));

  scoped_refptr<CompositionNode> marquee_composition(
      new CompositionNode(marquee_scene_builder.Pass()));

  animations.Add(marquee_composition,
                 make_scoped_refptr(new AnimationList<CompositionNode>(
                     marquee_animations.Pass())));

  return RenderTreeWithAnimations(marquee_composition,
                                  new NodeAnimationsMap(animations.Pass()));
}

}  // namespace scenes
}  // namespace test
}  // namespace renderer
}  // namespace cobalt
