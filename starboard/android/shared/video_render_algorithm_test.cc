// Copyright 2026 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <jni.h>

#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <list>
#include <memory>
#include <random>
#include <thread>
#include <utility>
#include <vector>

#include "starboard/android/shared/fixed_frame_rate_estimator.h"
#include "starboard/android/shared/video_frame_release_platform_helper.h"
#include "starboard/android/shared/video_frame_tracker.h"
#include "starboard/android/shared/video_render_algorithm_android_v2.h"
#include "starboard/android/shared/video_render_algorithm_host.h"
#include "starboard/shared/starboard/player/filter/media_time_provider.h"
#include "starboard/shared/starboard/player/filter/video_frame_internal.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/jni_zero/jni_zero.h"

namespace starboard {
namespace {

namespace v2 = video_render_algorithm_v2;
using ::jni_zero::AttachCurrentThread;

class FakeMediaTimeProvider : public MediaTimeProvider {
 public:
  void Play() override { info_.is_playing = true; }
  void Pause() override { info_.is_playing = false; }
  void SetPlaybackRate(double rate) override { info_.playback_rate = rate; }
  void Seek(int64_t pts) override { info_.media_time = pts; }

  MediaTimeInfo GetCurrentMediaTime() override {
    ++query_count;
    return info_;
  }

  int64_t GetAudioWriteHead() override { return info_.media_time; }
  int64_t AdjustTimestampToAudioClock(int64_t ts) override { return ts; }

  MediaTimeInfo info_{/*media_time=*/0, /*is_playing=*/true,
                      /*is_eos_played=*/false, /*is_underflow=*/false,
                      /*playback_rate=*/1.0};
  int query_count = 0;
};

class FakeRenderHost : public VideoRenderAlgorithmHost {
 public:
  void SetPlaybackRate(double playback_rate) override {
    codec_playback_rate = playback_rate;
    ++set_playback_rate_calls;
  }

  SbPlayerOutputMode GetOutputMode() const override { return output_mode; }

  jni_zero::ScopedJavaLocalRef<jobject> AcquireJavaSurface() override {
    ++acquire_surface_calls;
    if (!surface) {
      return {};
    }
    return jni_zero::ScopedJavaLocalRef<jobject>(AttachCurrentThread(),
                                                 surface);
  }

  uint64_t GetSurfaceGeneration() const override { return surface_generation; }

  void SetNextTickWaitDurationUs(int64_t wait_duration_us) override {
    last_wait_duration_us = wait_duration_us;
    ++set_wait_calls;
  }

  bool is_playback_paused() const override { return paused; }

  SbPlayerOutputMode output_mode = kSbPlayerOutputModePunchOut;
  bool paused = false;
  double codec_playback_rate = 1.0;
  int set_playback_rate_calls = 0;
  int acquire_surface_calls = 0;
  uint64_t surface_generation = 1;
  int64_t last_wait_duration_us = -1;
  int set_wait_calls = 0;
  jni_zero::ScopedJavaGlobalRef<jobject> surface;
};

class FakeReleasePlatform : public VideoFrameReleasePlatform {
 public:
  struct State {
    VsyncSnapshot vsync{/*sampled_vsync_time_ns=*/1'000'000'000LL,
                        /*vsync_duration_ns=*/16'666'667LL,
                        /*vsync_offset_ns=*/13'333'333LL};
    bool playback_active = true;
    int set_active_calls = 0;
    int update_surface_calls = 0;
    int clear_surface_calls = 0;
    float last_frame_rate = 0.0f;
    bool last_seamless_only = false;
    int64_t sample_count = 1;
  };

  explicit FakeReleasePlatform(std::shared_ptr<State> state)
      : state_(std::move(state)) {}

  VsyncSnapshot GetVsyncSnapshot() const override { return state_->vsync; }

  void SetPlaybackActive(bool active) override {
    state_->playback_active = active;
    ++state_->set_active_calls;
  }

  void UpdateSurfaceFrameRate(const jni_zero::JavaRef<jobject>&,
                              float frame_rate,
                              bool seamless_only) override {
    state_->last_frame_rate = frame_rate;
    state_->last_seamless_only = seamless_only;
    ++state_->update_surface_calls;
  }

  void ClearSurfaceFrameRate() override {
    state_->last_frame_rate = 0.0f;
    ++state_->clear_surface_calls;
  }

  int64_t vsync_sample_count() const override { return state_->sample_count; }

 private:
  std::shared_ptr<State> state_;
};

class ScopedTestJavaSurface {
 public:
  ScopedTestJavaSurface() {
    JNIEnv* env = AttachCurrentThread();
    jni_zero::ScopedJavaLocalRef<jclass> st_cls(
        env, env->FindClass("android/graphics/SurfaceTexture"));
    jmethodID st_ctor = env->GetMethodID(st_cls.obj(), "<init>", "(I)V");
    jni_zero::ScopedJavaLocalRef<jobject> st(
        env, env->NewObject(st_cls.obj(), st_ctor, 0));

    jni_zero::ScopedJavaLocalRef<jclass> surf_cls(
        env, env->FindClass("android/view/Surface"));
    jmethodID surf_ctor = env->GetMethodID(
        surf_cls.obj(), "<init>", "(Landroid/graphics/SurfaceTexture;)V");
    jni_zero::ScopedJavaLocalRef<jobject> surf(
        env, env->NewObject(surf_cls.obj(), surf_ctor, st.obj()));

    surface_texture_.Reset(st);
    surface_.Reset(surf);
  }

  ~ScopedTestJavaSurface() {
    JNIEnv* env = AttachCurrentThread();
    if (surface_) {
      jni_zero::ScopedJavaLocalRef<jclass> surf_cls(
          env, env->FindClass("android/view/Surface"));
      jmethodID rel = env->GetMethodID(surf_cls.obj(), "release", "()V");
      env->CallVoidMethod(surface_.obj(), rel);
      surface_.Reset();
    }
    if (surface_texture_) {
      jni_zero::ScopedJavaLocalRef<jclass> st_cls(
          env, env->FindClass("android/graphics/SurfaceTexture"));
      jmethodID rel = env->GetMethodID(st_cls.obj(), "release", "()V");
      env->CallVoidMethod(surface_texture_.obj(), rel);
      surface_texture_.Reset();
    }
  }

  const jni_zero::ScopedJavaGlobalRef<jobject>& surface() const {
    return surface_;
  }

