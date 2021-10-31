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

#include "cobalt/loader/image/png_image_decoder.h"

#include "base/logging.h"
#include "base/trace_event/trace_event.h"
#include "cobalt/base/console_log.h"
#include "nb/memory_scope.h"

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
// This multiplier produces exactly the same result for all values in range 0 -
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
    render_tree::ResourceProvider* resource_provider,
    const base::DebuggerHooks& debugger_hooks)
    : ImageDataDecoder(resource_provider, debugger_hooks),
      png_(NULL),
      info_(NULL),
      has_alpha_(false),
      interlace_buffer_(0) {
  TRACK_MEMORY_SCOPE("Rendering");
  TRACE_EVENT0("cobalt::loader::image", "PNGImageDecoder::PNGImageDecoder()");

  png_ = png_create_read_struct(PNG_LIBPNG_VER_STRING, 0, DecodingFailed,
                                DecodingWarning);
  info_ = png_create_info_struct(png_);
  png_set_progressive_read_fn(png_, this, HeaderAvailable, RowAvailable,
                              DecodeDone);
}

size_t PNGImageDecoder::DecodeChunkInternal(const uint8* data, size_t size) {
  TRACK_MEMORY_SCOPE("Rendering");
  TRACE_EVENT0("cobalt::loader::image",
               "PNGImageDecoder::DecodeChunkInternal()");
  // int setjmp(jmp_buf env) saves the current environment (ths program state),
  // at some point of program execution, into a platform-specific data
  // structure (jmp_buf) that can be used at some later point of program
  // execution by longjmp to restore the program state to that saved by setjmp
  // into jmp_buf. This process can be imagined to be a "jump" back to the point
  // of program execution where setjmp saved the environment. The return value
  // from setjmp indicates whether control reached that point normally or from a
  // call to longjmp. If the return is from a direct invocation, setjmp returns
  // 0. If the return is from a call to longjmp, setjmp returns a nonzero value.
  MSVC_PUSH_DISABLE_WARNING(4611);
  // warning C4611: interaction between '_setjmp' and C++ object destruction is
  // non-portable.
  if (setjmp(png_->jmpbuf)) {
    // image data is empty.
    DLOG(WARNING) << "Decoder encounters an error.";
    set_state(kError);
    return 0;
  }
  MSVC_POP_WARNING();

  png_process_data(png_, info_, const_cast<png_bytep>(data), size);

  // All the data is decoded by libpng internally.
  return size;
}

scoped_refptr<Image> PNGImageDecoder::FinishInternal() {
  if (state() != kDone) {
    decoded_image_data_.reset();
    return NULL;
  }
  SB_DCHECK(decoded_image_data_);
  return CreateStaticImage(std::move(decoded_image_data_));
}

PNGImageDecoder::~PNGImageDecoder() {
  TRACE_EVENT0("cobalt::loader::image", "PNGImageDecoder::~PNGImageDecoder()");
  // Both are created at the same time. So they should be both zero
  // or both non-zero. Use && here to be safer.
  if (png_ && info_) {
    // png_destroy_read_struct() frees the memory associated with the read
    // png_struct struct that holds information from the given PNG file, the
    // associated png_info struct for holding the image information and png_info
    // struct for holding the information at end of the given PNG file.
    png_destroy_read_struct(&png_, &info_, 0);
  }

  delete[] interlace_buffer_;
  interlace_buffer_ = 0;
  info_ = NULL;
  png_ = NULL;
}

// Called when we have obtained the header information (including the size).
// static
void PNGImageDecoder::HeaderAvailable(png_structp png, png_infop info) {
  TRACK_MEMORY_SCOPE("Rendering");
  TRACE_EVENT0("cobalt::loader::image", "PNGImageDecoder::~PNGImageDecoder()");
  PNGImageDecoder* decoder =
      static_cast<PNGImageDecoder*>(png_get_progressive_ptr(png));
  decoder->HeaderAvailableCallback();
}

// Called when a row is ready.
// static
void PNGImageDecoder::RowAvailable(png_structp png, png_bytep row_buffer,
                                   png_uint_32 row_index, int interlace_pass) {
  PNGImageDecoder* decoder =
      static_cast<PNGImageDecoder*>(png_get_progressive_ptr(png));
  decoder->RowAvailableCallback(row_buffer, row_index);
}

// Called when decoding is done.
// static
void PNGImageDecoder::DecodeDone(png_structp png, png_infop info) {
  TRACK_MEMORY_SCOPE("Rendering");
  TRACE_EVENT0("cobalt::loader::image", "PNGImageDecoder::DecodeDone()");

  PNGImageDecoder* decoder =
      static_cast<PNGImageDecoder*>(png_get_progressive_ptr(png));
  decoder->DecodeDoneCallback();
}

