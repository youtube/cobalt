// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/bind.h"
#include "base/callback.h"
#include "base/stl_util.h"
#include "base/threading/simple_thread.h"
#include "media/base/filter_host.h"
#include "media/base/filters.h"
#include "media/base/media_log.h"
#include "media/base/pipeline_impl.h"
#include "media/base/mock_callback.h"
#include "media/base/mock_filters.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/size.h"

using ::testing::_;
using ::testing::DeleteArg;
using ::testing::InSequence;
using ::testing::Invoke;
using ::testing::Mock;
using ::testing::NotNull;
using ::testing::Return;
using ::testing::ReturnRef;
using ::testing::StrictMock;

namespace media {

// Total bytes of the data source.
static const int kTotalBytes = 1024;

// Buffered bytes of the data source.
static const int kBufferedBytes = 1024;

// Used for setting expectations on pipeline callbacks.  Using a StrictMock
// also lets us test for missing callbacks.
class CallbackHelper {
 public:
  CallbackHelper() {}
  virtual ~CallbackHelper() {}

  MOCK_METHOD1(OnStart, void(PipelineStatus));
  MOCK_METHOD1(OnSeek, void(PipelineStatus));
  MOCK_METHOD1(OnStop, void(PipelineStatus));
  MOCK_METHOD1(OnEnded, void(PipelineStatus));
  MOCK_METHOD1(OnError, void(PipelineStatus));

 private:
  DISALLOW_COPY_AND_ASSIGN(CallbackHelper);
};

// TODO(scherkus): even though some filters are initialized on separate
// threads these test aren't flaky... why?  It's because filters' Initialize()
// is executed on |message_loop_| and the mock filters instantly call
// InitializationComplete(), which keeps the pipeline humming along.  If
// either filters don't call InitializationComplete() immediately or filter
// initialization is moved to a separate thread this test will become flaky.
class PipelineImplTest : public ::testing::Test {
 public:
  PipelineImplTest()
      : pipeline_(new PipelineImpl(&message_loop_, new MediaLog())) {
    pipeline_->Init(
        base::Bind(&CallbackHelper::OnEnded, base::Unretained(&callbacks_)),
        base::Bind(&CallbackHelper::OnError, base::Unretained(&callbacks_)),
        Pipeline::NetworkEventCB());
    mocks_.reset(new MockFilterCollection());

    // InitializeDemuxer adds overriding expectations for expected non-NULL
    // streams.
    DemuxerStream* null_pointer = NULL;
    EXPECT_CALL(*mocks_->demuxer(), GetStream(_))
        .WillRepeatedly(Return(null_pointer));

    EXPECT_CALL(*mocks_->demuxer(), GetStartTime())
        .WillRepeatedly(Return(base::TimeDelta()));
  }

  virtual ~PipelineImplTest() {
    if (!pipeline_->IsRunning()) {
      return;
    }

    // Expect a stop callback if we were started.
    EXPECT_CALL(callbacks_, OnStop(PIPELINE_OK));
    pipeline_->Stop(base::Bind(&CallbackHelper::OnStop,
                               base::Unretained(&callbacks_)));
    message_loop_.RunAllPending();

    mocks_.reset();
  }

 protected:
  // Sets up expectations to allow the demuxer to initialize.
  typedef std::vector<MockDemuxerStream*> MockDemuxerStreamVector;
  void InitializeDemuxer(MockDemuxerStreamVector* streams,
                         const base::TimeDelta& duration) {
    mocks_->demuxer()->SetTotalAndBufferedBytesAndDuration(
        kTotalBytes, kBufferedBytes, duration);
    EXPECT_CALL(*mocks_->demuxer(), SetPlaybackRate(0.0f));
    EXPECT_CALL(*mocks_->demuxer(), SetPreload(AUTO));
    EXPECT_CALL(*mocks_->demuxer(), Seek(mocks_->demuxer()->GetStartTime(), _))
        .WillOnce(Invoke(&RunFilterStatusCB));
    EXPECT_CALL(*mocks_->demuxer(), Stop(_))
        .WillOnce(Invoke(&RunStopFilterCallback));

    // Configure the demuxer to return the streams.
    for (size_t i = 0; i < streams->size(); ++i) {
      scoped_refptr<DemuxerStream> stream((*streams)[i]);
      EXPECT_CALL(*mocks_->demuxer(), GetStream(stream->type()))
          .WillRepeatedly(Return(stream));
    }
  }

  StrictMock<MockDemuxerStream>* CreateStream(DemuxerStream::Type type) {
    StrictMock<MockDemuxerStream>* stream =
        new StrictMock<MockDemuxerStream>();
    EXPECT_CALL(*stream, type())
        .WillRepeatedly(Return(type));
    return stream;
  }

