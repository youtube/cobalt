// Copyright 2016 The Cobalt Authors. All Rights Reserved.
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

#include "base/logging.h"
#include "base/trace_event/trace_event.h"
#include "cobalt/loader/image/animated_webp_image.h"
#include "nb/memory_scope.h"
#include "starboard/configuration.h"
#include "starboard/memory.h"

namespace cobalt {
namespace loader {
namespace image {

WEBPImageDecoder::WEBPImageDecoder(
    render_tree::ResourceProvider* resource_provider,
    const base::DebuggerHooks& debugger_hooks)
    : ImageDataDecoder(resource_provider, debugger_hooks),
      internal_decoder_(NULL) {
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
}

WEBPImageDecoder::~WEBPImageDecoder() {
  TRACE_EVENT0("cobalt::loader::image",
               "WEBPImageDecoder::~WEBPImageDecoder()");
  DeleteInternalDecoder();
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

    if (config_.input.has_animation) {
      animated_webp_image_ = new AnimatedWebPImage(
          math::Size(config_.input.width, config_.input.height),
          !!config_.input.has_alpha, resource_provider(), debugger_hooks());
    } else {
      decoded_image_data_ = AllocateImageData(
          math::Size(config_.input.width, config_.input.height),
          !!config_.input.has_alpha);
      if (decoded_image_data_ == NULL) {
        return 0;
      }
      if (!CreateInternalDecoder()) {
        return 0;
      }
    }
    set_state(kReadLines);
  }

  if (state() == kReadLines) {
    if (config_.input.has_animation) {
      animated_webp_image_->AppendChunk(data, input_byte);
    } else {
      auto data_to_decode = data;
      auto bytes_to_decode = input_byte;

      // |cached_uncompressed_data_| is non-empty indicates that the last
      // WebPIUpdate() returns VP8_STATUS_SUSPENDED, and we have to concatenate
      // the data before send it to WebPIUpdate() again.  This should rarely
      // happen.
      if (!cached_uncompressed_data_.empty()) {
        cached_uncompressed_data_.insert(
            cached_uncompressed_data_.end(),
            reinterpret_cast<const char*>(data),
            reinterpret_cast<const char*>(data) + input_byte);
        data_to_decode =
            reinterpret_cast<const uint8*>(cached_uncompressed_data_.data());
        bytes_to_decode = cached_uncompressed_data_.size();
      }
      // Send the available data to the decoder without copying. Note that as
      // webp images are mostly used in the form of animated webp, the data sent
      // to DecodeChunkInternal() contains a whole webp image most of the time.
      // Not making an extra copy of the data inside the internal decoder is
      // more optimal in such casts, so WebPIUpdate() is used, instead of
      // WebPIAppend().  The latter makes a copying of the data inside the
      // internal decoder.
      // Returns VP8_STATUS_OK when the image is successfully decoded. Returns
      // VP8_STATUS_SUSPENDED when more data is expected, in such case we have
      // to cache the data already appended as required by WebPIUpdate().
      // Any other return codes indicate an error.
      VP8StatusCode status =
          WebPIUpdate(internal_decoder_, data_to_decode, bytes_to_decode);
      if (status == VP8_STATUS_OK) {
        DCHECK(decoded_image_data_);
        DCHECK(config_.output.u.RGBA.rgba);

        DCHECK_EQ(config_.output.u.RGBA.stride,
                  decoded_image_data_->GetDescriptor().pitch_in_bytes);
        set_state(kDone);
      } else if (status == VP8_STATUS_SUSPENDED) {
        // Only copying the data into |cached_uncompressed_data_| when it is
        // empty, as otherwise the data has already been appended into it before
        // calling WebPIUpdate().
        if (cached_uncompressed_data_.empty()) {
          cached_uncompressed_data_.assign(
              reinterpret_cast<const char*>(data),
              reinterpret_cast<const char*>(data) + input_byte);
        }
      } else {
        DLOG(ERROR) << "WebPIAppend error, status code: " << status;
        DeleteInternalDecoder();
        set_state(kError);
        return 0;
      }
    }
  }

  return input_byte;
}

scoped_refptr<Image> WEBPImageDecoder::FinishInternal() {
  if (config_.input.has_animation) {
    set_state(kDone);
    return animated_webp_image_;
  }
  if (state() != kDone) {
    decoded_image_data_.reset();
    return NULL;
  }
  SB_DCHECK(decoded_image_data_);
  return CreateStaticImage(std::move(decoded_image_data_));
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

bool WEBPImageDecoder::CreateInternalDecoder() {
  TRACK_MEMORY_SCOPE("Rendering");
  TRACE_EVENT0("cobalt::loader::image",
               "WEBPImageDecoder::CreateInternalDecoder()");
  bool has_alpha = !!config_.input.has_alpha;
  config_.output.colorspace = pixel_format() == render_tree::kPixelFormatRGBA8
                                  ? (has_alpha ? MODE_rgbA : MODE_RGBA)
                                  : (has_alpha ? MODE_bgrA : MODE_BGRA);

  auto image_data_descriptor = decoded_image_data_->GetDescriptor();
  config_.output.u.RGBA.rgba = decoded_image_data_->GetMemory();
  config_.output.u.RGBA.stride = image_data_descriptor.pitch_in_bytes;
  config_.output.u.RGBA.size = image_data_descriptor.pitch_in_bytes *
                               image_data_descriptor.size.height();
  config_.output.is_external_memory = 1;
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
