// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/decrypting_audio_decoder.h"

#include <cstdlib>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/message_loop_proxy.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/bind_to_loop.h"
#include "media/base/buffers.h"
#include "media/base/data_buffer.h"
#include "media/base/decoder_buffer.h"
#include "media/base/decryptor.h"
#include "media/base/demuxer_stream.h"
#include "media/base/pipeline.h"

namespace media {

#define BIND_TO_LOOP(function) \
    media::BindToLoop(message_loop_, base::Bind(function, this))

static inline bool IsOutOfSync(const base::TimeDelta& timestamp_1,
                               const base::TimeDelta& timestamp_2) {
  // Out of sync of 100ms would be pretty noticeable and we should keep any
  // drift below that.
  const int64 kOutOfSyncThresholdInMicroseconds = 100000;
  return std::abs(timestamp_1.InMicroseconds() - timestamp_2.InMicroseconds()) >
         kOutOfSyncThresholdInMicroseconds;
}

DecryptingAudioDecoder::DecryptingAudioDecoder(
    const scoped_refptr<base::MessageLoopProxy>& message_loop,
    const RequestDecryptorNotificationCB& request_decryptor_notification_cb)
    : message_loop_(message_loop),
      state_(kUninitialized),
      request_decryptor_notification_cb_(request_decryptor_notification_cb),
      decryptor_(NULL),
      key_added_while_decode_pending_(false),
      bits_per_channel_(0),
      channel_layout_(CHANNEL_LAYOUT_NONE),
      samples_per_second_(0),
      bytes_per_sample_(0),
      output_timestamp_base_(kNoTimestamp()),
      total_samples_decoded_(0) {
}

void DecryptingAudioDecoder::Initialize(
    const scoped_refptr<DemuxerStream>& stream,
    const PipelineStatusCB& status_cb,
    const StatisticsCB& statistics_cb) {
  if (!message_loop_->BelongsToCurrentThread()) {
    message_loop_->PostTask(FROM_HERE, base::Bind(
        &DecryptingAudioDecoder::DoInitialize, this,
        stream, status_cb, statistics_cb));
    return;
  }
  DoInitialize(stream, status_cb, statistics_cb);
}

void DecryptingAudioDecoder::Read(const ReadCB& read_cb) {
  // Complete operation asynchronously on different stack of execution as per
  // the API contract of AudioDecoder::Read()
  message_loop_->PostTask(FROM_HERE, base::Bind(
      &DecryptingAudioDecoder::DoRead, this, read_cb));
}

void DecryptingAudioDecoder::Reset(const base::Closure& closure) {
  if (!message_loop_->BelongsToCurrentThread()) {
    message_loop_->PostTask(FROM_HERE, base::Bind(
        &DecryptingAudioDecoder::Reset, this, closure));
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

  decryptor_->ResetDecoder(Decryptor::kAudio);

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
    base::ResetAndReturn(&read_cb_).Run(kAborted, NULL);
  }

  DCHECK(read_cb_.is_null());
  DoReset();
}

int DecryptingAudioDecoder::bits_per_channel() {
  return bits_per_channel_;
}

ChannelLayout DecryptingAudioDecoder::channel_layout() {
  return channel_layout_;
}

int DecryptingAudioDecoder::samples_per_second() {
  return samples_per_second_;
}

DecryptingAudioDecoder::~DecryptingAudioDecoder() {
}

void DecryptingAudioDecoder::DoInitialize(
    const scoped_refptr<DemuxerStream>& stream,
    const PipelineStatusCB& status_cb,
    const StatisticsCB& statistics_cb) {
  DVLOG(2) << "DoInitialize()";
  DCHECK(message_loop_->BelongsToCurrentThread());
  DCHECK_EQ(state_, kUninitialized) << state_;
  DCHECK(stream);

  const AudioDecoderConfig& config = stream->audio_decoder_config();
  if (!config.IsValidConfig()) {
    DLOG(ERROR) << "Invalid audio stream config.";
    status_cb.Run(PIPELINE_ERROR_DECODE);
    return;
  }

  // DecryptingAudioDecoder only accepts potentially encrypted stream.
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
      BIND_TO_LOOP(&DecryptingAudioDecoder::SetDecryptor));
}

void DecryptingAudioDecoder::SetDecryptor(Decryptor* decryptor) {
  DVLOG(2) << "SetDecryptor()";
  DCHECK(message_loop_->BelongsToCurrentThread());
  DCHECK_EQ(state_, kDecryptorRequested) << state_;
  DCHECK(!init_cb_.is_null());
  DCHECK(!request_decryptor_notification_cb_.is_null());

  request_decryptor_notification_cb_.Reset();
  decryptor_ = decryptor;

  scoped_ptr<AudioDecoderConfig> scoped_config(new AudioDecoderConfig());
  scoped_config->CopyFrom(demuxer_stream_->audio_decoder_config());

  state_ = kPendingDecoderInit;
  decryptor_->InitializeAudioDecoder(
      scoped_config.Pass(),
      BIND_TO_LOOP(&DecryptingAudioDecoder::FinishInitialization));
}

