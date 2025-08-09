/*
 *  Copyright 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import <AVFoundation/AVFoundation.h>
#import <Foundation/Foundation.h>

#import "sdk/objc/base/RTCMacros.h"

NS_ASSUME_NONNULL_BEGIN

typedef NS_ENUM(NSInteger, RTCVideoRotation) {
  RTCVideoRotation_0 = 0,
  RTCVideoRotation_90 = 90,
  RTCVideoRotation_180 = 180,
  RTCVideoRotation_270 = 270,
};

@protocol RTC_OBJC_TYPE
(RTCVideoFrameBuffer);

// RTCVideoFrame is an ObjectiveC version of webrtc::VideoFrame.
RTC_OBJC_EXPORT
@interface RTC_OBJC_TYPE (RTCVideoFrame) : NSObject

/** Width without rotation applied. */
@property(nonatomic, readonly) int width;

/** Height without rotation applied. */
@property(nonatomic, readonly) int height;
@property(nonatomic, readonly) RTCVideoRotation rotation;

/** Timestamp in nanoseconds. */
@property(nonatomic, readonly) int64_t timeStampNs;

/** Timestamp 90 kHz. */
@property(nonatomic, assign) int32_t timeStamp;

@property(nonatomic, readonly) id<RTC_OBJC_TYPE(RTCVideoFrameBuffer)> buffer;

- (instancetype)init NS_UNAVAILABLE;
- (instancetype)new NS_UNAVAILABLE;

/** Initialize an RTCVideoFrame from a frame buffer, rotation, and timestamp.
 */
- (instancetype)initWithBuffer:(id<RTC_OBJC_TYPE(RTCVideoFrameBuffer)>)frameBuffer
                      rotation:(RTCVideoRotation)rotation
                   timeStampNs:(int64_t)timeStampNs;

/** Return a frame that is guaranteed to be I420, i.e. it is possible to access
 *  the YUV data on it.
 */
- (RTC_OBJC_TYPE(RTCVideoFrame) *)newI420VideoFrame;

@end

NS_ASSUME_NONNULL_END
