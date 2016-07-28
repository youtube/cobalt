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

#include "cobalt/loader/image/webp_image_decoder.h"

#include "base/logging.h"

namespace cobalt {
namespace loader {
namespace image {

WEBPImageDecoder::WEBPImageDecoder(
    render_tree::ResourceProvider* resource_provider)
    : ImageDataDecoder(resource_provider), internal_decoder_(NULL) {
  // Initialize the configuration as empty.
  WebPInitDecoderConfig(&config_);
  // Skip the in-loop filtering.
  config_.options.bypass_filtering = 1;
  // Use faster pointwise upsampler.
  config_.options.no_fancy_upsampling = 1;
  // Don't use multi-threaded decoding.
  config_.options.use_threads = 0;
  // Discard enhancement layer.
  config_.options.no_enhancement = 1;
}

WEBPImageDecoder::~WEBPImageDecoder() { DeleteInternalDecoder(); }

size_t WEBPImageDecoder::DecodeChunkInternal(const uint8* data,
                                             size_t input_byte) {
  const uint8* next_input_byte = data;
  size_t bytes_in_buffer = input_byte;

  if (state() == kWaitingForHeader) {
    bool has_alpha = false;
    if (!ReadHeader(next_input_byte, bytes_in_buffer, &has_alpha)) {
      return 0;
    }

    if (!CreateInternalDecoder(has_alpha)) {
      return 0;
    }
    set_state(kReadLines);
  }

  if (state() == kReadLines) {
    // Copies and decodes the next available data. Returns VP8_STATUS_OK when
    // the image is successfully decoded. Returns VP8_STATUS_SUSPENDED when more
    // data is expected. Returns error in other cases.
    VP8StatusCode status =
        WebPIAppend(internal_decoder_, next_input_byte, bytes_in_buffer);
    if (status == VP8_STATUS_OK) {
      DCHECK(image_data());
      DCHECK(config_.output.u.RGBA.rgba);
      memcpy(image_data()->GetMemory(), config_.output.u.RGBA.rgba,
             config_.output.u.RGBA.size);
      DeleteInternalDecoder();
      set_state(kDone);
    } else if (status != VP8_STATUS_SUSPENDED) {
      DLOG(ERROR) << "WebPIAppend error, status code: " << status;
      DeleteInternalDecoder();
      set_state(kError);
    }
  }

  return input_byte;
}

bool WEBPImageDecoder::ReadHeader(const uint8* data, size_t size,
                                  bool* has_alpha) {
  // Retrieve features from the bitstream. The *features structure is filled
  // with information gathered from the bitstream.
  // Returns VP8_STATUS_OK when the features are successfully retrieved. Returns
  // VP8_STATUS_NOT_ENOUGH_DATA when more data is needed to retrieve the
  // features from headers. Returns error in other cases.
  VP8StatusCode status = WebPGetFeatures(data, size, &config_.input);
  if (status == VP8_STATUS_OK) {
    int width = config_.input.width;
    int height = config_.input.height;
    *has_alpha = !!config_.input.has_alpha;

    AllocateImageData(math::Size(width, height));
    return true;
  } else if (status == VP8_STATUS_NOT_ENOUGH_DATA) {
    // Data is not enough for decoding the header.
    return false;
  } else {
    DLOG(ERROR) << "WebPGetFeatures error, status code: " << status;
    set_state(kError);
    return false;
  }
}

bool WEBPImageDecoder::CreateInternalDecoder(bool has_alpha) {
  config_.output.colorspace = has_alpha ? MODE_rgbA : MODE_RGBA;
  config_.output.u.RGBA.stride = image_data()->GetDescriptor().pitch_in_bytes;
  config_.output.u.RGBA.size =
      static_cast<size_t>(config_.output.u.RGBA.stride *
                          image_data()->GetDescriptor().size.height());
  // We don't use image buffer as the decoding buffer because libwebp will read
  // from it while we assume that our image buffer is write only.
  config_.output.is_external_memory = 0;

  // Instantiate a new incremental decoder object with the requested
  // configuration.
  internal_decoder_ = WebPIDecode(NULL, 0, &config_);

  if (internal_decoder_ == NULL) {
    DLOG(WARNING) << "Create internal WEBP decoder failed.";
    set_state(kError);
    return false;
  }
  return true;
}

void WEBPImageDecoder::DeleteInternalDecoder() {
  if (internal_decoder_) {
    // Deletes the WebPIDecoder object and associated memory. Must always be
    // called if WebPIDecode succeeded.
    WebPIDelete(internal_decoder_);
    internal_decoder_ = NULL;
    WebPFreeDecBuffer(&config_.output);
  }
}

}  // namespace image
}  // namespace loader
}  // namespace cobalt
