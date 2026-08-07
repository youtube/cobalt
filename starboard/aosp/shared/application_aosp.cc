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
#include <android/native_window.h>

#include "starboard/aosp/shared/key_map.h"
#include "starboard/aosp/shared/window_internal.h"
#include "starboard/aosp/shared/window_surface.h"
#include "starboard/common/log.h"
#include "starboard/event.h"
#include "starboard/input.h"
#include "starboard/key.h"
#include "starboard/window.h"

namespace starboard {

namespace {

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
  SbKey sb_key = AndroidKeyCodeToSbKey(key_code);
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
  data->character = static_cast<wchar_t>(unicode_char);
  data->key_modifiers = MetaStateToSbKeyModifiers(meta_state);
  Inject(new Event(kSbEventTypeInput, data,
                   &ApplicationAOSP::DeleteDestructor<SbInputData>));
  // The volume keys are reported to the app but not consumed, so Android
  // still changes the volume and shows its own indicator.
  return !SystemHandlesKeyCode(key_code);
}

}  // namespace starboard
