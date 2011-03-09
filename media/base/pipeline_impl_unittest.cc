// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/callback.h"
#include "base/stl_util-inl.h"
#include "media/base/pipeline_impl.h"
#include "media/base/media_format.h"
#include "media/base/filters.h"
#include "media/base/filter_host.h"
#include "media/base/mock_callback.h"
#include "media/base/mock_filters.h"
#include "testing/gtest/include/gtest/gtest.h"

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

  MOCK_METHOD0(OnStart, void());
  MOCK_METHOD0(OnSeek, void());
  MOCK_METHOD0(OnStop, void());
  MOCK_METHOD0(OnEnded, void());
  MOCK_METHOD0(OnError, void());

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
      : pipeline_(new PipelineImpl(&message_loop_)) {
    pipeline_->Init(
        NewCallback(reinterpret_cast<CallbackHelper*>(&callbacks_),
                    &CallbackHelper::OnEnded),
        NewCallback(reinterpret_cast<CallbackHelper*>(&callbacks_),
                    &CallbackHelper::OnError),
        NULL);
    mocks_.reset(new MockFilterCollection());
  }

  virtual ~PipelineImplTest() {
    if (!pipeline_->IsRunning()) {
      return;
    }

    // Expect a stop callback if we were started.
    EXPECT_CALL(callbacks_, OnStop());
    pipeline_->Stop(NewCallback(reinterpret_cast<CallbackHelper*>(&callbacks_),
                                &CallbackHelper::OnStop));
    message_loop_.RunAllPending();

    mocks_.reset();
  }

 protected:
  // Sets up expectations to allow the data source to initialize.
  void InitializeDataSource() {
    EXPECT_CALL(*mocks_->data_source(), Initialize("", NotNull()))
        .WillOnce(DoAll(SetTotalBytes(mocks_->data_source(), kTotalBytes),
                        SetBufferedBytes(mocks_->data_source(), kBufferedBytes),
                        Invoke(&RunFilterCallback)));
    EXPECT_CALL(*mocks_->data_source(), SetPlaybackRate(0.0f));
    EXPECT_CALL(*mocks_->data_source(), IsUrlSupported(_))
        .WillOnce(Return(true));
    EXPECT_CALL(*mocks_->data_source(), Seek(base::TimeDelta(), NotNull()))
        .WillOnce(Invoke(&RunFilterCallback));
    EXPECT_CALL(*mocks_->data_source(), Stop(NotNull()))
        .WillOnce(Invoke(&RunStopFilterCallback));
  }

  // Sets up expectations to allow the demuxer to initialize.
  typedef std::vector<MockDemuxerStream*> MockDemuxerStreamVector;
  void InitializeDemuxer(MockDemuxerStreamVector* streams,
                         const base::TimeDelta& duration) {
    EXPECT_CALL(*mocks_->demuxer(),
                Initialize(mocks_->data_source(), NotNull()))
        .WillOnce(DoAll(SetDuration(mocks_->data_source(), duration),
                        Invoke(&RunFilterCallback)));
    EXPECT_CALL(*mocks_->demuxer(), GetNumberOfStreams())
        .WillRepeatedly(Return(streams->size()));
    EXPECT_CALL(*mocks_->demuxer(), SetPlaybackRate(0.0f));
    EXPECT_CALL(*mocks_->demuxer(), Seek(base::TimeDelta(), NotNull()))
        .WillOnce(Invoke(&RunFilterCallback));
    EXPECT_CALL(*mocks_->demuxer(), Stop(NotNull()))
        .WillOnce(Invoke(&RunStopFilterCallback));

    // Configure the demuxer to return the streams.
    for (size_t i = 0; i < streams->size(); ++i) {
      scoped_refptr<DemuxerStream> stream((*streams)[i]);
      EXPECT_CALL(*mocks_->demuxer(), GetStream(i))
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
                Initialize(stream, NotNull(), NotNull()))
        .WillOnce(DoAll(Invoke(&RunFilterCallback3), DeleteArg<2>()));
    EXPECT_CALL(*mocks_->video_decoder(), SetPlaybackRate(0.0f));
    EXPECT_CALL(*mocks_->video_decoder(), Seek(base::TimeDelta(), NotNull()))
        .WillOnce(Invoke(&RunFilterCallback));
    EXPECT_CALL(*mocks_->video_decoder(), Stop(NotNull()))
        .WillOnce(Invoke(&RunStopFilterCallback));
  }

  // Sets up expectations to allow the audio decoder to initialize.
  void InitializeAudioDecoder(MockDemuxerStream* stream) {
    EXPECT_CALL(*mocks_->audio_decoder(),
                Initialize(stream, NotNull(), NotNull()))
        .WillOnce(DoAll(Invoke(&RunFilterCallback3), DeleteArg<2>()));
    EXPECT_CALL(*mocks_->audio_decoder(), SetPlaybackRate(0.0f));
    EXPECT_CALL(*mocks_->audio_decoder(), Seek(base::TimeDelta(), NotNull()))
        .WillOnce(Invoke(&RunFilterCallback));
    EXPECT_CALL(*mocks_->audio_decoder(), Stop(NotNull()))
        .WillOnce(Invoke(&RunStopFilterCallback));
  }

  // Sets up expectations to allow the video renderer to initialize.
  void InitializeVideoRenderer() {
    EXPECT_CALL(*mocks_->video_renderer(),
                Initialize(mocks_->video_decoder(), NotNull(), NotNull()))
        .WillOnce(DoAll(Invoke(&RunFilterCallback3), DeleteArg<2>()));
    EXPECT_CALL(*mocks_->video_renderer(), SetPlaybackRate(0.0f));
    EXPECT_CALL(*mocks_->video_renderer(), Seek(base::TimeDelta(), NotNull()))
        .WillOnce(Invoke(&RunFilterCallback));
    EXPECT_CALL(*mocks_->video_renderer(), Stop(NotNull()))
        .WillOnce(Invoke(&RunStopFilterCallback));
  }

  // Sets up expectations to allow the audio renderer to initialize.
  void InitializeAudioRenderer(bool disable_after_init_callback = false) {
    if (disable_after_init_callback) {
      EXPECT_CALL(*mocks_->audio_renderer(),
                  Initialize(mocks_->audio_decoder(), NotNull()))
          .WillOnce(DoAll(Invoke(&RunFilterCallback),
                          DisableAudioRenderer(mocks_->audio_renderer())));
    } else {
      EXPECT_CALL(*mocks_->audio_renderer(),
                  Initialize(mocks_->audio_decoder(), NotNull()))
          .WillOnce(Invoke(&RunFilterCallback));
    }
    EXPECT_CALL(*mocks_->audio_renderer(), SetPlaybackRate(0.0f));
    EXPECT_CALL(*mocks_->audio_renderer(), SetVolume(1.0f));
    EXPECT_CALL(*mocks_->audio_renderer(), Seek(base::TimeDelta(), NotNull()))
        .WillOnce(Invoke(&RunFilterCallback));
    EXPECT_CALL(*mocks_->audio_renderer(), Stop(NotNull()))
        .WillOnce(Invoke(&RunStopFilterCallback));
  }

  // Sets up expectations on the callback and initializes the pipeline.  Called
  // after tests have set expectations any filters they wish to use.
  void InitializePipeline() {
    // Expect an initialization callback.
    EXPECT_CALL(callbacks_, OnStart());
    pipeline_->Start(mocks_->filter_collection(), "",
                     NewCallback(reinterpret_cast<CallbackHelper*>(&callbacks_),
                                 &CallbackHelper::OnStart));
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
    EXPECT_CALL(*mocks_->data_source(), Seek(seek_time, NotNull()))
        .WillOnce(Invoke(&RunFilterCallback));
    EXPECT_CALL(*mocks_->demuxer(), Seek(seek_time, NotNull()))
        .WillOnce(Invoke(&RunFilterCallback));

    if (audio_stream_) {
      EXPECT_CALL(*mocks_->audio_decoder(), Seek(seek_time, NotNull()))
          .WillOnce(Invoke(&RunFilterCallback));
      EXPECT_CALL(*mocks_->audio_renderer(), Seek(seek_time, NotNull()))
          .WillOnce(Invoke(&RunFilterCallback));
    }

    if (video_stream_) {
      EXPECT_CALL(*mocks_->video_decoder(), Seek(seek_time, NotNull()))
          .WillOnce(Invoke(&RunFilterCallback));
      EXPECT_CALL(*mocks_->video_renderer(), Seek(seek_time, NotNull()))
          .WillOnce(Invoke(&RunFilterCallback));
    }

    // We expect a successful seek callback.
    EXPECT_CALL(callbacks_, OnSeek());

  }

  void DoSeek(const base::TimeDelta& seek_time) {
    pipeline_->Seek(seek_time,
                    NewCallback(reinterpret_cast<CallbackHelper*>(&callbacks_),
                                &CallbackHelper::OnSeek));

    // We expect the time to be updated only after the seek has completed.
    EXPECT_TRUE(seek_time != pipeline_->GetCurrentTime());
    message_loop_.RunAllPending();
    EXPECT_TRUE(seek_time == pipeline_->GetCurrentTime());
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

  // StrictMock<> will ensure these never get called, and valgrind/purify will
  // make sure the callbacks are instantly deleted.
  pipeline_->Stop(NewCallback(reinterpret_cast<CallbackHelper*>(&callbacks_),
                              &CallbackHelper::OnStop));
  pipeline_->Seek(kZero,
                  NewCallback(reinterpret_cast<CallbackHelper*>(&callbacks_),
                              &CallbackHelper::OnSeek));

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
  size_t width = 1u;
  size_t height = 1u;
  pipeline_->GetVideoSize(&width, &height);
  EXPECT_EQ(0u, width);
  EXPECT_EQ(0u, height);

  EXPECT_EQ(PIPELINE_OK, pipeline_->GetError());
}

