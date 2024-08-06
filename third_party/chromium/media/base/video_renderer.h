// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_CHROMIUM_MEDIA_BASE_VIDEO_RENDERER_H_
#define THIRD_PARTY_CHROMIUM_MEDIA_BASE_VIDEO_RENDERER_H_

#include "base/functional/callback_forward.h"
#include "third_party/chromium/media/base/media_export.h"
#include "third_party/chromium/media/base/pipeline_status.h"
#include "third_party/chromium/media/base/time_source.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace media_m96 {

class CdmContext;
class DemuxerStream;
class RendererClient;

class MEDIA_EXPORT VideoRenderer {
 public:
  VideoRenderer();
  VideoRenderer(const VideoRenderer&) = delete;
  VideoRenderer& operator=(const VideoRenderer&) = delete;

  // Stops all operations and fires all pending callbacks.
  virtual ~VideoRenderer();

  // Initializes a VideoRenderer with |stream|, executing |init_cb| upon
  // completion. If initialization fails, only |init_cb|
  // (not RendererClient::OnError) will be called.
  //
  // |cdm_context| can be used to handle encrypted streams. May be null if the
  // stream is not encrypted.
  //
  // |wall_clock_time_cb| is used to convert media timestamps into wallclock
  // timestamps.
  //
  // VideoRenderer may be reinitialized for playback of a different demuxer
  // stream by calling Initialize again when the renderer is in a flushed
  // state (i.e. after Flush call, but before StartPlayingFrom). This is used
  // for media track switching.
  virtual void Initialize(DemuxerStream* stream,
                          CdmContext* cdm_context,
                          RendererClient* client,
                          const TimeSource::WallClockTimeCB& wall_clock_time_cb,
                          PipelineStatusCallback init_cb) = 0;

  // Discards any video data and stops reading from |stream|, executing
  // |callback| when completed.
  //
  // Clients should expect |buffering_state_cb| to be called with
  // BUFFERING_HAVE_NOTHING while flushing is in progress.
  virtual void Flush(base::OnceClosure callback) = 0;

  // Starts playback at |timestamp| by reading from |stream| and decoding and
  // rendering video.
  //
  // Only valid to call after a successful Initialize() or Flush().
  virtual void StartPlayingFrom(base::TimeDelta timestamp) = 0;

  // Called when time starts or stops moving. Time progresses when a base time
  // has been set and the playback rate is > 0. If either condition changes,
  // time stops progressing.
  virtual void OnTimeProgressing() = 0;
  virtual void OnTimeStopped() = 0;

  // Sets a hint indicating target latency. See comment in header for
  // media_m96::Renderer::SetLatencyHint().
  // |latency_hint| may be nullopt to indicate the hint has been cleared
  // (restore UA default).
  virtual void SetLatencyHint(absl::optional<base::TimeDelta> latency_hint) = 0;
};

}  // namespace media_m96

#endif  // THIRD_PARTY_CHROMIUM_MEDIA_BASE_VIDEO_RENDERER_H_
