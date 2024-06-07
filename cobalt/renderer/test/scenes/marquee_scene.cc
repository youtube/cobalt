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

#include "cobalt/renderer/test/scenes/marquee_scene.h"

#include <memory>
#include <string>

#include "base/bind.h"
#include "cobalt/math/size_f.h"
#include "cobalt/math/transform_2d.h"
#include "cobalt/render_tree/composition_node.h"
#include "cobalt/render_tree/font.h"
#include "cobalt/render_tree/glyph_buffer.h"
#include "cobalt/render_tree/matrix_transform_node.h"
#include "cobalt/render_tree/rect_node.h"
#include "cobalt/render_tree/text_node.h"
#include "cobalt/render_tree/typeface.h"

using cobalt::math::Matrix3F;
using cobalt::math::RectF;
using cobalt::math::SizeF;
using cobalt::math::TranslateMatrix;
using cobalt::render_tree::animations::AnimateNode;
using cobalt::render_tree::animations::Animation;
using cobalt::render_tree::animations::AnimationList;
using cobalt::render_tree::Brush;
using cobalt::render_tree::ColorRGBA;
using cobalt::render_tree::CompositionNode;
using cobalt::render_tree::Font;
using cobalt::render_tree::FontStyle;
using cobalt::render_tree::GlyphBuffer;
using cobalt::render_tree::MatrixTransformNode;
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

scoped_refptr<GlyphBuffer> CreateGlyphBuffer(
    ResourceProvider* resource_provider, FontStyle font_style, int font_size,
    const std::string& text) {
  scoped_refptr<Font> font =
      resource_provider->GetLocalTypeface("Roboto", font_style)
          ->CreateFontWithSize(font_size);
  return resource_provider->CreateGlyphBuffer(text, font);
}

void AnimateMarqueeElement(base::TimeDelta start_time, const RectF& text_bounds,
                           const SizeF& output_dimensions,
                           MatrixTransformNode::Builder* transform_node,
                           base::TimeDelta time) {
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
  transform_node->transform = TranslateMatrix(x_position, 0);
}

}  // namespace

scoped_refptr<render_tree::Node> CreateMarqueeScene(
    ResourceProvider* resource_provider, const math::SizeF& output_dimensions,
    base::TimeDelta start_time) {
  CompositionNode::Builder marquee_scene_builder;
  AnimateNode::Builder animations;

  const std::string kMarqueeText("YouTube");

  // All animations will affect the top-level composition node, and so they
  // will all go in this same list.
  AnimationList<CompositionNode>::Builder marquee_animations;

  // Load a font for use with text rendering.
  const float kFontSize = 40.0f;
  scoped_refptr<render_tree::GlyphBuffer> glyph_buffer = CreateGlyphBuffer(
      resource_provider, FontStyle(), kFontSize, kMarqueeText);
  RectF text_bounds(glyph_buffer->GetBounds());

  // Use information about the string we are rendering to properly position
  // it in the vertical center of the screen and far enough offscreen that
  // a switch from the right side to the left is not noticed.

  // Center the text's bounding box vertically on the screen.
  float y_position =
      (output_dimensions.height() / 2.0f) - text_bounds.y() -
          (text_bounds.height() / 2.0f);

  // Add a background rectangle to the text in order to demonstrate the
  // relationship between the text's origin and its bounding box.
  marquee_scene_builder.AddChild(base::WrapRefCounted(
      new RectNode(RectF(text_bounds.x(), y_position + text_bounds.y(),
                         text_bounds.width(), text_bounds.height()),
                   std::unique_ptr<Brush>(
                       new SolidColorBrush(ColorRGBA(0.7f, 0.2f, 1.0f))))));

  // Add the actual text node to our composition.
  marquee_scene_builder.AddChild(new TextNode(math::Vector2dF(0, y_position),
                                              glyph_buffer,
                                              ColorRGBA(0.0f, 0.0f, 0.0f)));

  scoped_refptr<MatrixTransformNode> marquee_node(new MatrixTransformNode(
      new CompositionNode(std::move(marquee_scene_builder)),
      Matrix3F::Identity()));

  animations.Add(marquee_node, base::Bind(&AnimateMarqueeElement, start_time,
                                          text_bounds, output_dimensions));

  return new AnimateNode(animations, marquee_node);
}

}  // namespace scenes
}  // namespace test
}  // namespace renderer
}  // namespace cobalt
