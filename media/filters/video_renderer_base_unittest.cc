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
#include "media/base/mock_filter_host.h"
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

ACTION(OnStop) {
  arg0.Run();
}

class VideoRendererBaseTest : public ::testing::Test {
 public:
  VideoRendererBaseTest()
      : decoder_(new MockVideoDecoder()),
        cv_(&lock_),
        event_(false, false),
        timeout_(base::TimeDelta::FromMilliseconds(
            TestTimeouts::action_timeout_ms())),
        seeking_(false) {
    renderer_ = new VideoRendererBase(
        base::Bind(&VideoRendererBaseTest::PaintCBWasCalled,
                   base::Unretained(this)),
        base::Bind(&VideoRendererBaseTest::SetOpaqueCBWasCalled,
                   base::Unretained(this)));
    renderer_->set_host(&host_);

    EXPECT_CALL(*decoder_, natural_size())
        .WillRepeatedly(ReturnRef(kNaturalSize));

    EXPECT_CALL(stats_callback_object_, OnStatistics(_))
        .Times(AnyNumber());
  }

  virtual ~VideoRendererBaseTest() {
    read_queue_.clear();

    if (renderer_) {
      Stop();
    }
  }

  MOCK_METHOD0(PaintCBWasCalled, void());

  MOCK_CONST_METHOD1(SetOpaqueCBWasCalled, void(bool));

  void Initialize() {
    // TODO(scherkus): really, really, really need to inject a thread into
    // VideoRendererBase... it makes mocking much harder.
    EXPECT_CALL(host_, GetTime())
        .WillRepeatedly(Invoke(this, &VideoRendererBaseTest::GetTime));

    // Expects the video renderer to get duration from the host.
    EXPECT_CALL(host_, GetDuration())
        .WillRepeatedly(Return(
            base::TimeDelta::FromMicroseconds(kVideoDuration)));

    // Monitor reads from the decoder.
    EXPECT_CALL(*decoder_, Read(_))
        .WillRepeatedly(Invoke(this, &VideoRendererBaseTest::FrameRequested));

    InSequence s;

    // We expect the video size to be set.
    EXPECT_CALL(host_, SetNaturalVideoSize(kNaturalSize));

    // Set playback rate before anything else happens.
    renderer_->SetPlaybackRate(1.0f);

    // Initialize, we shouldn't have any reads.
    renderer_->Initialize(decoder_,
                          NewExpectedClosure(), NewStatisticsCallback());

    // Now seek to trigger prerolling.
    Seek(0);
  }

  void StartSeeking(int64 timestamp) {
    EXPECT_FALSE(seeking_);

    // Seek to trigger prerolling.
    seeking_ = true;
    renderer_->Seek(base::TimeDelta::FromMicroseconds(timestamp),
                    base::Bind(&VideoRendererBaseTest::OnSeekComplete,
                               base::Unretained(this),
                               PIPELINE_OK));
  }

  void Play() {
    SCOPED_TRACE("Play()");
    renderer_->Play(NewWaitableClosure());
    WaitForClosure();
  }

  // Seek and preroll to the given timestamp.
  //
  // Use |kEndOfStream| to preroll end of stream frames.
  void Seek(int64 timestamp) {
    SCOPED_TRACE(base::StringPrintf("Seek(%" PRId64 ")", timestamp));
    StartSeeking(timestamp);
    FinishSeeking(timestamp);
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
      CHECK(!read_cb_.is_null()) << "Can't deliver a frame without a callback";
      std::swap(read_cb, read_cb_);
    }

