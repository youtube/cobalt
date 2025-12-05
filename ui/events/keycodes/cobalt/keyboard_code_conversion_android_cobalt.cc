// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#define KeyboardCodeFromAndroidKeyCode \
  KeyboardCodeFromAndroidKeyCode_ChromiumImpl
#include "ui/events/keycodes/keyboard_code_conversion_android.cc"
#undef KeyboardCodeFromAndroidKeyCode

#include <android/keycodes.h>

#include "ui/events/keycodes/keyboard_code_conversion_android.h"

namespace ui {

KeyboardCode KeyboardCodeFromAndroidKeyCode(int keycode) {
  switch (keycode) {
    // Cobalt keycode mappings
    case AKEYCODE_CAPTIONS:
      return VKEY_SUBTITLES;
    case AKEYCODE_SEARCH:
      return VKEY_BROWSER_SEARCH;
    case AKEYCODE_BACK:
      return VKEY_ESCAPE;
    // End of Cobalt keycode mappings
    default:
      return KeyboardCodeFromAndroidKeyCode_ChromiumImpl(keycode);
  }
}

}  // namespace ui