TEST_F(PipelineImplTest, NeverInitializes) {
  EXPECT_CALL(*mocks_->data_source(), Initialize("", NotNull()))
      .WillOnce(Invoke(&DestroyFilterCallback));
  EXPECT_CALL(*mocks_->data_source(), IsUrlSupported(_))
      .WillOnce(Return(true));
  EXPECT_CALL(*mocks_->data_source(), Stop(NotNull()))
      .WillOnce(Invoke(&RunStopFilterCallback));

  // This test hangs during initialization by never calling
  // InitializationComplete().  StrictMock<> will ensure that the callback is
  // never executed.
  pipeline_->Start(mocks_->filter_collection(), "",
                   NewCallback(reinterpret_cast<CallbackHelper*>(&callbacks_),
                               &CallbackHelper::OnStart));
  message_loop_.RunAllPending();

  EXPECT_FALSE(pipeline_->IsInitialized());
  EXPECT_EQ(PIPELINE_OK, pipeline_->GetError());

  // Because our callback will get executed when the test tears down, we'll
  // verify that nothing has been called, then set our expectation for the call
  // made during tear down.
  Mock::VerifyAndClear(&callbacks_);
  EXPECT_CALL(callbacks_, OnStart());
}

TEST_F(PipelineImplTest, RequiredFilterMissing) {
  EXPECT_CALL(callbacks_, OnError());

  // Sets up expectations on the callback and initializes the pipeline.  Called
  // after tests have set expectations any filters they wish to use.
  // Expect an initialization callback.
  EXPECT_CALL(callbacks_, OnStart());

  // Create a filter collection with missing filter.
  FilterCollection* collection = mocks_->filter_collection(false);
  pipeline_->Start(collection, "",
                   NewCallback(reinterpret_cast<CallbackHelper*>(&callbacks_),
                               &CallbackHelper::OnStart));
  message_loop_.RunAllPending();

  EXPECT_FALSE(pipeline_->IsInitialized());
  EXPECT_EQ(PIPELINE_ERROR_REQUIRED_FILTER_MISSING,
            pipeline_->GetError());
}

