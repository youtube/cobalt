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

#import "starboard/tvos/shared/input_device.h"

#include "starboard/common/log.h"
#import "starboard/tvos/shared/starboard_application.h"
#import "starboard/tvos/shared/window_manager.h"

/**
 *  @brief Used to give devices a unique identifier.
 */
static NSInteger gNextDeviceID = 1;

@implementation SBDInputDevice

- (instancetype)init {
  self = [super init];
  if (self) {
    @synchronized([self class]) {
      _deviceID = gNextDeviceID;
      gNextDeviceID++;
    }
  }
  return self;
}

- (SbWindow)currentWindowHandle {
  // TODO: Input devices should be attached to a SbWindow rather than just
  // using the current window.
  SBDWindowManager* windowManager = SBDGetApplication().windowManager;
  SBDApplicationWindow* applicationWindow =
      windowManager.currentApplicationWindow;
  SbWindow starboardWindow =
      [windowManager starboardWindowForApplicationWindow:applicationWindow];
  return starboardWindow;
}

- (void)disconnect {
}

@end
