// Copyright 2015 Google Inc. All Rights Reserved.
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

#ifndef COBALT_WEB_ANIMATIONS_ANIMATION_EFFECT_READ_ONLY_H_
#define COBALT_WEB_ANIMATIONS_ANIMATION_EFFECT_READ_ONLY_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "cobalt/script/wrappable.h"
#include "cobalt/web_animations/animation_effect_timing_read_only.h"

namespace cobalt {
namespace web_animations {

// Animation effects are represented in the Web Animations API by the
// AnimationEffectReadOnly interface.
//   https://www.w3.org/TR/2015/WD-web-animations-1-20150707/#the-animationeffectreadonly-interface
// This class hosts the timing timing information for the given effect, and
// acts as a base class to its only (in the current spec) subclass:
// KeyframeEffectReadOnly, which contains keyframe information for an effect.
class AnimationEffectReadOnly : public script::Wrappable {
 public:
  AnimationEffectReadOnly(
      const scoped_refptr<AnimationEffectTimingReadOnly>& timing)
      : timing_(timing) {}

  const scoped_refptr<AnimationEffectTimingReadOnly>& timing() const {
    return timing_;
  }

  DEFINE_WRAPPABLE_TYPE(AnimationEffectReadOnly);

 protected:
  ~AnimationEffectReadOnly() override {}

 private:
  scoped_refptr<AnimationEffectTimingReadOnly> timing_;

  DISALLOW_COPY_AND_ASSIGN(AnimationEffectReadOnly);
};

}  // namespace web_animations
}  // namespace cobalt

#endif  // COBALT_WEB_ANIMATIONS_ANIMATION_EFFECT_READ_ONLY_H_
