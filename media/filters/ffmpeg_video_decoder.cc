// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/ffmpeg_video_decoder.h"

#include <algorithm>
#include <string>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/location.h"
#include "base/message_loop_proxy.h"
#include "base/string_number_conversions.h"
#include "media/base/bind_to_loop.h"
#include "media/base/decoder_buffer.h"
#include "media/base/demuxer_stream.h"
#include "media/base/limits.h"
#include "media/base/media_switches.h"
#include "media/base/pipeline.h"
#include "media/base/video_decoder_config.h"
#include "media/base/video_frame.h"
#include "media/base/video_util.h"
#include "media/ffmpeg/ffmpeg_common.h"
#include "media/filters/ffmpeg_glue.h"

namespace media {

// Always try to use three threads for video decoding.  There is little reason
// not to since current day CPUs tend to be multi-core and we measured
// performance benefits on older machines such as P4s with hyperthreading.
//
// Handling decoding on separate threads also frees up the pipeline thread to
// continue processing. Although it'd be nice to have the option of a single
// decoding thread, FFmpeg treats having one thread the same as having zero
// threads (i.e., avcodec_decode_video() will execute on the calling thread).
// Yet another reason for having two threads :)
static const int kDecodeThreads = 2;
static const int kMaxDecodeThreads = 16;

// Returns the number of threads given the FFmpeg CodecID. Also inspects the
// command line for a valid --video-threads flag.
static int GetThreadCount(CodecID codec_id) {
  // Refer to http://crbug.com/93932 for tsan suppressions on decoding.
  int decode_threads = kDecodeThreads;

  const CommandLine* cmd_line = CommandLine::ForCurrentProcess();
  std::string threads(cmd_line->GetSwitchValueASCII(switches::kVideoThreads));
  if (threads.empty() || !base::StringToInt(threads, &decode_threads))
    return decode_threads;

  decode_threads = std::max(decode_threads, 0);
  decode_threads = std::min(decode_threads, kMaxDecodeThreads);
  return decode_threads;
}

FFmpegVideoDecoder::FFmpegVideoDecoder(
    const scoped_refptr<base::MessageLoopProxy>& message_loop)
    : message_loop_(message_loop),
      state_(kUninitialized),
      codec_context_(NULL),
      av_frame_(NULL) {
}

int FFmpegVideoDecoder::GetVideoBuffer(AVCodecContext* codec_context,
                                       AVFrame* frame) {
  // Don't use |codec_context_| here! With threaded decoding,
  // it will contain unsynchronized width/height/pix_fmt values,
  // whereas |codec_context| contains the current threads's
  // updated width/height/pix_fmt, which can change for adaptive
  // content.
  VideoFrame::Format format = PixelFormatToVideoFormat(codec_context->pix_fmt);
  if (format == VideoFrame::INVALID)
    return AVERROR(EINVAL);
  DCHECK(format == VideoFrame::YV12 || format == VideoFrame::YV16);

  gfx::Size size(codec_context->width, codec_context->height);
  int ret;
  if ((ret = av_image_check_size(size.width(), size.height(), 0, NULL)) < 0)
    return ret;

  gfx::Size natural_size;
  if (codec_context->sample_aspect_ratio.num > 0) {
    natural_size = GetNaturalSize(size,
                                  codec_context->sample_aspect_ratio.num,
                                  codec_context->sample_aspect_ratio.den);
  } else {
    natural_size = demuxer_stream_->video_decoder_config().natural_size();
  }

  if (!VideoFrame::IsValidConfig(format, size, gfx::Rect(size), natural_size))
    return AVERROR(EINVAL);

  scoped_refptr<VideoFrame> video_frame =
      VideoFrame::CreateFrame(format, size, gfx::Rect(size), natural_size,
                              kNoTimestamp());

  for (int i = 0; i < 3; i++) {
    frame->base[i] = video_frame->data(i);
    frame->data[i] = video_frame->data(i);
    frame->linesize[i] = video_frame->stride(i);
  }

  frame->opaque = NULL;
  video_frame.swap(reinterpret_cast<VideoFrame**>(&frame->opaque));
  frame->type = FF_BUFFER_TYPE_USER;
  frame->pkt_pts = codec_context->pkt ? codec_context->pkt->pts :
                                        AV_NOPTS_VALUE;
  frame->width = codec_context->width;
  frame->height = codec_context->height;
  frame->format = codec_context->pix_fmt;

  return 0;
}

static int GetVideoBufferImpl(AVCodecContext* s, AVFrame* frame) {
  FFmpegVideoDecoder* vd = static_cast<FFmpegVideoDecoder*>(s->opaque);
  return vd->GetVideoBuffer(s, frame);
}

static void ReleaseVideoBufferImpl(AVCodecContext* s, AVFrame* frame) {
  scoped_refptr<VideoFrame> video_frame;
  video_frame.swap(reinterpret_cast<VideoFrame**>(&frame->opaque));

  // The FFmpeg API expects us to zero the data pointers in
  // this callback
  memset(frame->data, 0, sizeof(frame->data));
  frame->opaque = NULL;
}

void FFmpegVideoDecoder::Initialize(const scoped_refptr<DemuxerStream>& stream,
                                    const PipelineStatusCB& status_cb,
                                    const StatisticsCB& statistics_cb) {
  DCHECK(message_loop_->BelongsToCurrentThread());
  PipelineStatusCB initialize_cb = BindToCurrentLoop(status_cb);

  FFmpegGlue::InitializeFFmpeg();
  DCHECK(!demuxer_stream_) << "Already initialized.";

  if (!stream) {
    initialize_cb.Run(PIPELINE_ERROR_DECODE);
    return;
  }

  demuxer_stream_ = stream;
  statistics_cb_ = statistics_cb;

  if (!ConfigureDecoder()) {
    initialize_cb.Run(DECODER_ERROR_NOT_SUPPORTED);
    return;
  }

  // Success!
  state_ = kNormal;
  initialize_cb.Run(PIPELINE_OK);
}

void FFmpegVideoDecoder::Read(const ReadCB& read_cb) {
  DCHECK(message_loop_->BelongsToCurrentThread());
  DCHECK(!read_cb.is_null());
  CHECK_NE(state_, kUninitialized);
  CHECK(read_cb_.is_null()) << "Overlapping decodes are not supported.";
  read_cb_ = BindToCurrentLoop(read_cb);

  // Return empty frames if decoding has finished.
  if (state_ == kDecodeFinished) {
    base::ResetAndReturn(&read_cb_).Run(kOk, VideoFrame::CreateEmptyFrame());
    return;
  }

  ReadFromDemuxerStream();
}

void FFmpegVideoDecoder::Reset(const base::Closure& closure) {
  DCHECK(message_loop_->BelongsToCurrentThread());
  DCHECK(reset_cb_.is_null());
  reset_cb_ = BindToCurrentLoop(closure);

  // Defer the reset if a read is pending.
  if (!read_cb_.is_null())
    return;

  DoReset();
}

void FFmpegVideoDecoder::DoReset() {
  DCHECK(read_cb_.is_null());

  avcodec_flush_buffers(codec_context_);
  state_ = kNormal;
  base::ResetAndReturn(&reset_cb_).Run();
}

void FFmpegVideoDecoder::Stop(const base::Closure& closure) {
  DCHECK(message_loop_->BelongsToCurrentThread());
  base::ScopedClosureRunner runner(BindToCurrentLoop(closure));

  if (state_ == kUninitialized)
    return;

  if (!read_cb_.is_null())
    base::ResetAndReturn(&read_cb_).Run(kOk, NULL);

  ReleaseFFmpegResources();
  state_ = kUninitialized;
}

FFmpegVideoDecoder::~FFmpegVideoDecoder() {
  DCHECK_EQ(kUninitialized, state_);
  DCHECK(!codec_context_);
  DCHECK(!av_frame_);
}

void FFmpegVideoDecoder::ReadFromDemuxerStream() {
  DCHECK_NE(state_, kUninitialized);
  DCHECK_NE(state_, kDecodeFinished);
  DCHECK(!read_cb_.is_null());

  demuxer_stream_->Read(base::Bind(&FFmpegVideoDecoder::BufferReady, this));
}

void FFmpegVideoDecoder::BufferReady(
    DemuxerStream::Status status,
    const scoped_refptr<DecoderBuffer>& buffer) {
  DCHECK(message_loop_->BelongsToCurrentThread());
  DCHECK_NE(state_, kDecodeFinished);
  DCHECK_EQ(status != DemuxerStream::kOk, !buffer) << status;

  if (state_ == kUninitialized)
    return;

  DCHECK(!read_cb_.is_null());

  if (status == DemuxerStream::kConfigChanged) {
    if (!ConfigureDecoder()) {
      base::ResetAndReturn(&read_cb_).Run(kDecodeError, NULL);
      state_ = kDecodeFinished;
      if (!reset_cb_.is_null())
        base::ResetAndReturn(&reset_cb_).Run();
      return;
    }

    if (reset_cb_.is_null()) {
      ReadFromDemuxerStream();
      return;
    }
  }

  if (!reset_cb_.is_null()) {
    base::ResetAndReturn(&read_cb_).Run(kOk, NULL);
    DoReset();
    return;
  }

  if (status == DemuxerStream::kAborted) {
    base::ResetAndReturn(&read_cb_).Run(kOk, NULL);
    return;
  }

  DCHECK_EQ(status, DemuxerStream::kOk);
  DecodeBuffer(buffer);
}

void FFmpegVideoDecoder::DecodeBuffer(
    const scoped_refptr<DecoderBuffer>& buffer) {
  DCHECK(message_loop_->BelongsToCurrentThread());
  DCHECK_NE(state_, kUninitialized);
  DCHECK_NE(state_, kDecodeFinished);
  DCHECK(reset_cb_.is_null());
  DCHECK(!read_cb_.is_null());
  DCHECK(buffer);

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
  //     Any time Reset() is called.

  // Transition to kFlushCodec on the first end of stream buffer.
  if (state_ == kNormal && buffer->IsEndOfStream()) {
    state_ = kFlushCodec;
  }

  scoped_refptr<VideoFrame> video_frame;
  if (!Decode(buffer, &video_frame)) {
    state_ = kDecodeFinished;
    base::ResetAndReturn(&read_cb_).Run(kDecodeError, NULL);
    return;
  }

  // Any successful decode counts!
  if (buffer->GetDataSize()) {
    PipelineStatistics statistics;
    statistics.video_bytes_decoded = buffer->GetDataSize();
    statistics_cb_.Run(statistics);
  }

  // If we didn't get a frame then we've either completely finished decoding or
  // we need more data.
  if (!video_frame) {
    if (state_ == kFlushCodec) {
      state_ = kDecodeFinished;
      base::ResetAndReturn(&read_cb_).Run(kOk, VideoFrame::CreateEmptyFrame());
      return;
    }

    ReadFromDemuxerStream();
    return;
  }

  base::ResetAndReturn(&read_cb_).Run(kOk, video_frame);
}

bool FFmpegVideoDecoder::Decode(
    const scoped_refptr<DecoderBuffer>& buffer,
    scoped_refptr<VideoFrame>* video_frame) {
  DCHECK(video_frame);

  // Create a packet for input data.
  // Due to FFmpeg API changes we no longer have const read-only pointers.
  AVPacket packet;
  av_init_packet(&packet);
  packet.data = const_cast<uint8*>(buffer->GetData());
  packet.size = buffer->GetDataSize();

  // Let FFmpeg handle presentation timestamp reordering.
  codec_context_->reordered_opaque = buffer->GetTimestamp().InMicroseconds();

  // Reset frame to default values.
  avcodec_get_frame_defaults(av_frame_);

  // This is for codecs not using get_buffer to initialize
  // |av_frame_->reordered_opaque|
  av_frame_->reordered_opaque = codec_context_->reordered_opaque;

  int frame_decoded = 0;
  int result = avcodec_decode_video2(codec_context_,
                                     av_frame_,
                                     &frame_decoded,
                                     &packet);
  // Log the problem if we can't decode a video frame and exit early.
  if (result < 0) {
    LOG(ERROR) << "Error decoding a video frame with timestamp: "
               << buffer->GetTimestamp().InMicroseconds() << " us, duration: "
               << buffer->GetDuration().InMicroseconds() << " us, packet size: "
               << buffer->GetDataSize() << " bytes";
    *video_frame = NULL;
    return false;
  }

  // If no frame was produced then signal that more data is required to
  // produce more frames. This can happen under two circumstances:
  //   1) Decoder was recently initialized/flushed
  //   2) End of stream was reached and all internal frames have been output
  if (frame_decoded == 0) {
    *video_frame = NULL;
    return true;
  }

  // TODO(fbarchard): Work around for FFmpeg http://crbug.com/27675
  // The decoder is in a bad state and not decoding correctly.
  // Checking for NULL avoids a crash in CopyPlane().
  if (!av_frame_->data[VideoFrame::kYPlane] ||
      !av_frame_->data[VideoFrame::kUPlane] ||
      !av_frame_->data[VideoFrame::kVPlane]) {
    LOG(ERROR) << "Video frame was produced yet has invalid frame data.";
    *video_frame = NULL;
    return false;
  }

  if (!av_frame_->opaque) {
    LOG(ERROR) << "VideoFrame object associated with frame data not set.";
    return false;
  }
  *video_frame = static_cast<VideoFrame*>(av_frame_->opaque);

  (*video_frame)->SetTimestamp(
      base::TimeDelta::FromMicroseconds(av_frame_->reordered_opaque));

  return true;
}

void FFmpegVideoDecoder::ReleaseFFmpegResources() {
  if (codec_context_) {
    av_free(codec_context_->extradata);
    avcodec_close(codec_context_);
    av_free(codec_context_);
    codec_context_ = NULL;
  }
  if (av_frame_) {
    av_free(av_frame_);
    av_frame_ = NULL;
  }
}

bool FFmpegVideoDecoder::ConfigureDecoder() {
  const VideoDecoderConfig& config = demuxer_stream_->video_decoder_config();

  if (!config.IsValidConfig()) {
    DLOG(ERROR) << "Invalid video stream - " << config.AsHumanReadableString();
    return false;
  }

  if (config.is_encrypted()) {
    DLOG(ERROR) << "Encrypted video stream not supported.";
    return false;
  }

  // Release existing decoder resources if necessary.
  ReleaseFFmpegResources();

  // Initialize AVCodecContext structure.
  codec_context_ = avcodec_alloc_context3(NULL);
  VideoDecoderConfigToAVCodecContext(config, codec_context_);

  // Enable motion vector search (potentially slow), strong deblocking filter
  // for damaged macroblocks, and set our error detection sensitivity.
  codec_context_->error_concealment = FF_EC_GUESS_MVS | FF_EC_DEBLOCK;
  codec_context_->thread_count = GetThreadCount(codec_context_->codec_id);
  codec_context_->opaque = this;
  codec_context_->flags |= CODEC_FLAG_EMU_EDGE;
  codec_context_->get_buffer = GetVideoBufferImpl;
  codec_context_->release_buffer = ReleaseVideoBufferImpl;

  AVCodec* codec = avcodec_find_decoder(codec_context_->codec_id);
  if (!codec || avcodec_open2(codec_context_, codec, NULL) < 0) {
    ReleaseFFmpegResources();
    return false;
  }

  av_frame_ = avcodec_alloc_frame();
  return true;
}

}  // namespace media
