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

#ifndef STARBOARD_TVOS_SHARED_MEDIA_APPLICATION_DRM_SYSTEM_H_
#define STARBOARD_TVOS_SHARED_MEDIA_APPLICATION_DRM_SYSTEM_H_

#import <Foundation/Foundation.h>

#include "starboard/drm.h"

@class AVContentKeyRequest;
@class AVContentKeySession;

NS_ASSUME_NONNULL_BEGIN

/**
 *  @brief An application DRM system.
 */
@interface SBDApplicationDrmSystem : NSObject

/**
 *  @brief The @c AVContentKeySession used to decrypt playback.
 */
@property(nonatomic) AVContentKeySession* keySession;

/**
 *  @brief Called when key data is received from Starboard.
 *  @param key The key data to pass on to the OS to decrypt playback.
 *  @param ticket The opaque ID used to distinguish between concurrent key
 *      decryptions.
 *  @param sessionId Used to identify the key request associated the given
 *      key response.
 */
- (void)updateSessionWithKey:(NSData*)key
                      ticket:(NSInteger)ticket
                   sessionId:(NSData*)sessionId;

/**
 *  @brief Generates the data payload to pass to the fairplay server.
 *  @param certificationData Data that represents the fairplay certificate.
 *  @param contentIdentifier The unique identifier for the asset that needs
 *      decryption.
 *  @param initData The identifier of the encrypted content.
 *      @remarks This is the "skd://" string.
 *  @param ticket The opaque ID used to distinguish between concurrent key
 *      decryptions.
 */
- (void)
    generateSessionUpdateRequestWithCertificationData:(NSData*)certificationData
                                    contentIdentifier:(NSData*)contentIdentifier
                                             initData:(NSData*)initData
                                               ticket:(NSInteger)ticket;

/**
 *  @brief Process the @c AVContentKeyRequest. Return whether the web app
 *      should be notified that encrypted media was encountered -- if the
 *      key request was generated in response to a prefetch, then an
 *      encrypted-media-encountered event should NOT be dispatched.
 *  @param keyRequest The key request to be processed/decrypted.
 */
- (BOOL)processKeyRequest:(AVContentKeyRequest*)keyRequest;

- (instancetype)init NS_UNAVAILABLE;

/**
 *  @brief Designated initializer.
 *  @param context Parameter to be passed to session callback functions.
 *  @param updateRequestFunc A callback that will receive generated session
 *      update request when requested from the DRM system.
 *  @param updatedFunc A callback for notifications that a session has been
 *      added, and subsequent encrypted samples are actively ready to be
 *      decoded.
 */
- (instancetype)initWithSessionContext:(void*)context
              sessionUpdateRequestFunc:
                  (SbDrmSessionUpdateRequestFunc)updateRequestFunc
                    sessionUpdatedFunc:(SbDrmSessionUpdatedFunc)updatedFunc
    NS_DESIGNATED_INITIALIZER;

@end

NS_ASSUME_NONNULL_END

#endif  // STARBOARD_TVOS_SHARED_MEDIA_APPLICATION_DRM_SYSTEM_H_
