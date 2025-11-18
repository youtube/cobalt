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

#ifndef STARBOARD_TVOS_SHARED_APPLICATION_VIEW_H_
#define STARBOARD_TVOS_SHARED_APPLICATION_VIEW_H_

#import <UIKit/UIKit.h>

@class SBDKeyboardInputDevice;

NS_ASSUME_NONNULL_BEGIN

/**
 *  @brief Represents a viewport for the starboard application.
 */
@interface SBDApplicationView : UIView

/**
 *  @brief The @c UIView that contains the rendered interface as well as any
 *      interactable views.
 */
@property(nonatomic, readonly) UIView* interfaceContainer;

/**
 *  @brief The @c UIView that all player views should be added to.
 */
@property(nonatomic, readonly) UIView* playerContainer;

/**
 *  @brief The @c UIView that all search controllers should be added to.
 */
@property(nonatomic, readonly) UIView* searchContainer;

- (instancetype)init NS_UNAVAILABLE;

/**
 *  @brief Designated Initializer.
 */
- (instancetype)initWithFrame:(CGRect)frame;

/**
 *  @brief Use this to force focus on the default focus view. Normally this
 *    is not focusable if UI navigation is in use, but this can be used to take
 *    focus away from UI navigation. Returns the default focus view.
 */
- (UIView*)forceDefaultFocus:(BOOL)forceFocus;

@end

NS_ASSUME_NONNULL_END

#endif  // STARBOARD_TVOS_SHARED_APPLICATION_VIEW_H_
