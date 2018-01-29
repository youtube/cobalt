/*
 * Copyright 2017 Google Inc. All Rights Reserved.
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

#ifndef COBALT_INPUT_INPUT_POLLER_IMPL_H_
#define COBALT_INPUT_INPUT_POLLER_IMPL_H_

#include <map>

#include "base/containers/small_map.h"
#include "base/hash_tables.h"
#include "cobalt/input/input_poller.h"
#include "cobalt/system_window/input_event.h"
#include "starboard/input.h"
#include "starboard/mutex.h"
#include "starboard/window.h"

namespace cobalt {
namespace input {

class InputPollerImpl : public InputPoller {
 public:
  InputPollerImpl();
  virtual ~InputPollerImpl() {}

  bool IsPressed(SbKey keycode) override;
  // Returns analog position. The value is normalized to a range from
  // -1.0 to 1.0
  float AnalogInput(SbKey analog_input_id) override;
  void UpdateInputEvent(const system_window::InputEvent* input_event);

 private:
  typedef base::SmallMap<std::map<SbKey, float>, 8> KeyOffsetMap;

  starboard::Mutex input_mutex_;
  base::hash_set<int> pressed_keys_;
  KeyOffsetMap key_offset_map_;
};

}  // namespace input
}  // namespace cobalt

#endif  // COBALT_INPUT_INPUT_POLLER_IMPL_H_
