// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/ffmpeg_video_decoder.h"

#include "base/bind.h"
#include "base/message_loop.h"
#include "media/base/demuxer_stream.h"
#include "media/base/filter_host.h"
#include "media/base/limits.h"
#include "media/base/video_decoder_config.h"
#include "media/base/video_frame.h"
#include "media/ffmpeg/ffmpeg_common.h"
#include "media/video/ffmpeg_video_decode_engine.h"

namespace media {

FFmpegVideoDecoder::FFmpegVideoDecoder(MessageLoop* message_loop)
    : message_loop_(message_loop),
      state_(kUninitialized),
      decode_engine_(new FFmpegVideoDecodeEngine()) {
}

FFmpegVideoDecoder::~FFmpegVideoDecoder() {}

void FFmpegVideoDecoder::Initialize(DemuxerStream* demuxer_stream,
                                    const base::Closure& callback,
                                    const StatisticsCallback& stats_callback) {
  if (MessageLoop::current() != message_loop_) {
    message_loop_->PostTask(FROM_HERE, base::Bind(
        &FFmpegVideoDecoder::Initialize, this,
        make_scoped_refptr(demuxer_stream), callback, stats_callback));
    return;
  }

  DCHECK(!demuxer_stream_);

  if (!demuxer_stream) {
    host()->SetError(PIPELINE_ERROR_DECODE);
    callback.Run();
    return;
  }

  demuxer_stream_ = demuxer_stream;
  statistics_callback_ = stats_callback;

  const VideoDecoderConfig& config = demuxer_stream->video_decoder_config();

  // TODO(scherkus): this check should go in PipelineImpl prior to creating
  // decoder objects.
  if (!config.IsValidConfig()) {
    DLOG(ERROR) << "Invalid video stream -"
                << " codec: " << config.codec()
                << " format: " << config.format()
                << " coded size: [" << config.coded_size().width()
                << "," << config.coded_size().height() << "]"
                << " visible rect: [" << config.visible_rect().x()
                << "," << config.visible_rect().y()
                << "," << config.visible_rect().width()
                << "," << config.visible_rect().height() << "]"
                << " natural size: [" << config.natural_size().width()
                << "," << config.natural_size().height() << "]"
                << " frame rate: " << config.frame_rate_numerator()
                << "/" << config.frame_rate_denominator()
                << " aspect ratio: " << config.aspect_ratio_numerator()
                << "/" << config.aspect_ratio_denominator();

    host()->SetError(PIPELINE_ERROR_DECODE);
    callback.Run();
    return;
  }

  pts_stream_.Initialize(GetFrameDuration(config));
  natural_size_ = config.natural_size();

  if (!decode_engine_->Initialize(config)) {
    host()->SetError(PIPELINE_ERROR_DECODE);
    callback.Run();
    return;
  }

  state_ = kNormal;
  callback.Run();
}

void FFmpegVideoDecoder::Stop(const base::Closure& callback) {
  if (MessageLoop::current() != message_loop_) {
    message_loop_->PostTask(FROM_HERE, base::Bind(
        &FFmpegVideoDecoder::Stop, this, callback));
    return;
  }

  decode_engine_->Uninitialize();
  state_ = kUninitialized;
  callback.Run();
}

void FFmpegVideoDecoder::Seek(base::TimeDelta time, const FilterStatusCB& cb) {
  if (MessageLoop::current() != message_loop_) {
    message_loop_->PostTask(FROM_HERE, base::Bind(
        &FFmpegVideoDecoder::Seek, this, time, cb));
    return;
  }

  pts_stream_.Seek(time);
  cb.Run(PIPELINE_OK);
}

void FFmpegVideoDecoder::Pause(const base::Closure& callback) {
  if (MessageLoop::current() != message_loop_) {
    message_loop_->PostTask(FROM_HERE, base::Bind(
        &FFmpegVideoDecoder::Pause, this, callback));
    return;
  }

  callback.Run();
}

void FFmpegVideoDecoder::Flush(const base::Closure& callback) {
  if (MessageLoop::current() != message_loop_) {
    message_loop_->PostTask(FROM_HERE, base::Bind(
        &FFmpegVideoDecoder::Flush, this, callback));
    return;
  }

  decode_engine_->Flush();
  pts_stream_.Flush();
  state_ = kNormal;
  callback.Run();
}

void FFmpegVideoDecoder::Read(const ReadCB& callback) {
  // TODO(scherkus): forced task post since VideoRendererBase::FrameReady() will
  // call Read() on FFmpegVideoDecoder's thread as we executed |read_cb_|.
  message_loop_->PostTask(FROM_HERE, base::Bind(
      &FFmpegVideoDecoder::DoRead, this, callback));
}

const gfx::Size& FFmpegVideoDecoder::natural_size() {
  return natural_size_;
}

void FFmpegVideoDecoder::DoRead(const ReadCB& callback) {
  DCHECK_EQ(MessageLoop::current(), message_loop_);
  CHECK(!callback.is_null());
  CHECK(read_cb_.is_null()) << "Overlapping decodes are not supported.";

  // This can happen during shutdown after Stop() has been called.
  if (state_ == kUninitialized) {
    return;
  }

  // Return empty frames if decoding has finished.
  if (state_ == kDecodeFinished) {
    callback.Run(VideoFrame::CreateEmptyFrame());
    return;
  }

  read_cb_ = callback;
  ReadFromDemuxerStream();
}


void FFmpegVideoDecoder::ReadFromDemuxerStream() {
  DCHECK_NE(state_, kUninitialized);
  DCHECK_NE(state_, kDecodeFinished);
  DCHECK(!read_cb_.is_null());

  demuxer_stream_->Read(base::Bind(&FFmpegVideoDecoder::DecodeBuffer, this));
}

void FFmpegVideoDecoder::DecodeBuffer(const scoped_refptr<Buffer>& buffer) {
  // TODO(scherkus): forced task post since FFmpegDemuxerStream::Read() can
  // immediately execute our callback on FFmpegVideoDecoder's thread.
  message_loop_->PostTask(FROM_HERE, base::Bind(
      &FFmpegVideoDecoder::DoDecodeBuffer, this, buffer));
}

void FFmpegVideoDecoder::DoDecodeBuffer(const scoped_refptr<Buffer>& buffer) {
  DCHECK_EQ(MessageLoop::current(), message_loop_);
  DCHECK_NE(state_, kUninitialized);
  DCHECK_NE(state_, kDecodeFinished);
  DCHECK(!read_cb_.is_null());

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
  //     A decoding error occurs and decoding needs to stop.
  // kFlushCodec -> kDecodeFinished:
  //     When avcodec_decode_video2() returns 0 data or errors out.
  // (any state) -> kNormal:
  //     Any time Flush() is called.

  // Transition to kFlushCodec on the first end of stream buffer.
  if (state_ == kNormal && buffer->IsEndOfStream()) {
    state_ = kFlushCodec;
  }

  // Push all incoming timestamps into the priority queue as long as we have
  // not yet received an end of stream buffer.  It is important that this line
  // stay below the state transition into kFlushCodec done above.
  if (state_ == kNormal) {
    pts_stream_.EnqueuePts(buffer.get());
  }

  scoped_refptr<VideoFrame> video_frame;
  if (!decode_engine_->Decode(buffer, &video_frame)) {
    state_ = kDecodeFinished;
    DeliverFrame(VideoFrame::CreateEmptyFrame());
    host()->SetError(PIPELINE_ERROR_DECODE);
    return;
  }

  // Any successful decode counts!
  if (buffer->GetDataSize()) {
    PipelineStatistics statistics;
    statistics.video_bytes_decoded = buffer->GetDataSize();
    statistics_callback_.Run(statistics);
  }

  // If we didn't get a frame then we've either completely finished decoding or
  // we need more data.
  if (!video_frame) {
    if (state_ == kFlushCodec) {
      state_ = kDecodeFinished;
      DeliverFrame(VideoFrame::CreateEmptyFrame());
      return;
    }

    ReadFromDemuxerStream();
    return;
  }

  // If we got a frame make sure its timestamp is correct before sending it off.
  pts_stream_.UpdatePtsAndDuration(video_frame.get());
  video_frame->SetTimestamp(pts_stream_.current_pts());
  video_frame->SetDuration(pts_stream_.current_duration());

  DeliverFrame(video_frame);
}

void FFmpegVideoDecoder::DeliverFrame(
    const scoped_refptr<VideoFrame>& video_frame) {
  // Reset the callback before running to protect against reentrancy.
  ReadCB read_cb = read_cb_;
  read_cb_.Reset();
  read_cb.Run(video_frame);
}

}  // namespace media
