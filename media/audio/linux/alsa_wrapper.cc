// Copyright (c) 2009 The Chromium Authors. All rights reserved.
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

int AlsaWrapper::PcmRecover(snd_pcm_t* handle, int err, int silent) {
  return snd_pcm_recover(handle, err, silent);
}

const char* AlsaWrapper::PcmName(snd_pcm_t* handle) {
  return snd_pcm_name(handle);
}

int AlsaWrapper::PcmSetParams(snd_pcm_t* handle, snd_pcm_format_t format,
                              snd_pcm_access_t access, unsigned int channels,
                              unsigned int rate, int soft_resample,
                              unsigned int latency) {
  return snd_pcm_set_params(handle, format, access, channels, rate,
                            soft_resample, latency);
}

snd_pcm_sframes_t AlsaWrapper::PcmAvailUpdate(snd_pcm_t* handle) {
  return snd_pcm_avail_update(handle);
}

const char* AlsaWrapper::StrError(int errnum) {
  return snd_strerror(errnum);
}
