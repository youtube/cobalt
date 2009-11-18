// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/stl_util-inl.h"
#include "base/waitable_event.h"
#include "media/base/pipeline_impl.h"
#include "media/base/media_format.h"
#include "media/base/filters.h"
#include "media/base/factory.h"
#include "media/base/filter_host.h"
#include "media/base/mock_filters.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::DoAll;
using ::testing::InSequence;
using ::testing::Invoke;
using ::testing::Mock;
using ::testing::NotNull;
using ::testing::Return;
using ::testing::ReturnRef;
using ::testing::StrictMock;

namespace {

// Total bytes of the data source.
const int kTotalBytes = 1024;

// Buffered bytes of the data source.
const int kBufferedBytes = 1024;

}  // namespace

namespace media {

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
      : pipeline_(new PipelineImpl(&message_loop_)),
        mocks_(new MockFilterFactory()) {
    pipeline_->SetPipelineErrorCallback(NewCallback(
        reinterpret_cast<CallbackHelper*>(&callbacks_),
        &CallbackHelper::OnError));
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

    // Free allocated media formats (if any).
    STLDeleteElements(&stream_media_formats_);
  }

 protected:
  // Sets up expectations to allow the data source to initialize.
  void InitializeDataSource() {
    EXPECT_CALL(*mocks_->data_source(), Initialize("", NotNull()))
        .WillOnce(DoAll(SetTotalBytes(mocks_->data_source(), kTotalBytes),
                        SetBufferedBytes(mocks_->data_source(), kBufferedBytes),
                        Invoke(&RunFilterCallback)));
    EXPECT_CALL(*mocks_->data_source(), SetPlaybackRate(0.0f));
    EXPECT_CALL(*mocks_->data_source(), Seek(base::TimeDelta(), NotNull()))
        .WillOnce(Invoke(&RunFilterCallback));
    EXPECT_CALL(*mocks_->data_source(), Stop());
    EXPECT_CALL(*mocks_->data_source(), media_format())
        .WillOnce(ReturnRef(data_source_media_format_));
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
    EXPECT_CALL(*mocks_->demuxer(), Stop());

    // Configure the demuxer to return the streams.
    for (size_t i = 0; i < streams->size(); ++i) {
      scoped_refptr<DemuxerStream> stream = (*streams)[i];
      EXPECT_CALL(*mocks_->demuxer(), GetStream(i))
          .WillRepeatedly(Return(stream));
    }
  }

  // Create a stream with an associated media format.
  StrictMock<MockDemuxerStream>* CreateStream(const std::string& mime_type) {
    StrictMock<MockDemuxerStream>* stream =
        new StrictMock<MockDemuxerStream>();

    // Sets the mime type of this stream's media format, which is usually
    // checked to determine the type of decoder to create.
    MediaFormat* media_format = new MediaFormat();
    media_format->SetAsString(MediaFormat::kMimeType, mime_type);
    EXPECT_CALL(*stream, media_format())
        .WillRepeatedly(ReturnRef(*media_format));
    stream_media_formats_.push_back(media_format);

    return stream;
  }

  // Sets up expectations to allow the video decoder to initialize.
  void InitializeVideoDecoder(MockDemuxerStream* stream) {
    EXPECT_CALL(*mocks_->video_decoder(), Initialize(stream, NotNull()))
        .WillOnce(Invoke(&RunFilterCallback));
    EXPECT_CALL(*mocks_->video_decoder(), SetPlaybackRate(0.0f));
    EXPECT_CALL(*mocks_->video_decoder(), Seek(base::TimeDelta(), NotNull()))
        .WillOnce(Invoke(&RunFilterCallback));
    EXPECT_CALL(*mocks_->video_decoder(), Stop());
    EXPECT_CALL(*mocks_->video_decoder(), media_format())
        .WillOnce(ReturnRef(video_decoder_media_format_));
  }

