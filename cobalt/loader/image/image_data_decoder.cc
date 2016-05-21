/*
 * Copyright 2016 Google Inc. All Rights Reserved.
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

#include "cobalt/loader/image/image_data_decoder.h"

#include <algorithm>

#include "base/debug/trace_event.h"

namespace cobalt {
namespace loader {
namespace image {

namespace {
// The capacity of data buffer.
uint32 kMaxBufferSizeBytes = 32 * 1024L;
}  // namespace

ImageDataDecoder::ImageDataDecoder(
    render_tree::ResourceProvider* resource_provider)
    : resource_provider_(resource_provider), state_(kWaitingForHeader) {
  CalculatePixelFormat();
}

void ImageDataDecoder::DecodeChunk(const uint8* data, size_t size) {
  TRACE_EVENT0("cobalt::loader::image_decoder",
               "ImageDataDecoder::DecodeChunk");

  size_t offset = 0;
  while (offset < size) {
    if (state_ == kError) {
      // Previous chunk causes an error, so there is nothing to do in here.
      return;
    }

    const uint8* input_bytes;
    size_t input_size;

    if (data_buffer_.empty()) {
      // Nothing in the data_buffer, so no data append needs to be performed.
      input_bytes = data + offset;
      input_size = size - offset;
      offset += input_size;
    } else {
      size_t fill_buffer_size =
          std::min(kMaxBufferSizeBytes - data_buffer_.size(), size - offset);

      // Append new data to data_buffer
      data_buffer_.insert(
          data_buffer_.begin() + static_cast<int64>(data_buffer_.size()),
          data + offset, data + offset + fill_buffer_size);

      input_bytes = &data_buffer_[0];
      input_size = data_buffer_.size();
      offset += fill_buffer_size;
    }

    size_t decoded_size = DecodeChunkInternal(input_bytes, input_size);
    size_t undecoded_size = input_size - decoded_size;

    if (undecoded_size == 0) {
      // Remove all elements from the data_buffer.
      data_buffer_.clear();
    } else {
      data_buffer_.reserve(kMaxBufferSizeBytes);
      if (data_buffer_.empty()) {
        // data_buffer is empty, so assign the undecoded data to it.
        data_buffer_.assign(data + offset - undecoded_size, data + offset);
      } else if (decoded_size != 0) {
        // data_buffer is not empty, so erase the decoded data from it.
        data_buffer_.erase(
            data_buffer_.begin(),
            data_buffer_.begin() + static_cast<int64>(decoded_size));
      }
    }
  }
}

bool ImageDataDecoder::FinishWithSuccess() {
  TRACE_EVENT0("cobalt::loader::image_decoder",
               "ImageDataDecoder::FinishWithSuccess");

  FinishInternal();

  if (state_ != kDone) {
    image_data_.reset();
  }

  return state_ == kDone;
}

void ImageDataDecoder::AllocateImageData(const math::Size& size) {
  image_data_ = resource_provider_->AllocateImageData(
      size, pixel_format(), render_tree::kAlphaFormatPremultiplied);
}

void ImageDataDecoder::CalculatePixelFormat() {
  pixel_format_ = render_tree::kPixelFormatRGBA8;
  if (!resource_provider_->PixelFormatSupported(pixel_format_)) {
    pixel_format_ = render_tree::kPixelFormatBGRA8;
  }

  DCHECK(resource_provider_->PixelFormatSupported(pixel_format_));
}

}  // namespace image
}  // namespace loader
}  // namespace cobalt
