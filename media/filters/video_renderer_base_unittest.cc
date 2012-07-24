// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/callback.h"
#include "base/format_macros.h"
#include "base/stl_util.h"
#include "base/stringprintf.h"
#include "base/synchronization/condition_variable.h"
#include "base/synchronization/lock.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/test_timeouts.h"
#include "media/base/data_buffer.h"
#include "media/base/limits.h"
#include "media/base/mock_callback.h"
#include "media/base/mock_filters.h"
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

static const int64 kFrameDuration = 10;
static const int64 kVideoDuration = kFrameDuration * 100;
static const int64 kEndOfStream = kint64min;
static const gfx::Size kNaturalSize(16u, 16u);

class VideoRendererBaseTest : public ::testing::Test {
 public:
  VideoRendererBaseTest()
      : decoder_(new MockVideoDecoder()),
        cv_(&lock_),
        event_(false, false),
        timeout_(TestTimeouts::action_timeout()),
        prerolling_(false),
        paint_cv_(&lock_),
        paint_was_called_(false),
        should_queue_read_cb_(false) {
    renderer_ = new VideoRendererBase(
        base::Bind(&VideoRendererBaseTest::Paint, base::Unretained(this)),
        base::Bind(&VideoRendererBaseTest::OnSetOpaque, base::Unretained(this)),
        true);

    // We expect these to be called but we don't care how/when.
    EXPECT_CALL(*decoder_, natural_size())
        .WillRepeatedly(ReturnRef(kNaturalSize));
    EXPECT_CALL(*decoder_, Stop(_))
        .WillRepeatedly(RunClosure());
    EXPECT_CALL(statistics_cb_object_, OnStatistics(_))
        .Times(AnyNumber());
    EXPECT_CALL(*this, OnTimeUpdate(_))
        .Times(AnyNumber());
    EXPECT_CALL(*this, OnSetOpaque(_))
        .Times(AnyNumber());
  }

  virtual ~VideoRendererBaseTest() {
    read_queue_.clear();

    if (renderer_) {
      Stop();
    }
  }

  // Callbacks passed into VideoRendererBase().
  MOCK_CONST_METHOD1(OnSetOpaque, void(bool));

  // Callbacks passed into Initialize().
  MOCK_METHOD1(OnTimeUpdate, void(base::TimeDelta));
  MOCK_METHOD1(OnNaturalSizeChanged, void(const gfx::Size&));
  MOCK_METHOD0(OnEnded, void());
  MOCK_METHOD1(OnError, void(PipelineStatus));

  void Initialize() {
    // TODO(scherkus): really, really, really need to inject a thread into
    // VideoRendererBase... it makes mocking much harder.

    // Monitor reads from the decoder.
    EXPECT_CALL(*decoder_, Read(_))
        .WillRepeatedly(Invoke(this, &VideoRendererBaseTest::FrameRequested));

    EXPECT_CALL(*decoder_, Reset(_))
        .WillRepeatedly(Invoke(this, &VideoRendererBaseTest::FlushRequested));

    InSequence s;

    // We expect the video size to be set.
    EXPECT_CALL(*this, OnNaturalSizeChanged(kNaturalSize));

    // Set playback rate before anything else happens.
    renderer_->SetPlaybackRate(1.0f);

    // Initialize, we shouldn't have any reads.
    renderer_->Initialize(
        decoder_,
        NewExpectedStatusCB(PIPELINE_OK),
        base::Bind(&MockStatisticsCB::OnStatistics,
                   base::Unretained(&statistics_cb_object_)),
        base::Bind(&VideoRendererBaseTest::OnTimeUpdate,
                   base::Unretained(this)),
        base::Bind(&VideoRendererBaseTest::OnNaturalSizeChanged,
                   base::Unretained(this)),
        base::Bind(&VideoRendererBaseTest::OnEnded, base::Unretained(this)),
        base::Bind(&VideoRendererBaseTest::OnError, base::Unretained(this)),
        base::Bind(&VideoRendererBaseTest::GetTime, base::Unretained(this)),
        base::Bind(&VideoRendererBaseTest::GetDuration,
                   base::Unretained(this)));

    // Start prerolling.
    Preroll(0);
  }

