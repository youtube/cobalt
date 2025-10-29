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

#import "localized_strings.h"

@implementation SBDLocalizedStrings

+ (NSString*)retry {
  return NSLocalizedStringWithDefaultValue(@"Retry", nil, NSBundle.mainBundle,
                                           @"Retry",
                                           @"The text of the retry button.");
}

+ (NSString*)exit {
  return NSLocalizedStringWithDefaultValue(@"Exit", nil, NSBundle.mainBundle,
                                           @"Exit",
                                           @"The text of the exit button.");
}

+ (NSString*)noNetworkDialogTitle {
  return NSLocalizedStringWithDefaultValue(
      @"NoNetworkDialogTitle", nil, NSBundle.mainBundle, @"No Network Detected",
      @"The title \"No network connection\" dialog.");
}

+ (NSString*)noNetworkDialogMessage {
  return NSLocalizedStringWithDefaultValue(
      @"NoNetworkDialogMessage", nil, NSBundle.mainBundle,
      @"Check your Internet connection.",
      @"The body \"No network connection\" dialog.");
}

@end
