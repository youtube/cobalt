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

#include "cobalt/renderer/test/scenes/scene_helpers.h"

#include "cobalt/math/matrix3_f.h"
#include "cobalt/math/size_f.h"
#include "cobalt/render_tree/color_rgba.h"
#include "cobalt/render_tree/composition_node.h"
#include "cobalt/render_tree/rect_node.h"

using cobalt::math::Matrix3F;
using cobalt::math::RectF;
using cobalt::math::SizeF;
using cobalt::render_tree::Brush;
using cobalt::render_tree::ColorRGBA;
using cobalt::render_tree::CompositionNode;
using cobalt::render_tree::RectNode;
using cobalt::render_tree::SolidColorBrush;

namespace cobalt {
namespace renderer {
namespace test {
namespace scenes {

scoped_refptr<render_tree::Node> AddBlankBackgroundToScene(
    const scoped_refptr<render_tree::Node>& scene,
    const SizeF& output_dimensions) {
  // Declare a rectangle that fills the background to effectively clear the
  // screen.
  CompositionNode::Builder with_background_scene_builder;
  with_background_scene_builder.AddChild(base::WrapRefCounted(new RectNode(
      RectF(output_dimensions), std::unique_ptr<Brush>(new SolidColorBrush(
                                    ColorRGBA(1.0f, 1.0f, 1.0f))))));
  with_background_scene_builder.AddChild(scene);

  return new render_tree::animations::AnimateNode(
      new CompositionNode(std::move(with_background_scene_builder)));
}

}  // namespace scenes
}  // namespace test
}  // namespace renderer
}  // namespace cobalt
