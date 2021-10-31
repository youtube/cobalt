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

#include "cobalt/renderer/test/scenes/scaling_text_scene.h"

#include <cmath>
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
using cobalt::math::ScaleMatrix;
using cobalt::math::TranslateMatrix;
using cobalt::render_tree::ColorRGBA;
using cobalt::render_tree::CompositionNode;
using cobalt::render_tree::Font;
using cobalt::render_tree::FontStyle;
using cobalt::render_tree::GlyphBuffer;
using cobalt::render_tree::MatrixTransformNode;
using cobalt::render_tree::ResourceProvider;
using cobalt::render_tree::TextNode;
using cobalt::render_tree::animations::Animation;
using cobalt::render_tree::animations::AnimationList;
using cobalt::render_tree::animations::AnimateNode;

namespace cobalt {
namespace renderer {
namespace test {
namespace scenes {

namespace {

void ScaleText(MatrixTransformNode::Builder* transform_node,
               base::TimeDelta time) {
  transform_node->transform =
      ScaleMatrix(sin((time.InSecondsF() / 5) * 2 * M_PI) + 1.5);
}

}  // namespace

scoped_refptr<render_tree::Node> CreateScalingTextScene(
    ResourceProvider* resource_provider, const math::SizeF& output_dimensions,
    base::TimeDelta start_time) {
  const std::string kText(
      "ABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890abcdefghijklmnopqrstuvwxyz");

  // Load a font for use with text rendering.
  const float kBaseFontSize = 20.0f;
  scoped_refptr<Font> font =
      resource_provider->GetLocalTypeface("Roboto", FontStyle())
          ->CreateFontWithSize(kBaseFontSize);
  scoped_refptr<render_tree::GlyphBuffer> glyph_buffer =
      resource_provider->CreateGlyphBuffer(kText, font);
  RectF bounds(glyph_buffer->GetBounds());

  // Add the actual text node to our composition.
  CompositionNode::Builder text_collection_builder;
  for (int i = 0; i < 60; ++i) {
    text_collection_builder.AddChild(new MatrixTransformNode(
        new TextNode(math::Vector2dF(0, 0), glyph_buffer,
                     ColorRGBA(0.0f, 0.0f, 0.0f)),
        TranslateMatrix(100, 50 + i * (10)) * ScaleMatrix(1 + i * 0.03f)));
  }

  scoped_refptr<MatrixTransformNode> scaling_text_node(new MatrixTransformNode(
      new CompositionNode(std::move(text_collection_builder)),
      Matrix3F::Identity()));

  // Setup the scaling animation on the text
  AnimateNode::Builder animations;
  animations.Add(scaling_text_node, base::Bind(&ScaleText));

  return new AnimateNode(animations, scaling_text_node);
}

}  // namespace scenes
}  // namespace test
}  // namespace renderer
}  // namespace cobalt
