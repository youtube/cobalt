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

#ifndef STARBOARD_TVOS_SHARED_WINDOW_MANAGER_H_
#define STARBOARD_TVOS_SHARED_WINDOW_MANAGER_H_

#import <Foundation/Foundation.h>

#import "starboard/tvos/shared/application_window.h"

@protocol SBDStarboardApplication;

NS_ASSUME_NONNULL_BEGIN

/**
 *  @brief Maintains a collection of windows created via the Starboard
 *      interface.
 *  @remarks This class should only be accessed from the main thread.
 */
@interface SBDWindowManager : NSObject

/** Returns the resolution of the display. */
@property(nonatomic, readonly) CGSize screenSize;

/** Returns the points-to-pixels scale used by the display. */
@property(nonatomic, readonly) CGFloat screenScale;

/**
 *  @brief Returns the current application window.
 */
@property(nonatomic, readonly, nullable)
    SBDApplicationWindow* currentApplicationWindow;

/**
 *  @brief Creates a platform window with the given parameters.
 *  @param name The name of the window.
 *  @param size The size of the window.
 *  @param windowed If the window should be presented non-fullscreen.
 */
- (SBDApplicationWindow*)windowWithName:(nullable NSString*)name
                                   size:(SbWindowSize)size
                               windowed:(BOOL)windowed;

/**
 *  @brief Destroys a window.
 *  @param window The platform window to destroy.
 */
- (void)destroyWindow:(SBDApplicationWindow*)applicationWindow;

/**
 *  @brief Returns the application window for a Starboard window.
 *  @param window The Starboard window.
 */
- (SBDApplicationWindow*)applicationWindowForStarboardWindow:(SbWindow)window;

/**
 *  @brief Returns the Starboard window for an application window.
 *  @param window The application window whose handle we'd like.
 */
- (SbWindow)starboardWindowForApplicationWindow:(SBDApplicationWindow*)window;

/**
 *  @brief Designated initializer
 */
- (instancetype)init;

@end

NS_ASSUME_NONNULL_END

#endif  // STARBOARD_TVOS_SHARED_WINDOW_MANAGER_H_
