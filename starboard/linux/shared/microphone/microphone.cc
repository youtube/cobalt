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

#include "starboard/shared/starboard/microphone/microphone_internal.h"

#include <alsa/asoundlib.h>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

#include "starboard/common/log.h"
#include "starboard/thread.h"

namespace {

const SbMicrophoneId kSbMicrophoneIdDefault =  // Changed constexpr to const
    reinterpret_cast<SbMicrophoneId>(1);

constexpr int kSampleRateInHz = 48000;
constexpr int kChannels = 1;
// 10ms of audio data.
constexpr int kBufferFrames = kSampleRateInHz / 100;
// 2 seconds of audio data.
constexpr int kCircularBufferSizeInFrames = 2 * kSampleRateInHz;

// A singleton class to manage a single, shared, pre-warmed audio producer
// for the default microphone. This class spawns a thread to continuously read
// from the ALSA device into a circular buffer.
class DefaultAudioProducer {
 public:
  static DefaultAudioProducer& GetInstance() {
    static DefaultAudioProducer instance;
    return instance;
  }

  // Increments a reference count and starts the reader thread if it's the
  // first user. This is called to "pre-warm" the microphone.
  void Start();

  // Decrements the reference count and stops the reader thread if there are
  // no more users.
  void Stop();

  // Reads audio from the circular buffer. This is called by consumer streams.
  int Read(void* out_audio_data, int audio_data_size);

 private:
  DefaultAudioProducer() = default;
  ~DefaultAudioProducer();

  void ReaderThread();
  bool OpenDevice();
  void CloseDevice();

  std::atomic<int> ref_count_{0};
  snd_pcm_t* handle_ = nullptr;
  std::thread reader_thread_;
  std::atomic<bool> keep_reading_{false};

