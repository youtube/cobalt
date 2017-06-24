// Copyright 2017 Google Inc. All Rights Reserved.
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

#include "starboard/shared/starboard/player/decoded_audio_internal.h"

#include <algorithm>

#include "starboard/log.h"

namespace starboard {
namespace shared {
namespace starboard {
namespace player {

DecodedAudio::DecodedAudio()
    : channels_(0),
      sample_type_(kSbMediaAudioSampleTypeInt16),
      storage_type_(kSbMediaAudioFrameStorageTypeInterleaved),
      pts_(0),
      size_(0) {}

DecodedAudio::DecodedAudio(int channels,
                           SbMediaAudioSampleType sample_type,
                           SbMediaAudioFrameStorageType storage_type,
                           SbMediaTime pts,
                           size_t size)
    : channels_(channels),
      sample_type_(sample_type),
      storage_type_(storage_type),
      pts_(pts),
      buffer_(new uint8_t[size]),
      size_(size) {}

int DecodedAudio::frames() const {
  int bytes_per_sample;
  if (sample_type_ == kSbMediaAudioSampleTypeInt16) {
    bytes_per_sample = 2;
  } else {
    SB_DCHECK(sample_type_ == kSbMediaAudioSampleTypeFloat32);
    bytes_per_sample = 4;
  }
  SB_DCHECK(size_ % (bytes_per_sample * channels_) == 0);
  return size_ / bytes_per_sample / channels_;
}

void DecodedAudio::ShrinkTo(size_t new_size) {
  SB_DCHECK(new_size <= size_);
  size_ = new_size;
}

void DecodedAudio::SwitchFormatTo(
    SbMediaAudioSampleType new_sample_type,
    SbMediaAudioFrameStorageType new_storage_type) {
  if (new_sample_type == sample_type_ && new_storage_type == storage_type_) {
    return;
  }

  if (storage_type_ != kSbMediaAudioFrameStorageTypeInterleaved ||
      new_storage_type != kSbMediaAudioFrameStorageTypeInterleaved) {
    SB_NOTREACHED();
    // TODO: Implement switching between other storage type pairs.
    return;
  }

  if (sample_type_ == kSbMediaAudioSampleTypeInt16 &&
      new_sample_type == kSbMediaAudioSampleTypeFloat32 &&
      storage_type_ == kSbMediaAudioFrameStorageTypeInterleaved &&
      new_storage_type == kSbMediaAudioFrameStorageTypeInterleaved) {
    size_t new_size = sizeof(float) * frames() * channels();
    scoped_array<uint8_t> new_buffer(new uint8_t[new_size]);
    float* new_samples = reinterpret_cast<float*>(new_buffer.get());
    int16_t* old_samples = reinterpret_cast<int16_t*>(buffer_.get());

    for (int i = 0; i < frames() * channels(); ++i) {
      new_samples[i] = static_cast<float>(old_samples[i]) / 32768.f;
    }

    buffer_.swap(new_buffer);
    sample_type_ = new_sample_type;
    size_ = new_size;

    return;
  }

  if (sample_type_ == kSbMediaAudioSampleTypeFloat32 &&
      new_sample_type == kSbMediaAudioSampleTypeInt16 &&
      storage_type_ == kSbMediaAudioFrameStorageTypeInterleaved &&
      new_storage_type == kSbMediaAudioFrameStorageTypeInterleaved) {
    size_t new_size = sizeof(int16_t) * frames() * channels();
    scoped_array<uint8_t> new_buffer(new uint8_t[new_size]);
    int16_t* new_samples = reinterpret_cast<int16_t*>(new_buffer.get());
    float* old_samples = reinterpret_cast<float*>(buffer_.get());

    for (int i = 0; i < frames() * channels(); ++i) {
      float sample = std::max(old_samples[i], -1.f);
      sample = std::min(sample, 1.f);
      new_samples[i] = static_cast<int16_t>(sample * 32767.f);
    }

    buffer_.swap(new_buffer);
    sample_type_ = new_sample_type;
    size_ = new_size;

    return;
  }

  // TODO: Implement switching between other sample and storage types.
  SB_NOTREACHED();
}

}  // namespace player
}  // namespace starboard
}  // namespace shared
}  // namespace starboard
