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

#ifndef COBALT_LOADER_IMAGE_WEBP_IMAGE_DECODER_H_
#define COBALT_LOADER_IMAGE_WEBP_IMAGE_DECODER_H_

#include <string>

#include "cobalt/loader/image/image_data_decoder.h"
#include "third_party/libwebp/webp/decode.h"

namespace cobalt {
namespace loader {
namespace image {

class WEBPImageDecoder : public ImageDataDecoder {
 public:
  explicit WEBPImageDecoder(render_tree::ResourceProvider* resource_provider);
  ~WEBPImageDecoder() OVERRIDE;

  // From ImageDataDecoder
  std::string GetTypeString() const OVERRIDE { return "WEBPImageDecoder"; }

  // Returns true if the signature is valid for the particular image type.
  static bool IsValidSignature(const uint8* header) {
    return !memcmp(header, "RIFF", 4) && !memcmp(header + 8, "WEBPVP", 6);
  }

 private:
  // From ImageDataDecoder
  size_t DecodeChunkInternal(const uint8* data, size_t input_byte) OVERRIDE;

  bool ReadHeader(const uint8* data, size_t size, bool* has_alpha);
  bool CreateInternalDecoder(bool has_alpha);
  void DeleteInternalDecoder();

  WebPIDecoder* internal_decoder_;
  WebPDecoderConfig config_;
};

}  // namespace image
}  // namespace loader
}  // namespace cobalt

#endif  // COBALT_LOADER_IMAGE_WEBP_IMAGE_DECODER_H_
