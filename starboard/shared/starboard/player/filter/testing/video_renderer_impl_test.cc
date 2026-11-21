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

#include <unistd.h>

#include <atomic>
#include <limits>
#include <list>
#include <memory>
#include <string>
#include <thread>
#include <utility>

#include "starboard/common/time.h"
#include "starboard/shared/starboard/experimental_features.h"
#include "starboard/shared/starboard/player/filter/testing/test_util.h"
#include "starboard/shared/starboard/player/filter/video_decoder_internal.h"
#include "starboard/shared/starboard/player/filter/video_render_algorithm.h"
#include "starboard/shared/starboard/player/filter/video_renderer_impl_internal.h"
#include "starboard/shared/starboard/player/input_buffer_internal.h"
#include "starboard/shared/starboard/player/job_queue.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace {

// The interval at which VideoRendererImpl polls for frames once the preroll
// timeout has expired.
constexpr int64_t kPollIntervalUsec = 25'000;
// The preroll timeout of the fake decoder.  The production value on Android is
// 250 ms, a shorter one keeps the tests fast.
constexpr int64_t kPrerollTimeoutUsec = 40'000;
constexpr int64_t kNoPrerollTimeout = std::numeric_limits<int64_t>::max();
// Far more frames than any test delivers, unless the test sets its own count.
constexpr size_t kPrerollFrameCount = 8;
constexpr int64_t kFrameDurationUsec = 33'000;
// How long to wait for something that is expected to happen.
constexpr int64_t kWaitUsec = 5'000'000;

constexpr int kModePoll = 0;
constexpr int kModeEvent = 1;
constexpr int kModeEventTwoFrames = 2;

class FakeVideoDecoder : public VideoDecoder {
 public:
  FakeVideoDecoder(size_t preroll_frame_count, int64_t preroll_timeout)
      : preroll_frame_count_(preroll_frame_count),
        preroll_timeout_(preroll_timeout) {}

  void Initialize(const DecoderStatusCB& decoder_status_cb,
                  const ErrorCB& error_cb) override {
    decoder_status_cb_ = decoder_status_cb;
  }
  size_t GetPrerollFrameCount() const override { return preroll_frame_count_; }
  int64_t GetPrerollTimeout() const override { return preroll_timeout_; }
  size_t GetMaxNumberOfCachedFrames() const override { return 12; }
  void WriteInputBuffers(const InputBuffers& input_buffers) override {}
  void WriteEndOfStream() override {}
  void Reset() override {}
  SbDecodeTarget GetCurrentDecodeTarget() override {
    return kSbDecodeTargetInvalid;
  }

  // Can be called on any thread, as a real decoder does.
  void DeliverFrame(int64_t timestamp) {
    decoder_status_cb_(kNeedMoreInput, new VideoFrame(timestamp));
  }
  void DeliverEndOfStream() {
    decoder_status_cb_(kBufferFull, VideoFrame::CreateEOSFrame());
  }
  void ReleaseAllFrames() { decoder_status_cb_(kReleaseAllFrames, nullptr); }

 private:
  const size_t preroll_frame_count_;
  const int64_t preroll_timeout_;
  DecoderStatusCB decoder_status_cb_;
};

class FakeMediaTimeProvider : public MediaTimeProvider {
 public:
  void Play() override {}
  void Pause() override {}
  void SetPlaybackRate(double playback_rate) override {}
  void Seek(int64_t seek_to_pts) override {}
  int64_t GetCurrentMediaTime(bool* is_playing,
                              bool* is_eos_played,
                              bool* is_underflow,
                              double* playback_rate) override {
    *is_playing = false;
    *is_eos_played = false;
    *is_underflow = false;
    *playback_rate = 1.0;
    return 0;
  }
  int64_t GetAudioWriteHead() override { return 0; }
  int64_t AdjustTimestampToAudioClock(int64_t timestamp) override {
    return timestamp;
  }
};

class FakeVideoRenderAlgorithm : public VideoRenderAlgorithm {
 public:
  void Render(MediaTimeProvider* media_time_provider,
              std::list<scoped_refptr<VideoFrame>>* frames,
              VideoRendererSink::DrawFrameCB draw_frame_cb) override {}
  void Seek(int64_t seek_to_time) override {}
  int GetDroppedFrames() override { return 0; }
};

