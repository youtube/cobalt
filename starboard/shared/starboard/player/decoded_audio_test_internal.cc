// Copyright 2023 The Cobalt Authors. All Rights Reserved.
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

#include <cmath>
#include <utility>

#include "starboard/common/log.h"
#include "starboard/shared/starboard/media/media_util.h"
#include "starboard/shared/starboard/player/decoded_audio_internal.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace {

constexpr int kChannels = 2;
constexpr int64_t kTimestampUsec = 1'000'000;
constexpr int kSizeInBytes = 4192;
constexpr int kSampleRate = 22050;

constexpr double kPi = 3.1415926535897932384626;

constexpr SbMediaAudioSampleType kSampleTypes[] = {
    kSbMediaAudioSampleTypeInt16Deprecated, kSbMediaAudioSampleTypeFloat32};
constexpr SbMediaAudioFrameStorageType kStorageTypes[] = {
    kSbMediaAudioFrameStorageTypeInterleaved,
    kSbMediaAudioFrameStorageTypePlanar};

// The following two functions fill `data` with audio samples of a sine wave
// starting from `initial_angle`.  `stride` is the number of samples per group.
// For example:
// 1. When `stride` is one, all samples filled with be continuous.
// 2. When `stride` is 2, filling `data` and `data + 1` fills an interleaved
//    stereo audio buffer with samples.
void Fill(int16_t* data,
          double initial_angle,
          double angle_step,
          int count,
          int stride) {
  SB_DCHECK(data);

  auto current_angel = initial_angle;
  for (int i = 0; i < count; ++i) {
    *data = static_cast<int16_t>(sin(current_angel) * 32767);
    current_angel += angle_step;
    data += stride;
  }
}

void Fill(float* data,
          double initial_angle,
          double angle_step,
          int count,
          int stride) {
  SB_DCHECK(data);

  auto current_angel = initial_angle;
  for (int i = 0; i < count; ++i) {
    *data = static_cast<float>(sin(current_angel));
    current_angel += angle_step;
    data += stride;
  }
}

// The following two functions verify `data` with audio samples against a sine
// wave starting from `initial_angle`.  `stride` is the number of samples per
// group.  For example:
// 1. When `stride` is one, all samples will be treated as continuous.
// 2. When `stride` is 2, verifying against `data` and `data + 1` verifies an
//    interleaved stereo audio buffer.
void Verify(const int16_t* data,
            double initial_angle,
            double angle_step,
            int count,
            int stride) {
  SB_DCHECK(data);

  auto current_angel = initial_angle;
  for (int i = 0; i < count; ++i) {
    const auto current_value = static_cast<int16_t>(sin(current_angel) * 32767);
    // Using `ASSERT_NEAR()` to allow for small value drifting due to
    // conversions between sample types.
    ASSERT_NEAR(static_cast<double>(*data), static_cast<double>(current_value),
                2.0 / 32767);
    current_angel += angle_step;
    data += stride;
  }
}

void Verify(const float* data,
            double initial_angle,
            double angle_step,
            int count,
            int stride) {
  SB_DCHECK(data);

  auto current_angel = initial_angle;
  for (int i = 0; i < count; ++i) {
    const auto current_value = static_cast<float>(sin(current_angel));
    // Using `ASSERT_NEAR()` to allow for small value drifting due to
    // conversions between sample types.
    ASSERT_NEAR(static_cast<double>(*data), static_cast<double>(current_value),
                2.0 / 32767);
    current_angel += angle_step;
    data += stride;
  }
}

