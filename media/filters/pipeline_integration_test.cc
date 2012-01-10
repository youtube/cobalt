// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "media/base/filter_collection.h"
#include "media/base/media_log.h"
#include "media/base/message_loop_factory_impl.h"
#include "media/base/pipeline_impl.h"
#include "media/base/test_data_util.h"
#include "media/filters/ffmpeg_audio_decoder.h"
#include "media/filters/ffmpeg_demuxer_factory.h"
#include "media/filters/ffmpeg_video_decoder.h"
#include "media/filters/file_data_source_factory.h"
#include "media/filters/null_audio_renderer.h"
#include "media/filters/video_renderer_base.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::AnyNumber;

namespace media {

// Integration tests for PipelineImpl. Real demuxers, real decoders, and
// base renderer implementations are used to verify pipeline functionality. The
// renderers used in these tests rely heavily on the AudioRendererBase &
// VideoRendererBase implementations which contain a majority of the code used
// in the real AudioRendererImpl & VideoRendererImpl implementations used in the
// browser. The renderers in this test don't actually write data to a display or
// audio device. Both of these devices are simulated since they have little
// effect on verifying pipeline behavior and allow tests to run faster than
// real-time.
class PipelineIntegrationTest : public testing::Test {
 public:
  PipelineIntegrationTest()
      : message_loop_factory_(new MessageLoopFactoryImpl()),
        pipeline_(new PipelineImpl(&message_loop_, new MediaLog())),
        ended_(false) {
    pipeline_->Init(
        base::Bind(&PipelineIntegrationTest::OnEnded, base::Unretained(this)),
        base::Bind(&PipelineIntegrationTest::OnError, base::Unretained(this)),
        Pipeline::NetworkEventCB());

    EXPECT_CALL(*this, OnVideoRendererPaint()).Times(AnyNumber());
    EXPECT_CALL(*this, OnSetOpaque(true));
  }

  virtual ~PipelineIntegrationTest() {
    if (!pipeline_->IsRunning())
      return;

    Stop();
  }

  void OnStatusCallback(PipelineStatus expected_status,
                        PipelineStatus status) {
    DCHECK_EQ(status, expected_status);
    message_loop_.PostTask(FROM_HERE, MessageLoop::QuitClosure());
  }

  PipelineStatusCB QuitOnStatusCB(PipelineStatus expected_status) {
    return base::Bind(&PipelineIntegrationTest::OnStatusCallback,
                      base::Unretained(this),
                      expected_status);
  }

  void OnEnded(PipelineStatus status) {
    DCHECK_EQ(status, PIPELINE_OK);
    DCHECK(!ended_);
    ended_ = true;
    message_loop_.PostTask(FROM_HERE, MessageLoop::QuitClosure());
  }

  void WaitUntilOnEnded() {
    if (!ended_) {
      message_loop_.Run();
      DCHECK(ended_);
    }
  }

  MOCK_METHOD1(OnError, void(PipelineStatus));

  void Start(const std::string& url, PipelineStatus expected_status) {
    pipeline_->Start(CreateFilterCollection(), url,
                     QuitOnStatusCB(expected_status));
    message_loop_.Run();
  }

  void Play() {
    pipeline_->SetPlaybackRate(1);
  }

  void Pause() {
    pipeline_->SetPlaybackRate(0);
  }

  void Seek(base::TimeDelta seek_time) {
    ended_ = false;

    pipeline_->Seek(seek_time, QuitOnStatusCB(PIPELINE_OK));
    message_loop_.Run();
  }

  void Stop() {
    DCHECK(pipeline_->IsRunning());
    pipeline_->Stop(QuitOnStatusCB(PIPELINE_OK));
    message_loop_.Run();
  }

  void QuitAfterCurrentTimeTask(const base::TimeDelta& quit_time) {
    if (pipeline_->GetCurrentTime() >= quit_time) {
      message_loop_.Quit();
      return;
    }

    message_loop_.PostDelayedTask(
        FROM_HERE,
        base::Bind(&PipelineIntegrationTest::QuitAfterCurrentTimeTask,
                   base::Unretained(this), quit_time),
        10);
  }

