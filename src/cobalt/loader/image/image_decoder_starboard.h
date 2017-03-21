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

#ifndef COBALT_LOADER_IMAGE_IMAGE_DECODER_STARBOARD_H_
#define COBALT_LOADER_IMAGE_IMAGE_DECODER_STARBOARD_H_

#if defined(STARBOARD)

#include <string>
#include <vector>

#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "cobalt/loader/image/image_data_decoder.h"
#include "starboard/decode_target.h"

#if SB_VERSION(3) && SB_HAS(GRAPHICS)

namespace cobalt {
namespace loader {
namespace image {

class ImageDecoderStarboard : public ImageDataDecoder {
 public:
  explicit ImageDecoderStarboard(
      render_tree::ResourceProvider* resource_provider, const char* mime_type,
      SbDecodeTargetFormat format);
  ~ImageDecoderStarboard() OVERRIDE;

  // From ImageDataDecoder
  std::string GetTypeString() const OVERRIDE { return "ImageDecoderStarboard"; }

  bool IsHardwareDecoder() const OVERRIDE { return true; }

  SbDecodeTarget RetrieveSbDecodeTarget() OVERRIDE { return target_; }

 private:
  // From ImageDataDecoder
  size_t DecodeChunkInternal(const uint8* data, size_t size) OVERRIDE;
  void FinishInternal() OVERRIDE;

  const char* mime_type_;
  SbDecodeTargetFormat format_;
  std::vector<uint8> buffer_;
  SbDecodeTargetProvider* provider_;
  SbDecodeTarget target_;
};

}  // namespace image
}  // namespace loader
}  // namespace cobalt

#endif  // SB_VERSION(3) && SB_HAS(GRAPHICS)

#endif  // defined(STARBOARD)

#endif  // COBALT_LOADER_IMAGE_IMAGE_DECODER_STARBOARD_H_
