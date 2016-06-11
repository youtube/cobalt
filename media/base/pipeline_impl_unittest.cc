// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/bind.h"
#include "base/message_loop.h"
#include "base/stl_util.h"
#include "base/threading/simple_thread.h"
#include "media/base/clock.h"
#include "media/base/gmock_callback_support.h"
#include "media/base/media_log.h"
#include "media/base/mock_filters.h"
#include "media/base/pipeline_impl.h"
#include "media/base/test_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/size.h"

using ::testing::_;
using ::testing::DeleteArg;
using ::testing::DoAll;
// TODO(scherkus): Remove InSequence after refactoring PipelineImpl.
using ::testing::InSequence;
using ::testing::Invoke;
using ::testing::InvokeWithoutArgs;
using ::testing::Mock;
using ::testing::NotNull;
using ::testing::Return;
using ::testing::ReturnRef;
using ::testing::SaveArg;
using ::testing::StrictMock;
using ::testing::WithArg;

namespace media {

// Demuxer properties.
static const int kTotalBytes = 1024;

ACTION_P(SetDemuxerProperties, duration) {
  arg0->SetTotalBytes(kTotalBytes);
  arg0->SetDuration(duration);
}

ACTION_P2(Stop, pipeline, stop_cb) {
  pipeline->Stop(stop_cb);
}

ACTION_P2(SetError, pipeline, status) {
  pipeline->SetErrorForTesting(status);
}

// Used for setting expectations on pipeline callbacks.  Using a StrictMock
// also lets us test for missing callbacks.
class CallbackHelper {
 public:
  CallbackHelper() {}
  virtual ~CallbackHelper() {}

  MOCK_METHOD1(OnStart, void(PipelineStatus));
  MOCK_METHOD1(OnSeek, void(PipelineStatus));
  MOCK_METHOD0(OnStop, void());
  MOCK_METHOD1(OnEnded, void(PipelineStatus));
  MOCK_METHOD1(OnError, void(PipelineStatus));
  MOCK_METHOD1(OnBufferingState, void(PipelineImpl::BufferingState));

 private:
  DISALLOW_COPY_AND_ASSIGN(CallbackHelper);
};

// TODO(scherkus): even though some filters are initialized on separate
// threads these test aren't flaky... why?  It's because filters' Initialize()
// is executed on |message_loop_| and the mock filters instantly call
// InitializationComplete(), which keeps the pipeline humming along.  If
// either filters don't call InitializationComplete() immediately or filter
// initialization is moved to a separate thread this test will become flaky.
class PipelineTest : public ::testing::Test {
 public:
  PipelineTest()
      : pipeline_(new PipelineImpl(message_loop_.message_loop_proxy(),
                                   new MediaLog())) {
    mocks_.reset(new MockFilterCollection());

    // InitializeDemuxer() adds overriding expectations for expected non-NULL
    // streams.
    DemuxerStream* null_pointer = NULL;
    EXPECT_CALL(*mocks_->demuxer(), GetStream(_))
        .WillRepeatedly(Return(null_pointer));

    EXPECT_CALL(*mocks_->demuxer(), GetStartTime())
        .WillRepeatedly(Return(base::TimeDelta()));
  }

  virtual ~PipelineTest() {
    // Shutdown sequence.
    if (pipeline_->IsRunning()) {
      EXPECT_CALL(*mocks_->demuxer(), Stop(_))
          .WillOnce(RunClosure<0>());

      if (audio_stream_)
        EXPECT_CALL(*mocks_->audio_renderer(), Stop(_))
            .WillOnce(RunClosure<0>());

      if (video_stream_)
        EXPECT_CALL(*mocks_->video_renderer(), Stop(_))
            .WillOnce(RunClosure<0>());
    }

    // Expect a stop callback if we were started.
    EXPECT_CALL(callbacks_, OnStop());
    pipeline_->Stop(base::Bind(&CallbackHelper::OnStop,
                               base::Unretained(&callbacks_)));
    message_loop_.RunUntilIdle();

    pipeline_ = NULL;
    mocks_.reset();
  }

 protected:
  // Sets up expectations to allow the demuxer to initialize.
  typedef std::vector<MockDemuxerStream*> MockDemuxerStreamVector;
  void InitializeDemuxer(MockDemuxerStreamVector* streams,
                         const base::TimeDelta& duration) {
    EXPECT_CALL(*mocks_->demuxer(), Initialize(_, _))
        .WillOnce(DoAll(SetDemuxerProperties(duration),
                        RunCallback<1>(PIPELINE_OK)));

    // Configure the demuxer to return the streams.
    for (size_t i = 0; i < streams->size(); ++i) {
      scoped_refptr<DemuxerStream> stream((*streams)[i]);
      EXPECT_CALL(*mocks_->demuxer(), GetStream(stream->type()))
          .WillRepeatedly(Return(stream));
    }
  }

  void InitializeDemuxer(MockDemuxerStreamVector* streams) {
    // Initialize with a default non-zero duration.
    InitializeDemuxer(streams, base::TimeDelta::FromSeconds(10));
  }

  StrictMock<MockDemuxerStream>* CreateStream(DemuxerStream::Type type) {
    StrictMock<MockDemuxerStream>* stream =
        new StrictMock<MockDemuxerStream>();
    EXPECT_CALL(*stream, type())
        .WillRepeatedly(Return(type));
    return stream;
  }

  // Sets up expectations to allow the video renderer to initialize.
  void InitializeVideoRenderer(const scoped_refptr<DemuxerStream>& stream) {
    EXPECT_CALL(*mocks_->video_renderer(),
                Initialize(stream, _, _, _, _, _, _, _, _, _))
        .WillOnce(RunCallback<2>(PIPELINE_OK));
    EXPECT_CALL(*mocks_->video_renderer(), SetPlaybackRate(0.0f));

    // Startup sequence.
    EXPECT_CALL(*mocks_->video_renderer(),
                Preroll(mocks_->demuxer()->GetStartTime(), _))
        .WillOnce(RunCallback<1>(PIPELINE_OK));
    EXPECT_CALL(*mocks_->video_renderer(), Play(_))
        .WillOnce(RunClosure<0>());
  }

  // Sets up expectations to allow the audio renderer to initialize.
  void InitializeAudioRenderer(const scoped_refptr<DemuxerStream>& stream,
                               bool disable_after_init_cb) {
    if (disable_after_init_cb) {
      EXPECT_CALL(*mocks_->audio_renderer(),
                  Initialize(stream, _, _, _, _, _, _, _, _))
          .WillOnce(DoAll(RunCallback<2>(PIPELINE_OK),
                          WithArg<7>(RunClosure<0>())));  // |disabled_cb|.
    } else {
      EXPECT_CALL(*mocks_->audio_renderer(),
                  Initialize(stream, _, _, _, _, _, _, _, _))
          .WillOnce(DoAll(SaveArg<5>(&audio_time_cb_),
                          RunCallback<2>(PIPELINE_OK)));
    }
  }

