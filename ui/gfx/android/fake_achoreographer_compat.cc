// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/android/fake_achoreographer_compat.h"

#include <algorithm>

#include "base/check_op.h"
#include "ui/gfx/android/achoreographer_compat.h"

namespace gfx {

namespace {

FakeAChoreographerCompat* g_active_fake = nullptr;
AChoreographer* const kFakeChoreographer =
    reinterpret_cast<AChoreographer*>(0x1234);

}  // namespace

FakeAChoreographerCompat::FakeAChoreographerCompat(bool compat_supported,
                                                   bool compat33_supported)
    : compat_(CreateTestAChoreographerCompat(compat_supported)),
      compat33_(CreateTestAChoreographerCompat33(compat33_supported)) {
  CHECK_EQ(g_active_fake, nullptr);
  g_active_fake = this;
  AChoreographerCompat::SetForTesting(&compat_);
  AChoreographerCompat33::SetForTesting(&compat33_);
}

FakeAChoreographerCompat::~FakeAChoreographerCompat() {
  CHECK_EQ(g_active_fake, this);
  AChoreographerCompat::SetForTesting(nullptr);
  AChoreographerCompat33::SetForTesting(nullptr);
  g_active_fake = nullptr;
}

void FakeAChoreographerCompat::TriggerVsync(
    const FrameCallbackData& callback_data) {
  CHECK(compat33_.supported);

  // Copy and clear because callbacks might post new callbacks.
  auto callbacks = std::move(vsync_callbacks_);
  vsync_callbacks_.clear();

  const AChoreographerFrameCallbackData* android_callback_data =
      reinterpret_cast<const AChoreographerFrameCallbackData*>(&callback_data);
  for (const auto& [callback, data] : callbacks) {
    callback(android_callback_data, data);
  }
}

void FakeAChoreographerCompat::TriggerFrameCallback64(
    int64_t frame_time_nanos) {
  CHECK(compat_.supported);

  // Copy and clear because callbacks might post new callbacks.
  auto callbacks = std::move(frame_callbacks_64_);
  frame_callbacks_64_.clear();

  for (const auto& [callback, data] : callbacks) {
    callback(frame_time_nanos, data);
  }
}

void FakeAChoreographerCompat::TriggerRefreshRateCallback(
    int64_t vsync_period_nanos) {
  CHECK(compat_.supported);

  // Copy in case callbacks modify the list (e.g. unregister).
  auto callbacks = refresh_rate_callbacks_;

  for (const auto& [callback, data] : callbacks) {
    callback(vsync_period_nanos, data);
  }
}

// static
AChoreographerCompat FakeAChoreographerCompat::CreateTestAChoreographerCompat(
    bool supported) {
  AChoreographerCompat compat;
  compat.supported = supported;
  if (!supported) {
    return compat;
  }
  compat.AChoreographer_getInstanceFn = []() { return kFakeChoreographer; };
  compat.AChoreographer_postFrameCallback64Fn =
      [](AChoreographer* choreographer, AChoreographer_frameCallback64 callback,
         void* data) {
        CHECK_EQ(choreographer, kFakeChoreographer);
        CHECK_NE(g_active_fake, nullptr);
        g_active_fake->frame_callbacks_64_.emplace_back(callback, data);
      };
  compat.AChoreographer_registerRefreshRateCallbackFn =
      [](AChoreographer* choreographer,
         AChoreographer_refreshRateCallback callback, void* data) {
        CHECK_EQ(choreographer, kFakeChoreographer);
        CHECK_NE(g_active_fake, nullptr);
        g_active_fake->refresh_rate_callbacks_.emplace_back(callback, data);
      };
  compat.AChoreographer_unregisterRefreshRateCallbackFn =
      [](AChoreographer* choreographer,
         AChoreographer_refreshRateCallback callback, void* data) {
        CHECK_EQ(choreographer, kFakeChoreographer);
        CHECK_NE(g_active_fake, nullptr);
        std::erase(g_active_fake->refresh_rate_callbacks_,
                   std::make_pair(callback, data));
      };
  return compat;
}

// static
AChoreographerCompat33
FakeAChoreographerCompat::CreateTestAChoreographerCompat33(bool supported) {
  AChoreographerCompat33 compat33;
  compat33.supported = supported;
  if (!supported) {
    return compat33;
  }
  compat33.AChoreographer_postVsyncCallbackFn =
      [](AChoreographer* choreographer, AChoreographer_vsyncCallback callback,
         void* data) {
        CHECK_EQ(choreographer, kFakeChoreographer);
        CHECK_NE(g_active_fake, nullptr);
        g_active_fake->vsync_callbacks_.emplace_back(callback, data);
      };
  compat33.AChoreographerFrameCallbackData_getFrameTimeNanosFn =
      [](const AChoreographerFrameCallbackData* callback_data) {
        return reinterpret_cast<const FrameCallbackData*>(callback_data)
            ->frame_time_nanos;
      };
  compat33.AChoreographerFrameCallbackData_getFrameTimelinesLengthFn =
      [](const AChoreographerFrameCallbackData* callback_data) {
        return reinterpret_cast<const FrameCallbackData*>(callback_data)
            ->timelines.size();
      };
  compat33.AChoreographerFrameCallbackData_getPreferredFrameTimelineIndexFn =
      [](const AChoreographerFrameCallbackData* callback_data) {
        return reinterpret_cast<const FrameCallbackData*>(callback_data)
            ->preferred_index;
      };
  compat33.AChoreographerFrameCallbackData_getFrameTimelineVsyncIdFn =
      [](const AChoreographerFrameCallbackData* callback_data, size_t index) {
        return reinterpret_cast<const FrameCallbackData*>(callback_data)
            ->timelines[index]
            .vsync_id;
      };
  compat33
      .AChoreographerFrameCallbackData_getFrameTimelineExpectedPresentationTimeNanosFn =
      [](const AChoreographerFrameCallbackData* callback_data, size_t index) {
        return reinterpret_cast<const FrameCallbackData*>(callback_data)
            ->timelines[index]
            .expected_presentation_time_nanos;
      };
  compat33.AChoreographerFrameCallbackData_getFrameTimelineDeadlineNanosFn =
      [](const AChoreographerFrameCallbackData* callback_data, size_t index) {
        return reinterpret_cast<const FrameCallbackData*>(callback_data)
            ->timelines[index]
            .deadline_nanos;
      };
  return compat33;
}

}  // namespace gfx
