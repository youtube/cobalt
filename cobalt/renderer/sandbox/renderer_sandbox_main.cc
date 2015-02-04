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

#include <cmath>

#include "base/at_exit.h"
#include "base/debug/trace_event.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/path_service.h"
#include "cobalt/base/cobalt_paths.h"
#include "cobalt/base/init_cobalt.h"
#include "cobalt/math/point_f.h"
#include "cobalt/math/transform_2d.h"
#include "cobalt/render_tree/composition_node.h"
#include "cobalt/render_tree/image.h"
#include "cobalt/render_tree/image_node.h"
#include "cobalt/render_tree/rect_node.h"
#include "cobalt/render_tree/text_node.h"
#include "cobalt/renderer/backend/default_graphics_system.h"
#include "cobalt/renderer/backend/display.h"
#include "cobalt/renderer/backend/graphics_context.h"
#include "cobalt/renderer/backend/graphics_system.h"
#include "cobalt/renderer/pipeline.h"
#include "cobalt/renderer/renderer_module.h"
#include "cobalt/renderer/test/png_utils/png_decode.h"
#include "cobalt/trace_event/scoped_trace_to_file.h"

using cobalt::render_tree::ResourceProvider;
using cobalt::renderer::test::png_utils::DecodePNGToRenderTreeImage;

namespace {

class RenderTreeBuilder {
 public:
  explicit RenderTreeBuilder(ResourceProvider* resource_provider);

  scoped_refptr<cobalt::render_tree::Node> Build(double seconds_elapsed,
                                                 float width,
                                                 float height) const;

 private:
  struct SpriteInfo {
    float scale;
    float rotation_speed;           // In revolutions per second.
    cobalt::math::PointF position;  // In normalized device coordinates.
  };

  std::vector<SpriteInfo> sprite_infos_;