  // Sets up expectations on the callback and initializes the pipeline.  Called
  // after tests have set expectations any filters they wish to use.
  void InitializePipeline(PipelineStatus start_status) {
    EXPECT_CALL(callbacks_, OnStart(start_status));

    if (start_status == PIPELINE_OK) {
      EXPECT_CALL(callbacks_, OnBufferingState(PipelineImpl::kHaveMetadata));
      EXPECT_CALL(*mocks_->demuxer(), SetPlaybackRate(0.0f));

      if (audio_stream_) {
        EXPECT_CALL(*mocks_->audio_renderer(), SetPlaybackRate(0.0f));
        EXPECT_CALL(*mocks_->audio_renderer(), SetVolume(1.0f));

        // Startup sequence.
        EXPECT_CALL(*mocks_->audio_renderer(), Preroll(base::TimeDelta(), _))
            .WillOnce(RunCallback<1>(PIPELINE_OK));
        EXPECT_CALL(*mocks_->audio_renderer(), Play(_))
            .WillOnce(RunClosure<0>());
      }
      EXPECT_CALL(callbacks_,
                  OnBufferingState(PipelineImpl::kPrerollCompleted));
    }

    pipeline_->Start(
        mocks_->Create().Pass(), SetDecryptorReadyCB(),
        base::Bind(&CallbackHelper::OnEnded, base::Unretained(&callbacks_)),
        base::Bind(&CallbackHelper::OnError, base::Unretained(&callbacks_)),
        base::Bind(&CallbackHelper::OnStart, base::Unretained(&callbacks_)),
        base::Bind(&CallbackHelper::OnBufferingState,
                   base::Unretained(&callbacks_)),
        base::Closure());
    message_loop_.RunUntilIdle();
  }

  void CreateAudioStream() {
    audio_stream_ = CreateStream(DemuxerStream::AUDIO);
  }

  void CreateVideoStream() {
    video_stream_ = CreateStream(DemuxerStream::VIDEO);
    EXPECT_CALL(*video_stream_, video_decoder_config())
        .WillRepeatedly(ReturnRef(video_decoder_config_));
  }

  MockDemuxerStream* audio_stream() {
    return audio_stream_;
  }

  MockDemuxerStream* video_stream() {
    return video_stream_;
  }

  void ExpectSeek(const base::TimeDelta& seek_time) {
    // Every filter should receive a call to Seek().
    EXPECT_CALL(*mocks_->demuxer(), Seek(seek_time, _))
        .WillOnce(RunCallback<1>(PIPELINE_OK));
    EXPECT_CALL(*mocks_->demuxer(), SetPlaybackRate(_));

    if (audio_stream_) {
      EXPECT_CALL(*mocks_->audio_renderer(), Pause(_))
          .WillOnce(RunClosure<0>());
      EXPECT_CALL(*mocks_->audio_renderer(), Flush(_))
          .WillOnce(RunClosure<0>());
      EXPECT_CALL(*mocks_->audio_renderer(), Preroll(seek_time, _))
          .WillOnce(RunCallback<1>(PIPELINE_OK));
      EXPECT_CALL(*mocks_->audio_renderer(), SetPlaybackRate(_));
      EXPECT_CALL(*mocks_->audio_renderer(), SetVolume(_));
      EXPECT_CALL(*mocks_->audio_renderer(), Play(_))
          .WillOnce(RunClosure<0>());
    }

    if (video_stream_) {
      EXPECT_CALL(*mocks_->video_renderer(), Pause(_))
          .WillOnce(RunClosure<0>());
      EXPECT_CALL(*mocks_->video_renderer(), Flush(_))
          .WillOnce(RunClosure<0>());
      EXPECT_CALL(*mocks_->video_renderer(), Preroll(seek_time, _))
          .WillOnce(RunCallback<1>(PIPELINE_OK));
      EXPECT_CALL(*mocks_->video_renderer(), SetPlaybackRate(_));
      EXPECT_CALL(*mocks_->video_renderer(), Play(_))
          .WillOnce(RunClosure<0>());
    }

    EXPECT_CALL(callbacks_, OnBufferingState(PipelineImpl::kPrerollCompleted));

    // We expect a successful seek callback.
    EXPECT_CALL(callbacks_, OnSeek(PIPELINE_OK));
  }

  void DoSeek(const base::TimeDelta& seek_time) {
    pipeline_->Seek(seek_time,
                    base::Bind(&CallbackHelper::OnSeek,
                               base::Unretained(&callbacks_)));

    // We expect the time to be updated only after the seek has completed.
    EXPECT_NE(seek_time, pipeline_->GetMediaTime());
    message_loop_.RunUntilIdle();
    EXPECT_EQ(seek_time, pipeline_->GetMediaTime());
  }

  // Fixture members.
  StrictMock<CallbackHelper> callbacks_;
  MessageLoop message_loop_;
  scoped_refptr<PipelineImpl> pipeline_;
  scoped_ptr<media::MockFilterCollection> mocks_;
  scoped_refptr<StrictMock<MockDemuxerStream> > audio_stream_;
  scoped_refptr<StrictMock<MockDemuxerStream> > video_stream_;
  AudioRenderer::TimeCB audio_time_cb_;
  VideoDecoderConfig video_decoder_config_;

