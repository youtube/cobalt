// Copyright 2021 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_LOADER_IMAGE_FAILURE_IMAGE_DECODER_H_
#define COBALT_LOADER_IMAGE_FAILURE_IMAGE_DECODER_H_

#include <memory>
#include <string>

#include "base/compiler_specific.h"
#include "cobalt/loader/image/image.h"
#include "cobalt/loader/image/image_data_decoder.h"

namespace cobalt {
namespace loader {
namespace image {

// This class always signals a decoding error rather than decoding.  It can
// be used for imitating the normal image decoders without doing any
// decoding works.
class FailureImageDecoder : public ImageDataDecoder {
 public:
  explicit FailureImageDecoder(render_tree::ResourceProvider* resource_provider,
                               const base::DebuggerHooks& debugger_hooks)
      : ImageDataDecoder(resource_provider, debugger_hooks) {}

  // From ImageDataDecoder
  std::string GetTypeString() const override { return "FailureImageDecoder"; }

 private:
  // From ImageDataDecoder
  size_t DecodeChunkInternal(const uint8* data, size_t input_byte) override {
    set_state(kError);
    return input_byte;
  }
  scoped_refptr<Image> FinishInternal() override {
    // Indicate the failure of decoding works.
    return nullptr;
  }
};

}  // namespace image
}  // namespace loader
}  // namespace cobalt

#endif  // COBALT_LOADER_IMAGE_FAILURE_IMAGE_DECODER_H_
