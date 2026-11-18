/*
 *  Copyright (c) 2026 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/video_coding/timing/default_video_jitter_timing.h"

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

DefaultVideoJitterTiming::DefaultVideoJitterTiming(const Environment& env)
    : DefaultVideoJitterTiming(&env.clock(), env.field_trials()) {}

DefaultVideoJitterTiming::DefaultVideoJitterTiming(
    Clock* clock,
    const FieldTrialsView& field_trials)
    : clock_(clock),
      ts_extrapolator_(clock_->CurrentTime(), field_trials),
      jitter_estimator_(clock_, field_trials) {}

void DefaultVideoJitterTiming::Reset() {
  ts_extrapolator_.Reset(clock_->CurrentTime());
  jitter_estimator_.Reset();
}

void DefaultVideoJitterTiming::OnCompleteTemporalUnit(uint32_t rtp_timestamp,
                                                      Timestamp receive_time) {
  ts_extrapolator_.Update(receive_time, rtp_timestamp);
}

std::optional<Timestamp> DefaultVideoJitterTiming::ExtrapolateLocalTime(
    uint32_t rtp_timestamp) const {
  return ts_extrapolator_.ExtrapolateLocalTime(rtp_timestamp);
}

std::optional<TimeDelta> DefaultVideoJitterTiming::OnDecodableTemporalUnit(
    uint32_t rtp_timestamp,
    DataSize superframe_size,
    Timestamp max_receive_time,
    bool was_retransmitted) {
  if (was_retransmitted) {
    jitter_estimator_.FrameNacked();
    return std::nullopt;
  }
  std::optional<TimeDelta> inter_frame_delay_variation =
      ifdv_calculator_.Calculate(rtp_timestamp, max_receive_time);
  if (inter_frame_delay_variation) {
    jitter_estimator_.UpdateEstimate(*inter_frame_delay_variation,
                                     superframe_size);
  }
  return jitter_estimator_.GetEstimate();
}

void DefaultVideoJitterTiming::UpdateRtt(TimeDelta rtt) {
  jitter_estimator_.UpdateRtt(rtt);
}

}  // namespace webrtc