 private:
  DISALLOW_COPY_AND_ASSIGN(PipelineTest);
};

// Test that playback controls methods no-op when the pipeline hasn't been
// started.
TEST_F(PipelineTest, NotStarted) {
  const base::TimeDelta kZero;

  EXPECT_FALSE(pipeline_->IsRunning());
  EXPECT_FALSE(pipeline_->HasAudio());
  EXPECT_FALSE(pipeline_->HasVideo());

  // Setting should still work.
  EXPECT_EQ(0.0f, pipeline_->GetPlaybackRate());
  pipeline_->SetPlaybackRate(-1.0f);
  EXPECT_EQ(0.0f, pipeline_->GetPlaybackRate());
  pipeline_->SetPlaybackRate(1.0f);
  EXPECT_EQ(1.0f, pipeline_->GetPlaybackRate());

  // Setting should still work.
  EXPECT_EQ(1.0f, pipeline_->GetVolume());
  pipeline_->SetVolume(-1.0f);
  EXPECT_EQ(1.0f, pipeline_->GetVolume());
  pipeline_->SetVolume(0.0f);
  EXPECT_EQ(0.0f, pipeline_->GetVolume());

  EXPECT_TRUE(kZero == pipeline_->GetMediaTime());
  EXPECT_EQ(0u, pipeline_->GetBufferedTimeRanges().size());
  EXPECT_TRUE(kZero == pipeline_->GetMediaDuration());

  EXPECT_EQ(0, pipeline_->GetTotalBytes());

  // Should always get set to zero.
  gfx::Size size(1, 1);
  pipeline_->GetNaturalVideoSize(&size);
  EXPECT_EQ(0, size.width());
  EXPECT_EQ(0, size.height());
}

TEST_F(PipelineTest, NeverInitializes) {
  // Don't execute the callback passed into Initialize().
  EXPECT_CALL(*mocks_->demuxer(), Initialize(_, _));

  // This test hangs during initialization by never calling
  // InitializationComplete().  StrictMock<> will ensure that the callback is
  // never executed.
  pipeline_->Start(
      mocks_->Create().Pass(), SetDecryptorReadyCB(),
      base::Bind(&CallbackHelper::OnEnded, base::Unretained(&callbacks_)),
      base::Bind(&CallbackHelper::OnError, base::Unretained(&callbacks_)),
      base::Bind(&CallbackHelper::OnStart, base::Unretained(&callbacks_)),
      base::Bind(&CallbackHelper::OnBufferingState,
                 base::Unretained(&callbacks_)),
      base::Closure());
  message_loop_.RunUntilIdle();


  // Because our callback will get executed when the test tears down, we'll
  // verify that nothing has been called, then set our expectation for the call
  // made during tear down.
  Mock::VerifyAndClear(&callbacks_);
  EXPECT_CALL(callbacks_, OnStart(PIPELINE_OK));
}

TEST_F(PipelineTest, URLNotFound) {
  EXPECT_CALL(*mocks_->demuxer(), Initialize(_, _))
      .WillOnce(RunCallback<1>(PIPELINE_ERROR_URL_NOT_FOUND));
  EXPECT_CALL(*mocks_->demuxer(), Stop(_))
      .WillOnce(RunClosure<0>());

  InitializePipeline(PIPELINE_ERROR_URL_NOT_FOUND);
}

TEST_F(PipelineTest, NoStreams) {
  EXPECT_CALL(*mocks_->demuxer(), Initialize(_, _))
      .WillOnce(RunCallback<1>(PIPELINE_OK));
  EXPECT_CALL(*mocks_->demuxer(), Stop(_))
      .WillOnce(RunClosure<0>());

  InitializePipeline(PIPELINE_ERROR_COULD_NOT_RENDER);
}

TEST_F(PipelineTest, AudioStream) {
  CreateAudioStream();
  MockDemuxerStreamVector streams;
  streams.push_back(audio_stream());

  InitializeDemuxer(&streams);
  InitializeAudioRenderer(audio_stream(), false);

  InitializePipeline(PIPELINE_OK);
  EXPECT_TRUE(pipeline_->HasAudio());
  EXPECT_FALSE(pipeline_->HasVideo());
}

TEST_F(PipelineTest, VideoStream) {
  CreateVideoStream();
  MockDemuxerStreamVector streams;
  streams.push_back(video_stream());

  InitializeDemuxer(&streams);
  InitializeVideoRenderer(video_stream());

  InitializePipeline(PIPELINE_OK);
  EXPECT_FALSE(pipeline_->HasAudio());
  EXPECT_TRUE(pipeline_->HasVideo());
}

TEST_F(PipelineTest, AudioVideoStream) {
  CreateAudioStream();
  CreateVideoStream();
  MockDemuxerStreamVector streams;
  streams.push_back(audio_stream());
  streams.push_back(video_stream());

  InitializeDemuxer(&streams);
  InitializeAudioRenderer(audio_stream(), false);
  InitializeVideoRenderer(video_stream());

  InitializePipeline(PIPELINE_OK);
  EXPECT_TRUE(pipeline_->HasAudio());
  EXPECT_TRUE(pipeline_->HasVideo());
}

TEST_F(PipelineTest, Seek) {
  CreateAudioStream();
  CreateVideoStream();
  MockDemuxerStreamVector streams;
  streams.push_back(audio_stream());
  streams.push_back(video_stream());

  InitializeDemuxer(&streams, base::TimeDelta::FromSeconds(3000));
  InitializeAudioRenderer(audio_stream(), false);
  InitializeVideoRenderer(video_stream());

  // Initialize then seek!
  InitializePipeline(PIPELINE_OK);

  // Every filter should receive a call to Seek().
  base::TimeDelta expected = base::TimeDelta::FromSeconds(2000);
  ExpectSeek(expected);
  DoSeek(expected);
}

TEST_F(PipelineTest, SetVolume) {
  CreateAudioStream();
  MockDemuxerStreamVector streams;
  streams.push_back(audio_stream());

  InitializeDemuxer(&streams);
  InitializeAudioRenderer(audio_stream(), false);

  // The audio renderer should receive a call to SetVolume().
  float expected = 0.5f;
  EXPECT_CALL(*mocks_->audio_renderer(), SetVolume(expected));

  // Initialize then set volume!
  InitializePipeline(PIPELINE_OK);
  pipeline_->SetVolume(expected);
}

TEST_F(PipelineTest, Properties) {
  CreateVideoStream();
  MockDemuxerStreamVector streams;
  streams.push_back(video_stream());

  const base::TimeDelta kDuration = base::TimeDelta::FromSeconds(100);
  InitializeDemuxer(&streams, kDuration);
  InitializeVideoRenderer(video_stream());

  InitializePipeline(PIPELINE_OK);
  EXPECT_EQ(kDuration.ToInternalValue(),
            pipeline_->GetMediaDuration().ToInternalValue());
  EXPECT_EQ(kTotalBytes, pipeline_->GetTotalBytes());
  EXPECT_FALSE(pipeline_->DidLoadingProgress());
}

TEST_F(PipelineTest, GetBufferedTimeRanges) {
  CreateVideoStream();
  MockDemuxerStreamVector streams;
  streams.push_back(video_stream());

  const base::TimeDelta kDuration = base::TimeDelta::FromSeconds(100);
  InitializeDemuxer(&streams, kDuration);
  InitializeVideoRenderer(video_stream());

  InitializePipeline(PIPELINE_OK);

  EXPECT_EQ(0u, pipeline_->GetBufferedTimeRanges().size());

  EXPECT_FALSE(pipeline_->DidLoadingProgress());
  pipeline_->AddBufferedByteRange(0, kTotalBytes / 8);
  EXPECT_TRUE(pipeline_->DidLoadingProgress());
  EXPECT_FALSE(pipeline_->DidLoadingProgress());
  EXPECT_EQ(1u, pipeline_->GetBufferedTimeRanges().size());
  EXPECT_EQ(base::TimeDelta(), pipeline_->GetBufferedTimeRanges().start(0));
  EXPECT_EQ(kDuration / 8, pipeline_->GetBufferedTimeRanges().end(0));
  pipeline_->AddBufferedTimeRange(base::TimeDelta(), kDuration / 8);
  EXPECT_EQ(base::TimeDelta(), pipeline_->GetBufferedTimeRanges().start(0));
  EXPECT_EQ(kDuration / 8, pipeline_->GetBufferedTimeRanges().end(0));

  base::TimeDelta kSeekTime = kDuration / 2;
  ExpectSeek(kSeekTime);
  DoSeek(kSeekTime);

  EXPECT_TRUE(pipeline_->DidLoadingProgress());
  EXPECT_FALSE(pipeline_->DidLoadingProgress());
  pipeline_->AddBufferedByteRange(kTotalBytes / 2,
                                  kTotalBytes / 2 + kTotalBytes / 8);
  EXPECT_TRUE(pipeline_->DidLoadingProgress());
  EXPECT_FALSE(pipeline_->DidLoadingProgress());
  EXPECT_EQ(2u, pipeline_->GetBufferedTimeRanges().size());
  EXPECT_EQ(base::TimeDelta(), pipeline_->GetBufferedTimeRanges().start(0));
  EXPECT_EQ(kDuration / 8, pipeline_->GetBufferedTimeRanges().end(0));
  EXPECT_EQ(kDuration / 2, pipeline_->GetBufferedTimeRanges().start(1));
  EXPECT_EQ(kDuration / 2 + kDuration / 8,
            pipeline_->GetBufferedTimeRanges().end(1));

  pipeline_->AddBufferedTimeRange(kDuration / 4, 3 * kDuration / 8);
  EXPECT_EQ(base::TimeDelta(), pipeline_->GetBufferedTimeRanges().start(0));
  EXPECT_EQ(kDuration / 8, pipeline_->GetBufferedTimeRanges().end(0));
  EXPECT_EQ(kDuration / 4, pipeline_->GetBufferedTimeRanges().start(1));
  EXPECT_EQ(3* kDuration / 8, pipeline_->GetBufferedTimeRanges().end(1));
  EXPECT_EQ(kDuration / 2, pipeline_->GetBufferedTimeRanges().start(2));
  EXPECT_EQ(kDuration / 2 + kDuration / 8,
            pipeline_->GetBufferedTimeRanges().end(2));
}

TEST_F(PipelineTest, DisableAudioRenderer) {
  CreateAudioStream();
  CreateVideoStream();
  MockDemuxerStreamVector streams;
  streams.push_back(audio_stream());
  streams.push_back(video_stream());

  InitializeDemuxer(&streams);
  InitializeAudioRenderer(audio_stream(), false);
  InitializeVideoRenderer(video_stream());

  InitializePipeline(PIPELINE_OK);
  EXPECT_TRUE(pipeline_->HasAudio());
  EXPECT_TRUE(pipeline_->HasVideo());

  EXPECT_CALL(*mocks_->demuxer(), OnAudioRendererDisabled());
  pipeline_->OnAudioDisabled();

  // Verify that ended event is fired when video ends.
  EXPECT_CALL(callbacks_, OnEnded(PIPELINE_OK));
  pipeline_->OnVideoRendererEnded();
}

TEST_F(PipelineTest, DisableAudioRendererDuringInit) {
  CreateAudioStream();
  CreateVideoStream();
  MockDemuxerStreamVector streams;
  streams.push_back(audio_stream());
  streams.push_back(video_stream());

  InitializeDemuxer(&streams);
  InitializeAudioRenderer(audio_stream(), true);
  InitializeVideoRenderer(video_stream());

  EXPECT_CALL(*mocks_->demuxer(),
              OnAudioRendererDisabled());

  InitializePipeline(PIPELINE_OK);
  EXPECT_FALSE(pipeline_->HasAudio());
  EXPECT_TRUE(pipeline_->HasVideo());

  // Verify that ended event is fired when video ends.
  EXPECT_CALL(callbacks_, OnEnded(PIPELINE_OK));
  pipeline_->OnVideoRendererEnded();
}

TEST_F(PipelineTest, EndedCallback) {
  CreateAudioStream();
  CreateVideoStream();
  MockDemuxerStreamVector streams;
  streams.push_back(audio_stream());
  streams.push_back(video_stream());

  InitializeDemuxer(&streams);
  InitializeAudioRenderer(audio_stream(), false);
  InitializeVideoRenderer(video_stream());
  InitializePipeline(PIPELINE_OK);

  // The ended callback shouldn't run until both renderers have ended.
  pipeline_->OnAudioRendererEnded();
  message_loop_.RunUntilIdle();

  EXPECT_CALL(callbacks_, OnEnded(PIPELINE_OK));
  pipeline_->OnVideoRendererEnded();
  message_loop_.RunUntilIdle();
}

// Static function & time variable used to simulate changes in wallclock time.
static int64 g_static_clock_time;
static base::Time StaticClockFunction() {
  return base::Time::FromInternalValue(g_static_clock_time);
}

TEST_F(PipelineTest, AudioStreamShorterThanVideo) {
  base::TimeDelta duration = base::TimeDelta::FromSeconds(10);

  CreateAudioStream();
  CreateVideoStream();
  MockDemuxerStreamVector streams;
  streams.push_back(audio_stream());
  streams.push_back(video_stream());

  // Replace the clock so we can simulate wallclock time advancing w/o using
  // Sleep().
  pipeline_->SetClockForTesting(new Clock(&StaticClockFunction));

  InitializeDemuxer(&streams, duration);
  InitializeAudioRenderer(audio_stream(), false);
  InitializeVideoRenderer(video_stream());
  InitializePipeline(PIPELINE_OK);

  EXPECT_EQ(0, pipeline_->GetMediaTime().ToInternalValue());

  float playback_rate = 1.0f;
  EXPECT_CALL(*mocks_->demuxer(), SetPlaybackRate(playback_rate));
  EXPECT_CALL(*mocks_->video_renderer(), SetPlaybackRate(playback_rate));
  EXPECT_CALL(*mocks_->audio_renderer(), SetPlaybackRate(playback_rate));
  pipeline_->SetPlaybackRate(playback_rate);
  message_loop_.RunUntilIdle();

  InSequence s;

  // Verify that the clock doesn't advance since it hasn't been started by
  // a time update from the audio stream.
  int64 start_time = pipeline_->GetMediaTime().ToInternalValue();
  g_static_clock_time +=
      base::TimeDelta::FromMilliseconds(100).ToInternalValue();
  EXPECT_EQ(pipeline_->GetMediaTime().ToInternalValue(), start_time);

  // Signal end of audio stream.
  pipeline_->OnAudioRendererEnded();
  message_loop_.RunUntilIdle();

  // Verify that the clock advances.
  start_time = pipeline_->GetMediaTime().ToInternalValue();
  g_static_clock_time +=
      base::TimeDelta::FromMilliseconds(100).ToInternalValue();
  EXPECT_GT(pipeline_->GetMediaTime().ToInternalValue(), start_time);

  // Signal end of video stream and make sure OnEnded() callback occurs.
  EXPECT_CALL(callbacks_, OnEnded(PIPELINE_OK));
  pipeline_->OnVideoRendererEnded();
}

TEST_F(PipelineTest, ErrorDuringSeek) {
  CreateAudioStream();
  MockDemuxerStreamVector streams;
  streams.push_back(audio_stream());

  InitializeDemuxer(&streams);
  InitializeAudioRenderer(audio_stream(), false);
  InitializePipeline(PIPELINE_OK);

  float playback_rate = 1.0f;
  EXPECT_CALL(*mocks_->demuxer(), SetPlaybackRate(playback_rate));
  EXPECT_CALL(*mocks_->audio_renderer(), SetPlaybackRate(playback_rate));
  pipeline_->SetPlaybackRate(playback_rate);
  message_loop_.RunUntilIdle();

  base::TimeDelta seek_time = base::TimeDelta::FromSeconds(5);

  // Preroll() isn't called as the demuxer errors out first.
  EXPECT_CALL(*mocks_->audio_renderer(), Pause(_))
      .WillOnce(RunClosure<0>());
  EXPECT_CALL(*mocks_->audio_renderer(), Flush(_))
      .WillOnce(RunClosure<0>());
  EXPECT_CALL(*mocks_->audio_renderer(), Stop(_))
      .WillOnce(RunClosure<0>());

  EXPECT_CALL(*mocks_->demuxer(), Seek(seek_time, _))
      .WillOnce(RunCallback<1>(PIPELINE_ERROR_READ));
  EXPECT_CALL(*mocks_->demuxer(), Stop(_))
      .WillOnce(RunClosure<0>());

  pipeline_->Seek(seek_time, base::Bind(&CallbackHelper::OnSeek,
                                        base::Unretained(&callbacks_)));
  EXPECT_CALL(callbacks_, OnSeek(PIPELINE_ERROR_READ));
  message_loop_.RunUntilIdle();
}

// Invoked function OnError. This asserts that the pipeline does not enqueue
// non-teardown related tasks while tearing down.
static void TestNoCallsAfterError(PipelineImpl* pipeline,
                                  MessageLoop* message_loop,
                                  PipelineStatus /* status */) {
  CHECK(pipeline);
  CHECK(message_loop);

  // When we get to this stage, the message loop should be empty.
  message_loop->AssertIdle();

  // Make calls on pipeline after error has occurred.
  pipeline->SetPlaybackRate(0.5f);
  pipeline->SetVolume(0.5f);

  // No additional tasks should be queued as a result of these calls.
  message_loop->AssertIdle();
}

TEST_F(PipelineTest, NoMessageDuringTearDownFromError) {
  CreateAudioStream();
  MockDemuxerStreamVector streams;
  streams.push_back(audio_stream());

  InitializeDemuxer(&streams);
  InitializeAudioRenderer(audio_stream(), false);
  InitializePipeline(PIPELINE_OK);

  // Trigger additional requests on the pipeline during tear down from error.
  base::Callback<void(PipelineStatus)> cb = base::Bind(
      &TestNoCallsAfterError, pipeline_, &message_loop_);
  ON_CALL(callbacks_, OnError(_))
      .WillByDefault(Invoke(&cb, &base::Callback<void(PipelineStatus)>::Run));

  base::TimeDelta seek_time = base::TimeDelta::FromSeconds(5);

  // Seek() isn't called as the demuxer errors out first.
  EXPECT_CALL(*mocks_->audio_renderer(), Pause(_))
      .WillOnce(RunClosure<0>());
  EXPECT_CALL(*mocks_->audio_renderer(), Flush(_))
      .WillOnce(RunClosure<0>());
  EXPECT_CALL(*mocks_->audio_renderer(), Stop(_))
      .WillOnce(RunClosure<0>());

  EXPECT_CALL(*mocks_->demuxer(), Seek(seek_time, _))
      .WillOnce(RunCallback<1>(PIPELINE_ERROR_READ));
  EXPECT_CALL(*mocks_->demuxer(), Stop(_))
      .WillOnce(RunClosure<0>());

  pipeline_->Seek(seek_time, base::Bind(&CallbackHelper::OnSeek,
                                        base::Unretained(&callbacks_)));
  EXPECT_CALL(callbacks_, OnSeek(PIPELINE_ERROR_READ));
  message_loop_.RunUntilIdle();
}

TEST_F(PipelineTest, StartTimeIsZero) {
  CreateVideoStream();
  MockDemuxerStreamVector streams;
  streams.push_back(video_stream());

  const base::TimeDelta kDuration = base::TimeDelta::FromSeconds(100);
  InitializeDemuxer(&streams, kDuration);
  InitializeVideoRenderer(video_stream());

  InitializePipeline(PIPELINE_OK);
  EXPECT_FALSE(pipeline_->HasAudio());
  EXPECT_TRUE(pipeline_->HasVideo());

  EXPECT_EQ(base::TimeDelta(), pipeline_->GetMediaTime());
}

TEST_F(PipelineTest, StartTimeIsNonZero) {
  const base::TimeDelta kStartTime = base::TimeDelta::FromSeconds(4);
  const base::TimeDelta kDuration = base::TimeDelta::FromSeconds(100);

  EXPECT_CALL(*mocks_->demuxer(), GetStartTime())
      .WillRepeatedly(Return(kStartTime));

  CreateVideoStream();
  MockDemuxerStreamVector streams;
  streams.push_back(video_stream());

  InitializeDemuxer(&streams, kDuration);
  InitializeVideoRenderer(video_stream());

  InitializePipeline(PIPELINE_OK);
  EXPECT_FALSE(pipeline_->HasAudio());
  EXPECT_TRUE(pipeline_->HasVideo());

  EXPECT_EQ(kStartTime, pipeline_->GetMediaTime());
}

static void RunTimeCB(const AudioRenderer::TimeCB& time_cb,
                       int time_in_ms,
                       int max_time_in_ms) {
  time_cb.Run(base::TimeDelta::FromMilliseconds(time_in_ms),
              base::TimeDelta::FromMilliseconds(max_time_in_ms));
}

TEST_F(PipelineTest, AudioTimeUpdateDuringSeek) {
  CreateAudioStream();
  MockDemuxerStreamVector streams;
  streams.push_back(audio_stream());

  InitializeDemuxer(&streams);
  InitializeAudioRenderer(audio_stream(), false);
  InitializePipeline(PIPELINE_OK);

  float playback_rate = 1.0f;
  EXPECT_CALL(*mocks_->demuxer(), SetPlaybackRate(playback_rate));
  EXPECT_CALL(*mocks_->audio_renderer(), SetPlaybackRate(playback_rate));
  pipeline_->SetPlaybackRate(playback_rate);
  message_loop_.RunUntilIdle();

  // Provide an initial time update so that the pipeline transitions out of the
  // "waiting for time update" state.
  audio_time_cb_.Run(base::TimeDelta::FromMilliseconds(100),
                     base::TimeDelta::FromMilliseconds(500));

  base::TimeDelta seek_time = base::TimeDelta::FromSeconds(5);

  // Arrange to trigger a time update while the demuxer is in the middle of
  // seeking. This update should be ignored by the pipeline and the clock should
  // not get updated.
  base::Closure closure = base::Bind(&RunTimeCB, audio_time_cb_, 300, 700);
  EXPECT_CALL(*mocks_->demuxer(), Seek(seek_time, _))
      .WillOnce(DoAll(InvokeWithoutArgs(&closure, &base::Closure::Run),
                      RunCallback<1>(PIPELINE_OK)));

  EXPECT_CALL(*mocks_->audio_renderer(), Pause(_))
      .WillOnce(RunClosure<0>());
  EXPECT_CALL(*mocks_->audio_renderer(), Flush(_))
      .WillOnce(RunClosure<0>());
  EXPECT_CALL(*mocks_->audio_renderer(), Preroll(seek_time, _))
      .WillOnce(RunCallback<1>(PIPELINE_OK));
  EXPECT_CALL(*mocks_->demuxer(), SetPlaybackRate(_));
  EXPECT_CALL(*mocks_->audio_renderer(), SetPlaybackRate(_));
  EXPECT_CALL(*mocks_->audio_renderer(), SetVolume(_));
  EXPECT_CALL(*mocks_->audio_renderer(), Play(_))
      .WillOnce(RunClosure<0>());

  EXPECT_CALL(callbacks_, OnBufferingState(PipelineImpl::kPrerollCompleted));
  EXPECT_CALL(callbacks_, OnSeek(PIPELINE_OK));
  DoSeek(seek_time);

  EXPECT_EQ(pipeline_->GetMediaTime(), seek_time);

  // Now that the seek is complete, verify that time updates advance the current
  // time.
  base::TimeDelta new_time = seek_time + base::TimeDelta::FromMilliseconds(100);
  audio_time_cb_.Run(new_time, new_time);

  EXPECT_EQ(pipeline_->GetMediaTime(), new_time);
}

class FlexibleCallbackRunner : public base::DelegateSimpleThread::Delegate {
 public:
  FlexibleCallbackRunner(base::TimeDelta delay, PipelineStatus status,
                         const PipelineStatusCB& status_cb)
      : delay_(delay),
        status_(status),
        status_cb_(status_cb) {
    if (delay_ < base::TimeDelta()) {
      status_cb_.Run(status_);
      return;
    }
  }
  virtual void Run() {
    if (delay_ < base::TimeDelta()) return;
    base::PlatformThread::Sleep(delay_);
    status_cb_.Run(status_);
  }

