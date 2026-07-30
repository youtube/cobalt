// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_ANDROID_FAKE_ACHOREOGRAPHER_COMPAT_H_
#define UI_GFX_ANDROID_FAKE_ACHOREOGRAPHER_COMPAT_H_

#include <sys/types.h>

#include <utility>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "ui/gfx/android/achoreographer_compat.h"

namespace gfx {

class FakeAChoreographerCompat {
 public:
  FakeAChoreographerCompat(bool compat_supported, bool compat33_supported);
  ~FakeAChoreographerCompat();

  FakeAChoreographerCompat(const FakeAChoreographerCompat&) = delete;
  FakeAChoreographerCompat& operator=(const FakeAChoreographerCompat&) = delete;

  const AChoreographerCompat& compat() const { return compat_; }
  const AChoreographerCompat33& compat33() const { return compat33_; }

  struct FrameTimeline {
    int64_t vsync_id;
    int64_t deadline_nanos;
    int64_t expected_presentation_time_nanos;
  };
  struct FrameCallbackData {
    int64_t frame_time_nanos;
    std::vector<FrameTimeline> timelines;
    size_t preferred_index;
  };

  void TriggerVsync(const FrameCallbackData& callback_data);
  void TriggerFrameCallback64(int64_t frame_time_nanos);
  void TriggerRefreshRateCallback(int64_t vsync_period_nanos);

 private:
  static AChoreographerCompat CreateTestAChoreographerCompat(bool supported);
  static AChoreographerCompat33 CreateTestAChoreographerCompat33(
      bool supported);

  const AChoreographerCompat compat_;
  const AChoreographerCompat33 compat33_;

  using FrameCallback64WithData =
      std::pair<AChoreographer_frameCallback64, raw_ptr<void>>;
  std::vector<FrameCallback64WithData> frame_callbacks_64_;

  using RefreshRateCallbackWithData =
      std::pair<AChoreographer_refreshRateCallback, raw_ptr<void>>;
  std::vector<RefreshRateCallbackWithData> refresh_rate_callbacks_;

  using VsyncCallbackWithData =
      std::pair<AChoreographer_vsyncCallback, raw_ptr<void>>;
  std::vector<VsyncCallbackWithData> vsync_callbacks_;
};

}  // namespace gfx

#endif  // UI_GFX_ANDROID_FAKE_ACHOREOGRAPHER_COMPAT_H_
