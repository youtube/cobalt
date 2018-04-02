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

#include "cobalt/renderer/test/scenes/image_wrap_scene.h"

#include <cmath>

#include "base/bind.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "cobalt/math/size_f.h"
#include "cobalt/math/transform_2d.h"
#include "cobalt/render_tree/color_rgba.h"
#include "cobalt/render_tree/composition_node.h"
#include "cobalt/render_tree/image_node.h"
#include "cobalt/render_tree/matrix_transform_node.h"
#include "cobalt/render_tree/rect_node.h"
#include "cobalt/render_tree/resource_provider.h"
#include "cobalt/renderer/test/png_utils/png_decode.h"

using cobalt::math::Matrix3F;
using cobalt::math::SizeF;
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

void AnimateImage(base::TimeDelta start_time,
                  ImageNode::Builder* composition_node, base::TimeDelta time) {
  float elapsed_time_in_seconds = (time - start_time).InSecondsF();

  // Determine the scale of the local image transform, given the current
  // time.
  const float kScalePeriodInSeconds = 1.5f;
  const float kScaleAmplitude = 0.4f;
  const float kScalePhaseShift = 0.55f;
  float scale =
      kScalePhaseShift + kScaleAmplitude *
      sin(2 * M_PI * elapsed_time_in_seconds / kScalePeriodInSeconds);

  // Determine the rotation of the local transform given the current time.
  const float kRotationPeriod = 3.0f;
  float rotation = 2 * M_PI * elapsed_time_in_seconds / kRotationPeriod;

  // Combine the scale and rotation together, translating to the center to
  // use that as the origin.
  composition_node->local_transform =
      math::TranslateMatrix(0.5f, 0.5f) *
      math::ScaleMatrix(scale) * math::RotateMatrix(rotation);
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

scoped_refptr<render_tree::Node> CreateImageWrapScene(
    ResourceProvider* resource_provider, const math::SizeF& output_dimensions,
    base::TimeDelta start_time) {
  AnimateNode::Builder animations;

  // Create an image node that will have its local_transform animated to
  // demonstrate what local_transform allows one to do.
  scoped_refptr<ImageNode> image_node(new ImageNode(
      GetTestImage(resource_provider), math::RectF(output_dimensions)));

  // Scale our image down to half the screen, and center it.
  scoped_refptr<MatrixTransformNode> image_wrap_scene(new MatrixTransformNode(
      image_node, math::ScaleMatrix(0.5f) *
                      math::TranslateMatrix(output_dimensions.width() / 2,
                                            output_dimensions.height() / 2)));

  animations.Add(image_node, base::Bind(&AnimateImage, start_time));

  return new AnimateNode(animations, image_wrap_scene);
}

}  // namespace scenes
}  // namespace test
}  // namespace renderer
}  // namespace cobalt
