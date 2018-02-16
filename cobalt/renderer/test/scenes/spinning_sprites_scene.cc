// Copyright 2015 Google Inc. All Rights Reserved.
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

#include "cobalt/renderer/test/scenes/spinning_sprites_scene.h"

#include <cmath>
#include <vector>

#include "base/debug/trace_event.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "base/rand_util.h"
#include "cobalt/base/cobalt_paths.h"
#include "cobalt/math/point_f.h"
#include "cobalt/math/transform_2d.h"
#include "cobalt/render_tree/composition_node.h"
#include "cobalt/render_tree/image.h"
#include "cobalt/render_tree/image_node.h"
#include "cobalt/render_tree/matrix_transform_node.h"
#include "cobalt/renderer/test/png_utils/png_decode.h"

using cobalt::math::Matrix3F;
using cobalt::math::PointF;
using cobalt::math::RectF;
using cobalt::math::RotateMatrix;
using cobalt::math::ScaleMatrix;
using cobalt::math::SizeF;
using cobalt::math::TranslateMatrix;
using cobalt::render_tree::animations::AnimateNode;
using cobalt::render_tree::animations::Animation;
using cobalt::render_tree::animations::AnimationList;
using cobalt::render_tree::CompositionNode;
using cobalt::render_tree::Image;
using cobalt::render_tree::ImageNode;
using cobalt::render_tree::MatrixTransformNode;
using cobalt::render_tree::Node;
using cobalt::render_tree::ResourceProvider;
using cobalt::renderer::test::png_utils::DecodePNGToRenderTreeImage;

namespace cobalt {
namespace renderer {
namespace test {
namespace scenes {

namespace {

struct SpriteInfo {
  float scale;
  float rotation_speed;  // In revolutions per second.
  PointF position;       // In normalized device coordinates.
};

float RandRange(float lower, float upper) {
  return base::RandDouble() * (upper - lower) + lower;
}

void AnimateSprite(base::TimeDelta start_time, const SpriteInfo& sprite_info,
                   const RectF& child_rect,
                   MatrixTransformNode::Builder* transform_node,
                   base::TimeDelta time) {
  // Setup child node's scale and rotation and translation.
  float theta =
      (time - start_time).InSecondsF() * sprite_info.rotation_speed * 2 * M_PI;
  transform_node->transform =
      transform_node->transform * RotateMatrix(theta) *
      ScaleMatrix(sprite_info.scale) *
      TranslateMatrix(  // Set child center as origin.
          -0.5 * child_rect.width(), -0.5 * child_rect.height());
}

std::vector<SpriteInfo> CreateSpriteInfos() {
  // Create a set of randomly positioned and sized sprites.
  std::vector<SpriteInfo> sprite_infos;
  const int kNumSprites = 40;
  for (int i = 0; i < kNumSprites; ++i) {
    const float kMinScale = 0.1f;
    const float kMaxScale = 0.7f;
    SpriteInfo sprite_info;
    sprite_info.scale = RandRange(kMinScale, kMaxScale);

    sprite_info.position.set_x(static_cast<float>(base::RandDouble()));
    sprite_info.position.set_y(static_cast<float>(base::RandDouble()));

    const float kMinRotationSpeed = 0.01f;
    const float kMaxRotationSpeed = 0.3f;
    sprite_info.rotation_speed =
        RandRange(kMinRotationSpeed, kMaxRotationSpeed);

    sprite_infos.push_back(sprite_info);
  }
  return sprite_infos;
}

scoped_refptr<Image> GetTestImage(ResourceProvider* resource_provider) {
  // Load a test image from disk.
  FilePath data_directory;
  CHECK(PathService::Get(base::DIR_TEST_DATA, &data_directory));
  return DecodePNGToRenderTreeImage(
      data_directory.Append(FILE_PATH_LITERAL("test"))
          .Append(FILE_PATH_LITERAL("scenes"))
          .Append(FILE_PATH_LITERAL("demo_image.png")),
      resource_provider);
}

}  // namespace

scoped_refptr<render_tree::Node> CreateSpinningSpritesScene(
    ResourceProvider* resource_provider, const math::SizeF& output_dimensions,
    base::TimeDelta start_time) {
  // Create an image for each SpriteInfo we have in our sprite_infos vector.
  // They will be positioned and scaled according to their SpriteInfo settings,
  // and rotated according to time.
  CompositionNode::Builder spinning_sprites_scene_builder;
  AnimateNode::Builder animations;

  // Now, setup a plurality of spinning images that contain the test image.
  std::vector<SpriteInfo> sprite_infos = CreateSpriteInfos();
  scoped_refptr<Image> test_image = GetTestImage(resource_provider);
  AnimationList<CompositionNode>::Builder sprite_animation_list;
  for (int i = 0; i < sprite_infos.size(); ++i) {
    const SpriteInfo& sprite_info = sprite_infos[i];

    // Create the child image node that references the image data
    scoped_refptr<ImageNode> image_node(new ImageNode(test_image));

    scoped_refptr<MatrixTransformNode> transformed_node(new MatrixTransformNode(
        image_node,
        TranslateMatrix(  // Place the image within the scene.
            sprite_info.position.x() * output_dimensions.width(),
            sprite_info.position.y() * output_dimensions.height())));

    // Add the child image node to the list of composed nodes.
    spinning_sprites_scene_builder.AddChild(transformed_node);

    animations.Add(transformed_node,
                   base::Bind(&AnimateSprite, start_time, sprite_info,
                              image_node->data().destination_rect));
  }

  scoped_refptr<CompositionNode> spinning_sprites_composition(
      new CompositionNode(spinning_sprites_scene_builder.Pass()));

  return new AnimateNode(animations, spinning_sprites_composition);
}

}  // namespace scenes
}  // namespace test
}  // namespace renderer
}  // namespace cobalt
