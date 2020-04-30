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

#include "cobalt/renderer/rasterizer/skia/skottie_animation.h"

namespace cobalt {
namespace renderer {
namespace rasterizer {
namespace skia {

SkottieAnimation::SkottieAnimation(const char* data, size_t length) {
  skottie::Animation::Builder builder;
  skottie_animation_ = builder.make(data, length);
  animation_size_ = math::Size(skottie_animation_->size().width(),
                               skottie_animation_->size().height());
  json_size_in_bytes_ = builder.getStats().fJsonSize;
}

void SkottieAnimation::SetProperties(
    render_tree::LottieAnimation::LottieProperties properties) {
  properties_ = properties;
}

void SkottieAnimation::SetAnimationTime(base::TimeDelta animation_time) {
  double frame_time = animation_time.InSecondsF();
  if (properties_.loop) {
    frame_time =
        std::fmod(animation_time.InSecondsF(), skottie_animation_->duration());
  }
  skottie_animation_->seekFrameTime(frame_time);
}

}  // namespace skia
}  // namespace rasterizer
}  // namespace renderer
}  // namespace cobalt
