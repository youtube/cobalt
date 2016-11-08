// Copyright (c) 2016, NVIDIA CORPORATION.  All rights reserved.
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

#include "third_party/starboard/android/shared/application_android.h"
#include "third_party/starboard/android/shared/window_internal.h"

#include <stdlib.h>
#include <unistd.h>
#include <EGL/egl.h>
#include <android/log.h>

#include "starboard/event.h"
#include "starboard/log.h"
#include "starboard/input.h"
#include "starboard/time.h"
#include "third_party/starboard/android/shared/window_internal.h"
#include "starboard/shared/starboard/audio_sink/audio_sink_internal.h"
#include "starboard/memory.h"

#define LOGI(...) ((void)__android_log_print(ANDROID_LOG_INFO, "native-activity", __VA_ARGS__))

extern starboard::Queue<AInputEvent *> sharedEventQueue;

namespace starboard {
namespace android {
namespace shared {

const int kKeyboardDeviceId = 1;
const int kGamepadDeviceId = 2;

void ApplicationAndroid::SetExternalFilesDir(const char *dir) {
    external_files_dir_ = dir;
}

const char * ApplicationAndroid::GetExternalFilesDir() {
    return external_files_dir_;
}

void ApplicationAndroid::SetExternalCacheDir(const char *dir) {
    external_cache_dir_ = dir;
}

const char * ApplicationAndroid::GetExternalCacheDir() {
    return external_cache_dir_;
}

SbWindow ApplicationAndroid::CreateWindow(const SbWindowOptions* options) {
  LOGI("CreateWindow\n");
  window_ = new SbWindowPrivate(hwindow_, options);
  return window_;
}

void ApplicationAndroid::SetWindowHandle(ANativeWindow *handle) {
    hwindow_ = handle;
}

void ApplicationAndroid::SetVideoWindowHandle(ANativeWindow *handle) {
    vwindow_ = handle;
}

SbWindow ApplicationAndroid::GetWindow() {
    return window_;
}

ANativeWindow* ApplicationAndroid::GetWindowHandle() {
    return hwindow_;
}

ANativeWindow* ApplicationAndroid::GetVideoWindowHandle() {
    return vwindow_;
}

void ApplicationAndroid::SetMessageFDs(int readFD, int writeFD) {
  msg_read_fd = readFD;
  msg_write_fd = writeFD;
}

int ApplicationAndroid::GetMessageReadFD() {
  return msg_read_fd;
}

int ApplicationAndroid::GetMessageWriteFD() {
  return msg_write_fd;
}

bool ApplicationAndroid::DestroyWindow(SbWindow window) {
  LOGI("DestroyWindow\n");
  if(!SbWindowIsValid(window)) {
    return false;
  }
  delete window_;
  window_ = kSbWindowInvalid;
  return true;
}

void ApplicationAndroid::Initialize() {
  SbAudioSinkPrivate::Initialize();
  LOGI("Initialize\n");
}

void ApplicationAndroid::Teardown() {
  SbAudioSinkPrivate::TearDown();
  const unsigned int message = 1;
  if (write(GetMessageWriteFD(), &message, sizeof(message)) != sizeof(message)) {
    LOGI("Failure writing android_app redraw: %s\n", strerror(errno));
  }

  LOGI("Teardown\n");
}

bool ApplicationAndroid::MayHaveSystemEvents() {
  return true;
}

::starboard::shared::starboard::Application::Event* ApplicationAndroid::PollNextSystemEvent() {
  //TODO: Also map system/window commands
  AInputEvent *event = sharedEventQueue.Poll();
  return CreateEventFromAndroidEvent(event);
}

::starboard::shared::starboard::Application::Event* ApplicationAndroid::CreateEventFromAndroidEvent(AInputEvent *event) {
  if(event != NULL) {
    SbInputData *data = new SbInputData();
    SbMemorySet(data, 0, sizeof(*data));

    data->window = window_;
    if(!SbWindowIsValid(data->window))
      return NULL;

    int16_t keycode = AKeyEvent_getKeyCode(event);
    data->key = KeyCodeToSbKey(keycode);
    if(data->key == kSbKeyUnknown) {
      delete data;
      return NULL;
    }
    data->key_location = KeyCodeToSbKeyLocation(keycode);

    int32_t action = AKeyEvent_getAction(event);
    switch(action) {
      case AKEY_EVENT_ACTION_DOWN:
        data->type = kSbInputEventTypePress;
        break;
      case AKEY_EVENT_ACTION_UP:
        data->type = kSbInputEventTypeUnpress;
        break;
      default:
        delete data;
        return NULL;
    }

    //TODO: Accept inputs other than keyboard/gamepad
    int32_t source = AInputEvent_getSource(event);
    data->device_type = kSbInputDeviceTypeKeyboard;
    data->device_id = kKeyboardDeviceId;

    int32_t meta = AKeyEvent_getMetaState(event);
    data->key_modifiers = MetaStateToModifiers(meta);

    //TODO: Set data->char (map to unicode) ?
    LOGI("Received event with: akey = %d; Sending event with: type=%d, key=%d, location=%d, modifiers=%d\n", keycode, data->type, data->key, data->key_location, data->key_modifiers);
    return new Event(kSbEventTypeInput, data, &DeleteDestructor<SbInputData>);
  }
  return NULL;
}

::starboard::shared::starboard::Application::Event*
ApplicationAndroid::WaitForSystemEventWithTimeout(SbTime time) {
  return CreateEventFromAndroidEvent(sharedEventQueue.GetTimed(time));
}

void ApplicationAndroid::WakeSystemEventWait() {
  sharedEventQueue.Wake();
}

SbKeyLocation ApplicationAndroid::KeyCodeToSbKeyLocation(int16_t code) {
    switch(code) {
      case AKEYCODE_ALT_LEFT:
      case AKEYCODE_SHIFT_LEFT:
      case AKEYCODE_CTRL_LEFT:
      case AKEYCODE_META_LEFT:
        return kSbKeyLocationLeft;
      case AKEYCODE_ALT_RIGHT:
      case AKEYCODE_SHIFT_RIGHT:
      case AKEYCODE_CTRL_RIGHT:
      case AKEYCODE_META_RIGHT:
        return kSbKeyLocationRight;
    }
    return kSbKeyLocationUnspecified;
}

unsigned int ApplicationAndroid::MetaStateToModifiers(int32_t state) {
    unsigned int modifiers = kSbKeyModifiersNone;
    if(state & AMETA_ALT_ON)
      modifiers |= kSbKeyModifiersAlt;
    if(state & AMETA_CTRL_ON)
      modifiers |= kSbKeyModifiersCtrl;
    if(state & AMETA_META_ON)
      modifiers |= kSbKeyModifiersMeta;
    if(state & AMETA_SHIFT_ON)
      modifiers |= kSbKeyModifiersShift;
    return modifiers;
}

// Converts an AKeyCode code into an SbKey.
SbKey ApplicationAndroid::KeyCodeToSbKey(int16_t code) {
  switch (code) {
    case AKEYCODE_UNKNOWN:
      return kSbKeyUnknown;

    case AKEYCODE_SOFT_LEFT:
      return kSbKeyUnknown;
    case AKEYCODE_SOFT_RIGHT:
      return kSbKeyUnknown;

    case AKEYCODE_HOME:
      return kSbKeyHome;

    // We need this to be ESC, as using BACK _seems_ to be correct, but never allows exit (ESC!=BACK in that case)
    // B will fall back to BACK, which we will map to ESC here
    case AKEYCODE_BACK:
      return kSbKeyEscape;

    case AKEYCODE_CALL:
      return kSbKeyUnknown;
    case AKEYCODE_ENDCALL:
      return kSbKeyUnknown;

    case AKEYCODE_0:
      return kSbKey0;
    case AKEYCODE_1:
      return kSbKey1;
    case AKEYCODE_2:
      return kSbKey2;
    case AKEYCODE_3:
      return kSbKey3;
    case AKEYCODE_4:
      return kSbKey4;
    case AKEYCODE_5:
      return kSbKey5;
    case AKEYCODE_6:
      return kSbKey6;
    case AKEYCODE_7:
      return kSbKey7;
    case AKEYCODE_8:
      return kSbKey8;
    case AKEYCODE_9:
      return kSbKey9;

    case AKEYCODE_STAR:
      return kSbKeyUnknown;
    case AKEYCODE_POUND:
      return kSbKeyUnknown;

    case AKEYCODE_DPAD_UP:
      return kSbKeyGamepadDPadUp;
    case AKEYCODE_DPAD_DOWN:
      return kSbKeyGamepadDPadDown;
    case AKEYCODE_DPAD_LEFT:
      return kSbKeyGamepadDPadLeft;
    case AKEYCODE_DPAD_RIGHT:
      return kSbKeyGamepadDPadRight;
    case AKEYCODE_DPAD_CENTER:
      return kSbKeyUnknown;

    case AKEYCODE_VOLUME_UP:
      return kSbKeyVolumeUp;
    case AKEYCODE_VOLUME_DOWN:
      return kSbKeyVolumeDown;

    case AKEYCODE_POWER:
      return kSbKeyPower;

    case AKEYCODE_CAMERA:
      return kSbKeyUnknown;

    case AKEYCODE_CLEAR:
      return kSbKeyClear;

    case AKEYCODE_A:
      return kSbKeyA;
    case AKEYCODE_B:
      return kSbKeyB;
    case AKEYCODE_C:
      return kSbKeyC;
    case AKEYCODE_D:
      return kSbKeyD;
    case AKEYCODE_E:
      return kSbKeyE;
    case AKEYCODE_F:
      return kSbKeyF;
    case AKEYCODE_G:
      return kSbKeyG;
    case AKEYCODE_H:
      return kSbKeyH;
    case AKEYCODE_I:
      return kSbKeyI;
    case AKEYCODE_J:
      return kSbKeyJ;
    case AKEYCODE_K:
      return kSbKeyK;
    case AKEYCODE_L:
      return kSbKeyL;
    case AKEYCODE_M:
      return kSbKeyM;
    case AKEYCODE_N:
      return kSbKeyN;
    case AKEYCODE_O:
      return kSbKeyO;
    case AKEYCODE_P:
      return kSbKeyP;
    case AKEYCODE_Q:
      return kSbKeyQ;
    case AKEYCODE_R:
      return kSbKeyR;
    case AKEYCODE_S:
      return kSbKeyS;
    case AKEYCODE_T:
      return kSbKeyT;
    case AKEYCODE_U:
      return kSbKeyU;
    case AKEYCODE_V:
      return kSbKeyV;
    case AKEYCODE_W:
      return kSbKeyW;
    case AKEYCODE_X:
      return kSbKeyX;
    case AKEYCODE_Y:
      return kSbKeyY;
    case AKEYCODE_Z:
      return kSbKeyZ;

    case AKEYCODE_COMMA:
      return kSbKeyOemComma;
    case AKEYCODE_PERIOD:
      return kSbKeyOemPeriod;

    case AKEYCODE_ALT_LEFT:
    case AKEYCODE_ALT_RIGHT:
      return kSbKeyMenu;

    case AKEYCODE_SHIFT_LEFT:
    case AKEYCODE_SHIFT_RIGHT:
      return kSbKeyShift;
    case AKEYCODE_TAB:
      return kSbKeyTab;
    case AKEYCODE_SPACE:
      return kSbKeySpace;

    case AKEYCODE_SYM:
      return kSbKeyUnknown;
    case AKEYCODE_EXPLORER:
      return kSbKeyUnknown;
    case AKEYCODE_ENVELOPE:
      return kSbKeyUnknown;

    case AKEYCODE_ENTER:
      return kSbKeyReturn;

    // WAR: Backspace sends AKEYCODE_DEL
    case AKEYCODE_DEL:
      return kSbKeyBack;

    case AKEYCODE_GRAVE:
      return kSbKeyOem3;

    case AKEYCODE_MINUS:
      return kSbKeyOemMinus;
    case AKEYCODE_EQUALS:
      return kSbKeyOemPlus;

    case AKEYCODE_LEFT_BRACKET:
      return kSbKeyOem4;
    case AKEYCODE_RIGHT_BRACKET:
      return kSbKeyOem6;
    case AKEYCODE_BACKSLASH:
      return kSbKeyOem5;
    case AKEYCODE_SEMICOLON:
      return kSbKeyOem1;
    case AKEYCODE_APOSTROPHE:
      return kSbKeyOem7;

    case AKEYCODE_SLASH:
      return kSbKeyDivide;
    case AKEYCODE_AT:
      return kSbKeyUnknown;
    case AKEYCODE_NUM:
      return kSbKeyNumlock;
    case AKEYCODE_HEADSETHOOK:
      return kSbKeyUnknown;
    case AKEYCODE_FOCUS:
      return kSbKeyUnknown;
    case AKEYCODE_PLUS:
      return kSbKeyOemPlus;

    case AKEYCODE_MENU:
      return kSbKeyMenu;

    case AKEYCODE_NOTIFICATION:
      return kSbKeyUnknown;
    case AKEYCODE_SEARCH:
      return kSbKeyBrowserSearch;

    case AKEYCODE_MEDIA_PLAY_PAUSE:
      return kSbKeyMediaPlayPause;
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

    case AKEYCODE_MUTE:
      return kSbKeyVolumeMute;

    case AKEYCODE_PAGE_UP:
      return kSbKeyPrior;
    case AKEYCODE_PAGE_DOWN:
      return kSbKeyNext;
    case AKEYCODE_PICTSYMBOLS:
      return kSbKeyUnknown;
    case AKEYCODE_SWITCH_CHARSET:
      return kSbKeyUnknown;

    //TODO: Double-check these
    case AKEYCODE_BUTTON_A:
      return kSbKeyReturn;
    case AKEYCODE_BUTTON_B:
      return kSbKeyBack;
    case AKEYCODE_BUTTON_C:
      return kSbKeyUnknown;
    case AKEYCODE_BUTTON_X:
      return kSbKeyGamepad3;
    case AKEYCODE_BUTTON_Y:
      return kSbKeyGamepad4;
    case AKEYCODE_BUTTON_Z:
      return kSbKeyUnknown;
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
      return kSbKeyUnknown;

    case AKEYCODE_ESCAPE:
      return kSbKeyEscape;
    case AKEYCODE_FORWARD_DEL:
      return kSbKeyUnknown;
    case AKEYCODE_CTRL_LEFT:
    case AKEYCODE_CTRL_RIGHT:
      return kSbKeyControl;
    case AKEYCODE_CAPS_LOCK:
      return kSbKeyCapital;
    case AKEYCODE_SCROLL_LOCK:
      return kSbKeyScroll;
    case AKEYCODE_META_LEFT:
    case AKEYCODE_META_RIGHT:
      return kSbKeyMenu;
    case AKEYCODE_FUNCTION:
      return kSbKeyUnknown;
    case AKEYCODE_SYSRQ:
      return kSbKeyUnknown;
    case AKEYCODE_BREAK:
      return kSbKeyUnknown;
    case AKEYCODE_MOVE_HOME:
      return kSbKeyUnknown;
    case AKEYCODE_MOVE_END:
      return kSbKeyUnknown;
    case AKEYCODE_INSERT:
      return kSbKeyInsert;
    case AKEYCODE_FORWARD:
      return kSbKeyBrowserForward;

    case AKEYCODE_MEDIA_PLAY:
      return kSbKeyMediaPlayPause;
    case AKEYCODE_MEDIA_PAUSE:
      return kSbKeyMediaPlayPause;
    case AKEYCODE_MEDIA_CLOSE:
      return kSbKeyUnknown;
    case AKEYCODE_MEDIA_EJECT:
      return kSbKeyUnknown;
    case AKEYCODE_MEDIA_RECORD:
      return kSbKeyUnknown;

    case AKEYCODE_F1:
      return kSbKeyF1;
    case AKEYCODE_F2:
      return kSbKeyF2;
    case AKEYCODE_F3:
      return kSbKeyF3;
    case AKEYCODE_F4:
      return kSbKeyF4;
    case AKEYCODE_F5:
      return kSbKeyF5;
    case AKEYCODE_F6:
      return kSbKeyF6;
    case AKEYCODE_F7:
      return kSbKeyF7;
    case AKEYCODE_F8:
      return kSbKeyF8;
    case AKEYCODE_F9:
      return kSbKeyF9;
    case AKEYCODE_F10:
      return kSbKeyF10;
    case AKEYCODE_F11:
      return kSbKeyF11;
    case AKEYCODE_F12:
      return kSbKeyF12;

    case AKEYCODE_NUM_LOCK:
      return kSbKeyNumlock;
    case AKEYCODE_NUMPAD_0:
      return kSbKeyNumpad0;
    case AKEYCODE_NUMPAD_1:
      return kSbKeyNumpad1;
    case AKEYCODE_NUMPAD_2:
      return kSbKeyNumpad2;
    case AKEYCODE_NUMPAD_3:
      return kSbKeyNumpad3;
    case AKEYCODE_NUMPAD_4:
      return kSbKeyNumpad4;
    case AKEYCODE_NUMPAD_5:
      return kSbKeyNumpad5;
    case AKEYCODE_NUMPAD_6:
      return kSbKeyNumpad6;
    case AKEYCODE_NUMPAD_7:
      return kSbKeyNumpad7;
    case AKEYCODE_NUMPAD_8:
      return kSbKeyNumpad8;
    case AKEYCODE_NUMPAD_9:
      return kSbKeyNumpad9;
    case AKEYCODE_NUMPAD_DIVIDE:
      return kSbKeyDivide;
    case AKEYCODE_NUMPAD_MULTIPLY:
      return kSbKeyMultiply;
    case AKEYCODE_NUMPAD_SUBTRACT:
      return kSbKeyOemMinus;
    case AKEYCODE_NUMPAD_ADD:
      return kSbKeyOemPlus;
    case AKEYCODE_NUMPAD_DOT:
      return kSbKeyOemPeriod;
    case AKEYCODE_NUMPAD_COMMA:
      return kSbKeyOemComma;
    case AKEYCODE_NUMPAD_ENTER:
      return kSbKeyReturn;
    case AKEYCODE_NUMPAD_EQUALS:
      return kSbKeyOemPlus;
    case AKEYCODE_NUMPAD_LEFT_PAREN:
      return kSbKeyUnknown;
    case AKEYCODE_NUMPAD_RIGHT_PAREN:
      return kSbKeyUnknown;

    case AKEYCODE_VOLUME_MUTE:
      return kSbKeyVolumeMute;
    case AKEYCODE_INFO:
      return kSbKeyUnknown;
    case AKEYCODE_CHANNEL_UP:
      return kSbKeyUnknown;
    case AKEYCODE_CHANNEL_DOWN:
      return kSbKeyUnknown;
    case AKEYCODE_ZOOM_IN:
      return kSbKeyUnknown;
    case AKEYCODE_ZOOM_OUT:
      return kSbKeyUnknown;
    case AKEYCODE_TV:
      return kSbKeyUnknown;
    case AKEYCODE_WINDOW:
      return kSbKeyUnknown;
    case AKEYCODE_GUIDE:
      return kSbKeyUnknown;
    case AKEYCODE_DVR:
      return kSbKeyUnknown;
    case AKEYCODE_BOOKMARK:
      return kSbKeyUnknown;
    case AKEYCODE_CAPTIONS:
      return kSbKeyUnknown;
    case AKEYCODE_SETTINGS:
      return kSbKeyUnknown;
    case AKEYCODE_TV_POWER:
      return kSbKeyUnknown;
    case AKEYCODE_TV_INPUT:
      return kSbKeyUnknown;
    case AKEYCODE_STB_POWER:
      return kSbKeyUnknown;
    case AKEYCODE_STB_INPUT:
      return kSbKeyUnknown;
    case AKEYCODE_AVR_POWER:
      return kSbKeyUnknown;
    case AKEYCODE_AVR_INPUT:
      return kSbKeyUnknown;
    case AKEYCODE_PROG_RED:
      return kSbKeyUnknown;
    case AKEYCODE_PROG_GREEN:
      return kSbKeyUnknown;
    case AKEYCODE_PROG_YELLOW:
      return kSbKeyUnknown;
    case AKEYCODE_PROG_BLUE:
      return kSbKeyUnknown;
    case AKEYCODE_APP_SWITCH:
      return kSbKeyUnknown;
    case AKEYCODE_BUTTON_1:
      return kSbKeyUnknown;
    case AKEYCODE_BUTTON_2:
      return kSbKeyUnknown;
    case AKEYCODE_BUTTON_3:
      return kSbKeyUnknown;
    case AKEYCODE_BUTTON_4:
      return kSbKeyUnknown;
    case AKEYCODE_BUTTON_5:
      return kSbKeyUnknown;
    case AKEYCODE_BUTTON_6:
      return kSbKeyUnknown;
    case AKEYCODE_BUTTON_7:
      return kSbKeyUnknown;
    case AKEYCODE_BUTTON_8:
      return kSbKeyUnknown;
    case AKEYCODE_BUTTON_9:
      return kSbKeyUnknown;
    case AKEYCODE_BUTTON_10:
      return kSbKeyUnknown;
    case AKEYCODE_BUTTON_11:
      return kSbKeyUnknown;
    case AKEYCODE_BUTTON_12:
      return kSbKeyUnknown;
    case AKEYCODE_BUTTON_13:
      return kSbKeyUnknown;
    case AKEYCODE_BUTTON_14:
      return kSbKeyUnknown;
    case AKEYCODE_BUTTON_15:
      return kSbKeyUnknown;
    case AKEYCODE_BUTTON_16:
      return kSbKeyUnknown;

    case AKEYCODE_LANGUAGE_SWITCH:
      return kSbKeyUnknown;
    case AKEYCODE_MANNER_MODE:
      return kSbKeyUnknown;
    case AKEYCODE_3D_MODE:
      return kSbKeyUnknown;
    case AKEYCODE_CONTACTS:
      return kSbKeyUnknown;
    case AKEYCODE_CALENDAR:
      return kSbKeyUnknown;
    case AKEYCODE_MUSIC:
      return kSbKeyUnknown;

    case AKEYCODE_CALCULATOR:
      return kSbKeyMediaLaunchApp2;

    case AKEYCODE_ZENKAKU_HANKAKU:
      return kSbKeyDbeDbcschar;
    case AKEYCODE_EISU:
      return kSbKeyUnknown;
    case AKEYCODE_MUHENKAN:
      return kSbKeyNonconvert;
    case AKEYCODE_HENKAN:
      return kSbKeyConvert;
    case AKEYCODE_KATAKANA_HIRAGANA:
      return kSbKeyKana;
    case AKEYCODE_YEN:
      return kSbKeyUnknown;
    case AKEYCODE_RO:
      return kSbKeyUnknown;
    case AKEYCODE_KANA:
      return kSbKeyKana;

    case AKEYCODE_ASSIST:
      return kSbKeyUnknown;
    case AKEYCODE_BRIGHTNESS_DOWN:
      return kSbKeyBrightnessDown;
    case AKEYCODE_BRIGHTNESS_UP:
      return kSbKeyBrightnessUp;
    case AKEYCODE_MEDIA_AUDIO_TRACK:
      return kSbKeyUnknown;

    case AKEYCODE_SLEEP:
      return kSbKeyUnknown;
    case AKEYCODE_WAKEUP:
      return kSbKeyUnknown;
    case AKEYCODE_PAIRING:
      return kSbKeyUnknown;
    case AKEYCODE_MEDIA_TOP_MENU:
      return kSbKeyUnknown;
    case AKEYCODE_11:
      return kSbKeyUnknown;
    case AKEYCODE_12:
      return kSbKeyUnknown;
    case AKEYCODE_LAST_CHANNEL:
      return kSbKeyUnknown;
    case AKEYCODE_TV_DATA_SERVICE:
      return kSbKeyUnknown;
    case AKEYCODE_VOICE_ASSIST:
      return kSbKeyUnknown;
    case AKEYCODE_TV_RADIO_SERVICE:
      return kSbKeyUnknown;
    case AKEYCODE_TV_TELETEXT:
      return kSbKeyUnknown;
    case AKEYCODE_TV_NUMBER_ENTRY:
      return kSbKeyUnknown;
    case AKEYCODE_TV_TERRESTRIAL_ANALOG:
      return kSbKeyUnknown;
    case AKEYCODE_TV_TERRESTRIAL_DIGITAL:
      return kSbKeyUnknown;
    case AKEYCODE_TV_SATELLITE:
      return kSbKeyUnknown;
    case AKEYCODE_TV_SATELLITE_BS:
      return kSbKeyUnknown;
    case AKEYCODE_TV_SATELLITE_CS:
      return kSbKeyUnknown;
    case AKEYCODE_TV_SATELLITE_SERVICE:
      return kSbKeyUnknown;
    case AKEYCODE_TV_NETWORK:
      return kSbKeyUnknown;
    case AKEYCODE_TV_ANTENNA_CABLE:
      return kSbKeyUnknown;
    case AKEYCODE_TV_INPUT_HDMI_1:
      return kSbKeyUnknown;
    case AKEYCODE_TV_INPUT_HDMI_2:
      return kSbKeyUnknown;
    case AKEYCODE_TV_INPUT_HDMI_3:
      return kSbKeyUnknown;
    case AKEYCODE_TV_INPUT_HDMI_4:
      return kSbKeyUnknown;
    case AKEYCODE_TV_INPUT_COMPOSITE_1:
      return kSbKeyUnknown;
    case AKEYCODE_TV_INPUT_COMPOSITE_2:
      return kSbKeyUnknown;
    case AKEYCODE_TV_INPUT_COMPONENT_1:
      return kSbKeyUnknown;
    case AKEYCODE_TV_INPUT_COMPONENT_2:
      return kSbKeyUnknown;
    case AKEYCODE_TV_INPUT_VGA_1:
      return kSbKeyUnknown;
    case AKEYCODE_TV_AUDIO_DESCRIPTION:
      return kSbKeyUnknown;
    case AKEYCODE_TV_AUDIO_DESCRIPTION_MIX_UP:
      return kSbKeyUnknown;
    case AKEYCODE_TV_AUDIO_DESCRIPTION_MIX_DOWN:
      return kSbKeyUnknown;
    case AKEYCODE_TV_ZOOM_MODE:
      return kSbKeyUnknown;
    case AKEYCODE_TV_CONTENTS_MENU:
      return kSbKeyUnknown;
    case AKEYCODE_TV_MEDIA_CONTEXT_MENU:
      return kSbKeyUnknown;
    case AKEYCODE_TV_TIMER_PROGRAMMING:
      return kSbKeyUnknown;
    case AKEYCODE_HELP:
      return kSbKeyHelp;
  }
  LOGI("Unknown code: 0x%x\n", code);
  return kSbKeyUnknown;
}  // NOLINT(readability/fn_size)

}  // namespace android
}  // namespace starboard
}  // namespace shared
