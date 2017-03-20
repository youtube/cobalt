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

#include "cobalt/loader/image/dummy_gif_image_decoder.h"

namespace cobalt {
namespace loader {
namespace image {

const char kDummyAdsGif[] = {
    '\x47', '\x49', '\x46', '\x38', '\x39', '\x61', '\x01', '\x00', '\x01',
    '\x00', '\x80', '\x00', '\x00', '\x00', '\x00', '\x00', '\xFF', '\xFF',
    '\xFF', '\x21', '\xF9', '\x04', '\x01', '\x00', '\x00', '\x00', '\x00',
    '\x2C', '\x00', '\x00', '\x00', '\x00', '\x01', '\x00', '\x01', '\x00',
    '\x00', '\x02', '\x01', '\x44', '\x00', '\x3B'};

DummyGIFImageDecoder::DummyGIFImageDecoder(
    render_tree::ResourceProvider* resource_provider)
    : ImageDataDecoder(resource_provider) {}

size_t DummyGIFImageDecoder::DecodeChunkInternal(const uint8* data,
                                                 size_t input_byte) {
  if (input_byte < sizeof(kDummyAdsGif)) {
    return 0;
  }

  if (state() != kWaitingForHeader ||
      memcmp(data, kDummyAdsGif, sizeof(kDummyAdsGif)) != 0) {
    set_state(kError);
    return input_byte;
  }

  set_state(kDone);
  return input_byte;
}

}  // namespace image
}  // namespace loader
}  // namespace cobalt