 private:
  base::TimeDelta delay_;
  PipelineStatus status_;
  PipelineStatusCB status_cb_;
};

void TestPipelineStatusNotification(base::TimeDelta delay) {
  PipelineStatusNotification note;
  // Arbitrary error value we expect to fish out of the notification after the
  // callback is fired.
  const PipelineStatus expected_error = PIPELINE_ERROR_URL_NOT_FOUND;
  FlexibleCallbackRunner runner(delay, expected_error, note.Callback());
  base::DelegateSimpleThread thread(&runner, "FlexibleCallbackRunner");
  thread.Start();
  note.Wait();
  EXPECT_EQ(note.status(), expected_error);
  thread.Join();
}

// Test that in-line callback (same thread, no yield) works correctly.
TEST(PipelineStatusNotificationTest, InlineCallback) {
  TestPipelineStatusNotification(base::TimeDelta::FromMilliseconds(-1));
}

// Test that different-thread, no-delay callback works correctly.
TEST(PipelineStatusNotificationTest, ImmediateCallback) {
  TestPipelineStatusNotification(base::TimeDelta::FromMilliseconds(0));
}

// Test that different-thread, some-delay callback (the expected common case)
// works correctly.
TEST(PipelineStatusNotificationTest, DelayedCallback) {
  TestPipelineStatusNotification(base::TimeDelta::FromMilliseconds(20));
}

class PipelineTeardownTest : public PipelineTest {
 public:
  enum TeardownState {
    kInitDemuxer,
    kInitAudioRenderer,
    kInitVideoRenderer,
    kPausing,
    kFlushing,
    kSeeking,
    kPrerolling,
    kStarting,
    kPlaying,
  };

