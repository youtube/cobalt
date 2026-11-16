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

#include "media/starboard/playback_underflow_detector.h"

#include "base/memory/scoped_refptr.h"
#include "media/base/decoder_buffer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {
namespace {

using Transition = PlaybackUnderflowDetector::Transition;

base::TimeDelta Ms(int64_t ms) {
  return base::Milliseconds(ms);
}

constexpr PlaybackUnderflowDetector::Config kConfig = {
    .resume_threshold = base::Milliseconds(500),
    .underflow_margin = base::Milliseconds(100),
    .video_underflow_delay = base::Seconds(3),
    .saturation_delay = base::Seconds(1),
};

class PlaybackUnderflowDetectorTest : public testing::Test {
 protected:
  PlaybackUnderflowDetectorTest() : detector_(kConfig) {
    detector_.Reset(/*has_audio=*/true, /*has_video=*/true);
  }

  // Records data for |type| written up to |end_timestamp| at |now_|.
  void Write(DemuxerStream::Type type, base::TimeDelta end_timestamp) {
    detector_.OnBufferWritten(type, end_timestamp);
    if (type == DemuxerStream::AUDIO) {
      last_audio_write_time_ = now_;
    } else {
      last_video_write_time_ = now_;
    }
  }

  // Updates at |now_| with both reads pending unless specified.
  Transition Update(base::TimeDelta media_time,
                    bool audio_read_pending = true,
                    bool video_read_pending = true) {
    return detector_.Update(now_, media_time,
                            {.read_pending = audio_read_pending,
                             .last_write_time = last_audio_write_time_},
                            {.read_pending = video_read_pending,
                             .last_write_time = last_video_write_time_});
  }

  void Advance(base::TimeDelta delta) { now_ += delta; }

  // Video starves at 10 s while audio keeps the clock going during the video
  // underflow delay, checked every 100 ms, so video is 3 s behind the
  // playback position when the underflow starts. Returns that position.
  base::TimeDelta StartVideoUnderflowBehind() {
    Write(DemuxerStream::AUDIO, Ms(60000));
    Write(DemuxerStream::VIDEO, Ms(10000));
    base::TimeDelta media_time = Ms(10000);
    while (Update(media_time) != Transition::kUnderflowStarted &&
           media_time < Ms(20000)) {
      Advance(Ms(100));
      media_time += Ms(100);
    }
    return media_time;
  }

