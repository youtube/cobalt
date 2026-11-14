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

#include "api/field_trials_view.h"
#include "api/units/timestamp.h"
#include "modules/video_coding/timing/timestamp_extrapolator.h"
#include "system_wrappers/include/clock.h"

namespace webrtc {

DefaultVideoJitterTiming::DefaultVideoJitterTiming(
    Clock* clock,
    const FieldTrialsView& field_trials)
    : clock_(clock), ts_extrapolator_(clock_->CurrentTime(), field_trials) {}

void DefaultVideoJitterTiming::Reset() {
  ts_extrapolator_.Reset(clock_->CurrentTime());
}

void DefaultVideoJitterTiming::OnCompleteTemporalUnit(uint32_t rtp_timestamp,
                                                      Timestamp receive_time) {
  ts_extrapolator_.Update(receive_time, rtp_timestamp);
}

std::optional<Timestamp> DefaultVideoJitterTiming::ExtrapolateLocalTime(
    uint32_t rtp_timestamp) const {
  return ts_extrapolator_.ExtrapolateLocalTime(rtp_timestamp);
}

}  // namespace webrtc