  // Instead of immediately satisfying a decoder Read request, queue it up.
  void QueueReadCB() {
    should_queue_read_cb_ = true;
  }

  void SatisfyQueuedReadCB() {
    base::AutoLock l(lock_);
    CHECK(should_queue_read_cb_ && !queued_read_cb_.is_null());
    should_queue_read_cb_ = false;
    VideoDecoder::ReadCB read_cb(queued_read_cb_);
    queued_read_cb_.Reset();
    base::AutoUnlock u(lock_);
    read_cb.Run(VideoDecoder::kOk, VideoFrame::CreateEmptyFrame());
  }

  void StartPrerolling(int64 timestamp, PipelineStatus expected_status) {
    EXPECT_FALSE(prerolling_);

    prerolling_ = true;
    renderer_->Preroll(base::TimeDelta::FromMicroseconds(timestamp),
                       base::Bind(&VideoRendererBaseTest::OnPrerollComplete,
                                  base::Unretained(this), expected_status));
  }

  void Play() {
    SCOPED_TRACE("Play()");
    renderer_->Play(NewWaitableClosure());
    WaitForClosure();
  }

  // Preroll to the given timestamp.
  //
  // Use |kEndOfStream| to preroll end of stream frames.
  void Preroll(int64 timestamp) {
    SCOPED_TRACE(base::StringPrintf("Preroll(%" PRId64 ")", timestamp));
    StartPrerolling(timestamp, PIPELINE_OK);
    FinishPrerolling(timestamp);
  }

  void Pause() {
    SCOPED_TRACE("Pause()");
    renderer_->Pause(NewWaitableClosure());
    WaitForClosure();
  }

  void Flush() {
    SCOPED_TRACE("Flush()");
    renderer_->Flush(NewWaitableClosure());
    WaitForClosure();
  }

  void Stop() {
    SCOPED_TRACE("Stop()");
    renderer_->Stop(NewWaitableClosure());
    WaitForClosure();
  }

  void Shutdown() {
    Pause();
    Flush();
    Stop();
  }

  // Delivers a frame with the given timestamp to the video renderer.
  //
  // Use |kEndOfStream| to pass in an end of stream frame.
  void DeliverFrame(int64 timestamp) {
    // Lock+swap to avoid re-entrancy issues.
    VideoDecoder::ReadCB read_cb;
    {
      base::AutoLock l(lock_);
      std::swap(read_cb, read_cb_);
    }

    if (timestamp == kEndOfStream) {
      read_cb.Run(VideoDecoder::kOk, VideoFrame::CreateEmptyFrame());
    } else {
      read_cb.Run(VideoDecoder::kOk, CreateFrame(timestamp, kFrameDuration));
    }
  }

  void DecoderError() {
    // Lock+swap to avoid re-entrancy issues.
    VideoDecoder::ReadCB read_cb;
    {
      base::AutoLock l(lock_);
      std::swap(read_cb, read_cb_);
    }

    read_cb.Run(VideoDecoder::kDecodeError, NULL);
  }

  void AbortRead() {
    // Lock+swap to avoid re-entrancy issues.
    VideoDecoder::ReadCB read_cb;
    {
      base::AutoLock l(lock_);
      std::swap(read_cb, read_cb_);
    }

    read_cb.Run(VideoDecoder::kOk, NULL);
  }

  void ExpectCurrentFrame(bool present) {
    scoped_refptr<VideoFrame> frame;
    renderer_->GetCurrentFrame(&frame);
    if (present) {
      EXPECT_TRUE(frame);
    } else {
      EXPECT_FALSE(frame);
    }
    renderer_->PutCurrentFrame(frame);
  }

