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

#ifndef COBALT_RENDERER_RASTERIZER_SKIA_SKOTTIE_ANIMATION_H_
#define COBALT_RENDERER_RASTERIZER_SKIA_SKOTTIE_ANIMATION_H_

#include "cobalt/math/size_f.h"
#include "cobalt/render_tree/lottie_animation.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/modules/skottie/include/Skottie.h"

namespace cobalt {
namespace renderer {
namespace rasterizer {
namespace skia {

// A subclass of render_tree::LottieAnimation that holds information about
// the Skottie animation object associated with the given animation data.
class SkottieAnimation : public render_tree::LottieAnimation {
 public:
  SkottieAnimation(const char* data, size_t length);

  const math::Size& GetSize() const override { return animation_size_; }

  uint32 GetEstimatedSizeInBytes() const override {
    return json_size_in_bytes_;
  }

  bool IsOpaque() const override { return false; }

  void SetAnimationTimeInternal(base::TimeDelta animate_function_time) override;

  sk_sp<skottie::Animation> GetSkottieAnimation() { return skottie_animation_; }

  // Rendering the lottie animation can be CPU-intensive. To minimize the cost,
  // the animation can be updated in an offscreen render target only as needed,
  // then the offscreen target rendered to the screen.
  void ResetRenderCache() { cached_animation_time_ = base::TimeDelta::Min(); }
  void UpdateRenderCache(SkCanvas* render_target, const math::SizeF& size);

 private:
  void UpdateAnimationFrameAndAnimateFunctionTimes(
      base::TimeDelta current_animation_time,
      base::TimeDelta current_animate_function_time);

  sk_sp<skottie::Animation> skottie_animation_;
  math::Size animation_size_;
  uint32 json_size_in_bytes_;

  // |seek_counter_| is used to indicate whether a particular seek has already
  // been processed. When |LottieProperties::seek_counter| is different, then
  // the requested seek should be performed and |seek_counter_| updated to match
  // LottieProperties.
  size_t seek_counter_ = 0;

  // |total_loops_| keeps track of how many times the animation has played
  // in its entirety.
  int total_loops_ = 0;
  // |direction_| is used to indicate whether the animation should be playing
  // in the the direction set by |LottieProperties::direction|, or whether it
  // should be playing in the opposite direction (ex: when mode = "bounce", the
  // animation needs to switch direction in between each loop).
  int direction_ = 1;
  // |is_complete_| indicates whether the animation has finished playback. It
  // should be set to true only after the animation frame time exceeds the
  // animation duration due to normal playback, and after the "complete" event
  // is triggered. It should be reset to false whenever the animation frame time
  // is updated by a change outside of normal playback, such as seeking or
  // stopping the animation.
  bool is_complete_ = false;

  // The last timestamp from the animation function in which we updated the
  // the frame time for |skottie_animation_|. Used for calculating the time
  // elapsed and updating the frame time again.
  base::TimeDelta last_updated_animate_function_time_;

  // The most recently updated frame time for |skottie_animation_|.
  base::TimeDelta last_updated_animation_time_;

  // This is the animation time used for the last cache update.
  base::TimeDelta cached_animation_time_;
};

}  // namespace skia
}  // namespace rasterizer
}  // namespace renderer
}  // namespace cobalt

#endif  // COBALT_RENDERER_RASTERIZER_SKIA_SKOTTIE_ANIMATION_H_
