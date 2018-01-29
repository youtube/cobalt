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

#ifndef COBALT_DOM_ANIMATION_EVENT_H_
#define COBALT_DOM_ANIMATION_EVENT_H_

#include <string>

#include "cobalt/base/token.h"
#include "cobalt/cssom/property_definitions.h"
#include "cobalt/dom/event.h"

namespace cobalt {
namespace dom {

// The completion of a CSS Animation generates a corresponding DOM Event.
//   https://www.w3.org/TR/2013/WD-css3-animations-20130219/#animation-events
class AnimationEvent : public Event {
 public:
  explicit AnimationEvent(const std::string& type)
      : Event(base::Token(type), kBubbles, kNotCancelable),
        animation_name_(""),
        elapsed_time_(0) {}

  AnimationEvent(base::Token type, const std::string& animation_name,
                 float elapsed_time)
      : Event(type, kBubbles, kNotCancelable),
        animation_name_(animation_name),
        elapsed_time_(elapsed_time) {}

  // The name of the @keyframes rule identifying the animation.
  std::string animation_name() const { return animation_name_; }

  // The amount of time the animation has been running, in seconds, when this
  // event fired. Note that this value is not affected by the value of
  // animation-delay.
  float elapsed_time() const { return elapsed_time_; }

  DEFINE_WRAPPABLE_TYPE(AnimationEvent);

 private:
  ~AnimationEvent() override {}

  std::string animation_name_;
  float elapsed_time_;
};

}  // namespace dom
}  // namespace cobalt

#endif  // COBALT_DOM_ANIMATION_EVENT_H_
