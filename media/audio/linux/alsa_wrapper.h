// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// AlsaWrapper is a simple stateless class that wraps the alsa library commands
// we want to use.  It's purpose is to allow injection of a mock so that the
// higher level code is testable.

#include <alsa/asoundlib.h>

#include "base/basictypes.h"
#include "media/base/media_export.h"

class MEDIA_EXPORT AlsaWrapper {
 public:
  AlsaWrapper();
  virtual ~AlsaWrapper();

  virtual int DeviceNameHint(int card, const char* iface, void*** hints);
  virtual char* DeviceNameGetHint(const void* hint, const char* id);
  virtual int DeviceNameFreeHint(void** hints);
  virtual int CardNext(int* rcard);

  virtual int PcmOpen(snd_pcm_t** handle, const char* name,
                      snd_pcm_stream_t stream, int mode);
  virtual int PcmClose(snd_pcm_t* handle);
  virtual int PcmPrepare(snd_pcm_t* handle);
  virtual int PcmDrop(snd_pcm_t* handle);
  virtual int PcmDelay(snd_pcm_t* handle, snd_pcm_sframes_t* delay);
  virtual snd_pcm_sframes_t PcmWritei(snd_pcm_t* handle,
                                      const void* buffer,
                                      snd_pcm_uframes_t size);
  virtual snd_pcm_sframes_t PcmReadi(snd_pcm_t* handle,
                                     void* buffer,
                                     snd_pcm_uframes_t size);
  virtual int PcmRecover(snd_pcm_t* handle, int err, int silent);
  virtual int PcmSetParams(snd_pcm_t* handle, snd_pcm_format_t format,
                           snd_pcm_access_t access, unsigned int channels,
                           unsigned int rate, int soft_resample,
                           unsigned int latency);
  virtual int PcmGetParams(snd_pcm_t* handle, snd_pcm_uframes_t* buffer_size,
                           snd_pcm_uframes_t* period_size);
  virtual const char* PcmName(snd_pcm_t* handle);
  virtual snd_pcm_sframes_t PcmAvailUpdate(snd_pcm_t* handle);
  virtual snd_pcm_state_t PcmState(snd_pcm_t* handle);
  virtual int PcmStart(snd_pcm_t* handle);

  virtual const char* StrError(int errnum);

 private:
  int ConfigureHwParams(snd_pcm_t* handle, snd_pcm_hw_params_t* hw_params,
                        snd_pcm_format_t format, snd_pcm_access_t access,
                        unsigned int channels, unsigned int rate,
                        int soft_resample, unsigned int latency);
  DISALLOW_COPY_AND_ASSIGN(AlsaWrapper);
};
