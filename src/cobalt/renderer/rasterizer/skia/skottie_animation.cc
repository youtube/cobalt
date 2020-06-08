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
  // Seeking to a particular frame takes precedence over normal playback.
  // Check whether "seek()" has been called but has yet to occur.
  if (seek_counter_ != properties_.seek_counter) {
    if (properties_.seek_value_is_frame) {
      skottie_animation_->seekFrame(properties_.seek_value);
      current_animation_time_ = base::TimeDelta::FromSecondsD(
          properties_.seek_value * skottie_animation_->fps() *
          skottie_animation_->duration());
    } else {
      skottie_animation_->seekFrameTime(properties_.seek_value *
                                        skottie_animation_->duration());
      current_animation_time_ = base::TimeDelta::FromSecondsD(
          properties_.seek_value * skottie_animation_->duration());
    }
    last_updated_animate_function_time_ = animate_function_time;
    seek_counter_ = properties_.seek_counter;
    is_complete_ = false;
    return;
  }

  // Reset |current_animation_time| to the start of the animation if the
  // animation is stopped.
  if (properties_.state == LottieState::kStopped) {
    skottie_animation_->seekFrameTime(0);
    current_animation_time_ = base::TimeDelta();
    last_updated_animate_function_time_ = animate_function_time;
    is_complete_ = false;
    return;
  }

  // Update |current_animation_time_| if the animation is playing.
  if (properties_.state == LottieState::kPlaying) {
    base::TimeDelta time_elapsed =
        animate_function_time - last_updated_animate_function_time_;
    current_animation_time_ += time_elapsed * properties_.speed;
  }

  double current_frame_time = current_animation_time_.InSecondsF();
  if (properties_.loop) {
    // If the animation mode is "bounce", then the animation should change
    // the direction in which it plays after each loop.
    int new_loop_count =
        floor(current_frame_time / skottie_animation_->duration());

    // Check whether the number of loops exceeds the limits set by
    // LottieProperties::count.
    // (Note: LottieProperties::count refers to the number of loops after the
    // animation plays once through.)
    if (properties_.count > 0 && new_loop_count > properties_.count) {
      current_frame_time = skottie_animation_->duration();
    } else {
      // If the animation should continue playing, check whether the animation
      // has completed and needs to trigger a "loop" and potentially reverse
      // direction.
      if (new_loop_count > total_loops_) {
        if (!properties_.onloop_callback.is_null()) {
          properties_.onloop_callback.Run();
        }
        if (properties_.mode == LottieMode::kBounce) {
          direction_ *= -1;
        }
      }
      current_frame_time =
          std::fmod(current_frame_time, skottie_animation_->duration());
    }
    total_loops_ = new_loop_count;
  }
  if (direction_ * properties_.direction == -1) {
    current_frame_time = skottie_animation_->duration() - current_frame_time;
  }
  if (!is_complete_ && current_frame_time > skottie_animation_->duration() &&
      !properties_.oncomplete_callback.is_null()) {
    is_complete_ = true;
    properties_.oncomplete_callback.Run();
  }
  skottie_animation_->seekFrameTime(current_frame_time);
  last_updated_animate_function_time_ = animate_function_time;
}

}  // namespace skia
}  // namespace rasterizer
}  // namespace renderer
}  // namespace cobalt
