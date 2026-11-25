/*
 *  Copyright 2026 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import <Foundation/Foundation.h>

#import "sdk/objc/base/RTCMacros.h"

NS_ASSUME_NONNULL_BEGIN

/**
 * The fingerprint of a certificate, mirroring the WebIDL RTCDtlsFingerprint
 * dictionary. See https://w3c.github.io/webrtc-pc/#dom-rtcdtlsfingerprint
 */
RTC_OBJC_EXPORT
@interface RTC_OBJC_TYPE (RTCDtlsFingerprint) : NSObject

/** The hash function used to compute the fingerprint, e.g. "sha-256". */
@property(nonatomic, readonly) NSString *algorithm;

/**
 * The value of the fingerprint as colon-separated hexadecimal, using the
 * syntax of the fingerprint in RFC 4572 Section 5, e.g. "AB:CD:...".
 */
@property(nonatomic, readonly) NSString *value;

- (instancetype)initWithAlgorithm:(NSString *)algorithm
                            value:(NSString *)value NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

@end

NS_ASSUME_NONNULL_END
