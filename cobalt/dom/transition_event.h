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

#ifndef COBALT_DOM_TRANSITION_EVENT_H_
#define COBALT_DOM_TRANSITION_EVENT_H_

#include <string>

#include "cobalt/base/token.h"
#include "cobalt/cssom/property_definitions.h"
#include "cobalt/dom/event.h"

namespace cobalt {
namespace dom {

// The completion of a CSS Transition generates a corresponding DOM Event.
//   https://www.w3.org/TR/2013/WD-css3-transitions-20131119/#transition-events
class TransitionEvent : public Event {
 public:
  explicit TransitionEvent(const std::string& type)
      : Event(base::Token(type), kBubbles, kCancelable),
        property_(cssom::kNoneProperty),
        elapsed_time_(0) {}

  TransitionEvent(base::Token type, cssom::PropertyKey property,
                  float elapsed_time)
      : Event(type, kBubbles, kCancelable),
        property_(property),
        elapsed_time_(elapsed_time) {}

  // The name of the CSS property associated with the transition.
  std::string property_name() const { return GetPropertyName(property_); }

  // The amount of time the transition has been running, in seconds, when this
  // event fired. Note that this value is not affected by the value of
  // transition-delay.
  float elapsed_time() const { return elapsed_time_; }

  DEFINE_WRAPPABLE_TYPE(TransitionEvent);

 private:
  ~TransitionEvent() override {}

  cssom::PropertyKey property_;
  float elapsed_time_;
};

}  // namespace dom
}  // namespace cobalt

#endif  // COBALT_DOM_TRANSITION_EVENT_H_
