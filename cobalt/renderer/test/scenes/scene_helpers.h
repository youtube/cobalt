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

#ifndef COBALT_RENDERER_TEST_SCENES_SCENE_HELPERS_H_
#define COBALT_RENDERER_TEST_SCENES_SCENE_HELPERS_H_

#include "cobalt/math/size_f.h"
#include "cobalt/render_tree/animations/node_animations_map.h"
#include "cobalt/render_tree/node.h"

namespace cobalt {
namespace renderer {
namespace test {
namespace scenes {

// Passes the input value through a sawtooth function, useful for defining
// animations.
inline float Sawtooth(float x) { return x - static_cast<int>(x); }

struct RenderTreeWithAnimations {
  RenderTreeWithAnimations(
      scoped_refptr<render_tree::Node> render_tree,
      scoped_refptr<render_tree::animations::NodeAnimationsMap> animations)
      : render_tree(render_tree), animations(animations) {}

  scoped_refptr<render_tree::Node> render_tree;
  scoped_refptr<render_tree::animations::NodeAnimationsMap> animations;
};

RenderTreeWithAnimations AddBlankBackgroundToScene(
    const RenderTreeWithAnimations& scene,
    const math::SizeF& output_dimensions);

}  // namespace scenes
}  // namespace test
}  // namespace renderer
}  // namespace cobalt

#endif  // COBALT_RENDERER_TEST_SCENES_SCENE_HELPERS_H_