scoped_refptr<InputBuffer> CreateVideoInputBuffer(int64_t timestamp) {
  static const uint8_t kData[] = {0};

  SbPlayerSampleInfo sample_info = {};
  sample_info.type = kSbMediaTypeVideo;
  sample_info.buffer = kData;
  sample_info.buffer_size = sizeof(kData);
  sample_info.timestamp = timestamp;
  sample_info.video_sample_info.is_key_frame = true;
  sample_info.video_sample_info.stream_info.codec = kSbMediaVideoCodecH264;
  sample_info.video_sample_info.stream_info.mime = "";
  sample_info.video_sample_info.stream_info.max_video_capabilities = "";
  return new InputBuffer(StubDeallocateSampleFunc, nullptr, nullptr,
                         sample_info);
}

class VideoRendererImplTest : public ::testing::Test {
 protected:
  ~VideoRendererImplTest() override {
    // The renderer has to be destroyed before |job_queue_|.
    video_renderer_.reset();
  }

  void CreateRenderer(int preroll_timeout_mode,
                      size_t preroll_frame_count = kPrerollFrameCount,
                      int64_t preroll_timeout = kPrerollTimeoutUsec) {
    ExperimentalFeatures::Map settings;
    if (preroll_timeout_mode != kModePoll) {
      settings[std::string(kMediaVideoPrerollTimeoutMode.key())] =
          static_cast<int64_t>(preroll_timeout_mode);
    }
    auto decoder = std::make_unique<FakeVideoDecoder>(preroll_frame_count,
                                                      preroll_timeout);
    decoder_ = decoder.get();
    video_renderer_ = std::make_unique<VideoRendererImpl>(
        &job_queue_, std::move(decoder), &media_time_provider_,
        std::make_unique<FakeVideoRenderAlgorithm>(), /*sink=*/nullptr,
        ExperimentalFeatures(std::move(settings)));
    video_renderer_->Initialize(
        [](SbPlayerError error, const std::string& error_message) {
          FAIL() << error_message;
        },
        [this] {
          prerolled_at_ = CurrentMonotonicTime();
          ++prerolled_count_;
        },
        [] {});
  }

  // Seeks and writes one input, after which the fake decoder may deliver
  // frames.  The call to OnSeekTimeout() that the seek schedules is due
  // between |seeked_at_| + |kPrerollTimeoutUsec| and
  // |seek_returned_at_| + |kPrerollTimeoutUsec|, and the first poll can't run
  // before |seeked_at_| + |kPrerollTimeoutUsec| + |kPollIntervalUsec|.
  void SeekAndWriteInput() {
    prerolled_count_ = 0;
    next_frame_timestamp_ = 0;
    seeked_at_ = CurrentMonotonicTime();
    video_renderer_->Seek(0);
    seek_returned_at_ = CurrentMonotonicTime();
    video_renderer_->WriteSamples({CreateVideoInputBuffer(0)});
  }

  void DeliverFrame() {
    decoder_->DeliverFrame(next_frame_timestamp_);
    next_frame_timestamp_ += kFrameDurationUsec;
  }

  // Runs the jobs that are due until |time| has passed.  Every job due at
  // |time| or earlier has run when this returns.
  void RunUntil(int64_t time) {
    while (CurrentMonotonicTime() <= time) {
      job_queue_.RunUntilIdle();
      usleep(500);
    }
    job_queue_.RunUntilIdle();
  }

  // Runs jobs until preroll has ended.  Returns false if it didn't.
  bool RunUntilPrerolled() {
    const int64_t deadline = CurrentMonotonicTime() + kWaitUsec;
    while (prerolled_count_ == 0 && CurrentMonotonicTime() < deadline) {
      job_queue_.RunUntilIdle();
      usleep(500);
    }
    return prerolled_count_ > 0;
  }

  // Runs until the preroll timeout has expired, i.e. until the call to
  // OnSeekTimeout() that was scheduled by the seek has been made.
  void RunUntilTimeoutExpired() {
    RunUntil(seek_returned_at_ + kPrerollTimeoutUsec);
  }

