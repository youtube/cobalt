// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FILTERS_PIPELINE_INTEGRATION_TEST_BASE_H_
#define MEDIA_FILTERS_PIPELINE_INTEGRATION_TEST_BASE_H_

#include "base/message_loop.h"
#include "base/md5.h"
#include "media/audio/null_audio_sink.h"
#include "media/base/filter_collection.h"
#include "media/base/message_loop_factory.h"
#include "media/base/pipeline.h"
#include "media/filters/chunk_demuxer.h"
#include "media/filters/ffmpeg_video_decoder.h"
#include "media/filters/video_renderer_base.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace media {

// Empty MD5 hash string.  Used to verify videos which have decoded no frames.
extern const char kNullVideoHash[];

// Integration tests for Pipeline. Real demuxers, real decoders, and
// base renderer implementations are used to verify pipeline functionality. The
// renderers used in these tests rely heavily on the AudioRendererBase &
// VideoRendererBase implementations which contain a majority of the code used
// in the real AudioRendererImpl & SkCanvasVideoRenderer implementations used in
// the browser. The renderers in this test don't actually write data to a
// display or audio device. Both of these devices are simulated since they have
// little effect on verifying pipeline behavior and allow tests to run faster
// than real-time.
class PipelineIntegrationTestBase {
 public:
  PipelineIntegrationTestBase();
  virtual ~PipelineIntegrationTestBase();

  bool WaitUntilOnEnded();
  PipelineStatus WaitUntilEndedOrError();
  bool Start(const std::string& url, PipelineStatus expected_status);
  void Play();
  void Pause();
  bool Seek(base::TimeDelta seek_time);
  void Stop();
  bool WaitUntilCurrentTimeIsAfter(const base::TimeDelta& wait_time);
  scoped_ptr<FilterCollection> CreateFilterCollection(const std::string& url);
  scoped_ptr<FilterCollection> CreateFilterCollection(
      ChunkDemuxerClient* client);

  // Returns the MD5 hash of all video frames seen.  Should only be called once
  // after playback completes.  First time hashes should be generated with
  // --video-threads=1 to ensure correctness.
  std::string GetVideoHash();

  // Returns the MD5 hash of all audio frames seen.  Should only be called once
  // after playback completes.
  std::string GetAudioHash();

 protected:
  MessageLoop message_loop_;
  base::MD5Context md5_context_;
  scoped_ptr<MessageLoopFactory> message_loop_factory_;
  scoped_refptr<Pipeline> pipeline_;
  scoped_refptr<FFmpegVideoDecoder> decoder_;
  scoped_refptr<VideoRendererBase> renderer_;
  scoped_refptr<NullAudioSink> audio_sink_;
  bool ended_;
  PipelineStatus pipeline_status_;

  void OnStatusCallback(PipelineStatus expected_status,
                        PipelineStatus status);
  PipelineStatusCB QuitOnStatusCB(PipelineStatus expected_status);
  void OnEnded(PipelineStatus status);
  void OnError(PipelineStatus status);
  void QuitAfterCurrentTimeTask(const base::TimeDelta& quit_time);
  scoped_ptr<FilterCollection> CreateFilterCollection(
      const scoped_refptr<Demuxer>& demuxer);
  void OnVideoRendererPaint();

  MOCK_METHOD1(OnSetOpaque, void(bool));
};

}  // namespace media

#endif  // MEDIA_FILTERS_PIPELINE_INTEGRATION_TEST_BASE_H_
