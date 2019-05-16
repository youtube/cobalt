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

#include "cobalt/renderer/test/scenes/all_scenes_combined_scene.h"

#include "base/trace_event/trace_event.h"
#include "cobalt/math/matrix3_f.h"
#include "cobalt/math/size_f.h"
#include "cobalt/render_tree/composition_node.h"
#include "cobalt/renderer/test/scenes/growing_rect_scene.h"
#include "cobalt/renderer/test/scenes/image_wrap_scene.h"
#include "cobalt/renderer/test/scenes/marquee_scene.h"
#include "cobalt/renderer/test/scenes/spinning_sprites_scene.h"

using cobalt::math::Matrix3F;
using cobalt::math::SizeF;
using cobalt::render_tree::CompositionNode;
using cobalt::render_tree::ResourceProvider;
using cobalt::render_tree::animations::AnimateNode;

namespace cobalt {
namespace renderer {
namespace test {
namespace scenes {

scoped_refptr<render_tree::Node> CreateAllScenesCombinedScene(
    ResourceProvider* resource_provider, const SizeF& output_dimensions,
    base::TimeDelta start_time) {
  TRACE_EVENT0("cobalt::renderer_sandbox",
               "CreateAllScenesCombinedScene()");
  CompositionNode::Builder all_scenes_combined_scene_builder;

  // Compose all the render trees representing the sub-scenes through a
  // composition node, and merge the animations into one large set.
  scoped_refptr<render_tree::Node> growing_rect_scene =
      CreateGrowingRectScene(output_dimensions, start_time);
  all_scenes_combined_scene_builder.AddChild(growing_rect_scene);

  scoped_refptr<render_tree::Node> spinning_sprites_scene =
      CreateSpinningSpritesScene(resource_provider, output_dimensions,
                                 start_time);
  all_scenes_combined_scene_builder.AddChild(spinning_sprites_scene);

  scoped_refptr<render_tree::Node> image_wrap_scene =
      CreateImageWrapScene(resource_provider, output_dimensions, start_time);
  all_scenes_combined_scene_builder.AddChild(image_wrap_scene);

  scoped_refptr<render_tree::Node> marquee_scene =
      CreateMarqueeScene(resource_provider, output_dimensions, start_time);
  all_scenes_combined_scene_builder.AddChild(marquee_scene);

  return new AnimateNode(
      new CompositionNode(std::move(all_scenes_combined_scene_builder)));
}

}  // namespace scenes
}  // namespace test
}  // namespace renderer
}  // namespace cobalt