void DecryptingAudioDecoder::FinishInitialization(bool success) {
  DVLOG(2) << "FinishInitialization()";
  DCHECK(message_loop_->BelongsToCurrentThread());
  DCHECK_EQ(state_, kPendingDecoderInit) << state_;
  DCHECK(!init_cb_.is_null());
  DCHECK(reset_cb_.is_null());  // No Reset() before initialization finished.
  DCHECK(read_cb_.is_null());  // No Read() before initialization finished.

  if (!success) {
    base::ResetAndReturn(&init_cb_).Run(DECODER_ERROR_NOT_SUPPORTED);
    state_ = kDecodeFinished;
    return;
  }

  // Success!
  const AudioDecoderConfig& config = demuxer_stream_->audio_decoder_config();
  bits_per_channel_ = config.bits_per_channel();
  channel_layout_ = config.channel_layout();
  samples_per_second_ = config.samples_per_second();
  const int kBitsPerByte = 8;
  bytes_per_sample_ = ChannelLayoutToChannelCount(channel_layout_) *
      bits_per_channel_ / kBitsPerByte;

  decryptor_->RegisterKeyAddedCB(
      Decryptor::kAudio, BIND_TO_LOOP(&DecryptingAudioDecoder::OnKeyAdded));

  state_ = kIdle;
  base::ResetAndReturn(&init_cb_).Run(PIPELINE_OK);
}

void DecryptingAudioDecoder::DoRead(const ReadCB& read_cb) {
  DVLOG(3) << "DoRead()";
  DCHECK(message_loop_->BelongsToCurrentThread());
  DCHECK(state_ == kIdle || state_ == kDecodeFinished) << state_;
  DCHECK(!read_cb.is_null());
  CHECK(read_cb_.is_null()) << "Overlapping decodes are not supported.";

  // Return empty (end-of-stream) frames if decoding has finished.
  if (state_ == kDecodeFinished) {
    read_cb.Run(kOk, scoped_refptr<Buffer>(new DataBuffer(0)));
    return;
  }

  if (!queued_audio_frames_.empty()) {
    read_cb.Run(kOk, queued_audio_frames_.front());
    queued_audio_frames_.pop_front();
    return;
  }

  read_cb_ = read_cb;
  state_ = kPendingDemuxerRead;
  ReadFromDemuxerStream();
}

void DecryptingAudioDecoder::ReadFromDemuxerStream() {
  DCHECK(message_loop_->BelongsToCurrentThread());
  DCHECK_EQ(state_, kPendingDemuxerRead) << state_;
  DCHECK(!read_cb_.is_null());

  demuxer_stream_->Read(
      base::Bind(&DecryptingAudioDecoder::DoDecryptAndDecodeBuffer, this));
}