TEST_F(PipelineImplTest, URLNotFound) {
  EXPECT_CALL(*mocks_->data_source(), Initialize("", NotNull()))
      .WillOnce(DoAll(SetError(mocks_->data_source(),
                               PIPELINE_ERROR_URL_NOT_FOUND),
                      Invoke(&RunFilterCallback)));
  EXPECT_CALL(*mocks_->data_source(), IsUrlSupported(_))
      .WillOnce(Return(true));
  EXPECT_CALL(callbacks_, OnError());
  EXPECT_CALL(*mocks_->data_source(), Stop(NotNull()))
      .WillOnce(Invoke(&RunStopFilterCallback));

  InitializePipeline();
  EXPECT_FALSE(pipeline_->IsInitialized());
  EXPECT_EQ(PIPELINE_ERROR_URL_NOT_FOUND, pipeline_->GetError());
}

TEST_F(PipelineImplTest, NoStreams) {
  // Manually set these expectations because SetPlaybackRate() is not called if
  // we cannot fully initialize the pipeline.
  EXPECT_CALL(*mocks_->data_source(), Initialize("", NotNull()))
      .WillOnce(Invoke(&RunFilterCallback));
  EXPECT_CALL(*mocks_->data_source(), IsUrlSupported(_))
      .WillOnce(Return(true));
  EXPECT_CALL(*mocks_->data_source(), Stop(NotNull()))
      .WillOnce(Invoke(&RunStopFilterCallback));

  EXPECT_CALL(*mocks_->demuxer(), Initialize(mocks_->data_source(), NotNull()))
      .WillOnce(Invoke(&RunFilterCallback));
  EXPECT_CALL(*mocks_->demuxer(), GetNumberOfStreams())
      .WillRepeatedly(Return(0));
  EXPECT_CALL(*mocks_->demuxer(), Stop(NotNull()))
      .WillOnce(Invoke(&RunStopFilterCallback));
  EXPECT_CALL(callbacks_, OnError());

  InitializePipeline();
  EXPECT_FALSE(pipeline_->IsInitialized());
  EXPECT_EQ(PIPELINE_ERROR_COULD_NOT_RENDER, pipeline_->GetError());
}

