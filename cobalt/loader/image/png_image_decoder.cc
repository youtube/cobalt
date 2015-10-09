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

#include "cobalt/loader/image/png_image_decoder.h"

#include "base/debug/trace_event.h"
#include "base/logging.h"

namespace cobalt {
namespace loader {
namespace image {

namespace {

// Gamma constants.
const double kMaxGamma = 21474.83;
const double kDefaultGamma = 2.2;
const double kInverseGamma = 0.45455;

// Protect against large PNGs. See Mozilla's bug #251381 for more info.
const uint32 kMaxPNGSize = 1000000UL;

// Use fix point multiplier instead of integer division or floating point math.
// This multipler produces exactly the same result for all values in range 0 -
// 255.
const uint32 kFixPointOffset = 24;
const uint32 kFixPointShifted = 1U << kFixPointOffset;
const uint32 kFixPointMultiplier =
    static_cast<uint32>(1.0 / 255.0 * kFixPointShifted) + 1;

// Multiplies unsigned value by fixpoint value and converts back to unsigned.
uint32 FixPointUnsignedMultiply(uint32 fixed, uint32 alpha) {
  return (fixed * alpha * kFixPointMultiplier) >> kFixPointOffset;
}

// Call back functions from libpng
// static
void DecodingFailed(png_structp png, png_const_charp) {
  DLOG(WARNING) << "Decoding failed.";
  longjmp(png->jmpbuf, 1);
}

// static
void DecodingWarning(png_structp png, png_const_charp warning_msg) {
  DLOG(WARNING) << "Decoding warning message: " << warning_msg;
  // Mozilla did this, so we will too.
  // Convert a tRNS warning to be an error (see
  // http://bugzilla.mozilla.org/show_bug.cgi?id=251381 )
  if (!strncmp(warning_msg, "Missing PLTE before tRNS", 24)) {
    png_error(png, warning_msg);
  }
}

}  // namespace

PNGImageDecoder::PNGImageDecoder(
    render_tree::ResourceProvider* resource_provider)
    : ImageDataDecoder(resource_provider),
      png_(NULL),
      info_(NULL),
      has_alpha_(false),
      interlace_buffer_(0) {
  png_ = png_create_read_struct(PNG_LIBPNG_VER_STRING, 0, DecodingFailed,
                                DecodingWarning);
  info_ = png_create_info_struct(png_);
  png_set_progressive_read_fn(png_, this, HeaderAvailable, RowAvailable, 0);
}

void PNGImageDecoder::DecodeChunk(const char* data, size_t size) {
  TRACE_EVENT0("cobalt::loader::image_decoder", "PNGImageDecoder::DecodeChunk");

  // int setjmp(jmp_buf env) saves the current environment (ths program state),
  // at some point of program execution, into a platform-specific data
  // structure (jmp_buf) that can be used at some later point of program
  // execution by longjmp to restore the program state to that saved by setjmp
  // into jmp_buf. This process can be imagined to be a "jump" back to the point
  // of program execution where setjmp saved the environment. The return value
  // from setjmp indicates whether control reached that point normally or from a
  // call to longjmp.
  // If the return is from a direct invocation, setjmp returns 0. If the return
  // is from a call to longjmp, setjmp returns a nonzero value.
MSVC_PUSH_DISABLE_WARNING(4611);
  // warning C4611: interaction between '_setjmp' and C++ object destruction is
  // non-portable.
  if (setjmp(png_->jmpbuf)) {
    // |image_data_| is empty.
    DLOG(WARNING) << "Decoder encounters an error.";
    return;
  }
MSVC_POP_WARNING();

  png_process_data(png_, info_,
                   reinterpret_cast<png_bytep>(const_cast<char*>(data)), size);
}

void PNGImageDecoder::Finish() {
  TRACE_EVENT0("cobalt::loader::image_decoder", "PNGImageDecoder::Finish");
}

PNGImageDecoder::~PNGImageDecoder() {
  // Both are created at the same time. So they should be both zero
  // or both non-zero. Use && here to be safer.
  if (png_ && info_) {
    // This will zero the pointers.
    png_destroy_read_struct(&png_, &info_, 0);
  }

  delete[] interlace_buffer_;
  interlace_buffer_ = 0;
  info_ = NULL;
  png_ = NULL;
}

// Called when we have obtained the header information (including the size).
// static
void PNGImageDecoder::HeaderAvailable(png_structp png, png_infop) {
  PNGImageDecoder* decoder =
      static_cast<PNGImageDecoder*>(png_get_progressive_ptr(png));
  decoder->HeaderAvailableCallback();
}

// Called when a row is ready.
// static
void PNGImageDecoder::RowAvailable(png_structp png, png_bytep row_buffer,
                                   png_uint_32 row_index, int interlace_pass) {
  UNREFERENCED_PARAMETER(interlace_pass);
  PNGImageDecoder* decoder =
      static_cast<PNGImageDecoder*>(png_get_progressive_ptr(png));
  decoder->RowAvailableCallback(row_buffer, row_index);
}

void PNGImageDecoder::HeaderAvailableCallback() {
  png_uint_32 width = png_get_image_width(png_, info_);
  png_uint_32 height = png_get_image_height(png_, info_);

  // Protect against large images.
  if (width > kMaxPNGSize || height > kMaxPNGSize) {
    DLOG(WARNING) << "Large PNG with width: " << width
                  << ", height: " << height;
    longjmp(png_->jmpbuf, 1);
    return;
  }

  // A valid PNG image must contain an IHDR chunk, one or more IDAT chunks,
  // and an IEND chunk.
  int bit_depth;
  int color_type;
  int interlace_type;
  int compression_type;
  int filter_type;
  png_get_IHDR(png_, info_, &width, &height, &bit_depth, &color_type,
               &interlace_type, &compression_type, &filter_type);

  // Expand to ensure we use 24-bit for RGB and 32-bit for RGBA.
  if (color_type == PNG_COLOR_TYPE_PALETTE ||
      (color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8)) {
    png_set_expand(png_);
  }

  png_bytep trns = 0;
  int trns_count = 0;
  if (png_get_valid(png_, info_, PNG_INFO_tRNS)) {
    png_get_tRNS(png_, info_, &trns, &trns_count, 0);
    png_set_expand(png_);
  }

  if (bit_depth == 16) {
    png_set_strip_16(png_);
  }

  if (color_type == PNG_COLOR_TYPE_GRAY ||
      color_type == PNG_COLOR_TYPE_GRAY_ALPHA) {
    png_set_gray_to_rgb(png_);
  }

  // Deal with gamma and keep it under our control.
  double gamma;
  if (png_get_gAMA(png_, info_, &gamma)) {
    if (gamma <= 0.0 || gamma > kMaxGamma) {
      gamma = kInverseGamma;
      png_set_gAMA(png_, info_, gamma);
    }
    png_set_gamma(png_, kDefaultGamma, gamma);
  } else {
    png_set_gamma(png_, kDefaultGamma, kInverseGamma);
  }

  // Tell libpng to send us rows for interlaced pngs.
  if (interlace_type == PNG_INTERLACE_ADAM7) {
    png_set_interlace_handling(png_);
  }

  // Update our info now.
  png_read_update_info(png_, info_);
  int channels = png_get_channels(png_, info_);
  DCHECK(channels == 3 || channels == 4);

  has_alpha_ = (channels == 4);

  if (PNG_INTERLACE_ADAM7 == png_get_interlace_type(png_, info_)) {
    size_t size = channels * width;
    interlace_buffer_ = new png_byte[size];
    if (!interlace_buffer_) {
      DLOG(WARNING) << "Allocate interlace buffer failed.";
      longjmp(png_->jmpbuf, 1);
      return;
    }
  }

  AllocateImageData(
      math::Size(static_cast<int>(width), static_cast<int>(height)));
}

void PNGImageDecoder::RowAvailableCallback(png_bytep row_buffer,
                                           png_uint_32 row_index) {
  DCHECK(image_data_.get());

  /* libpng comments (here to explain what follows).
   *
   * this function is called for every row in the image.  If the
   * image is interlacing, and you turned on the interlace handler,
   * this function will be called for every row in every pass.
   * Some of these rows will not be changed from the previous pass.
   * When the row is not changed, the new_row variable will be NULL.
   * The rows and passes are called in order, so you don't really
   * need the row_num and pass, but I'm supplying them because it
   * may make your life easier.
   */

  // Nothing to do if the row is unchanged, or the row is outside
  // the image bounds: libpng may send extra rows, ignore them to
  // make our lives easier.
  if (!row_buffer) {
    return;
  }

  /* libpng comments (continued).
   *
   * For the non-NULL rows of interlaced images, you must call
   * png_progressive_combine_row() passing in the row and the
   * old row.  You can call this function for NULL rows (it will
   * just return) and for non-interlaced images (it just does the
   * memcpy for you) if it will make the code easier.  Thus, you
   * can just do this for all cases:
   *
   *    png_progressive_combine_row(png_ptr, old_row, new_row);
   *
   * where old_row is what was displayed for previous rows.  Note
   * that the first pass (pass == 0 really) will completely cover
   * the old row, so the rows do not have to be initialized.  After
   * the first pass (and only for interlaced images), you will have
   * to pass the current row, and the function will combine the
   * old row and the new row.
   */

  int color_channels = has_alpha_ ? 4 : 3;
  png_bytep row = row_buffer;

  int width = image_data_->GetDescriptor().size.width();
  if (interlace_buffer_) {
    row = interlace_buffer_ + (row_index * color_channels * width);
    png_progressive_combine_row(png_, row, row_buffer);
  }

  // Write the decoded row pixels to the |image_data_|.
  uint8* pixel_data = image_data_->GetMemory() +
                      image_data_->GetDescriptor().pitch_in_bytes * row_index;

  png_bytep pixel = row_buffer;
  for (int x = 0; x < width; ++x, pixel += color_channels) {
    uint32 alpha = static_cast<uint32>(has_alpha_ ? pixel[3] : 255);

    *pixel_data++ = static_cast<uint8>(
        alpha < 255 ? FixPointUnsignedMultiply(pixel[0], alpha) : pixel[0]);
    *pixel_data++ = static_cast<uint8>(
        alpha < 255 ? FixPointUnsignedMultiply(pixel[1], alpha) : pixel[1]);
    *pixel_data++ = static_cast<uint8>(
        alpha < 255 ? FixPointUnsignedMultiply(pixel[2], alpha) : pixel[2]);
    *pixel_data++ = static_cast<uint8>(alpha);
  }
}

void PNGImageDecoder::AllocateImageData(const math::Size& size) {
  image_data_ = resource_provider_->AllocateImageData(
      size, render_tree::kPixelFormatRGBA8,
      render_tree::kAlphaFormatPremultiplied);
}

}  // namespace image
}  // namespace loader
}  // namespace cobalt
