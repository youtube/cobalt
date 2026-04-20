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

#import "starboard/tvos/shared/media/application_drm_system.h"

#import <AVFoundation/AVFoundation.h>

#import "starboard/tvos/shared/defines.h"
#import "starboard/tvos/shared/media/drm_manager.h"
#import "starboard/tvos/shared/starboard_application.h"

@interface SBDKeyPrefetchData : NSObject
@property(readonly) NSData* certificationData;
@property(readonly) NSData* contentIdentifier;
@property(readonly) NSData* initializationData;
@property(readonly) NSInteger ticket;
@end

@implementation SBDKeyPrefetchData
@synthesize certificationData = _certificationData;
@synthesize contentIdentifier = _contentIdentifier;
@synthesize initializationData = _initializationData;
@synthesize ticket = _ticket;

- (instancetype)initWithCertificationData:(NSData*)certificationData
                        contentIdentifier:(NSData*)contentIdentifier
                                 initData:(NSData*)initData
                                   ticket:(NSInteger)ticket {
  self = [super init];
  if (self) {
    _certificationData = certificationData;
    _contentIdentifier = contentIdentifier;
    _initializationData = initData;
    _ticket = ticket;
  }
  return self;
}
@end

@implementation SBDApplicationDrmSystem {
  /**
   *  @brief Context parameter to be passed to session callback functions.
   */
  void* _sessionContext;

  /**
   *  @brief Callback that will receive generated session update request when
   *      requested from the DRM system.
   */
  SbDrmSessionUpdateRequestFunc _sessionUpdateRequestFunc;

  /**
   *  @brief A callback for notifications that a session has been added, and
   *      subsequent encrypted samples are actively ready to be decoded.
   */
  SbDrmSessionUpdatedFunc _sessionUpdatedFunc;

  /**
   *  @brief Data for key prefetches.
   */
  NSMutableDictionary<NSString*, SBDKeyPrefetchData*>* _keyPrefetchData;

  /**
   *  @brief The @c AVContentKeyRequests that are waiting to receive a
   *      generateSessionUpdateRequest.
   */
  NSMutableDictionary<NSString*, AVContentKeyRequest*>*
      _keyRequestsPendingUpdateRequest;

  /**
   *  @brief The @c AVContentKeyRequests that are waiting to receive a
   *      key.
   */
  NSMutableDictionary<NSData*, AVContentKeyRequest*>* _keyRequestsPendingKey;
}

- (instancetype)initWithSessionContext:(void*)context
              sessionUpdateRequestFunc:
                  (SbDrmSessionUpdateRequestFunc)updateRequestFunc
                    sessionUpdatedFunc:(SbDrmSessionUpdatedFunc)updatedFunc {
  self = [super init];
  if (self) {
    _sessionContext = context;
    _sessionUpdateRequestFunc = updateRequestFunc;
    _sessionUpdatedFunc = updatedFunc;
    _keyRequestsPendingUpdateRequest = [NSMutableDictionary dictionary];
    _keyRequestsPendingKey = [NSMutableDictionary dictionary];
    _keyPrefetchData = [NSMutableDictionary dictionary];
  }
  return self;
}

- (void)updateSessionWithKey:(NSData*)key
                      ticket:(NSInteger)ticket
                   sessionId:(NSData*)sessionId {
  AVContentKeyRequest* keyRequest;
  @synchronized(self) {
    keyRequest = _keyRequestsPendingKey[sessionId];
    [_keyRequestsPendingKey removeObjectForKey:sessionId];
  }

  AVContentKeyResponse* keyResponse = [AVContentKeyResponse
      contentKeyResponseWithFairPlayStreamingKeyResponseData:key];
  [keyRequest processContentKeyResponse:keyResponse];

  SBDDrmManager* drmManager = SBDGetApplication().drmManager;
  SbDrmSystem starboardDrmSystem =
      [drmManager starboardDrmSystemForApplicationDrmSystem:self];
  const char* errorMessage = NULL;
  _sessionUpdatedFunc(starboardDrmSystem, _sessionContext, ticket,
                      kSbDrmStatusSuccess, errorMessage, sessionId.bytes,
                      sessionId.length);
}

