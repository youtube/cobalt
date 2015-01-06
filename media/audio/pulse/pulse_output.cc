// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/pulse/pulse_output.h"

#include <pulse/pulseaudio.h>

#include "base/message_loop.h"
#include "media/audio/audio_manager_base.h"
#include "media/audio/audio_parameters.h"
#include "media/audio/audio_util.h"

namespace media {

// A helper class that acquires pa_threaded_mainloop_lock() while in scope.
class AutoPulseLock {
 public:
  explicit AutoPulseLock(pa_threaded_mainloop* pa_mainloop)
      : pa_mainloop_(pa_mainloop) {
    pa_threaded_mainloop_lock(pa_mainloop_);
  }

  ~AutoPulseLock() {
    pa_threaded_mainloop_unlock(pa_mainloop_);
  }

 private:
  pa_threaded_mainloop* pa_mainloop_;

  DISALLOW_COPY_AND_ASSIGN(AutoPulseLock);
};

static pa_sample_format_t BitsToPASampleFormat(int bits_per_sample) {
  switch (bits_per_sample) {
    case 8:
      return PA_SAMPLE_U8;
    case 16:
      return PA_SAMPLE_S16LE;
    case 24:
      return PA_SAMPLE_S24LE;
    case 32:
      return PA_SAMPLE_S32LE;
    default:
      NOTREACHED() << "Invalid bits per sample: " << bits_per_sample;
      return PA_SAMPLE_INVALID;
  }
}

static pa_channel_position ChromiumToPAChannelPosition(Channels channel) {
  switch (channel) {
    // PulseAudio does not differentiate between left/right and
    // stereo-left/stereo-right, both translate to front-left/front-right.
    case LEFT:
      return PA_CHANNEL_POSITION_FRONT_LEFT;
    case RIGHT:
      return PA_CHANNEL_POSITION_FRONT_RIGHT;
    case CENTER:
      return PA_CHANNEL_POSITION_FRONT_CENTER;
    case LFE:
      return PA_CHANNEL_POSITION_LFE;
    case BACK_LEFT:
      return PA_CHANNEL_POSITION_REAR_LEFT;
    case BACK_RIGHT:
      return PA_CHANNEL_POSITION_REAR_RIGHT;
    case LEFT_OF_CENTER:
      return PA_CHANNEL_POSITION_FRONT_LEFT_OF_CENTER;
    case RIGHT_OF_CENTER:
      return PA_CHANNEL_POSITION_FRONT_RIGHT_OF_CENTER;
    case BACK_CENTER:
      return PA_CHANNEL_POSITION_REAR_CENTER;
    case SIDE_LEFT:
      return PA_CHANNEL_POSITION_SIDE_LEFT;
    case SIDE_RIGHT:
      return PA_CHANNEL_POSITION_SIDE_RIGHT;
    case CHANNELS_MAX:
      return PA_CHANNEL_POSITION_INVALID;
    default:
      NOTREACHED() << "Invalid channel: " << channel;
      return PA_CHANNEL_POSITION_INVALID;
  }
}

static pa_channel_map ChannelLayoutToPAChannelMap(
    ChannelLayout channel_layout) {
  pa_channel_map channel_map;
  pa_channel_map_init(&channel_map);

  channel_map.channels = ChannelLayoutToChannelCount(channel_layout);
  for (Channels ch = LEFT; ch < CHANNELS_MAX;
       ch = static_cast<Channels>(ch + 1)) {
    int channel_index = ChannelOrder(channel_layout, ch);
    if (channel_index < 0)
      continue;

    channel_map.map[channel_index] = ChromiumToPAChannelPosition(ch);
  }

  return channel_map;
}

// static, pa_context_notify_cb
void PulseAudioOutputStream::ContextNotifyCallback(pa_context* c,
                                                   void* p_this) {
  PulseAudioOutputStream* stream = static_cast<PulseAudioOutputStream*>(p_this);

  // Forward unexpected failures to the AudioSourceCallback if available.  All
  // these variables are only modified under pa_threaded_mainloop_lock() so this
  // should be thread safe.
  if (c && stream->source_callback_ &&
      pa_context_get_state(c) == PA_CONTEXT_FAILED) {
    stream->source_callback_->OnError(stream, pa_context_errno(c));
  }

  pa_threaded_mainloop_signal(stream->pa_mainloop_, 0);
}

// static, pa_stream_notify_cb
void PulseAudioOutputStream::StreamNotifyCallback(pa_stream* s, void* p_this) {
  PulseAudioOutputStream* stream = static_cast<PulseAudioOutputStream*>(p_this);

  // Forward unexpected failures to the AudioSourceCallback if available.  All
  // these variables are only modified under pa_threaded_mainloop_lock() so this
  // should be thread safe.
  if (s && stream->source_callback_ &&
      pa_stream_get_state(s) == PA_STREAM_FAILED) {
    stream->source_callback_->OnError(
        stream, pa_context_errno(stream->pa_context_));
  }

  pa_threaded_mainloop_signal(stream->pa_mainloop_, 0);
}

// static, pa_stream_success_cb_t
void PulseAudioOutputStream::StreamSuccessCallback(pa_stream* s, int success,
                                                   void* p_this) {
  PulseAudioOutputStream* stream = static_cast<PulseAudioOutputStream*>(p_this);
  pa_threaded_mainloop_signal(stream->pa_mainloop_, 0);
}

// static, pa_stream_request_cb_t
void PulseAudioOutputStream::StreamRequestCallback(pa_stream* s, size_t len,
                                                   void* p_this) {
  // Fulfill write request; must always result in a pa_stream_write() call.
  static_cast<PulseAudioOutputStream*>(p_this)->FulfillWriteRequest(len);
}

PulseAudioOutputStream::PulseAudioOutputStream(const AudioParameters& params,
                                               AudioManagerBase* manager)
    : params_(params),
      manager_(manager),
      pa_context_(NULL),
      pa_mainloop_(NULL),
      pa_stream_(NULL),
      volume_(1.0f),
      source_callback_(NULL) {
  DCHECK(manager_->GetMessageLoop()->BelongsToCurrentThread());

  CHECK(params_.IsValid());
  audio_bus_ = AudioBus::Create(params_);
}

PulseAudioOutputStream::~PulseAudioOutputStream() {
  // All internal structures should already have been freed in Close(), which
  // calls AudioManagerBase::ReleaseOutputStream() which deletes this object.
  DCHECK(!pa_stream_);
  DCHECK(!pa_context_);
  DCHECK(!pa_mainloop_);
}

// Helper macro for Open() to avoid code spam and string bloat.
#define RETURN_ON_FAILURE(expression, message) do { \
  if (!(expression)) { \
    if (pa_context_) { \
      DLOG(ERROR) << message << "  Error: " \
                  << pa_strerror(pa_context_errno(pa_context_)); \
    } else { \
      DLOG(ERROR) << message; \
    } \
    return false; \
  } \
} while(0)

