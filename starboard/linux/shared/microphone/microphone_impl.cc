// Copyright 2026 The Cobalt Authors. All Rights Reserved.
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

// clang-format off
#include "starboard/shared/starboard/microphone/microphone_internal.h"
// clang-format on

#include <alsa/asoundlib.h>

#include "starboard/common/check_op.h"
#include "starboard/common/log.h"
#include "starboard/common/result.h"

namespace starboard {

namespace {

constexpr int kSampleRateInHz = 16'000;
constexpr int kChannels = 1;
constexpr int kFramesPerBuffer = kSampleRateInHz / 100;
constexpr int kMinReadSizeBytes =
    kFramesPerBuffer * kChannels * sizeof(int16_t);
// We request a buffer twice the size of a single read to allow for double-
// buffering
constexpr int kBufferSizeMultiplier = 2;

Unexpected<std::string> SndError(const std::string& method_name, int error) {
  return Failure(method_name + " failed: " + snd_strerror(error));
}

Result<void> OpenPcm(snd_pcm_t*& handle) {
  int error = snd_pcm_open(&handle, "default", SND_PCM_STREAM_CAPTURE,
                           /*mode=SND_PCM_NONBLOCK*/ 0);
  if (error < 0) {
    handle = nullptr;
    return SndError("snd_pcm_open", error);
  }

  snd_pcm_hw_params_t* hw_params;
  snd_pcm_hw_params_alloca(&hw_params);

  error = snd_pcm_hw_params_any(handle, hw_params);
  if (error < 0) {
    return SndError("snd_pcm_hw_params_any", error);
  }

  error = snd_pcm_hw_params_set_access(handle, hw_params,
                                       SND_PCM_ACCESS_RW_INTERLEAVED);
  if (error < 0) {
    return SndError("snd_pcm_hw_params_set_access", error);
  }

  error =
      snd_pcm_hw_params_set_format(handle, hw_params, SND_PCM_FORMAT_S16_LE);
  if (error < 0) {
    return SndError("snd_pcm_hw_params_set_format", error);
  }

  unsigned int sample_rate = kSampleRateInHz;
  error = snd_pcm_hw_params_set_rate_near(handle, hw_params, &sample_rate,
                                          /*dir=*/nullptr);
  if (error < 0) {
    return SndError("snd_pcm_hw_params_set_rate_near", error);
  }

  error = snd_pcm_hw_params_set_channels(handle, hw_params, kChannels);
  if (error < 0) {
    return SndError("snd_pcm_hw_params_set_channels", error);
  }

  snd_pcm_uframes_t period_size_in_frames = kFramesPerBuffer;
  error = snd_pcm_hw_params_set_period_size_near(
      handle, hw_params, &period_size_in_frames, nullptr);
  if (error < 0) {
    return SndError("snd_pcm_hw_params_set_period_size_near", error);
  }

  snd_pcm_uframes_t buffer_size_in_frames =
      period_size_in_frames * kBufferSizeMultiplier;
  error = snd_pcm_hw_params_set_buffer_size_near(handle, hw_params,
                                                 &buffer_size_in_frames);
  if (error < 0) {
    return SndError("snd_pcm_hw_params_set_buffer_size_near", error);
  }

  error = snd_pcm_hw_params(handle, hw_params);
  if (error < 0) {
    return SndError("snd_pcm_hw_params", error);
  }

  error = snd_pcm_nonblock(handle, 1);
  if (error < 0) {
    return SndError("snd_pcm_nonblocl", error);
  }

  error = snd_pcm_prepare(handle);
  if (error < 0) {
    return SndError("snd_pcm_prepare", error);
  }

  error = snd_pcm_start(handle);
  if (error < 0) {
    return SndError("snd_pcm_start", error);
  }
  return Success();
}

class SbMicrophoneImpl : public SbMicrophonePrivate {
 public:
  SbMicrophoneImpl() : handle_(nullptr) {}
  ~SbMicrophoneImpl() override { Close(); }

  bool Open() override {
    if (handle_) {
      snd_pcm_state_t state = snd_pcm_state(handle_);
      if (state == SND_PCM_STATE_RUNNING) {
        return true;
      }
      if (state == SND_PCM_STATE_PREPARED) {
        int error = snd_pcm_start(handle_);
        if (error >= 0) {
          return true;
        } else {
          SB_LOG(ERROR) << "Failed to restart prepared stream: "
                        << snd_strerror(error);
          Close();
        }
      } else {
        Close();
      }
    }

    // Open the default capture device.
    if (Result<void> result = OpenPcm(handle_); !result) {
      SB_LOG(ERROR) << "OpenPcm failed: " << result.error();
      Close();
      return false;
    }

    return true;
  }

  bool Close() override {
    if (!handle_) {
      return true;
    }

    int error = snd_pcm_drop(handle_);
    if (error != 0) {
      SB_LOG(WARNING) << "snd_pcm_drop failed, but we ignore it: "
                      << snd_strerror(error);
    }

    error = snd_pcm_close(handle_);
    if (error != 0) {
      SB_LOG(WARNING) << "snd_pcm_close failed, but we ignore it: "
                      << snd_strerror(error);
    }

    handle_ = nullptr;
    return true;
  }

