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

#include <stdint.h>  // For uint32_t, uint16_t
#include <stdio.h>   // For FILE, fopen, fclose, fwrite, rewind
#include "starboard/common/log.h"
#include "starboard/common/memory.h"
#include "starboard/shared/starboard/microphone/microphone_internal.h"
#include "starboard/thread.h"

namespace {

const int kSampleRateInHz = 16000;
const int kSamplesPerBuffer = 128;
const int kBufferSizeInBytes = kSamplesPerBuffer * sizeof(int16_t);
const int kChannels = 1;

// --- WAV Header structure ---
typedef struct {
  char chunk_id[4];  // "RIFF"
  uint32_t chunk_size;
  char format[4];        // "WAVE"
  char subchunk1_id[4];  // "fmt "
  uint32_t subchunk1_size;
  uint16_t audio_format;
  uint16_t num_channels;
  uint32_t sample_rate;
  uint32_t byte_rate;
  uint16_t block_align;
  uint16_t bits_per_sample;
  char subchunk2_id[4];  // "data"
  uint32_t subchunk2_size;
} WavHeader;
// ---

void write_wav_header(FILE* file,
                      uint32_t sample_rate,
                      uint16_t num_channels,
                      uint16_t bits_per_sample,
                      uint32_t data_size) {
  if (!file) {
    return;
  }
  WavHeader header;

  // RIFF chunk
  memcpy(header.chunk_id, "RIFF", 4);
  header.chunk_size = 36 + data_size;
  memcpy(header.format, "WAVE", 4);

  // fmt subchunk
  memcpy(header.subchunk1_id, "fmt ", 4);
  header.subchunk1_size = 16;
  header.audio_format = 1;  // PCM
  header.num_channels = num_channels;
  header.sample_rate = sample_rate;
  header.byte_rate = sample_rate * num_channels * (bits_per_sample / 8);
  header.block_align = num_channels * (bits_per_sample / 8);
  header.bits_per_sample = bits_per_sample;

  // data subchunk
  memcpy(header.subchunk2_id, "data", 4);
  header.subchunk2_size = data_size;

  fwrite(&header, sizeof(WavHeader), 1, file);
}

class SbMicrophoneImpl : public SbMicrophonePrivate {
 public:
  SbMicrophoneImpl(SbMicrophoneId id, int sample_rate_in_hz, int buffer_size)
      :  // id_(id),
        sample_rate_in_hz_(sample_rate_in_hz),
        buffer_size_(buffer_size),
        handle_(NULL),
        debug_output_file_(NULL),
        total_frames_read_(0) {
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

    // Open debug WAV file
    debug_output_file_ = fopen("starboard_mic_output.wav", "wb");
    if (debug_output_file_) {
      SB_LOG(INFO)
          << "YO THOR - Opened debug WAV file: starboard_mic_output.wav";
      // Write placeholder header
      write_wav_header(debug_output_file_, sample_rate_in_hz_, kChannels, 16,
                       0);
    } else {
      SB_LOG(ERROR) << "YO THOR - Failed to open debug WAV file.";
    }

    // Open the default capture device.
    int error = snd_pcm_open(&handle_, "default", SND_PCM_STREAM_CAPTURE, 0);
    if (error < 0) {
      handle_ = NULL;
      SB_LOG(ERROR) << "YO THOR - snd_pcm_open failed: " << snd_strerror(error);
      return false;
    }

    snd_pcm_hw_params_t* hw_params;
    snd_pcm_hw_params_alloca(&hw_params);

    error = snd_pcm_hw_params_any(handle_, hw_params);
    SB_LOG(INFO) << "YO THOR - snd_pcm_hw_params_any result: " << error;
    if (error < 0) {
      Close();
      return false;
    }

    error = snd_pcm_hw_params_set_access(handle_, hw_params,
                                         SND_PCM_ACCESS_RW_INTERLEAVED);
    SB_LOG(INFO) << "YO THOR - snd_pcm_hw_params_set_access result: " << error;
    if (error < 0) {
      Close();
      return false;
    }

    error =
        snd_pcm_hw_params_set_format(handle_, hw_params, SND_PCM_FORMAT_S16_LE);
    SB_LOG(INFO) << "YO THOR - snd_pcm_hw_params_set_format result: " << error;
    if (error < 0) {
      Close();
      return false;
    }

    error = snd_pcm_hw_params_set_rate_near(
        handle_, hw_params, (unsigned int*)&sample_rate_in_hz_, NULL);
    SB_LOG(INFO) << "YO THOR - snd_pcm_hw_params_set_rate_near result: "
                 << error;
    if (error < 0) {
      Close();
      return false;
    }

    error = snd_pcm_hw_params_set_channels(handle_, hw_params, kChannels);
    SB_LOG(INFO) << "YO THOR - snd_pcm_hw_params_set_channels result: "
                 << error;
    if (error < 0) {
      Close();
      return false;
    }

    snd_pcm_uframes_t period_size_in_frames =
        buffer_size_ / (kChannels * sizeof(int16_t));
    error = snd_pcm_hw_params_set_period_size_near(
        handle_, hw_params, &period_size_in_frames, NULL);
    SB_LOG(INFO) << "YO THOR - snd_pcm_hw_params_set_period_size_near result: "
                 << error;
    if (error < 0) {
      Close();
      return false;
    }

    snd_pcm_uframes_t buffer_size_in_frames = period_size_in_frames * 2;
    error = snd_pcm_hw_params_set_buffer_size_near(handle_, hw_params,
                                                   &buffer_size_in_frames);
    SB_LOG(INFO) << "YO THOR - snd_pcm_hw_params_set_buffer_size_near result: "
                 << error;
    if (error < 0) {
      Close();
      return false;
    }

    error = snd_pcm_hw_params(handle_, hw_params);
    SB_LOG(INFO) << "YO THOR - snd_pcm_hw_params result: " << error;
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
      snd_pcm_drop(handle_);
      snd_pcm_close(handle_);
      handle_ = NULL;
    }