  void WaitUntilCurrentTimeIsAfter(const base::TimeDelta& wait_time) {
    DCHECK(pipeline_->IsRunning());
    DCHECK_GT(pipeline_->GetPlaybackRate(), 0);
    DCHECK(wait_time <= pipeline_->GetMediaDuration());

    message_loop_.PostDelayedTask(
        FROM_HERE,
        base::Bind(&PipelineIntegrationTest::QuitAfterCurrentTimeTask,
                   base::Unretained(this),
                   wait_time),
        10);
    message_loop_.Run();
  }

  scoped_ptr<FilterCollection> CreateFilterCollection() {
    scoped_ptr<FilterCollection> collection(
        new FilterCollection());
    collection->SetDemuxerFactory(scoped_ptr<DemuxerFactory>(
        new FFmpegDemuxerFactory(
            scoped_ptr<DataSourceFactory>(new FileDataSourceFactory()),
            &message_loop_)));
    collection->AddAudioDecoder(new FFmpegAudioDecoder(
        message_loop_factory_->GetMessageLoop("AudioDecoderThread")));
    collection->AddVideoDecoder(new FFmpegVideoDecoder(
        message_loop_factory_->GetMessageLoop("VideoDecoderThread")));
    collection->AddVideoRenderer(new VideoRendererBase(
        base::Bind(&PipelineIntegrationTest::OnVideoRendererPaint,
                   base::Unretained(this)),
        base::Bind(&PipelineIntegrationTest::OnSetOpaque,
                   base::Unretained(this))));
    collection->AddAudioRenderer(new NullAudioRenderer());
    return collection.Pass();
  }

 protected:
  MessageLoop message_loop_;
  scoped_ptr<MessageLoopFactory> message_loop_factory_;
  scoped_refptr<Pipeline> pipeline_;
  bool ended_;

 private:
  MOCK_METHOD0(OnVideoRendererPaint, void());
  MOCK_METHOD1(OnSetOpaque, void(bool));
};


TEST_F(PipelineIntegrationTest, BasicPlayback) {
  Start(GetTestDataURL("bear-320x240.webm"), PIPELINE_OK);

  Play();

  WaitUntilOnEnded();
}

TEST_F(PipelineIntegrationTest, SeekWhilePaused) {
  Start(GetTestDataURL("bear-320x240.webm"), PIPELINE_OK);

  base::TimeDelta duration(pipeline_->GetMediaDuration());
  base::TimeDelta start_seek_time(duration / 4);
  base::TimeDelta seek_time(duration * 3 / 4);

  Play();
  WaitUntilCurrentTimeIsAfter(start_seek_time);
  Pause();
  Seek(seek_time);
  EXPECT_EQ(pipeline_->GetCurrentTime(), seek_time);
  Play();
  WaitUntilOnEnded();

  // Make sure seeking after reaching the end works as expected.
  Pause();
  Seek(seek_time);
  EXPECT_EQ(pipeline_->GetCurrentTime(), seek_time);
  Play();
  WaitUntilOnEnded();
}

TEST_F(PipelineIntegrationTest, SeekWhilePlaying) {
  Start(GetTestDataURL("bear-320x240.webm"), PIPELINE_OK);

  base::TimeDelta duration(pipeline_->GetMediaDuration());
  base::TimeDelta start_seek_time(duration / 4);
  base::TimeDelta seek_time(duration * 3 / 4);

  Play();
  WaitUntilCurrentTimeIsAfter(start_seek_time);
  Seek(seek_time);
  EXPECT_GE(pipeline_->GetCurrentTime(), seek_time);
  WaitUntilOnEnded();

  // Make sure seeking after reaching the end works as expected.
  Seek(seek_time);
  EXPECT_GE(pipeline_->GetCurrentTime(), seek_time);
  WaitUntilOnEnded();
}

}  // namespace media
