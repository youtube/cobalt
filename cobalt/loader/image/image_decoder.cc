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

#include "cobalt/loader/image/jpeg_image_decoder.h"
#include "cobalt/loader/image/png_image_decoder.h"

namespace cobalt {
namespace loader {
namespace image {

namespace {

bool MatchesPNGSignature(const uint8* contents) {
  return !memcmp(contents, "\x89\x50\x4E\x47\x0D\x0A\x1A\x0A", 8);
}

bool MatchesJPEGSignature(const uint8* contents) {
  return !memcmp(contents, "\xFF\xD8\xFF", 3);
}

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

  // Call different types of decoders by matching the image signature. If it is
  // the first time decoding a chunk, create an internal specific image decoder,
  // otherwise just call DecodeChunk.
  if (decoder_) {
    decoder_->DecodeChunk(data, size);
    return;
  }
  // If the first chunk is less than |kLengthOfLongestSignature|, the next
  // chunk could have more signature information.
  const int index = signature_cache_.position;
  int data_offset = kLengthOfLongestSignature - index;
  if (index + size < kLengthOfLongestSignature) {
    memcpy(signature_cache_.data + index, data, size);
    signature_cache_.position += static_cast<int>(size);
    return;
  } else {
    memcpy(signature_cache_.data + index, data,
           static_cast<size_t>(data_offset));
    signature_cache_.position += data_offset;
  }

  if (MatchesJPEGSignature(signature_cache_.data)) {
    decoder_ = make_scoped_ptr<ImageDataDecoder>(
        new JPEGImageDecoder(resource_provider_));
  } else if (MatchesPNGSignature(signature_cache_.data)) {
    decoder_ = make_scoped_ptr<ImageDataDecoder>(
        new PNGImageDecoder(resource_provider_));
  } else {
    error_state_ = kUnsupportedImageFormat;
    return;
  }

  if (index == 0) {
    // This case means the first chunk is large enough for matching signature.
    decoder_->DecodeChunk(data, size);
  } else {
    decoder_->DecodeChunk(reinterpret_cast<char*>(signature_cache_.data),
                          kLengthOfLongestSignature);
    data += data_offset;
    decoder_->DecodeChunk(data, size - data_offset);
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

}  // namespace image
}  // namespace loader
}  // namespace cobalt
