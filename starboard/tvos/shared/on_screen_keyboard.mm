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

#include "starboard/extension/on_screen_keyboard.h"

#import <Foundation/Foundation.h>

#include "starboard/common/log.h"
#import "starboard/tvos/shared/application_window.h"
#include "starboard/tvos/shared/on_screen_keyboard.h"
#import "starboard/tvos/shared/starboard_application.h"
#import "starboard/tvos/shared/window_manager.h"
#include "starboard/window.h"

namespace starboard {
namespace shared {
namespace uikit {

namespace {
void SetBackgroundColor(SbWindow window, uint8_t r, uint8_t g, uint8_t b) {
  @autoreleasepool {
    SBDWindowManager* windowManager = SBDGetApplication().windowManager;
    SBDApplicationWindow* applicationWindow =
        [windowManager applicationWindowForStarboardWindow:window];

    [applicationWindow setOnScreenKeyboardBackgroundColorWithRed:r / 255.0
                                                           green:g / 255.0
                                                            blue:b / 255.0
                                                           alpha:1];
  }
}

void SetLightTheme(SbWindow window, bool light_theme) {
  @autoreleasepool {
    SBDWindowManager* windowManager = SBDGetApplication().windowManager;
    SBDApplicationWindow* applicationWindow =
        [windowManager applicationWindowForStarboardWindow:window];
    [applicationWindow setOnScreenKeyboardLightTheme:light_theme];
  }
}

void Show(SbWindow window, const char* input_text, int ticket) {
  @autoreleasepool {
    SBDWindowManager* windowManager = SBDGetApplication().windowManager;
    SBDApplicationWindow* applicationWindow =
        [windowManager applicationWindowForStarboardWindow:window];

    NSString* searchText = [NSString stringWithCString:input_text
                                              encoding:NSUTF8StringEncoding];
    [applicationWindow showOnScreenKeyboardWithText:searchText ticket:ticket];
  }
}

void Hide(SbWindow window, int ticket) {
  @autoreleasepool {
    SBDWindowManager* windowManager = SBDGetApplication().windowManager;
    SBDApplicationWindow* applicationWindow =
        [windowManager applicationWindowForStarboardWindow:window];
    [applicationWindow hideOnScreenKeyboardWithTicket:ticket];
  }
}

void Focus(SbWindow window, int ticket) {
  @autoreleasepool {
    SBDWindowManager* windowManager = SBDGetApplication().windowManager;
    SBDApplicationWindow* applicationWindow =
        [windowManager applicationWindowForStarboardWindow:window];
    [applicationWindow focusOnScreenKeyboardWithTicket:ticket];
  }
}

void Blur(SbWindow window, int ticket) {
  @autoreleasepool {
    SBDWindowManager* windowManager = SBDGetApplication().windowManager;
    SBDApplicationWindow* applicationWindow =
        [windowManager applicationWindowForStarboardWindow:window];
    [applicationWindow blurOnScreenKeyboardWithTicket:ticket];
  }
}

void UpdateSuggestions(SbWindow window,
                       const char* suggestions[],
                       int num_suggestions,
                       int ticket) {}

bool IsShown(SbWindow window) {
  @autoreleasepool {
    SBDWindowManager* windowManager = SBDGetApplication().windowManager;
    SBDApplicationWindow* applicationWindow =
        [windowManager applicationWindowForStarboardWindow:window];
    return applicationWindow.keyboardShowing;
  }
}

bool SuggestionsSupported(SbWindow window) {
  return false;
}

bool GetBoundingRect(SbWindow window, SbWindowRect* bounding_rect) {
  SB_DCHECK(bounding_rect);
  if (window == kSbWindowInvalid) {
    return false;
  }
  @autoreleasepool {
    __block bool returnValue;
    dispatch_sync(dispatch_get_main_queue(), ^{
      SBDWindowManager* windowManager = SBDGetApplication().windowManager;
      SBDApplicationWindow* applicationWindow =
          [windowManager applicationWindowForStarboardWindow:window];

      if (!applicationWindow) {
        returnValue = false;
      } else {
        *bounding_rect = [applicationWindow getOnScreenKeyboardFrame];
        returnValue = bounding_rect->width != 0 && bounding_rect->height != 0;
      }
    });
    return returnValue;
  }
}

void SetKeepFocus(SbWindow window, bool keep_focus) {
  @autoreleasepool {
    SBDWindowManager* windowManager = SBDGetApplication().windowManager;
    SBDApplicationWindow* applicationWindow =
        [windowManager applicationWindowForStarboardWindow:window];
    [applicationWindow onScreenKeyboardKeepFocus:keep_focus];
  }
}

const CobaltExtensionOnScreenKeyboardApi kGetOnScreenKeyboardApi = {
    kCobaltExtensionOnScreenKeyboardName,
    1,
    &SetBackgroundColor,
    &SetLightTheme,
    &Show,
    &Hide,
    &Focus,
    &Blur,
    &UpdateSuggestions,
    &IsShown,
    &SuggestionsSupported,
    &GetBoundingRect,
    &SetKeepFocus};
}  // namespace

const void* GetOnScreenKeyboardApi() {
  return &kGetOnScreenKeyboardApi;
}

}  // namespace uikit
}  // namespace shared
}  // namespace starboard