// Fill `decoded_audio` with sine wave samples, with phase shift of Pi/2 on each
// channel.
void Fill(scoped_refptr<DecodedAudio>* decoded_audio) {
  SB_DCHECK(decoded_audio);
  SB_DCHECK(*decoded_audio);

  bool is_int16 =
      (*decoded_audio)->sample_type() == kSbMediaAudioSampleTypeInt16Deprecated;
  bool is_interleaved = (*decoded_audio)->storage_type() ==
                        kSbMediaAudioFrameStorageTypeInterleaved;

  for (int i = 0; i < (*decoded_audio)->channels(); ++i) {
    if (is_int16 && is_interleaved) {
      Fill((*decoded_audio)->data_as_int16() + i, kPi / 2 * i, kPi / 180,
           (*decoded_audio)->frames(), (*decoded_audio)->channels());
    } else if (!is_int16 && is_interleaved) {
      Fill((*decoded_audio)->data_as_float32() + i, kPi / 2 * i, kPi / 180,
           (*decoded_audio)->frames(), (*decoded_audio)->channels());
    } else if (is_int16 && !is_interleaved) {
      Fill((*decoded_audio)->data_as_int16() + (*decoded_audio)->frames() * i,
           kPi / 2 * i, kPi / 180, (*decoded_audio)->frames(), 1);
    } else if (!is_int16 && !is_interleaved) {
      Fill((*decoded_audio)->data_as_float32() + (*decoded_audio)->frames() * i,
           kPi / 2 * i, kPi / 180, (*decoded_audio)->frames(), 1);
    }
  }
}

// verify `decoded_audio` against sine wave samples, with phase shift of Pi/2 on
// each channel.
void Verify(const scoped_refptr<DecodedAudio>& decoded_audio) {
  SB_DCHECK(decoded_audio);

  bool is_int16 =
      decoded_audio->sample_type() == kSbMediaAudioSampleTypeInt16Deprecated;
  bool is_interleaved =
      decoded_audio->storage_type() == kSbMediaAudioFrameStorageTypeInterleaved;

  for (int i = 0; i < decoded_audio->channels(); ++i) {
    if (is_int16 && is_interleaved) {
      ASSERT_NO_FATAL_FAILURE(
          Verify(decoded_audio->data_as_int16() + i, kPi / 2 * i, kPi / 180,
                 decoded_audio->frames(), decoded_audio->channels()));
    } else if (!is_int16 && is_interleaved) {
      ASSERT_NO_FATAL_FAILURE(
          Verify(decoded_audio->data_as_float32() + i, kPi / 2 * i, kPi / 180,
                 decoded_audio->frames(), decoded_audio->channels()));
    } else if (is_int16 && !is_interleaved) {
      ASSERT_NO_FATAL_FAILURE(
          Verify(decoded_audio->data_as_int16() + decoded_audio->frames() * i,
                 kPi / 2 * i, kPi / 180, decoded_audio->frames(), 1));
    } else if (!is_int16 && !is_interleaved) {
      ASSERT_NO_FATAL_FAILURE(
          Verify(decoded_audio->data_as_float32() + decoded_audio->frames() * i,
                 kPi / 2 * i, kPi / 180, decoded_audio->frames(), 1));
    }
  }
}

TEST(DecodedAudioTest, DefaultCtor) {
  scoped_refptr<DecodedAudio> decoded_audio(new DecodedAudio);
  EXPECT_TRUE(decoded_audio->is_end_of_stream());
}

TEST(DecodedAudioTest, CtorWithSize) {
  for (auto sample_type : kSampleTypes) {
    for (auto storage_type : kStorageTypes) {
      scoped_refptr<DecodedAudio> decoded_audio(new DecodedAudio(
          kChannels, sample_type, storage_type, kTimestampUsec, kSizeInBytes));

      EXPECT_FALSE(decoded_audio->is_end_of_stream());
      EXPECT_EQ(decoded_audio->channels(), kChannels);
      EXPECT_EQ(decoded_audio->sample_type(), sample_type);
      EXPECT_EQ(decoded_audio->storage_type(), storage_type);
      EXPECT_EQ(decoded_audio->size_in_bytes(), kSizeInBytes);
      EXPECT_EQ(decoded_audio->frames(),
                kSizeInBytes / GetBytesPerSample(decoded_audio->sample_type()) /
                    kChannels);

      Fill(&decoded_audio);
      Verify(decoded_audio);
    }
  }
}

TEST(DecodedAudioTest, CtorWithMoveCtor) {
  Buffer original(128);
  memset(original.data(), 'x', 128);

  const uint8_t* original_data_pointer = original.data();

  scoped_refptr<DecodedAudio> decoded_audio(
      new DecodedAudio(kChannels, kSampleTypes[0], kStorageTypes[0],
                       kTimestampUsec, 128, std::move(original)));
  ASSERT_EQ(decoded_audio->size_in_bytes(), 128);
  ASSERT_NE(decoded_audio->data(), nullptr);
  ASSERT_EQ(decoded_audio->data(), original_data_pointer);

  for (int i = 0; i < decoded_audio->size_in_bytes(); ++i) {
    ASSERT_EQ(decoded_audio->data()[i], 'x');
  }
}

