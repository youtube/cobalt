// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/ffmpeg_video_decoder.h"

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/message_loop.h"
#include "base/string_number_conversions.h"
#include "media/base/decoder_buffer.h"
#include "media/base/decryptor.h"
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
    const base::Callback<MessageLoop*()>& message_loop_cb)
    : message_loop_factory_cb_(message_loop_cb),
      message_loop_(NULL),
      state_(kUninitialized),
      codec_context_(NULL),
      av_frame_(NULL),
      frame_rate_numerator_(0),
      frame_rate_denominator_(0),
      decryptor_(NULL) {
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

  int width = codec_context->width;
  int height = codec_context->height;
  int ret;
  if ((ret = av_image_check_size(width, height, 0, NULL)) < 0)
    return ret;

  scoped_refptr<VideoFrame> video_frame =
      VideoFrame::CreateFrame(format, width, height,
                              kNoTimestamp(), kNoTimestamp());

  for (int i = 0; i < 3; i++) {
    frame->base[i] = video_frame->data(i);
    frame->data[i] = video_frame->data(i);
    frame->linesize[i] = video_frame->stride(i);
  }

  frame->opaque = video_frame.release();
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
  // We're releasing the reference to the buffer allocated in
  // GetVideoBuffer() here, so the explicit Release() here is
  // intentional.
  scoped_refptr<VideoFrame> video_frame =
      static_cast<VideoFrame*>(frame->opaque);
  video_frame->Release();

  // The FFmpeg API expects us to zero the data pointers in
  // this callback
  memset(frame->data, 0, sizeof(frame->data));
  frame->opaque = NULL;
}

void FFmpegVideoDecoder::Initialize(const scoped_refptr<DemuxerStream>& stream,
                                    const PipelineStatusCB& status_cb,
                                    const StatisticsCB& statistics_cb) {
  // Ensure FFmpeg has been initialized
  FFmpegGlue::GetInstance();

  if (!message_loop_) {
    message_loop_ = message_loop_factory_cb_.Run();
    message_loop_factory_cb_.Reset();

    message_loop_->PostTask(FROM_HERE, base::Bind(
        &FFmpegVideoDecoder::Initialize, this,
        stream, status_cb, statistics_cb));
    return;
  }

  DCHECK_EQ(MessageLoop::current(), message_loop_);
  DCHECK(!demuxer_stream_);

  if (!stream) {
    status_cb.Run(PIPELINE_ERROR_DECODE);
    return;
  }

  demuxer_stream_ = stream;
  statistics_cb_ = statistics_cb;

  const VideoDecoderConfig& config = stream->video_decoder_config();

  // TODO(scherkus): this check should go in Pipeline prior to creating
  // decoder objects.
  if (!config.IsValidConfig()) {
    DLOG(ERROR) << "Invalid video stream - " << config.AsHumanReadableString();
    status_cb.Run(PIPELINE_ERROR_DECODE);
    return;
  }

  // Initialize AVCodecContext structure.
  codec_context_ = avcodec_alloc_context3(NULL);
  VideoDecoderConfigToAVCodecContext(config, codec_context_);

  // Enable motion vector search (potentially slow), strong deblocking filter
  // for damaged macroblocks, and set our error detection sensitivity.
  codec_context_->error_concealment = FF_EC_GUESS_MVS | FF_EC_DEBLOCK;
  codec_context_->err_recognition = AV_EF_CAREFUL;
  codec_context_->thread_count = GetThreadCount(codec_context_->codec_id);
  codec_context_->opaque = this;
  codec_context_->flags |= CODEC_FLAG_EMU_EDGE;
  codec_context_->get_buffer = GetVideoBufferImpl;
  codec_context_->release_buffer = ReleaseVideoBufferImpl;

  AVCodec* codec = avcodec_find_decoder(codec_context_->codec_id);
  if (!codec) {
    status_cb.Run(PIPELINE_ERROR_DECODE);
    return;
  }

  if (avcodec_open2(codec_context_, codec, NULL) < 0) {
    status_cb.Run(PIPELINE_ERROR_DECODE);
    return;
  }

  // Success!
  state_ = kNormal;
  av_frame_ = avcodec_alloc_frame();
  natural_size_ = config.natural_size();
  frame_rate_numerator_ = config.frame_rate_numerator();
  frame_rate_denominator_ = config.frame_rate_denominator();
  status_cb.Run(PIPELINE_OK);
}

void FFmpegVideoDecoder::Read(const ReadCB& read_cb) {
  // Complete operation asynchronously on different stack of execution as per
  // the API contract of VideoDecoder::Read()
  message_loop_->PostTask(FROM_HERE, base::Bind(
      &FFmpegVideoDecoder::DoRead, this, read_cb));
}

void FFmpegVideoDecoder::Reset(const base::Closure& closure) {
  if (MessageLoop::current() != message_loop_) {
    message_loop_->PostTask(FROM_HERE, base::Bind(
        &FFmpegVideoDecoder::Reset, this, closure));
    return;
  }

  reset_cb_ = closure;

  // Defer the reset if a read is pending.
  if (!read_cb_.is_null())
    return;

  DoReset();
}

