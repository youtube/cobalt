// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/decrypting_video_decoder.h"

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/debug/trace_event.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/message_loop_proxy.h"
#include "media/base/bind_to_loop.h"
#include "media/base/decoder_buffer.h"
#include "media/base/decryptor.h"
#include "media/base/demuxer_stream.h"
#include "media/base/pipeline.h"
#include "media/base/video_decoder_config.h"
#include "media/base/video_frame.h"

namespace media {

#define BIND_TO_LOOP(function) \
    media::BindToLoop(message_loop_, base::Bind(function, this))

DecryptingVideoDecoder::DecryptingVideoDecoder(
    const scoped_refptr<base::MessageLoopProxy>& message_loop,
    const RequestDecryptorNotificationCB& request_decryptor_notification_cb)
    : message_loop_(message_loop),
      state_(kUninitialized),
      request_decryptor_notification_cb_(request_decryptor_notification_cb),
      decryptor_(NULL),
      key_added_while_decode_pending_(false),
      trace_id_(0) {
}

void DecryptingVideoDecoder::Initialize(
    const scoped_refptr<DemuxerStream>& stream,
    const PipelineStatusCB& status_cb,
    const StatisticsCB& statistics_cb) {
  if (!message_loop_->BelongsToCurrentThread()) {
    message_loop_->PostTask(FROM_HERE, base::Bind(
        &DecryptingVideoDecoder::DoInitialize, this,
        stream, status_cb, statistics_cb));
    return;
  }
  DoInitialize(stream, status_cb, statistics_cb);
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

  DVLOG(2) << "Reset() - state: " << state_;
  DCHECK(state_ == kIdle ||
         state_ == kPendingDemuxerRead ||
         state_ == kPendingDecode ||
         state_ == kWaitingForKey ||
         state_ == kDecodeFinished) << state_;
  DCHECK(init_cb_.is_null());  // No Reset() during pending initialization.
  DCHECK(reset_cb_.is_null());

  reset_cb_ = closure;

  decryptor_->ResetDecoder(Decryptor::kVideo);

  // Reset() cannot complete if the read callback is still pending.
  // Defer the resetting process in this case. The |reset_cb_| will be fired
  // after the read callback is fired - see DoDecryptAndDecodeBuffer() and
  // DoDeliverFrame().
  if (state_ == kPendingDemuxerRead || state_ == kPendingDecode) {
    DCHECK(!read_cb_.is_null());
    return;
  }

  if (state_ == kWaitingForKey) {
    DCHECK(!read_cb_.is_null());
    pending_buffer_to_decode_ = NULL;
    base::ResetAndReturn(&read_cb_).Run(kOk, NULL);
  }

  DCHECK(read_cb_.is_null());
  DoReset();
}

void DecryptingVideoDecoder::Stop(const base::Closure& closure) {
  if (!message_loop_->BelongsToCurrentThread()) {
    message_loop_->PostTask(FROM_HERE, base::Bind(
        &DecryptingVideoDecoder::Stop, this, closure));
    return;
  }

  DVLOG(2) << "Stop() - state: " << state_;

  // At this point the render thread is likely paused (in WebMediaPlayerImpl's
  // Destroy()), so running |closure| can't wait for anything that requires the
  // render thread to be processing messages to complete (such as PPAPI
  // callbacks).
  if (decryptor_) {
    decryptor_->RegisterKeyAddedCB(Decryptor::kVideo, Decryptor::KeyAddedCB());
    decryptor_->DeinitializeDecoder(Decryptor::kVideo);
    decryptor_ = NULL;
  }
  if (!request_decryptor_notification_cb_.is_null()) {
    base::ResetAndReturn(&request_decryptor_notification_cb_).Run(
        DecryptorNotificationCB());
  }
  pending_buffer_to_decode_ = NULL;
  if (!init_cb_.is_null())
    base::ResetAndReturn(&init_cb_).Run(DECODER_ERROR_NOT_SUPPORTED);
  if (!read_cb_.is_null())
    base::ResetAndReturn(&read_cb_).Run(kOk, NULL);
  if (!reset_cb_.is_null())
    base::ResetAndReturn(&reset_cb_).Run();
  state_ = kStopped;
  closure.Run();
}

DecryptingVideoDecoder::~DecryptingVideoDecoder() {
  DCHECK(state_ == kUninitialized || state_ == kStopped) << state_;
}

void DecryptingVideoDecoder::DoInitialize(
    const scoped_refptr<DemuxerStream>& stream,
    const PipelineStatusCB& status_cb,
    const StatisticsCB& statistics_cb) {
  DVLOG(2) << "DoInitialize()";
  DCHECK(message_loop_->BelongsToCurrentThread());
  DCHECK_EQ(state_, kUninitialized) << state_;
  DCHECK(stream);

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

  state_ = kDecryptorRequested;
  request_decryptor_notification_cb_.Run(
      BIND_TO_LOOP(&DecryptingVideoDecoder::SetDecryptor));
}

void DecryptingVideoDecoder::SetDecryptor(Decryptor* decryptor) {
  DVLOG(2) << "SetDecryptor()";
  DCHECK(message_loop_->BelongsToCurrentThread());

  if (state_ == kStopped)
    return;

  DCHECK_EQ(state_, kDecryptorRequested) << state_;
  DCHECK(!init_cb_.is_null());
  DCHECK(!request_decryptor_notification_cb_.is_null());
  request_decryptor_notification_cb_.Reset();

  decryptor_ = decryptor;

  scoped_ptr<VideoDecoderConfig> scoped_config(new VideoDecoderConfig());
  scoped_config->CopyFrom(demuxer_stream_->video_decoder_config());

  state_ = kPendingDecoderInit;
  decryptor_->InitializeVideoDecoder(
      scoped_config.Pass(),
      BIND_TO_LOOP(&DecryptingVideoDecoder::FinishInitialization));
}

void DecryptingVideoDecoder::FinishInitialization(bool success) {
  DVLOG(2) << "FinishInitialization()";
  DCHECK(message_loop_->BelongsToCurrentThread());

  if (state_ == kStopped)
    return;

  DCHECK_EQ(state_, kPendingDecoderInit) << state_;
  DCHECK(!init_cb_.is_null());
  DCHECK(reset_cb_.is_null());  // No Reset() before initialization finished.
  DCHECK(read_cb_.is_null());  // No Read() before initialization finished.

  if (!success) {
    base::ResetAndReturn(&init_cb_).Run(DECODER_ERROR_NOT_SUPPORTED);
    state_ = kStopped;
    return;
  }

  decryptor_->RegisterKeyAddedCB(
      Decryptor::kVideo, BIND_TO_LOOP(&DecryptingVideoDecoder::OnKeyAdded));

  // Success!
  state_ = kIdle;
  base::ResetAndReturn(&init_cb_).Run(PIPELINE_OK);
}

void DecryptingVideoDecoder::DoRead(const ReadCB& read_cb) {
  DVLOG(3) << "DoRead()";
  DCHECK(message_loop_->BelongsToCurrentThread());
  DCHECK(state_ == kIdle || state_ == kDecodeFinished) << state_;
  DCHECK(!read_cb.is_null());
  CHECK(read_cb_.is_null()) << "Overlapping decodes are not supported.";

  // Return empty frames if decoding has finished.
  if (state_ == kDecodeFinished) {
    read_cb.Run(kOk, VideoFrame::CreateEmptyFrame());
    return;
  }

  read_cb_ = read_cb;
  state_ = kPendingDemuxerRead;
  ReadFromDemuxerStream();
}

void DecryptingVideoDecoder::ReadFromDemuxerStream() {
  DCHECK(message_loop_->BelongsToCurrentThread());
  DCHECK_EQ(state_, kPendingDemuxerRead) << state_;
  DCHECK(!read_cb_.is_null());

  demuxer_stream_->Read(
      base::Bind(&DecryptingVideoDecoder::DoDecryptAndDecodeBuffer, this));
}

void DecryptingVideoDecoder::DoDecryptAndDecodeBuffer(
    DemuxerStream::Status status,
    const scoped_refptr<DecoderBuffer>& buffer) {
  if (!message_loop_->BelongsToCurrentThread()) {
    message_loop_->PostTask(FROM_HERE, base::Bind(
        &DecryptingVideoDecoder::DoDecryptAndDecodeBuffer, this,
        status, buffer));
    return;
  }

  DVLOG(3) << "DoDecryptAndDecodeBuffer()";

  if (state_ == kStopped)
    return;

  DCHECK_EQ(state_, kPendingDemuxerRead) << state_;
  DCHECK(!read_cb_.is_null());
  DCHECK_EQ(buffer != NULL, status == DemuxerStream::kOk) << status;

  if (!reset_cb_.is_null()) {
    base::ResetAndReturn(&read_cb_).Run(kOk, NULL);
    if (!reset_cb_.is_null())
      DoReset();
    return;
  }

  if (status == DemuxerStream::kAborted) {
    DVLOG(2) << "DoDecryptAndDecodeBuffer() - kAborted";
    state_ = kIdle;
    base::ResetAndReturn(&read_cb_).Run(kOk, NULL);
    return;
  }

  if (status == DemuxerStream::kConfigChanged) {
    // TODO(xhwang): Add config change support.
    // The |state_| is chosen to be kDecodeFinished here to be consistent with
    // the implementation of FFmpegVideoDecoder.
    DVLOG(2) << "DoDecryptAndDecodeBuffer() - kConfigChanged";
    state_ = kDecodeFinished;
    base::ResetAndReturn(&read_cb_).Run(kDecodeError, NULL);
    return;
  }

  DCHECK_EQ(status, DemuxerStream::kOk);
  pending_buffer_to_decode_ = buffer;
  state_ = kPendingDecode;
  DecodePendingBuffer();
}

void DecryptingVideoDecoder::DecodePendingBuffer() {
  DCHECK(message_loop_->BelongsToCurrentThread());
  DCHECK_EQ(state_, kPendingDecode) << state_;
  TRACE_EVENT_ASYNC_BEGIN0(
      "eme", "DecryptingVideoDecoder::DecodePendingBuffer", ++trace_id_);
  decryptor_->DecryptAndDecodeVideo(
      pending_buffer_to_decode_,
      base::Bind(&DecryptingVideoDecoder::DeliverFrame, this,
                 pending_buffer_to_decode_->GetDataSize()));
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
  DVLOG(3) << "DoDeliverFrame() - status: " << status;
  DCHECK(message_loop_->BelongsToCurrentThread());
  TRACE_EVENT_ASYNC_END0(
      "eme", "DecryptingVideoDecoder::DecodePendingBuffer", trace_id_);

  if (state_ == kStopped)
    return;

  DCHECK_EQ(state_, kPendingDecode) << state_;
  DCHECK(!read_cb_.is_null());
  DCHECK(pending_buffer_to_decode_);

  bool need_to_try_again_if_nokey_is_returned = key_added_while_decode_pending_;
  key_added_while_decode_pending_ = false;

  scoped_refptr<DecoderBuffer> scoped_pending_buffer_to_decode =
      pending_buffer_to_decode_;
  pending_buffer_to_decode_ = NULL;

  if (!reset_cb_.is_null()) {
    base::ResetAndReturn(&read_cb_).Run(kOk, NULL);
    DoReset();
    return;
  }

  DCHECK_EQ(status == Decryptor::kSuccess, frame != NULL);

  if (status == Decryptor::kError) {
    DVLOG(2) << "DoDeliverFrame() - kError";
    state_ = kDecodeFinished;
    base::ResetAndReturn(&read_cb_).Run(kDecodeError, NULL);
    return;
  }

  if (status == Decryptor::kNoKey) {
    DVLOG(2) << "DoDeliverFrame() - kNoKey";
    // Set |pending_buffer_to_decode_| back as we need to try decoding the
    // pending buffer again when new key is added to the decryptor.
    pending_buffer_to_decode_ = scoped_pending_buffer_to_decode;

    if (need_to_try_again_if_nokey_is_returned) {
      // The |state_| is still kPendingDecode.
      DecodePendingBuffer();
      return;
    }

    state_ = kWaitingForKey;
    return;
  }

  // The buffer has been accepted by the decoder, let's report statistics.
  if (buffer_size) {
    PipelineStatistics statistics;
    statistics.video_bytes_decoded = buffer_size;
    statistics_cb_.Run(statistics);
  }

  if (status == Decryptor::kNeedMoreData) {
    DVLOG(2) << "DoDeliverFrame() - kNeedMoreData";
    if (scoped_pending_buffer_to_decode->IsEndOfStream()) {
      state_ = kDecodeFinished;
      base::ResetAndReturn(&read_cb_).Run(
          kOk, media::VideoFrame::CreateEmptyFrame());
      return;
    }

    state_ = kPendingDemuxerRead;
    ReadFromDemuxerStream();
    return;
  }

  DCHECK_EQ(status, Decryptor::kSuccess);
  // No frame returned with kSuccess should be end-of-stream frame.
  DCHECK(!frame->IsEndOfStream());
  state_ = kIdle;
  base::ResetAndReturn(&read_cb_).Run(kOk, frame);
}

void DecryptingVideoDecoder::OnKeyAdded() {
  DVLOG(2) << "OnKeyAdded()";
  DCHECK(message_loop_->BelongsToCurrentThread());

  if (state_ == kPendingDecode) {
    key_added_while_decode_pending_ = true;
    return;
  }

  if (state_ == kWaitingForKey) {
    state_ = kPendingDecode;
    DecodePendingBuffer();
  }
}

void DecryptingVideoDecoder::DoReset() {
  DCHECK(init_cb_.is_null());
  DCHECK(read_cb_.is_null());
  state_ = kIdle;
  base::ResetAndReturn(&reset_cb_).Run();
}

}  // namespace media