  std::mutex buffer_mutex_;
  std::vector<int16_t> buffer_;
  size_t read_pos_ = 0;
  size_t write_pos_ = 0;
  std::condition_variable data_available_cv_;
};

void DefaultAudioProducer::Start() {
  if (ref_count_++ == 0) {
    SB_LOG(INFO) << "YO THOR Starting default audio producer.";
    keep_reading_ = true;
    reader_thread_ = std::thread(&DefaultAudioProducer::ReaderThread, this);
  }
}

void DefaultAudioProducer::Stop() {
  if (--ref_count_ == 0) {
    SB_LOG(INFO) << "YO THOR Stopping default audio producer.";
    if (keep_reading_) {
      keep_reading_ = false;
      data_available_cv_.notify_all();  // Wake up the reader thread to exit.
      if (reader_thread_.joinable()) {
        reader_thread_.join();
      }
    }
  }
}

DefaultAudioProducer::~DefaultAudioProducer() {
  SB_DCHECK(ref_count_ == 0);
  if (reader_thread_.joinable()) {
    keep_reading_ = false;
    data_available_cv_.notify_all();
    reader_thread_.join();
  }
  CloseDevice();
}

int DefaultAudioProducer::Read(void* out_audio_data, int audio_data_size) {
  if (!keep_reading_ || !handle_) {
    return 0;
  }

  int frames_to_read = audio_data_size / (kChannels * sizeof(int16_t));
  int frames_read = 0;
  auto* out_ptr = static_cast<int16_t*>(out_audio_data);

  std::unique_lock<std::mutex> lock(buffer_mutex_);
  while (frames_read < frames_to_read) {
    if (read_pos_ == write_pos_) {
      // Buffer is empty, wait for more data.
      data_available_cv_.wait(lock);
      if (!keep_reading_) {
        return frames_read * kChannels * sizeof(int16_t);
      }
      continue;
    }

    if (read_pos_ < write_pos_) {
      int frames_to_copy =
          std::min(frames_to_read - frames_read, (int)(write_pos_ - read_pos_));
      memcpy(out_ptr + frames_read, &buffer_[read_pos_],
             frames_to_copy * sizeof(int16_t));
      read_pos_ += frames_to_copy;
      frames_read += frames_to_copy;
    } else {  // wrap around
      int frames_to_copy = std::min(frames_to_read - frames_read,
                                    (int)(buffer_.size() - read_pos_));
      memcpy(out_ptr + frames_read, &buffer_[read_pos_],
             frames_to_copy * sizeof(int16_t));
      read_pos_ = (read_pos_ + frames_to_copy) % buffer_.size();
      frames_read += frames_to_copy;
    }
  }

  return frames_read * kChannels * sizeof(int16_t);
}

void DefaultAudioProducer::ReaderThread() {
  // Removed SbThreadSetName call here.
  if (!OpenDevice()) {
    return;
  }

  std::vector<int16_t> read_buffer(kBufferFrames * kChannels);
  while (keep_reading_) {
    int frames_read = snd_pcm_readi(handle_, read_buffer.data(), kBufferFrames);

    if (frames_read < 0) {
      SB_LOG(ERROR) << "snd_pcm_readi error: " << snd_strerror(frames_read);
      snd_pcm_recover(handle_, frames_read, 1);
      continue;
    }

    if (frames_read > 0) {
      std::unique_lock<std::mutex> lock(buffer_mutex_);
      for (int i = 0; i < frames_read; ++i) {
        buffer_[write_pos_] = read_buffer[i];
        write_pos_ = (write_pos_ + 1) % buffer_.size();
        if (write_pos_ == read_pos_) {
          // Buffer is full, advance read pointer to drop oldest frame.
          read_pos_ = (read_pos_ + 1) % buffer_.size();
        }
      }
      lock.unlock();
      data_available_cv_.notify_one();
    }
  }
  CloseDevice();
}

bool DefaultAudioProducer::OpenDevice() {
  std::lock_guard<std::mutex> lock(buffer_mutex_);
  if (handle_) {
    return true;
  }

  buffer_.resize(kCircularBufferSizeInFrames * kChannels);

  int error = snd_pcm_open(&handle_, "default", SND_PCM_STREAM_CAPTURE, 0);
  if (error < 0) {
    handle_ = nullptr;
    SB_LOG(ERROR) << "snd_pcm_open failed: " << snd_strerror(error);
    return false;
  }

  snd_pcm_hw_params_t* hw_params;
  snd_pcm_hw_params_alloca(&hw_params);
  snd_pcm_hw_params_any(handle_, hw_params);
  snd_pcm_hw_params_set_access(handle_, hw_params,
                               SND_PCM_ACCESS_RW_INTERLEAVED);
  snd_pcm_hw_params_set_format(handle_, hw_params, SND_PCM_FORMAT_S16_LE);
  unsigned int sample_rate = kSampleRateInHz;
  snd_pcm_hw_params_set_rate_near(handle_, hw_params, &sample_rate, 0);
  snd_pcm_hw_params_set_channels(handle_, hw_params, kChannels);
  snd_pcm_uframes_t period_size = kBufferFrames;
  snd_pcm_hw_params_set_period_size_near(handle_, hw_params, &period_size, 0);
  snd_pcm_uframes_t buffer_size_in_frames = period_size * 4;
  snd_pcm_hw_params_set_buffer_size_near(handle_, hw_params,
                                         &buffer_size_in_frames);

  error = snd_pcm_hw_params(handle_, hw_params);
  if (error < 0) {
    SB_LOG(ERROR) << "snd_pcm_hw_params failed: " << snd_strerror(error);
    CloseDevice();
    return false;
  }

  error = snd_pcm_prepare(handle_);
  if (error < 0) {
    SB_LOG(ERROR) << "snd_pcm_prepare failed: " << snd_strerror(error);
    CloseDevice();
    return false;
  }

  error = snd_pcm_start(handle_);
  if (error < 0) {
    SB_LOG(ERROR) << "snd_pcm_start failed: " << snd_strerror(error);
    CloseDevice();
    return false;
  }
  return true;
}

void DefaultAudioProducer::CloseDevice() {
  if (handle_) {
    snd_pcm_drop(handle_);
    snd_pcm_close(handle_);
    handle_ = nullptr;
  }
}

class SbMicrophoneImpl : public SbMicrophonePrivate {
 public:
  SbMicrophoneImpl(SbMicrophoneId id)
      : is_default_(id == kSbMicrophoneIdDefault) {}
  ~SbMicrophoneImpl() override { Close(); }

