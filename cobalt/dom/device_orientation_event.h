/*
 * Copyright 2017 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_DOM_DEVICE_ORIENTATION_EVENT_H_
#define COBALT_DOM_DEVICE_ORIENTATION_EVENT_H_

#include <string>

#include "base/optional.h"
#include "cobalt/dom/device_orientation_event_init.h"
#include "cobalt/dom/event.h"
#include "cobalt/script/wrappable.h"

namespace cobalt {
namespace dom {

// Represents a device orientation event, where the new orientation of the
// device is represented as intrinsic rotations on the Z (by |alpha|
// degrees), X' (by |beta| degrees), Y'' (by |gamma| degrees) axes; intrinsic
// meaning each of the rotations is done on the axis of the rotated system
// resulting of the previous rotation.
//   https://www.w3.org/TR/2016/CR-orientation-event-20160818/
class DeviceOrientationEvent : public Event {
 public:
  DeviceOrientationEvent();
  explicit DeviceOrientationEvent(const std::string& type);

  DeviceOrientationEvent(const std::string& type,
                         const DeviceOrientationEventInit& init);

  base::Optional<double> alpha() const { return alpha_; }
  base::Optional<double> beta() const { return beta_; }
  base::Optional<double> gamma() const { return gamma_; }
  bool absolute() const { return absolute_; }

  DEFINE_WRAPPABLE_TYPE(DeviceOrientationEvent);

 private:
  base::Optional<double> alpha_;
  base::Optional<double> beta_;
  base::Optional<double> gamma_;
  bool absolute_;

  DISALLOW_COPY_AND_ASSIGN(DeviceOrientationEvent);
};

}  // namespace dom
}  // namespace cobalt

#endif  // COBALT_DOM_DEVICE_ORIENTATION_EVENT_H_
