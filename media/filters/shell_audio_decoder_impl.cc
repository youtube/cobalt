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

#include "media/filters/shell_audio_decoder_impl.h"

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/debug/trace_event.h"
#include "base/logging.h"
#include "media/base/bind_to_loop.h"
#include "media/base/pipeline_status.h"
#include "media/base/shell_media_statistics.h"
#include "media/mp4/aac.h"

namespace media {

using base::Time;
using base::TimeDelta;

const size_t kFramesPerAccessUnit = mp4::AAC::kFramesPerAccessUnit;

namespace {

bool ValidateConfig(const AudioDecoderConfig& config) {
  if (!config.IsValidConfig()) {
    NOTREACHED() << "Invalid audio stream -"
                 << " codec: " << config.codec()
                 << " channel layout: " << config.channel_layout()
                 << " bits per channel: " << config.bits_per_channel()
                 << " samples per second: " << config.samples_per_second();
    return false;
  }

  // check this is a config we can decode and play back
  if (config.codec() != kCodecAAC) {
    NOTREACHED();
    return false;
  }

  return true;
}

}  // namespace

//==============================================================================
// ShellAudioDecoderImpl
//
ShellAudioDecoderImpl::ShellAudioDecoderImpl(
    const scoped_refptr<base::MessageLoopProxy>& message_loop,
    ShellRawAudioDecoderFactory* raw_audio_decoder_factory)
    : message_loop_(message_loop),
      raw_audio_decoder_factory_(raw_audio_decoder_factory),
      demuxer_read_and_decode_in_progress_(false),
      samples_per_second_(0),
      num_channels_(0) {
  DCHECK(message_loop_);
  DCHECK(raw_audio_decoder_factory);
}

ShellAudioDecoderImpl::~ShellAudioDecoderImpl() {
  DCHECK(read_cb_.is_null());
  DCHECK(reset_cb_.is_null());
}

void ShellAudioDecoderImpl::Initialize(
    const scoped_refptr<DemuxerStream>& stream,
    const PipelineStatusCB& status_cb,
    const StatisticsCB& statistics_cb) {
  TRACE_EVENT0("media_stack", "ShellAudioDecoderImpl::Initialize()");

  DCHECK(stream);
  DCHECK(!status_cb.is_null());
  DCHECK(!statistics_cb.is_null());
  DCHECK(!demuxer_stream_);

  demuxer_stream_ = stream;
  statistics_cb_ = statistics_cb;
  const AudioDecoderConfig& config = demuxer_stream_->audio_decoder_config();

  // check config for support
  if (!ValidateConfig(config)) {
    status_cb.Run(DECODER_ERROR_NOT_SUPPORTED);
    return;
  }

  samples_per_second_ = config.samples_per_second();
  num_channels_ = ChannelLayoutToChannelCount(config.channel_layout());

#if __SAVE_DECODER_OUTPUT__
  test_probe_.Initialize("shell_audio_decoder_probe.wav", num_channels_,
                         config.samples_per_second(), bits_per_channel(),
                         bits_per_channel() == sizeof(float));  // use_floats?
  test_probe_.CloseAfter(30 * 1000);
#endif

  UPDATE_MEDIA_STATISTICS(STAT_TYPE_AUDIO_CODEC, config.codec());
  UPDATE_MEDIA_STATISTICS(STAT_TYPE_AUDIO_CHANNELS, num_channels_);
  UPDATE_MEDIA_STATISTICS(STAT_TYPE_AUDIO_SAMPLE_PER_SECOND,
                          samples_per_second_);

  LOG(INFO) << "Configuration at Start: "
            << demuxer_stream_->audio_decoder_config().AsHumanReadableString();
  raw_decoder_ = raw_audio_decoder_factory_->Create(config);

  if (!raw_decoder_) {
    status_cb.Run(DECODER_ERROR_NOT_SUPPORTED);
    return;
  }

  status_cb.Run(PIPELINE_OK);
}

void ShellAudioDecoderImpl::Read(const AudioDecoder::ReadCB& read_cb) {
  TRACE_EVENT0("media_stack", "ShellAudioDecoderImpl::Read()");

  // This may be called from another thread (H/W audio thread) so redirect
  // this request to the decoder's message loop
  if (!message_loop_->BelongsToCurrentThread()) {
    message_loop_->PostTask(
        FROM_HERE, base::Bind(&ShellAudioDecoderImpl::Read, this, read_cb));
    return;
  }

  DCHECK(demuxer_stream_);  // Initialize() has been called.
  DCHECK(!read_cb.is_null());
  DCHECK(read_cb_.is_null());   // No Read() in progress.
  DCHECK(reset_cb_.is_null());  //  No Reset() in progress.

  if (queued_buffers_.empty()) {
    read_cb_ = read_cb;
  } else {
    scoped_refptr<DecoderBuffer> buffer = queued_buffers_.front();
    queued_buffers_.pop();
    TRACE_EVENT1("media_stack",
                 "ShellAudioDecoderImpl::Read() deliver audio data.",
                 "timestamp", buffer->GetTimestamp().InMicroseconds());

    Buffers buffers;
    buffers.push_back(buffer);
    read_cb.Run(kOk, buffers);
  }

  TryToReadFromDemuxerStream();
}

int ShellAudioDecoderImpl::bits_per_channel() {
  DCHECK(raw_decoder_);

  if (raw_decoder_) {
    return raw_decoder_->GetBytesPerSample() * 8;
  }

  return sizeof(float) * 8;
}

ChannelLayout ShellAudioDecoderImpl::channel_layout() {
  DCHECK(demuxer_stream_);
  const AudioDecoderConfig& config = demuxer_stream_->audio_decoder_config();
  return config.channel_layout();
}

int ShellAudioDecoderImpl::samples_per_second() {
  return samples_per_second_;
}

void ShellAudioDecoderImpl::Reset(const base::Closure& reset_cb) {
  TRACE_EVENT0("media_stack", "ShellAudioDecoderImpl::Reset()");

  if (!message_loop_->BelongsToCurrentThread()) {
    message_loop_->PostTask(
        FROM_HERE, base::Bind(&ShellAudioDecoderImpl::Reset, this, reset_cb));
    return;
  }

  DCHECK(demuxer_stream_);  // Initialize() has been called.
  DCHECK(!reset_cb.is_null());
  DCHECK(reset_cb_.is_null());  // No Reset() in progress.

  if (demuxer_read_and_decode_in_progress_) {
    reset_cb_ = reset_cb;
    return;
  }

#if __SAVE_DECODER_OUTPUT__
  test_probe_.Close();
#endif

  DCHECK(read_cb_.is_null());  // No Read() in progress.

  // Release the buffers we queued internally
  while (!queued_buffers_.empty()) {
    queued_buffers_.pop();
  }
  raw_decoder_->Flush();
  eos_buffer_ = NULL;
  reset_cb.Run();
}

void ShellAudioDecoderImpl::TryToReadFromDemuxerStream() {
  DCHECK(message_loop_->BelongsToCurrentThread());

  if (!demuxer_read_and_decode_in_progress_ &&
      queued_buffers_.size() < kMaxQueuedBuffers) {
    demuxer_read_and_decode_in_progress_ = true;
    if (eos_buffer_) {
      // We have already received EOS from demuxer stream, so we no longer need
      // to request from DemuxerStream.  However, we still have to send the eos
      // buffer to raw decoder so it can keep decoding any left over buffers.
      TRACE_EVENT0("media_stack",
                   "ShellAudioDecoderImpl::TryToReadFromDemuxerStream() EOS");
      DCHECK(eos_buffer_);
      message_loop_->PostTask(
          FROM_HERE, base::Bind(&ShellAudioDecoderImpl::OnDemuxerRead, this,
                                DemuxerStream::kOk, eos_buffer_));
    } else {
      TRACE_EVENT0("media_stack",
                   "ShellAudioDecoderImpl::TryToReadFromDemuxerStream() Read");
      demuxer_stream_->Read(BindToCurrentLoop(
          base::Bind(&ShellAudioDecoderImpl::OnDemuxerRead, this)));
    }
  }
}

void ShellAudioDecoderImpl::OnDemuxerRead(
    DemuxerStream::Status status,
    const scoped_refptr<DecoderBuffer>& buffer) {
  TRACE_EVENT0("media_stack", "ShellAudioDecoderImpl::OnDemuxerRead()");

  if (!message_loop_->BelongsToCurrentThread()) {
    message_loop_->PostTask(
        FROM_HERE, base::Bind(&ShellAudioDecoderImpl::OnDemuxerRead, this,
                              status, buffer));
    return;
  }

#if !defined(__LB_SHELL__FOR_RELEASE__)
  // What the test does is:
  //   Play for kTimeBetweenUnderflow + kTimeToUnderflow;
  //   for (;;) {
  //     Play for kTimeBetweenUnderflow;
  //     Underflow for kTimeToUnderflow;
  //   }
  const bool kEnableUnderflowTest = false;
  if (kEnableUnderflowTest) {
    static const TimeDelta kTimeBetweenUnderflow =
        TimeDelta::FromMilliseconds(27293);
    static const TimeDelta kTimeToUnderflow = TimeDelta::FromMilliseconds(4837);
    static Time last_hit;

    if (status == DemuxerStream::kOk && !buffer->IsEndOfStream() &&
        buffer->GetTimestamp().InMicroseconds() == 0) {
      last_hit = Time::Now();
    }
    if (Time::Now() - last_hit > kTimeBetweenUnderflow + kTimeToUnderflow) {
      last_hit = Time::Now();
      message_loop_->PostDelayedTask(
          FROM_HERE, base::Bind(&ShellAudioDecoderImpl::OnDemuxerRead, this,
                                status, buffer),
          kTimeToUnderflow);
      return;
    }
  }
#endif  // !defined(__LB_SHELL__FOR_RELEASE__)

  DCHECK(demuxer_read_and_decode_in_progress_);

  if (status == DemuxerStream::kOk) {
    if (buffer->IsEndOfStream()) {
      TRACE_EVENT0("media_stack",
                   "ShellAudioDecoderImpl::DecodeBuffer() EOS reached.");
      eos_buffer_ = buffer;
    }
    raw_decoder_->Decode(
        buffer, base::Bind(&ShellAudioDecoderImpl::OnBufferDecoded, this));
    return;
  }

  DCHECK(!buffer);
  demuxer_read_and_decode_in_progress_ = false;

  if (status == DemuxerStream::kConfigChanged) {
    // We don't support audio config change.
    // We must call audio_decoder_config() to acknowledge the config change.
    // Otherwise, the demuxer will keep sending back kConfigChange for any
    // Read() call until the config change is acknowledged.
    demuxer_stream_->audio_decoder_config();
    TryToReadFromDemuxerStream();
  } else {
    DCHECK_EQ(status, DemuxerStream::kAborted);
    // We are seeking or stopping, fulfill any outstanding callbacks.
    if (!read_cb_.is_null()) {
      base::ResetAndReturn(&read_cb_).Run(kAborted, Buffers());
    }
    if (!reset_cb_.is_null()) {
      Reset(base::ResetAndReturn(&reset_cb_));
    }
  }
}

void ShellAudioDecoderImpl::OnBufferDecoded(
    ShellRawAudioDecoder::DecodeStatus status,
    const scoped_refptr<DecoderBuffer>& buffer) {
  if (!message_loop_->BelongsToCurrentThread()) {
    message_loop_->PostTask(
        FROM_HERE, base::Bind(&ShellAudioDecoderImpl::OnBufferDecoded, this,
                              status, buffer));
    return;
  }

  DCHECK(demuxer_read_and_decode_in_progress_);
  demuxer_read_and_decode_in_progress_ = false;

  bool eos_decoded = false;
  if (status == ShellRawAudioDecoder::BUFFER_DECODED) {
    DCHECK(buffer);
    queued_buffers_.push(buffer);

    eos_decoded = buffer->IsEndOfStream();
    if (!eos_decoded) {
      TRACE_EVENT1("media_stack",
                   "ShellAudioDecoderImpl::DecodeBuffer() data decoded.",
                   "timestamp", buffer->GetTimestamp().InMicroseconds());
      DCHECK_EQ(buffer->GetDataSize(),
                kFramesPerAccessUnit * bits_per_channel() / 8 * num_channels_);

      PipelineStatistics statistics;
      statistics.audio_bytes_decoded = buffer->GetDataSize();
      statistics_cb_.Run(statistics);

#if __SAVE_DECODER_OUTPUT__
      test_probe_.AddData(buffer->GetData());
#endif
    }
  } else if (status == ShellRawAudioDecoder::NEED_MORE_DATA) {
    DCHECK_EQ(status, ShellRawAudioDecoder::NEED_MORE_DATA);
    DCHECK(!buffer);
    if (reset_cb_.is_null()) {
      TryToReadFromDemuxerStream();
    } else {
      Reset(base::ResetAndReturn(&reset_cb_));
    }
    return;
  }

  if (!read_cb_.is_null()) {
    DCHECK(!queued_buffers_.empty());
    scoped_refptr<DecoderBuffer> buffer = queued_buffers_.front();
    queued_buffers_.pop();
    TRACE_EVENT1("media_stack",
                 "ShellAudioDecoderImpl::OnBufferDecoded() deliver audio data.",
                 "timestamp", buffer->GetTimestamp().InMicroseconds());

    Buffers buffers;
    buffers.push_back(buffer);
    base::ResetAndReturn(&read_cb_).Run(kOk, buffers);
  }

  if (reset_cb_.is_null()) {
    if (!eos_decoded) {
      TryToReadFromDemuxerStream();
    }
  } else {
    Reset(base::ResetAndReturn(&reset_cb_));
  }
}

}  // namespace media