    if (debug_output_file_) {
      SB_LOG(INFO) << "YO THOR - Finalizing debug WAV file.";
      uint32_t total_data_size =
          total_frames_read_ * kChannels * sizeof(int16_t);
      rewind(debug_output_file_);
      write_wav_header(debug_output_file_, sample_rate_in_hz_, kChannels, 16,
                       total_data_size);
      fclose(debug_output_file_);
      debug_output_file_ = NULL;
      SB_LOG(INFO) << "YO THOR - Wrote " << total_data_size
                   << " bytes of audio data.";
    }
    return true;
  }

  int Read(void* out_audio_data, int audio_data_size) override {
    static int read_log_counter = 0;
    if (!handle_ || !out_audio_data || audio_data_size <= 0) {
      return -1;
    }

    int frames_to_read = audio_data_size / (kChannels * sizeof(int16_t));
    int frames_read = snd_pcm_readi(handle_, out_audio_data, frames_to_read);

    if (read_log_counter < 20) {
      SB_LOG(INFO) << "YO THOR - SbMicrophoneImpl::Read - frames_to_read: "
                   << frames_to_read << ", frames_read: " << frames_read;
      SB_LOG(INFO) << "YO THOR - ALSA PCM State: "
                   << snd_pcm_state_name(snd_pcm_state(handle_));
      read_log_counter++;
    }

    if (frames_read > 0) {
      total_frames_read_ += frames_read;
      if (debug_output_file_) {
        fwrite(out_audio_data, frames_read * kChannels * sizeof(int16_t), 1,
               debug_output_file_);
      }
    }

    if (frames_read < 0) {
      SB_LOG(ERROR)
          << "YO THOR - SbMicrophoneImpl::Read - snd_pcm_readi error: "
          << frames_read;
      // Recover from the error.
      int err = snd_pcm_recover(handle_, frames_read, 1);
      SB_LOG(INFO) << "YO THOR - snd_pcm_recover result: " << err;
      if (err >= 0) {
        // For capture streams, we need to explicitly start the stream again
        // after recovery.
        err = snd_pcm_start(handle_);
        if (err < 0) {
          SB_LOG(ERROR) << "Failed to restart microphone after recovery.";
        }
      }
      return -1;
    }
    if (frames_read < frames_to_read) {
      SB_LOG(WARNING) << "Short read from microphone: " << frames_read << "/"
                      << frames_to_read;
    }

    return frames_read * kChannels * sizeof(int16_t);
  }

  int GetAvailableFrames() {
    static int avail_log_counter = 0;
    if (!handle_) {
      return -1;
    }
    int available_frames = snd_pcm_avail_update(handle_);
    if (avail_log_counter < 20) {
      SB_LOG(INFO) << "YO THOR - SbMicrophoneImpl::GetAvailableFrames - "
                   << "snd_pcm_avail_update returned: " << available_frames;
      avail_log_counter++;
    }
    return available_frames;
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

  // SbMicrophoneId id_;
  int sample_rate_in_hz_;
  int buffer_size_;
  snd_pcm_t* handle_;
  FILE* debug_output_file_;
  long total_frames_read_;
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
