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

#include "cobalt/renderer/test/png_utils/png_decode.h"

#include "base/debug/trace_event.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "third_party/libpng/png.h"

namespace cobalt {
namespace renderer {
namespace test {
namespace png_utils {

namespace {

enum AlphaFormat {
  // The premultiplied alpha format specifies pixel values where each RGB
  // component has been multiplied by the A component (assuming values ranging
  // from [0.0, 1.0]).  Thus, all RGB components should be less than or equal
  // to the A component.
  kAlphaFormatPremultiplied,

  // Unpremultiplied alpha refers to the more standard RGBA format where each
  // component is independent of the others.
  kAlphaFormatUnpremultiplied,
};

class PNGFileReadContext {
 public:
  explicit PNGFileReadContext(const FilePath& file_path,
                              AlphaFormat alpha_format);
  ~PNGFileReadContext();

  void DecodeImageTo(const std::vector<png_bytep>& rows);

  int width() const { return width_; }
  int height() const { return height_; }

 private:
  FILE* fp_;
  png_structp png_;
  png_infop png_metadata_;

  int width_;
  int height_;
};

void PremultiplyAlphaTransform(png_structp ptr, png_row_infop row_info,
                               png_bytep data) {
  for (unsigned int i = 0; i < row_info->width; ++i) {
    uint8_t* color_bytes = data + i * 4;
    uint8_t alpha_value = color_bytes[3];
    for (int c = 0; c < 3; ++c) {
      uint8_t component_value = color_bytes[c];
      // The following code divides by 255 and rounds without explicitly using a
      // division instruction.
      // For more information read:
      // http://answers.google.com/answers/threadview/id/502016.html
      uint32_t product = component_value * alpha_value + 128;
      color_bytes[c] = (product + (product >> 8)) >> 8;
    }
  }
}

PNGFileReadContext::PNGFileReadContext(const FilePath& file_path,
                                       AlphaFormat alpha_format) {
  TRACE_EVENT0("renderer::test::png_utils",
               "PNGFileReadContext::PNGFileReadContext()");

  // Much of this PNG loading code is based on a section from the libpng manual:
  // http://www.libpng.org/pub/png/libpng-1.2.5-manual.html#section-3

  fp_ = file_util::OpenFile(file_path, "rb");
  DCHECK(fp_) << "Unable to open: " << file_path.value();

  uint8_t header[8];
  fread(header, 1, 8, fp_);
  DCHECK(!png_sig_cmp(header, 0, 8)) << "Invalid PNG header.";

  // Set up a libpng context for reading images.
  png_ = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
  DCHECK(png_);

  // Create a structure to contain metadata about the image.
  png_metadata_ = png_create_info_struct(png_);
  DCHECK(png_metadata_);

  // libpng expects to longjump to png_->jmpbuf if it encounters an error.
  // According to longjmp's documentation, this implies that stack unwinding
  // will occur in that case, though C++ objects with non-trivial destructors
  // will not be called.  This is fine though, since we abort upon errors here.
  // If alternative behavior is desired, custom error and warning handler
  // functions can be passed into the png_create_read_struct() call above.
  if (setjmp(png_->jmpbuf)) {
    DLOG(FATAL) << "libpng returned error reading " << file_path.value();
    abort();
  }
  // Set up for file i/o.
  png_init_io(png_, fp_);
  // Tell png we already read 8 bytes.
  png_set_sig_bytes(png_, 8);
  // Read the image info.
  png_read_info(png_, png_metadata_);

  // Transform PNGs into a canonical RGBA form, in order to simplify the process
  // of reading png files of varying formats.  Of course, if we would like to
  // maintain data in formats other than RGBA, this logic should be adjusted.
  {
    if (png_get_bit_depth(png_, png_metadata_) == 16) {
      png_set_strip_16(png_);
    }

    png_byte color = png_get_color_type(png_, png_metadata_);

    if (color == PNG_COLOR_TYPE_GRAY &&
        png_get_bit_depth(png_, png_metadata_) < 8) {
      png_set_expand_gray_1_2_4_to_8(png_);
    }

    // Convert from grayscale or palette to color.
    if (!(color & PNG_COLOR_MASK_COLOR)) {
      png_set_gray_to_rgb(png_);
    } else if (color == PNG_COLOR_TYPE_PALETTE) {
      png_set_palette_to_rgb(png_);
    }

    // Add an alpha channel if missing.
    if (!(color & PNG_COLOR_MASK_ALPHA)) {
      png_set_add_alpha(png_, 0xff /* opaque */, PNG_FILLER_AFTER);
    }
  }

  if (alpha_format == kAlphaFormatPremultiplied) {
    png_set_read_user_transform_fn(png_, &PremultiplyAlphaTransform);
  }

  // End transformations. Get the updated info, and then verify.
  png_read_update_info(png_, png_metadata_);
  DCHECK_EQ(PNG_COLOR_TYPE_RGBA, png_get_color_type(png_, png_metadata_));
  DCHECK_EQ(8, png_get_bit_depth(png_, png_metadata_));

  width_ = png_get_image_width(png_, png_metadata_);
  height_ = png_get_image_height(png_, png_metadata_);
}

PNGFileReadContext::~PNGFileReadContext() {
  // Time to clean up.  First create a structure to read image metadata (like
  // comments) from the end of the png file, then read the remaining data in
  // the png file, and then finally release our context and close the file.
  png_infop end = png_create_info_struct(png_);
  DCHECK(end);

  // Read the end data in the png file.
  png_read_end(png_, end);

  // Release our png reading context and associated info structs.
  png_destroy_read_struct(&png_, &png_metadata_, &end);

  file_util::CloseFile(fp_);
}

void PNGFileReadContext::DecodeImageTo(const std::vector<png_bytep>& rows) {
  TRACE_EVENT0("renderer::test::png_utils",
               "PNGFileReadContext::DecodeImageTo()");
  // Execute the read of png image data.
  png_read_image(png_, const_cast<png_bytep*>(&rows[0]));
}

scoped_array<uint8_t> DecodePNGToRGBAInternal(const FilePath& png_file_path,
                                              int* width, int* height,
                                              AlphaFormat alpha_format) {
  PNGFileReadContext png_read_context(png_file_path, alpha_format);

  // Setup pointers to the rows in which libpng should read out the decoded png
  // image data to.

  // We set bpp to 4 because we know that we're dealing with RGBA.
  const int bytes_per_pixel = 4;
  int pitch = png_read_context.width() * 4;
  scoped_array<uint8_t> data(new uint8_t[pitch * png_read_context.height()]);
  std::vector<png_bytep> rows(png_read_context.height());
  uint8_t* row = data.get();
  for (int i = 0; i < png_read_context.height(); ++i) {
    rows[i] = row;
    row += pitch;
  }

  png_read_context.DecodeImageTo(rows);

  // And finally return the output decoded PNG data.
  DCHECK(width);
  DCHECK(height);
  *width = png_read_context.width();
  *height = png_read_context.height();

  return data.Pass();
}

}  // namespace

scoped_array<uint8_t> DecodePNGToRGBA(const FilePath& png_file_path, int* width,
                                      int* height) {
  TRACE_EVENT0("renderer::test::png_utils", "DecodePNGToRGBA()");
  return DecodePNGToRGBAInternal(png_file_path, width, height,
                                 kAlphaFormatUnpremultiplied);
}

scoped_array<uint8_t> DecodePNGToPremultipliedAlphaRGBA(
    const FilePath& png_file_path, int* width, int* height) {
  TRACE_EVENT0("renderer::test::png_utils",
               "DecodePNGToPremultipliedAlphaRGBA()");
  return DecodePNGToRGBAInternal(png_file_path, width, height,
                                 kAlphaFormatPremultiplied);
}

scoped_refptr<cobalt::render_tree::Image> DecodePNGToRenderTreeImage(
    const FilePath& png_file_path,
    render_tree::ResourceProvider* resource_provider) {
  TRACE_EVENT0("renderer::test::png_utils", "DecodePNGToRenderTreeImage()");
  PNGFileReadContext png_read_context(png_file_path, kAlphaFormatPremultiplied);

  // Setup pointers to the rows in which libpng should read out the decoded png
  // image data to.
  // Currently, we decode all images to RGBA and load those.
  scoped_ptr<render_tree::ImageData> data =
      resource_provider->AllocateImageData(
          math::Size(png_read_context.width(), png_read_context.height()),
          render_tree::kPixelFormatRGBA8,
          render_tree::kAlphaFormatPremultiplied);
  std::vector<png_bytep> rows(png_read_context.height());
  uint8_t* row = data->GetMemory();
  for (int i = 0; i < png_read_context.height(); ++i) {
    rows[i] = row;
    row += data->GetDescriptor().pitch_in_bytes;
  }

  png_read_context.DecodeImageTo(rows);

  // And now create a texture out of the image data.
  return resource_provider->CreateImage(data.Pass());
}


}  // namespace png_utils
}  // namespace test
}  // namespace renderer
}  // namespace cobalt
