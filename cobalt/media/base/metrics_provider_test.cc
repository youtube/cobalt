// Copyright 2024 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/media/base/metrics_provider.h"

#include <memory>
#include <string>

#include "base/test/metrics/histogram_tester.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace media {
namespace {
constexpr char kUmaPrefix[] = "Cobalt.Media.";

class MediaMetricsProviderTest : public ::testing::Test {
 protected:
  MediaMetricsProviderTest() : metrics_(&clock_) {}

  void SetUp() override { clock_.SetNowTicks(base::TimeTicks()); }

  base::HistogramTester histogram_tester_;
  base::SimpleTestTickClock clock_;

  MediaMetricsProvider metrics_;
};

TEST_F(MediaMetricsProviderTest, ReportsSeekLatency) {
  metrics_.StartTrackingAction(MediaAction::WEBMEDIAPLAYER_SEEK);

  clock_.Advance(base::TimeDelta::FromMilliseconds(100));
  metrics_.EndTrackingAction(MediaAction::WEBMEDIAPLAYER_SEEK);

  histogram_tester_.ExpectUniqueSample(
      std::string(kUmaPrefix) + "WebMediaPlayer.Seek.Timing", 100, 1);
}

TEST_F(MediaMetricsProviderTest, SupportsTrackingMultipleActions) {
  metrics_.StartTrackingAction(MediaAction::WEBMEDIAPLAYER_SEEK);
  metrics_.StartTrackingAction(MediaAction::UNKNOWN_ACTION);

  clock_.Advance(base::TimeDelta::FromMilliseconds(100));
  metrics_.EndTrackingAction(MediaAction::UNKNOWN_ACTION);

  clock_.Advance(base::TimeDelta::FromMilliseconds(1000));
  metrics_.EndTrackingAction(MediaAction::WEBMEDIAPLAYER_SEEK);

  histogram_tester_.ExpectUniqueSample(
      std::string(kUmaPrefix) + "WebMediaPlayer.Seek.Timing", 1100, 1);
}

TEST_F(MediaMetricsProviderTest, SbPlayerCreate) {
  metrics_.StartTrackingAction(MediaAction::SBPLAYER_CREATE);

  clock_.Advance(base::TimeDelta::FromMicroseconds(570));
  metrics_.EndTrackingAction(MediaAction::SBPLAYER_CREATE);

  histogram_tester_.ExpectUniqueSample(
      std::string(kUmaPrefix) + "SbPlayer.Create.LatencyTiming", 570, 1);
}

TEST_F(MediaMetricsProviderTest, SbPlayerCreateUrlPlayer) {
  metrics_.StartTrackingAction(MediaAction::SBPLAYER_CREATE_URL_PLAYER);

  clock_.Advance(base::TimeDelta::FromMicroseconds(670));
  metrics_.EndTrackingAction(MediaAction::SBPLAYER_CREATE_URL_PLAYER);

  histogram_tester_.ExpectUniqueSample(
      std::string(kUmaPrefix) + "SbPlayer.CreateUrlPlayer.LatencyTiming", 670,
      1);
}

TEST_F(MediaMetricsProviderTest, SbPlayerDestroy) {
  metrics_.StartTrackingAction(MediaAction::SBPLAYER_DESTROY);

  clock_.Advance(base::TimeDelta::FromMilliseconds(100));
  metrics_.EndTrackingAction(MediaAction::SBPLAYER_DESTROY);

  histogram_tester_.ExpectUniqueSample(
      std::string(kUmaPrefix) + "SbPlayer.Destroy.LatencyTiming", 100, 1);
}

TEST_F(MediaMetricsProviderTest, SbPlayerGetPreferredOutputMode) {
  metrics_.StartTrackingAction(MediaAction::SBPLAYER_GET_PREFERRED_OUTPUT_MODE);

  clock_.Advance(base::TimeDelta::FromMicroseconds(570));
  metrics_.EndTrackingAction(MediaAction::SBPLAYER_GET_PREFERRED_OUTPUT_MODE);

  histogram_tester_.ExpectUniqueSample(
      std::string(kUmaPrefix) + "SbPlayer.GetPreferredOutputMode.LatencyTiming",
      570, 1);
}

TEST_F(MediaMetricsProviderTest, SbPlayerSeek) {
  metrics_.StartTrackingAction(MediaAction::SBPLAYER_SEEK);

  clock_.Advance(base::TimeDelta::FromMicroseconds(570));
  metrics_.EndTrackingAction(MediaAction::SBPLAYER_SEEK);

  histogram_tester_.ExpectUniqueSample(
      std::string(kUmaPrefix) + "SbPlayer.Seek.LatencyTiming", 570, 1);
}

TEST_F(MediaMetricsProviderTest, SbPlayerSetBounds) {
  metrics_.StartTrackingAction(MediaAction::SBPLAYER_SET_BOUNDS);

  clock_.Advance(base::TimeDelta::FromMicroseconds(570));
  metrics_.EndTrackingAction(MediaAction::SBPLAYER_SET_BOUNDS);

  histogram_tester_.ExpectUniqueSample(
      std::string(kUmaPrefix) + "SbPlayer.SetBounds.LatencyTiming", 570, 1);
}

TEST_F(MediaMetricsProviderTest, SbPlayerSetPlaybackRate) {
  metrics_.StartTrackingAction(MediaAction::SBPLAYER_SET_PLAYBACK_RATE);

  clock_.Advance(base::TimeDelta::FromMicroseconds(570));
  metrics_.EndTrackingAction(MediaAction::SBPLAYER_SET_PLAYBACK_RATE);

  histogram_tester_.ExpectUniqueSample(
      std::string(kUmaPrefix) + "SbPlayer.SetPlaybackRate.LatencyTiming", 570,
      1);
}

TEST_F(MediaMetricsProviderTest, SbPlayerSetVolume) {
  metrics_.StartTrackingAction(MediaAction::SBPLAYER_SET_VOLUME);

  clock_.Advance(base::TimeDelta::FromMicroseconds(570));
  metrics_.EndTrackingAction(MediaAction::SBPLAYER_SET_VOLUME);

  histogram_tester_.ExpectUniqueSample(
      std::string(kUmaPrefix) + "SbPlayer.SetVolume.LatencyTiming", 570, 1);
}

TEST_F(MediaMetricsProviderTest, SbPlayerGetInfo) {
  metrics_.StartTrackingAction(MediaAction::SBPLAYER_GET_INFO);

  clock_.Advance(base::TimeDelta::FromMicroseconds(570));
  metrics_.EndTrackingAction(MediaAction::SBPLAYER_GET_INFO);

  histogram_tester_.ExpectUniqueSample(
      std::string(kUmaPrefix) + "SbPlayer.GetInfo.LatencyTiming", 570, 1);
}

TEST_F(MediaMetricsProviderTest, SbPlayerGetCurrentFrame) {
  metrics_.StartTrackingAction(MediaAction::SBPLAYER_GET_CURRENT_FRAME);

  clock_.Advance(base::TimeDelta::FromMicroseconds(570));
  metrics_.EndTrackingAction(MediaAction::SBPLAYER_GET_CURRENT_FRAME);

  histogram_tester_.ExpectUniqueSample(
      std::string(kUmaPrefix) + "SbPlayer.GetCurrentFrame.LatencyTiming", 570,
      1);
}

TEST_F(MediaMetricsProviderTest, SbPlayerGetAudioConfig) {
  metrics_.StartTrackingAction(MediaAction::SBPLAYER_GET_AUDIO_CONFIG);

  clock_.Advance(base::TimeDelta::FromMicroseconds(570));
  metrics_.EndTrackingAction(MediaAction::SBPLAYER_GET_AUDIO_CONFIG);

  histogram_tester_.ExpectUniqueSample(
      std::string(kUmaPrefix) + "SbPlayer.GetAudioConfig.LatencyTiming", 570,
      1);
}

TEST_F(MediaMetricsProviderTest, SbPlayerWriteEndOfStreamAudio) {
  metrics_.StartTrackingAction(MediaAction::SBPLAYER_WRITE_END_OF_STREAM_AUDIO);

  clock_.Advance(base::TimeDelta::FromMicroseconds(570));
  metrics_.EndTrackingAction(MediaAction::SBPLAYER_WRITE_END_OF_STREAM_AUDIO);

  histogram_tester_.ExpectUniqueSample(
      std::string(kUmaPrefix) + "SbPlayer.WriteEndOfStream.Audio.LatencyTiming",
      570, 1);
}

TEST_F(MediaMetricsProviderTest, SbPlayerWriteEndOfStreamVideo) {
  metrics_.StartTrackingAction(MediaAction::SBPLAYER_WRITE_END_OF_STREAM_VIDEO);

  clock_.Advance(base::TimeDelta::FromMicroseconds(570));
  metrics_.EndTrackingAction(MediaAction::SBPLAYER_WRITE_END_OF_STREAM_VIDEO);

  histogram_tester_.ExpectUniqueSample(
      std::string(kUmaPrefix) + "SbPlayer.WriteEndOfStream.Video.LatencyTiming",
      570, 1);
}

TEST_F(MediaMetricsProviderTest, SbDrmCloseSession) {
  metrics_.StartTrackingAction(MediaAction::SBDRM_CLOSE_SESSION);

  clock_.Advance(base::TimeDelta::FromMicroseconds(570));
  metrics_.EndTrackingAction(MediaAction::SBDRM_CLOSE_SESSION);

  histogram_tester_.ExpectUniqueSample(
      std::string(kUmaPrefix) + "SbDrm.CloseSession.LatencyTiming", 570, 1);
}

TEST_F(MediaMetricsProviderTest, SbDrmCreate) {
  metrics_.StartTrackingAction(MediaAction::SBDRM_CREATE);

  clock_.Advance(base::TimeDelta::FromMicroseconds(570));
  metrics_.EndTrackingAction(MediaAction::SBDRM_CREATE);

  histogram_tester_.ExpectUniqueSample(
      std::string(kUmaPrefix) + "SbDrm.Create.LatencyTiming", 570, 1);
}

TEST_F(MediaMetricsProviderTest, SbDrmDestroy) {
  metrics_.StartTrackingAction(MediaAction::SBDRM_DESTROY);

  clock_.Advance(base::TimeDelta::FromMicroseconds(570));
  metrics_.EndTrackingAction(MediaAction::SBDRM_DESTROY);

  histogram_tester_.ExpectUniqueSample(
      std::string(kUmaPrefix) + "SbDrm.Destroy.LatencyTiming", 570, 1);
}

TEST_F(MediaMetricsProviderTest, SbDrmGenerateSessionUpdateRequest) {
  metrics_.StartTrackingAction(
      MediaAction::SBDRM_GENERATE_SESSION_UPDATE_REQUEST);

  clock_.Advance(base::TimeDelta::FromMilliseconds(100));
  metrics_.EndTrackingAction(
      MediaAction::SBDRM_GENERATE_SESSION_UPDATE_REQUEST);

  histogram_tester_.ExpectUniqueSample(
      std::string(kUmaPrefix) +
          "SbDrm.GenerateSessionUpdateRequest.LatencyTiming",
      100, 1);
}

TEST_F(MediaMetricsProviderTest, SbDrmUpdateSession) {
  metrics_.StartTrackingAction(MediaAction::SBDRM_UPDATE_SESSION);

  clock_.Advance(base::TimeDelta::FromMilliseconds(100));
  metrics_.EndTrackingAction(MediaAction::SBDRM_UPDATE_SESSION);

  histogram_tester_.ExpectUniqueSample(
      std::string(kUmaPrefix) + "SbDrm.UpdateSession.LatencyTiming", 100, 1);
}

TEST_F(MediaMetricsProviderTest, SbMediaIsBufferPoolAllocateOnDemand) {
  metrics_.StartTrackingAction(
      MediaAction::SBMEDIA_BUFFER_POOL_ALLOCATE_ON_DEMAND);

  clock_.Advance(base::TimeDelta::FromMicroseconds(570));
  metrics_.EndTrackingAction(
      MediaAction::SBMEDIA_BUFFER_POOL_ALLOCATE_ON_DEMAND);

  histogram_tester_.ExpectUniqueSample(
      std::string(kUmaPrefix) +
          "SbMedia.BufferPoolAllocateOnDemand.LatencyTiming",
      570, 1);
}

TEST_F(MediaMetricsProviderTest, SbMediaGetInitialBufferCapacity) {
  metrics_.StartTrackingAction(MediaAction::SBMEDIA_GET_INIT_BUFFER_CAPACITY);

  clock_.Advance(base::TimeDelta::FromMicroseconds(570));
  metrics_.EndTrackingAction(MediaAction::SBMEDIA_GET_INIT_BUFFER_CAPACITY);

  histogram_tester_.ExpectUniqueSample(
      std::string(kUmaPrefix) + "SbMedia.GetInitBufferCapacity.LatencyTiming",
      570, 1);
}

TEST_F(MediaMetricsProviderTest, SbMediaGetBufferAllocationUnit) {
  metrics_.StartTrackingAction(MediaAction::SBMEDIA_GET_BUFFER_ALLOCATION_UNIT);

  clock_.Advance(base::TimeDelta::FromMicroseconds(570));
  metrics_.EndTrackingAction(MediaAction::SBMEDIA_GET_BUFFER_ALLOCATION_UNIT);

  histogram_tester_.ExpectUniqueSample(
      std::string(kUmaPrefix) + "SbMedia.GetBufferAllocationUnit.LatencyTiming",
      570, 1);
}

TEST_F(MediaMetricsProviderTest, SbMediaGetAudioBufferBudget) {
  metrics_.StartTrackingAction(MediaAction::SBMEDIA_GET_AUDIO_BUFFER_BUDGET);

  clock_.Advance(base::TimeDelta::FromMicroseconds(570));
  metrics_.EndTrackingAction(MediaAction::SBMEDIA_GET_AUDIO_BUFFER_BUDGET);

  histogram_tester_.ExpectUniqueSample(
      std::string(kUmaPrefix) + "SbMedia.GetAudioBufferBudget.LatencyTiming",
      570, 1);
}

TEST_F(MediaMetricsProviderTest,
       SbMediaGetBufferGarbageCollectionDurationThreshold) {
  metrics_.StartTrackingAction(
      MediaAction::SBMEDIA_GET_BUFFER_GARBAGE_COLLECTION_DURATION_THRESHOLD);

  clock_.Advance(base::TimeDelta::FromMicroseconds(570));
  metrics_.EndTrackingAction(
      MediaAction::SBMEDIA_GET_BUFFER_GARBAGE_COLLECTION_DURATION_THRESHOLD);

  histogram_tester_.ExpectUniqueSample(
      std::string(kUmaPrefix) +
          "SbMedia.GetBufferGarbageCollectionDurationThreshold.LatencyTiming",
      570, 1);
}

TEST_F(MediaMetricsProviderTest, SbMediaGetProgressiveBufferBudget) {
  metrics_.StartTrackingAction(
      MediaAction::SBMEDIA_GET_PROGRESSIVE_BUFFER_BUDGET);

  clock_.Advance(base::TimeDelta::FromMicroseconds(570));
  metrics_.EndTrackingAction(
      MediaAction::SBMEDIA_GET_PROGRESSIVE_BUFFER_BUDGET);

  histogram_tester_.ExpectUniqueSample(
      std::string(kUmaPrefix) +
          "SbMedia.GetProgressiveBufferBudget.LatencyTiming",
      570, 1);
}

TEST_F(MediaMetricsProviderTest, SbMediaGetVideoBufferBudget) {
  metrics_.StartTrackingAction(MediaAction::SBMEDIA_GET_VIDEO_BUFFER_BUDGET);

  clock_.Advance(base::TimeDelta::FromMicroseconds(570));
  metrics_.EndTrackingAction(MediaAction::SBMEDIA_GET_VIDEO_BUFFER_BUDGET);

  histogram_tester_.ExpectUniqueSample(
      std::string(kUmaPrefix) + "SbMedia.GetVideoBufferBudget.LatencyTiming",
      570, 1);
}

}  // namespace
}  // namespace media
}  // namespace cobalt