  // Sets up expectations to allow the video decoder to initialize.
  void InitializeVideoDecoder(MockDemuxerStream* stream) {
    EXPECT_CALL(*mocks_->video_decoder(),
                Initialize(stream, _, _))
        .WillOnce(Invoke(&RunFilterCallback3));
    EXPECT_CALL(*mocks_->video_decoder(), SetPlaybackRate(0.0f));
    EXPECT_CALL(*mocks_->video_decoder(),
                Seek(mocks_->demuxer()->GetStartTime(), _))
        .WillOnce(Invoke(&RunFilterStatusCB));
    EXPECT_CALL(*mocks_->video_decoder(), Stop(_))
        .WillOnce(Invoke(&RunStopFilterCallback));
  }

  // Sets up expectations to allow the audio decoder to initialize.
  void InitializeAudioDecoder(MockDemuxerStream* stream) {
    EXPECT_CALL(*mocks_->audio_decoder(), Initialize(stream, _, _))
        .WillOnce(Invoke(&RunFilterCallback3));
    EXPECT_CALL(*mocks_->audio_decoder(), SetPlaybackRate(0.0f));
    EXPECT_CALL(*mocks_->audio_decoder(), Seek(base::TimeDelta(), _))
        .WillOnce(Invoke(&RunFilterStatusCB));
    EXPECT_CALL(*mocks_->audio_decoder(), Stop(_))
        .WillOnce(Invoke(&RunStopFilterCallback));
  }

  // Sets up expectations to allow the video renderer to initialize.
  void InitializeVideoRenderer() {
    EXPECT_CALL(*mocks_->video_renderer(),
                Initialize(mocks_->video_decoder(), _, _))
        .WillOnce(Invoke(&RunFilterCallback3));
    EXPECT_CALL(*mocks_->video_renderer(), SetPlaybackRate(0.0f));
    EXPECT_CALL(*mocks_->video_renderer(),
                Seek(mocks_->demuxer()->GetStartTime(), _))
        .WillOnce(Invoke(&RunFilterStatusCB));
    EXPECT_CALL(*mocks_->video_renderer(), Stop(_))
        .WillOnce(Invoke(&RunStopFilterCallback));
  }

  // Sets up expectations to allow the audio renderer to initialize.
  void InitializeAudioRenderer(bool disable_after_init_callback = false) {
    if (disable_after_init_callback) {
      EXPECT_CALL(*mocks_->audio_renderer(),
                  Initialize(mocks_->audio_decoder(), _, _))
          .WillOnce(DoAll(Invoke(&RunFilterCallback3),
                          DisableAudioRenderer(mocks_->audio_renderer())));
    } else {
      EXPECT_CALL(*mocks_->audio_renderer(),
                  Initialize(mocks_->audio_decoder(), _, _))
          .WillOnce(Invoke(&RunFilterCallback3));
    }
    EXPECT_CALL(*mocks_->audio_renderer(), SetPlaybackRate(0.0f));
    EXPECT_CALL(*mocks_->audio_renderer(), SetVolume(1.0f));
    EXPECT_CALL(*mocks_->audio_renderer(), Seek(base::TimeDelta(), _))
        .WillOnce(Invoke(&RunFilterStatusCB));
    EXPECT_CALL(*mocks_->audio_renderer(), Stop(_))
        .WillOnce(Invoke(&RunStopFilterCallback));
  }

  // Sets up expectations on the callback and initializes the pipeline.  Called
  // after tests have set expectations any filters they wish to use.
  void InitializePipeline() {
    InitializePipeline(PIPELINE_OK);
  }
  // Most tests can expect the |filter_collection|'s |build_status| to get
  // reflected in |Start()|'s argument.
  void InitializePipeline(PipelineStatus start_status) {
    InitializePipeline(start_status, start_status);
  }
  // But some tests require different statuses in build & Start.
  void InitializePipeline(PipelineStatus build_status,
                          PipelineStatus start_status) {
    // Expect an initialization callback.
    EXPECT_CALL(callbacks_, OnStart(start_status));

    pipeline_->Start(mocks_->filter_collection(true,
                                               true,
                                               true,
                                               build_status),
                     "",
                     base::Bind(&CallbackHelper::OnStart,
                                base::Unretained(&callbacks_)));

    message_loop_.RunAllPending();
  }

  void CreateAudioStream() {
    audio_stream_ = CreateStream(DemuxerStream::AUDIO);
  }

