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
  virtual void Read(const ReadCB& read_cb) override;
  virtual Type type() override;
  virtual const AudioDecoderConfig& audio_decoder_config() override;
  virtual const VideoDecoderConfig& video_decoder_config() override;
  virtual void EnableBitstreamConverter() override;

#if defined(__LB_SHELL__) || defined(COBALT)
  bool StreamWasEncrypted() const override {
    return video_config_.is_encrypted();
  }
#endif

 protected:
  virtual ~DummyDemuxerStream();

 private:
  Type type_;
  AudioDecoderConfig audio_config_;
  VideoDecoderConfig video_config_;

  DISALLOW_COPY_AND_ASSIGN(DummyDemuxerStream);
};

class MEDIA_EXPORT DummyDemuxer : public Demuxer {
 public:
  DummyDemuxer(bool has_video, bool has_audio);

  // Demuxer implementation.
  virtual void Initialize(DemuxerHost* host,
                          const PipelineStatusCB& status_cb) override;
  virtual scoped_refptr<DemuxerStream> GetStream(
      DemuxerStream::Type type) override;
  virtual base::TimeDelta GetStartTime() const override;

 protected:
  virtual ~DummyDemuxer();

 private:
  std::vector< scoped_refptr<DummyDemuxerStream> > streams_;

  DISALLOW_COPY_AND_ASSIGN(DummyDemuxer);
};

}  // namespace media

#endif  // MEDIA_FILTERS_DUMMY_DEMUXER_H_
