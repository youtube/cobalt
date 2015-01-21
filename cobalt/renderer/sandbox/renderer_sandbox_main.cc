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
#include "third_party/libpng/png.h"

using cobalt::render_tree::ResourceProvider;

namespace {

scoped_refptr<cobalt::render_tree::Image> LoadPNG(
    const FilePath& png_file_path, ResourceProvider* resource_provider) {
  // Much of this PNG loading code is based on a section from the libpng manual:
  // http://www.libpng.org/pub/png/libpng-1.2.5-manual.html#section-3

  FILE* fp = file_util::OpenFile(png_file_path, "rb");
  DCHECK(fp) << "Unable to open: " << png_file_path.value();

  uint8_t header[8];
  fread(header, 1, 8, fp);
  DCHECK(!png_sig_cmp(header, 0, 8)) << "Invalid PNG header.";

  // Set up a libpng context for reading images.
  png_structp png =
      png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
  DCHECK(png);

  // Create a structure to contain metadata about the image.
  png_infop png_metadata = png_create_info_struct(png);
  DCHECK(png_metadata);

  // libpng expects to longjump to png->jmpbuf if it encounters an error.
  // According to longjmp's documentation, this implies that stack unwinding
  // will occur in that case, though C++ objects with non-trivial destructors
  // will not be called.  This is fine though, since we abort upon errors here.
  // If alternative behavior is desired, custom error and warning handler
  // functions can be passed into the png_create_read_struct() call above.
  if (setjmp(png->jmpbuf)) {
    NOTREACHED() << "libpng returned error reading " << png_file_path.value();
    abort();
  }
  // Set up for file i/o.
  png_init_io(png, fp);
  // Tell png we already read 8 bytes.
  png_set_sig_bytes(png, 8);
  // Read the image info.
  png_read_info(png, png_metadata);

  // Transform PNGs into a canonical RGBA form, in order to simplify the process
  // of reading png files of varying formats.  Of course, if we would like to
  // maintain data in formats other than RGBA, this logic should be adjusted.
  {
    if (png_get_bit_depth(png, png_metadata) == 16) {
      png_set_strip_16(png);
    }

    png_byte color = png_get_color_type(png, png_metadata);

    if (color == PNG_COLOR_TYPE_GRAY &&
        png_get_bit_depth(png, png_metadata) < 8) {
      png_set_expand_gray_1_2_4_to_8(png);
    }

    // Convert from grayscale or palette to color.
    if (!(color & PNG_COLOR_MASK_COLOR)) {
      png_set_gray_to_rgb(png);
    } else if (color == PNG_COLOR_TYPE_PALETTE) {
      png_set_palette_to_rgb(png);
    }

    // Add an alpha channel if missing.
    if (!(color & PNG_COLOR_MASK_ALPHA)) {
      png_set_add_alpha(png, 0xff /* opaque */, PNG_FILLER_AFTER);
    }
  }

  // End transformations. Get the updated info, and then verify.
  png_read_update_info(png, png_metadata);
  DCHECK_EQ(PNG_COLOR_TYPE_RGBA, png_get_color_type(png, png_metadata));
  DCHECK_EQ(8, png_get_bit_depth(png, png_metadata));

  int width = png_get_image_width(png, png_metadata);
  int height = png_get_image_height(png, png_metadata);

  // Allocate texture memory and set up row pointers for read.
  int pitch = width * 4;  // Since everything is RGBA at this point.

  // Setup pointers to the rows in which libpng should read out the decoded png
  // image data to.
  scoped_ptr<ResourceProvider::ImageData> data =
      resource_provider->AllocateImageData(
          width, height, ResourceProvider::ImageData::kPixelFormatRGBA8);
  std::vector<png_bytep> rows(height);
  uint8_t* row = data->GetMemory();
  for (int i = 0; i < height; ++i) {
    rows[i] = row;
    row += pitch;
  }

  // Execute the read of png image data.
  png_read_image(png, &rows[0]);

  // Time to clean up.  First create a structure to read image metadata (like
  // comments) from the end of the png file, then read the remaining data in
  // the png file, and then finally release our context and close the file.
  png_infop end = png_create_info_struct(png);
  DCHECK(end);

  // Read the end data in the png file.
  png_read_end(png, end);

  // Release our png reading context and associated info structs.
  png_destroy_read_struct(&png, &png_metadata, &end);

  file_util::CloseFile(fp);

  // And now create a texture out of the image data.
  return resource_provider->CreateImage(data.Pass());
}

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
  test_image_ =
      LoadPNG(data_directory.Append(FILE_PATH_LITERAL("renderer_sandbox"))
                  .Append(FILE_PATH_LITERAL("test_image.png")),
              resource_provider);

  // Create our test font.
  test_font_ = resource_provider->GetPreInstalledFont(
      "Droid Sans", ResourceProvider::kNormal, 40);
}

namespace {

// Passes the input value through a sawtooth function.
float Sawtooth(float x) { return x - static_cast<int>(x); }

}  // namespace

scoped_refptr<cobalt::render_tree::Node> RenderTreeBuilder::Build(
    double seconds_elapsed, float width, float height) const {
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

  // Construct a renderer module using default options.
  cobalt::renderer::RendererModule::Options renderer_module_options;
  cobalt::renderer::RendererModule renderer_module(renderer_module_options);

  // Construct our render tree builder which will be the source of our render
  // trees within the main loop.
  RenderTreeBuilder render_tree_builder(
      renderer_module.pipeline()->GetResourceProvider());

  // Repeatedly render/animate and flip the screen.
  base::Time start_time = base::Time::Now();
  while (true) {
    double seconds_elapsed = (base::Time::Now() - start_time).InSecondsF();

    // Stop after 30 seconds have passed.
    if (seconds_elapsed > 30) {
      break;
    }

    // Build the render tree that we will output to the screen
    scoped_refptr<cobalt::render_tree::Node> render_tree =
        render_tree_builder.Build(
            seconds_elapsed,
            renderer_module.render_target()->GetSurfaceInfo().width,
            renderer_module.render_target()->GetSurfaceInfo().height);

    // Submit the render tree to be rendered.
    renderer_module.pipeline()->Submit(render_tree);
  }

  return 0;
}