  void ExpectCurrentTimestamp(int64 timestamp) {
    scoped_refptr<VideoFrame> frame;
    renderer_->GetCurrentFrame(&frame);
    EXPECT_EQ(timestamp, frame->GetTimestamp().InMicroseconds());
    renderer_->PutCurrentFrame(frame);
  }

  base::Closure NewWaitableClosure() {
    return base::Bind(&base::WaitableEvent::Signal, base::Unretained(&event_));
  }

  void WaitForClosure() {
    ASSERT_TRUE(event_.TimedWait(timeout_));
    event_.Reset();
  }

  // Creates a frame with given timestamp and duration.
  scoped_refptr<VideoFrame> CreateFrame(int64 timestamp, int64 duration) {
    scoped_refptr<VideoFrame> frame =
        VideoFrame::CreateFrame(VideoFrame::RGB32, kNaturalSize.width(),
                                kNaturalSize.height(),
                                base::TimeDelta::FromMicroseconds(timestamp),
                                base::TimeDelta::FromMicroseconds(duration));
    return frame;
  }

  // Advances clock to |timestamp| and waits for the frame at |timestamp| to get
  // rendered using |read_cb_| as the signal that the frame has rendered.
  void RenderFrame(int64 timestamp) {
    base::AutoLock l(lock_);
    time_ = base::TimeDelta::FromMicroseconds(timestamp);
    paint_was_called_ = false;
    if (read_cb_.is_null()) {
      cv_.TimedWait(timeout_);
      CHECK(!read_cb_.is_null()) << "Timed out waiting for read to occur.";
    }
    WaitForPaint_Locked();
  }

  // Advances clock to |timestamp| (which should be the timestamp of the last
  // frame plus duration) and waits for the ended signal before returning.
  void RenderLastFrame(int64 timestamp) {
    EXPECT_CALL(*this, OnEnded())
        .WillOnce(Invoke(&event_, &base::WaitableEvent::Signal));
    {
      base::AutoLock l(lock_);
      time_ = base::TimeDelta::FromMicroseconds(timestamp);
    }
    CHECK(event_.TimedWait(timeout_)) << "Timed out waiting for ended signal.";
  }

  base::WaitableEvent* event() { return &event_; }
  const base::TimeDelta& timeout() { return timeout_; }

  void VerifyNotPrerolling() {
    base::AutoLock l(lock_);
    ASSERT_FALSE(prerolling_);
  }

 protected:
  // Fixture members.
  scoped_refptr<VideoRendererBase> renderer_;
  scoped_refptr<MockVideoDecoder> decoder_;
  MockStatisticsCB statistics_cb_object_;

  // Receives all the buffers that renderer had provided to |decoder_|.
  std::deque<scoped_refptr<VideoFrame> > read_queue_;

 private:
  // Called by VideoRendererBase for accessing the current time.
  base::TimeDelta GetTime() {
    base::AutoLock l(lock_);
    return time_;
  }

  base::TimeDelta GetDuration() {
    return base::TimeDelta::FromMicroseconds(kVideoDuration);
  }

  // Called by VideoRendererBase when it wants a frame.
  void FrameRequested(const VideoDecoder::ReadCB& callback) {
    base::AutoLock l(lock_);
    if (should_queue_read_cb_) {
      CHECK(queued_read_cb_.is_null());
      queued_read_cb_ = callback;
      return;
    }
    CHECK(read_cb_.is_null());
    read_cb_ = callback;
    cv_.Signal();
  }

  void FlushRequested(const base::Closure& callback) {
    // Lock+swap to avoid re-entrancy issues.
    VideoDecoder::ReadCB read_cb;
    {
      base::AutoLock l(lock_);
      std::swap(read_cb, read_cb_);
    }

    // Abort pending read.
    if (!read_cb.is_null())
      read_cb.Run(VideoDecoder::kOk, NULL);

    callback.Run();
  }

