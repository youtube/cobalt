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

#include "cobalt/loader/image/lottie_animation.h"

#include "base/trace_event/trace_event.h"
#include "nb/memory_scope.h"

namespace cobalt {
namespace loader {
namespace image {

LottieAnimation::LottieAnimation(
    render_tree::ResourceProvider* resource_provider)
    : resource_provider_(resource_provider) {
  TRACE_EVENT0("cobalt::loader::image", "LottieAnimation::LottieAnimation()");
}

void LottieAnimation::AppendChunk(const uint8* data, size_t size) {
  TRACE_EVENT0("cobalt::loader::image", "LottieAnimation::AppendChunk()");
  data_buffer_.insert(data_buffer_.end(), data, data + size);
}

void LottieAnimation::FinishReadingData() {
  TRACE_EVENT0("cobalt::loader::image", "LottieAnimation::FinishReadingData()");
  animation_ = resource_provider_->CreateLottieAnimation(
      reinterpret_cast<char*>(data_buffer_.data()), data_buffer_.size());
}

}  // namespace image
}  // namespace loader
}  // namespace cobalt