void FFmpegVideoDecoder::DoReset() {
  DCHECK(read_cb_.is_null());

  avcodec_flush_buffers(codec_context_);
  state_ = kNormal;
  reset_cb_.Run();
  reset_cb_.Reset();
}

void FFmpegVideoDecoder::Stop(const base::Closure& closure) {
  if (MessageLoop::current() != message_loop_) {
    message_loop_->PostTask(FROM_HERE, base::Bind(
        &FFmpegVideoDecoder::Stop, this, closure));
    return;
  }

  ReleaseFFmpegResources();
  state_ = kUninitialized;
  closure.Run();
}

const gfx::Size& FFmpegVideoDecoder::natural_size() {
  return natural_size_;
}

void FFmpegVideoDecoder::set_decryptor(Decryptor* decryptor) {
  DCHECK_EQ(state_, kUninitialized);
  decryptor_ = decryptor;
}

FFmpegVideoDecoder::~FFmpegVideoDecoder() {
  ReleaseFFmpegResources();
}

void FFmpegVideoDecoder::DoRead(const ReadCB& read_cb) {
  DCHECK_EQ(MessageLoop::current(), message_loop_);
  DCHECK(!read_cb.is_null());
  CHECK(read_cb_.is_null()) << "Overlapping decodes are not supported.";

  // This can happen during shutdown after Stop() has been called.
  if (state_ == kUninitialized) {
    return;
  }

  // Return empty frames if decoding has finished.
  if (state_ == kDecodeFinished) {
    read_cb.Run(kOk, VideoFrame::CreateEmptyFrame());
    return;
  }

  read_cb_ = read_cb;
  ReadFromDemuxerStream();
}


void FFmpegVideoDecoder::ReadFromDemuxerStream() {
  DCHECK_NE(state_, kUninitialized);
  DCHECK_NE(state_, kDecodeFinished);
  DCHECK(!read_cb_.is_null());

  demuxer_stream_->Read(base::Bind(&FFmpegVideoDecoder::DecodeBuffer, this));
}

void FFmpegVideoDecoder::DecodeBuffer(
    const scoped_refptr<DecoderBuffer>& buffer) {
  // TODO(scherkus): fix FFmpegDemuxerStream::Read() to not execute our read
  // callback on the same execution stack so we can get rid of forced task post.
  message_loop_->PostTask(FROM_HERE, base::Bind(
      &FFmpegVideoDecoder::DoDecodeBuffer, this, buffer));
}

void FFmpegVideoDecoder::DoDecodeBuffer(
    const scoped_refptr<DecoderBuffer>& buffer) {
  DCHECK_EQ(MessageLoop::current(), message_loop_);
  DCHECK_NE(state_, kUninitialized);
  DCHECK_NE(state_, kDecodeFinished);
  DCHECK(!read_cb_.is_null());

  if (!reset_cb_.is_null()) {
    DeliverFrame(NULL);
    DoReset();
    return;
  }

  if (!buffer) {
    DeliverFrame(NULL);
    return;
  }

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

  scoped_refptr<DecoderBuffer> unencrypted_buffer = buffer;
  if (buffer->GetDecryptConfig() && buffer->GetDataSize()) {
    unencrypted_buffer = decryptor_->Decrypt(buffer);

    if (!unencrypted_buffer || !unencrypted_buffer->GetDataSize()) {
      state_ = kDecodeFinished;
      base::ResetAndReturn(&read_cb_).Run(kDecryptError, NULL);
      return;
    }
  }

  scoped_refptr<VideoFrame> video_frame;
  if (!Decode(unencrypted_buffer, &video_frame)) {
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
      DeliverFrame(VideoFrame::CreateEmptyFrame());
      return;
    }

    ReadFromDemuxerStream();
    return;
  }

  DeliverFrame(video_frame);
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

  if (frame_rate_numerator_ == 0) {
    // A framerate of zero indicates that no timing information was available
    // during initial stream demuxing, and that the framerate should be inferred
    // from the first frame's duration.
    frame_rate_numerator_ = buffer->GetDuration().InMicroseconds();
    frame_rate_denominator_ = base::Time::kMicrosecondsPerSecond;
  }

  // Determine timestamp and calculate the duration based on the repeat
  // picture count. According to FFmpeg docs, the total duration can be
  // calculated as follows:
  //   fps = 1 / time_base
  //
  //   duration = (1 / fps) + (repeat_pict) / (2 * fps)
  //            = (2 + repeat_pict) / (2 * fps)
  //            = (2 + repeat_pict) / (2 * (1 / time_base))
  DCHECK_LE(av_frame_->repeat_pict, 2);  // Sanity check.
  AVRational doubled_time_base;
  doubled_time_base.num = frame_rate_denominator_;
  doubled_time_base.den = frame_rate_numerator_ * 2;

  (*video_frame)->SetTimestamp(
      base::TimeDelta::FromMicroseconds(av_frame_->reordered_opaque));
  (*video_frame)->SetDuration(
      ConvertFromTimeBase(doubled_time_base, 2 + av_frame_->repeat_pict));

  return true;
}

void FFmpegVideoDecoder::DeliverFrame(
    const scoped_refptr<VideoFrame>& video_frame) {
  // Reset the callback before running to protect against reentrancy.
  base::ResetAndReturn(&read_cb_).Run(kOk, video_frame);
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

}  // namespace media