TEST(DecodedAudioTest, AdjustForSeekTime) {
  for (int channels = 1; channels <= 6; ++channels) {
    for (auto sample_type : kSampleTypes) {
      scoped_refptr<DecodedAudio> original_decoded_audio(new DecodedAudio(
          kChannels, sample_type, kSbMediaAudioFrameStorageTypeInterleaved,
          kTimestampUsec, kSizeInBytes));
      Fill(&original_decoded_audio);

      scoped_refptr<DecodedAudio> adjusted_decoded_audio =
          original_decoded_audio->Clone();

      // Adjust to the beginning of `adjusted_decoded_audio` should be a no-op.
      adjusted_decoded_audio->AdjustForSeekTime(
          kSampleRate, adjusted_decoded_audio->timestamp());
      ASSERT_EQ(*original_decoded_audio, *adjusted_decoded_audio);

      // Adjust to an invalid timestamp before the time range of
      // `adjusted_decoded_audio`, it's a no-op.
      adjusted_decoded_audio->AdjustForSeekTime(
          kSampleRate, adjusted_decoded_audio->timestamp() - 1'000'000LL / 2);
      ASSERT_EQ(*original_decoded_audio, *adjusted_decoded_audio);

      // Adjust to an invalid timestamp after the time range of
      // `adjusted_decoded_audio`, it's also a no-op.
      adjusted_decoded_audio->AdjustForSeekTime(
          kSampleRate, adjusted_decoded_audio->timestamp() + 1'000'000LL * 100);
      ASSERT_EQ(*original_decoded_audio, *adjusted_decoded_audio);

      const int64_t duration =
          AudioFramesToDuration(adjusted_decoded_audio->frames(), kSampleRate);
      const int64_t duration_of_one_frame =
          AudioFramesToDuration(1, kSampleRate) + 1;
      for (int i = 1; i < 10; ++i) {
        adjusted_decoded_audio = original_decoded_audio->Clone();
        // Adjust to the middle of `adjusted_decoded_audio`.
        int64_t seek_time =
            adjusted_decoded_audio->timestamp() + duration * i / 10;
        adjusted_decoded_audio->AdjustForSeekTime(kSampleRate, seek_time);
        ASSERT_NEAR(adjusted_decoded_audio->frames(),
                    original_decoded_audio->frames() * (10 - i) / 10, 1);

        ASSERT_LE(adjusted_decoded_audio->timestamp(), seek_time);
        ASSERT_NEAR(adjusted_decoded_audio->timestamp(), seek_time,
                    duration_of_one_frame);

        auto offset_in_bytes = original_decoded_audio->size_in_bytes() -
                               adjusted_decoded_audio->size_in_bytes();
        ASSERT_TRUE(memcmp(adjusted_decoded_audio->data(),
                           original_decoded_audio->data() + offset_in_bytes,
                           adjusted_decoded_audio->size_in_bytes()) == 0);
      }
    }
  }
}

TEST(DecodedAudioTest, AdjustForDiscardedDurations) {
  for (int channels = 1; channels <= 6; ++channels) {
    for (auto sample_type : kSampleTypes) {
      scoped_refptr<DecodedAudio> original_decoded_audio(new DecodedAudio(
          kChannels, sample_type, kSbMediaAudioFrameStorageTypeInterleaved,
          kTimestampUsec, kSizeInBytes));
      Fill(&original_decoded_audio);

      scoped_refptr<DecodedAudio> adjusted_decoded_audio =
          original_decoded_audio->Clone();

      adjusted_decoded_audio->AdjustForDiscardedDurations(kSampleRate, 0, 0);
      ASSERT_EQ(*original_decoded_audio, *adjusted_decoded_audio);

      auto duration_of_decoded_audio =
          AudioFramesToDuration(original_decoded_audio->frames(), kSampleRate);
      auto quarter_duration = duration_of_decoded_audio / 4;
      adjusted_decoded_audio->AdjustForDiscardedDurations(
          kSampleRate, quarter_duration, quarter_duration);
      ASSERT_NEAR(adjusted_decoded_audio->frames(),
                  original_decoded_audio->frames() / 2, 2);
      ASSERT_EQ(adjusted_decoded_audio->timestamp(),
                original_decoded_audio->timestamp());

      adjusted_decoded_audio = original_decoded_audio->Clone();
      // Adjust more frames than it has from front
      adjusted_decoded_audio->AdjustForDiscardedDurations(
          kSampleRate, duration_of_decoded_audio * 2, 0);
      ASSERT_EQ(adjusted_decoded_audio->frames(), 0);
      ASSERT_EQ(adjusted_decoded_audio->timestamp(),
                original_decoded_audio->timestamp());

      adjusted_decoded_audio = original_decoded_audio->Clone();
      // Adjust more frames than it has from back
      adjusted_decoded_audio->AdjustForDiscardedDurations(
          kSampleRate, 0, duration_of_decoded_audio * 2);
      ASSERT_EQ(adjusted_decoded_audio->frames(), 0);
      ASSERT_EQ(adjusted_decoded_audio->timestamp(),
                original_decoded_audio->timestamp());
    }
  }
}

