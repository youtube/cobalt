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

void SkottieAnimation::SetProperties(LottieProperties properties) {
  properties_ = properties;
}

void SkottieAnimation::SetAnimationTime(base::TimeDelta animate_function_time) {
  if (properties_.state == LottieState::kPlaying) {
    // Only update |current_animation_time_| if the animation is playing.
    base::TimeDelta time_elapsed =
        animate_function_time - last_updated_animate_function_time_;
    current_animation_time_ += time_elapsed * properties_.speed;
  } else if (properties_.state == LottieState::kStopped) {
    // Reset to the start of the animation if it has been stopped.
    current_animation_time_ = base::TimeDelta();
  }

  double current_frame_time = current_animation_time_.InSecondsF();
  if (properties_.loop) {
    current_frame_time = std::fmod(current_animation_time_.InSecondsF(),
                                   skottie_animation_->duration());
  }
  if (properties_.direction == -1) {
    current_frame_time = skottie_animation_->duration() - current_frame_time;
  }
  skottie_animation_->seekFrameTime(current_frame_time);
  last_updated_animate_function_time_ = animate_function_time;
}

}  // namespace skia
}  // namespace rasterizer
}  // namespace renderer
}  // namespace cobalt
