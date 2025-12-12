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

#import "starboard/tvos/shared/application_window.h"
#import "starboard/tvos/shared/starboard_application.h"
#import "starboard/tvos/shared/window_manager.h"
#include "starboard/window.h"

SbWindow SbWindowCreate(const SbWindowOptions* options) {
  SbWindowOptions default_options;
  if (options == nullptr) {
    SbWindowSetDefaultOptions(&default_options);
    options = &default_options;
  }

  @autoreleasepool {
    SBDWindowManager* windowManager = SBDGetApplication().windowManager;

    NSString* name;
    if (options->name) {
      name = [[NSString alloc] initWithCString:options->name
                                      encoding:NSUTF8StringEncoding];
    }

    SBDApplicationWindow* window =
        [windowManager windowWithName:name
                                 size:options->size
                             windowed:options->windowed];

    return [windowManager starboardWindowForApplicationWindow:window];
  }
}
