// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/linux/alsa_wrapper.h"

#include <alsa/asoundlib.h>

AlsaWrapper::AlsaWrapper() {
}

AlsaWrapper::~AlsaWrapper() {
}

int AlsaWrapper::PcmOpen(snd_pcm_t** handle, const char* name,
                         snd_pcm_stream_t stream, int mode) {
  return snd_pcm_open(handle, name, stream, mode);
}

int AlsaWrapper::DeviceNameHint(int card, const char* iface, void*** hints) {
  return snd_device_name_hint(card, iface, hints);
}

char* AlsaWrapper::DeviceNameGetHint(const void* hint, const char* id) {
  return snd_device_name_get_hint(hint, id);
}

int AlsaWrapper::DeviceNameFreeHint(void** hints) {
  return snd_device_name_free_hint(hints);
}

int AlsaWrapper::CardNext(int* rcard) {
  return snd_card_next(rcard);
}

int AlsaWrapper::PcmClose(snd_pcm_t* handle) {
  return snd_pcm_close(handle);
}

int AlsaWrapper::PcmPrepare(snd_pcm_t* handle) {
  return snd_pcm_prepare(handle);
}

int AlsaWrapper::PcmDrop(snd_pcm_t* handle) {
  return snd_pcm_drop(handle);
}

int AlsaWrapper::PcmDelay(snd_pcm_t* handle, snd_pcm_sframes_t* delay) {
  return snd_pcm_delay(handle, delay);
}

snd_pcm_sframes_t AlsaWrapper::PcmWritei(snd_pcm_t* handle,
                                         const void* buffer,
                                         snd_pcm_uframes_t size) {
  return snd_pcm_writei(handle, buffer, size);
}

snd_pcm_sframes_t AlsaWrapper::PcmReadi(snd_pcm_t* handle,
                                        void* buffer,
                                        snd_pcm_uframes_t size) {
  return snd_pcm_readi(handle, buffer, size);
}

int AlsaWrapper::PcmRecover(snd_pcm_t* handle, int err, int silent) {
  return snd_pcm_recover(handle, err, silent);
}

const char* AlsaWrapper::PcmName(snd_pcm_t* handle) {
  return snd_pcm_name(handle);
}

int AlsaWrapper::ConfigureHwParams(snd_pcm_t* handle,
                                   snd_pcm_hw_params_t* hw_params,
                                   snd_pcm_format_t format,
                                   snd_pcm_access_t access,
                                   unsigned int channels,
                                   unsigned int rate,
                                   int soft_resample,
                                   unsigned int latency) {
  int err = 0;
  if ((err = snd_pcm_hw_params_any(handle, hw_params)) < 0)
    return err;

  if ((err = snd_pcm_hw_params_set_rate_resample(handle, hw_params,
                                                 soft_resample)) < 0) {
    return err;
  }

  if ((err = snd_pcm_hw_params_set_format(handle, hw_params, format)) < 0)
    return err;

  int dir = 0;
  unsigned new_rate = rate;
  if ((err = snd_pcm_hw_params_set_rate_near(handle, hw_params,
                                             &new_rate, &dir)) < 0) {
    return err;
  }

  if ((err = snd_pcm_hw_params_set_access(handle, hw_params, access)) < 0)
    return err;

  if ((err = snd_pcm_hw_params_set_channels(handle, hw_params, channels)) < 0)
    return err;

  unsigned buffer_time = latency;
  if (buffer_time == 0) {
    if ((err = snd_pcm_hw_params_get_buffer_time_max(hw_params,
                                                      &buffer_time, 0)) < 0) {
      return err;
    }
    if (buffer_time > 500000)
      buffer_time = 500000;
  }

  unsigned period_time = buffer_time / 4;
  if ((err = snd_pcm_hw_params_set_period_time_near(handle, hw_params,
                                                    &period_time, 0)) < 0) {
    return err;
  }

  err = snd_pcm_hw_params_set_buffer_time_near(handle, hw_params,
                                                &buffer_time, 0);
  return err;
}

int AlsaWrapper::PcmSetParams(snd_pcm_t* handle, snd_pcm_format_t format,
                              snd_pcm_access_t access, unsigned int channels,
                              unsigned int rate, int soft_resample,
                              unsigned int latency) {
  int err = 0;
  snd_pcm_hw_params_t *hw_params;
  if ((err = snd_pcm_hw_params_malloc(&hw_params)) < 0)
    return err;

  if ((err = ConfigureHwParams(handle, hw_params, format, access, channels,
                               rate, soft_resample, latency)) >= 0) {
    err = snd_pcm_hw_params(handle, hw_params);
  }

  snd_pcm_hw_params_free(hw_params);
  return err;
}

int AlsaWrapper::PcmGetParams(snd_pcm_t* handle, snd_pcm_uframes_t* buffer_size,
                              snd_pcm_uframes_t* period_size) {
  return snd_pcm_get_params(handle, buffer_size, period_size);
}

snd_pcm_sframes_t AlsaWrapper::PcmAvailUpdate(snd_pcm_t* handle) {
  return snd_pcm_avail_update(handle);
}

snd_pcm_state_t AlsaWrapper::PcmState(snd_pcm_t* handle) {
  return snd_pcm_state(handle);
}

const char* AlsaWrapper::StrError(int errnum) {
  return snd_strerror(errnum);
}

int AlsaWrapper::PcmStart(snd_pcm_t* handle) {
  return snd_pcm_start(handle);
}

