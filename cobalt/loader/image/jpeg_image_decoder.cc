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

#include "cobalt/loader/image/jpeg_image_decoder.h"

#include <algorithm>
#include <utility>

#include "base/logging.h"
#include "base/trace_event/trace_event.h"
#include "cobalt/base/console_log.h"
#include "nb/memory_scope.h"
#include "third_party/libjpeg-turbo/jpegint.h"

namespace cobalt {
namespace loader {
namespace image {

namespace {

const JDIMENSION kInvalidHeight = 0xFFFFFF;
const JDIMENSION kDctScaleSize = 8;

JDIMENSION AlignUp(JDIMENSION value, JDIMENSION alignment) {
  return (value + alignment - 1) / alignment * alignment;
}

bool CanDecodeIntoYUV(const jpeg_decompress_struct& decompress_info) {
  auto comp_infos = decompress_info.cur_comp_info;

  // Only images encoded to YCbCr in three planes are supported.
  if (decompress_info.jpeg_color_space != JCS_YCbCr ||
      decompress_info.comps_in_scan != 3) {
    return false;
  }

  // Ensure that it is either YUV 420 or YUV 444.
  bool is_yuv_420 =
      comp_infos[0]->h_samp_factor == 2 && comp_infos[0]->v_samp_factor == 2 &&
      comp_infos[1]->h_samp_factor == 1 && comp_infos[1]->v_samp_factor == 1 &&
      comp_infos[2]->h_samp_factor == 1 && comp_infos[2]->v_samp_factor == 1 &&
      decompress_info.max_h_samp_factor == 2 &&
      decompress_info.max_v_samp_factor == 2;
  bool is_yuv_444 =
      comp_infos[0]->h_samp_factor == 1 && comp_infos[0]->v_samp_factor == 1 &&
      comp_infos[1]->h_samp_factor == 1 && comp_infos[1]->v_samp_factor == 1 &&
      comp_infos[2]->h_samp_factor == 1 && comp_infos[2]->v_samp_factor == 1 &&
      decompress_info.max_h_samp_factor == 1 &&
      decompress_info.max_v_samp_factor == 1;
  if (!is_yuv_420 && !is_yuv_444) {
    return false;
  }

  // The dimension of the image may not be a multiple of the sample factors,
  // this can happen when the sample factors are not 1.  In such case the
  // mapping of the u/v plane won't be even, because the mapping of the last
  // vertical line on the u/v plane will be different than the other vertical
  // lines.  Our renderer cannot handle this properly so we return false in this
  // case.
  if (decompress_info.image_width % decompress_info.max_h_samp_factor != 0 ||
      decompress_info.image_height % decompress_info.max_v_samp_factor != 0) {
    return false;
  }

  // Ensure that the DCT block size is expected.
  return comp_infos[0]->DCT_scaled_size == kDctScaleSize &&
         comp_infos[1]->DCT_scaled_size == kDctScaleSize &&
         comp_infos[2]->DCT_scaled_size == kDctScaleSize;
}

void ErrorManagerExit(j_common_ptr common_ptr) {
  // Returns the control to the setjmp point. The buffer which is filled by a
  // previous call to setjmp that contains information to store the environment
  // to that point.
  jmp_buf* buffer = static_cast<jmp_buf*>(common_ptr->client_data);
  longjmp(*buffer, 1);
}

void SourceManagerInitSource(j_decompress_ptr decompress_ptr) {
  // no-op.
}

boolean SourceManagerFillInputBuffer(j_decompress_ptr decompress_ptr) {
  // Normally, this function is called when we need to read more of the encoded
  // buffer into memory and a return false indicates that we have no data to
  // supply yet, but in our case, the encoded buffer is always in memory, so
  // this should never get called unless there is an error, in which case we
  // return false to indicate that.
  return false;
}

boolean SourceManagerResyncToRestart(j_decompress_ptr decompress_ptr,
                                     int desired) {
  return false;
}

void SourceManagerTermSource(j_decompress_ptr decompress_ptr) {
  // no-op.
}

void SourceManagerSkipInputData(j_decompress_ptr decompress_ptr,
                                long num_bytes) {  // NOLINT(runtime/int)
  // Since all our data that is required to be decoded should be in the buffer
  // already, trying to skip beyond it means that there is some kind of error or
  // corrupt input data. We acknowledge these error cases by setting
  // |bytes_in_buffer| to 0 which will result in a call to
  // SourceManagerFillInputBuffer() which will return false to indicate that an
  // error has occurred.
  size_t bytes_to_skip = std::min(decompress_ptr->src->bytes_in_buffer,
                                  static_cast<size_t>(num_bytes));
  decompress_ptr->src->bytes_in_buffer -= bytes_to_skip;
  decompress_ptr->src->next_input_byte += bytes_to_skip;
}

}  // namespace

JPEGImageDecoder::JPEGImageDecoder(
    render_tree::ResourceProvider* resource_provider,
    const base::DebuggerHooks& debugger_hooks,
    bool allow_image_decoding_to_multi_plane)
    : ImageDataDecoder(resource_provider, debugger_hooks),
      allow_image_decoding_to_multi_plane_(
          allow_image_decoding_to_multi_plane) {
  TRACE_EVENT0("cobalt::loader::image", "JPEGImageDecoder::JPEGImageDecoder()");
  TRACK_MEMORY_SCOPE("Rendering");
  memset(&info_, 0, sizeof(info_));
  memset(&source_manager_, 0, sizeof(source_manager_));

  info_.err = jpeg_std_error(&error_manager_);
  error_manager_.error_exit = &ErrorManagerExit;

  // Allocate and initialize JPEG decompression object.
  jpeg_create_decompress(&info_);

  info_.src = &source_manager_;

  // Set up callback functions.
  // Even some of them are no-op, we have to set them up for jpeg lib.
  source_manager_.init_source = SourceManagerInitSource;
  source_manager_.fill_input_buffer = SourceManagerFillInputBuffer;
  source_manager_.skip_input_data = SourceManagerSkipInputData;
  source_manager_.resync_to_restart = SourceManagerResyncToRestart;
  source_manager_.term_source = SourceManagerTermSource;
}

JPEGImageDecoder::~JPEGImageDecoder() {
  // Deallocate a JPEG decompression object.
  jpeg_destroy_decompress(&info_);
}

size_t JPEGImageDecoder::DecodeChunkInternal(const uint8* data,
                                             size_t input_byte) {
  TRACK_MEMORY_SCOPE("Rendering");
  TRACE_EVENT0("cobalt::loader::image",
               "JPEGImageDecoder::DecodeChunkInternal()");
  // |client_data| is available for use by application.
  jmp_buf jump_buffer;
  info_.client_data = &jump_buffer;

  // int setjmp(jmp_buf env) saves the current environment (this program state),
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
  if (setjmp(jump_buffer)) {
    // image data is empty.
    DLOG(WARNING) << "Decoder encounters an error.";
    set_state(kError);
    return 0;
  }
  MSVC_POP_WARNING();

  // Next byte to read from buffer.
  info_.src->next_input_byte = reinterpret_cast<const JOCTET*>(data);
  // Number of bytes remaining in buffer.
  info_.src->bytes_in_buffer = input_byte;

  if (state() == kWaitingForHeader) {
    if (!ReadHeader()) {
      // Data is not enough for decoding the header.
      return 0;
    }

    if (!StartDecompress()) {
      return 0;
    }
    set_state(kReadLines);
  }

  if (state() == kReadLines) {
    if (info_.buffered_image) {  // Progressive JPEG.
      if (!DecodeProgressiveJPEG()) {
        // The size of undecoded bytes is info_.src->bytes_in_buffer.
        return input_byte - info_.src->bytes_in_buffer;
      }
    } else if (!ReadLines()) {  // Baseline sequential JPEG.
      // The size of undecoded bytes is info_.src->bytes_in_buffer.
      return input_byte - info_.src->bytes_in_buffer;
    }

    if (!jpeg_finish_decompress(&info_)) {
      // In this case, we did read all the rows, so we don't really have to
      // treat this as an error.
      DLOG(WARNING) << "Data source requests suspension of the decompressor.";
    }
    set_state(kDone);
  }

  return input_byte;
}

scoped_refptr<Image> JPEGImageDecoder::FinishInternal() {
  if (state() != kDone) {
    decoded_image_data_.reset();
    raw_image_memory_.reset();
    return NULL;
  }

  SB_DCHECK(output_format_ != kOutputFormatInvalid);

  if (output_format_ == kOutputFormatRGBA ||
      output_format_ == kOutputFormatBGRA) {
    SB_DCHECK(decoded_image_data_);
    return CreateStaticImage(std::move(decoded_image_data_));
  }

  SB_DCHECK(output_format_ == kOutputFormatYUV);
  SB_DCHECK(raw_image_memory_);

  render_tree::MultiPlaneImageDataDescriptor descriptor(
      render_tree::kMultiPlaneImageFormatYUV3PlaneBT601FullRange);

  auto uv_plane_width = y_plane_width_ / h_sample_factor_;
  auto uv_plane_height = y_plane_height_ / v_sample_factor_;
  auto y_plane_size = y_plane_width_ * y_plane_height_;
  auto uv_plane_size = uv_plane_width * uv_plane_height;

  math::Size plane_size(info_.image_width, info_.image_height);
  descriptor.AddPlane(
      0, render_tree::ImageDataDescriptor(
             plane_size, render_tree::kPixelFormatY8,
             render_tree::kAlphaFormatPremultiplied, y_plane_width_));

  plane_size.SetSize(plane_size.width() / h_sample_factor_,
                     plane_size.height() / v_sample_factor_);
  descriptor.AddPlane(y_plane_size, render_tree::ImageDataDescriptor(
                                        plane_size, render_tree::kPixelFormatU8,
                                        render_tree::kAlphaFormatPremultiplied,
                                        uv_plane_width));
  descriptor.AddPlane(
      y_plane_size + uv_plane_size,
      render_tree::ImageDataDescriptor(plane_size, render_tree::kPixelFormatV8,
                                       render_tree::kAlphaFormatPremultiplied,
                                       uv_plane_width));

  auto image = resource_provider()->CreateMultiPlaneImageFromRawMemory(
      std::move(raw_image_memory_), descriptor);
  SB_DCHECK(image);
  return new StaticImage(image);
}

bool JPEGImageDecoder::ReadHeader() {
  TRACK_MEMORY_SCOPE("Rendering");
  TRACE_EVENT0("cobalt::loader::image", "JPEGImageDecoder::ReadHeader()");
  if (jpeg_read_header(&info_, true) == JPEG_SUSPENDED) {
    // Since |jpeg_read_header| doesn't have enough data, go back to the state
    // before reading the header.
    info_.global_state = DSTATE_START;
    return false;
  }

  if (allow_image_decoding_to_multi_plane_ && CanDecodeIntoYUV(info_)) {
    output_format_ = kOutputFormatYUV;
  } else if (pixel_format() == render_tree::kPixelFormatRGBA8) {
    output_format_ = kOutputFormatRGBA;
  } else if (pixel_format() == render_tree::kPixelFormatBGRA8) {
    output_format_ = kOutputFormatBGRA;
  } else {
    NOTREACHED() << "Unsupported pixel format: " << pixel_format();
  }
  if (output_format_ == kOutputFormatYUV) {
    info_.out_color_space = JCS_YCbCr;
    // Enable raw data output to avoid any copying.
    info_.raw_data_out = TRUE;

    auto y_info = info_.cur_comp_info[0];
    h_sample_factor_ = y_info->h_samp_factor;
    v_sample_factor_ = y_info->v_samp_factor;
    y_plane_width_ = AlignUp(y_info->width_in_blocks,
                             static_cast<JDIMENSION>(y_info->h_samp_factor)) *
                     kDctScaleSize;
    y_plane_height_ = AlignUp(y_info->height_in_blocks,
                              static_cast<JDIMENSION>(y_info->v_samp_factor)) *
                      kDctScaleSize;

    // Raw read mode requires that the output data is aligned to dct block size.
    auto aligned_size = math::Size(static_cast<int>(y_plane_width_),
                                   static_cast<int>(y_plane_height_));
    auto y_plane_size_in_bytes = aligned_size.width() * aligned_size.height();
    auto uv_plane_size_in_bytes =
        y_plane_size_in_bytes / h_sample_factor_ / v_sample_factor_;
    raw_image_memory_ = resource_provider()->AllocateRawImageMemory(
        y_plane_size_in_bytes + uv_plane_size_in_bytes * 2, sizeof(void*));
    return raw_image_memory_ != NULL;
  }
  // TODO: switch libjpeg version to support JCS_RGBA_8888 output.
  info_.out_color_space = JCS_RGB;

  decoded_image_data_ =
      AllocateImageData(math::Size(static_cast<int>(info_.image_width),
                                   static_cast<int>(info_.image_height)),
                        false);
  return decoded_image_data_ != NULL;
}

bool JPEGImageDecoder::StartDecompress() {
  TRACK_MEMORY_SCOPE("Rendering");
  TRACE_EVENT0("cobalt::loader::image", "JPEGImageDecoder::StartDecompress()");
  // jpeg_has_multiple_scans() returns TRUE if the incoming image file has more
  // than one scan.
  info_.buffered_image = jpeg_has_multiple_scans(&info_);

  // Compute output image dimensions
  jpeg_calc_output_dimensions(&info_);

  if (!jpeg_start_decompress(&info_)) {
    LOG(WARNING) << "Start decompressor failed.";
    set_state(kError);
    return false;
  }

  return true;
}

// A baseline sequential JPEG is stored as one top-to-bottom scan of the image.
// Progressive JPEG divides the file into a series of scans. It is starting with
// a very low quality image, and then following scans gradually improve the
// quality.
// TODO: support displaying the low resolution image while decoding the
// progressive JPEG.
bool JPEGImageDecoder::DecodeProgressiveJPEG() {
  TRACK_MEMORY_SCOPE("Rendering");
  TRACE_EVENT0("cobalt::loader::image",
               "JPEGImageDecoder::DecodeProgressiveJPEG()");
  int status;
  do {
    // |jpeg_consume_input| decodes the input data as it arrives.
    status = jpeg_consume_input(&info_);
  } while (status == JPEG_REACHED_SOS || status == JPEG_ROW_COMPLETED ||
           status == JPEG_SCAN_COMPLETED);

  while (info_.output_scanline == kInvalidHeight ||
         info_.output_scanline <= info_.output_height) {
    if (info_.output_scanline == 0) {
      // Initialize for an output pass in buffered-image mode.
      // The |input_scan_number| indicates the scan of the image to be
      // processed.
      if (!jpeg_start_output(&info_, info_.input_scan_number)) {
        // Decompression is suspended.
        return false;
      }
    }

    if (info_.output_scanline == kInvalidHeight) {
      // Recover from the previous set flag.
      info_.output_scanline = 0;
    }

    if (!ReadLines()) {
      if (info_.output_scanline == 0) {
        // Data is not enough for one line scan, so flag the |output_scanline|
        // to make sure that we don't call |jpeg_start_output| multiple times
        // for the same scan.
        info_.output_scanline = kInvalidHeight;
      }
      return false;
    }

    if (info_.output_scanline >= info_.output_height) {
      // Finish up after an output pass in buffered-image mode.
      if (!jpeg_finish_output(&info_)) {
        // Decompression is suspended due to
        // input_scan_number <= output_scan_number and EOI is not reached.
        // The suspension will be recovered when more data are coming.
        return false;
      }

      // The output scan number is the notional scan being processed by the
      // output side. The decompressor will not allow output scan number to get
      // ahead of input scan number.
      DCHECK_GE(info_.input_scan_number, info_.output_scan_number);
      // This scan pass is done, so reset the output scanline.
      info_.output_scanline = 0;
      if (info_.input_scan_number == info_.output_scan_number) {
        // No more scan needed at this point.
        break;
      }
    }
  }

  // |jpeg_input_complete| tests for the end of image.
  if (!jpeg_input_complete(&info_)) {
    return false;
  }

  return true;
}

// Responsible for swizzeling and alpha-premultiplying a row of pixels.
template <int r, int g, int b, int a>
void FillRow(int width, uint8* dest, JSAMPLE* source) {
  for (int x = 0; x < width; ++x, source += 3, dest += 4) {
    dest[r] = source[0];
    dest[g] = source[1];
    dest[b] = source[2];
    dest[a] = 0xFF;
  }
}

bool JPEGImageDecoder::ReadYUVLines() {
  DCHECK(output_format_ == kOutputFormatYUV);
  TRACK_MEMORY_SCOPE("Rendering");
  TRACE_EVENT0("cobalt::loader::image", "JPEGImageDecoder::ReadYUVLines()");

  while (info_.output_scanline < info_.output_height) {
    JSAMPROW y[kDctScaleSize * 2], u[kDctScaleSize], v[kDctScaleSize];
    JSAMPARRAY planes[3] = {y, u, v};

    auto offset = info_.output_scanline * y_plane_width_;
    auto y_plane_size = y_plane_width_ * y_plane_height_;
    auto uv_plane_size = y_plane_size / (h_sample_factor_ * v_sample_factor_);

    uint8* y_plane_addr = raw_image_memory_->GetMemory() + offset;
    uint8* u_plane_addr = raw_image_memory_->GetMemory() + y_plane_size +
                          offset / (h_sample_factor_ * v_sample_factor_);
    uint8* v_plane_addr = raw_image_memory_->GetMemory() + y_plane_size +
                          uv_plane_size +
                          offset / (h_sample_factor_ * v_sample_factor_);
    for (JDIMENSION i = 0; i < kDctScaleSize * v_sample_factor_; ++i) {
      y[i] = y_plane_addr + y_plane_width_ * i;
    }
    for (JDIMENSION i = 0; i < kDctScaleSize; ++i) {
      u[i] = u_plane_addr + y_plane_width_ / h_sample_factor_ * i;
      v[i] = v_plane_addr + y_plane_width_ / h_sample_factor_ * i;
    }
    auto size =
        jpeg_read_raw_data(&info_, planes, kDctScaleSize * v_sample_factor_);
    if (size != kDctScaleSize * v_sample_factor_) {
      return false;
    }
  }

  return true;
}

bool JPEGImageDecoder::ReadRgbaOrGbraLines() {
  DCHECK(output_format_ == kOutputFormatRGBA ||
         output_format_ == kOutputFormatBGRA);

  TRACK_MEMORY_SCOPE("Rendering");
  TRACE_EVENT0("cobalt::loader::image",
               "JPEGImageDecoder::ReadRgbaOrGbraLines()");

  // Creation of 2-D sample arrays which is for one row.
  // See the comments in jmemmgr.c.
  JSAMPARRAY buffer = (*info_.mem->alloc_sarray)(
      (j_common_ptr)&info_, JPOOL_IMAGE, info_.output_width * 4, 1);

  while (info_.output_scanline < info_.output_height) {
    // |info_.output_scanline| would be increased 1 after reading one row.
    int row_index = static_cast<int>(info_.output_scanline);

    // jpeg_read_scanlines() returns up to the maximum number of scanlines of
    // decompressed image data. This may be less than the number requested in
    // cases such as bottom of image, data source suspension, and operating
    // modes that emit multiple scanlines at a time. Image data should be
    // returned in top-to-bottom scanline order.
    // TODO: Investigate the performance improvements by processing
    // multiple pixel rows. It may have performance advantage to use values
    // larger than 1. For example, JPEG images often use 4:2:0 downsampling, and
    // in that case libjpeg needs to make an additional copy of half the image
    // pixels(see merged_2v_upsample()).
    if (jpeg_read_scanlines(&info_, buffer, 1) != 1) {
      return false;
    }

    // Write the decoded row pixels to image data.
    uint8* pixel_data =
        decoded_image_data_->GetMemory() +
        decoded_image_data_->GetDescriptor().pitch_in_bytes * row_index;

    JSAMPLE* sample_buffer = *buffer;
    switch (output_format_) {
      case kOutputFormatRGBA: {
        FillRow<0, 1, 2, 3>(static_cast<int>(info_.output_width), pixel_data,
                            sample_buffer);
      } break;
      case kOutputFormatBGRA: {
        FillRow<2, 1, 0, 3>(static_cast<int>(info_.output_width), pixel_data,
                            sample_buffer);
      } break;
      case kOutputFormatInvalid:
      case kOutputFormatYUV: {
        NOTREACHED();
      } break;
    }
  }

  return true;
}

bool JPEGImageDecoder::ReadLines() {
  return output_format_ == kOutputFormatYUV ? ReadYUVLines()
                                            : ReadRgbaOrGbraLines();
}

}  // namespace image
}  // namespace loader
}  // namespace cobalt