TEST(DecodedAudioTest, SwitchFormatTo) {
  for (auto original_sample_type : kSampleTypes) {
    for (auto original_storage_type : kStorageTypes) {
      scoped_refptr<DecodedAudio> original_decoded_audio(new DecodedAudio(
          kChannels, original_sample_type, original_storage_type,
          kTimestampUsec, kSizeInBytes));

      Fill(&original_decoded_audio);

      for (auto new_sample_type : kSampleTypes) {
        for (auto new_storage_type : kStorageTypes) {
          if (!original_decoded_audio->IsFormat(new_sample_type,
                                                new_storage_type)) {
            scoped_refptr<DecodedAudio> new_decoded_audio =
                original_decoded_audio->SwitchFormatTo(new_sample_type,
                                                       new_storage_type);

            EXPECT_FALSE(new_decoded_audio->is_end_of_stream());
            EXPECT_EQ(new_decoded_audio->channels(),
                      original_decoded_audio->channels());
            EXPECT_EQ(new_decoded_audio->timestamp(),
                      original_decoded_audio->timestamp());
            EXPECT_EQ(new_decoded_audio->frames(),
                      original_decoded_audio->frames());
            EXPECT_TRUE(
                new_decoded_audio->IsFormat(new_sample_type, new_storage_type));

            ASSERT_NO_FATAL_FAILURE(Verify(new_decoded_audio));
          }
        }
      }
    }
  }
}

TEST(DecodedAudioTest, Clone) {
  for (auto sample_type : kSampleTypes) {
    scoped_refptr<DecodedAudio> decoded_audio(new DecodedAudio(
        kChannels, sample_type, kSbMediaAudioFrameStorageTypeInterleaved,
        kTimestampUsec, kSizeInBytes));
    Fill(&decoded_audio);
    auto copy = decoded_audio->Clone();
    ASSERT_EQ(*copy, *decoded_audio);
    ASSERT_GT(decoded_audio->size_in_bytes(), 0);
    decoded_audio->data()[0] = ~decoded_audio->data()[0];
    ASSERT_NE(*copy, *decoded_audio);
  }
}

