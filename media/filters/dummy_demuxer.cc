// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/dummy_demuxer.h"

#include "base/logging.h"

#if defined(__LB_SHELL__)
#include "media/base/shell_filter_graph_log.h"
#endif

namespace media {

DummyDemuxerStream::DummyDemuxerStream(Type type)
    : type_(type) {
}

DummyDemuxerStream::~DummyDemuxerStream() {}

DemuxerStream::Type DummyDemuxerStream::type() {
  return type_;
}

const AudioDecoderConfig& DummyDemuxerStream::audio_decoder_config() {
  CHECK_EQ(type_, AUDIO);
  return audio_config_;
}

const VideoDecoderConfig& DummyDemuxerStream::video_decoder_config() {
  CHECK_EQ(type_, VIDEO);
  return video_config_;
}

void DummyDemuxerStream::Read(const ReadCB& read_cb) {}

void DummyDemuxerStream::EnableBitstreamConverter() {}

#if defined(__LB_SHELL__)
void DummyDemuxerStream::SetFilterGraphLog(
    scoped_refptr<ShellFilterGraphLog> filter_graph_log) {
  filter_graph_log_ = filter_graph_log;
}

scoped_refptr<ShellFilterGraphLog> DummyDemuxerStream::filter_graph_log() {
  return filter_graph_log_;
}
#endif

DummyDemuxer::DummyDemuxer(bool has_video, bool has_audio) {
  streams_.resize(DemuxerStream::NUM_TYPES);
  if (has_audio) {
    streams_[DemuxerStream::AUDIO] =
        new DummyDemuxerStream(DemuxerStream::AUDIO);
    streams_[DemuxerStream::AUDIO]->SetFilterGraphLog(filter_graph_log_);
  }
  if (has_video) {
    streams_[DemuxerStream::VIDEO] =
        new DummyDemuxerStream(DemuxerStream::VIDEO);
    streams_[DemuxerStream::VIDEO]->SetFilterGraphLog(filter_graph_log_);
  }
}

void DummyDemuxer::Initialize(DemuxerHost* host,
                              const PipelineStatusCB& status_cb) {
#if defined(__LB_SHELL__)
  DCHECK(filter_graph_log_);
#endif
  host->SetDuration(media::kInfiniteDuration());
  status_cb.Run(PIPELINE_OK);
}

scoped_refptr<DemuxerStream> DummyDemuxer::GetStream(DemuxerStream::Type type) {
  return streams_[type];
}

base::TimeDelta DummyDemuxer::GetStartTime() const {
  return base::TimeDelta();
}

DummyDemuxer::~DummyDemuxer() {}

#if defined(__LB_SHELL__)
void DummyDemuxer::SetFilterGraphLog(
    scoped_refptr<ShellFilterGraphLog> filter_graph_log) {
  filter_graph_log_ = filter_graph_log;
}
#endif

}  // namespace media