  enum StopOrError {
    kStop,
    kError,
  };

  PipelineTeardownTest() {}
  virtual ~PipelineTeardownTest() {}

  void RunTest(TeardownState state, StopOrError stop_or_error) {
    switch (state) {
      case kInitDemuxer:
      case kInitAudioRenderer:
      case kInitVideoRenderer:
        DoInitialize(state, stop_or_error);
        break;

      case kPausing:
      case kFlushing:
      case kSeeking:
      case kPrerolling:
      case kStarting:
        DoInitialize(state, stop_or_error);
        DoSeek(state, stop_or_error);
        break;

      case kPlaying:
        DoInitialize(state, stop_or_error);
        DoStopOrError(stop_or_error);
        break;
    }
  }

 private:
  // TODO(scherkus): We do radically different things whether teardown is
  // invoked via stop vs error. The teardown path should be the same,
  // see http://crbug.com/110228
  void DoInitialize(TeardownState state, StopOrError stop_or_error) {
    PipelineStatus expected_status =
        SetInitializeExpectations(state, stop_or_error);

    EXPECT_CALL(callbacks_, OnStart(expected_status));
    pipeline_->Start(
        mocks_->Create().Pass(), SetDecryptorReadyCB(),
        base::Bind(&CallbackHelper::OnEnded, base::Unretained(&callbacks_)),
        base::Bind(&CallbackHelper::OnError, base::Unretained(&callbacks_)),
        base::Bind(&CallbackHelper::OnStart, base::Unretained(&callbacks_)),
        base::Bind(&CallbackHelper::OnBufferingState,
                   base::Unretained(&callbacks_)),
        base::Closure());
    message_loop_.RunUntilIdle();
  }