  // Sets up expectations to allow the audio decoder to initialize.
  void InitializeAudioDecoder(MockDemuxerStream* stream) {
    EXPECT_CALL(*mocks_->audio_decoder(), Initialize(stream, NotNull()))
        .WillOnce(Invoke(&RunFilterCallback));
    EXPECT_CALL(*mocks_->audio_decoder(), SetPlaybackRate(0.0f));
    EXPECT_CALL(*mocks_->audio_decoder(), Seek(base::TimeDelta(), NotNull()))
        .WillOnce(Invoke(&RunFilterCallback));
    EXPECT_CALL(*mocks_->audio_decoder(), Stop());
    EXPECT_CALL(*mocks_->audio_decoder(), media_format())
        .WillOnce(ReturnRef(audio_decoder_media_format_));
  }

  // Sets up expectations to allow the video renderer to initialize.
  void InitializeVideoRenderer() {
    EXPECT_CALL(*mocks_->video_renderer(),
                Initialize(mocks_->video_decoder(), NotNull()))
        .WillOnce(Invoke(&RunFilterCallback));
    EXPECT_CALL(*mocks_->video_renderer(), SetPlaybackRate(0.0f));
    EXPECT_CALL(*mocks_->video_renderer(), Seek(base::TimeDelta(), NotNull()))
        .WillOnce(Invoke(&RunFilterCallback));
    EXPECT_CALL(*mocks_->video_renderer(), Stop());
  }

  // Sets up expectations to allow the audio renderer to initialize.
  void InitializeAudioRenderer() {
    EXPECT_CALL(*mocks_->audio_renderer(),
                Initialize(mocks_->audio_decoder(), NotNull()))
        .WillOnce(Invoke(&RunFilterCallback));
    EXPECT_CALL(*mocks_->audio_renderer(), SetPlaybackRate(0.0f));
    EXPECT_CALL(*mocks_->audio_renderer(), SetVolume(1.0f));
    EXPECT_CALL(*mocks_->audio_renderer(), Seek(base::TimeDelta(), NotNull()))
        .WillOnce(Invoke(&RunFilterCallback));
    EXPECT_CALL(*mocks_->audio_renderer(), Stop());
  }

  // Sets up expectations on the callback and initializes the pipeline.  Called
  // after tests have set expectations any filters they wish to use.
  void InitializePipeline() {
    // Expect an initialization callback.
    EXPECT_CALL(callbacks_, OnStart());
    pipeline_->Start(mocks_, "",
                     NewCallback(reinterpret_cast<CallbackHelper*>(&callbacks_),
                                 &CallbackHelper::OnStart));
    message_loop_.RunAllPending();
  }

  // Fixture members.
  StrictMock<CallbackHelper> callbacks_;
  MessageLoop message_loop_;
  scoped_refptr<PipelineImpl> pipeline_;
  scoped_refptr<media::MockFilterFactory> mocks_;

  MediaFormat data_source_media_format_;
  MediaFormat audio_decoder_media_format_;
  MediaFormat video_decoder_media_format_;

  typedef std::vector<MediaFormat*> MediaFormatVector;
  MediaFormatVector stream_media_formats_;

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
  EXPECT_FALSE(pipeline_->IsRendered(""));
  EXPECT_FALSE(pipeline_->IsRendered(AudioDecoder::major_mime_type()));
  EXPECT_FALSE(pipeline_->IsRendered(VideoDecoder::major_mime_type()));

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
  EXPECT_TRUE(kZero == pipeline_->GetDuration());

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
  EXPECT_CALL(*mocks_->data_source(), Stop());

