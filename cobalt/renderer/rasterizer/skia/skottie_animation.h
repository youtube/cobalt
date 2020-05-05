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

#include "cobalt/render_tree/lottie_animation.h"
#include "third_party/skia/modules/skottie/include/Skottie.h"

namespace cobalt {
namespace renderer {
namespace rasterizer {
namespace skia {

// A subclass of render_tree::LottieAnnimation that holds information about
// the Skottie animation object associated with the given animation data.
class SkottieAnimation : public render_tree::LottieAnimation {
 public:
  SkottieAnimation(const char* data, size_t length);

  const math::Size& GetSize() const override { return animation_size_; }

  uint32 GetEstimatedSizeInBytes() const override {
    return json_size_in_bytes_;
  }

  bool IsOpaque() const override { return false; }

  void SetProperties(
      render_tree::LottieAnimation::LottieProperties properties) override;

  void SetAnimationTime(base::TimeDelta animate_function_time) override;

  sk_sp<skottie::Animation> GetSkottieAnimation() { return skottie_animation_; }

  bool stopped() const {
    return properties_.current_state ==
           render_tree::LottieAnimation::AnimationState::kStopped;
  }

 private:
  sk_sp<skottie::Animation> skottie_animation_;
  math::Size animation_size_;
  uint32 json_size_in_bytes_;

  render_tree::LottieAnimation::LottieProperties properties_;

  // The last timestamp from the animation function in which we updated the
  // the frame time for |skottie_animation_|. Used for calculating the time
  // elapsed and updating the frame time again.
  base::TimeDelta last_updated_animate_function_time_;

  // The most recently updated frame time for |skottie_animation_|.
  base::TimeDelta current_animation_time_;
};

}  // namespace skia
}  // namespace rasterizer
}  // namespace renderer
}  // namespace cobalt

#endif  // COBALT_RENDERER_RASTERIZER_SKIA_SKOTTIE_ANIMATION_H_
