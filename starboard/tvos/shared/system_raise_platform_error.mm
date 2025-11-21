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

#import <UIKit/UIKit.h>

#include "starboard/system.h"
#import "starboard/tvos/shared/defines.h"
#import "starboard/tvos/shared/localized_strings.h"
#import "starboard/tvos/shared/starboard_application.h"
#import "starboard/tvos/shared/window_manager.h"

bool SbSystemRaisePlatformError(SbSystemPlatformErrorType type,
                                SbSystemPlatformErrorCallback callback,
                                void* user_data) {
  @autoreleasepool {
    NSString* title = SBDLocalizedStrings.noNetworkDialogTitle;
    NSString* message = SBDLocalizedStrings.noNetworkDialogMessage;

    // UI operations must happen on the main thread.
    onApplicationMainThread(^{
      UIAlertController* alert = [UIAlertController
          alertControllerWithTitle:title
                           message:message
                    preferredStyle:UIAlertControllerStyleAlert];

      UIAlertAction* retryAction = [UIAlertAction
          actionWithTitle:SBDLocalizedStrings.retry
                    style:UIAlertActionStyleDefault
                  handler:^(UIAlertAction* action) {
                    SBDWindowManager* windowManager =
                        SBDGetApplication().windowManager;
                    windowManager.currentApplicationWindow.processUIPresses =
                        YES;
                    callback(kSbSystemPlatformErrorResponsePositive, user_data);
                  }];
      [alert addAction:retryAction];

      UIAlertAction* exitAction = [UIAlertAction
          actionWithTitle:SBDLocalizedStrings.exit
                    style:UIAlertActionStyleDefault
                  handler:^(UIAlertAction* action) {
                    SBDWindowManager* windowManager =
                        SBDGetApplication().windowManager;
                    windowManager.currentApplicationWindow.processUIPresses =
                        YES;
                    callback(kSbSystemPlatformErrorResponseCancel, user_data);
                  }];
      [alert addAction:exitAction];

      // Present the error dialogue. Do not send UIPresses to the client since
      // they are meant for the error dialogue.
      SBDWindowManager* windowManager = SBDGetApplication().windowManager;
      windowManager.currentApplicationWindow.processUIPresses = NO;
      [UIApplication.sharedApplication.keyWindow.rootViewController
          presentViewController:alert
                       animated:YES
                     completion:nil];
    });
    return true;
  }
}
