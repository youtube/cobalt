// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/pipeline_integration_test_base.h"

#include "base/bind.h"
#include "media/base/media_log.h"
#include "media/filters/audio_renderer_impl.h"
#include "media/filters/chunk_demuxer.h"
#include "media/filters/ffmpeg_audio_decoder.h"
#include "media/filters/ffmpeg_demuxer.h"
#include "media/filters/ffmpeg_video_decoder.h"
#include "media/filters/file_data_source.h"

using ::testing::AnyNumber;

namespace media {

const char kNullHash[] = "d41d8cd98f00b204e9800998ecf8427e";

PipelineIntegrationTestBase::PipelineIntegrationTestBase()
    : hashing_enabled_(false),
      message_loop_factory_(new MessageLoopFactory()),
      pipeline_(new Pipeline(message_loop_.message_loop_proxy(),
                             new MediaLog())),
      ended_(false),
      pipeline_status_(PIPELINE_OK) {
  base::MD5Init(&md5_context_);
  EXPECT_CALL(*this, OnSetOpaque(true)).Times(AnyNumber());
}

PipelineIntegrationTestBase::~PipelineIntegrationTestBase() {
  if (!pipeline_->IsRunning())
    return;

  Stop();
}

void PipelineIntegrationTestBase::OnStatusCallback(
    PipelineStatus status) {
  pipeline_status_ = status;
  message_loop_.PostTask(FROM_HERE, MessageLoop::QuitClosure());
}

void PipelineIntegrationTestBase::OnStatusCallbackChecked(
    PipelineStatus expected_status,
    PipelineStatus status) {
  EXPECT_EQ(status, expected_status);
  OnStatusCallback(status);
}

PipelineStatusCB PipelineIntegrationTestBase::QuitOnStatusCB(
    PipelineStatus expected_status) {
  return base::Bind(&PipelineIntegrationTestBase::OnStatusCallbackChecked,
                    base::Unretained(this),
                    expected_status);
}

void PipelineIntegrationTestBase::OnEnded(PipelineStatus status) {
  DCHECK_EQ(status, PIPELINE_OK);
  DCHECK(!ended_);
  ended_ = true;
  pipeline_status_ = status;
  message_loop_.PostTask(FROM_HERE, MessageLoop::QuitClosure());
}

bool PipelineIntegrationTestBase::WaitUntilOnEnded() {
  if (ended_)
    return (pipeline_status_ == PIPELINE_OK);
  message_loop_.Run();
  EXPECT_TRUE(ended_);
  return ended_ && (pipeline_status_ == PIPELINE_OK);
}

PipelineStatus PipelineIntegrationTestBase::WaitUntilEndedOrError() {
  if (ended_ || pipeline_status_ != PIPELINE_OK)
    return pipeline_status_;
  message_loop_.Run();
  return pipeline_status_;
}

void PipelineIntegrationTestBase::OnError(PipelineStatus status) {
  DCHECK_NE(status, PIPELINE_OK);
  pipeline_status_ = status;
  message_loop_.PostTask(FROM_HERE, MessageLoop::QuitClosure());
}

bool PipelineIntegrationTestBase::Start(const std::string& url,
                                        PipelineStatus expected_status) {
  EXPECT_CALL(*this, OnBufferingState(Pipeline::kHaveMetadata));
  EXPECT_CALL(*this, OnBufferingState(Pipeline::kPrerollCompleted));
  pipeline_->Start(
      CreateFilterCollection(url),
      base::Bind(&PipelineIntegrationTestBase::OnEnded, base::Unretained(this)),
      base::Bind(&PipelineIntegrationTestBase::OnError, base::Unretained(this)),
      QuitOnStatusCB(expected_status),
      base::Bind(&PipelineIntegrationTestBase::OnBufferingState,
                 base::Unretained(this)));
  message_loop_.Run();
  return (pipeline_status_ == PIPELINE_OK);
}

bool PipelineIntegrationTestBase::Start(const std::string& url,
                                        PipelineStatus expected_status,
                                        bool hashing_enabled) {
  hashing_enabled_ = hashing_enabled;
  return Start(url, expected_status);
}

bool PipelineIntegrationTestBase::Start(const std::string& url) {
  EXPECT_CALL(*this, OnBufferingState(Pipeline::kHaveMetadata));
  EXPECT_CALL(*this, OnBufferingState(Pipeline::kPrerollCompleted));
  pipeline_->Start(
      CreateFilterCollection(url),
      base::Bind(&PipelineIntegrationTestBase::OnEnded, base::Unretained(this)),
      base::Bind(&PipelineIntegrationTestBase::OnError, base::Unretained(this)),
      base::Bind(&PipelineIntegrationTestBase::OnStatusCallback,
                 base::Unretained(this)),
      base::Bind(&PipelineIntegrationTestBase::OnBufferingState,
                 base::Unretained(this)));
  message_loop_.Run();
  return (pipeline_status_ == PIPELINE_OK);
}

void PipelineIntegrationTestBase::Play() {
  pipeline_->SetPlaybackRate(1);
}

void PipelineIntegrationTestBase::Pause() {
  pipeline_->SetPlaybackRate(0);
}

bool PipelineIntegrationTestBase::Seek(base::TimeDelta seek_time) {
  ended_ = false;

  EXPECT_CALL(*this, OnBufferingState(Pipeline::kPrerollCompleted));
  pipeline_->Seek(seek_time, QuitOnStatusCB(PIPELINE_OK));
  message_loop_.Run();
  return (pipeline_status_ == PIPELINE_OK);
}

void PipelineIntegrationTestBase::Stop() {
  DCHECK(pipeline_->IsRunning());
  pipeline_->Stop(MessageLoop::QuitClosure());
  message_loop_.Run();
}

void PipelineIntegrationTestBase::QuitAfterCurrentTimeTask(
    const base::TimeDelta& quit_time) {
  if (pipeline_->GetMediaTime() >= quit_time ||
      pipeline_status_ != PIPELINE_OK) {
    message_loop_.Quit();
    return;
  }

  message_loop_.PostDelayedTask(
      FROM_HERE,
      base::Bind(&PipelineIntegrationTestBase::QuitAfterCurrentTimeTask,
                 base::Unretained(this), quit_time),
      base::TimeDelta::FromMilliseconds(10));
}

bool PipelineIntegrationTestBase::WaitUntilCurrentTimeIsAfter(
    const base::TimeDelta& wait_time) {
  DCHECK(pipeline_->IsRunning());
  DCHECK_GT(pipeline_->GetPlaybackRate(), 0);
  DCHECK(wait_time <= pipeline_->GetMediaDuration());

  message_loop_.PostDelayedTask(
      FROM_HERE,
      base::Bind(&PipelineIntegrationTestBase::QuitAfterCurrentTimeTask,
                 base::Unretained(this),
                 wait_time),
      base::TimeDelta::FromMilliseconds(10));
  message_loop_.Run();
  return (pipeline_status_ == PIPELINE_OK);
}

scoped_ptr<FilterCollection>
PipelineIntegrationTestBase::CreateFilterCollection(const std::string& url) {
  scoped_refptr<FileDataSource> data_source = new FileDataSource();
  CHECK(data_source->Initialize(url));
  return CreateFilterCollection(
      new FFmpegDemuxer(message_loop_.message_loop_proxy(), data_source),
      NULL);
}

scoped_ptr<FilterCollection>
PipelineIntegrationTestBase::CreateFilterCollection(
    const scoped_refptr<Demuxer>& demuxer,
    Decryptor* decryptor) {
  scoped_ptr<FilterCollection> collection(new FilterCollection());
  collection->SetDemuxer(demuxer);
  collection->AddAudioDecoder(new FFmpegAudioDecoder(
      base::Bind(&MessageLoopFactory::GetMessageLoop,
                 base::Unretained(message_loop_factory_.get()),
                 media::MessageLoopFactory::kDecoder)));
  scoped_refptr<VideoDecoder> decoder = new FFmpegVideoDecoder(
      base::Bind(&MessageLoopFactory::GetMessageLoop,
                 base::Unretained(message_loop_factory_.get()),
                 media::MessageLoopFactory::kDecoder),
      decryptor);
  collection->GetVideoDecoders()->push_back(decoder);

  // Disable frame dropping if hashing is enabled.
  renderer_ = new VideoRendererBase(
      base::Bind(&PipelineIntegrationTestBase::OnVideoRendererPaint,
                 base::Unretained(this)),
      base::Bind(&PipelineIntegrationTestBase::OnSetOpaque,
                 base::Unretained(this)),
      !hashing_enabled_);
  collection->AddVideoRenderer(renderer_);
  audio_sink_ = new NullAudioSink();
  if (hashing_enabled_)
    audio_sink_->StartAudioHashForTesting();
  scoped_refptr<AudioRendererImpl> audio_renderer(new AudioRendererImpl(
      audio_sink_));
  // Disable underflow if hashing is enabled.
  if (hashing_enabled_)
    audio_renderer->DisableUnderflowForTesting();
  collection->AddAudioRenderer(audio_renderer);
  return collection.Pass();
}

void PipelineIntegrationTestBase::OnVideoRendererPaint() {
  if (!hashing_enabled_)
    return;
  scoped_refptr<VideoFrame> frame;
  renderer_->GetCurrentFrame(&frame);
  if (frame)
    frame->HashFrameForTesting(&md5_context_);
  renderer_->PutCurrentFrame(frame);
}

std::string PipelineIntegrationTestBase::GetVideoHash() {
  DCHECK(hashing_enabled_);
  base::MD5Digest digest;
  base::MD5Final(&digest, &md5_context_);
  return base::MD5DigestToBase16(digest);
}

std::string PipelineIntegrationTestBase::GetAudioHash() {
  DCHECK(hashing_enabled_);
  return audio_sink_->GetAudioHashForTesting();
}

}  // namespace media
