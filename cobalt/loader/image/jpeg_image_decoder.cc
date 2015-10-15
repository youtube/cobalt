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

#include "cobalt/loader/image/jpeg_image_decoder.h"

#include <algorithm>

#include "base/debug/trace_event.h"
#include "base/logging.h"
#include "third_party/libjpeg/jpegint.h"

namespace cobalt {
namespace loader {
namespace image {

namespace {

// Since there is no theoretical maximum of JPEG header, we set a very large
// buffer size to hold the data. The max capacity for CircularBufferShell can be
// set to anything, and it will only allocate enough to hold what is needs to.
// (within a factor of 2).
uint32 kMaxBufferSizeBytes = 1024 * 1024L;
JDIMENSION kInvalidHeight = 0xFFFFFF;

void ErrorManagerExit(j_common_ptr common_ptr) {
  // Returns the control to the setjmp point. The buffer which is filled by a
  // previous call to setjmp that contains information to store the environment
  // to that point.
  jmp_buf* buffer = static_cast<jmp_buf*>(common_ptr->client_data);
  longjmp(*buffer, 1);
}

void SourceManagerInitSource(j_decompress_ptr decompress_ptr) {
  UNREFERENCED_PARAMETER(decompress_ptr);
  // no-op.
}

boolean SourceManagerFillInputBuffer(j_decompress_ptr decompress_ptr) {
  UNREFERENCED_PARAMETER(decompress_ptr);
  // Normally, this function is called when we need to read more of the encoded
  // buffer into memory and a return false indicates that we have no data to
  // supply yet, but in our case, the encoded buffer is always in memory, so
  // this should never get called unless there is an error, in which case we
  // return false to indicate that.
  return false;
}

boolean SourceManagerResyncToRestart(j_decompress_ptr decompress_ptr,
                                     int desired) {
  UNREFERENCED_PARAMETER(decompress_ptr);
  UNREFERENCED_PARAMETER(desired);
  return false;
}

void SourceManagerTermSource(j_decompress_ptr decompress_ptr) {
  UNREFERENCED_PARAMETER(decompress_ptr);
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
    render_tree::ResourceProvider* resource_provider)
    : ImageDataDecoder(resource_provider),
      state_(kWaitingForHeader),
      data_buffer_(new base::CircularBufferShell(kMaxBufferSizeBytes)) {
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

void JPEGImageDecoder::DecodeChunk(const char* data, size_t size) {
  TRACE_EVENT0("cobalt::loader::image_decoder",
               "JPEGImageDecoder::DecodeChunk");
  if (state_ == kError) {
    // Previous chunk causes an error, so there is nothing to do in here.
    return;
  }

  if (size == 0) {
    DLOG(WARNING) << "Decoder received 0 bytes.";
    return;
  }

  // |client_data| is available for use by application.
  jmp_buf jump_buffer;
  info_.client_data = &jump_buffer;

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
  if (setjmp(jump_buffer)) {
    // |image_data_| is empty.
    DLOG(WARNING) << "Decoder encounters an error.";
    state_ = kError;
    return;
  }
MSVC_POP_WARNING();

  const char* next_input_byte;
  size_t bytes_in_buffer;
  scoped_array<char> input_byte;

  if (data_buffer_->GetLength() == 0) {
    // No need to save the data to |data_buffer_|.
    next_input_byte = data;
    bytes_in_buffer = size;
  } else {
    // Append the new data to the end of |data_buffer_|.
    size_t bytes = 0;
    DCHECK_LT(size, kMaxBufferSizeBytes - data_buffer_->GetLength());
    data_buffer_->Write(data, size, &bytes);
    DCHECK_EQ(bytes, size);

    size_t data_length = data_buffer_->GetLength();
    input_byte.reset(new char[data_length]);
    data_buffer_->Read(input_byte.get(), data_length, &bytes);
    DCHECK_EQ(bytes, data_length);

    next_input_byte = input_byte.get();
    bytes_in_buffer = data_length;
  }

  // Next byte to read from buffer.
  info_.src->next_input_byte = reinterpret_cast<const JOCTET*>(next_input_byte);
  // Number of bytes remaining in buffer.
  info_.src->bytes_in_buffer = bytes_in_buffer;

  if (state_ == kWaitingForHeader) {
    if (!ReadHeader()) {
      DCHECK_EQ(0, data_buffer_->GetLength());

      // Data is not enough for decoding the header. Save the undecoded bytes
      // in |data_buffer_|.
      size_t bytes_written = 0;
      data_buffer_->Write(next_input_byte, bytes_in_buffer, &bytes_written);
      DCHECK_EQ(bytes_written, bytes_in_buffer);
      return;
    }

    if (!StartDecompress()) {
      DLOG(WARNING) << "Start decompressor failed.";
      state_ = kError;
      return;
    }
    state_ = kReadLines;
  }

  if (state_ == kReadLines) {
    if (info_.buffered_image) {  // Progressive JPEG.
      if (!DecodeProgressiveJPEG()) {
        CacheSourceBytesToBuffer();
        return;
      }
    } else if (!ReadLines()) {  // Baseline sequential JPEG.
      CacheSourceBytesToBuffer();
      return;
    }

    if (!jpeg_finish_decompress(&info_)) {
      DLOG(ERROR) << "Data source requests suspension of the decompressor.";
      state_ = kError;
      return;
    }
    state_ = kDone;
  }
}

void JPEGImageDecoder::Finish() {
  TRACE_EVENT0("cobalt::loader::image_decoder", "JPEGImageDecoder::Finish");

  if (state_ != kDone) {
    image_data_.reset();
  }
}

bool JPEGImageDecoder::ReadHeader() {
  if (jpeg_read_header(&info_, true) == JPEG_SUSPENDED) {
    // Since |jpeg_read_header| doesn't have enough data, go back to the state
    // before reading the header.
    info_.global_state = DSTATE_START;
    return false;
  }

  AllocateImageData(math::Size(static_cast<int>(info_.image_width),
                               static_cast<int>(info_.image_height)));
  return true;
}

bool JPEGImageDecoder::StartDecompress() {
  // jpeg_has_multiple_scans() returns TRUE if the incoming image file has more
  // than one scan.
  info_.buffered_image = jpeg_has_multiple_scans(&info_);

  // TODO(***REMOVED***): switch libjpeg version to support JCS_RGBA_8888 output.
  info_.out_color_space = JCS_RGB;

  // Compute output image dimensions
  jpeg_calc_output_dimensions(&info_);

  if (!jpeg_start_decompress(&info_)) {
    return false;
  }

  return true;
}

// A baseline sequential JPEG is stored as one top-to-bottom scan of the image.
// Progressive JPEG divides the file into a series of scans. It is starting with
// a very low quality image, and then following scans gradually improve the
// quality.
// TODO(***REMOVED***): support displaying the low resolution image while decoding
// the progressive JPEG.
bool JPEGImageDecoder::DecodeProgressiveJPEG() {
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

    if (info_.output_scanline == info_.output_height) {
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

bool JPEGImageDecoder::ReadLines() {
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
    // TODO(***REMOVED***): Investigate the performance improvements by processing
    // multiple pixel rows. It may have performance advantage to use values
    // larger than 1. For example, JPEG images often use 4:2:0 downsampling, and
    // in that case libjpeg needs to make an additional copy of half the image
    // pixels(see merged_2v_upsample()).
    if (jpeg_read_scanlines(&info_, buffer, 1) != 1) {
      return false;
    }

    // Write the decoded row pixels to the |image_data_|.
    uint8* pixel_data = image_data_->GetMemory() +
                        image_data_->GetDescriptor().pitch_in_bytes * row_index;

    JSAMPLE* sample_buffer = *buffer;
    for (uint32 x = 0; x < info_.output_width; ++x, sample_buffer += 3) {
      *pixel_data++ = sample_buffer[0];
      *pixel_data++ = sample_buffer[1];
      *pixel_data++ = sample_buffer[2];
      *pixel_data++ = 0xFF;
    }
  }

  return true;
}

void JPEGImageDecoder::AllocateImageData(const math::Size& size) {
  image_data_ = resource_provider_->AllocateImageData(
      size, render_tree::kPixelFormatRGBA8,
      render_tree::kAlphaFormatPremultiplied);
}

void JPEGImageDecoder::CacheSourceBytesToBuffer() {
  if (info_.src->bytes_in_buffer != 0) {
    const uint8* data =
        reinterpret_cast<const uint8*>(info_.src->next_input_byte);
    size_t size = info_.src->bytes_in_buffer;

    // Save the undecoded bytes in |data_buffer_|.
    size_t bytes_written = 0;
    data_buffer_->Write(data, size, &bytes_written);
    DCHECK_EQ(size, bytes_written);
  }
}

}  // namespace image
}  // namespace loader
}  // namespace cobalt