  // True if no poll can have run yet.  The polls are |kPollIntervalUsec| apart
  // and the first one comes that long after the timeout expired, so before
  // this point in time whatever ended preroll wasn't a poll.
  bool IsBeforeFirstPoll() const {
    return CurrentMonotonicTime() <
           seeked_at_ + kPrerollTimeoutUsec + kPollIntervalUsec;
  }

  // The time from the seek to the end of preroll.
  int64_t prerolled_after() const { return prerolled_at_ - seeked_at_; }

  JobQueue job_queue_;
  FakeMediaTimeProvider media_time_provider_;
  FakeVideoDecoder* decoder_ = nullptr;
  std::unique_ptr<VideoRendererImpl> video_renderer_;

  std::atomic_int prerolled_count_{0};
  int64_t prerolled_at_ = 0;
  int64_t seeked_at_ = 0;
  int64_t seek_returned_at_ = 0;
  int64_t next_frame_timestamp_ = 0;
};

class VideoRendererImplPrerollTimeoutTest
    : public VideoRendererImplTest,
      public ::testing::WithParamInterface<int> {};

// The tests below that tell a frame event from a poll by the time are only
// conclusive when the test thread wasn't held up for most of a poll interval.
// They repeat the scenario until it is conclusive.
constexpr int kMaxAttempts = 20;

// 0 frames when the timeout expires, a frame a little later: preroll ends at
// the next poll, not when the frame is decoded.  An unknown mode is mode 0.
TEST_P(VideoRendererImplPrerollTimeoutTest, PollModeWaitsForThePoll) {
  CreateRenderer(GetParam());

  bool conclusive = false;
  for (int i = 0; i < kMaxAttempts && !conclusive; ++i) {
    SeekAndWriteInput();
    RunUntilTimeoutExpired();
    EXPECT_EQ(prerolled_count_.load(), 0);

    DeliverFrame();
    job_queue_.RunUntilIdle();
    if (IsBeforeFirstPoll()) {
      conclusive = true;
      EXPECT_EQ(prerolled_count_.load(), 0);
    }

    ASSERT_TRUE(RunUntilPrerolled());
    EXPECT_GE(prerolled_after(), kPrerollTimeoutUsec + kPollIntervalUsec);
    RunUntil(CurrentMonotonicTime() + 2 * kPollIntervalUsec);
    EXPECT_EQ(prerolled_count_.load(), 1);
  }
  EXPECT_TRUE(conclusive);
}

INSTANTIATE_TEST_SUITE_P(PollModes,
                         VideoRendererImplPrerollTimeoutTest,
                         ::testing::Values(kModePoll, 3, -1));

class VideoRendererImplPrerollEventTest
    : public VideoRendererImplTest,
      public ::testing::WithParamInterface<int> {};

INSTANTIATE_TEST_SUITE_P(EventModes,
                         VideoRendererImplPrerollEventTest,
                         ::testing::Values(kModeEvent, kModeEventTwoFrames));

// The same input as above with mode 1: preroll ends when the frame is decoded,
// before the poll.
TEST_F(VideoRendererImplTest, EventModeEndsPrerollOnTheFrame) {
  CreateRenderer(kModeEvent);

  bool conclusive = false;
  for (int i = 0; i < kMaxAttempts && !conclusive; ++i) {
    SeekAndWriteInput();
    RunUntilTimeoutExpired();
    EXPECT_EQ(prerolled_count_.load(), 0);

    DeliverFrame();
    job_queue_.RunUntilIdle();
    if (IsBeforeFirstPoll()) {
      conclusive = true;
      EXPECT_EQ(prerolled_count_.load(), 1);
      EXPECT_LT(prerolled_after(), kPrerollTimeoutUsec + kPollIntervalUsec);
    }

    ASSERT_TRUE(RunUntilPrerolled());
    RunUntil(CurrentMonotonicTime() + 2 * kPollIntervalUsec);
    EXPECT_EQ(prerolled_count_.load(), 1);
  }
  EXPECT_TRUE(conclusive);
}

