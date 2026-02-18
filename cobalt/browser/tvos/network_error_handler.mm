// Copyright 2026 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/browser/tvos/network_error_handler.h"

#import <UIKit/UIKit.h>

#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/web_contents.h"

@implementation NetworkErrorHandler

+ (void)ShowNetworkErrorDialog:(content::WebContents*)web_contents {
  @autoreleasepool {
    NSString* title = NSLocalizedStringWithDefaultValue(
        @"NoNetworkDialogTitle", nil, NSBundle.mainBundle,
        @"No Network Detected", @"The title \"No network connection\" dialog.");
    NSString* message = NSLocalizedStringWithDefaultValue(
        @"NoNetworkDialogMessage", nil, NSBundle.mainBundle,
        @"Check your Internet connection.",
        @"The body \"No network connection\" dialog.");
    NSString* retry_button = NSLocalizedStringWithDefaultValue(
        @"NoNetworkDialogRetry", nil, NSBundle.mainBundle, @"Retry",
        @"Retry button");
    NSString* exit_button = NSLocalizedStringWithDefaultValue(
        @"NoNetworkDialogExit", nil, NSBundle.mainBundle, @"Exit",
        @"Exit button");

    UIAlertController* alert = [UIAlertController
        alertControllerWithTitle:title
                         message:message
                  preferredStyle:UIAlertControllerStyleAlert];

    UIAlertAction* retryAction = [UIAlertAction
        actionWithTitle:retry_button
                  style:UIAlertActionStyleDefault
                handler:^(UIAlertAction* action) {
                  web_contents->GetController().Reload(
                      content::ReloadType::NORMAL, /*check_for_repost*/ false);
                }];
    [alert addAction:retryAction];

    UIAlertAction* exitAction =
        [UIAlertAction actionWithTitle:exit_button
                                 style:UIAlertActionStyleDefault
                               handler:^(UIAlertAction* action) {
                                 exit(/*error_level*/ 0);
                               }];
    [alert addAction:exitAction];

    // Present the error dialogue. Do not send UIPresses to the client since
    // they are meant for the error dialogue.
    [UIApplication.sharedApplication.keyWindow.rootViewController
        presentViewController:alert
                     animated:YES
                   completion:nil];
  }
}

@end

void ShowPlatformErrorDialog(content::WebContents* web_contents) {
  [NetworkErrorHandler ShowNetworkErrorDialog:web_contents];
}
