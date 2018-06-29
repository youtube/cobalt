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

#ifndef COBALT_INPUT_INPUT_POLLER_H_
#define COBALT_INPUT_INPUT_POLLER_H_

#include "base/memory/ref_counted.h"
#include "starboard/key.h"

namespace cobalt {
namespace input {

// Thread safe InputPoller provides the owner's ability to get the key state
// and analog key position.
class InputPoller : public base::RefCountedThreadSafe<InputPoller> {
 public:
  InputPoller() {}
  virtual ~InputPoller() {}

  virtual bool IsPressed(SbKey keycode) = 0;
  // Returns analog position. The value is normalized to a range from
  // -1.0 to 1.0
  virtual float AnalogInput(SbKey analog_input_id) = 0;
};

}  // namespace input
}  // namespace cobalt

#endif  // COBALT_INPUT_INPUT_POLLER_H_
