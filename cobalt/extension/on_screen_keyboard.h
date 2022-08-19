// Copyright 2022 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_EXTENSION_ON_SCREEN_KEYBOARD_H_
#define COBALT_EXTENSION_ON_SCREEN_KEYBOARD_H_

#include <stdint.h>

#include "starboard/system.h"
#include "starboard/window.h"

#ifdef __cplusplus
extern "C" {
#endif

#define kCobaltExtensionOnScreenKeyboardName \
  "dev.cobalt.extension.OnScreenKeyboard"

typedef struct CobaltExtensionOnScreenKeyboardApi {
  // Name should be the string
  // |kCobaltExtensionOnScreenKeyboardName|. This helps to validate that
  // the extension API is correct.
  const char* name;

  // This specifies the version of the API that is implemented.
  uint32_t version;

  // The fields below this point were added in version 1 or later.

  // This function overrides the background color of on-screen keyboard.
  // The function is only enabled for Apple TV at the moment.
  // background_color should be a string of below formats:
  // 1. hex color notation #RRGGBB, e.g. "#0000FF".
  // 2. rgb color notation rgb(R, G, B), e.g. "rgb(0, 0, 255)".
  void (*SetBackgroundColor)(SbWindow window, const char* background_color);

  // This function overrides the dark theme of on-screen keyboard.
  // The function is only enabled for Apple TV at the moment.
  void (*SetDarkTheme)(SbWindow window, bool dark_theme);
} CobaltExtensionOnScreenKeyboardApi;

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // COBALT_EXTENSION_ON_SCREEN_KEYBOARD_H_
