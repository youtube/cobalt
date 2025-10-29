// Copyright 2018 The Cobalt Authors. All Rights Reserved.
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

#ifndef INTERNAL_STARBOARD_SHARED_UIKIT_SEARCH_CONTROLLER_FOCUS_DELEGATE_H_
#define INTERNAL_STARBOARD_SHARED_UIKIT_SEARCH_CONTROLLER_FOCUS_DELEGATE_H_

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

/**
 *  @brief Handles updating the application's focus in regards to search
 *      controller events.
 */
@protocol SBDSearchControllerFocusDelegate

/**
 *  @brief Notifies the delegate that the application's focus should be moved
 *      to the search controller.
 */
- (void)searchShouldFocus;

/**
 *  @brief Notifies the delegate that focus moved to the search controller.
 */
- (void)searchDidFocus;

/**
 *  @brief Notifies the delegate that the application's focus should be moved
 *      off of the search controller.
 */
- (void)searchShouldBlur;

/**
 *  @brief Notifies the delegate that focus moved off the search controller.
 */
- (void)searchDidBlur;

/**
 *  @brief Notifies the delegate that the search controller was shown.
 */
- (void)searchDidShow;

/**
 *  @brief Notifies the delegate that the search controller was hidden.
 */
- (void)searchDidHide;

@end

NS_ASSUME_NONNULL_END

#endif  // INTERNAL_STARBOARD_SHARED_UIKIT_SEARCH_CONTROLLER_FOCUS_DELEGATE_H_
