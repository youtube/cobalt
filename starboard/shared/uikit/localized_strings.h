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

#ifndef INTERNAL_STARBOARD_SHARED_UIKIT_LOCALIZED_STRINGS_H_
#define INTERNAL_STARBOARD_SHARED_UIKIT_LOCALIZED_STRINGS_H_

#import <Foundation/Foundation.h>

/**
 *  @brief Localized strings.
 */
@interface SBDLocalizedStrings : NSObject

/**
 *  @brief The text of the retry button.
 */
@property(class, nonatomic, readonly) NSString* retry;

/**
 *  @brief The text of the exit button.
 */
@property(class, nonatomic, readonly) NSString* exit;

/**
 *  @brief The title "No network connection" dialog.
 */
@property(class, nonatomic, readonly) NSString* noNetworkDialogTitle;

/**
 *  @brief The body "No network connection" dialog.
 */
@property(class, nonatomic, readonly) NSString* noNetworkDialogMessage;

@end

#endif  // INTERNAL_STARBOARD_SHARED_UIKIT_LOCALIZED_STRINGS_H_
