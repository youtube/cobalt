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

#ifndef STARBOARD_TVOS_SHARED_STARBOARD_APPLICATION_H_
#define STARBOARD_TVOS_SHARED_STARBOARD_APPLICATION_H_

#import <Foundation/Foundation.h>

#include "starboard/system.h"

@class SBDDrmManager;
@class SBDEglAdapter;
@class SBDPlayerManager;
@class UIPress;
@class UIPressesEvent;
@class UIView;
@protocol SBDStarboardApplication;

NS_ASSUME_NONNULL_BEGIN

#ifdef __cplusplus
extern "C" {
#endif

/**
 *  @memberof SBDStarboardApplication-p
 *  @brief Returns the global @c SBDStarboardApplication instance.
 */
id<SBDStarboardApplication> SBDGetApplication(void);

#ifdef __cplusplus
}  // extern "C"
#endif

/**
 *  @brief A Starboard application.
 */
@protocol SBDStarboardApplication

/**
 *  @brief Enables Starboard to manage DRM playback.
 */
@property(nonatomic, readonly) SBDDrmManager* drmManager;

/**
 *  @brief Enables Starboard to manage platform players.
 */
@property(nonatomic, readonly) SBDPlayerManager* playerManager;

// Suspends the application by forwarding the press events stored by calls to
// `registerMenuPressBegan` and `registerMenuPressEnded` to UIKit.
- (void)suspendApplication;

// Registers the menu press events got on pressesBegan to use them at the
// application suspension.
- (void)registerMenuPressBegan:(UIPress*)press
                  pressesEvent:(UIPressesEvent*)pressesEvent;

// Registers the menu press events got on pressesEnded to use them at the
// application suspension.
- (void)registerMenuPressEnded:(UIPress*)press
                  pressesEvent:(UIPressesEvent*)pressesEvent;
@end

NS_ASSUME_NONNULL_END

#endif  // STARBOARD_TVOS_SHARED_STARBOARD_APPLICATION_H_
