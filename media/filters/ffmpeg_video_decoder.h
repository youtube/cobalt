// Copyright (c) 2009 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#ifndef MEDIA_FILTERS_FFMPEG_VIDEO_DECODER_H_
#define MEDIA_FILTERS_FFMPEG_VIDEO_DECODER_H_

#include <queue>

#include "media/base/factory.h"
#include "media/base/pts_heap.h"
#include "media/filters/decoder_base.h"
#include "media/filters/video_decode_engine.h"
#include "testing/gtest/include/gtest/gtest_prod.h"

// FFmpeg types.
struct AVCodecContext;
struct AVFrame;
struct AVRational;
struct AVStream;

namespace media {

class FFmpegVideoDecodeEngine : public VideoDecodeEngine {
 public:
  FFmpegVideoDecodeEngine();
  virtual ~FFmpegVideoDecodeEngine();

  // Implementation of the VideoDecodeEngine Interface.
  virtual void Initialize(AVStream* stream, Task* done_cb);
  virtual void DecodeFrame(const Buffer& buffer, AVFrame* yuv_frame,
                           bool* got_result, Task* done_cb);
  virtual void Flush(Task* done_cb);
  virtual VideoSurface::Format GetSurfaceFormat() const;

  virtual State state() const { return state_; }

  virtual AVCodecContext* codec_context() const { return codec_context_; }

  virtual void SetCodecContextForTest(AVCodecContext* context) {
    codec_context_ = context;
  }

 private:
  AVCodecContext* codec_context_;
  State state_;

  DISALLOW_COPY_AND_ASSIGN(FFmpegVideoDecodeEngine);
};

class FFmpegVideoDecoder : public DecoderBase<VideoDecoder, VideoFrame> {
 public:
  static FilterFactory* CreateFactory();
  static bool IsMediaFormatSupported(const MediaFormat& media_format);

 private:
  friend class FilterFactoryImpl1<FFmpegVideoDecoder, VideoDecodeEngine*>;
  friend class DecoderPrivateMock;
  friend class FFmpegVideoDecoderTest;
  FRIEND_TEST(FFmpegVideoDecoderTest, FindPtsAndDuration);
  FRIEND_TEST(FFmpegVideoDecoderTest, DoDecode_EnqueueVideoFrameError);
  FRIEND_TEST(FFmpegVideoDecoderTest, DoDecode_FinishEnqueuesEmptyFrames);
  FRIEND_TEST(FFmpegVideoDecoderTest, DoDecode_TestStateTransition);
  FRIEND_TEST(FFmpegVideoDecoderTest, DoSeek);

  // The TimeTuple struct is used to hold the needed timestamp data needed for
  // enqueuing a video frame.
  struct TimeTuple {
    base::TimeDelta timestamp;
    base::TimeDelta duration;
  };

  FFmpegVideoDecoder(VideoDecodeEngine* engine);
  virtual ~FFmpegVideoDecoder();

  // Implement DecoderBase template methods.
  virtual void DoInitialize(DemuxerStream* demuxer_stream, bool* success,
                            Task* done_cb);
  virtual void DoSeek(base::TimeDelta time, Task* done_cb);
  virtual void DoDecode(Buffer* buffer, Task* done_cb);

  virtual bool EnqueueVideoFrame(VideoSurface::Format surface_format,
                                 const TimeTuple& time,
                                 const AVFrame* frame);

  // Create an empty video frame and queue it.
  virtual void EnqueueEmptyFrame();

  virtual void CopyPlane(size_t plane, const VideoSurface& surface,
                         const AVFrame* frame);

  // Methods that pickup after the decode engine has finished its action.
  virtual void OnInitializeComplete(bool* success /* Not owned */,
                                    Task* done_cb);
  virtual void OnDecodeComplete(AVFrame* yuv_frame, bool* got_frame,
                                Task* done_cb);

  // Attempt to get the PTS and Duration for this frame by examining the time
  // info provided via packet stream (stored in |pts_heap|), or the info
  // writen into the AVFrame itself.  If no data is available in either, then
  // attempt to generate a best guess of the pts based on the last known pts.
  //
  // Data inside the AVFrame (if provided) is trusted the most, followed
  // by data from the packet stream.  Estimation based on the |last_pts| is
  // reserved as a last-ditch effort.
  virtual TimeTuple FindPtsAndDuration(const AVRational& time_base,
                                       const PtsHeap& pts_heap,
                                       const TimeTuple& last_pts,
                                       const AVFrame* frame);

  // Signals the pipeline that a decode error occurs, and moves the decoder
  // into the kDecodeFinished state.
  virtual void SignalPipelineError();

  // Injection point for unittest to provide a mock engine.  Takes ownership of
  // the provided engine.
  virtual void SetVideoDecodeEngineForTest(VideoDecodeEngine* engine);

  size_t width_;
  size_t height_;

  PtsHeap pts_heap_;  // Heap of presentation timestamps.
  TimeTuple last_pts_;
  scoped_ptr<AVRational> time_base_;  // Pointer to avoid needing full type.

  enum DecoderState {
    kNormal,
    kFlushCodec,
    kDecodeFinished,
  };

  DecoderState state_;

  scoped_ptr<VideoDecodeEngine> decode_engine_;

  DISALLOW_COPY_AND_ASSIGN(FFmpegVideoDecoder);
};

}  // namespace media

#endif  // MEDIA_FILTERS_FFMPEG_VIDEO_DECODER_H_
