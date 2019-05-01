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

#ifndef COBALT_WEB_ANIMATIONS_ANIMATION_SET_H_
#define COBALT_WEB_ANIMATIONS_ANIMATION_SET_H_

#include "cobalt/web_animations/animation_set.h"

#include <set>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "cobalt/cssom/property_definitions.h"
#include "cobalt/web_animations/animation.h"

namespace cobalt {
namespace web_animations {

// Defines a data structure to maintain a set of animations.  Creating a new
// custom class for this datastructure allows us to create many internal
// data structures to allow for efficient lookup of different data, such as
// acquiring the set of all properties affected.
class AnimationSet : public base::RefCounted<AnimationSet> {
 public:
  // We use raw pointers here to avoid cyclic references; AnimationSets
  // are referenced by Animations themselves and it's expected that the
  // Animation destructor will deregister all sets it's contained within.
  typedef std::set<Animation*> InternalSet;

  AnimationSet();

  void AddAnimation(Animation* animation);
  void RemoveAnimation(Animation* animation);

  bool IsEmpty() const { return animations_.empty(); }
  bool IsPropertyAnimated(cssom::PropertyKey property_name) const;

  int GetSize() const { return static_cast<int>(animations_.size()); }

  const InternalSet& animations() const { return animations_; }

 private:
  ~AnimationSet() {}

  InternalSet animations_;

  friend class base::RefCounted<AnimationSet>;

  DISALLOW_COPY_AND_ASSIGN(AnimationSet);
};

}  // namespace web_animations
}  // namespace cobalt

#endif  // COBALT_WEB_ANIMATIONS_ANIMATION_SET_H_
