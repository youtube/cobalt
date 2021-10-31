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

#ifndef COBALT_LOADER_IMAGE_STUB_IMAGE_DECODER_H_
#define COBALT_LOADER_IMAGE_STUB_IMAGE_DECODER_H_

#include <memory>
#include <string>

#include "base/compiler_specific.h"
#include "cobalt/loader/image/image.h"
#include "cobalt/loader/image/image_data_decoder.h"

namespace cobalt {
namespace loader {
namespace image {

// This class always returns a small image without decoding.  It can be used
// during tests or benchmarks to minimize the impact of image decoding.
class StubImageDecoder : public ImageDataDecoder {
 public:
  explicit StubImageDecoder(render_tree::ResourceProvider* resource_provider,
                            const base::DebuggerHooks& debugger_hooks)
      : ImageDataDecoder(resource_provider, debugger_hooks) {}

  // From ImageDataDecoder
  std::string GetTypeString() const override { return "StubImageDecoder"; }

  static bool IsValidSignature(const uint8* header) {
    return true;
  }

 private:
  // From ImageDataDecoder
  size_t DecodeChunkInternal(const uint8* data, size_t input_byte) override {
    if (!decoded_image_data_) {
      decoded_image_data_ = AllocateImageData(math::Size(4, 4), true);
      DCHECK(decoded_image_data_);
    }
    set_state(kDone);
    return input_byte;
  }
  scoped_refptr<Image> FinishInternal() override {
    return CreateStaticImage(std::move(decoded_image_data_));
  }

  std::unique_ptr<render_tree::ImageData> decoded_image_data_;
};

}  // namespace image
}  // namespace loader
}  // namespace cobalt

#endif  // COBALT_LOADER_IMAGE_STUB_IMAGE_DECODER_H_
