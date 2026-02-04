//
// Copyright 2020 Comcast Cable Communications Management, LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// SPDX-License-Identifier: Apache-2.0
//
// Copyright 2016 The Cobalt Authors. All Rights Reserved.
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

#ifndef THIRD_PARTY_STARBOARD_RDK_SHARED_ESS_INPUT_H_
#define THIRD_PARTY_STARBOARD_RDK_SHARED_ESS_INPUT_H_

#include "starboard/configuration.h"
#include "starboard/event.h"
#include "starboard/input.h"

namespace third_party {
namespace starboard {
namespace rdk {
namespace shared {

class EssInput {
public:
  EssInput();
  ~EssInput();
  void OnKeyPressed(unsigned int key);
  void OnKeyReleased(unsigned int key);

private:
  void CreateKey(unsigned int key, SbInputEventType type, unsigned int modifiers, bool repeatable);
  void CreateRepeatKey();
  void DeleteRepeatKey();
  void OnKeyboardKey(unsigned int key, SbInputEventType type);
  bool UpdateModifiers(unsigned int key, SbInputEventType type);

  unsigned int key_modifiers_ { 0 };
  unsigned key_repeat_key_ { 0 };
  unsigned key_repeat_modifiers_ { 0 };
  int key_repeat_state_ { 0 };
  SbEventId key_repeat_event_id_ { kSbEventIdInvalid };
  int64_t key_repeat_interval_ { 0 };
};

}  // namespace shared
}  // namespace rdk
}  // namespace starboard
}  // namespace third_party

#endif  // THIRD_PARTY_STARBOARD_RDK_SHARED_ESS_INPUT_H_
