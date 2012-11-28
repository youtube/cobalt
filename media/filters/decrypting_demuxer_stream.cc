// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/decrypting_demuxer_stream.h"

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/message_loop_proxy.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/video_decoder_config.h"
#include "media/base/bind_to_loop.h"
#include "media/base/decoder_buffer.h"
#include "media/base/decryptor.h"
#include "media/base/demuxer_stream.h"
#include "media/base/pipeline.h"

namespace media {

#define BIND_TO_LOOP(function) \
    media::BindToLoop(message_loop_, base::Bind(function, this))

DecryptingDemuxerStream::DecryptingDemuxerStream(
    const scoped_refptr<base::MessageLoopProxy>& message_loop,
    const RequestDecryptorNotificationCB& request_decryptor_notification_cb)
    : message_loop_(message_loop),
      state_(kUninitialized),
      stream_type_(UNKNOWN),
      request_decryptor_notification_cb_(request_decryptor_notification_cb),
      decryptor_(NULL),
      key_added_while_decrypt_pending_(false) {
}

void DecryptingDemuxerStream::Initialize(
    const scoped_refptr<DemuxerStream>& stream,
    const PipelineStatusCB& status_cb) {
  if (!message_loop_->BelongsToCurrentThread()) {
    message_loop_->PostTask(FROM_HERE, base::Bind(
        &DecryptingDemuxerStream::DoInitialize, this, stream, status_cb));
    return;
  }
  DoInitialize(stream, status_cb);
}

void DecryptingDemuxerStream::Read(const ReadCB& read_cb) {
  DVLOG(3) << "Read()";
  DCHECK(message_loop_->BelongsToCurrentThread());
  DCHECK_EQ(state_, kIdle) << state_;
  DCHECK(!read_cb.is_null());
  CHECK(read_cb_.is_null()) << "Overlapping reads are not supported.";

  read_cb_ = read_cb;
  state_ = kPendingDemuxerRead;
  demuxer_stream_->Read(
      base::Bind(&DecryptingDemuxerStream::DecryptBuffer, this));
}

void DecryptingDemuxerStream::Reset(const base::Closure& closure) {
  if (!message_loop_->BelongsToCurrentThread()) {
    message_loop_->PostTask(FROM_HERE, base::Bind(
        &DecryptingDemuxerStream::Reset, this, closure));
    return;
  }

  DVLOG(2) << "Reset() - state: " << state_;
  DCHECK(state_ != kUninitialized && state_ != kDecryptorRequested) << state_;
  DCHECK(init_cb_.is_null());  // No Reset() during pending initialization.
  DCHECK(reset_cb_.is_null());

  reset_cb_ = closure;

  decryptor_->CancelDecrypt(GetDecryptorStreamType());

  // Reset() cannot complete if the read callback is still pending.
  // Defer the resetting process in this case. The |reset_cb_| will be fired
  // after the read callback is fired - see DoDecryptBuffer() and
  // DoDeliverBuffer().
  if (state_ == kPendingDemuxerRead || state_ == kPendingDecrypt) {
    DCHECK(!read_cb_.is_null());
    return;
  }

  if (state_ == kWaitingForKey) {
    DCHECK(!read_cb_.is_null());
    pending_buffer_to_decrypt_ = NULL;
    base::ResetAndReturn(&read_cb_).Run(kAborted, NULL);
  }

  DCHECK(read_cb_.is_null());
  DoReset();
}

const AudioDecoderConfig& DecryptingDemuxerStream::audio_decoder_config() {
  DCHECK(state_ != kUninitialized && state_ != kDecryptorRequested) << state_;
  CHECK_EQ(stream_type_, AUDIO);
  return *audio_config_;
}

const VideoDecoderConfig& DecryptingDemuxerStream::video_decoder_config() {
  DCHECK(state_ != kUninitialized && state_ != kDecryptorRequested) << state_;
  CHECK_EQ(stream_type_, VIDEO);
  return *video_config_;
}

DemuxerStream::Type DecryptingDemuxerStream::type() {
  DCHECK(state_ != kUninitialized && state_ != kDecryptorRequested) << state_;
  return stream_type_;
}

void DecryptingDemuxerStream::EnableBitstreamConverter() {
  demuxer_stream_->EnableBitstreamConverter();
}

DecryptingDemuxerStream::~DecryptingDemuxerStream() {}

void DecryptingDemuxerStream::DoInitialize(
    const scoped_refptr<DemuxerStream>& stream,
    const PipelineStatusCB& status_cb) {
  DVLOG(2) << "DoInitialize()";
  DCHECK(message_loop_->BelongsToCurrentThread());
  DCHECK_EQ(state_, kUninitialized) << state_;

  // Only valid potentially encrypted audio or video stream is accepted.
  if (!((stream->type() == AUDIO &&
         stream->audio_decoder_config().IsValidConfig() &&
         stream->audio_decoder_config().is_encrypted()) ||
        (stream->type() == VIDEO &&
         stream->video_decoder_config().IsValidConfig() &&
         stream->video_decoder_config().is_encrypted()))) {
    status_cb.Run(DEMUXER_ERROR_NO_SUPPORTED_STREAMS);
    return;
  }

  DCHECK(!demuxer_stream_);
  demuxer_stream_ = stream;
  stream_type_ = stream->type();
  init_cb_ = status_cb;

  state_ = kDecryptorRequested;
  request_decryptor_notification_cb_.Run(
      BIND_TO_LOOP(&DecryptingDemuxerStream::SetDecryptor));
}

void DecryptingDemuxerStream::SetDecryptor(Decryptor* decryptor) {
  DVLOG(2) << "SetDecryptor()";
  DCHECK(message_loop_->BelongsToCurrentThread());
  DCHECK_EQ(state_, kDecryptorRequested) << state_;
  DCHECK(!init_cb_.is_null());
  DCHECK(!request_decryptor_notification_cb_.is_null());

  request_decryptor_notification_cb_.Reset();
  decryptor_ = decryptor;

  switch (stream_type_) {
    case AUDIO: {
      const AudioDecoderConfig& input_audio_config =
          demuxer_stream_->audio_decoder_config();
      audio_config_.reset(new AudioDecoderConfig());
      audio_config_->Initialize(input_audio_config.codec(),
                                input_audio_config.bits_per_channel(),
                                input_audio_config.channel_layout(),
                                input_audio_config.samples_per_second(),
                                input_audio_config.extra_data(),
                                input_audio_config.extra_data_size(),
                                false,  // Output audio is not encrypted.
                                false);
      break;
    }

    case VIDEO: {
      const VideoDecoderConfig& input_video_config =
          demuxer_stream_->video_decoder_config();
      video_config_.reset(new VideoDecoderConfig());
      video_config_->Initialize(input_video_config.codec(),
                                input_video_config.profile(),
                                input_video_config.format(),
                                input_video_config.coded_size(),
                                input_video_config.visible_rect(),
                                input_video_config.natural_size(),
                                input_video_config.extra_data(),
                                input_video_config.extra_data_size(),
                                false,  // Output video is not encrypted.
                                false);
      break;
    }

    default:
      NOTREACHED();
      return;
  }

  decryptor_->RegisterKeyAddedCB(
      GetDecryptorStreamType(),
      BIND_TO_LOOP(&DecryptingDemuxerStream::OnKeyAdded));

  state_ = kIdle;
  base::ResetAndReturn(&init_cb_).Run(PIPELINE_OK);
}

void DecryptingDemuxerStream::DecryptBuffer(
    DemuxerStream::Status status,
    const scoped_refptr<DecoderBuffer>& buffer) {
  // In theory, we don't need to force post the task here, because we do a
  // force task post in DeliverBuffer(). Therefore, even if
  // demuxer_stream_->Read() execute the read callback on the same execution
  // stack we are still fine. But it looks like a force post task makes the
  // logic more understandable and manageable, so why not?
  message_loop_->PostTask(FROM_HERE, base::Bind(
      &DecryptingDemuxerStream::DoDecryptBuffer, this, status, buffer));
}

void DecryptingDemuxerStream::DoDecryptBuffer(
    DemuxerStream::Status status,
    const scoped_refptr<DecoderBuffer>& buffer) {
  DVLOG(3) << "DoDecryptBuffer()";
  DCHECK(message_loop_->BelongsToCurrentThread());
  DCHECK_EQ(state_, kPendingDemuxerRead) << state_;
  DCHECK(!read_cb_.is_null());
  DCHECK_EQ(buffer != NULL, status == kOk) << status;

  if (!reset_cb_.is_null()) {
    base::ResetAndReturn(&read_cb_).Run(kAborted, NULL);
    DoReset();
    return;
  }

  if (status == kAborted) {
    DVLOG(2) << "DoDecryptBuffer() - kAborted.";
    state_ = kIdle;
    base::ResetAndReturn(&read_cb_).Run(kAborted, NULL);
    return;
  }

  if (status == kConfigChanged) {
    DVLOG(2) << "DoDecryptBuffer() - kConfigChanged.";
    state_ = kIdle;
    // TODO(xhwang): Support kConfigChanged!
    base::ResetAndReturn(&read_cb_).Run(kAborted, NULL);
    return;
  }

  if (buffer->IsEndOfStream()) {
    DVLOG(2) << "DoDecryptBuffer() - EOS buffer.";
    state_ = kIdle;
    base::ResetAndReturn(&read_cb_).Run(status, buffer);
    return;
  }

  pending_buffer_to_decrypt_ = buffer;
  state_ = kPendingDecrypt;
  DecryptPendingBuffer();
}

void DecryptingDemuxerStream::DecryptPendingBuffer() {
  DCHECK(message_loop_->BelongsToCurrentThread());
  DCHECK_EQ(state_, kPendingDecrypt) << state_;
  decryptor_->Decrypt(
      GetDecryptorStreamType(),
      pending_buffer_to_decrypt_,
      base::Bind(&DecryptingDemuxerStream::DeliverBuffer, this));
}

void DecryptingDemuxerStream::DeliverBuffer(
    Decryptor::Status status,
    const scoped_refptr<DecoderBuffer>& decrypted_buffer) {
  // We need to force task post here because the DecryptCB can be executed
  // synchronously in Reset(). Instead of using more complicated logic in
  // those function to fix it, why not force task post here to make everything
  // simple and clear?
  message_loop_->PostTask(FROM_HERE, base::Bind(
      &DecryptingDemuxerStream::DoDeliverBuffer, this,
      status, decrypted_buffer));
}

void DecryptingDemuxerStream::DoDeliverBuffer(
    Decryptor::Status status,
    const scoped_refptr<DecoderBuffer>& decrypted_buffer) {
  DVLOG(3) << "DoDeliverBuffer() - status: " << status;
  DCHECK(message_loop_->BelongsToCurrentThread());
  DCHECK_EQ(state_, kPendingDecrypt) << state_;
  DCHECK_NE(status, Decryptor::kNeedMoreData);
  DCHECK(!read_cb_.is_null());
  DCHECK(pending_buffer_to_decrypt_);

  bool need_to_try_again_if_nokey = key_added_while_decrypt_pending_;
  key_added_while_decrypt_pending_ = false;

  if (!reset_cb_.is_null()) {
    pending_buffer_to_decrypt_ = NULL;
    base::ResetAndReturn(&read_cb_).Run(kAborted, NULL);
    DoReset();
    return;
  }

  DCHECK_EQ(status == Decryptor::kSuccess, decrypted_buffer.get() != NULL);

  if (status == Decryptor::kError) {
    DVLOG(2) << "DoDeliverBuffer() - kError";
    pending_buffer_to_decrypt_ = NULL;
    state_ = kIdle;
    base::ResetAndReturn(&read_cb_).Run(kAborted, NULL);
    return;
  }

  if (status == Decryptor::kNoKey) {
    DVLOG(2) << "DoDeliverBuffer() - kNoKey";
    if (need_to_try_again_if_nokey) {
      // The |state_| is still kPendingDecrypt.
      DecryptPendingBuffer();
      return;
    }

    state_ = kWaitingForKey;
    return;
  }

  DCHECK_EQ(status, Decryptor::kSuccess);
  pending_buffer_to_decrypt_ = NULL;
  state_ = kIdle;
  base::ResetAndReturn(&read_cb_).Run(kOk, decrypted_buffer);
}

void DecryptingDemuxerStream::OnKeyAdded() {
  DCHECK(message_loop_->BelongsToCurrentThread());

  if (state_ == kPendingDecrypt) {
    key_added_while_decrypt_pending_ = true;
    return;
  }

  if (state_ == kWaitingForKey) {
    state_ = kPendingDecrypt;
    DecryptPendingBuffer();
  }
}

void DecryptingDemuxerStream::DoReset() {
  DCHECK(init_cb_.is_null());
  DCHECK(read_cb_.is_null());
  state_ = kIdle;
  base::ResetAndReturn(&reset_cb_).Run();
}

Decryptor::StreamType DecryptingDemuxerStream::GetDecryptorStreamType() const {
  DCHECK(stream_type_ == AUDIO || stream_type_ == VIDEO);
  return stream_type_ == AUDIO ? Decryptor::kAudio : Decryptor::kVideo;
}

}  // namespace media
