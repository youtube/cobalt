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
#include "base/logging.h"
#include "base/stringprintf.h"
#include "lb_audio_decoder.h"
#include "lb_memory_manager.h"
#include "media/base/pipeline_status.h"
#include "media/base/shell_buffer_factory.h"
#include "media/base/shell_filter_graph_log.h"
#include "media/base/shell_filter_graph_log_constants.h"
#include "media/base/video_frame.h"
#include "media/filters/shell_avc_parser.h"
#include "media/mp4/aac.h"

using base::TimeDelta;

namespace media {

// TODO(***REMOVED***) : These should be eventually get from the low level decoder.
#if defined(__LB_ANDROID__)
const int kAudioBytesPerSample = sizeof(int16);
#elif defined(__LB_LINUX__)
const int kAudioBytesPerSample = sizeof(float);
#elif defined(__LB_PS4__)
const int kAudioBytesPerSample = sizeof(float);
#elif defined(__LB_WIIU__)
const int kAudioBytesPerSample = sizeof(int16_t);
#else
#error kAudioBytesPerSample has to be specified!
#endif

//==============================================================================
// Static ShellAudioDecoder Methods
//

// static
ShellAudioDecoder* ShellAudioDecoder::Create(
    const scoped_refptr<base::MessageLoopProxy>& message_loop) {
  return new ShellAudioDecoderImpl(message_loop);
}

//==============================================================================
// ShellAudioDecoderImpl
//
ShellAudioDecoderImpl::ShellAudioDecoderImpl(
    const scoped_refptr<base::MessageLoopProxy>& message_loop)
    : message_loop_(message_loop)
    , shell_audio_decoder_status_(kUninitialized)
    , samples_per_second_(0)
    , num_channels_(0)
    , raw_decoder_(NULL)
    , pending_renderer_read_(false)
    , pending_demuxer_read_(false) {
}

ShellAudioDecoderImpl::~ShellAudioDecoderImpl() {
  delete raw_decoder_;
  DCHECK(!pending_renderer_read_);
  DCHECK(read_cb_.is_null());
  DCHECK(reset_cb_.is_null());
}

void ShellAudioDecoderImpl::Initialize(
    const scoped_refptr<DemuxerStream> &stream,
    const media::PipelineStatusCB &status_cb,
    const media::StatisticsCB &statistics_cb) {
  filter_graph_log_ = stream->filter_graph_log();
  filter_graph_log_->LogEvent(kObjectIdAudioDecoder, kEventInitialize);
  demuxer_stream_ = stream;
  statistics_cb_ = statistics_cb;
  const AudioDecoderConfig& config = demuxer_stream_->audio_decoder_config();

  // check config for support
  if (!ValidateConfig(config)) {
    status_cb.Run(DECODER_ERROR_NOT_SUPPORTED);
    return;
  }

  samples_per_second_= config.samples_per_second();
  num_channels_ = ChannelLayoutToChannelCount(config.channel_layout());

#if __SAVE_DECODER_OUTPUT__
  test_probe_.Initialize("shell_audio_decoder_probe.wav",
                         num_channels_,
                         config.samples_per_second(),
                         bits_per_channel(),
                         bits_per_channel() == sizeof(float));  // use_floats?
  test_probe_.CloseAfter(30 * 1000);
#endif

  pending_renderer_read_ = false;
  pending_demuxer_read_ = false;

  raw_decoder_ = LB::LBAudioDecoder::Create(filter_graph_log_);
  if (raw_decoder_ == NULL) {
    status_cb.Run(PIPELINE_ERROR_DECODE);
    return;
  }

  raw_decoder_->SetDecryptor(demuxer_stream_->GetDecryptor());
  if (!raw_decoder_->UpdateConfig(config)) {
    status_cb.Run(DECODER_ERROR_NOT_SUPPORTED);
    return;
  }

  // success!
  shell_audio_decoder_status_ = kNormal;
  status_cb.Run(PIPELINE_OK);
}

bool ShellAudioDecoderImpl::ValidateConfig(const AudioDecoderConfig& config) {
  if (!config.IsValidConfig()) {
    DLOG(ERROR) << "Invalid audio stream -"
                << " codec: " << config.codec()
                << " channel layout: " << config.channel_layout()
                << " bits per channel: " << config.bits_per_channel()
                << " samples per second: " << config.samples_per_second();
    return false;
  }
  // check this is a config we can decode and play back
  if (config.codec() != kCodecAAC) {
    return false;
  }

  // Now we only support stereo and mono streams
  // TODO(***REMOVED***) : Get this from low-level decoder or streamer.
  int channels = ChannelLayoutToChannelCount(config.channel_layout());
  if (channels != 1 && channels != 2 && channels != 6 && channels != 8) {
    return false;
  }
  return true;
}

void ShellAudioDecoderImpl::Read(const media::AudioDecoder::ReadCB& read_cb) {
  // This may be called from another thread (H/W audio thread) so redirect
  // this request to the decoder's message loop
  if (!message_loop_->BelongsToCurrentThread()) {
    message_loop_->PostTask(FROM_HERE, base::Bind(
        &ShellAudioDecoderImpl::Read, this, read_cb));
    return;
  }

  DCHECK(message_loop_->BelongsToCurrentThread());
  DCHECK(!read_cb.is_null());
  // Save the callback before posting the task
  DCHECK(read_cb_.is_null());

  // Save the callback and then process the task
  read_cb_ = read_cb;
  DoRead();
}

void ShellAudioDecoderImpl::QueueBuffer(DemuxerStream::Status status,
    const scoped_refptr<ShellBuffer>& buffer) {
  if (!message_loop_->BelongsToCurrentThread()) {
    message_loop_->PostTask(FROM_HERE, base::Bind(
        &ShellAudioDecoderImpl::QueueBuffer, this, status, buffer));
    return;
  }
  DCHECK(message_loop_->BelongsToCurrentThread());
  DCHECK(pending_demuxer_read_);
  pending_demuxer_read_ = false;

  queued_buffers_.push_back(std::make_pair(status, buffer));
  if (pending_renderer_read_) {
    DCHECK(!read_cb_.is_null());
    pending_renderer_read_ = false;
    DoDecodeBuffer();
  }

  if (status == DemuxerStream::kOk && !buffer->IsEndOfStream()) {
    message_loop_->PostTask(FROM_HERE, base::Bind(
        &ShellAudioDecoderImpl::RequestBuffer, this));
  }

  if (shell_audio_decoder_status_ == kFlushing && !pending_renderer_read_) {
    // Reset was called, but there were pending reads.
    // Call the Reset callback now
    DCHECK(!reset_cb_.is_null());
    message_loop_->PostTask(FROM_HERE, base::Bind(
        &ShellAudioDecoderImpl::Reset,
        this,
        reset_cb_));
    reset_cb_.Reset();
  }
}

void ShellAudioDecoderImpl::RequestBuffer() {
  // check for EOS
  if (!pending_demuxer_read_ && queued_buffers_.size() < kMaxQueuedBuffers
      && shell_audio_decoder_status_ == kNormal) {
    pending_demuxer_read_ = true;
    demuxer_stream_->Read(base::Bind(&ShellAudioDecoderImpl::QueueBuffer,
                                     this));
  }
}

void ShellAudioDecoderImpl::DoRead() {
  DCHECK(message_loop_->BelongsToCurrentThread());
  DCHECK_NE(shell_audio_decoder_status_, kUninitialized);
  DCHECK(!read_cb_.is_null());
  filter_graph_log_->LogEvent(kObjectIdAudioDecoder, kEventRead);

  if (shell_audio_decoder_status_ == kNormal) {
    DoDecodeBuffer();
    RequestBuffer();
  } else if (shell_audio_decoder_status_ == kStopped) {
    filter_graph_log_->LogEvent(kObjectIdAudioDecoder, kEventEndOfStreamSent);
    base::ResetAndReturn(&read_cb_).Run(kOk,
                                        ShellBuffer::CreateEOSBuffer(
                                            kNoTimestamp(),
                                            filter_graph_log_));
  } else {
    // report decode error downstream
    base::ResetAndReturn(&read_cb_).Run(AudioDecoder::kDecodeError, NULL);
  }
}

void ShellAudioDecoderImpl::DoDecodeBuffer() {
  DCHECK(message_loop_->BelongsToCurrentThread());
  DCHECK(!read_cb_.is_null());
  filter_graph_log_->LogEvent(kObjectIdAudioDecoder, kEventDecode);

  // drop input data if we are flushing or stopped
  if (shell_audio_decoder_status_ != kNormal) {
    return;
  }

  // No data is queued up, so save this for later
  if (queued_buffers_.empty()) {
    DCHECK(!pending_renderer_read_);
    pending_renderer_read_ = true;
    return;
  }

  // Get the buffer from the front of the queue
  QueuedBuffer queued_buffer = queued_buffers_.front();
  queued_buffers_.pop_front();

  DemuxerStream::Status status = queued_buffer.first;
  const scoped_refptr<ShellBuffer>& buffer = queued_buffer.second;

  // check for demuxer error
  if (status != DemuxerStream::kOk) {
    DCHECK(!buffer);
    AudioDecoder::Status decoder_status =
        (status == DemuxerStream::kAborted) ? kAborted : kDecodeError;
    DLOG_IF(WARNING, decoder_status == kDecodeError) << "Demuxer error";
    base::ResetAndReturn(&read_cb_).Run(decoder_status, NULL);
    return;
  }

  // check for EndOfStream, if so flush the input queue.
  if (buffer->IsEndOfStream()) {
    filter_graph_log_->LogEvent(kObjectIdAudioDecoder,
                                kEventEndOfStreamReceived);
    // Consume any additional EOS buffers that are queued up
    while (!queued_buffers_.empty()) {
      DCHECK(queued_buffers_.front().second->IsEndOfStream());
      queued_buffers_.pop_front();
    }
  }

  scoped_refptr<ShellBuffer> decoded_buffer = raw_decoder_->Decode(buffer);
  if (!decoded_buffer) {
    DCHECK(!pending_renderer_read_);
    pending_renderer_read_ = true;
    return;
  }

  if (decoded_buffer->IsEndOfStream()) {
    // Set to kStopped so that subsequent read requests will get EOS
    shell_audio_decoder_status_ = kStopped;
    // pass EOS buffer down the chain
    filter_graph_log_->LogEvent(kObjectIdAudioDecoder,
                                kEventEndOfStreamSent);
    base::ResetAndReturn(&read_cb_).Run(kOk, decoded_buffer);
    return;
  }

  message_loop_->PostTask(FROM_HERE, base::Bind(
      &ShellAudioDecoderImpl::RequestBuffer, this));

  DCHECK_EQ(decoded_buffer->GetDataSize(),
            mp4::AAC::kSamplesPerFrame * bits_per_channel() / 8 *
                num_channels_);

#if __SAVE_DECODER_OUTPUT__
  test_probe_.AddData(decoded_buffer->GetData());
#endif

  filter_graph_log_->LogEvent(kObjectIdAudioDecoder, kEventDataDecoded);

  base::ResetAndReturn(&read_cb_).Run(AudioDecoder::kOk, decoded_buffer);

  if (shell_audio_decoder_status_ == kFlushing) {
    // Reset was called, but there were pending reads.
    // Call the Reset callback now
    DCHECK(!reset_cb_.is_null());
    message_loop_->PostTask(FROM_HERE, base::Bind(
        &ShellAudioDecoderImpl::Reset,
        this,
        reset_cb_));
    reset_cb_.Reset();
  }
}

int ShellAudioDecoderImpl::bits_per_channel() {
  return kAudioBytesPerSample * 8;
}

ChannelLayout ShellAudioDecoderImpl::channel_layout() {
  DCHECK(demuxer_stream_);
  const AudioDecoderConfig& config = demuxer_stream_->audio_decoder_config();
  return config.channel_layout();
}

int ShellAudioDecoderImpl::samples_per_second() {
  return samples_per_second_;
}

void ShellAudioDecoderImpl::Reset(const base::Closure& closure) {
  if (!message_loop_->BelongsToCurrentThread()) {
    message_loop_->PostTask(FROM_HERE, base::Bind(
        &ShellAudioDecoderImpl::Reset, this, closure));
    return;
  }

  DCHECK(message_loop_->BelongsToCurrentThread());
  shell_audio_decoder_status_ = kFlushing;
  if (pending_demuxer_read_ || !read_cb_.is_null()) {
    DCHECK(reset_cb_.is_null());
    reset_cb_ = closure;
    return;
  }
  // This should have been reset before calling Reset again
  DCHECK(reset_cb_.is_null());

  filter_graph_log_->LogEvent(kObjectIdAudioDecoder, kEventReset);

#if __SAVE_DECODER_OUTPUT__
  test_probe_.Close();
#endif

  // Release the buffers we queued internally
  queued_buffers_.clear();
  // Sanity-check our assumption that there is no pending decode or in-flight
  // I/O requests
  DCHECK(read_cb_.is_null());
  DCHECK(!pending_renderer_read_);
  DCHECK(!pending_demuxer_read_);
  raw_decoder_->Flush();
  shell_audio_decoder_status_ = kNormal;
  closure.Run();
}

}  // namespace media