  PipelineStatus SetInitializeExpectations(TeardownState state,
                                           StopOrError stop_or_error) {
    PipelineStatus status = PIPELINE_OK;
    base::Closure stop_cb = base::Bind(
        &CallbackHelper::OnStop, base::Unretained(&callbacks_));

    if (state == kInitDemuxer) {
      if (stop_or_error == kStop) {
        EXPECT_CALL(*mocks_->demuxer(), Initialize(_, _))
            .WillOnce(DoAll(Stop(pipeline_, stop_cb),
                            RunCallback<1>(PIPELINE_OK)));
        EXPECT_CALL(callbacks_, OnStop());
      } else {
        status = DEMUXER_ERROR_COULD_NOT_OPEN;
        EXPECT_CALL(*mocks_->demuxer(), Initialize(_, _))
            .WillOnce(RunCallback<1>(status));
      }

      EXPECT_CALL(*mocks_->demuxer(), Stop(_)).WillOnce(RunClosure<0>());
      return status;
    }

    CreateAudioStream();
    CreateVideoStream();
    MockDemuxerStreamVector streams;
    streams.push_back(audio_stream());
    streams.push_back(video_stream());
    InitializeDemuxer(&streams, base::TimeDelta::FromSeconds(3000));

    if (state == kInitAudioRenderer) {
      if (stop_or_error == kStop) {
        EXPECT_CALL(*mocks_->audio_renderer(),
                    Initialize(_, _, _, _, _, _, _, _, _))
            .WillOnce(DoAll(Stop(pipeline_, stop_cb),
                            RunCallback<2>(PIPELINE_OK)));
        EXPECT_CALL(callbacks_, OnStop());
      } else {
        status = PIPELINE_ERROR_INITIALIZATION_FAILED;
        EXPECT_CALL(*mocks_->audio_renderer(),
                    Initialize(_, _, _, _, _, _, _, _, _))
            .WillOnce(RunCallback<2>(status));
      }

      EXPECT_CALL(*mocks_->demuxer(), Stop(_)).WillOnce(RunClosure<0>());
      EXPECT_CALL(*mocks_->audio_renderer(), Stop(_)).WillOnce(RunClosure<0>());
      return status;
    }

    EXPECT_CALL(*mocks_->audio_renderer(),
                Initialize(_, _, _, _, _, _, _, _, _))
        .WillOnce(RunCallback<2>(PIPELINE_OK));

    if (state == kInitVideoRenderer) {
      if (stop_or_error == kStop) {
        EXPECT_CALL(*mocks_->video_renderer(),
                    Initialize(_, _, _, _, _, _, _, _, _, _))
            .WillOnce(DoAll(Stop(pipeline_, stop_cb),
                            RunCallback<2>(PIPELINE_OK)));
        EXPECT_CALL(callbacks_, OnStop());
      } else {
        status = PIPELINE_ERROR_INITIALIZATION_FAILED;
        EXPECT_CALL(*mocks_->video_renderer(),
                    Initialize(_, _, _, _, _, _, _, _, _, _))
            .WillOnce(RunCallback<2>(status));
      }

      EXPECT_CALL(*mocks_->demuxer(), Stop(_)).WillOnce(RunClosure<0>());
      EXPECT_CALL(*mocks_->audio_renderer(), Stop(_)).WillOnce(RunClosure<0>());
      EXPECT_CALL(*mocks_->video_renderer(), Stop(_)).WillOnce(RunClosure<0>());
      return status;
    }

    EXPECT_CALL(*mocks_->video_renderer(),
                Initialize(_, _, _, _, _, _, _, _, _, _))
        .WillOnce(RunCallback<2>(PIPELINE_OK));

    EXPECT_CALL(callbacks_, OnBufferingState(PipelineImpl::kHaveMetadata));

    // If we get here it's a successful initialization.
    EXPECT_CALL(*mocks_->audio_renderer(), Preroll(base::TimeDelta(), _))
        .WillOnce(RunCallback<1>(PIPELINE_OK));
    EXPECT_CALL(*mocks_->video_renderer(), Preroll(base::TimeDelta(), _))
        .WillOnce(RunCallback<1>(PIPELINE_OK));

    EXPECT_CALL(*mocks_->demuxer(), SetPlaybackRate(0.0f));
    EXPECT_CALL(*mocks_->audio_renderer(), SetPlaybackRate(0.0f));
    EXPECT_CALL(*mocks_->video_renderer(), SetPlaybackRate(0.0f));
    EXPECT_CALL(*mocks_->audio_renderer(), SetVolume(1.0f));

    EXPECT_CALL(*mocks_->audio_renderer(), Play(_))
        .WillOnce(RunClosure<0>());
    EXPECT_CALL(*mocks_->video_renderer(), Play(_))
        .WillOnce(RunClosure<0>());

    if (status == PIPELINE_OK)
      EXPECT_CALL(callbacks_,
                  OnBufferingState(PipelineImpl::kPrerollCompleted));

    return status;
  }

