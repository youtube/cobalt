// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/dummy_demuxer.h"

#include "base/logging.h"

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

void DummyDemuxerStream::Read(const ReadCallback& read_callback) {}

void DummyDemuxerStream::EnableBitstreamConverter() {}

DummyDemuxer::DummyDemuxer(bool has_video, bool has_audio, bool local_source)
    : has_video_(has_video),
      has_audio_(has_audio),
      local_source_(local_source) {
  streams_.resize(DemuxerStream::NUM_TYPES);
  if (has_audio)
    streams_[DemuxerStream::AUDIO] =
        new DummyDemuxerStream(DemuxerStream::AUDIO);
  if (has_video)
    streams_[DemuxerStream::VIDEO] =
        new DummyDemuxerStream(DemuxerStream::VIDEO);
}

DummyDemuxer::~DummyDemuxer() {}

void DummyDemuxer::SetPreload(Preload preload) {}

int DummyDemuxer::GetBitrate() {
  return 0;
}

scoped_refptr<DemuxerStream> DummyDemuxer::GetStream(DemuxerStream::Type type) {
  return streams_[type];
}

base::TimeDelta DummyDemuxer::GetStartTime() const {
  return base::TimeDelta();
}

bool DummyDemuxer::IsLocalSource() {
  return local_source_;
}

bool DummyDemuxer::IsSeekable() {
  // This is always false because DummyDemuxer is only used by WebRTC and such
  // streams are not seekable.
  return false;
}

}  // namespace media
