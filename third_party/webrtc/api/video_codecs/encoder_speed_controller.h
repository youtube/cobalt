/*
 *  Copyright (c) 2025 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_VIDEO_CODECS_ENCODER_SPEED_CONTROLLER_H_
#define API_VIDEO_CODECS_ENCODER_SPEED_CONTROLLER_H_

#include <array>
#include <memory>
#include <optional>
#include <vector>

#include "api/units/time_delta.h"

namespace webrtc {

// Utility class intended to help dynamically find the optimal speed settings to
// use for a video encoder. An instance of this class is intended to handle a
// single session at a single resolution. I.e. and new instance should be
// created if the resolution is updated. That also provides the opportunity to
// configure a new set of available speeds, more appropriate for the new
// resolution. If spatial SVC and/or simulcast is used, the caller of this class
// must make sure the frame interval is adjusted if the encodings of a temporal
// unit is serialized.
class EncoderSpeedController {
 public:
  // The `ReferenceClass` allows the controller to pick a separate speed level
  // based on the importance of the frame. Frames that act as references for
  // many subsequent frames typically warrant a higher effort level.
  enum class ReferenceClass : int {
    kKey = 0,       // Key-frames, or long-term references.
    kMain,          // "Normal" delta frames or a temporal base layer
    kIntermediate,  // Reference for a short-live frame tree (e.g T1 in L1T3)
    kNoneReference  // A frame not used as reference sub subsequent frames.
  };
  struct Config {
    // Represents an assignable speed level, with specific speeds for one or
    // more temporal layers.
    struct SpeedLevel {
      // The actual speed levels (values of the integers below) are
      // implementation specific. It is up to the user to make mappings
      // between these and what the API surface of the encoder looks like,
      // if it is not using integers.

      // Array of speeds, indexed by ReferenceClass.
      std::array<int, 4> speeds;

      // Don't use this speed level if the average QP is lower than `min_qp`.
      std::optional<int> min_qp;
    };
    // Ordered vector of speed levels, start with the slowest speed (lower
    // effort) and the increasing the average speed for each entry.
    std::vector<SpeedLevel> speed_levels;

    // An index into `speed_levels` at which the controller should start.
    int start_speed_index;
  };

  // Input data to the controller about the frame that is about the be encoded.
  struct FrameEncodingInfo {
    // The reference class of the frame to be encoded.
    ReferenceClass reference_type;
    // True iff the frame is a repeat of the previous frame (e.g. the frames
    // used during quality convergence of a variable fps screenshare feed).
    bool is_repeat_frame;
  };

  // Output from the controller, indicates which speed the encoder should be
  // configured with given the frame info that was submitted.
  struct EncodeSettings {
    // Speed the encoder should use for this frame.
    int speed;
  };

  // Data the controller should be fed with after a frame has been encoded,
  // providing info about the resulting encoding.
  struct EncodeResults {
    // The speed setting used for this encoded frame.
    int speed;
    // The time it took to encode the frame.
    TimeDelta encode_time;
    // The _average_ frame QP of the encoded frame.
    int qp;
    // The frame encoding info - same as what was originally given as argument
    // to `GetEncodingSettings()`.
    FrameEncodingInfo frame_info;
  };

  // Creates an instance of the speed controller. This should be called any
  // time the encoder has been recreated e.g. due to a resolution change.
  static std::unique_ptr<EncoderSpeedController> Create(
      const Config& config,
      TimeDelta start_frame_interval);

  virtual ~EncoderSpeedController() = default;

  // Should be called any time the rate targets of the encoder changed.
  // The frame interval (1s/fps) effectively sets the time limit for an encoding
  // operation.
  virtual void SetFrameInterval(TimeDelta frame_interval) = 0;

  // Should be called before each frame to be encoded, and the encoder should
  // thereafter be configured with requested settings.
  virtual EncodeSettings GetEncodeSettings(FrameEncodingInfo frame_info) = 0;

  // Should be called after each frame has completed encoding.
  virtual void OnEncodedFrame(EncodeResults results) = 0;
};

}  // namespace webrtc

#endif  // API_VIDEO_CODECS_ENCODER_SPEED_CONTROLLER_H_
