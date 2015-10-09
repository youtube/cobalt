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

#ifndef LOADER_IMAGE_IMAGE_DECODER_H_
#define LOADER_IMAGE_IMAGE_DECODER_H_

#include "base/callback.h"
#include "cobalt/loader/image/image_data_decoder.h"
#include "cobalt/render_tree/image.h"
#include "cobalt/render_tree/resource_provider.h"

namespace cobalt {
namespace loader {
namespace image {

// This class handles decoding any image type. When the image type is determined
// by the signature in the data being received, an image data decoder specific
// to that image type is created to parse the data. When the decoding is
// complete, the image decoder retrieves the data and uses it to create the
// image.
class ImageDecoder : public Decoder {
 public:
  typedef base::Callback<void(const scoped_refptr<render_tree::Image>&)>
      SuccessCallback;
  typedef base::Callback<void(const std::string&)> ErrorCallback;

  ImageDecoder(render_tree::ResourceProvider* resource_provider,
               const SuccessCallback& success_callback,
               const ErrorCallback& error_callback);

  // From Decoder.
  void DecodeChunk(const char* data, size_t size) OVERRIDE;
  void Finish() OVERRIDE;

 private:
  // True if an error has occurred and we should not continue decoding.
  enum Error {
    kNoError,
    kUnsupportedImageFormat,
  };

  // The current longest signature is PNG signature.
  static const int kLengthOfLongestSignature = 8;

  struct SignatureCache {
    uint8 data[kLengthOfLongestSignature];
    int position;
  };

  render_tree::ResourceProvider* const resource_provider_;
  const SuccessCallback success_callback_;
  const ErrorCallback error_callback_;
  scoped_ptr<ImageDataDecoder> decoder_;
  SignatureCache signature_cache_;
  Error error_state_;
};

}  // namespace image
}  // namespace loader
}  // namespace cobalt

#endif  // LOADER_IMAGE_IMAGE_DECODER_H_
