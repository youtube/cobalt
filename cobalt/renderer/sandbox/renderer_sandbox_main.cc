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
#include "cobalt/render_tree/image.h"
#include "cobalt/render_tree/image_node.h"
#include "cobalt/renderer/backend/display.h"
#include "cobalt/renderer/backend/graphics_context.h"
#include "cobalt/renderer/backend/graphics_system.h"
#include "cobalt/renderer/backend/default_graphics_system.h"
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

scoped_refptr<cobalt::render_tree::Node> BuildRenderTree(
    double seconds_elapsed,
    const scoped_refptr<cobalt::render_tree::Image>& test_image) {
  // Currently this is a very simple test that wraps the passed in image
  // in a render tree and returns that.
  return make_scoped_refptr(new cobalt::render_tree::ImageNode(test_image));
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

  // Load a test image from disk.
  FilePath data_directory;
  CHECK(PathService::Get(base::DIR_SOURCE_ROOT, &data_directory));
  scoped_refptr<cobalt::render_tree::Image> test_image = LoadPNG(
      data_directory.
      Append(FILE_PATH_LITERAL("renderer_sandbox")).
      Append(FILE_PATH_LITERAL("test_image.png")));

  // Determine in advance the image size and format that we will use to
  // render to.
  SkImageInfo output_image_info =
      SkImageInfo::MakeN32(display->GetRenderTarget()->GetSurfaceInfo().width_,
                           display->GetRenderTarget()->GetSurfaceInfo().height_,
                           kPremul_SkAlphaType);

  // Repeatedly render/animate and flip the screen.
  base::Time start_time = base::Time::Now();
  while (true) {
    double seconds_elapsed = (base::Time::Now() - start_time).InSecondsF();
    if (seconds_elapsed > 30) break;

    // Build the render tree that we will output to the screen
    scoped_refptr<cobalt::render_tree::Node> render_tree =
        BuildRenderTree(seconds_elapsed, test_image);

    // Allocate the pixels for the output image.  By manually creating the
    // pixels, we can retain ownership of them, and thus pass them into the
    // graphics system without a copy.
    scoped_array<unsigned char> bitmap_pixels(
        new unsigned char[output_image_info.height() *
                          output_image_info.minRowBytes()]);
    SkBitmap bitmap;
    bitmap.installPixels(output_image_info, bitmap_pixels.get(),
                         output_image_info.minRowBytes());
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