    if (timestamp == kEndOfStream) {
      read_cb.Run(VideoFrame::CreateEmptyFrame());
    } else {
      read_cb.Run(CreateFrame(timestamp, kFrameDuration));
    }
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
    if (read_cb_.is_null()) {
      cv_.TimedWait(timeout_);
      CHECK(!read_cb_.is_null()) << "Timed out waiting for read to occur.";
    }
  }

  // Advances clock to |timestamp| (which should be the timestamp of the last
  // frame plus duration) and waits for the ended signal before returning.
  void RenderLastFrame(int64 timestamp) {
    EXPECT_CALL(host_, NotifyEnded())
        .WillOnce(Invoke(&event_, &base::WaitableEvent::Signal));
    {
      base::AutoLock l(lock_);
      time_ = base::TimeDelta::FromMicroseconds(timestamp);
    }
    CHECK(event_.TimedWait(timeout_)) << "Timed out waiting for ended signal.";
  }

  base::WaitableEvent* event() { return &event_; }
  const base::TimeDelta& timeout() { return timeout_; }

 protected:
  StatisticsCallback NewStatisticsCallback() {
    return base::Bind(&MockStatisticsCallback::OnStatistics,
                      base::Unretained(&stats_callback_object_));
  }

  // Fixture members.
  scoped_refptr<VideoRendererBase> renderer_;
  scoped_refptr<MockVideoDecoder> decoder_;
  StrictMock<MockFilterHost> host_;
  MockStatisticsCallback stats_callback_object_;

  // Receives all the buffers that renderer had provided to |decoder_|.
  std::deque<scoped_refptr<VideoFrame> > read_queue_;

 private:
  // Called by VideoRendererBase for accessing the current time.
  base::TimeDelta GetTime() {
    base::AutoLock l(lock_);
    return time_;
  }

  // Called by VideoRendererBase when it wants a frame.
  void FrameRequested(const VideoDecoder::ReadCB& callback) {
    base::AutoLock l(lock_);
    CHECK(read_cb_.is_null());
    read_cb_ = callback;
    cv_.Signal();
  }

  void OnSeekComplete(PipelineStatus expected_status, PipelineStatus status) {
    base::AutoLock l(lock_);
    EXPECT_EQ(status, expected_status);
    EXPECT_TRUE(seeking_);
    seeking_ = false;
    cv_.Signal();
  }

  void FinishSeeking(int64 timestamp) {
    EXPECT_CALL(*this, PaintCBWasCalled());
    EXPECT_TRUE(seeking_);

    // Satisfy the read requests.  The callback must be executed in order
    // to exit the loop since VideoRendererBase can read a few extra frames
    // after |timestamp| in order to preroll.
    base::AutoLock l(lock_);
    int i = 0;
    while (seeking_) {
      if (!read_cb_.is_null()) {
        VideoDecoder::ReadCB read_cb;
        std::swap(read_cb, read_cb_);

        // Unlock to deliver the frame to avoid re-entrancy issues.
        base::AutoUnlock ul(lock_);
        if (timestamp == kEndOfStream) {
          read_cb.Run(VideoFrame::CreateEmptyFrame());
        } else {
          read_cb.Run(CreateFrame(i * kFrameDuration, kFrameDuration));
          i++;
        }
      } else {
        // We want to wait iff we're still seeking but have no pending read.
        cv_.TimedWait(timeout_);
        CHECK(!seeking_ || !read_cb_.is_null())
            << "Timed out waiting for seek or read to occur.";
      }
    }

    EXPECT_TRUE(read_cb_.is_null());
  }

  base::Lock lock_;
  base::ConditionVariable cv_;
  base::WaitableEvent event_;
  base::TimeDelta timeout_;

  // Used in conjunction with |lock_| and |cv_| for satisfying reads.
  bool seeking_;
  VideoDecoder::ReadCB read_cb_;
  base::TimeDelta time_;

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
  //
  // Put the gmock expectation here to avoid racing with the rendering thread.
  EXPECT_CALL(*this, PaintCBWasCalled())
      .Times(limits::kMaxVideoFrames - 1);
  for (int i = 1; i < limits::kMaxVideoFrames; ++i) {
    RenderFrame(kFrameDuration * i);
  }

  // Finish rendering the last frame, we should NOT get a new frame but instead
  // get notified of end of stream.
  DeliverFrame(kEndOfStream);
  RenderLastFrame(kFrameDuration * limits::kMaxVideoFrames);

  Shutdown();
}

TEST_F(VideoRendererBaseTest, Seek_Exact) {
  Initialize();
  Pause();
  Flush();
  Seek(kFrameDuration * 6);
  ExpectCurrentTimestamp(kFrameDuration * 6);
  Shutdown();
}

TEST_F(VideoRendererBaseTest, Seek_RightBefore) {
  Initialize();
  Pause();
  Flush();
  Seek(kFrameDuration * 6 - 1);
  ExpectCurrentTimestamp(kFrameDuration * 5);
  Shutdown();
}

TEST_F(VideoRendererBaseTest, Seek_RightAfter) {
  Initialize();
  Pause();
  Flush();
  Seek(kFrameDuration * 6 + 1);
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
#define MAYBE_GetCurrentFrame_EndOfStream FLAKY_GetCurrentFrame_EndOfStream
#else
#define MAYBE_GetCurrentFrame_EndOfStream GetCurrentFrame_EndOfStream
#endif
TEST_F(VideoRendererBaseTest, MAYBE_GetCurrentFrame_EndOfStream) {
  Initialize();
  Play();
  Pause();
  Flush();

  // Seek and preroll only end of stream frames.
  Seek(kEndOfStream);
  ExpectCurrentFrame(false);

  // Start playing, we should immediately get notified of end of stream.
  EXPECT_CALL(host_, NotifyEnded())
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

}  // namespace media
