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

#include <iomanip>
#include <set>
#include <sstream>

#include "starboard/event.h"
#include "starboard/input.h"
#include "starboard/log.h"
#include "starboard/system.h"
#include "starboard/window.h"

namespace {
// Helper set to keep track of which keys are currently pressed.
typedef std::set<SbKey> KeySet;
KeySet s_is_pressed;
}  // namespace

SbWindow g_window;

void SbEventHandle(const SbEvent* event) {
  switch (event->type) {
    case kSbEventTypeStart: {
      SB_LOG(INFO) << "START";
      SbEventStartData* data = static_cast<SbEventStartData*>(event->data);
      g_window = SbWindowCreate(NULL);
      SB_CHECK(SbWindowIsValid(g_window));

#if SB_API_VERSION >= 4
      SB_LOG(INFO) << "    F1 - Pause";
      SB_LOG(INFO) << "    F2 - Unpause";
      SB_LOG(INFO) << "    F3 - Suspend";
#endif  // SB_API_VERSION >= 4
      SB_LOG(INFO) << "    F5 - Stop";
      break;
    }
    case kSbEventTypeInput: {
      SbInputData* data = static_cast<SbInputData*>(event->data);

      SB_LOG(INFO) << "INPUT: type=" << data->type
                   << ", window=" << data->window
                   << ", device_type=" << data->device_type
                   << ", device_id=" << data->device_id
                   << ", key=0x" << std::hex << data->key
                   << ", character=" << data->character
                   << ", modifiers=0x" << std::hex << data->key_modifiers
                   << ", location=" << std::dec << data->key_location
                   << ", position="
                   << "[ " << data->position.x << " , " << data->position.y
                   << " ]";

      // Track which keys are currently pressed, from our perspective outside
      // of Starboard.  Print out the current state after each key event.
      if (data->type == kSbInputEventTypePress ||
          data->type == kSbInputEventTypeUnpress) {
        if (data->type == kSbInputEventTypePress) {
          s_is_pressed.insert(data->key);
        } else {
          s_is_pressed.erase(data->key);
        }
        if (!s_is_pressed.empty()) {
          std::stringstream keys;
          keys << "Keys currently pressed:";
          for (KeySet::const_iterator iter = s_is_pressed.begin();
               iter != s_is_pressed.end(); ++iter) {
            keys << " " << std::hex << *iter;
          }
          SB_LOG(INFO) << keys.str();
        }
      }

      switch (data->key) {
#if SB_API_VERSION >= 4
        case kSbKeyF1:
          SbSystemRequestPause();
          break;
        case kSbKeyF2:
          SbSystemRequestUnpause();
          break;
        case kSbKeyF3:
          SbSystemRequestSuspend();
          break;
#endif  // SB_API_VERSION >= 4
        case kSbKeyF5:
          SbSystemRequestStop(0);
          break;
        default:
          // Do nothing.
          break;
      }
      break;
    }
    case kSbEventTypePause: {
      SB_LOG(INFO) << "PAUSE";
      break;
    }
    case kSbEventTypeResume: {
      SB_LOG(INFO) << "RESUME";
      break;
    }
    case kSbEventTypeStop: {
      SB_LOG(INFO) << "STOP";
      SbWindowDestroy(g_window);
      break;
    }
    case kSbEventTypeSuspend: {
      SB_LOG(INFO) << "SUSPEND";
      break;
    }
    case kSbEventTypeUnpause: {
      SB_LOG(INFO) << "UNPAUSE";
      break;
    }
    default:
      SB_LOG(INFO) << "Event Type " << event->type << " not handled.";
      break;
  }
}
