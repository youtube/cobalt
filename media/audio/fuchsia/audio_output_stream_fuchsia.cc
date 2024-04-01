// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/fuchsia/audio_output_stream_fuchsia.h"

#include <lib/sys/cpp/component_context.h>
#include <zircon/syscalls.h>

#include "base/bind.h"
#include "base/fuchsia/fuchsia_logging.h"
#include "base/fuchsia/process_context.h"
#include "base/logging.h"
#include "base/memory/writable_shared_memory_region.h"
#include "media/audio/fuchsia/audio_manager_fuchsia.h"
#include "media/base/audio_sample_types.h"
#include "media/base/audio_timestamp_helper.h"

namespace media {

namespace {

// Current AudioRenderer implementation allows only one buffer with id=0.
// TODO(crbug.com/1131179): Replace with an incrementing buffer id now that
// AddPayloadBuffer() and RemovePayloadBuffer() are implemented properly in
// AudioRenderer.
const uint32_t kBufferId = 0;

fuchsia::media::AudioRenderUsage GetStreamUsage(
    const AudioParameters& parameters) {
  if (parameters.latency_tag() == AudioLatency::LATENCY_RTC)
    return fuchsia::media::AudioRenderUsage::COMMUNICATION;
  return fuchsia::media::AudioRenderUsage::MEDIA;
}

}  // namespace

AudioOutputStreamFuchsia::AudioOutputStreamFuchsia(
    AudioManagerFuchsia* manager,
    const AudioParameters& parameters)
    : manager_(manager),
      parameters_(parameters),
      audio_bus_(AudioBus::Create(parameters)) {}

AudioOutputStreamFuchsia::~AudioOutputStreamFuchsia() {
  // Close() must be called first.
  DCHECK(!audio_renderer_);
}

bool AudioOutputStreamFuchsia::Open() {
  DCHECK(!audio_renderer_);

  // Connect |audio_renderer_| to the audio service.
  fuchsia::media::AudioPtr audio_server =
      base::ComponentContextForProcess()
          ->svc()
          ->Connect<fuchsia::media::Audio>();
  audio_server->CreateAudioRenderer(audio_renderer_.NewRequest());
  audio_renderer_.set_error_handler(
      fit::bind_member(this, &AudioOutputStreamFuchsia::OnRendererError));

  audio_renderer_->SetUsage(GetStreamUsage(parameters_));

  // Inform the |audio_renderer_| of the format required by the caller.
  fuchsia::media::AudioStreamType format;
  format.sample_format = fuchsia::media::AudioSampleFormat::FLOAT;
  format.channels = parameters_.channels();
  format.frames_per_second = parameters_.sample_rate();
  audio_renderer_->SetPcmStreamType(std::move(format));

  // Use number of samples to specify media position.
  audio_renderer_->SetPtsUnits(parameters_.sample_rate(), 1);

  // Setup OnMinLeadTimeChanged event listener. This event is used to get
  // |min_lead_time_|, which indicates how far ahead audio samples need to be
  // sent to the renderer.
  audio_renderer_.events().OnMinLeadTimeChanged =
      fit::bind_member(this, &AudioOutputStreamFuchsia::OnMinLeadTimeChanged);
  audio_renderer_->EnableMinLeadTimeEvents(true);

  // The renderer may fail initialization asynchronously, which is handled in
  // OnRendererError().
  return true;
}

void AudioOutputStreamFuchsia::Start(AudioSourceCallback* callback) {
  DCHECK(!callback_);
  DCHECK(reference_time_.is_null());
  DCHECK(!timer_.IsRunning());
  callback_ = callback;

  // Delay PumpSamples() until OnMinLeadTimeChanged is received and Pause() is
  // not pending.
  if (!min_lead_time_.has_value() || pause_pending_)
    return;

  PumpSamples();
}

void AudioOutputStreamFuchsia::Stop() {
  callback_ = nullptr;
  timer_.Stop();

  // Nothing to do if playback is not started or being stopped.
  if (reference_time_.is_null() || pause_pending_)
    return;

  reference_time_ = base::TimeTicks();
  pause_pending_ = true;
  audio_renderer_->Pause(
      fit::bind_member(this, &AudioOutputStreamFuchsia::OnPauseComplete));
  audio_renderer_->DiscardAllPacketsNoReply();
}

// This stream is always used with sub second buffer sizes, where it's
// sufficient to simply always flush upon Start().
void AudioOutputStreamFuchsia::Flush() {}

void AudioOutputStreamFuchsia::SetVolume(double volume) {
  DCHECK(0.0 <= volume && volume <= 1.0) << volume;
  volume_ = volume;
}

void AudioOutputStreamFuchsia::GetVolume(double* volume) {
  *volume = volume_;
}

void AudioOutputStreamFuchsia::Close() {
  Stop();
  audio_renderer_.Unbind();

  // Signal to the manager that we're closed and can be removed. This should be
  // the last call in the function as it deletes |this|.
  manager_->ReleaseOutputStream(this);
}

base::TimeTicks AudioOutputStreamFuchsia::GetCurrentStreamTime() {
  DCHECK(!reference_time_.is_null());
  return reference_time_ +
         AudioTimestampHelper::FramesToTime(stream_position_samples_,
                                            parameters_.sample_rate());
}

size_t AudioOutputStreamFuchsia::GetMinBufferSize() {
  // Ensure that |payload_buffer_| fits enough packets to cover min_lead_time_
  // plus one extra packet.
  int min_packets = (AudioTimestampHelper::TimeToFrames(
                         min_lead_time_.value(), parameters_.sample_rate()) +
                     parameters_.frames_per_buffer() - 1) /
                        parameters_.frames_per_buffer() +
                    1;

  return parameters_.GetBytesPerBuffer(kSampleFormatF32) * min_packets;
}

bool AudioOutputStreamFuchsia::InitializePayloadBuffer() {
  size_t buffer_size = GetMinBufferSize();
  auto region = base::WritableSharedMemoryRegion::Create(buffer_size);
  payload_buffer_ = region.Map();
  if (!payload_buffer_.IsValid()) {
    LOG(WARNING) << "Failed to allocate VMO of size " << buffer_size;
    return false;
  }

  payload_buffer_pos_ = 0;
  audio_renderer_->AddPayloadBuffer(
      kBufferId, base::WritableSharedMemoryRegion::TakeHandleForSerialization(
                     std::move(region))
                     .PassPlatformHandle());

  return true;
}

void AudioOutputStreamFuchsia::OnMinLeadTimeChanged(int64_t min_lead_time) {
  bool min_lead_time_was_unknown = !min_lead_time_.has_value();

  min_lead_time_ = base::Nanoseconds(min_lead_time);

  // When min_lead_time_ increases we may need to reallocate |payload_buffer_|.
  // Code below just unmaps the current buffer. The new buffer will be allocated
  // lated in PumpSamples(). This is necessary because VMO allocation may fail
  // and it's not possible to report that error here - OnMinLeadTimeChanged()
  // may be invoked before Start().
  if (payload_buffer_.IsValid() &&
      GetMinBufferSize() > payload_buffer_.size()) {
    payload_buffer_ = {};

    // Discard all packets currently in flight. This is required because
    // AddPayloadBuffer() will fail if there are any packets in flight.
    audio_renderer_->DiscardAllPacketsNoReply();
  }

  // If playback was started but we were waiting for MinLeadTime, then start
  // pumping samples now.
  if (is_started() && min_lead_time_was_unknown) {
    DCHECK(!timer_.IsRunning());
    PumpSamples();
  }
}

void AudioOutputStreamFuchsia::OnRendererError(zx_status_t status) {
  ZX_LOG(WARNING, status) << "AudioRenderer has failed";
  ReportError();
}

void AudioOutputStreamFuchsia::ReportError() {
  reference_time_ = base::TimeTicks();
  timer_.Stop();
  if (callback_)
    callback_->OnError(AudioSourceCallback::ErrorType::kUnknown);
}

void AudioOutputStreamFuchsia::OnPauseComplete(int64_t reference_time,
                                               int64_t media_time) {
  DCHECK(pause_pending_);
  pause_pending_ = false;

  // If the stream was restarted while Pause() was pending then we can start
  // pumping samples again.
  if (is_started())
    PumpSamples();
}

void AudioOutputStreamFuchsia::PumpSamples() {
  DCHECK(is_started());
  DCHECK(audio_renderer_);

  // Allocate payload buffer if necessary.
  if (!payload_buffer_.IsValid() && !InitializePayloadBuffer()) {
    ReportError();
    return;
  }

  base::TimeTicks now = base::TimeTicks::Now();

  base::TimeDelta delay;
  if (reference_time_.is_null()) {
    delay = min_lead_time_.value() + parameters_.GetBufferDuration() / 2;
    stream_position_samples_ = 0;
  } else {
    auto stream_time = GetCurrentStreamTime();

    // Adjust stream position if we missed timer deadline.
    if (now + min_lead_time_.value() > stream_time) {
      stream_position_samples_ += AudioTimestampHelper::TimeToFrames(
          now + min_lead_time_.value() - stream_time,
          parameters_.sample_rate());
    }

    delay = stream_time - now;
  }

  // Request more samples from |callback_|.
  int frames_filled = callback_->OnMoreData(delay, now, 0, audio_bus_.get());
  DCHECK_EQ(frames_filled, audio_bus_->frames());

  audio_bus_->Scale(volume_);

  // Save samples to the |payload_buffer_|.
  size_t packet_size = parameters_.GetBytesPerBuffer(kSampleFormatF32);
  DCHECK_LE(payload_buffer_pos_ + packet_size, payload_buffer_.size());

  // We skip clipping since that occurs at the shared memory boundary.
  audio_bus_->ToInterleaved<Float32SampleTypeTraitsNoClip>(
      audio_bus_->frames(),
      reinterpret_cast<float*>(static_cast<uint8_t*>(payload_buffer_.memory()) +
                               payload_buffer_pos_));

  // Send a new packet.
  fuchsia::media::StreamPacket packet;
  packet.pts = stream_position_samples_;
  packet.payload_buffer_id = kBufferId;
  packet.payload_offset = payload_buffer_pos_;
  packet.payload_size = packet_size;
  packet.flags = 0;
  audio_renderer_->SendPacketNoReply(std::move(packet));

  // Start playback if the stream was previously stopped.
  if (reference_time_.is_null()) {
    reference_time_ = now + delay;
    audio_renderer_->PlayNoReply(reference_time_.ToZxTime(),
                                 stream_position_samples_);
  }

  stream_position_samples_ += frames_filled;
  payload_buffer_pos_ =
      (payload_buffer_pos_ + packet_size) % payload_buffer_.size();

  SchedulePumpSamples(now);
}

void AudioOutputStreamFuchsia::SchedulePumpSamples(base::TimeTicks now) {
  base::TimeTicks next_pump_time = GetCurrentStreamTime() -
                                   min_lead_time_.value() -
                                   parameters_.GetBufferDuration() / 2;
  timer_.Start(FROM_HERE, next_pump_time - now,
               base::BindOnce(&AudioOutputStreamFuchsia::PumpSamples,
                              base::Unretained(this)));
}

}  // namespace media
