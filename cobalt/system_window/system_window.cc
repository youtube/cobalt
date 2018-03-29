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

#include "cobalt/system_window/system_window.h"

#include <algorithm>
#include <cmath>

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/stringprintf.h"
#include "cobalt/base/event_dispatcher.h"
#include "cobalt/system_window/input_event.h"
#include "starboard/double.h"
#include "starboard/system.h"

namespace cobalt {
namespace system_window {

namespace {

SystemWindow* g_the_window = NULL;

int Round(const float f) {
  double d(f + 0.5f);
  return static_cast<int>(SbDoubleFloor(d));
}

}  // namespace

SystemWindow::SystemWindow(base::EventDispatcher* event_dispatcher,
                           const base::optional<math::Size>& window_size)
    : event_dispatcher_(event_dispatcher),
      window_(kSbWindowInvalid),
      key_down_(false) {
  if (!window_size) {
    window_ = SbWindowCreate(NULL);
  } else {
    SbWindowOptions options;
    SbWindowSetDefaultOptions(&options);
    options.size.width = window_size->width();
    options.size.height = window_size->height();
    window_ = SbWindowCreate(&options);
  }
  DCHECK(SbWindowIsValid(window_));
  DCHECK(!g_the_window) << "TODO: Support multiple SystemWindows.";
  g_the_window = this;
}

SystemWindow::~SystemWindow() {
  DCHECK_EQ(this, g_the_window);

  if (g_the_window == this) {
    g_the_window = NULL;
  }
  SbWindowDestroy(window_);
}

math::Size SystemWindow::GetWindowSize() const {
  SbWindowSize window_size;
  if (!SbWindowGetSize(window_, &window_size)) {
    DLOG(WARNING) << "SbWindowGetSize() failed.";
    return math::Size(0, 0);
  }
  return math::Size(window_size.width, window_size.height);
}

float SystemWindow::GetVideoPixelRatio() const {
  SbWindowSize window_size;
  if (!SbWindowGetSize(window_, &window_size)) {
    DLOG(WARNING) << "SbWindowGetSize() failed.";
    return 1.0;
  }
  return window_size.video_pixel_ratio;
}

math::Size SystemWindow::GetVideoOutputResolution() const {
  float ratio = GetVideoPixelRatio();
  math::Size size = GetWindowSize();
  return math::Size(Round(size.width() * ratio), Round(size.height() * ratio));
}

SbWindow SystemWindow::GetSbWindow() { return window_; }

void* SystemWindow::GetWindowHandle() {
  return SbWindowGetPlatformHandle(window_);
}

void SystemWindow::DispatchInputEvent(const SbInputData& data,
                                      InputEvent::Type type, bool is_repeat) {
  // Starboard handily uses the Microsoft key mapping, which is also what Cobalt
  // uses.
  int key_code = static_cast<int>(data.key);
#if SB_API_VERSION >= 6
  float pressure = data.pressure;
  uint32 modifiers = data.key_modifiers;
  if (((data.device_type == kSbInputDeviceTypeTouchPad) ||
       (data.device_type == kSbInputDeviceTypeTouchScreen))) {
    switch (type) {
      case InputEvent::kPointerDown:
      case InputEvent::kPointerMove:
      case InputEvent::kTouchpadDown:
      case InputEvent::kTouchpadMove:
        // For touch contact input, ensure that the device button state is also
        // reported as pressed.
        //   https://www.w3.org/TR/2015/REC-pointerevents-20150224/#button-states
        key_code = kSbKeyMouse1;
        modifiers |= InputEvent::kLeftButton;
        if (!std::isnan(pressure)) {
          pressure = std::max(pressure, 0.5f);
        }
        break;
      case InputEvent::kKeyDown:
      case InputEvent::kKeyUp:
      case InputEvent::kKeyMove:
      case InputEvent::kInput:
      case InputEvent::kPointerUp:
      case InputEvent::kTouchpadUp:
      case InputEvent::kWheel:
        break;
    }
  }

#if SB_HAS(ON_SCREEN_KEYBOARD)
  scoped_ptr<InputEvent> input_event(
      new InputEvent(type, data.device_id, key_code, modifiers, is_repeat,
                     math::PointF(data.position.x, data.position.y),
                     math::PointF(data.delta.x, data.delta.y), pressure,
                     math::PointF(data.size.x, data.size.y),
                     math::PointF(data.tilt.x, data.tilt.y),
                     data.input_text ? data.input_text : ""));
#else   // SB_HAS(ON_SCREEN_KEYBOARD)
  scoped_ptr<InputEvent> input_event(
      new InputEvent(type, data.device_id, key_code, modifiers, is_repeat,
                     math::PointF(data.position.x, data.position.y),
                     math::PointF(data.delta.x, data.delta.y), pressure,
                     math::PointF(data.size.x, data.size.y),
                     math::PointF(data.tilt.x, data.tilt.y)));
#endif  // SB_HAS(ON_SCREEN_KEYBOARD)
#else
  scoped_ptr<InputEvent> input_event(
      new InputEvent(type, data.device_id, key_code, data.key_modifiers,
                     is_repeat, math::PointF(data.position.x, data.position.y),
                     math::PointF(data.delta.x, data.delta.y)));
#endif
  event_dispatcher()->DispatchEvent(input_event.PassAs<base::Event>());
}

void SystemWindow::HandlePointerInputEvent(const SbInputData& data) {
  switch (data.type) {
    case kSbInputEventTypePress: {
      InputEvent::Type input_event_type =
          data.device_type == kSbInputDeviceTypeTouchPad
              ? InputEvent::kTouchpadDown
              : InputEvent::kPointerDown;
      DispatchInputEvent(data, input_event_type, false /* is_repeat */);
      break;
    }
    case kSbInputEventTypeUnpress: {
      InputEvent::Type input_event_type =
          data.device_type == kSbInputDeviceTypeTouchPad
              ? InputEvent::kTouchpadUp
              : InputEvent::kPointerUp;
      DispatchInputEvent(data, input_event_type, false /* is_repeat */);
      break;
    }
#if SB_API_VERSION >= 6
    case kSbInputEventTypeWheel: {
      DispatchInputEvent(data, InputEvent::kWheel, false /* is_repeat */);
      break;
    }
#endif
    case kSbInputEventTypeMove: {
      InputEvent::Type input_event_type =
          data.device_type == kSbInputDeviceTypeTouchPad
              ? InputEvent::kTouchpadMove
              : InputEvent::kPointerMove;
      DispatchInputEvent(data, input_event_type, false /* is_repeat */);
      break;
    }
    default:
      SB_NOTREACHED();
      break;
  }
}

void SystemWindow::HandleInputEvent(const SbInputData& data) {
  DCHECK_EQ(window_, data.window);
  // Handle supported pointer device types.
  if ((kSbInputDeviceTypeMouse == data.device_type) ||
      (kSbInputDeviceTypeTouchScreen == data.device_type) ||
      (kSbInputDeviceTypeTouchPad == data.device_type)) {
    HandlePointerInputEvent(data);
    return;
  }

  // Handle all other input device types.
  switch (data.type) {
    case kSbInputEventTypePress: {
      DispatchInputEvent(data, InputEvent::kKeyDown, key_down_);
      key_down_ = true;
      break;
    }
    case kSbInputEventTypeUnpress: {
      DispatchInputEvent(data, InputEvent::kKeyUp, false /* is_repeat */);
      key_down_ = false;
      break;
    }
    case kSbInputEventTypeMove: {
      DispatchInputEvent(data, InputEvent::kKeyMove, false /* is_repeat */);
      break;
    }
#if SB_HAS(ON_SCREEN_KEYBOARD)
    case kSbInputEventTypeInput: {
      DispatchInputEvent(data, InputEvent::kInput, false /* is_repeat */);
      break;
    }
#endif  // SB_HAS(ON_SCREEN_KEYBOARD)
    default:
      break;
  }
}

SystemWindow* SystemWindow::PrimaryWindow() { return g_the_window; }

void HandleInputEvent(const SbEvent* event) {
  if (event->type != kSbEventTypeInput) {
    return;
  }

  DCHECK(g_the_window);
  DCHECK(event->data);
  SbInputData* data = reinterpret_cast<SbInputData*>(event->data);
  g_the_window->HandleInputEvent(*data);
  return;
}

}  // namespace system_window
}  // namespace cobalt
