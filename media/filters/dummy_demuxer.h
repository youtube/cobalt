// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Implements the Demuxer interface. DummyDemuxer returns corresponding
// DummyDemuxerStream as signal for media pipeline to construct correct
// playback channels. Used in WebRTC local video capture pipeline, where
// demuxing is not needed.

#ifndef MEDIA_FILTERS_DUMMY_DEMUXER_H_
#define MEDIA_FILTERS_DUMMY_DEMUXER_H_

#include <vector>

#include "media/base/audio_decoder_config.h"
#include "media/base/demuxer.h"
#include "media/base/video_decoder_config.h"

namespace media {

class DummyDemuxerStream : public DemuxerStream {
 public:
  explicit DummyDemuxerStream(Type type);

  // DemuxerStream implementation.
  virtual void Read(const ReadCB& read_cb) OVERRIDE;
  virtual Type type() OVERRIDE;
  virtual const AudioDecoderConfig& audio_decoder_config() OVERRIDE;
  virtual const VideoDecoderConfig& video_decoder_config() OVERRIDE;
  virtual void EnableBitstreamConverter() OVERRIDE;

 private:
  virtual ~DummyDemuxerStream();

  Type type_;
  AudioDecoderConfig audio_config_;
  VideoDecoderConfig video_config_;

  DISALLOW_COPY_AND_ASSIGN(DummyDemuxerStream);
};

class MEDIA_EXPORT DummyDemuxer : public Demuxer {
 public:
  DummyDemuxer(bool has_video, bool has_audio, bool local_source);
  virtual ~DummyDemuxer();

  // Demuxer implementation.
  virtual void Initialize(DemuxerHost* host,
                          const PipelineStatusCB& status_cb) OVERRIDE;
  virtual scoped_refptr<DemuxerStream> GetStream(
      DemuxerStream::Type type) OVERRIDE;
  virtual base::TimeDelta GetStartTime() const OVERRIDE;
  virtual int GetBitrate() OVERRIDE;
  virtual bool IsLocalSource() OVERRIDE;
  virtual bool IsSeekable() OVERRIDE;

 private:
  bool has_video_;
  bool has_audio_;
  bool local_source_;
  std::vector< scoped_refptr<DummyDemuxerStream> > streams_;

  DISALLOW_COPY_AND_ASSIGN(DummyDemuxer);
};

}  // namespace media

#endif  // MEDIA_FILTERS_DUMMY_DEMUXER_H_
