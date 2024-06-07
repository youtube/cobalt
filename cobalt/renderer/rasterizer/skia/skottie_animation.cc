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

#include "base/bind.h"
#include "third_party/skia/include/core/SkRect.h"

namespace cobalt {
namespace renderer {
namespace rasterizer {
namespace skia {

SkottieAnimation::SkottieAnimation(const char* data, size_t length)
    : last_updated_animation_time_(base::TimeDelta()) {
  ResetRenderCache();
  skottie::Animation::Builder builder;
  skottie_animation_ = builder.make(data, length);
  animation_size_ = math::Size(skottie_animation_->size().width(),
                               skottie_animation_->size().height());
  json_size_in_bytes_ = builder.getStats().fJsonSize;
}

void SkottieAnimation::SetAnimationTimeInternal(
    base::TimeDelta animate_function_time) {
  // Seeking to a particular frame takes precedence over normal playback.
  // Check whether "seek()" has been called but has yet to occur.
  if (seek_counter_ != properties_.seek_counter) {
    base::TimeDelta current_animation_time;
    if (properties_.seek_value_is_frame) {
      skottie_animation_->seekFrame(properties_.seek_value);
      current_animation_time = base::TimeDelta::FromSecondsD(
          properties_.seek_value / skottie_animation_->fps());
    } else {
      skottie_animation_->seekFrameTime(properties_.seek_value *
                                        skottie_animation_->duration());
      current_animation_time = base::TimeDelta::FromSecondsD(
          properties_.seek_value * skottie_animation_->duration());
    }
    seek_counter_ = properties_.seek_counter;
    is_complete_ = false;
    UpdateAnimationFrameAndAnimateFunctionTimes(current_animation_time,
                                                animate_function_time);
    return;
  }

  // Reset the animation time to the start of the animation if the animation is
  // stopped.
  if (properties_.state == LottieState::kStopped) {
    skottie_animation_->seekFrameTime(0);
    is_complete_ = false;
    UpdateAnimationFrameAndAnimateFunctionTimes(base::TimeDelta(),
                                                animate_function_time);
    return;
  }

  // Do not update the animation time is the animation is paused.
  if (properties_.state == LottieState::kPaused) {
    skottie_animation_->seekFrameTime(
        last_updated_animation_time_.InSecondsF());
    UpdateAnimationFrameAndAnimateFunctionTimes(last_updated_animation_time_,
                                                animate_function_time);
    return;
  }

  // Do not update the animation time if it has already reached the last frame.
  if (is_complete_) {
    return;
  }

  base::TimeDelta current_animation_time = last_updated_animation_time_;
  base::TimeDelta time_elapsed =
      animate_function_time - last_updated_animate_function_time_;
  current_animation_time += time_elapsed * properties_.speed;
  base::TimeDelta animation_duration =
      base::TimeDelta::FromSecondsD(skottie_animation_->duration());

  if (properties_.loop) {
    // If the animation mode is "bounce", then the animation should change
    // the direction in which it plays after each loop.
    int new_loop_count = floor(current_animation_time / animation_duration);

    // Check whether the number of loops exceeds the limits set by
    // LottieProperties::count.
    // (Note: LottieProperties::count refers to the number of loops after the
    // animation plays once through.)
    if (properties_.count > 0 && new_loop_count > properties_.count) {
      current_animation_time = animation_duration;
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
      current_animation_time = base::TimeDelta::FromSecondsD(
          std::fmod(current_animation_time.InSecondsF(),
                    animation_duration.InSecondsF()));
    }
    total_loops_ = new_loop_count;
  }

  if (direction_ * properties_.direction == -1) {
    current_animation_time = animation_duration - current_animation_time;
  }

  if (!is_complete_ && current_animation_time > animation_duration &&
      !properties_.oncomplete_callback.is_null()) {
    is_complete_ = true;
    properties_.oncomplete_callback.Run();
  }

  skottie_animation_->seekFrameTime(current_animation_time.InSecondsF());
  UpdateAnimationFrameAndAnimateFunctionTimes(current_animation_time,
                                              animate_function_time);
}

void SkottieAnimation::UpdateRenderCache(SkCanvas* render_target,
    const math::SizeF& size) {
  DCHECK(render_target);
  if (cached_animation_time_ == last_updated_animation_time_) {
    // The render cache is already up-to-date.
    return;
  }

  cached_animation_time_ = last_updated_animation_time_;
  SkRect bounding_rect = SkRect::MakeWH(size.width(), size.height());
  render_target->clear(SK_ColorTRANSPARENT);
  skottie_animation_->render(render_target, &bounding_rect);
  render_target->flush();
}

void SkottieAnimation::UpdateAnimationFrameAndAnimateFunctionTimes(
    base::TimeDelta current_animation_time,
    base::TimeDelta current_animate_function_time) {
  last_updated_animate_function_time_ = current_animate_function_time;

  if (current_animation_time == last_updated_animation_time_) {
    return;
  }

  // Dispatch a frame event every time a new frame is entered, and if the
  // animation is not complete.
  double frame =
      current_animation_time.InSecondsF() * skottie_animation_->fps();
  double seeker = current_animation_time.InSecondsF() /
                  skottie_animation_->duration() * 100;
  if (!properties_.onenterframe_callback.is_null() && !is_complete_) {
    properties_.onenterframe_callback.Run(frame, seeker);
  }

  last_updated_animation_time_ = current_animation_time;
}

}  // namespace skia
}  // namespace rasterizer
}  // namespace renderer
}  // namespace cobalt
