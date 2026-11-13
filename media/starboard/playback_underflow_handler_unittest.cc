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

#include "media/starboard/playback_underflow_handler.h"

#include <memory>
#include <string>
#include <vector>

#include "base/memory/scoped_refptr.h"
#include "base/test/task_environment.h"
#include "media/base/decoder_buffer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {
namespace {

// Matches the check interval in playback_underflow_handler.cc.
constexpr base::TimeDelta kCheckInterval = base::Milliseconds(100);

base::TimeDelta Ms(int64_t ms) {
  return base::Milliseconds(ms);
}

class FakeDelegate : public PlaybackUnderflowHandler::Delegate {
 public:
  base::TimeDelta GetCurrentMediaTime() override { return media_time; }
  bool IsReadPending(DemuxerStream::Type type) const override {
    return type == DemuxerStream::AUDIO ? audio_read_pending
                                        : video_read_pending;
  }
  bool IsPausedByApp() const override { return paused_by_app; }
  bool IsPlaybackActive() const override { return playback_active; }
  void PauseSbPlayer() override { sbplayer_calls.push_back("pause"); }
  void ResumeSbPlayer() override { sbplayer_calls.push_back("resume"); }
  void ReportBufferingState(BufferingState state,
                            BufferingStateChangeReason reason) override {
    buffering_states.push_back(state);
    buffering_reasons.push_back(reason);
  }

  base::TimeDelta media_time;
  bool audio_read_pending = true;
  bool video_read_pending = true;
  bool paused_by_app = false;
  bool playback_active = true;
  std::vector<std::string> sbplayer_calls;
  std::vector<BufferingState> buffering_states;
  std::vector<BufferingStateChangeReason> buffering_reasons;
};

class PlaybackUnderflowHandlerTest : public testing::Test {
 protected:
  PlaybackUnderflowHandlerTest()
      : handler_(PlaybackUnderflowHandler::Create(
            /*enabled=*/true,
            &delegate_,
            task_environment_.GetMainThreadTaskRunner())) {}

  // Starts playback at 0 with audio written up to |audio_end| and video up
  // to |video_end|, and reaches presenting.
  void StartPresenting(base::TimeDelta audio_end, base::TimeDelta video_end) {
    handler_->OnStartPlayingFrom(/*has_audio=*/true, /*has_video=*/true);
    Write(DemuxerStream::AUDIO, base::TimeDelta(), audio_end);
    Write(DemuxerStream::VIDEO, base::TimeDelta(), video_end);
    handler_->OnPresenting();
  }

  void Write(DemuxerStream::Type type,
             base::TimeDelta start,
             base::TimeDelta end) {
    auto buffer = base::MakeRefCounted<DecoderBuffer>(0);
    buffer->set_timestamp(start);
    buffer->set_duration(end - start);
    handler_->OnBuffersWritten(type, {buffer});
  }