  // This test hangs during initialization by never calling
  // InitializationComplete().  StrictMock<> will ensure that the callback is
  // never executed.
  pipeline_->Start(mocks_, "",
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
  mocks_->set_creation_successful(false);

  InitializePipeline();
  EXPECT_FALSE(pipeline_->IsInitialized());
  EXPECT_EQ(PIPELINE_ERROR_REQUIRED_FILTER_MISSING,
            pipeline_->GetError());
}

TEST_F(PipelineImplTest, URLNotFound) {
  EXPECT_CALL(*mocks_->data_source(), Initialize("", NotNull()))
      .WillOnce(DoAll(SetError(mocks_->data_source(),
                               PIPELINE_ERROR_URL_NOT_FOUND),
                      Invoke(&RunFilterCallback)));
  EXPECT_CALL(callbacks_, OnError());
  EXPECT_CALL(*mocks_->data_source(), Stop());

  InitializePipeline();
  EXPECT_FALSE(pipeline_->IsInitialized());
  EXPECT_EQ(PIPELINE_ERROR_URL_NOT_FOUND, pipeline_->GetError());
}

TEST_F(PipelineImplTest, NoStreams) {
  // Manually set these expectations because SetPlaybackRate() is not called if
  // we cannot fully initialize the pipeline.
  EXPECT_CALL(*mocks_->data_source(), Initialize("", NotNull()))
      .WillOnce(Invoke(&RunFilterCallback));
  EXPECT_CALL(*mocks_->data_source(), Stop());
  EXPECT_CALL(*mocks_->data_source(), media_format())
      .WillOnce(ReturnRef(data_source_media_format_));

  EXPECT_CALL(*mocks_->demuxer(), Initialize(mocks_->data_source(), NotNull()))
      .WillOnce(Invoke(&RunFilterCallback));
  EXPECT_CALL(*mocks_->demuxer(), GetNumberOfStreams())
      .WillRepeatedly(Return(0));
  EXPECT_CALL(*mocks_->demuxer(), Stop());
  EXPECT_CALL(callbacks_, OnError());

  InitializePipeline();
  EXPECT_FALSE(pipeline_->IsInitialized());
  EXPECT_EQ(PIPELINE_ERROR_COULD_NOT_RENDER, pipeline_->GetError());
}

TEST_F(PipelineImplTest, AudioStream) {
  scoped_refptr<StrictMock<MockDemuxerStream> > stream =
      CreateStream("audio/x-foo");
  MockDemuxerStreamVector streams;
  streams.push_back(stream);

  InitializeDataSource();
  InitializeDemuxer(&streams, base::TimeDelta());
  InitializeAudioDecoder(stream);
  InitializeAudioRenderer();

  InitializePipeline();
  EXPECT_TRUE(pipeline_->IsInitialized());
  EXPECT_EQ(PIPELINE_OK, pipeline_->GetError());
  EXPECT_TRUE(pipeline_->IsRendered(media::mime_type::kMajorTypeAudio));
  EXPECT_FALSE(pipeline_->IsRendered(media::mime_type::kMajorTypeVideo));
}

TEST_F(PipelineImplTest, VideoStream) {
  scoped_refptr<StrictMock<MockDemuxerStream> > stream =
      CreateStream("video/x-foo");
  MockDemuxerStreamVector streams;
  streams.push_back(stream);

  InitializeDataSource();
  InitializeDemuxer(&streams, base::TimeDelta());
  InitializeVideoDecoder(stream);
  InitializeVideoRenderer();

  InitializePipeline();
  EXPECT_TRUE(pipeline_->IsInitialized());
  EXPECT_EQ(PIPELINE_OK, pipeline_->GetError());
  EXPECT_FALSE(pipeline_->IsRendered(media::mime_type::kMajorTypeAudio));
  EXPECT_TRUE(pipeline_->IsRendered(media::mime_type::kMajorTypeVideo));
}

TEST_F(PipelineImplTest, AudioVideoStream) {
  scoped_refptr<StrictMock<MockDemuxerStream> > audio_stream =
      CreateStream("audio/x-foo");
  scoped_refptr<StrictMock<MockDemuxerStream> > video_stream =
      CreateStream("video/x-foo");
  MockDemuxerStreamVector streams;
  streams.push_back(audio_stream);
  streams.push_back(video_stream);

  InitializeDataSource();
  InitializeDemuxer(&streams, base::TimeDelta());
  InitializeAudioDecoder(audio_stream);
  InitializeAudioRenderer();
  InitializeVideoDecoder(video_stream);
  InitializeVideoRenderer();

  InitializePipeline();
  EXPECT_TRUE(pipeline_->IsInitialized());
  EXPECT_EQ(PIPELINE_OK, pipeline_->GetError());
  EXPECT_TRUE(pipeline_->IsRendered(media::mime_type::kMajorTypeAudio));
  EXPECT_TRUE(pipeline_->IsRendered(media::mime_type::kMajorTypeVideo));
}

TEST_F(PipelineImplTest, Seek) {
  scoped_refptr<StrictMock<MockDemuxerStream> > audio_stream =
      CreateStream("audio/x-foo");
  scoped_refptr<StrictMock<MockDemuxerStream> > video_stream =
      CreateStream("video/x-foo");
  MockDemuxerStreamVector streams;
  streams.push_back(audio_stream);
  streams.push_back(video_stream);

  InitializeDataSource();
  InitializeDemuxer(&streams, base::TimeDelta::FromSeconds(3000));
  InitializeAudioDecoder(audio_stream);
  InitializeAudioRenderer();
  InitializeVideoDecoder(video_stream);
  InitializeVideoRenderer();

  // Every filter should receive a call to Seek().
  base::TimeDelta expected = base::TimeDelta::FromSeconds(2000);
  EXPECT_CALL(*mocks_->data_source(), Seek(expected, NotNull()))
      .WillOnce(Invoke(&RunFilterCallback));
  EXPECT_CALL(*mocks_->demuxer(), Seek(expected, NotNull()))
      .WillOnce(Invoke(&RunFilterCallback));
  EXPECT_CALL(*mocks_->audio_decoder(), Seek(expected, NotNull()))
      .WillOnce(Invoke(&RunFilterCallback));
  EXPECT_CALL(*mocks_->audio_renderer(), Seek(expected, NotNull()))
      .WillOnce(Invoke(&RunFilterCallback));
  EXPECT_CALL(*mocks_->video_decoder(), Seek(expected, NotNull()))
      .WillOnce(Invoke(&RunFilterCallback));
  EXPECT_CALL(*mocks_->video_renderer(), Seek(expected, NotNull()))
      .WillOnce(Invoke(&RunFilterCallback));

  // We expect a successful seek callback.
  EXPECT_CALL(callbacks_, OnSeek());

  // Initialize then seek!
  InitializePipeline();
  pipeline_->Seek(expected,
                  NewCallback(reinterpret_cast<CallbackHelper*>(&callbacks_),
                              &CallbackHelper::OnSeek));

  // We expect the time to be updated only after the seek has completed.
  EXPECT_TRUE(expected != pipeline_->GetCurrentTime());
  message_loop_.RunAllPending();
  EXPECT_TRUE(expected == pipeline_->GetCurrentTime());
}

TEST_F(PipelineImplTest, SetVolume) {
  scoped_refptr<StrictMock<MockDemuxerStream> > audio_stream =
      CreateStream("audio/x-foo");
  MockDemuxerStreamVector streams;
  streams.push_back(audio_stream);

  InitializeDataSource();
  InitializeDemuxer(&streams, base::TimeDelta());
  InitializeAudioDecoder(audio_stream);
  InitializeAudioRenderer();

  // The audio renderer should receive a call to SetVolume().
  float expected = 0.5f;
  EXPECT_CALL(*mocks_->audio_renderer(), SetVolume(expected));

  // Initialize then set volume!
  InitializePipeline();
  pipeline_->SetVolume(expected);
}

TEST_F(PipelineImplTest, Properties) {
  scoped_refptr<StrictMock<MockDemuxerStream> > stream =
      CreateStream("video/x-foo");
  MockDemuxerStreamVector streams;
  streams.push_back(stream);

  InitializeDataSource();
  const base::TimeDelta kDuration = base::TimeDelta::FromSeconds(100);
  InitializeDemuxer(&streams, kDuration);
  InitializeVideoDecoder(stream);
  InitializeVideoRenderer();

  InitializePipeline();
  EXPECT_TRUE(pipeline_->IsInitialized());
  EXPECT_EQ(PIPELINE_OK, pipeline_->GetError());
  EXPECT_EQ(kDuration.ToInternalValue(),
            pipeline_->GetDuration().ToInternalValue());
  EXPECT_EQ(kTotalBytes, pipeline_->GetTotalBytes());
  EXPECT_EQ(kBufferedBytes, pipeline_->GetBufferedBytes());
  EXPECT_EQ(kDuration.ToInternalValue(),
            pipeline_->GetBufferedTime().ToInternalValue());
}

TEST_F(PipelineImplTest, BroadcastMessage) {
  scoped_refptr<StrictMock<MockDemuxerStream> > audio_stream =
      CreateStream("audio/x-foo");
  scoped_refptr<StrictMock<MockDemuxerStream> > video_stream =
      CreateStream("video/x-foo");
  MockDemuxerStreamVector streams;
  streams.push_back(audio_stream);
  streams.push_back(video_stream);

  InitializeDataSource();
  InitializeDemuxer(&streams, base::TimeDelta());
  InitializeAudioDecoder(audio_stream);
  InitializeAudioRenderer();
  InitializeVideoDecoder(video_stream);
  InitializeVideoRenderer();

  InitializePipeline();
  EXPECT_TRUE(pipeline_->IsInitialized());
  EXPECT_EQ(PIPELINE_OK, pipeline_->GetError());
  EXPECT_TRUE(pipeline_->IsRendered(mime_type::kMajorTypeAudio));
  EXPECT_TRUE(pipeline_->IsRendered(mime_type::kMajorTypeVideo));

  EXPECT_CALL(*mocks_->audio_renderer(), SetPlaybackRate(1.0f))
      .WillOnce(BroadcastMessage(mocks_->audio_renderer(),
                                 kMsgDisableAudio));
  EXPECT_CALL(*mocks_->data_source(),
              OnReceivedMessage(kMsgDisableAudio));
  EXPECT_CALL(*mocks_->demuxer(),
              OnReceivedMessage(kMsgDisableAudio));
  EXPECT_CALL(*mocks_->audio_decoder(),
              OnReceivedMessage(kMsgDisableAudio));
  EXPECT_CALL(*mocks_->audio_renderer(),
              OnReceivedMessage(kMsgDisableAudio));
  EXPECT_CALL(*mocks_->video_decoder(),
              OnReceivedMessage(kMsgDisableAudio));
  EXPECT_CALL(*mocks_->video_renderer(),
              OnReceivedMessage(kMsgDisableAudio));

  mocks_->audio_renderer()->SetPlaybackRate(1.0f);
}

TEST_F(PipelineImplTest, EndedCallback) {
  scoped_refptr<StrictMock<MockDemuxerStream> > audio_stream =
      CreateStream("audio/x-foo");
  scoped_refptr<StrictMock<MockDemuxerStream> > video_stream =
      CreateStream("video/x-foo");
  MockDemuxerStreamVector streams;
  streams.push_back(audio_stream);
  streams.push_back(video_stream);

  // Set our ended callback.
  pipeline_->SetPipelineEndedCallback(
      NewCallback(reinterpret_cast<CallbackHelper*>(&callbacks_),
                  &CallbackHelper::OnEnded));

  InitializeDataSource();
  InitializeDemuxer(&streams, base::TimeDelta());
  InitializeAudioDecoder(audio_stream);
  InitializeAudioRenderer();
  InitializeVideoDecoder(video_stream);
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

}  // namespace media
