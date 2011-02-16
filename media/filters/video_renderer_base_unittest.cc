// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/stl_util-inl.h"
#include "media/base/callback.h"
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
  AutoCallbackRunner auto_runner(arg0);
}

// Mocked subclass of VideoRendererBase for testing purposes.
class MockVideoRendererBase : public VideoRendererBase {
 public:
  MockVideoRendererBase() {}
  virtual ~MockVideoRendererBase() {}

  // VideoRendererBase implementation.
  MOCK_METHOD1(OnInitialize, bool(VideoDecoder* decoder));
  MOCK_METHOD1(OnStop, void(FilterCallback* callback));
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
        seeking_(false) {
    renderer_->set_host(&host_);

    // Queue all reads from the decoder.
    EXPECT_CALL(*decoder_, ProduceVideoFrame(_))
        .WillRepeatedly(Invoke(this, &VideoRendererBaseTest::EnqueueCallback));

    // Sets the essential media format keys for this decoder.
    decoder_media_format_.SetAsString(MediaFormat::kMimeType,
                                      mime_type::kUncompressedVideo);
    decoder_media_format_.SetAsInteger(MediaFormat::kSurfaceType,
                                       VideoFrame::TYPE_SYSTEM_MEMORY);
    decoder_media_format_.SetAsInteger(MediaFormat::kSurfaceFormat,
                                       VideoFrame::YV12);
    decoder_media_format_.SetAsInteger(MediaFormat::kWidth, kWidth);
    decoder_media_format_.SetAsInteger(MediaFormat::kHeight, kHeight);
    EXPECT_CALL(*decoder_, media_format())
        .WillRepeatedly(ReturnRef(decoder_media_format_));
  }

  virtual ~VideoRendererBaseTest() {
    read_queue_.clear();

    // Expect a call into the subclass.
    EXPECT_CALL(*renderer_, OnStop(NotNull()))
        .WillOnce(DoAll(OnStop(), Return()))
        .RetiresOnSaturation();

    renderer_->Stop(NewExpectedCallback());
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

    InSequence s;

    // We expect the video size to be set.
    EXPECT_CALL(host_, SetVideoSize(kWidth, kHeight));

    // Then our subclass will be asked to initialize.
    EXPECT_CALL(*renderer_, OnInitialize(_))
        .WillOnce(Return(true));

    // Initialize, we shouldn't have any reads.
    renderer_->Initialize(decoder_,
                          NewExpectedCallback(), NewStatisticsCallback());
    EXPECT_EQ(0u, read_queue_.size());

    // Now seek to trigger prerolling.
    Seek(0);
  }

  void Seek(int64 timestamp) {
    EXPECT_CALL(*renderer_, OnFrameAvailable());
    EXPECT_FALSE(seeking_);

    // Now seek to trigger prerolling.
    seeking_ = true;
    renderer_->Seek(base::TimeDelta::FromMicroseconds(timestamp),
                    NewCallback(this, &VideoRendererBaseTest::OnSeekComplete));

    // Now satisfy the read requests.  The callback must be executed in order
    // to exit the loop since VideoRendererBase can read a few extra frames
    // after |timestamp| in order to preroll.
    for (int64 i = 0; seeking_; ++i) {
      CreateFrame(i * kDuration, kDuration);
    }
  }

  void Flush() {
    renderer_->Pause(NewExpectedCallback());

    EXPECT_CALL(*decoder_, ProvidesBuffer())
        .WillRepeatedly(Return(true));

    renderer_->Flush(NewExpectedCallback());
  }

  void CreateFrame(int64 timestamp, int64 duration) {
    const base::TimeDelta kZero;
    scoped_refptr<VideoFrame> frame;
    VideoFrame::CreateFrame(VideoFrame::RGB32, kWidth, kHeight,
                            base::TimeDelta::FromMicroseconds(timestamp),
                            base::TimeDelta::FromMicroseconds(duration),
                            &frame);
    decoder_->VideoFrameReady(frame);
  }