  void RunCheck() { task_environment_.FastForwardBy(kCheckInterval); }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  FakeDelegate delegate_;
  std::unique_ptr<PlaybackUnderflowHandler> handler_;
};

TEST_F(PlaybackUnderflowHandlerTest, NoActionWhileDataIsAhead) {
  StartPresenting(Ms(5000), Ms(10000));
  for (delegate_.media_time = base::TimeDelta();
       delegate_.media_time < Ms(4500); delegate_.media_time += Ms(500)) {
    RunCheck();
  }
  EXPECT_TRUE(delegate_.sbplayer_calls.empty());
  EXPECT_TRUE(delegate_.buffering_states.empty());
  EXPECT_FALSE(handler_->IsPausedForUnderflow());
}

TEST_F(PlaybackUnderflowHandlerTest, PausesAndResumes) {
  StartPresenting(Ms(5000), Ms(10000));

  delegate_.media_time = Ms(5000);
  RunCheck();
  EXPECT_EQ(delegate_.sbplayer_calls, std::vector<std::string>({"pause"}));
  EXPECT_EQ(delegate_.buffering_states,
            std::vector<BufferingState>({BUFFERING_HAVE_NOTHING}));
  EXPECT_TRUE(handler_->IsPausedForUnderflow());

  // Staying starved does nothing more.
  RunCheck();
  EXPECT_EQ(delegate_.sbplayer_calls.size(), 1u);

  Write(DemuxerStream::AUDIO, Ms(5000), Ms(6000));
  RunCheck();
  EXPECT_EQ(delegate_.sbplayer_calls,
            std::vector<std::string>({"pause", "resume"}));
  EXPECT_EQ(delegate_.buffering_states,
            std::vector<BufferingState>(
                {BUFFERING_HAVE_NOTHING, BUFFERING_HAVE_ENOUGH}));
  EXPECT_FALSE(handler_->IsPausedForUnderflow());
}

TEST_F(PlaybackUnderflowHandlerTest, RecoversWhilePausedByApp) {
  StartPresenting(Ms(5000), Ms(10000));
  delegate_.media_time = Ms(5000);
  RunCheck();
  ASSERT_TRUE(handler_->IsPausedForUnderflow());

  // The user pauses, then data arrives. The underflow still ends: the handler
  // asks the delegate to resume (which applies the app's rate, i.e. stays
  // paused) and reports HAVE_ENOUGH.
  delegate_.paused_by_app = true;
  Write(DemuxerStream::AUDIO, Ms(5000), Ms(6000));
  RunCheck();
  EXPECT_EQ(delegate_.sbplayer_calls,
            std::vector<std::string>({"pause", "resume"}));
  EXPECT_EQ(delegate_.buffering_states.back(), BUFFERING_HAVE_ENOUGH);
  EXPECT_FALSE(handler_->IsPausedForUnderflow());
}

TEST_F(PlaybackUnderflowHandlerTest, NoUnderflowWhileAppIsPaused) {
  StartPresenting(Ms(5000), Ms(10000));
  delegate_.paused_by_app = true;
  delegate_.media_time = Ms(5000);
  RunCheck();
  RunCheck();
  EXPECT_TRUE(delegate_.sbplayer_calls.empty());
  EXPECT_TRUE(delegate_.buffering_states.empty());
}

TEST_F(PlaybackUnderflowHandlerTest, NoCheckWhilePlaybackInactive) {
  StartPresenting(Ms(5000), Ms(10000));
  delegate_.playback_active = false;
  delegate_.media_time = Ms(5000);
  RunCheck();
  EXPECT_TRUE(delegate_.buffering_states.empty());
}

TEST_F(PlaybackUnderflowHandlerTest, NoCheckBeforePresenting) {
  handler_->OnStartPlayingFrom(/*has_audio=*/true, /*has_video=*/true);
  Write(DemuxerStream::AUDIO, base::TimeDelta(), Ms(5000));
  Write(DemuxerStream::VIDEO, base::TimeDelta(), Ms(10000));
  delegate_.media_time = Ms(5000);
  RunCheck();
  EXPECT_TRUE(delegate_.buffering_states.empty());
}

TEST_F(PlaybackUnderflowHandlerTest, FlushStopsMonitoringAndLeavesUnderflow) {
  StartPresenting(Ms(5000), Ms(10000));
  delegate_.media_time = Ms(5000);
  RunCheck();
  ASSERT_TRUE(handler_->IsPausedForUnderflow());

  handler_->OnFlush();
  EXPECT_FALSE(handler_->IsPausedForUnderflow());
  const size_t reports = delegate_.buffering_states.size();
  RunCheck();
  EXPECT_EQ(delegate_.buffering_states.size(), reports);
}

TEST_F(PlaybackUnderflowHandlerTest,
       EndOfStreamDuringUnderflowLeavesUnderflow) {
  StartPresenting(Ms(5000), Ms(10000));
  delegate_.media_time = Ms(5000);
  RunCheck();
  ASSERT_TRUE(handler_->IsPausedForUnderflow());

  handler_->OnEndOfStream();
  EXPECT_FALSE(handler_->IsPausedForUnderflow());
  EXPECT_EQ(delegate_.sbplayer_calls,
            std::vector<std::string>({"pause", "resume"}));
  EXPECT_EQ(delegate_.buffering_states,
            std::vector<BufferingState>(
                {BUFFERING_HAVE_NOTHING, BUFFERING_HAVE_ENOUGH}));
  // Monitoring has stopped.
  RunCheck();
  EXPECT_EQ(delegate_.buffering_states.size(), 2u);
}

TEST_F(PlaybackUnderflowHandlerTest, EndOfStreamStopsMonitoring) {
  StartPresenting(Ms(5000), Ms(10000));
  handler_->OnEndOfStream();
  delegate_.media_time = Ms(5000);
  RunCheck();
  EXPECT_TRUE(delegate_.buffering_states.empty());
}

TEST_F(PlaybackUnderflowHandlerTest, ReportsDemuxerUnderflowReason) {
  StartPresenting(Ms(5000), Ms(10000));
  delegate_.media_time = Ms(5000);
  RunCheck();
  Write(DemuxerStream::AUDIO, Ms(5000), Ms(6000));
  RunCheck();
  EXPECT_EQ(delegate_.buffering_reasons,
            std::vector<BufferingStateChangeReason>(
                {DEMUXER_UNDERFLOW, BUFFERING_CHANGE_REASON_UNKNOWN}));
}

TEST_F(PlaybackUnderflowHandlerTest, VideoOnlyUnderflowIsDeferred) {
  // Video runs out while audio still has data: tolerated for 3 seconds.
  StartPresenting(Ms(10000), Ms(5000));
  delegate_.media_time = Ms(5000);
  task_environment_.FastForwardBy(base::Milliseconds(2900));
  EXPECT_TRUE(delegate_.sbplayer_calls.empty());
  EXPECT_FALSE(handler_->IsPausedForUnderflow());

  task_environment_.FastForwardBy(base::Milliseconds(200));
  EXPECT_EQ(delegate_.sbplayer_calls, std::vector<std::string>({"pause"}));
  EXPECT_TRUE(handler_->IsPausedForUnderflow());
}

TEST_F(PlaybackUnderflowHandlerTest, AppPauseCancelsVideoUnderflowDelay) {
  StartPresenting(Ms(10000), Ms(5000));
  delegate_.media_time = Ms(5000);
  task_environment_.FastForwardBy(Ms(2000));

  // The user pauses for 10 seconds, then resumes.
  delegate_.paused_by_app = true;
  task_environment_.FastForwardBy(Ms(10000));
  delegate_.paused_by_app = false;

  // The time spent paused doesn't count: the 3 second delay starts over.
  RunCheck();
  EXPECT_TRUE(delegate_.sbplayer_calls.empty());
  task_environment_.FastForwardBy(Ms(2800));
  EXPECT_TRUE(delegate_.sbplayer_calls.empty());
  task_environment_.FastForwardBy(Ms(300));
  EXPECT_EQ(delegate_.sbplayer_calls, std::vector<std::string>({"pause"}));
}

TEST_F(PlaybackUnderflowHandlerTest,
       InactivePlaybackCancelsVideoUnderflowDelay) {
  StartPresenting(Ms(10000), Ms(5000));
  delegate_.media_time = Ms(5000);
  task_environment_.FastForwardBy(Ms(2000));

  delegate_.playback_active = false;
  task_environment_.FastForwardBy(Ms(10000));
  delegate_.playback_active = true;

  RunCheck();
  EXPECT_TRUE(delegate_.sbplayer_calls.empty());
  task_environment_.FastForwardBy(Ms(2800));
  EXPECT_TRUE(delegate_.sbplayer_calls.empty());
  task_environment_.FastForwardBy(Ms(300));
  EXPECT_EQ(delegate_.sbplayer_calls, std::vector<std::string>({"pause"}));
}

TEST_F(PlaybackUnderflowHandlerTest, ResumesWhenPipelineIsSaturated) {
  StartPresenting(Ms(5000), Ms(10000));
  delegate_.media_time = Ms(5000);
  RunCheck();
  ASSERT_TRUE(handler_->IsPausedForUnderflow());

  // Some audio is written, then the SbPlayer stops asking for more before
  // |resume_threshold| is reached: resume 1 second after the last write.
  Write(DemuxerStream::AUDIO, Ms(5000), Ms(5200));
  delegate_.audio_read_pending = false;
  task_environment_.FastForwardBy(Ms(900));
  EXPECT_TRUE(handler_->IsPausedForUnderflow());
  task_environment_.FastForwardBy(Ms(200));
  EXPECT_FALSE(handler_->IsPausedForUnderflow());
  EXPECT_EQ(delegate_.sbplayer_calls,
            std::vector<std::string>({"pause", "resume"}));
}

TEST_F(PlaybackUnderflowHandlerTest, PresentingDuringUnderflowResumes) {
  StartPresenting(Ms(5000), Ms(10000));
  delegate_.media_time = Ms(5000);
  RunCheck();
  ASSERT_TRUE(handler_->IsPausedForUnderflow());

  // Defensive path: the SbPlayer presents again without a Flush() (e.g. if it
  // is ever re-prerolled). The renderer reports HAVE_ENOUGH itself; the
  // handler restores the rate.
  handler_->OnPresenting();
  EXPECT_FALSE(handler_->IsPausedForUnderflow());
  EXPECT_EQ(delegate_.sbplayer_calls,
            std::vector<std::string>({"pause", "resume"}));
  EXPECT_EQ(delegate_.buffering_states,
            std::vector<BufferingState>({BUFFERING_HAVE_NOTHING}));
}

TEST_F(PlaybackUnderflowHandlerTest, DisabledHandlerDoesNothing) {
  handler_ = PlaybackUnderflowHandler::Create(
      /*enabled=*/false, &delegate_,
      task_environment_.GetMainThreadTaskRunner());

  // Same setup that triggers an underflow with an enabled handler.
  StartPresenting(Ms(5000), Ms(10000));
  delegate_.media_time = Ms(5000);
  RunCheck();
  RunCheck();
  handler_->OnEndOfStream();
  handler_->OnFlush();

  EXPECT_TRUE(delegate_.sbplayer_calls.empty());
  EXPECT_TRUE(delegate_.buffering_states.empty());
  EXPECT_FALSE(handler_->IsPausedForUnderflow());
}

}  // namespace
}  // namespace media