TEST(DecodedAudioTest, SwitchFormatTo_NeonSimdExhaustive) {
  // 1. Test conversions starting from Int16 Planar containing all possible
  // int16_t values.
  int total_samples = 65536;
  int size_in_bytes = total_samples * sizeof(int16_t);

  scoped_refptr<DecodedAudio> int16_planar_base(new DecodedAudio(
      kChannels, kSbMediaAudioSampleTypeInt16Deprecated,
      kSbMediaAudioFrameStorageTypePlanar, kTimestampUsec, size_in_bytes));

  int16_t* int16_data = int16_planar_base->data_as_int16();
  for (int i = 0; i < total_samples; ++i) {
    int16_data[i] = static_cast<int16_t>(i - 32768);
  }

  // Test Case 1: Int16 Planar -> Float32 Interleaved (Combined Conversion)
  {
    scoped_refptr<DecodedAudio> ref_float_interleaved =
        int16_planar_base->SwitchFormatTo(
            kSbMediaAudioSampleTypeFloat32,
            kSbMediaAudioFrameStorageTypeInterleaved, /*enable_simd=*/false);

    scoped_refptr<DecodedAudio> simd_float_interleaved =
        int16_planar_base->SwitchFormatTo(
            kSbMediaAudioSampleTypeFloat32,
            kSbMediaAudioFrameStorageTypeInterleaved, /*enable_simd=*/true);

    ASSERT_EQ(ref_float_interleaved->size_in_bytes(),
              simd_float_interleaved->size_in_bytes());
    const float* ref_data = ref_float_interleaved->data_as_float32();
    const float* simd_data = simd_float_interleaved->data_as_float32();
    int num_floats = ref_float_interleaved->size_in_bytes() / sizeof(float);
    for (int i = 0; i < num_floats; ++i) {
      ASSERT_FLOAT_EQ(ref_data[i], simd_data[i]) << "Mismatch at index " << i;
    }
  }

  // Test Case 2: Int16 Planar -> Float32 Planar (Sample Type conversion only)
  {
    scoped_refptr<DecodedAudio> ref_float_planar =
        int16_planar_base->SwitchFormatTo(kSbMediaAudioSampleTypeFloat32,
                                          kSbMediaAudioFrameStorageTypePlanar,
                                          /*enable_simd=*/false);

    scoped_refptr<DecodedAudio> simd_float_planar =
        int16_planar_base->SwitchFormatTo(kSbMediaAudioSampleTypeFloat32,
                                          kSbMediaAudioFrameStorageTypePlanar,
                                          /*enable_simd=*/true);

    ASSERT_EQ(ref_float_planar->size_in_bytes(),
              simd_float_planar->size_in_bytes());
    const float* ref_data = ref_float_planar->data_as_float32();
    const float* simd_data = simd_float_planar->data_as_float32();
    int num_floats = ref_float_planar->size_in_bytes() / sizeof(float);
    for (int i = 0; i < num_floats; ++i) {
      ASSERT_FLOAT_EQ(ref_data[i], simd_data[i]) << "Mismatch at index " << i;
    }
  }

  // 2. Test conversions starting from Float32 containing clamping values.
  // We will populate floats from -2.0f to 2.0f.
  int num_float_samples =
      8000;  // Must be multiple of 8 for SIMD loop alignment
  int float_size_in_bytes = num_float_samples * sizeof(float);
  scoped_refptr<DecodedAudio> float_interleaved_base(
      new DecodedAudio(kChannels, kSbMediaAudioSampleTypeFloat32,
                       kSbMediaAudioFrameStorageTypeInterleaved, kTimestampUsec,
                       float_size_in_bytes));

  float* float_data = float_interleaved_base->data_as_float32();
  for (int i = 0; i < num_float_samples; ++i) {
    float_data[i] = -2.0f + 4.0f * (static_cast<float>(i) / num_float_samples);
  }

  // Test Case 3: Float32 Interleaved -> Int16 Planar (Combined Conversion)
  {
    scoped_refptr<DecodedAudio> ref_int16_planar =
        float_interleaved_base->SwitchFormatTo(
            kSbMediaAudioSampleTypeInt16Deprecated,
            kSbMediaAudioFrameStorageTypePlanar, /*enable_simd=*/false);

    scoped_refptr<DecodedAudio> simd_int16_planar =
        float_interleaved_base->SwitchFormatTo(
            kSbMediaAudioSampleTypeInt16Deprecated,
            kSbMediaAudioFrameStorageTypePlanar, /*enable_simd=*/true);

    ASSERT_EQ(ref_int16_planar->size_in_bytes(),
              simd_int16_planar->size_in_bytes());
    const int16_t* ref_data = ref_int16_planar->data_as_int16();
    const int16_t* simd_data = simd_int16_planar->data_as_int16();
    int num_ints = ref_int16_planar->size_in_bytes() / sizeof(int16_t);
    for (int i = 0; i < num_ints; ++i) {
      ASSERT_EQ(ref_data[i], simd_data[i]) << "Mismatch at index " << i;
    }
  }

  // Test Case 4: Float32 Interleaved -> Int16 Interleaved (Sample Type
  // conversion only)
  {
    scoped_refptr<DecodedAudio> ref_int16_interleaved =
        float_interleaved_base->SwitchFormatTo(
            kSbMediaAudioSampleTypeInt16Deprecated,
            kSbMediaAudioFrameStorageTypeInterleaved, /*enable_simd=*/false);

    scoped_refptr<DecodedAudio> simd_int16_interleaved =
        float_interleaved_base->SwitchFormatTo(
            kSbMediaAudioSampleTypeInt16Deprecated,
            kSbMediaAudioFrameStorageTypeInterleaved, /*enable_simd=*/true);

    ASSERT_EQ(ref_int16_interleaved->size_in_bytes(),
              simd_int16_interleaved->size_in_bytes());
    const int16_t* ref_data = ref_int16_interleaved->data_as_int16();
    const int16_t* simd_data = simd_int16_interleaved->data_as_int16();
    int num_ints = ref_int16_interleaved->size_in_bytes() / sizeof(int16_t);
    for (int i = 0; i < num_ints; ++i) {
      ASSERT_EQ(ref_data[i], simd_data[i]) << "Mismatch at index " << i;
    }
  }

  // Test Case 5: Int16 Planar -> Int16 Interleaved (Layout conversion only)
  {
    scoped_refptr<DecodedAudio> ref_int16_interleaved =
        int16_planar_base->SwitchFormatTo(
            kSbMediaAudioSampleTypeInt16Deprecated,
            kSbMediaAudioFrameStorageTypeInterleaved, /*enable_simd=*/false);

    scoped_refptr<DecodedAudio> simd_int16_interleaved =
        int16_planar_base->SwitchFormatTo(
            kSbMediaAudioSampleTypeInt16Deprecated,
            kSbMediaAudioFrameStorageTypeInterleaved, /*enable_simd=*/true);

    ASSERT_EQ(ref_int16_interleaved->size_in_bytes(),
              simd_int16_interleaved->size_in_bytes());
    const int16_t* ref_data = ref_int16_interleaved->data_as_int16();
    const int16_t* simd_data = simd_int16_interleaved->data_as_int16();
    int num_ints = ref_int16_interleaved->size_in_bytes() / sizeof(int16_t);
    for (int i = 0; i < num_ints; ++i) {
      ASSERT_EQ(ref_data[i], simd_data[i]) << "Mismatch at index " << i;
    }
  }

  // Test Case 6: Int16 Interleaved -> Int16 Planar (Layout conversion only)
  {
    scoped_refptr<DecodedAudio> int16_interleaved =
        int16_planar_base->SwitchFormatTo(
            kSbMediaAudioSampleTypeInt16Deprecated,
            kSbMediaAudioFrameStorageTypeInterleaved, /*enable_simd=*/false);

    scoped_refptr<DecodedAudio> ref_planar = int16_interleaved->SwitchFormatTo(
        kSbMediaAudioSampleTypeInt16Deprecated,
        kSbMediaAudioFrameStorageTypePlanar, /*enable_simd=*/false);

    scoped_refptr<DecodedAudio> simd_planar = int16_interleaved->SwitchFormatTo(
        kSbMediaAudioSampleTypeInt16Deprecated,
        kSbMediaAudioFrameStorageTypePlanar, /*enable_simd=*/true);

    ASSERT_EQ(ref_planar->size_in_bytes(), simd_planar->size_in_bytes());
    const int16_t* ref_data = ref_planar->data_as_int16();
    const int16_t* simd_data = simd_planar->data_as_int16();
    int num_ints = ref_planar->size_in_bytes() / sizeof(int16_t);
    for (int i = 0; i < num_ints; ++i) {
      ASSERT_EQ(ref_data[i], simd_data[i]) << "Mismatch at index " << i;
    }
  }

  // Test Case 3: Float32 Interleaved -> Int16 Planar (Combined Conversion)
  {
    scoped_refptr<DecodedAudio> ref_int16_planar =
        float_interleaved_base->SwitchFormatTo(
            kSbMediaAudioSampleTypeInt16Deprecated,
            kSbMediaAudioFrameStorageTypePlanar, /*enable_simd=*/false);

    scoped_refptr<DecodedAudio> simd_int16_planar =
        float_interleaved_base->SwitchFormatTo(
            kSbMediaAudioSampleTypeInt16Deprecated,
            kSbMediaAudioFrameStorageTypePlanar, /*enable_simd=*/true);

    ASSERT_EQ(ref_int16_planar->size_in_bytes(),
              simd_int16_planar->size_in_bytes());
    const int16_t* ref_data = ref_int16_planar->data_as_int16();
    const int16_t* simd_data = simd_int16_planar->data_as_int16();
    int num_ints = ref_int16_planar->size_in_bytes() / sizeof(int16_t);
    for (int i = 0; i < num_ints; ++i) {
      ASSERT_EQ(ref_data[i], simd_data[i]) << "Mismatch at index " << i;
    }
  }

  // Test Case 4: Float32 Interleaved -> Int16 Interleaved (Sample Type
  // conversion only)
  {
    scoped_refptr<DecodedAudio> ref_int16_interleaved =
        float_interleaved_base->SwitchFormatTo(
            kSbMediaAudioSampleTypeInt16Deprecated,
            kSbMediaAudioFrameStorageTypeInterleaved, /*enable_simd=*/false);

    scoped_refptr<DecodedAudio> simd_int16_interleaved =
        float_interleaved_base->SwitchFormatTo(
            kSbMediaAudioSampleTypeInt16Deprecated,
            kSbMediaAudioFrameStorageTypeInterleaved, /*enable_simd=*/true);

    ASSERT_EQ(ref_int16_interleaved->size_in_bytes(),
              simd_int16_interleaved->size_in_bytes());
    const int16_t* ref_data = ref_int16_interleaved->data_as_int16();
    const int16_t* simd_data = simd_int16_interleaved->data_as_int16();
    int num_ints = ref_int16_interleaved->size_in_bytes() / sizeof(int16_t);
    for (int i = 0; i < num_ints; ++i) {
      ASSERT_EQ(ref_data[i], simd_data[i]) << "Mismatch at index " << i;
    }
  }

  // Test Case 7: Float32 Interleaved -> Float32 Planar (Layout conversion only)
  {
    scoped_refptr<DecodedAudio> ref_float_planar =
        float_interleaved_base->SwitchFormatTo(
            kSbMediaAudioSampleTypeFloat32, kSbMediaAudioFrameStorageTypePlanar,
            /*enable_simd=*/false);

    scoped_refptr<DecodedAudio> simd_float_planar =
        float_interleaved_base->SwitchFormatTo(
            kSbMediaAudioSampleTypeFloat32, kSbMediaAudioFrameStorageTypePlanar,
            /*enable_simd=*/true);

    ASSERT_EQ(ref_float_planar->size_in_bytes(),
              simd_float_planar->size_in_bytes());
    const float* ref_data = ref_float_planar->data_as_float32();
    const float* simd_data = simd_float_planar->data_as_float32();
    int num_floats = ref_float_planar->size_in_bytes() / sizeof(float);
    for (int i = 0; i < num_floats; ++i) {
      ASSERT_FLOAT_EQ(ref_data[i], simd_data[i]) << "Mismatch at index " << i;
    }
  }

  // Test Case 8: Float32 Planar -> Float32 Interleaved (Layout conversion only)
  {
    scoped_refptr<DecodedAudio> float_planar =
        float_interleaved_base->SwitchFormatTo(
            kSbMediaAudioSampleTypeFloat32, kSbMediaAudioFrameStorageTypePlanar,
            /*enable_simd=*/false);

    scoped_refptr<DecodedAudio> ref_interleaved = float_planar->SwitchFormatTo(
        kSbMediaAudioSampleTypeFloat32,
        kSbMediaAudioFrameStorageTypeInterleaved, /*enable_simd=*/false);

    scoped_refptr<DecodedAudio> simd_interleaved = float_planar->SwitchFormatTo(
        kSbMediaAudioSampleTypeFloat32,
        kSbMediaAudioFrameStorageTypeInterleaved, /*enable_simd=*/true);

    ASSERT_EQ(ref_interleaved->size_in_bytes(),
              simd_interleaved->size_in_bytes());
    const float* ref_data = ref_interleaved->data_as_float32();
    const float* simd_data = simd_interleaved->data_as_float32();
    int num_floats = ref_interleaved->size_in_bytes() / sizeof(float);
    for (int i = 0; i < num_floats; ++i) {
      ASSERT_FLOAT_EQ(ref_data[i], simd_data[i]) << "Mismatch at index " << i;
    }
  }
}

}  // namespace
}  // namespace starboard
