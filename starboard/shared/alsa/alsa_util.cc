// Copyright 2016 Google Inc. All Rights Reserved.
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

#include "starboard/shared/alsa/alsa_util.h"

#include <alsa/asoundlib.h>

#include "starboard/log.h"

#define ALSA_CHECK(error, alsa_function, failure_return)      \
  do {                                                        \
    if (error < 0) {                                          \
      SB_LOG(ERROR) << __FUNCTION__ << ": " << #alsa_function \
                    << "() failed with error "                \
                    << snd_strerror(error)                    \
                    << " (" << error << ")";                  \
      return (failure_return);                                \
    }                                                         \
  } while (false)

namespace starboard {
namespace shared {
namespace alsa {

namespace {

const snd_pcm_uframes_t kSilenceThresholdInFrames = 256U;
const snd_pcm_uframes_t kStartThresholdInFrames = 1024U;

template <typename T, typename CloseFunc>
class AutoClose {
 public:
  explicit AutoClose(CloseFunc close_func)
      : valid_(false), value_(NULL), close_func_(close_func) {}
  ~AutoClose() {
    if (valid_) {
      close_func_(value_);
    }
  }
  operator T() {
    SB_DCHECK(valid_);
    return value_;
  }
  T* operator&() {  // NOLINT(runtime/operator)
    SB_DCHECK(!valid_);
    return &value_;
  }
  bool is_valid() const { return valid_; }
  void set_valid() {
    SB_DCHECK(!valid_);
    valid_ = true;
  }
  T Detach() {
    SB_DCHECK(valid_);
    valid_ = false;
    return value_;
  }

