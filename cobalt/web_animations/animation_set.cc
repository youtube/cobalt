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

#include "cobalt/web_animations/animation_set.h"

#include "cobalt/web_animations/keyframe_effect_read_only.h"

namespace cobalt {
namespace web_animations {

AnimationSet::AnimationSet() {}

void AnimationSet::AddAnimation(Animation* animation) {
  animations_.insert(animation);
  animation->OnAddedToAnimationSet(this);
}

void AnimationSet::RemoveAnimation(Animation* animation) {
  animations_.erase(animation);
  animation->OnRemovedFromAnimationSet(this);
}

bool AnimationSet::IsPropertyAnimated(cssom::PropertyKey property_name) const {
  for (InternalSet::const_iterator iter = animations_.begin();
       iter != animations_.end(); ++iter) {
    const KeyframeEffectReadOnly* keyframe_effect =
        base::polymorphic_downcast<const KeyframeEffectReadOnly*>(
            (*iter)->effect().get());
    if (keyframe_effect->data().IsPropertyAnimated(property_name)) {
      return true;
    }
  }

  return false;
}

}  // namespace web_animations
}  // namespace cobalt
