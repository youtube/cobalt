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
#include "starboard/memory.h"
#include "starboard/shared/starboard/media/media_util.h"

namespace starboard {
namespace shared {
namespace starboard {
namespace player {

namespace {

void ConvertSample(const int16_t* source, float* destination) {
  *destination = static_cast<float>(*source) / 32768.f;
}

void ConvertSample(const float* source, int16_t* destination) {
  float sample = std::max(*source, -1.f);
  sample = std::min(sample, 1.f);
  *destination = static_cast<int16_t>(sample * 32767.f);
}

}  // namespace

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
  return static_cast<int>(size_ / bytes_per_sample / channels_);
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

  if (new_storage_type == storage_type_) {
    SwitchSampleTypeTo(new_sample_type);
    return;
  }

  if (new_sample_type == sample_type_) {
    SwitchStorageTypeTo(new_storage_type);
    return;
  }

  // Both sample types and storage types are different, use the slowest way.
  size_t new_size =
      media::GetBytesPerSample(new_sample_type) * frames() * channels();
  scoped_array<uint8_t> new_buffer(new uint8_t[new_size]);

#define InterleavedSampleAddr(start_addr, channel, frame) \
  (start_addr + (frame * channels() + channel))
#define PlanarSampleAddr(start_addr, channel, frame) \
  (start_addr + (channel * frames() + frame))
#define GetSampleAddr(StorageType, start_addr, channel, frame) \
  (StorageType##SampleAddr(start_addr, channel, frame))
#define SwitchTo(OldSampleType, OldStorageType, NewSampleType, NewStorageType) \
  do {                                                                         \
    const OldSampleType* old_samples =                                         \
        reinterpret_cast<OldSampleType*>(buffer_.get());                       \
    NewSampleType* new_samples =                                               \
        reinterpret_cast<NewSampleType*>(new_buffer.get());                    \
                                                                               \
    for (int channel = 0; channel < channels(); ++channel) {                   \
      for (int frame = 0; frame < frames(); ++frame) {                         \
        const OldSampleType* old_sample =                                      \
            GetSampleAddr(OldStorageType, old_samples, channel, frame);        \
        NewSampleType* new_sample =                                            \
            GetSampleAddr(NewStorageType, new_samples, channel, frame);        \
        ConvertSample(old_sample, new_sample);                                 \
      }                                                                        \
    }                                                                          \
  } while (false)

  if (sample_type_ == kSbMediaAudioSampleTypeInt16 &&
      storage_type_ == kSbMediaAudioFrameStorageTypeInterleaved &&
      new_sample_type == kSbMediaAudioSampleTypeFloat32 &&
      new_storage_type == kSbMediaAudioFrameStorageTypePlanar) {
    SwitchTo(int16_t, Interleaved, float, Planar);
  } else if (sample_type_ == kSbMediaAudioSampleTypeInt16 &&
             storage_type_ == kSbMediaAudioFrameStorageTypePlanar &&
             new_sample_type == kSbMediaAudioSampleTypeFloat32 &&
             new_storage_type == kSbMediaAudioFrameStorageTypeInterleaved) {
    SwitchTo(int16_t, Planar, float, Interleaved);
  } else if (sample_type_ == kSbMediaAudioSampleTypeFloat32 &&
             storage_type_ == kSbMediaAudioFrameStorageTypeInterleaved &&
             new_sample_type == kSbMediaAudioSampleTypeInt16 &&
             new_storage_type == kSbMediaAudioFrameStorageTypePlanar) {
    SwitchTo(float, Interleaved, int16_t, Planar);
  } else if (sample_type_ == kSbMediaAudioSampleTypeFloat32 &&
             storage_type_ == kSbMediaAudioFrameStorageTypePlanar &&
             new_sample_type == kSbMediaAudioSampleTypeInt16 &&
             new_storage_type == kSbMediaAudioFrameStorageTypeInterleaved) {
    SwitchTo(float, Planar, int16_t, Interleaved);
  } else {
    SB_NOTREACHED();
  }

  buffer_.swap(new_buffer);
  sample_type_ = new_sample_type;
  storage_type_ = new_storage_type;
  size_ = new_size;
}

void DecodedAudio::SwitchSampleTypeTo(SbMediaAudioSampleType new_sample_type) {
  size_t new_size =
      media::GetBytesPerSample(new_sample_type) * frames() * channels();
  scoped_array<uint8_t> new_buffer(new uint8_t[new_size]);

  if (sample_type_ == kSbMediaAudioSampleTypeInt16 &&
      new_sample_type == kSbMediaAudioSampleTypeFloat32) {
    const int16_t* old_samples = reinterpret_cast<int16_t*>(buffer_.get());
    float* new_samples = reinterpret_cast<float*>(new_buffer.get());

    for (int i = 0; i < frames() * channels(); ++i) {
      ConvertSample(old_samples + i, new_samples + i);
    }
  } else if (sample_type_ == kSbMediaAudioSampleTypeFloat32 &&
             new_sample_type == kSbMediaAudioSampleTypeInt16) {
    const float* old_samples = reinterpret_cast<float*>(buffer_.get());
    int16_t* new_samples = reinterpret_cast<int16_t*>(new_buffer.get());

    for (int i = 0; i < frames() * channels(); ++i) {
      ConvertSample(old_samples + i, new_samples + i);
    }
  }

  buffer_.swap(new_buffer);
  sample_type_ = new_sample_type;
  size_ = new_size;
}

void DecodedAudio::SwitchStorageTypeTo(
    SbMediaAudioFrameStorageType new_storage_type) {
  scoped_array<uint8_t> new_buffer(new uint8_t[size_]);
  int bytes_per_sample = media::GetBytesPerSample(sample_type_);
  uint8_t* old_samples = buffer_.get();
  uint8_t* new_samples = new_buffer.get();

  if (storage_type_ == kSbMediaAudioFrameStorageTypeInterleaved &&
      new_storage_type == kSbMediaAudioFrameStorageTypePlanar) {
    for (int channel = 0; channel < channels(); ++channel) {
      for (int frame = 0; frame < frames(); ++frame) {
        uint8_t* old_sample =
            old_samples + (frame * channels() + channel) * bytes_per_sample;
        uint8_t* new_sample =
            new_samples + (channel * frames() + frame) * bytes_per_sample;
        SbMemoryCopy(new_sample, old_sample, bytes_per_sample);
      }
    }
  } else if (storage_type_ == kSbMediaAudioFrameStorageTypePlanar &&
             new_storage_type == kSbMediaAudioFrameStorageTypeInterleaved) {
    for (int channel = 0; channel < channels(); ++channel) {
      for (int frame = 0; frame < frames(); ++frame) {
        uint8_t* old_sample =
            old_samples + (channel * frames() + frame) * bytes_per_sample;
        uint8_t* new_sample =
            new_samples + (frame * channels() + channel) * bytes_per_sample;
        SbMemoryCopy(new_sample, old_sample, bytes_per_sample);
      }
    }
  }

  buffer_.swap(new_buffer);
  storage_type_ = new_storage_type;
}

}  // namespace player
}  // namespace starboard
}  // namespace shared
}  // namespace starboard
