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

#include "starboard/android/shared/input_events_generator.h"

#include <android/input.h>
#include <android/keycodes.h>
#include <jni.h>

#include "starboard/android/shared/application_android.h"
#include "starboard/android/shared/jni_env_ext.h"
#include "starboard/android/shared/jni_utils.h"
#include "starboard/double.h"
#include "starboard/key.h"

namespace starboard {
namespace android {
namespace shared {

using ::starboard::shared::starboard::Application;
typedef ::starboard::android::shared::InputEventsGenerator::Event Event;
typedef ::starboard::android::shared::InputEventsGenerator::Events Events;

namespace {

SbKeyLocation AInputEventToSbKeyLocation(AInputEvent *event) {
  int32_t keycode = AKeyEvent_getKeyCode(event);
  switch (keycode) {
    case AKEYCODE_ALT_LEFT:
    case AKEYCODE_CTRL_LEFT:
    case AKEYCODE_META_LEFT:
    case AKEYCODE_SHIFT_LEFT:
      return kSbKeyLocationLeft;
    case AKEYCODE_ALT_RIGHT:
    case AKEYCODE_CTRL_RIGHT:
    case AKEYCODE_META_RIGHT:
    case AKEYCODE_SHIFT_RIGHT:
      return kSbKeyLocationRight;
  }
  return kSbKeyLocationUnspecified;
}

unsigned int AInputEventToSbModifiers(AInputEvent *event) {
  int32_t meta = AKeyEvent_getMetaState(event);
  unsigned int modifiers = kSbKeyModifiersNone;
  if (meta & AMETA_ALT_ON) {
    modifiers |= kSbKeyModifiersAlt;
  }
  if (meta & AMETA_CTRL_ON) {
    modifiers |= kSbKeyModifiersCtrl;
  }
  if (meta & AMETA_META_ON) {
    modifiers |= kSbKeyModifiersMeta;
  }
  if (meta & AMETA_SHIFT_ON) {
    modifiers |= kSbKeyModifiersShift;
  }
  return modifiers;
}

std::unique_ptr<Event> CreateMoveEventWithKey(
    int32_t device_id,
    SbWindow window,
    SbKey key,
    SbKeyLocation location,
    const SbInputVector& input_vector) {
  std::unique_ptr<SbInputData> data(new SbInputData());
  SbMemorySet(data.get(), 0, sizeof(*data));

  // window
  data->window = window;
  data->type = kSbInputEventTypeMove;
  data->device_type = kSbInputDeviceTypeGamepad;
  data->device_id = device_id;

  // key
  data->key = key;
  data->key_location = location;
  data->key_modifiers = kSbKeyModifiersNone;
  data->position = input_vector;

  return std::unique_ptr<Event>(
      new Application::Event(kSbEventTypeInput, data.release(),
                             &Application::DeleteDestructor<SbInputData>));
}

float GetFlat(jobject input_device, int axis) {
  JniEnvExt* env = JniEnvExt::Get();
  ScopedLocalJavaRef<jobject> motion_range(env->CallObjectMethodOrAbort(
      input_device, "getMotionRange",
      "(I)Landroid/view/InputDevice$MotionRange;", axis));

  float flat = env->CallFloatMethodOrAbort(
      motion_range.Get(), "getFlat", "()F");

  SB_DCHECK(flat < 1.0f);
  return flat;
}

bool IsDPadKey(SbKey key) {
  return key == kSbKeyGamepadDPadUp || key == kSbKeyGamepadDPadDown ||
         key == kSbKeyGamepadDPadLeft || key == kSbKeyGamepadDPadRight;
}

SbKey AInputEventToSbKey(AInputEvent* event) {
  int32_t keycode = AKeyEvent_getKeyCode(event);
  switch (keycode) {
    // Modifiers
    case AKEYCODE_ALT_LEFT:
    case AKEYCODE_ALT_RIGHT:
    case AKEYCODE_MENU:
      return kSbKeyMenu;
    case AKEYCODE_CTRL_LEFT:
    case AKEYCODE_CTRL_RIGHT:
      return kSbKeyControl;
    case AKEYCODE_META_LEFT:
      return kSbKeyLwin;
    case AKEYCODE_META_RIGHT:
      return kSbKeyRwin;
    case AKEYCODE_SHIFT_LEFT:
    case AKEYCODE_SHIFT_RIGHT:
      return kSbKeyShift;
    case AKEYCODE_CAPS_LOCK:
      return kSbKeyCapital;
    case AKEYCODE_NUM_LOCK:
      return kSbKeyNumlock;
    case AKEYCODE_SCROLL_LOCK:
      return kSbKeyScroll;

    // System functions
    case AKEYCODE_SLEEP:
      return kSbKeySleep;
    case AKEYCODE_HELP:
      return kSbKeyHelp;

    // Navigation
    case AKEYCODE_BACK:  // Android back button, not backspace
    case AKEYCODE_ESCAPE:
      return kSbKeyEscape;

    // Enter/Select
    case AKEYCODE_ENTER:
    case AKEYCODE_NUMPAD_ENTER:
      return kSbKeyReturn;

    // Focus movement
    case AKEYCODE_PAGE_UP:
      return kSbKeyPrior;
    case AKEYCODE_PAGE_DOWN:
      return kSbKeyNext;
    case AKEYCODE_MOVE_HOME:
      return kSbKeyHome;
    case AKEYCODE_MOVE_END:
      return kSbKeyEnd;

    // D-pad
    case AKEYCODE_DPAD_UP:
      return kSbKeyGamepadDPadUp;
    case AKEYCODE_DPAD_DOWN:
      return kSbKeyGamepadDPadDown;
    case AKEYCODE_DPAD_LEFT:
      return kSbKeyGamepadDPadLeft;
    case AKEYCODE_DPAD_RIGHT:
      return kSbKeyGamepadDPadRight;
    case AKEYCODE_DPAD_CENTER:
      return kSbKeyGamepad1;

    // Game controller
    case AKEYCODE_BUTTON_A:
      return kSbKeyGamepad1;
    case AKEYCODE_BUTTON_B:
      return kSbKeyGamepad2;
    case AKEYCODE_BUTTON_C:
      return kSbKeyUnknown;
    case AKEYCODE_BUTTON_X:
      return kSbKeyGamepad3;
    case AKEYCODE_BUTTON_Y:
      return kSbKeyGamepad4;
    case AKEYCODE_BUTTON_L1:
      return kSbKeyGamepadLeftBumper;
    case AKEYCODE_BUTTON_R1:
      return kSbKeyGamepadRightBumper;
    case AKEYCODE_BUTTON_L2:
      return kSbKeyGamepadLeftTrigger;
    case AKEYCODE_BUTTON_R2:
      return kSbKeyGamepadRightTrigger;
    case AKEYCODE_BUTTON_THUMBL:
      return kSbKeyGamepadLeftStick;
    case AKEYCODE_BUTTON_THUMBR:
      return kSbKeyGamepadRightStick;
    case AKEYCODE_BUTTON_START:
      return kSbKeyGamepad6;
    case AKEYCODE_BUTTON_SELECT:
      return kSbKeyGamepad5;
    case AKEYCODE_BUTTON_MODE:
      return kSbKeyModechange;

    // Media transport
    case AKEYCODE_MEDIA_PLAY_PAUSE:
      return kSbKeyMediaPlayPause;
    case AKEYCODE_MEDIA_PLAY:
      return kSbKeyPlay;
    case AKEYCODE_MEDIA_PAUSE:
      return kSbKeyPause;
    case AKEYCODE_MEDIA_STOP:
      return kSbKeyMediaStop;
    case AKEYCODE_MEDIA_NEXT:
      return kSbKeyMediaNextTrack;
    case AKEYCODE_MEDIA_PREVIOUS:
      return kSbKeyMediaPrevTrack;
    case AKEYCODE_MEDIA_REWIND:
      return kSbKeyMediaRewind;
    case AKEYCODE_MEDIA_FAST_FORWARD:
      return kSbKeyMediaFastForward;

#if SB_API_VERSION >= 6
    // TV Remote specific
    case AKEYCODE_CHANNEL_UP:
      return kSbKeyChannelUp;
    case AKEYCODE_CHANNEL_DOWN:
      return kSbKeyChannelDown;
    case AKEYCODE_CAPTIONS:
      return kSbKeyClosedCaption;
    case AKEYCODE_INFO:
      return kSbKeyInfo;
    case AKEYCODE_GUIDE:
      return kSbKeyGuide;
    case AKEYCODE_LAST_CHANNEL:
      return kSbKeyLast;
    case AKEYCODE_MEDIA_AUDIO_TRACK:
      return kSbKeyMediaAudioTrack;

    case AKEYCODE_PROG_RED:
      return kSbKeyRed;
    case AKEYCODE_PROG_GREEN:
      return kSbKeyGreen;
    case AKEYCODE_PROG_YELLOW:
      return kSbKeyYellow;
    case AKEYCODE_PROG_BLUE:
      return kSbKeyBlue;
#endif  // SB_API_VERSION >= 6

    // Whitespace
    case AKEYCODE_TAB:
      return kSbKeyTab;
    case AKEYCODE_SPACE:
      return kSbKeySpace;

    // Deletion
    case AKEYCODE_DEL:  // Backspace
      return kSbKeyBack;
    case AKEYCODE_FORWARD_DEL:
      return kSbKeyDelete;
    case AKEYCODE_CLEAR:
      return kSbKeyClear;

    // Insert
    case AKEYCODE_INSERT:
      return kSbKeyInsert;

    // Symbols
    case AKEYCODE_NUMPAD_ADD:
      return kSbKeyAdd;
    case AKEYCODE_PLUS:
    case AKEYCODE_EQUALS:
    case AKEYCODE_NUMPAD_EQUALS:
      return kSbKeyOemPlus;
    case AKEYCODE_NUMPAD_SUBTRACT:
      return kSbKeySubtract;
    case AKEYCODE_MINUS:
      return kSbKeyOemMinus;
    case AKEYCODE_NUMPAD_MULTIPLY:
      return kSbKeyMultiply;
    case AKEYCODE_NUMPAD_DIVIDE:
      return kSbKeyDivide;
    case AKEYCODE_COMMA:
    case AKEYCODE_NUMPAD_COMMA:
      return kSbKeyOemComma;
    case AKEYCODE_NUMPAD_DOT:
      return kSbKeyDecimal;
    case AKEYCODE_PERIOD:
      return kSbKeyOemPeriod;
    case AKEYCODE_SEMICOLON:
      return kSbKeyOem1;
    case AKEYCODE_SLASH:
      return kSbKeyOem2;
    case AKEYCODE_GRAVE:
      return kSbKeyOem3;
    case AKEYCODE_LEFT_BRACKET:
      return kSbKeyOem4;
    case AKEYCODE_BACKSLASH:
      return kSbKeyOem5;
    case AKEYCODE_RIGHT_BRACKET:
      return kSbKeyOem6;
    case AKEYCODE_APOSTROPHE:
      return kSbKeyOem7;

    // Function keys
    case AKEYCODE_F1:
    case AKEYCODE_F2:
    case AKEYCODE_F3:
    case AKEYCODE_F4:
    case AKEYCODE_F5:
    case AKEYCODE_F6:
    case AKEYCODE_F7:
    case AKEYCODE_F8:
    case AKEYCODE_F9:
    case AKEYCODE_F10:
    case AKEYCODE_F11:
    case AKEYCODE_F12:
      return static_cast<SbKey>(kSbKeyF1 + (keycode - AKEYCODE_F1));

    // Digits
    case AKEYCODE_0:
    case AKEYCODE_1:
    case AKEYCODE_2:
    case AKEYCODE_3:
    case AKEYCODE_4:
    case AKEYCODE_5:
    case AKEYCODE_6:
    case AKEYCODE_7:
    case AKEYCODE_8:
    case AKEYCODE_9:
      return static_cast<SbKey>(kSbKey0 + (keycode - AKEYCODE_0));

    // Numpad digits
    case AKEYCODE_NUMPAD_0:
    case AKEYCODE_NUMPAD_1:
    case AKEYCODE_NUMPAD_2:
    case AKEYCODE_NUMPAD_3:
    case AKEYCODE_NUMPAD_4:
    case AKEYCODE_NUMPAD_5:
    case AKEYCODE_NUMPAD_6:
    case AKEYCODE_NUMPAD_7:
    case AKEYCODE_NUMPAD_8:
    case AKEYCODE_NUMPAD_9:
      return static_cast<SbKey>(kSbKeyNumpad0 + (keycode - AKEYCODE_NUMPAD_0));

    // Alphabetic
    case AKEYCODE_A:
    case AKEYCODE_B:
    case AKEYCODE_C:
    case AKEYCODE_D:
    case AKEYCODE_E:
    case AKEYCODE_F:
    case AKEYCODE_G:
    case AKEYCODE_H:
    case AKEYCODE_I:
    case AKEYCODE_J:
    case AKEYCODE_K:
    case AKEYCODE_L:
    case AKEYCODE_M:
    case AKEYCODE_N:
    case AKEYCODE_O:
    case AKEYCODE_P:
    case AKEYCODE_Q:
    case AKEYCODE_R:
    case AKEYCODE_S:
    case AKEYCODE_T:
    case AKEYCODE_U:
    case AKEYCODE_V:
    case AKEYCODE_W:
    case AKEYCODE_X:
    case AKEYCODE_Y:
    case AKEYCODE_Z:
      return static_cast<SbKey>(kSbKeyA + (keycode - AKEYCODE_A));

    // Don't handle these keys so the OS can in a uniform manner.
    case AKEYCODE_VOLUME_UP:
    case AKEYCODE_VOLUME_DOWN:
    case AKEYCODE_MUTE:
    case AKEYCODE_BRIGHTNESS_UP:
    case AKEYCODE_BRIGHTNESS_DOWN:
    case AKEYCODE_SEARCH:
    default:
      return kSbKeyUnknown;
  }
}

}  // namespace

InputEventsGenerator::InputEventsGenerator(SbWindow window)
    : window_(window),
      hat_value_(),
      left_thumbstick_key_pressed_{kSbKeyUnknown, kSbKeyUnknown} {
  SB_DCHECK(SbWindowIsValid(window_));
}

InputEventsGenerator::~InputEventsGenerator() {}

// For a left joystick, AMOTION_EVENT_AXIS_X reports the absolute X position of
// the joystick. The value is normalized to a range from -1.0 (left) to 1.0
// (right).
//
// For a left joystick, AMOTION_EVENT_AXIS_Y reports the absolute Y position of
// the joystick. The value is normalized to a range from -1.0 (up or far) to 1.0
// (down or near).
//
// On game pads with two analog joysticks, AMOTION_EVENT_AXIS_Z is often
// reinterpreted to report the absolute X position of the second joystick.
//
// On game pads with two analog joysticks, AMOTION_EVENT_AXIS_RZ is often
// reinterpreted to report the absolute Y position of the second joystick.
void InputEventsGenerator::ProcessJoyStickEvent(FlatAxis axis,
                                                int32_t motion_axis,
                                                AInputEvent* android_event,
                                                Events* events) {
  SB_DCHECK(AMotionEvent_getPointerCount(android_event) > 0);

  int32_t device_id = AInputEvent_getDeviceId(android_event);
  SB_DCHECK(device_flat_.find(device_id) != device_flat_.end());

  float flat = device_flat_[device_id][axis];
  float offset = AMotionEvent_getAxisValue(android_event, motion_axis, 0);
  int sign = offset < 0.0f ? -1 : 1;

  if (SbDoubleAbsolute(offset) < flat) {
    offset = sign * flat;
  }
  // Rescaled the range:
  // [-1.0f, -flat] to [-1.0f, 0.0f] and [flat, 1.0f] to [0.0f, 1.0f]
  offset = (offset - sign * flat) / (1 - flat);

  // Report up and left as negative values.
  SbInputVector input_vector;
  SbKey key = kSbKeyUnknown;
  SbKeyLocation location = kSbKeyLocationUnspecified;
  switch (axis) {
    case kLeftX: {
      input_vector.x = offset;
      input_vector.y = 0.0f;
      key = kSbKeyGamepadLeftStickLeft;
      location = kSbKeyLocationLeft;
      break;
    }
    case kLeftY: {
      input_vector.x = 0.0f;
      input_vector.y = offset;
      key = kSbKeyGamepadLeftStickUp;
      location = kSbKeyLocationLeft;
      break;
    }
    case kRightX: {
      input_vector.x = offset;
      input_vector.y = 0.0f;
      key = kSbKeyGamepadRightStickLeft;
      location = kSbKeyLocationRight;
      break;
    }
    case kRightY: {
      input_vector.x = 0.0f;
      input_vector.y = offset;
      key = kSbKeyGamepadRightStickUp;
      location = kSbKeyLocationRight;
      break;
    }
    default:
      SB_NOTREACHED();
  }

  events->push_back(
      CreateMoveEventWithKey(device_id, window_, key, location, input_vector));
}

namespace {

// Generate a Starboard event from an Android event, with the SbKey and
// SbInputEventType pre-specified (so that it can be used by event
// synthesization as well.)
void PushKeyEvent(SbKey key,
                  SbInputEventType type,
                  SbWindow window,
                  AInputEvent* android_event,
                  Events* events) {
  if (key == kSbKeyUnknown) {
    SB_NOTREACHED();
    return;
  }

  std::unique_ptr<SbInputData> data(new SbInputData());
  SbMemorySet(data.get(), 0, sizeof(*data));

  // window
  data->window = window;
  data->type = type;

  // device
  // TODO: differentiate gamepad, remote, etc.
  data->device_type = kSbInputDeviceTypeKeyboard;
  data->device_id = AInputEvent_getDeviceId(android_event);

  // key
  data->key = key;
  data->key_location = AInputEventToSbKeyLocation(android_event);
  data->key_modifiers = AInputEventToSbModifiers(android_event);

  std::unique_ptr<Event> event(
      new Event(kSbEventTypeInput, data.release(),
                &Application::DeleteDestructor<SbInputData>));
  events->push_back(std::move(event));
}

// Some helper enumerations to index into the InputEventsGenerator::hat_value_
// array.
enum HatAxis {
  kHatX,
  kHatY,
};

struct HatValue {
  HatAxis axis;
  float value;
};

// Converts Starboard DPad direction keys to Starboard left thumbstick
// direction keys.
SbKey ConvertDPadKeyToThumbstickKey(SbKey key) {
  switch (key) {
    case kSbKeyGamepadDPadUp:
      return kSbKeyGamepadLeftStickUp;
    case kSbKeyGamepadDPadDown:
      return kSbKeyGamepadLeftStickDown;
    case kSbKeyGamepadDPadLeft:
      return kSbKeyGamepadLeftStickLeft;
    case kSbKeyGamepadDPadRight:
      return kSbKeyGamepadLeftStickRight;
    default: {
      SB_NOTREACHED();
      return kSbKeyUnknown;
    }
  }
}

// Convert a Starboard DPad direction key to a (axis, direction) pair.
HatValue HatValueForDPadKey(SbKey key) {
  SB_DCHECK(IsDPadKey(key));

  switch (key) {
    case kSbKeyGamepadDPadUp:
      return HatValue({kHatY, -1.0f});
    case kSbKeyGamepadDPadDown:
      return HatValue({kHatY, 1.0f});
    case kSbKeyGamepadDPadLeft:
      return HatValue({kHatX, -1.0f});
    case kSbKeyGamepadDPadRight:
      return HatValue({kHatX, 1.0f});
    default: {
      SB_NOTREACHED();
      return HatValue({kHatX, 0.0f});
    }
  }
}

// The inverse of HatValueForDPadKey().
SbKey KeyForHatValue(const HatValue& hat_value) {
  SB_DCHECK(hat_value.value > 0.5f || hat_value.value < -0.5f);
  if (hat_value.axis == kHatX) {
    if (hat_value.value > 0.5f) {
      return kSbKeyGamepadDPadRight;
    } else {
      return kSbKeyGamepadDPadLeft;
    }
  } else if (hat_value.axis == kHatY) {
    if (hat_value.value > 0.5f) {
      return kSbKeyGamepadDPadDown;
    } else {
      return kSbKeyGamepadDPadUp;
    }
  } else {
    SB_NOTREACHED();
    return kSbKeyUnknown;
  }
}

// Analyzes old axis values and new axis values and fire off any synthesized
// key press/unpress events as necessary.
void PossiblySynthesizeHatKeyEvents(HatAxis axis,
                                    float old_value,
                                    float new_value,
                                    SbWindow window,
                                    AInputEvent* android_event,
                                    Events* events) {
  if (old_value == new_value) {
    // No events to generate if the hat motion value did not change.
    return;
  }

  if (old_value > 0.5f || old_value < -0.5f) {
    PushKeyEvent(KeyForHatValue(HatValue({axis, old_value})),
                 kSbInputEventTypeUnpress, window, android_event, events);
  }
  if (new_value > 0.5f || new_value < -0.5f) {
    PushKeyEvent(KeyForHatValue(HatValue({axis, new_value})),
                 kSbInputEventTypePress, window, android_event, events);
  }
}

}  // namespace

bool InputEventsGenerator::ProcessKeyEvent(AInputEvent* android_event,
                                           Events* events) {
  if (!input_events_filter_.ShouldProcessKeyEvent(android_event)) {
    return false;
  }

  SbInputEventType type;
  switch (AKeyEvent_getAction(android_event)) {
    case AKEY_EVENT_ACTION_DOWN:
      type = kSbInputEventTypePress;
      break;
    case AKEY_EVENT_ACTION_UP:
      type = kSbInputEventTypeUnpress;
      break;
    default:
      // TODO: send multiple events for AKEY_EVENT_ACTION_MULTIPLE
      return false;
  }

  SbKey key = AInputEventToSbKey(android_event);
  if (key == kSbKeyUnknown) {
    return false;
  }

  if (AKeyEvent_getFlags(android_event) & AKEY_EVENT_FLAG_FALLBACK &&
      IsDPadKey(key)) {
    // For fallback DPad keys, we flow into special processing to manage the
    // differentiation between the actual DPad and the left thumbstick, since
    // Android conflates the key down/up events for these inputs.
    ProcessFallbackDPadEvent(type, key, android_event, events);
  } else {
    PushKeyEvent(key, type, window_, android_event, events);
  }
  return true;
}

namespace {

SbKey ButtonStateToSbKey(int32_t button_state) {
  if (button_state & AMOTION_EVENT_BUTTON_PRIMARY) {
    return kSbKeyMouse1;
  } else if (button_state & AMOTION_EVENT_BUTTON_SECONDARY) {
    return kSbKeyMouse2;
  } else if (button_state & AMOTION_EVENT_BUTTON_TERTIARY) {
    return kSbKeyMouse3;
  } else if (button_state & AMOTION_EVENT_BUTTON_BACK) {
    return kSbKeyBrowserBack;
  } else if (button_state & AMOTION_EVENT_BUTTON_FORWARD) {
    return kSbKeyBrowserForward;
  }
  return kSbKeyUnknown;
}

// Get an SbKeyModifiers from a button state
unsigned int ButtonStateToSbModifiers(unsigned int button_state) {
  unsigned int key_modifiers = kSbKeyModifiersNone;
#if SB_API_VERSION >= 6
  if (button_state & AMOTION_EVENT_BUTTON_PRIMARY) {
    key_modifiers |= kSbKeyModifiersPointerButtonLeft;
  }
  if (button_state & AMOTION_EVENT_BUTTON_SECONDARY) {
    key_modifiers |= kSbKeyModifiersPointerButtonMiddle;
  }
  if (button_state & AMOTION_EVENT_BUTTON_TERTIARY) {
    key_modifiers |= kSbKeyModifiersPointerButtonRight;
  }
  if (button_state & AMOTION_EVENT_BUTTON_BACK) {
    key_modifiers |= kSbKeyModifiersPointerButtonBack;
  }
  if (button_state & AMOTION_EVENT_BUTTON_FORWARD) {
    key_modifiers |= kSbKeyModifiersPointerButtonForward;
  }
#endif
  return key_modifiers;
}

#if SB_API_VERSION < 6
SbKey ScrollAxisToKey(float hscroll, float vscroll) {
  if (vscroll != 0) {
    return vscroll < 0 ? kSbKeyDown : kSbKeyUp;
  } else if (hscroll != 0) {
    return hscroll > 0 ? kSbKeyLeft : kSbKeyRight;
  }
  return kSbKeyUnknown;
}
#endif

}  // namespace

bool InputEventsGenerator::ProcessPointerEvent(AInputEvent* android_event,
                                               Events* events) {
  float offset_x =
      AMotionEvent_getAxisValue(android_event, AMOTION_EVENT_AXIS_X, 0);
  float offset_y =
      AMotionEvent_getAxisValue(android_event, AMOTION_EVENT_AXIS_Y, 0);

  std::unique_ptr<SbInputData> data(new SbInputData());
  SbMemorySet(data.get(), 0, sizeof(*data));

  data->window = window_;
  SB_DCHECK(SbWindowIsValid(data->window));
#if SB_API_VERSION >= 6
  data->pressure = NAN;
  data->size = {NAN, NAN};
  data->tilt = {NAN, NAN};
#endif
  unsigned int button_state = AMotionEvent_getButtonState(android_event);
  unsigned int button_modifiers = ButtonStateToSbModifiers(button_state);

  // Default to reporting pointer events as mouse events.
  data->device_type = kSbInputDeviceTypeMouse;

  // Report both stylus and touchscreen events as touchscreen device events.
  int32_t event_source = AInputEvent_getSource(android_event);
  if (((event_source & AINPUT_SOURCE_TOUCHSCREEN) != 0) ||
      ((event_source & AINPUT_SOURCE_STYLUS) != 0)) {
    data->device_type = kSbInputDeviceTypeTouchScreen;
  }

  data->device_id = AInputEvent_getDeviceId(android_event);
  data->key_modifiers =
      button_modifiers | AInputEventToSbModifiers(android_event);
  data->position.x = offset_x;
  data->position.y = offset_y;
  data->key = ButtonStateToSbKey(button_state);

  switch (AKeyEvent_getAction(android_event) & AMOTION_EVENT_ACTION_MASK) {
    case AMOTION_EVENT_ACTION_UP:
      data->type = kSbInputEventTypeUnpress;
      break;
    case AMOTION_EVENT_ACTION_DOWN:
      data->type = kSbInputEventTypePress;
      break;
    case AMOTION_EVENT_ACTION_MOVE:
    case AMOTION_EVENT_ACTION_HOVER_MOVE:
      data->type = kSbInputEventTypeMove;
      break;
    case AMOTION_EVENT_ACTION_SCROLL: {
      float hscroll = AMotionEvent_getAxisValue(
          android_event, AMOTION_EVENT_AXIS_HSCROLL, 0);  // left is -1
      float vscroll = AMotionEvent_getAxisValue(
          android_event, AMOTION_EVENT_AXIS_VSCROLL, 0);  // down is -1
      float wheel =
          AMotionEvent_getAxisValue(android_event, AMOTION_EVENT_AXIS_WHEEL, 0);
#if SB_API_VERSION >= 6
      data->type = kSbInputEventTypeWheel;
      data->key = kSbKeyUnknown;
      data->delta.y = -vscroll;
      data->delta.x = hscroll;
#else
      // This version of Starboard does not support wheel event types, send
      // keyboard event types instead.
      data->device_type = kSbInputDeviceTypeKeyboard;
      data->key = ScrollAxisToKey(hscroll, vscroll);

      std::unique_ptr<SbInputData> data_press(new SbInputData());
      SbMemoryCopy(data_press.get(), data.get(), sizeof(*data_press));

      // Send a press and unpress event.
      data_press->type = kSbInputEventTypePress;
      events->push_back(std::unique_ptr<Event>(
          new Application::Event(kSbEventTypeInput, data_press.release(),
                                 &Application::DeleteDestructor<SbInputData>)));

      data->type = kSbInputEventTypeUnpress;
#endif
      break;
    }
    default:
      return false;
  }

  events->push_back(std::unique_ptr<Event>(
      new Application::Event(kSbEventTypeInput, data.release(),
                             &Application::DeleteDestructor<SbInputData>)));
  return true;
}

bool InputEventsGenerator::ProcessMotionEvent(AInputEvent* android_event,
                                              Events* events) {
  int32_t event_source = AInputEvent_getSource(android_event);
  if ((event_source & AINPUT_SOURCE_CLASS_POINTER) != 0) {
    return ProcessPointerEvent(android_event, events);
  }
  if ((event_source & AINPUT_SOURCE_JOYSTICK) == 0) {
    // Only handles joystick events in the code below.
    return false;
  }

  UpdateDeviceFlatMapIfNecessary(android_event);
  ProcessJoyStickEvent(kLeftX, AMOTION_EVENT_AXIS_X, android_event, events);
  ProcessJoyStickEvent(kLeftY, AMOTION_EVENT_AXIS_Y, android_event, events);
  ProcessJoyStickEvent(kRightX, AMOTION_EVENT_AXIS_Z, android_event, events);
  ProcessJoyStickEvent(kRightY, AMOTION_EVENT_AXIS_RZ, android_event, events);

  // Remember the "hat" input values (dpad on the game controller) to help
  // differentiate hat vs. stick fallback events.
  UpdateHatValuesAndPossiblySynthesizeKeyEvents(android_event, events);

  // Lie to Android and tell it that we did not process the motion event,
  // causing Android to synthesize dpad key events for us. When we handle
  // those synthesized key events we'll enqueue kSbKeyGamepadLeft rather
  // than kSbKeyGamepadDPad events if they're from the joystick.
  return false;
}

// Special processing to disambiguate between DPad events and left-thumbstick
// direction key events.
void InputEventsGenerator::ProcessFallbackDPadEvent(SbInputEventType type,
                                                    SbKey key,
                                                    AInputEvent* android_event,
                                                    Events* events) {
  SB_DCHECK(AKeyEvent_getFlags(android_event) & AKEY_EVENT_FLAG_FALLBACK);
  SB_DCHECK(IsDPadKey(key));

  HatAxis hat_axis = HatValueForDPadKey(key).axis;

  if (hat_value_[hat_axis] != 0.0f && type == kSbInputEventTypePress) {
    // Direction pad events are all assumed to be coming from the hat controls
    // if motion events for that hat DPAD is active, but we do still handle
    // repeat keys here.
    if (AKeyEvent_getRepeatCount(android_event) > 0) {
      SB_LOG(INFO) << AKeyEvent_getRepeatCount(android_event);
      PushKeyEvent(key, kSbInputEventTypePress, window_, android_event, events);
    }
    return;
  }

  // If we get this far, then we are exclusively dealing with thumbstick events,
  // as actual DPad events are processed in motion events by checking the
  // hat axis representing the DPad.
  SbKey thumbstick_key = ConvertDPadKeyToThumbstickKey(key);

  if (left_thumbstick_key_pressed_[hat_axis] != kSbKeyUnknown &&
      (type == kSbInputEventTypeUnpress ||
       left_thumbstick_key_pressed_[hat_axis] != thumbstick_key)) {
    // Fire an unpressed event if our current key differs from the last seen
    // key.
    PushKeyEvent(left_thumbstick_key_pressed_[hat_axis],
                 kSbInputEventTypeUnpress, window_, android_event, events);
  }

  if (type == kSbInputEventTypePress) {
    PushKeyEvent(thumbstick_key, kSbInputEventTypePress, window_,
                 android_event, events);
    left_thumbstick_key_pressed_[hat_axis] = thumbstick_key;
  } else if (type == kSbInputEventTypeUnpress) {
    left_thumbstick_key_pressed_[hat_axis] = kSbKeyUnknown;
  } else {
    SB_NOTREACHED();
  }
}

// Update |InputEventsGenerator::hat_value_| according to the incoming motion
// event's data.  Possibly generate DPad events based on any changes in value
// here.
void InputEventsGenerator::UpdateHatValuesAndPossiblySynthesizeKeyEvents(
    AInputEvent* android_event,
    Events* events) {
  float new_hat_x =
      AMotionEvent_getAxisValue(android_event, AMOTION_EVENT_AXIS_HAT_X, 0);
  PossiblySynthesizeHatKeyEvents(kHatX, hat_value_[kHatX], new_hat_x, window_,
                                 android_event, events);
  hat_value_[kHatX] = new_hat_x;

  float new_hat_y =
      AMotionEvent_getAxisValue(android_event, AMOTION_EVENT_AXIS_HAT_Y, 0);
  PossiblySynthesizeHatKeyEvents(kHatY, hat_value_[kHatY], new_hat_y, window_,
                                 android_event, events);
  hat_value_[kHatY] = new_hat_y;
}

void InputEventsGenerator::UpdateDeviceFlatMapIfNecessary(
    AInputEvent* android_event) {
  int32_t device_id = AInputEvent_getDeviceId(android_event);
  if (device_flat_.find(device_id) != device_flat_.end()) {
    // |device_flat_| is already contains the device flat information.
    return;
  }

  JniEnvExt* env = JniEnvExt::Get();
  ScopedLocalJavaRef<jobject> input_device(env->CallStaticObjectMethodOrAbort(
      "android/view/InputDevice", "getDevice", "(I)Landroid/view/InputDevice;",
      device_id));
  float flats[kNumAxes] = {GetFlat(input_device.Get(), AMOTION_EVENT_AXIS_X),
                           GetFlat(input_device.Get(), AMOTION_EVENT_AXIS_Y),
                           GetFlat(input_device.Get(), AMOTION_EVENT_AXIS_Z),
                           GetFlat(input_device.Get(), AMOTION_EVENT_AXIS_RZ)};
  device_flat_[device_id] = std::vector<float>(flats, flats + kNumAxes);
}

bool InputEventsGenerator::CreateInputEventsFromAndroidEvent(
    AInputEvent* android_event,
    Events* events) {
  if (android_event == NULL ||
      (AInputEvent_getType(android_event) != AINPUT_EVENT_TYPE_KEY &&
       AInputEvent_getType(android_event) != AINPUT_EVENT_TYPE_MOTION)) {
    return false;
  }

  switch (AInputEvent_getType(android_event)) {
    case AINPUT_EVENT_TYPE_KEY:
      return ProcessKeyEvent(android_event, events);
    case AINPUT_EVENT_TYPE_MOTION: {
      return ProcessMotionEvent(android_event, events);
    }
    default:
      SB_NOTREACHED();
  }

  return false;
}

void InputEventsGenerator::CreateInputEventsFromSbKey(SbKey key,
                                                      Events* events) {
  events->clear();

  // Press event
  std::unique_ptr<SbInputData> data(new SbInputData());
  SbMemorySet(data.get(), 0, sizeof(*data));

  data->window = window_;
  data->type = kSbInputEventTypePress;

  data->device_type = kSbInputDeviceTypeKeyboard;
  data->device_id = 0;

  data->key = key;
  data->key_location = kSbKeyLocationUnspecified;
  data->key_modifiers = kSbKeyModifiersNone;

  events->push_back(std::unique_ptr<Event>(
      new Application::Event(kSbEventTypeInput, data.release(),
                             &Application::DeleteDestructor<SbInputData>)));

  // Unpress event
  data.reset(new SbInputData());
  SbMemorySet(data.get(), 0, sizeof(*data));

  data->window = window_;
  data->type = kSbInputEventTypeUnpress;

  data->device_type = kSbInputDeviceTypeKeyboard;
  data->device_id = 0;

  data->key = key;
  data->key_location = kSbKeyLocationUnspecified;
  data->key_modifiers = kSbKeyModifiersNone;

  events->push_back(std::unique_ptr<Event>(
      new Application::Event(kSbEventTypeInput, data.release(),
                             &Application::DeleteDestructor<SbInputData>)));
}

}  // namespace shared
}  // namespace android
}  // namespace starboard
