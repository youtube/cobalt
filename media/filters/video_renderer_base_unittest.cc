// Copyright (c) 2011 The Chromium Authors. All rights reserved.
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

ACTION(OnStop) {
  arg0.Run();
}

// Mocked subclass of VideoRendererBase for testing purposes.
class MockVideoRendererBase : public VideoRendererBase {
 public:
  MockVideoRendererBase() {}
  virtual ~MockVideoRendererBase() {}

  // VideoRendererBase implementation.
  MOCK_METHOD1(OnInitialize, bool(VideoDecoder* decoder));
  MOCK_METHOD1(OnStop, void(const base::Closure& callback));
  MOCK_METHOD0(OnFrameAvailable, void());

  // Used for verifying check points during tests.
  MOCK_METHOD1(CheckPoint, void(int id));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockVideoRendererBase);
};

class VideoRendererBaseTest : public ::testing::Test {
 public:
  VideoRendererBaseTest()
      : renderer_(new MockVideoRendererBase()),
        decoder_(new MockVideoDecoder()),
        cv_(&lock_),
        event_(false, false),
        seeking_(false) {
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

  void Initialize() {
    // Who knows how many times ThreadMain() will execute!
    //
    // TODO(scherkus): really, really, really need to inject a thread into
    // VideoRendererBase... it makes mocking much harder.
    EXPECT_CALL(host_, GetTime()).WillRepeatedly(Return(base::TimeDelta()));

    // Expects the video renderer to get duration from the host.
    EXPECT_CALL(host_, GetDuration())
        .WillRepeatedly(Return(base::TimeDelta()));

    // Monitor reads from the decoder.
    EXPECT_CALL(*decoder_, Read(_))
        .WillRepeatedly(Invoke(this, &VideoRendererBaseTest::FrameRequested));

    InSequence s;

    // We expect the video size to be set.
    EXPECT_CALL(host_, SetNaturalVideoSize(kNaturalSize));

    // Then our subclass will be asked to initialize.
    EXPECT_CALL(*renderer_, OnInitialize(_))
        .WillOnce(Return(true));

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

  void Seek(int64 timestamp) {
    SCOPED_TRACE(base::StringPrintf("Seek(%" PRId64 ")", timestamp));
    StartSeeking(timestamp);
    FinishSeeking();
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

    // Expect a call into the subclass.
    EXPECT_CALL(*renderer_, OnStop(_))
        .WillOnce(DoAll(OnStop(), Return()))
        .RetiresOnSaturation();

    renderer_->Stop(NewWaitableClosure());
    WaitForClosure();
  }

  void Shutdown() {
    Pause();
    Flush();
    Stop();
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
    base::TimeDelta timeout =
        base::TimeDelta::FromMilliseconds(TestTimeouts::action_timeout_ms());
    ASSERT_TRUE(event_.TimedWait(timeout));
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

 protected:
  static const gfx::Size kNaturalSize;
  static const int64 kDuration;

  StatisticsCallback NewStatisticsCallback() {
    return base::Bind(&MockStatisticsCallback::OnStatistics,
                      base::Unretained(&stats_callback_object_));
  }

  // Fixture members.
  scoped_refptr<MockVideoRendererBase> renderer_;
  scoped_refptr<MockVideoDecoder> decoder_;
  StrictMock<MockFilterHost> host_;
  MockStatisticsCallback stats_callback_object_;

  // Receives all the buffers that renderer had provided to |decoder_|.
  std::deque<scoped_refptr<VideoFrame> > read_queue_;

 private:
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

  void FinishSeeking() {
    EXPECT_CALL(*renderer_, OnFrameAvailable());
    EXPECT_TRUE(seeking_);

    base::TimeDelta timeout =
        base::TimeDelta::FromMilliseconds(TestTimeouts::action_timeout_ms());

    // Satisfy the read requests.  The callback must be executed in order
    // to exit the loop since VideoRendererBase can read a few extra frames
    // after |timestamp| in order to preroll.
    int64 i = 0;
    base::AutoLock l(lock_);
    while (seeking_) {
      if (!read_cb_.is_null()) {
        VideoDecoder::ReadCB read_cb;
        std::swap(read_cb, read_cb_);

        // Unlock to deliver the frame to avoid re-entrancy issues.
        base::AutoUnlock ul(lock_);
        read_cb.Run(CreateFrame(i * kDuration, kDuration));
        ++i;
      } else {
        // We want to wait iff we're still seeking but have no pending read.
        cv_.TimedWait(timeout);
        CHECK(!seeking_ || !read_cb_.is_null())
            << "Timed out waiting for seek or read to occur.";
      }
    }

    EXPECT_TRUE(read_cb_.is_null());
  }

  base::Lock lock_;
  base::ConditionVariable cv_;
  base::WaitableEvent event_;

  // Used in conjunction with |lock_| and |cv_| for satisfying reads.
  bool seeking_;
  VideoDecoder::ReadCB read_cb_;

  DISALLOW_COPY_AND_ASSIGN(VideoRendererBaseTest);
};

const gfx::Size VideoRendererBaseTest::kNaturalSize(16u, 16u);
const int64 VideoRendererBaseTest::kDuration = 10;

// Test initialization where the subclass failed for some reason.
TEST_F(VideoRendererBaseTest, Initialize_Failed) {
  InSequence s;

  // We expect the video size to be set.
  EXPECT_CALL(host_, SetNaturalVideoSize(kNaturalSize));

  // Our subclass will fail when asked to initialize.
  EXPECT_CALL(*renderer_, OnInitialize(_))
      .WillOnce(Return(false));

  // We expect to receive an error.
  EXPECT_CALL(host_, SetError(PIPELINE_ERROR_INITIALIZATION_FAILED));

  // Initialize, we expect to have no reads.
  renderer_->Initialize(decoder_,
                        NewExpectedClosure(), NewStatisticsCallback());
  EXPECT_EQ(0u, read_queue_.size());
}

// Test successful initialization and preroll.
TEST_F(VideoRendererBaseTest, Initialize_Successful) {
  Initialize();
  ExpectCurrentTimestamp(0);
  Shutdown();
}

TEST_F(VideoRendererBaseTest, Play) {
  Initialize();
  Play();
  Shutdown();
}

TEST_F(VideoRendererBaseTest, Seek_Exact) {
  Initialize();
  Pause();
  Flush();
  Seek(kDuration * 6);
  ExpectCurrentTimestamp(kDuration * 6);
  Shutdown();
}

TEST_F(VideoRendererBaseTest, Seek_RightBefore) {
  Initialize();
  Pause();
  Flush();
  Seek(kDuration * 6 - 1);
  ExpectCurrentTimestamp(kDuration * 5);
  Shutdown();
}

TEST_F(VideoRendererBaseTest, Seek_RightAfter) {
  Initialize();
  Pause();
  Flush();
  Seek(kDuration * 6 + 1);
  ExpectCurrentTimestamp(kDuration * 6);
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
