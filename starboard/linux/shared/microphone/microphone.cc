// Copyright 2024 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <alsa/asoundlib.h>

#include "starboard/common/log.h"
#include "starboard/common/memory.h"
#include "starboard/shared/starboard/microphone/microphone_internal.h"
#include "starboard/thread.h"

namespace {

const int kSampleRateInHz = 16000;
const int kSamplesPerBuffer = 128;
const int kBufferSizeInBytes = kSamplesPerBuffer * sizeof(int16_t);
const int kChannels = 1;

class SbMicrophoneImpl : public SbMicrophonePrivate {
 public:
  SbMicrophoneImpl(SbMicrophoneId id, int sample_rate_in_hz, int buffer_size)
      : id_(id),
        sample_rate_in_hz_(sample_rate_in_hz),
        buffer_size_(buffer_size),
        handle_(NULL) {
    SB_LOG(INFO) << "YO THOR - SbMicrophoneImpl::SbMicrophoneImpl"
                 << " sample_rate_in_hz: " << sample_rate_in_hz
                 << " buffer_size: " << buffer_size;
  }
  ~SbMicrophoneImpl() override { Close(); }

  bool Open() override {
    SB_LOG(INFO) << "YO THOR - SbMicrophoneImpl::Open";
    if (handle_) {
      return true;
    }

    // Get the device name for the given ID.
    char device_name[256];
    if (!GetDeviceName(id_, device_name, sizeof(device_name))) {
      return false;
    }

    int error = snd_pcm_open(&handle_, device_name, SND_PCM_STREAM_CAPTURE, 0);
    if (error < 0) {
      handle_ = NULL;
      return false;
    }

    snd_pcm_hw_params_t* hw_params;
    snd_pcm_hw_params_alloca(&hw_params);

    error = snd_pcm_hw_params_any(handle_, hw_params);
    if (error < 0) {
      Close();
      return false;
    }

    error = snd_pcm_hw_params_set_access(handle_, hw_params,
                                         SND_PCM_ACCESS_RW_INTERLEAVED);
    if (error < 0) {
      Close();
      return false;
    }

    error =
        snd_pcm_hw_params_set_format(handle_, hw_params, SND_PCM_FORMAT_S16_LE);
    if (error < 0) {
      Close();
      return false;
    }

    error = snd_pcm_hw_params_set_rate_near(
        handle_, hw_params, (unsigned int*)&sample_rate_in_hz_, NULL);
    if (error < 0) {
      Close();
      return false;
    }

    error = snd_pcm_hw_params_set_channels(handle_, hw_params, kChannels);
    if (error < 0) {
      Close();
      return false;
    }

    snd_pcm_uframes_t buffer_size_in_frames =
        buffer_size_ / (kChannels * sizeof(int16_t));
    error = snd_pcm_hw_params_set_buffer_size_near(handle_, hw_params,
                                                   &buffer_size_in_frames);
    if (error < 0) {
      Close();
      return false;
    }

    error = snd_pcm_hw_params(handle_, hw_params);
    if (error < 0) {
      Close();
      return false;
    }

    error = snd_pcm_prepare(handle_);
    if (error < 0) {
      Close();
      return false;
    }

    error = snd_pcm_start(handle_);
    if (error < 0) {
      Close();
      return false;
    }

    return true;
  }

  bool Close() override {
    if (handle_) {
      snd_pcm_close(handle_);
      handle_ = NULL;
    }
    return true;
  }

  int Read(void* out_audio_data, int audio_data_size) override {
    SB_LOG(INFO) << "YO THOR - SbMicrophoneImpl::Read - audio_data_size: "
                 << audio_data_size;
    if (!handle_ || !out_audio_data || audio_data_size <= 0) {
      return -1;
    }

    int frames_to_read = audio_data_size / (kChannels * sizeof(int16_t));
    SB_LOG(INFO) << "YO THOR - SbMicrophoneImpl::Read - frames_to_read: "
                 << frames_to_read;
    int frames_read = snd_pcm_readi(handle_, out_audio_data, frames_to_read);
    SB_LOG(INFO) << "YO THOR - SbMicrophoneImpl::Read - frames_read: "
                 << frames_read;

    if (frames_read < 0) {
      // Recover from the error.
      snd_pcm_recover(handle_, frames_read, 1);
      return -1;
    }
    if (frames_read < frames_to_read) {
      SB_LOG(WARNING) << "Short read from microphone: " << frames_read << "/"
                      << frames_to_read;
    }

    return frames_read * kChannels * sizeof(int16_t);
  }

  int GetAvailableFrames() {
    if (!handle_) {
      return -1;
    }
    return snd_pcm_avail_update(handle_);
  }

 private:
  bool GetDeviceName(SbMicrophoneId id, char* name, int name_size) {
    void** hints;
    int error = snd_device_name_hint(-1, "pcm", &hints);
    if (error != 0) {
      return false;
    }

    int count = 0;
    for (void** n = hints; *n != NULL; n++) {
      char* dev_name = snd_device_name_get_hint(*n, "NAME");
      char* ioid = snd_device_name_get_hint(*n, "IOID");

      if (dev_name != NULL && (ioid == NULL || strcmp(ioid, "Input") == 0)) {
        count++;
        if (count == (intptr_t)id) {
          snprintf(name, name_size, "%s", dev_name);
          free(dev_name);
          free(ioid);
          snd_device_name_free_hint(hints);
          return true;
        }
      }

      if (dev_name != NULL) {
        free(dev_name);
      }
      if (ioid != NULL) {
        free(ioid);
      }
    }
    snd_device_name_free_hint(hints);
    return false;
  }

