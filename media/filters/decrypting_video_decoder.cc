// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/decrypting_video_decoder.h"

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/location.h"
#include "base/message_loop_proxy.h"
#include "media/base/decoder_buffer.h"
#include "media/base/decryptor.h"
#include "media/base/demuxer_stream.h"
#include "media/base/pipeline.h"
#include "media/base/video_decoder_config.h"
#include "media/base/video_frame.h"

namespace media {

DecryptingVideoDecoder::DecryptingVideoDecoder(
    const MessageLoopFactoryCB& message_loop_factory_cb,
    Decryptor* decryptor)
    : message_loop_factory_cb_(message_loop_factory_cb),
      state_(kUninitialized),
      decryptor_(decryptor) {
}

void DecryptingVideoDecoder::Initialize(
    const scoped_refptr<DemuxerStream>& stream,
    const PipelineStatusCB& status_cb,
    const StatisticsCB& statistics_cb) {
  DCHECK(!message_loop_);
  message_loop_ = base::ResetAndReturn(&message_loop_factory_cb_).Run();
  message_loop_->PostTask(FROM_HERE, base::Bind(
      &DecryptingVideoDecoder::DoInitialize, this,
      stream, status_cb, statistics_cb));
}

void DecryptingVideoDecoder::Read(const ReadCB& read_cb) {
  // Complete operation asynchronously on different stack of execution as per
  // the API contract of VideoDecoder::Read()
  message_loop_->PostTask(FROM_HERE, base::Bind(
      &DecryptingVideoDecoder::DoRead, this, read_cb));
}

void DecryptingVideoDecoder::Reset(const base::Closure& closure) {
  if (!message_loop_->BelongsToCurrentThread()) {
    message_loop_->PostTask(FROM_HERE, base::Bind(
        &DecryptingVideoDecoder::Reset, this, closure));
    return;
  }

  DCHECK_NE(state_, kUninitialized);
  DCHECK(init_cb_.is_null());  // No Reset() during pending initialization.
  DCHECK(stop_cb_.is_null());  // No Reset() during pending Stop().
  DCHECK(reset_cb_.is_null());
  reset_cb_ = closure;

  decryptor_->CancelDecryptAndDecodeVideo();

  // Reset() cannot complete if the read callback is still pending.
  // Defer the resetting process in this case. The |reset_cb_| will be fired
  // after the read callback is fired - see DoDecryptAndDecodeBuffer() and
  // DoDeliverFrame().
  if (!read_cb_.is_null())
    return;

  DoReset();
}

void DecryptingVideoDecoder::Stop(const base::Closure& closure) {
  if (!message_loop_->BelongsToCurrentThread()) {
    message_loop_->PostTask(FROM_HERE, base::Bind(
        &DecryptingVideoDecoder::Stop, this, closure));
    return;
  }

  DCHECK(stop_cb_.is_null());
  stop_cb_ = closure;

  decryptor_->StopVideoDecoder();

  // Stop() cannot complete if the init or read callback is still pending.
  // Defer the stopping process in these cases. The |stop_cb_| will be fired
  // after the init or read callback is fired - see DoFinishInitialization(),
  // DoDecryptAndDecodeBuffer() and DoDeliverFrame().
  if (!init_cb_.is_null() || !read_cb_.is_null())
    return;

  DoStop();
}

DecryptingVideoDecoder::~DecryptingVideoDecoder() {
  DCHECK_EQ(state_, kUninitialized);
}

void DecryptingVideoDecoder::DoInitialize(
    const scoped_refptr<DemuxerStream>& stream,
    const PipelineStatusCB& status_cb,
    const StatisticsCB& statistics_cb) {
  DCHECK(message_loop_->BelongsToCurrentThread());
  DCHECK(stream);
  DCHECK_EQ(state_, kUninitialized);

  const VideoDecoderConfig& config = stream->video_decoder_config();
  if (!config.IsValidConfig()) {
    DLOG(ERROR) << "Invalid video stream config: "
                << config.AsHumanReadableString();
    status_cb.Run(PIPELINE_ERROR_DECODE);
    return;
  }

  // DecryptingVideoDecoder only accepts potentially encrypted stream.
  if (!config.is_encrypted()) {
    status_cb.Run(DECODER_ERROR_NOT_SUPPORTED);
    return;
  }

  DCHECK(!demuxer_stream_);
  demuxer_stream_ = stream;
  statistics_cb_ = statistics_cb;

  init_cb_ = status_cb;
  decryptor_->InitializeVideoDecoder(config, base::Bind(
      &DecryptingVideoDecoder::FinishInitialization, this));
}

void DecryptingVideoDecoder::FinishInitialization(bool success) {
  message_loop_->PostTask(FROM_HERE, base::Bind(
      &DecryptingVideoDecoder::DoFinishInitialization, this, success));
}

void DecryptingVideoDecoder::DoFinishInitialization(bool success) {
  DCHECK(message_loop_->BelongsToCurrentThread());
  DCHECK_EQ(state_, kUninitialized);
  DCHECK(!init_cb_.is_null());
  DCHECK(reset_cb_.is_null());
  DCHECK(read_cb_.is_null());

  if (!stop_cb_.is_null()) {
    base::ResetAndReturn(&init_cb_).Run(DECODER_ERROR_NOT_SUPPORTED);
    DoStop();
    return;
  }

  if (!success) {
    base::ResetAndReturn(&init_cb_).Run(DECODER_ERROR_NOT_SUPPORTED);
    return;
  }

  // Success!
  state_ = kNormal;
  base::ResetAndReturn(&init_cb_).Run(PIPELINE_OK);
}

void DecryptingVideoDecoder::DoRead(const ReadCB& read_cb) {
  DCHECK(message_loop_->BelongsToCurrentThread());
  DCHECK(!read_cb.is_null());
  CHECK_NE(state_, kUninitialized);
  CHECK(read_cb_.is_null()) << "Overlapping decodes are not supported.";

  // Return empty frames if decoding has finished.
  if (state_ == kDecodeFinished) {
    read_cb.Run(kOk, VideoFrame::CreateEmptyFrame());
    return;
  }

  read_cb_ = read_cb;
  ReadFromDemuxerStream();
}

void DecryptingVideoDecoder::ReadFromDemuxerStream() {
  DCHECK(message_loop_->BelongsToCurrentThread());
  DCHECK_EQ(state_, kNormal);
  DCHECK(!read_cb_.is_null());

  demuxer_stream_->Read(
      base::Bind(&DecryptingVideoDecoder::DecryptAndDecodeBuffer, this));
}

void DecryptingVideoDecoder::DecryptAndDecodeBuffer(
    DemuxerStream::Status status,
    const scoped_refptr<DecoderBuffer>& buffer) {
  // In theory, we don't need to force post the task here, because we do a
  // force task post in DeliverFrame(). Therefore, even if
  // demuxer_stream_->Read() execute the read callback on the same execution
  // stack we are still fine. But it looks like a force post task makes the
  // logic more understandable and manageable, so why not?
  message_loop_->PostTask(FROM_HERE, base::Bind(
      &DecryptingVideoDecoder::DoDecryptAndDecodeBuffer, this,
      status, buffer));
}

void DecryptingVideoDecoder::DoDecryptAndDecodeBuffer(
    DemuxerStream::Status status,
    const scoped_refptr<DecoderBuffer>& buffer) {
  DCHECK(message_loop_->BelongsToCurrentThread());
  DCHECK_EQ(state_, kNormal);
  DCHECK(!read_cb_.is_null());
  DCHECK_EQ(buffer != NULL, status == DemuxerStream::kOk) << status;

  if (!stop_cb_.is_null()) {
    base::ResetAndReturn(&read_cb_).Run(kOk, NULL);
    if (!reset_cb_.is_null())
      DoReset();

    DoStop();
    return;
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

  if (status == DemuxerStream::kConfigChanged) {
    // TODO(xhwang): Add config change support.
    base::ResetAndReturn(&read_cb_).Run(kDecodeError, NULL);
    return;
  }

  DCHECK_EQ(status, DemuxerStream::kOk);
  decryptor_->DecryptAndDecodeVideo(
      buffer, base::Bind(&DecryptingVideoDecoder::DeliverFrame, this,
                         buffer->GetDataSize()));
}

void DecryptingVideoDecoder::DeliverFrame(
    int buffer_size,
    Decryptor::Status status,
    const scoped_refptr<VideoFrame>& frame) {
  // We need to force task post here because the VideoDecodeCB can be executed
  // synchronously in Reset()/Stop(). Instead of using more complicated logic in
  // those function to fix it, why not force task post here to make everything
  // simple and clear?
  message_loop_->PostTask(FROM_HERE, base::Bind(
      &DecryptingVideoDecoder::DoDeliverFrame, this,
      buffer_size, status, frame));
}

void DecryptingVideoDecoder::DoDeliverFrame(
    int buffer_size,
    Decryptor::Status status,
    const scoped_refptr<VideoFrame>& frame) {
  DCHECK(message_loop_->BelongsToCurrentThread());
  DCHECK_EQ(state_, kNormal);
  DCHECK(!read_cb_.is_null());

  if (!stop_cb_.is_null()) {
    base::ResetAndReturn(&read_cb_).Run(kOk, NULL);
    if (!reset_cb_.is_null())
      DoReset();

    DoStop();
    return;
  }

  if (!reset_cb_.is_null()) {
    base::ResetAndReturn(&read_cb_).Run(kOk, NULL);
    DoReset();
    return;
  }

  if (status == Decryptor::kNoKey || status == Decryptor::kError) {
    DCHECK(!frame);
    state_ = kDecodeFinished;
    base::ResetAndReturn(&read_cb_).Run(kDecodeError, NULL);
    return;
  }

  // The buffer has been accepted by the decoder, let's report statistics.
  if (buffer_size) {
    PipelineStatistics statistics;
    statistics.video_bytes_decoded = buffer_size;
    statistics_cb_.Run(statistics);
  }

  if (status == Decryptor::kNeedMoreData) {
    DCHECK(!frame);
    ReadFromDemuxerStream();
    return;
  }

  DCHECK_EQ(status, Decryptor::kSuccess);
  if (frame->IsEndOfStream())
    state_ = kDecodeFinished;

  base::ResetAndReturn(&read_cb_).Run(kOk, frame);
}

void DecryptingVideoDecoder::DoReset() {
  DCHECK(read_cb_.is_null());
  DCHECK_NE(state_, kUninitialized);
  state_ = kNormal;
  base::ResetAndReturn(&reset_cb_).Run();
}

void DecryptingVideoDecoder::DoStop() {
  DCHECK(read_cb_.is_null());
  state_ = kUninitialized;
  base::ResetAndReturn(&stop_cb_).Run();
}

}  // namespace media
