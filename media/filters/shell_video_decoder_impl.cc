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
#include "base/stringprintf.h"
#include "lb_shell/lb_shell_constants.h"
#include "lb_video_decoder.h"
#include "media/base/bind_to_loop.h"
#include "media/base/pipeline_status.h"
#include "media/base/shell_buffer_factory.h"
#include "media/base/shell_filter_graph_log.h"
#include "media/base/shell_filter_graph_log_constants.h"
#include "media/base/shell_media_statistics.h"
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
    , raw_decoder_(NULL)
    , read_pending_(false)
    , decoder_thread_("H264 Decoder") {
}

ShellVideoDecoderImpl::~ShellVideoDecoderImpl() {
#if !defined(__LB_SHELL__FOR_RELEASE__)
  if (decoder_thread_.message_loop())
    decoder_thread_.message_loop()->AssertIdle();
#endif

  delete raw_decoder_;
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
  status_cb_ = status_cb;
  filter_graph_log_ = demuxer_stream_->filter_graph_log();
  filter_graph_log_->LogEvent(kObjectIdVideoDecoder, kEventInitialize);

  decoder_config_.CopyFrom(demuxer_stream_->video_decoder_config());
  DLOG(INFO) << "Configuration at Start: " <<
      decoder_config_.AsHumanReadableString();

  raw_decoder_ = LBVideoDecoder::Create(
      decoder_config_, demuxer_stream_->GetDecryptor(),
      demuxer_stream_->StreamWasEncrypted());

  if (!raw_decoder_) {
    status_cb_.Run(DECODER_ERROR_NOT_SUPPORTED);
    return;
  }

  UPDATE_MEDIA_STATISTICS(STAT_TYPE_VIDEO_CODEC, decoder_config_.codec());
  UPDATE_MEDIA_STATISTICS(STAT_TYPE_VIDEO_WIDTH,
                          decoder_config_.natural_size().width());
  UPDATE_MEDIA_STATISTICS(STAT_TYPE_VIDEO_HEIGHT,
                          decoder_config_.natural_size().height());

  base::Thread::Options options;

#if defined(__LB_PS4__)
  options.stack_size = kMediaStackThreadStackSize;
#endif  // defined(__LB_PS4__)

  if (decoder_thread_.StartWithOptions(options)) {
    state_ = kNormal;
    status_cb_.Run(PIPELINE_OK);
  } else {
    status_cb_.Run(PIPELINE_ERROR_DECODE);
  }
}

void ShellVideoDecoderImpl::Read(const ReadCB& read_cb) {
  DCHECK(media_pipeline_message_loop_->BelongsToCurrentThread());
  decoder_thread_.message_loop_proxy()->PostTask(
      FROM_HERE,
      base::Bind(
          &ShellVideoDecoderImpl::DoRead,
          this,
          read_cb));
}

