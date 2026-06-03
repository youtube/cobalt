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

#ifndef STARBOARD_TVOS_SHARED_MEDIA_DRM_MANAGER_H_
#define STARBOARD_TVOS_SHARED_MEDIA_DRM_MANAGER_H_

#import <Foundation/Foundation.h>

#include "starboard/drm.h"

@class SBDApplicationDrmSystem;

/**
 *  @brief A manager of @c SBDApplicationDrmSystem instances.
 */
@interface SBDDrmManager : NSObject

/**
 *  @brief Creates an application DRM system with the given parameters.
 *  @param context The context to be passed with the callback functions.
 *  @param updateRequestFunc The function to be called when SPC data has been
 *      retrieved from the OS.
 *  @param updatedFunc The function to call when the OS has used the CKC
 *      data to decrypt a video.
 */
- (SBDApplicationDrmSystem*)
        drmSystemWithContext:(void*)context
    sessionUpdateRequestFunc:(SbDrmSessionUpdateRequestFunc)updateRequestFunc
          sessionUpdatedFunc:(SbDrmSessionUpdatedFunc)updatedFunc;

/**
 *  @brief Destroys a DRM system.
 *  @param applicationDrmSystem The application DRM system to destroy.
 */
- (void)destroyDrmSystem:(SBDApplicationDrmSystem*)applicationDrmSystem;

/**
 *  @brief Returns true if the drm system is an application DRM system.
 */
- (bool)isApplicationDrmSystem:(SbDrmSystem)drmSystem;

/**
 *  @brief Return the application DRM system referenced by the given
 *      SbDrmSystem.
 */
- (SBDApplicationDrmSystem*)applicationDrmSystemForStarboardDrmSystem:
    (SbDrmSystem)starboardDrmSystem;

/**
 *  @brief Return the starboard DRM system referenced by the given
 *      SBDApplicationDrmSystem.
 */
- (SbDrmSystem)starboardDrmSystemForApplicationDrmSystem:
    (SBDApplicationDrmSystem*)applicationDrmSystem;

/**
 *  @brief Designated initializer
 */
- (instancetype)init;

@end

#endif  // STARBOARD_TVOS_SHARED_MEDIA_DRM_MANAGER_H_