  SbMicrophoneId id_;
  int sample_rate_in_hz_;
  int buffer_size_;
  snd_pcm_t* handle_;
};

}  // namespace

int SbMicrophonePrivate::GetAvailableMicrophones(
    SbMicrophoneInfo* out_info_array,
    int info_array_size) {
  if (info_array_size <= 0 || !out_info_array) {
    // Return the number of available microphones if the output array is null or
    // the size is not positive.
    void** hints;
    int error = snd_device_name_hint(-1, "pcm", &hints);
    if (error != 0) {
      return 0;
    }

    int count = 0;
    for (void** n = hints; *n != NULL; n++) {
      char* name = snd_device_name_get_hint(*n, "NAME");
      char* desc = snd_device_name_get_hint(*n, "DESC");
      char* ioid = snd_device_name_get_hint(*n, "IOID");

      if (name != NULL && (ioid == NULL || strcmp(ioid, "Input") == 0)) {
        count++;
      }

      if (name != NULL) {
        free(name);
      }
      if (desc != NULL) {
        free(desc);
      }
      if (ioid != NULL) {
        free(ioid);
      }
    }
    snd_device_name_free_hint(hints);
    return count;
  }

  void** hints;
  int error = snd_device_name_hint(-1, "pcm", &hints);
  if (error != 0) {
    return 0;
  }

  int count = 0;
  for (void** n = hints; *n != NULL && count < info_array_size; n++) {
    char* name = snd_device_name_get_hint(*n, "NAME");
    char* desc = snd_device_name_get_hint(*n, "DESC");
    char* ioid = snd_device_name_get_hint(*n, "IOID");

    if (name != NULL && (ioid == NULL || strcmp(ioid, "Input") == 0)) {
      SbMicrophoneInfo* info = &out_info_array[count];
      info->id = reinterpret_cast<SbMicrophoneId>(count + 1);
      info->type = kSbMicrophoneUnknown;
      info->max_sample_rate_hz = kSampleRateInHz;
      info->min_read_size = kSamplesPerBuffer;
      snprintf(info->label, kSbMicrophoneLabelSize, "%s", desc ? desc : name);
      count++;
    }

    if (name != NULL) {
      free(name);
    }
    if (desc != NULL) {
      free(desc);
    }
    if (ioid != NULL) {
      free(ioid);
    }
  }
  snd_device_name_free_hint(hints);
  return count;
}

bool SbMicrophonePrivate::IsMicrophoneSampleRateSupported(
    SbMicrophoneId id,
    int sample_rate_in_hz) {
  return sample_rate_in_hz == 16000 || sample_rate_in_hz == 48000;
}

SbMicrophone SbMicrophonePrivate::CreateMicrophone(SbMicrophoneId id,
                                                   int sample_rate_in_hz,
                                                   int buffer_size_bytes) {
  if (!SbMicrophoneIdIsValid(id) ||
      !IsMicrophoneSampleRateSupported(id, sample_rate_in_hz) ||
      buffer_size_bytes <= 0) {
    return kSbMicrophoneInvalid;
  }

  return new SbMicrophoneImpl(id, sample_rate_in_hz, buffer_size_bytes);
}

void SbMicrophonePrivate::DestroyMicrophone(SbMicrophone microphone) {
  if (!SbMicrophoneIsValid(microphone)) {
    return;
  }
  delete microphone;
}

int SbMicrophoneGetAvailable(SbMicrophoneInfo* out_info_array,
                             int info_array_size) {
  return SbMicrophonePrivate::GetAvailableMicrophones(out_info_array,
                                                      info_array_size);
}

bool SbMicrophoneIsSampleRateSupported(SbMicrophoneId id,
                                       int sample_rate_in_hz) {
  return SbMicrophonePrivate::IsMicrophoneSampleRateSupported(
      id, sample_rate_in_hz);
}

SbMicrophone SbMicrophoneCreate(SbMicrophoneId id,
                                int sample_rate_in_hz,
                                int buffer_size_bytes) {
  return SbMicrophonePrivate::CreateMicrophone(id, sample_rate_in_hz,
                                               buffer_size_bytes);
}

bool SbMicrophoneOpen(SbMicrophone microphone) {
  if (!SbMicrophoneIsValid(microphone)) {
    return false;
  }
  return microphone->Open();
}

bool SbMicrophoneClose(SbMicrophone microphone) {
  if (!SbMicrophoneIsValid(microphone)) {
    return false;
  }
  return microphone->Close();
}

int SbMicrophoneRead(SbMicrophone microphone,
                     void* out_audio_data,
                     int audio_data_size) {
  if (!SbMicrophoneIsValid(microphone)) {
    return -1;
  }
  return microphone->Read(out_audio_data, audio_data_size);
}

void SbMicrophoneDestroy(SbMicrophone microphone) {
  SbMicrophonePrivate::DestroyMicrophone(microphone);
}

int SbMicrophoneGetAvailableFrames(SbMicrophone microphone) {
  if (!SbMicrophoneIsValid(microphone)) {
    return -1;
  }
  return static_cast<SbMicrophoneImpl*>(microphone)->GetAvailableFrames();
}
