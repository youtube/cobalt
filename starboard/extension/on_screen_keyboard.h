// Copyright 2023 The Cobalt Authors. All Rights Reserved.
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

#ifndef STARBOARD_EXTENSION_ON_SCREEN_KEYBOARD_H_
#define STARBOARD_EXTENSION_ON_SCREEN_KEYBOARD_H_

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

  // This function overrides the background color of on-screen keyboard in RGB
  // color space, where r, g, b are between 0 and 255.
  void (*SetBackgroundColor)(SbWindow window, uint8_t r, uint8_t g, uint8_t b);

  // This function overrides the light theme of on-screen keyboard.
  void (*SetLightTheme)(SbWindow window, bool light_theme);
} CobaltExtensionOnScreenKeyboardApi;

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // STARBOARD_EXTENSION_ON_SCREEN_KEYBOARD_H_
