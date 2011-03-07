// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/linux/alsa_input.h"

#include "base/basictypes.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/time.h"
#include "media/audio/linux/alsa_util.h"
#include "media/audio/linux/alsa_wrapper.h"

static const int kNumPacketsInRingBuffer = 3;

// If a read failed with no audio data, try again after this duration.
static const int kNoAudioReadAgainTimeoutMs = 20;

static const char kDefaultDevice1[] = "default";
static const char kDefaultDevice2[] = "plug:default";

const char* AlsaPcmInputStream::kAutoSelectDevice = "";

AlsaPcmInputStream::AlsaPcmInputStream(const std::string& device_name,
                                       const AudioParameters& params,
                                       AlsaWrapper* wrapper)
    : device_name_(device_name),
      params_(params),
      bytes_per_packet_(params.samples_per_packet *
                        (params.channels * params.bits_per_sample) / 8),
      wrapper_(wrapper),
      packet_duration_ms_(
          (params.samples_per_packet * base::Time::kMillisecondsPerSecond) /
          params.sample_rate),
      callback_(NULL),
      device_handle_(NULL),
      ALLOW_THIS_IN_INITIALIZER_LIST(task_factory_(this)) {
}

AlsaPcmInputStream::~AlsaPcmInputStream() {}

bool AlsaPcmInputStream::Open() {
  if (device_handle_)
    return false;  // Already open.

  snd_pcm_format_t pcm_format = alsa_util::BitsToFormat(
      params_.bits_per_sample);
  if (pcm_format == SND_PCM_FORMAT_UNKNOWN) {
    LOG(WARNING) << "Unsupported bits per sample: "
                 << params_.bits_per_sample;
    return false;
  }

  int latency_us = packet_duration_ms_ * kNumPacketsInRingBuffer *
      base::Time::kMicrosecondsPerMillisecond;
  if (device_name_ == kAutoSelectDevice) {
    device_handle_ = alsa_util::OpenCaptureDevice(wrapper_, kDefaultDevice1,
                                                  params_.channels,
                                                  params_.sample_rate,
                                                  pcm_format, latency_us);
    if (!device_handle_) {
      device_handle_ = alsa_util::OpenCaptureDevice(wrapper_, kDefaultDevice2,
                                                    params_.channels,
                                                    params_.sample_rate,
                                                    pcm_format, latency_us);
    }
  } else {
    device_handle_ = alsa_util::OpenCaptureDevice(wrapper_,
                                                  device_name_.c_str(),
                                                  params_.channels,
                                                  params_.sample_rate,
                                                  pcm_format, latency_us);
  }

  if (device_handle_)
    audio_packet_.reset(new uint8[bytes_per_packet_]);

  return device_handle_ != NULL;
}

void AlsaPcmInputStream::Start(AudioInputCallback* callback) {
  DCHECK(!callback_ && callback);
  callback_ = callback;
  int error = wrapper_->PcmPrepare(device_handle_);
  if (error < 0) {
    HandleError("PcmPrepare", error);
  } else {
    error = wrapper_->PcmStart(device_handle_);
    if (error < 0)
      HandleError("PcmStart", error);
  }

  if (error < 0) {
    callback_ = NULL;
  } else {
    // We start reading data a little later than when the packet might have got
    // filled, to accommodate some delays in the audio driver. This could
    // also give us a smooth read sequence going forward.
    int64 delay_ms = packet_duration_ms_ + kNoAudioReadAgainTimeoutMs;
    next_read_time_ = base::Time::Now() + base::TimeDelta::FromMilliseconds(
        delay_ms);
    MessageLoop::current()->PostDelayedTask(
        FROM_HERE,
        task_factory_.NewRunnableMethod(&AlsaPcmInputStream::ReadAudio),
        delay_ms);
  }
}

bool AlsaPcmInputStream::Recover(int original_error) {
  int error = wrapper_->PcmRecover(device_handle_, original_error, 1);
  if (error < 0) {
    // Docs say snd_pcm_recover returns the original error if it is not one
    // of the recoverable ones, so this log message will probably contain the
    // same error twice.
    LOG(WARNING) << "Unable to recover from \""
                 << wrapper_->StrError(original_error) << "\": "
                 << wrapper_->StrError(error);
    return false;
  }

  if (original_error == -EPIPE) {  // Buffer underrun/overrun.
    // For capture streams we have to repeat the explicit start() to get
    // data flowing again.
    error = wrapper_->PcmStart(device_handle_);
    if (error < 0) {
      HandleError("PcmStart", error);
      return false;
    }
  }

  return true;
}

void AlsaPcmInputStream::ReadAudio() {
  DCHECK(callback_);

  snd_pcm_sframes_t frames = wrapper_->PcmAvailUpdate(device_handle_);
  if (frames < 0) {  // Potentially recoverable error?
    LOG(WARNING) << "PcmAvailUpdate(): " << wrapper_->StrError(frames);
    Recover(frames);
  }

  if (frames < params_.samples_per_packet) {
    // Not enough data yet or error happened. In both cases wait for a very
    // small duration before checking again.
    MessageLoop::current()->PostDelayedTask(
        FROM_HERE,
        task_factory_.NewRunnableMethod(&AlsaPcmInputStream::ReadAudio),
        kNoAudioReadAgainTimeoutMs);
    return;
  }

  int num_packets = frames / params_.samples_per_packet;
  while (num_packets--) {
    int frames_read = wrapper_->PcmReadi(device_handle_, audio_packet_.get(),
                                         params_.samples_per_packet);
    if (frames_read == params_.samples_per_packet) {
      callback_->OnData(this, audio_packet_.get(), bytes_per_packet_);
    } else {
      LOG(WARNING) << "PcmReadi returning less than expected frames: "
                   << frames_read << " vs. " << params_.samples_per_packet
                   << ". Dropping this packet.";
    }
  }

  next_read_time_ += base::TimeDelta::FromMilliseconds(packet_duration_ms_);
  int64 delay_ms = (next_read_time_ - base::Time::Now()).InMilliseconds();
  if (delay_ms < 0) {
    LOG(WARNING) << "Audio read callback behind schedule by "
                 << (packet_duration_ms_ - delay_ms) << " (ms).";
    delay_ms = 0;
  }

  MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      task_factory_.NewRunnableMethod(&AlsaPcmInputStream::ReadAudio),
      delay_ms);
}

void AlsaPcmInputStream::Stop() {
  if (!device_handle_ || !callback_)
    return;

  task_factory_.RevokeAll();  // Cancel the next scheduled read.
  int error = wrapper_->PcmDrop(device_handle_);
  if (error < 0)
    HandleError("PcmDrop", error);
}

void AlsaPcmInputStream::Close() {
  scoped_ptr<AlsaPcmInputStream> self_deleter(this);

  // Check in case we were already closed or not initialized yet.
  if (!device_handle_ || !callback_)
    return;

  task_factory_.RevokeAll();  // Cancel the next scheduled read.
  int error = alsa_util::CloseDevice(wrapper_, device_handle_);
  if (error < 0)
    HandleError("PcmClose", error);

  audio_packet_.reset();
  device_handle_ = NULL;
  callback_->OnClose(this);
}

void AlsaPcmInputStream::HandleError(const char* method, int error) {
  LOG(WARNING) << method << ": " << wrapper_->StrError(error);
  callback_->OnError(this, error);
}
