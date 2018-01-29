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

#include "base/atomicops.h"
#include "base/callback.h"
#include "cobalt/loader/decoder.h"
#include "cobalt/loader/image/image.h"
#include "cobalt/loader/image/image_data_decoder.h"
#include "cobalt/render_tree/resource_provider.h"

namespace cobalt {
namespace loader {
namespace image {

// This class handles decoding any image. If image type is not given, it is
// determined by the signature in the data. Then an image data decoder specific
// to that image type is created to parse the data. When the decoding is
// complete, the image decoder retrieves the data and uses it to create the
// image.
class ImageDecoder : public Decoder {
 public:
  // The various types of images we support decoding.
  enum ImageType {
    kImageTypeInvalid,
    kImageTypeGIF,
    kImageTypeJPEG,
    kImageTypePNG,
    kImageTypeWebP,
  };

  typedef base::Callback<void(const scoped_refptr<Image>&)> SuccessCallback;
  typedef base::Callback<void(const std::string&)> ErrorCallback;

  ImageDecoder(render_tree::ResourceProvider* resource_provider,
               const SuccessCallback& success_callback,
               const ErrorCallback& error_callback);
  ImageDecoder(render_tree::ResourceProvider* resource_provider,
               const SuccessCallback& success_callback,
               const ErrorCallback& error_callback, ImageType image_type);

  // From Decoder.
  LoadResponseType OnResponseStarted(
      Fetcher* fetcher,
      const scoped_refptr<net::HttpResponseHeaders>& headers) override;
  void DecodeChunk(const char* data, size_t size) override;
  void Finish() override;
  bool Suspend() override;
  void Resume(render_tree::ResourceProvider* resource_provider) override;

  // Called when this ImageDecoder's deletion has been posted to a message loop.
  // This prevents any additional decoding from occuring prior to the decoder
  // being deleted.
  void SetDeletionPending();

  // Call this function to use the StubImageDecoder which produces a small image
  // without decoding.
  static void UseStubImageDecoder();

 private:
  enum State {
    kWaitingForHeader,
    kDecoding,
    kNotApplicable,
    kUnsupportedImageFormat,
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
  ImageType image_type_;
  scoped_ptr<ImageDataDecoder> decoder_;
  SignatureCache signature_cache_;
  State state_;
  std::string error_message_;
  std::string mime_type_;

  // Whether or not there is a pending task deleting this decoder on a message
  // loop.
  base::subtle::Atomic32 is_deletion_pending_;
};

}  // namespace image
}  // namespace loader
}  // namespace cobalt

#endif  // COBALT_LOADER_IMAGE_IMAGE_DECODER_H_
