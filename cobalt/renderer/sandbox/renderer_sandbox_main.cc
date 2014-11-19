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
#include "base/path_service.h"
#include "cobalt/base/cobalt_paths.h"
#include "cobalt/base/init_cobalt.h"
#include "cobalt/math/point_f.h"
#include "cobalt/math/transform_2d.h"
#include "cobalt/render_tree/composition_node.h"
#include "cobalt/render_tree/image.h"
#include "cobalt/render_tree/image_node.h"
#include "cobalt/renderer/backend/display.h"
#include "cobalt/renderer/backend/graphics_context.h"
#include "cobalt/renderer/backend/graphics_system.h"
#include "cobalt/renderer/backend/default_graphics_system.h"
#include "cobalt/renderer/rasterizer/skia/skia_font.h"
#include "cobalt/renderer/rasterizer/skia/skia_software_image.h"
#include "cobalt/renderer/rasterizer/skia/rasterizer.h"
#include "third_party/libpng/png.h"

#include "third_party/skia/include/core/SkCanvas.h"

namespace {

scoped_refptr<cobalt::render_tree::Image>
LoadPNG(const FilePath& png_file_path) {
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
  scoped_array<uint8_t> data(new uint8_t[pitch * height]);
  std::vector<png_bytep> rows(height);
  uint8_t* row = &data[0];
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
  return scoped_refptr<cobalt::render_tree::Image>(
      new cobalt::renderer::rasterizer::SkiaSoftwareImage(
          width, height, pitch, data.Pass()));
}

class RenderTreeBuilder {
 public:
  RenderTreeBuilder();

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

RenderTreeBuilder::RenderTreeBuilder() {
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
                  .Append(FILE_PATH_LITERAL("test_image.png")));

  // Create our test font.
  SkAutoTUnref<SkTypeface> typeface(
      SkTypeface::CreateFromName("Droid Sans", SkTypeface::kNormal));
  test_font_ = scoped_refptr<cobalt::render_tree::Font>(
      new cobalt::renderer::rasterizer::SkiaFont(typeface, 40));
}

scoped_refptr<cobalt::render_tree::Node> RenderTreeBuilder::Build(
    double seconds_elapsed, float width, float height) const {
  // Create an image for each SpriteInfo we have in our sprite_infos_ vector.
  // They will be positioned and scaled according to their SpriteInfo settings,
  // and rotated according to time.
  scoped_ptr<cobalt::render_tree::CompositionNodeMutable> mutable_composition(
      new cobalt::render_tree::CompositionNodeMutable());
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
  cobalt::math::SizeF text_bounds = test_font_->GetBounds(kMarqueeText);

  float y_position = height / 2.0f - text_bounds.height() / 2.0f;

  // Calculate the animated x position of the text.
  const float kTextVelocityInScreenWidthsPerSecond = 0.1f;
  float text_start_position = -text_bounds.width();
  float text_end_position = width;
  float text_distance = seconds_elapsed * kTextVelocityInScreenWidthsPerSecond;
  float periodic_position = text_distance - static_cast<int>(text_distance);
  float x_position =
      text_start_position +
      (text_end_position - text_start_position) * periodic_position;

  // Add the text node to our composition.
  mutable_composition->AddChild(
      make_scoped_refptr(
          new cobalt::render_tree::TextNode(kMarqueeText, test_font_)),
      cobalt::math::TranslateMatrix(x_position, y_position));

  // Finally construct the composition node and return it.
  return make_scoped_refptr(
      new cobalt::render_tree::CompositionNode(mutable_composition.Pass()));
}

// Determine the Cobalt texture format that is compatible with a given
// Skia texture format.
cobalt::renderer::backend::SurfaceInfo::Format SkiaFormatToCobaltFormat(
    SkColorType skia_format) {
  switch (skia_format) {
    case kRGBA_8888_SkColorType:
      return cobalt::renderer::backend::SurfaceInfo::kFormatRGBA8;
    case kBGRA_8888_SkColorType:
      return cobalt::renderer::backend::SurfaceInfo::kFormatBGRA8;
    default: {
      DLOG(FATAL) << "Unsupported Skia image format!";
      return cobalt::renderer::backend::SurfaceInfo::kFormatRGBA8;
    }
  }
}

}  // namespace

int main(int argc, char* argv[]) {
  base::AtExitManager at_exit;
  cobalt::InitCobalt(argc, argv);

  // Load up the platform's default graphics system.
  scoped_ptr<cobalt::renderer::backend::GraphicsSystem> graphics =
      cobalt::renderer::backend::CreateDefaultGraphicsSystem();

  // Create/initialize the default display
  scoped_ptr<cobalt::renderer::backend::Display> display =
      graphics->CreateDefaultDisplay();

  // Create a graphics context associated with the default display's render
  // target so that we can output to the display.
  scoped_ptr<cobalt::renderer::backend::GraphicsContext> graphics_context =
      graphics->CreateGraphicsContext(display->GetRenderTarget());

  // Determine in advance the image size and format that we will use to
  // render to.
  SkImageInfo output_image_info =
      SkImageInfo::MakeN32(display->GetRenderTarget()->GetSurfaceInfo().width_,
                           display->GetRenderTarget()->GetSurfaceInfo().height_,
                           kPremul_SkAlphaType);

  // Construct our render tree builder which will be the source of our render
  // trees within the main loop.
  RenderTreeBuilder render_tree_builder;

  // Repeatedly render/animate and flip the screen.
  base::Time start_time = base::Time::Now();
  while (true) {
    double seconds_elapsed = (base::Time::Now() - start_time).InSecondsF();
    if (seconds_elapsed > 30) break;

    // Build the render tree that we will output to the screen
    scoped_refptr<cobalt::render_tree::Node> render_tree =
        render_tree_builder.Build(
            seconds_elapsed,
            display->GetRenderTarget()->GetSurfaceInfo().width_,
            display->GetRenderTarget()->GetSurfaceInfo().height_);

    // Allocate the pixels for the output image.  By manually creating the
    // pixels, we can retain ownership of them, and thus pass them into the
    // graphics system without a copy.
    scoped_array<unsigned char> bitmap_pixels(
        new unsigned char[output_image_info.height() *
                          output_image_info.minRowBytes()]);
    SkBitmap bitmap;
    bitmap.installPixels(output_image_info, bitmap_pixels.get(),
                         output_image_info.minRowBytes());

    // Setup the canvas such that it assumes normalized device coordinates, so
    // that (0,0) represents the top left corner and (1,1) represents the
    // bottom right corner.
    SkCanvas canvas(bitmap);

    // Create the rasterizer and setup its render target to the bitmap we have
    // just created above.
    cobalt::renderer::rasterizer::RasterizerSkia rasterizer(&canvas);

    // Finally, rasterize the render tree to the output canvas using the
    // rasterizer we just created.
    render_tree->Accept(&rasterizer);

    // The rasterized pixels are still on the CPU, ship them off to the GPU
    // for output to the display.  We must first create a backend GPU texture
    // with the data so that it is visible to the GPU.
    scoped_ptr<cobalt::renderer::backend::Texture> output_texture =
        graphics_context->CreateTexture(
            output_image_info.width(), output_image_info.height(),
            SkiaFormatToCobaltFormat(output_image_info.colorType()),
            output_image_info.minRowBytes(), bitmap_pixels.Pass());

    // Issue the command to render the texture to the display.
    graphics_context->BlitToRenderTarget(output_texture.get());

    // Flush all graphics commands and flip the display (and possibly block
    // if we're flipping faster than the display refresh rate).
    graphics_context->Submit();
  }

  return 0;
}