  bool Open() override {
    if (is_default_) {
      DefaultAudioProducer::GetInstance().Start();
      return true;
    }
    // Non-default microphones are not supported in this implementation.
    return false;
  }

  bool Close() override {
    if (is_default_) {
      DefaultAudioProducer::GetInstance().Stop();
    }
    return true;
  }

  int Read(void* out_audio_data, int audio_data_size) override {
    if (is_default_) {
      return DefaultAudioProducer::GetInstance().Read(out_audio_data,
                                                      audio_data_size);
    }
    return -1;
  }

 private:
  const bool is_default_;
};

}  // namespace

int SbMicrophonePrivate::GetAvailableMicrophones(
    SbMicrophoneInfo* out_info_array,
    int info_array_size) {
  // Pre-warm the default microphone producer on the first call.
  DefaultAudioProducer::GetInstance().Start();

  void** hints;
  int error = snd_device_name_hint(-1, "pcm", &hints);
  if (error != 0) {
    return 0;
  }

  std::vector<SbMicrophoneInfo> found_microphones;
  for (void** n = hints; *n != NULL; n++) {
    char* name = snd_device_name_get_hint(*n, "NAME");
    if (!name) {
      continue;
    }

    char* ioid = snd_device_name_get_hint(*n, "IOID");
    if (ioid != NULL && strcmp(ioid, "Input") != 0) {
      free(name);
      if (ioid) {
        free(ioid);
      }
      continue;
    }

    char* desc = snd_device_name_get_hint(*n, "DESC");

    SbMicrophoneInfo info = {};
    // Identify the default microphone and assign it the well-known ID.
    if (strcmp(name, "default") == 0) {
      info.id = kSbMicrophoneIdDefault;
    } else {
      // Use a simple integer cast for other microphone IDs for simplicity.
      info.id = reinterpret_cast<SbMicrophoneId>(found_microphones.size() + 2);
    }
    info.max_sample_rate_hz = kSampleRateInHz;
    snprintf(info.label, kSbMicrophoneLabelSize, "%s", desc ? desc : name);

    found_microphones.push_back(info);

    free(name);
    if (desc) {
      free(desc);
    }
    if (ioid) {
      free(ioid);
    }
  }
  snd_device_name_free_hint(hints);

  if (info_array_size <= 0 || !out_info_array) {
    return found_microphones.size();
  }

  int mics_to_copy = std::min<int>(found_microphones.size(), info_array_size);
  for (int i = 0; i < mics_to_copy; ++i) {
    out_info_array[i] = found_microphones[i];
  }

  return mics_to_copy;
}

bool SbMicrophonePrivate::IsMicrophoneSampleRateSupported(
    SbMicrophoneId id,
    int sample_rate_in_hz) {
  return sample_rate_in_hz == kSampleRateInHz;
}

SbMicrophone SbMicrophonePrivate::CreateMicrophone(SbMicrophoneId id,
                                                   int sample_rate_in_hz,
                                                   int buffer_size_bytes) {
  if (!SbMicrophoneIdIsValid(id) ||
      !IsMicrophoneSampleRateSupported(id, sample_rate_in_hz)) {
    return kSbMicrophoneInvalid;
  }
  return new SbMicrophoneImpl(id);
}

void SbMicrophonePrivate::DestroyMicrophone(SbMicrophone microphone) {
  if (!SbMicrophoneIsValid(microphone)) {
    return;
  }
  delete microphone;
}

// Public API Implementation

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
  if (!SbMicrophoneIdIsValid(id) ||
      !SbMicrophonePrivate::IsMicrophoneSampleRateSupported(
          id, sample_rate_in_hz)) {
    return kSbMicrophoneInvalid;
  }
  return new SbMicrophoneImpl(id);
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