// Mode 1 with a frame decoded before the timeout expires: preroll ends when
// the timeout expires, as in mode 0.
TEST_F(VideoRendererImplTest, EventModeEndsPrerollAtTheTimeoutWithOneFrame) {
  CreateRenderer(kModeEvent);

  SeekAndWriteInput();
  DeliverFrame();
  job_queue_.RunUntilIdle();
  // Only the timeout can end preroll, and it hasn't expired, unless this
  // thread was held up for that long.
  if (CurrentMonotonicTime() < seeked_at_ + kPrerollTimeoutUsec) {
    EXPECT_EQ(prerolled_count_.load(), 0);
  }

  ASSERT_TRUE(RunUntilPrerolled());
  EXPECT_GE(prerolled_after(), kPrerollTimeoutUsec);
  RunUntil(CurrentMonotonicTime() + 2 * kPollIntervalUsec);
  EXPECT_EQ(prerolled_count_.load(), 1);
}

// Mode 2 with one frame when the timeout expires: preroll doesn't end.  It
// ends when the second frame is decoded, before the poll.
TEST_F(VideoRendererImplTest, TwoFrameModeEndsPrerollOnTheSecondFrame) {
  CreateRenderer(kModeEventTwoFrames);

  bool conclusive = false;
  for (int i = 0; i < kMaxAttempts && !conclusive; ++i) {
    SeekAndWriteInput();
    DeliverFrame();
    RunUntilTimeoutExpired();
    // The fallback to one frame takes two polls, so it can't have happened
    // before the first poll.
    const bool first_check_is_conclusive = IsBeforeFirstPoll();
    if (first_check_is_conclusive) {
      EXPECT_EQ(prerolled_count_.load(), 0);
    }

    DeliverFrame();
    job_queue_.RunUntilIdle();
    if (first_check_is_conclusive && IsBeforeFirstPoll()) {
      conclusive = true;
      EXPECT_EQ(prerolled_count_.load(), 1);
      EXPECT_LT(prerolled_after(), kPrerollTimeoutUsec + kPollIntervalUsec);
    }

    ASSERT_TRUE(RunUntilPrerolled());
    RunUntil(CurrentMonotonicTime() + 2 * kPollIntervalUsec);
    EXPECT_EQ(prerolled_count_.load(), 1);
  }
  EXPECT_TRUE(conclusive);
}

// Mode 2 with two frames when the timeout expires: preroll ends then.
TEST_F(VideoRendererImplTest, TwoFrameModeEndsPrerollAtTheTimeoutWithTwo) {
  CreateRenderer(kModeEventTwoFrames);

  SeekAndWriteInput();
  DeliverFrame();
  DeliverFrame();
  RunUntilTimeoutExpired();

  EXPECT_EQ(prerolled_count_.load(), 1);
  EXPECT_GE(prerolled_after(), kPrerollTimeoutUsec);
  RunUntil(CurrentMonotonicTime() + 3 * kPollIntervalUsec);
  EXPECT_EQ(prerolled_count_.load(), 1);
}

// Mode 2 with one frame and never a second: preroll ends with the one frame
// at the second poll, about 50 ms after the timeout expired.
TEST_F(VideoRendererImplTest, TwoFrameModeFallsBackToOneFrame) {
  CreateRenderer(kModeEventTwoFrames);

  bool conclusive = false;
  for (int i = 0; i < kMaxAttempts && !conclusive; ++i) {
    SeekAndWriteInput();
    DeliverFrame();

    ASSERT_TRUE(RunUntilPrerolled());
    // Not before the second poll.
    EXPECT_GE(prerolled_after(), kPrerollTimeoutUsec + 2 * kPollIntervalUsec);
    // And at the second poll, not the third, which can't run before the time
    // below.  When preroll ended later than that the test thread was held up
    // and the attempt says nothing.
    conclusive =
        prerolled_after() < kPrerollTimeoutUsec + 3 * kPollIntervalUsec;
    RunUntil(CurrentMonotonicTime() + 3 * kPollIntervalUsec);
    EXPECT_EQ(prerolled_count_.load(), 1);
  }
  EXPECT_TRUE(conclusive);
}

