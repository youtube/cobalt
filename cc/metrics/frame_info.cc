// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/metrics/frame_info.h"

#include <algorithm>

#include "base/check.h"
#include "build/build_config.h"

namespace cc {

namespace {

bool IsCompositorSmooth(FrameInfo::SmoothThread thread) {
  return thread == FrameInfo::SmoothThread::kSmoothCompositor ||
         thread == FrameInfo::SmoothThread::kSmoothBoth;
}

bool IsMainSmooth(FrameInfo::SmoothThread thread) {
  return thread == FrameInfo::SmoothThread::kSmoothMain ||
         thread == FrameInfo::SmoothThread::kSmoothBoth;
}

bool ValidateFinalStateIsForMainThread(FrameInfo::FrameFinalState state) {
  switch (state) {
    case FrameInfo::FrameFinalState::kPresentedPartialOldMain:
    case FrameInfo::FrameFinalState::kPresentedPartialNewMain:
      // Frames that contain main-thread update cannot have a 'partial update'
      // state.
      return false;

    case FrameInfo::FrameFinalState::kPresentedAll:
    case FrameInfo::FrameFinalState::kNoUpdateDesired:
    case FrameInfo::FrameFinalState::kDropped:
      return true;
  }
}

}  // namespace

bool FrameInfo::IsDroppedAffectingSmoothness() const {
  // If neither of the threads are expected to be smooth, then this frame cannot
  // affect smoothness.
  if (smooth_thread == SmoothThread::kSmoothNone)
    return false;

  return WasSmoothMainUpdateDropped() || WasSmoothCompositorUpdateDropped();
}

void FrameInfo::MergeWith(const FrameInfo& other) {
#if BUILDFLAG(IS_ANDROID)
  // TODO(1278168): on android-webview, multiple frames can be submitted against
  // the same BeginFrameArgs. This can trip the DCHECK()s in this function.
  if (was_merged)
    return;
  if (main_thread_response == MainThreadResponse::kIncluded &&
      other.main_thread_response == MainThreadResponse::kIncluded) {
    return;
  }
#endif
  DCHECK(!was_merged);
  DCHECK(!other.was_merged);
  DCHECK(Validate());
  DCHECK(other.Validate());

  if (main_thread_response == MainThreadResponse::kIncluded) {
    // |this| includes the main-thread updates. Therefore:
    //   - |other| must not also include main-thread updates.
    //   - |this| must have a valid final-state.
    DCHECK_EQ(MainThreadResponse::kMissing, other.main_thread_response);
    DCHECK(ValidateFinalStateIsForMainThread(final_state));

    // If the compositor-only update did not include any changes from the
    // main-thread, then it did drop the main-thread update. Therefore, overall
    // the main-thread update was dropped, even if the 'main thread update' is
    // presented in a subsequent frame.
    bool compositor_only_change_included_new_main =
        other.final_state == FrameFinalState::kPresentedAll ||
        other.final_state == FrameFinalState::kPresentedPartialNewMain;
    main_update_was_dropped = final_state == FrameFinalState::kDropped ||
                              !compositor_only_change_included_new_main;

    compositor_update_was_dropped =
        other.final_state == FrameFinalState::kDropped;
  } else {
    // |this| does not include main-thread updates. Therefore:
    //   - |other| must include main-thread updates.
    //   - |other| must have a valid final-state.
    DCHECK_EQ(MainThreadResponse::kIncluded, other.main_thread_response);
    DCHECK(ValidateFinalStateIsForMainThread(other.final_state));

    main_update_was_dropped = other.final_state == FrameFinalState::kDropped;
    compositor_update_was_dropped = final_state == FrameFinalState::kDropped;
  }

  was_merged = true;
  main_thread_response = MainThreadResponse::kIncluded;

  // The |scroll_thread| information cannot change once the frame starts.
  // However, if a frame did not have any scroll-events, or the scroll-events
  // for the frame did not cause any visual updates, then |scroll_thread| is
  // reset. Therefore, either |scroll_thread| should be the same for |this| and
  // |other|, or one of them must be |kUnknown|.
  if (scroll_thread != other.scroll_thread) {
    if (scroll_thread == SmoothEffectDrivingThread::kUnknown) {
      scroll_thread = other.scroll_thread;
    } else {
      DCHECK_EQ(other.scroll_thread, SmoothEffectDrivingThread::kUnknown);
    }
  }

  if (other.has_missing_content)
    has_missing_content = true;

  if (other.final_state == FrameFinalState::kDropped)
    final_state = FrameFinalState::kDropped;

  const bool is_compositor_smooth = IsCompositorSmooth(smooth_thread) ||
                                    IsCompositorSmooth(other.smooth_thread);
  const bool is_main_smooth =
      IsMainSmooth(smooth_thread) || IsMainSmooth(other.smooth_thread);
  if (is_compositor_smooth && is_main_smooth) {
    smooth_thread = SmoothThread::kSmoothBoth;
  } else if (is_compositor_smooth) {
    smooth_thread = SmoothThread::kSmoothCompositor;
  } else if (is_main_smooth) {
    smooth_thread = SmoothThread::kSmoothMain;
  } else {
    smooth_thread = SmoothThread::kSmoothNone;
  }

  total_latency = std::max(total_latency, other.total_latency);

  // Validate the state after the merge.
  DCHECK(Validate());
}

bool FrameInfo::Validate() const {
  // If |scroll_thread| is set, then the |smooth_thread| must include that
  // thread.
  if (scroll_thread == SmoothEffectDrivingThread::kCompositor) {
    DCHECK(IsCompositorSmooth(smooth_thread));
  } else if (scroll_thread == SmoothEffectDrivingThread::kMain) {
    DCHECK(IsMainSmooth(smooth_thread));
  }

  return true;
}

bool FrameInfo::WasSmoothCompositorUpdateDropped() const {
  if (!IsCompositorSmooth(smooth_thread))
    return false;

  if (was_merged)
    return compositor_update_was_dropped;
  return final_state == FrameFinalState::kDropped;
}

bool FrameInfo::WasSmoothMainUpdateDropped() const {
  if (!IsMainSmooth(smooth_thread))
    return false;

  if (was_merged)
    return main_update_was_dropped;

  switch (final_state) {
    case FrameFinalState::kDropped:
    case FrameFinalState::kPresentedPartialOldMain:
      return true;

    case FrameFinalState::kPresentedPartialNewMain:
      // Although this frame dropped the main-thread updates for this particular
      // frame, it did include new main-thread update. So do not treat this as a
      // dropped frame.
      return false;

    case FrameFinalState::kNoUpdateDesired:
    case FrameFinalState::kPresentedAll:
      return false;
  }

  return false;
}

bool FrameInfo::WasSmoothMainUpdateExpected() const {
  return final_state != FrameFinalState::kNoUpdateDesired;
}

bool FrameInfo::IsScrollPrioritizeFrameDropped() const {
  // If any scroll is active the dropped frame for only the scrolling thread is
  // reported. If no scroll is active then reports if dropped frames is
  // affecting smoothness.
  switch (scroll_thread) {
    case SmoothEffectDrivingThread::kCompositor:
      return WasSmoothCompositorUpdateDropped();
    case SmoothEffectDrivingThread::kMain:
      return WasSmoothMainUpdateDropped();
    case SmoothEffectDrivingThread::kUnknown:
      return IsDroppedAffectingSmoothness();
  }
}

}  // namespace cc
