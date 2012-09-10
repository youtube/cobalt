// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// The object has one error state: |state_| == kInError.  When |state_| ==
// kInError, all public API functions will fail with an error (Start() will call
// the OnError() function on the callback immediately), or no-op themselves with
// the exception of Close().  Even if an error state has been entered, if Open()
// has previously returned successfully, Close() must be called.

#include "media/audio/linux/cras_output.h"

#include <cras_client.h>

#include "base/logging.h"
#include "media/audio/audio_util.h"
#include "media/audio/linux/alsa_util.h"
#include "media/audio/linux/audio_manager_linux.h"

namespace media {

// Helps make log messages readable.
std::ostream& operator<<(std::ostream& os,
                         CrasOutputStream::InternalState state) {
  switch (state) {
    case CrasOutputStream::kInError:
      os << "kInError";
      break;
    case CrasOutputStream::kCreated:
      os << "kCreated";
      break;
    case CrasOutputStream::kIsOpened:
      os << "kIsOpened";
      break;
    case CrasOutputStream::kIsPlaying:
      os << "kIsPlaying";
      break;
    case CrasOutputStream::kIsStopped:
      os << "kIsStopped";
      break;
    case CrasOutputStream::kIsClosed:
      os << "kIsClosed";
      break;
    default:
      os << "UnknownState";
      break;
  };
  return os;
}

// Overview of operation:
// 1) An object of CrasOutputStream is created by the AudioManager
// factory: audio_man->MakeAudioStream().
// 2) Next some thread will call Open(), at that point a client is created and
// configured for the correct format and sample rate.
// 3) Then Start(source) is called and a stream is added to the CRAS client
// which will create its own thread that periodically calls the source for more
// data as buffers are being consumed.
// 4) When finished Stop() is called, which is handled by stopping the stream.
// 5) Finally Close() is called. It cleans up and notifies the audio manager,
// which likely will destroy this object.

CrasOutputStream::CrasOutputStream(const AudioParameters& params,
                                   AudioManagerLinux* manager)
    : client_(NULL),
      stream_id_(0),
      samples_per_packet_(params.frames_per_buffer()),
      bytes_per_frame_(0),
      frame_rate_(params.sample_rate()),
      num_channels_(params.channels()),
      pcm_format_(alsa_util::BitsToFormat(params.bits_per_sample())),
      state_(kCreated),
      volume_(1.0),
      manager_(manager),
      source_callback_(NULL),
      audio_bus_(AudioBus::Create(params)) {
  // We must have a manager.
  DCHECK(manager_);

  // Sanity check input values.
  if (params.sample_rate() <= 0) {
    LOG(WARNING) << "Unsupported audio frequency.";
    TransitionTo(kInError);
    return;
  }

  if (AudioParameters::AUDIO_PCM_LINEAR != params.format() &&
      AudioParameters::AUDIO_PCM_LOW_LATENCY != params.format()) {
    LOG(WARNING) << "Unsupported audio format.";
    TransitionTo(kInError);
    return;
  }

  if (pcm_format_ == SND_PCM_FORMAT_UNKNOWN) {
    LOG(WARNING) << "Unsupported bits per sample: " << params.bits_per_sample();
    TransitionTo(kInError);
    return;
  }
}

CrasOutputStream::~CrasOutputStream() {
  InternalState current_state = state();
  DCHECK(current_state == kCreated ||
         current_state == kIsClosed ||
         current_state == kInError);
}

bool CrasOutputStream::Open() {
  if (!CanTransitionTo(kIsOpened)) {
    NOTREACHED() << "Invalid state: " << state();
    return false;
  }

  // We do not need to check if the transition was successful because
  // CanTransitionTo() was checked above, and it is assumed that this
  // object's public API is only called on one thread so the state cannot
  // transition out from under us.
  TransitionTo(kIsOpened);

  // Create the client and connect to the CRAS server.
  int err = cras_client_create(&client_);
  if (err < 0) {
    LOG(WARNING) << "Couldn't create CRAS client.\n";
    client_ = NULL;
    TransitionTo(kInError);
    return false;
  }
  err = cras_client_connect(client_);
  if (err) {
    LOG(WARNING) << "Couldn't connect CRAS client.\n";
    cras_client_destroy(client_);
    client_ = NULL;
    TransitionTo(kInError);
    return false;
  }
  // Then start running the client.
  err = cras_client_run_thread(client_);
  if (err) {
    LOG(WARNING) << "Couldn't run CRAS client.\n";
    cras_client_destroy(client_);
    client_ = NULL;
    TransitionTo(kInError);
    return false;
  }

  return true;
}

void CrasOutputStream::Close() {
  // Sanity Check that we can transition to closed.
  if (TransitionTo(kIsClosed) != kIsClosed) {
    NOTREACHED() << "Unable to transition Closed.";
    return;
  }

  if (client_) {
    cras_client_stop(client_);
    cras_client_destroy(client_);
    client_ = NULL;
  }

  // Signal to the manager that we're closed and can be removed.
  // Should be last call in the method as it deletes "this".
  manager_->ReleaseOutputStream(this);
}

void CrasOutputStream::Start(AudioSourceCallback* callback) {
  CHECK(callback);
  source_callback_ = callback;

  // Only start if we can enter the playing state.
  if (TransitionTo(kIsPlaying) != kIsPlaying)
    return;

  // Prepare |audio_format| and |stream_params| for the stream we
  // will create.
  cras_audio_format* audio_format = cras_audio_format_create(
      pcm_format_,
      frame_rate_,
      num_channels_);
  if (audio_format == NULL) {
    LOG(WARNING) << "Error setting up audio parameters.";
    TransitionTo(kInError);
    callback->OnError(this, -ENOMEM);
    return;
  }
  cras_stream_params* stream_params = cras_client_stream_params_create(
     CRAS_STREAM_OUTPUT,
     samples_per_packet_ * 2,  // Total latency.
     samples_per_packet_ / 2,  // Call back when this many left.
     samples_per_packet_,  // Call back with at least this much space.
     CRAS_STREAM_TYPE_DEFAULT,
     0,
     this,
     CrasOutputStream::PutSamples,
     CrasOutputStream::StreamError,
     audio_format);
  if (stream_params == NULL) {
    LOG(WARNING) << "Error setting up stream parameters.";
    TransitionTo(kInError);
    callback->OnError(this, -ENOMEM);
    cras_audio_format_destroy(audio_format);
    return;
  }

  // Before starting the stream, save the number of bytes in a frame for use in
  // the callback.
  bytes_per_frame_ = cras_client_format_bytes_per_frame(audio_format);

  // Adding the stream will start the audio callbacks requesting data.
  int err = cras_client_add_stream(client_, &stream_id_, stream_params);
  if (err < 0) {
    LOG(WARNING) << "Failed to add the stream";
    TransitionTo(kInError);
    callback->OnError(this, err);
    cras_audio_format_destroy(audio_format);
    cras_client_stream_params_destroy(stream_params);
    return;
  }

  // Set initial volume.
  cras_client_set_stream_volume(client_, stream_id_, volume_);

  // Done with config params.
  cras_audio_format_destroy(audio_format);
  cras_client_stream_params_destroy(stream_params);
}

void CrasOutputStream::Stop() {
  if (!client_)
    return;
  // Removing the stream from the client stops audio.
  cras_client_rm_stream(client_, stream_id_);
  TransitionTo(kIsStopped);
}

void CrasOutputStream::SetVolume(double volume) {
  if (!client_)
    return;
  volume_ = static_cast<float>(volume);
  cras_client_set_stream_volume(client_, stream_id_, volume_);
}

void CrasOutputStream::GetVolume(double* volume) {
  *volume = volume_;
}

// Static callback asking for samples.
int CrasOutputStream::PutSamples(cras_client* client,
                                 cras_stream_id_t stream_id,
                                 uint8* samples,
                                 size_t frames,
                                 const timespec* sample_ts,
                                 void* arg) {
  CrasOutputStream* me = static_cast<CrasOutputStream*>(arg);
  return me->Render(frames, samples, sample_ts);
}

// Static callback for stream errors.
int CrasOutputStream::StreamError(cras_client* client,
                                  cras_stream_id_t stream_id,
                                  int err,
                                  void* arg) {
  CrasOutputStream* me = static_cast<CrasOutputStream*>(arg);
  me->NotifyStreamError(err);
  return 0;
}

// Note this is run from a real time thread, so don't waste cycles here.
uint32 CrasOutputStream::Render(size_t frames,
                                uint8* buffer,
                                const timespec* sample_ts) {
  timespec latency_ts  = {0, 0};

  // Determine latency and pass that on to the source.
  cras_client_calc_playback_latency(sample_ts, &latency_ts);
  uint32 latency_usec = (latency_ts.tv_sec * 1000000) +
      latency_ts.tv_nsec / 1000;

  uint32 frames_latency = latency_usec * frame_rate_ / 1000000;
  uint32 bytes_latency = frames_latency * bytes_per_frame_;
  DCHECK_EQ(frames, static_cast<size_t>(audio_bus_->frames()));
  int frames_filled = source_callback_->OnMoreData(
      audio_bus_.get(), AudioBuffersState(0, bytes_latency));
  // Note: If this ever changes to output raw float the data must be clipped and
  // sanitized since it may come from an untrusted source such as NaCl.
  audio_bus_->ToInterleaved(
      frames_filled, bytes_per_frame_ / num_channels_, buffer);
  return frames_filled;
}

void CrasOutputStream::NotifyStreamError(int err) {
  // This will remove the stream from the client.
  if (state_ == kIsClosed || state_ == kInError)
    return;  // Don't care about error if we aren't using it.
  TransitionTo(kInError);
  if (source_callback_)
    source_callback_->OnError(this, err);
}

bool CrasOutputStream::CanTransitionTo(InternalState to) {
  switch (state_) {
    case kCreated:
      return to == kIsOpened || to == kIsClosed || to == kInError;

    case kIsOpened:
      return to == kIsPlaying || to == kIsStopped ||
          to == kIsClosed || to == kInError;

    case kIsPlaying:
      return to == kIsPlaying || to == kIsStopped ||
          to == kIsClosed || to == kInError;

    case kIsStopped:
      return to == kIsPlaying || to == kIsStopped ||
          to == kIsClosed || to == kInError;

    case kInError:
      return to == kIsClosed || to == kInError;

    case kIsClosed:
      return false;
  }
  return false;
}

CrasOutputStream::InternalState
CrasOutputStream::TransitionTo(InternalState to) {
  if (!CanTransitionTo(to)) {
    state_ = kInError;
  } else {
    state_ = to;
  }
  return state_;
}

CrasOutputStream::InternalState CrasOutputStream::state() {
  return state_;
}

}  // namespace media
