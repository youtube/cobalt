// Copyright 2016 Google Inc. All Rights Reserved.
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

#include "cobalt/loader/image/image_data_decoder.h"

#include <algorithm>

#include "base/debug/trace_event.h"

namespace cobalt {
namespace loader {
namespace image {

namespace {
// Sanity check max size of data buffer.
uint32 kMaxBufferSizeBytes = 4 * 1024 * 1024L;
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
      // Nothing in |data_buffer_|, so no data append needs to be performed.
      input_bytes = data + offset;
      input_size = size - offset;
      offset += input_size;
    } else {
      DCHECK_GE(kMaxBufferSizeBytes, data_buffer_.size());
      size_t fill_buffer_size =
          std::min(kMaxBufferSizeBytes - data_buffer_.size(), size - offset);

      // Append new data to data_buffer
      data_buffer_.insert(
          data_buffer_.end(),
          data + offset, data + offset + fill_buffer_size);

      input_bytes = &data_buffer_[0];
      input_size = data_buffer_.size();
      offset += fill_buffer_size;
    }

    size_t decoded_size = DecodeChunkInternal(input_bytes, input_size);
    if (decoded_size == 0 && offset < size) {
      LOG(ERROR) << "Unable to make progress decoding image.";
      state_ = kError;
      return;
    }

    size_t undecoded_size = input_size - decoded_size;
    if (undecoded_size == 0) {
      // Remove all elements from the data_buffer.
      data_buffer_.clear();
    } else {
      if (data_buffer_.empty()) {
        if (undecoded_size > kMaxBufferSizeBytes) {
          LOG(ERROR) << "Max buffer size too small: " << undecoded_size
                     << "bytes required!";
          state_ = kError;
          return;
        }

        // |data_buffer_| is empty, so assign the undecoded data to it.
        data_buffer_.reserve(undecoded_size);
        data_buffer_.assign(data + offset - undecoded_size, data + offset);
      } else if (decoded_size != 0) {
        // |data_buffer_| is not empty, so erase the decoded data from it.
        data_buffer_.erase(
            data_buffer_.begin(),
            data_buffer_.begin() + static_cast<ptrdiff_t>(decoded_size));
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

bool ImageDataDecoder::AllocateImageData(const math::Size& size,
                                         bool has_alpha) {
  DCHECK(resource_provider_->AlphaFormatSupported(
      render_tree::kAlphaFormatOpaque));
  DCHECK(resource_provider_->AlphaFormatSupported(
      render_tree::kAlphaFormatPremultiplied));
  image_data_ = resource_provider_->AllocateImageData(
      size, pixel_format(), has_alpha ? render_tree::kAlphaFormatPremultiplied
                                      : render_tree::kAlphaFormatOpaque);
  if (!image_data_) {
    DLOG(WARNING) << "Failed to allocate image data (" << size.width() << "x"
                  << size.height() << ").";
  }
  return image_data_;
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
