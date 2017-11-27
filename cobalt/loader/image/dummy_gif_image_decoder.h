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

#ifndef COBALT_LOADER_IMAGE_DUMMY_GIF_IMAGE_DECODER_H_
#define COBALT_LOADER_IMAGE_DUMMY_GIF_IMAGE_DECODER_H_

#include <string>

#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "cobalt/loader/image/image_data_decoder.h"

namespace cobalt {
namespace loader {
namespace image {

// This DummyGIFImageDecoder is intended for getting around with a specific
// ads gif.
class DummyGIFImageDecoder : public ImageDataDecoder {
 public:
  explicit DummyGIFImageDecoder(
      render_tree::ResourceProvider* resource_provider);
  ~DummyGIFImageDecoder() override {}

  // From ImageDataDecoder
  std::string GetTypeString() const override { return "DummyGIFImageDecoder"; }

 private:
  // From ImageDataDecoder
  size_t DecodeChunkInternal(const uint8* data, size_t input_byte) override;
};

}  // namespace image
}  // namespace loader
}  // namespace cobalt

#endif  // COBALT_LOADER_IMAGE_DUMMY_GIF_IMAGE_DECODER_H_