 private:
  jni_zero::ScopedJavaGlobalRef<jobject> surface_texture_;
  jni_zero::ScopedJavaGlobalRef<jobject> surface_;
};

struct DrawnFrame {
  int64_t pts_us;
  int64_t release_ns;
};

VideoRendererSink::DrawFrameCB MakeRecorder(std::vector<DrawnFrame>* out) {
  return [out](const scoped_refptr<VideoFrame>& frame, int64_t release_ns) {
    out->push_back({frame->timestamp(), release_ns});
    return VideoRendererSink::kReleased;
  };
}

// ============================================================================
// Mask Normalization & Selection Tests
// ============================================================================

TEST(VideoRenderAlgorithmV2Test, MaskNormalizationAndHoldbackRules) {
  EXPECT_FALSE(ShouldUseVideoRenderAlgorithmAndroidV2(0, 34));
  EXPECT_FALSE(ShouldUseVideoRenderAlgorithmAndroidV2(-1, 34));
  // Bit 0 is required to activate V2.
  EXPECT_FALSE(ShouldUseVideoRenderAlgorithmAndroidV2(2, 34));

  // Bit 30 forces legacy even when all V2 bits are set.
  EXPECT_FALSE(ShouldUseVideoRenderAlgorithmAndroidV2(
      static_cast<int>(0x3FFFu | v2::kForceLegacyAlgorithm), 34));

  // Bit 13 -> Bit 7 -> Bit 6 dependency chain.
  const uint32_t m13 = NormalizeVideoRenderAlgorithmMask(
      v2::kEnableNewAlgorithm | v2::kSurfaceFrameRateSeamlessOnly, 34);
  EXPECT_TRUE(m13 & v2::kSurfaceFrameRateSeamlessOnly);
  EXPECT_TRUE(m13 & v2::kSurfaceSetFrameRate);
  EXPECT_TRUE(m13 & v2::kDynamicDisplayRefreshRate);

  // Bit 4 -> Bit 3 dependency.
  const uint32_t m4 = NormalizeVideoRenderAlgorithmMask(
      v2::kEnableNewAlgorithm | v2::kFixedFrameRateEstimator, 34);
  EXPECT_TRUE(m4 & v2::kIncrementalFloatSpeedSmoother);

  // API < 30 strips Surface.setFrameRate bits while keeping Bit 6.
  const uint32_t m_api29 = NormalizeVideoRenderAlgorithmMask(
      v2::kEnableNewAlgorithm | v2::kSurfaceFrameRateSeamlessOnly, 29);
  EXPECT_FALSE(m_api29 & v2::kSurfaceSetFrameRate);
  EXPECT_FALSE(m_api29 & v2::kSurfaceFrameRateSeamlessOnly);
  EXPECT_TRUE(m_api29 & v2::kDynamicDisplayRefreshRate);

  // API < 33 strips FrameTimeline VsyncCallback bit.
  const uint32_t m_api31 = NormalizeVideoRenderAlgorithmMask(
      v2::kEnableNewAlgorithm | v2::kVsyncCallbackFrameTimeline, 31);
  EXPECT_FALSE(m_api31 & v2::kVsyncCallbackFrameTimeline);
}

// ============================================================================
// FixedFrameRateEstimator Unit Tests (Media3 Parity)
// ============================================================================

TEST(FixedFrameRateEstimatorTest,
     SyncsRecoversFromSingleOutlierAndSwitchesFps) {
  FixedFrameRateEstimator estimator;
  constexpr int64_t k30FpsNs = 33'333'333LL;
  int64_t pts_ns = 0;
  estimator.OnNextFrame(pts_ns);
  EXPECT_FALSE(estimator.IsSynced());

  for (int i = 0;
       i <
       FixedFrameRateEstimator::kConsecutiveMatchingFrameDurationsForSync - 1;
       ++i) {
    pts_ns += k30FpsNs;
    estimator.OnNextFrame(pts_ns);
    EXPECT_FALSE(estimator.IsSynced());
  }
  pts_ns += k30FpsNs;
  estimator.OnNextFrame(pts_ns);
  EXPECT_TRUE(estimator.IsSynced());
  EXPECT_EQ(estimator.GetFrameDurationNs(), k30FpsNs);
  EXPECT_NEAR(estimator.GetFrameRate(), 30.0f, 0.05f);

  // Single outlier frame gap drops sync temporarily, then recovers after 15
  // matching durations without losing the reported frame rate estimate.
  pts_ns += k30FpsNs * 2;
  estimator.OnNextFrame(pts_ns);
  EXPECT_FALSE(estimator.IsSynced());
  EXPECT_NEAR(estimator.GetFrameRate(), 30.0f, 0.05f);

  for (int i = 0;
       i < FixedFrameRateEstimator::kConsecutiveMatchingFrameDurationsForSync;
       ++i) {
    pts_ns += k30FpsNs;
    estimator.OnNextFrame(pts_ns);
  }
  EXPECT_TRUE(estimator.IsSynced());

  // Switch to 60fps: candidate matcher promotes once 15 matching frames arrive.
  constexpr int64_t k60FpsNs = 16'666'667LL;
  for (int i = 0;
       i < FixedFrameRateEstimator::kConsecutiveMatchingFrameDurationsForSync;
       ++i) {
    pts_ns += k60FpsNs;
    estimator.OnNextFrame(pts_ns);
  }
  EXPECT_TRUE(estimator.IsSynced());
  EXPECT_EQ(estimator.GetFrameDurationNs(), k60FpsNs);
  EXPECT_NEAR(estimator.GetFrameRate(), 60.0f, 0.05f);
}

TEST(FixedFrameRateEstimatorTest, HandlesMillisecondContainerPrecisionAndVfr) {
  FixedFrameRateEstimator estimator;
  constexpr int64_t k23976Ns = 41'708'333LL;
  for (int i = 0; i <= 30; ++i) {
    const int64_t ms_rounded_ns = ((i * k23976Ns) / 1'000'000LL) * 1'000'000LL;
    estimator.OnNextFrame(ms_rounded_ns);
  }
  EXPECT_TRUE(estimator.IsSynced());
  EXPECT_LT(std::abs(estimator.GetFrameDurationNs() - k23976Ns), 500'000LL);
  EXPECT_NEAR(estimator.GetFrameRate(), 23.976f, 0.1f);

  // VFR jittering >1ms never falsely locks sync, and clears estimate after 30
  // unsynced frames.
  int64_t pts_ns = 2'000'000'000LL;
  for (int i = 0; i < 35; ++i) {
    pts_ns += (i % 2 == 0) ? 25'000'000LL : 41'000'000LL;
    estimator.OnNextFrame(pts_ns);
    EXPECT_FALSE(estimator.IsSynced());
  }
  EXPECT_EQ(estimator.GetFrameRate(), FixedFrameRateEstimator::kNoFrameRate);
}

// ============================================================================
// Bit-by-Bit Functional & Edge-Case Tests
// ============================================================================

TEST(VideoRenderAlgorithmV2Test, Mask1LegacyParityHoldsWhenPausedAndDropsLate) {
  FakeRenderHost host;
  FakeMediaTimeProvider clock;
  auto platform_state = std::make_shared<FakeReleasePlatform::State>();
  int64_t now_ns = 1'000'000'000LL;

  VideoRenderAlgorithmAndroidV2 alg(
      &host, /*frame_tracker=*/nullptr, v2::kEnableNewAlgorithm, 34,
      std::make_unique<FakeReleasePlatform>(platform_state),
      [&] { return now_ns; });

  std::vector<DrawnFrame> drawn;
  std::list<scoped_refptr<VideoFrame>> frames{
      make_scoped_refptr<VideoFrame>(0),
      make_scoped_refptr<VideoFrame>(33'333)};

  // Paused on Mask=1 (no Bit 1): renders nothing.
  clock.info_.is_playing = false;
  alg.Render(&clock, &frames, MakeRecorder(&drawn));
  EXPECT_TRUE(drawn.empty());
  EXPECT_EQ(frames.size(), 2U);

  // Playing: frame at -40ms is dropped, frame at +10ms is drawn.
  clock.info_.is_playing = true;
  clock.info_.media_time = 40'000;
  frames.clear();
  frames.push_back(make_scoped_refptr<VideoFrame>(0));
  frames.push_back(make_scoped_refptr<VideoFrame>(50'000));
  alg.Render(&clock, &frames, MakeRecorder(&drawn));
  EXPECT_EQ(alg.GetDroppedFrames(), 1);
  ASSERT_EQ(drawn.size(), 1U);
  EXPECT_EQ(drawn[0].pts_us, 50'000);
}

TEST(VideoRenderAlgorithmV2Test,
     Bit1_PausedSeekDrawsFirstFrameOutsideWindowAndHoldsSecondUntilAudioMoves) {
  FakeRenderHost host;
  FakeMediaTimeProvider clock;
  auto platform_state = std::make_shared<FakeReleasePlatform::State>();
  int64_t now_ns = 2'000'000'000LL;

  VideoRenderAlgorithmAndroidV2 alg(
      &host, /*frame_tracker=*/nullptr,
      v2::kEnableNewAlgorithm | v2::kFirstFrameImmediateAndAudioGate, 34,
      std::make_unique<FakeReleasePlatform>(platform_state),
      [&] { return now_ns; });

  // Simulate paused seek to 10.0s where first post-seek keyframe is at 10.08s
  // (+80ms > +50ms ready threshold).
  host.paused = true;
  clock.info_.is_playing = false;
  clock.info_.media_time = 10'000'000;
  alg.Seek(10'000'000);

  std::list<scoped_refptr<VideoFrame>> frames{
      make_scoped_refptr<VideoFrame>(10'080'000),
      make_scoped_refptr<VideoFrame>(10'113'333)};
  std::vector<DrawnFrame> drawn;
  alg.Render(&clock, &frames, MakeRecorder(&drawn));

  ASSERT_EQ(drawn.size(), 1U);
  EXPECT_EQ(drawn[0].pts_us, 10'080'000);
  EXPECT_EQ(drawn[0].release_ns, now_ns);
  EXPECT_EQ(frames.size(), 1U);

  // Unpause, but AudioTrack has not consumed frames yet (media_time == 10.0s):
  // Frame 1 MUST remain held to prevent startup jump-start freeze.
  host.paused = false;
  clock.info_.is_playing = true;
  clock.info_.playback_rate = 1.0;
  now_ns += 50'000'000LL;
  alg.Render(&clock, &frames, MakeRecorder(&drawn));
  EXPECT_EQ(drawn.size(), 1U);
  EXPECT_EQ(frames.size(), 1U);

  // Once AudioTrack advances past 10.0s to 10.09s, Frame 1 releases cleanly.
  clock.info_.media_time = 10'090'000;
  alg.Render(&clock, &frames, MakeRecorder(&drawn));
  EXPECT_EQ(drawn.size(), 2U);
  EXPECT_TRUE(frames.empty());
}

TEST(VideoRenderAlgorithmV2Test,
     Bit2_StarvationFloorForcesProgressEvery100msUnderLag) {
  FakeRenderHost host;
  FakeMediaTimeProvider clock;
  auto platform_state = std::make_shared<FakeReleasePlatform::State>();
  int64_t now_ns = 1'000'000'000LL;

  VideoRenderAlgorithmAndroidV2 alg(
      &host, /*frame_tracker=*/nullptr,
      v2::kEnableNewAlgorithm | v2::kFirstFrameImmediateAndAudioGate |
          v2::kStarvationFloor100ms,
      34, std::make_unique<FakeReleasePlatform>(platform_state),
      [&] { return now_ns; });

  std::vector<DrawnFrame> drawn;
  std::list<scoped_refptr<VideoFrame>> frames{
      make_scoped_refptr<VideoFrame>(0)};
  alg.Render(&clock, &frames, MakeRecorder(&drawn));
  ASSERT_EQ(drawn.size(), 1U);

  // Simulate sustained 60ms decode lag across 500ms at 60fps (30 frames).
  for (int i = 1; i <= 30; ++i) {
    const int64_t pts_us = i * 16'667;
    now_ns = 1'000'000'000LL + (pts_us + 60'000) * 1'000LL;
    clock.info_.media_time = pts_us + 60'000;
    frames.push_back(make_scoped_refptr<VideoFrame>(pts_us));
    alg.Render(&clock, &frames, MakeRecorder(&drawn));
  }

  const auto stats = alg.GetStats();
  // ~4-5 forced frames over 500ms (~10 fps progress instead of 0 fps freeze).
  EXPECT_GE(stats.forced_by_starvation_floor, 4);
  EXPECT_LE(stats.forced_by_starvation_floor, 6);

  // If audio clock stalls completely at the last rendered media time,
  // starvation floor must NOT burn additional frames.
  clock.info_.media_time = 27 * 16'667 + 60'000;
  {
    now_ns += 200'000'000LL;
    std::list<scoped_refptr<VideoFrame>> f31{
        make_scoped_refptr<VideoFrame>(31 * 16'667)};
    alg.Render(&clock, &f31, MakeRecorder(&drawn));
  }
  const int64_t forced_before = alg.GetStats().forced_by_starvation_floor;
  now_ns += 200'000'000LL;
  // clock.info_.media_time intentionally kept unchanged!
  frames.clear();
  frames.push_back(make_scoped_refptr<VideoFrame>(32 * 16'667));
  alg.Render(&clock, &frames, MakeRecorder(&drawn));
  EXPECT_EQ(alg.GetStats().forced_by_starvation_floor, forced_before);
}

TEST(VideoRenderAlgorithmV2Test,
     Bit3And4_FloatSpeed1Point1xHasZeroResyncsAnd0Point1xDoesNotDivideByZero) {
  FakeRenderHost host;
  FakeMediaTimeProvider clock;
  auto platform_state = std::make_shared<FakeReleasePlatform::State>();
  int64_t now_ns = 1'000'000'000LL;

  VideoRenderAlgorithmAndroidV2 v2_alg(
      &host, /*frame_tracker=*/nullptr,
      v2::kEnableNewAlgorithm | v2::kFirstFrameImmediateAndAudioGate |
          v2::kFixedFrameRateEstimator,
      34, std::make_unique<FakeReleasePlatform>(platform_state),
      [&] { return now_ns; });

  clock.info_.playback_rate = 1.1;
  std::vector<DrawnFrame> drawn;
  // Run 180 frames (6 seconds) of 30fps at 1.1x speed.
  for (int i = 0; i < 180; ++i) {
    const int64_t pts_us = i * 33'333;
    const int64_t wall_offset_us = static_cast<int64_t>(pts_us / 1.1);
    now_ns = 1'000'000'000LL +
             std::max<int64_t>(0, wall_offset_us - 10'000) * 1'000LL;
    clock.info_.media_time =
        i == 0
            ? 0
            : static_cast<int64_t>((now_ns - 1'000'000'000LL) / 1'000.0 * 1.1);
    std::list<scoped_refptr<VideoFrame>> frames{
        make_scoped_refptr<VideoFrame>(pts_us)};
    v2_alg.Render(&clock, &frames, MakeRecorder(&drawn));
  }
  EXPECT_EQ(drawn.size(), 180U);
  EXPECT_EQ(v2_alg.GetStats().smoother_resets, 0);
  EXPECT_EQ(v2_alg.GetStats().dropped_late_frames, 0);

  // Verify 0.1x speed (which crashed legacy Java with divide-by-zero) works.
  clock.info_.playback_rate = 0.1;
  for (int i = 180; i < 190; ++i) {
    const int64_t pts_us = i * 33'333;
    clock.info_.media_time = pts_us - 1'000;
    now_ns += 333'330'000LL;
    std::list<scoped_refptr<VideoFrame>> frames{
        make_scoped_refptr<VideoFrame>(pts_us)};
    v2_alg.Render(&clock, &frames, MakeRecorder(&drawn));
  }
  EXPECT_EQ(drawn.size(), 190U);
}

TEST(
    VideoRenderAlgorithmV2Test,
    Bit5_SkipsDuplicateVsyncWithoutFalseTrackerDropsAndScalesStarvationTo85ms) {
  FakeRenderHost host;
  FakeMediaTimeProvider clock;
  VideoFrameTracker tracker(/*max_pending_frames_size=*/256);
  auto platform_state = std::make_shared<FakeReleasePlatform::State>();
  int64_t now_ns = 1'000'000'000LL;

  VideoRenderAlgorithmAndroidV2 alg(
      &host, &tracker,
      v2::kEnableNewAlgorithm | v2::kFirstFrameImmediateAndAudioGate |
          v2::kStarvationFloor100ms | v2::kFixedFrameRateEstimator |
          v2::kSkipSameVsyncReleaseTime,
      34, std::make_unique<FakeReleasePlatform>(platform_state),
      [&] { return now_ns; });

  // 29.97fps at 4.0x (~120 output frames/s) on a 60Hz display: ~half of frames
  // snap to the same VSYNC and must be skipped cleanly without false drops.
  clock.info_.playback_rate = 4.0;
  std::vector<DrawnFrame> drawn;
  for (int i = 0; i < 60; ++i) {
    const int64_t pts_us = i * 33'367;
    tracker.OnInputBuffer(pts_us);
    const int64_t media_us =
        (i == 0) ? 0 : std::max<int64_t>(1, pts_us - 20'000);
    clock.info_.media_time = media_us;
    now_ns = 1'000'000'000LL + (media_us / 4) * 1'000LL;
    std::list<scoped_refptr<VideoFrame>> frames{
        make_scoped_refptr<VideoFrame>(pts_us)};
    const size_t prev_drawn = drawn.size();
    alg.Render(&clock, &frames, MakeRecorder(&drawn));
    if (drawn.size() > prev_drawn) {
      tracker.OnFrameRendered(pts_us);
    }
  }

  EXPECT_GT(alg.GetStats().skipped_same_vsync, 20);
  EXPECT_EQ(alg.GetDroppedFrames(), 0);

  // Verify 0.85f starvation scaling: make the frame right after a same-VSYNC
  // skip arrive late at +90ms (>= 85ms, < 100ms) after last render.
  alg.Seek(0);
  drawn.clear();
  clock.info_.playback_rate = 2.0;
  clock.info_.media_time = 0;
  now_ns = 2'000'000'000LL;
  platform_state->vsync = {2'000'000'000LL, 16'666'667LL, 13'333'333LL};

  // Frame 0 immediate.
  {
    std::list<scoped_refptr<VideoFrame>> f{make_scoped_refptr<VideoFrame>(0)};
    alg.Render(&clock, &f, MakeRecorder(&drawn));
  }
  // Frame 1 & Frame 2 both snap to the same VSYNC -> Frame 1 drawn, Frame 2
  // skipped.
  clock.info_.media_time = 10'000;
  now_ns = 2'005'000'000LL;
  {
    std::list<scoped_refptr<VideoFrame>> f{
        make_scoped_refptr<VideoFrame>(20'000),
        make_scoped_refptr<VideoFrame>(24'000)};
    alg.Render(&clock, &f, MakeRecorder(&drawn));
  }
  ASSERT_GE(alg.GetStats().skipped_same_vsync, 1);

  // 90ms after Frame 1 was drawn (between 85ms and 100ms), a late frame
  // arrives: scaled 85ms starvation floor forces it to draw!
  now_ns += 90'000'000LL;
  clock.info_.media_time = 250'000;
  {
    std::list<scoped_refptr<VideoFrame>> f{
        make_scoped_refptr<VideoFrame>(100'000)};
    alg.Render(&clock, &f, MakeRecorder(&drawn));
  }
  EXPECT_EQ(alg.GetStats().forced_by_starvation_floor, 1);
}

TEST(
    VideoRenderAlgorithmV2Test,
    Bit6_DynamicRefreshSwitch60To23976HzS6StarHasZeroDuplicateVsyncCollisions) {
  constexpr int64_t k23976Ns = 41'708'333LL;
  constexpr int64_t k60HzNs = 16'666'667LL;

  // Sweep 24 distinct switch phases across the full 41.7ms 23.976Hz period.
  int stale_collisions = 0;
  int dynamic_collisions = 0;

  for (int phase_idx = 0; phase_idx < 24; ++phase_idx) {
    const int64_t phase_offset_ns = (k23976Ns * phase_idx) / 24;

    for (bool dynamic_refresh : {false, true}) {
      FakeRenderHost host;
      FakeMediaTimeProvider clock;
      auto state = std::make_shared<FakeReleasePlatform::State>();
      state->vsync = {1'000'000'000LL, k60HzNs, (k60HzNs * 80) / 100};
      int64_t now_ns = 1'000'000'000LL;

      const uint32_t mask =
          v2::kEnableNewAlgorithm | v2::kFirstFrameImmediateAndAudioGate |
          v2::kFixedFrameRateEstimator |
          (dynamic_refresh ? v2::kDynamicDisplayRefreshRate : 0u);
      VideoRenderAlgorithmAndroidV2 alg(
          &host, nullptr, mask, 34,
          std::make_unique<FakeReleasePlatform>(state), [&] { return now_ns; });

      std::vector<DrawnFrame> drawn;
      for (int i = 0; i < 90; ++i) {
        if (i == 30) {
          // Display switches to 23.976Hz at arbitrary phase `phase_offset_ns`.
          state->vsync.sampled_vsync_time_ns =
              1'000'000'000LL + i * k23976Ns + phase_offset_ns;
          if (dynamic_refresh) {
            state->vsync.vsync_duration_ns = k23976Ns;
            state->vsync.vsync_offset_ns = (k23976Ns * 80) / 100;
          }
        }
        const int64_t pts_us = (i * k23976Ns) / 1'000LL;
        now_ns =
            1'000'000'000LL + std::max<int64_t>(0, i * k23976Ns - 10'000'000LL);
        clock.info_.media_time =
            (i == 0) ? 0 : std::max<int64_t>(1, pts_us - 10'000);
        std::list<scoped_refptr<VideoFrame>> f{
            make_scoped_refptr<VideoFrame>(pts_us)};
        alg.Render(&clock, &f, MakeRecorder(&drawn));
      }

      // Project post-switch release timestamps onto the actual 23.976Hz VSYNC
      // slots to detect [0, 2, 0, 2] SurfaceFlinger frame collisions.
      int64_t prev_slot = -1;
      for (size_t j = 35; j < drawn.size(); ++j) {
        const int64_t rel = drawn[j].release_ns -
                            (1'000'000'000LL + 30 * k23976Ns + phase_offset_ns);
        const int64_t slot = static_cast<int64_t>(std::floor(
                                 static_cast<double>(rel) / k23976Ns)) +
                             1;
        if (j > 35 && slot == prev_slot) {
          if (dynamic_refresh) {
            ++dynamic_collisions;
          } else {
            ++stale_collisions;
          }
        }
        prev_slot = slot;
      }
    }
  }

  EXPECT_GT(stale_collisions, 0);
  EXPECT_EQ(dynamic_collisions, 0);
}

TEST(VideoRenderAlgorithmV2Test,
     Bit7And13_SurfaceSetFrameRateVotesScalesResetsAndReVotesOnSurfaceChange) {
  ScopedTestJavaSurface test_surface;
  FakeRenderHost host;
  host.surface = test_surface.surface();
  FakeMediaTimeProvider clock;
  auto state = std::make_shared<FakeReleasePlatform::State>();
  int64_t now_ns = 1'000'000'000LL;

  VideoRenderAlgorithmAndroidV2 alg(
      &host, nullptr,
      v2::kEnableNewAlgorithm | v2::kFirstFrameImmediateAndAudioGate |
          v2::kFixedFrameRateEstimator | v2::kSurfaceFrameRateSeamlessOnly,
      34, std::make_unique<FakeReleasePlatform>(state), [&] { return now_ns; });

  std::vector<DrawnFrame> drawn;
  for (int i = 0; i < 20; ++i) {
    const int64_t pts_us = i * 41'708;
    clock.info_.media_time =
        (i == 0) ? 0 : std::max<int64_t>(1, pts_us - 10'000);
    now_ns = 1'000'000'000LL + clock.info_.media_time * 1'000LL;
    std::list<scoped_refptr<VideoFrame>> f{
        make_scoped_refptr<VideoFrame>(pts_us)};
    alg.Render(&clock, &f, MakeRecorder(&drawn));
  }

  EXPECT_EQ(state->update_surface_calls, 1);
  EXPECT_TRUE(state->last_seamless_only);
  EXPECT_NEAR(state->last_frame_rate, 23.976f, 0.05f);
  // Steady subsequent frames make zero AcquireJavaSurface / JNI calls!
  const int acquire_before = host.acquire_surface_calls;
  {
    const int64_t pts_us = 20 * 41'708;
    clock.info_.media_time = pts_us - 10'000;
    now_ns = 1'000'000'000LL + clock.info_.media_time * 1'000LL;
    std::list<scoped_refptr<VideoFrame>> f{
        make_scoped_refptr<VideoFrame>(pts_us)};
    alg.Render(&clock, &f, MakeRecorder(&drawn));
  }
  EXPECT_EQ(host.acquire_surface_calls, acquire_before);

  // Speed change scales voted Surface frame rate.
  clock.info_.playback_rate = 1.5;
  {
    const int64_t pts_us = 21 * 41'708;
    clock.info_.media_time = pts_us - 10'000;
    now_ns += 27'800'000LL;
    std::list<scoped_refptr<VideoFrame>> f{
        make_scoped_refptr<VideoFrame>(pts_us)};
    alg.Render(&clock, &f, MakeRecorder(&drawn));
  }
  EXPECT_NEAR(state->last_frame_rate, 23.976f * 1.5f, 0.1f);

  // Surface replacement (surface_generation increment) re-votes on new surface.
  ++host.surface_generation;
  {
    const int64_t pts_us = 22 * 41'708;
    clock.info_.media_time = pts_us - 10'000;
    now_ns += 27'800'000LL;
    std::list<scoped_refptr<VideoFrame>> f{
        make_scoped_refptr<VideoFrame>(pts_us)};
    alg.Render(&clock, &f, MakeRecorder(&drawn));
  }
  EXPECT_EQ(state->update_surface_calls, 3);

  // Seek resets Surface.setFrameRate vote to 0.
  alg.Seek(0);
  EXPECT_EQ(state->last_frame_rate, 0.0f);
  EXPECT_GE(state->clear_surface_calls, 1);
}

TEST(VideoRenderAlgorithmV2Test,
     Bit8_VsyncMidpointHysteresisEliminatesBoundaryChatter) {
  constexpr int64_t k50HzNs = 20'000'000LL;
  int chatter_without_hyst = 0;
  int chatter_with_hyst = 0;

  for (bool enable_hyst : {false, true}) {
    FakeRenderHost host;
    FakeMediaTimeProvider clock;
    auto state = std::make_shared<FakeReleasePlatform::State>();
    state->vsync = {1'000'000'000LL, k50HzNs, 16'000'000LL};
    int64_t now_ns = 1'000'000'000LL;

    const uint32_t mask = v2::kEnableNewAlgorithm |
                          v2::kIncrementalFloatSpeedSmoother |
                          (enable_hyst ? v2::kVsyncMidpointHysteresis : 0u);
    VideoRenderAlgorithmAndroidV2 alg(
        &host, nullptr, mask, 34, std::make_unique<FakeReleasePlatform>(state),
        [&] { return now_ns; });

    std::vector<DrawnFrame> drawn;
    // Place unadjusted release times right on the 10ms midpoint of a 20ms VSYNC
    // grid with +-50us VSYNC sample jitter.
    for (int i = 0; i < 100; ++i) {
      const int64_t vsync_jitter_ns = (i % 2 == 0) ? 50'000LL : -50'000LL;
      state->vsync.sampled_vsync_time_ns =
          1'000'000'000LL + i * k50HzNs + vsync_jitter_ns;
      const int64_t pts_us = i * 20'000;
      now_ns = 1'000'000'000LL + i * k50HzNs;
      clock.info_.media_time = pts_us - 10'000;  // +10ms == exact half-VSYNC!
      std::list<scoped_refptr<VideoFrame>> f{
          make_scoped_refptr<VideoFrame>(pts_us)};
      alg.Render(&clock, &f, MakeRecorder(&drawn));
    }

    for (size_t i = 2; i < drawn.size(); ++i) {
      const int64_t step_vsyncs = static_cast<int64_t>(std::llround(
          static_cast<double>(drawn[i].release_ns - drawn[i - 1].release_ns) /
          k50HzNs));
      if (step_vsyncs != 1) {
        if (enable_hyst) {
          ++chatter_with_hyst;
        } else {
          ++chatter_without_hyst;
        }
      }
    }
  }

  EXPECT_GT(chatter_without_hyst, 50);
  EXPECT_EQ(chatter_with_hyst, 0);
}

TEST(VideoRenderAlgorithmV2Test,
     Bit9AndPlatformHelper_RealJniLifecycleAndSurfaceVoteOnAndroid) {
  ScopedTestJavaSurface test_surface;
  VideoFrameReleasePlatformHelper helper(
      /*track_display_changes=*/true, /*prefer_vsync_callback_v33=*/true,
      /*pause_sampler_when_inactive=*/true);

  helper.SetPlaybackActive(true);
  helper.UpdateSurfaceFrameRate(test_surface.surface(), 23.976f,
                                /*seamless_only=*/true);
  helper.UpdateSurfaceFrameRate(test_surface.surface(), 23.976f,
                                /*seamless_only=*/false);
  std::this_thread::sleep_for(std::chrono::milliseconds(50));
  const auto snap = helper.GetVsyncSnapshot();
  EXPECT_GT(snap.vsync_duration_ns, 0);

  helper.SetPlaybackActive(false);
  helper.ClearSurfaceFrameRate();
}

TEST(VideoRenderAlgorithmV2Test,
     Bit10_AudioEosDrawsFinalFrameAndNormalPlaybackProtectsLastFrameBeforeEos) {
  FakeRenderHost host;
  FakeMediaTimeProvider clock;
  VideoFrameTracker tracker(100);
  auto state = std::make_shared<FakeReleasePlatform::State>();
  int64_t now_ns = 1'000'000'000LL;

  VideoRenderAlgorithmAndroidV2 alg(
      &host, &tracker,
      v2::kEnableNewAlgorithm | v2::kFirstFrameImmediateAndAudioGate |
          v2::kAudioEosDrawLastAndLastFrameGuard,
      34, std::make_unique<FakeReleasePlatform>(state), [&] { return now_ns; });

  for (int64_t pts : {10'000, 20'000, 30'000}) {
    tracker.OnInputBuffer(pts);
  }
  clock.info_.media_time = 10'000;
  clock.info_.is_eos_played = true;

  std::list<scoped_refptr<VideoFrame>> frames{
      make_scoped_refptr<VideoFrame>(10'000),
      make_scoped_refptr<VideoFrame>(20'000),
      make_scoped_refptr<VideoFrame>(30'000), VideoFrame::CreateEOSFrame()};
  std::vector<DrawnFrame> drawn;
  alg.Render(&clock, &frames, MakeRecorder(&drawn));

  // Intermediate frames 10000 and 20000 were skipped; final frame 30000 WAS
  // drawn!
  ASSERT_EQ(drawn.size(), 1U);
  EXPECT_EQ(drawn[0].pts_us, 30'000);
  EXPECT_TRUE(frames.empty());
  EXPECT_EQ(alg.GetStats().skipped_audio_eos, 2);
  tracker.OnFrameRendered(30'000);
  EXPECT_EQ(alg.GetDroppedFrames(), 0);

  // Also verify `!is_last_frame` guard during normal non-audio-EOS playback
  // when the final frame before video EOS arrives -80ms late.
  alg.Seek(0);
  drawn.clear();
  clock.info_.is_eos_played = false;
  clock.info_.media_time = 0;
  {
    std::list<scoped_refptr<VideoFrame>> f0{make_scoped_refptr<VideoFrame>(0)};
    alg.Render(&clock, &f0, MakeRecorder(&drawn));
  }
  clock.info_.media_time = 180'000;  // Frame 100000 is -80ms late!
  std::list<scoped_refptr<VideoFrame>> tail{
      make_scoped_refptr<VideoFrame>(100'000), VideoFrame::CreateEOSFrame()};
  alg.Render(&clock, &tail, MakeRecorder(&drawn));
  ASSERT_EQ(drawn.size(), 2U);
  EXPECT_EQ(drawn[1].pts_us, 100'000);
  EXPECT_TRUE(tail.empty());
}

TEST(VideoRenderAlgorithmV2Test,
     Bit11_DecodeToTextureShrinksReadyLookaheadWindow) {
  FakeRenderHost host;
  host.output_mode = kSbPlayerOutputModeDecodeToTexture;
  FakeMediaTimeProvider clock;
  auto state = std::make_shared<FakeReleasePlatform::State>();
  state->vsync = {1'000'000'000LL, 16'666'667LL, 13'333'333LL};
  int64_t now_ns = 1'000'000'000LL;

  VideoRenderAlgorithmAndroidV2 alg(
      &host, nullptr,
      v2::kEnableNewAlgorithm | v2::kFirstFrameImmediateAndAudioGate |
          v2::kDisableTextureModeLookahead,
      34, std::make_unique<FakeReleasePlatform>(state), [&] { return now_ns; });

  std::vector<DrawnFrame> drawn;
  {
    std::list<scoped_refptr<VideoFrame>> f0{make_scoped_refptr<VideoFrame>(0)};
    alg.Render(&clock, &f0, MakeRecorder(&drawn));
  }
  ASSERT_EQ(drawn.size(), 1U);

  // Frame at +35ms early (< 50ms PunchOut window, > 16.67ms VSYNC window) MUST
  // be held in DecodeToTexture so SurfaceTexture does not latch it 35ms early.
  clock.info_.media_time = 5'000;
  now_ns = 1'005'000'000LL;
  std::list<scoped_refptr<VideoFrame>> frames{
      make_scoped_refptr<VideoFrame>(40'000)};
  alg.Render(&clock, &frames, MakeRecorder(&drawn));
  EXPECT_EQ(drawn.size(), 1U);
  EXPECT_EQ(frames.size(), 1U);

  // Once within 1 VSYNC (~10ms early), it releases to SurfaceTexture.
  clock.info_.media_time = 30'000;
  now_ns = 1'030'000'000LL;
  alg.Render(&clock, &frames, MakeRecorder(&drawn));
  EXPECT_EQ(drawn.size(), 2U);
}

TEST(VideoRenderAlgorithmV2Test,
     Bit12_DeadlineWakeupsClampStrictlyUnder66msAndStopSamplerWhenPaused) {
  FakeRenderHost host;
  FakeMediaTimeProvider clock;
  auto state = std::make_shared<FakeReleasePlatform::State>();
  int64_t now_ns = 1'000'000'000LL;
  int clock_reads = 0;

  VideoRenderAlgorithmAndroidV2 alg(
      &host, nullptr, 0x3FFFu, 34, std::make_unique<FakeReleasePlatform>(state),
      [&] {
        ++clock_reads;
        return now_ns;
      });

  std::vector<DrawnFrame> drawn;
  // Awaiting first frame with empty queue -> 8ms wait.
  std::list<scoped_refptr<VideoFrame>> empty_frames;
  alg.Render(&clock, &empty_frames, MakeRecorder(&drawn));
  EXPECT_EQ(host.last_wait_duration_us, 8'000);

  // Draw Frame 0 + process Frame 1 waiting at +85ms unadjusted (~+67ms
  // snapped): wait = clamp(66667 - 42000, 8000, 50000) = 24667us (<= 50ms <
  // 66ms).
  clock.info_.media_time = 10'000;
  std::list<scoped_refptr<VideoFrame>> frames{
      make_scoped_refptr<VideoFrame>(0), make_scoped_refptr<VideoFrame>(16'667),
      make_scoped_refptr<VideoFrame>(95'000)};
  clock_reads = 0;
  clock.query_count = 0;
  alg.Render(&clock, &frames, MakeRecorder(&drawn));
  EXPECT_EQ(drawn.size(), 2U);
  EXPECT_EQ(frames.size(), 1U);
  // Single CLOCK_MONOTONIC read and single GetCurrentMediaTime() query across
  // the multi-frame Render() pass!
  EXPECT_EQ(clock_reads, 1);
  EXPECT_EQ(clock.query_count, 1);
  EXPECT_GE(host.last_wait_duration_us, 20'000);
  EXPECT_LE(host.last_wait_duration_us, 40'000);

  // Far-future frame (+200ms early) clamps strictly to 50ms (never >= 66ms).
  frames.clear();
  frames.push_back(make_scoped_refptr<VideoFrame>(210'000));
  alg.Render(&clock, &frames, MakeRecorder(&drawn));
  EXPECT_EQ(host.last_wait_duration_us, 50'000);

  // Pausing stops VSYNC sampler, sets 50ms wait, and skips locking media clock!
  host.paused = true;
  clock.query_count = 0;
  alg.Render(&clock, &frames, MakeRecorder(&drawn));
  EXPECT_FALSE(state->playback_active);
  EXPECT_EQ(host.last_wait_duration_us, 50'000);
  EXPECT_EQ(clock.query_count, 0);
}

TEST(VideoRenderAlgorithmV2Test,
     DestroyingHostBeforeAlgorithmDoesNotUseAfterFree) {
  auto host = std::make_unique<FakeRenderHost>();
  auto tracker = std::make_unique<VideoFrameTracker>(64);
  auto state = std::make_shared<FakeReleasePlatform::State>();
  auto alg = std::make_unique<VideoRenderAlgorithmAndroidV2>(
      host.get(), tracker.get(), 0x3FFFu, 34,
      std::make_unique<FakeReleasePlatform>(state));

  // Mirror VideoRendererImpl::~VideoRendererImpl() destruction order: decoder
  // (host + tracker) is destroyed before algorithm_.
  tracker.reset();
  host.reset();
  alg.reset();
}

TEST(VideoRenderAlgorithmV2Test,
     Bit1PlusBit12_PausedSeekAndMidStreamPausePreserveAudioGate) {
  FakeRenderHost host;
  FakeMediaTimeProvider clock;
  auto state = std::make_shared<FakeReleasePlatform::State>();
  int64_t now_ns = 1'000'000'000LL;

  VideoRenderAlgorithmAndroidV2 alg(
      &host, nullptr, 0x3FFFu, 34, std::make_unique<FakeReleasePlatform>(state),
      [&] { return now_ns; });

  // 1. Paused seek to 10s with two prerolled frames: Frame 0 draws immediately,
  // Frame 1 stays held, and unpausing while audio is still frozen at 10s holds
  // Frame 1 until audio advances past 10s.
  host.paused = true;
  clock.info_.is_playing = false;
  clock.info_.media_time = 10'000'000;
  alg.Seek(10'000'000);

  std::vector<DrawnFrame> drawn;
  std::list<scoped_refptr<VideoFrame>> seek_frames{
      make_scoped_refptr<VideoFrame>(10'000'000),
      make_scoped_refptr<VideoFrame>(10'016'667)};
  alg.Render(&clock, &seek_frames, MakeRecorder(&drawn));
  ASSERT_EQ(drawn.size(), 1U);
  EXPECT_EQ(seek_frames.size(), 1U);

  host.paused = false;
  clock.info_.is_playing = true;
  clock.info_.playback_rate = 1.0;
  clock.info_.media_time = 10'000'000;  // AudioTrack warming up, not advanced.
  alg.Render(&clock, &seek_frames, MakeRecorder(&drawn));
  EXPECT_EQ(drawn.size(), 1U);
  EXPECT_EQ(host.last_wait_duration_us, 8'000);

  clock.info_.media_time = 10'005'000;
  now_ns += 5'000'000LL;
  alg.Render(&clock, &seek_frames, MakeRecorder(&drawn));
  EXPECT_EQ(drawn.size(), 2U);

  // 2. Mid-playback pause at 25s -> unpause while audio is frozen at 25s must
  // also hold subsequent frames until audio advances past 25s.
  now_ns += 15'000'000'000LL;
  clock.info_.media_time = 25'000'000;
  host.paused = true;
  std::list<scoped_refptr<VideoFrame>> resume_frames{
      make_scoped_refptr<VideoFrame>(25'016'667)};
  alg.Render(&clock, &resume_frames, MakeRecorder(&drawn));
  EXPECT_EQ(drawn.size(), 2U);

  host.paused = false;
  clock.info_.media_time = 25'000'000;
  alg.Render(&clock, &resume_frames, MakeRecorder(&drawn));
  EXPECT_EQ(drawn.size(), 2U);
  EXPECT_EQ(host.last_wait_duration_us, 8'000);

  clock.info_.media_time = 25'004'000;
  now_ns += 4'000'000LL;
  alg.Render(&clock, &resume_frames, MakeRecorder(&drawn));
  EXPECT_EQ(drawn.size(), 3U);
}

TEST(VideoRenderAlgorithmV2Test,
     Bit10_MultiBatchAudioEosSkipsIntermediateBatchesWithoutOverwrites) {
  FakeRenderHost host;
  FakeMediaTimeProvider clock;
  VideoFrameTracker tracker(/*max_pending_frames_size=*/64);
  auto state = std::make_shared<FakeReleasePlatform::State>();
  int64_t now_ns = 1'000'000'000LL;

  VideoRenderAlgorithmAndroidV2 alg(
      &host, &tracker, 0x3FFFu, 34,
      std::make_unique<FakeReleasePlatform>(state), [&] { return now_ns; });

  for (int64_t pts : {10'000, 20'000, 30'000, 40'000, 50'000}) {
    tracker.OnInputBuffer(pts);
  }
  clock.info_.media_time = 10'000;
  clock.info_.is_eos_played = true;

  // Batch 1 arrives without video EOS yet: skips 10k & 20k, holds 30k undrawn.
  std::vector<DrawnFrame> drawn;
  std::list<scoped_refptr<VideoFrame>> frames{
      make_scoped_refptr<VideoFrame>(10'000),
      make_scoped_refptr<VideoFrame>(20'000),
      make_scoped_refptr<VideoFrame>(30'000)};
  alg.Render(&clock, &frames, MakeRecorder(&drawn));
  EXPECT_EQ(drawn.size(), 0U);
  ASSERT_EQ(frames.size(), 1U);
  EXPECT_EQ(frames.front()->timestamp(), 30'000);
  EXPECT_EQ(host.last_wait_duration_us, 8'000);

  // Batch 2 appends 40k, 50k, and EOS: skips 30k & 40k, draws ONLY 50k!
  frames.push_back(make_scoped_refptr<VideoFrame>(40'000));
  frames.push_back(make_scoped_refptr<VideoFrame>(50'000));
  frames.push_back(VideoFrame::CreateEOSFrame());
  alg.Render(&clock, &frames, MakeRecorder(&drawn));
  ASSERT_EQ(drawn.size(), 1U);
  EXPECT_EQ(drawn[0].pts_us, 50'000);
  EXPECT_TRUE(frames.empty());
  EXPECT_EQ(alg.GetStats().skipped_audio_eos, 4);

  tracker.OnFrameRendered(50'000);
  EXPECT_EQ(alg.GetDroppedFrames(), 0);
}

TEST(VideoRenderAlgorithmV2Test,
     Bit8_MidpointHysteresisWorksWithoutBit3Smoother) {
  constexpr int64_t k50HzNs = 20'000'000LL;
  FakeRenderHost host;
  FakeMediaTimeProvider clock;
  auto state = std::make_shared<FakeReleasePlatform::State>();
  state->vsync = {1'000'000'000LL, k50HzNs, (k50HzNs * 80) / 100};
  int64_t now_ns = 1'000'000'000LL;

  // Enable ONLY Bit 0 + Bit 8 (legacy parity smoother, no Bit 3).
  VideoRenderAlgorithmAndroidV2 alg(
      &host, nullptr, v2::kEnableNewAlgorithm | v2::kVsyncMidpointHysteresis,
      34, std::make_unique<FakeReleasePlatform>(state), [&] { return now_ns; });

  std::vector<DrawnFrame> drawn;
  int chatter = 0;
  std::optional<int64_t> prev_step_ns;
  for (int i = 0; i < 30; ++i) {
    const int64_t pts_us = (i + 1) * 20'000;
    const int64_t jitter_ns = (i % 2 == 0) ? -400'000LL : +400'000LL;
    now_ns = 1'000'000'000LL + (i + 1) * k50HzNs + 10'000'000LL + jitter_ns;
    clock.info_.media_time = pts_us;
    std::list<scoped_refptr<VideoFrame>> f{
        make_scoped_refptr<VideoFrame>(pts_us)};
    alg.Render(&clock, &f, MakeRecorder(&drawn));
    if (drawn.size() >= 2) {
      const int64_t step =
          drawn.back().release_ns - drawn[drawn.size() - 2].release_ns;
      if (prev_step_ns.has_value() && step != *prev_step_ns) {
        ++chatter;
      }
      prev_step_ns = step;
    }
  }
  EXPECT_LE(chatter, 2);
}

TEST(VideoRenderAlgorithmV2Test,
     ExtremeTimestampsAndNonFiniteRatesAreSafeWithoutOverflow) {
  EXPECT_EQ(NormalizeVideoRenderAlgorithmMask(0x80000001u, 34), 0u);

  FakeRenderHost host;
  FakeMediaTimeProvider clock;
  auto state = std::make_shared<FakeReleasePlatform::State>();
  int64_t now_ns = 1'000'000'000LL;

  VideoRenderAlgorithmAndroidV2 alg(
      &host, nullptr, 0x3FFFu, 34, std::make_unique<FakeReleasePlatform>(state),
      [&] { return now_ns; });

  std::vector<DrawnFrame> drawn;
  clock.info_.playback_rate = std::numeric_limits<double>::quiet_NaN();
  {
    std::list<scoped_refptr<VideoFrame>> f{
        make_scoped_refptr<VideoFrame>(std::numeric_limits<int64_t>::max()),
        make_scoped_refptr<VideoFrame>(std::numeric_limits<int64_t>::min())};
    alg.Render(&clock, &f, MakeRecorder(&drawn));
  }
  clock.info_.playback_rate = std::numeric_limits<double>::infinity();
  {
    std::list<scoped_refptr<VideoFrame>> f{make_scoped_refptr<VideoFrame>(100)};
    alg.Render(&clock, &f, MakeRecorder(&drawn));
  }
  clock.info_.playback_rate = 1.0;
  clock.info_.media_time = 100;
  {
    std::list<scoped_refptr<VideoFrame>> f{
        make_scoped_refptr<VideoFrame>(std::numeric_limits<int64_t>::max())};
    alg.Render(&clock, &f, MakeRecorder(&drawn));
  }
  EXPECT_GE(drawn.size(), 1U);
}

}  // namespace
}  // namespace starboard
