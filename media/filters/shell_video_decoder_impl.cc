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
#include "build/build_config.h"  // Must come before OS_STARBOARD.
#if !defined(OS_STARBOARD)
#include "lb_shell/lb_shell_constants.h"
#endif
#include "media/base/bind_to_loop.h"
#include "media/base/pipeline_status.h"
#include "media/base/shell_buffer_factory.h"
#include "media/base/shell_media_statistics.h"
#include "media/base/video_decoder_config.h"
#include "media/base/video_frame.h"

namespace media {

//==============================================================================
// ShellVideoDecoderImpl
//

ShellVideoDecoderImpl::ShellVideoDecoderImpl(
    const scoped_refptr<base::MessageLoopProxy>& message_loop,
    ShellRawVideoDecoderFactory* raw_video_decoder_factory)
    : state_(kUninitialized),
      media_pipeline_message_loop_(message_loop),
      raw_video_decoder_factory_(raw_video_decoder_factory),
      decoder_thread_("Video Decoder") {
  DCHECK(raw_video_decoder_factory_);
}

ShellVideoDecoderImpl::~ShellVideoDecoderImpl() {}

void ShellVideoDecoderImpl::Initialize(
    const scoped_refptr<DemuxerStream>& stream,
    const PipelineStatusCB& status_cb,
    const StatisticsCB& statistics_cb) {
  TRACE_EVENT0("media_stack", "ShellVideoDecoderImpl::Initialize()");
  DCHECK(!decoder_thread_.IsRunning());
  DCHECK(media_pipeline_message_loop_->BelongsToCurrentThread());
  // check for no already attached stream, valid input stream, and save it
  DCHECK(!demuxer_stream_);

  if (!stream) {
    status_cb.Run(PIPELINE_ERROR_DECODE);
    return;
  }
  demuxer_stream_ = stream;

  VideoDecoderConfig decoder_config;
  decoder_config.CopyFrom(demuxer_stream_->video_decoder_config());
  DLOG(INFO) << "Configuration at Start: "
             << decoder_config.AsHumanReadableString();

  raw_decoder_ = raw_video_decoder_factory_->Create(
      decoder_config, demuxer_stream_->GetDecryptor(),
      demuxer_stream_->StreamWasEncrypted());

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

#if !defined(OS_STARBOARD)
// TODO(***REMOVED***): Determine where to define thread constants in a Starboard
// world.
#if defined(__LB_PS4__)
  options.stack_size = kMediaStackThreadStackSize;
#endif  // defined(__LB_PS4__)
#endif

  if (decoder_thread_.StartWithOptions(options)) {
    state_ = kNormal;
    status_cb.Run(PIPELINE_OK);
  } else {
    status_cb.Run(PIPELINE_ERROR_DECODE);
  }
}

void ShellVideoDecoderImpl::Read(const ReadCB& read_cb) {
  TRACE_EVENT0("media_stack", "ShellVideoDecoderImpl::DoRead()");
  if (!decoder_thread_.message_loop_proxy()->BelongsToCurrentThread()) {
    decoder_thread_.message_loop_proxy()->PostTask(
        FROM_HERE, base::Bind(&ShellVideoDecoderImpl::Read, this, read_cb));
    return;
  }

  DCHECK(!read_cb.is_null());
  DCHECK(read_cb_.is_null()) << "overlapping reads not supported";
  DCHECK_NE(state_, kUninitialized);

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

  if (buffer_to_decode_) {
    DecodeBuffer(buffer_to_decode_);
  } else if (state_ == kFlushCodec) {
    DecodeBuffer(eof_buffer_);
  } else {
    ReadFromDemuxerStream();  // Kick off a read
  }
}

void ShellVideoDecoderImpl::ReadFromDemuxerStream() {
  DCHECK(decoder_thread_.message_loop_proxy()->BelongsToCurrentThread());
  DCHECK_NE(state_, kDecodeFinished);

  media_pipeline_message_loop_->PostTask(
      FROM_HERE, base::Bind(&DemuxerStream::Read, demuxer_stream_,
                            BindToCurrentLoop(base::Bind(
                                &ShellVideoDecoderImpl::BufferReady, this))));
}

void ShellVideoDecoderImpl::BufferReady(
    DemuxerStream::Status demuxer_status,
    const scoped_refptr<DecoderBuffer>& buffer) {
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
    const scoped_refptr<DecoderBuffer>& buffer) {
  TRACE_EVENT0("media_stack", "ShellVideoDecoderImpl::DecodeBuffer()");
  SCOPED_MEDIA_STATISTICS(STAT_TYPE_VIDEO_FRAME_DECODE);

  DCHECK(decoder_thread_.message_loop_proxy()->BelongsToCurrentThread());
  DCHECK_NE(state_, kUninitialized);
  DCHECK_NE(state_, kDecodeFinished);
  DCHECK(buffer);
  if (buffer_to_decode_)
    DCHECK_EQ(buffer_to_decode_, buffer);

  buffer_to_decode_ = buffer;

  // if we deferred reset based on a pending read, process that reset now
  // after returning an empty frame
  if (!reset_cb_.is_null()) {
    DoReset();
    return;
  }

  // if we've encountered an EOS buffer then attempt no more reads from upstream
  // empty queue and prepare to transition to kDecodeFinished.
  if (buffer->IsEndOfStream()) {
    TRACE_EVENT0("media_stack",
                 "ShellVideoDecoderImpl::DecodeBuffer() EOS received");
    eof_buffer_ = buffer;
    // We pipeline reads, so it is possible that we will receive more than one
    // read callback with EOS after the first
    if (state_ == kNormal) {
      state_ = kFlushCodec;
    }
  }

  ShellRawVideoDecoder::DecodeCB decode_cb =
      base::Bind(&ShellVideoDecoderImpl::DecodeCallback, this);
  raw_decoder_->Decode(buffer, BindToCurrentLoop(decode_cb));
}

void ShellVideoDecoderImpl::DecodeCallback(
    ShellRawVideoDecoder::DecodeStatus status,
    const scoped_refptr<VideoFrame>& frame) {
  DCHECK(buffer_to_decode_);

  if (!reset_cb_.is_null()) {
    DoReset();
    return;
  }

  if (status == ShellRawVideoDecoder::RETRY_WITH_SAME_BUFFER) {
    if (frame) {
      base::ResetAndReturn(&read_cb_).Run(kOk, frame);
    } else {
      decoder_thread_.message_loop_proxy()->PostTask(
          FROM_HERE, base::Bind(&ShellVideoDecoderImpl::DecodeBuffer, this,
                                buffer_to_decode_));
    }
    return;
  }

  buffer_to_decode_ = NULL;

  if (status == ShellRawVideoDecoder::FRAME_DECODED) {
    TRACE_EVENT1("media_stack", "ShellVideoDecoderImpl frame decoded",
                 "timestamp", frame->GetTimestamp().InMicroseconds());
    DCHECK(frame);
    base::ResetAndReturn(&read_cb_).Run(kOk, frame);
    return;
  }

  if (status == ShellRawVideoDecoder::NEED_MORE_DATA) {
    DCHECK(!frame);
    if (state_ == kFlushCodec) {
      state_ = kDecodeFinished;
      base::ResetAndReturn(&read_cb_).Run(kOk, VideoFrame::CreateEmptyFrame());
      return;
    }

    ReadFromDemuxerStream();
    return;
  }

  if (status == ShellRawVideoDecoder::FATAL_ERROR) {
    DecoderFatalError();
    return;
  }

  NOTREACHED();
}

void ShellVideoDecoderImpl::Reset(const base::Closure& closure) {
  TRACE_EVENT0("media_stack", "ShellVideoDecoderImpl::Reset()");
  if (!decoder_thread_.message_loop_proxy()->BelongsToCurrentThread()) {
    decoder_thread_.message_loop_proxy()->PostTask(
        FROM_HERE, base::Bind(&ShellVideoDecoderImpl::Reset, this, closure));
    return;
  }

  reset_cb_ = BindToLoop(media_pipeline_message_loop_, closure);

  // Defer the reset if a read is pending.
  if (!read_cb_.is_null())
    return;

  DoReset();
}

void ShellVideoDecoderImpl::DoReset() {
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

  if (state_ != kShellDecodeError)
    state_ = kNormal;
  eof_buffer_ = NULL;
  buffer_to_decode_ = NULL;

  base::ResetAndReturn(&reset_cb_).Run();
}

void ShellVideoDecoderImpl::DecoderFatalError() {
  TRACE_EVENT0("media_stack", "ShellVideoDecoderImpl::DecoderFatalError()");
  DCHECK(decoder_thread_.message_loop_proxy()->BelongsToCurrentThread());
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
  DCHECK(media_pipeline_message_loop_->BelongsToCurrentThread());

  decoder_thread_.Stop();
  raw_decoder_.reset(NULL);

  // terminate playback
  state_ = kUninitialized;

  if (!read_cb_.is_null())
    base::ResetAndReturn(&read_cb_).Run(kOk, NULL);

  closure.Run();
}

void ShellVideoDecoderImpl::NearlyUnderflow() {
  DCHECK(media_pipeline_message_loop_->BelongsToCurrentThread());
  if (raw_decoder_) {
    raw_decoder_->NearlyUnderflow();
  }
}

void ShellVideoDecoderImpl::HaveEnoughFrames() {
  DCHECK(media_pipeline_message_loop_->BelongsToCurrentThread());
  if (raw_decoder_) {
    raw_decoder_->HaveEnoughFrames();
  }
}

}  // namespace media
