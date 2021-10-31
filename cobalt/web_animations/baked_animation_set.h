// Copyright 2015 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_WEB_ANIMATIONS_BAKED_ANIMATION_SET_H_
#define COBALT_WEB_ANIMATIONS_BAKED_ANIMATION_SET_H_

#include <memory>
#include <vector>

#include "base/memory/ref_counted.h"
#include "base/time/time.h"
#include "cobalt/cssom/css_declared_style_data.h"
#include "cobalt/web_animations/animation.h"
#include "cobalt/web_animations/animation_effect_timing_read_only.h"
#include "cobalt/web_animations/animation_set.h"
#include "cobalt/web_animations/keyframe_effect_read_only.h"

namespace cobalt {
namespace web_animations {

// A BakedAnimation is an immutable object that can be used to animate
// CSS style over time.  The BakedAnimation object is completely independent
// of references to other objects and can be easily copied and passed around.
// It is constructed from a web_animations::Animation object, and upon
// construction will traverse other web_animations objects referenced by
// the Animation object, extracting all internal Data objects necessary to
// apply the animation to CSS styles.
// Its primary use-case is to snapshot the state of Web Animations at layout
// time so that animation data can be packaged and shipped off to the renderer
// thread with no dependencies on the Animation script objects.
class BakedAnimation {
 public:
  explicit BakedAnimation(const Animation& animation);

  // Apply an animation at the given timeline time, using the provided
  // style as the underlying input data, and updating it to the animated
  // output data.
  void Apply(const base::TimeDelta& timeline_time,
             cssom::MutableCSSComputedStyleData* in_out_style) const;

  // Returns the timeline time at which this animation will end.  If the
  // animation has no ending, base::TimeDelta::Max() will be returned.
  base::TimeDelta end_time() const;

 private:
  const Animation::Data animation_data_;
  const AnimationEffectTimingReadOnly::Data effect_timing_data_;
  const KeyframeEffectReadOnly::Data keyframe_data_;
};

// Maintains an immutable set of BakedAnimations, which should contain all
// data necessary to apply all animations to a targeted CSS style.
class BakedAnimationSet {
 public:
  explicit BakedAnimationSet(const AnimationSet& animation_set);
  explicit BakedAnimationSet(const BakedAnimationSet& animation_set);

  // Apply all animations in the set to the specified input style, updating
  // it to the output animated style.
  void Apply(const base::TimeDelta& timeline_time,
             cssom::MutableCSSComputedStyleData* in_out_style) const;

  // Returns the timeline time at which point all animations in the set are
  // ended, or base::TimeDelta::Max() if at leats one animation will never end.
  base::TimeDelta end_time() const;

 private:
  typedef std::vector<std::unique_ptr<BakedAnimation>> AnimationList;
  AnimationList animations_;
};

}  // namespace web_animations
}  // namespace cobalt

#endif  // COBALT_WEB_ANIMATIONS_BAKED_ANIMATION_SET_H_
