/*
 *  Copyright 2026 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "RTCDtlsFingerprint.h"

@implementation RTC_OBJC_TYPE (RTCDtlsFingerprint)

@synthesize algorithm = _algorithm;
@synthesize value = _value;

- (instancetype)initWithAlgorithm:(NSString *)algorithm
                            value:(NSString *)value {
  self = [super init];
  if (self) {
    _algorithm = [algorithm copy];
    _value = [value copy];
  }
  return self;
}

@end