  void OnPrerollComplete(PipelineStatus expected_status,
                         PipelineStatus status) {
    base::AutoLock l(lock_);
    EXPECT_EQ(status, expected_status);
    EXPECT_TRUE(prerolling_);
    prerolling_ = false;
    cv_.Signal();
  }

  void FinishPrerolling(int64 timestamp) {
    // Satisfy the read requests.  The callback must be executed in order
    // to exit the loop since VideoRendererBase can read a few extra frames
    // after |timestamp| in order to preroll.
    base::AutoLock l(lock_);
    EXPECT_TRUE(prerolling_);
    paint_was_called_ = false;
    int i = 0;
    while (prerolling_) {
      if (!read_cb_.is_null()) {
        VideoDecoder::ReadCB read_cb;
        std::swap(read_cb, read_cb_);

        // Unlock to deliver the frame to avoid re-entrancy issues.
        base::AutoUnlock ul(lock_);
        if (timestamp == kEndOfStream) {
          read_cb.Run(VideoDecoder::kOk, VideoFrame::CreateEmptyFrame());
        } else {
          read_cb.Run(VideoDecoder::kOk,
                      CreateFrame(i * kFrameDuration, kFrameDuration));
          i++;
        }
      } else {
        // We want to wait iff we're still prerolling but have no pending read.
        cv_.TimedWait(timeout_);
        CHECK(!prerolling_ || !read_cb_.is_null())
            << "Timed out waiting for preroll or read to occur.";
      }
    }

    EXPECT_TRUE(read_cb_.is_null());
    WaitForPaint_Locked();
  }

  void Paint() {
    base::AutoLock l(lock_);
    paint_was_called_ = true;
    paint_cv_.Signal();
  }

  void WaitForPaint_Locked() {
    lock_.AssertAcquired();
    if (paint_was_called_)
      return;
    paint_cv_.TimedWait(timeout_);
    EXPECT_TRUE(paint_was_called_);
  }

  base::Lock lock_;
  base::ConditionVariable cv_;
  base::WaitableEvent event_;
  base::TimeDelta timeout_;

  // Used in conjunction with |lock_| and |cv_| for satisfying reads.
  bool prerolling_;
  VideoDecoder::ReadCB read_cb_;
  base::TimeDelta time_;

  // Used in conjunction with |lock_| to wait for Paint() calls.
  base::ConditionVariable paint_cv_;
  bool paint_was_called_;

  // Holding queue for Read callbacks for exercising delayed demux/decode.
  bool should_queue_read_cb_;
  VideoDecoder::ReadCB queued_read_cb_;

  DISALLOW_COPY_AND_ASSIGN(VideoRendererBaseTest);
};

TEST_F(VideoRendererBaseTest, Initialize) {
  Initialize();
  ExpectCurrentTimestamp(0);
  Shutdown();
}

TEST_F(VideoRendererBaseTest, Play) {
  Initialize();
  Play();
  Shutdown();
}

TEST_F(VideoRendererBaseTest, EndOfStream) {
  Initialize();
  Play();

  // Finish rendering up to the next-to-last frame.
  for (int i = 1; i < limits::kMaxVideoFrames; ++i)
    RenderFrame(kFrameDuration * i);

  // Finish rendering the last frame, we should NOT get a new frame but instead
  // get notified of end of stream.
  DeliverFrame(kEndOfStream);
  RenderLastFrame(kFrameDuration * limits::kMaxVideoFrames);

  Shutdown();
}

TEST_F(VideoRendererBaseTest, DecoderError) {
  Initialize();
  Play();
  RenderFrame(kFrameDuration);
  EXPECT_CALL(*this, OnError(PIPELINE_ERROR_DECODE));
  DecoderError();
  Shutdown();
}

TEST_F(VideoRendererBaseTest, DecoderErrorDuringPreroll) {
  Initialize();
  Pause();
  Flush();
  StartPrerolling(kFrameDuration * 6, PIPELINE_ERROR_DECODE);
  DecoderError();
  Shutdown();
}

