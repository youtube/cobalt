// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/ffmpeg_video_decoder.h"

#include <deque>

#include "base/task.h"
#include "media/base/filters.h"
#include "media/base/limits.h"
#include "media/base/media_format.h"
#include "media/base/video_frame.h"
#include "media/ffmpeg/ffmpeg_common.h"
#include "media/ffmpeg/ffmpeg_util.h"
#include "media/filters/ffmpeg_interfaces.h"
#include "media/filters/ffmpeg_video_decode_engine.h"
#include "media/filters/video_decode_engine.h"

namespace media {

FFmpegVideoDecoder::FFmpegVideoDecoder(VideoDecodeEngine* engine)
    : width_(0),
      height_(0),
      time_base_(new AVRational()),
      state_(kNormal),
      decode_engine_(engine) {
}

FFmpegVideoDecoder::~FFmpegVideoDecoder() {
}

void FFmpegVideoDecoder::DoInitialize(DemuxerStream* demuxer_stream,
                                      bool* success,
                                      Task* done_cb) {
  AutoTaskRunner done_runner(done_cb);
  *success = false;

  // Get the AVStream by querying for the provider interface.
  AVStreamProvider* av_stream_provider;
  if (!demuxer_stream->QueryInterface(&av_stream_provider)) {
    return;
  }
  AVStream* av_stream = av_stream_provider->GetAVStream();

  time_base_->den = av_stream->r_frame_rate.num;
  time_base_->num = av_stream->r_frame_rate.den;

  // TODO(ajwong): We don't need these extra variables if |media_format_| has
  // them.  Remove.
  width_ = av_stream->codec->width;
  height_ = av_stream->codec->height;
  if (width_ > Limits::kMaxDimension ||
      height_ > Limits::kMaxDimension ||
      (width_ * height_) > Limits::kMaxCanvas) {
    return;
  }

  decode_engine_->Initialize(
      message_loop(),
      av_stream,
      NewCallback(this, &FFmpegVideoDecoder::OnEmptyBufferDone),
      NewCallback(this, &FFmpegVideoDecoder::OnDecodeComplete),
      NewRunnableMethod(this,
                        &FFmpegVideoDecoder::OnInitializeComplete,
                        success,
                        done_runner.release()));
}

void FFmpegVideoDecoder::OnInitializeComplete(bool* success, Task* done_cb) {
  AutoTaskRunner done_runner(done_cb);

  *success = decode_engine_->state() == VideoDecodeEngine::kNormal;
  if (*success) {
    media_format_.SetAsString(MediaFormat::kMimeType,
                              mime_type::kUncompressedVideo);
    media_format_.SetAsInteger(MediaFormat::kWidth, width_);
    media_format_.SetAsInteger(MediaFormat::kHeight, height_);
    media_format_.SetAsInteger(
        MediaFormat::kSurfaceType,
        static_cast<int>(VideoFrame::TYPE_SYSTEM_MEMORY));
    media_format_.SetAsInteger(
        MediaFormat::kSurfaceFormat,
        static_cast<int>(decode_engine_->GetSurfaceFormat()));
  }
}

void FFmpegVideoDecoder::DoStop(Task* done_cb) {
  decode_engine_->Stop(done_cb);
}

void FFmpegVideoDecoder::DoSeek(base::TimeDelta time, Task* done_cb) {
  // Everything in the presentation time queue is invalid, clear the queue.
  while (!pts_heap_.IsEmpty())
    pts_heap_.Pop();

  // We're back where we started.  It should be completely safe to flush here
  // since DecoderBase uses |expecting_discontinuous_| to verify that the next
  // time DoDecode() is called we will have a discontinuous buffer.
  //
  // TODO(ajwong): Should we put a guard here to prevent leaving kError.
  state_ = kNormal;

  decode_engine_->Flush(done_cb);
}

void FFmpegVideoDecoder::DoDecode(Buffer* buffer) {
  // TODO(ajwong): This DoDecode() and OnDecodeComplete() set of functions is
  // too complicated to easily unittest.  The test becomes fragile. Try to
  // find a way to reorganize into smaller units for testing.

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
  //
  // If the decoding is finished, we just always return empty frames.
  if (state_ == kDecodeFinished) {
    EnqueueEmptyFrame();
    OnEmptyBufferDone(NULL);
    return;
  }

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
  if (state_ == kNormal &&
      buffer->GetTimestamp() != StreamSample::kInvalidTimestamp) {
    pts_heap_.Push(buffer->GetTimestamp());
  }

  // Otherwise, attempt to decode a single frame.
  decode_engine_->EmptyThisBuffer(buffer);
}

void FFmpegVideoDecoder::OnDecodeComplete(
    scoped_refptr<VideoFrame> video_frame) {
  if (video_frame.get()) {
    // If we actually got data back, enqueue a frame.
    last_pts_ = FindPtsAndDuration(*time_base_, &pts_heap_, last_pts_,
                                   video_frame.get());

    video_frame->SetTimestamp(last_pts_.timestamp);
    video_frame->SetDuration(last_pts_.duration);
    EnqueueVideoFrame(video_frame);
  } else {
    // When in kFlushCodec, any errored decode, or a 0-lengthed frame,
    // is taken as a signal to stop decoding.
    if (state_ == kFlushCodec) {
      state_ = kDecodeFinished;
      EnqueueEmptyFrame();
    }
  }

  OnEmptyBufferDone(NULL);
}

void FFmpegVideoDecoder::OnEmptyBufferDone(scoped_refptr<Buffer> buffer) {
  // Currently we just ignore the returned buffer.
  DecoderBase<VideoDecoder, VideoFrame>::OnDecodeComplete();
}

void FFmpegVideoDecoder::FillThisBuffer(scoped_refptr<VideoFrame> frame) {
  DecoderBase<VideoDecoder, VideoFrame>::FillThisBuffer(frame);
  // Notify decode engine the available of new frame.
  decode_engine_->FillThisBuffer(frame);
}

void FFmpegVideoDecoder::EnqueueVideoFrame(
    const scoped_refptr<VideoFrame>& video_frame) {
  EnqueueResult(video_frame);
}

void FFmpegVideoDecoder::EnqueueEmptyFrame() {
  scoped_refptr<VideoFrame> video_frame;
  VideoFrame::CreateEmptyFrame(&video_frame);
  EnqueueResult(video_frame);
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
  if (timestamp != StreamSample::kInvalidTimestamp &&
      timestamp.ToInternalValue() != 0) {
    pts.timestamp = timestamp;
    // We need to clean up the timestamp we pushed onto the |pts_heap|.
    if (!pts_heap->IsEmpty())
      pts_heap->Pop();
  } else if (!pts_heap->IsEmpty()) {
    // If the frame did not have pts, try to get the pts from the |pts_heap|.
    pts.timestamp = pts_heap->Top();
    pts_heap->Pop();
  } else if (last_pts.timestamp != StreamSample::kInvalidTimestamp &&
             last_pts.duration != StreamSample::kInvalidTimestamp) {
    // Guess assuming this frame was the same as the last frame.
    pts.timestamp = last_pts.timestamp + last_pts.duration;
  } else {
    // Now we really have no clue!!!  Mark an invalid timestamp and let the
    // video renderer handle it (i.e., drop frame).
    pts.timestamp = StreamSample::kInvalidTimestamp;
  }

  // Fill in the duration, using the frame itself as the authoratative source.
  base::TimeDelta duration = frame->GetDuration();
  if (duration != StreamSample::kInvalidTimestamp &&
      duration.ToInternalValue() != 0) {
    pts.duration = duration;
  } else {
    // Otherwise assume a normal frame duration.
    pts.duration = ConvertTimestamp(time_base, 1);
  }

  return pts;
}

void FFmpegVideoDecoder::SignalPipelineError() {
  host()->SetError(PIPELINE_ERROR_DECODE);
  state_ = kDecodeFinished;
}

void FFmpegVideoDecoder::SetVideoDecodeEngineForTest(
    VideoDecodeEngine* engine) {
  decode_engine_.reset(engine);
}

// static
FilterFactory* FFmpegVideoDecoder::CreateFactory() {
  return new FilterFactoryImpl1<FFmpegVideoDecoder, FFmpegVideoDecodeEngine*>(
      new FFmpegVideoDecodeEngine());
}

// static
bool FFmpegVideoDecoder::IsMediaFormatSupported(const MediaFormat& format) {
  std::string mime_type;
  return format.GetAsString(MediaFormat::kMimeType, &mime_type) &&
      mime_type::kFFmpegVideo == mime_type;
}

}  // namespace media
