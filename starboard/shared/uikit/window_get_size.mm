// Copyright 2017 The Cobalt Authors. All Rights Reserved.
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

#import <Foundation/Foundation.h>

#include "starboard/window.h"

#import "starboard/shared/uikit/application_window.h"
#import "starboard/shared/uikit/starboard_application.h"
#import "starboard/shared/uikit/window_manager.h"

bool SbWindowGetSize(SbWindow window, SbWindowSize* size) {
  if (window == kSbWindowInvalid) {
    return false;
  }

  @autoreleasepool {
    SBDWindowManager* windowManager = SBDGetApplication().windowManager;
    SBDApplicationWindow* applicationWindow =
        [windowManager applicationWindowForStarboardWindow:window];

    if (applicationWindow) {
      *size = applicationWindow.size;
      return true;
    }
    return false;
  }
}