  void DoSeek(TeardownState state, StopOrError stop_or_error) {
    InSequence s;
    PipelineStatus status = SetSeekExpectations(state, stop_or_error);

    EXPECT_CALL(*mocks_->demuxer(), Stop(_)).WillOnce(RunClosure<0>());
    EXPECT_CALL(*mocks_->audio_renderer(), Stop(_)).WillOnce(RunClosure<0>());
    EXPECT_CALL(*mocks_->video_renderer(), Stop(_)).WillOnce(RunClosure<0>());
    EXPECT_CALL(callbacks_, OnSeek(status));

    if (status == PIPELINE_OK) {
      EXPECT_CALL(callbacks_, OnStop());
    }

    pipeline_->Seek(base::TimeDelta::FromSeconds(10), base::Bind(
        &CallbackHelper::OnSeek, base::Unretained(&callbacks_)));
    message_loop_.RunUntilIdle();
  }

  PipelineStatus SetSeekExpectations(TeardownState state,
                                     StopOrError stop_or_error) {
    PipelineStatus status = PIPELINE_OK;
    base::Closure stop_cb = base::Bind(
        &CallbackHelper::OnStop, base::Unretained(&callbacks_));

    if (state == kPausing) {
      if (stop_or_error == kStop) {
        EXPECT_CALL(*mocks_->audio_renderer(), Pause(_))
            .WillOnce(DoAll(Stop(pipeline_, stop_cb), RunClosure<0>()));
      } else {
        status = PIPELINE_ERROR_READ;
        EXPECT_CALL(*mocks_->audio_renderer(), Pause(_))
            .WillOnce(DoAll(SetError(pipeline_, status), RunClosure<0>()));
      }

      return status;
    }

    EXPECT_CALL(*mocks_->audio_renderer(), Pause(_)).WillOnce(RunClosure<0>());
    EXPECT_CALL(*mocks_->video_renderer(), Pause(_)).WillOnce(RunClosure<0>());

    if (state == kFlushing) {
      if (stop_or_error == kStop) {
        EXPECT_CALL(*mocks_->audio_renderer(), Flush(_))
            .WillOnce(DoAll(Stop(pipeline_, stop_cb), RunClosure<0>()));
      } else {
        status = PIPELINE_ERROR_READ;
        EXPECT_CALL(*mocks_->audio_renderer(), Flush(_))
            .WillOnce(DoAll(SetError(pipeline_, status), RunClosure<0>()));
      }

      return status;
    }

    EXPECT_CALL(*mocks_->audio_renderer(), Flush(_)).WillOnce(RunClosure<0>());
    EXPECT_CALL(*mocks_->video_renderer(), Flush(_)).WillOnce(RunClosure<0>());

    if (state == kSeeking) {
      if (stop_or_error == kStop) {
        EXPECT_CALL(*mocks_->demuxer(), Seek(_, _))
            .WillOnce(DoAll(Stop(pipeline_, stop_cb),
                            RunCallback<1>(PIPELINE_OK)));
      } else {
        status = PIPELINE_ERROR_READ;
        EXPECT_CALL(*mocks_->demuxer(), Seek(_, _))
            .WillOnce(RunCallback<1>(status));
      }

      return status;
    }

    EXPECT_CALL(*mocks_->demuxer(), Seek(_, _))
        .WillOnce(RunCallback<1>(PIPELINE_OK));

    if (state == kPrerolling) {
      if (stop_or_error == kStop) {
        EXPECT_CALL(*mocks_->audio_renderer(), Preroll(_, _))
            .WillOnce(DoAll(Stop(pipeline_, stop_cb),
                            RunCallback<1>(PIPELINE_OK)));
      } else {
        status = PIPELINE_ERROR_READ;
        EXPECT_CALL(*mocks_->audio_renderer(), Preroll(_, _))
            .WillOnce(RunCallback<1>(status));
      }

      return status;
    }

    EXPECT_CALL(*mocks_->audio_renderer(), Preroll(_, _))
        .WillOnce(RunCallback<1>(PIPELINE_OK));
    EXPECT_CALL(*mocks_->video_renderer(), Preroll(_, _))
        .WillOnce(RunCallback<1>(PIPELINE_OK));

    // Playback rate and volume are updated prior to starting.
    EXPECT_CALL(*mocks_->demuxer(), SetPlaybackRate(0.0f));
    EXPECT_CALL(*mocks_->audio_renderer(), SetPlaybackRate(0.0f));
    EXPECT_CALL(*mocks_->video_renderer(), SetPlaybackRate(0.0f));
    EXPECT_CALL(*mocks_->audio_renderer(), SetVolume(1.0f));

    if (state == kStarting) {
      if (stop_or_error == kStop) {
        EXPECT_CALL(*mocks_->audio_renderer(), Play(_))
            .WillOnce(DoAll(Stop(pipeline_, stop_cb), RunClosure<0>()));
      } else {
        status = PIPELINE_ERROR_READ;
        EXPECT_CALL(*mocks_->audio_renderer(), Play(_))
            .WillOnce(DoAll(SetError(pipeline_, status), RunClosure<0>()));
      }
      return status;
    }

    NOTREACHED() << "State not supported: " << state;
    return status;
  }

