/*
 * Copyright 2014 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include "media/filters/shell_video_decoder_impl.h"

#include <limits.h>  // for ULLONG_MAX

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/debug/trace_event.h"
#include "base/logging.h"
#include "lb_shell/lb_shell_constants.h"
#include "lb_video_decoder.h"
#include "media/base/bind_to_loop.h"
#include "media/base/pipeline_status.h"
#include "media/base/shell_buffer_factory.h"
#include "media/base/shell_media_statistics.h"
#include "media/base/video_decoder_config.h"
#include "media/base/video_frame.h"

using LB::LBVideoDecoder;

namespace media {

//==============================================================================
// Static ShellVideoDecoder Methods
//

// static
ShellVideoDecoder* ShellVideoDecoder::Create(
    const scoped_refptr<base::MessageLoopProxy>& message_loop) {
  return new ShellVideoDecoderImpl(message_loop);
}

//==============================================================================
// ShellVideoDecoderImpl
//

ShellVideoDecoderImpl::ShellVideoDecoderImpl(
    const scoped_refptr<base::MessageLoopProxy>& message_loop)
    : state_(kUninitialized)
    , media_pipeline_message_loop_(message_loop)
    , decoder_thread_("Video Decoder") {
}

ShellVideoDecoderImpl::~ShellVideoDecoderImpl() {
#if !defined(__LB_SHELL__FOR_RELEASE__)
  if (decoder_thread_.message_loop())
    decoder_thread_.message_loop()->AssertIdle();
#endif
}

void ShellVideoDecoderImpl::Initialize(
    const scoped_refptr<DemuxerStream>& stream,
    const PipelineStatusCB& status_cb,
    const StatisticsCB& statistics_cb) {
  DCHECK(!decoder_thread_.IsRunning());
  DCHECK(media_pipeline_message_loop_->BelongsToCurrentThread());
  // check for no already attached stream, valid input stream, and save it
  DCHECK(!demuxer_stream_);
  if (!stream) {
    status_cb.Run(PIPELINE_ERROR_DECODE);
    return;
  }
  demuxer_stream_ = stream;

  TRACE_EVENT0("lb_shell", "ShellVideoDecoderImpl::Initialize()");

  VideoDecoderConfig decoder_config;
  decoder_config.CopyFrom(demuxer_stream_->video_decoder_config());
  DLOG(INFO) << "Configuration at Start: "
             << decoder_config.AsHumanReadableString();

  raw_decoder_.reset(LBVideoDecoder::Create(
      decoder_config, demuxer_stream_->GetDecryptor(),
      demuxer_stream_->StreamWasEncrypted()));

  if (!raw_decoder_) {
    status_cb.Run(DECODER_ERROR_NOT_SUPPORTED);
    return;
  }

  UPDATE_MEDIA_STATISTICS(STAT_TYPE_VIDEO_CODEC, decoder_config.codec());
  UPDATE_MEDIA_STATISTICS(STAT_TYPE_VIDEO_WIDTH,
                          decoder_config.natural_size().width());
  UPDATE_MEDIA_STATISTICS(STAT_TYPE_VIDEO_HEIGHT,
                          decoder_config.natural_size().height());

  base::Thread::Options options;

#if defined(__LB_PS4__)
  options.stack_size = kMediaStackThreadStackSize;
#endif  // defined(__LB_PS4__)

  if (decoder_thread_.StartWithOptions(options)) {
    state_ = kNormal;
    status_cb.Run(PIPELINE_OK);
  } else {
    status_cb.Run(PIPELINE_ERROR_DECODE);
  }
}

void ShellVideoDecoderImpl::Read(const ReadCB& read_cb) {
  if (!decoder_thread_.message_loop_proxy()->BelongsToCurrentThread()) {
    decoder_thread_.message_loop_proxy()->PostTask(
        FROM_HERE,
        base::Bind(
            &ShellVideoDecoderImpl::Read,
            this,
            read_cb));
    return;
  }

  DCHECK(!read_cb.is_null());
  DCHECK(read_cb_.is_null()) << "overlapping reads not supported";
  DCHECK_NE(state_, kUninitialized);

  TRACE_EVENT0("lb_shell", "ShellVideoDecoderImpl::DoRead()");

  read_cb_ = BindToLoop(media_pipeline_message_loop_, read_cb);

  // if an error has occurred, return error
  if (state_ == kShellDecodeError) {
    base::ResetAndReturn(&read_cb_).Run(kDecodeError, NULL);
    return;
  }

  // if decoding is done return empty frame
  if (state_ == kDecodeFinished) {
    base::ResetAndReturn(&read_cb_).Run(kOk, VideoFrame::CreateEmptyFrame());
    return;
  }

  // Kick off a read
  if (state_ == kFlushCodec) {
    DecodeBuffer(eof_buffer_);
  } else {
    ReadFromDemuxerStream();
  }
}

void ShellVideoDecoderImpl::ReadFromDemuxerStream() {
  DCHECK(decoder_thread_.message_loop_proxy()->BelongsToCurrentThread());
  DCHECK_NE(state_, kDecodeFinished);

  media_pipeline_message_loop_->PostTask(FROM_HERE, base::Bind(
      &DemuxerStream::Read, demuxer_stream_,
      BindToCurrentLoop(
          base::Bind(&ShellVideoDecoderImpl::BufferReady, this))));
}

void ShellVideoDecoderImpl::BufferReady(
    DemuxerStream::Status demuxer_status,
    const scoped_refptr<ShellBuffer>& buffer) {
  DCHECK(decoder_thread_.message_loop_proxy()->BelongsToCurrentThread());
  // must either be Ok and have a buffer or not Ok and no buffer
  DCHECK_EQ(demuxer_status != DemuxerStream::kOk, !buffer) << demuxer_status;

  if (state_ == kUninitialized) {
    // Stop has been called before BufferReady is posted. read_cb_ should be
    // called and cleared inside Stop().
    DCHECK(read_cb_.is_null());
    return;
  }

  if (state_ == kShellDecodeError) {
    DLOG(WARNING) << "read returned but decoder is in error state";
    return;
  }

  // if we deferred reset based on a pending read, process that reset now
  // after returning an empty frame
  if (!reset_cb_.is_null()) {
    DoReset();
    return;
  }

  if (demuxer_status == DemuxerStream::kConfigChanged) {
    VideoDecoderConfig decoder_config;
    decoder_config.CopyFrom(demuxer_stream_->video_decoder_config());
    DLOG(INFO) << "Configuration Changed: "
               << decoder_config.AsHumanReadableString();
    // One side effect of asking for the video configuration is that
    // the MediaSource demuxer stack uses that request to determine
    // that the video decoder has updated its configuration.
    // We therefore must ask for the updated video configuration
    // before requesting any data, or the MediaSource stack will
    // assert.
    if (!raw_decoder_->UpdateConfig(decoder_config)) {
      DLOG(ERROR) << "Error Reconfig H264 decoder";
      DecoderFatalError();
      return;
    }

    UPDATE_MEDIA_STATISTICS(STAT_TYPE_VIDEO_CODEC, decoder_config.codec());
    UPDATE_MEDIA_STATISTICS(STAT_TYPE_VIDEO_WIDTH,
                            decoder_config.natural_size().width());
    UPDATE_MEDIA_STATISTICS(STAT_TYPE_VIDEO_HEIGHT,
                            decoder_config.natural_size().height());

    ReadFromDemuxerStream();
    return;
  }

  // if stream is aborted service this and any pending reads with
  // empty frames
  if (demuxer_status == DemuxerStream::kAborted) {
    if (!read_cb_.is_null()) {
      base::ResetAndReturn(&read_cb_).Run(kOk, NULL);
    }
    return;
  }

  DCHECK(buffer);
  if (!buffer) {
    DecoderFatalError();
    return;
  }

  // at this point demuxerstream state should be OK
  DCHECK_EQ(demuxer_status, DemuxerStream::kOk);

  // Decode this one
  DecodeBuffer(buffer);
}

void ShellVideoDecoderImpl::DecodeBuffer(
    const scoped_refptr<ShellBuffer>& buffer) {
  TRACE_EVENT0("lb_shell", "ShellVideoDecoderImpl::DecodeBuffer()");
  SCOPED_MEDIA_STATISTICS(STAT_TYPE_VIDEO_FRAME_DECODE);

  DCHECK(decoder_thread_.message_loop_proxy()->BelongsToCurrentThread());
  DCHECK_NE(state_, kUninitialized);
  DCHECK_NE(state_, kDecodeFinished);
  DCHECK(buffer);

  // if we deferred reset based on a pending read, process that reset now
  // after returning an empty frame
  if (!reset_cb_.is_null()) {
    DoReset();
    return;
  }

  // if we've encountered an EOS buffer then attempt no more reads from upstream
  // empty queue and prepare to transition to kDecodeFinished.
  if (buffer->IsEndOfStream()) {
    eof_buffer_ = buffer;
    TRACE_EVENT0("lb_shell",
                 "ShellVideoDecoderImpl::DecodeBuffer() EOS Received");
    // We pipeline reads, so it is possible that we will receive more than one
    // read callback with EOS after the first
    if (state_ == kNormal) {
      state_ = kFlushCodec;
    }
  }

  scoped_refptr<VideoFrame> frame;

  LBVideoDecoder::DecodeStatus status = raw_decoder_->Decode(buffer, &frame);

  if (status == LBVideoDecoder::FRAME_DECODED) {
    DCHECK(frame);
    base::ResetAndReturn(&read_cb_).Run(kOk, frame);
    return;
  }

  if (status == LBVideoDecoder::NEED_MORE_DATA) {
    DCHECK(!frame);
    if (state_ == kFlushCodec) {
      state_ = kDecodeFinished;
      base::ResetAndReturn(&read_cb_).Run(kOk, VideoFrame::CreateEmptyFrame());
      return;
    }

    ReadFromDemuxerStream();
    return;
  }

  if (status == LBVideoDecoder::RETRY_WITH_SAME_BUFFER) {
    decoder_thread_.message_loop_proxy()->PostTask(FROM_HERE,
        base::Bind(&ShellVideoDecoderImpl::DecodeBuffer, this, buffer));
    return;
  }

  if (status == LBVideoDecoder::FATAL_ERROR) {
    DecoderFatalError();
  }

  NOTREACHED();
}

void ShellVideoDecoderImpl::Reset(const base::Closure& closure) {
  if (!decoder_thread_.message_loop_proxy()->BelongsToCurrentThread()) {
    decoder_thread_.message_loop_proxy()->PostTask(
        FROM_HERE,
        base::Bind(
            &ShellVideoDecoderImpl::Reset,
            this,
            closure));
    return;
  }

  TRACE_EVENT0("lb_shell", "ShellVideoDecoderImpl::Reset()");
  reset_cb_ = BindToLoop(media_pipeline_message_loop_, closure);

  // Defer the reset if a read is pending.
  if (!read_cb_.is_null())
    return;

  DoReset();
}

void ShellVideoDecoderImpl::DoReset() {
  // any pending read must have been serviced before calling me
  DCHECK(read_cb_.is_null());
  DCHECK(!reset_cb_.is_null());
  DCHECK(decoder_thread_.message_loop_proxy()->BelongsToCurrentThread());

  // service any pending read call with a NULL buffer
  if (!read_cb_.is_null()) {
    base::ResetAndReturn(&read_cb_).Run(kOk, NULL);
  }

  if (!raw_decoder_->Flush()) {
    DLOG(ERROR) << "Error Flush Decoder";
    DecoderFatalError();
  }

  base::ResetAndReturn(&reset_cb_).Run();
}

void ShellVideoDecoderImpl::DecoderFatalError() {
  DCHECK(decoder_thread_.message_loop_proxy()->BelongsToCurrentThread());
  TRACE_EVENT0("lb_shell", "ShellVideoDecoderImpl::DecoderFatalError()");
  // fatal error within the decoder
  DLOG(ERROR) << "fatal video decoder error.";
  // service any read callbacks with error
  if (!read_cb_.is_null()) {
    base::ResetAndReturn(&read_cb_).Run(kDecodeError, NULL);
  }
  // terminate playback
  state_ = kShellDecodeError;
}

void ShellVideoDecoderImpl::Stop(const base::Closure& closure) {
  if (!decoder_thread_.message_loop_proxy()->BelongsToCurrentThread()) {
    DCHECK(media_pipeline_message_loop_->BelongsToCurrentThread());
    decoder_thread_.message_loop_proxy()->PostTask(
        FROM_HERE,
        base::Bind(
            &ShellVideoDecoderImpl::Stop,
            this,
            closure));
    decoder_thread_.Stop();
    return;
  }

  raw_decoder_.reset(NULL);

  // terminate playback
  state_ = kUninitialized;

  if (!read_cb_.is_null())
    base::ResetAndReturn(&read_cb_).Run(kOk, NULL);

  closure.Run();
}

}  // namespace media
