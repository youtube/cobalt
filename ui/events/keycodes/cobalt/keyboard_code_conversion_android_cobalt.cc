// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
#include "ui/events/keycodes/keyboard_code_conversion_android.h"
// clang-format on

#define KeyboardCodeFromAndroidKeyCode \
  KeyboardCodeFromAndroidKeyCode_ChromiumImpl
#include "ui/events/keycodes/keyboard_code_conversion_android.cc"
#undef KeyboardCodeFromAndroidKeyCode

#include <android/keycodes.h>

namespace ui {

KeyboardCode KeyboardCodeFromAndroidKeyCode(int keycode) {
  switch (keycode) {
    // Cobalt keycode mappings
<<<<<<< HEAD
    case AKEYCODE_CAPTIONS:
      return KEY_SUBTITLES;
=======
>>>>>>> b870fd7deb (ui/events/keycodes: remapping cobalt-specific keycode values (#9067))
    case AKEYCODE_SEARCH:
      return VKEY_BROWSER_SEARCH;
    case AKEYCODE_BACK:
      return VKEY_ESCAPE;
    case AKEYCODE_MEDIA_PLAY:
      return VKEY_PLAY;
    case AKEYCODE_MEDIA_PAUSE:
      return VKEY_PAUSE;

    case AKEYCODE_PROG_RED:
      return VKEY_RED;
    case AKEYCODE_PROG_GREEN:
      return VKEY_GREEN;
    case AKEYCODE_PROG_YELLOW:
      return VKEY_YELLOW;
    case AKEYCODE_PROG_BLUE:
      return VKEY_BLUE;

    case AKEYCODE_CHANNEL_UP:
      return VKEY_CHANNEL_UP;
    case AKEYCODE_CHANNEL_DOWN:
      return VKEY_CHANNEL_DOWN;
    case AKEYCODE_CAPTIONS:
      return VKEY_SUBTITLES;
    case AKEYCODE_INFO:
      return VKEY_INFO;
    case AKEYCODE_GUIDE:
      return VKEY_GUIDE;
    case AKEYCODE_MEDIA_AUDIO_TRACK:
      return VKEY_MEDIA_AUDIO_TRACK;
    // End of Cobalt keycode mappings
    default:
      return KeyboardCodeFromAndroidKeyCode_ChromiumImpl(keycode);
  }
}

}  // namespace ui
