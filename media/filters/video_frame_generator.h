// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FILTERS_VIDEO_FRAME_GENERATOR_H_
#define MEDIA_FILTERS_VIDEO_FRAME_GENERATOR_H_

#include "base/time.h"
#include "media/base/video_decoder.h"
#include "media/base/pipeline_status.h"

namespace base {
class MessageLoopProxy;
}

namespace media {

class DemuxerStream;
class VideoFrame;

// A filter generates raw frames and passes them to media engine as a video
// decoder filter.
class MEDIA_EXPORT VideoFrameGenerator : public VideoDecoder {
 public:
  VideoFrameGenerator(
      const scoped_refptr<base::MessageLoopProxy>& message_loop_proxy,
      const gfx::Size& size,
      const base::TimeDelta& frame_duration);

  // VideoDecoder implementation.
  virtual void Initialize(
      const scoped_refptr<DemuxerStream>& stream,
      const PipelineStatusCB& status_cb,
      const StatisticsCB& statistics_cb) override;
  virtual void Read(const ReadCB& read_cb) override;
  virtual void Reset(const base::Closure& closure) override;
  virtual void Stop(const base::Closure& closure) override;

 protected:
  virtual ~VideoFrameGenerator();

 private:
  void InitializeOnDecoderThread(
      const scoped_refptr<DemuxerStream>& stream,
      const PipelineStatusCB& status_cb,
      const StatisticsCB& statistics_cb);
  void ReadOnDecoderThread(const ReadCB& read_cb);
  void ResetOnDecoderThread(const base::Closure& closure);
  void StopOnDecoderThread(const base::Closure& closure);

  scoped_refptr<base::MessageLoopProxy> message_loop_proxy_;
  gfx::Size size_;
  bool stopped_;

  base::TimeDelta current_time_;
  base::TimeDelta frame_duration_;

  DISALLOW_COPY_AND_ASSIGN(VideoFrameGenerator);
};

}  // namespace media

#endif  // MEDIA_FILTERS_VIDEO_FRAME_GENERATOR_H_
