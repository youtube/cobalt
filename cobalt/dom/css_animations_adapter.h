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

#ifndef COBALT_DOM_CSS_ANIMATIONS_ADAPTER_H_
#define COBALT_DOM_CSS_ANIMATIONS_ADAPTER_H_

#include <map>
#include <string>

#include "base/memory/ref_counted.h"
#include "cobalt/cssom/animation_set.h"
#include "cobalt/web_animations/animation.h"

namespace cobalt {
namespace dom {

class DOMAnimatable;

// The CSSAnimationsAdapter class is responsible for connecting CSS Animations
// to the Web Animations framework.  As CSS Animation events are generated
// from cssom::AnimationSet::EventHandler method calls(), they are translated
// into the creation and deletion of Web Animation animations.
class CSSAnimationsAdapter : public cssom::AnimationSet::EventHandler {
 public:
  // The parameter 'target' specifies which element will be used as the
  // generated animations' target element.
  explicit CSSAnimationsAdapter(
      const scoped_refptr<dom::DOMAnimatable>& target);
  virtual ~CSSAnimationsAdapter();

  // From cssom::AnimationSet::EventHandler.
  void OnAnimationStarted(const cssom::Animation& css_animation) override;
  void OnAnimationRemoved(const cssom::Animation& css_animation) override;

 private:
  // The AnimationWithEventHandler struct maintains a reference to the Animation
  // object and also owns an Animation::EventHandler that connects animation
  // events to this CSSTransitionsAdapter object.
  struct AnimationWithEventHandler {
    AnimationWithEventHandler(
        const scoped_refptr<web_animations::Animation>& animation,
        scoped_ptr<web_animations::Animation::EventHandler> event_handler)
        : animation(animation), event_handler(event_handler.Pass()) {}
    ~AnimationWithEventHandler() {}

    scoped_refptr<web_animations::Animation> animation;
    scoped_ptr<web_animations::Animation::EventHandler> event_handler;
  };
  typedef std::map<std::string, AnimationWithEventHandler*> AnimationMap;

  // Called to handle Animation events.  When a CSS animation's corresponding
  // web animation enters the after phase, we fire the animationend event.
  void HandleAnimationEnterAfterPhase(const cssom::Animation& css_animation);

  scoped_refptr<dom::DOMAnimatable> animatable_;

  AnimationMap animation_map_;

  DISALLOW_COPY_AND_ASSIGN(CSSAnimationsAdapter);
};

}  // namespace dom
}  // namespace cobalt

#endif  // COBALT_DOM_CSS_ANIMATIONS_ADAPTER_H_
