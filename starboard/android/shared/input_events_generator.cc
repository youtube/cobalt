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

#include "starboard/input.h"
#include "starboard/key.h"

namespace starboard {
namespace android {
namespace shared {

using ::starboard::shared::starboard::Application;

namespace {

const int kKeyboardDeviceId = 1;

SbKey AInputEventToSbKey(AInputEvent *event) {
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
    case AKEYCODE_SEARCH:
      return kSbKeyBrowserSearch;
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

    // Volume
    case AKEYCODE_VOLUME_UP:
      return kSbKeyVolumeUp;
    case AKEYCODE_VOLUME_DOWN:
      return kSbKeyVolumeDown;
    case AKEYCODE_MUTE:
      return kSbKeyVolumeMute;

    // Brightness
    case AKEYCODE_BRIGHTNESS_DOWN:
      return kSbKeyBrightnessDown;
    case AKEYCODE_BRIGHTNESS_UP:
      return kSbKeyBrightnessUp;

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

    default:
      return kSbKeyUnknown;
  }
}

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

}  // namespace

Application::Event*
CreateInputEvent(AInputEvent *event, SbWindow window) {
  if (event == NULL
      || AInputEvent_getType(event) != AINPUT_EVENT_TYPE_KEY
      || !SbWindowIsValid(window)) {
    return NULL;
  }

  SbInputData* data = new SbInputData();
  SbMemorySet(data, 0, sizeof(*data));

  // window
  data->window = window;

  // type
  switch (AKeyEvent_getAction(event)) {
    case AKEY_EVENT_ACTION_DOWN:
      data->type = kSbInputEventTypePress;
      break;
    case AKEY_EVENT_ACTION_UP:
      data->type = kSbInputEventTypeUnpress;
      break;
    default:
      // TODO: send multiple events for AKEY_EVENT_ACTION_MULTIPLE
      delete data;
      return NULL;
  }

  // device
  // TODO: differentiate gamepad, remote, etc.
  data->device_type = kSbInputDeviceTypeKeyboard;
  data->device_id = kKeyboardDeviceId;

  // key
  data->key = AInputEventToSbKey(event);
  data->key_location = AInputEventToSbKeyLocation(event);
  data->key_modifiers = AInputEventToSbModifiers(event);

  return new Application::Event(
      kSbEventTypeInput, data, &Application::DeleteDestructor<SbInputData>);
}

}  // namespace shared
}  // namespace android
}  // namespace starboard