  void DoStopOrError(StopOrError stop_or_error) {
    InSequence s;

    EXPECT_CALL(*mocks_->demuxer(), Stop(_)).WillOnce(RunClosure<0>());
    EXPECT_CALL(*mocks_->audio_renderer(), Stop(_)).WillOnce(RunClosure<0>());
    EXPECT_CALL(*mocks_->video_renderer(), Stop(_)).WillOnce(RunClosure<0>());

    if (stop_or_error == kStop) {
      EXPECT_CALL(callbacks_, OnStop());
      pipeline_->Stop(base::Bind(
          &CallbackHelper::OnStop, base::Unretained(&callbacks_)));
    } else {
      EXPECT_CALL(callbacks_, OnError(PIPELINE_ERROR_READ));
      pipeline_->SetErrorForTesting(PIPELINE_ERROR_READ);
    }

    message_loop_.RunUntilIdle();
  }

  DISALLOW_COPY_AND_ASSIGN(PipelineTeardownTest);
};

#define INSTANTIATE_TEARDOWN_TEST(stop_or_error, state) \
    TEST_F(PipelineTeardownTest, stop_or_error##_##state) { \
      RunTest(k##state, k##stop_or_error); \
    }

INSTANTIATE_TEARDOWN_TEST(Stop, InitDemuxer);
INSTANTIATE_TEARDOWN_TEST(Stop, InitAudioRenderer);
INSTANTIATE_TEARDOWN_TEST(Stop, InitVideoRenderer);
INSTANTIATE_TEARDOWN_TEST(Stop, Pausing);
INSTANTIATE_TEARDOWN_TEST(Stop, Flushing);
INSTANTIATE_TEARDOWN_TEST(Stop, Seeking);
INSTANTIATE_TEARDOWN_TEST(Stop, Prerolling);
INSTANTIATE_TEARDOWN_TEST(Stop, Starting);
INSTANTIATE_TEARDOWN_TEST(Stop, Playing);

INSTANTIATE_TEARDOWN_TEST(Error, InitDemuxer);
INSTANTIATE_TEARDOWN_TEST(Error, InitAudioRenderer);
INSTANTIATE_TEARDOWN_TEST(Error, InitVideoRenderer);
INSTANTIATE_TEARDOWN_TEST(Error, Pausing);
INSTANTIATE_TEARDOWN_TEST(Error, Flushing);
INSTANTIATE_TEARDOWN_TEST(Error, Seeking);
INSTANTIATE_TEARDOWN_TEST(Error, Prerolling);
INSTANTIATE_TEARDOWN_TEST(Error, Starting);
INSTANTIATE_TEARDOWN_TEST(Error, Playing);

}  // namespace media
