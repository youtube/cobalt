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

#import "starboard/tvos/shared/window_manager.h"

#import <objc/runtime.h>

#include "starboard/common/log.h"
#import "starboard/tvos/shared/application_window.h"
#import "starboard/tvos/shared/defines.h"

@implementation SBDWindowManager {
  // Only one application window will be used. It will persist across suspend
  // and resume so that any subwindows attached (e.g. onscreen keyboard)
  // persist as expected.
  SBDApplicationWindow* _window;
  int _windowUseCount;
}

- (instancetype)init {
  self = [super init];
  if (self) {
    _windowUseCount = 0;
  }
  return self;
}

- (CGSize)screenSize {
  CGFloat scale = UIScreen.mainScreen.scale;
  CGSize size = UIScreen.mainScreen.bounds.size;
  return CGSizeMake(size.width * scale, size.height * scale);
}

- (CGFloat)screenScale {
  return UIScreen.mainScreen.scale;
}

- (SBDApplicationWindow*)currentApplicationWindow {
  if (_windowUseCount > 0) {
    return _window;
  }
  return nil;
}

- (SBDApplicationWindow*)applicationWindowForStarboardWindow:(SbWindow)window {
  if (window == kSbWindowInvalid) {
    return nil;
  }
  return (__bridge SBDApplicationWindow*)window;
}

- (SbWindow)starboardWindowForApplicationWindow:(SBDApplicationWindow*)window {
  if (window == nil) {
    return kSbWindowInvalid;
  }
  return (__bridge SbWindow)window;
}

- (SBDApplicationWindow*)windowWithName:(nullable NSString*)name
                                   size:(SbWindowSize)size
                               windowed:(BOOL)windowed {
  __block SBDApplicationWindow* applicationWindow;
  onApplicationMainThread(^{
    // Share one application window that is persisted across suspend and resume.
    if (!self->_window) {
      self->_window = [[SBDApplicationWindow alloc] initWithName:name];
      [self->_window makeKeyAndVisible];
    } else {  // _windowUseCount should not ever be larger than 1 in the current
              // user cases.
      if (self->_windowUseCount == 0) {
        [self->_window updateWindowSize];
      }
    }
    ++self->_windowUseCount;
    applicationWindow = self->_window;
  });

  return applicationWindow;
}

- (void)destroyWindow:(SBDApplicationWindow*)applicationWindow {
  dispatch_async(dispatch_get_main_queue(), ^{
    if (applicationWindow == self->_window) {
      SB_DCHECK(self->_windowUseCount > 0);
      --self->_windowUseCount;
    }
  });
}

@end