  void CreateVideoStream() {
    video_stream_ = CreateStream(DemuxerStream::VIDEO);
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
        .WillOnce(Invoke(&RunFilterStatusCB));

    if (audio_stream_) {
      EXPECT_CALL(*mocks_->audio_decoder(), Seek(seek_time, _))
          .WillOnce(Invoke(&RunFilterStatusCB));
      EXPECT_CALL(*mocks_->audio_renderer(), Seek(seek_time, _))
          .WillOnce(Invoke(&RunFilterStatusCB));
    }

    if (video_stream_) {
      EXPECT_CALL(*mocks_->video_decoder(), Seek(seek_time, _))
          .WillOnce(Invoke(&RunFilterStatusCB));
      EXPECT_CALL(*mocks_->video_renderer(), Seek(seek_time, _))
          .WillOnce(Invoke(&RunFilterStatusCB));
    }

    // We expect a successful seek callback.
    EXPECT_CALL(callbacks_, OnSeek(PIPELINE_OK));
  }

  void DoSeek(const base::TimeDelta& seek_time) {
    pipeline_->Seek(seek_time,
                    base::Bind(&CallbackHelper::OnSeek,
                               base::Unretained(&callbacks_)));

    // We expect the time to be updated only after the seek has completed.
    EXPECT_NE(seek_time, pipeline_->GetCurrentTime());
    message_loop_.RunAllPending();
    EXPECT_EQ(seek_time, pipeline_->GetCurrentTime());
  }

  // Fixture members.
  StrictMock<CallbackHelper> callbacks_;
  MessageLoop message_loop_;
  scoped_refptr<PipelineImpl> pipeline_;
  scoped_ptr<media::MockFilterCollection> mocks_;
  scoped_refptr<StrictMock<MockDemuxerStream> > audio_stream_;
  scoped_refptr<StrictMock<MockDemuxerStream> > video_stream_;