TEST_F(VideoRendererBaseTest, Preroll_Exact) {
  Initialize();
  Pause();
  Flush();
  Preroll(kFrameDuration * 6);
  ExpectCurrentTimestamp(kFrameDuration * 6);
  Shutdown();
}

TEST_F(VideoRendererBaseTest, Preroll_RightBefore) {
  Initialize();
  Pause();
  Flush();
  Preroll(kFrameDuration * 6 - 1);
  ExpectCurrentTimestamp(kFrameDuration * 5);
  Shutdown();
}

TEST_F(VideoRendererBaseTest, Preroll_RightAfter) {
  Initialize();
  Pause();
  Flush();
  Preroll(kFrameDuration * 6 + 1);
  ExpectCurrentTimestamp(kFrameDuration * 6);
  Shutdown();
}

TEST_F(VideoRendererBaseTest, GetCurrentFrame_Initialized) {
  Initialize();
  ExpectCurrentFrame(true);  // Due to prerolling.
  Shutdown();
}

TEST_F(VideoRendererBaseTest, GetCurrentFrame_Playing) {
  Initialize();
  Play();
  ExpectCurrentFrame(true);
  Shutdown();
}

TEST_F(VideoRendererBaseTest, GetCurrentFrame_Paused) {
  Initialize();
  Play();
  Pause();
  ExpectCurrentFrame(true);
  Shutdown();
}

TEST_F(VideoRendererBaseTest, GetCurrentFrame_Flushed) {
  Initialize();
  Play();
  Pause();
  Flush();
  ExpectCurrentFrame(false);
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
  Preroll(kEndOfStream);
  ExpectCurrentFrame(false);

  // Start playing, we should immediately get notified of end of stream.
  EXPECT_CALL(*this, OnEnded())
      .WillOnce(Invoke(event(), &base::WaitableEvent::Signal));
  Play();
  CHECK(event()->TimedWait(timeout())) << "Timed out waiting for ended signal.";

  Shutdown();
}

TEST_F(VideoRendererBaseTest, GetCurrentFrame_Shutdown) {
  Initialize();
  Shutdown();
  ExpectCurrentFrame(false);
}

// Stop() is called immediately during an error.
TEST_F(VideoRendererBaseTest, GetCurrentFrame_Error) {
  Initialize();
  Stop();
  ExpectCurrentFrame(false);
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
  renderer_->Flush(NewWaitableClosure());

  // Return the frame and wait.
  renderer_->PutCurrentFrame(frame);
  WaitForClosure();

  Stop();
}

// Verify that a late decoder response doesn't break invariants in the renderer.
TEST_F(VideoRendererBaseTest, StopDuringOutstandingRead) {
  Initialize();
  Pause();
  Flush();
  QueueReadCB();
  StartPrerolling(kFrameDuration * 6, PIPELINE_OK);  // Force-decode some more.
  renderer_->Stop(NewWaitableClosure());
  SatisfyQueuedReadCB();
  WaitForClosure();  // Finish the Stop().
}

TEST_F(VideoRendererBaseTest, AbortPendingRead_Playing) {
  Initialize();
  Play();

  // Render a frame to trigger a Read().
  RenderFrame(kFrameDuration);

  AbortRead();

  Pause();
  Flush();
  Preroll(kFrameDuration * 6);
  ExpectCurrentTimestamp(kFrameDuration * 6);
  Shutdown();
}

TEST_F(VideoRendererBaseTest, AbortPendingRead_Flush) {
  Initialize();
  Play();

  // Render a frame to trigger a Read().
  RenderFrame(kFrameDuration);

  Pause();
  Flush();
  Shutdown();
}

TEST_F(VideoRendererBaseTest, AbortPendingRead_Preroll) {
  Initialize();
  Pause();
  Flush();
  StartPrerolling(kFrameDuration * 6, PIPELINE_OK);
  AbortRead();
  VerifyNotPrerolling();
  Shutdown();
}

}  // namespace media
