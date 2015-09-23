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

#include "cobalt/web_animations/animation.h"
#include "cobalt/web_animations/animation_set.h"
#include "cobalt/web_animations/animation_timeline.h"

namespace cobalt {
namespace web_animations {

AnimationTimeline::AnimationTimeline() : animations_(new AnimationSet()) {}

AnimationTimeline::~AnimationTimeline() { DCHECK(animations_->IsEmpty()); }

void AnimationTimeline::Register(Animation* animation) {
  animations_->AddAnimation(animation);
}

void AnimationTimeline::Deregister(Animation* animation) {
  animations_->RemoveAnimation(animation);
}

unsigned int AnimationTimeline::num_animations() const {
  return static_cast<unsigned int>(animations_->GetSize());
}

}  // namespace web_animations
}  // namespace cobalt