void DecryptingAudioDecoder::DoDecryptAndDecodeBuffer(
    DemuxerStream::Status status,
    const scoped_refptr<DecoderBuffer>& buffer) {
  if (!message_loop_->BelongsToCurrentThread()) {
    message_loop_->PostTask(FROM_HERE, base::Bind(
        &DecryptingAudioDecoder::DoDecryptAndDecodeBuffer, this,
        status, buffer));
    return;
  }

  DVLOG(3) << "DoDecryptAndDecodeBuffer()";
  DCHECK_EQ(state_, kPendingDemuxerRead) << state_;
  DCHECK(!read_cb_.is_null());
  DCHECK_EQ(buffer != NULL, status == DemuxerStream::kOk) << status;

  if (!reset_cb_.is_null()) {
    base::ResetAndReturn(&read_cb_).Run(kAborted, NULL);
    DoReset();
    return;
  }

  if (status == DemuxerStream::kAborted) {
    DVLOG(2) << "DoDecryptAndDecodeBuffer() - kAborted";
    state_ = kIdle;
    base::ResetAndReturn(&read_cb_).Run(kAborted, NULL);
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

  // Initialize the |next_output_timestamp_| to be the timestamp of the first
  // non-EOS buffer.
  if (output_timestamp_base_ == kNoTimestamp() && !buffer->IsEndOfStream()) {
    DCHECK_EQ(total_samples_decoded_, 0);
    output_timestamp_base_ = buffer->GetTimestamp();
  }

  pending_buffer_to_decode_ = buffer;
  state_ = kPendingDecode;
  DecodePendingBuffer();
}

void DecryptingAudioDecoder::DecodePendingBuffer() {
  DCHECK(message_loop_->BelongsToCurrentThread());
  DCHECK_EQ(state_, kPendingDecode) << state_;
  decryptor_->DecryptAndDecodeAudio(
      pending_buffer_to_decode_,
      base::Bind(&DecryptingAudioDecoder::DeliverFrame, this,
                 pending_buffer_to_decode_->GetDataSize()));
}

void DecryptingAudioDecoder::DeliverFrame(
    int buffer_size,
    Decryptor::Status status,
    const Decryptor::AudioBuffers& frames) {
  // We need to force task post here because the AudioDecodeCB can be executed
  // synchronously in Reset(). Instead of using more complicated logic in
  // those function to fix it, why not force task post here to make everything
  // simple and clear?
  message_loop_->PostTask(FROM_HERE, base::Bind(
      &DecryptingAudioDecoder::DoDeliverFrame, this,
      buffer_size, status, frames));
}

void DecryptingAudioDecoder::DoDeliverFrame(
    int buffer_size,
    Decryptor::Status status,
    const Decryptor::AudioBuffers& frames) {
  DVLOG(3) << "DoDeliverFrame() - status: " << status;
  DCHECK(message_loop_->BelongsToCurrentThread());
  DCHECK_EQ(state_, kPendingDecode) << state_;
  DCHECK(!read_cb_.is_null());
  DCHECK(pending_buffer_to_decode_);
  DCHECK(queued_audio_frames_.empty());

  bool need_to_try_again_if_nokey_is_returned = key_added_while_decode_pending_;
  key_added_while_decode_pending_ = false;

  scoped_refptr<DecoderBuffer> scoped_pending_buffer_to_decode =
      pending_buffer_to_decode_;
  pending_buffer_to_decode_ = NULL;

  if (!reset_cb_.is_null()) {
    base::ResetAndReturn(&read_cb_).Run(kAborted, NULL);
    DoReset();
    return;
  }

  DCHECK_EQ(status == Decryptor::kSuccess, !frames.empty());

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
    statistics.audio_bytes_decoded = buffer_size;
    statistics_cb_.Run(statistics);
  }

  if (status == Decryptor::kNeedMoreData) {
    DVLOG(2) << "DoDeliverFrame() - kNeedMoreData";
    if (scoped_pending_buffer_to_decode->IsEndOfStream()) {
      state_ = kDecodeFinished;
      base::ResetAndReturn(&read_cb_).Run(
          kOk, scoped_refptr<Buffer>(new DataBuffer(0)));
      return;
    }

    state_ = kPendingDemuxerRead;
    ReadFromDemuxerStream();
    return;
  }

  DCHECK_EQ(status, Decryptor::kSuccess);
  DCHECK(!frames.empty());
  EnqueueFrames(frames);

  state_ = kIdle;
  base::ResetAndReturn(&read_cb_).Run(kOk, queued_audio_frames_.front());
  queued_audio_frames_.pop_front();
}

void DecryptingAudioDecoder::OnKeyAdded() {
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

void DecryptingAudioDecoder::DoReset() {
  DCHECK(init_cb_.is_null());
  DCHECK(read_cb_.is_null());
  output_timestamp_base_ = kNoTimestamp();
  total_samples_decoded_ = 0;
  state_ = kIdle;
  base::ResetAndReturn(&reset_cb_).Run();
}

void DecryptingAudioDecoder::EnqueueFrames(
    const Decryptor::AudioBuffers& frames) {
  queued_audio_frames_ = frames;

  for (Decryptor::AudioBuffers::iterator iter = queued_audio_frames_.begin();
      iter != queued_audio_frames_.end();
      ++iter) {
    scoped_refptr<Buffer>& frame = *iter;

    DCHECK(!frame->IsEndOfStream()) << "EOS frame returned.";
    DCHECK_GT(frame->GetDataSize(), 0) << "Empty frame returned.";

    base::TimeDelta cur_timestamp = output_timestamp_base_ +
        NumberOfSamplesToDuration(total_samples_decoded_);
    if (IsOutOfSync(cur_timestamp, frame->GetTimestamp())) {
      DVLOG(1)  << "Timestamp returned by the decoder ("
                << frame->GetTimestamp().InMilliseconds() << " ms)"
                << " does not match the input timestamp and number of samples"
                << " decoded (" << cur_timestamp.InMilliseconds() << " ms).";
    }
    frame->SetTimestamp(cur_timestamp);

    int frame_size = frame->GetDataSize();
    DCHECK_EQ(frame_size % bytes_per_sample_, 0) <<
        "Decoder didn't output full samples";
    int samples_decoded = frame_size / bytes_per_sample_;
    total_samples_decoded_ += samples_decoded;

    base::TimeDelta next_timestamp = output_timestamp_base_ +
        NumberOfSamplesToDuration(total_samples_decoded_);
    base::TimeDelta duration = next_timestamp - cur_timestamp;
    frame->SetDuration(duration);
  }
}

base::TimeDelta DecryptingAudioDecoder::NumberOfSamplesToDuration(
    int number_of_samples) const {
  DCHECK(samples_per_second_);
  return base::TimeDelta::FromMicroseconds(base::Time::kMicrosecondsPerSecond *
                                           number_of_samples /
                                           samples_per_second_);
}

}  // namespace media
