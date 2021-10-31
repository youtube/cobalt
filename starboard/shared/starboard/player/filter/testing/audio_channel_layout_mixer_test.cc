// Copyright 2019 The Cobalt Authors. All Rights Reserved.
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
#include <functional>
#include <numeric>

#include "starboard/common/ref_counted.h"
#include "starboard/common/scoped_ptr.h"
#include "starboard/shared/starboard/player/decoded_audio_internal.h"
#include "starboard/shared/starboard/player/filter/audio_channel_layout_mixer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace shared {
namespace starboard {
namespace player {
namespace filter {
namespace testing {
namespace {

using ::testing::Combine;
using ::testing::Values;

class AudioChannelLayoutMixerTest
    : public ::testing::TestWithParam<
          std::tuple<SbMediaAudioSampleType, SbMediaAudioFrameStorageType>> {
 protected:
  AudioChannelLayoutMixerTest()
      : sample_type_(std::get<0>(GetParam())),
        storage_type_(std::get<1>(GetParam())) {
    SB_DCHECK(sample_type_ == kSbMediaAudioSampleTypeInt16Deprecated ||
              sample_type_ == kSbMediaAudioSampleTypeFloat32);
    SB_DCHECK(storage_type_ == kSbMediaAudioFrameStorageTypeInterleaved ||
              storage_type_ == kSbMediaAudioFrameStorageTypePlanar);
  }

  scoped_refptr<DecodedAudio> GetTestDecodedAudio(int num_of_channels) {
    // Interleaved test audio data, stored in float.
    const float kMonoInputAudioData[] = {
        -1.0f, -0.5f, 0.0f, 0.5f, 1.0f,
    };

    const float kStereoInputAudioData[] = {
        -1.0f, 0.5f, -0.5f, 0.5f, 0.0f, 0.0f, 0.5f, 0.5f, 1.0f, 1.0f,
    };

    const float kQuadInputAudioData[] = {
        -1.0f, 1.0f, 0.0f, 0.0f, -0.5f, 0.5f, -0.5f, -0.5f, 0.0f, 0.0f,
        0.0f,  0.0f, 0.5f, 0.5f, 0.5f,  0.5f, 1.0f,  1.0f,  1.0f, 1.0f,
    };

    const float kFivePointOneInputAudioData[] = {
        -1.0f, 1.0f,  1.0f, -1.0f, -1.0f, -1.0f, -0.5f, 0.5f, 0.5f, -0.5f,
        -0.5f, -0.5f, 0.0f, 0.0f,  0.0f,  0.0f,  0.0f,  0.0f, 0.5f, 0.5f,
        0.5f,  0.5f,  0.5f, 0.5f,  1.0f,  1.0f,  1.0f,  1.0f, 1.0f, 1.0f,
    };

    const size_t kInputFrames = SB_ARRAY_SIZE(kMonoInputAudioData);

    const float* data_buffer;
    if (num_of_channels == 1) {
      data_buffer = kMonoInputAudioData;
    } else if (num_of_channels == 2) {
      data_buffer = kStereoInputAudioData;
    } else if (num_of_channels == 4) {
      data_buffer = kQuadInputAudioData;
    } else if (num_of_channels == 6) {
      data_buffer = kFivePointOneInputAudioData;
    } else {
      SB_NOTREACHED() << "Invalid number of channels.";
    }

    scoped_refptr<DecodedAudio> decoded_audio(new DecodedAudio(
        num_of_channels, sample_type_, storage_type_, 0,
        kInputFrames * num_of_channels *
            (sample_type_ == kSbMediaAudioSampleTypeFloat32 ? 4 : 2)));

    if (sample_type_ == kSbMediaAudioSampleTypeFloat32) {
      float* dest_buffer = reinterpret_cast<float*>(decoded_audio->buffer());
      for (size_t i = 0; i < num_of_channels * kInputFrames; i++) {
        int src_index = i;
        if (storage_type_ == kSbMediaAudioFrameStorageTypePlanar) {
          src_index = i % kInputFrames * num_of_channels + i / kInputFrames;
        }
        dest_buffer[i] = data_buffer[src_index];
      }
    } else {
      int16_t* dest_buffer =
          reinterpret_cast<int16_t*>(decoded_audio->buffer());
      for (size_t i = 0; i < num_of_channels * kInputFrames; i++) {
        int src_index = i;
        if (storage_type_ == kSbMediaAudioFrameStorageTypePlanar) {
          src_index = i % kInputFrames * num_of_channels + i / kInputFrames;
        }
        dest_buffer[i] = data_buffer[src_index] * kSbInt16Max;
      }
    }
    return decoded_audio;
  }

  void AssertExpectedAndOutputMatch(const scoped_refptr<DecodedAudio>& input,
                                    const scoped_refptr<DecodedAudio>& output,
                                    int output_num_of_channels,
                                    const float* expected_output) {
    ASSERT_EQ(output_num_of_channels, output->channels());
    ASSERT_EQ(input->sample_type(), output->sample_type());
    ASSERT_EQ(input->storage_type(), output->storage_type());
    ASSERT_EQ(input->timestamp(), output->timestamp());
    ASSERT_EQ(input->size() * output->channels(),
              output->size() * input->channels());

    if (sample_type_ == kSbMediaAudioSampleTypeFloat32) {
      float* output_buffer = reinterpret_cast<float*>(output->buffer());
      for (size_t i = 0; i < output->frames() * output_num_of_channels; i++) {
        int src_index = i;
        if (storage_type_ == kSbMediaAudioFrameStorageTypePlanar) {
          src_index = i % output->frames() * output_num_of_channels +
                      i / output->frames();
        }
        if (expected_output[src_index] >= 1.0f) {
          ASSERT_GE(output_buffer[i], 0.999f);
        } else if (expected_output[src_index] <= -0.999f) {
          ASSERT_LE(output_buffer[i], -1.0f);
        } else {
          ASSERT_LE(fabs(expected_output[src_index] - output_buffer[i]),
                    0.001f);
        }
      }
    } else {
      int16_t* output_buffer = reinterpret_cast<int16_t*>(output->buffer());
      for (size_t i = 0; i < output->frames() * output_num_of_channels; i++) {
        int src_index = i;
        if (storage_type_ == kSbMediaAudioFrameStorageTypePlanar) {
          src_index = i % output->frames() * output_num_of_channels +
                      i / output->frames();
        }
        ASSERT_LE(fabs(expected_output[src_index] -
                       static_cast<float>(output_buffer[i]) /
                           static_cast<float>(kSbInt16Max)),
                  0.001f);
      }
    }
  }

  SbMediaAudioSampleType sample_type_;
  SbMediaAudioFrameStorageType storage_type_;
};

TEST_P(AudioChannelLayoutMixerTest, MixToMono) {
  scoped_ptr<AudioChannelLayoutMixer> mixer =
      AudioChannelLayoutMixer::Create(sample_type_, storage_type_, 1);
  ASSERT_TRUE(mixer);

  const float kExpectedMonoToMonoOutput[] = {
      -1.0f, -0.5f, 0.0f, 0.5f, 1.0f,
  };
  scoped_refptr<DecodedAudio> mono_input = GetTestDecodedAudio(1);
  scoped_refptr<DecodedAudio> mono_output = mixer->Mix(mono_input);
  AssertExpectedAndOutputMatch(mono_input, mono_output, 1,
                               kExpectedMonoToMonoOutput);

  const float kExpectedStereoToMonoOutput[] = {
      -0.25f, 0.0f, 0.0f, 0.5f, 1.0f,
  };
  scoped_refptr<DecodedAudio> stereo_input = GetTestDecodedAudio(2);
  scoped_refptr<DecodedAudio> stereo_output = mixer->Mix(stereo_input);
  AssertExpectedAndOutputMatch(stereo_input, stereo_output, 1,
                               kExpectedStereoToMonoOutput);

  const float kExpectedQuadToMonoOutput[] = {
      0.0f, -0.25f, 0.0f, 0.5f, 1.0f,
  };
  scoped_refptr<DecodedAudio> quad_input = GetTestDecodedAudio(4);
  scoped_refptr<DecodedAudio> quad_output = mixer->Mix(quad_input);
  AssertExpectedAndOutputMatch(quad_input, quad_output, 1,
                               kExpectedQuadToMonoOutput);

  const float kExpectedFivePointOneToMonoOutput[] = {
      0.0f, 0.0f, 0.0f, 1.0f, 1.0f,
  };
  scoped_refptr<DecodedAudio> five_point_one_input = GetTestDecodedAudio(6);
  scoped_refptr<DecodedAudio> five_point_one_output =
      mixer->Mix(five_point_one_input);
  AssertExpectedAndOutputMatch(five_point_one_input, five_point_one_output, 1,
                               kExpectedFivePointOneToMonoOutput);
}

TEST_P(AudioChannelLayoutMixerTest, MixToStereo) {
  scoped_ptr<AudioChannelLayoutMixer> mixer =
      AudioChannelLayoutMixer::Create(sample_type_, storage_type_, 2);
  ASSERT_TRUE(mixer);

  const float kExpectedMonoToStereoOutput[] = {
      -1.0f, -1.0f, -0.5f, -0.5f, 0.0f, 0.0f, 0.5f, 0.5f, 1.0f, 1.0f,
  };
  scoped_refptr<DecodedAudio> mono_input = GetTestDecodedAudio(1);
  scoped_refptr<DecodedAudio> mono_output = mixer->Mix(mono_input);
  AssertExpectedAndOutputMatch(mono_input, mono_output, 2,
                               kExpectedMonoToStereoOutput);

  const float kExpectedStereoToStereoOutput[] = {
      -1.0f, 0.5f, -0.5f, 0.5f, 0.0f, 0.0f, 0.5f, 0.5f, 1.0f, 1.0f,
  };
  scoped_refptr<DecodedAudio> stereo_input = GetTestDecodedAudio(2);
  scoped_refptr<DecodedAudio> stereo_output = mixer->Mix(stereo_input);
  AssertExpectedAndOutputMatch(stereo_input, stereo_output, 2,
                               kExpectedStereoToStereoOutput);

  const float kExpectedQuadToStereoOutput[] = {
      -0.5f, 0.5f, -0.5f, 0.0f, 0.0f, 0.0f, 0.5f, 0.5f, 1.0f, 1.0f,
  };
  scoped_refptr<DecodedAudio> quad_input = GetTestDecodedAudio(4);
  scoped_refptr<DecodedAudio> quad_output = mixer->Mix(quad_input);
  AssertExpectedAndOutputMatch(quad_input, quad_output, 2,
                               kExpectedQuadToStereoOutput);

  const float kExpectedFivePointOneToStereoOutput[] = {
      -1.0f, 1.0f, -0.5f, 0.5f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f,
  };
  scoped_refptr<DecodedAudio> five_point_one_input = GetTestDecodedAudio(6);
  scoped_refptr<DecodedAudio> five_point_one_output =
      mixer->Mix(five_point_one_input);
  AssertExpectedAndOutputMatch(five_point_one_input, five_point_one_output, 2,
                               kExpectedFivePointOneToStereoOutput);
}

TEST_P(AudioChannelLayoutMixerTest, MixToQuad) {
  scoped_ptr<AudioChannelLayoutMixer> mixer =
      AudioChannelLayoutMixer::Create(sample_type_, storage_type_, 4);
  ASSERT_TRUE(mixer);

  const float kExpectedMonoToQuadOutput[] = {
      -1.0f, -1.0f, 0.0f, 0.0f, -0.5f, -0.5f, 0.0f, 0.0f, 0.0f, 0.0f,
      0.0f,  0.0f,  0.5f, 0.5f, 0.0f,  0.0f,  1.0f, 1.0f, 0.0f, 0.0f,
  };
  scoped_refptr<DecodedAudio> mono_input = GetTestDecodedAudio(1);
  scoped_refptr<DecodedAudio> mono_output = mixer->Mix(mono_input);
  AssertExpectedAndOutputMatch(mono_input, mono_output, 4,
                               kExpectedMonoToQuadOutput);

  const float kExpectedStereoToQuadOutput[] = {
      -1.0f, 0.5f, 0.0f, 0.0f, -0.5f, 0.5f, 0.0f, 0.0f, 0.0f, 0.0f,
      0.0f,  0.0f, 0.5f, 0.5f, 0.0f,  0.0f, 1.0f, 1.0f, 0.0f, 0.0f,
  };
  scoped_refptr<DecodedAudio> stereo_input = GetTestDecodedAudio(2);
  scoped_refptr<DecodedAudio> stereo_output = mixer->Mix(stereo_input);
  AssertExpectedAndOutputMatch(stereo_input, stereo_output, 4,
                               kExpectedStereoToQuadOutput);

  const float kExpectedQuadToQuadOutput[] = {
      -1.0f, 1.0f, 0.0f, 0.0f, -0.5f, 0.5f, -0.5f, -0.5f, 0.0f, 0.0f,
      0.0f,  0.0f, 0.5f, 0.5f, 0.5f,  0.5f, 1.0f,  1.0f,  1.0f, 1.0f,
  };
  scoped_refptr<DecodedAudio> quad_input = GetTestDecodedAudio(4);
  scoped_refptr<DecodedAudio> quad_output = mixer->Mix(quad_input);
  AssertExpectedAndOutputMatch(quad_input, quad_output, 4,
                               kExpectedQuadToQuadOutput);

  const float kExpectedFivePointOneToQuadOutput[] = {
      -0.2929f, 1.0f, -1.0f, -1.0f, -0.1464f, 0.8535f, -0.5f,
      -0.5f,    0.0f, 0.0f,  0.0f,  0.0f,     0.8535f, 0.8535f,
      0.5f,     0.5f, 1.0f,  1.0f,  1.0f,     1.0f,
  };
  scoped_refptr<DecodedAudio> five_point_one_input = GetTestDecodedAudio(6);
  scoped_refptr<DecodedAudio> five_point_one_output =
      mixer->Mix(five_point_one_input);
  AssertExpectedAndOutputMatch(five_point_one_input, five_point_one_output, 4,
                               kExpectedFivePointOneToQuadOutput);
}

TEST_P(AudioChannelLayoutMixerTest, MixToFivePointOne) {
  scoped_ptr<AudioChannelLayoutMixer> mixer =
      AudioChannelLayoutMixer::Create(sample_type_, storage_type_, 6);
  ASSERT_TRUE(mixer);

  const float kExpectedMonoToFivePointOneOutput[] = {
      0.0f, 0.0f, -1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, -0.5f, 0.0f,
      0.0f, 0.0f, 0.0f,  0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,  0.0f,
      0.5f, 0.0f, 0.0f,  0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f,  0.0f,
  };
  scoped_refptr<DecodedAudio> mono_input = GetTestDecodedAudio(1);
  scoped_refptr<DecodedAudio> mono_output = mixer->Mix(mono_input);
  AssertExpectedAndOutputMatch(mono_input, mono_output, 6,
                               kExpectedMonoToFivePointOneOutput);

  const float kExpectedStereoToFivePointOneOutput[] = {
      -1.0f, 0.5f, 0.0f, 0.0f, 0.0f, 0.0f, -0.5f, 0.5f, 0.0f, 0.0f,
      0.0f,  0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,  0.0f, 0.5f, 0.5f,
      0.0f,  0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f,  0.0f, 0.0f, 0.0f,
  };
  scoped_refptr<DecodedAudio> stereo_input = GetTestDecodedAudio(2);
  scoped_refptr<DecodedAudio> stereo_output = mixer->Mix(stereo_input);
  AssertExpectedAndOutputMatch(stereo_input, stereo_output, 6,
                               kExpectedStereoToFivePointOneOutput);

  const float kExpectedQuadToFivePointOneOutput[] = {
      -1.0f, 1.0f,  0.0f, 0.0f, 0.0f, 0.0f, -0.5f, 0.5f, 0.0f, 0.0f,
      -0.5f, -0.5f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,  0.0f, 0.5f, 0.5f,
      0.0f,  0.0f,  0.5f, 0.5f, 1.0f, 1.0f, 0.0f,  0.0f, 1.0f, 1.0f,
  };
  scoped_refptr<DecodedAudio> quad_input = GetTestDecodedAudio(4);
  scoped_refptr<DecodedAudio> quad_output = mixer->Mix(quad_input);
  AssertExpectedAndOutputMatch(quad_input, quad_output, 6,
                               kExpectedQuadToFivePointOneOutput);

  const float kExpectedFivePointOneToFivePointOneOutput[] = {
      -1.0f, 1.0f,  1.0f, -1.0f, -1.0f, -1.0f, -0.5f, 0.5f, 0.5f, -0.5f,
      -0.5f, -0.5f, 0.0f, 0.0f,  0.0f,  0.0f,  0.0f,  0.0f, 0.5f, 0.5f,
      0.5f,  0.5f,  0.5f, 0.5f,  1.0f,  1.0f,  1.0f,  1.0f, 1.0f, 1.0f,
  };
  scoped_refptr<DecodedAudio> five_point_one_input = GetTestDecodedAudio(6);
  scoped_refptr<DecodedAudio> five_point_one_output =
      mixer->Mix(five_point_one_input);
  AssertExpectedAndOutputMatch(five_point_one_input, five_point_one_output, 6,
                               kExpectedFivePointOneToFivePointOneOutput);
}

INSTANTIATE_TEST_CASE_P(AudioChannelLayoutMixerTests,
                        AudioChannelLayoutMixerTest,
                        Combine(Values(kSbMediaAudioSampleTypeInt16Deprecated,
                                       kSbMediaAudioSampleTypeFloat32),
                                Values(kSbMediaAudioFrameStorageTypeInterleaved,
                                       kSbMediaAudioFrameStorageTypePlanar)));

}  // namespace
}  // namespace testing
}  // namespace filter
}  // namespace player
}  // namespace starboard
}  // namespace shared
}  // namespace starboard