bool PulseAudioOutputStream::Open() {
  DCHECK(manager_->GetMessageLoop()->BelongsToCurrentThread());

  pa_mainloop_ = pa_threaded_mainloop_new();
  RETURN_ON_FAILURE(pa_mainloop_, "Failed to create PulseAudio main loop.");

  pa_mainloop_api* pa_mainloop_api = pa_threaded_mainloop_get_api(pa_mainloop_);
  pa_context_ = pa_context_new(pa_mainloop_api, "Chromium");
  RETURN_ON_FAILURE(pa_context_, "Failed to create PulseAudio context.");

  // A state callback must be set before calling pa_threaded_mainloop_lock() or
  // pa_threaded_mainloop_wait() calls may lead to dead lock.
  pa_context_set_state_callback(pa_context_, &ContextNotifyCallback, this);

  // Lock the main loop while setting up the context.  Failure to do so may lead
  // to crashes as the PulseAudio thread tries to run before things are ready.
  AutoPulseLock auto_lock(pa_mainloop_);

  RETURN_ON_FAILURE(
      pa_threaded_mainloop_start(pa_mainloop_) == 0,
      "Failed to start PulseAudio main loop.");
  RETURN_ON_FAILURE(
      pa_context_connect(pa_context_, NULL, PA_CONTEXT_NOAUTOSPAWN, NULL) == 0,
      "Failed to connect PulseAudio context.");

  // Wait until |pa_context_| is ready.  pa_threaded_mainloop_wait() must be
  // called after pa_context_get_state() in case the context is already ready,
  // otherwise pa_threaded_mainloop_wait() will hang indefinitely.
  while (true) {
    pa_context_state_t context_state = pa_context_get_state(pa_context_);
    RETURN_ON_FAILURE(
        PA_CONTEXT_IS_GOOD(context_state), "Invalid PulseAudio context state.");
    if (context_state == PA_CONTEXT_READY)
      break;
    pa_threaded_mainloop_wait(pa_mainloop_);
  }

  // Set sample specifications.
  pa_sample_spec pa_sample_specifications;
  pa_sample_specifications.format = BitsToPASampleFormat(
      params_.bits_per_sample());
  pa_sample_specifications.rate = params_.sample_rate();
  pa_sample_specifications.channels = params_.channels();

  // Get channel mapping and open playback stream.
  pa_channel_map* map = NULL;
  pa_channel_map source_channel_map = ChannelLayoutToPAChannelMap(
      params_.channel_layout());
  if (source_channel_map.channels != 0) {
    // The source data uses a supported channel map so we will use it rather
    // than the default channel map (NULL).
    map = &source_channel_map;
  }
  pa_stream_ = pa_stream_new(
      pa_context_, "Playback", &pa_sample_specifications, map);
  RETURN_ON_FAILURE(pa_stream_, "Failed to create PulseAudio stream.");
  pa_stream_set_state_callback(pa_stream_, &StreamNotifyCallback, this);

  // Even though we start the stream corked below, PulseAudio will issue one
  // stream request after setup.  FulfillWriteRequest() must fulfill the write.
  pa_stream_set_write_callback(pa_stream_, &StreamRequestCallback, this);

  // Tell pulse audio we only want callbacks of a certain size.
  pa_buffer_attr pa_buffer_attributes;
  pa_buffer_attributes.maxlength = params_.GetBytesPerBuffer();
  pa_buffer_attributes.minreq = params_.GetBytesPerBuffer();
  pa_buffer_attributes.prebuf = params_.GetBytesPerBuffer();
  pa_buffer_attributes.tlength = params_.GetBytesPerBuffer();
  pa_buffer_attributes.fragsize = static_cast<uint32_t>(-1);

  // Connect playback stream.
  // TODO(dalecurtis): Pulse tends to want really large buffer sizes if we are
  // not using the native sample rate.  We should always open the stream with
  // PA_STREAM_FIX_RATE and ensure this is true.
  RETURN_ON_FAILURE(
      pa_stream_connect_playback(
          pa_stream_, NULL, &pa_buffer_attributes,
          static_cast<pa_stream_flags_t>(
              PA_STREAM_ADJUST_LATENCY | PA_STREAM_AUTO_TIMING_UPDATE |
              PA_STREAM_NOT_MONOTONIC | PA_STREAM_START_CORKED),
          NULL, NULL) == 0,
      "Failed to connect PulseAudio stream.");

  // Wait for the stream to be ready.
  while (true) {
    pa_stream_state_t stream_state = pa_stream_get_state(pa_stream_);
    RETURN_ON_FAILURE(
        PA_STREAM_IS_GOOD(stream_state), "Invalid PulseAudio stream state.");
    if (stream_state == PA_STREAM_READY)
      break;
    pa_threaded_mainloop_wait(pa_mainloop_);
  }

  return true;
}

