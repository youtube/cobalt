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

#ifndef STARBOARD_TVOS_SHARED_SEARCH_RESULTS_VIEW_CONTROLLER_H_
#define STARBOARD_TVOS_SHARED_SEARCH_RESULTS_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

NS_ASSUME_NONNULL_BEGIN

/**
 *  @brief A view's delegate is made aware of focus state changes.
 */
@protocol SBDSearchResultsFocusDelegate <NSObject>

/**
 *  @brief Called when the search results receive focus.
 */
- (void)resultsDidReceiveFocus;

/**
 *  @brief Called when the search results lose focus.
 */
- (void)resultsDidLoseFocus;
@end

/**
 *  @brief The @c UIViewController for a UISearchController's search results.
 */
@interface SBDSearchResultsViewController : UIViewController

- (instancetype)init;

/**
 *  @brief The delegate that handles focus updates.
 */
@property(nonatomic, weak) id<SBDSearchResultsFocusDelegate> focusDelegate;

@end

NS_ASSUME_NONNULL_END

#endif  // STARBOARD_TVOS_SHARED_SEARCH_RESULTS_VIEW_CONTROLLER_H_