- (void)makeKeyRequestData:(AVContentKeyRequest*)keyRequest
         certificationData:(NSData*)certificationData
         contentIdentifier:(NSData*)contentIdentifier
                  initData:(NSData*)initData
                    ticket:(NSInteger)ticket {
  [keyRequest
      makeStreamingContentKeyRequestDataForApp:certificationData
                             contentIdentifier:contentIdentifier
                                       options:nil
                             completionHandler:^(
                                 NSData* _Nullable contentKeyRequestData,
                                 NSError* _Nullable error) {
                               if (error) {
                                 [keyRequest
                                     processContentKeyResponseError:error];
                                 return;
                               }
                               [self streamingContentKeyRequest:keyRequest
                                              completedWithData:
                                                  contentKeyRequestData
                                                       initData:initData
                                                         ticket:ticket];
                             }];
}

- (void)
    generateSessionUpdateRequestWithCertificationData:(NSData*)certificationData
                                    contentIdentifier:(NSData*)contentIdentifier
                                             initData:(NSData*)initData
                                               ticket:(NSInteger)ticket {
  NSString* requestIdentifier =
      [[NSString alloc] initWithData:initData
                            encoding:NSUTF16LittleEndianStringEncoding];

  // This flow is also used to initiate prefetching of a content key. If no
  // key request can be found for the given init data, then a prefetch was
  // requested.
  AVContentKeyRequest* keyRequest;
  @synchronized(self) {
    keyRequest = _keyRequestsPendingUpdateRequest[requestIdentifier];
    [_keyRequestsPendingUpdateRequest removeObjectForKey:requestIdentifier];
  }

  if (!keyRequest) {
    // This is a key prefetch. Cache the data for use once the keyRequest
    // is generated.
    SBDKeyPrefetchData* prefetchData =
        [[SBDKeyPrefetchData alloc] initWithCertificationData:certificationData
                                            contentIdentifier:contentIdentifier
                                                     initData:initData
                                                       ticket:ticket];
    @synchronized(self) {
      _keyPrefetchData[requestIdentifier] = prefetchData;
    }

    // Tell the key session to generate a key request with the given
    // identifier. This will go through the system just as if AVPlayer
    // initiated a content key request.
    [_keySession processContentKeyRequestWithIdentifier:requestIdentifier
                                     initializationData:nil
                                                options:nil];
    return;
  }

  [self makeKeyRequestData:keyRequest
         certificationData:certificationData
         contentIdentifier:contentIdentifier
                  initData:initData
                    ticket:ticket];
}

- (void)streamingContentKeyRequest:(AVContentKeyRequest*)keyRequest
                 completedWithData:(NSData*)contentKeyRequestData
                          initData:(NSData*)initData
                            ticket:(NSInteger)ticket {
  NSData* sessionId = [NSData dataWithBytes:&ticket length:sizeof(ticket)];
  @synchronized(self) {
    _keyRequestsPendingKey[sessionId] = keyRequest;
  }

  SBDDrmManager* drmManager = SBDGetApplication().drmManager;
  SbDrmSystem starboardDrmSystem =
      [drmManager starboardDrmSystemForApplicationDrmSystem:self];
  const char* errorMessage = NULL;
  _sessionUpdateRequestFunc(
      starboardDrmSystem, _sessionContext, ticket, kSbDrmStatusSuccess,
      kSbDrmSessionRequestTypeLicenseRequest, errorMessage, sessionId.bytes,
      sessionId.length, contentKeyRequestData.bytes,
      contentKeyRequestData.length, static_cast<const char*>(initData.bytes));
}

- (BOOL)processKeyRequest:(AVContentKeyRequest*)keyRequest {
  @synchronized(self) {
    // Check if this keyRequest matches a prefetch.
    SBDKeyPrefetchData* prefetchData = _keyPrefetchData[keyRequest.identifier];
    if (prefetchData) {
      // Fulfill the prefetch.
      [_keyPrefetchData removeObjectForKey:keyRequest.identifier];
      [self makeKeyRequestData:keyRequest
             certificationData:prefetchData.certificationData
             contentIdentifier:prefetchData.contentIdentifier
                      initData:prefetchData.initializationData
                        ticket:prefetchData.ticket];
      return NO;
    }

    // No information is available for this key request, so inform the web app
    // that encrypted media was encountered.
    _keyRequestsPendingUpdateRequest[keyRequest.identifier] = keyRequest;
    return YES;
  }
}

@end
