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

#ifndef COBALT_LOADER_IMAGE_STUB_IMAGE_DECODER_H_
#define COBALT_LOADER_IMAGE_STUB_IMAGE_DECODER_H_

#include <string>

#include "base/compiler_specific.h"
#include "cobalt/loader/image/image_data_decoder.h"

namespace cobalt {
namespace loader {
namespace image {

// This class always returns a small image without decoding.  It can be used
// during tests or benchmarks to minimize the impact of image decoding.
class StubImageDecoder : public ImageDataDecoder {
 public:
  explicit StubImageDecoder(render_tree::ResourceProvider* resource_provider)
      : ImageDataDecoder(resource_provider) {}

  // From ImageDataDecoder
  std::string GetTypeString() const override { return "StubImageDecoder"; }

  static bool IsValidSignature(const uint8* header) {
    UNREFERENCED_PARAMETER(header);
    return true;
  }

 private:
  // From ImageDataDecoder
  size_t DecodeChunkInternal(const uint8* data, size_t input_byte) override {
    UNREFERENCED_PARAMETER(data);
    UNREFERENCED_PARAMETER(input_byte);
    if (!image_data()) {
      bool results = AllocateImageData(math::Size(4, 4), true);
      DCHECK(results);
    }
    set_state(kDone);
    return input_byte;
  }
};

}  // namespace image
}  // namespace loader
}  // namespace cobalt

#endif  // COBALT_LOADER_IMAGE_STUB_IMAGE_DECODER_H_
