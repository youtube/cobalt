/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef SDK_OBJC_CLASSES_VIDEO_OBJC_VIDEO_TRACK_SOURCE_H_
#define SDK_OBJC_CLASSES_VIDEO_OBJC_VIDEO_TRACK_SOURCE_H_

#import "base/RTCVideoCapturer.h"

#include "base/RTCMacros.h"
#include "media/base/adapted_video_track_source.h"
#include "rtc_base/timestamp_aligner.h"

RTC_FWD_DECL_OBJC_CLASS(RTC_OBJC_TYPE(RTCVideoFrame));

@interface RTCObjCVideoSourceAdapter : NSObject <RTC_OBJC_TYPE (RTCVideoCapturerDelegate)>
@end

namespace webrtc {

class ObjCVideoTrackSource : public rtc::AdaptedVideoTrackSource {
 public:
  ObjCVideoTrackSource();
  explicit ObjCVideoTrackSource(bool is_screencast);
  explicit ObjCVideoTrackSource(RTCObjCVideoSourceAdapter* adapter);

  bool is_screencast() const override;

  // Indicates that the encoder should denoise video before encoding it.
  // If it is not set, the default configuration is used which is different
  // depending on video codec.
  absl::optional<bool> needs_denoising() const override;

  SourceState state() const override;

  bool remote() const override;

  void OnCapturedFrame(RTC_OBJC_TYPE(RTCVideoFrame) * frame);

  // Called by RTCVideoSource.
  void OnOutputFormatRequest(int width, int height, int fps);

 private:
  rtc::VideoBroadcaster broadcaster_;
  rtc::TimestampAligner timestamp_aligner_;

  RTCObjCVideoSourceAdapter* adapter_;
  bool is_screencast_;
};

}  // namespace webrtc

#endif  // SDK_OBJC_CLASSES_VIDEO_OBJC_VIDEO_TRACK_SOURCE_H_
