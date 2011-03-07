// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/linux/alsa_util.h"

#include <string>

#include "base/logging.h"
#include "media/audio/linux/alsa_wrapper.h"

namespace alsa_util {

static snd_pcm_t* OpenDevice(AlsaWrapper* wrapper,
                             const char* device_name,
                             snd_pcm_stream_t type,
                             int channels,
                             int sample_rate,
                             snd_pcm_format_t pcm_format,
                             int latency_us) {
  snd_pcm_t* handle = NULL;
  int error = wrapper->PcmOpen(&handle, device_name, type, SND_PCM_NONBLOCK);
  if (error < 0) {
    LOG(WARNING) << "PcmOpen: " << device_name << ","
                 << wrapper->StrError(error);
    return NULL;
  }

  error = wrapper->PcmSetParams(handle, pcm_format,
                                SND_PCM_ACCESS_RW_INTERLEAVED, channels,
                                sample_rate, 1, latency_us);
  if (error < 0) {
    LOG(WARNING) << "PcmSetParams: " << device_name << ", "
                 << wrapper->StrError(error) << " - Format: " << pcm_format
                 << " Channels: " << channels << " Latency: " << latency_us;
    if (!alsa_util::CloseDevice(wrapper, handle)) {
      // TODO(ajwong): Retry on certain errors?
      LOG(WARNING) << "Unable to close audio device. Leaking handle.";
    }
    return NULL;
  }

  return handle;
}

snd_pcm_format_t BitsToFormat(int bits_per_sample) {
  switch (bits_per_sample) {
    case 8:
      return SND_PCM_FORMAT_U8;

    case 16:
      return SND_PCM_FORMAT_S16;

    case 24:
      return SND_PCM_FORMAT_S24;

    case 32:
      return SND_PCM_FORMAT_S32;

    default:
      return SND_PCM_FORMAT_UNKNOWN;
  }
}

int CloseDevice(AlsaWrapper* wrapper, snd_pcm_t* handle) {
  std::string device_name = wrapper->PcmName(handle);
  int error = wrapper->PcmClose(handle);
  if (error < 0) {
    LOG(ERROR) << "PcmClose: " << device_name << ", "
               << wrapper->StrError(error);
  }

  return error;
}

snd_pcm_t* OpenCaptureDevice(AlsaWrapper* wrapper,
                             const char* device_name,
                             int channels,
                             int sample_rate,
                             snd_pcm_format_t pcm_format,
                             int latency_us) {
  return OpenDevice(wrapper, device_name, SND_PCM_STREAM_CAPTURE, channels,
                    sample_rate, pcm_format, latency_us);
}

snd_pcm_t* OpenPlaybackDevice(AlsaWrapper* wrapper,
                              const char* device_name,
                              int channels,
                              int sample_rate,
                              snd_pcm_format_t pcm_format,
                              int latency_us) {
  return OpenDevice(wrapper, device_name, SND_PCM_STREAM_PLAYBACK, channels,
                    sample_rate, pcm_format, latency_us);
}

}  // namespace alsa_util