TEST_F(PipelineImplTest, AudioStream) {
  CreateAudioStream();
  MockDemuxerStreamVector streams;
  streams.push_back(audio_stream());

  InitializeDataSource();
  InitializeDemuxer(&streams, base::TimeDelta());
  InitializeAudioDecoder(audio_stream());
  InitializeAudioRenderer();

  InitializePipeline();
  EXPECT_TRUE(pipeline_->IsInitialized());
  EXPECT_EQ(PIPELINE_OK, pipeline_->GetError());
  EXPECT_TRUE(pipeline_->HasAudio());
  EXPECT_FALSE(pipeline_->HasVideo());
}

TEST_F(PipelineImplTest, VideoStream) {
  CreateVideoStream();
  MockDemuxerStreamVector streams;
  streams.push_back(video_stream());

  InitializeDataSource();
  InitializeDemuxer(&streams, base::TimeDelta());
  InitializeVideoDecoder(video_stream());
  InitializeVideoRenderer();

  InitializePipeline();
  EXPECT_TRUE(pipeline_->IsInitialized());
  EXPECT_EQ(PIPELINE_OK, pipeline_->GetError());
  EXPECT_FALSE(pipeline_->HasAudio());
  EXPECT_TRUE(pipeline_->HasVideo());
}

TEST_F(PipelineImplTest, AudioVideoStream) {
  CreateAudioStream();
  CreateVideoStream();
  MockDemuxerStreamVector streams;
  streams.push_back(audio_stream());
  streams.push_back(video_stream());

  InitializeDataSource();
  InitializeDemuxer(&streams, base::TimeDelta());
  InitializeAudioDecoder(audio_stream());
  InitializeAudioRenderer();
  InitializeVideoDecoder(video_stream());
  InitializeVideoRenderer();

  InitializePipeline();
  EXPECT_TRUE(pipeline_->IsInitialized());
  EXPECT_EQ(PIPELINE_OK, pipeline_->GetError());
  EXPECT_TRUE(pipeline_->HasAudio());
  EXPECT_TRUE(pipeline_->HasVideo());
}

