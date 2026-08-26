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
#include <atomic>
#include <cstring>
#include <optional>
#include <type_traits>
#include <utility>

#include "starboard/common/check_op.h"
#include "starboard/common/log.h"
#include "starboard/common/media.h"
#include "starboard/common/pointer_arithmetic.h"
#include "starboard/shared/starboard/media/media_util.h"

#if (SB_IS(ARCH_ARM) || SB_IS(ARCH_ARM64)) && defined(USE_NEON)
#include <arm_neon.h>
#define USE_NEON_FOR_AUDIO 1
#endif  // (SB_IS(ARCH_ARM) || SB_IS(ARCH_ARM64)) && defined(USE_NEON)

namespace starboard {

namespace {

constexpr bool kIsSimdBasedAudioFormatSwitchingDefaultEnabled = false;

static_assert(std::is_trivially_destructible<std::atomic<bool>>::value,
              "g_enable_simd_based_audio_format_switching must be trivially "
              "destructible.");
std::atomic<bool> g_enable_simd_based_audio_format_switching{
    kIsSimdBasedAudioFormatSwitchingDefaultEnabled};

#if defined(USE_NEON_FOR_AUDIO)
bool GetSimdBasedAudioFormatSwitchingSetting() {
  return g_enable_simd_based_audio_format_switching.load(
      std::memory_order_acquire);
}
#endif  // defined(USE_NEON_FOR_AUDIO)

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
  SB_DCHECK_GT(channels_, 0);
  SB_DCHECK_GE(size_in_bytes_, 0);
  // TODO(b/275199195): Enable the SB_DCHECK below.
  // SB_DCHECK_EQ(size_in_bytes_ % (GetBytesPerSample(sample_type_) *
  // channels_),
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
  SB_DCHECK_GT(channels_, 0);
  SB_DCHECK_GE(size_in_bytes_, 0);
  SB_DCHECK_EQ(size_in_bytes_ % (GetBytesPerSample(sample_type_) * channels_),
               0);
}

void DecodedAudio::EnableSimdBasedAudioFormatSwitching() {
  g_enable_simd_based_audio_format_switching.store(true,
                                                   std::memory_order_release);
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

  if (storage_type_ == kSbMediaAudioFrameStorageTypeInterleaved) {
    offset_in_bytes_ += frames_to_skip * bytes_per_frame;
    size_in_bytes_ -= frames_to_skip * bytes_per_frame;
    timestamp_ += AudioFramesToDuration(frames_to_skip, sample_rate);
    return;
  }

  SB_DCHECK_EQ(storage_type_, kSbMediaAudioFrameStorageTypePlanar);

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
  timestamp_ += AudioFramesToDuration(frames_to_skip, sample_rate);
  offset_in_bytes_ = 0;
  size_in_bytes_ = new_frames * bytes_per_frame;
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
  SB_DCHECK_EQ(storage_type(), kSbMediaAudioFrameStorageTypeInterleaved);

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

bool DecodedAudio::IsFormat(SbMediaAudioSampleType sample_type,
                            SbMediaAudioFrameStorageType storage_type) const {
  return sample_type_ == sample_type && storage_type_ == storage_type;
}

scoped_refptr<DecodedAudio> DecodedAudio::SwitchFormatTo(
    SbMediaAudioSampleType new_sample_type,
    SbMediaAudioFrameStorageType new_storage_type,
    std::optional<bool> force_simd) const {
  // The caller should call IsFormat() to check before calling SwitchFormatTo(),
  // as SwitchFormatTo() always copies the whole buffer and is not optimal.
  SB_DCHECK(new_sample_type != sample_type_ ||
            new_storage_type != storage_type_);

#if defined(USE_NEON_FOR_AUDIO)
  bool enable_simd =
      force_simd.value_or(GetSimdBasedAudioFormatSwitchingSetting());
#else   // !defined(USE_NEON_FOR_AUDIO)
  bool enable_simd = false;
#endif  // defined(USE_NEON_FOR_AUDIO)

  if (new_storage_type == storage_type_) {
    return SwitchSampleTypeTo(new_sample_type, enable_simd);
  }

  if (new_sample_type == sample_type_) {
    return SwitchStorageTypeTo(new_storage_type, enable_simd);
  }

  // Both sample types and storage types are different, use the slowest way.
  int new_size = GetBytesPerSample(new_sample_type) * frames() * channels();
  auto new_decoded_audio = make_scoped_refptr<DecodedAudio>(
      channels(), new_sample_type, new_storage_type, timestamp(), new_size);

#if defined(USE_NEON_FOR_AUDIO)
  if (enable_simd && channels() == 2 && IsAligned(frames(), 8)) {
    if (SwitchFormatTo_NEON(new_sample_type, new_storage_type,
                            new_decoded_audio.get())) {
      return new_decoded_audio;
    }
    SB_LOG(WARNING) << "SIMD format switching requested and aligned, but not "
                       "implemented in NEON for "
                    << GetMediaAudioSampleTypeName(sample_type_) << " ("
                    << GetMediaAudioStorageTypeName(storage_type_) << ") -> "
                    << GetMediaAudioSampleTypeName(new_sample_type) << " ("
                    << GetMediaAudioStorageTypeName(new_storage_type)
                    << "). Falling back to C++ scalar.";
  }
#endif  // USE_NEON_FOR_AUDIO

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
  auto copy = make_scoped_refptr<DecodedAudio>(
      channels(), sample_type(), storage_type(), timestamp(), size_in_bytes());

  memcpy(copy->data(), data(), size_in_bytes());
  copy->set_discarded_durations(discarded_duration_from_front_,
                                discarded_duration_from_back_);

  return copy;
}

scoped_refptr<DecodedAudio> DecodedAudio::SwitchSampleTypeTo(
    SbMediaAudioSampleType new_sample_type,
    bool enable_simd) const {
  int new_size = GetBytesPerSample(new_sample_type) * frames() * channels();
  auto new_decoded_audio = make_scoped_refptr<DecodedAudio>(
      channels(), new_sample_type, storage_type(), timestamp(), new_size);

  if (sample_type_ == kSbMediaAudioSampleTypeInt16Deprecated &&
      new_sample_type == kSbMediaAudioSampleTypeFloat32) {
    const int16_t* old_samples = reinterpret_cast<const int16_t*>(this->data());
    float* new_samples = reinterpret_cast<float*>(new_decoded_audio->data());
    int total_samples = frames() * channels();

#if defined(USE_NEON_FOR_AUDIO)
    if (enable_simd && IsAligned(total_samples, 16)) {
      if (SwitchSampleTypeTo_NEON(new_sample_type, new_decoded_audio.get())) {
        return new_decoded_audio;
      }
    }
#endif  // USE_NEON_FOR_AUDIO

    for (int i = 0; i < total_samples; ++i) {
      ConvertSample(old_samples + i, new_samples + i);
    }
  } else if (sample_type_ == kSbMediaAudioSampleTypeFloat32 &&
             new_sample_type == kSbMediaAudioSampleTypeInt16Deprecated) {
    const float* old_samples = reinterpret_cast<const float*>(this->data());
    int16_t* new_samples =
        reinterpret_cast<int16_t*>(new_decoded_audio->data());
    int total_samples = frames() * channels();

#if defined(USE_NEON_FOR_AUDIO)
    if (enable_simd && IsAligned(total_samples, 16)) {
      if (SwitchSampleTypeTo_NEON(new_sample_type, new_decoded_audio.get())) {
        return new_decoded_audio;
      }
    }
#endif  // USE_NEON_FOR_AUDIO

    for (int i = 0; i < total_samples; ++i) {
      ConvertSample(old_samples + i, new_samples + i);
    }
  }

  return new_decoded_audio;
}

scoped_refptr<DecodedAudio> DecodedAudio::SwitchStorageTypeTo(
    SbMediaAudioFrameStorageType new_storage_type,
    bool enable_simd) const {
  auto new_decoded_audio = make_scoped_refptr<DecodedAudio>(
      channels(), sample_type(), new_storage_type, timestamp(),
      size_in_bytes());
  int bytes_per_sample = GetBytesPerSample(sample_type());
  const uint8_t* old_samples = this->data();
  uint8_t* new_samples = new_decoded_audio->data();

#if defined(USE_NEON_FOR_AUDIO)
  if (enable_simd && channels() == 2 && IsAligned(frames(), 8)) {
    if (SwitchStorageTypeTo_NEON(new_storage_type, new_decoded_audio.get())) {
      return new_decoded_audio;
    }
  }
#endif  // USE_NEON_FOR_AUDIO

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

#if defined(USE_NEON_FOR_AUDIO)
bool DecodedAudio::SwitchFormatTo_NEON(
    SbMediaAudioSampleType new_sample_type,
    SbMediaAudioFrameStorageType new_storage_type,
    DecodedAudio* destination_audio) const {
  SB_DCHECK_EQ(channels(), 2);
  SB_DCHECK_EQ(frames() % 8, 0);

  if (sample_type_ == kSbMediaAudioSampleTypeInt16Deprecated &&
      storage_type_ == kSbMediaAudioFrameStorageTypePlanar &&
      new_sample_type == kSbMediaAudioSampleTypeFloat32 &&
      new_storage_type == kSbMediaAudioFrameStorageTypeInterleaved) {
    const int16_t* left_planar = reinterpret_cast<const int16_t*>(data());
    const int16_t* right_planar = left_planar + frames();
    float* interleaved_dst =
        reinterpret_cast<float*>(destination_audio->data());
    int num_frames = frames();

    for (int i = 0; i + 7 < num_frames; i += 8) {
      int16x8_t left_s16 = vld1q_s16(left_planar + i);
      int16x8_t right_s16 = vld1q_s16(right_planar + i);

      int32x4_t left_low_s32 = vmovl_s16(vget_low_s16(left_s16));
      int32x4_t left_high_s32 = vmovl_s16(vget_high_s16(left_s16));
      float32x4_t left_low_f32 = vcvtq_n_f32_s32(left_low_s32, 15);
      float32x4_t left_high_f32 = vcvtq_n_f32_s32(left_high_s32, 15);

      int32x4_t right_low_s32 = vmovl_s16(vget_low_s16(right_s16));
      int32x4_t right_high_s32 = vmovl_s16(vget_high_s16(right_s16));
      float32x4_t right_low_f32 = vcvtq_n_f32_s32(right_low_s32, 15);
      float32x4_t right_high_f32 = vcvtq_n_f32_s32(right_high_s32, 15);

      float32x4x2_t out_low;
      out_low.val[0] = left_low_f32;
      out_low.val[1] = right_low_f32;
      float32x4x2_t out_high;
      out_high.val[0] = left_high_f32;
      out_high.val[1] = right_high_f32;

      vst2q_f32(interleaved_dst + i * 2, out_low);
      vst2q_f32(interleaved_dst + i * 2 + 8, out_high);
    }
    return true;
  } else if (sample_type_ == kSbMediaAudioSampleTypeFloat32 &&
             storage_type_ == kSbMediaAudioFrameStorageTypeInterleaved &&
             new_sample_type == kSbMediaAudioSampleTypeInt16Deprecated &&
             new_storage_type == kSbMediaAudioFrameStorageTypePlanar) {
    const float* interleaved_src = reinterpret_cast<const float*>(data());
    int16_t* left_planar =
        reinterpret_cast<int16_t*>(destination_audio->data());
    int16_t* right_planar = left_planar + destination_audio->frames();
    int num_frames = destination_audio->frames();

    float32x4_t min_val = vdupq_n_f32(-1.0f);
    float32x4_t max_val = vdupq_n_f32(1.0f);
    float32x4_t scale = vdupq_n_f32(32767.f);
    for (int i = 0; i + 7 < num_frames; i += 8) {
      float32x4x2_t src_low = vld2q_f32(interleaved_src + i * 2);
      float32x4x2_t src_high = vld2q_f32(interleaved_src + i * 2 + 8);

      float32x4_t left_low_clamp =
          vminq_f32(vmaxq_f32(src_low.val[0], min_val), max_val);
      int32x4_t left_low_s32 = vcvtq_s32_f32(vmulq_f32(left_low_clamp, scale));

      float32x4_t left_high_clamp =
          vminq_f32(vmaxq_f32(src_high.val[0], min_val), max_val);
      int32x4_t left_high_s32 =
          vcvtq_s32_f32(vmulq_f32(left_high_clamp, scale));

      int16x4_t left_low_s16 = vqmovn_s32(left_low_s32);
      int16x4_t left_high_s16 = vqmovn_s32(left_high_s32);
      int16x8_t left_s16 = vcombine_s16(left_low_s16, left_high_s16);
      vst1q_s16(left_planar + i, left_s16);

      float32x4_t right_low_clamp =
          vminq_f32(vmaxq_f32(src_low.val[1], min_val), max_val);
      int32x4_t right_low_s32 =
          vcvtq_s32_f32(vmulq_f32(right_low_clamp, scale));

      float32x4_t right_high_clamp =
          vminq_f32(vmaxq_f32(src_high.val[1], min_val), max_val);
      int32x4_t right_high_s32 =
          vcvtq_s32_f32(vmulq_f32(right_high_clamp, scale));

      int16x4_t right_low_s16 = vqmovn_s32(right_low_s32);
      int16x4_t right_high_s16 = vqmovn_s32(right_high_s32);
      int16x8_t right_s16 = vcombine_s16(right_low_s16, right_high_s16);
      vst1q_s16(right_planar + i, right_s16);
    }
    return true;
  }
  return false;
}

bool DecodedAudio::SwitchSampleTypeTo_NEON(
    SbMediaAudioSampleType new_sample_type,
    DecodedAudio* destination_audio) const {
  int total_samples = frames() * channels();
  SB_DCHECK_EQ(total_samples % 16, 0);

  if (sample_type_ == kSbMediaAudioSampleTypeInt16Deprecated &&
      new_sample_type == kSbMediaAudioSampleTypeFloat32) {
    const int16_t* old_samples = reinterpret_cast<const int16_t*>(data());
    float* new_samples = reinterpret_cast<float*>(destination_audio->data());

    for (int i = 0; i + 15 < total_samples; i += 16) {
      int16x8_t src0_s16 = vld1q_s16(old_samples + i);
      int16x8_t src1_s16 = vld1q_s16(old_samples + i + 8);

      int32x4_t low0_s32 = vmovl_s16(vget_low_s16(src0_s16));
      int32x4_t high0_s32 = vmovl_s16(vget_high_s16(src0_s16));
      int32x4_t low1_s32 = vmovl_s16(vget_low_s16(src1_s16));
      int32x4_t high1_s32 = vmovl_s16(vget_high_s16(src1_s16));

      vst1q_f32(new_samples + i, vcvtq_n_f32_s32(low0_s32, 15));
      vst1q_f32(new_samples + i + 4, vcvtq_n_f32_s32(high0_s32, 15));
      vst1q_f32(new_samples + i + 8, vcvtq_n_f32_s32(low1_s32, 15));
      vst1q_f32(new_samples + i + 12, vcvtq_n_f32_s32(high1_s32, 15));
    }
    return true;
  } else if (sample_type_ == kSbMediaAudioSampleTypeFloat32 &&
             new_sample_type == kSbMediaAudioSampleTypeInt16Deprecated) {
    const float* old_samples = reinterpret_cast<const float*>(data());
    int16_t* new_samples =
        reinterpret_cast<int16_t*>(destination_audio->data());

    float32x4_t min_val = vdupq_n_f32(-1.0f);
    float32x4_t max_val = vdupq_n_f32(1.0f);
    float32x4_t scale = vdupq_n_f32(32767.f);
    for (int i = 0; i + 15 < total_samples; i += 16) {
      float32x4_t src0 = vld1q_f32(old_samples + i);
      float32x4_t src1 = vld1q_f32(old_samples + i + 4);
      float32x4_t src2 = vld1q_f32(old_samples + i + 8);
      float32x4_t src3 = vld1q_f32(old_samples + i + 12);

      float32x4_t src0_clamp = vminq_f32(vmaxq_f32(src0, min_val), max_val);
      float32x4_t src1_clamp = vminq_f32(vmaxq_f32(src1, min_val), max_val);
      float32x4_t src2_clamp = vminq_f32(vmaxq_f32(src2, min_val), max_val);
      float32x4_t src3_clamp = vminq_f32(vmaxq_f32(src3, min_val), max_val);

      int32x4_t src0_s32 = vcvtq_s32_f32(vmulq_f32(src0_clamp, scale));
      int32x4_t src1_s32 = vcvtq_s32_f32(vmulq_f32(src1_clamp, scale));
      int32x4_t src2_s32 = vcvtq_s32_f32(vmulq_f32(src2_clamp, scale));
      int32x4_t src3_s32 = vcvtq_s32_f32(vmulq_f32(src3_clamp, scale));

      int16x4_t s0_s16 = vqmovn_s32(src0_s32);
      int16x4_t s1_s16 = vqmovn_s32(src1_s32);
      int16x4_t s2_s16 = vqmovn_s32(src2_s32);
      int16x4_t s3_s16 = vqmovn_s32(src3_s32);

      vst1q_s16(new_samples + i, vcombine_s16(s0_s16, s1_s16));
      vst1q_s16(new_samples + i + 8, vcombine_s16(s2_s16, s3_s16));
    }
    return true;
  }
  return false;
}

bool DecodedAudio::SwitchStorageTypeTo_NEON(
    SbMediaAudioFrameStorageType new_storage_type,
    DecodedAudio* destination_audio) const {
  SB_DCHECK_EQ(channels(), 2);
  SB_DCHECK_EQ(frames() % 8, 0);

  const uint8_t* old_samples = data();
  uint8_t* new_samples = destination_audio->data();

  if (storage_type_ == kSbMediaAudioFrameStorageTypeInterleaved &&
      new_storage_type == kSbMediaAudioFrameStorageTypePlanar) {
    if (sample_type_ == kSbMediaAudioSampleTypeInt16Deprecated) {
      const int16_t* interleaved_src =
          reinterpret_cast<const int16_t*>(old_samples);
      int16_t* left_planar = reinterpret_cast<int16_t*>(new_samples);
      int16_t* right_planar = left_planar + frames();
      int num_frames = frames();
      for (int i = 0; i + 7 < num_frames; i += 8) {
        int16x8x2_t src = vld2q_s16(interleaved_src + i * 2);
        vst1q_s16(left_planar + i, src.val[0]);
        vst1q_s16(right_planar + i, src.val[1]);
      }
      return true;
    } else if (sample_type_ == kSbMediaAudioSampleTypeFloat32) {
      const float* interleaved_src =
          reinterpret_cast<const float*>(old_samples);
      float* left_planar = reinterpret_cast<float*>(new_samples);
      float* right_planar = left_planar + frames();
      int num_frames = frames();
      for (int i = 0; i + 7 < num_frames; i += 8) {
        float32x4x2_t src0 = vld2q_f32(interleaved_src + i * 2);
        float32x4x2_t src1 = vld2q_f32(interleaved_src + i * 2 + 8);
        vst1q_f32(left_planar + i, src0.val[0]);
        vst1q_f32(right_planar + i, src0.val[1]);
        vst1q_f32(left_planar + i + 4, src1.val[0]);
        vst1q_f32(right_planar + i + 4, src1.val[1]);
      }
      return true;
    }
  } else if (storage_type_ == kSbMediaAudioFrameStorageTypePlanar &&
             new_storage_type == kSbMediaAudioFrameStorageTypeInterleaved) {
    if (sample_type_ == kSbMediaAudioSampleTypeInt16Deprecated) {
      const int16_t* left_planar =
          reinterpret_cast<const int16_t*>(old_samples);
      const int16_t* right_planar = left_planar + frames();
      int16_t* interleaved_dst = reinterpret_cast<int16_t*>(new_samples);
      int num_frames = frames();
      for (int i = 0; i + 7 < num_frames; i += 8) {
        int16x8x2_t src;
        src.val[0] = vld1q_s16(left_planar + i);
        src.val[1] = vld1q_s16(right_planar + i);
        vst2q_s16(interleaved_dst + i * 2, src);
      }
      return true;
    } else if (sample_type_ == kSbMediaAudioSampleTypeFloat32) {
      const float* left_planar = reinterpret_cast<const float*>(old_samples);
      const float* right_planar = left_planar + frames();
      float* interleaved_dst = reinterpret_cast<float*>(new_samples);
      int num_frames = frames();
      for (int i = 0; i + 7 < num_frames; i += 8) {
        float32x4x2_t src0;
        src0.val[0] = vld1q_f32(left_planar + i);
        src0.val[1] = vld1q_f32(right_planar + i);
        float32x4x2_t src1;
        src1.val[0] = vld1q_f32(left_planar + i + 4);
        src1.val[1] = vld1q_f32(right_planar + i + 4);
        vst2q_f32(interleaved_dst + i * 2, src0);
        vst2q_f32(interleaved_dst + i * 2 + 8, src1);
      }
      return true;
    }
  }
  return false;
}
#endif  // defined(USE_NEON_FOR_AUDIO)

}  // namespace starboard
