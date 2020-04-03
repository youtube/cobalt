// Copyright 2020 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_LOADER_IMAGE_LOTTIE_ANIMATION_H_
#define COBALT_LOADER_IMAGE_LOTTIE_ANIMATION_H_

#include <vector>

#include "base/cancelable_callback.h"
#include "base/threading/thread.h"
#include "base/trace_event/trace_event.h"
#include "cobalt/loader/image/image.h"
#include "cobalt/render_tree/resource_provider.h"

namespace cobalt {
namespace loader {
namespace image {

class LottieAnimation : public Image {
 public:
  explicit LottieAnimation(render_tree::ResourceProvider* resource_provider);

  const math::Size& GetSize() const override { return animation_->GetSize(); }

  uint32 GetEstimatedSizeInBytes() const override {
    return animation_->GetEstimatedSizeInBytes();
  }

  bool IsAnimated() const override {
    // Even though this class represents an animation, IsAnimated() should
    // return false because there is only one render_tree::Image object ever
    // associated with an instance of this class.
    return false;
  }

  bool IsOpaque() const override { return animation_->IsOpaque(); }

  void AppendChunk(const uint8* data, size_t input_byte);

  void FinishReadingData();

  scoped_refptr<render_tree::Image> animation() { return animation_; }

 private:
  render_tree::ResourceProvider* resource_provider_;
  std::vector<uint8> data_buffer_;
  scoped_refptr<render_tree::Image> animation_;
};

}  // namespace image
}  // namespace loader
}  // namespace cobalt

#endif  // COBALT_LOADER_IMAGE_LOTTIE_ANIMATION_H_
