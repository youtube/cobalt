// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/message_loop.h"
#include "base/stl_util.h"
#include "base/stringprintf.h"
#include "base/synchronization/lock.h"
#include "base/timer.h"
#include "media/base/data_buffer.h"
#include "media/base/gmock_callback_support.h"
#include "media/base/limits.h"
#include "media/base/mock_filters.h"
#include "media/base/test_helpers.h"
#include "media/base/video_frame.h"
#include "media/filters/video_renderer_base.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::InSequence;
using ::testing::Invoke;
using ::testing::NotNull;
using ::testing::Return;
using ::testing::ReturnRef;
using ::testing::StrictMock;

namespace media {

static const int kFrameDurationInMs = 10;
static const int kVideoDurationInMs = kFrameDurationInMs * 100;
static const gfx::Size kNaturalSize(16u, 16u);

class VideoRendererBaseTest : public ::testing::Test {
 public:
  VideoRendererBaseTest()
      : decoder_(new MockVideoDecoder()),
        demuxer_stream_(new MockDemuxerStream()) {
    renderer_ = new VideoRendererBase(
        base::Bind(&VideoRendererBaseTest::OnPaint, base::Unretained(this)),
        base::Bind(&VideoRendererBaseTest::OnSetOpaque, base::Unretained(this)),
        true);

    EXPECT_CALL(*demuxer_stream_, type())
        .WillRepeatedly(Return(DemuxerStream::VIDEO));

    // We expect these to be called but we don't care how/when.
    EXPECT_CALL(*decoder_, Stop(_))
        .WillRepeatedly(RunClosure<0>());
    EXPECT_CALL(statistics_cb_object_, OnStatistics(_))
        .Times(AnyNumber());
    EXPECT_CALL(*this, OnTimeUpdate(_))
        .Times(AnyNumber());
    EXPECT_CALL(*this, OnPaint())
        .Times(AnyNumber());
    EXPECT_CALL(*this, OnSetOpaque(_))
        .Times(AnyNumber());
  }

  virtual ~VideoRendererBaseTest() {}

  // Callbacks passed into VideoRendererBase().
  MOCK_CONST_METHOD0(OnPaint, void());
  MOCK_CONST_METHOD1(OnSetOpaque, void(bool));

  // Callbacks passed into Initialize().
  MOCK_METHOD1(OnTimeUpdate, void(base::TimeDelta));
  MOCK_METHOD1(OnNaturalSizeChanged, void(const gfx::Size&));

  void Initialize() {
    InitializeWithDuration(kVideoDurationInMs);
  }

  void InitializeWithDuration(int duration_ms) {
    duration_ = base::TimeDelta::FromMilliseconds(duration_ms);

    // Monitor reads from the decoder.
    EXPECT_CALL(*decoder_, Read(_))
        .WillRepeatedly(Invoke(this, &VideoRendererBaseTest::FrameRequested));

    EXPECT_CALL(*decoder_, Reset(_))
        .WillRepeatedly(Invoke(this, &VideoRendererBaseTest::FlushRequested));

    InSequence s;

    EXPECT_CALL(*decoder_, Initialize(_, _, _))
        .WillOnce(RunCallback<1>(PIPELINE_OK));

    // Set playback rate before anything else happens.
    renderer_->SetPlaybackRate(1.0f);

    // Initialize, we shouldn't have any reads.
    InitializeRenderer(PIPELINE_OK);

    // We expect the video size to be set.
    EXPECT_CALL(*this, OnNaturalSizeChanged(kNaturalSize));

    // Start prerolling.
    QueuePrerollFrames(0);
    Preroll(0, PIPELINE_OK);
  }