  scoped_refptr<cobalt::render_tree::Image> test_image_;
  scoped_refptr<cobalt::render_tree::Font> test_font_;
};

RenderTreeBuilder::RenderTreeBuilder(ResourceProvider* resource_provider) {
  TRACE_EVENT0("cobalt::renderer_sandbox",
               "RenderTreeBuilder::RenderTreeBuilder()");

  // Create a set of randomly positioned and sized sprites.
  const int kNumSprites = 20;
  for (int i = 0; i < kNumSprites; ++i) {
    const float kMinScale = 0.1f;
    const float kMaxScale = 0.7f;
    SpriteInfo sprite_info;
    sprite_info.scale =
        (rand() / static_cast<float>(RAND_MAX)) * (kMaxScale - kMinScale) +
        kMinScale;

    sprite_info.position.set_x(rand() / static_cast<float>(RAND_MAX));
    sprite_info.position.set_y(rand() / static_cast<float>(RAND_MAX));

    const float kMinRotationSpeed = 0.01f;
    const float kMaxRotationSpeed = 0.3f;
    sprite_info.rotation_speed = (rand() / static_cast<float>(RAND_MAX)) *
                                     (kMaxRotationSpeed - kMinRotationSpeed) +
                                 kMinRotationSpeed;

    sprite_infos_.push_back(sprite_info);
  }

  // Load a test image from disk.
  FilePath data_directory;
  CHECK(PathService::Get(base::DIR_SOURCE_ROOT, &data_directory));
  test_image_ = DecodePNGToRenderTreeImage(
      data_directory.Append(FILE_PATH_LITERAL("renderer_sandbox"))
          .Append(FILE_PATH_LITERAL("test_image.png")),
      resource_provider);

  // Create our test font.
  test_font_ = resource_provider->GetPreInstalledFont(
      "Droid Sans", ResourceProvider::kNormal, 40);
}

// Passes the input value through a sawtooth function.
float Sawtooth(float x) { return x - static_cast<int>(x); }

scoped_refptr<cobalt::render_tree::Node> RenderTreeBuilder::Build(
    double seconds_elapsed, float width, float height) const {
  TRACE_EVENT0("cobalt::renderer_sandbox", "RenderTreeBuilder::Build()");

  // Create an image for each SpriteInfo we have in our sprite_infos_ vector.
  // They will be positioned and scaled according to their SpriteInfo settings,
  // and rotated according to time.
  scoped_ptr<cobalt::render_tree::CompositionNodeMutable> mutable_composition(
      new cobalt::render_tree::CompositionNodeMutable());
  // Declare a rectangle that fills the background to effectively clear the
  // screen.
  mutable_composition->AddChild(
      make_scoped_refptr(new cobalt::render_tree::RectNode(
          cobalt::math::SizeF(width, height),
          scoped_ptr<cobalt::render_tree::Brush>(
              new cobalt::render_tree::SolidColorBrush(
                  cobalt::render_tree::ColorRGBA(1.0f, 1.0f, 1.0f))))),
      cobalt::math::Matrix3F::Identity());


  // First, create a centered, sawtoothed-growing black rectangle in the
  // background.
  const float kBackgroundRectPeriod = 5.0f;
  float background_rect_size =
      Sawtooth(seconds_elapsed / kBackgroundRectPeriod);
  cobalt::math::SizeF rect_size(background_rect_size * width,
                                background_rect_size * height);
  mutable_composition->AddChild(
      make_scoped_refptr(new cobalt::render_tree::RectNode(
          rect_size,
          scoped_ptr<cobalt::render_tree::Brush>(
              new cobalt::render_tree::SolidColorBrush(
                  cobalt::render_tree::ColorRGBA(0.2f, 0.2f, 0.2f))))),
      cobalt::math::TranslateMatrix((width - rect_size.width()) / 2,
                                    (height - rect_size.height()) / 2));

  // Now, setup a plurality of spinning images that contain the YouTube logo.
  for (std::vector<SpriteInfo>::const_iterator iter = sprite_infos_.begin();
       iter != sprite_infos_.end(); ++iter) {
    // Create the child image node that references the image data
    scoped_refptr<cobalt::render_tree::ImageNode> image_node(
        new cobalt::render_tree::ImageNode(test_image_));

    // Setup child image node's scale and rotation and translation.
    float theta = seconds_elapsed * iter->rotation_speed * 2 * M_PI;
    cobalt::math::Matrix3F sprite_matrix =
        cobalt::math::TranslateMatrix(  // Place the image within the scene.
            iter->position.x() * width,
            iter->position.y() * height) *
        cobalt::math::RotateMatrix(theta) *
        cobalt::math::ScaleMatrix(iter->scale) *
        cobalt::math::TranslateMatrix(  // Set image center as origin.
            -0.5 * image_node->destination_size().width(),
            -0.5 * image_node->destination_size().height());

    // Add the child image node to the list of composed nodes.
    mutable_composition->AddChild(image_node, sprite_matrix);
  }

  // Add a text marque that passes by the screen.
  const std::string kMarqueeText("YouTube");

  // Use information about the string we are rendering to properly position
  // it in the vertical center of the screen and far enough offscreen that
  // a switch from the right side to the left is not noticed.
  cobalt::math::RectF text_bounds = test_font_->GetBounds(kMarqueeText);

  // Center the text's bounding box vertically on the screen.
  float y_position =
      height / 2.0f - text_bounds.y() - text_bounds.height() / 2.0f;

  // Calculate the animated x position of the text.
  const float kTextMarqueePeriod = 10.0f;
  float text_start_position = -text_bounds.right();
  float text_end_position = width - text_bounds.x();
  float periodic_position = Sawtooth(seconds_elapsed / kTextMarqueePeriod);
  float x_position =
      text_start_position +
      (text_end_position - text_start_position) * periodic_position;

  // Add a background rectangle to the text in order to demonstrate the
  // relationship between the text's origin and its bounding box.
  mutable_composition->AddChild(
      make_scoped_refptr(new cobalt::render_tree::RectNode(
          cobalt::math::SizeF(text_bounds.width(), text_bounds.height()),
          scoped_ptr<cobalt::render_tree::Brush>(
              new cobalt::render_tree::SolidColorBrush(
                  cobalt::render_tree::ColorRGBA(0.7f, 0.2f, 1.0f))))),
      cobalt::math::TranslateMatrix(x_position + text_bounds.x(),
                                    y_position + text_bounds.y()));
  // Add the actual text node to our composition.
  mutable_composition->AddChild(
      make_scoped_refptr(new cobalt::render_tree::TextNode(
          kMarqueeText, test_font_,
          cobalt::render_tree::ColorRGBA(0.0f, 0.0f, 0.0f))),
      cobalt::math::TranslateMatrix(x_position, y_position));

  // Finally construct the composition node and return it.
  return make_scoped_refptr(
      new cobalt::render_tree::CompositionNode(mutable_composition.Pass()));
}

}  // namespace

int main(int argc, char* argv[]) {
  base::AtExitManager at_exit;
  cobalt::InitCobalt(argc, argv);

  cobalt::trace_event::ScopedTraceToFile trace_to_file(
      FilePath(FILE_PATH_LITERAL("renderer_sandbox_trace.json")));

  // Construct a renderer module using default options.
  cobalt::renderer::RendererModule::Options renderer_module_options;
  cobalt::renderer::RendererModule renderer_module(renderer_module_options);

  // Construct our render tree builder which will be the source of our render
  // trees within the main loop.
  RenderTreeBuilder render_tree_builder(
      renderer_module.pipeline()->GetResourceProvider());

  // Repeatedly render/animate and flip the screen.
  int frame = 0;
  base::Time start_time = base::Time::Now();
  while (true) {
    double seconds_elapsed = (base::Time::Now() - start_time).InSecondsF();

    // Stop after 30 seconds have passed.
    if (seconds_elapsed > 30) {
      break;
    }

    ++frame;
    TRACE_EVENT1("renderer_sandbox", "frame", "frame_number", frame);
    // Build the render tree that we will output to the screen
    scoped_refptr<cobalt::render_tree::Node> render_tree =
        render_tree_builder.Build(
            seconds_elapsed,
            renderer_module.render_target()->GetSurfaceInfo().width,
            renderer_module.render_target()->GetSurfaceInfo().height);

    // Submit the render tree to be rendered.
    {
      TRACE_EVENT0("renderer_sandbox", "submit");
      renderer_module.pipeline()->Submit(render_tree);
    }
  }

  return 0;
}
