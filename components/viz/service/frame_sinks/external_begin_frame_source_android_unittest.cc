// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/frame_sinks/external_begin_frame_source_android.h"

#include <string>
#include <variant>
#include <vector>

#include "base/android/android_info.h"
#include "base/android/java_handler_thread.h"
#include "base/containers/flat_map.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_base.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "components/viz/common/features.h"
#include "components/viz/test/begin_frame_source_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/functional/overload.h"
#include "ui/gfx/android/fake_achoreographer_compat.h"

namespace viz {

class ExternalBeginFrameSourceAndroidTest : public ::testing::Test,
                                            public BeginFrameObserverBase {
 public:
  ~ExternalBeginFrameSourceAndroidTest() override {
    thread_->task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(&ExternalBeginFrameSourceAndroidTest::TeardownOnThread,
                       base::Unretained(this)));
    thread_->Stop();
  }

  void CreateThread() {
    thread_ = std::make_unique<base::android::JavaHandlerThread>("TestThread");
    thread_->Start();

    thread_->task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(&ExternalBeginFrameSourceAndroidTest::InitOnThread,
                       base::Unretained(this)));
  }

  void WaitForFrames(uint32_t frame_count) {
    frames_done_event_.Reset();
    thread_->task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(
            &ExternalBeginFrameSourceAndroidTest::AddObserverOnThread,
            base::Unretained(this), frame_count));
    frames_done_event_.Wait();
  }

  ExternalBeginFrameSourceAndroid* begin_frame_source() {
    return begin_frame_source_.get();
  }

 private:
  void InitOnThread() {
    begin_frame_source_ = std::make_unique<ExternalBeginFrameSourceAndroid>(
        BeginFrameSource::kNotRestartableId, 60.f,
        /*requires_align_with_java=*/false);
  }

  void TeardownOnThread() { begin_frame_source_.reset(); }

  void AddObserverOnThread(uint32_t frame_count) {
    pending_frames_ = frame_count;
    begin_frame_source_->AddObserver(this);
  }

  bool OnBeginFrameDerivedImpl(const BeginFrameArgs& args) override {
    if (pending_frames_ == 0)
      return false;

    if (--pending_frames_ == 0) {
      begin_frame_source_->RemoveObserver(this);
      frames_done_event_.Signal();
    }
    return true;
  }
  void OnBeginFrameSourcePausedChanged(bool paused) override {}

  base::WaitableEvent frames_done_event_;
  std::unique_ptr<base::android::JavaHandlerThread> thread_;

  // Only accessed from TestThread.
  std::unique_ptr<ExternalBeginFrameSourceAndroid> begin_frame_source_;
  uint32_t pending_frames_ = 0;
};

TEST_F(ExternalBeginFrameSourceAndroidTest, DeliversFrames) {
  CreateThread();
  // Ensure we receive frames. When this returns we are no longer observing the
  // BeginFrameSource.
  WaitForFrames(10);
  // Ensure we can re-observe the same BeginFrameSource and get more frames.
  WaitForFrames(10);
}

TEST_F(ExternalBeginFrameSourceAndroidTest, DeliversFramesAfterIntervalChange) {
  CreateThread();
  // Ensure we receive frames. When this returns we are no longer observing the
  // BeginFrameSource.
  WaitForFrames(10);
  begin_frame_source()->UpdateRefreshRate(30.f);
  // Ensure we can re-observe the same BeginFrameSource and get more frames.
  WaitForFrames(10);
}

base::flat_map<base::TimeDelta, float> CreateSupportedRatesMap(
    const std::vector<float>& display_supported_refresh_rates) {
  base::flat_map<base::TimeDelta, float> supported_rates;
  for (float rate : display_supported_refresh_rates) {
    supported_rates[base::Hertz(rate)] = rate;
  }
  return supported_rates;
}

TEST_F(ExternalBeginFrameSourceAndroidTest, SetSupportedRefreshRates) {
  CreateThread();
  // Ensure begin_frame_source() is initialized by waiting for some frames
  // first.
  WaitForFrames(5);

  // Ensure we can call SetSupportedRefreshRates.
  begin_frame_source()->SetSupportedRefreshRates(
      CreateSupportedRatesMap({60.f, 90.f, 120.f}));

  // Ensure we can still receive frames.
  WaitForFrames(5);
}

