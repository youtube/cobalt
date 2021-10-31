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

#ifndef COBALT_WEB_ANIMATIONS_ANIMATABLE_H_
#define COBALT_WEB_ANIMATIONS_ANIMATABLE_H_

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "cobalt/script/wrappable.h"
#include "cobalt/web_animations/animation.h"
#include "cobalt/web_animations/animation_timeline.h"

namespace cobalt {
namespace web_animations {

// Objects that may be the target of an KeyframeEffectReadOnly object implement
// the Animatable interface.
//   https://www.w3.org/TR/2015/WD-web-animations-1-20150707/#the-animatable-interface
// We unfortunately are not able to express this in IDL currently as it
// represents an interface that dom::Element and dom::PseudoElement must
// implement, but we don't have an IDL inheritance mechanism in place right now.
class Animatable : public script::Wrappable {
 public:
  Animatable() {}

  // Returns the default timeline associated with the underlying Animatable
  // object.
  virtual scoped_refptr<AnimationTimeline> GetDefaultTimeline() const = 0;

  // Registers an animation with this Animatable object.
  virtual void Register(web_animations::Animation* animation) = 0;
  virtual void Deregister(web_animations::Animation* animation) = 0;

  DEFINE_WRAPPABLE_TYPE(Animatable);

 protected:
  virtual ~Animatable() {}

 private:
  friend class base::RefCounted<Animatable>;

  DISALLOW_COPY_AND_ASSIGN(Animatable);
};

}  // namespace web_animations
}  // namespace cobalt

#endif  // COBALT_WEB_ANIMATIONS_ANIMATABLE_H_
