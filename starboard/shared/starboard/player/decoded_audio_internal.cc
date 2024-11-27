// Copyright 2017 The Cobalt Authors. All Rights Reserved.
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
#include <cstring>
#include <utility>

#include "starboard/common/log.h"
#include "starboard/common/media.h"
#include "starboard/shared/starboard/media/media_util.h"

namespace starboard {
namespace shared {
namespace starboard {
namespace player {

namespace {

using ::starboard::shared::starboard::media::AudioDurationToFrames;
using ::starboard::shared::starboard::media::AudioFramesToDuration;
using ::starboard::shared::starboard::media::GetBytesPerSample;

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
      sample_type_(kSbMediaAudioSampleTypeInt16Deprecated),
      storage_type_(kSbMediaAudioFrameStorageTypeInterleaved),
      timestamp_(0),
      offset_in_bytes_(0),
      size_in_bytes_(0) {}

DecodedAudio::DecodedAudio(int channels,
                           SbMediaAudioSampleType sample_type,
                           SbMediaAudioFrameStorageType storage_type,
                           int64_t timestamp,
                           int size_in_bytes)
    : channels_(channels),
      sample_type_(sample_type),
      storage_type_(storage_type),
      timestamp_(timestamp),
      storage_(size_in_bytes),
      offset_in_bytes_(0),
      size_in_bytes_(size_in_bytes) {
  SB_DCHECK(channels_ > 0);
  SB_DCHECK(size_in_bytes_ >= 0);
  // TODO(b/275199195): Enable the SB_DCHECK below.
  // SB_DCHECK(size_in_bytes_ % (GetBytesPerSample(sample_type_) * channels_) ==
  //           0);
}

DecodedAudio::DecodedAudio(int channels,
                           SbMediaAudioSampleType sample_type,
                           SbMediaAudioFrameStorageType storage_type,
                           int64_t timestamp,
                           int size_in_bytes,
                           Buffer&& storage)
    : channels_(channels),
      sample_type_(sample_type),
      storage_type_(storage_type),
      timestamp_(timestamp),
      storage_(std::move(storage)),
      offset_in_bytes_(0),
      size_in_bytes_(size_in_bytes) {
  SB_DCHECK(channels_ > 0);
  SB_DCHECK(size_in_bytes_ >= 0);
  SB_DCHECK(size_in_bytes_ % (GetBytesPerSample(sample_type_) * channels_) ==
            0);
}

int DecodedAudio::frames() const {
  int bytes_per_sample = GetBytesPerSample(sample_type_);
  SB_DCHECK(size_in_bytes_ % (bytes_per_sample * channels_) == 0);
  return static_cast<int>(size_in_bytes_ / bytes_per_sample / channels_);
}

bool DecodedAudio::IsFormat(SbMediaAudioSampleType sample_type,
                            SbMediaAudioFrameStorageType storage_type) const {
  return sample_type_ == sample_type && storage_type_ == storage_type;
}

void DecodedAudio::ShrinkTo(int new_size_in_bytes) {
  SB_DCHECK(new_size_in_bytes <= size_in_bytes_);
  size_in_bytes_ = new_size_in_bytes;
}

void DecodedAudio::AdjustForSeekTime(int sample_rate, int64_t seeking_to_time) {
  SB_DCHECK(!is_end_of_stream());
  SB_DCHECK(sample_rate != 0);

  int frames_to_skip =
      media::AudioDurationToFrames(seeking_to_time - timestamp(), sample_rate);

  if (sample_rate == 0 || frames_to_skip < 0 || frames_to_skip >= frames()) {
    SB_LOG(WARNING) << "AdjustForSeekTime failed for seeking_to_time: "
                    << seeking_to_time << ", sample_rate: " << sample_rate
                    << ", timestamp: " << timestamp() << ", and there are "
                    << frames() << " frames in the DecodedAudio object.";
    return;
  }

  const auto bytes_per_sample = GetBytesPerSample(sample_type_);
  const auto bytes_per_frame = bytes_per_sample * channels();

  if (storage_type_ == kSbMediaAudioFrameStorageTypeInterleaved) {
    offset_in_bytes_ += frames_to_skip * bytes_per_frame;
    size_in_bytes_ -= frames_to_skip * bytes_per_frame;
    timestamp_ += media::AudioFramesToDuration(frames_to_skip, sample_rate);
    return;
  }

  SB_DCHECK(storage_type_ == kSbMediaAudioFrameStorageTypePlanar);

  Buffer new_storage(size_in_bytes_ - frames_to_skip * bytes_per_frame);
  const auto new_frames = frames() - frames_to_skip;

  const uint8_t* source_addr = data();
  uint8_t* dest_addr = new_storage.data();
  for (int channel = 0; channel < channels(); ++channel) {
    memcpy(dest_addr, source_addr + bytes_per_sample * frames_to_skip,
           new_frames * bytes_per_frame);
    source_addr += frames() * bytes_per_sample;
    dest_addr += new_frames * bytes_per_sample;
  }

  storage_ = std::move(new_storage);
  timestamp_ += media::AudioFramesToDuration(frames_to_skip, sample_rate);
  offset_in_bytes_ = 0;
  size_in_bytes_ = new_frames * bytes_per_frame;
}

void DecodedAudio::AdjustForDiscardedDurations(
    int sample_rate,
    int64_t discarded_duration_from_front,
    int64_t discarded_duration_from_back) {
  SB_DCHECK(discarded_duration_from_front >= 0);
  SB_DCHECK(discarded_duration_from_back >= 0);
  SB_DCHECK(storage_type() == kSbMediaAudioFrameStorageTypeInterleaved);

  if (discarded_duration_from_front == 0 && discarded_duration_from_back == 0) {
    return;
  }

  const auto bytes_per_frame = GetBytesPerSample(sample_type()) * channels_;
  int current_frames = frames();
  int discarded_frames_from_front =
      (discarded_duration_from_front >=
       AudioFramesToDuration(current_frames, sample_rate))
          ? current_frames
          : AudioDurationToFrames(discarded_duration_from_front, sample_rate);
  offset_in_bytes_ += bytes_per_frame * discarded_frames_from_front;
  size_in_bytes_ -= bytes_per_frame * discarded_frames_from_front;

  current_frames = frames();
  int discarded_frames_from_back =
      (discarded_duration_from_back >=
       AudioFramesToDuration(current_frames, sample_rate))
          ? current_frames
          : AudioDurationToFrames(discarded_duration_from_back, sample_rate);
  size_in_bytes_ -= bytes_per_frame * discarded_frames_from_back;
}

scoped_refptr<DecodedAudio> DecodedAudio::SwitchFormatTo(
    SbMediaAudioSampleType new_sample_type,
    SbMediaAudioFrameStorageType new_storage_type) const {
  // The caller should call IsFormat() to check before calling SwitchFormatTo(),
  // as SwitchFormatTo() always copies the whole buffer and is not optimal.
  SB_DCHECK(new_sample_type != sample_type_ ||
            new_storage_type != storage_type_);

  if (new_storage_type == storage_type_) {
    return SwitchSampleTypeTo(new_sample_type);
  }

  if (new_sample_type == sample_type_) {
    return SwitchStorageTypeTo(new_storage_type);
  }

  // Both sample types and storage types are different, use the slowest way.
  int new_size =
      media::GetBytesPerSample(new_sample_type) * frames() * channels();
  scoped_refptr<DecodedAudio> new_decoded_audio = new DecodedAudio(
      channels(), new_sample_type, new_storage_type, timestamp(), new_size);

#define InterleavedSampleAddr(start_addr, channel, frame) \
  (start_addr + (frame * channels() + channel))
#define PlanarSampleAddr(start_addr, channel, frame) \
  (start_addr + (channel * frames() + frame))
#define GetSampleAddr(StorageType, start_addr, channel, frame) \
  (StorageType##SampleAddr(start_addr, channel, frame))
#define SwitchTo(OldSampleType, OldStorageType, NewSampleType, NewStorageType) \
  do {                                                                         \
    const OldSampleType* old_samples =                                         \
        reinterpret_cast<const OldSampleType*>(this->data());                  \
    NewSampleType* new_samples =                                               \
        reinterpret_cast<NewSampleType*>(new_decoded_audio->data());           \
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

  if (sample_type_ == kSbMediaAudioSampleTypeInt16Deprecated &&
      storage_type_ == kSbMediaAudioFrameStorageTypeInterleaved &&
      new_sample_type == kSbMediaAudioSampleTypeFloat32 &&
      new_storage_type == kSbMediaAudioFrameStorageTypePlanar) {
    SwitchTo(int16_t, Interleaved, float, Planar);
  } else if (sample_type_ == kSbMediaAudioSampleTypeInt16Deprecated &&
             storage_type_ == kSbMediaAudioFrameStorageTypePlanar &&
             new_sample_type == kSbMediaAudioSampleTypeFloat32 &&
             new_storage_type == kSbMediaAudioFrameStorageTypeInterleaved) {
    SwitchTo(int16_t, Planar, float, Interleaved);
  } else if (sample_type_ == kSbMediaAudioSampleTypeFloat32 &&
             storage_type_ == kSbMediaAudioFrameStorageTypeInterleaved &&
             new_sample_type == kSbMediaAudioSampleTypeInt16Deprecated &&
             new_storage_type == kSbMediaAudioFrameStorageTypePlanar) {
    SwitchTo(float, Interleaved, int16_t, Planar);
  } else if (sample_type_ == kSbMediaAudioSampleTypeFloat32 &&
             storage_type_ == kSbMediaAudioFrameStorageTypePlanar &&
             new_sample_type == kSbMediaAudioSampleTypeInt16Deprecated &&
             new_storage_type == kSbMediaAudioFrameStorageTypeInterleaved) {
    SwitchTo(float, Planar, int16_t, Interleaved);
  } else {
    SB_NOTREACHED();
  }

  return new_decoded_audio;
}

scoped_refptr<DecodedAudio> DecodedAudio::Clone() const {
  scoped_refptr<DecodedAudio> copy = new DecodedAudio(
      channels(), sample_type(), storage_type(), timestamp(), size_in_bytes());

  memcpy(copy->data(), data(), size_in_bytes());

  return copy;
}

scoped_refptr<DecodedAudio> DecodedAudio::SwitchSampleTypeTo(
    SbMediaAudioSampleType new_sample_type) const {
  int new_size =
      media::GetBytesPerSample(new_sample_type) * frames() * channels();
  scoped_refptr<DecodedAudio> new_decoded_audio = new DecodedAudio(
      channels(), new_sample_type, storage_type(), timestamp(), new_size);

  if (sample_type_ == kSbMediaAudioSampleTypeInt16Deprecated &&
      new_sample_type == kSbMediaAudioSampleTypeFloat32) {
    const int16_t* old_samples = reinterpret_cast<const int16_t*>(this->data());
    float* new_samples = reinterpret_cast<float*>(new_decoded_audio->data());

    for (int i = 0; i < frames() * channels(); ++i) {
      ConvertSample(old_samples + i, new_samples + i);
    }
  } else if (sample_type_ == kSbMediaAudioSampleTypeFloat32 &&
             new_sample_type == kSbMediaAudioSampleTypeInt16Deprecated) {
    const float* old_samples = reinterpret_cast<const float*>(this->data());
    int16_t* new_samples =
        reinterpret_cast<int16_t*>(new_decoded_audio->data());

    for (int i = 0; i < frames() * channels(); ++i) {
      ConvertSample(old_samples + i, new_samples + i);
    }
  }

  return new_decoded_audio;
}

scoped_refptr<DecodedAudio> DecodedAudio::SwitchStorageTypeTo(
    SbMediaAudioFrameStorageType new_storage_type) const {
  scoped_refptr<DecodedAudio> new_decoded_audio =
      new DecodedAudio(channels(), sample_type(), new_storage_type, timestamp(),
                       size_in_bytes());
  int bytes_per_sample = media::GetBytesPerSample(sample_type());
  const uint8_t* old_samples = this->data();
  uint8_t* new_samples = new_decoded_audio->data();

  if (storage_type_ == kSbMediaAudioFrameStorageTypeInterleaved &&
      new_storage_type == kSbMediaAudioFrameStorageTypePlanar) {
    for (int channel = 0; channel < channels(); ++channel) {
      for (int frame = 0; frame < frames(); ++frame) {
        const uint8_t* old_sample =
            old_samples + (frame * channels() + channel) * bytes_per_sample;
        uint8_t* new_sample =
            new_samples + (channel * frames() + frame) * bytes_per_sample;
        memcpy(new_sample, old_sample, bytes_per_sample);
      }
    }
  } else if (storage_type_ == kSbMediaAudioFrameStorageTypePlanar &&
             new_storage_type == kSbMediaAudioFrameStorageTypeInterleaved) {
    for (int channel = 0; channel < channels(); ++channel) {
      for (int frame = 0; frame < frames(); ++frame) {
        const uint8_t* old_sample =
            old_samples + (channel * frames() + frame) * bytes_per_sample;
        uint8_t* new_sample =
            new_samples + (frame * channels() + channel) * bytes_per_sample;
        memcpy(new_sample, old_sample, bytes_per_sample);
      }
    }
  }

  return new_decoded_audio;
}

bool operator==(const DecodedAudio& left, const DecodedAudio& right) {
  if (left.is_end_of_stream() && right.is_end_of_stream()) {
    return true;
  }
  if (left.is_end_of_stream() || right.is_end_of_stream()) {
    return false;
  }

  return left.timestamp() == right.timestamp() &&
         left.channels() == right.channels() &&
         left.sample_type() == right.sample_type() &&
         left.storage_type() == right.storage_type() &&
         left.size_in_bytes() == right.size_in_bytes() &&
         memcmp(left.data(), right.data(), right.size_in_bytes()) == 0;
}

bool operator!=(const DecodedAudio& left, const DecodedAudio& right) {
  return !(left == right);
}

std::ostream& operator<<(std::ostream& os, const DecodedAudio& decoded_audio) {
  if (decoded_audio.is_end_of_stream()) {
    return os << "(eos)";
  }
  return os << "timestamp: " << decoded_audio.timestamp()
            << ", channels: " << decoded_audio.channels() << ", sample type: "
            << GetMediaAudioSampleTypeName(decoded_audio.sample_type())
            << ", storage type: "
            << GetMediaAudioStorageTypeName(decoded_audio.storage_type())
            << ", frames: " << decoded_audio.frames();
}

}  // namespace player
}  // namespace starboard
}  // namespace shared
}  // namespace starboard
