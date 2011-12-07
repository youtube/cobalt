// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FILTERS_FFMPEG_VIDEO_DECODER_H_
#define MEDIA_FILTERS_FFMPEG_VIDEO_DECODER_H_

#include <deque>

#include "base/memory/scoped_ptr.h"
#include "media/base/filters.h"
#include "media/base/pts_stream.h"
#include "ui/gfx/size.h"

class MessageLoop;

struct AVCodecContext;
struct AVFrame;

namespace media {

class MEDIA_EXPORT FFmpegVideoDecoder : public VideoDecoder {
 public:
  explicit FFmpegVideoDecoder(MessageLoop* message_loop);
  virtual ~FFmpegVideoDecoder();

  // Filter implementation.
  virtual void Stop(const base::Closure& callback) OVERRIDE;
  virtual void Seek(base::TimeDelta time, const FilterStatusCB& cb) OVERRIDE;
  virtual void Pause(const base::Closure& callback) OVERRIDE;
  virtual void Flush(const base::Closure& callback) OVERRIDE;

  // VideoDecoder implementation.
  virtual void Initialize(DemuxerStream* demuxer_stream,
                          const base::Closure& callback,
                          const StatisticsCallback& stats_callback) OVERRIDE;
  virtual void Read(const ReadCB& callback) OVERRIDE;
  virtual const gfx::Size& natural_size() OVERRIDE;

 private:
  enum DecoderState {
    kUninitialized,
    kNormal,
    kFlushCodec,
    kDecodeFinished,
  };

  // Carries out the reading operation scheduled by Read().
  void DoRead(const ReadCB& callback);

  // Reads from the demuxer stream with corresponding callback method.
  void ReadFromDemuxerStream();
  void DecodeBuffer(const scoped_refptr<Buffer>& buffer);

  // Carries out the decoding operation scheduled by DecodeBuffer().
  void DoDecodeBuffer(const scoped_refptr<Buffer>& buffer);
  bool Decode(const scoped_refptr<Buffer>& buffer,
              scoped_refptr<VideoFrame>* video_frame);

  // Delivers the frame to |read_cb_| and resets the callback.
  void DeliverFrame(const scoped_refptr<VideoFrame>& video_frame);

  // Releases resources associated with |codec_context_| and |av_frame_|
  // and resets them to NULL.
  void ReleaseFFmpegResources();

  // Allocates a video frame based on the current format and dimensions based on
  // the current state of |codec_context_|.
  scoped_refptr<VideoFrame> AllocateVideoFrame();

  MessageLoop* message_loop_;

  PtsStream pts_stream_;
  DecoderState state_;

  StatisticsCallback statistics_callback_;

  ReadCB read_cb_;

  // FFmpeg structures owned by this object.
  AVCodecContext* codec_context_;
  AVFrame* av_frame_;

  // Frame rate of the video.
  int frame_rate_numerator_;
  int frame_rate_denominator_;

  // TODO(scherkus): I think this should be calculated by VideoRenderers based
  // on information provided by VideoDecoders (i.e., aspect ratio).
  gfx::Size natural_size_;

  // Pointer to the demuxer stream that will feed us compressed buffers.
  scoped_refptr<DemuxerStream> demuxer_stream_;

  DISALLOW_COPY_AND_ASSIGN(FFmpegVideoDecoder);
};

}  // namespace media

#endif  // MEDIA_FILTERS_FFMPEG_VIDEO_DECODER_H_