void ShellVideoDecoderImpl::DoRead(const ReadCB& read_cb) {
  DCHECK(decoder_thread_.message_loop_proxy()->BelongsToCurrentThread());
  DCHECK(!read_cb.is_null());
  DCHECK(read_cb_.is_null()) << "overlapping reads not supported";
  DCHECK_NE(state_, kUninitialized);

  filter_graph_log_->LogEvent(kObjectIdVideoDecoder, kEventRead);
  read_cb_ = BindToLoop(media_pipeline_message_loop_, read_cb);

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

void ShellVideoDecoderImpl::ReadFromDemuxerStreamTask(
    const DemuxerStream::ReadCB& callback) {
  // Reads must be done from the pipeline thread for encrypted content.
  DCHECK(media_pipeline_message_loop_->BelongsToCurrentThread());
  demuxer_stream_->Read(callback);
}

void ShellVideoDecoderImpl::ReadFromDemuxerStream() {
  DCHECK(decoder_thread_.message_loop_proxy()->BelongsToCurrentThread());
  DCHECK_NE(state_, kDecodeFinished);

  // only one read to demuxer stream at a time, and only if we have a free
  // buffer to decode into
  if (!read_pending_) {
    read_pending_ = true;
    media_pipeline_message_loop_->PostTask(FROM_HERE, base::Bind(
        &ShellVideoDecoderImpl::ReadFromDemuxerStreamTask, this,
        BindToCurrentLoop(
            base::Bind(&ShellVideoDecoderImpl::BufferReady, this))));
  }
}

void ShellVideoDecoderImpl::BufferReady(
    DemuxerStream::Status demuxer_status,
    const scoped_refptr<ShellBuffer>& buffer) {
  DCHECK(decoder_thread_.message_loop_proxy()->BelongsToCurrentThread());
  // must either be Ok and have a buffer or not Ok and no buffer
  DCHECK_EQ(demuxer_status != DemuxerStream::kOk, !buffer) << demuxer_status;

  // Clear the read_pending_ flag since our callback has been called
  read_pending_ = false;

  if (state_ == kUninitialized) {
    // pending reads sometimes don't complete until after we're done.
    DLOG(WARNING) << "read returned but decoder is Uninitialized";
    return;
  }

  if (demuxer_status == DemuxerStream::kConfigChanged) {
    DLOG(INFO) << "Configuration Changed: " <<
        demuxer_stream_->video_decoder_config().AsHumanReadableString();
    // One side effect of asking for the video configuration is that
    // the MediaSource demuxer stack uses that request to determine
    // that the video decoder has updated its configuration.
    // We therefore must ask for the updated video configuration
    // before requesting any data, or the MediaSource stack will
    // assert.
    if (!raw_decoder_->UpdateConfig(demuxer_stream_->video_decoder_config())) {
      DLOG(ERROR) << "Error Reconfig H264 decoder";
      DecoderFatalError();
      return;
    }

    decoder_config_.CopyFrom(demuxer_stream_->video_decoder_config());

    UPDATE_MEDIA_STATISTICS(STAT_TYPE_VIDEO_CODEC, decoder_config_.codec());
    UPDATE_MEDIA_STATISTICS(STAT_TYPE_VIDEO_WIDTH,
                            decoder_config_.natural_size().width());
    UPDATE_MEDIA_STATISTICS(STAT_TYPE_VIDEO_HEIGHT,
                            decoder_config_.natural_size().height());

    // it's assumed ConfigChanged is not received during a reset
    DCHECK(reset_cb_.is_null());
    ReadFromDemuxerStream();
    return;
  }

  // if we deferred reset based on a pending read, process that reset now
  // after returning an empty frame
  if (!reset_cb_.is_null()) {
    DoReset();
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

  if (!buffer) {
    DLOG(WARNING) << "read returned empty buffer.";
    return;
  }

  // at this point demuxerstream state should be OK
  DCHECK_EQ(demuxer_status, DemuxerStream::kOk);
  // and buffer should be valid
  DCHECK(buffer);

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
  DCHECK(reset_cb_.is_null());
  DCHECK(buffer);
  filter_graph_log_->LogEvent(kObjectIdVideoDecoder, kEventDecode);

  // if we've encountered an EOS buffer then attempt no more reads from upstream
  // empty queue and prepare to transition to kDecodeFinished.
  if (buffer->IsEndOfStream()) {
    eof_buffer_ = buffer;
    filter_graph_log_->LogEvent(kObjectIdVideoDecoder,
                                kEventEndOfStreamReceived);
    // We pipeline reads, so it is possible that we will receive more than one
    // read callback with EOS after the first
    if (state_ == kNormal) {
      state_ = kFlushCodec;
    }
  }

  scoped_refptr<VideoFrame> frame;

  LBVideoDecoder::DecodeStatus status =
      raw_decoder_->Decode(buffer, &frame);

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

    read_pending_ = false;
    ReadFromDemuxerStream();
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

  DCHECK(decoder_thread_.message_loop_proxy()->BelongsToCurrentThread());
  filter_graph_log_->LogEvent(kObjectIdVideoDecoder, kEventReset);
  reset_cb_ = BindToLoop(media_pipeline_message_loop_, closure);

  // Defer the reset if a read is pending.
  if (read_pending_)
    return;

  DoReset();
}

void ShellVideoDecoderImpl::DoReset() {
  // any pending read must have been serviced before calling me
  DCHECK(!read_pending_);
  DCHECK(!reset_cb_.is_null());
  DCHECK(decoder_thread_.message_loop_proxy()->BelongsToCurrentThread());

  // service any pending read call with a NULL buffer
  if (!read_cb_.is_null()) {
    base::ResetAndReturn(&read_cb_).Run(kOk, NULL);
  }

  if (!raw_decoder_->Flush()) {
    DLOG(ERROR) << "Error Flush Decoder";
    DecoderFatalError();
    return;
  }

  base::ResetAndReturn(&reset_cb_).Run();
}

void ShellVideoDecoderImpl::DecoderFatalError() {
  DCHECK(decoder_thread_.message_loop_proxy()->BelongsToCurrentThread());
  filter_graph_log_->LogEvent(kObjectIdVideoDecoder, kEventFatalError);
  // fatal error within the decoder
  DLOG(ERROR) << "fatal video decoder error.";
  // service any read callbacks with error
  if (!read_cb_.is_null()) {
    base::ResetAndReturn(&read_cb_).Run(kDecodeError, NULL);
  }
  // terminate playback
  state_ = kUninitialized;
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
  DCHECK(decoder_thread_.message_loop_proxy()->BelongsToCurrentThread());

  delete raw_decoder_;
  raw_decoder_ = NULL;

  // terminate playback
  state_ = kUninitialized;

  if (!read_cb_.is_null())
    base::ResetAndReturn(&read_cb_).Run(kOk, NULL);

  closure.Run();
}

}  // namespace media