#undef RETURN_ON_FAILURE

void PulseAudioOutputStream::Reset() {
  if (!pa_mainloop_) {
    DCHECK(!pa_stream_);
    DCHECK(!pa_context_);
    return;
  }

  {
    AutoPulseLock auto_lock(pa_mainloop_);

    // Close the stream.
    if (pa_stream_) {
      // Ensure all samples are played out before shutdown.
      WaitForPulseOperation(pa_stream_flush(
          pa_stream_, &StreamSuccessCallback, this));

      // Release PulseAudio structures.
      pa_stream_disconnect(pa_stream_);
      pa_stream_set_write_callback(pa_stream_, NULL, NULL);
      pa_stream_set_state_callback(pa_stream_, NULL, NULL);
      pa_stream_unref(pa_stream_);
      pa_stream_ = NULL;
    }

    if (pa_context_) {
      pa_context_disconnect(pa_context_);
      pa_context_set_state_callback(pa_context_, NULL, NULL);
      pa_context_unref(pa_context_);
      pa_context_ = NULL;
    }
  }

  pa_threaded_mainloop_stop(pa_mainloop_);
  pa_threaded_mainloop_free(pa_mainloop_);
  pa_mainloop_ = NULL;
}

void PulseAudioOutputStream::Close() {
  DCHECK(manager_->GetMessageLoop()->BelongsToCurrentThread());

  Reset();

  // Signal to the manager that we're closed and can be removed.
  // This should be the last call in the function as it deletes "this".
  manager_->ReleaseOutputStream(this);
}

int PulseAudioOutputStream::GetHardwareLatencyInBytes() {
  int negative = 0;
  pa_usec_t pa_latency_micros = 0;
  if (pa_stream_get_latency(pa_stream_, &pa_latency_micros, &negative) != 0)
    return 0;

  if (negative)
    return 0;

  return (pa_latency_micros * params_.sample_rate() *
          params_.GetBytesPerFrame()) / base::Time::kMicrosecondsPerSecond;
}