// `features::kDeriveVSyncIntervalFromFrameTimelines` is disabled.
struct DeriveFeatureDisabled {};

// `features::kDeriveVSyncIntervalFromFrameTimelines` is enabled.
struct DeriveFeatureEnabled {
  // See `features::kDeriveVSyncIntervalFromFrameTimelinesModeParam`.
  std::string mode;

  // See `features::kDeriveVSyncIntervalFromFrameTimelinesSnapToleranceParam`.
  double snap_tolerance;
};

// Configuration of `features::kDeriveVSyncIntervalFromFrameTimelines`.
using DeriveFeatureConfig =
    std::variant<DeriveFeatureDisabled, DeriveFeatureEnabled>;

// Data provided to a callback posted via `AChoreographer_postFrameCallback64`.
struct FrameCallback64Data {
  int64_t frame_time_nanos;
};

using FrameCallbackData = gfx::FakeAChoreographerCompat::FrameCallbackData;

// Data provided to a callback posted via either
// `AChoreographer_postVsyncCallback` or `AChoreographer_postFrameCallback64`.
using CallbackData = std::variant<FrameCallbackData, FrameCallback64Data>;

using VsyncIntervalSource =
    ExternalBeginFrameSourceAndroid::VsyncIntervalSource;

struct AChoreographerImplTestParams {
  std::string test_name;
  DeriveFeatureConfig derive_feature_config;

  // Whether `gfx::AChoreographerCompat33::Get().supported` is true or false.
  bool compat33_supported;

  // The VSync interval that the OS provided to Chrome's callback registered via
  // `AChoreographer_registerRefreshRateCallback`.
  int64_t os_provided_vsync_interval_nanos;

  // The refresh rates that the display supports, returned by
  // `Display.getSupportedRefreshRates()`. `AChoreographerImpl` might use these
  // to snap a timeline-derived VSync interval to the closest display-supported
  // interval.
  std::vector<float> display_supported_refresh_rates;

  // Frame information that the OS provides to Chrome's callback posted via
  // either `AChoreographer_postVsyncCallback` or
  // `AChoreographer_postFrameCallback64`. If it's the former callback, the data
  // includes frame timelines, which `AChoreographerImpl` might use to derive
  // the VSync interval.
  CallbackData callback_data;

  // The VSync interval that `ExternalBeginFrameSourceAndroid` is expected
  // provide to observers, based on the parameters above.
  base::TimeDelta expected_interval;

  // The enum value of the UMA histogram
  // "Android.ExternalBeginFrameSourceAChorepherImpl.VsyncIntervalSource"
  // that `ExternalBeginFrameSourceAndroid::AChoreographerImpl` is expected to
  // emit.
  VsyncIntervalSource expected_interval_source;
};

std::vector<gfx::FakeAChoreographerCompat::FrameTimeline> CreateTimelines(
    const std::vector<int64_t>& expected_presentation_times_nanos) {
  std::vector<gfx::FakeAChoreographerCompat::FrameTimeline> timelines;
  int64_t vsync_id = 1;
  for (int64_t expected_presentation_time_nanos :
       expected_presentation_times_nanos) {
    timelines.push_back(
        {.vsync_id = vsync_id++,
         .deadline_nanos = expected_presentation_time_nanos,
         .expected_presentation_time_nanos = expected_presentation_time_nanos});
  }
  return timelines;
}

base::test::ScopedFeatureList InitFeatures(const DeriveFeatureConfig& config) {
  base::test::ScopedFeatureList feature_list;
  std::visit(
      absl::Overload{[&](const DeriveFeatureDisabled& disabled) {
                       feature_list.InitAndDisableFeature(
                           features::kDeriveVSyncIntervalFromFrameTimelines);
                     },
                     [&](const DeriveFeatureEnabled& enabled) {
                       base::FieldTrialParams params;
                       params["mode"] = enabled.mode;
                       params["snap_tolerance"] =
                           base::NumberToString(enabled.snap_tolerance);
                       feature_list.InitAndEnableFeatureWithParameters(
                           features::kDeriveVSyncIntervalFromFrameTimelines,
                           params);
                     }},
      config);
  return feature_list;
}