TEST_F(PipelineImplTest, Seek) {
  CreateAudioStream();
  CreateVideoStream();
  MockDemuxerStreamVector streams;
  streams.push_back(audio_stream());
  streams.push_back(video_stream());

  InitializeDataSource();
  InitializeDemuxer(&streams, base::TimeDelta::FromSeconds(3000));
  InitializeAudioDecoder(audio_stream());
  InitializeAudioRenderer();
  InitializeVideoDecoder(video_stream());
  InitializeVideoRenderer();

  // Every filter should receive a call to Seek().
  base::TimeDelta expected = base::TimeDelta::FromSeconds(2000);
  ExpectSeek(expected);

  // Initialize then seek!
  InitializePipeline();
  DoSeek(expected);
}

TEST_F(PipelineImplTest, SetVolume) {
  CreateAudioStream();
  MockDemuxerStreamVector streams;
  streams.push_back(audio_stream());

  InitializeDataSource();
  InitializeDemuxer(&streams, base::TimeDelta());
  InitializeAudioDecoder(audio_stream());
  InitializeAudioRenderer();

  // The audio renderer should receive a call to SetVolume().
  float expected = 0.5f;
  EXPECT_CALL(*mocks_->audio_renderer(), SetVolume(expected));

  // Initialize then set volume!
  InitializePipeline();
  pipeline_->SetVolume(expected);
}

TEST_F(PipelineImplTest, Properties) {
  CreateVideoStream();
  MockDemuxerStreamVector streams;
  streams.push_back(video_stream());

  InitializeDataSource();
  const base::TimeDelta kDuration = base::TimeDelta::FromSeconds(100);
  InitializeDemuxer(&streams, kDuration);
  InitializeVideoDecoder(video_stream());
  InitializeVideoRenderer();

  InitializePipeline();
  EXPECT_TRUE(pipeline_->IsInitialized());
  EXPECT_EQ(PIPELINE_OK, pipeline_->GetError());
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

  InitializeDataSource();
  const base::TimeDelta kDuration = base::TimeDelta::FromSeconds(100);
  InitializeDemuxer(&streams, kDuration);
  InitializeVideoDecoder(video_stream());
  InitializeVideoRenderer();

  InitializePipeline();
  EXPECT_TRUE(pipeline_->IsInitialized());
  EXPECT_EQ(PIPELINE_OK, pipeline_->GetError());

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

  InitializeDataSource();
  InitializeDemuxer(&streams, base::TimeDelta());
  InitializeAudioDecoder(audio_stream());
  InitializeAudioRenderer();
  InitializeVideoDecoder(video_stream());
  InitializeVideoRenderer();

  InitializePipeline();
  EXPECT_TRUE(pipeline_->IsInitialized());
  EXPECT_EQ(PIPELINE_OK, pipeline_->GetError());
  EXPECT_TRUE(pipeline_->HasAudio());
  EXPECT_TRUE(pipeline_->HasVideo());

  EXPECT_CALL(*mocks_->audio_renderer(), SetPlaybackRate(1.0f))
      .WillOnce(DisableAudioRenderer(mocks_->audio_renderer()));
  EXPECT_CALL(*mocks_->data_source(),
              OnAudioRendererDisabled());
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
  EXPECT_CALL(callbacks_, OnEnded());
  FilterHost* host = pipeline_;
  host->NotifyEnded();
}

