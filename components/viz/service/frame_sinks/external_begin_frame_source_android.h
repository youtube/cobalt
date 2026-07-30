// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_FRAME_SINKS_EXTERNAL_BEGIN_FRAME_SOURCE_ANDROID_H_
#define COMPONENTS_VIZ_SERVICE_FRAME_SINKS_EXTERNAL_BEGIN_FRAME_SOURCE_ANDROID_H_

#include <jni.h>

#include <memory>
#include <optional>

#include "base/android/jni_weak_ref.h"
#include "base/containers/flat_map.h"
#include "base/time/time.h"
#include "components/viz/common/frame_sinks/begin_frame_args.h"
#include "components/viz/common/frame_sinks/begin_frame_source.h"
#include "components/viz/service/viz_service_export.h"

namespace viz {

// An implementation of ExternalBeginFrameSource which is driven by VSync
// signals coming from org.chromium.ui.VSyncMonitor.
class VIZ_SERVICE_EXPORT ExternalBeginFrameSourceAndroid
    : public ExternalBeginFrameSource,
      public ExternalBeginFrameSourceClient {
 public:
  // Source of the VSync interval (aka VSync period) that `AChoreographerImpl`
  // provides to `ExternalBeginFrameSourceAndroid`, together with the reason for
  // choosing the particular source.
  // LINT.IfChange(VsyncIntervalSource)
  enum class VsyncIntervalSource {
    // -------------------------------------------------------------------------
    // Source: VSync interval that the OS PROVIDED to `RefreshRateCallback`
    // (registered via `AChoreographer_registerRefreshRateCallback`).
    // -------------------------------------------------------------------------

    // Reason: The OS doesn't support `AChoreographer_postVsyncCallback`.
    kOsProvidedTimelineDerivedNotSupported = 0,

    // Reason: The OS provided only one frame timeline to `VsyncCallback`.
    kOsProvidedOnlyOneTimeline = 1,

    // Reason: `features::kDeriveVSyncIntervalFromFrameTimelines` was disabled.
    kOsProvidedAlways = 2,

    // Reason: The timeline-derived VSync interval was less than 1 ms, implying
    // a refresh rate above 1 kHz.
    kOsProvidedTimelineDerivedTooShort = 3,

    // Reason: `features::kDeriveVSyncIntervalFromFrameTimelines` was enabled in
    // `DeriveVSyncIntervalFromFrameTimelinesMode::kDeriveIfLonger` mode and the
    // OS-provided VSync interval was longer than or equal to the
    // timeline-derived VSync interval.
    kOsProvidedLongerThanOrEqualToTimelineDerived = 4,

    // -------------------------------------------------------------------------
    // Source: DERIVED from the frame TIMELINES that the OS provided to
    // `VsyncCallback` (posted via `AChoreographer_postVsyncCallback`) by taking
    // the difference between the presentation timestamps of the first two
    // timelines. Can be either SNAPPED to the closest supported interval in
    // `Display.getSupportedRefreshRates()` (if the display-supported interval
    // is within
    // `features::kDeriveVSyncIntervalFromFrameTimelinesSnapToleranceParam` of
    // the timeline-derived interval) or UNSNAPPED.
    //
    // The advantage of deriving the VSync interval is that it avoids using an
    // incorrect VSync interval immediately before/after a refresh rate change,
    // because frame timelines are synchronized with the Android choreographer.
    // -------------------------------------------------------------------------

    // Reason: `features::kDeriveVSyncIntervalFromFrameTimelines` was enabled in
    // `DeriveVSyncIntervalFromFrameTimelinesMode::kDeriveIfLonger` and the
    // timeline-derived VSync interval was longer than the OS-derived VSync
    // interval mode.
    kSnappedTimelineDerivedLongerThanOsProvided = 5,
    kUnsnappedTimelineDerivedLongerThanOsProvided = 6,

    // Reason: `features::kDeriveVSyncIntervalFromFrameTimelines` was enabled in
    // `DeriveVSyncIntervalFromFrameTimelinesMode::kAlwaysDerive` mode.
    kSnappedTimelineDerivedAlways = 7,
    kUnsnappedTimelineDerivedAlways = 8,

    kMaxValue = kUnsnappedTimelineDerivedAlways,
  };
  // LINT.ThenChange(//tools/metrics/histograms/metadata/android/enums.xml:AndroidExternalBeginFrameSourceAChoreographerImplVsyncIntervalSource,
  // //base/tracing/protos/chrome_track_event.proto:VsyncIntervalSource)

  ExternalBeginFrameSourceAndroid(uint32_t restart_id,
                                  float refresh_rate,
                                  bool requires_align_with_java);

  ExternalBeginFrameSourceAndroid(const ExternalBeginFrameSourceAndroid&) =
      delete;
  ExternalBeginFrameSourceAndroid& operator=(
      const ExternalBeginFrameSourceAndroid&) = delete;

  ~ExternalBeginFrameSourceAndroid() override;

  void OnVSync(JNIEnv* env, int64_t time_micros, int64_t period_micros);
  void UpdateRefreshRate(float refresh_rate) override;
  void SetSupportedRefreshRates(
      const base::flat_map<base::TimeDelta, float>& supported_rates) override;

 private:
  class AChoreographerImpl;

  // ExternalBeginFrameSourceClient implementation.
  void OnNeedsBeginFrames(bool needs_begin_frames) override;

  void SetEnabled(bool enabled);
  void OnVSyncImpl(int64_t time_nanos,
                   base::TimeDelta vsync_period,
                   std::optional<PossibleDeadlines> possible_deadlines);

  std::unique_ptr<AChoreographerImpl> achoreographer_;
  base::android::ScopedJavaGlobalRef<jobject> j_object_;
  BeginFrameArgsGenerator begin_frame_args_generator_;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_FRAME_SINKS_EXTERNAL_BEGIN_FRAME_SOURCE_ANDROID_H_
