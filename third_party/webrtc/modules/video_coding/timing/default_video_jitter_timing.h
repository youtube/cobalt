/*
 *  Copyright (c) 2026 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_VIDEO_CODING_TIMING_DEFAULT_VIDEO_JITTER_TIMING_H_
#define MODULES_VIDEO_CODING_TIMING_DEFAULT_VIDEO_JITTER_TIMING_H_

#include <cstdint>
#include <optional>

#include "api/environment/environment.h"
#include "api/field_trials_view.h"
#include "api/units/data_size.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "modules/video_coding/timing/inter_frame_delay_variation_calculator.h"
#include "modules/video_coding/timing/jitter_estimator.h"
#include "modules/video_coding/timing/timestamp_extrapolator.h"
#include "system_wrappers/include/clock.h"

namespace webrtc {

class DefaultVideoJitterTiming {
 public:
  explicit DefaultVideoJitterTiming(const Environment& env);
  // TODO(b/493549134): Remove once no longer used.
  DefaultVideoJitterTiming(Clock* clock, const FieldTrialsView& field_trials);
  ~DefaultVideoJitterTiming() = default;

  // Resets members to its initial state.
  void Reset();

  // Updates the extrapolator with the timestamp of a complete temporal unit.
  void OnCompleteTemporalUnit(uint32_t rtp_timestamp, Timestamp receive_time);

  // Returns the extrapolated local time for a given RTP timestamp.
  std::optional<Timestamp> ExtrapolateLocalTime(uint32_t rtp_timestamp) const;

  // Updates the jitter estimator with the information of a decodable temporal
  // unit. Returns the current jitter estimate if available.
  std::optional<TimeDelta> OnDecodableTemporalUnit(uint32_t rtp_timestamp,
                                                   DataSize superframe_size,
                                                   Timestamp max_receive_time,
                                                   bool was_retransmitted);

  // Updates the jitter estimator with the current RTT.
  void UpdateRtt(TimeDelta rtt);

 private:
  Clock* const clock_;
  TimestampExtrapolator ts_extrapolator_;
  JitterEstimator jitter_estimator_;
  InterFrameDelayVariationCalculator ifdv_calculator_;
};

}  // namespace webrtc

#endif  // MODULES_VIDEO_CODING_TIMING_DEFAULT_VIDEO_JITTER_TIMING_H_
