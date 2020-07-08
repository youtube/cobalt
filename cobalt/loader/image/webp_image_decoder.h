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

#ifndef COBALT_LOADER_IMAGE_WEBP_IMAGE_DECODER_H_
#define COBALT_LOADER_IMAGE_WEBP_IMAGE_DECODER_H_

#include <memory>
#include <string>
#include <vector>

#include "cobalt/loader/image/animated_webp_image.h"
#include "cobalt/loader/image/image.h"
#include "cobalt/loader/image/image_data_decoder.h"
#include "third_party/libwebp/src/webp/decode.h"

namespace cobalt {
namespace loader {
namespace image {

class WEBPImageDecoder : public ImageDataDecoder {
 public:
  explicit WEBPImageDecoder(render_tree::ResourceProvider* resource_provider,
                            const base::DebuggerHooks& debugger_hooks);
  ~WEBPImageDecoder() override;

  // From ImageDataDecoder
  std::string GetTypeString() const override { return "WEBPImageDecoder"; }

 private:
  // From ImageDataDecoder
  size_t DecodeChunkInternal(const uint8* data, size_t input_byte) override;
  scoped_refptr<Image> FinishInternal() override;

  bool ReadHeader(const uint8* data, size_t size);
  bool CreateInternalDecoder();
  void DeleteInternalDecoder();

  WebPIDecoder* internal_decoder_;
  WebPDecoderConfig config_;
  scoped_refptr<AnimatedWebPImage> animated_webp_image_;
  std::unique_ptr<render_tree::ImageData> decoded_image_data_;

  std::string cached_uncompressed_data_;
};

}  // namespace image
}  // namespace loader
}  // namespace cobalt

#endif  // COBALT_LOADER_IMAGE_WEBP_IMAGE_DECODER_H_
