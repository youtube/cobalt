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

#include "starboard/shared/starboard/player/filter/audio_resampler.h"

#include <tuple>
#include <vector>

#include "starboard/common/string.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace {

using ::testing::Combine;
using ::testing::ValuesIn;

typedef ::testing::tuple<SbMediaAudioSampleType,
                         SbMediaAudioFrameStorageType,
                         int,
                         SbMediaAudioSampleType,
                         SbMediaAudioFrameStorageType,
                         int,
                         int>
    AudioResamplerTestParam;

SbMediaAudioSampleType kSampleTypesToTest[] = {
    kSbMediaAudioSampleTypeInt16Deprecated,
    kSbMediaAudioSampleTypeFloat32,
};
SbMediaAudioFrameStorageType kStorageTypesToTest[] = {
    kSbMediaAudioFrameStorageTypeInterleaved,
    kSbMediaAudioFrameStorageTypePlanar,
};
int kSampleRatesToTest[] = {22050, 44100, 48000};
int kChannelsToTest[] = {1, 2, 6};

const char* ConvertSampleTypeToString(SbMediaAudioSampleType sample_type) {
  if (sample_type == kSbMediaAudioSampleTypeInt16Deprecated) {
    return "int16";
  } else if (sample_type == kSbMediaAudioSampleTypeFloat32) {
    return "float32";
  }
  SB_NOTREACHED();
  return "";
}

const char* ConvertStorageTypeToString(
    SbMediaAudioFrameStorageType storage_type) {
  if (storage_type == kSbMediaAudioFrameStorageTypeInterleaved) {
    return "interleaved";
  } else if (storage_type == kSbMediaAudioFrameStorageTypePlanar) {
    return "planar";
  }
  SB_NOTREACHED();
  return "";
}

class AudioResamplerTest
    : public ::testing::TestWithParam<AudioResamplerTestParam> {
 protected:
  AudioResamplerTest() {
    const AudioResamplerTestParam& param = GetParam();
    source_sample_type_ = std::get<0>(param);
    source_storage_type_ = std::get<1>(param);
    source_sample_rate_ = std::get<2>(param);
    destination_sample_type_ = std::get<3>(param);
    destination_storage_type_ = std::get<4>(param);
    destination_sample_rate_ = std::get<5>(param);
    channels_ = std::get<6>(param);

    GenerateAudioInputs();
  }

  void GenerateAudioInputs() {
    const int kNumberOfInputs = 40;
    const int kSamplesPerInput = 1024;

    int total_frames = 0;
    for (int i = 0; i < kNumberOfInputs; i++) {
      int sample_size =
          source_sample_type_ == kSbMediaAudioSampleTypeInt16Deprecated
              ? sizeof(int16_t)
              : sizeof(float);
      int audio_size = kSamplesPerInput * channels_ * sample_size;
      scoped_refptr<DecodedAudio> input = new DecodedAudio(
          channels_, source_sample_type_, source_storage_type_,
          1'000'000LL * total_frames / source_sample_rate_, audio_size);
      total_frames += kSamplesPerInput;
      inputs_.push_back(input);
    }
  }

  SbMediaAudioSampleType source_sample_type_;
  SbMediaAudioFrameStorageType source_storage_type_;
  int source_sample_rate_;
  SbMediaAudioSampleType destination_sample_type_;
  SbMediaAudioFrameStorageType destination_storage_type_;
  int destination_sample_rate_;
  int channels_;

  std::vector<scoped_refptr<DecodedAudio>> inputs_;
};

TEST_P(AudioResamplerTest, SunnyDay) {
  std::unique_ptr<AudioResampler> resampler = AudioResampler::Create(
      source_sample_type_, source_storage_type_, source_sample_rate_,
      destination_sample_type_, destination_storage_type_,
      destination_sample_rate_, channels_);

  int total_input_frames = 0;
  std::vector<scoped_refptr<DecodedAudio>> outputs;
  for (auto input : inputs_) {
    scoped_refptr<DecodedAudio> output = resampler->Resample(input);
    total_input_frames += input->frames();
    if (output) {
      outputs.push_back(output);
    }
  }
  scoped_refptr<DecodedAudio> output = resampler->WriteEndOfStream();
  if (output) {
    outputs.push_back(output);
  }

  // Theoretically, if the input is too small, the last output could consist
  // of multiple inputs. But as our audio unit is always larger than resampler
  // block size. The amount of outputs should be always same as the inputs, and
  // they should have same timestamp.
  EXPECT_EQ(inputs_.size(), outputs.size());
  int total_output_frames = 0;
  for (int i = 0; i < outputs.size(); i++) {
    EXPECT_EQ(inputs_[i]->timestamp(), outputs[i]->timestamp());
    total_output_frames += outputs[i]->frames();
    EXPECT_NEAR(
        inputs_[i]->frames() * destination_sample_rate_ / source_sample_rate_,
        outputs[i]->frames(), 5);
  }
  EXPECT_NEAR(
      total_input_frames * destination_sample_rate_ / source_sample_rate_,
      total_output_frames, 5);
}

std::string GetTestConfigName(
    ::testing::TestParamInfo<AudioResamplerTestParam> info) {
  const AudioResamplerTestParam& param = info.param;
  SbMediaAudioSampleType source_sample_type = std::get<0>(param);
  SbMediaAudioFrameStorageType source_storage_type = std::get<1>(param);
  int source_sample_rate = std::get<2>(param);
  SbMediaAudioSampleType destination_sample_type = std::get<3>(param);
  SbMediaAudioFrameStorageType destination_storage_type = std::get<4>(param);
  int destination_sample_rate = std::get<5>(param);
  int channels = std::get<6>(param);
  std::string name = FormatString(
      "%s_%s_%d_to_%s_%s_%d_channels_%d",
      ConvertSampleTypeToString(source_sample_type),
      ConvertStorageTypeToString(source_storage_type), source_sample_rate,
      ConvertSampleTypeToString(destination_sample_type),
      ConvertStorageTypeToString(destination_storage_type),
      destination_sample_rate, channels);
  return name;
}

INSTANTIATE_TEST_CASE_P(AudioResamplerTests,
                        AudioResamplerTest,
                        Combine(ValuesIn(kSampleTypesToTest),
                                ValuesIn(kStorageTypesToTest),
                                ValuesIn(kSampleRatesToTest),
                                ValuesIn(kSampleTypesToTest),
                                ValuesIn(kStorageTypesToTest),
                                ValuesIn(kSampleRatesToTest),
                                ValuesIn(kChannelsToTest)),
                        GetTestConfigName);

}  // namespace

}  // namespace starboard
