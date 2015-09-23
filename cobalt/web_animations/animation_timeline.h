/*
 * Copyright 2015 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef WEB_ANIMATIONS_ANIMATION_TIMELINE_H_
#define WEB_ANIMATIONS_ANIMATION_TIMELINE_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/optional.h"
#include "cobalt/script/wrappable.h"

namespace cobalt {
namespace web_animations {

class Animation;
class AnimationSet;

// Implements the AnimationTimeline IDL interface.
//   http://www.w3.org/TR/2015/WD-web-animations-1-20150707/#the-animationtimeline-interface
class AnimationTimeline : public script::Wrappable {
 public:
  AnimationTimeline();

  // Returns the current sample time of the timeline, in milliseconds.  If the
  // returned optional is not engaged, this timeline is 'unresolved'.
  virtual base::optional<double> current_time() const = 0;

  // Custom, not in any spec.

  // Returns the number of animations registered to this timeline.  This is not
  // in the spec, but serves as a poor-man's replacement for getAnimations()
  // until we have support for that.
  unsigned int num_animations() const;

  // Registers and deregisters an animation with this timeline so that we are
  // able to track all animations currently associated with this timeline.
  // This will be requiered in order to implement getAnimations().
  void Register(Animation* animation);
  void Deregister(Animation* animation);

  DEFINE_WRAPPABLE_TYPE(AnimationTimeline);

 protected:
  ~AnimationTimeline() OVERRIDE;

 private:
  scoped_refptr<AnimationSet> animations_;

  DISALLOW_COPY_AND_ASSIGN(AnimationTimeline);
};

}  // namespace web_animations
}  // namespace cobalt

#endif  // WEB_ANIMATIONS_ANIMATION_TIMELINE_H_