  void InitializeRenderer(PipelineStatus expected) {
    SCOPED_TRACE(base::StringPrintf("InitializeRenderer(%d)", expected));
    VideoRendererBase::VideoDecoderList decoders;
    decoders.push_back(decoder_);

    WaitableMessageLoopEvent event;
    renderer_->Initialize(
        demuxer_stream_,
        decoders,
        event.GetPipelineStatusCB(),
        base::Bind(&MockStatisticsCB::OnStatistics,
                   base::Unretained(&statistics_cb_object_)),
        base::Bind(&VideoRendererBaseTest::OnTimeUpdate,
                   base::Unretained(this)),
        base::Bind(&VideoRendererBaseTest::OnNaturalSizeChanged,
                   base::Unretained(this)),
        ended_event_.GetClosure(),
        error_event_.GetPipelineStatusCB(),
        base::Bind(&VideoRendererBaseTest::GetTime, base::Unretained(this)),
        base::Bind(&VideoRendererBaseTest::GetDuration,
                   base::Unretained(this)));
    event.RunAndWaitForStatus(expected);
  }

  void Play() {
    SCOPED_TRACE("Play()");
    WaitableMessageLoopEvent event;
    renderer_->Play(event.GetClosure());
    event.RunAndWait();
  }

  void Preroll(int timestamp_ms, PipelineStatus expected) {
    SCOPED_TRACE(base::StringPrintf("Preroll(%d, %d)", timestamp_ms, expected));
    WaitableMessageLoopEvent event;
    renderer_->Preroll(
        base::TimeDelta::FromMilliseconds(timestamp_ms),
        event.GetPipelineStatusCB());
    event.RunAndWaitForStatus(expected);
  }

  void Pause() {
    SCOPED_TRACE("Pause()");
    WaitableMessageLoopEvent event;
    renderer_->Pause(event.GetClosure());
    event.RunAndWait();
  }

  void Flush() {
    SCOPED_TRACE("Flush()");
    WaitableMessageLoopEvent event;
    renderer_->Flush(event.GetClosure());
    event.RunAndWait();
  }

  void Stop() {
    SCOPED_TRACE("Stop()");
    WaitableMessageLoopEvent event;
    renderer_->Stop(event.GetClosure());
    event.RunAndWait();
  }

  void Shutdown() {
    Pause();
    Flush();
    Stop();
  }

  // Queues a VideoFrame with |next_frame_timestamp_|.
  void QueueNextFrame() {
    DCHECK_EQ(&message_loop_, MessageLoop::current());
    DCHECK_LT(next_frame_timestamp_.InMicroseconds(),
              duration_.InMicroseconds());

    scoped_refptr<VideoFrame> frame = VideoFrame::CreateFrame(
        VideoFrame::RGB32, kNaturalSize, gfx::Rect(kNaturalSize), kNaturalSize,
        next_frame_timestamp_);
    decode_results_.push_back(std::make_pair(
        VideoDecoder::kOk, frame));
    next_frame_timestamp_ +=
        base::TimeDelta::FromMilliseconds(kFrameDurationInMs);
  }

  void QueueEndOfStream() {
    DCHECK_EQ(&message_loop_, MessageLoop::current());
    decode_results_.push_back(std::make_pair(
        VideoDecoder::kOk, VideoFrame::CreateEmptyFrame()));
  }

  void QueueDecodeError() {
    DCHECK_EQ(&message_loop_, MessageLoop::current());
    scoped_refptr<VideoFrame> null_frame;
    decode_results_.push_back(std::make_pair(
        VideoDecoder::kDecodeError, null_frame));
  }

  void QueueAbortedRead() {
    DCHECK_EQ(&message_loop_, MessageLoop::current());
    scoped_refptr<VideoFrame> null_frame;
    decode_results_.push_back(std::make_pair(
        VideoDecoder::kOk, null_frame));
  }

  void QueuePrerollFrames(int timestamp_ms) {
    DCHECK_EQ(&message_loop_, MessageLoop::current());
    next_frame_timestamp_ = base::TimeDelta();
    base::TimeDelta timestamp = base::TimeDelta::FromMilliseconds(timestamp_ms);
    while (next_frame_timestamp_ < timestamp) {
      QueueNextFrame();
    }

    // Queue the frame at |timestamp| plus additional ones for prerolling.
    for (int i = 0; i < limits::kMaxVideoFrames; ++i) {
      QueueNextFrame();
    }
  }

  scoped_refptr<VideoFrame> GetCurrentFrame() {
    scoped_refptr<VideoFrame> frame;
    renderer_->GetCurrentFrame(&frame);
    renderer_->PutCurrentFrame(frame);
    return frame;
  }

