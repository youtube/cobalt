/*
 * Copyright 2025 The WebRTC project authors. All rights reserved.
 *
 * Use of this source code is governed by a BSD-style license
 * that can be found in the LICENSE file in the root of the source
 * tree. An additional intellectual property rights grant can be found
 * in the file PATENTS.  All contributing project authors may
 * be found in the AUTHORS file in the root of the source tree.
 */

#ifndef VIDEO_CORRUPTION_DETECTION_EVALUATION_FILE_BASED_ENCODER_H_
#define VIDEO_CORRUPTION_DETECTION_EVALUATION_FILE_BASED_ENCODER_H_

#include <string>

#include "api/rtc_error.h"
#include "api/units/data_rate.h"
#include "api/video/video_codec_type.h"
#include "video/corruption_detection/evaluation/test_clip.h"

namespace webrtc {

// An encoder that encodes a raw video clip specified by `clip` given in a Y4M
// or YUV file. The user cannot reach individual frames. However, the user
// should be able to reach the encoded file from the encoded file path returned
// by `Encode` if successful.
class FileBasedEncoder {
 public:
  FileBasedEncoder() = default;
  virtual ~FileBasedEncoder() = default;

  // Encodes the raw video clip specified by `clip` given in a Y4M or YUV file.
  // Creates an encoded file where the encoded frames are stored. The encoded
  // path is returned if successful.
  // TODO: bugs.webrtc.org/358039777 - Make pure virtual when all subclasses
  // has implemented this method.
  virtual RTCErrorOr<std::string> Encode(const TestClip& clip,
                                         DataRate bitrate) {
    return Encode2(clip, bitrate);
  }

  // TODO: bugs.webrtc.org/358039777 - Remove.
  [[deprecated("Use above Encode instead")]]
  virtual RTCErrorOr<std::string> Encode2(const TestClip& clip,
                                          DataRate bitrate) {
    return Encode(clip, bitrate);
  }

  // Returns the used codec for encoding.
  virtual VideoCodecType GetCodec() const = 0;
};

}  // namespace webrtc

#endif  // VIDEO_CORRUPTION_DETECTION_EVALUATION_FILE_BASED_ENCODER_H_
