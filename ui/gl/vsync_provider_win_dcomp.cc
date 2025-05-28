// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/vsync_provider_win_dcomp.h"

#include "base/trace_event/typed_macros.h"
#include "ui/gl/direct_composition_support.h"

namespace gl {

VSyncProviderWinDComp::VSyncProviderWinDComp() = default;

VSyncProviderWinDComp::~VSyncProviderWinDComp() = default;

void VSyncProviderWinDComp::GetVSyncParameters(UpdateVSyncCallback callback) {
  base::TimeTicks timebase;
  base::TimeDelta interval;
  if (GetVSyncParametersIfAvailable(&timebase, &interval)) {
    std::move(callback).Run(timebase, interval);
  }
}

// Use the compositor clock to determine vsync interval (as opposed
// to algining with primary monitor) to more optimally align vsync
// interval to video FPS in multi-display mixed refresh rate or VRR configs.
bool VSyncProviderWinDComp::GetVSyncParametersIfAvailable(
    base::TimeTicks* out_timebase,
    base::TimeDelta* out_interval) {
  TRACE_EVENT0("gpu", "VSyncProviderWinDComp::GetVSyncIntervalIfAvailable");
  HRESULT hr = S_OK;

  COMPOSITION_FRAME_ID frame_id;
  hr = gl::DCompositionGetFrameId(COMPOSITION_FRAME_ID_COMPLETED, &frame_id);
  CHECK_EQ(S_OK, hr);

  COMPOSITION_FRAME_STATS stats;
  hr = gl::DCompositionGetStatistics(frame_id, &stats, 0, nullptr, nullptr);
  CHECK_EQ(S_OK, hr);

  *out_timebase = base::TimeTicks::FromQPCValue(stats.startTime);
  *out_interval = base::TimeDelta::FromQPCValue(stats.framePeriod);
  return true;
}

bool VSyncProviderWinDComp::SupportGetVSyncParametersIfAvailable() const {
  return true;
}

bool VSyncProviderWinDComp::IsHWClock() const {
  return true;
}

}  // namespace gl