  int GetCurrentTimestampInMs() {
    scoped_refptr<VideoFrame> frame = GetCurrentFrame();
    if (!frame)
      return -1;
    return frame->GetTimestamp().InMilliseconds();
  }

  void WaitForError(PipelineStatus expected) {
    SCOPED_TRACE(base::StringPrintf("WaitForError(%d)", expected));
    error_event_.RunAndWaitForStatus(expected);
  }

  void WaitForEnded() {
    SCOPED_TRACE("WaitForEnded()");
    ended_event_.RunAndWait();
  }

  void WaitForPendingRead() {
    SCOPED_TRACE("WaitForPendingRead()");
    if (!read_cb_.is_null())
      return;

    DCHECK(pending_read_cb_.is_null());

    WaitableMessageLoopEvent event;
    pending_read_cb_ = event.GetClosure();
    event.RunAndWait();

    DCHECK(!read_cb_.is_null());
    DCHECK(pending_read_cb_.is_null());
  }

  void SatisfyPendingRead() {
    CHECK(!read_cb_.is_null());
    CHECK(!decode_results_.empty());

    base::Closure closure = base::Bind(
        read_cb_, decode_results_.front().first,
        decode_results_.front().second);

    read_cb_.Reset();
    decode_results_.pop_front();

    message_loop_.PostTask(FROM_HERE, closure);
  }

  void AdvanceTimeInMs(int time_ms) {
    DCHECK_EQ(&message_loop_, MessageLoop::current());
    base::AutoLock l(lock_);
    time_ += base::TimeDelta::FromMilliseconds(time_ms);
    DCHECK_LE(time_.InMicroseconds(), duration_.InMicroseconds());
  }

 protected:
  // Fixture members.
  scoped_refptr<VideoRendererBase> renderer_;
  scoped_refptr<MockVideoDecoder> decoder_;
  scoped_refptr<MockDemuxerStream> demuxer_stream_;
  MockStatisticsCB statistics_cb_object_;

 private:
  base::TimeDelta GetTime() {
    base::AutoLock l(lock_);
    return time_;
  }

  base::TimeDelta GetDuration() {
    return duration_;
  }

  void FrameRequested(const VideoDecoder::ReadCB& read_cb) {
    // TODO(scherkus): Make VideoRendererBase call on right thread.
    if (&message_loop_ != MessageLoop::current()) {
      message_loop_.PostTask(FROM_HERE, base::Bind(
          &VideoRendererBaseTest::FrameRequested, base::Unretained(this),
          read_cb));
      return;
    }

    CHECK(read_cb_.is_null());
    read_cb_ = read_cb;

    // Wake up WaitForPendingRead() if needed.
    if (!pending_read_cb_.is_null())
      base::ResetAndReturn(&pending_read_cb_).Run();

    if (decode_results_.empty())
      return;

    SatisfyPendingRead();
  }

  void FlushRequested(const base::Closure& callback) {
    // TODO(scherkus): Make VideoRendererBase call on right thread.
    if (&message_loop_ != MessageLoop::current()) {
      message_loop_.PostTask(FROM_HERE, base::Bind(
          &VideoRendererBaseTest::FlushRequested, base::Unretained(this),
          callback));
      return;
    }

    decode_results_.clear();
    if (!read_cb_.is_null()) {
      QueueAbortedRead();
      SatisfyPendingRead();
    }

    message_loop_.PostTask(FROM_HERE, callback);
  }

  MessageLoop message_loop_;

  // Used to protect |time_|.
  base::Lock lock_;
  base::TimeDelta time_;

  // Used for satisfying reads.
  VideoDecoder::ReadCB read_cb_;
  base::TimeDelta next_frame_timestamp_;
  base::TimeDelta duration_;

  WaitableMessageLoopEvent error_event_;
  WaitableMessageLoopEvent ended_event_;
  base::Closure pending_read_cb_;

  std::deque<std::pair<
      VideoDecoder::Status, scoped_refptr<VideoFrame> > > decode_results_;

