// Copyright 2015 Google Inc. All Rights Reserved.
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

#ifndef COBALT_LOADER_IMAGE_IMAGE_DECODER_H_
#define COBALT_LOADER_IMAGE_IMAGE_DECODER_H_

#include <string>

#include "base/callback.h"
#include "cobalt/loader/decoder.h"
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

  // Ensure that the image data decoder is initialized so that
  // IsHardwareDecoder() can be queried to call DecodeChunk() from an
  // appropriate thread. Returns whether DecodeChunk() is ready to be called.
  //
  // Optionally, DecodeChunk() will initialize the image data decoder as
  // needed, but care must be taken to call the function from one consistent
  // thread when using this approach.
  bool EnsureDecoderIsInitialized(const char* data, size_t size);

  // Returns whether the current image data decoder is a software or hardware
  // decoder. This is intended to help decide which thread should be used to
  // call DecodeChunk().
  bool IsHardwareDecoder() const;

  // From Decoder.
  LoadResponseType OnResponseStarted(
      Fetcher* fetcher,
      const scoped_refptr<net::HttpResponseHeaders>& headers) OVERRIDE;
  void DecodeChunk(const char* data, size_t size) OVERRIDE;
  void Finish() OVERRIDE;
  bool Suspend() OVERRIDE;
  void Resume(render_tree::ResourceProvider* resource_provider) OVERRIDE;

  // Call this function to use the StubImageDecoder which produces a small image
  // without decoding.
  static void UseStubImageDecoder();

 private:
  enum State {
    kWaitingForHeader,
    kDecoding,
    kNotApplicable,
    kUnsupportedImageFormat,
    kNoResourceProvider,
    kSuspended,
  };

  // The current longest signature is WEBP signature.
  static const size_t kLengthOfLongestSignature = 14;

  struct SignatureCache {
    uint8 data[kLengthOfLongestSignature];
    size_t position;
  };

  void DecodeChunkInternal(const uint8* input_bytes, size_t size);
  bool InitializeInternalDecoder(const uint8* input_bytes, size_t size,
                                 size_t* consumed_size);

  render_tree::ResourceProvider* resource_provider_;
  const SuccessCallback success_callback_;
  const ErrorCallback error_callback_;
  scoped_ptr<ImageDataDecoder> decoder_;
  SignatureCache signature_cache_;
  State state_;
  size_t data_consumed_size;
  std::string error_message_;
  std::string mime_type_;
};

}  // namespace image
}  // namespace loader
}  // namespace cobalt

#endif  // COBALT_LOADER_IMAGE_IMAGE_DECODER_H_