TEST_F(PipelineImplTest, DisableAudioRendererDuringInit) {
  CreateAudioStream();
  CreateVideoStream();
  MockDemuxerStreamVector streams;
  streams.push_back(audio_stream());
  streams.push_back(video_stream());

  InitializeDataSource();
  InitializeDemuxer(&streams, base::TimeDelta());
  InitializeAudioDecoder(audio_stream());
  InitializeAudioRenderer(true);
  InitializeVideoDecoder(video_stream());
  InitializeVideoRenderer();

  EXPECT_CALL(*mocks_->data_source(),
              OnAudioRendererDisabled());
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

  InitializePipeline();
  EXPECT_TRUE(pipeline_->IsInitialized());
  EXPECT_EQ(PIPELINE_OK, pipeline_->GetError());
  EXPECT_FALSE(pipeline_->HasAudio());
  EXPECT_TRUE(pipeline_->HasVideo());

  // Verify that ended event is fired when video ends.
  EXPECT_CALL(*mocks_->video_renderer(), HasEnded())
      .WillOnce(Return(true));
  EXPECT_CALL(callbacks_, OnEnded());
  FilterHost* host = pipeline_;
  host->NotifyEnded();
}

TEST_F(PipelineImplTest, EndedCallback) {
  CreateAudioStream();
  CreateVideoStream();
  MockDemuxerStreamVector streams;
  streams.push_back(audio_stream());
  streams.push_back(video_stream());

  InitializeDataSource();
  InitializeDemuxer(&streams, base::TimeDelta());
  InitializeAudioDecoder(audio_stream());
  InitializeAudioRenderer();
  InitializeVideoDecoder(video_stream());
  InitializeVideoRenderer();
  InitializePipeline();

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
  EXPECT_CALL(callbacks_, OnEnded());
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

  InitializeDataSource();
  InitializeDemuxer(&streams, duration);
  InitializeAudioDecoder(audio_stream());
  InitializeAudioRenderer();
  InitializeVideoDecoder(video_stream());
  InitializeVideoRenderer();
  InitializePipeline();

  // For convenience to simulate filters calling the methods.
  FilterHost* host = pipeline_;

  // Replace the clock so we can simulate wallclock time advancing w/o using
  // Sleep().
  pipeline_->SetClockForTesting(new Clock(&StaticClockFunction));

  EXPECT_EQ(0, host->GetTime().ToInternalValue());

  float playback_rate = 1.0f;
  EXPECT_CALL(*mocks_->data_source(), SetPlaybackRate(playback_rate));
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
  EXPECT_CALL(callbacks_, OnEnded());
  host->NotifyEnded();
}

TEST_F(PipelineImplTest, ErrorDuringSeek) {
  CreateAudioStream();
  MockDemuxerStreamVector streams;
  streams.push_back(audio_stream());

  InitializeDataSource();
  InitializeDemuxer(&streams, base::TimeDelta::FromSeconds(10));
  InitializeAudioDecoder(audio_stream());
  InitializeAudioRenderer();
  InitializePipeline();

  float playback_rate = 1.0f;
  EXPECT_CALL(*mocks_->data_source(), SetPlaybackRate(playback_rate));
  EXPECT_CALL(*mocks_->demuxer(), SetPlaybackRate(playback_rate));
  EXPECT_CALL(*mocks_->audio_decoder(), SetPlaybackRate(playback_rate));
  EXPECT_CALL(*mocks_->audio_renderer(), SetPlaybackRate(playback_rate));
  pipeline_->SetPlaybackRate(playback_rate);
  message_loop_.RunAllPending();

  InSequence s;

  base::TimeDelta seek_time = base::TimeDelta::FromSeconds(5);
  EXPECT_CALL(*mocks_->data_source(), Seek(seek_time, NotNull()))
      .WillOnce(Invoke(&RunFilterCallback));

  EXPECT_CALL(*mocks_->demuxer(), Seek(seek_time, NotNull()))
      .WillOnce(DoAll(SetError(mocks_->demuxer(),
                               PIPELINE_ERROR_READ),
                      Invoke(&RunFilterCallback)));

  pipeline_->Seek(seek_time, NewExpectedCallback());
  EXPECT_CALL(callbacks_, OnError());
  message_loop_.RunAllPending();
}

}  // namespace media