 private:
  DISALLOW_COPY_AND_ASSIGN(PipelineImplTest);
};

// Test that playback controls methods no-op when the pipeline hasn't been
// started.
TEST_F(PipelineImplTest, NotStarted) {
  const base::TimeDelta kZero;

  // StrictMock<> will ensure these never get called, and valgrind will
  // make sure the callbacks are instantly deleted.
  pipeline_->Stop(base::Bind(&CallbackHelper::OnStop,
                             base::Unretained(&callbacks_)));
  pipeline_->Seek(kZero,
                  base::Bind(&CallbackHelper::OnSeek,
                             base::Unretained(&callbacks_)));

  EXPECT_FALSE(pipeline_->IsRunning());
  EXPECT_FALSE(pipeline_->IsInitialized());
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

  EXPECT_TRUE(kZero == pipeline_->GetCurrentTime());
  EXPECT_TRUE(kZero == pipeline_->GetBufferedTime());
  EXPECT_TRUE(kZero == pipeline_->GetMediaDuration());

  EXPECT_EQ(0, pipeline_->GetBufferedBytes());
  EXPECT_EQ(0, pipeline_->GetTotalBytes());

  // Should always get set to zero.
  gfx::Size size(1, 1);
  pipeline_->GetNaturalVideoSize(&size);
  EXPECT_EQ(0, size.width());
  EXPECT_EQ(0, size.height());
}

TEST_F(PipelineImplTest, NeverInitializes) {
  // This test hangs during initialization by never calling
  // InitializationComplete().  StrictMock<> will ensure that the callback is
  // never executed.
  pipeline_->Start(mocks_->filter_collection(false,
                                             false,
                                             true,
                                             PIPELINE_OK),
                   "",
                   base::Bind(&CallbackHelper::OnStart,
                              base::Unretained(&callbacks_)));
  message_loop_.RunAllPending();

  EXPECT_FALSE(pipeline_->IsInitialized());

  // Because our callback will get executed when the test tears down, we'll
  // verify that nothing has been called, then set our expectation for the call
  // made during tear down.
  Mock::VerifyAndClear(&callbacks_);
  EXPECT_CALL(callbacks_, OnStart(PIPELINE_OK));
}

TEST_F(PipelineImplTest, RequiredFilterMissing) {
  EXPECT_CALL(callbacks_, OnError(PIPELINE_ERROR_REQUIRED_FILTER_MISSING));

  // Sets up expectations on the callback and initializes the pipeline.  Called
  // after tests have set expectations any filters they wish to use.
  // Expect an initialization callback.
  EXPECT_CALL(callbacks_, OnStart(PIPELINE_ERROR_REQUIRED_FILTER_MISSING));

  // Create a filter collection with missing filter.
  FilterCollection* collection =
      mocks_->filter_collection(false,
                                true,
                                true,
                                PIPELINE_ERROR_REQUIRED_FILTER_MISSING);
  pipeline_->Start(collection, "",
                   base::Bind(&CallbackHelper::OnStart,
                              base::Unretained(&callbacks_)));
  message_loop_.RunAllPending();

  EXPECT_FALSE(pipeline_->IsInitialized());
}

TEST_F(PipelineImplTest, URLNotFound) {
  // TODO(acolwell,fischman): Since OnStart() is getting called with an error
  // code already, OnError() doesn't also need to get called.  Fix the pipeline
  // (and it's consumers!) so that OnError doesn't need to be called after
  // another callback has already reported the error.  Same applies to NoStreams
  // below.
  EXPECT_CALL(callbacks_, OnError(PIPELINE_ERROR_URL_NOT_FOUND));
  InitializePipeline(PIPELINE_ERROR_URL_NOT_FOUND);
  EXPECT_FALSE(pipeline_->IsInitialized());
}

TEST_F(PipelineImplTest, NoStreams) {
  // Manually set these expectations because SetPlaybackRate() is not called if
  // we cannot fully initialize the pipeline.
  EXPECT_CALL(*mocks_->demuxer(), Stop(_))
      .WillOnce(Invoke(&RunStopFilterCallback));
  // TODO(acolwell,fischman): see TODO in URLNotFound above.
  EXPECT_CALL(callbacks_, OnError(PIPELINE_ERROR_COULD_NOT_RENDER));

  InitializePipeline(PIPELINE_OK, PIPELINE_ERROR_COULD_NOT_RENDER);
  EXPECT_FALSE(pipeline_->IsInitialized());
}

#if defined(OS_MACOSX)
// Crashes on OS X - http://crbug.com/105234
#define MAYBE_AudioStream DISABLED_AudioStream
#else
#define MAYBE_AudioStream AudioStream
#endif
TEST_F(PipelineImplTest, MAYBE_AudioStream) {
  CreateAudioStream();
  MockDemuxerStreamVector streams;
  streams.push_back(audio_stream());

  InitializeDemuxer(&streams, base::TimeDelta());
  InitializeAudioDecoder(audio_stream());
  InitializeAudioRenderer();

  InitializePipeline(PIPELINE_OK);
  EXPECT_TRUE(pipeline_->IsInitialized());
  EXPECT_TRUE(pipeline_->HasAudio());
  EXPECT_FALSE(pipeline_->HasVideo());
}

TEST_F(PipelineImplTest, VideoStream) {
  CreateVideoStream();
  MockDemuxerStreamVector streams;
  streams.push_back(video_stream());

  InitializeDemuxer(&streams, base::TimeDelta());
  InitializeVideoDecoder(video_stream());
  InitializeVideoRenderer();

  InitializePipeline(PIPELINE_OK);
  EXPECT_TRUE(pipeline_->IsInitialized());
  EXPECT_FALSE(pipeline_->HasAudio());
  EXPECT_TRUE(pipeline_->HasVideo());
}

TEST_F(PipelineImplTest, AudioVideoStream) {
  CreateAudioStream();
  CreateVideoStream();
  MockDemuxerStreamVector streams;
  streams.push_back(audio_stream());
  streams.push_back(video_stream());

  InitializeDemuxer(&streams, base::TimeDelta());
  InitializeAudioDecoder(audio_stream());
  InitializeAudioRenderer();
  InitializeVideoDecoder(video_stream());
  InitializeVideoRenderer();

  InitializePipeline(PIPELINE_OK);
  EXPECT_TRUE(pipeline_->IsInitialized());
  EXPECT_TRUE(pipeline_->HasAudio());
  EXPECT_TRUE(pipeline_->HasVideo());
}

TEST_F(PipelineImplTest, Seek) {
  CreateAudioStream();
  CreateVideoStream();
  MockDemuxerStreamVector streams;
  streams.push_back(audio_stream());
  streams.push_back(video_stream());

  InitializeDemuxer(&streams, base::TimeDelta::FromSeconds(3000));
  InitializeAudioDecoder(audio_stream());
  InitializeAudioRenderer();
  InitializeVideoDecoder(video_stream());
  InitializeVideoRenderer();

  // Every filter should receive a call to Seek().
  base::TimeDelta expected = base::TimeDelta::FromSeconds(2000);
  ExpectSeek(expected);

  // Initialize then seek!
  InitializePipeline(PIPELINE_OK);
  DoSeek(expected);
}

TEST_F(PipelineImplTest, SetVolume) {
  CreateAudioStream();
  MockDemuxerStreamVector streams;
  streams.push_back(audio_stream());

  InitializeDemuxer(&streams, base::TimeDelta());
  InitializeAudioDecoder(audio_stream());
  InitializeAudioRenderer();

  // The audio renderer should receive a call to SetVolume().
  float expected = 0.5f;
  EXPECT_CALL(*mocks_->audio_renderer(), SetVolume(expected));

  // Initialize then set volume!
  InitializePipeline(PIPELINE_OK);
  pipeline_->SetVolume(expected);
}

TEST_F(PipelineImplTest, Properties) {
  CreateVideoStream();
  MockDemuxerStreamVector streams;
  streams.push_back(video_stream());

  const base::TimeDelta kDuration = base::TimeDelta::FromSeconds(100);
  InitializeDemuxer(&streams, kDuration);
  InitializeVideoDecoder(video_stream());
  InitializeVideoRenderer();

  InitializePipeline(PIPELINE_OK);
  EXPECT_TRUE(pipeline_->IsInitialized());
  EXPECT_EQ(kDuration.ToInternalValue(),
            pipeline_->GetMediaDuration().ToInternalValue());
  EXPECT_EQ(kTotalBytes, pipeline_->GetTotalBytes());
  EXPECT_EQ(kBufferedBytes, pipeline_->GetBufferedBytes());

  // Because kTotalBytes and kBufferedBytes are equal to each other,
  // the entire video should be buffered.
  EXPECT_EQ(kDuration.ToInternalValue(),
            pipeline_->GetBufferedTime().ToInternalValue());
}

TEST_F(PipelineImplTest, GetBufferedTime) {
  CreateVideoStream();
  MockDemuxerStreamVector streams;
  streams.push_back(video_stream());

  const base::TimeDelta kDuration = base::TimeDelta::FromSeconds(100);
  InitializeDemuxer(&streams, kDuration);
  InitializeVideoDecoder(video_stream());
  InitializeVideoRenderer();

  InitializePipeline(PIPELINE_OK);
  EXPECT_TRUE(pipeline_->IsInitialized());

  // TODO(vrk): The following mini-test cases are order-dependent, and should
  // probably be separated into independent test cases.

  // Buffered time is 0 if no bytes are buffered.
  pipeline_->SetBufferedBytes(0);
  EXPECT_EQ(0, pipeline_->GetBufferedTime().ToInternalValue());

  // We should return buffered_time_ if it is set, valid and less than
  // the current time.
  const base::TimeDelta buffered = base::TimeDelta::FromSeconds(10);
  pipeline_->SetBufferedTime(buffered);
  EXPECT_EQ(buffered.ToInternalValue(),
            pipeline_->GetBufferedTime().ToInternalValue());

  // Test the case where the current time is beyond the buffered time.
  base::TimeDelta kSeekTime = buffered + base::TimeDelta::FromSeconds(5);
  ExpectSeek(kSeekTime);
  DoSeek(kSeekTime);

  // Verify that buffered time is equal to the current time.
  EXPECT_EQ(kSeekTime, pipeline_->GetCurrentTime());
  EXPECT_EQ(kSeekTime, pipeline_->GetBufferedTime());

  // Clear buffered time.
  pipeline_->SetBufferedTime(base::TimeDelta());

  double time_percent =
      static_cast<double>(pipeline_->GetCurrentTime().ToInternalValue()) /
      kDuration.ToInternalValue();

  int estimated_bytes = static_cast<int>(time_percent * kTotalBytes);

  // Test VBR case where bytes have been consumed slower than the average rate.
  pipeline_->SetBufferedBytes(estimated_bytes - 10);
  EXPECT_EQ(pipeline_->GetCurrentTime(), pipeline_->GetBufferedTime());

  // Test VBR case where the bytes have been consumed faster than the average
  // rate.
  pipeline_->SetBufferedBytes(estimated_bytes + 10);
  EXPECT_LT(pipeline_->GetCurrentTime(), pipeline_->GetBufferedTime());

  // If media has been fully received, we should return the duration
  // of the media.
  pipeline_->SetBufferedBytes(kTotalBytes);
  EXPECT_EQ(kDuration.ToInternalValue(),
            pipeline_->GetBufferedTime().ToInternalValue());

  // If media is loaded, we should return duration of media.
  pipeline_->SetLoaded(true);
  EXPECT_EQ(kDuration.ToInternalValue(),
            pipeline_->GetBufferedTime().ToInternalValue());
}

TEST_F(PipelineImplTest, DisableAudioRenderer) {
  CreateAudioStream();
  CreateVideoStream();
  MockDemuxerStreamVector streams;
  streams.push_back(audio_stream());
  streams.push_back(video_stream());

  InitializeDemuxer(&streams, base::TimeDelta());
  InitializeAudioDecoder(audio_stream());
  InitializeAudioRenderer();
  InitializeVideoDecoder(video_stream());
  InitializeVideoRenderer();

  InitializePipeline(PIPELINE_OK);
  EXPECT_TRUE(pipeline_->IsInitialized());
  EXPECT_TRUE(pipeline_->HasAudio());
  EXPECT_TRUE(pipeline_->HasVideo());

  EXPECT_CALL(*mocks_->audio_renderer(), SetPlaybackRate(1.0f))
      .WillOnce(DisableAudioRenderer(mocks_->audio_renderer()));
  EXPECT_CALL(*mocks_->demuxer(),
              OnAudioRendererDisabled());
  EXPECT_CALL(*mocks_->audio_decoder(),
              OnAudioRendererDisabled());
  EXPECT_CALL(*mocks_->audio_renderer(),
              OnAudioRendererDisabled());
  EXPECT_CALL(*mocks_->video_decoder(),
              OnAudioRendererDisabled());
  EXPECT_CALL(*mocks_->video_renderer(),
              OnAudioRendererDisabled());

  mocks_->audio_renderer()->SetPlaybackRate(1.0f);

  // Verify that ended event is fired when video ends.
  EXPECT_CALL(*mocks_->video_renderer(), HasEnded())
      .WillOnce(Return(true));
  EXPECT_CALL(callbacks_, OnEnded(PIPELINE_OK));
  FilterHost* host = pipeline_;
  host->NotifyEnded();
}

TEST_F(PipelineImplTest, DisableAudioRendererDuringInit) {
  CreateAudioStream();
  CreateVideoStream();
  MockDemuxerStreamVector streams;
  streams.push_back(audio_stream());
  streams.push_back(video_stream());

  InitializeDemuxer(&streams, base::TimeDelta());
  InitializeAudioDecoder(audio_stream());
  InitializeAudioRenderer(true);
  InitializeVideoDecoder(video_stream());
  InitializeVideoRenderer();

  EXPECT_CALL(*mocks_->demuxer(),
              OnAudioRendererDisabled());
  EXPECT_CALL(*mocks_->audio_decoder(),
              OnAudioRendererDisabled());
  EXPECT_CALL(*mocks_->audio_renderer(),
              OnAudioRendererDisabled());
  EXPECT_CALL(*mocks_->video_decoder(),
              OnAudioRendererDisabled());
  EXPECT_CALL(*mocks_->video_renderer(),
              OnAudioRendererDisabled());

  InitializePipeline(PIPELINE_OK);
  EXPECT_TRUE(pipeline_->IsInitialized());
  EXPECT_FALSE(pipeline_->HasAudio());
  EXPECT_TRUE(pipeline_->HasVideo());

  // Verify that ended event is fired when video ends.
  EXPECT_CALL(*mocks_->video_renderer(), HasEnded())
      .WillOnce(Return(true));
  EXPECT_CALL(callbacks_, OnEnded(PIPELINE_OK));
  FilterHost* host = pipeline_;
  host->NotifyEnded();
}

TEST_F(PipelineImplTest, EndedCallback) {
  CreateAudioStream();
  CreateVideoStream();
  MockDemuxerStreamVector streams;
  streams.push_back(audio_stream());
  streams.push_back(video_stream());

  InitializeDemuxer(&streams, base::TimeDelta());
  InitializeAudioDecoder(audio_stream());
  InitializeAudioRenderer();
  InitializeVideoDecoder(video_stream());
  InitializeVideoRenderer();
  InitializePipeline(PIPELINE_OK);

  // For convenience to simulate filters calling the methods.
  FilterHost* host = pipeline_;

  // Due to short circuit evaluation we only need to test a subset of cases.
  InSequence s;
  EXPECT_CALL(*mocks_->audio_renderer(), HasEnded())
      .WillOnce(Return(false));
  host->NotifyEnded();

  EXPECT_CALL(*mocks_->audio_renderer(), HasEnded())
      .WillOnce(Return(true));
  EXPECT_CALL(*mocks_->video_renderer(), HasEnded())
      .WillOnce(Return(false));
  host->NotifyEnded();

  EXPECT_CALL(*mocks_->audio_renderer(), HasEnded())
      .WillOnce(Return(true));
  EXPECT_CALL(*mocks_->video_renderer(), HasEnded())
      .WillOnce(Return(true));
  EXPECT_CALL(callbacks_, OnEnded(PIPELINE_OK));
  host->NotifyEnded();
}

// Static function & time variable used to simulate changes in wallclock time.
static int64 g_static_clock_time;
static base::Time StaticClockFunction() {
  return base::Time::FromInternalValue(g_static_clock_time);
}

TEST_F(PipelineImplTest, AudioStreamShorterThanVideo) {
  base::TimeDelta duration = base::TimeDelta::FromSeconds(10);

  CreateAudioStream();
  CreateVideoStream();
  MockDemuxerStreamVector streams;
  streams.push_back(audio_stream());
  streams.push_back(video_stream());

  InitializeDemuxer(&streams, duration);
  InitializeAudioDecoder(audio_stream());
  InitializeAudioRenderer();
  InitializeVideoDecoder(video_stream());
  InitializeVideoRenderer();
  InitializePipeline(PIPELINE_OK);

  // For convenience to simulate filters calling the methods.
  FilterHost* host = pipeline_;

  // Replace the clock so we can simulate wallclock time advancing w/o using
  // Sleep().
  pipeline_->SetClockForTesting(new Clock(&StaticClockFunction));

  EXPECT_EQ(0, host->GetTime().ToInternalValue());

  float playback_rate = 1.0f;
  EXPECT_CALL(*mocks_->demuxer(), SetPlaybackRate(playback_rate));
  EXPECT_CALL(*mocks_->video_decoder(), SetPlaybackRate(playback_rate));
  EXPECT_CALL(*mocks_->audio_decoder(), SetPlaybackRate(playback_rate));
  EXPECT_CALL(*mocks_->video_renderer(), SetPlaybackRate(playback_rate));
  EXPECT_CALL(*mocks_->audio_renderer(), SetPlaybackRate(playback_rate));
  pipeline_->SetPlaybackRate(playback_rate);
  message_loop_.RunAllPending();

  InSequence s;

  // Verify that the clock doesn't advance since it hasn't been started by
  // a time update from the audio stream.
  int64 start_time = host->GetTime().ToInternalValue();
  g_static_clock_time +=
      base::TimeDelta::FromMilliseconds(100).ToInternalValue();
  EXPECT_EQ(host->GetTime().ToInternalValue(), start_time);

  // Signal end of audio stream.
  EXPECT_CALL(*mocks_->audio_renderer(), HasEnded())
      .WillOnce(Return(true));
  EXPECT_CALL(*mocks_->video_renderer(), HasEnded())
      .WillOnce(Return(false));
  host->NotifyEnded();
  message_loop_.RunAllPending();

  // Verify that the clock advances.
  start_time = host->GetTime().ToInternalValue();
  g_static_clock_time +=
      base::TimeDelta::FromMilliseconds(100).ToInternalValue();
  EXPECT_GT(host->GetTime().ToInternalValue(), start_time);

  // Signal end of video stream and make sure OnEnded() callback occurs.
  EXPECT_CALL(*mocks_->audio_renderer(), HasEnded())
      .WillOnce(Return(true));
  EXPECT_CALL(*mocks_->video_renderer(), HasEnded())
      .WillOnce(Return(true));
  EXPECT_CALL(callbacks_, OnEnded(PIPELINE_OK));
  host->NotifyEnded();
}

void SendReadErrorToCB(::testing::Unused, const FilterStatusCB& cb) {
  cb.Run(PIPELINE_ERROR_READ);
}

TEST_F(PipelineImplTest, ErrorDuringSeek) {
  CreateAudioStream();
  MockDemuxerStreamVector streams;
  streams.push_back(audio_stream());

  InitializeDemuxer(&streams, base::TimeDelta::FromSeconds(10));
  InitializeAudioDecoder(audio_stream());
  InitializeAudioRenderer();
  InitializePipeline(PIPELINE_OK);

  float playback_rate = 1.0f;
  EXPECT_CALL(*mocks_->demuxer(), SetPlaybackRate(playback_rate));
  EXPECT_CALL(*mocks_->audio_decoder(), SetPlaybackRate(playback_rate));
  EXPECT_CALL(*mocks_->audio_renderer(), SetPlaybackRate(playback_rate));
  pipeline_->SetPlaybackRate(playback_rate);
  message_loop_.RunAllPending();

  InSequence s;

  base::TimeDelta seek_time = base::TimeDelta::FromSeconds(5);

  EXPECT_CALL(*mocks_->demuxer(), Seek(seek_time, _))
      .WillOnce(Invoke(&SendReadErrorToCB));

  pipeline_->Seek(seek_time,base::Bind(&CallbackHelper::OnSeek,
                                       base::Unretained(&callbacks_)));
  EXPECT_CALL(callbacks_, OnSeek(PIPELINE_ERROR_READ));
  EXPECT_CALL(callbacks_, OnError(PIPELINE_ERROR_READ));
  message_loop_.RunAllPending();
}

// Invoked function OnError. This asserts that the pipeline does not enqueue
// non-teardown related tasks while tearing down.
static void TestNoCallsAfterError(
    PipelineImpl* pipeline, MessageLoop* message_loop,
    PipelineStatus /* status */) {
  CHECK(pipeline);
  CHECK(message_loop);

  // When we get to this stage, the message loop should be empty.
  message_loop->AssertIdle();

  // Make calls on pipeline after error has occurred.
  pipeline->SetPlaybackRate(0.5f);
  pipeline->SetVolume(0.5f);
  pipeline->SetPreload(AUTO);

  // No additional tasks should be queued as a result of these calls.
  message_loop->AssertIdle();
}

TEST_F(PipelineImplTest, NoMessageDuringTearDownFromError) {
  CreateAudioStream();
  MockDemuxerStreamVector streams;
  streams.push_back(audio_stream());

  InitializeDemuxer(&streams, base::TimeDelta::FromSeconds(10));
  InitializeAudioDecoder(audio_stream());
  InitializeAudioRenderer();
  InitializePipeline(PIPELINE_OK);

  // Trigger additional requests on the pipeline during tear down from error.
  base::Callback<void(PipelineStatus)> cb = base::Bind(
      &TestNoCallsAfterError, pipeline_, &message_loop_);
  ON_CALL(callbacks_, OnError(_))
      .WillByDefault(Invoke(&cb, &base::Callback<void(PipelineStatus)>::Run));

  InSequence s;

  base::TimeDelta seek_time = base::TimeDelta::FromSeconds(5);

  EXPECT_CALL(*mocks_->demuxer(), Seek(seek_time, _))
      .WillOnce(Invoke(&SendReadErrorToCB));

  pipeline_->Seek(seek_time,base::Bind(&CallbackHelper::OnSeek,
                                       base::Unretained(&callbacks_)));
  EXPECT_CALL(callbacks_, OnSeek(PIPELINE_ERROR_READ));
  EXPECT_CALL(callbacks_, OnError(PIPELINE_ERROR_READ));
  message_loop_.RunAllPending();
}

TEST_F(PipelineImplTest, StartTimeIsZero) {
  CreateVideoStream();
  MockDemuxerStreamVector streams;
  streams.push_back(video_stream());

  const base::TimeDelta kDuration = base::TimeDelta::FromSeconds(100);
  InitializeDemuxer(&streams, kDuration);
  InitializeVideoDecoder(video_stream());
  InitializeVideoRenderer();

  InitializePipeline(PIPELINE_OK);
  EXPECT_TRUE(pipeline_->IsInitialized());
  EXPECT_FALSE(pipeline_->HasAudio());
  EXPECT_TRUE(pipeline_->HasVideo());

  EXPECT_EQ(base::TimeDelta(), pipeline_->GetCurrentTime());
}

TEST_F(PipelineImplTest, StartTimeIsNonZero) {
  const base::TimeDelta kStartTime = base::TimeDelta::FromSeconds(4);
  const base::TimeDelta kDuration = base::TimeDelta::FromSeconds(100);

  EXPECT_CALL(*mocks_->demuxer(), GetStartTime())
      .WillRepeatedly(Return(kStartTime));

  CreateVideoStream();
  MockDemuxerStreamVector streams;
  streams.push_back(video_stream());

  InitializeDemuxer(&streams, kDuration);
  InitializeVideoDecoder(video_stream());
  InitializeVideoRenderer();

  InitializePipeline(PIPELINE_OK);
  EXPECT_TRUE(pipeline_->IsInitialized());
  EXPECT_FALSE(pipeline_->HasAudio());
  EXPECT_TRUE(pipeline_->HasVideo());

  EXPECT_EQ(kStartTime, pipeline_->GetCurrentTime());
}

class FlexibleCallbackRunner : public base::DelegateSimpleThread::Delegate {
 public:
  FlexibleCallbackRunner(int delayInMs, PipelineStatus status,
                         const PipelineStatusCB& callback)
      : delayInMs_(delayInMs), status_(status), callback_(callback) {
    if (delayInMs_ < 0) {
      callback_.Run(status_);
      return;
    }
  }
  virtual void Run() {
    if (delayInMs_ < 0) return;
    base::PlatformThread::Sleep(delayInMs_);
    callback_.Run(status_);
  }

 private:
  int delayInMs_;
  PipelineStatus status_;
  PipelineStatusCB callback_;
};

void TestPipelineStatusNotification(int delayInMs) {
  PipelineStatusNotification note;
  // Arbitrary error value we expect to fish out of the notification after the
  // callback is fired.
  const PipelineStatus expected_error = PIPELINE_ERROR_URL_NOT_FOUND;
  FlexibleCallbackRunner runner(delayInMs, expected_error, note.Callback());
  base::DelegateSimpleThread thread(&runner, "FlexibleCallbackRunner");
  thread.Start();
  note.Wait();
  EXPECT_EQ(note.status(), expected_error);
  thread.Join();
}

// Test that in-line callback (same thread, no yield) works correctly.
TEST(PipelineStatusNotificationTest, InlineCallback) {
  TestPipelineStatusNotification(-1);
}

// Test that different-thread, no-delay callback works correctly.
TEST(PipelineStatusNotificationTest, ImmediateCallback) {
  TestPipelineStatusNotification(0);
}

// Test that different-thread, some-delay callback (the expected common case)
// works correctly.
TEST(PipelineStatusNotificationTest, DelayedCallback) {
  TestPipelineStatusNotification(20);
}

}  // namespace media