// Mode 2: a seek while waiting for the second frame starts the count of polls
// over, so the fallback of the new seek again takes two polls.
TEST_F(VideoRendererImplTest, SeekResetsTheFallback) {
  CreateRenderer(kModeEventTwoFrames);

  bool conclusive = false;
  for (int i = 0; i < kMaxAttempts && !conclusive; ++i) {
    SeekAndWriteInput();
    DeliverFrame();
    // Until the first poll has found the one frame, and before the second.
    RunUntil(seek_returned_at_ + kPrerollTimeoutUsec + kPollIntervalUsec +
             5'000);
    if (prerolled_count_.load() != 0) {
      continue;
    }

    SeekAndWriteInput();
    DeliverFrame();
    ASSERT_TRUE(RunUntilPrerolled());
    // Had the seek kept the count, or taken the first call for a poll, preroll
    // would have ended a poll earlier.
    EXPECT_GE(prerolled_after(), kPrerollTimeoutUsec + 2 * kPollIntervalUsec);
    conclusive =
        prerolled_after() < kPrerollTimeoutUsec + 3 * kPollIntervalUsec;
    RunUntil(CurrentMonotonicTime() + 3 * kPollIntervalUsec);
    EXPECT_EQ(prerolled_count_.load(), 1);
  }
  EXPECT_TRUE(conclusive);
}

// Mode 2: releasing all frames after the first poll found one frame resets the
// consecutive one-frame poll count even when a replacement frame is decoded
// before the second poll runs, so the replacement frame still gets two full
// polls before falling back.
TEST_F(VideoRendererImplTest, ReleaseAllFramesResetsTheFallbackCount) {
  CreateRenderer(kModeEventTwoFrames);

  bool conclusive = false;
  for (int i = 0; i < kMaxAttempts && !conclusive; ++i) {
    SeekAndWriteInput();
    DeliverFrame();
    RunUntilTimeoutExpired();
    if (!IsBeforeFirstPoll()) {
      continue;
    }

    // Let the first poll observe 1 frame (one_frame_polls_ == 1).
    RunUntil(seek_returned_at_ + kPrerollTimeoutUsec + kPollIntervalUsec +
             5'000);
    if (prerolled_count_.load() != 0 ||
        CurrentMonotonicTime() >=
            seeked_at_ + kPrerollTimeoutUsec + 2 * kPollIntervalUsec) {
      continue;
    }

    // Release all frames and deliver a fresh replacement frame before the
    // second poll runs.  The second poll must treat this as poll #1 of the new
    // frame (not poll #2 of the old one) and wait for the third poll before
    // falling back.
    const int64_t third_poll_floor =
        seeked_at_ + kPrerollTimeoutUsec + 3 * kPollIntervalUsec;
    decoder_->ReleaseAllFrames();
    DeliverFrame();
    if (CurrentMonotonicTime() >=
        seeked_at_ + kPrerollTimeoutUsec + 2 * kPollIntervalUsec) {
      continue;
    }
    ASSERT_TRUE(RunUntilPrerolled());
    EXPECT_GE(prerolled_at_, third_poll_floor);
    conclusive = true;
    RunUntil(CurrentMonotonicTime() + 2 * kPollIntervalUsec);
    EXPECT_EQ(prerolled_count_.load(), 1);
  }
  EXPECT_TRUE(conclusive);
}

// Mode 2: a frame that is dropped because it is before the seek time doesn't
// count, neither for the two frames nor for the fallback.
TEST_F(VideoRendererImplTest, TwoFrameModeIgnoresFramesBeforeTheSeekTime) {
  CreateRenderer(kModeEventTwoFrames);

  prerolled_count_ = 0;
  seeked_at_ = CurrentMonotonicTime();
  video_renderer_->Seek(10 * kFrameDurationUsec);
  seek_returned_at_ = CurrentMonotonicTime();
  video_renderer_->WriteSamples({CreateVideoInputBuffer(0)});
  decoder_->DeliverFrame(0);
  decoder_->DeliverFrame(kFrameDurationUsec);

  RunUntil(seek_returned_at_ + kPrerollTimeoutUsec + 5 * kPollIntervalUsec);
  EXPECT_EQ(prerolled_count_.load(), 0);

  decoder_->DeliverFrame(10 * kFrameDurationUsec);
  decoder_->DeliverFrame(11 * kFrameDurationUsec);
  job_queue_.RunUntilIdle();
  EXPECT_EQ(prerolled_count_.load(), 1);
}

// Mode 2 with the only frame decoded after the timeout expired: the fallback
// still ends preroll, two polls after the frame.
TEST_F(VideoRendererImplTest, TwoFrameModeFallsBackWhenTheFrameComesLate) {
  CreateRenderer(kModeEventTwoFrames);

  SeekAndWriteInput();
  RunUntilTimeoutExpired();
  EXPECT_EQ(prerolled_count_.load(), 0);
  DeliverFrame();

  ASSERT_TRUE(RunUntilPrerolled());
  EXPECT_GE(prerolled_after(), kPrerollTimeoutUsec + 2 * kPollIntervalUsec);
  RunUntil(CurrentMonotonicTime() + 3 * kPollIntervalUsec);
  EXPECT_EQ(prerolled_count_.load(), 1);
}

// No frame at all: preroll doesn't end in any mode, as before, and the poll
// keeps going so a frame still ends it.
TEST_P(VideoRendererImplPrerollEventTest, KeepsPollingWithoutFrames) {
  CreateRenderer(GetParam());

  SeekAndWriteInput();
  RunUntil(seeked_at_ + kPrerollTimeoutUsec + 6 * kPollIntervalUsec);
  EXPECT_EQ(prerolled_count_.load(), 0);

  DeliverFrame();
  DeliverFrame();
  ASSERT_TRUE(RunUntilPrerolled());
  RunUntil(CurrentMonotonicTime() + 3 * kPollIntervalUsec);
  EXPECT_EQ(prerolled_count_.load(), 1);
}

// End of stream after the timeout expired without a frame ends preroll.
TEST_P(VideoRendererImplPrerollEventTest, EndOfStreamAfterTimeoutEndsPreroll) {
  CreateRenderer(GetParam());

  SeekAndWriteInput();
  RunUntilTimeoutExpired();
  EXPECT_EQ(prerolled_count_.load(), 0);

  video_renderer_->WriteEndOfStream();
  decoder_->DeliverEndOfStream();
  job_queue_.RunUntilIdle();
  EXPECT_EQ(prerolled_count_.load(), 1);

  RunUntil(CurrentMonotonicTime() + 3 * kPollIntervalUsec);
  EXPECT_EQ(prerolled_count_.load(), 1);
}

// Frames decoded on another thread at about the time of a poll: the prerolled
// callback runs once, whichever of the two ends preroll.
TEST_P(VideoRendererImplPrerollEventTest, PrerolledOnceWhenEventMeetsPoll) {
  CreateRenderer(GetParam());

  for (int i = 0; i < 20; ++i) {
    SeekAndWriteInput();
    // Spread the frames over the 2 ms around the first poll.
    const int64_t deliver_at = seeked_at_ + kPrerollTimeoutUsec +
                               kPollIntervalUsec - 1'000 + (i % 20) * 100;
    std::thread decoder_thread([this, deliver_at] {
      while (CurrentMonotonicTime() < deliver_at) {
      }
      DeliverFrame();
      DeliverFrame();
    });

    EXPECT_TRUE(RunUntilPrerolled());
    // The decoder must be idle before the next seek, as after Reset().
    decoder_thread.join();
    RunUntil(CurrentMonotonicTime() + kPollIntervalUsec + 2'000);
    EXPECT_EQ(prerolled_count_.load(), 1) << "iteration " << i;
  }
}

// The same with the frames at about the time the timeout expires.
TEST_P(VideoRendererImplPrerollEventTest, PrerolledOnceWhenEventMeetsTimeout) {
  CreateRenderer(GetParam());

  for (int i = 0; i < 20; ++i) {
    SeekAndWriteInput();
    const int64_t deliver_at =
        seeked_at_ + kPrerollTimeoutUsec - 1'000 + (i % 20) * 100;
    std::thread decoder_thread([this, deliver_at] {
      while (CurrentMonotonicTime() < deliver_at) {
      }
      DeliverFrame();
      DeliverFrame();
    });

    EXPECT_TRUE(RunUntilPrerolled());
    decoder_thread.join();
    RunUntil(CurrentMonotonicTime() + kPollIntervalUsec + 2'000);
    EXPECT_EQ(prerolled_count_.load(), 1) << "iteration " << i;
  }
}

class VideoRendererImplAllModesTest
    : public VideoRendererImplTest,
      public ::testing::WithParamInterface<int> {};

INSTANTIATE_TEST_SUITE_P(AllModes,
                         VideoRendererImplAllModesTest,
                         ::testing::Values(kModePoll,
                                           kModeEvent,
                                           kModeEventTwoFrames));

// A frame decoded before the timeout expires, fewer than the preroll frame
// count: with one frame required preroll ends when the timeout expires, at the
// call made by the timer and not at a poll.
TEST_P(VideoRendererImplPrerollTimeoutTest, OneFrameEndsPrerollAtTheTimeout) {
  CreateRenderer(GetParam());

  bool conclusive = false;
  for (int i = 0; i < kMaxAttempts && !conclusive; ++i) {
    SeekAndWriteInput();
    DeliverFrame();
    ASSERT_TRUE(RunUntilPrerolled());
    EXPECT_GE(prerolled_after(), kPrerollTimeoutUsec);
    conclusive = prerolled_after() < kPrerollTimeoutUsec + kPollIntervalUsec;
    RunUntil(CurrentMonotonicTime() + 2 * kPollIntervalUsec);
    EXPECT_EQ(prerolled_count_.load(), 1);
  }
  EXPECT_TRUE(conclusive);
}

// The preroll frame count is reached before the timeout expires: preroll ends
// on that frame in every mode, and the timeout does nothing.
TEST_P(VideoRendererImplAllModesTest, FrameCountBeforeTimeout) {
  const size_t kFrameCount = 3;
  CreateRenderer(GetParam(), kFrameCount);

  SeekAndWriteInput();
  for (size_t i = 0; i < kFrameCount - 1; ++i) {
    DeliverFrame();
  }
  job_queue_.RunUntilIdle();
  if (CurrentMonotonicTime() < seeked_at_ + kPrerollTimeoutUsec) {
    EXPECT_EQ(prerolled_count_.load(), 0);
  }
  DeliverFrame();
  job_queue_.RunUntilIdle();
  EXPECT_EQ(prerolled_count_.load(), 1);

  RunUntil(seeked_at_ + kPrerollTimeoutUsec + 3 * kPollIntervalUsec);
  EXPECT_EQ(prerolled_count_.load(), 1);
}

// A decoder without a preroll timeout: one frame never ends preroll, in every
// mode.
TEST_P(VideoRendererImplAllModesTest, NoTimeout) {
  CreateRenderer(GetParam(), kPrerollFrameCount, kNoPrerollTimeout);

  SeekAndWriteInput();
  DeliverFrame();
  DeliverFrame();
  RunUntil(seeked_at_ + kPrerollTimeoutUsec + 4 * kPollIntervalUsec);
  EXPECT_EQ(prerolled_count_.load(), 0);
}

// A seek after the timeout expired starts over: the next preroll doesn't end
// on a frame decoded before its own timeout expires.
TEST_P(VideoRendererImplAllModesTest, SeekRearmsTheTimeout) {
  CreateRenderer(GetParam());

  SeekAndWriteInput();
  RunUntilTimeoutExpired();
  EXPECT_EQ(prerolled_count_.load(), 0);

  SeekAndWriteInput();
  DeliverFrame();
  DeliverFrame();
  job_queue_.RunUntilIdle();
  if (CurrentMonotonicTime() < seeked_at_ + kPrerollTimeoutUsec) {
    EXPECT_EQ(prerolled_count_.load(), 0);
  }

  ASSERT_TRUE(RunUntilPrerolled());
  EXPECT_GE(prerolled_after(), kPrerollTimeoutUsec);
  RunUntil(CurrentMonotonicTime() + 3 * kPollIntervalUsec);
  EXPECT_EQ(prerolled_count_.load(), 1);
}

}  // namespace
}  // namespace starboard
