// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/ffmpeg_video_decoder.h"

#include <deque>

#include "base/task.h"
#include "media/base/callback.h"
#include "media/base/filters.h"
#include "media/base/filter_host.h"
#include "media/base/limits.h"
#include "media/base/media_format.h"
#include "media/base/video_frame.h"
#include "media/ffmpeg/ffmpeg_common.h"
#include "media/filters/ffmpeg_interfaces.h"
#include "media/video/ffmpeg_video_decode_engine.h"
#include "media/video/video_decode_context.h"

namespace media {

FFmpegVideoDecoder::FFmpegVideoDecoder(MessageLoop* message_loop,
                                       VideoDecodeContext* decode_context)
    : message_loop_(message_loop),
      time_base_(new AVRational()),
      state_(kUnInitialized),
      decode_engine_(new FFmpegVideoDecodeEngine()),
      decode_context_(decode_context) {
  memset(&info_, 0, sizeof(info_));
}

FFmpegVideoDecoder::~FFmpegVideoDecoder() {
}

void FFmpegVideoDecoder::Initialize(DemuxerStream* demuxer_stream,
                                    FilterCallback* callback,
                                    StatisticsCallback* stats_callback) {
  if (MessageLoop::current() != message_loop_) {
    message_loop_->PostTask(
        FROM_HERE,
        NewRunnableMethod(this,
                          &FFmpegVideoDecoder::Initialize,
                          make_scoped_refptr(demuxer_stream),
                          callback, stats_callback));
    return;
  }

  DCHECK_EQ(MessageLoop::current(), message_loop_);
  DCHECK(!demuxer_stream_);
  DCHECK(!initialize_callback_.get());

  demuxer_stream_ = demuxer_stream;
  initialize_callback_.reset(callback);
  statistics_callback_.reset(stats_callback);

  // Get the AVStream by querying for the provider interface.
  AVStreamProvider* av_stream_provider;
  if (!demuxer_stream->QueryInterface(&av_stream_provider)) {
    VideoCodecInfo info = {0};
    OnInitializeComplete(info);
    return;
  }
  AVStream* av_stream = av_stream_provider->GetAVStream();

  time_base_->den = av_stream->r_frame_rate.num;
  time_base_->num = av_stream->r_frame_rate.den;

  int width = av_stream->codec->coded_width;
  int height = av_stream->codec->coded_height;
  if (width > Limits::kMaxDimension ||
      height > Limits::kMaxDimension ||
      (width * height) > Limits::kMaxCanvas) {
    VideoCodecInfo info = {0};
    OnInitializeComplete(info);
    return;
  }

  VideoCodecConfig config(CodecIDToVideoCodec(av_stream->codec->codec_id),
                          width, height,
                          av_stream->r_frame_rate.num,
                          av_stream->r_frame_rate.den,
                          av_stream->codec->extradata,
                          av_stream->codec->extradata_size);
  state_ = kInitializing;
  decode_engine_->Initialize(message_loop_, this, NULL, config);
}

void FFmpegVideoDecoder::OnInitializeComplete(const VideoCodecInfo& info) {
  // TODO(scherkus): Dedup this from OmxVideoDecoder::OnInitializeComplete.
  DCHECK_EQ(MessageLoop::current(), message_loop_);
  DCHECK(initialize_callback_.get());

  info_ = info;
  AutoCallbackRunner done_runner(initialize_callback_.release());

  if (info.success) {
    media_format_.SetAsInteger(MediaFormat::kWidth,
                               info.stream_info.surface_width);
    media_format_.SetAsInteger(MediaFormat::kHeight,
                               info.stream_info.surface_height);
    media_format_.SetAsInteger(
        MediaFormat::kSurfaceType,
        static_cast<int>(info.stream_info.surface_type));
    media_format_.SetAsInteger(
        MediaFormat::kSurfaceFormat,
        static_cast<int>(info.stream_info.surface_format));
    state_ = kNormal;
  } else {
    host()->SetError(PIPELINE_ERROR_DECODE);
  }
}

void FFmpegVideoDecoder::Stop(FilterCallback* callback) {
  if (MessageLoop::current() != message_loop_) {
    message_loop_->PostTask(FROM_HERE,
                             NewRunnableMethod(this,
                                               &FFmpegVideoDecoder::Stop,
                                               callback));
    return;
  }

  DCHECK_EQ(MessageLoop::current(), message_loop_);
  DCHECK(!uninitialize_callback_.get());

  uninitialize_callback_.reset(callback);
  if (state_ != kUnInitialized)
    decode_engine_->Uninitialize();
  else
    OnUninitializeComplete();
}

void FFmpegVideoDecoder::OnUninitializeComplete() {
  DCHECK_EQ(MessageLoop::current(), message_loop_);
  DCHECK(uninitialize_callback_.get());

  AutoCallbackRunner done_runner(uninitialize_callback_.release());
  state_ = kStopped;

  // TODO(jiesun): Destroy the decoder context.
}

void FFmpegVideoDecoder::Pause(FilterCallback* callback) {
  if (MessageLoop::current() != message_loop_) {
    message_loop_->PostTask(FROM_HERE,
                             NewRunnableMethod(this,
                                               &FFmpegVideoDecoder::Pause,
                                               callback));
    return;
  }

  AutoCallbackRunner done_runner(callback);
  state_ = kPausing;
}

void FFmpegVideoDecoder::Flush(FilterCallback* callback) {
  if (MessageLoop::current() != message_loop_) {
    message_loop_->PostTask(FROM_HERE,
                             NewRunnableMethod(this,
                                               &FFmpegVideoDecoder::Flush,
                                               callback));
    return;
  }

  DCHECK_EQ(MessageLoop::current(), message_loop_);
  DCHECK(!flush_callback_.get());

  state_ = kFlushing;

  FlushBuffers();

  flush_callback_.reset(callback);

  decode_engine_->Flush();
}

void FFmpegVideoDecoder::OnFlushComplete() {
  DCHECK_EQ(MessageLoop::current(), message_loop_);
  DCHECK(flush_callback_.get());

  AutoCallbackRunner done_runner(flush_callback_.release());

  // Everything in the presentation time queue is invalid, clear the queue.
  while (!pts_heap_.IsEmpty())
    pts_heap_.Pop();

  // Mark flush operation had been done.
  state_ = kNormal;
}

void FFmpegVideoDecoder::Seek(base::TimeDelta time,
                              FilterCallback* callback) {
  if (MessageLoop::current() != message_loop_) {
     message_loop_->PostTask(FROM_HERE,
                              NewRunnableMethod(this,
                                                &FFmpegVideoDecoder::Seek,
                                                time,
                                                callback));
     return;
  }

  DCHECK_EQ(MessageLoop::current(), message_loop_);
  DCHECK(!seek_callback_.get());

  seek_callback_.reset(callback);
  decode_engine_->Seek();
}

void FFmpegVideoDecoder::OnSeekComplete() {
  DCHECK_EQ(MessageLoop::current(), message_loop_);
  DCHECK(seek_callback_.get());

  AutoCallbackRunner done_runner(seek_callback_.release());
}

void FFmpegVideoDecoder::OnError() {
  NOTIMPLEMENTED();
}

void FFmpegVideoDecoder::OnFormatChange(VideoStreamInfo stream_info) {
  NOTIMPLEMENTED();
}

void FFmpegVideoDecoder::OnReadComplete(Buffer* buffer_in) {
  scoped_refptr<Buffer> buffer(buffer_in);
  message_loop_->PostTask(
      FROM_HERE,
      NewRunnableMethod(this,
                        &FFmpegVideoDecoder::OnReadCompleteTask,
                        buffer));
}

void FFmpegVideoDecoder::OnReadCompleteTask(scoped_refptr<Buffer> buffer) {
  DCHECK_EQ(MessageLoop::current(), message_loop_);
  DCHECK_NE(state_, kStopped);  // because of Flush() before Stop().

  // During decode, because reads are issued asynchronously, it is possible to
  // receive multiple end of stream buffers since each read is acked. When the
  // first end of stream buffer is read, FFmpeg may still have frames queued
  // up in the decoder so we need to go through the decode loop until it stops
  // giving sensible data.  After that, the decoder should output empty
  // frames.  There are three states the decoder can be in:
  //
  //   kNormal: This is the starting state. Buffers are decoded. Decode errors
  //            are discarded.
  //   kFlushCodec: There isn't any more input data. Call avcodec_decode_video2
  //                until no more data is returned to flush out remaining
  //                frames. The input buffer is ignored at this point.
  //   kDecodeFinished: All calls return empty frames.
  //
  // These are the possible state transitions.
  //
  // kNormal -> kFlushCodec:
  //     When buffer->IsEndOfStream() is first true.
  // kNormal -> kDecodeFinished:
  //     A catastrophic failure occurs, and decoding needs to stop.
  // kFlushCodec -> kDecodeFinished:
  //     When avcodec_decode_video2() returns 0 data or errors out.
  // (any state) -> kNormal:
  //     Any time buffer->IsDiscontinuous() is true.

  // Transition to kFlushCodec on the first end of stream buffer.
  if (state_ == kNormal && buffer->IsEndOfStream()) {
    state_ = kFlushCodec;
  }

  // Push all incoming timestamps into the priority queue as long as we have
  // not yet received an end of stream buffer.  It is important that this line
  // stay below the state transition into kFlushCodec done above.
  //
  // TODO(ajwong): This push logic, along with the pop logic below needs to
  // be reevaluated to correctly handle decode errors.
  if (state_ == kNormal && !buffer->IsEndOfStream() &&
      buffer->GetTimestamp() != kNoTimestamp) {
    pts_heap_.Push(buffer->GetTimestamp());
  }

  // Otherwise, attempt to decode a single frame.
  decode_engine_->ConsumeVideoSample(buffer);
}

const MediaFormat& FFmpegVideoDecoder::media_format() {
  return media_format_;
}

void FFmpegVideoDecoder::ProduceVideoFrame(
    scoped_refptr<VideoFrame> video_frame) {
  if (MessageLoop::current() != message_loop_) {
    message_loop_->PostTask(
        FROM_HERE,
        NewRunnableMethod(this,
                          &FFmpegVideoDecoder::ProduceVideoFrame, video_frame));
    return;
  }

  DCHECK_EQ(MessageLoop::current(), message_loop_);

  // Synchronized flushing before stop should prevent this.
  DCHECK_NE(state_, kStopped);

  // If the decoding is finished, we just always return empty frames.
  if (state_ == kDecodeFinished) {
    // Signal VideoRenderer the end of the stream event.
    scoped_refptr<VideoFrame> empty_frame;
    VideoFrame::CreateEmptyFrame(&empty_frame);
    VideoFrameReady(empty_frame);

    // Fall through, because we still need to keep record of this frame.
  }

  // Notify decode engine the available of new frame.
  decode_engine_->ProduceVideoFrame(video_frame);
}

void FFmpegVideoDecoder::ConsumeVideoFrame(
    scoped_refptr<VideoFrame> video_frame,
    const PipelineStatistics& statistics) {
  DCHECK_EQ(MessageLoop::current(), message_loop_);
  DCHECK_NE(state_, kStopped);

  statistics_callback_->Run(statistics);

  if (video_frame.get()) {
    if (kPausing == state_ || kFlushing == state_) {
      frame_queue_flushed_.push_back(video_frame);
      if (kFlushing == state_)
        FlushBuffers();
      return;
    }

    // If we actually got data back, enqueue a frame.
    last_pts_ = FindPtsAndDuration(*time_base_, &pts_heap_, last_pts_,
                                   video_frame.get());

    video_frame->SetTimestamp(last_pts_.timestamp);
    video_frame->SetDuration(last_pts_.duration);

    VideoFrameReady(video_frame);
  } else {
    // When in kFlushCodec, any errored decode, or a 0-lengthed frame,
    // is taken as a signal to stop decoding.
    if (state_ == kFlushCodec) {
      state_ = kDecodeFinished;

      // Signal VideoRenderer the end of the stream event.
      scoped_refptr<VideoFrame> video_frame;
      VideoFrame::CreateEmptyFrame(&video_frame);
      VideoFrameReady(video_frame);
    }
  }
}

void FFmpegVideoDecoder::ProduceVideoSample(
    scoped_refptr<Buffer> buffer) {
  DCHECK_EQ(MessageLoop::current(), message_loop_);
  DCHECK_NE(state_, kStopped);

  demuxer_stream_->Read(
      NewCallback(this, &FFmpegVideoDecoder::OnReadComplete));
}

FFmpegVideoDecoder::TimeTuple FFmpegVideoDecoder::FindPtsAndDuration(
    const AVRational& time_base,
    PtsHeap* pts_heap,
    const TimeTuple& last_pts,
    const VideoFrame* frame) {
  TimeTuple pts;

  // First search the VideoFrame for the pts. This is the most authoritative.
  // Make a special exclusion for the value pts == 0.  Though this is
  // technically a valid value, it seems a number of FFmpeg codecs will
  // mistakenly always set pts to 0.
  //
  // TODO(scherkus): FFmpegVideoDecodeEngine should be able to detect this
  // situation and set the timestamp to kInvalidTimestamp.
  DCHECK(frame);
  base::TimeDelta timestamp = frame->GetTimestamp();
  if (timestamp != kNoTimestamp &&
      timestamp.ToInternalValue() != 0) {
    pts.timestamp = timestamp;
    // We need to clean up the timestamp we pushed onto the |pts_heap|.
    if (!pts_heap->IsEmpty())
      pts_heap->Pop();
  } else if (!pts_heap->IsEmpty()) {
    // If the frame did not have pts, try to get the pts from the |pts_heap|.
    pts.timestamp = pts_heap->Top();
    pts_heap->Pop();
  } else if (last_pts.timestamp != kNoTimestamp &&
             last_pts.duration != kNoTimestamp) {
    // Guess assuming this frame was the same as the last frame.
    pts.timestamp = last_pts.timestamp + last_pts.duration;
  } else {
    // Now we really have no clue!!!  Mark an invalid timestamp and let the
    // video renderer handle it (i.e., drop frame).
    pts.timestamp = kNoTimestamp;
  }

  // Fill in the duration, using the frame itself as the authoratative source.
  base::TimeDelta duration = frame->GetDuration();
  if (duration != kNoTimestamp &&
      duration.ToInternalValue() != 0) {
    pts.duration = duration;
  } else {
    // Otherwise assume a normal frame duration.
    pts.duration = ConvertTimestamp(time_base, 1);
  }

  return pts;
}

bool FFmpegVideoDecoder::ProvidesBuffer() {
  DCHECK(info_.success);
  return info_.provides_buffers;
}

void FFmpegVideoDecoder::FlushBuffers() {
  while (!frame_queue_flushed_.empty()) {
    scoped_refptr<VideoFrame> video_frame;
    video_frame = frame_queue_flushed_.front();
    frame_queue_flushed_.pop_front();

    // Depends on who own the buffers, we either return it to the renderer
    // or return it to the decode engine.
    if (ProvidesBuffer())
      decode_engine_->ProduceVideoFrame(video_frame);
    else
      VideoFrameReady(video_frame);
  }
}

void FFmpegVideoDecoder::SetVideoDecodeEngineForTest(
    VideoDecodeEngine* engine) {
  decode_engine_.reset(engine);
}

}  // namespace media