  void ExpectCurrentTimestamp(int64 timestamp) {
    scoped_refptr<VideoFrame> frame;
    renderer_->GetCurrentFrame(&frame);
    EXPECT_EQ(timestamp, frame->GetTimestamp().InMicroseconds());
    renderer_->PutCurrentFrame(frame);
  }

 protected:
  static const size_t kWidth;
  static const size_t kHeight;
  static const int64 kDuration;

  StatisticsCallback* NewStatisticsCallback() {
    return NewCallback(&stats_callback_object_,
                       &MockStatisticsCallback::OnStatistics);
  }

  // Fixture members.
  scoped_refptr<MockVideoRendererBase> renderer_;
  scoped_refptr<MockVideoDecoder> decoder_;
  StrictMock<MockFilterHost> host_;
  MediaFormat decoder_media_format_;
  MockStatisticsCallback stats_callback_object_;

  // Receives all the buffers that renderer had provided to |decoder_|.
  std::deque<scoped_refptr<VideoFrame> > read_queue_;

 private:
  void EnqueueCallback(scoped_refptr<VideoFrame> frame) {
    read_queue_.push_back(frame);
  }

  void OnSeekComplete() {
    EXPECT_TRUE(seeking_);
    seeking_ = false;
  }

  bool seeking_;

  DISALLOW_COPY_AND_ASSIGN(VideoRendererBaseTest);
};

const size_t VideoRendererBaseTest::kWidth = 16u;
const size_t VideoRendererBaseTest::kHeight = 16u;
const int64 VideoRendererBaseTest::kDuration = 10;

// Test initialization where the decoder's media format is malformed.
TEST_F(VideoRendererBaseTest, Initialize_BadMediaFormat) {
  InSequence s;

  // Don't set a media format.
  MediaFormat media_format;
  scoped_refptr<MockVideoDecoder> bad_decoder(new MockVideoDecoder());
  EXPECT_CALL(*bad_decoder, media_format())
      .WillRepeatedly(ReturnRef(media_format));

  // We expect to receive an error.
  EXPECT_CALL(host_, SetError(PIPELINE_ERROR_INITIALIZATION_FAILED));

  // Initialize, we expect to have no reads.
  renderer_->Initialize(bad_decoder,
                        NewExpectedCallback(), NewStatisticsCallback());
  EXPECT_EQ(0u, read_queue_.size());
}

// Test initialization where the subclass failed for some reason.
TEST_F(VideoRendererBaseTest, Initialize_Failed) {
  InSequence s;

  // We expect the video size to be set.
  EXPECT_CALL(host_, SetVideoSize(kWidth, kHeight));

  // Our subclass will fail when asked to initialize.
  EXPECT_CALL(*renderer_, OnInitialize(_))
      .WillOnce(Return(false));

  // We expect to receive an error.
  EXPECT_CALL(host_, SetError(PIPELINE_ERROR_INITIALIZATION_FAILED));

  // Initialize, we expect to have no reads.
  renderer_->Initialize(decoder_,
                        NewExpectedCallback(), NewStatisticsCallback());
  EXPECT_EQ(0u, read_queue_.size());
}

// Test successful initialization and preroll.
TEST_F(VideoRendererBaseTest, Initialize_Successful) {
  Initialize();
  ExpectCurrentTimestamp(0);
  Flush();
}

TEST_F(VideoRendererBaseTest, Play) {
  Initialize();
  renderer_->Play(NewExpectedCallback());
  Flush();
}

TEST_F(VideoRendererBaseTest, Seek_Exact) {
  Initialize();
  Flush();
  Seek(kDuration * 6);
  ExpectCurrentTimestamp(kDuration * 6);
  Flush();
}

TEST_F(VideoRendererBaseTest, Seek_RightBefore) {
  Initialize();
  Flush();
  Seek(kDuration * 6 - 1);
  ExpectCurrentTimestamp(kDuration * 5);
  Flush();
}

TEST_F(VideoRendererBaseTest, Seek_RightAfter) {
  Initialize();
  Flush();
  Seek(kDuration * 6 + 1);
  ExpectCurrentTimestamp(kDuration * 6);
  Flush();
}

}  // namespace media
