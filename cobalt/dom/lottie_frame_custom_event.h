// Copyright 2020 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_DOM_LOTTIE_FRAME_CUSTOM_EVENT_H_
#define COBALT_DOM_LOTTIE_FRAME_CUSTOM_EVENT_H_

#include <memory>
#include <string>

#include "cobalt/dom/event.h"
#include "cobalt/dom/lottie_frame_custom_event_detail.h"

namespace cobalt {
namespace dom {

// Lottie frame events carry custom data about the frame rendered and how far
// the animation has played.
//   https://lottiefiles.github.io/lottie-player/events.html
class LottieFrameCustomEvent : public Event {
 public:
  explicit LottieFrameCustomEvent(const std::string& type) : Event(type) {}
  LottieFrameCustomEvent(const std::string& type, const EventInit& init_dict)
      : Event(type, init_dict) {}

  void set_detail(const LottieFrameCustomEventDetail& detail) {
    detail_ = detail;
  }

  const LottieFrameCustomEventDetail& detail() const { return detail_; }

  DEFINE_WRAPPABLE_TYPE(LottieFrameCustomEvent);

 protected:
  ~LottieFrameCustomEvent() override {}

  LottieFrameCustomEventDetail detail_;
};

}  // namespace dom
}  // namespace cobalt

#endif  // COBALT_DOM_LOTTIE_FRAME_CUSTOM_EVENT_H_