class AChoreographerImplTest
    : public ::testing::TestWithParam<AChoreographerImplTestParams> {
 public:
  AChoreographerImplTest()
      : scoped_feature_list_(InitFeatures(GetParam().derive_feature_config)),
        fake_compat_(/*compat_supported=*/true, GetParam().compat33_supported),
        // The `refresh_rate` argument is only used when `AChoreographerImpl` is
        // NOT supported, so we just set it to a value different than
        // `base::Hertz(GetParam().os_provided_vsync_interval_nanos)` to ensure
        // that it doesn't affect the behavior.
        begin_frame_source_(BeginFrameSource::kNotRestartableId,
                            /*refresh_rate=*/9999.f,
                            /*requires_align_with_java=*/false) {
    begin_frame_source_.AddObserver(&observer_);
  }

  ~AChoreographerImplTest() override {
    begin_frame_source_.RemoveObserver(&observer_);
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  gfx::FakeAChoreographerCompat fake_compat_;
  ExternalBeginFrameSourceAndroid begin_frame_source_;
  MockBeginFrameObserver observer_;
};

TEST_P(AChoreographerImplTest, GetVsyncInterval) {
  if (base::android::android_info::sdk_int() <
      base::android::android_info::SDK_VERSION_R) {
    GTEST_SKIP() << "AChoreographerImpl requires at least Android R";
  }

  const auto& param = GetParam();

  begin_frame_source_.SetSupportedRefreshRates(
      CreateSupportedRatesMap(param.display_supported_refresh_rates));
  fake_compat_.TriggerRefreshRateCallback(
      param.os_provided_vsync_interval_nanos);

  base::MetricsSubSampler::ScopedAlwaysSampleForTesting always_sample;
  base::HistogramTester histogram_tester;

  EXPECT_CALL(observer_, OnBeginFrame(testing::Field(&BeginFrameArgs::interval,
                                                     param.expected_interval)));

  std::visit(absl::Overload{[&](const FrameCallbackData& data) {
                              fake_compat_.TriggerVsync(data);
                            },
                            [&](const FrameCallback64Data& data) {
                              fake_compat_.TriggerFrameCallback64(
                                  data.frame_time_nanos);
                            }},
             param.callback_data);

  histogram_tester.ExpectUniqueSample(
      "Android.ExternalBeginFrameSourceAChoreographerImpl.VsyncIntervalSource",
      param.expected_interval_source, 1);
}

INSTANTIATE_TEST_SUITE_P(
    AChoreographerImplTest,
    AChoreographerImplTest,
    testing::Values(
        AChoreographerImplTestParams{
            .test_name = "TimelinesNotSupported",
            .derive_feature_config =
                DeriveFeatureEnabled{
                    .mode = "always_derive",
                    .snap_tolerance = 2.0,
                },
            .compat33_supported = false,
            .os_provided_vsync_interval_nanos = 8'333'333,
            .display_supported_refresh_rates = {60.f, 90.f},
            .callback_data =
                FrameCallback64Data{.frame_time_nanos = 10'000'000},
            .expected_interval = base::Milliseconds(8.333),
            .expected_interval_source = ExternalBeginFrameSourceAndroid::
                VsyncIntervalSource::kOsProvidedTimelineDerivedNotSupported,
        },
        AChoreographerImplTestParams{
            .test_name = "OnlyOneTimeline",
            .derive_feature_config =
                DeriveFeatureEnabled{
                    .mode = "always_derive",
                    .snap_tolerance = 2.0,
                },
            .compat33_supported = true,
            .os_provided_vsync_interval_nanos = 8'333'333,
            .display_supported_refresh_rates = {60.f, 90.f},
            .callback_data =
                FrameCallbackData{
                    .frame_time_nanos = 10'000'000,
                    .timelines = CreateTimelines({10'000'000}),
                    .preferred_index = 0,
                },
            .expected_interval = base::Milliseconds(8.333),
            .expected_interval_source = ExternalBeginFrameSourceAndroid::
                VsyncIntervalSource::kOsProvidedOnlyOneTimeline,
        },
        AChoreographerImplTestParams{
            .test_name = "DeriveFeatureDisabled",
            .derive_feature_config = DeriveFeatureDisabled{},
            .compat33_supported = true,
            .os_provided_vsync_interval_nanos = 8'333'333,
            .display_supported_refresh_rates = {60.f, 90.f},
            .callback_data =
                FrameCallbackData{
                    .frame_time_nanos = 10'000'000,
                    // Diff 16 ms.
                    .timelines = CreateTimelines({10'000'000, 26'666'666}),
                    .preferred_index = 0,
                },
            .expected_interval = base::Milliseconds(8.333),
            .expected_interval_source = ExternalBeginFrameSourceAndroid::
                VsyncIntervalSource::kOsProvidedAlways,
        },
        AChoreographerImplTestParams{
            .test_name = "AlwaysDerive_NoSupportedRates",
            .derive_feature_config =
                DeriveFeatureEnabled{
                    .mode = "always_derive",
                    .snap_tolerance = 0.1,
                },
            .compat33_supported = true,
            .os_provided_vsync_interval_nanos = 16'666'666,
            .display_supported_refresh_rates = {},
            .callback_data =
                FrameCallbackData{
                    .frame_time_nanos = 10'000'000,
                    // Diff 12 ms.
                    .timelines = CreateTimelines({10'000'000, 22'000'000}),
                    .preferred_index = 0,
                },
            .expected_interval = base::Milliseconds(12),
            .expected_interval_source = ExternalBeginFrameSourceAndroid::
                VsyncIntervalSource::kUnsnappedTimelineDerivedAlways,
        },
        AChoreographerImplTestParams{
            .test_name = "AlwaysDerive_DerivedTooShort",
            .derive_feature_config =
                DeriveFeatureEnabled{
                    .mode = "always_derive",
                    .snap_tolerance = 0.1,
                },
            .compat33_supported = true,
            .os_provided_vsync_interval_nanos = 16'666'666,
            .display_supported_refresh_rates = {60.f, 90.f},
            .callback_data =
                FrameCallbackData{
                    .frame_time_nanos = 10'000'000,
                    // Diff only 0.5 ms, which is too short.
                    .timelines = CreateTimelines({10'000'000, 10'500'000}),
                    .preferred_index = 0,
                },
            .expected_interval =
                base::Milliseconds(16.666),  // returns OS rate (60Hz)
            .expected_interval_source = ExternalBeginFrameSourceAndroid::
                VsyncIntervalSource::kOsProvidedTimelineDerivedTooShort,
        },
        AChoreographerImplTestParams{
            .test_name =
                "AlwaysDerive_BelowLowestSupported_WithinSnapTolerance",
            .derive_feature_config =
                DeriveFeatureEnabled{
                    .mode = "always_derive",
                    .snap_tolerance = 0.1,
                },
            .compat33_supported = true,
            .os_provided_vsync_interval_nanos = 8'333'333,
            .display_supported_refresh_rates = {60.f, 90.f},
            .callback_data =
                FrameCallbackData{
                    .frame_time_nanos = 10'000'000,
                    // Diff 18 ms ⇒ snaps to 16.666 ms (60 Hz).
                    .timelines = CreateTimelines({10'000'000, 28'000'000}),
                    .preferred_index = 0,
                },
            .expected_interval = base::Milliseconds(16.666),
            .expected_interval_source = ExternalBeginFrameSourceAndroid::
                VsyncIntervalSource::kSnappedTimelineDerivedAlways,
        },
        AChoreographerImplTestParams{
            .test_name =
                "AlwaysDerive_AboveHighestSupported_WithinSnapTolerance",
            .derive_feature_config =
                DeriveFeatureEnabled{
                    .mode = "always_derive",
                    .snap_tolerance = 0.1,
                },
            .compat33_supported = true,
            .os_provided_vsync_interval_nanos = 16'666'666,
            .display_supported_refresh_rates = {60.f, 90.f},
            .callback_data =
                FrameCallbackData{
                    .frame_time_nanos = 10'000'000,
                    // Diff 10.5 ms ⇒ snaps to 11.111 ms (90 Hz).
                    .timelines = CreateTimelines({10'000'000, 20'500'000}),
                    .preferred_index = 0,
                },
            .expected_interval = base::Milliseconds(11.111),
            .expected_interval_source = ExternalBeginFrameSourceAndroid::
                VsyncIntervalSource::kSnappedTimelineDerivedAlways,
        },
        AChoreographerImplTestParams{
            .test_name = "AlwaysDerive_BetweenSupportedCloserToHigher_"
                         "WithinSnapTolerance",
            .derive_feature_config =
                DeriveFeatureEnabled{
                    .mode = "always_derive",
                    .snap_tolerance = 0.3,
                },
            .compat33_supported = true,
            .os_provided_vsync_interval_nanos = 16'666'666,
            .display_supported_refresh_rates = {60.f, 90.f},
            .callback_data =
                FrameCallbackData{
                    .frame_time_nanos = 10'000'000,
                    // Diff 13.8 ms ⇒ snaps to 11.111 ms (90 Hz), because
                    // |13.8 - 11.111| < |13.8 - 16.666|.
                    .timelines = CreateTimelines({10'000'000, 23'800'000}),
                    .preferred_index = 0,
                },
            .expected_interval = base::Milliseconds(11.111),
            .expected_interval_source = ExternalBeginFrameSourceAndroid::
                VsyncIntervalSource::kSnappedTimelineDerivedAlways,
        },
        AChoreographerImplTestParams{
            .test_name = "AlwaysDerive_BetweenSupportedCloserToLower_"
                         "WithinSnapTolerance",
            .derive_feature_config =
                DeriveFeatureEnabled{
                    .mode = "always_derive",
                    .snap_tolerance = 0.3,
                },
            .compat33_supported = true,
            .os_provided_vsync_interval_nanos = 8'333'333,
            .display_supported_refresh_rates = {60.f, 90.f},
            .callback_data =
                FrameCallbackData{
                    .frame_time_nanos = 10'000'000,
                    // Diff 13.9 ms ⇒ snaps to 16.666 ms (60 Hz), because
                    // |13.9 - 16.666| < |13.9 - 11.111|.
                    .timelines = CreateTimelines({10'000'000, 23'900'000}),
                    .preferred_index = 0,
                },
            .expected_interval = base::Milliseconds(16.666),
            .expected_interval_source = ExternalBeginFrameSourceAndroid::
                VsyncIntervalSource::kSnappedTimelineDerivedAlways,
        },
        AChoreographerImplTestParams{
            .test_name =
                "AlwaysDerive_BelowLowestSupported_OutsideSnapTolerance",
            .derive_feature_config =
                DeriveFeatureEnabled{
                    .mode = "always_derive",
                    .snap_tolerance = 0.1,
                },
            .compat33_supported = true,
            .os_provided_vsync_interval_nanos = 16'666'666,
            .display_supported_refresh_rates = {60.f, 90.f},
            .callback_data =
                FrameCallbackData{
                    .frame_time_nanos = 10'000'000,
                    // Diff 20 ms ⇒ too far to snap to 16.666 ms (60 Hz).
                    .timelines = CreateTimelines({10'000'000, 30'000'000}),
                    .preferred_index = 0,
                },
            .expected_interval = base::Milliseconds(20),
            .expected_interval_source = ExternalBeginFrameSourceAndroid::
                VsyncIntervalSource::kUnsnappedTimelineDerivedAlways,
        },
        AChoreographerImplTestParams{
            .test_name =
                "AlwaysDerive_AboveHighestSupported_OutsideSnapTolerance",
            .derive_feature_config =
                DeriveFeatureEnabled{
                    .mode = "always_derive",
                    .snap_tolerance = 0.1,
                },
            .compat33_supported = true,
            .os_provided_vsync_interval_nanos = 16'666'666,
            .display_supported_refresh_rates = {60.f, 90.f},
            .callback_data =
                FrameCallbackData{
                    .frame_time_nanos = 10'000'000,
                    // Diff 9 ms ⇒ too far to snap to 11.111 ms (90 Hz).
                    .timelines = CreateTimelines({10'000'000, 19'000'000}),
                    .preferred_index = 0,
                },
            .expected_interval = base::Milliseconds(9),
            .expected_interval_source = ExternalBeginFrameSourceAndroid::
                VsyncIntervalSource::kUnsnappedTimelineDerivedAlways,
        },
        AChoreographerImplTestParams{
            .test_name = "AlwaysDerive_BetweenSupported_OutsideSnapTolerance",
            .derive_feature_config =
                DeriveFeatureEnabled{
                    .mode = "always_derive",
                    .snap_tolerance = 0.1,
                },
            .compat33_supported = true,
            .os_provided_vsync_interval_nanos = 16'666'666,
            .display_supported_refresh_rates = {60.f, 90.f},
            .callback_data =
                FrameCallbackData{
                    .frame_time_nanos = 10'000'000,
                    // Diff 14 ms ⇒ too far to snap to either 16.666 ms (60 Hz)
                    // or 11.111 ms (60 Hz).
                    .timelines = CreateTimelines({10'000'000, 24'000'000}),
                    .preferred_index = 0,
                },
            .expected_interval = base::Milliseconds(14),
            .expected_interval_source = ExternalBeginFrameSourceAndroid::
                VsyncIntervalSource::kUnsnappedTimelineDerivedAlways,
        },
        AChoreographerImplTestParams{
            .test_name = "AlwaysDerive_ZeroTolerance",
            .derive_feature_config =
                DeriveFeatureEnabled{
                    .mode = "always_derive",
                    .snap_tolerance = 0.0,
                },
            .compat33_supported = true,
            .os_provided_vsync_interval_nanos = 16'666'666,
            .display_supported_refresh_rates = {60.f, 90.f},
            .callback_data =
                FrameCallbackData{
                    .frame_time_nanos = 10'000'000,
                    // Diff 16.8 ms, but still won't snap to 16.666 ms (60 Hz).
                    .timelines = CreateTimelines({10'000'000, 26'800'000}),
                    .preferred_index = 0,
                },
            .expected_interval = base::Milliseconds(16.8),
            .expected_interval_source = ExternalBeginFrameSourceAndroid::
                VsyncIntervalSource::kUnsnappedTimelineDerivedAlways,
        },
        AChoreographerImplTestParams{
            .test_name = "DeriveIfLonger_OsProvidedLonger",
            .derive_feature_config =
                DeriveFeatureEnabled{
                    .mode = "derive_if_longer",
                    .snap_tolerance = 0.1,
                },
            .compat33_supported = true,
            .os_provided_vsync_interval_nanos = 16'666'666,
            .display_supported_refresh_rates = {60.f, 90.f},
            .callback_data =
                FrameCallbackData{
                    .frame_time_nanos = 10'000'000,
                    // Diff 12 ms ⇒ snaps to 11.111 ms (90 Hz).
                    .timelines = CreateTimelines({10'000'000, 22'000'000}),
                    .preferred_index = 0,
                },
            .expected_interval = base::Milliseconds(16.666),
            .expected_interval_source = VsyncIntervalSource::
                kOsProvidedLongerThanOrEqualToTimelineDerived},
        AChoreographerImplTestParams{
            .test_name = "DeriveIfLonger_DerivedWins_WithinSnapTolerance",
            .derive_feature_config =
                DeriveFeatureEnabled{
                    .mode = "derive_if_longer",
                    .snap_tolerance = 0.2,
                },
            .compat33_supported = true,
            .os_provided_vsync_interval_nanos = 11'111'111,
            .display_supported_refresh_rates = {60.f, 90.f},
            .callback_data =
                FrameCallbackData{
                    .frame_time_nanos = 10'000'000,
                    // Diff 15 ms ⇒ snaps to 16.666 ms (60 Hz).
                    .timelines = CreateTimelines({10'000'000, 25'000'000}),
                    .preferred_index = 0,
                },
            .expected_interval = base::Milliseconds(16.666),
            .expected_interval_source = VsyncIntervalSource::
                kSnappedTimelineDerivedLongerThanOsProvided},
        AChoreographerImplTestParams{
            .test_name = "DeriveIfLonger_DerivedWins_OutsideSnapTolerance",
            .derive_feature_config =
                DeriveFeatureEnabled{
                    .mode = "derive_if_longer",
                    .snap_tolerance = 0.1,
                },
            .compat33_supported = true,
            .os_provided_vsync_interval_nanos = 11'111'111,
            .display_supported_refresh_rates = {60.f, 90.f},
            .callback_data =
                FrameCallbackData{
                    .frame_time_nanos = 10'000'000,
                    // Diff 15 ms ⇒ too far to snap to 16.666 ms (60 Hz).
                    .timelines = CreateTimelines({10'000'000, 25'000'000}),
                    .preferred_index = 0,
                },
            .expected_interval = base::Milliseconds(15),
            .expected_interval_source = VsyncIntervalSource::
                kUnsnappedTimelineDerivedLongerThanOsProvided}),
    [](const auto& info) { return info.param.test_name; });

}  // namespace viz
