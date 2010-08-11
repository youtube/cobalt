// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FILTERS_FFMPEG_VIDEO_DECODER_H_
#define MEDIA_FILTERS_FFMPEG_VIDEO_DECODER_H_

#include "base/gtest_prod_util.h"
#include "base/time.h"
#include "media/base/pts_heap.h"
#include "media/base/video_frame.h"
#include "media/filters/decoder_base.h"

// FFmpeg types.
struct AVRational;

namespace media {

class VideoDecodeEngine;

class FFmpegVideoDecoder : public DecoderBase<VideoDecoder, VideoFrame> {
 public:
  explicit FFmpegVideoDecoder(VideoDecodeEngine* engine);
  virtual ~FFmpegVideoDecoder();

  static FilterFactory* CreateFactory();
  static bool IsMediaFormatSupported(const MediaFormat& media_format);

 protected:
  virtual void DoInitialize(DemuxerStream* demuxer_stream, bool* success,
                            Task* done_cb);
  virtual void DoStop(Task* done_cb);
  virtual void DoSeek(base::TimeDelta time, Task* done_cb);
  virtual void DoDecode(Buffer* input);

 protected:
  virtual void OnEmptyBufferDone(scoped_refptr<Buffer> buffer);
  virtual void FillThisBuffer(scoped_refptr<VideoFrame> frame);

 private:
  friend class FilterFactoryImpl1<FFmpegVideoDecoder, VideoDecodeEngine*>;
  friend class DecoderPrivateMock;
  friend class FFmpegVideoDecoderTest;
  FRIEND_TEST_ALL_PREFIXES(FFmpegVideoDecoderTest, FindPtsAndDuration);
  FRIEND_TEST_ALL_PREFIXES(FFmpegVideoDecoderTest,
                           DoDecode_EnqueueVideoFrameError);
  FRIEND_TEST_ALL_PREFIXES(FFmpegVideoDecoderTest,
                           DoDecode_FinishEnqueuesEmptyFrames);
  FRIEND_TEST_ALL_PREFIXES(FFmpegVideoDecoderTest,
                           DoDecode_TestStateTransition);
  FRIEND_TEST_ALL_PREFIXES(FFmpegVideoDecoderTest, DoSeek);

  // The TimeTuple struct is used to hold the needed timestamp data needed for
  // enqueuing a video frame.
  struct TimeTuple {
    base::TimeDelta timestamp;
    base::TimeDelta duration;
  };

  enum DecoderState {
    kNormal,
    kFlushCodec,
    kDecodeFinished,
  };

  // Implement DecoderBase template methods.
  virtual void EnqueueVideoFrame(const scoped_refptr<VideoFrame>& video_frame);

  // Create an empty video frame and queue it.
  virtual void EnqueueEmptyFrame();

  // Methods that pickup after the decode engine has finished its action.
  virtual void OnInitializeComplete(bool* success /* Not owned */,
                                    Task* done_cb);

  virtual void OnDecodeComplete(scoped_refptr<VideoFrame> video_frame);

  // Attempt to get the PTS and Duration for this frame by examining the time
  // info provided via packet stream (stored in |pts_heap|), or the info
  // writen into the AVFrame itself.  If no data is available in either, then
  // attempt to generate a best guess of the pts based on the last known pts.
  //
  // Data inside the AVFrame (if provided) is trusted the most, followed
  // by data from the packet stream.  Estimation based on the |last_pts| is
  // reserved as a last-ditch effort.
  virtual TimeTuple FindPtsAndDuration(const AVRational& time_base,
                                       PtsHeap* pts_heap,
                                       const TimeTuple& last_pts,
                                       const VideoFrame* frame);

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
  DecoderState state_;
  scoped_ptr<VideoDecodeEngine> decode_engine_;

  DISALLOW_COPY_AND_ASSIGN(FFmpegVideoDecoder);
};

}  // namespace media

#endif  // MEDIA_FILTERS_FFMPEG_VIDEO_DECODER_H_