  int Read(void* out_audio_data, int audio_data_size) override {
    if (!handle_ || !out_audio_data || audio_data_size <= 0) {
      return -1;
    }
    snd_pcm_state_t state = snd_pcm_state(handle_);
    if (state != SND_PCM_STATE_PREPARED && state != SND_PCM_STATE_RUNNING) {
      SB_LOG(ERROR) << "SbMicrophoneImpl::Read - unexpected pcm state: "
                    << snd_pcm_state_name(state);
    }
    int frames_to_read = audio_data_size / (kChannels * sizeof(int16_t));
    int frames_read = snd_pcm_readi(handle_, out_audio_data, frames_to_read);

    if (frames_read == -EAGAIN) {
      return 0;
    }

    if (frames_read < 0) {
      SB_LOG(ERROR) << "SbMicrophoneImpl::Read - snd_pcm_readi error: "
                    << frames_read << " (" << snd_strerror(frames_read) << ")";
      // Recover from the error.
      int err = snd_pcm_recover(handle_, frames_read, 1);
      SB_LOG(INFO) << "snd_pcm_recover result: " << err << " ("
                   << snd_strerror(err) << ")";
      if (err >= 0) {
        // For capture streams, we need to explicitly start the stream again
        // after recovery.
        err = snd_pcm_start(handle_);
        if (err < 0) {
          SB_LOG(ERROR) << "Failed to restart microphone after recovery: "
                        << snd_strerror(err);
          Close();
        }
      } else {
        SB_LOG(ERROR) << "Failed to recover microphone. Closing.";
        Close();
      }
      return -1;
    }
    if (frames_read < frames_to_read) {
      SB_LOG(WARNING) << "Short read from microphone: " << frames_read << "/"
                      << frames_to_read;
    }

    return frames_read * kChannels * sizeof(int16_t);
  }

 private:
  snd_pcm_t* handle_;
};

}  // namespace

}  // namespace starboard

// static
int SbMicrophonePrivate::GetAvailableMicrophones(
    SbMicrophoneInfo* out_info_array,
    int info_array_size) {
  void** hints;
  int error = snd_device_name_hint(-1, "pcm", &hints);
  if (error != 0) {
    SB_LOG(WARNING) << "snd_device_name_hint failed:" << snd_strerror(error);
    return 0;
  }

  int count = 0;
  for (void** n = hints; *n != nullptr; n++) {
    char* name = snd_device_name_get_hint(*n, "NAME");
    char* desc = snd_device_name_get_hint(*n, "DESC");
    char* ioid = snd_device_name_get_hint(*n, "IOID");

    if (name != nullptr && (ioid == nullptr || strcmp(ioid, "Input") == 0)) {
      // Filter out the "null" device, which is not a real microphone.
      if (strcmp(name, "null") == 0) {
        if (name != nullptr) {
          free(name);
        }
        if (desc != nullptr) {
          free(desc);
        }
        if (ioid != nullptr) {
          free(ioid);
        }
        continue;
      }
      if (out_info_array && count < info_array_size) {
        SbMicrophoneInfo* info = &out_info_array[count];
        info->id = reinterpret_cast<SbMicrophoneId>(count + 1);
        info->type = kSbMicrophoneUnknown;
        info->max_sample_rate_hz = starboard::kSampleRateInHz;
        info->min_read_size = starboard::kMinReadSizeBytes;
        snprintf(info->label, kSbMicrophoneLabelSize, "%s", desc ? desc : name);
      }
      count++;
    }

    if (name != nullptr) {
      free(name);
    }
    if (desc != nullptr) {
      free(desc);
    }
    if (ioid != nullptr) {
      free(ioid);
    }
  }
  snd_device_name_free_hint(hints);
  return count;
}

// static
bool SbMicrophonePrivate::IsMicrophoneSampleRateSupported(
    SbMicrophoneId id,
    int sample_rate_in_hz) {
  if (!SbMicrophoneIdIsValid(id)) {
    return false;
  }
  return sample_rate_in_hz == starboard::kSampleRateInHz;
}

namespace {
constexpr int kUnusedBufferSize = 32 * 1024;
// Only a single microphone is supported.
SbMicrophone s_microphone = kSbMicrophoneInvalid;
}  // namespace

// static
SbMicrophone SbMicrophonePrivate::CreateMicrophone(SbMicrophoneId id,
                                                   int sample_rate_in_hz,
                                                   int buffer_bytes) {
  SB_LOG(INFO) << "SbMicrophonePrivate::CreateMicrophone():"
               << " id=" << id << ", sample_rate_in_hz=" << sample_rate_in_hz
               << ", buffer_bytes=" << buffer_bytes;

  if (!SbMicrophoneIdIsValid(id)) {
    return kSbMicrophoneInvalid;
  }
  if (!IsMicrophoneSampleRateSupported(id, sample_rate_in_hz)) {
    SB_LOG(ERROR) << "CreateMicrophone() - FAILED: Sample rate not supported: "
                  << sample_rate_in_hz;
    return kSbMicrophoneInvalid;
  }
  if (buffer_bytes > kUnusedBufferSize || buffer_bytes <= 0) {
    SB_LOG(ERROR) << "CreateMicrophone() - FAILED: Bad buffer size: "
                  << buffer_bytes;
    return kSbMicrophoneInvalid;
  }

  if (s_microphone != kSbMicrophoneInvalid) {
    SB_LOG(ERROR) << "CreateMicrophone() - FAILED: Microphone already exists";
    return kSbMicrophoneInvalid;
  }

  s_microphone = new starboard::SbMicrophoneImpl();
  return s_microphone;
}

// static
void SbMicrophonePrivate::DestroyMicrophone(SbMicrophone microphone) {
  if (!SbMicrophoneIsValid(microphone)) {
    return;
  }

  SB_DCHECK_EQ(s_microphone, microphone);
  s_microphone->Close();

  delete s_microphone;
  s_microphone = kSbMicrophoneInvalid;
}