void PulseAudioOutputStream::FulfillWriteRequest(size_t requested_bytes) {
  CHECK_EQ(requested_bytes, static_cast<size_t>(params_.GetBytesPerBuffer()));

  int frames_filled = 0;
  if (source_callback_) {
    frames_filled = source_callback_->OnMoreData(
        audio_bus_.get(), AudioBuffersState(0, GetHardwareLatencyInBytes()));
  }

  // Zero any unfilled data so it plays back as silence.
  if (frames_filled < audio_bus_->frames()) {
    audio_bus_->ZeroFramesPartial(
        frames_filled, audio_bus_->frames() - frames_filled);
  }

  // PulseAudio won't always be able to provide a buffer large enough, so we may
  // need to request multiple buffers and fill them individually.
  int current_frame = 0;
  size_t bytes_remaining = requested_bytes;
  while (bytes_remaining > 0) {
    void* buffer = NULL;
    size_t bytes_to_fill = bytes_remaining;
    CHECK_GE(pa_stream_begin_write(pa_stream_, &buffer, &bytes_to_fill), 0);

    // In case PulseAudio gives us a bigger buffer than we want, cap our size.
    bytes_to_fill = std::min(
        std::min(bytes_remaining, bytes_to_fill),
        static_cast<size_t>(params_.GetBytesPerBuffer()));

    int frames_to_fill = bytes_to_fill / params_.GetBytesPerFrame();;

    // Note: If this ever changes to output raw float the data must be clipped
    // and sanitized since it may come from an untrusted source such as NaCl.
    audio_bus_->ToInterleavedPartial(
        current_frame, frames_to_fill, params_.bits_per_sample() / 8, buffer);
    media::AdjustVolume(buffer, bytes_to_fill, params_.channels(),
                        params_.bits_per_sample() / 8, volume_);

    if (pa_stream_write(pa_stream_, buffer, bytes_to_fill, NULL, 0LL,
                        PA_SEEK_RELATIVE) < 0) {
      if (source_callback_) {
        source_callback_->OnError(this, pa_context_errno(pa_context_));
      }
    }

    bytes_remaining -= bytes_to_fill;
    current_frame = frames_to_fill;
  }
}

void PulseAudioOutputStream::Start(AudioSourceCallback* callback) {
  DCHECK(manager_->GetMessageLoop()->BelongsToCurrentThread());
  CHECK(callback);
  CHECK(pa_stream_);

  AutoPulseLock auto_lock(pa_mainloop_);

  // Ensure the context and stream are ready.
  if (pa_context_get_state(pa_context_) != PA_CONTEXT_READY &&
      pa_stream_get_state(pa_stream_) != PA_STREAM_READY) {
    callback->OnError(this, pa_context_errno(pa_context_));
    return;
  }

  source_callback_ = callback;

  // Uncork (resume) the stream.
  WaitForPulseOperation(pa_stream_cork(
      pa_stream_, 0, &StreamSuccessCallback, this));
}

void PulseAudioOutputStream::Stop() {
  DCHECK(manager_->GetMessageLoop()->BelongsToCurrentThread());

  // Cork (pause) the stream.  Waiting for the main loop lock will ensure
  // outstanding callbacks have completed.
  AutoPulseLock auto_lock(pa_mainloop_);

  // Flush the stream prior to cork, doing so after will cause hangs.  Write
  // callbacks are suspended while inside pa_threaded_mainloop_lock() so this
  // is all thread safe.
  WaitForPulseOperation(pa_stream_flush(
      pa_stream_, &StreamSuccessCallback, this));

  WaitForPulseOperation(pa_stream_cork(
      pa_stream_, 1, &StreamSuccessCallback, this));

  source_callback_ = NULL;
}

void PulseAudioOutputStream::SetVolume(double volume) {
  DCHECK(manager_->GetMessageLoop()->BelongsToCurrentThread());

  volume_ = static_cast<float>(volume);
}

void PulseAudioOutputStream::GetVolume(double* volume) {
  DCHECK(manager_->GetMessageLoop()->BelongsToCurrentThread());

  *volume = volume_;
}

void PulseAudioOutputStream::WaitForPulseOperation(pa_operation* op) {
  CHECK(op);
  while (pa_operation_get_state(op) == PA_OPERATION_RUNNING) {
    pa_threaded_mainloop_wait(pa_mainloop_);
  }
  pa_operation_unref(op);
}

}  // namespace media
