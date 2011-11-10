// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/linux/alsa_input.h"

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/time.h"
#include "media/audio/audio_manager.h"
#include "media/audio/linux/alsa_output.h"
#include "media/audio/linux/alsa_util.h"
#include "media/audio/linux/alsa_wrapper.h"
#include "media/audio/linux/audio_manager_linux.h"

static const int kNumPacketsInRingBuffer = 3;

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
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_factory_(this)),
      read_callback_behind_schedule_(false) {
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

  uint32 latency_us = packet_duration_ms_ * kNumPacketsInRingBuffer *
      base::Time::kMicrosecondsPerMillisecond;

  // Use the same minimum required latency as output.
  latency_us = std::max(latency_us, AlsaPcmOutputStream::kMinLatencyMicros);

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
    // We start reading data half |packet_duration_ms_| later than when the
    // packet might have got filled, to accommodate some delays in the audio
    // driver. This could also give us a smooth read sequence going forward.
    int64 delay_ms = packet_duration_ms_ + packet_duration_ms_ / 2;
    next_read_time_ = base::Time::Now() + base::TimeDelta::FromMilliseconds(
        delay_ms);
    MessageLoop::current()->PostDelayedTask(
        FROM_HERE,
        base::Bind(&AlsaPcmInputStream::ReadAudio, weak_factory_.GetWeakPtr()),
        delay_ms);

    static_cast<AudioManagerLinux*>(AudioManager::GetAudioManager())->
        IncreaseActiveInputStreamCount();
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

snd_pcm_sframes_t AlsaPcmInputStream::GetCurrentDelay() {
  snd_pcm_sframes_t delay = -1;

  int error = wrapper_->PcmDelay(device_handle_, &delay);
  if (error < 0)
    Recover(error);

  // snd_pcm_delay() may not work in the beginning of the stream. In this case
  // return delay of data we know currently is in the ALSA's buffer.
  if (delay < 0)
    delay = wrapper_->PcmAvailUpdate(device_handle_);

  return delay;
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
    // Even Though read callback was behind schedule, there is no data, so
    // reset the next_read_time_.
    if (read_callback_behind_schedule_) {
      next_read_time_ = base::Time::Now();
      read_callback_behind_schedule_ = false;
    }

    uint32 next_check_time = packet_duration_ms_ / 2;
    MessageLoop::current()->PostDelayedTask(
        FROM_HERE,
        base::Bind(&AlsaPcmInputStream::ReadAudio, weak_factory_.GetWeakPtr()),
        next_check_time);
    return;
  }

  int num_packets = frames / params_.samples_per_packet;
  int num_packets_read = num_packets;
  int bytes_per_frame = params_.channels * params_.bits_per_sample / 8;
  uint32 hardware_delay_bytes =
      static_cast<uint32>(GetCurrentDelay() * bytes_per_frame);
  while (num_packets--) {
    int frames_read = wrapper_->PcmReadi(device_handle_, audio_packet_.get(),
                                         params_.samples_per_packet);
    if (frames_read == params_.samples_per_packet) {
      callback_->OnData(this, audio_packet_.get(), bytes_per_packet_,
                        hardware_delay_bytes);
    } else {
      LOG(WARNING) << "PcmReadi returning less than expected frames: "
                   << frames_read << " vs. " << params_.samples_per_packet
                   << ". Dropping this packet.";
    }
  }

  next_read_time_ += base::TimeDelta::FromMilliseconds(
      packet_duration_ms_ * num_packets_read);
  int64 delay_ms = (next_read_time_ - base::Time::Now()).InMilliseconds();
  if (delay_ms < 0) {
    LOG(WARNING) << "Audio read callback behind schedule by "
                 << (packet_duration_ms_ - delay_ms) << " (ms).";
    // Read callback is behind schedule. Assuming there is data pending in
    // the soundcard, invoke the read callback immediate in order to catch up.
    read_callback_behind_schedule_ = true;
    delay_ms = 0;
  }

  MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&AlsaPcmInputStream::ReadAudio, weak_factory_.GetWeakPtr()),
      delay_ms);
}

void AlsaPcmInputStream::Stop() {
  if (!device_handle_ || !callback_)
    return;

  // Stop is always called before Close. In case of error, this will be
  // also called when closing the input controller.
  static_cast<AudioManagerLinux*>(AudioManager::GetAudioManager())->
      DecreaseActiveInputStreamCount();

  weak_factory_.InvalidateWeakPtrs();  // Cancel the next scheduled read.
  int error = wrapper_->PcmDrop(device_handle_);
  if (error < 0)
    HandleError("PcmDrop", error);
}

void AlsaPcmInputStream::Close() {
  scoped_ptr<AlsaPcmInputStream> self_deleter(this);

  // Check in case we were already closed or not initialized yet.
  if (!device_handle_ || !callback_)
    return;

  weak_factory_.InvalidateWeakPtrs();  // Cancel the next scheduled read.
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
