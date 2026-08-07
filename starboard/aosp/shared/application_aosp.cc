// Copyright 2026 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/aosp/shared/application_aosp.h"

#include <android/input.h>
#include <android/keycodes.h>
#include <android/native_window.h>

#include "starboard/aosp/shared/window_internal.h"
#include "starboard/aosp/shared/window_surface.h"
#include "starboard/common/log.h"
#include "starboard/event.h"
#include "starboard/input.h"
#include "starboard/key.h"
#include "starboard/window.h"

namespace starboard {

namespace {

SbKey MapAndroidKeyCodeToSbKey(int key_code) {
  switch (key_code) {
    case AKEYCODE_DPAD_UP:
      return kSbKeyUp;
    case AKEYCODE_DPAD_DOWN:
      return kSbKeyDown;
    case AKEYCODE_DPAD_LEFT:
      return kSbKeyLeft;
    case AKEYCODE_DPAD_RIGHT:
      return kSbKeyRight;
    case AKEYCODE_DPAD_CENTER:
    case AKEYCODE_ENTER:
    case AKEYCODE_NUMPAD_ENTER:
      return kSbKeyReturn;
    case AKEYCODE_ESCAPE:
    case AKEYCODE_BACK:
      // "back" maps to Escape (see starboard/key.h)
      return kSbKeyEscape;
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
    default:
      return kSbKeyUnknown;
  }
}

unsigned int MetaStateToSbKeyModifiers(int meta_state) {
  unsigned int modifiers = kSbKeyModifiersNone;
  if (meta_state & AMETA_ALT_ON) {
    modifiers |= kSbKeyModifiersAlt;
  }
  if (meta_state & AMETA_CTRL_ON) {
    modifiers |= kSbKeyModifiersCtrl;
  }
  if (meta_state & AMETA_META_ON) {
    modifiers |= kSbKeyModifiersMeta;
  }
  if (meta_state & AMETA_SHIFT_ON) {
    modifiers |= kSbKeyModifiersShift;
  }
  return modifiers;
}

}  // namespace

SbWindow ApplicationAOSP::CreateWindow(const SbWindowOptions* /*options*/) {
  // The window holds its own reference, so the ANativeWindow stays valid even
  // if the Android surface is destroyed before SbWindowDestroy() is called.
  ANativeWindow* native_window = android::shared::AcquireWindowSurface();
  if (native_window == nullptr) {
    SB_LOG(ERROR) << "SbWindowCreate: no Android surface available.";
    return kSbWindowInvalid;
  }
  SbWindow window = new SbWindowPrivate();
  window->native_window = native_window;
  window_ = window;
  return window;
}

bool ApplicationAOSP::DestroyWindow(SbWindow window) {
  if (!SbWindowIsValid(window)) {
    return false;
  }
  if (window_ == window) {
    window_ = kSbWindowInvalid;
  }
  ANativeWindow_release(window->native_window);
  delete window;
  return true;
}

bool ApplicationAOSP::InjectKeyEvent(int key_code,
                                     int action,
                                     int unicode_char,
                                     int meta_state) {
  SbKey sb_key = MapAndroidKeyCodeToSbKey(key_code);
  if (sb_key == kSbKeyUnknown) {
    return false;
  }

  SbInputData* data = new SbInputData();
  data->window = window_;
  data->device_type = kSbInputDeviceTypeRemote;
  // Android delivers repeats as additional ACTION_DOWN events; treat anything
  // that isn't an explicit ACTION_UP as a press.
  data->type = action != AKEY_EVENT_ACTION_UP ? kSbInputEventTypePress
                                              : kSbInputEventTypeUnpress;
  data->key = sb_key;
  // KeyEvent.getUnicodeChar() returns 0 for keys with no character, which is
  // also what SbInputData expects.
  data->character = static_cast<wchar_t>(unicode_char);
  data->key_modifiers = MetaStateToSbKeyModifiers(meta_state);
  Inject(new Event(kSbEventTypeInput, data,
                   &ApplicationAOSP::DeleteDestructor<SbInputData>));
  return true;
}

}  // namespace starboard
