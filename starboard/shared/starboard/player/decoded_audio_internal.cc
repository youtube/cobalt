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

#include "starboard/common/check_op.h"
#include "starboard/common/log.h"
#include "starboard/common/media.h"
#include "starboard/shared/starboard/media/media_util.h"

namespace starboard {

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

// static
DecodedAudio DecodedAudio::CreateEOSBuffer() {
  return DecodedAudio();
}

DecodedAudio::DecodedAudio()
    : channels_(0),
      sample_type_(kSbMediaAudioSampleTypeInt16Deprecated),
      timestamp_(0),
      offset_in_bytes_(0),
      size_in_bytes_(0) {}

DecodedAudio::DecodedAudio(int channels,
                           SbMediaAudioSampleType sample_type,
                           int64_t timestamp,
                           int size_in_bytes)
    : channels_(channels),
      sample_type_(sample_type),
      timestamp_(timestamp),
      storage_(size_in_bytes),
      offset_in_bytes_(0),
      size_in_bytes_(size_in_bytes) {
  SB_DCHECK_GT(channels_, 0);
  SB_DCHECK_GE(size_in_bytes_, 0);
  // TODO(b/275199195): Enable the SB_DCHECK below.
  // SB_DCHECK_EQ(size_in_bytes_ % (GetBytesPerSample(sample_type_) *
  // channels_),
  //           0);
}

DecodedAudio::DecodedAudio(int channels,
                           SbMediaAudioSampleType sample_type,
                           int64_t timestamp,
                           int size_in_bytes,
                           Buffer&& storage)
    : channels_(channels),
      sample_type_(sample_type),
      timestamp_(timestamp),
      storage_(std::move(storage)),
      offset_in_bytes_(0),
      size_in_bytes_(size_in_bytes) {
  SB_DCHECK_GT(channels_, 0);
  SB_DCHECK_GE(size_in_bytes_, 0);
  SB_DCHECK_EQ(size_in_bytes_ % (GetBytesPerSample(sample_type_) * channels_),
               0);
}

DecodedAudio::DecodedAudio(DecodedAudio&& other) noexcept
    : channels_(std::exchange(other.channels_, 0)),
      sample_type_(other.sample_type_),
      timestamp_(std::exchange(other.timestamp_, 0)),
      storage_(std::move(other.storage_)),
      offset_in_bytes_(std::exchange(other.offset_in_bytes_, 0)),
      size_in_bytes_(std::exchange(other.size_in_bytes_, 0)) {}

DecodedAudio& DecodedAudio::operator=(DecodedAudio&& other) noexcept {
  if (this == &other) {
    return *this;
  }

  channels_ = std::exchange(other.channels_, 0);
  sample_type_ = other.sample_type_;
  timestamp_ = std::exchange(other.timestamp_, 0);
  storage_ = std::move(other.storage_);
  offset_in_bytes_ = std::exchange(other.offset_in_bytes_, 0);
  size_in_bytes_ = std::exchange(other.size_in_bytes_, 0);

  return *this;
}

int DecodedAudio::frames() const {
  int bytes_per_sample = GetBytesPerSample(sample_type_);
  SB_DCHECK_EQ(size_in_bytes_ % (bytes_per_sample * channels_), 0);
  return static_cast<int>(size_in_bytes_ / bytes_per_sample / channels_);
}

void DecodedAudio::ShrinkTo(int new_size_in_bytes) {
  SB_DCHECK_LE(new_size_in_bytes, size_in_bytes_);
  size_in_bytes_ = new_size_in_bytes;
}

void DecodedAudio::AdjustForSeekTime(int sample_rate, int64_t seeking_to_time) {
  SB_DCHECK(!is_end_of_stream());
  SB_DCHECK_NE(sample_rate, 0);

  int frames_to_skip =
      AudioDurationToFrames(seeking_to_time - timestamp(), sample_rate);

  if (sample_rate == 0 || frames_to_skip < 0 || frames_to_skip >= frames()) {
    SB_LOG(WARNING) << "AdjustForSeekTime failed for seeking_to_time: "
                    << seeking_to_time << ", sample_rate: " << sample_rate
                    << ", timestamp: " << timestamp() << ", and there are "
                    << frames() << " frames in the DecodedAudio object.";
    return;
  }

  const auto bytes_per_sample = GetBytesPerSample(sample_type_);
  const auto bytes_per_frame = bytes_per_sample * channels();

  if (frames_to_skip > 0) {
    offset_in_bytes_ += frames_to_skip * bytes_per_frame;
    size_in_bytes_ -= frames_to_skip * bytes_per_frame;
    timestamp_ += AudioFramesToDuration(frames_to_skip, sample_rate);
  }
}

void DecodedAudio::AdjustForDiscardedDurations(
    int sample_rate,
    int64_t discarded_duration_from_front,
    int64_t discarded_duration_from_back) {
  if (discarded_duration_from_front < 0) {
    SB_LOG(WARNING) << "discarded_duration_from_front is negative with value "
                    << discarded_duration_from_front << ". Setting to 0.";
    discarded_duration_from_front = 0;
  }
  if (discarded_duration_from_back < 0) {
    SB_LOG(WARNING) << "discarded_duration_from_back is negative with value "
                    << discarded_duration_from_back << ". Setting to 0.";
    discarded_duration_from_back = 0;
  }

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

DecodedAudio DecodedAudio::SwitchFormatTo(
    SbMediaAudioSampleType new_sample_type) const {
  // The caller should check sample type before calling SwitchFormatTo(),
  // as SwitchFormatTo() always copies the whole buffer and is not optimal.
  SB_DCHECK_NE(new_sample_type, sample_type_);

  return SwitchSampleTypeTo(new_sample_type);
}

DecodedAudio DecodedAudio::CloneForTesting() const {
  DecodedAudio copy(channels(), sample_type(), timestamp(), size_in_bytes());

  if (size_in_bytes() > 0) {
    memcpy(copy.data(), data(), size_in_bytes());
  }

  return copy;
}

DecodedAudio DecodedAudio::SwitchSampleTypeTo(
    SbMediaAudioSampleType new_sample_type) const {
  int new_size = GetBytesPerSample(new_sample_type) * frames() * channels();
  DecodedAudio new_decoded_audio(channels(), new_sample_type, timestamp(),
                                 new_size);

  if (sample_type_ == kSbMediaAudioSampleTypeInt16Deprecated &&
      new_sample_type == kSbMediaAudioSampleTypeFloat32) {
    const int16_t* old_samples = reinterpret_cast<const int16_t*>(this->data());
    float* new_samples = reinterpret_cast<float*>(new_decoded_audio.data());
    int total_samples = frames() * channels();

    for (int i = 0; i < total_samples; ++i) {
      ConvertSample(old_samples + i, new_samples + i);
    }
  } else if (sample_type_ == kSbMediaAudioSampleTypeFloat32 &&
             new_sample_type == kSbMediaAudioSampleTypeInt16Deprecated) {
    const float* old_samples = reinterpret_cast<const float*>(this->data());
    int16_t* new_samples = reinterpret_cast<int16_t*>(new_decoded_audio.data());
    int total_samples = frames() * channels();

    for (int i = 0; i < total_samples; ++i) {
      ConvertSample(old_samples + i, new_samples + i);
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
            << ", frames: " << decoded_audio.frames();
}

}  // namespace starboard
