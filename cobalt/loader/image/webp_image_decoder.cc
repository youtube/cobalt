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

#include "cobalt/loader/image/webp_image_decoder.h"

#include "base/debug/trace_event.h"
#include "base/logging.h"
#include "cobalt/loader/image/animated_webp_image.h"
#include "nb/memory_scope.h"
#include "starboard/memory.h"

namespace cobalt {
namespace loader {
namespace image {

WEBPImageDecoder::WEBPImageDecoder(
    render_tree::ResourceProvider* resource_provider)
    : ImageDataDecoder(resource_provider),
      internal_decoder_(NULL),
      has_animation_(false) {
  TRACK_MEMORY_SCOPE("Rendering");
  TRACE_EVENT0("cobalt::loader::image", "WEBPImageDecoder::WEBPImageDecoder()");
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

WEBPImageDecoder::~WEBPImageDecoder() {
  TRACE_EVENT0("cobalt::loader::image",
               "WEBPImageDecoder::~WEBPImageDecoder()");
  DeleteInternalDecoder();
}

uint8_t* WEBPImageDecoder::GetOriginalMemory() {
  return config_.output.u.RGBA.rgba;
}

size_t WEBPImageDecoder::DecodeChunkInternal(const uint8* data,
                                             size_t input_byte) {
  TRACK_MEMORY_SCOPE("Rendering");
  TRACE_EVENT0("cobalt::loader::image",
               "WEBPImageDecoder::DecodeChunkInternal()");
  if (state() == kWaitingForHeader) {
    if (!ReadHeader(data, input_byte)) {
      return 0;
    }

    if (!config_.input.has_animation) {
      if (!AllocateImageData(
              math::Size(config_.input.width, config_.input.height),
              !!config_.input.has_alpha)) {
        return 0;
      }
      if (!CreateInternalDecoder(!!config_.input.has_alpha)) {
        return 0;
      }
    } else {
      has_animation_ = true;
      animated_webp_image_ = new AnimatedWebPImage(
          math::Size(config_.input.width, config_.input.height),
          !!config_.input.has_alpha, resource_provider());
    }
    set_state(kReadLines);
  }

  if (state() == kReadLines) {
    if (!config_.input.has_animation) {
      // Copies and decodes the next available data. Returns VP8_STATUS_OK when
      // the image is successfully decoded. Returns VP8_STATUS_SUSPENDED when
      // more data is expected. Returns error in other cases.
      VP8StatusCode status = WebPIAppend(internal_decoder_, data, input_byte);
      if (status == VP8_STATUS_OK) {
        DCHECK(image_data());
        DCHECK(config_.output.u.RGBA.rgba);

        // Copy the image data over line by line.  We copy line by line instead
        // of all at once so that we can adjust for differences in pitch between
        // source and destination buffers.
        uint8* cur_src = config_.output.u.RGBA.rgba;
        uint8* cur_dest = image_data()->GetMemory();
        int height = image_data()->GetDescriptor().size.height();
        int num_pixel_bytes = image_data()->GetDescriptor().size.width() * 4;
        for (int i = 0; i < height; ++i) {
          SbMemoryCopy(cur_dest, cur_src, num_pixel_bytes);
          cur_src += config_.output.u.RGBA.stride;
          cur_dest += image_data()->GetDescriptor().pitch_in_bytes;
        }

        set_state(kDone);
      } else if (status != VP8_STATUS_SUSPENDED) {
        DLOG(ERROR) << "WebPIAppend error, status code: " << status;
        DeleteInternalDecoder();
        set_state(kError);
        return 0;
      }
    } else {
      animated_webp_image_->AppendChunk(data, input_byte);
    }
  }

  return input_byte;
}

void WEBPImageDecoder::FinishInternal() {
  if (config_.input.has_animation) {
    set_state(kDone);
  }
}

bool WEBPImageDecoder::ReadHeader(const uint8* data, size_t size) {
  TRACK_MEMORY_SCOPE("Rendering");
  TRACE_EVENT0("cobalt::loader::image", "WEBPImageDecoder::ReadHeader()");
  // Retrieve features from the bitstream. The *features structure is filled
  // with information gathered from the bitstream.
  // Returns VP8_STATUS_OK when the features are successfully retrieved. Returns
  // VP8_STATUS_NOT_ENOUGH_DATA when more data is needed to retrieve the
  // features from headers. Returns error in other cases.
  VP8StatusCode status = WebPGetFeatures(data, size, &config_.input);
  if (status == VP8_STATUS_OK) {
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
  TRACK_MEMORY_SCOPE("Rendering");
  TRACE_EVENT0("cobalt::loader::image",
               "WEBPImageDecoder::CreateInternalDecoder()");
  config_.output.colorspace = pixel_format() == render_tree::kPixelFormatRGBA8
                                  ? (has_alpha ? MODE_rgbA : MODE_RGBA)
                                  : (has_alpha ? MODE_bgrA : MODE_BGRA);
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
  TRACE_EVENT0("cobalt::loader::image",
               "WEBPImageDecoder::DeleteInternalDecoder()");
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
