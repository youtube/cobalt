// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FILTERS_FFMPEG_VIDEO_DECODER_H_
#define MEDIA_FILTERS_FFMPEG_VIDEO_DECODER_H_

#include <deque>

#include "base/gtest_prod_util.h"
#include "base/time.h"
#include "media/base/filters.h"
#include "media/base/pts_heap.h"
#include "media/base/video_frame.h"
#include "media/filters/decoder_base.h"
#include "media/video/video_decode_engine.h"

// FFmpeg types.
struct AVRational;

namespace media {

class VideoDecodeEngine;

class FFmpegVideoDecoder : public VideoDecoder,
                           public VideoDecodeEngine::EventHandler {
 public:
  // Holds timestamp and duration data needed for properly enqueuing a frame.
  struct TimeTuple {
    base::TimeDelta timestamp;
    base::TimeDelta duration;
  };

  FFmpegVideoDecoder(MessageLoop* message_loop,
                     VideoDecodeContext* decode_context);
  virtual ~FFmpegVideoDecoder();

  // Filter implementation.
  virtual void Stop(FilterCallback* callback);
  virtual void Seek(base::TimeDelta time, FilterCallback* callback);
  virtual void Pause(FilterCallback* callback);
  virtual void Flush(FilterCallback* callback);

  // Decoder implementation.
  virtual void Initialize(DemuxerStream* demuxer_stream,
                          FilterCallback* callback,
                          StatisticsCallback* stats_callback);
  virtual const MediaFormat& media_format();
  virtual void ProduceVideoFrame(scoped_refptr<VideoFrame> video_frame);
  virtual bool ProvidesBuffer();

 private:
  // VideoDecodeEngine::EventHandler interface.
  virtual void OnInitializeComplete(const VideoCodecInfo& info);
  virtual void OnUninitializeComplete();
  virtual void OnFlushComplete();
  virtual void OnSeekComplete();
  virtual void OnError();
  virtual void OnFormatChange(VideoStreamInfo stream_info);
  virtual void ProduceVideoSample(scoped_refptr<Buffer> buffer);
  virtual void ConsumeVideoFrame(scoped_refptr<VideoFrame> frame,
                                 const PipelineStatistics& statistics);

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

  enum DecoderState {
    kUnInitialized,
    kInitializing,
    kNormal,
    kFlushCodec,
    kDecodeFinished,
    kPausing,
    kFlushing,
    kStopped
  };

  void OnFlushComplete(FilterCallback* callback);
  void OnSeekComplete(FilterCallback* callback);
  void OnReadComplete(Buffer* buffer);

  // TODO(jiesun): until demuxer pass scoped_refptr<Buffer>: we could not merge
  // this with OnReadComplete
  void OnReadCompleteTask(scoped_refptr<Buffer> buffer);

  // Flush the output buffers that we had held in Paused state.
  void FlushBuffers();

  // Attempt to get the PTS and Duration for this frame by examining the time
  // info provided via packet stream (stored in |pts_heap|), or the info
  // written into the AVFrame itself.  If no data is available in either, then
  // attempt to generate a best guess of the pts based on the last known pts.
  //
  // Data inside the AVFrame (if provided) is trusted the most, followed
  // by data from the packet stream.  Estimation based on the |last_pts| is
  // reserved as a last-ditch effort.
  virtual TimeTuple FindPtsAndDuration(const AVRational& time_base,
                                       PtsHeap* pts_heap,
                                       const TimeTuple& last_pts,
                                       const VideoFrame* frame);

  // Injection point for unittest to provide a mock engine.  Takes ownership of
  // the provided engine.
  virtual void SetVideoDecodeEngineForTest(VideoDecodeEngine* engine);

  MessageLoop* message_loop_;
  MediaFormat media_format_;

  PtsHeap pts_heap_;  // Heap of presentation timestamps.
  TimeTuple last_pts_;
  scoped_ptr<AVRational> time_base_;  // Pointer to avoid needing full type.
  DecoderState state_;
  scoped_ptr<VideoDecodeEngine> decode_engine_;
  scoped_ptr<VideoDecodeContext> decode_context_;

  scoped_ptr<FilterCallback> initialize_callback_;
  scoped_ptr<FilterCallback> uninitialize_callback_;
  scoped_ptr<FilterCallback> flush_callback_;
  scoped_ptr<FilterCallback> seek_callback_;
  scoped_ptr<StatisticsCallback> statistics_callback_;

  // Hold video frames when flush happens.
  std::deque<scoped_refptr<VideoFrame> > frame_queue_flushed_;

  VideoCodecInfo info_;

  // Pointer to the demuxer stream that will feed us compressed buffers.
  scoped_refptr<DemuxerStream> demuxer_stream_;

  DISALLOW_COPY_AND_ASSIGN(FFmpegVideoDecoder);
};

}  // namespace media

#endif  // MEDIA_FILTERS_FFMPEG_VIDEO_DECODER_H_
