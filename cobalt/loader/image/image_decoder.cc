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

#include "cobalt/loader/image/image_decoder.h"

#include <algorithm>

#include "cobalt/loader/image/jpeg_image_decoder.h"
#include "cobalt/loader/image/png_image_decoder.h"
#include "cobalt/loader/image/stub_image_decoder.h"
#include "cobalt/loader/image/webp_image_decoder.h"

namespace cobalt {
namespace loader {
namespace image {

namespace {

bool s_use_stub_image_decoder = false;

}  // namespace

ImageDecoder::ImageDecoder(render_tree::ResourceProvider* resource_provider,
                           const SuccessCallback& success_callback,
                           const ErrorCallback& error_callback)
    : resource_provider_(resource_provider),
      success_callback_(success_callback),
      error_callback_(error_callback),
      error_state_(kNoError) {
  signature_cache_.position = 0;

  DCHECK(resource_provider_);
}

void ImageDecoder::DecodeChunk(const char* data, size_t size) {
  if (error_state_ != kNoError) {
    // Do not attempt to continue processing data if we are in an error state.
    DCHECK(!decoder_);
    return;
  }

  if (size == 0) {
    DLOG(WARNING) << "Decoder received 0 bytes.";
    return;
  }

  const uint8* input_bytes = reinterpret_cast<const uint8*>(data);
  // Call different types of decoders by matching the image signature. If it is
  // the first time decoding a chunk, create an internal specific image decoder,
  // otherwise just call DecodeChunk.
  if (decoder_) {
    decoder_->DecodeChunk(input_bytes, size);
    return;
  }

  const size_t index = signature_cache_.position;
  size_t data_offset = kLengthOfLongestSignature - index;
  size_t fill_size = std::min(data_offset, size);
  memcpy(signature_cache_.data + index, input_bytes, fill_size);
  signature_cache_.position += fill_size;
  if (size < data_offset) {
    // Data is not enough for matching signature.
    return;
  }

  if (s_use_stub_image_decoder) {
    decoder_ = make_scoped_ptr<ImageDataDecoder>(
        new StubImageDecoder(resource_provider_));
  } else if (JPEGImageDecoder::IsValidSignature(signature_cache_.data)) {
    decoder_ = make_scoped_ptr<ImageDataDecoder>(
        new JPEGImageDecoder(resource_provider_));
  } else if (PNGImageDecoder::IsValidSignature(signature_cache_.data)) {
    decoder_ = make_scoped_ptr<ImageDataDecoder>(
        new PNGImageDecoder(resource_provider_));
  } else if (WEBPImageDecoder::IsValidSignature(signature_cache_.data)) {
    decoder_ = make_scoped_ptr<ImageDataDecoder>(
        new WEBPImageDecoder(resource_provider_));
  } else {
    error_state_ = kUnsupportedImageFormat;
    return;
  }

  if (index == 0) {
    // This case means the first chunk is large enough for matching signature.
    decoder_->DecodeChunk(input_bytes, size);
  } else {
    decoder_->DecodeChunk(signature_cache_.data, kLengthOfLongestSignature);
    input_bytes += data_offset;
    decoder_->DecodeChunk(input_bytes, size - data_offset);
  }
}

void ImageDecoder::Finish() {
  switch (error_state_) {
    case kNoError:
      if (decoder_) {
        decoder_->Finish();
        scoped_ptr<render_tree::ImageData> image_data =
            decoder_->RetrieveImageData();
        if (image_data) {
          success_callback_.Run(
              resource_provider_->CreateImage(image_data.Pass()));
        } else {
          error_callback_.Run(decoder_->GetTypeString() +
                              " failed to decode image");
        }
      } else {
        error_callback_.Run("No ImageDataDecoder");
      }
      break;
    case kUnsupportedImageFormat:
      error_callback_.Run("Unsupported image format");
      break;
  }
}

// static
void ImageDecoder::UseStubImageDecoder() { s_use_stub_image_decoder = true; }

}  // namespace image
}  // namespace loader
}  // namespace cobalt