  base::TimeTicks now_ = base::TimeTicks() + base::Seconds(100);
  base::TimeTicks last_audio_write_time_;
  base::TimeTicks last_video_write_time_;
  PlaybackUnderflowDetector detector_;
};

TEST_F(PlaybackUnderflowDetectorTest, NoUnderflowWhileDataIsAhead) {
  Write(DemuxerStream::AUDIO, Ms(5000));
  Write(DemuxerStream::VIDEO, Ms(5000));

  EXPECT_EQ(Update(Ms(1000)), Transition::kNone);
  EXPECT_FALSE(detector_.in_underflow());
}

TEST_F(PlaybackUnderflowDetectorTest, AudioUnderflowStartsImmediately) {
  Write(DemuxerStream::AUDIO, Ms(10000));
  Write(DemuxerStream::VIDEO, Ms(60000));

  EXPECT_EQ(Update(Ms(9800)), Transition::kNone);
  EXPECT_EQ(Update(Ms(10000)), Transition::kUnderflowStarted);
  EXPECT_TRUE(detector_.in_underflow());
}

TEST_F(PlaybackUnderflowDetectorTest, AudioUnderflowWithinMargin) {
  // Starboard's audio renderer may stop the clock slightly before the end of
  // the written audio. Within |underflow_margin| it is still an underflow.
  Write(DemuxerStream::AUDIO, Ms(10000));
  Write(DemuxerStream::VIDEO, Ms(60000));

  EXPECT_EQ(Update(Ms(9950)), Transition::kUnderflowStarted);
}

TEST_F(PlaybackUnderflowDetectorTest, AudioUnderflowEndsAtResumeThreshold) {
  Write(DemuxerStream::AUDIO, Ms(10000));
  Write(DemuxerStream::VIDEO, Ms(60000));
  ASSERT_EQ(Update(Ms(10000)), Transition::kUnderflowStarted);

  Write(DemuxerStream::AUDIO, Ms(10400));
  EXPECT_EQ(Update(Ms(10000)), Transition::kNone);
  Write(DemuxerStream::AUDIO, Ms(10500));
  EXPECT_EQ(Update(Ms(10000)), Transition::kUnderflowEnded);
  EXPECT_FALSE(detector_.in_underflow());
}

TEST_F(PlaybackUnderflowDetectorTest, VideoOnlyUnderflowIsDeferred) {
  Write(DemuxerStream::AUDIO, Ms(60000));
  Write(DemuxerStream::VIDEO, Ms(10000));

  // Video starved while audio has data: tolerated for |video_underflow_delay|.
  EXPECT_EQ(Update(Ms(10000)), Transition::kNone);
  Advance(Ms(2900));
  EXPECT_EQ(Update(Ms(12900)), Transition::kNone);
  Advance(Ms(100));
  EXPECT_EQ(Update(Ms(13000)), Transition::kUnderflowStarted);
}

TEST_F(PlaybackUnderflowDetectorTest, DeferredVideoUnderflowCancelsOnData) {
  Write(DemuxerStream::AUDIO, Ms(60000));
  Write(DemuxerStream::VIDEO, Ms(10000));
  EXPECT_EQ(Update(Ms(10000)), Transition::kNone);

  // Video catches up within the delay: the deferral restarts from scratch.
  Advance(Ms(2000));
  Write(DemuxerStream::VIDEO, Ms(15000));
  EXPECT_EQ(Update(Ms(12000)), Transition::kNone);
  Advance(Ms(3000));
  EXPECT_EQ(Update(Ms(15000)), Transition::kNone);
  Advance(Ms(2900));
  EXPECT_EQ(Update(Ms(17900)), Transition::kNone);
  Advance(Ms(100));
  EXPECT_EQ(Update(Ms(18000)), Transition::kUnderflowStarted);
}

TEST_F(PlaybackUnderflowDetectorTest, VideoDeferralSurvivesIdleReads) {
  Write(DemuxerStream::AUDIO, Ms(60000));
  Write(DemuxerStream::VIDEO, Ms(10000));
  EXPECT_EQ(Update(Ms(10000)), Transition::kNone);

  // A small video append arrives and no read is pending for a moment, but
  // video is still behind: the deferral keeps running.
  Advance(Ms(1000));
  Write(DemuxerStream::VIDEO, Ms(10500));
  EXPECT_EQ(Update(Ms(11000), /*audio_read_pending=*/true,
                   /*video_read_pending=*/false),
            Transition::kNone);
  Advance(Ms(1900));
  EXPECT_EQ(Update(Ms(12900)), Transition::kNone);
  Advance(Ms(100));
  EXPECT_EQ(Update(Ms(13000)), Transition::kUnderflowStarted);
}

TEST_F(PlaybackUnderflowDetectorTest, VideoDeferralNeedsPendingReadToStart) {
  Write(DemuxerStream::AUDIO, Ms(60000));
  Write(DemuxerStream::VIDEO, Ms(10000));

  // Video is behind but not waiting on the demuxer: not starved.
  for (int i = 0; i < 50; ++i) {
    EXPECT_EQ(Update(Ms(10000) + Ms(100) * i, /*audio_read_pending=*/true,
                     /*video_read_pending=*/false),
              Transition::kNone);
    Advance(Ms(100));
  }
  // The delay starts once video waits on the demuxer.
  EXPECT_EQ(Update(Ms(15000)), Transition::kNone);
  Advance(Ms(3000));
  EXPECT_EQ(Update(Ms(18000)), Transition::kUnderflowStarted);
}

TEST_F(PlaybackUnderflowDetectorTest, CancelVideoDeferralRestartsDelay) {
  Write(DemuxerStream::AUDIO, Ms(60000));
  Write(DemuxerStream::VIDEO, Ms(10000));
  EXPECT_EQ(Update(Ms(10000)), Transition::kNone);

  // Playback is paused for 10 seconds without updates, e.g. by the user.
  Advance(Ms(2000));
  detector_.CancelVideoDeferral();
  Advance(Ms(10000));

  // The delay starts over rather than firing right away.
  EXPECT_EQ(Update(Ms(12000)), Transition::kNone);
  Advance(Ms(2900));
  EXPECT_EQ(Update(Ms(14900)), Transition::kNone);
  Advance(Ms(100));
  EXPECT_EQ(Update(Ms(15000)), Transition::kUnderflowStarted);
}

TEST_F(PlaybackUnderflowDetectorTest, AudioUnderflowDuringVideoDeferral) {
  Write(DemuxerStream::AUDIO, Ms(11000));
  Write(DemuxerStream::VIDEO, Ms(10000));
  EXPECT_EQ(Update(Ms(10000)), Transition::kNone);

  // Audio runs out too while the video underflow is deferred: start now.
  Advance(Ms(1000));
  EXPECT_EQ(Update(Ms(11000)), Transition::kUnderflowStarted);
}

TEST_F(PlaybackUnderflowDetectorTest, VideoUnderflowWithoutAudioIsImmediate) {
  detector_.Reset(/*has_audio=*/false, /*has_video=*/true);
  Write(DemuxerStream::VIDEO, Ms(10000));

  EXPECT_EQ(Update(Ms(10000)), Transition::kUnderflowStarted);
}

TEST_F(PlaybackUnderflowDetectorTest,
       VideoUnderflowAfterAudioEosWithDataAheadIsDeferred) {
  // End of stream has been written for audio, but the written audio still
  // keeps the clock going: video starvation is tolerated as usual.
  Write(DemuxerStream::AUDIO, Ms(20000));
  detector_.OnEndOfStreamWritten(DemuxerStream::AUDIO);
  Write(DemuxerStream::VIDEO, Ms(10000));

  EXPECT_EQ(Update(Ms(10000)), Transition::kNone);
  Advance(Ms(2900));
  EXPECT_EQ(Update(Ms(12900)), Transition::kNone);
  Advance(Ms(100));
  EXPECT_EQ(Update(Ms(13000)), Transition::kUnderflowStarted);
}

TEST_F(PlaybackUnderflowDetectorTest,
       VideoUnderflowDuringDeferralIsImmediateOnceAudioPlaysOut) {
  Write(DemuxerStream::AUDIO, Ms(11000));
  detector_.OnEndOfStreamWritten(DemuxerStream::AUDIO);
  Write(DemuxerStream::VIDEO, Ms(10000));
  EXPECT_EQ(Update(Ms(10000)), Transition::kNone);

  // The last audio plays out during the delay: nothing keeps the clock going.
  Advance(Ms(1000));
  EXPECT_EQ(Update(Ms(11000)), Transition::kUnderflowStarted);
}

TEST_F(PlaybackUnderflowDetectorTest,
       VideoUnderflowAfterAudioPlayedOutIsImmediate) {
  // End of stream has been written for audio, and all of it has played.
  Write(DemuxerStream::AUDIO, Ms(9000));
  detector_.OnEndOfStreamWritten(DemuxerStream::AUDIO);
  Write(DemuxerStream::VIDEO, Ms(10000));

  EXPECT_EQ(Update(Ms(10000)), Transition::kUnderflowStarted);
}

TEST_F(PlaybackUnderflowDetectorTest, NoUnderflowWithoutPendingRead) {
  // Ran out of written audio, but the renderer isn't waiting on the demuxer
  // (e.g. it is throttling audio writes), so this is not an underflow.
  Write(DemuxerStream::AUDIO, Ms(10000));
  Write(DemuxerStream::VIDEO, Ms(60000));

  EXPECT_EQ(Update(Ms(10000), /*audio_read_pending=*/false), Transition::kNone);
}

TEST_F(PlaybackUnderflowDetectorTest, ResumesWhenPipelineIsSaturated) {
  Write(DemuxerStream::AUDIO, Ms(10000));
  Write(DemuxerStream::VIDEO, Ms(60000));
  ASSERT_EQ(Update(Ms(10000)), Transition::kUnderflowStarted);

  // Some audio arrives, then the pipeline stops asking for more before
  // reaching |resume_threshold| (e.g. its queues are full).
  Write(DemuxerStream::AUDIO, Ms(10200));
  Advance(Ms(100));
  EXPECT_EQ(Update(Ms(10000), /*audio_read_pending=*/false), Transition::kNone);
  Advance(Ms(800));
  EXPECT_EQ(Update(Ms(10000), /*audio_read_pending=*/false), Transition::kNone);
  Advance(Ms(200));
  EXPECT_EQ(Update(Ms(10000), /*audio_read_pending=*/false),
            Transition::kUnderflowEnded);
}

TEST_F(PlaybackUnderflowDetectorTest, NoSaturationResumeWhileReadsContinue) {
  Write(DemuxerStream::AUDIO, Ms(10000));
  Write(DemuxerStream::VIDEO, Ms(60000));
  ASSERT_EQ(Update(Ms(10000)), Transition::kUnderflowStarted);

  // Reads keep cycling (pending at each check): wait for the threshold.
  Write(DemuxerStream::AUDIO, Ms(10200));
  for (int i = 0; i < 30; ++i) {
    Advance(Ms(100));
    EXPECT_EQ(Update(Ms(10000)), Transition::kNone);
  }
}

TEST_F(PlaybackUnderflowDetectorTest, NoSaturationResumeWhileWritesContinue) {
  Write(DemuxerStream::AUDIO, Ms(10000));
  Write(DemuxerStream::VIDEO, Ms(60000));
  ASSERT_EQ(Update(Ms(10000)), Transition::kUnderflowStarted);

  // Small reads complete between checks, so no read is pending whenever the
  // state is checked, but data keeps being written: the SbPlayer is still
  // asking for data, so wait for the threshold.
  for (int i = 1; i <= 30; ++i) {
    Write(DemuxerStream::AUDIO, Ms(10100) + Ms(10) * i);
    Advance(Ms(100));
    EXPECT_EQ(Update(Ms(10000), /*audio_read_pending=*/false),
              Transition::kNone);
  }
}

TEST_F(PlaybackUnderflowDetectorTest, NoSaturationResumeWhileReadIsPending) {
  Write(DemuxerStream::AUDIO, Ms(10000));
  Write(DemuxerStream::VIDEO, Ms(60000));
  ASSERT_EQ(Update(Ms(10000)), Transition::kUnderflowStarted);

  // Still waiting on the demuxer: keep waiting, however long it takes.
  Advance(Ms(5000));
  EXPECT_EQ(Update(Ms(10000)), Transition::kNone);
}

TEST_F(PlaybackUnderflowDetectorTest, ResumesWhenStarvedStreamStopsAsking) {
  Write(DemuxerStream::AUDIO, Ms(10000));
  Write(DemuxerStream::VIDEO, Ms(60000));
  ASSERT_EQ(Update(Ms(10000)), Transition::kUnderflowStarted);

  // The SbPlayer stops asking for audio with nothing ahead. No data would
  // ever arrive, so resume after |saturation_delay| rather than staying
  // paused forever.
  Advance(Ms(900));
  EXPECT_EQ(Update(Ms(10000), /*audio_read_pending=*/false), Transition::kNone);
  Advance(Ms(100));
  EXPECT_EQ(Update(Ms(10000), /*audio_read_pending=*/false),
            Transition::kUnderflowEnded);
}

TEST_F(PlaybackUnderflowDetectorTest, VideoBehindResumesAtResumeThreshold) {
  const base::TimeDelta media_time = StartVideoUnderflowBehind();
  ASSERT_TRUE(detector_.in_underflow());
  ASSERT_EQ(media_time, Ms(13000));

  // Video must catch up with the playback position, plus |resume_threshold|.
  Write(DemuxerStream::VIDEO, Ms(12000));
  EXPECT_EQ(Update(media_time), Transition::kNone);
  Write(DemuxerStream::VIDEO, Ms(13400));
  EXPECT_EQ(Update(media_time), Transition::kNone);
  Write(DemuxerStream::VIDEO, Ms(13500));
  EXPECT_EQ(Update(media_time), Transition::kUnderflowEnded);
}

TEST_F(PlaybackUnderflowDetectorTest,
       VideoBehindResumesWhenSbPlayerStopsAsking) {
  const base::TimeDelta media_time = StartVideoUnderflowBehind();
  ASSERT_TRUE(detector_.in_underflow());
  ASSERT_EQ(media_time, Ms(13000));

  // Some video is written, but it is still behind the playback position when
  // the SbPlayer stops asking for more (e.g. its frame cache is full while
  // paused). Resume after |saturation_delay| rather than staying stuck.
  Write(DemuxerStream::VIDEO, Ms(12000));
  Advance(Ms(900));
  EXPECT_EQ(Update(media_time, /*audio_read_pending=*/true,
                   /*video_read_pending=*/false),
            Transition::kNone);
  Advance(Ms(100));
  EXPECT_EQ(Update(media_time, /*audio_read_pending=*/true,
                   /*video_read_pending=*/false),
            Transition::kUnderflowEnded);

  // Video is still behind, but isn't waiting on the demuxer: no new underflow
  // until it does.
  Advance(Ms(100));
  EXPECT_EQ(Update(media_time, /*audio_read_pending=*/true,
                   /*video_read_pending=*/false),
            Transition::kNone);
}

TEST_F(PlaybackUnderflowDetectorTest, EndOfStreamIsNeverAnUnderflow) {
  detector_.Reset(/*has_audio=*/false, /*has_video=*/true);
  Write(DemuxerStream::VIDEO, Ms(10000));
  detector_.OnEndOfStreamWritten(DemuxerStream::VIDEO);

  EXPECT_EQ(Update(Ms(20000)), Transition::kNone);
}

TEST_F(PlaybackUnderflowDetectorTest, EndOfStreamEndsUnderflow) {
  Write(DemuxerStream::AUDIO, Ms(10000));
  Write(DemuxerStream::VIDEO, Ms(60000));
  ASSERT_EQ(Update(Ms(10000)), Transition::kUnderflowStarted);

  detector_.OnEndOfStreamWritten(DemuxerStream::AUDIO);
  EXPECT_EQ(Update(Ms(10000)), Transition::kUnderflowEnded);
}

TEST_F(PlaybackUnderflowDetectorTest, WrittenEndIsMonotonic) {
  Write(DemuxerStream::AUDIO, Ms(10000));
  Write(DemuxerStream::VIDEO, Ms(60000));
  // An earlier buffer written later must not move the written end backwards.
  Write(DemuxerStream::AUDIO, Ms(9000));

  EXPECT_EQ(Update(Ms(9500)), Transition::kNone);
}

TEST_F(PlaybackUnderflowDetectorTest,
       OnBuffersWrittenUsesTimestampPlusDuration) {
  auto buffer = base::MakeRefCounted<DecoderBuffer>(0);
  buffer->set_timestamp(Ms(9000));
  buffer->set_duration(Ms(1000));
  detector_.OnBuffersWritten(DemuxerStream::AUDIO, {buffer});
  Write(DemuxerStream::VIDEO, Ms(60000));

  EXPECT_EQ(Update(Ms(9800)), Transition::kNone);
  EXPECT_EQ(Update(Ms(9950)), Transition::kUnderflowStarted);

  detector_.OnBuffersWritten(DemuxerStream::AUDIO,
                             {DecoderBuffer::CreateEOSBuffer()});
  EXPECT_EQ(Update(Ms(9950)), Transition::kUnderflowEnded);
}

TEST_F(PlaybackUnderflowDetectorTest, ResetLeavesUnderflow) {
  Write(DemuxerStream::AUDIO, Ms(10000));
  Write(DemuxerStream::VIDEO, Ms(60000));
  ASSERT_EQ(Update(Ms(10000)), Transition::kUnderflowStarted);

  detector_.Reset(/*has_audio=*/true, /*has_video=*/true);
  EXPECT_FALSE(detector_.in_underflow());
}

TEST_F(PlaybackUnderflowDetectorTest, ClearUnderflowKeepsWrittenData) {
  Write(DemuxerStream::AUDIO, Ms(10000));
  Write(DemuxerStream::VIDEO, Ms(60000));
  ASSERT_EQ(Update(Ms(10000)), Transition::kUnderflowStarted);

  detector_.ClearUnderflow();
  EXPECT_FALSE(detector_.in_underflow());
  // Written data is kept: still starved at the same position.
  EXPECT_EQ(Update(Ms(10000)), Transition::kUnderflowStarted);
}

}  // namespace
}  // namespace media
