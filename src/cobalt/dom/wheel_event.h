// Copyright 2017 Google Inc. All Rights Reserved.
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

#ifndef COBALT_DOM_WHEEL_EVENT_H_
#define COBALT_DOM_WHEEL_EVENT_H_

#include <string>

#include "cobalt/base/token.h"
#include "cobalt/dom/mouse_event.h"
#include "cobalt/dom/wheel_event_init.h"

namespace cobalt {
namespace dom {

// The WheelEvent provides specific contextual information associated with
// wheel devices. Wheels are devices that can be rotated in one or more spatial
// dimensions, and which can be associated with a pointer device
//   https://www.w3.org/TR/2016/WD-uievents-20160804/#events-wheelevents
class WheelEvent : public MouseEvent {
 public:
  // Web API: WheelEvent
  // DeltaMode values as defined by Web API Node.nodeType.
  typedef uint32 DeltaMode;  // Work around lack of strongly-typed enums
                             // in C++03.
  enum DeltaModeInternal {   // A name is given only to pacify the compiler.
                             // Use |DeltaMode| instead.
    kDomDeltaPixel = 0x00,
    kDomDeltaLine = 0x01,
    kDomDeltaPage = 0x02,
  };

  explicit WheelEvent(const std::string& type);
  WheelEvent(const std::string& type, const WheelEventInit& init_dict);
  WheelEvent(base::Token type, const scoped_refptr<Window>& view,
             const WheelEventInit& init_dict);

  // Creates an event with its "initialized flag" unset.
  explicit WheelEvent(UninitializedFlag uninitialized_flag);

  void InitWheelEvent(const std::string& type, bool bubbles, bool cancelable,
                      const scoped_refptr<Window>& view, int32 detail,
                      int32 screen_x, int32 screen_y, int32 client_x,
                      int32 client_y, uint16 button,
                      const scoped_refptr<EventTarget>& related_target,
                      const std::string& modifierslist, double delta_x,
                      double delta_y, double delta_z, uint32 delta_mode);

  double delta_x() const { return delta_x_; }
  double delta_y() const { return delta_y_; }
  double delta_z() const { return delta_z_; }
  DeltaMode delta_mode() const { return delta_mode_; }

  DEFINE_WRAPPABLE_TYPE(WheelEvent);

 private:
  double delta_x_;
  double delta_y_;
  double delta_z_;
  DeltaMode delta_mode_;

  ~WheelEvent() override {}
};

}  // namespace dom
}  // namespace cobalt

#endif  // COBALT_DOM_WHEEL_EVENT_H_