 private:
  bool valid_;
  T value_;
  CloseFunc close_func_;
};

class HWParams
    : public AutoClose<snd_pcm_hw_params_t*, void (*)(snd_pcm_hw_params_t*)> {
 public:
  HWParams()
      : AutoClose<snd_pcm_hw_params_t*, void (*)(snd_pcm_hw_params_t*)>(
            snd_pcm_hw_params_free) {}
};

class SWParams
    : public AutoClose<snd_pcm_sw_params_t*, void (*)(snd_pcm_sw_params_t*)> {
 public:
  SWParams()
      : AutoClose<snd_pcm_sw_params_t*, void (*)(snd_pcm_sw_params_t*)>(
            snd_pcm_sw_params_free) {}
};

class PcmHandle : public AutoClose<snd_pcm_t*, int (*)(snd_pcm_t*)> {
 public:
  PcmHandle() : AutoClose<snd_pcm_t*, int (*)(snd_pcm_t*)>(snd_pcm_close) {}
};

}  // namespace

void* AlsaOpenPlaybackDevice(int channel,
                             int sample_rate,
                             int frames_per_request,
                             int buffer_size_in_frames,
                             snd_pcm_format_t sample_type) {
  SB_DCHECK(sample_type == SND_PCM_FORMAT_FLOAT_LE ||
            sample_type == SND_PCM_FORMAT_S16);

  PcmHandle playback_handle;
  int error =
      snd_pcm_open(&playback_handle, "default", SND_PCM_STREAM_PLAYBACK, 0);
  ALSA_CHECK(error, snd_pcm_open, NULL);
  playback_handle.set_valid();

  HWParams hw_params;
  error = snd_pcm_hw_params_malloc(&hw_params);
  ALSA_CHECK(error, snd_pcm_hw_params_malloc, NULL);
  hw_params.set_valid();

  error = snd_pcm_hw_params_any(playback_handle, hw_params);
  ALSA_CHECK(error, snd_pcm_hw_params_any, NULL);

  error = snd_pcm_hw_params_set_access(playback_handle, hw_params,
                                       SND_PCM_ACCESS_RW_INTERLEAVED);
  ALSA_CHECK(error, snd_pcm_hw_params_set_access, NULL);

  error = snd_pcm_hw_params_set_format(playback_handle, hw_params, sample_type);
  ALSA_CHECK(error, snd_pcm_hw_params_set_format, NULL);

  error =
      snd_pcm_hw_params_set_rate(playback_handle, hw_params, sample_rate, 0);
  ALSA_CHECK(error, snd_pcm_hw_params_set_rate, NULL);

  error = snd_pcm_hw_params_set_channels(playback_handle, hw_params, channel);
  ALSA_CHECK(error, snd_pcm_hw_params_set_channels, NULL);

  snd_pcm_uframes_t buffer_size = buffer_size_in_frames;
  error = snd_pcm_hw_params_set_buffer_size_near(playback_handle, hw_params,
                                                 &buffer_size);
  ALSA_CHECK(error, snd_pcm_hw_params_set_buffer_size, NULL);

  error = snd_pcm_hw_params(playback_handle, hw_params);
  ALSA_CHECK(error, snd_pcm_hw_params, NULL);

  SWParams sw_params;
  error = snd_pcm_sw_params_malloc(&sw_params);
  ALSA_CHECK(error, snd_pcm_sw_params_malloc, NULL);
  sw_params.set_valid();

  error = snd_pcm_sw_params_current(playback_handle, sw_params);
  ALSA_CHECK(error, snd_pcm_sw_params_current, NULL);

  error = snd_pcm_sw_params_set_avail_min(playback_handle, sw_params,
                                          frames_per_request);
  ALSA_CHECK(error, snd_pcm_sw_params_set_avail_min, NULL);

  error = snd_pcm_sw_params_set_silence_threshold(playback_handle, sw_params,
                                                  kSilenceThresholdInFrames);
  ALSA_CHECK(error, snd_pcm_sw_params_set_silence_threshold, NULL);

  error = snd_pcm_sw_params_set_start_threshold(playback_handle, sw_params,
                                                kStartThresholdInFrames);
  ALSA_CHECK(error, snd_pcm_sw_params_set_start_threshold, NULL);

  error = snd_pcm_sw_params(playback_handle, sw_params);
  ALSA_CHECK(error, snd_pcm_sw_params, NULL);

  error = snd_pcm_prepare(playback_handle);
  ALSA_CHECK(error, snd_pcm_prepare, NULL);

  return playback_handle.Detach();
}

int AlsaWriteFrames(void* playback_handle,
                    const void* buffer,
                    int frames_to_write) {
  if (frames_to_write == 0) {
    return 0;
  }

  int error;
  snd_pcm_t* handle = reinterpret_cast<snd_pcm_t*>(playback_handle);

  int frames = 0;
  for (;;) {
    frames = snd_pcm_writei(handle, buffer, frames_to_write);
    if (frames > 0) {
      return frames;
    } else if (frames == -EPIPE) {
      error = snd_pcm_prepare(handle);
      ALSA_CHECK(error, snd_pcm_prepare, 0);
    } else {
      ALSA_CHECK(frames, snd_pcm_writei, 0);
      // "frames == 0" means snd_pcm_writei() is interrupted, we'll retry.
    }
  }

  SB_NOTREACHED();
  return 0;
}

int AlsaGetBufferedFrames(void* playback_handle) {
  int error;
  snd_pcm_t* handle = reinterpret_cast<snd_pcm_t*>(playback_handle);
  int state = snd_pcm_state(handle);
  // snd_pcm_delay() isn't able to catch xrun or setup, so we explicitly check
  // for them.
  if (state == SND_PCM_STATE_XRUN || state == SND_PCM_STATE_SETUP) {
    error = snd_pcm_prepare(handle);
    ALSA_CHECK(error, snd_pcm_prepare, -1);
    // The buffer has already been reset, so the delay is 0.
    return 0;
  }

  snd_pcm_sframes_t delay;
  error = snd_pcm_delay(handle, &delay);
  if (error == 0) {
    if (delay < 0) {
      SB_LOG(ERROR) << __FUNCTION__
                    << ": snd_pcm_delay() failed with negative delay " << delay;
      return -1;
    }
    return delay;
  }
  ALSA_CHECK(error, snd_pcm_delay, -1);
  return -1;
}

void AlsaCloseDevice(void* playback_handle) {
  if (playback_handle) {
    snd_pcm_drain(reinterpret_cast<snd_pcm_t*>(playback_handle));
    snd_pcm_close(reinterpret_cast<snd_pcm_t*>(playback_handle));
  }
}

bool AlsaDrain(void* playback_handle) {
  if (playback_handle) {
    snd_pcm_t* handle = reinterpret_cast<snd_pcm_t*>(playback_handle);

    int error;
    error = snd_pcm_drain(handle);
    ALSA_CHECK(error, snd_pcm_drain, false);
  }
  return true;
}

}  // namespace alsa
}  // namespace shared
}  // namespace starboard
