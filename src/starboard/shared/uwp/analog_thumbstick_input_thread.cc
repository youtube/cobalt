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

#include "starboard/shared/uwp/analog_thumbstick_input_thread.h"

#include <algorithm>
#include <map>
#include <vector>

#include "starboard/common/thread.h"
#include "starboard/double.h"
#include "starboard/shared/uwp/analog_thumbstick_input.h"
#include "starboard/thread.h"

namespace starboard {
namespace shared {
namespace uwp {

class AnalogThumbstickThread::Impl : public Thread {
 public:
  explicit Impl(Callback* cb) : Thread("AnalogGamepad"), callback_(cb) {
    stick_is_centered_[kSbKeyGamepadLeftStickLeft] = true;
    stick_is_centered_[kSbKeyGamepadRightStickLeft] = true;
    stick_is_centered_[kSbKeyGamepadLeftStickUp] = true;
    stick_is_centered_[kSbKeyGamepadRightStickUp] = true;

    Thread::Start();
  }
  ~Impl() { Thread::Join(); }

  void Run() override {
    while (!join_called()) {
      Update();
      // 120hz to provide smooth 60fps playback.
      SbThreadSleep(kSbTimeSecond / kPollingFrequency);
    }
  }

  void Update() {
    ThumbSticks thumb_state = GetCombinedThumbStickState();

    FireEventIfNecessary(kSbKeyGamepadLeftStickLeft, thumb_state.left_x);
    FireEventIfNecessary(kSbKeyGamepadLeftStickUp, thumb_state.left_y);
    FireEventIfNecessary(kSbKeyGamepadRightStickLeft, thumb_state.right_x);
    FireEventIfNecessary(kSbKeyGamepadRightStickUp, thumb_state.right_y);
  }

  void FireEventIfNecessary(SbKey sb_key, float value) {
    if (value == 0.0f) {
      if (stick_is_centered_[sb_key]) {
        // The previous stick input is in center position, so it is not
        // necessary to inject another center input event.
        return;
      }
      stick_is_centered_[sb_key] = true;
    } else {
      stick_is_centered_[sb_key] = false;
    }

    SbInputVector input_vector = {0, 0};

    switch (sb_key) {
      case kSbKeyGamepadRightStickLeft:
      case kSbKeyGamepadLeftStickLeft: {
        input_vector.x = value;
        break;
      }
      case kSbKeyGamepadRightStickUp:
      case kSbKeyGamepadLeftStickUp: {
        input_vector.y = -value;
        break;
      }
      default: {
        SB_NOTREACHED();
        break;
      }
    }
    callback_->OnJoystickUpdate(sb_key, input_vector);
  }

  ThumbSticks GetCombinedThumbStickState() {
    ThumbSticks all_thumb_state;
    GetGamepadThumbSticks(&thumb_sticks_);

    for (int i = 0; i < thumb_sticks_.size(); ++i) {
      ThumbSticks thumb_state = thumb_sticks_[i];

      all_thumb_state.left_x += thumb_state.left_x;
      all_thumb_state.left_y += thumb_state.left_y;
      all_thumb_state.right_x += thumb_state.right_x;
      all_thumb_state.right_y += thumb_state.right_y;
    }

    all_thumb_state.left_x = ClampToZeroOne(all_thumb_state.left_x);
    all_thumb_state.left_y = ClampToZeroOne(all_thumb_state.left_y);
    all_thumb_state.right_x = ClampToZeroOne(all_thumb_state.right_x);
    all_thumb_state.right_y = ClampToZeroOne(all_thumb_state.right_y);

    return all_thumb_state;
  }

  static float ClampToZeroOne(float in) {
    return std::max(-1.0f, std::min(1.0f, in));
  }
  Callback* callback_;
  std::map<SbKey, bool> stick_is_centered_;
  std::vector<ThumbSticks> thumb_sticks_;
};

AnalogThumbstickThread::AnalogThumbstickThread(Callback* cb) {
  impl_.reset(new Impl(cb));
}

AnalogThumbstickThread::~AnalogThumbstickThread() {
  impl_.reset(nullptr);
}

}  // namespace uwp
}  // namespace shared
}  // namespace starboard