  DISALLOW_COPY_AND_ASSIGN(VideoRendererBaseTest);
};

TEST_F(VideoRendererBaseTest, Initialize) {
  Initialize();
  EXPECT_EQ(0, GetCurrentTimestampInMs());
  Shutdown();
}

TEST_F(VideoRendererBaseTest, Play) {
  Initialize();
  Play();
  Shutdown();
}

TEST_F(VideoRendererBaseTest, EndOfStream_DefaultFrameDuration) {
  Initialize();
  Play();

  // Verify that the ended callback fires when the default last frame duration
  // has elapsed.
  int end_timestamp = kFrameDurationInMs * limits::kMaxVideoFrames +
      VideoRendererBase::kMaxLastFrameDuration().InMilliseconds();
  EXPECT_LT(end_timestamp, kVideoDurationInMs);

  QueueEndOfStream();
  AdvanceTimeInMs(end_timestamp);
  WaitForEnded();

  Shutdown();
}

TEST_F(VideoRendererBaseTest, EndOfStream_ClipDuration) {
  int duration = kVideoDurationInMs + kFrameDurationInMs / 2;
  InitializeWithDuration(duration);
  Play();

  // Render all frames except for the last |limits::kMaxVideoFrames| frames
  // and deliver all the frames between the start and |duration|. The preroll
  // inside Initialize() makes this a little confusing, but |timestamp| is
  // the current render time and QueueNextFrame() delivers a frame with a
  // timestamp that is |timestamp| + limits::kMaxVideoFrames *
  // kFrameDurationInMs.
  int timestamp = kFrameDurationInMs;
  int end_timestamp = duration - limits::kMaxVideoFrames * kFrameDurationInMs;
  for (; timestamp < end_timestamp; timestamp += kFrameDurationInMs) {
    QueueNextFrame();
  }

  // Queue the end of stream frame and wait for the last frame to be rendered.
  QueueEndOfStream();
  AdvanceTimeInMs(duration);
  WaitForEnded();

  Shutdown();
}

TEST_F(VideoRendererBaseTest, DecodeError_Playing) {
  Initialize();
  Play();

  QueueDecodeError();
  AdvanceTimeInMs(kVideoDurationInMs);
  WaitForError(PIPELINE_ERROR_DECODE);
  Shutdown();
}

TEST_F(VideoRendererBaseTest, DecodeError_DuringPreroll) {
  Initialize();
  Pause();
  Flush();

  QueueDecodeError();
  Preroll(kFrameDurationInMs * 6, PIPELINE_ERROR_DECODE);
  Shutdown();
}

TEST_F(VideoRendererBaseTest, Preroll_Exact) {
  Initialize();
  Pause();
  Flush();
  QueuePrerollFrames(kFrameDurationInMs * 6);

  Preroll(kFrameDurationInMs * 6, PIPELINE_OK);
  EXPECT_EQ(kFrameDurationInMs * 6, GetCurrentTimestampInMs());
  Shutdown();
}

TEST_F(VideoRendererBaseTest, Preroll_RightBefore) {
  Initialize();
  Pause();
  Flush();
  QueuePrerollFrames(kFrameDurationInMs * 6);

  Preroll(kFrameDurationInMs * 6 - 1, PIPELINE_OK);
  EXPECT_EQ(kFrameDurationInMs * 5, GetCurrentTimestampInMs());
  Shutdown();
}

TEST_F(VideoRendererBaseTest, Preroll_RightAfter) {
  Initialize();
  Pause();
  Flush();
  QueuePrerollFrames(kFrameDurationInMs * 6);

  Preroll(kFrameDurationInMs * 6 + 1, PIPELINE_OK);
  EXPECT_EQ(kFrameDurationInMs * 6, GetCurrentTimestampInMs());
  Shutdown();
}

TEST_F(VideoRendererBaseTest, GetCurrentFrame_Initialized) {
  Initialize();
  EXPECT_TRUE(GetCurrentFrame());  // Due to prerolling.
  Shutdown();
}

TEST_F(VideoRendererBaseTest, GetCurrentFrame_Playing) {
  Initialize();
  Play();
  EXPECT_TRUE(GetCurrentFrame());
  Shutdown();
}

TEST_F(VideoRendererBaseTest, GetCurrentFrame_Paused) {
  Initialize();
  Play();
  Pause();
  EXPECT_TRUE(GetCurrentFrame());
  Shutdown();
}

TEST_F(VideoRendererBaseTest, GetCurrentFrame_Flushed) {
  Initialize();
  Play();
  Pause();
  Flush();
  EXPECT_FALSE(GetCurrentFrame());
  Shutdown();
}

#if defined(OS_MACOSX)
// http://crbug.com/109405
#define MAYBE_GetCurrentFrame_EndOfStream DISABLED_GetCurrentFrame_EndOfStream
#else
#define MAYBE_GetCurrentFrame_EndOfStream GetCurrentFrame_EndOfStream
#endif
TEST_F(VideoRendererBaseTest, MAYBE_GetCurrentFrame_EndOfStream) {
  Initialize();
  Play();
  Pause();
  Flush();

  // Preroll only end of stream frames.
  QueueEndOfStream();
  Preroll(0, PIPELINE_OK);
  EXPECT_FALSE(GetCurrentFrame());

  // Start playing, we should immediately get notified of end of stream.
  Play();
  WaitForEnded();

  Shutdown();
}

TEST_F(VideoRendererBaseTest, GetCurrentFrame_Shutdown) {
  Initialize();
  Shutdown();
  EXPECT_FALSE(GetCurrentFrame());
}

// Stop() is called immediately during an error.
TEST_F(VideoRendererBaseTest, GetCurrentFrame_Error) {
  Initialize();
  Stop();
  EXPECT_FALSE(GetCurrentFrame());
}

// Verify that shutdown can only proceed after we return the current frame.
TEST_F(VideoRendererBaseTest, Shutdown_DuringPaint) {
  Initialize();
  Play();

  // Grab the frame.
  scoped_refptr<VideoFrame> frame;
  renderer_->GetCurrentFrame(&frame);
  EXPECT_TRUE(frame);

  Pause();

  // Start flushing -- it won't complete until we return the frame.
  WaitableMessageLoopEvent event;
  renderer_->Flush(event.GetClosure());

  // Return the frame and wait.
  renderer_->PutCurrentFrame(frame);
  event.RunAndWait();

  Stop();
}

// Verify that a late decoder response doesn't break invariants in the renderer.
TEST_F(VideoRendererBaseTest, StopDuringOutstandingRead) {
  Initialize();
  Play();

  // Advance time a bit to trigger a Read().
  AdvanceTimeInMs(kFrameDurationInMs);
  WaitForPendingRead();

  WaitableMessageLoopEvent event;
  renderer_->Stop(event.GetClosure());

  QueueEndOfStream();
  SatisfyPendingRead();

  event.RunAndWait();
}

TEST_F(VideoRendererBaseTest, AbortPendingRead_Playing) {
  Initialize();
  Play();

  // Advance time a bit to trigger a Read().
  AdvanceTimeInMs(kFrameDurationInMs);
  WaitForPendingRead();

  QueueAbortedRead();
  SatisfyPendingRead();

  Pause();
  Flush();
  QueuePrerollFrames(kFrameDurationInMs * 6);
  Preroll(kFrameDurationInMs * 6, PIPELINE_OK);
  EXPECT_EQ(kFrameDurationInMs * 6, GetCurrentTimestampInMs());
  Shutdown();
}

TEST_F(VideoRendererBaseTest, AbortPendingRead_Flush) {
  Initialize();
  Play();

  // Advance time a bit to trigger a Read().
  AdvanceTimeInMs(kFrameDurationInMs);
  WaitForPendingRead();

  Pause();
  Flush();
  Shutdown();
}

TEST_F(VideoRendererBaseTest, AbortPendingRead_Preroll) {
  Initialize();
  Pause();
  Flush();

  QueueAbortedRead();
  Preroll(kFrameDurationInMs * 6, PIPELINE_OK);
  Shutdown();
}

TEST_F(VideoRendererBaseTest, VideoDecoder_InitFailure) {
  InSequence s;

  EXPECT_CALL(*decoder_, Initialize(_, _, _))
      .WillOnce(RunCallback<1>(PIPELINE_ERROR_DECODE));
  InitializeRenderer(PIPELINE_ERROR_DECODE);

  Stop();
}

}  // namespace media
