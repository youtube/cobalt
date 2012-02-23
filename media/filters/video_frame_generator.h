// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FILTERS_VIDEO_FRAME_GENERATOR_H_
#define MEDIA_FILTERS_VIDEO_FRAME_GENERATOR_H_

#include "media/base/filters.h"
#include "media/base/pipeline_status.h"

namespace base {
class MessageLoopProxy;
}

namespace media {

class DemuxerStream;
class VideoFrame;

// A filter generates raw frames and passes them to media engine as a video
// decoder filter.
class MEDIA_EXPORT VideoFrameGenerator
    : public VideoDecoder {
 public:
  VideoFrameGenerator(
      base::MessageLoopProxy* message_loop_proxy,
      const gfx::Size& size,
      const base::TimeDelta& frame_duration);
  virtual ~VideoFrameGenerator();

  // Filter implementation.
  virtual void Stop(const base::Closure& callback) OVERRIDE;

  // Decoder implementation.
  virtual void Initialize(
      DemuxerStream* demuxer_stream,
      const PipelineStatusCB& filter_callback,
      const StatisticsCallback& stat_callback) OVERRIDE;
  virtual void Read(const ReadCB& callback) OVERRIDE;
  virtual const gfx::Size& natural_size() OVERRIDE;

 private:
  void StopOnDecoderThread(const base::Closure& callback);

  void InitializeOnDecoderThread(
      DemuxerStream* demuxer_stream,
      const PipelineStatusCB& filter_callback,
      const StatisticsCallback& stat_callback);
  void ReadOnDecoderThread(const ReadCB& callback);

  scoped_refptr<base::MessageLoopProxy> message_loop_proxy_;
  gfx::Size natural_size_;
  bool stopped_;

  base::TimeDelta current_time_;
  base::TimeDelta frame_duration_;

  DISALLOW_COPY_AND_ASSIGN(VideoFrameGenerator);
};

}  // namespace media

#endif  // MEDIA_FILTERS_VIDEO_FRAME_GENERATOR_H_