void PNGImageDecoder::HeaderAvailableCallback() {
  TRACK_MEMORY_SCOPE("Rendering");
  TRACE_EVENT0("cobalt::loader::image",
               "PNGImageDecoder::HeaderAvailableCallback()");
  DCHECK_EQ(state(), kWaitingForHeader);

  png_uint_32 width = png_get_image_width(png_, info_);
  png_uint_32 height = png_get_image_height(png_, info_);

  // Protect against large images.
  if (width > kMaxPNGSize || height > kMaxPNGSize) {
    DLOG(WARNING) << "Large PNG with width: " << width
                  << ", height: " << height;
    set_state(kError);
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

  if (interlace_type == PNG_INTERLACE_ADAM7) {
    // Notify libpng to send us rows for interlaced pngs.
    png_set_interlace_handling(png_);
    CLOG(WARNING, debugger_hooks()) << "Interlaced PNGs are not displayed "
                                       "properly in older versions of Cobalt";
  }

  // Updates |info_| to reflect any transformations that have been requested.
  // For example, rowbytes will be updated to handle expansion of an interlaced
  // image with png_read_update_info().
  png_read_update_info(png_, info_);
  int channels = png_get_channels(png_, info_);
  DCHECK(channels == 3 || channels == 4);

  has_alpha_ = (channels == 4);

  if (interlace_type == PNG_INTERLACE_ADAM7) {
    size_t size = channels * width * height;
    interlace_buffer_ = new png_byte[size];
    if (!interlace_buffer_) {
      DLOG(WARNING) << "Allocate interlace buffer failed.";
      set_state(kError);
      longjmp(png_->jmpbuf, 1);
      return;
    }
  }

  decoded_image_data_ = AllocateImageData(
      math::Size(static_cast<int>(width), static_cast<int>(height)),
      has_alpha_);
  if (!decoded_image_data_) {
    set_state(kError);
    longjmp(png_->jmpbuf, 1);
    return;
  }

  set_state(kReadLines);
}

// Responsible for swizzeling and alpha-premultiplying a row of pixels.
template <bool has_alpha, int r, int g, int b, int a>
void FillRow(int width, uint8* dest, png_bytep source) {
  const int color_channels = has_alpha ? 4 : 3;
  for (int x = 0; x < width; ++x, dest += 4, source += color_channels) {
    uint32 alpha = static_cast<uint32>(has_alpha ? source[3] : 255);

    dest[r] = static_cast<uint8>(
        has_alpha ? FixPointUnsignedMultiply(source[0], alpha) : source[0]);
    dest[g] = static_cast<uint8>(
        has_alpha ? FixPointUnsignedMultiply(source[1], alpha) : source[1]);
    dest[b] = static_cast<uint8>(
        has_alpha ? FixPointUnsignedMultiply(source[2], alpha) : source[2]);
    dest[a] = static_cast<uint8>(alpha);
  }
}

// This function is called for every row in the image.  If the image is
// interlacing, and you turned on the interlace handler, this function will be
// called for every row in every pass. Some of these rows will not be changed
// from the previous pass.
void PNGImageDecoder::RowAvailableCallback(png_bytep row_buffer,
                                           png_uint_32 row_index) {
  TRACK_MEMORY_SCOPE("Rendering");
  DCHECK_EQ(state(), kReadLines);

  // Nothing to do if the row is unchanged, or the row is outside
  // the image bounds: libpng may send extra rows, ignore them to
  // make our lives easier.
  if (!row_buffer) {
    return;
  }

  int color_channels = has_alpha_ ? 4 : 3;
  png_bytep row = row_buffer;

  int width = decoded_image_data_->GetDescriptor().size.width();
  // For non-NUll rows of interlaced images during progressive read,
  // png_progressive_combine_row() shall combine the data for the current row
  // with the previously processed row data.
  if (interlace_buffer_) {
    row = interlace_buffer_ + (row_index * color_channels * width);
    png_progressive_combine_row(png_, row, row_buffer);
  }

  // Write the decoded row pixels to image data.
  uint8* pixel_data =
      decoded_image_data_->GetMemory() +
      decoded_image_data_->GetDescriptor().pitch_in_bytes * row_index;

  png_bytep pixel = row;

  switch (pixel_format()) {
    case render_tree::kPixelFormatRGBA8: {
      if (has_alpha_) {
        FillRow<true, 0, 1, 2, 3>(width, pixel_data, pixel);
      } else {
        FillRow<false, 0, 1, 2, 3>(width, pixel_data, pixel);
      }
    } break;
    case render_tree::kPixelFormatBGRA8: {
      if (has_alpha_) {
        FillRow<true, 2, 1, 0, 3>(width, pixel_data, pixel);
      } else {
        FillRow<false, 2, 1, 0, 3>(width, pixel_data, pixel);
      }
    } break;
    case render_tree::kPixelFormatUYVY:
    case render_tree::kPixelFormatY8:
    case render_tree::kPixelFormatU8:
    case render_tree::kPixelFormatV8:
    case render_tree::kPixelFormatUV8:
    case render_tree::kPixelFormatInvalid: {
      NOTREACHED();
    } break;
  }
}

void PNGImageDecoder::DecodeDoneCallback() { set_state(kDone); }

}  // namespace image
}  // namespace loader
}  // namespace cobalt
