/*
 *  Copyright 2024 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "RTCRtpHeaderExtensionCapability+Private.h"
#import "RTCRtpTransceiver+Private.h"

#import "helpers/NSString+StdString.h"

@implementation RTC_OBJC_TYPE (RTCRtpHeaderExtensionCapability)

@synthesize uri = _uri;
@synthesize preferredId = _preferredId;
@synthesize preferredEncrypted = _preferredEncrypted;
@synthesize direction = _direction;

- (instancetype)init {
  webrtc::RtpHeaderExtensionCapability nativeRtpHeaderExtensionCapability;
  return [self initWithNativeRtpHeaderExtensionCapability:
                   nativeRtpHeaderExtensionCapability];
}

- (instancetype)initWithNativeRtpHeaderExtensionCapability:
    (const webrtc::RtpHeaderExtensionCapability &)
        nativeRtpHeaderExtensionCapability {
  self = [super init];
  if (self) {
    _uri = [NSString stringForStdString:nativeRtpHeaderExtensionCapability.uri];
    if (nativeRtpHeaderExtensionCapability.preferred_id) {
      _preferredId = [NSNumber numberWithInt:nativeRtpHeaderExtensionCapability
                                                 .preferred_id->value()];
    }
    _preferredEncrypted = nativeRtpHeaderExtensionCapability.preferred_encrypt;
    _direction = [RTC_OBJC_TYPE(RTCRtpTransceiver)
        rtpTransceiverDirectionFromNativeDirection:
            nativeRtpHeaderExtensionCapability.direction];
  }
  return self;
}

- (NSString *)description {
  return
      [NSString stringWithFormat:
                    @"RTC_OBJC_TYPE(RTCRtpHeaderExtensionCapability) {\n  uri: "
                    @"%@\n  preferredId: %@\n  preferredEncrypted: %d\n}",
                    _uri,
                    _preferredId,
                    _preferredEncrypted];
}

- (webrtc::RtpHeaderExtensionCapability)nativeRtpHeaderExtensionCapability {
  webrtc::RtpTransceiverDirection direction = [RTC_OBJC_TYPE(RTCRtpTransceiver)
      nativeRtpTransceiverDirectionFromDirection:_direction];
  if (_preferredId != nil) {
    return webrtc::RtpHeaderExtensionCapability(
        [NSString stdStringForString:_uri],
        webrtc::RtpHeaderExtensionId(_preferredId.intValue),
        _preferredEncrypted,
        direction);
  } else {
    return webrtc::RtpHeaderExtensionCapability(
        [NSString stdStringForString:_uri], _preferredEncrypted, direction);
  }
}

@end
